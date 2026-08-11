#include <atomic>
#include <memory>
#include <vector>
#include <string>
#include <filesystem>

#include <fmt/core.h>
#include <nlohmann/json.hpp>

#include <ll/api/io/Logger.h>
#include <ll/api/io/LoggerRegistry.h>
#include <ll/api/command/Command.h>
#include <ll/api/command/CommandHandle.h>
#include <ll/api/command/CommandRegistrar.h>
#include <ll/api/command/EnumName.h>
#include <ll/api/event/EventBus.h>
#include <ll/api/event/ListenerBase.h>
#include <ll/api/event/player/PlayerJoinEvent.h>
#include <ll/api/event/player/PlayerUseItemEvent.h>
#include <ll/api/utils/StringUtils.h>
#include <ll/api/utils/HashUtils.h>

#include <mc/world/item/ItemStack.h>
#include <mc/world/actor/player/Player.h>

#include <mc/deps/core/string/HashedString.h>

#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandVersion.h>
#include <mc/server/commands/CommandPermissionLevel.h>

#include "LOICollectionA/include/server/APIUtils.h"
#include "LOICollectionA/include/server/Plugins/LanguagePlugin.h"

#include "LOICollectionA/include/server/Events/modules/MenuEvent.h"

#include "LOICollectionA/utils/I18nUtils.h"
#include "LOICollectionA/utils/mc-server/CommandUtils.h"
#include "LOICollectionA/utils/mc-server/InventoryUtils.h"
#include "LOICollectionA/utils/mc-server/ScoreboardUtils.h"
#include "LOICollectionA/utils/core/SystemUtils.h"

#include "LOICollectionA/data/JsonStorage.h"

#include "LOICollectionA/base/Wrapper.h"
#include "LOICollectionA/base/ServiceProvider.h"

#include "LOICollectionA/frontend/AST.h"

#include "LOICollectionA/include/form/GUIManager.h"

#include "LOICollectionA/ConfigPlugin.h"

#include "LOICollectionA/include/server/Plugins/MenuPlugin.h"

using I18nUtilsTools::tr;

namespace LOICollection::server::Plugins {
    enum class MenuObject;

    constexpr inline auto MenuObjectName = ll::command::enum_name_v<MenuObject>;

    struct MenuPlugin::operation {
        ll::command::SoftEnum<MenuObject> Object;
    };

    struct MenuPlugin::Impl {
        std::atomic<bool> mRegistered{ false };

        Config::C_Menu options;
        
        std::shared_ptr<JsonStorage> db;
        std::shared_ptr<ll::io::Logger> logger;

        std::filesystem::path mGuiPath;
        
        ll::event::ListenerPtr PlayerJoinEventListener;
        ll::event::ListenerPtr PlayerUseItemEventListener;
        ll::event::ListenerPtr MenuCreateEventListener;
        ll::event::ListenerPtr MenuDeleteEventListener;
    };

    MenuPlugin::MenuPlugin() : mImpl(std::make_unique<Impl>()), mGui(std::make_unique<MenuGui>(*this)) {};
    MenuPlugin::~MenuPlugin() = default;

    std::shared_ptr<MenuPlugin> MenuPlugin::getShared() {
        static auto instance = std::shared_ptr<MenuPlugin>(new MenuPlugin());
        return instance;
    }

    std::error_code MenuPlugin::makeErrorCode(MenuPluginErrorCode e) {
        static MenuPluginErrorCategory cat;
        return std::error_code{ static_cast<int>(e), cat };
    }

    std::shared_ptr<JsonStorage> MenuPlugin::getDatabase() {
        return this->mImpl->db;
    }

    std::shared_ptr<ll::io::Logger> MenuPlugin::getLogger() {
        return this->mImpl->logger;
    }

