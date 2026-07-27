#pragma once

#include <string>

#include <nlohmann/json_fwd.hpp>

#include <ll/api/Expected.h>

#include "LOICollectionA/base/Macro.h"

#include "LOICollectionA/include/ModuleBase.h"
#include "LOICollectionA/include/ModManager.h"

#include "LOICollectionA/include/server/Plugins/gui/ShopGui.h"
#include "LOICollectionA/include/server/Plugins/types/ShopType.h"

class Player;
class JsonStorage;

namespace ll::io {
    class Logger;
}

namespace LOICollection::server::Plugins {
    enum class ShopPluginErrorCode : int {
        Invalid = 1,
        NotFound = 2,
        UnknownType = 3
    };

    struct ShopPluginErrorCategory : std::error_category {
        [[nodiscard]] const char* name() const noexcept override {
            return "ShopPluginError";
        }

        [[nodiscard]] std::string message(int ev) const override {
            switch (static_cast<ShopPluginErrorCode>(ev)) {
                case ShopPluginErrorCode::Invalid: return "Plugin is invalid";
                case ShopPluginErrorCode::NotFound: return "Shop data not found";
                case ShopPluginErrorCode::UnknownType: return "Unknown shop type";
                default:
                    return "Unknown";
            }
        }
    };

    class ShopPlugin : public std::enable_shared_from_this<ShopPlugin>,
                       public modules::ModuleBase,
                       public modules::AutoRegister<ShopPlugin> {
    public:
        ~ShopPlugin();

        ShopPlugin(ShopPlugin const&) = delete;
        ShopPlugin(ShopPlugin&&) = delete;
        ShopPlugin& operator=(ShopPlugin const&) = delete;
        ShopPlugin& operator=(ShopPlugin&&) = delete;

    public:
        LOICOLLECTION_A_NDAPI static std::shared_ptr<ShopPlugin> getShared();
        LOICOLLECTION_A_NDAPI static std::error_code makeErrorCode(ShopPluginErrorCode e);

        LOICOLLECTION_A_NDAPI std::shared_ptr<JsonStorage> getDatabase();
        LOICOLLECTION_A_NDAPI std::shared_ptr<ll::io::Logger> getLogger();

        LOICOLLECTION_A_NDAPI ll::Expected<void> create(const std::string& id, const nlohmann::ordered_json& data);
        LOICOLLECTION_A_NDAPI ll::Expected<void> remove(const std::string& id);

        LOICOLLECTION_A_NDAPI ll::Expected<bool> commodity(Player& player, int number, const nlohmann::ordered_json& data, ShopType type);
        LOICOLLECTION_A_NDAPI ll::Expected<bool> title(Player& player, const nlohmann::ordered_json& data, ShopType type);

        LOICOLLECTION_A_NDAPI ll::Expected<bool> checkModifiedData(Player& player, nlohmann::ordered_json data, int number);
        
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
        ShopPlugin();

        void registeryCommand();
        void listenEvent();
        void unlistenEvent();

        struct operation;

        struct Impl;
        std::unique_ptr<Impl> mImpl;
        std::unique_ptr<ShopGui> mGui;
    };
}