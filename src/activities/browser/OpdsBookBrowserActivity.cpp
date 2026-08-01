#include "OpdsBookBrowserActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>
#include <OpdsStream.h>
#include <WiFi.h>

#include <algorithm>
#include <cctype>

#include "MappedInputManager.h"
#include "OpdsCatalogStore.h"
#include "SdCardFontSystem.h"
#include "SilentRestart.h"
#include "activities/network/WifiSelectionActivity.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/DirectListTouch.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "network/OpdsBookStorage.h"
#include "network/OpdsEpubValidator.h"
#include "network/OpdsOfflineSync.h"
#include "network/OpdsSyncService.h"
#include "util/BookCacheUtils.h"
#include "util/UrlUtils.h"

namespace {
constexpr size_t OPDS_BROWSER_ENTRY_CAPACITY = MAX_OPDS_FEED_ENTRIES + 2;
}  // namespace

void OpdsBookBrowserActivity::onEnter() {
  Activity::onEnter();

  sdFontSystem.releaseLoadedFont(renderer);

  state = BrowserState::CHECK_WIFI;
  entryCount = 0;
  navigationHistory.clear();
  searchTemplate = "";
  currentPath = "";
  selectorIndex = 0;
  consumeConfirm = false;
  consumeBack = false;
  errorMessage.clear();
  statusMessage = tr(STR_CHECKING_WIFI);
  requestUpdate();

  if (!ensureEntryBuffer()) {
    state = BrowserState::ERROR;
    errorMessage = tr(STR_MEMORY_ERROR);
    requestUpdate();
    return;
  }

  checkAndConnectWifi();
}

void OpdsBookBrowserActivity::onExit() {
  Activity::onExit();
  clearEntries();
  entries.reset();
  navigationHistory.clear();

  // The Kobo platform keeps a known STA connection alive across activities.
#ifndef KOBO_LINUX
  if (WiFi.getMode() != WIFI_MODE_NULL) {
    WiFi.disconnect(false);
    delay(30);
    silentRestart();
  }
#endif
}

void OpdsBookBrowserActivity::loop() {
  processBackgroundJob();
  if (state == BrowserState::WIFI_SELECTION || state == BrowserState::SEARCH_INPUT) {
    return;
  }

  if (consumeConfirm && mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    consumeConfirm = false;
    return;
  }
  if (consumeBack && mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    consumeBack = false;
    return;
  }

  if (state == BrowserState::ERROR) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
        showLoadingBeforeFetch();
        fetchFeed(currentPath);
      } else {
        launchWifiSelection();
      }
    } else if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      navigateBack();
    }
    return;
  }

  if (state == BrowserState::CHECK_WIFI || state == BrowserState::LOADING) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      state == BrowserState::CHECK_WIFI ? onGoHome() : navigateBack();
    }
    return;
  }

  if (state == BrowserState::DOWNLOADING) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) OPDS_SYNC.cancel(activeJobId);
    return;
  }

  if (state == BrowserState::BROWSING) {
    // The original X3/X4 browser only moved a focus cursor through tiny,
    // fixed-height text rows.  Kobo renders this activity as a normal list,
    // publishes each visible row and activates the exact touched entry.
    consumeDirectListSelection(mappedInput, static_cast<int>(entryCount), selectorIndex);

    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      if (entryCount > 0) {
        const auto& entry = entries[selectorIndex];
        entry.type == OpdsEntryType::BOOK ? downloadBook(entry) : navigateToEntry(entry);
      }
    } else if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      navigateBack();
    } else if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
      if (!searchTemplate.empty() && selectorIndex == 0) launchSearch();
    }

    if (entryCount > 0) {
      buttonNavigator.onNextRelease([this] {
        selectorIndex = ButtonNavigator::nextIndex(selectorIndex, entryCount);
        requestUpdate();
      });
      buttonNavigator.onPreviousRelease([this] {
        selectorIndex = ButtonNavigator::previousIndex(selectorIndex, entryCount);
        requestUpdate();
      });
      buttonNavigator.onNextContinuous([this] {
        selectorIndex = ButtonNavigator::nextPageIndex(selectorIndex, entryCount, visibleItemCount());
        requestUpdate();
      });
      buttonNavigator.onPreviousContinuous([this] {
        selectorIndex = ButtonNavigator::previousPageIndex(selectorIndex, entryCount, visibleItemCount());
        requestUpdate();
      });
    }
  }
}

