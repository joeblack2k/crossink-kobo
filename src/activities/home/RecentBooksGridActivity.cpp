#include "RecentBooksGridActivity.h"

#include <Arduino.h>
#include <Bitmap.h>
#include <Epub.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <Xtc.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <string_view>
#include <utility>

#include "BookActions.h"
#include "CrossPointSettings.h"
#include "FileBrowserActionActivity.h"
#include "MappedInputManager.h"
#include "OpdsCatalogStore.h"
#include "OpdsServerStore.h"
#include "RecentBookProgress.h"
#include "RecentBooksStore.h"
#include "activities/browser/OpdsBookBrowserActivity.h"
#include "activities/reader/EpubReaderActivity.h"
#include "activities/util/ConfirmationActivity.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "activities/util/OptionSelectionActivity.h"
#include "components/CompactHeader.h"
#include "components/DirectListTouch.h"
#include "components/KoboIconMetrics.h"
#include "components/KoboLibraryLayout.h"
#include "components/TouchUiRegistry.h"
#include "components/UITheme.h"
#include "components/icons/book.h"
#include "fontIds.h"
#include "network/LocalCoverCache.h"
#include "network/OpdsCoverCache.h"
#include "network/OpdsOfflineSync.h"

namespace {
constexpr int kCoverCornerRadius = 2;
constexpr int kGridColumns = KoboLibraryLayout::kColumns;
constexpr unsigned long kLongPressMs = 1000;
constexpr int kTabRecent = 0;
constexpr int kTabBooks = 1;
constexpr int kTabSeries = 2;
constexpr int kTabCollections = 3;
constexpr int kPagePrevious = 4;
constexpr int kPageNext = 5;
constexpr int kActionSearch = 6;
constexpr int kActionMenu = 7;
constexpr int kNavHome = 20;
constexpr int kNavLibrary = 21;
constexpr int kNavLocalBooks = 22;
constexpr int kNavNetworkLibrary = 23;
constexpr int kNavSettings = 24;
constexpr int kNavDismiss = 25;
constexpr int kActionSort = 26;
constexpr int kActionFilter = 27;
constexpr int kActionSync = 28;

bool containsIgnoreCase(const std::string_view text, const std::string_view query) {
  if (query.empty()) return true;
  if (query.size() > text.size()) return false;
  return std::search(text.begin(), text.end(), query.begin(), query.end(), [](const char lhs, const char rhs) {
           return std::tolower(static_cast<unsigned char>(lhs)) == std::tolower(static_cast<unsigned char>(rhs));
         }) != text.end();
}

std::string collectionFromLibraryPath(const std::string& path) {
  constexpr char booksRoot[] = "/Books/";
  if (path.rfind(booksRoot, 0) != 0) return {};
  const std::size_t segmentStart = sizeof(booksRoot) - 1;
  const std::size_t separator = path.find('/', segmentStart);
  if (separator == std::string::npos || separator == segmentStart) return {};
  return path.substr(segmentStart, separator - segmentStart);
}

float seriesOrder(const std::string& index) {
  if (index.empty()) return 1000000.0f;
  char* end = nullptr;
  const float parsed = std::strtof(index.c_str(), &end);
  return end != index.c_str() ? parsed : 1000000.0f;
}

int moveHorizontalInGrid(const int currentIndex, const int totalItems, const bool moveRight) {
  if (totalItems <= 0) return 0;
  return moveRight ? ButtonNavigator::nextIndex(currentIndex, totalItems)
                   : ButtonNavigator::previousIndex(currentIndex, totalItems);
}

int moveVerticalInGrid(const int currentIndex, const int totalItems, const int columns, const int itemsPerPage,
                       const bool moveDown) {
  if (totalItems <= 0 || columns <= 0) return 0;

  const int safeItemsPerPage = std::max(columns, itemsPerPage);
  // Contract: safeItemsPerPage should describe whole grid rows. Partial rows
  // are allowed only on the final page after totalItems is applied below.
  if (safeItemsPerPage % columns != 0) {
    LOG_ERR("RBGA", "moveVerticalInGrid requires whole rows (itemsPerPage=%d columns=%d)", safeItemsPerPage, columns);
    return currentIndex;
  }
  const int totalPages = (totalItems + safeItemsPerPage - 1) / safeItemsPerPage;
  const int currentPage = currentIndex / safeItemsPerPage;
  const int indexInPage = currentIndex % safeItemsPerPage;
  const int currentRow = indexInPage / columns;
  const int currentColumn = indexInPage % columns;
  const int rowsPerPage = safeItemsPerPage / columns;

  if (moveDown) {
    if (currentRow < rowsPerPage - 1) {
      const int nextRowCandidate = currentIndex + columns;
      if (nextRowCandidate < totalItems && (nextRowCandidate / safeItemsPerPage) == currentPage) {
        return nextRowCandidate;
      }
    }

    const int nextPage = (currentPage + 1) % totalPages;
    const int nextPageStart = nextPage * safeItemsPerPage;
    const int nextPageCount = std::min(safeItemsPerPage, totalItems - nextPageStart);
    if (nextPageCount <= 0) return currentIndex;

    if (currentColumn < nextPageCount) {
      return nextPageStart + currentColumn;
    }
    return nextPageStart + nextPageCount - 1;
  }

  if (currentRow > 0) {
    return currentIndex - columns;
  }

  const int previousPage = (currentPage - 1 + totalPages) % totalPages;
  const int previousPageStart = previousPage * safeItemsPerPage;
  const int previousPageCount = std::min(safeItemsPerPage, totalItems - previousPageStart);
  if (previousPageCount <= 0) return currentIndex;

  int previousPageCandidate = previousPageStart + ((previousPageCount - 1) / columns) * columns + currentColumn;
  while (previousPageCandidate >= previousPageStart + previousPageCount) {
    previousPageCandidate -= columns;
  }
  return std::max(previousPageStart, previousPageCandidate);
}

void updateRecentBookCoverPath(const RecentBook& book, const std::string& coverBmpPath) {
  if (!RECENT_BOOKS.updateBook(book.path, book.title, book.author, coverBmpPath)) {
    LOG_ERR("RBGA", "failed to update recent book metadata: %s", book.path.c_str());
  }
}

bool hasThumbnailPlaceholder(const std::string& coverBmpPath) {
  return coverBmpPath.find("[WIDTH]") != std::string::npos || coverBmpPath.find("[HEIGHT]") != std::string::npos;
}

bool needsCoverThumbGeneration(const RecentBook& book, const std::string& thumbPath) {
  if (thumbPath.empty() || !Storage.exists(thumbPath.c_str())) {
    return true;
  }

  FsFile file;
  if (!Storage.openFileForRead("RBGA", thumbPath, file)) {
    return true;
  }
  Bitmap bitmap(file);
  const bool headerValid = bitmap.parseHeaders() == BmpReaderError::Ok &&
                           bitmap.getWidth() == RecentBooksGridActivity::COVER_WIDTH &&
                           bitmap.getHeight() == RecentBooksGridActivity::COVER_HEIGHT;
  const uint64_t requiredFileSize =
      headerValid ? static_cast<uint64_t>(bitmap.getPixelDataOffset()) + bitmap.getRequiredPixelDataSize() : 0;
  const bool hasExpectedSize = headerValid && static_cast<uint64_t>(file.size()) >= requiredFileSize;
  file.close();
  return !hasExpectedSize;
}

void calculateCoverFillCrop(const Bitmap& bitmap, float& cropX, float& cropY) {
  cropX = 0.0f;
  cropY = 0.0f;
  const float srcW = static_cast<float>(bitmap.getWidth());
  const float srcH = static_cast<float>(bitmap.getHeight());
  if (srcW <= 0.0f || srcH <= 0.0f) return;

  const float srcRatio = srcW / srcH;
  const float targetRatio = static_cast<float>(RecentBooksGridActivity::COVER_WIDTH) /
                            static_cast<float>(RecentBooksGridActivity::COVER_HEIGHT);
  if (srcRatio > targetRatio) {
    cropX = std::max(0.0f, 1.0f - (targetRatio / srcRatio));
  } else if (srcRatio < targetRatio) {
    cropY = std::max(0.0f, 1.0f - (srcRatio / targetRatio));
  }
}

std::string getReusableCoverPath(const RecentBook& book) {
  if (FsHelpers::hasEpubExtension(book.path)) {
    return Epub(book.path, "/.crosspoint").getThumbBmpPath();
  }
  if (FsHelpers::hasXtcExtension(book.path)) {
    return Xtc(book.path, "/.crosspoint").getThumbBmpPath();
  }
  return book.coverBmpPath;
}

void ensureReusableCoverPath(RecentBook& book) {
  if (hasThumbnailPlaceholder(book.coverBmpPath)) {
    return;
  }

  const std::string reusablePath = getReusableCoverPath(book);
  if (reusablePath.empty() || reusablePath == book.coverBmpPath) {
    return;
  }

  book.coverBmpPath = reusablePath;
  updateRecentBookCoverPath(book, reusablePath);
}
}  // namespace

