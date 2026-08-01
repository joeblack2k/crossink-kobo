#include "OpdsSettingsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>

#include <cstring>

#include "MappedInputManager.h"
#include "OpdsServerStore.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/DirectListTouch.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "network/OpdsCoverCache.h"
#include "network/OpdsOfflineSync.h"

namespace {
// Editable fields: Name, URL, Username, Password, Filename.
// The primary server additionally exposes operational controls; other servers
// remain simple configuration records.
constexpr int BASE_ITEMS = 6;
constexpr int PRIMARY_SYNC_ITEMS = 3;

std::string transferStatusText(const OpdsOfflineSync::Status& status) {
  std::string result = "Downloading " + std::to_string(status.completed) + "/" + std::to_string(status.total);
  if (status.currentTotalBytes != 0) {
    const size_t remaining =
        status.currentTotalBytes > status.currentBytes ? status.currentTotalBytes - status.currentBytes : 0;
    result += " — " + std::to_string(remaining / 1024) + " KiB left";
  }
  return result;
}
}  // namespace

int OpdsSettingsActivity::getMenuItemCount() const {
  const bool isPrimary = !isNewServer && serverIndex == 0;
  return BASE_ITEMS + (isPrimary ? PRIMARY_SYNC_ITEMS : 0) + (isNewServer ? 0 : 1);
}

void OpdsSettingsActivity::onEnter() {
  Activity::onEnter();

  selectedIndex = 0;
  isNewServer = (serverIndex < 0);
  showSaveError = false;

  if (!isNewServer) {
    // Edit flow: copy the selected server into local editable state.
    // Changes are persisted field-by-field through saveServer().
    const auto* server = OPDS_STORE.getServer(static_cast<size_t>(serverIndex));
    if (server) {
      editServer = *server;
    } else {
      // Server was deleted between navigation and entering this screen — treat as new
      isNewServer = true;
      serverIndex = -1;
    }
  }

  requestUpdate();
}

void OpdsSettingsActivity::onExit() { Activity::onExit(); }

void OpdsSettingsActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  consumeDirectListSelection(mappedInput, getMenuItemCount(), selectedIndex);

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    handleSelection();
    return;
  }

  const int menuItems = getMenuItemCount();
  buttonNavigator.onNext([this, menuItems] {
    selectedIndex = (selectedIndex + 1) % menuItems;
    requestUpdate();
  });

  buttonNavigator.onPrevious([this, menuItems] {
    selectedIndex = (selectedIndex + menuItems - 1) % menuItems;
    requestUpdate();
  });
}

bool OpdsSettingsActivity::saveServer() {
  bool success = false;

  if (isNewServer) {
    // Create flow: first save inserts a new server record into the multi-server store.
    success = OPDS_STORE.addServer(editServer);
    if (success) {
      // After the first successful save, promote to an existing server so
      // subsequent field edits update in-place rather than creating duplicates.
      isNewServer = false;
      serverIndex = static_cast<int>(OPDS_STORE.getCount()) - 1;
    } else {
      LOG_ERR("OPS", "Failed to add OPDS server");
    }
  } else {
    // Edit flow: update the same server entry in-place.
    success = OPDS_STORE.updateServer(static_cast<size_t>(serverIndex), editServer);
    if (!success) {
      LOG_ERR("OPS", "Failed to update OPDS server at index %d", serverIndex);
    }
  }

  showSaveError = !success;
  if (showSaveError) {
    requestUpdate();
  }

  return success;
}

void OpdsSettingsActivity::handleSelection() {
  // Each field edit is saved immediately so partially configured servers
  // survive navigation and power-loss scenarios.
  if (selectedIndex == 0) {
    // Server Name
    auto handler = [this](const ActivityResult& result) {
      if (!result.isCancelled) {
        const auto& kb = std::get<KeyboardResult>(result.data);
        editServer.name = kb.text;
        saveServer();
        requestUpdate();
      }
    };
    startActivityForResult(std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_SERVER_NAME),
                                                                   editServer.name, 63, InputType::Text),
                           handler);
  } else if (selectedIndex == 1) {
    // Server URL
    const std::string prefillUrl = editServer.url.empty() ? "https://" : editServer.url;
    auto handler = [this](const ActivityResult& result) {
      if (!result.isCancelled) {
        const auto& kb = std::get<KeyboardResult>(result.data);
        editServer.url = (kb.text == "https://" || kb.text == "http://") ? "" : kb.text;
        saveServer();
        requestUpdate();
      }
    };
    startActivityForResult(std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_OPDS_SERVER_URL),
                                                                   prefillUrl, 127, InputType::Url),
                           handler);
  } else if (selectedIndex == 2) {
    // Username
    auto handler = [this](const ActivityResult& result) {
      if (!result.isCancelled) {
        const auto& kb = std::get<KeyboardResult>(result.data);
        editServer.username = kb.text;
        saveServer();
        requestUpdate();
      }
    };
    startActivityForResult(std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_USERNAME),
                                                                   editServer.username, 63, InputType::Text),
                           handler);
  } else if (selectedIndex == 3) {
    // Password
    auto handler = [this](const ActivityResult& result) {
      if (!result.isCancelled) {
        const auto& kb = std::get<KeyboardResult>(result.data);
        editServer.password = kb.text;
        saveServer();
        requestUpdate();
      }
    };
    startActivityForResult(std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_PASSWORD),
                                                                   editServer.password, 63, InputType::Password),
                           handler);
  } else if (selectedIndex == 4) {
    editServer.filenameFormat = editServer.filenameFormat == OpdsFilenameFormat::AUTHOR_TITLE
                                    ? OpdsFilenameFormat::TITLE_AUTHOR
                                    : OpdsFilenameFormat::AUTHOR_TITLE;
    saveServer();
    requestUpdate();
  } else if (selectedIndex == 5) {
    editServer.syncAllBooks = !editServer.syncAllBooks;
    saveServer();
    if (editServer.syncAllBooks) OPDS_OFFLINE_SYNC.requestCatalogRefresh();
    requestUpdate();
  } else if (!isNewServer && serverIndex == 0 && selectedIndex == 6) {
    const auto status = OPDS_OFFLINE_SYNC.status();
    if (status.phase == OpdsOfflineSync::Phase::Paused) {
      OPDS_OFFLINE_SYNC.resume();
    } else {
      OPDS_OFFLINE_SYNC.pause();
    }
    requestUpdate();
  } else if (!isNewServer && serverIndex == 0 && selectedIndex == 7) {
    OPDS_OFFLINE_SYNC.cancel();
    requestUpdate();
  } else if (!isNewServer && serverIndex == 0 && selectedIndex == 8) {
    OPDS_COVER_CACHE.retryFailedForServer(editServer.id);
    requestUpdate();
  } else if (selectedIndex == getMenuItemCount() - 1 && !isNewServer) {
    // Delete flow is only available for existing servers.
    if (!OPDS_STORE.removeServer(static_cast<size_t>(serverIndex))) {
      LOG_ERR("OPS", "Failed to remove OPDS server at index %d", serverIndex);
      showSaveError = true;
      requestUpdate();
      return;
    }
    finish();
  }
}

void OpdsSettingsActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  // Reuse STR_OPDS_BROWSER as the "edit existing server" title.
  // New server creation uses STR_ADD_SERVER.
  const char* header = isNewServer ? tr(STR_ADD_SERVER) : tr(STR_OPDS_BROWSER);
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, header);
  GUI.drawSubHeader(renderer, Rect{0, metrics.topPadding + metrics.headerHeight, pageWidth, metrics.tabBarHeight},
                    tr(STR_CALIBRE_URL_HINT));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing + metrics.tabBarHeight;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing * 2;
  const int menuItems = getMenuItemCount();

  const StrId fieldNames[] = {StrId::STR_SERVER_NAME, StrId::STR_OPDS_SERVER_URL, StrId::STR_USERNAME,
                              StrId::STR_PASSWORD, StrId::STR_FILENAME};

  GUI.drawList(
      renderer, Rect{0, contentTop, pageWidth, contentHeight}, menuItems, static_cast<int>(selectedIndex),
      [this, &fieldNames](int index) {
        if (index < 5) {
          return std::string(I18N.get(fieldNames[index]));
        }
        if (index == 5) return std::string("Sync all books for offline use");
        if (!isNewServer && serverIndex == 0 && index == 6) return std::string("Pause background sync");
        if (!isNewServer && serverIndex == 0 && index == 7) return std::string("Cancel background sync");
        if (!isNewServer && serverIndex == 0 && index == 8) return std::string("Retry failed cover downloads");
        return std::string(tr(STR_DELETE_SERVER));
      },
      nullptr, nullptr,
      [this](int index) {
        if (index == 0) {
          return editServer.name.empty() ? std::string(tr(STR_NOT_SET)) : editServer.name;
        } else if (index == 1) {
          return editServer.url.empty() ? std::string(tr(STR_NOT_SET)) : editServer.url;
        } else if (index == 2) {
          return editServer.username.empty() ? std::string(tr(STR_NOT_SET)) : editServer.username;
        } else if (index == 3) {
          return editServer.password.empty() ? std::string(tr(STR_NOT_SET)) : std::string("******");
        } else if (index == 4) {
          return editServer.filenameFormat == OpdsFilenameFormat::TITLE_AUTHOR ? std::string(tr(STR_TITLE_AUTHOR))
                                                                               : std::string(tr(STR_AUTHOR_TITLE));
        } else if (index == 5) {
          return editServer.syncAllBooks ? std::string("On") : std::string("Off");
        } else if (!isNewServer && serverIndex == 0 && index == 6) {
          const auto status = OPDS_OFFLINE_SYNC.status();
          if (status.phase == OpdsOfflineSync::Phase::Paused) return std::string("Paused — tap to resume");
          if (status.phase == OpdsOfflineSync::Phase::Downloading) return transferStatusText(status);
          if (status.phase == OpdsOfflineSync::Phase::SyncingMetadata) return std::string("Syncing catalog");
          if (status.phase == OpdsOfflineSync::Phase::Failed) {
            return status.lastError.empty() ? std::string("Failed") : status.lastError;
          }
          if (status.phase == OpdsOfflineSync::Phase::Offline) return std::string("Offline — cached catalog");
          if (status.lastSuccessMs != 0) return std::string("Catalog synced");
          return std::string("Idle");
        } else if (!isNewServer && serverIndex == 0 && index == 7) {
          return std::string("Stop remaining downloads");
        } else if (!isNewServer && serverIndex == 0 && index == 8) {
          return std::string("Retry covers with a backoff error");
        }
        return std::string("");
      },
      true);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  if (showSaveError) {
    GUI.drawPopup(renderer, tr(STR_ERROR_GENERAL_FAILURE));
  }

  renderer.displayBuffer();
}