bool OpdsBookBrowserActivity::preventAutoSleep() {
  switch (state) {
    case BrowserState::CHECK_WIFI:
    case BrowserState::WIFI_SELECTION:
    case BrowserState::LOADING:
    case BrowserState::DOWNLOADING:
    case BrowserState::SEARCH_INPUT:
      return true;
    case BrowserState::BROWSING:
    case BrowserState::ERROR:
      return false;
  }
  return false;
}

void OpdsBookBrowserActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  // Show server name in header if available, otherwise generic title
  const char* headerTitle = server.name.empty() ? tr(STR_OPDS_BROWSER) : server.name.c_str();
  renderer.drawCenteredText(UI_12_FONT_ID, 15, headerTitle, true, EpdFontFamily::BOLD);

  if (state == BrowserState::CHECK_WIFI || state == BrowserState::LOADING) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, statusMessage.c_str());
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  if (state == BrowserState::ERROR) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 20, tr(STR_ERROR_MSG));
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 10, errorMessage.c_str());
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_RETRY), "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  if (state == BrowserState::DOWNLOADING) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 40, tr(STR_DOWNLOADING));
    auto title = renderer.truncatedText(UI_10_FONT_ID, statusMessage.c_str(), pageWidth - 40);
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 10, title.c_str());
    if (downloadTotal > 0) {
      GUI.drawProgressBar(renderer, Rect{50, pageHeight / 2 + 20, pageWidth - 100, 20}, downloadProgress,
                          downloadTotal);
    }
    const auto labels = mappedInput.mapLabels(tr(STR_CANCEL), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  const char* confirmLabel =
      (entryCount > 0 && entries[selectorIndex].type == OpdsEntryType::BOOK) ? tr(STR_DOWNLOAD) : tr(STR_OPEN);
  const char* searchLabel = (!searchTemplate.empty() && selectorIndex == 0) ? tr(STR_SEARCH) : tr(STR_DIR_UP);
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), confirmLabel, searchLabel, tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  if (entryCount == 0) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, tr(STR_NO_ENTRIES));
  } else {
    const auto& metrics = UITheme::getInstance().getMetrics();
    const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
    const int contentHeight =
        std::max(0, pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing);
    GUI.drawList(renderer, Rect{0, contentTop, pageWidth, contentHeight}, static_cast<int>(entryCount), selectorIndex,
                 [this](const int index) {
                   const auto& entry = entries[index];
                   std::string label = entry.type == OpdsEntryType::NAVIGATION ? "> " + entry.title : entry.title;
                   if (entry.type == OpdsEntryType::BOOK && !entry.author.empty()) label += " - " + entry.author;
                   return label;
                 });
  }
  renderer.displayBuffer();
}

int OpdsBookBrowserActivity::visibleItemCount() const {
  return std::max(1, UITheme::getNumberOfItemsPerPage(renderer, /*hasHeader=*/true, /*hasTabBar=*/false,
                                                      /*hasButtonHints=*/true, /*hasSubtitle=*/false));
}

void OpdsBookBrowserActivity::showLoadingBeforeFetch() {
  state = BrowserState::LOADING;
  statusMessage = tr(STR_LOADING);
  requestUpdate();
}

void OpdsBookBrowserActivity::fetchFeed(const std::string& path) {
  if (!ensureEntryBuffer()) {
    state = BrowserState::ERROR;
    errorMessage = tr(STR_MEMORY_ERROR);
    requestUpdate();
    return;
  }

  if (server.url.empty()) {
    state = BrowserState::ERROR;
    errorMessage = tr(STR_NO_SERVER_URL);
    requestUpdate();
    return;
  }

  clearEntries();
  const std::string url = (path.find("http") == 0) ? path : UrlUtils::buildUrl(server.url, path);
  LOG_DBG("OPDS", "Queueing feed refresh");
  activeJobId = OPDS_SYNC.enqueueCatalogRefresh(server, url);
  if (activeJobId == 0) {
    state = BrowserState::ERROR;
    errorMessage = tr(STR_MEMORY_ERROR);
    requestUpdate();
  }
}

