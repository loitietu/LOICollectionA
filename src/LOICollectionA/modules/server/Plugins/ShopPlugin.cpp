#include <atomic>
#include <memory>
#include <string>
#include <filesystem>

#include <fmt/core.h>
#include <nlohmann/json.hpp>

#include <ll/api/Expected.h>
#include <ll/api/io/Logger.h>
#include <ll/api/io/LoggerRegistry.h>
#include <ll/api/command/Command.h>
#include <ll/api/command/CommandHandle.h>
#include <ll/api/command/CommandRegistrar.h>
#include <ll/api/command/EnumName.h>
#include <ll/api/event/EventBus.h>
#include <ll/api/event/ListenerBase.h>
#include <ll/api/utils/HashUtils.h>

#include <mc/deps/nbt/Tag.h>
#include <mc/deps/nbt/CompoundTag.h>

#include <mc/deps/core/string/HashedString.h>

#include <mc/world/item/ItemStack.h>
#include <mc/world/actor/player/Player.h>

#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandVersion.h>
#include <mc/server/commands/CommandPermissionLevel.h>

#include "LOICollectionA/include/server/APIUtils.h"
#include "LOICollectionA/include/server/Plugins/LanguagePlugin.h"
#include "LOICollectionA/include/server/Plugins/ChatPlugin.h"

#include "LOICollectionA/include/server/Events/modules/ShopEvent.h"

#include "LOICollectionA/utils/I18nUtils.h"
#include "LOICollectionA/utils/mc-server/InventoryUtils.h"
#include "LOICollectionA/utils/mc-server/ScoreboardUtils.h"
#include "LOICollectionA/utils/core/SystemUtils.h"

#include "LOICollectionA/data/JsonStorage.h"

#include "LOICollectionA/base/Wrapper.h"
#include "LOICollectionA/base/ServiceProvider.h"

#include "LOICollectionA/frontend/AST.h"

#include "LOICollectionA/include/form/GUIManager.h"

#include "LOICollectionA/ConfigPlugin.h"

#include "LOICollectionA/include/server/Plugins/ShopPlugin.h"

using I18nUtilsTools::tr;

namespace LOICollection::server::Plugins {
    enum class ShopObject;

    constexpr inline auto ShopObjectName = ll::command::enum_name_v<ShopObject>;

    struct ShopPlugin::operation {
        ll::command::SoftEnum<ShopObject> Object;
    };

    struct ShopPlugin::Impl {
        std::atomic<bool> mRegistered{ false };

        bool ModuleEnabled = false;

        std::shared_ptr<JsonStorage> db;
        std::shared_ptr<ll::io::Logger> logger;

        std::filesystem::path mGuiPath;

        ll::event::ListenerPtr ShopCreateEventListener;
        ll::event::ListenerPtr ShopDeleteEventListener;
    };

    ShopPlugin::ShopPlugin() : mImpl(std::make_unique<Impl>()), mGui(std::make_unique<ShopGui>(*this)) {};
    ShopPlugin::~ShopPlugin() = default;

    std::shared_ptr<ShopPlugin> ShopPlugin::getShared() {
        static auto instance = std::shared_ptr<ShopPlugin>(new ShopPlugin());
        return instance;
    }

    std::error_code ShopPlugin::makeErrorCode(ShopPluginErrorCode e) {
        static ShopPluginErrorCategory cat;
        return std::error_code{ static_cast<int>(e), cat };
    }
    
    std::shared_ptr<JsonStorage> ShopPlugin::getDatabase() {
        return this->mImpl->db;
    }

    std::shared_ptr<ll::io::Logger> ShopPlugin::getLogger() {
        return this->mImpl->logger;
    }

