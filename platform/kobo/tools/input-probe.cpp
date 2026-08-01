#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace {

void printAxis(const int fd, const unsigned int code, const char* name) {
  input_absinfo info{};
  if (::ioctl(fd, static_cast<int>(EVIOCGABS(code)), &info) == 0) {
    std::printf("%s code=%u min=%d max=%d fuzz=%d flat=%d resolution=%d\n", name, code, info.minimum, info.maximum,
                info.fuzz, info.flat, info.resolution);
  }
}

}  // namespace

int main(int argc, char** argv) {
  std::string discoveredPath;
  if (argc <= 1) {
    for (unsigned int index = 0; index < 32; ++index) {
      const std::string candidate = "/dev/input/event" + std::to_string(index);
      const int candidateFd = ::open(candidate.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
      if (candidateFd < 0) {
        continue;
      }
      input_absinfo x{};
      input_absinfo y{};
      const bool isMultitouch = ::ioctl(candidateFd, static_cast<int>(EVIOCGABS(ABS_MT_POSITION_X)), &x) == 0 &&
                                ::ioctl(candidateFd, static_cast<int>(EVIOCGABS(ABS_MT_POSITION_Y)), &y) == 0;
      ::close(candidateFd);
      if (isMultitouch) {
        discoveredPath = candidate;
        break;
      }
    }
  }
  const char* path = argc > 1 ? argv[1] : discoveredPath.c_str();
  if (*path == '\0') {
    std::fputs("No multitouch evdev device found\n", stderr);
    return EXIT_FAILURE;
  }
  const int fd = ::open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
  if (fd < 0) {
    std::perror(path);
    return EXIT_FAILURE;
  }
  char name[256]{};
  if (::ioctl(fd, static_cast<int>(EVIOCGNAME(sizeof(name))), name) < 0) {
    std::perror("EVIOCGNAME");
    ::close(fd);
    return EXIT_FAILURE;
  }
  std::printf("device=%s name=%s evdev_record_bytes=%zu libc_input_event_bytes=%zu\n", path, name,
              sizeof(std::int32_t) * 3 + sizeof(std::uint16_t) * 2, sizeof(input_event));
  printAxis(fd, ABS_X, "ABS_X");
  printAxis(fd, ABS_Y, "ABS_Y");
  printAxis(fd, ABS_MT_POSITION_X, "ABS_MT_POSITION_X");
  printAxis(fd, ABS_MT_POSITION_Y, "ABS_MT_POSITION_Y");
  printAxis(fd, ABS_MT_TRACKING_ID, "ABS_MT_TRACKING_ID");
  ::close(fd);
  return EXIT_SUCCESS;
}