void OpdsBookBrowserActivity::applyFeedResult(OpdsSyncService::Result&& result) {
  if (result.code != OpdsSyncService::ResultCode::Ok) {
    state = BrowserState::ERROR;
    errorMessage =
        result.code == OpdsSyncService::ResultCode::ParseFailed ? tr(STR_PARSE_FEED_FAILED) : tr(STR_FETCH_FEED_FAILED);
    requestUpdate();
    return;
  }
  if (!ensureEntryBuffer() || result.entries.size() > MAX_OPDS_FEED_ENTRIES) {
    state = BrowserState::ERROR;
    errorMessage = tr(STR_MEMORY_ERROR);
    requestUpdate();
    return;
  }
  clearEntries();
  entryCount = result.entries.size();
  for (size_t index = 0; index < entryCount; ++index) entries[index] = std::move(result.entries[index]);
  searchTemplate = std::move(result.searchTemplate);
  const auto& nextUrl = result.nextUrl;
  const auto& prevUrl = result.previousUrl;
  // Every successfully parsed book feed becomes immediately useful offline:
  // this stores metadata only, never downloads EPUB payloads implicitly.
  const auto& snapshot = result.catalogEntries.empty() ? result.entries : result.catalogEntries;
  if (!OPDS_CATALOG.replaceServerSnapshot(server.id, snapshot)) {
    LOG_ERR("OPDS", "Could not persist OPDS catalog metadata");
  }
  // The Library owns the catalog, and the global coordinator owns optional
  // Sync All. The browser must never turn a foreground navigation activity
  // into a sequential bulk-download loop.
  if (server.syncAllBooks) OPDS_OFFLINE_SYNC.startPrimaryIfEnabled();
  if (autoOpenCatalog && currentPath.empty()) {
    const auto allBooks = std::find_if(entries.get(), entries.get() + entryCount, [](const OpdsEntry& entry) {
      if (entry.type != OpdsEntryType::NAVIGATION) return false;
      std::string title = entry.title;
      std::transform(title.begin(), title.end(), title.begin(),
                     [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
      return title == "all books";
    });
    if (allBooks != entries.get() + entryCount) {
      navigateToEntry(*allBooks);
      return;
    }
  }
  if (autoOpenCatalog && std::any_of(entries.get(), entries.get() + entryCount,
                                     [](const OpdsEntry& entry) { return entry.type == OpdsEntryType::BOOK; })) {
    autoOpenCatalog = false;
    activityManager.goToLibrary();
    return;
  }
  if (result.truncated) {
    LOG_DBG("OPDS", "Feed truncated to %zu entries", entryCount);
  }

  if (!prevUrl.empty()) {
    for (size_t i = entryCount; i > 0; --i) {
      entries[i] = std::move(entries[i - 1]);
    }
    entries[0] = OpdsEntry{OpdsEntryType::NAVIGATION, tr(STR_PREV_PAGE), "", prevUrl, ""};
    entryCount++;
  }
  if (!nextUrl.empty() && !appendEntry(OpdsEntry{OpdsEntryType::NAVIGATION, tr(STR_NEXT_PAGE), "", nextUrl, ""})) {
    LOG_DBG("OPDS", "No room for next-page entry");
  }

  selectorIndex = 0;
  state = entryCount == 0 ? BrowserState::ERROR : BrowserState::BROWSING;
  if (entryCount == 0) errorMessage = tr(STR_NO_ENTRIES);
  requestUpdate();
}

void OpdsBookBrowserActivity::processBackgroundJob() {
  if (activeJobId == 0) return;
  if (state == BrowserState::DOWNLOADING) {
    const auto current = OPDS_SYNC.progress(activeJobId);
    if (current.running && (downloadProgress != current.completedBytes || downloadTotal != current.totalBytes)) {
      downloadProgress = current.completedBytes;
      downloadTotal = current.totalBytes;
      requestUpdate();
    }
  }
  OpdsSyncService::Result result;
  if (!OPDS_SYNC.takeResult(activeJobId, result)) return;
  activeJobId = 0;
  if (result.kind == OpdsSyncService::JobKind::CatalogRefresh) {
    applyFeedResult(std::move(result));
  } else {
    applyDownloadResult(std::move(result));
  }
}

bool OpdsBookBrowserActivity::ensureEntryBuffer() {
  if (entries) return true;
  entries = makeUniqueNoThrow<OpdsEntry[]>(OPDS_BROWSER_ENTRY_CAPACITY);
  return entries != nullptr;
}

void OpdsBookBrowserActivity::clearEntries() {
  // Slots past entryCount are ignored and overwritten by the next feed parse.
  entryCount = 0;
}

bool OpdsBookBrowserActivity::appendEntry(OpdsEntry&& entry) {
  if (!entries || entryCount >= OPDS_BROWSER_ENTRY_CAPACITY) return false;
  entries[entryCount++] = std::move(entry);
  return true;
}

void OpdsBookBrowserActivity::navigateToEntry(const OpdsEntry& entry) {
  navigationHistory.push_back(currentPath);
  // Resolve to a full URL so sub-sub-navigation retains parent path context
  const std::string feedUrl = UrlUtils::buildUrl(server.url, currentPath);
  currentPath = UrlUtils::buildUrl(feedUrl, entry.href);

  clearEntries();
  selectorIndex = 0;
  showLoadingBeforeFetch();
  fetchFeed(currentPath);
}

void OpdsBookBrowserActivity::navigateBack() {
  if (navigationHistory.empty()) {
    onGoHome();
  } else {
    currentPath = navigationHistory.back();
    navigationHistory.pop_back();
    clearEntries();
    selectorIndex = 0;
    showLoadingBeforeFetch();
    fetchFeed(currentPath);
  }
}

void OpdsBookBrowserActivity::downloadBook(const OpdsEntry& book) {
  state = BrowserState::DOWNLOADING;
  statusMessage = book.title;
  downloadProgress = downloadTotal = 0;
  requestUpdate();

  // Build full download URL relative to the current feed, not the root server URL
  const std::string feedUrl = UrlUtils::buildUrl(server.url, currentPath);
  std::string downloadUrl = UrlUtils::buildUrl(feedUrl, book.href);
  const std::string serverKey = OpdsCatalogStore::serverKeyForIdentity(server.id);
  const std::string directory = "/Books/OPDS/" + serverKey;
  if (!Storage.ensureDirectoryExists(directory.c_str())) {
    LOG_ERR("OPDS", "Could not create OPDS destination: %s", directory.c_str());
    state = BrowserState::ERROR;
    errorMessage = tr(STR_DOWNLOAD_FAILED);
    requestUpdate();
    return;
  }
  const std::string filename =
      OpdsBookStorage::downloadPath(server.id, server.filenameFormat == OpdsFilenameFormat::TITLE_AUTHOR,
                                    OpdsCatalogStore::stableBookId(book), book.title, book.author);
  LOG_DBG("OPDS", "Queueing book download");
  OPDS_CATALOG.markAvailability(server.id, book, OpdsCatalogAvailability::Downloading, filename);
  activeDownloadBook = book;
  activeDownloadPath = filename;
  activeDownloadTemporaryPath = filename + ".part";
  activeJobId = OPDS_SYNC.enqueueBookDownload(server, downloadUrl, activeDownloadTemporaryPath);
  if (activeJobId == 0) {
    OPDS_CATALOG.markAvailability(server.id, book, OpdsCatalogAvailability::DownloadFailed);
    activeDownloadBook.reset();
    state = BrowserState::ERROR;
    errorMessage = tr(STR_MEMORY_ERROR);
    requestUpdate();
  }
}

void OpdsBookBrowserActivity::applyDownloadResult(OpdsSyncService::Result&& result) {
  if (!activeDownloadBook) {
    LOG_ERR("OPDS", "Download completed without an active book");
    state = BrowserState::ERROR;
    errorMessage = tr(STR_DOWNLOAD_FAILED);
    requestUpdate();
    return;
  }
  const OpdsEntry book = std::move(*activeDownloadBook);
  activeDownloadBook.reset();
  const std::string filename = activeDownloadPath;
  const std::string temporaryFilename = activeDownloadTemporaryPath;
  activeDownloadPath.clear();
  activeDownloadTemporaryPath.clear();
  if (result.code == OpdsSyncService::ResultCode::Ok) {
    std::string validationDetail;
    if (!validateOpdsEpubArchive(temporaryFilename, validationDetail) ||
        !Storage.rename(temporaryFilename.c_str(), filename.c_str())) {
      LOG_ERR("OPDS", "Downloaded EPUB publish failed: %s", validationDetail.c_str());
      OPDS_CATALOG.markAvailability(server.id, book, OpdsCatalogAvailability::DownloadFailed);
      state = BrowserState::ERROR;
      errorMessage = tr(STR_DOWNLOAD_FAILED);
      requestUpdate();
      return;
    }
    clearBookCache(filename);
    OPDS_CATALOG.markAvailability(server.id, book, OpdsCatalogAvailability::AvailableOffline, filename);
    if (directDownload) {
      onSelectBook(filename);
      return;
    }
    state = BrowserState::BROWSING;
  } else if (result.code == OpdsSyncService::ResultCode::Cancelled) {
    LOG_DBG("OPDS", "Download cancelled");
    mappedInput.suppressNextBackRelease();
    OPDS_CATALOG.markAvailability(server.id, book, OpdsCatalogAvailability::RemoteOnly);
    state = BrowserState::BROWSING;
  } else {
    OPDS_CATALOG.markAvailability(server.id, book, OpdsCatalogAvailability::DownloadFailed);
    state = BrowserState::ERROR;
    errorMessage = tr(STR_DOWNLOAD_FAILED);
  }
  requestUpdate();
}

void OpdsBookBrowserActivity::launchSearch() {
  consumeConfirm = true;
  state = BrowserState::SEARCH_INPUT;
  requestUpdate();

  auto keyboard = std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_SEARCH));
  startActivityForResult(std::move(keyboard), [this](const ActivityResult& result) {
    state = BrowserState::BROWSING;
    if (!result.isCancelled) {
      performSearch(std::get<KeyboardResult>(result.data).text);
    } else {
      requestUpdate();
    }
  });
}

