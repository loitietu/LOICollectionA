#pragma once

#include <memory>
#include <string>
#include <vector>

#include <ll/api/Expected.h>

#include "LOICollectionA/base/Macro.h"

#include "LOICollectionA/include/ModuleBase.h"
#include "LOICollectionA/include/ModManager.h"

class Player;
class JsonStorage;

namespace ll::io {
    class Logger;
}

namespace LOICollection::server::Plugins {
    enum class CdkPluginErrorCode : int {
        Invalid = 1,
        NotFound = 2,
        Received = 3,
        Exists = 4
    };

    struct CdkPluginErrorCategory : std::error_category {
        [[nodiscard]] const char* name() const noexcept override {
            return "CdkPluginError";
        }

        [[nodiscard]] std::string message(int ev) const override {
            switch (static_cast<CdkPluginErrorCode>(ev)) {
                case CdkPluginErrorCode::Invalid: return "Plugin is invalid";
                case CdkPluginErrorCode::NotFound: return "Cdk data not found";
                case CdkPluginErrorCode::Received: return "Received cdk";
                case CdkPluginErrorCode::Exists: return "Already exists cdk";
                default:
                    return "Unknown";
            }
        }
    };

    class CdkPlugin : public std::enable_shared_from_this<CdkPlugin>,
                      public modules::ModuleBase,
                      public modules::AutoRegister<CdkPlugin> {
    public:
        ~CdkPlugin();

        CdkPlugin(CdkPlugin const&) = delete;
        CdkPlugin(CdkPlugin&&) = delete;
        CdkPlugin& operator=(CdkPlugin const&) = delete;
        CdkPlugin& operator=(CdkPlugin&&) = delete;

    public:
        LOICOLLECTION_A_NDAPI static std::shared_ptr<CdkPlugin> getShared();
        LOICOLLECTION_A_NDAPI static std::error_code makeErrorCode(CdkPluginErrorCode e);

        LOICOLLECTION_A_NDAPI std::shared_ptr<JsonStorage> getDatabase();
        LOICOLLECTION_A_NDAPI std::shared_ptr<ll::io::Logger> getLogger();

        LOICOLLECTION_A_NDAPI ll::Expected<void> create(const std::string& id, int time, bool personal);
        LOICOLLECTION_A_NDAPI ll::Expected<void> remove(const std::string& id);

        LOICOLLECTION_A_NDAPI ll::Expected<void> convert(Player& player, const std::string& id);

        LOICOLLECTION_A_NDAPI ll::Expected<std::vector<std::string>> getCdks();

        LOICOLLECTION_A_NDAPI ll::Expected<bool> has(const std::string& id);

        LOICOLLECTION_A_NDAPI bool isValid();

    public:
        LOICOLLECTION_A_NDAPI std::string getName() override;

        LOICOLLECTION_A_NDAPI modules::ModulePriority getPriority() override;

        LOICOLLECTION_A_API   ll::Expected<bool> load() override;
        LOICOLLECTION_A_API   ll::Expected<bool> unload() override;
        LOICOLLECTION_A_API   ll::Expected<bool> registry() override;
        LOICOLLECTION_A_API   ll::Expected<bool> unregistry() override;

    private:
        CdkPlugin();

        ll::Expected<void> registeryUI();

        void registeryCommand();

        struct operation;

        struct Impl;
        std::unique_ptr<Impl> mImpl;
    };
}