void RecentBooksGridActivity::loadRecentBooks() {
  allRecentBooks.clear();
  recentBooks.clear();
  if (view == LibraryView::Catalog) {
    const auto& servers = OPDS_STORE.getServers();
    if (servers.empty()) {
      applySearch();
      return;
    }
    const auto catalogBooks = OPDS_CATALOG.getBooksForServer(servers.front().id);
    allRecentBooks.reserve(catalogBooks.size());
    for (const auto& catalogBook : catalogBooks) {
      RecentBook book;
      book.path = catalogBook.localPath;
      book.title = catalogBook.title;
      book.author = catalogBook.author;
      book.series = catalogBook.series;
      book.seriesIndex = catalogBook.seriesIndex;
      book.coverBmpPath = catalogBook.coverBmpPath;
      OpdsEntry entry;
      entry.type = OpdsEntryType::BOOK;
      entry.id = catalogBook.entryId;
      entry.title = catalogBook.title;
      entry.author = catalogBook.author;
      entry.href = catalogBook.acquisitionHref;
      entry.coverHref = catalogBook.coverHref;
      allRecentBooks.push_back(BookState{std::move(book), -1.0f, false, true, catalogBook.availability,
                                         servers.front().id, std::move(entry)});
    }
    // Catalog cards are a library, not a viewport cache. Queue every missing
    // remote thumbnail at low priority while the worker remains single-flight;
    // this lets later pages have covers without parsing EPUBs or blocking the
    // current render. request() is idempotent, so re-entering the tab is safe.
    for (const auto& state : allRecentBooks) {
      if (state.book.coverBmpPath.empty() || !Storage.exists(state.book.coverBmpPath.c_str())) {
        OPDS_COVER_CACHE.request(state.serverUrl, state.opdsEntry);
      }
    }
    applySearch();
    return;
  }
  const auto& books = RECENT_BOOKS.getBooks();
  allRecentBooks.reserve(books.size());

  for (const auto& book : books) {
    if (!Storage.exists(book.path.c_str())) continue;
    allRecentBooks.push_back(BookState{book});
  }
  if (view != LibraryView::Recent) hydrateGroupingMetadata();
  applySearch();
}

void RecentBooksGridActivity::hydrateGroupingMetadata() {
  // This runs only for metadata-driven tabs. Old v8 caches are upgraded once;
  // later entries are cheap cached OPF reads. Recent order never changes.
  for (auto& state : allRecentBooks) {
    if (!FsHelpers::hasEpubExtension(state.book.path)) continue;
    Epub epub(state.book.path, "/.crosspoint");
    // Metadata views must upgrade legacy caches on their first visit. A
    // cache-only open returns false after the v9 format bump and silently
    // leaves a partial Series grid; allow the one-time OPF rebuild here.
    if (!epub.load(true, true)) continue;
    const std::string series = epub.getSeries();
    const std::string seriesIndex = epub.getSeriesIndex();
    const std::string collection =
        epub.getCollection().empty() ? collectionFromLibraryPath(state.book.path) : epub.getCollection();
    if (series == state.book.series && seriesIndex == state.book.seriesIndex && collection == state.book.collection) {
      continue;
    }
    state.book.series = series;
    state.book.seriesIndex = seriesIndex;
    state.book.collection = collection;
    if (!RECENT_BOOKS.updateBook(state.book.path, state.book.title, state.book.author, state.book.coverBmpPath, series,
                                 seriesIndex, collection)) {
      LOG_ERR("RBGA", "Failed to persist library grouping metadata: %s", state.book.path.c_str());
    }
  }
}

void RecentBooksGridActivity::applySearch() {
  recentBooks.clear();
  recentBooks.reserve(allRecentBooks.size());
  for (auto state : allRecentBooks) {
    if (containsIgnoreCase(state.book.title, searchQuery) || containsIgnoreCase(state.book.author, searchQuery) ||
        containsIgnoreCase(state.book.series, searchQuery) || containsIgnoreCase(state.book.path, searchQuery)) {
      if (!state.progressLoaded) {
        state.progress = RecentBookProgress::loadPercent(state.book);
        state.progressLoaded = true;
      }
      if (view == LibraryView::Series && state.book.series.empty()) continue;
      if (view == LibraryView::Collections && state.book.collection.empty()) continue;
      const bool hasProgress = RecentBookProgress::hasPercent(state.progress);
      const bool unread = !hasProgress || state.progress <= 0.5f;
      const bool completed = hasProgress && state.progress >= 99.5f;
      if ((SETTINGS.koboLibraryFilter == CrossPointSettings::KOBO_LIBRARY_FILTER_UNREAD && !unread) ||
          (SETTINGS.koboLibraryFilter == CrossPointSettings::KOBO_LIBRARY_FILTER_COMPLETED && !completed)) {
        continue;
      }
      recentBooks.push_back(state);
    }
  }
  if (SETTINGS.koboLibrarySort == CrossPointSettings::KOBO_LIBRARY_SORT_TITLE) {
    std::stable_sort(recentBooks.begin(), recentBooks.end(), [](const BookState& lhs, const BookState& rhs) {
      const std::string_view a = lhs.book.title;
      const std::string_view b = rhs.book.title;
      return std::lexicographical_compare(
          a.begin(), a.end(), b.begin(), b.end(), [](const char left, const char right) {
            return std::tolower(static_cast<unsigned char>(left)) < std::tolower(static_cast<unsigned char>(right));
          });
    });
  }
  if (view == LibraryView::Series && SETTINGS.koboLibrarySort == CrossPointSettings::KOBO_LIBRARY_SORT_RECENT) {
    std::stable_sort(recentBooks.begin(), recentBooks.end(), [](const BookState& lhs, const BookState& rhs) {
      if (lhs.book.series != rhs.book.series) return lhs.book.series < rhs.book.series;
      const float lhsOrder = seriesOrder(lhs.book.seriesIndex);
      const float rhsOrder = seriesOrder(rhs.book.seriesIndex);
      if (lhsOrder != rhsOrder) return lhsOrder < rhsOrder;
      return lhs.book.title < rhs.book.title;
    });
  }
  selectorIndex = 0;
  loadedPageStart = NO_PAGE_LOADED;
}

