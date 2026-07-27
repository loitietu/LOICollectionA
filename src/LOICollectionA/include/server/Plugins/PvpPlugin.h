#pragma once

#include <memory>

#include <ll/api/Expected.h>

#include "LOICollectionA/base/Macro.h"

#include "LOICollectionA/include/ModuleBase.h"
#include "LOICollectionA/include/ModManager.h"

#include "LOICollectionA/include/server/Plugins/gui/PvpGui.h"

class Player;

namespace ll::io {
    class Logger;
}

namespace LOICollection::server::Plugins {
    enum class PvpPluginErrorCode : int {
        Invalid = 1
    };

    struct PvpPluginErrorCategory : std::error_category {
        [[nodiscard]] const char* name() const noexcept override {
            return "BehaviorEventPluginError";
        }

        [[nodiscard]] std::string message(int ev) const override {
            switch (static_cast<PvpPluginErrorCode>(ev)) {
                case PvpPluginErrorCode::Invalid: return "Plugin is invalid";
                default:
                    return "Unknown";
            }
        }
    };

    class PvpPlugin : public std::enable_shared_from_this<PvpPlugin>,
                      public modules::ModuleBase,
                      public modules::AutoRegister<PvpPlugin> {
    public:
        ~PvpPlugin();

        PvpPlugin(PvpPlugin const&) = delete;
        PvpPlugin(PvpPlugin&&) = delete;
        PvpPlugin& operator=(PvpPlugin const&) = delete;
        PvpPlugin& operator=(PvpPlugin&&) = delete;

    public:
        LOICOLLECTION_A_NDAPI static std::shared_ptr<PvpPlugin> getShared();
        LOICOLLECTION_A_NDAPI static std::error_code makeErrorCode(PvpPluginErrorCode e);

        LOICOLLECTION_A_NDAPI std::shared_ptr<ll::io::Logger> getLogger();

        LOICOLLECTION_A_NDAPI ll::Expected<void> enable(Player& player, bool value);

        LOICOLLECTION_A_NDAPI ll::Expected<bool> isEnable(Player& player);
        
        LOICOLLECTION_A_NDAPI bool isValid();

    public:
        LOICOLLECTION_A_NDAPI std::string getName() override;

        LOICOLLECTION_A_NDAPI modules::ModulePriority getPriority() override;

        LOICOLLECTION_A_API   ll::Expected<bool> load() override;
        LOICOLLECTION_A_API   ll::Expected<bool> unload() override;
        LOICOLLECTION_A_API   ll::Expected<bool> registry() override;
        LOICOLLECTION_A_API   ll::Expected<bool> unregistry() override;

    public:
        class gui;
        friend class gui;

    private:
        PvpPlugin();

        void registeryCommand();
        void listenEvent();
        void unlistenEvent();

        struct Impl;
        std::unique_ptr<Impl> mImpl;
        std::unique_ptr<PvpGui> mGui;
    };
}