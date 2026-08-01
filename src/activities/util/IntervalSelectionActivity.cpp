#include "IntervalSelectionActivity.h"

#include <GfxRenderer.h>
#include <HalGPIO.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <utility>

#include "components/TouchSlider.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "platform/DeviceCapabilities.h"

namespace {
void formatCompactSeconds(const int seconds, char* buf, const size_t len) {
  if (seconds < 60) {
    snprintf(buf, len, "%ds", seconds);
  } else if (seconds % 60 == 0) {
    snprintf(buf, len, "%dm", seconds / 60);
  } else {
    snprintf(buf, len, "%dm %ds", seconds / 60, seconds % 60);
  }
}
}  // namespace

int IntervalSelectionActivity::clampedValue(const int candidate) const {
  return std::clamp(candidate, minValue, maxValue);
}

void IntervalSelectionActivity::onEnter() {
  Activity::onEnter();
  value = clampedValue(value);
  requestUpdate();
}

void IntervalSelectionActivity::adjustValue(const int delta) {
  value = clampedValue(value + delta);
  requestUpdate();
}

void IntervalSelectionActivity::drawStepHintLine(const int y, const StrId labelId, const int step) {
  char stepText[24];
  if (valueFormatId != StrId::STR_NONE_OPT) {
    snprintf(stepText, sizeof(stepText), I18N.get(valueFormatId), static_cast<unsigned int>(step));
  } else {
    snprintf(stepText, sizeof(stepText), "%d", step);
  }
  char line[64];
  snprintf(line, sizeof(line), "%s %s", I18N.get(labelId), stepText);
  renderer.drawCenteredText(SMALL_FONT_ID, y, line, true);
}

void IntervalSelectionActivity::loop() {
  if (ignoreConfirmRelease) {
    const bool confirmReleased = mappedInput.wasReleased(MappedInputManager::Button::Confirm);
    if (confirmReleased || !mappedInput.isPressed(MappedInputManager::Button::Confirm)) {
      ignoreConfirmRelease = false;
      return;
    }
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    ActivityResult result;
    result.isCancelled = true;
    setResult(std::move(result));
    finish();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    setResult(IntervalResult{static_cast<uint32_t>(value)});
    finish();
    return;
  }

#if defined(SIMULATOR) || defined(KOBO_LINUX)
  if (TouchSlider::consume(mappedInput, minValue, maxValue, value)) {
    requestUpdate();
    return;
  }
#endif

  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Left}, [this] { adjustValue(-smallStep); });
  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Right}, [this] { adjustValue(smallStep); });

  // On X3 the side buttons sit on the left/right edges of the screen rather than as a vertical up/down
  // rocker (X4), so BTN_UP is physically the left button and BTN_DOWN the right one. Flip the large-step
  // direction there so the left button decreases and the right button increases, matching the layout.
  const auto capabilities = crossink::platform::deviceCapabilities(gpio);
  const int upDelta = capabilities.sideButtonsAreHorizontal ? -largeStep : largeStep;
  const int downDelta = capabilities.sideButtonsAreHorizontal ? largeStep : -largeStep;
  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Up}, [this, upDelta] { adjustValue(upDelta); });
  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Down},
                                       [this, downDelta] { adjustValue(downDelta); });
}

void IntervalSelectionActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect screen = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  const int screenWidth = screen.width;

  GUI.drawHeader(renderer, Rect{screen.x, screen.y + metrics.topPadding, screenWidth, metrics.headerHeight},
                 I18N.get(titleId), nullptr, readerActivity);

  char formattedValue[32];
  if (maxBoundaryLabelId != StrId::STR_NONE_OPT && value == maxValue) {
    snprintf(formattedValue, sizeof(formattedValue), "%s", I18N.get(maxBoundaryLabelId));
  } else if (showPercentValue) {
    snprintf(formattedValue, sizeof(formattedValue), "%d%%", value);
  } else if (valueFormatId == StrId::STR_SECONDS_VALUE_FORMAT) {
    formatCompactSeconds(value, formattedValue, sizeof(formattedValue));
  } else if (valueFormatId != StrId::STR_NONE_OPT) {
    snprintf(formattedValue, sizeof(formattedValue), I18N.get(valueFormatId), static_cast<unsigned int>(value));
  } else {
    snprintf(formattedValue, sizeof(formattedValue), "%d", value);
  }
  const int valueY = screen.y + metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  renderer.drawCenteredText(UI_12_FONT_ID, valueY, formattedValue, true, EpdFontFamily::BOLD);

  const int barWidth = std::max(1, screen.width - metrics.contentSidePadding * 2);
  const int barHeight = std::max(16, renderer.getLineHeight(UI_10_FONT_ID));
  const int barX = screen.x + metrics.contentSidePadding;
  const int barY = valueY + renderer.getLineHeight(UI_12_FONT_ID) + metrics.verticalSpacing;

  renderer.drawRect(barX, barY, barWidth, barHeight);

  const int range = std::max(1, maxValue - minValue);
  const int fillWidth = (barWidth - 4) * (value - minValue) / range;
  if (fillWidth > 0) {
    renderer.fillRect(barX + 2, barY + 2, fillWidth, barHeight - 4);
  }

  const int knobX = std::max(barX + 2, barX + 2 + fillWidth - 2);
  renderer.fillRect(knobX, barY - 4, 4, barHeight + 8, true);

#ifdef KOBO_LINUX
  // One-minute cells would be too small for a finger.  Publish large-step
  // cells instead: a tap picks the nearest useful timeout directly, while
  // physical/keyboard left/right remains available for the upstream's exact
  // one-minute adjustment semantics.
  const int segmentCount = TouchSlider::segmentCount(minValue, maxValue, largeStep);
  TouchSlider::registerSegments(barX, barY, barWidth, barHeight, minValue, maxValue, largeStep);
  for (int segment = 0; segment < segmentCount; ++segment) {
    const int segmentX = barX + (segment * barWidth) / segmentCount;
    if (segment != 0) {
      renderer.drawLine(segmentX, barY + 2, segmentX, barY + barHeight - 3, true);
    }
  }
#else
  // Two-line step hint: front buttons do the small step, side buttons the large step. Built from
  // separate label + value strings (rather than splitting one localized sentence) so the layout
  // doesn't depend on translators preserving a hidden separator.
  drawStepHintLine(barY + 30, StrId::STR_STEP_HINT_FRONT, smallStep);
  drawStepHintLine(barY + 52, StrId::STR_STEP_HINT_SIDE, largeStep);
#endif

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), "-", "+");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4, readerActivity);

  renderer.displayBuffer();
}