void RecentBooksGridActivity::launchLocalSearch() {
  // A fresh magnifier tap starts a fresh query.  Carrying a hidden previous
  // term into the keyboard made the no-result state difficult to escape by
  // touch and did not match the reference search affordance.
  startActivityForResult(std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, "Search library"),
                         [this](const ActivityResult& result) {
                           if (result.isCancelled) return;
                           const auto* keyboard = std::get_if<KeyboardResult>(&result.data);
                           if (!keyboard) {
                             LOG_ERR("RBGA", "Local search result missing keyboard text");
                             return;
                           }
                           searchQuery = keyboard->text;
                           applySearch();
                           ensureProgressLoaded(selectorIndex);
                           requestUpdate();
                         });
}

void RecentBooksGridActivity::openNavigationOverlay() {
  navigationOverlayOpen = true;
  requestUpdate();
}

void RecentBooksGridActivity::ensureProgressLoaded(const int index) {
  if (index < 0 || index >= static_cast<int>(recentBooks.size())) return;
  if (recentBooks[index].progressLoaded) {
    return;
  }

  recentBooks[index].progress = RecentBookProgress::loadPercent(recentBooks[index].book);
  recentBooks[index].progressLoaded = true;
}

void RecentBooksGridActivity::loadPageCovers(int pageStart) {
  const int pageEnd = std::min(pageStart + BOOKS_PER_PAGE, static_cast<int>(recentBooks.size()));
  for (int i = pageStart; i < pageEnd; ++i) {
    RecentBook& book = recentBooks[i].book;
    if (recentBooks[i].catalogBook) {
      // The OPDS cover is the canonical fallback for both remote cards and a
      // downloaded EPUB whose embedded cover is unsupported (for example a
      // progressive JPEG).  Never parse an EPUB from this render-side pass:
      // queue only a visible-card OPDS cover job when its durable BMP is
      // absent.
      if (book.coverBmpPath.empty() || !Storage.exists(book.coverBmpPath.c_str())) {
        OPDS_COVER_CACHE.request(recentBooks[i].serverUrl, recentBooks[i].opdsEntry);
      }
      continue;
    }
    ensureReusableCoverPath(book);
    const std::string coverPath =
        book.coverBmpPath.empty() ? "" : UITheme::getCoverThumbPath(book.coverBmpPath, COVER_WIDTH, COVER_HEIGHT);
    if (needsCoverThumbGeneration(book, coverPath)) {
      LOCAL_COVER_CACHE.request(book.path);
    }
  }
  loadedPageStart = pageStart;
}

void RecentBooksGridActivity::onEnter() {
  Activity::onEnter();
  loadRecentBooks();
  selectorIndex = 0;
  loadedPageStart = NO_PAGE_LOADED;
  ensureProgressLoaded(selectorIndex);
  observedCatalogChangeSerial = OPDS_OFFLINE_SYNC.changeSerial();
  requestUpdate();
}

void RecentBooksGridActivity::onExit() {
  Activity::onExit();
  allRecentBooks.clear();
  recentBooks.clear();
}

