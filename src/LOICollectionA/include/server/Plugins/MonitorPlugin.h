#pragma once

#include <memory>
#include <string>
#include <vector>

#include <ll/api/Expected.h>

#include "LOICollectionA/base/Macro.h"

#include "LOICollectionA/include/ModuleBase.h"
#include "LOICollectionA/include/ModManager.h"

class Player;

namespace LOICollection::server::Plugins {
    enum class SidebarType {
        Ascending = 0,
        Descending = 1
    };

    enum class MonitorPluginErrorCode : int {
        Invalid = 1
    };

    struct MonitorPluginErrorCategory : std::error_category {
        [[nodiscard]] const char* name() const noexcept override {
            return "MonitorPluginError";
        }

        [[nodiscard]] std::string message(int ev) const override {
            switch (static_cast<MonitorPluginErrorCode>(ev)) {
                case MonitorPluginErrorCode::Invalid: return "Plugin is invalid";
                default:
                    return "Unknown";
            }
        }
    };

    class MonitorPlugin : public std::enable_shared_from_this<MonitorPlugin>,
                          public modules::ModuleBase,
                          public modules::AutoRegister<MonitorPlugin> {
    public:
        ~MonitorPlugin();

        MonitorPlugin(MonitorPlugin const&) = delete;
        MonitorPlugin(MonitorPlugin&&) = delete;
        MonitorPlugin& operator=(MonitorPlugin const&) = delete;
        MonitorPlugin& operator=(MonitorPlugin&&) = delete;    

    public:
        LOICOLLECTION_A_NDAPI static std::shared_ptr<MonitorPlugin> getShared();
        LOICOLLECTION_A_NDAPI static std::error_code makeErrorCode(MonitorPluginErrorCode e);

        LOICOLLECTION_A_NDAPI std::shared_ptr<ll::io::Logger> getLogger();

        LOICOLLECTION_A_NDAPI ll::Expected<void> addSidebar(Player& player, const std::string& id, const std::string& name, SidebarType type);
        LOICOLLECTION_A_NDAPI ll::Expected<void> setSidebar(Player& player, const std::string& id, std::vector<std::pair<std::string, int>> data);
        LOICOLLECTION_A_NDAPI ll::Expected<void> removeSidebar(Player& player, const std::string& id);

        LOICOLLECTION_A_NDAPI bool isValid();

    public:
        LOICOLLECTION_A_NDAPI std::string getName() override;

        LOICOLLECTION_A_NDAPI modules::ModulePriority getPriority() override;

        LOICOLLECTION_A_API   ll::Expected<bool> load() override;
        LOICOLLECTION_A_API   ll::Expected<bool> unload() override;
        LOICOLLECTION_A_API   ll::Expected<bool> registry() override;
        LOICOLLECTION_A_API   ll::Expected<bool> unregistry() override;

    private:
        MonitorPlugin();

        void listenEvent();
        void unlistenEvent();

        struct Impl;
        std::unique_ptr<Impl> mImpl;
    };
}