    void MenuPlugin::registeryCommand() {
        ll::command::CommandRegistrar::getInstance(false).tryRegisterSoftEnum(MenuObjectName, this->getDatabase()->keys());

        ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance(false)
            .getOrCreateCommand("menu", tr({}, "commands.menu.description"), CommandPermissionLevel::Any, CommandFlagValue::NotCheat | CommandFlagValue::Async);
        command.overload<operation>().text("gui").optional("Object").execute(
            [this](CommandOrigin const& origin, CommandOutput& output, operation const& param) -> void {
                Actor* entity = origin.getEntity();
                if (entity == nullptr || !entity->isType(ActorType::Player))
                    return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));
                Player& player = *static_cast<Player*>(entity);

                this->mGui->open(player, param.Object.empty() ? 
                    this->mImpl->options.EntranceKey : std::string(param.Object)
                ).or_else([](ll::Error e) -> ll::Expected<void> {
                    if (e.isA<ll::ErrorCodeError>()
                        && (e.as<ll::ErrorCodeError>().ec == makeErrorCode(MenuPluginErrorCode::NotFound)
                            || e.as<ll::ErrorCodeError>().ec == makeErrorCode(MenuPluginErrorCode::PermissionDenied)))
                        return {};

                    return ll::Unexpected(e);
                })
                .or_else(modules::defaultErrorHandler<MenuPlugin>);
                
                output.success(fmt::runtime(tr(origin.getLocaleCode(), "commands.generic.ui")), player.getRealName());
            });
        command.overload().text("edit").execute([](CommandOrigin const& origin, CommandOutput& output) -> void {
            if (origin.getPermissionsLevel() < CommandPermissionLevel::GameDirectors)
                return output.error(tr(origin.getLocaleCode(), "commands.generic.permission"));
            
            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isType(ActorType::Player))
                return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));
            Player& player = *static_cast<Player*>(entity);

            form::GUIManager::getInstance().open("menu", "menu.edit", form::GUIManagerType::CustomForm, player)
                .or_else(modules::defaultErrorHandler<MenuPlugin>);

            output.success(fmt::runtime(tr(origin.getLocaleCode(), "commands.generic.ui")), player.getRealName());
        });
        command.overload().text("clock").execute([this](CommandOrigin const& origin, CommandOutput& output) -> void {
            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isType(ActorType::Player))
                return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));
            Player& player = *static_cast<Player*>(entity);

            auto itemStack = std::make_unique<ItemStack>();
            itemStack->reinit(this->mImpl->options.MenuItemId, 1, 0);
            
            if (!itemStack || itemStack->isNull())
                return output.error(tr(origin.getLocaleCode(), "commands.menu.error.item.null"));
            if (InventoryUtils::isItemInInventory(player, this->mImpl->options.MenuItemId, 1))
                return output.error(fmt::runtime(tr(origin.getLocaleCode(), "commands.menu.error.item.give")), player.getRealName());

            InventoryUtils::giveItem(player, *itemStack, 1);
            player.refreshInventory();

            output.success(fmt::runtime(tr(origin.getLocaleCode(), "commands.generic.ui")), player.getRealName());
        });
    }

    ll::Expected<void> MenuPlugin::registeryUI() {
        return form::GUIManager::getInstance().load("menu", (this->mImpl->mGuiPath / "menu.lcui").string())
            .transform([this]() -> void {
                form::GUIManager::getInstance().registerValue("menu.edit.keys", [this](Player&) -> ll::Expected<frontend::ArrayRef> {
                    auto values = std::make_shared<frontend::ArrayValue>();

                    for (const std::string& id : this->getDatabase()->keys())
                        values->elements.emplace_back(id);

                    return values;
                });

                form::GUIManager::getInstance().registerRequest("menu.edit.type", [this](frontend::ArrayRef args, Player&) -> ll::Expected<frontend::ArrayRef> {
                    if (args->elements.size() != 1 || !std::holds_alternative<std::string>(args->elements[0]))
                        return ll::makeStringError("menu.edit.type: must take exactly one string parameter");

                    auto values = std::make_shared<frontend::ArrayValue>();
                    values->elements.emplace_back(this->getDatabase()->get_ptr<std::string>("/" + std::get<std::string>(args->elements[0]) + "/type").value_or("Custom"));

                    return values;
                });

                form::GUIManager::getInstance().registerRequest("menu.edit.new.submit", [this](frontend::ArrayRef args, Player& player) -> ll::Expected<frontend::ArrayRef> {
                    if (args->elements.size() != 8 ||
                        !std::holds_alternative<std::string>(args->elements[0]) ||
                        !std::holds_alternative<std::string>(args->elements[1]) ||
                        !std::holds_alternative<std::string>(args->elements[2]) ||
                        !std::holds_alternative<std::string>(args->elements[3]) ||
                        !std::holds_alternative<std::string>(args->elements[4]) ||
                        !std::holds_alternative<std::string>(args->elements[5]) ||
                        !std::holds_alternative<std::string>(args->elements[6]) ||
                        !std::holds_alternative<float>(args->elements[7]))
                        return ll::makeStringError("menu.edit.new.submit: invalid parameters");

                    auto values = std::make_shared<frontend::ArrayValue>();

                    std::string type = std::get<std::string>(args->elements[0]);
                    std::string mObjectId = std::get<std::string>(args->elements[1]);
                    std::string mObjectTitle = std::get<std::string>(args->elements[2]);

                    if (mObjectId.empty() || mObjectTitle.empty()) {
                        return LanguagePlugin::getShared()->getLanguage(player)
                            .and_then([&player, values](const std::string& language) -> ll::Expected<frontend::ArrayRef> {
                                player.sendMessage(tr(language, "generic.tips.noinput"));

                                values->elements.emplace_back(false);
                                return values;
                            });
                    }

                    nlohmann::ordered_json mData = {
                        { "title", mObjectTitle },
                        { "info", nlohmann::ordered_json::object() },
                        { "permission", static_cast<int>(std::get<float>(args->elements[7])) }
                    };

                    if (type == "Simple") {
                        mData.update({
                            { "content", std::get<std::string>(args->elements[3]) },
                            { "customize", nlohmann::ordered_json::array() },
                            { "type", "Simple" }
                        });
                        mData["info"].update({
                            { "exit", std::get<std::string>(args->elements[4]) },
                            { "score", std::get<std::string>(args->elements[5]) },
                            { "permission", std::get<std::string>(args->elements[6]) }
                        });
                    } else if (type == "Modal") {
                        mData.update({
                            { "content", std::get<std::string>(args->elements[3]) },
                            { "confirmButton", nlohmann::ordered_json::object() },
                            { "cancelButton", nlohmann::ordered_json::object() },
                            { "type", "Modal" }
                        });
                        mData["info"].update({
                            { "score", std::get<std::string>(args->elements[5]) },
                            { "permission", std::get<std::string>(args->elements[6]) }
                        });
                    } else {
                        mData.update({
                            { "customize", nlohmann::ordered_json::array() },
                            { "run", nlohmann::ordered_json::array() },
                            { "type", "Custom" }
                        });
                        mData["info"].update({
                            { "exit", std::get<std::string>(args->elements[4]) }
                        });
                    }

                    return this->create(mObjectId, mData)
                        .transform([this, &player, mObjectId, values]() -> frontend::ArrayRef {
                            this->getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "menu.log1"), player)), mObjectId);

                            values->elements.emplace_back(true);
                            return values;
                        });
                });

                form::GUIManager::getInstance().registerCallback("menu.edit.remove", [this](frontend::ArrayRef args, Player& player) -> ll::Expected<void> {
                    if (args->elements.size() != 1 || !std::holds_alternative<std::string>(args->elements[0]))
                        return ll::makeStringError("menu.edit.remove: must take exactly one string parameter");

                    std::string id = std::get<std::string>(args->elements[0]);

                    return this->remove(id)
                        .transform([this, id, &player]() -> void {
                            this->getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "menu.log2"), player)), id);
                        });
                });

                form::GUIManager::getInstance().registerRequest("menu.edit.award.setting.data", [this](frontend::ArrayRef args, Player&) -> ll::Expected<frontend::ArrayRef> {
                    if (args->elements.size() != 2 ||
                        !std::holds_alternative<std::string>(args->elements[0]) ||
                        !std::holds_alternative<std::string>(args->elements[1]))
                        return ll::makeStringError("menu.edit.award.setting.data: must take exactly two string parameters");

                    auto id = std::get<std::string>(args->elements[0]);
                    auto values = std::make_shared<frontend::ArrayValue>();

                    values->elements.emplace_back(this->getDatabase()->get_ptr<std::string>("/" + id + "/title").value_or(""));
                    values->elements.emplace_back(this->getDatabase()->get_ptr<std::string>("/" + id + "/content").value_or(""));
                    values->elements.emplace_back(this->getDatabase()->get_ptr<std::string>("/" + id + "/info/exit").value_or(""));
                    values->elements.emplace_back(this->getDatabase()->get_ptr<std::string>("/" + id + "/info/score").value_or(""));
                    values->elements.emplace_back(this->getDatabase()->get_ptr<std::string>("/" + id + "/info/permission").value_or(""));

                    return values;
                });

                form::GUIManager::getInstance().registerRequest("menu.edit.award.setting.submit", [this](frontend::ArrayRef args, Player& player) -> ll::Expected<frontend::ArrayRef> {
                    if (args->elements.size() != 8 ||
                        !std::holds_alternative<std::string>(args->elements[0]) ||
                        !std::holds_alternative<std::string>(args->elements[1]) ||
                        !std::holds_alternative<std::string>(args->elements[2]) ||
                        !std::holds_alternative<std::string>(args->elements[3]) ||
                        !std::holds_alternative<std::string>(args->elements[4]) ||
                        !std::holds_alternative<std::string>(args->elements[5]) ||
                        !std::holds_alternative<std::string>(args->elements[6]) ||
                        !std::holds_alternative<float>(args->elements[7]))
                        return ll::makeStringError("menu.edit.award.setting.submit: invalid parameters");

                    auto values = std::make_shared<frontend::ArrayValue>();

                    std::string id = std::get<std::string>(args->elements[0]);
                    std::string type = std::get<std::string>(args->elements[1]);
                    std::string mObjectTitle = std::get<std::string>(args->elements[2]);

                    if (mObjectTitle.empty()) {
                        return LanguagePlugin::getShared()->getLanguage(player)
                            .and_then([&player, values](const std::string& language) -> ll::Expected<frontend::ArrayRef> {
                                player.sendMessage(tr(language, "generic.tips.noinput"));

                                values->elements.emplace_back(false);
                                return values;
                            });
                    }

                    this->getDatabase()->set_ptr("/" + id + "/title", mObjectTitle);

                    if (type == "Simple") {
                        this->getDatabase()->set_ptr("/" + id + "/content", std::get<std::string>(args->elements[3]));
                        this->getDatabase()->set_ptr("/" + id + "/info/exit", std::get<std::string>(args->elements[4]));
                        this->getDatabase()->set_ptr("/" + id + "/info/score", std::get<std::string>(args->elements[5]));
                        this->getDatabase()->set_ptr("/" + id + "/info/permission", std::get<std::string>(args->elements[6]));
                    } else if (type == "Modal") {
                        this->getDatabase()->set_ptr("/" + id + "/content", std::get<std::string>(args->elements[3]));
                        this->getDatabase()->set_ptr("/" + id + "/info/score", std::get<std::string>(args->elements[5]));
                        this->getDatabase()->set_ptr("/" + id + "/info/permission", std::get<std::string>(args->elements[6]));
                    } else {
                        this->getDatabase()->set_ptr("/" + id + "/info/exit", std::get<std::string>(args->elements[4]));
                    }

                    this->getDatabase()->set_ptr("/" + id + "/permission", static_cast<int>(std::get<float>(args->elements[7])));

                    return this->getDatabase()->save()
                        .transform([this, id, &player, values]() -> frontend::ArrayRef {
                            this->getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "menu.log5"), player)), id);

                            values->elements.emplace_back(true);
                            return values;
                        });
                });

                form::GUIManager::getInstance().registerRequest("menu.edit.award.new.submit", [this](frontend::ArrayRef args, Player& player) -> ll::Expected<frontend::ArrayRef> {
                    if (args->elements.size() != 11 ||
                        !std::holds_alternative<std::string>(args->elements[0]) ||
                        !std::holds_alternative<std::string>(args->elements[1]) ||
                        !std::holds_alternative<std::string>(args->elements[2]) ||
                        !std::holds_alternative<std::string>(args->elements[3]) ||
                        !std::holds_alternative<std::string>(args->elements[4]) ||
                        !std::holds_alternative<std::string>(args->elements[5]) ||
                        !std::holds_alternative<std::string>(args->elements[6]) ||
                        !std::holds_alternative<std::string>(args->elements[7]) ||
                        !std::holds_alternative<float>(args->elements[8]) ||
                        !std::holds_alternative<float>(args->elements[9]) ||
                        !std::holds_alternative<float>(args->elements[10]))
                        return ll::makeStringError("menu.edit.award.new.submit: invalid parameters");

                    auto values = std::make_shared<frontend::ArrayValue>();

                    std::string id = std::get<std::string>(args->elements[0]);
                    std::string type = std::get<std::string>(args->elements[1]);
                    std::string mObjectId = std::get<std::string>(args->elements[2]);
                    std::string mObjectTitle = std::get<std::string>(args->elements[3]);
                    std::string mObjectImage = std::get<std::string>(args->elements[4]);
                    std::string mObjectObjective = std::get<std::string>(args->elements[5]);
                    std::string mObjectScore = std::get<std::string>(args->elements[6]);
                    std::string mObjectRun = std::get<std::string>(args->elements[7]);

                    if ((type == "Simple" && (mObjectId.empty() || mObjectTitle.empty() || mObjectImage.empty())) ||
                        (type == "Modal" && mObjectTitle.empty())) {
                        return LanguagePlugin::getShared()->getLanguage(player)
                            .and_then([&player, values](const std::string& language) -> ll::Expected<frontend::ArrayRef> {
                                player.sendMessage(tr(language, "generic.tips.noinput"));

                                values->elements.emplace_back(false);
                                return values;
                            });
                    }

                    auto mData = this->getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id).value_or(nlohmann::ordered_json{});
                    std::string mType = std::get<float>(args->elements[8]) == 0 ? "button" : "from";

                    if (type == "Simple") {
                        nlohmann::ordered_json data = {
                            { "title", mObjectTitle },
                            { "image", mObjectImage },
                            { "id", mObjectId },
                            { "scores", nlohmann::ordered_json::object() },
                            { "run", mObjectRun },
                            { "type", mType },
                            { "permission", static_cast<int>(std::get<float>(args->elements[10])) }
                        };

                        if (!mObjectObjective.empty() && ScoreboardUtils::hasScoreboard(mObjectObjective))
                            data["scores"][mObjectObjective] = SystemUtils::toInt((mObjectScore.empty() ? "100" : mObjectScore), 0);

                        mData["customize"].push_back(data);
                    } else if (type == "Modal") {
                        nlohmann::ordered_json data = {
                            { "title", mObjectTitle },
                            { "scores", nlohmann::ordered_json::object() },
                            { "run", mObjectRun },
                            { "type", mType },
                            { "permission", static_cast<int>(std::get<float>(args->elements[10])) }
                        };

                        if (!mObjectObjective.empty() && ScoreboardUtils::hasScoreboard(mObjectObjective))
                            data["scores"][mObjectObjective] = SystemUtils::toInt((mObjectScore.empty() ? "100" : mObjectScore), 0);

                        (std::get<float>(args->elements[9]) == 0 ? mData["confirmButton"] : mData["cancelButton"]) = data;
                    }

                    this->getDatabase()->set_ptr("/" + id, mData);

                    return this->getDatabase()->save()
                        .transform([this, id, &player, values]() -> frontend::ArrayRef {
                            this->getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "menu.log6"), player)), id);

                            values->elements.emplace_back(true);
                            return values;
                        });
                });

                form::GUIManager::getInstance().registerRequest("menu.edit.award.packages", [this](frontend::ArrayRef args, Player&) -> ll::Expected<frontend::ArrayRef> {
                    if (args->elements.size() != 1 || !std::holds_alternative<std::string>(args->elements[0]))
                        return ll::makeStringError("menu.edit.award.packages: must take exactly one string parameter");

                    auto id = std::get<std::string>(args->elements[0]);
                    auto values = std::make_shared<frontend::ArrayValue>();

                    for (nlohmann::ordered_json& item : this->getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id + "/customize").value_or(nlohmann::ordered_json::array()))
                        values->elements.emplace_back(item.value("id", ""));

                    return values;
                });

                form::GUIManager::getInstance().registerCallback("menu.edit.award.remove", [this](frontend::ArrayRef args, Player& player) -> ll::Expected<void> {
                    if (args->elements.size() != 2 ||
                        !std::holds_alternative<std::string>(args->elements[0]) ||
                        !std::holds_alternative<std::string>(args->elements[1]))
                        return ll::makeStringError("menu.edit.award.remove: must take exactly two string parameters");

                    auto id = std::get<std::string>(args->elements[0]);
                    auto packageid = std::get<std::string>(args->elements[1]);

                    auto mContent = this->getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id + "/customize").value_or(nlohmann::ordered_json::array());
                    for (int i = static_cast<int>(mContent.size() - 1); i >= 0; i--) {
                        if (mContent.at(i).value("id", "") == packageid)
                            mContent.erase(i);
                    }

                    this->getDatabase()->set_ptr("/" + id + "/customize", mContent);

                    return this->getDatabase()->save()
                        .transform([this, id, packageid, &player]() -> void {
                            this->getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "menu.log3"), player)), id, packageid);
                        });
                });

                form::GUIManager::getInstance().registerRequest("menu.edit.award.command.lines", [this](frontend::ArrayRef args, Player& player) -> ll::Expected<frontend::ArrayRef> {
                    if (args->elements.size() != 1 || !std::holds_alternative<std::string>(args->elements[0]))
                        return ll::makeStringError("menu.edit.award.command.lines: must take exactly one string parameter");

                    auto id = std::get<std::string>(args->elements[0]);
                    auto content = this->getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id + "/run").value_or(nlohmann::ordered_json::array());

                    return LanguagePlugin::getShared()->getLanguage(player)
                        .transform([content = std::move(content)](const std::string& language) -> frontend::ArrayRef {
                            auto values = std::make_shared<frontend::ArrayValue>();

                            for (auto& line : content) {
                                std::string text = line.get<std::string>();
                                values->elements.emplace_back(text.empty() ? tr(language, "menu.gui.button3.command.empty") : text);
                            }

                            return values;
                        });
                });

                form::GUIManager::getInstance().registerRequest("menu.edit.award.command.line.data", [this](frontend::ArrayRef args, Player&) -> ll::Expected<frontend::ArrayRef> {
                    if (args->elements.size() != 2 ||
                        !std::holds_alternative<std::string>(args->elements[0]) ||
                        !std::holds_alternative<int>(args->elements[1]))
                        return ll::makeStringError("menu.edit.award.command.line.data: must take one string and one int parameter");

                    auto id = std::get<std::string>(args->elements[0]);
                    int index = std::get<int>(args->elements[1]);

                    auto content = this->getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id + "/run").value_or(nlohmann::ordered_json::array());
                    auto values = std::make_shared<frontend::ArrayValue>();

                    values->elements.emplace_back(index >= 0 && index < static_cast<int>(content.size()) ? content.at(index).get<std::string>() : "");

                    return values;
                });

                form::GUIManager::getInstance().registerCallback("menu.edit.award.command.line.submit", [this](frontend::ArrayRef args, Player& player) -> ll::Expected<void> {
                    if (args->elements.size() != 3 ||
                        !std::holds_alternative<std::string>(args->elements[0]) ||
                        !std::holds_alternative<int>(args->elements[1]) ||
                        !std::holds_alternative<std::string>(args->elements[2]))
                        return ll::makeStringError("menu.edit.award.command.line.submit: invalid parameters");

                    auto id = std::get<std::string>(args->elements[0]);
                    int index = std::get<int>(args->elements[1]);
                    std::string line = std::get<std::string>(args->elements[2]);

                    auto content = this->getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id + "/run").value_or(nlohmann::ordered_json::array());

                    if (index < 0 || index >= static_cast<int>(content.size()))
                        content.push_back(line);
                    else
                        content[index] = line;

                    this->getDatabase()->set_ptr("/" + id + "/run", content);

                    return this->getDatabase()->save()
                        .transform([this, id, &player]() -> void {
                            this->getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "menu.log4"), player)), id);
                        });
                });

                form::GUIManager::getInstance().registerCallback("menu.edit.award.command.remove.last", [this](frontend::ArrayRef args, Player& player) -> ll::Expected<void> {
                    if (args->elements.size() != 1 || !std::holds_alternative<std::string>(args->elements[0]))
                        return ll::makeStringError("menu.edit.award.command.remove.last: must take exactly one string parameter");

                    auto id = std::get<std::string>(args->elements[0]);
                    auto content = this->getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id + "/run").value_or(nlohmann::ordered_json::array());

                    if (!content.empty())
                        content.erase(content.end() - 1);

                    this->getDatabase()->set_ptr("/" + id + "/run", content);

                    return this->getDatabase()->save()
                        .transform([this, id, &player]() -> void {
                            this->getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "menu.log4"), player)), id);
                        });
                });
            });
    }

    void MenuPlugin::listenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        this->mImpl->PlayerUseItemEventListener = eventBus.emplaceListener<ll::event::PlayerUseItemEvent>([this](ll::event::PlayerUseItemEvent& event) -> void {
            if (event.self().isSimulatedPlayer())
                return;

            if (event.item().getTypeName() == this->mImpl->options.MenuItemId) {
                this->mGui->open(event.self(), "main").or_else([](ll::Error e) -> ll::Expected<void> {
                    if (e.isA<ll::ErrorCodeError>()
                        && (e.as<ll::ErrorCodeError>().ec == makeErrorCode(MenuPluginErrorCode::InsufficientScore)
                            || e.as<ll::ErrorCodeError>().ec == makeErrorCode(MenuPluginErrorCode::PermissionDenied)))
                        return {};

                    return ll::Unexpected(e);
                })
                .or_else(modules::defaultErrorHandler<MenuPlugin>);
            }
        });

        this->mImpl->PlayerJoinEventListener = eventBus.emplaceListener<ll::event::PlayerJoinEvent>([this](ll::event::PlayerJoinEvent& event) -> void {
            if (event.self().isSimulatedPlayer())
                return;

            auto itemStack = std::make_unique<ItemStack>();
            itemStack->reinit(this->mImpl->options.MenuItemId, 1, 0);

            if (!itemStack || InventoryUtils::isItemInInventory(event.self(), this->mImpl->options.MenuItemId, 1))
                return;
            
            InventoryUtils::giveItem(event.self(), *itemStack, 1);
            event.self().refreshInventory();
        });

        this->mImpl->MenuCreateEventListener = eventBus.emplaceListener<LOICollection::server::Events::MenuCreateEvent>([](LOICollection::server::Events::MenuCreateEvent& event) -> void {
            ll::command::CommandRegistrar::getInstance(false).addSoftEnumValues(MenuObjectName, { event.getTarget() });
        });

        this->mImpl->MenuDeleteEventListener = eventBus.emplaceListener<LOICollection::server::Events::MenuDeleteEvent>([](LOICollection::server::Events::MenuDeleteEvent& event) -> void {
            ll::command::CommandRegistrar::getInstance(false).removeSoftEnumValues(MenuObjectName, { event.getTarget() });
        });
    }

    void MenuPlugin::unlistenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(this->mImpl->PlayerUseItemEventListener);
        eventBus.removeListener(this->mImpl->PlayerJoinEventListener);
        eventBus.removeListener(this->mImpl->MenuCreateEventListener);
        eventBus.removeListener(this->mImpl->MenuDeleteEventListener);
    }

    ll::Expected<void> MenuPlugin::create(const std::string& id, const nlohmann::ordered_json& data) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(MenuPluginErrorCode::Invalid));

        if (!this->getDatabase()->has(id))
            this->getDatabase()->set(id, data);
        
        return this->getDatabase()->save();
    }

    ll::Expected<void> MenuPlugin::remove(const std::string& id) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(MenuPluginErrorCode::Invalid));

        this->getDatabase()->remove(id);

        return this->getDatabase()->save();
    }

    ll::Expected<void> MenuPlugin::handleAction(Player& player, const nlohmann::ordered_json& action, const nlohmann::ordered_json& original) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(MenuPluginErrorCode::Invalid));

        if (action.empty())
            return {};

        if (action.contains("permission")) {
            if (static_cast<int>(player.getCommandPermissionLevel()) < action["permission"]) {
                CommandUtils::executeCommand(player, original.value("info", nlohmann::ordered_json{}).value("permission", ""));

                return ll::makeErrorCodeError(makeErrorCode(MenuPluginErrorCode::PermissionDenied));
            }
        }

        if (action.contains("scores")) {
            for (const auto& [key, value] : action["scores"].items()) {
                if (value.get<int>() > ScoreboardUtils::getScore(player, key)) {
                   CommandUtils::executeCommand(player, original.value("info", nlohmann::ordered_json{}).value("score", ""));

                   return ll::makeErrorCodeError(makeErrorCode(MenuPluginErrorCode::InsufficientScore));
                }
            }
            for (const auto& [key, value] : action["scores"].items())
                ScoreboardUtils::reduceScore(player, key, value.get<int>());
        }

        if (action.value("type", "") == "button") {
            if (action["run"].is_string()) {
                CommandUtils::executeCommand(player, action["run"].get<std::string>());

                return {};
            }
            
            for (const auto& cmd : action["run"])
                CommandUtils::executeCommand(player, cmd.get<std::string>());
            
            return {};
        } 

        return this->mGui->open(player, action.value("run", "")).or_else([](ll::Error e) -> ll::Expected<void> {
            if (e.isA<ll::ErrorCodeError>()
                && (e.as<ll::ErrorCodeError>().ec == MenuPlugin::makeErrorCode(MenuPluginErrorCode::InsufficientScore)
                    || e.as<ll::ErrorCodeError>().ec == MenuPlugin::makeErrorCode(MenuPluginErrorCode::PermissionDenied)))
                return {};

            return ll::Unexpected(e);
        });
    }

    ll::Expected<bool> MenuPlugin::has(const std::string& id) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(MenuPluginErrorCode::Invalid));

        return this->getDatabase()->has(id);
    }

    bool MenuPlugin::isValid() {
        return this->getLogger() != nullptr && this->getDatabase() != nullptr;
    }

    std::string MenuPlugin::getName() {
        return "MenuPlugin";
    }

    modules::ModulePriority MenuPlugin::getPriority() {
        return modules::ModulePriority::High;
    }

    ll::Expected<bool> MenuPlugin::load() {
        if (!ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().ServerConfig.Plugins.Menu.ModuleEnabled)
            return false;

        auto mDataPath = std::filesystem::path(ServiceProvider::getInstance().getService<std::string>("ConfigPath")->data());

        this->mImpl->db = std::make_shared<JsonStorage>(mDataPath / "menu.json");
        this->mImpl->logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");
        this->mImpl->options = ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().ServerConfig.Plugins.Menu;
        this->mImpl->mGuiPath = std::filesystem::path(ServiceProvider::getInstance().getService<std::string>("GuiPath")->data());

        return this->mImpl->db->load()
            .transform([]() -> bool {
                return true;
            });
    }

    ll::Expected<bool> MenuPlugin::unload() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;

        this->mImpl->db.reset();
        this->mImpl->logger.reset();
        this->mImpl->options = {};

        if (this->mImpl->mRegistered.load(std::memory_order_acquire))
            this->unlistenEvent();

        return true;
    }

    ll::Expected<bool> MenuPlugin::registry() {     
        if (!this->mImpl->options.ModuleEnabled)
            return false;

        return this->registeryUI()
            .transform([this]() -> bool {
                this->registeryCommand();
                this->listenEvent();

                this->mImpl->mRegistered.store(true, std::memory_order_release);

                return true;
            });
    }

    ll::Expected<bool> MenuPlugin::unregistry() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;
        
        this->unlistenEvent();

        return this->getDatabase()->save()
            .transform([this]() -> bool {
                this->mImpl->mRegistered.store(false, std::memory_order_release);

                return true;
            });
    }
}