void RecentBooksGridActivity::loop() {
  LOCAL_COVER_CACHE.tick();
  if (view == LibraryView::Catalog && observedCatalogChangeSerial != OPDS_OFFLINE_SYNC.changeSerial()) {
    // Availability changes are persisted by the sync coordinator.  Rehydrate
    // this view so a successfully downloaded card becomes immediately usable.
    loadRecentBooks();
    selectorIndex = std::min(selectorIndex, std::max(0, static_cast<int>(recentBooks.size()) - 1));
    loadedPageStart = NO_PAGE_LOADED;
    observedCatalogChangeSerial = OPDS_OFFLINE_SYNC.changeSerial();
  }
  bool coverChanged = false;
  OpdsCoverCache::Change coverChange;
  while (OPDS_COVER_CACHE.takeChange(coverChange)) {
    for (auto& state : allRecentBooks) {
      if (!state.catalogBook || state.serverUrl != coverChange.serverId ||
          OpdsCatalogStore::stableBookId(state.opdsEntry) != coverChange.entryId) {
        continue;
      }
      if (coverChange.state == OpdsCoverCache::State::Ready) state.book.coverBmpPath = coverChange.bmpPath;
    }
    for (auto& state : recentBooks) {
      if (!state.catalogBook || state.serverUrl != coverChange.serverId ||
          OpdsCatalogStore::stableBookId(state.opdsEntry) != coverChange.entryId) {
        continue;
      }
      if (coverChange.state == OpdsCoverCache::State::Ready) {
        state.book.coverBmpPath = coverChange.bmpPath;
      }
      coverChanged = true;
    }
  }
  LocalCoverCache::Change localCoverChange;
  while (LOCAL_COVER_CACHE.takeChange(localCoverChange)) {
    if (localCoverChange.state != LocalCoverCache::State::Ready) continue;
    for (auto& state : allRecentBooks) {
      if (state.catalogBook || state.book.path != localCoverChange.bookPath) continue;
      state.book.coverBmpPath = localCoverChange.bmpPath;
      updateRecentBookCoverPath(state.book, localCoverChange.bmpPath);
      coverChanged = true;
    }
    for (auto& state : recentBooks) {
      if (state.catalogBook || state.book.path != localCoverChange.bookPath) continue;
      state.book.coverBmpPath = localCoverChange.bmpPath;
      coverChanged = true;
    }
  }
  if (coverChanged) requestUpdate();
#if defined(SIMULATOR) || defined(KOBO_LINUX)
  // A card is its own direct target: cover, title, author and progress form
  // one generous touch surface, matching what the user sees.
  if (!navigationOverlayOpen) {
    consumeDirectListSelection(mappedInput, static_cast<int>(recentBooks.size()), selectorIndex);
  }

  MappedInputManager::TouchTarget touchTarget;
  if (mappedInput.consumeTouchTarget(touchTarget) &&
      touchTarget.kind == static_cast<unsigned char>(TouchUiRegistry::TargetKind::Tab)) {
    if (navigationOverlayOpen) {
      switch (touchTarget.primary) {
        case kNavHome:
          onGoHome();
          return;
        case kNavLibrary:
        case kNavDismiss:
          navigationOverlayOpen = false;
          requestUpdate();
          return;
        case kNavLocalBooks:
          activityManager.goToFileBrowser();
          return;
        case kNavNetworkLibrary:
          activityManager.goToLibrary();
          return;
        case kNavSettings:
          activityManager.goToSettings();
          return;
        default:
          return;
      }
    }
    const int totalBooks = static_cast<int>(recentBooks.size());
    const int totalPages = std::max(1, (totalBooks + BOOKS_PER_PAGE - 1) / BOOKS_PER_PAGE);
    const int currentPage = totalBooks == 0 ? 0 : selectorIndex / BOOKS_PER_PAGE;
    switch (touchTarget.primary) {
      case kTabRecent:
        activityManager.goToRecentBooks();
        return;
      case kTabBooks:
        activityManager.goToLibrary();
        return;
      case kTabSeries:
        activityManager.goToLibrarySeries();
        return;
      case kTabCollections:
        activityManager.goToLibraryCollections();
        return;
      case kActionSearch:
        launchLocalSearch();
        return;
      case kActionSync:
        if (view == LibraryView::Catalog) {
          OPDS_OFFLINE_SYNC.requestCatalogRefresh();
          requestUpdate();
        }
        return;
      case kActionMenu:
        openNavigationOverlay();
        return;
      case kActionSort:
        SETTINGS.koboLibrarySort =
            static_cast<uint8_t>((SETTINGS.koboLibrarySort + 1) % CrossPointSettings::KOBO_LIBRARY_SORT_COUNT);
        if (!SETTINGS.saveToFile()) LOG_ERR("RBGA", "Failed to persist Kobo library sort");
        applySearch();
        ensureProgressLoaded(selectorIndex);
        requestUpdate();
        return;
      case kActionFilter:
        SETTINGS.koboLibraryFilter =
            static_cast<uint8_t>((SETTINGS.koboLibraryFilter + 1) % CrossPointSettings::KOBO_LIBRARY_FILTER_COUNT);
        if (!SETTINGS.saveToFile()) LOG_ERR("RBGA", "Failed to persist Kobo library filter");
        applySearch();
        ensureProgressLoaded(selectorIndex);
        requestUpdate();
        return;
      case kPagePrevious:
      case kPageNext: {
        if (totalBooks == 0 || totalPages <= 1) return;
        const int delta = touchTarget.primary == kPageNext ? 1 : -1;
        const int destinationPage = (currentPage + delta + totalPages) % totalPages;
        const int destinationStart = destinationPage * BOOKS_PER_PAGE;
        const int destinationCount = std::min(BOOKS_PER_PAGE, totalBooks - destinationStart);
        selectorIndex = destinationStart + std::min(selectorIndex % BOOKS_PER_PAGE, destinationCount - 1);
        ensureProgressLoaded(selectorIndex);
        requestUpdate();
        return;
      }
      default:
        break;
    }
  }
#endif

  if (longPressFired) {
    if (!mappedInput.isPressed(MappedInputManager::Button::Confirm)) {
      longPressFired = false;
    }
    return;
  }

  if (!recentBooks.empty() && selectorIndex >= 0 && selectorIndex < static_cast<int>(recentBooks.size()) &&
      mappedInput.isPressed(MappedInputManager::Button::Confirm) && mappedInput.getHeldTime() >= kLongPressMs) {
    longPressFired = true;
    showBookActionMenu(selectorIndex, true);
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (!recentBooks.empty() && selectorIndex >= 0 && selectorIndex < static_cast<int>(recentBooks.size())) {
      const auto& selected = recentBooks[selectorIndex];
      if (selected.catalogBook && selected.availability != OpdsCatalogAvailability::AvailableOffline) {
        if (selected.availability == OpdsCatalogAvailability::Downloading) {
          // The single-worker coordinator already owns this EPUB. Opening a
          // second direct browser would create a competing `.part` writer.
          LOG_DBG("RBGA", "Ignoring duplicate tap for OPDS download: %s", selected.opdsEntry.title.c_str());
          return;
        }
        const auto& servers = OPDS_STORE.getServers();
        if (servers.empty()) {
          LOG_ERR("RBGA", "No OPDS server configured for catalog download");
          return;
        }
        activityManager.replaceActivity(
            std::make_unique<OpdsBookBrowserActivity>(renderer, mappedInput, servers.front(), selected.opdsEntry));
        return;
      }
      LOG_DBG("RBGA", "Selected recent book: %s", recentBooks[selectorIndex].book.path.c_str());
      onSelectBook(recentBooks[selectorIndex].book.path);
      return;
    }
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (navigationOverlayOpen) {
      navigationOverlayOpen = false;
      requestUpdate();
      return;
    }
    onGoHome();
    return;
  }

  const int listSize = static_cast<int>(recentBooks.size());
  enum class NavDirection { Right, Left, Down, Up };
  auto handleNav = [this, listSize](NavDirection direction) {
    switch (direction) {
      case NavDirection::Right:
        selectorIndex = moveHorizontalInGrid(selectorIndex, listSize, true);
        break;
      case NavDirection::Left:
        selectorIndex = moveHorizontalInGrid(selectorIndex, listSize, false);
        break;
      case NavDirection::Down:
        selectorIndex = moveVerticalInGrid(selectorIndex, listSize, kGridColumns, BOOKS_PER_PAGE, true);
        break;
      case NavDirection::Up:
        selectorIndex = moveVerticalInGrid(selectorIndex, listSize, kGridColumns, BOOKS_PER_PAGE, false);
        break;
    }
    ensureProgressLoaded(selectorIndex);
    requestUpdate();
  };

  buttonNavigator.onRelease({MappedInputManager::Button::Right}, [&] { handleNav(NavDirection::Right); });
  buttonNavigator.onRelease({MappedInputManager::Button::Left}, [&] { handleNav(NavDirection::Left); });
  buttonNavigator.onRelease({MappedInputManager::Button::Down}, [&] { handleNav(NavDirection::Down); });
  buttonNavigator.onRelease({MappedInputManager::Button::Up}, [&] { handleNav(NavDirection::Up); });

  buttonNavigator.onContinuous({MappedInputManager::Button::Right}, [&] { handleNav(NavDirection::Right); });
  buttonNavigator.onContinuous({MappedInputManager::Button::Left}, [&] { handleNav(NavDirection::Left); });
  buttonNavigator.onContinuous({MappedInputManager::Button::Down}, [&] { handleNav(NavDirection::Down); });
  buttonNavigator.onContinuous({MappedInputManager::Button::Up}, [&] { handleNav(NavDirection::Up); });
}

void RecentBooksGridActivity::reloadAfterBookAction() {
  loadRecentBooks();
  if (recentBooks.empty()) {
    selectorIndex = 0;
  } else if (selectorIndex >= static_cast<int>(recentBooks.size())) {
    selectorIndex = static_cast<int>(recentBooks.size()) - 1;
  }
  loadedPageStart = NO_PAGE_LOADED;
  ensureProgressLoaded(selectorIndex);
  requestUpdate(true);
}

