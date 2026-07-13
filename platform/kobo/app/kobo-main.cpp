#include <Arduino.h>
#include <CrossPointSettings.h>
#include <CrossPointState.h>
#include <HalDisplay.h>
#include <HalGPIO.h>
#include <HalStorage.h>
#include <HalSystem.h>
#include <KoboEvdevTouch.h>
#include <KoboRefreshQualification.h>
#include <KoboSysfs.h>
#include <KoboTouchGesture.h>
#include <KoboWebTransferService.h>
#include <MappedInputManager.h>
#include <activities/Activity.h>
#include <activities/ActivityManager.h>
#include <components/TouchUiRegistry.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <ucontext.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <optional>

void setup();
void loop();
extern MappedInputManager mappedInputManager;

namespace {
constexpr char kDevFlag[] = "/boot/flags/CROSSINK_DEV_ENABLE";
constexpr char kDevInputPath[] = "/run/crossink-dev-input";
constexpr char kDevScreenshotPath[] = "/data/.crossink/screenshots/live.pbm";
std::atomic<bool> running{true};
crossink::kobo::KoboEvdevTouch touch;
crossink::kobo::KoboTouchGesture gestures;
crossink::kobo::TouchFrame lastTouchFrame{};
bool haveTouchFrame = false;
crossink::kobo::KoboFrontlightSysfs frontlight;
int appliedFrontlight = -1;
int appliedOrientation = -1;
int devInputDescriptor = -1;
// A Kobo/OPDS path can contain a server id plus a normal human book title.
// Keep the dev-only command channel comfortably above a filesystem pathname
// limit so corpus tooling cannot silently truncate an `open /Books/...` input.
std::array<char, 512> devInputCommand{};
std::size_t devInputCommandLength = 0;
std::optional<MappedInputManager::Button> pendingDevButtonRelease;
std::uintptr_t applicationImageBase = 0;

void stop(int /*signal*/) { running.store(false); }

std::size_t appendHex(char* const destination, std::size_t length, const std::uintptr_t value) {
  constexpr char digits[] = "0123456789abcdef";
  destination[length++] = '0';
  destination[length++] = 'x';
  bool emitted = false;
  for (int shift = static_cast<int>(sizeof(value) * 8) - 4; shift >= 0; shift -= 4) {
    const auto digit = static_cast<unsigned int>((value >> shift) & 0xfU);
    if (digit != 0 || emitted || shift == 0) {
      destination[length++] = digits[digit];
      emitted = true;
    }
  }
  return length;
}

void fatalSignal(const int signal, siginfo_t* const info, void* const context) {
  constexpr char prefix[] = "CrossInk Kobo Beta 1 fatal signal ";
  char record[128]{};
  std::size_t length = sizeof(prefix) - 1;
  std::memcpy(record, prefix, length);
  if (signal >= 10) record[length++] = static_cast<char>('0' + signal / 10);
  record[length++] = static_cast<char>('0' + signal % 10);
  constexpr char addressPrefix[] = " address=";
  std::memcpy(record + length, addressPrefix, sizeof(addressPrefix) - 1);
  length += sizeof(addressPrefix) - 1;
  length = appendHex(record, length, reinterpret_cast<std::uintptr_t>(info != nullptr ? info->si_addr : nullptr));
#if defined(__arm__)
  constexpr char pcPrefix[] = " pc=";
  std::memcpy(record + length, pcPrefix, sizeof(pcPrefix) - 1);
  length += sizeof(pcPrefix) - 1;
  const auto* const ucontext = static_cast<const ucontext_t*>(context);
  const std::uintptr_t pc = ucontext != nullptr ? ucontext->uc_mcontext.arm_pc : 0;
  length = appendHex(record, length, pc);
  constexpr char offsetPrefix[] = " offset=";
  std::memcpy(record + length, offsetPrefix, sizeof(offsetPrefix) - 1);
  length += sizeof(offsetPrefix) - 1;
  length = appendHex(record, length, pc >= applicationImageBase ? pc - applicationImageBase : 0);
  constexpr char lrPrefix[] = " lr=";
  std::memcpy(record + length, lrPrefix, sizeof(lrPrefix) - 1);
  length += sizeof(lrPrefix) - 1;
  const std::uintptr_t lr = ucontext != nullptr ? ucontext->uc_mcontext.arm_lr : 0;
  length = appendHex(record, length, lr);
  constexpr char lrOffsetPrefix[] = " lr_offset=";
  std::memcpy(record + length, lrOffsetPrefix, sizeof(lrOffsetPrefix) - 1);
  length += sizeof(lrOffsetPrefix) - 1;
  length = appendHex(record, length, lr >= applicationImageBase ? lr - applicationImageBase : 0);
#else
  (void)context;
#endif
  record[length++] = '\n';
  const int descriptor =
      ::open("/data/.crossink/crash/last-signal.txt", O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, S_IRUSR | S_IWUSR);
  if (descriptor >= 0) {
    (void)::write(descriptor, record, length);
    (void)::fsync(descriptor);
    (void)::close(descriptor);
  }
  struct sigaction resetAction{};
  resetAction.sa_handler = SIG_DFL;
  sigemptyset(&resetAction.sa_mask);
  (void)::sigaction(signal, &resetAction, nullptr);
  (void)::kill(::getpid(), signal);
  _exit(128 + signal);
}

void installSignalHandlers() {
  Dl_info imageInfo{};
  if (::dladdr(reinterpret_cast<const void*>(&fatalSignal), &imageInfo) != 0) {
    applicationImageBase = reinterpret_cast<std::uintptr_t>(imageInfo.dli_fbase);
  }
  std::signal(SIGINT, stop);
  std::signal(SIGTERM, stop);
  struct sigaction action{};
  action.sa_sigaction = fatalSignal;
  action.sa_flags = SA_SIGINFO | SA_RESETHAND;
  sigemptyset(&action.sa_mask);
  for (const int signal : {SIGABRT, SIGBUS, SIGFPE, SIGILL, SIGSEGV}) {
    (void)::sigaction(signal, &action, nullptr);
  }
}

std::uint64_t monotonicMicros() {
  timespec now{};
  if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return 0;
  return static_cast<std::uint64_t>(now.tv_sec) * 1'000'000ULL + now.tv_nsec / 1'000ULL;
}

MappedInputManager::Button mappedButton(const crossink::kobo::TouchAction action) {
  using Action = crossink::kobo::TouchAction;
  switch (action) {
    case Action::UiItem:
      break;
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

  // A renderer-published hitbox is the authoritative action for the pixels
  // the user touched. In particular, Home places Settings in the left half
  // of the permanent bottom frame; the generic gesture mapper calls that
  // half Back. Resolve the visual target first so a visible button can never
  // be shadowed by the X4-compatible Back/Confirm fallback.
  const auto visualTarget = TOUCH_UI.resolve(event.point.x, event.point.y);
  if (visualTarget.found) {
    if (event.release) {
      std::fprintf(stderr, "[KOBO] touch resolve x=%d y=%d found=1 kind=%u current=%d target=%d count=%d\n",
                   event.point.x, event.point.y, static_cast<unsigned int>(visualTarget.kind),
                   visualTarget.currentIndex, visualTarget.targetIndex, visualTarget.itemCount);
      mappedInputManager.injectTouchTarget(static_cast<unsigned char>(visualTarget.kind), visualTarget.targetIndex,
                                           visualTarget.kind == TouchUiRegistry::TargetKind::NavigationItem
                                               ? visualTarget.currentIndex
                                               : visualTarget.secondaryTarget,
                                           visualTarget.generation, event.point.x, event.point.y);
    }
    return;
  }

  if (event.action == crossink::kobo::TouchAction::UiItem) {
    if (event.release) {
      std::fprintf(stderr, "[KOBO] touch resolve x=%d y=%d found=0\n", event.point.x, event.point.y);
    }
    return;
  }
  const auto button = mappedButton(event.action);
  if (event.press) mappedInputManager.injectPress(button);
  if (event.release) mappedInputManager.injectRelease(button);
}

bool initializeDevInput() {
  if (::access(kDevFlag, F_OK) != 0) return false;
  ::unlink(kDevInputPath);
  if (::mkfifo(kDevInputPath, S_IRUSR | S_IWUSR) != 0) return false;
  devInputDescriptor = ::open(kDevInputPath, O_RDWR | O_NONBLOCK | O_CLOEXEC);
  if (devInputDescriptor < 0) {
    ::unlink(kDevInputPath);
    return false;
  }
  (void)::chmod(kDevInputPath, S_IRUSR | S_IWUSR);
  std::fprintf(stderr, "[KOBO] root-only dev input enabled at %s\n", kDevInputPath);
  return true;
}

void injectButton(const MappedInputManager::Button button) {
  mappedInputManager.injectPress(button);
  // Keep the press visible for one complete application frame.  Injecting a
  // press and release together is unlike real Kobo hardware and causes
  // legacy activities which inspect held/edge state to discard navigation.
  pendingDevButtonRelease = button;
}

[[noreturn]] void reexecDevProcess() {
  // Do not intentionally exit through the supervisor during a corpus run:
  // its early-exit watchdog correctly treats repeated short-lived processes
  // as a crash loop. exec keeps the same healthy process slot while giving
  // CrossInk a complete ordinary startup path and preserved state.
  crossink::kobo::stopWebTransfer();
  HalSystem::restart();
}

bool writeDevScreenshot() {
  // The active N437 renderer owns a DRM dumb buffer, not /dev/fb0.  Export
  // CrossInk's canonical packed mono frame instead, so a screenshot is an
  // exact logical representation of what the renderer submitted to the panel.
  //
  // The render task writes that packed buffer concurrently.  A diagnostic
  // capture without the same lock could splice two frames together (for
  // example a header from one library render and covers from the next), which
  // makes a healthy N437 screen look garbled in the evidence artifact.
  RenderLock renderLock;
  if (::mkdir("/data/.crossink", S_IRWXU) != 0 && errno != EEXIST) {
    std::fprintf(stderr, "[KOBO] screenshot mkdir parent failed: %s\n", std::strerror(errno));
    return false;
  }
  if (::mkdir("/data/.crossink/screenshots", S_IRWXU) != 0 && errno != EEXIST) {
    std::fprintf(stderr, "[KOBO] screenshot mkdir failed: %s\n", std::strerror(errno));
    return false;
  }
  const int descriptor = ::open(kDevScreenshotPath, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, S_IRUSR | S_IWUSR);
  if (descriptor < 0) {
    std::fprintf(stderr, "[KOBO] screenshot open failed: %s\n", std::strerror(errno));
    return false;
  }
  char header[64]{};
  const int headerLength =
      std::snprintf(header, sizeof(header), "P4\n%u %u\n", HalDisplay::DISPLAY_WIDTH, HalDisplay::DISPLAY_HEIGHT);
  const auto writeAll = [descriptor](const void* bytes, std::size_t remaining) {
    const auto* cursor = static_cast<const std::uint8_t*>(bytes);
    while (remaining > 0) {
      const ssize_t written = ::write(descriptor, cursor, remaining);
      if (written <= 0) return false;
      cursor += written;
      remaining -= static_cast<std::size_t>(written);
    }
    return true;
  };
  const std::size_t bufferSize = display.getBufferSize();
  const bool headerOk = headerLength > 0 && writeAll(header, static_cast<std::size_t>(headerLength));
  // Netpbm P4 uses a set bit for black, while CrossInk's packed framebuffer
  // uses a set bit for white. Invert only this diagnostic export; never the
  // panel data. Without this, screenshot review falsely reports a negative
  // UI although the physical display is correct.
  std::array<std::uint8_t, 4096> p4Bytes{};
  bool pixelsOk = headerOk;
  const auto* source = display.getFrameBuffer();
  for (std::size_t offset = 0; pixelsOk && offset < bufferSize; offset += p4Bytes.size()) {
    const std::size_t length = std::min(p4Bytes.size(), bufferSize - offset);
    for (std::size_t index = 0; index < length; ++index)
      p4Bytes[index] = static_cast<std::uint8_t>(~source[offset + index]);
    pixelsOk = writeAll(p4Bytes.data(), length);
  }
  const bool ok = headerOk && pixelsOk;
  if (!ok) {
    std::fprintf(stderr, "[KOBO] screenshot write failed: header=%d pixels=%d bytes=%zu errno=%d (%s)\n",
                 headerOk ? 1 : 0, pixelsOk ? 1 : 0, bufferSize, errno, std::strerror(errno));
  }
  (void)::fsync(descriptor);
  (void)::close(descriptor);
  return ok;
}

bool dispatchDevCommand(const char* command, const crossink::kobo::TouchContext context, const std::int32_t screenWidth,
                        const std::int32_t screenHeight) {
  if (std::strcmp(command, "screenshot") == 0) return writeDevScreenshot();
  // Development-image instrumentation only.  This keeps visual scale tests
  // reproducible without synthesizing a chain of navigation taps or changing
  // the persisted reader state.  The FIFO itself only exists with the explicit
  // CROSSINK_DEV_ENABLE boot flag.
  if (std::strcmp(command, "home") == 0) {
    activityManager.goHome();
    return true;
  }
  if (std::strcmp(command, "recents") == 0) {
    activityManager.goToRecentBooks();
    return true;
  }
  if (std::strcmp(command, "settings") == 0) {
    activityManager.goToSettings();
    return true;
  }
  // Acceptance harness only: persist a known book path and cleanly re-exec
  // the app through the normal supervisor. This is deliberately reachable
  // only through the root-owned FIFO that exists behind CROSSINK_DEV_ENABLE.
  if (std::strncmp(command, "open ", 5) == 0) {
    const char* const path = command + 5;
    if (std::strncmp(path, "/Books/", 7) != 0 || path[7] == '\0' || !Storage.exists(path)) return false;
    APP_STATE.openEpubPath = path;
    APP_STATE.lastSleepFromReader = true;
    APP_STATE.readerActivityLoadCount = 0;
    if (!APP_STATE.saveToFile()) return false;
    reexecDevProcess();
  }
  if (std::strcmp(command, "restart") == 0) {
    // The normal crash-loop guard increments this field before a failed reader
    // boot. A deliberate dev re-exec is neither a crash nor a new reader
    // failure, so preserve the book but reset that sentinel explicitly.
    APP_STATE.lastSleepFromReader = true;
    APP_STATE.readerActivityLoadCount = 0;
    if (!APP_STATE.saveToFile()) return false;
    reexecDevProcess();
  }
  // Acceptance harness only: prove the installed fatal-signal handler and
  // supervisor recovery path on real N437 hardware.  This FIFO exists only
  // when CROSSINK_DEV_ENABLE is explicitly present and is root-owned.
  if (std::strcmp(command, "crash") == 0) {
    std::raise(SIGABRT);
    return false;
  }
  if (std::strcmp(command, "regions") == 0) {
    std::array<TouchUiRegistry::Region, TouchUiRegistry::kMaxRegions> regions{};
    const std::size_t count = TOUCH_UI.snapshot(regions);
    const std::size_t forbiddenOverlaps = TOUCH_UI.forbiddenOverlapCount();
    std::fprintf(stderr, "[KOBO] touch regions count=%zu forbidden_overlaps=%zu\n", count, forbiddenOverlaps);
    for (std::size_t index = 0; index < count; ++index) {
      const auto& region = regions[index];
      std::fprintf(stderr,
                   "[KOBO] touch region index=%zu x=%d y=%d w=%d h=%d kind=%u current=%d target=%d count=%d "
                   "secondary=%d overlap_allowed=%d\n",
                   index, region.x, region.y, region.width, region.height, static_cast<unsigned int>(region.kind),
                   region.currentIndex, region.targetIndex, region.itemCount, region.secondaryTarget,
                   region.overlapAllowed ? 1 : 0);
    }
    return true;
  }
  int x = 0;
  int y = 0;
  if (std::sscanf(command, "tap %d %d", &x, &y) == 2) {
    if (x < 0 || y < 0 || x >= screenWidth || y >= screenHeight) return false;
    const std::uint64_t timestamp = monotonicMicros();
    gestures.reset();
    dispatch(gestures.update({{x, y}, true, true, timestamp, {x, y}}, context, screenWidth, screenHeight));
    dispatch(gestures.update({{x, y}, false, false, timestamp + 1, {x, y}}, context, screenWidth, screenHeight));
    return true;
  }

  struct NamedButton {
    const char* name;
    MappedInputManager::Button button;
  };
  static constexpr std::array<NamedButton, 9> buttons{{
      {"back", MappedInputManager::Button::Back},
      {"confirm", MappedInputManager::Button::Confirm},
      {"up", MappedInputManager::Button::Up},
      {"down", MappedInputManager::Button::Down},
      {"left", MappedInputManager::Button::Left},
      {"right", MappedInputManager::Button::Right},
      {"power", MappedInputManager::Button::Power},
      {"page-back", MappedInputManager::Button::PageBack},
      {"page-forward", MappedInputManager::Button::PageForward},
  }};
  const char* name = std::strncmp(command, "button ", 7) == 0 ? command + 7 : command;
  for (const auto& candidate : buttons) {
    if (std::strcmp(name, candidate.name) == 0) {
      injectButton(candidate.button);
      return true;
    }
  }
  return false;
}

bool processDevInput(const crossink::kobo::TouchContext context, const std::int32_t screenWidth,
                     const std::int32_t screenHeight) {
  if (devInputDescriptor < 0) return false;
  char character = 0;
  while (::read(devInputDescriptor, &character, 1) == 1) {
    if (character == '\n') {
      devInputCommand[devInputCommandLength] = '\0';
      const bool handled = dispatchDevCommand(devInputCommand.data(), context, screenWidth, screenHeight);
      std::fprintf(stderr, "[KOBO] dev input %s: %s\n", handled ? "accepted" : "rejected", devInputCommand.data());
      devInputCommandLength = 0;
      return handled;
    }
    if (character != '\r' && devInputCommandLength + 1 < devInputCommand.size()) {
      devInputCommand[devInputCommandLength++] = character;
    } else if (devInputCommandLength + 1 >= devInputCommand.size()) {
      devInputCommandLength = 0;
    }
  }
  return false;
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
  if (pendingDevButtonRelease.has_value()) {
    mappedInputManager.injectRelease(*pendingDevButtonRelease);
    pendingDevButtonRelease.reset();
  }

  if (appliedOrientation != SETTINGS.orientation) {
    touch.setOrientation(screenOrientation());
    appliedOrientation = SETTINGS.orientation;
  }
  const bool landscape = screenOrientation() == crossink::kobo::ScreenOrientation::LandscapeClockwise ||
                         screenOrientation() == crossink::kobo::ScreenOrientation::LandscapeCounterClockwise;
  const std::int32_t screenWidth = landscape ? KoboTouchTransform::kPortraitHeight : KoboTouchTransform::kPortraitWidth;
  const std::int32_t screenHeight =
      landscape ? KoboTouchTransform::kPortraitWidth : KoboTouchTransform::kPortraitHeight;

  // Reader mode remains enabled while reader child activities (chapter menu,
  // settings, bookmarks) are on the activity stack. That state is too broad
  // to choose touch semantics: a child overlay can briefly have zero published
  // touch regions while it is rendered. Treat only the actual page activity as
  // a reader surface. Every other activity gets navigation semantics, whose
  // bottom-left half is a guaranteed Back escape hatch even if its hit regions
  // are temporarily absent. This prevents a reader menu from becoming a
  // touch-dead end.
  const bool actualReaderPage = activityManager.canSnapshotForSleepOverlay();
  const TouchContext context =
      TOUCH_UI.size() != 0 || !actualReaderPage ? TouchContext::Navigation : TouchContext::Reader;
  if (processDevInput(context, screenWidth, screenHeight)) return;
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

void syncRefreshProfilePreference() {
  const auto requested = SETTINGS.koboRefreshProfile == CrossPointSettings::KOBO_REFRESH_FAST
                             ? crossink::kobo::RefreshProfile::Fast
                             : crossink::kobo::RefreshProfile::Safe;
  const bool qualified = crossink::kobo::koboFastRefreshQualified();
  if (!display.setRefreshProfile(requested, qualified) && requested == crossink::kobo::RefreshProfile::Fast) {
    SETTINGS.koboRefreshProfile = CrossPointSettings::KOBO_REFRESH_SAFE;
    (void)SETTINGS.saveToFile();
    std::fprintf(stderr, "[KOBO][EPD] rejected unqualified Fast profile; restored Safe\n");
  }
}
}  // namespace

int main() {
  if (std::getenv("CROSSPOINT_SIM_SD") == nullptr) {
    setenv("CROSSPOINT_SIM_SD", "/data", 0);
  }
  installSignalHandlers();
  setup();
  (void)initializeDevInput();

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
    syncRefreshProfilePreference();
    if (appliedFrontlight != SETTINGS.frontlightBrightness && frontlight.setPercentage(SETTINGS.frontlightBrightness)) {
      appliedFrontlight = SETTINGS.frontlightBrightness;
    }
    gpio.beginFrame();
    updateTouch();
    loop();
  }
  if (devInputDescriptor >= 0) ::close(devInputDescriptor);
  ::unlink(kDevInputPath);
  return EXIT_SUCCESS;
}