void OpdsBookBrowserActivity::performSearch(const std::string& query) {
  if (query.empty() || searchTemplate.empty()) {
    state = BrowserState::BROWSING;
    requestUpdate();
    return;
  }

  auto urlEncode = [](const std::string& s) {
    std::string out;
    out.reserve(s.size() * 3);
    for (unsigned char c : s) {
      if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
        out += static_cast<char>(c);
      else {
        char buf[4];
        snprintf(buf, sizeof(buf), "%%%02X", c);
        out += buf;
      }
    }
    return out;
  };

  std::string url = searchTemplate;
  const std::string placeholder = "{searchTerms}";
  const size_t pos = url.find(placeholder);
  if (pos != std::string::npos) url.replace(pos, placeholder.length(), urlEncode(query));

  navigationHistory.push_back(currentPath);  // <-- add this
  currentPath = url;                         // <-- add this

  showLoadingBeforeFetch();
  fetchFeed(url);
}

void OpdsBookBrowserActivity::checkAndConnectWifi() {
  if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
    if (directDownload) {
      downloadBook(*directDownload);
      return;
    }
    showLoadingBeforeFetch();
    fetchFeed(currentPath);
    return;
  }
  launchWifiSelection();
}

void OpdsBookBrowserActivity::launchWifiSelection() {
  state = BrowserState::WIFI_SELECTION;
  requestUpdate();

  startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
                         [this](const ActivityResult& result) { onWifiSelectionComplete(!result.isCancelled); });
}

void OpdsBookBrowserActivity::onWifiSelectionComplete(const bool connected) {
  if (connected) {
    if (directDownload) {
      downloadBook(*directDownload);
      return;
    }
    showLoadingBeforeFetch();
    fetchFeed(currentPath);
  } else {
    // Leave WiFi up; onExit's silent reboot handles teardown without fragmenting.
    state = BrowserState::ERROR;
    errorMessage = tr(STR_WIFI_CONN_FAILED);
    requestUpdate();
  }
}