void RecentBooksGridActivity::promptDeleteBook(const RecentBook& book, std::string catalogServerUrl,
                                               OpdsEntry catalogEntry) {
  const std::string path = book.path;
  auto handler = [this, path, catalogServerUrl = std::move(catalogServerUrl),
                  catalogEntry = std::move(catalogEntry)](const ActivityResult& res) {
    if (res.isCancelled) {
      LOG_DBG("RBGA", "Delete cancelled");
      return;
    }

    LOG_DBG("RBGA", "Attempting to delete: %s", path.c_str());
    BookActions::clearFileMetadata(path);
    if (!Storage.remove(path.c_str())) {
      LOG_ERR("RBGA", "Failed to delete file: %s", path.c_str());
      return;
    }

    RECENT_BOOKS.removeByPath(path);
    if (!catalogServerUrl.empty()) {
      OPDS_CATALOG.markAvailability(catalogServerUrl, catalogEntry, OpdsCatalogAvailability::RemoteOnly);
    }
    reloadAfterBookAction();
  };

  const std::string heading = tr(STR_DELETE) + std::string("? ");
  startActivityForResult(std::make_unique<ConfirmationActivity>(renderer, mappedInput, heading, book.title),
                         std::move(handler));
}

void RecentBooksGridActivity::promptRemoveBook(const std::string& path, const std::string& title) {
  auto handler = [this, path](const ActivityResult& res) {
    if (res.isCancelled) {
      LOG_DBG("RBGA", "Remove from recents cancelled");
      return;
    }
    if (RECENT_BOOKS.removeByPath(path)) {
      LOG_DBG("RBGA", "Removed from recents: %s", path.c_str());
      reloadAfterBookAction();
    }
  };

  startActivityForResult(
      std::make_unique<ConfirmationActivity>(renderer, mappedInput, tr(STR_REMOVE_FROM_RECENTS), title),
      std::move(handler));
}

void RecentBooksGridActivity::showBookActionMenu(const int bookIndex, const bool ignoreInitialConfirmRelease) {
  if (bookIndex < 0 || bookIndex >= static_cast<int>(recentBooks.size())) return;

  const BookState state = recentBooks[bookIndex];
  const RecentBook book = state.book;
  if (state.catalogBook && state.availability != OpdsCatalogAvailability::AvailableOffline) {
    if (state.availability == OpdsCatalogAvailability::Downloading) return;
    const auto& servers = OPDS_STORE.getServers();
    if (!servers.empty()) {
      activityManager.replaceActivity(
          std::make_unique<OpdsBookBrowserActivity>(renderer, mappedInput, servers.front(), state.opdsEntry));
    }
    return;
  }
  std::vector<FileBrowserActionActivity::MenuItem> items =
      BookActions::buildBookActionItems(book.path, /*includeRemoveFromRecents=*/true);

  startActivityForResult(
      std::make_unique<FileBrowserActionActivity>(renderer, mappedInput, book.title, std::move(items),
                                                  ignoreInitialConfirmRelease),
      [this, book, state](const ActivityResult& result) {
        if (result.isCancelled) {
          return;
        }

        const auto* actionResult = std::get_if<FileBrowserActionResult>(&result.data);
        if (!actionResult) {
          LOG_ERR("RBGA", "Book action result missing");
          return;
        }

        switch (static_cast<FileBrowserAction>(actionResult->action)) {
          case FileBrowserAction::Delete:
            promptDeleteBook(book, state.catalogBook ? state.serverUrl : std::string{},
                             state.catalogBook ? state.opdsEntry : OpdsEntry{});
            return;
          case FileBrowserAction::DeleteCache:
            startActivityForResult(
                std::make_unique<ConfirmationActivity>(
                    renderer, mappedInput, BookActions::confirmationHeading(StrId::STR_DELETE_CACHE), book.title),
                [this, book](const ActivityResult& confirmation) {
                  if (!confirmation.isCancelled) {
                    if (!BookActions::clearBookCache(book.path)) {
                      LOG_ERR("RBGA", "Failed to clear book cache for: %s", book.path.c_str());
                    } else {
                      BookActions::drawToast(renderer, tr(STR_BOOK_CACHE_DELETED));
                      delay(1000);
                    }
                  }
                  reloadAfterBookAction();
                });
            return;
          case FileBrowserAction::DeleteStats:
            startActivityForResult(
                std::make_unique<ConfirmationActivity>(
                    renderer, mappedInput, BookActions::confirmationHeading(StrId::STR_DELETE_BOOK_STATS), book.title),
                [this, book](const ActivityResult& confirmation) {
                  if (!confirmation.isCancelled) {
                    if (!BookActions::deleteBookStats(book.path)) {
                      LOG_ERR("RBGA", "Failed to delete book stats for: %s", book.path.c_str());
                    } else {
                      BookActions::drawToast(renderer, tr(STR_BOOK_STATS_DELETED));
                      delay(1000);
                    }
                  }
                  reloadAfterBookAction();
                });
            return;
          case FileBrowserAction::ResetReaderSettings:
            startActivityForResult(
                std::make_unique<ConfirmationActivity>(
                    renderer, mappedInput, BookActions::confirmationHeading(StrId::STR_RESET_BOOK_READER_SETTINGS),
                    book.title),
                [this, book](const ActivityResult& confirmation) {
                  if (!confirmation.isCancelled) {
                    if (!BookActions::resetBookReaderSettings(book.path)) {
                      LOG_ERR("RBGA", "Failed to reset reader settings for: %s", book.path.c_str());
                    } else {
                      BookActions::drawToast(renderer, tr(STR_BOOK_READER_SETTINGS_RESET));
                      delay(1000);
                    }
                  }
                  reloadAfterBookAction();
                });
            return;
          case FileBrowserAction::ToggleCompleted: {
            bool completed = false;
            if (BookActions::toggleBookCompleted(book.path, book.title, completed)) {
              BookActions::drawToast(renderer, completed ? tr(STR_MARKED_FINISHED) : tr(STR_MARKED_UNFINISHED));
              delay(1000);
            }
            reloadAfterBookAction();
            return;
          }
          case FileBrowserAction::EpubRenderMode: {
            const uint8_t currentIndex =
                BookActions::epubRenderModeDisplayIndex(EpubReaderActivity::loadBookRenderMode(book.path));
            startActivityForResult(
                std::make_unique<OptionSelectionActivity>(renderer, mappedInput, "RecentGridEpubRenderModeSelect",
                                                          StrId::STR_EPUB_RENDER_MODE,
                                                          BookActions::epubRenderModeOptions(), currentIndex),
                [this, book](const ActivityResult& selectionResult) {
                  if (!selectionResult.isCancelled) {
                    const auto* selection = std::get_if<OptionSelectionResult>(&selectionResult.data);
                    if (selection != nullptr &&
                        !EpubReaderActivity::saveBookRenderMode(
                            book.path, BookActions::epubRenderModeForDisplayIndex(selection->index))) {
                      LOG_ERR("RBGA", "Failed to save render mode for: %s", book.path.c_str());
                    }
                  }
                  reloadAfterBookAction();
                });
            return;
          }
          case FileBrowserAction::RemoveFromRecents:
            promptRemoveBook(book.path, book.title);
            return;
          case FileBrowserAction::PinFavorite:
          case FileBrowserAction::UnpinFavorite:
          case FileBrowserAction::SetSleepFolder:
          case FileBrowserAction::ClearSleepFolder:
          case FileBrowserAction::ViewBookmarks:
          case FileBrowserAction::ViewClippings:
          case FileBrowserAction::DeleteBookmarks:
          case FileBrowserAction::DeleteClippings:
            return;
        }
      });
}

void RecentBooksGridActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();

  // The Kobo library deliberately uses the same four visual bands as the
  // approved mock-up: status/app header, tabs, cover grid and pagination.
  // CompactHeader already owns real Wi-Fi/clock/battery rendering.
  const int searchGlyphSize = std::max(28, metrics.tabBarHeight / 2);
  const int searchTouchSize = searchGlyphSize + std::max(16, metrics.verticalSpacing * 2);
  const int headerActionGap = std::max(8, metrics.verticalSpacing);
  const int menuX = metrics.contentSidePadding;
  const int menuY = metrics.topPadding + metrics.batteryBarHeight +
                    std::max(0, (metrics.headerHeight - metrics.batteryBarHeight - searchTouchSize) / 2);
  CompactHeader::drawTitle(renderer, "LIBRARY", false, searchTouchSize + headerActionGap);
  const int headerBottom = CompactHeader::headerBottomY(metrics);
  // The visual action boxes can be larger than the header band at 200%.
  // Keep their direct-touch area inside that band so it never steals the top
  // edge of the first library tab.
  const int headerActionHeight = std::max(1, headerBottom - menuY);
  // A native, high-contrast magnifier rather than a decorative bitmap.  The
  // registered hit rectangle includes finger padding, while the compact glyph
  // stays visually aligned with the reference header.
  const int searchX = pageWidth - metrics.contentSidePadding - metrics.batteryWidth - searchTouchSize - headerActionGap;
  const int syncX = searchX - searchTouchSize - headerActionGap;
  const int searchY = menuY;
  const int lensSize = searchGlyphSize * 3 / 5;
  const int lensX = searchX + (searchTouchSize - lensSize) / 2;
  const int lensY = searchY + (searchTouchSize - lensSize) / 2;
  renderer.drawRoundedRect(lensX, lensY, lensSize, lensSize, 2, lensSize / 2, true);
  renderer.drawLine(lensX + lensSize - 3, lensY + lensSize - 3, lensX + searchGlyphSize, lensY + searchGlyphSize, 3,
                    true);
#ifdef KOBO_LINUX
  TOUCH_UI.registerDirect(menuX, menuY, searchTouchSize, headerActionHeight, TouchUiRegistry::TargetKind::Tab,
                          kActionMenu);
  TOUCH_UI.registerDirect(searchX, searchY, searchTouchSize, headerActionHeight, TouchUiRegistry::TargetKind::Tab,
                          kActionSearch);
  if (view == LibraryView::Catalog) {
    // The header indicator is also a real action. Its shape distinguishes an
    // idle refresh from metadata sync, sequential downloads and a retained
    // failure without spending another row of the dense 300-ppi grid.
    const auto syncStatus = OPDS_OFFLINE_SYNC.status();
    const int radius = std::max(8, searchGlyphSize / 3);
    const int centerX = syncX + searchTouchSize / 2;
    const int centerY = searchY + searchTouchSize / 2;
    if (syncStatus.phase == OpdsOfflineSync::Phase::SyncingMetadata) {
      renderer.drawRoundedRect(centerX - radius, centerY - radius, radius * 2, radius * 2, 2, radius, true);
      renderer.fillRectDither(centerX - radius + 3, centerY - radius + 3, radius * 2 - 6, radius * 2 - 6,
                              Color::LightGray);
    } else if (syncStatus.phase == OpdsOfflineSync::Phase::Downloading) {
      renderer.drawRect(centerX - radius, centerY - radius, radius * 2, radius * 2);
      const int filled = syncStatus.total == 0 ? radius
                                               : std::max(1, (radius * 2 - 4) * static_cast<int>(syncStatus.completed) /
                                                                 static_cast<int>(syncStatus.total));
      renderer.fillRect(centerX - radius + 2, centerY + radius - 2 - filled, radius * 2 - 4, filled);
    } else if (syncStatus.phase == OpdsOfflineSync::Phase::Failed) {
      renderer.drawLine(centerX - radius, centerY - radius, centerX + radius, centerY + radius, 3, true);
      renderer.drawLine(centerX + radius, centerY - radius, centerX - radius, centerY + radius, 3, true);
    } else if (syncStatus.phase == OpdsOfflineSync::Phase::Paused) {
      renderer.fillRect(centerX - radius / 2 - 3, centerY - radius, 4, radius * 2);
      renderer.fillRect(centerX + radius / 2 - 1, centerY - radius, 4, radius * 2);
    } else if (syncStatus.phase == OpdsOfflineSync::Phase::Offline) {
      renderer.drawRoundedRect(centerX - radius, centerY - radius / 2, radius * 2, radius, 2, radius / 2, true);
      renderer.drawLine(centerX - radius, centerY + radius, centerX + radius, centerY - radius, 3, true);
    } else if (syncStatus.lastSuccessMs != 0) {
      renderer.drawRoundedRect(centerX - radius, centerY - radius, radius * 2, radius * 2, 2, radius, true);
      renderer.drawLine(centerX - radius / 2, centerY, centerX - 1, centerY + radius / 2, 2, true);
      renderer.drawLine(centerX - 1, centerY + radius / 2, centerX + radius / 2 + 2, centerY - radius / 2, 2, true);
    } else {
      renderer.drawArc(radius, centerX, centerY, -1, -1, 2, true);
      renderer.drawArc(radius, centerX, centerY, 1, 1, 2, true);
      renderer.drawLine(centerX + radius - 1, centerY - radius / 2, centerX + radius + 5, centerY - radius / 2 - 5, 2,
                        true);
    }
    TOUCH_UI.registerDirect(syncX, searchY, searchTouchSize, headerActionHeight, TouchUiRegistry::TargetKind::Tab,
                            kActionSync);
  }
#endif
  const int menuLineX = menuX + searchTouchSize / 4;
  const int menuLineW = searchTouchSize / 2;
  const int menuLineTop = menuY + searchTouchSize / 3;
  renderer.drawLine(menuLineX, menuLineTop, menuLineX + menuLineW, menuLineTop, 3, true);
  renderer.drawLine(menuLineX, menuLineTop + searchTouchSize / 5, menuLineX + menuLineW,
                    menuLineTop + searchTouchSize / 5, 3, true);
  renderer.drawLine(menuLineX, menuLineTop + (searchTouchSize * 2) / 5, menuLineX + menuLineW,
                    menuLineTop + (searchTouchSize * 2) / 5, 3, true);
  const std::vector<TabInfo> tabs = {{"RECENT", view == LibraryView::Recent},
                                     {"BOOKS", view == LibraryView::Catalog},
                                     {"SERIES", view == LibraryView::Series},
                                     {"COLLECTIONS", view == LibraryView::Collections}};
  GUI.drawTabBar(renderer, Rect{0, headerBottom, pageWidth, metrics.tabBarHeight}, tabs, false);
  const int controlsY = headerBottom + metrics.tabBarHeight + std::max(2, metrics.verticalSpacing / 2);
  const int controlsH = std::max(36, metrics.verticalSpacing * 3);
  const int controlsGap = std::max(8, metrics.verticalSpacing);
  const int controlsW = (pageWidth - metrics.contentSidePadding * 2 - controlsGap) / 2;
  const int sortX = metrics.contentSidePadding;
  const int filterX = sortX + controlsW + controlsGap;
  const char* sortValue = SETTINGS.koboLibrarySort == CrossPointSettings::KOBO_LIBRARY_SORT_TITLE ? "A-Z" : "Recent";
  const char* filterValue = "All";
  if (SETTINGS.koboLibraryFilter == CrossPointSettings::KOBO_LIBRARY_FILTER_UNREAD) filterValue = "Unread";
  if (SETTINGS.koboLibraryFilter == CrossPointSettings::KOBO_LIBRARY_FILTER_COMPLETED) filterValue = "Finished";
  const auto drawControl = [&](const int x, const char* label, const char* value) {
    renderer.drawRoundedRect(x, controlsY, controlsW, controlsH, 1, 5, true);
    const std::string text = std::string(label) + ": " + value + " v";
    renderer.drawText(SMALL_FONT_ID, x + metrics.verticalSpacing,
                      controlsY + (controlsH - renderer.getLineHeight(SMALL_FONT_ID)) / 2, text.c_str());
  };
  drawControl(sortX, "Sort", sortValue);
  drawControl(filterX, "Filter", filterValue);
