#pragma once
#include <OpdsParser.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "OpdsServerStore.h"
#include "activities/Activity.h"
#include "network/OpdsSyncService.h"
#include "util/ButtonNavigator.h"

/**
 * Activity for browsing and downloading books from an OPDS server.
 * Supports navigation through catalog hierarchy and downloading EPUBs.
 */
class OpdsBookBrowserActivity final : public Activity {
 public:
  enum class BrowserState { CHECK_WIFI, WIFI_SELECTION, LOADING, BROWSING, DOWNLOADING, ERROR, SEARCH_INPUT };

  explicit OpdsBookBrowserActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, OpdsServer server,
                                   std::optional<OpdsEntry> directDownload = std::nullopt, bool autoOpenCatalog = false)
      : Activity("OpdsBookBrowser", renderer, mappedInput),
        buttonNavigator(),
        server(std::move(server)),
        directDownload(std::move(directDownload)),
        autoOpenCatalog(autoOpenCatalog) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  ButtonNavigator buttonNavigator;
  BrowserState state = BrowserState::LOADING;
  std::unique_ptr<OpdsEntry[]> entries;
  size_t entryCount = 0;
  std::vector<std::string> navigationHistory;
  std::string currentPath;
  std::string searchTemplate;
  bool consumeConfirm = false;
  bool consumeBack = false;  // Added missing member
  int selectorIndex = 0;
  std::string errorMessage;
  std::string statusMessage;
  size_t downloadProgress = 0;
  size_t downloadTotal = 0;
  uint64_t activeJobId = 0;
  std::optional<OpdsEntry> activeDownloadBook;
  std::string activeDownloadPath;
  std::vector<OpdsEntry> bulkQueue;
  size_t bulkIndex = 0;
  bool bulkRunning = false;

  OpdsServer server;  // Copied at construction — safe even if the store changes during browsing
  std::optional<OpdsEntry> directDownload;
  bool autoOpenCatalog = false;

  void checkAndConnectWifi();
  void launchWifiSelection();
  void onWifiSelectionComplete(bool connected);
  void showLoadingBeforeFetch();
  void fetchFeed(const std::string& path);
  void processBackgroundJob();
  void applyFeedResult(OpdsSyncService::Result&& result);
  void applyDownloadResult(OpdsSyncService::Result&& result);
  bool ensureEntryBuffer();
  void clearEntries();
  bool appendEntry(OpdsEntry&& entry);
  void navigateToEntry(const OpdsEntry& entry);
  void navigateBack();
  void downloadBook(const OpdsEntry& book);
  void startBulkSyncIfEnabled();
  void downloadNextBulkBook();
  void launchSearch();
  void performSearch(const std::string& query);
  [[nodiscard]] int visibleItemCount() const;
  bool preventAutoSleep() override;
};
