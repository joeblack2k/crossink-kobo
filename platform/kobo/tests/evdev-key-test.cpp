#include <linux/input.h>

#include <cstdlib>

#include "KoboEvdevKey.h"

using crossink::kobo::KoboEvdevKey;

int main() {
  KoboEvdevKey key;
  key.beginFrame();
  key.ingest(EV_KEY, KEY_POWER, 1, 1'000'000);
  if (!key.isPressed() || !key.wasPressed() || key.wasReleased()) return EXIT_FAILURE;
  key.beginFrame();
  key.ingest(EV_KEY, KEY_POWER, 2, 1'500'000);
  if (!key.isPressed() || key.wasPressed() || key.heldMilliseconds() != 500) return EXIT_FAILURE;
  key.ingest(EV_KEY, KEY_POWER, 0, 1'600'000);
  if (key.isPressed() || !key.wasReleased()) return EXIT_FAILURE;
  return EXIT_SUCCESS;
}