#ifdef KOBO_LINUX
  TOUCH_UI.registerDirect(sortX, controlsY, controlsW, controlsH, TouchUiRegistry::TargetKind::Tab, kActionSort);
  TOUCH_UI.registerDirect(filterX, controlsY, controlsW, controlsH, TouchUiRegistry::TargetKind::Tab, kActionFilter);
#endif
  const KoboLibraryLayout layout = KoboLibraryLayout::make(pageWidth, pageHeight, metrics, headerBottom,
                                                           controlsH + std::max(2, metrics.verticalSpacing / 2));
  constexpr int selectionPadding = 4;
  constexpr int selectionOutlineGap = 2;
  constexpr int selectionOuterInset = selectionPadding + selectionOutlineGap;

  const int totalBooks = static_cast<int>(recentBooks.size());
  const int totalPages = std::max(1, (totalBooks + BOOKS_PER_PAGE - 1) / BOOKS_PER_PAGE);
  const int currentPage = (totalBooks > 0) ? (selectorIndex / BOOKS_PER_PAGE) : 0;
  const int pageStart = currentPage * BOOKS_PER_PAGE;
  const int pageCount = std::min(BOOKS_PER_PAGE, totalBooks - pageStart);

  if (recentBooks.empty()) {
    // The Kobo library chrome is deliberately Dutch to match the approved
    // product mock-up, independent from upstream CrossInk's content language.
    const char* emptyMessage =
        searchQuery.empty()
            ? (view == LibraryView::Series
                   ? "No series found"
                   : (view == LibraryView::Collections
                          ? "No collections found"
                          : (view == LibraryView::Catalog ? "No cached OPDS books" : "No recent books")))
            : "No books found";
    renderer.drawText(UI_10_FONT_ID, layout.sidePadding, layout.contentTop + 20, emptyMessage);
  } else {
    for (int i = 0; i < pageCount; ++i) {
      const int bookIdx = pageStart + i;
      const Rect coverRect = layout.coverRect(i);
      const Rect cardRect = layout.cardRect(i);
      const int coverX = coverRect.x;
      const int coverY = coverRect.y;
      const int coverWidth = coverRect.width;
      const int coverHeight = coverRect.height;
      const int metadataX = cardRect.x;
      const int metadataWidth = cardRect.width;
      bool drawn = false;
      const bool remoteOnly = recentBooks[bookIdx].catalogBook &&
                              recentBooks[bookIdx].availability != OpdsCatalogAvailability::AvailableOffline;
      const std::string thumbPath =
          recentBooks[bookIdx].book.coverBmpPath.empty()
              ? ""
              : UITheme::getCoverThumbPath(recentBooks[bookIdx].book.coverBmpPath, COVER_WIDTH, COVER_HEIGHT);
      if (!thumbPath.empty() && Storage.exists(thumbPath.c_str())) {
        FsFile file;
        if (Storage.openFileForRead("RBGA", thumbPath, file)) {
          Bitmap bmp(file);
          if (bmp.parseHeaders() == BmpReaderError::Ok && bmp.getWidth() > 0 && bmp.getHeight() > 0) {
            float cropX = 0.0f;
            float cropY = 0.0f;
            calculateCoverFillCrop(bmp, cropX, cropY);
            renderer.fillRoundedRect(coverX, coverY, coverWidth, coverHeight, kCoverCornerRadius, Color::White);
            renderer.drawBitmap(bmp, coverX, coverY, coverWidth, coverHeight, cropX, cropY);
            renderer.maskRoundedRectOutsideCorners(coverX, coverY, coverWidth, coverHeight, kCoverCornerRadius,
                                                   Color::White);
            if (remoteOnly) {
              // A cached remote cover must retain the same unmistakably light
              // availability state as its placeholder. A deterministic mono
              // dither overlay is stable on Pearl and avoids alpha artefacts.
              renderer.fillRectDither(coverX + 2, coverY + 2, std::max(1, coverWidth - 4), std::max(1, coverHeight - 4),
                                      Color::White);
            }
            renderer.drawRoundedRect(coverX, coverY, coverWidth, coverHeight, 2, kCoverCornerRadius, true);
            drawn = true;
          }
          file.close();
        }
      }
      if (!drawn) {
        // Remote-only catalog books use the same deterministic mono dither as
        // disabled controls. It is visibly lighter on Pearl without alpha or
        // a grayscale buffer, and a full repaint removes it cleanly after a
        // successful download.
        renderer.fillRoundedRect(coverX, coverY, coverWidth, coverHeight, kCoverCornerRadius,
                                 remoteOnly ? Color::LightGray : Color::White);
        renderer.drawRoundedRect(coverX, coverY, coverWidth, coverHeight, 2, kCoverCornerRadius, true);
        constexpr int sourceIconSize = 32;
        const int iconSize = KoboIconMetrics::coverPlaceholderSize(sourceIconSize, coverWidth, coverHeight);
        KoboIconMetrics::drawScaledSquare(renderer, BookIcon, coverX + (coverWidth - iconSize) / 2,
                                          coverY + (coverHeight - iconSize) / 2, sourceIconSize, iconSize);
      }
      if (bookIdx == static_cast<int>(selectorIndex)) {
        renderer.drawRoundedRect(coverX - selectionPadding, coverY - selectionPadding,
                                 coverWidth + selectionPadding * 2, coverHeight + selectionPadding * 2, 3,
                                 kCoverCornerRadius + selectionPadding, true);
        renderer.drawRoundedRect(coverX - selectionOuterInset, coverY - selectionOuterInset,
                                 coverWidth + selectionOuterInset * 2, coverHeight + selectionOuterInset * 2, 1,
                                 kCoverCornerRadius + selectionOuterInset, true);
      }

#ifdef KOBO_LINUX
      TOUCH_UI.registerItem(cardRect.x, cardRect.y, cardRect.width, cardRect.height, selectorIndex, bookIdx,
                            totalBooks);
#endif
      const int metadataY = coverY + coverHeight + std::max(2, metrics.verticalSpacing / 2);
      // The physical grid has room for two rows only.  A deliberately single
      // title baseline preserves the visual rhythm of the reference library
      // and guarantees that author/status never draw into the pagination bar.
      const auto title = renderer.truncatedText(SMALL_FONT_ID, recentBooks[bookIdx].book.title.c_str(), metadataWidth);
      int textY = metadataY;
      renderer.drawText(SMALL_FONT_ID, metadataX, textY, title.c_str(), true, EpdFontFamily::BOLD);
      textY += renderer.getLineHeight(SMALL_FONT_ID);
      const auto author =
          renderer.truncatedText(SMALL_FONT_ID, recentBooks[bookIdx].book.author.c_str(), metadataWidth);
      if (!author.empty()) {
        renderer.drawText(SMALL_FONT_ID, metadataX, textY, author.c_str());
        textY += renderer.getLineHeight(SMALL_FONT_ID);
      }
      const bool hasProgress =
          recentBooks[bookIdx].progressLoaded && RecentBookProgress::hasPercent(recentBooks[bookIdx].progress);
      if (hasProgress) {
        const int progressHeight = std::max(4, metrics.progressBarHeight / 2);
        // The general progress component renders a large percentage caption
        // below the bar.  That is useful in a settings page, but it would
        // overlap the next row in this dense library grid.  Cards keep the
        // visual cue compact, like the reference mock-up.
        const Rect progressRect{metadataX, textY + 2, metadataWidth, progressHeight};
        renderer.drawRect(progressRect.x, progressRect.y, progressRect.width, progressRect.height);
        const int filledWidth =
            std::max(0, (progressRect.width - 4) * static_cast<int>(recentBooks[bookIdx].progress) / 100);
        if (filledWidth > 0) {
          renderer.fillRect(progressRect.x + 2, progressRect.y + 2, filledWidth, std::max(1, progressRect.height - 4));
        }
      } else {
        const auto availability = recentBooks[bookIdx].availability;
        const char* state = "Unread";
        if (recentBooks[bookIdx].catalogBook) {
          state = availability == OpdsCatalogAvailability::DownloadFailed ? "Retry download" : "Download";
        }
        renderer.drawText(SMALL_FONT_ID, metadataX, textY, state);
      }
    }
  }

  const int footerHeight = layout.persistentFooterTop - layout.footerTop;
  const int buttonGap = std::max(8, metrics.contentSidePadding / 2);
  const int buttonWidth = (pageWidth - layout.sidePadding * 2 - buttonGap * 2) / 3;
  const int previousX = layout.sidePadding;
  const int pageX = previousX + buttonWidth + buttonGap;
  const int nextX = pageX + buttonWidth + buttonGap;
  const int buttonY = layout.footerTop + std::max(2, footerHeight / 7);
  const int buttonHeight = std::max(1, footerHeight - std::max(4, footerHeight / 5));
  const bool canPage = totalPages > 1;
  const auto drawFooterButton = [&](const int x, const char* label, const bool enabled) {
    renderer.drawRoundedRect(x, buttonY, buttonWidth, buttonHeight, 1, 6, true);
    if (!enabled) renderer.fillRectDither(x + 2, buttonY + 2, buttonWidth - 4, buttonHeight - 4, Color::LightGray);
    const int textX = x + (buttonWidth - renderer.getTextWidth(SMALL_FONT_ID, label)) / 2;
    renderer.drawText(SMALL_FONT_ID, textX, buttonY + (buttonHeight - renderer.getLineHeight(SMALL_FONT_ID)) / 2,
                      label);
  };
  drawFooterButton(previousX, "< Previous", canPage);
  const std::string pageLabel = "Page " + std::to_string(currentPage + 1) + " of " + std::to_string(totalPages);
  renderer.drawText(SMALL_FONT_ID, pageX + (buttonWidth - renderer.getTextWidth(SMALL_FONT_ID, pageLabel.c_str())) / 2,
                    buttonY + (buttonHeight - renderer.getLineHeight(SMALL_FONT_ID)) / 2, pageLabel.c_str());
  drawFooterButton(nextX, "Next >", canPage);