    void ShopPlugin::registeryCommand() {
        ll::command::CommandRegistrar::getInstance(false).tryRegisterSoftEnum(ShopObjectName, this->getDatabase()->keys());

        ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance(false)
            .getOrCreateCommand("shop", tr({}, "commands.shop.description"), CommandPermissionLevel::Any, CommandFlagValue::NotCheat | CommandFlagValue::Async);
        command.overload<operation>().text("gui").required("Object").execute(
            [this](CommandOrigin const& origin, CommandOutput& output, operation const& param) -> void {
                Actor* entity = origin.getEntity();
                if (entity == nullptr || !entity->isType(ActorType::Player))
                    return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));
                Player& player = *static_cast<Player*>(entity);

                this->mGui->open(player, param.Object).or_else(modules::defaultErrorHandler<ShopPlugin>);

                output.success(fmt::runtime(tr(origin.getLocaleCode(), "commands.generic.ui")), player.getRealName());
            });
        command.overload().text("edit").execute([](CommandOrigin const& origin, CommandOutput& output) -> void {
            if (origin.getPermissionsLevel() < CommandPermissionLevel::GameDirectors)
                return output.error(tr(origin.getLocaleCode(), "commands.generic.permission"));
            
            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isType(ActorType::Player))
                return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));
            Player& player = *static_cast<Player*>(entity);
            
            form::GUIManager::getInstance().open("shop", "shop.edit", form::GUIManagerType::CustomForm, player)
                .or_else(modules::defaultErrorHandler<ShopPlugin>);

            output.success(fmt::runtime(tr(origin.getLocaleCode(), "commands.generic.ui")), player.getRealName());
        });
    }

    ll::Expected<void> ShopPlugin::registeryUI() {
        return form::GUIManager::getInstance().load("shop", (this->mImpl->mGuiPath / "shop.lcui").string())
            .transform([this]() -> void {
                form::GUIManager::getInstance().registerValue("shop.edit.keys", [this](Player&) -> ll::Expected<frontend::ArrayRef> {
                    auto values = std::make_shared<frontend::ArrayValue>();

                    for (const std::string& id : this->getDatabase()->keys())
                        values->elements.emplace_back(id);

                    return values;
                });

                form::GUIManager::getInstance().registerRequest("shop.edit.type", [this](frontend::ArrayRef args, Player&) -> ll::Expected<frontend::ArrayRef> {
                    if (args->elements.size() != 1 || !std::holds_alternative<std::string>(args->elements[0]))
                        return ll::makeStringError("shop.edit.type: must take exactly one string parameter");

                    auto values = std::make_shared<frontend::ArrayValue>();
                    values->elements.emplace_back(this->getDatabase()->get_ptr<std::string>("/" + std::get<std::string>(args->elements[0]) + "/type").value_or("buy"));

                    return values;
                });

                form::GUIManager::getInstance().registerRequest("shop.edit.new.submit", [this](frontend::ArrayRef args, Player& player) -> ll::Expected<frontend::ArrayRef> {
                    if (args->elements.size() != 8 ||
                        !std::holds_alternative<std::string>(args->elements[0]) ||
                        !std::holds_alternative<std::string>(args->elements[1]) ||
                        !std::holds_alternative<std::string>(args->elements[2]) ||
                        !std::holds_alternative<std::string>(args->elements[3]) ||
                        !std::holds_alternative<std::string>(args->elements[4]) ||
                        !std::holds_alternative<std::string>(args->elements[5]) ||
                        !std::holds_alternative<std::string>(args->elements[6]) ||
                        !std::holds_alternative<std::string>(args->elements[7]))
                        return ll::makeStringError("shop.edit.new.submit: invalid parameters");

                    auto values = std::make_shared<frontend::ArrayValue>();

                    std::string type = std::get<std::string>(args->elements[0]);
                    std::string mObjectId = std::get<std::string>(args->elements[1]);
                    std::string mObjectTitle = std::get<std::string>(args->elements[2]);
                    std::string mObjectContent = std::get<std::string>(args->elements[3]);

                    if (mObjectId.empty() || mObjectTitle.empty() || mObjectContent.empty()) {
                        return LanguagePlugin::getShared()->getLanguage(player)
                            .and_then([&player, values](const std::string& language) -> ll::Expected<frontend::ArrayRef> {
                                player.sendMessage(tr(language, "generic.tips.noinput"));

                                values->elements.emplace_back(false);
                                return values;
                            });
                    }

                    nlohmann::ordered_json data = {
                        { "title", mObjectTitle },
                        { "content", mObjectContent },
                        { "info", nlohmann::ordered_json::object() },
                        { "classiflcation", nlohmann::ordered_json::array() }
                    };

                    if (type == "buy") {
                        data["type"] = "buy";
                        data["info"].update({
                            { "exit", std::get<std::string>(args->elements[4]) },
                            { "score", std::get<std::string>(args->elements[5]) }
                        });
                    } else {
                        data["type"] = "sell";
                        data["info"].update({
                            { "exit", std::get<std::string>(args->elements[4]) },
                            { "title", std::get<std::string>(args->elements[6]) },
                            { "item", std::get<std::string>(args->elements[7]) }
                        });
                    }

                    return this->create(mObjectId, data)
                        .transform([this, &player, mObjectId, values]() -> frontend::ArrayRef {
                            this->getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "shop.log1"), player)), mObjectId);

                            values->elements.emplace_back(true);
                            return values;
                        });
                });

                form::GUIManager::getInstance().registerCallback("shop.edit.remove", [this](frontend::ArrayRef args, Player& player) -> ll::Expected<void> {
                    if (args->elements.size() != 1 || !std::holds_alternative<std::string>(args->elements[0]))
                        return ll::makeStringError("shop.edit.remove: must take exactly one string parameter");

                    std::string id = std::get<std::string>(args->elements[0]);

                    return this->remove(id)
                        .transform([this, id, &player]() -> void {
                            this->getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "shop.log2"), player)), id);
                        });
                });

                form::GUIManager::getInstance().registerRequest("shop.edit.award.setting.data", [this](frontend::ArrayRef args, Player&) -> ll::Expected<frontend::ArrayRef> {
                    if (args->elements.size() != 2 ||
                        !std::holds_alternative<std::string>(args->elements[0]) ||
                        !std::holds_alternative<std::string>(args->elements[1]))
                        return ll::makeStringError("shop.edit.award.setting.data: must take exactly two string parameters");

                    auto id = std::get<std::string>(args->elements[0]);
                    auto values = std::make_shared<frontend::ArrayValue>();

                    values->elements.emplace_back(this->getDatabase()->get_ptr<std::string>("/" + id + "/title").value_or(""));
                    values->elements.emplace_back(this->getDatabase()->get_ptr<std::string>("/" + id + "/content").value_or(""));
                    values->elements.emplace_back(this->getDatabase()->get_ptr<std::string>("/" + id + "/info/exit").value_or(""));
                    values->elements.emplace_back(this->getDatabase()->get_ptr<std::string>("/" + id + "/info/score").value_or(""));
                    values->elements.emplace_back(this->getDatabase()->get_ptr<std::string>("/" + id + "/info/title").value_or(""));
                    values->elements.emplace_back(this->getDatabase()->get_ptr<std::string>("/" + id + "/info/item").value_or(""));

                    return values;
                });

                form::GUIManager::getInstance().registerRequest("shop.edit.award.setting.submit", [this](frontend::ArrayRef args, Player& player) -> ll::Expected<frontend::ArrayRef> {
                    if (args->elements.size() != 8 ||
                        !std::holds_alternative<std::string>(args->elements[0]) ||
                        !std::holds_alternative<std::string>(args->elements[1]) ||
                        !std::holds_alternative<std::string>(args->elements[2]) ||
                        !std::holds_alternative<std::string>(args->elements[3]) ||
                        !std::holds_alternative<std::string>(args->elements[4]) ||
                        !std::holds_alternative<std::string>(args->elements[5]) ||
                        !std::holds_alternative<std::string>(args->elements[6]) ||
                        !std::holds_alternative<std::string>(args->elements[7]))
                        return ll::makeStringError("shop.edit.award.setting.submit: invalid parameters");

                    auto values = std::make_shared<frontend::ArrayValue>();

                    std::string id = std::get<std::string>(args->elements[0]);
                    std::string type = std::get<std::string>(args->elements[1]);
                    std::string mObjectTitle = std::get<std::string>(args->elements[2]);
                    std::string mObjectContent = std::get<std::string>(args->elements[3]);

                    if (mObjectTitle.empty() || mObjectContent.empty()) {
                        return LanguagePlugin::getShared()->getLanguage(player)
                            .and_then([&player, values](const std::string& language) -> ll::Expected<frontend::ArrayRef> {
                                player.sendMessage(tr(language, "generic.tips.noinput"));

                                values->elements.emplace_back(false);
                                return values;
                            });
                    }

                    this->getDatabase()->set_ptr("/" + id + "/title", mObjectTitle);
                    this->getDatabase()->set_ptr("/" + id + "/content", mObjectContent);
                    this->getDatabase()->set_ptr("/" + id + "/info/exit", std::get<std::string>(args->elements[4]));

                    if (type == "buy") {
                        this->getDatabase()->set_ptr("/" + id + "/info/score", std::get<std::string>(args->elements[5]));
                    } else {
                        this->getDatabase()->set_ptr("/" + id + "/info/title", std::get<std::string>(args->elements[6]));
                        this->getDatabase()->set_ptr("/" + id + "/info/item", std::get<std::string>(args->elements[7]));
                    }

                    return this->getDatabase()->save()
                        .transform([this, id, &player, values]() -> frontend::ArrayRef {
                            this->getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "shop.log4"), player)), id);

                            values->elements.emplace_back(true);
                            return values;
                        });
                });

                form::GUIManager::getInstance().registerRequest("shop.edit.award.packages", [this](frontend::ArrayRef args, Player&) -> ll::Expected<frontend::ArrayRef> {
                    if (args->elements.size() != 1 || !std::holds_alternative<std::string>(args->elements[0]))
                        return ll::makeStringError("shop.edit.award.packages: must take exactly one string parameter");

                    auto id = std::get<std::string>(args->elements[0]);
                    auto values = std::make_shared<frontend::ArrayValue>();

                    for (nlohmann::ordered_json& item : this->getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id + "/classiflcation").value_or(nlohmann::ordered_json::array()))
                        values->elements.emplace_back(item.value("title", ""));

                    return values;
                });

                form::GUIManager::getInstance().registerRequest("shop.edit.award.new.submit", [this](frontend::ArrayRef args, Player& player) -> ll::Expected<frontend::ArrayRef> {
                    if (args->elements.size() != 12 ||
                        !std::holds_alternative<std::string>(args->elements[0]) ||
                        !std::holds_alternative<std::string>(args->elements[1]) ||
                        !std::holds_alternative<std::string>(args->elements[2]) ||
                        !std::holds_alternative<std::string>(args->elements[3]) ||
                        !std::holds_alternative<std::string>(args->elements[4]) ||
                        !std::holds_alternative<std::string>(args->elements[5]) ||
                        !std::holds_alternative<std::string>(args->elements[6]) ||
                        !std::holds_alternative<std::string>(args->elements[7]) ||
                        !std::holds_alternative<std::string>(args->elements[8]) ||
                        !std::holds_alternative<std::string>(args->elements[9]) ||
                        !std::holds_alternative<std::string>(args->elements[10]) ||
                        !std::holds_alternative<std::string>(args->elements[11]))
                        return ll::makeStringError("shop.edit.award.new.submit: invalid parameters");

                    auto values = std::make_shared<frontend::ArrayValue>();

                    std::string id = std::get<std::string>(args->elements[0]);
                    std::string type = std::get<std::string>(args->elements[1]);
                    std::string awardType = std::get<std::string>(args->elements[2]);
                    std::string mObjectTitle = std::get<std::string>(args->elements[3]);
                    std::string mObjectImage = std::get<std::string>(args->elements[4]);

                    if (mObjectTitle.empty() || mObjectImage.empty()) {
                        return LanguagePlugin::getShared()->getLanguage(player)
                            .and_then([&player, values](const std::string& language) -> ll::Expected<frontend::ArrayRef> {
                                player.sendMessage(tr(language, "generic.tips.noinput"));

                                values->elements.emplace_back(false);
                                return values;
                            });
                    }

                    nlohmann::ordered_json data;

                    if (awardType == "commodity") {
                        std::string mObjectObjective = std::get<std::string>(args->elements[6]);
                        std::string mObjectScore = std::get<std::string>(args->elements[7]);

                        data.update({
                            { "title", mObjectTitle },
                            { "image", mObjectImage },
                            { "introduce", std::get<std::string>(args->elements[5]) },
                            { "number", std::get<std::string>(args->elements[9]) },
                            { "id", std::get<std::string>(args->elements[8]) },
                            { "scores", nlohmann::ordered_json::object() },
                            { "type", "commodity" }
                        });

                        if (!mObjectObjective.empty() && ScoreboardUtils::hasScoreboard(mObjectObjective))
                            data["scores"][mObjectObjective] = SystemUtils::toInt((mObjectScore.empty() ? "100" : mObjectScore), 0);
                    } else if (awardType == "title") {
                        std::string mObjectObjective = std::get<std::string>(args->elements[6]);
                        std::string mObjectScore = std::get<std::string>(args->elements[7]);

                        data.update({
                            { "title", mObjectTitle },
                            { "image", mObjectImage },
                            { "introduce", std::get<std::string>(args->elements[5]) },
                            { "confirmButton", std::get<std::string>(args->elements[10]) },
                            { "cancelButton", std::get<std::string>(args->elements[9]) },
                            { "id", std::get<std::string>(args->elements[8]) },
                            { "scores", nlohmann::ordered_json::object() },
                            { "type", "title" }
                        });

                        if (!mObjectObjective.empty() && ScoreboardUtils::hasScoreboard(mObjectObjective))
                            data["scores"][mObjectObjective] = SystemUtils::toInt((mObjectScore.empty() ? "100" : mObjectScore), 0);

                        if (type == "buy")
                            data["time"] = SystemUtils::toInt(std::get<std::string>(args->elements[11]), 0);
                    } else {
                        data.update({
                            { "title", mObjectTitle },
                            { "image", mObjectImage },
                            { "id", std::get<std::string>(args->elements[8]) },
                            { "type", "from" }
                        });
                    }

                    int mIndex = static_cast<int>(this->getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id + "/classiflcation").value_or(nlohmann::ordered_json::array()).size());
                    this->getDatabase()->set_ptr("/" + id + "/classiflcation/" + std::to_string(mIndex), data);

                    return this->getDatabase()->save()
                        .transform([this, id, &player, values]() -> frontend::ArrayRef {
                            this->getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "shop.log5"), player)), id);

                            values->elements.emplace_back(true);
                            return values;
                        });
                });

                form::GUIManager::getInstance().registerCallback("shop.edit.award.remove", [this](frontend::ArrayRef args, Player& player) -> ll::Expected<void> {
                    if (args->elements.size() != 2 ||
                        !std::holds_alternative<std::string>(args->elements[0]) ||
                        !std::holds_alternative<std::string>(args->elements[1]))
                        return ll::makeStringError("shop.edit.award.remove: must take exactly two string parameters");

                    auto id = std::get<std::string>(args->elements[0]);
                    auto packageid = std::get<std::string>(args->elements[1]);

                    auto mContent = this->getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id + "/classiflcation").value_or(nlohmann::ordered_json::array());
                    for (int i = static_cast<int>(mContent.size() - 1); i >= 0; i--) {
                        if (mContent.at(i).value("title", "") == packageid)
                            mContent.erase(i);
                    }

                    this->getDatabase()->set_ptr("/" + id + "/classiflcation", mContent);

                    return this->getDatabase()->save()
                        .transform([this, id, packageid, &player]() -> void {
                            this->getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "shop.log3"), player)), id, packageid);
                        });
                });
            });
    }

    void ShopPlugin::listenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        this->mImpl->ShopCreateEventListener = eventBus.emplaceListener<LOICollection::server::Events::ShopCreateEvent>([](LOICollection::server::Events::ShopCreateEvent& event) -> void {
            ll::command::CommandRegistrar::getInstance(false).addSoftEnumValues(ShopObjectName, { event.getTarget() });
        });

        this->mImpl->ShopDeleteEventListener = eventBus.emplaceListener<LOICollection::server::Events::ShopDeleteEvent>([](LOICollection::server::Events::ShopDeleteEvent& event) -> void {
            ll::command::CommandRegistrar::getInstance(false).removeSoftEnumValues(ShopObjectName, { event.getTarget() });
        });
    }

    void ShopPlugin::unlistenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(this->mImpl->ShopCreateEventListener);
        eventBus.removeListener(this->mImpl->ShopDeleteEventListener);
    }

    ll::Expected<void> ShopPlugin::create(const std::string& id, const nlohmann::ordered_json& data) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(ShopPluginErrorCode::Invalid));

        if (!this->getDatabase()->has(id))
            this->getDatabase()->set(id, data);
        
        return this->getDatabase()->save();
    }

    ll::Expected<void> ShopPlugin::remove(const std::string& id) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(ShopPluginErrorCode::Invalid));

        this->getDatabase()->remove(id);
        
        return this->getDatabase()->save();
    }

    ll::Expected<bool> ShopPlugin::commodity(Player& player, int number, const nlohmann::ordered_json& data, ShopType type) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(ShopPluginErrorCode::Invalid));

        if (data.empty())
            return false;

        if (type == ShopType::buy) {
            return this->checkModifiedData(player, data, number)
                .and_then([number, &data, &player](bool exists) -> ll::Expected<bool> {
                    if (exists) {
                        auto itemStack = std::make_unique<ItemStack>();

                        if (data.contains("nbt"))
                            itemStack = std::make_unique<ItemStack>(ItemStack::fromTag(CompoundTag::fromSnbt(data.value("nbt", ""))->mTags));
                        else
                            itemStack->reinit(data.value("id", ""), 1, 0);
                        
                        InventoryUtils::giveItem(player, *itemStack, number);
                        player.refreshInventory();

                        return true;
                    }

                    return false;
                });
        } else if (InventoryUtils::isItemInInventory(player, data.value("id", ""), number)) {
            nlohmann::ordered_json mScoreboardBase = data.value("scores", nlohmann::ordered_json{});
            for (auto it = mScoreboardBase.begin(); it != mScoreboardBase.end(); ++it)
                ScoreboardUtils::addScore(player, it.key(), (it.value().get<int>() * number));

            InventoryUtils::clearItem(player, data.value("id", ""), number);
            player.refreshInventory();

            return true;
        }

        return false;
    }
    
    ll::Expected<bool> ShopPlugin::title(Player& player, const nlohmann::ordered_json& data, ShopType type) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(ShopPluginErrorCode::Invalid));

        if (data.empty())
            return false;

        std::string id = data.value("id", "None");

        if (type == ShopType::buy) {
            return this->checkModifiedData(player, data, 1)
                .and_then([id, &data, &player](bool exists) -> ll::Expected<bool> {
                    if (exists) {
                        if (data.contains("time")) {
                            return ChatPlugin::getShared()->addTitle(player, id, data.value("time", 0))
                                .or_else([](ll::Error e) -> ll::Expected<void> {
                                    if (e.isA<ll::ErrorCodeError>() && e.as<ll::ErrorCodeError>().ec == ChatPlugin::makeErrorCode(ChatPluginErrorCode::Invalid))
                                        return {};

                                    return ll::Unexpected(e);
                                })
                                .transform([]() -> bool {
                                    return true;
                                });
                        }

                        return ChatPlugin::getShared()->addTitle(player, id, 0).or_else([](ll::Error e) -> ll::Expected<void> {
                            if (e.isA<ll::ErrorCodeError>() && e.as<ll::ErrorCodeError>().ec == ChatPlugin::makeErrorCode(ChatPluginErrorCode::Invalid))
                                return {};

                            return ll::Unexpected(e);
                        }).transform([]() -> bool {
                            return true;
                        });
                    }

                    return false;
                });
        }
        
        return ChatPlugin::getShared()->hasTitle(player, id)
            .or_else([](ll::Error e) -> ll::Expected<bool> {
                if (e.isA<ll::ErrorCodeError>() && e.as<ll::ErrorCodeError>().ec == ChatPlugin::makeErrorCode(ChatPluginErrorCode::Invalid))
                    return false;

                return ll::Unexpected(e);
            })
            .and_then([id, &data, &player](bool exists) -> ll::Expected<bool> {
                if (!exists)
                    return false;

                nlohmann::ordered_json mScoreboardBase = data.value("scores", nlohmann::ordered_json{});
                for (auto it = mScoreboardBase.begin(); it != mScoreboardBase.end(); ++it)
                    ScoreboardUtils::addScore(player, it.key(), it.value().get<int>());

                return ChatPlugin::getShared()->delTitle(player, id)
                    .or_else([](ll::Error e) -> ll::Expected<void> {
                        if (e.isA<ll::ErrorCodeError>() && e.as<ll::ErrorCodeError>().ec == ChatPlugin::makeErrorCode(ChatPluginErrorCode::Invalid))
                            return {};

                        return ll::Unexpected(e);
                    })
                    .transform([]() -> bool {
                        return true;
                    });
            });
    }

    ll::Expected<bool> ShopPlugin::checkModifiedData(Player& player, nlohmann::ordered_json data, int number) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(ShopPluginErrorCode::Invalid));

        if (!data.contains("scores"))
            return true;

        for (auto it = data["scores"].begin(); it != data["scores"].end(); ++it) {
            if ((it.value().get<int>() * number) > ScoreboardUtils::getScore(player, it.key()))
                return false;
        }

        for (auto it = data["scores"].begin(); it != data["scores"].end(); ++it)
            ScoreboardUtils::reduceScore(player, it.key(), (it.value().get<int>() * number));
        
        return true;
    }

    ll::Expected<bool> ShopPlugin::has(const std::string& id) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(ShopPluginErrorCode::Invalid));

        return this->getDatabase()->has(id);
    }

    bool ShopPlugin::isValid() {
        return this->getLogger() != nullptr && this->getDatabase() != nullptr;
    }

    std::string ShopPlugin::getName() {
        return "ShopPlugin";
    }

    modules::ModulePriority ShopPlugin::getPriority() {
        return modules::ModulePriority::High;
    }

    ll::Expected<bool> ShopPlugin::load() {
        if (!ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().ServerConfig.Plugins.Shop)
            return false;

        auto mDataPath = std::filesystem::path(ServiceProvider::getInstance().getService<std::string>("ConfigPath")->data());

        this->mImpl->db = std::make_shared<JsonStorage>(mDataPath / "shop.json");
        this->mImpl->logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");
        this->mImpl->ModuleEnabled = true;
        this->mImpl->mGuiPath = std::filesystem::path(ServiceProvider::getInstance().getService<std::string>("GuiPath")->data());

        return this->mImpl->db->load()
            .transform([]() -> bool {
                return true;
            });
    }

    ll::Expected<bool> ShopPlugin::unload() {
        if (!this->mImpl->ModuleEnabled)
            return false;

        this->mImpl->db.reset();
        this->mImpl->logger.reset();
        this->mImpl->ModuleEnabled = false;

        return true;
    }

    ll::Expected<bool> ShopPlugin::registry() {
        if (!this->mImpl->ModuleEnabled)
            return false;

        return this->registeryUI()
            .transform([this]() -> bool {
                this->registeryCommand();

                this->mImpl->mRegistered.store(true, std::memory_order_release);

                return true;
            });
    }

    ll::Expected<bool> ShopPlugin::unregistry() {
        if (!this->mImpl->ModuleEnabled)
            return false;

        return this->getDatabase()->save()
            .transform([this]() -> bool {
                this->mImpl->mRegistered.store(false, std::memory_order_release);

                return true;
            });
    }
}
