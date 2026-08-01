#pragma once

#include <string>
#include <vector>

#include "../Activity.h"
#include "OpdsCatalogStore.h"
#include "RecentBooksStore.h"
#include "util/ButtonNavigator.h"

class RecentBooksGridActivity final : public Activity {
 public:
  enum class LibraryView : uint8_t { Recent, Catalog, Series, Collections };
  static constexpr int BOOKS_PER_PAGE = 6;  // Kobo library: 3 cols x 2 rows
  static constexpr int MAX_GRID_BOOKS = BOOKS_PER_PAGE * 2;
  // Generate thumbnails close to their physical N437 rendering size.  The
  // previous X4-sized 123×180 cache looked visibly blocky at 300 ppi.
  static constexpr int COVER_HEIGHT = 450;
  static constexpr int COVER_WIDTH = 300;

  explicit RecentBooksGridActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                   const LibraryView view = LibraryView::Recent)
      : Activity("RecentBooksGrid", renderer, mappedInput), view(view) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool requiresCleanRefreshOnEntry() const override { return true; }

 private:
  struct BookState {
    RecentBook book;
    float progress = -1.0f;
    bool progressLoaded = false;
    bool catalogBook = false;
    OpdsCatalogAvailability availability = OpdsCatalogAvailability::AvailableOffline;
    std::string serverUrl;
    OpdsEntry opdsEntry;
  };
  static constexpr int NO_PAGE_LOADED = -1;

  ButtonNavigator buttonNavigator;
  int selectorIndex = 0;
  bool longPressFired = false;
  bool navigationOverlayOpen = false;
  std::vector<BookState> allRecentBooks;
  std::vector<BookState> recentBooks;
  std::string searchQuery;
  int loadedPageStart = NO_PAGE_LOADED;
  uint32_t observedCatalogChangeSerial = 0;
  LibraryView view;

  void loadRecentBooks();
  void hydrateGroupingMetadata();
  void applySearch();
  void launchLocalSearch();
  void openNavigationOverlay();
  void loadPageCovers(int pageStart);
  void ensureProgressLoaded(int index);
  void reloadAfterBookAction();
  void promptDeleteBook(const RecentBook& book, std::string catalogServerUrl = {}, OpdsEntry catalogEntry = {});
  void promptRemoveBook(const std::string& path, const std::string& title);
  void showBookActionMenu(int bookIndex, bool ignoreInitialConfirmRelease = false);
};