#ifdef KOBO_LINUX
  if (canPage) {
    TOUCH_UI.registerDirect(previousX, buttonY, buttonWidth, buttonHeight, TouchUiRegistry::TargetKind::Tab,
                            kPagePrevious);
    TOUCH_UI.registerDirect(nextX, buttonY, buttonWidth, buttonHeight, TouchUiRegistry::TargetKind::Tab, kPageNext);
  }
#endif

  // Keep the device-wide two-button frame visible and outside the pagination
  // hitboxes.  KoboTouchGesture owns this final band as Back/Select.
  const auto labels = mappedInput.mapLabels("Back", "Open", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  if (navigationOverlayOpen) {
#ifdef KOBO_LINUX
    // This overlay is modal. The library grid/header was drawn first and
    // published its direct targets, but none of those may remain reachable
    // beneath the opaque panel or its outside-dismiss scrim.
    TOUCH_UI.clear();
#endif
    const int panelWidth = pageWidth * 3 / 4;
    const int panelTop = metrics.topPadding;
    const int panelBottom = layout.persistentFooterTop;
    renderer.fillRectDither(panelWidth, panelTop, pageWidth - panelWidth, panelBottom - panelTop, Color::LightGray);
    renderer.fillRect(0, panelTop, panelWidth, panelBottom - panelTop, false);
    renderer.drawLine(panelWidth, panelTop, panelWidth, panelBottom, 3, true);
    renderer.drawText(UI_12_FONT_ID, layout.sidePadding, panelTop + metrics.batteryBarHeight + metrics.verticalSpacing,
                      "MENU", true, EpdFontFamily::BOLD);
    const std::array<std::pair<const char*, int>, 5> entries = {{{"Home", kNavHome},
                                                                 {"Library", kNavLibrary},
                                                                 {"Local files", kNavLocalBooks},
                                                                 {"OPDS Library", kNavNetworkLibrary},
                                                                 {"Settings", kNavSettings}}};
    const int rowHeight = std::max(metrics.listRowHeight, searchTouchSize + metrics.verticalSpacing);
    const int rowX = layout.sidePadding;
    const int rowW = panelWidth - layout.sidePadding * 2;
    int rowY = panelTop + metrics.batteryBarHeight + metrics.tabBarHeight;
    for (const auto& entry : entries) {
      renderer.drawRoundedRect(rowX, rowY, rowW, rowHeight, 1, 5, true);
      renderer.drawText(SMALL_FONT_ID, rowX + metrics.verticalSpacing,
                        rowY + (rowHeight - renderer.getLineHeight(SMALL_FONT_ID)) / 2, entry.first);
#ifdef KOBO_LINUX
      TOUCH_UI.registerDirect(rowX, rowY, rowW, rowHeight, TouchUiRegistry::TargetKind::Tab, entry.second);
#endif
      rowY += rowHeight + metrics.verticalSpacing;
    }
#ifdef KOBO_LINUX
    TOUCH_UI.registerDirect(panelWidth, panelTop, pageWidth - panelWidth, panelBottom - panelTop,
                            TouchUiRegistry::TargetKind::Tab, kNavDismiss);
#endif
  }

  renderer.displayBuffer();

  if (!recentBooks.empty() && loadedPageStart != pageStart) {
    loadPageCovers(pageStart);
  }
}
