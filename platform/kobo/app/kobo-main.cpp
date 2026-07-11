#include <Arduino.h>
#include <HalGPIO.h>
#include <KoboEvdevTouch.h>
#include <KoboSysfs.h>
#include <KoboTouchGesture.h>
#include <MappedInputManager.h>
#include <CrossPointSettings.h>

#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

void setup();
void loop();
extern MappedInputManager mappedInputManager;

// The shared main loop invokes the simulator smoke-test hook when SIMULATOR
// compatibility is enabled. Kobo uses those compatibility APIs without the
// desktop smoke-test harness, so provide the platform no-op here.
void runSimulatorSmokeTestTick() {}

namespace {
std::atomic<bool> running{true};
crossink::kobo::KoboEvdevTouch touch;
crossink::kobo::KoboTouchGesture gestures;
crossink::kobo::TouchFrame lastTouchFrame{};
bool haveTouchFrame = false;
crossink::kobo::KoboFrontlightSysfs frontlight;
int appliedFrontlight = -1;
int appliedOrientation = -1;

void stop(int /*signal*/) { running.store(false); }

void fatalSignal(const int signal) {
  constexpr char prefix[] = "CrossInk Kobo Beta 1 fatal signal ";
  char record[sizeof(prefix) + 4]{};
  std::size_t length = sizeof(prefix) - 1;
  std::memcpy(record, prefix, length);
  if (signal >= 10) record[length++] = static_cast<char>('0' + signal / 10);
  record[length++] = static_cast<char>('0' + signal % 10);
  record[length++] = '\n';
  const int descriptor = ::open("/data/.crossink/crash/last-signal.txt", O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC,
                                S_IRUSR | S_IWUSR);
  if (descriptor >= 0) {
    (void)::write(descriptor, record, length);
    (void)::fsync(descriptor);
    (void)::close(descriptor);
  }
  _exit(128 + signal);
}

void installSignalHandlers() {
  std::signal(SIGINT, stop);
  std::signal(SIGTERM, stop);
  std::signal(SIGABRT, fatalSignal);
  std::signal(SIGBUS, fatalSignal);
  std::signal(SIGFPE, fatalSignal);
  std::signal(SIGILL, fatalSignal);
  std::signal(SIGSEGV, fatalSignal);
}

std::uint64_t monotonicMicros() {
  timespec now{};
  if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return 0;
  return static_cast<std::uint64_t>(now.tv_sec) * 1'000'000ULL + now.tv_nsec / 1'000ULL;
}

MappedInputManager::Button mappedButton(const crossink::kobo::TouchAction action) {
  using Action = crossink::kobo::TouchAction;
  switch (action) {
    case Action::Back:
      return MappedInputManager::Button::Back;
    case Action::Confirm:
      return MappedInputManager::Button::Confirm;
    case Action::Left:
      return MappedInputManager::Button::Left;
    case Action::Right:
      return MappedInputManager::Button::Right;
    case Action::Up:
      return MappedInputManager::Button::Up;
    case Action::Down:
      return MappedInputManager::Button::Down;
    case Action::PageBack:
      return MappedInputManager::Button::PageBack;
    case Action::PageForward:
      return MappedInputManager::Button::PageForward;
    case Action::None:
      break;
  }
  return MappedInputManager::Button::Confirm;
}

void dispatch(const crossink::kobo::TouchDispatch event) {
  if (event.action == crossink::kobo::TouchAction::None) return;
  const auto button = mappedButton(event.action);
  if (event.press) mappedInputManager.injectPress(button);
  if (event.release) mappedInputManager.injectRelease(button);
}

crossink::kobo::ScreenOrientation screenOrientation() {
  using Orientation = crossink::kobo::ScreenOrientation;
  switch (SETTINGS.orientation) {
    case CrossPointSettings::LANDSCAPE_CW:
      return Orientation::LandscapeClockwise;
    case CrossPointSettings::INVERTED:
      return Orientation::Inverted;
    case CrossPointSettings::LANDSCAPE_CCW:
      return Orientation::LandscapeCounterClockwise;
    case CrossPointSettings::PORTRAIT:
    default:
      return Orientation::Portrait;
  }
}

void updateTouch() {
  using crossink::kobo::KoboTouchTransform;
  using crossink::kobo::TouchContext;
  mappedInputManager.clearInjectedInputFrame();

  if (appliedOrientation != SETTINGS.orientation) {
    touch.setOrientation(screenOrientation());
    appliedOrientation = SETTINGS.orientation;
  }
  const bool landscape = screenOrientation() == crossink::kobo::ScreenOrientation::LandscapeClockwise ||
                         screenOrientation() == crossink::kobo::ScreenOrientation::LandscapeCounterClockwise;
  const std::int32_t screenWidth =
      landscape ? KoboTouchTransform::kPortraitHeight : KoboTouchTransform::kPortraitWidth;
  const std::int32_t screenHeight =
      landscape ? KoboTouchTransform::kPortraitWidth : KoboTouchTransform::kPortraitHeight;

  const TouchContext context = mappedInputManager.isReaderMode() ? TouchContext::Reader : TouchContext::Navigation;
  crossink::kobo::TouchFrame frame{};
  bool received = false;
  while (touch.readFrame(frame)) {
    received = true;
    lastTouchFrame = frame;
    haveTouchFrame = true;
    dispatch(gestures.update(frame, context, screenWidth, screenHeight));
  }
  if (!received && haveTouchFrame && lastTouchFrame.down) {
    lastTouchFrame.timestampMicros = monotonicMicros();
    lastTouchFrame.positionChanged = false;
    dispatch(gestures.update(lastTouchFrame, context, screenWidth, screenHeight));
  }
}
}  // namespace

int main() {
  if (std::getenv("CROSSPOINT_SIM_SD") == nullptr) {
    setenv("CROSSPOINT_SIM_SD", "/data", 0);
  }
  installSignalHandlers();
  setup();

  if (!frontlight.discover()) {
    std::fprintf(stderr, "[KOBO] no frontlight backlight device discovered\n");
  }

  crossink::kobo::TouchDeviceInfo touchDevice;
  if (crossink::kobo::KoboEvdevTouch::discover(touchDevice)) {
    if (!touch.open(touchDevice)) {
      std::fprintf(stderr, "[KOBO] failed to open touch input %s\n", touchDevice.path.c_str());
    } else {
      std::fprintf(stderr, "[KOBO] touch input %s (%s), raw x=%d..%d y=%d..%d\n", touchDevice.path.c_str(),
                   touchDevice.name.c_str(), touchDevice.x.minimum, touchDevice.x.maximum, touchDevice.y.minimum,
                   touchDevice.y.maximum);
    }
  } else {
    std::fprintf(stderr, "[KOBO] no absolute touch input discovered\n");
  }

  while (running.load()) {
    if (appliedFrontlight != SETTINGS.frontlightBrightness &&
        frontlight.setPercentage(SETTINGS.frontlightBrightness)) {
      appliedFrontlight = SETTINGS.frontlightBrightness;
    }
    gpio.beginFrame();
    updateTouch();
    loop();
  }
  return EXIT_SUCCESS;
}
