#pragma once

#include <algorithm>

#include "components/themes/BaseTheme.h"

// Geometry shared by Kobo library surfaces.  The activity owns the data and
// actions; this small value object owns only physical placement so a card's
// paint and touch rectangles cannot drift apart.
struct KoboLibraryLayout final {
  static constexpr int kColumns = 3;
  static constexpr int kRows = 2;
  static constexpr int kCardsPerPage = kColumns * kRows;

  int pageWidth = 0;
  int contentTop = 0;
  // `footerTop` starts the mock-up style pagination band.  The permanent
  // Kobo Back/Select frame below it remains reserved for the gesture layer.
  int footerTop = 0;
  int persistentFooterTop = 0;
  int sidePadding = 0;
  int columnGap = 0;
  int rowGap = 0;
  int cardWidth = 0;
  int coverHeight = 0;
  int metadataHeight = 0;

  [[nodiscard]] static KoboLibraryLayout make(const int width, const int height, const ThemeMetrics& metrics,
                                              const int headerBottom, const int controlsHeight = 0) {
    KoboLibraryLayout layout;
    layout.pageWidth = width;
    layout.sidePadding = std::max(metrics.contentSidePadding, width / 32);
    layout.columnGap = std::max(metrics.verticalSpacing * 2, width / 48);
    layout.rowGap = std::max(metrics.verticalSpacing, height / 80);
    layout.contentTop = headerBottom + metrics.tabBarHeight + controlsHeight + metrics.verticalSpacing * 2;
    const int persistentFooterHeight = std::max(metrics.buttonHintsHeight, height / 14);
    const int paginationHeight = std::max(metrics.buttonHintsHeight, height / 13);
    layout.persistentFooterTop = height - persistentFooterHeight;
    layout.footerTop = layout.persistentFooterTop - paginationHeight;
    const int usableWidth = std::max(1, width - layout.sidePadding * 2 - layout.columnGap * (kColumns - 1));
    layout.cardWidth = usableWidth / kColumns;
    // A library card always reserves exactly three compact metadata baselines:
    // title, author, then progress/read state.  Reserving this area up front
    // is what prevents a long title on the second row from colliding with the
    // pagination bar on the 1448 px N437 panel.
    layout.metadataHeight = std::max(height / 11, metrics.listRowHeight);
    const int rowsHeight = std::max(1, layout.footerTop - layout.contentTop - layout.rowGap);
    const int maxCoverForRows = std::max(1, (rowsHeight - layout.metadataHeight * kRows) / kRows);
    layout.coverHeight = std::max(1, std::min(layout.cardWidth * 3 / 2, maxCoverForRows));
    return layout;
  }

  [[nodiscard]] Rect coverRect(const int slot) const {
    const int row = slot / kColumns;
    const int column = slot % kColumns;
    const int cardX = sidePadding + column * (cardWidth + columnGap);
    // CrossInk caches Kobo covers as 2:3 (3:5) one-bit BMPs.  Keeping that
    // physical ratio avoids the blank right-hand strip produced by the
    // proportional one-bit renderer when a wide card frame was requested.
    const int coverWidth = std::min(cardWidth, coverHeight * 2 / 3);
    return Rect{cardX + (cardWidth - coverWidth) / 2,
                contentTop + row * (coverHeight + metadataHeight + rowGap), coverWidth, coverHeight};
  }

  [[nodiscard]] Rect cardRect(const int slot) const {
    const int row = slot / kColumns;
    const int column = slot % kColumns;
    return Rect{sidePadding + column * (cardWidth + columnGap),
                contentTop + row * (coverHeight + metadataHeight + rowGap), cardWidth,
                coverHeight + metadataHeight};
  }
};
