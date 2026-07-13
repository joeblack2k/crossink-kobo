#pragma once

#include <string>
#include <vector>

#include "activities/Activity.h"

// Kobo's equivalent of a compact neofetch screen.  It is deliberately
// read-only: passwords and other credentials never enter this activity.
class DeviceInfoActivity final : public Activity {
 public:
  explicit DeviceInfoActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("DeviceInfo", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  std::vector<std::string> lines;
  void refreshLines();
};
