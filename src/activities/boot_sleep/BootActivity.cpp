#include "BootActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "AppVersion.h"
#include "fontIds.h"
#include "images/Logo120.h"

void BootActivity::onEnter() {
  Activity::onEnter();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  renderer.drawImage(Logo120, (pageWidth - 120) / 2, (pageHeight - 120) / 2, 120, 120);
  renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 70, tr(STR_CROSSINK), true, EpdFontFamily::BOLD);
  renderer.drawCenteredText(SMALL_FONT_ID, pageHeight / 2 + 95, tr(STR_BOOTING));
  // The Kobo accessibility scale can use a substantially taller UI font.
  // Keep the build label within the logical framebuffer instead of relying on
  // the X4-era fixed 30 px bottom offset.
  const int versionY = pageHeight - renderer.getLineHeight(SMALL_FONT_ID) - 4;
  renderer.drawCenteredText(SMALL_FONT_ID, versionY, CROSSINK_VERSION);
  renderer.displayBuffer();
}
