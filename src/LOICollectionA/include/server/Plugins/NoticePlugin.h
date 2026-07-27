#pragma once

#include <memory>
#include <string>

#include <ll/api/Expected.h>

#include "LOICollectionA/base/Macro.h"

#include "LOICollectionA/include/ModuleBase.h"
#include "LOICollectionA/include/ModManager.h"

#include "LOICollectionA/include/server/Plugins/gui/NoticeGui.h"

class Player;
class JsonStorage;

namespace ll::io {
    class Logger;
}

namespace LOICollection::server::Plugins {
    enum class NoticePluginErrorCode : int {
        Invalid = 1
    };

    struct NoticePluginErrorCategory : std::error_category {
        [[nodiscard]] const char* name() const noexcept override {
            return "NoticePluginError";
        }

        [[nodiscard]] std::string message(int ev) const override {
            switch (static_cast<NoticePluginErrorCode>(ev)) {
                case NoticePluginErrorCode::Invalid: return "Plugin is invalid";
                default:
                    return "Unknown";
            }
        }
    };

    class NoticePlugin : public std::enable_shared_from_this<NoticePlugin>,
                         public modules::ModuleBase,
                         public modules::AutoRegister<NoticePlugin> {
    public:
        ~NoticePlugin();

        NoticePlugin(NoticePlugin const&) = delete;
        NoticePlugin(NoticePlugin&&) = delete;
        NoticePlugin& operator=(NoticePlugin const&) = delete;
        NoticePlugin& operator=(NoticePlugin&&) = delete;

    public:
        LOICOLLECTION_A_NDAPI static std::shared_ptr<NoticePlugin> getShared();
        LOICOLLECTION_A_NDAPI static std::error_code makeErrorCode(NoticePluginErrorCode e);

        LOICOLLECTION_A_NDAPI std::shared_ptr<JsonStorage> getDatabase();
        LOICOLLECTION_A_NDAPI std::shared_ptr<ll::io::Logger> getLogger();

        LOICOLLECTION_A_NDAPI ll::Expected<void> create(const std::string& id, const std::string& title, int priority, bool poiontout);
        LOICOLLECTION_A_NDAPI ll::Expected<void> remove(const std::string& id);

        LOICOLLECTION_A_NDAPI ll::Expected<void> setClose(Player& player, bool enable);

        LOICOLLECTION_A_NDAPI ll::Expected<bool> has(const std::string& id);
        LOICOLLECTION_A_NDAPI ll::Expected<bool> isClose(Player& player);

        LOICOLLECTION_A_NDAPI bool isValid();

    public:
        LOICOLLECTION_A_NDAPI std::string getName() override;

        LOICOLLECTION_A_NDAPI modules::ModulePriority getPriority() override;

        LOICOLLECTION_A_API   ll::Expected<bool> load() override;
        LOICOLLECTION_A_API   ll::Expected<bool> unload() override;
        LOICOLLECTION_A_API   ll::Expected<bool> registry() override;
        LOICOLLECTION_A_API   ll::Expected<bool> unregistry() override;

    private:
        NoticePlugin();

        void registeryCommand();
        void listenEvent();
        void unlistenEvent();

        struct operation;

        struct Impl;
        std::unique_ptr<Impl> mImpl;
        std::unique_ptr<NoticeGui> mGui;
    };
}