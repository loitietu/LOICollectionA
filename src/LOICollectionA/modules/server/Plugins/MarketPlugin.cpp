#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <utility>
#include <algorithm>
#include <filesystem>
#include <unordered_map>

#include <fmt/core.h>

#include <ll/api/Expected.h>
#include <ll/api/io/Logger.h>
#include <ll/api/io/LoggerRegistry.h>

#include <ll/api/thread/ServerThreadExecutor.h>
#include <ll/api/base/Containers.h>

#include <ll/api/service/Bedrock.h>
#include <ll/api/command/Command.h>
#include <ll/api/command/CommandHandle.h>
#include <ll/api/command/CommandRegistrar.h>
#include <ll/api/event/EventBus.h>
#include <ll/api/event/ListenerBase.h>
#include <ll/api/event/player/PlayerJoinEvent.h>

#include <mc/deps/nbt/Tag.h>
#include <mc/deps/nbt/CompoundTag.h>

#include <mc/network/packet/TextPacket.h>

#include <mc/world/level/Level.h>
#include <mc/world/actor/player/Player.h>
#include <mc/world/actor/player/PlayerInventory.h>
#include <mc/world/actor/player/Inventory.h>

#include <mc/world/item/ItemStack.h>
#include <mc/world/item/SaveContext.h>
#include <mc/world/item/SaveContextFactory.h>

#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandPermissionLevel.h>

#include "LOICollectionA/include/server/APIUtils.h"
#include "LOICollectionA/include/server/Plugins/LanguagePlugin.h"

#include "LOICollectionA/coro/TimerManager.h"

#include "LOICollectionA/utils/I18nUtils.h"
#include "LOICollectionA/utils/mc-server/InventoryUtils.h"
#include "LOICollectionA/utils/mc-server/ScoreboardUtils.h"
#include "LOICollectionA/utils/core/SystemUtils.h"

#include "LOICollectionA/data/SQLiteStorage.h"

#include "LOICollectionA/base/Cache.h"
#include "LOICollectionA/base/Wrapper.h"
#include "LOICollectionA/base/ServiceProvider.h"

#include "LOICollectionA/ConfigPlugin.h"

#include "LOICollectionA/frontend/AST.h"

#include "LOICollectionA/include/form/GUIManager.h"

#include "LOICollectionA/include/server/Plugins/MarketPlugin.h"

using I18nUtilsTools::tr;

namespace LOICollection::server::Plugins {
    struct MarketPlugin::TradeEntry {
        std::string source;
        std::string target;

        MarketTradeType type = MarketTradeType::sell;
    };

    struct MarketPlugin::Impl {
        std::shared_ptr<TimerManager> mTimerManager;

        ll::ConcurrentDenseMap<std::string, TradeEntry> mTrades;
        ll::ConcurrentDenseMap<std::string, TradeEntry> mTradeRequests;

        LRUKCache<std::string, std::vector<std::string>> BlacklistCache;

        std::atomic<bool> mRegistered{ false };

        Config::C_Market options;

        std::shared_ptr<SQLiteStorage> db;
        std::shared_ptr<SQLiteStorage> db2;
        std::shared_ptr<ll::io::Logger> logger;

        std::string mGuiPath;
        std::string mGuiTradePath;
        
        ll::event::ListenerPtr PlayerJoinEventListener;

        Impl() : mTimerManager(std::make_shared<TimerManager>(ll::thread::ServerThreadExecutor::getDefault())),
            BlacklistCache(100, 100) {}
    };

    MarketPlugin::MarketPlugin() : mImpl(std::make_unique<Impl>()) {};
    MarketPlugin::~MarketPlugin() = default;

    std::shared_ptr<MarketPlugin> MarketPlugin::getShared() {
        static auto instance = std::shared_ptr<MarketPlugin>(new MarketPlugin());
        return instance;
    }

    std::error_code MarketPlugin::makeErrorCode(MarketPluginErrorCode e) {
        static MarketPluginErrorCategory cat;
        return std::error_code{ static_cast<int>(e), cat };
    }

    std::shared_ptr<SQLiteStorage> MarketPlugin::getDatabase() {
        return this->mImpl->db;
    }

    std::shared_ptr<ll::io::Logger> MarketPlugin::getLogger() {
        return this->mImpl->logger;
    }

    void MarketPlugin::registeryCommand() {
        ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance(false)
            .getOrCreateCommand("market", tr({}, "commands.market.description"), CommandPermissionLevel::Any, CommandFlagValue::NotCheat | CommandFlagValue::Async);
        command.overload().text("gui").execute([](CommandOrigin const& origin, CommandOutput& output) -> void {
            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isType(ActorType::Player))
                return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));
            Player& player = *static_cast<Player*>(entity);

            form::GUIManager::getInstance().open("market", "market.open", form::GUIManagerType::CustomForm, player)
                .or_else(modules::defaultErrorHandler<MarketPlugin>);

            output.success(fmt::runtime(tr(origin.getLocaleCode(), "commands.generic.ui")), player.getRealName());
        });
        command.overload().text("reload").execute([this](CommandOrigin const& origin, CommandOutput& output) -> void {
            if (origin.getPermissionsLevel() < CommandPermissionLevel::GameDirectors)
                return output.error(tr(origin.getLocaleCode(), "commands.generic.permission"));

            output.success(tr(origin.getLocaleCode(), "commands.generic.reload"));
            
            form::GUIManager::getInstance().load("market", this->mImpl->mGuiPath)
                .and_then([this]() -> ll::Expected<void> {
                    return form::GUIManager::getInstance().load("market.trade", this->mImpl->mGuiTradePath);
                })
                .transform([&origin, &output]() -> void {
                    output.success(tr(origin.getLocaleCode(), "commands.generic.reload.success"));
                })
                .or_else(modules::defaultErrorHandler<MarketPlugin>);
        });
    }

    ll::Expected<void> MarketPlugin::registeryUI() {
        return form::GUIManager::getInstance().load("market", this->mImpl->mGuiPath)
            .and_then([this]() -> ll::Expected<void> {
                return form::GUIManager::getInstance().load("market.trade", this->mImpl->mGuiTradePath);
            })
            .transform([this]() -> void {
                auto listPlayers = [](Player& player) -> std::vector<std::pair<std::string, std::string>> {
                    std::vector<std::pair<std::string, std::string>> players;

                    ll::service::getLevel()->forEachPlayer([&player, &players](Player& target) -> bool {
                        if (target.isSimulatedPlayer() || target.getUuid() == player.getUuid())
                            return true;

                        players.emplace_back(target.getRealName(), target.getUuid().asString());
                        return true;
                    });

                    return players;
                };

                auto listSellableInventory = [this](Player& player) -> ll::Expected<std::vector<std::pair<std::string, int>>> {
                    return LanguagePlugin::getShared()->getLanguage(player)
                        .transform([this, &player](const std::string& language) -> std::vector<std::pair<std::string, int>> {
                            std::vector<std::pair<std::string, int>> items;
                            std::vector<std::string> prohibitedItems = this->getProhibitedItems();

                            for (int i = 0; i < player.mInventory->mInventory->getContainerSize(); i++) {
                                ItemStack mItemStack = player.mInventory->mInventory->getItem(i);

                                if (!mItemStack || mItemStack.isNull() ||
                                    std::find(prohibitedItems.begin(), prohibitedItems.end(), mItemStack.getTypeName()) != prohibitedItems.end())
                                    continue;

                                items.emplace_back(fmt::format(fmt::runtime(tr(language, "market.gui.sell.item.text")),
                                    mItemStack.getName(), std::to_string(mItemStack.mCount)
                                ), i);
                            }

                            return items;
                        });
                };

                auto listBuyItems = [this](Player& player) -> ll::Expected<std::vector<std::pair<std::string, std::string>>> {
                    return this->getItems()
                        .and_then([this, &player](const std::vector<std::string>& ids) -> ll::Expected<std::vector<std::pair<std::string, std::string>>> {
                            return this->getItemsData(ids)
                                .and_then([this, &player](std::unordered_map<std::string, std::unordered_map<std::string, std::string>> data) -> ll::Expected<std::vector<std::pair<std::string, std::string>>> {
                                    std::vector<std::pair<std::string, std::string>> items;
                                    std::string mUuid = player.getUuid().asString();

                                    for (auto& item : data) {
                                        auto blacklists = this->getBlacklist(item.second.at("player_uuid"));
                                        if (!blacklists.has_value())
                                            return ll::Unexpected(blacklists.error());

                                        if (std::find(blacklists.value().begin(), blacklists.value().end(), mUuid) != blacklists.value().end())
                                            continue;

                                        items.emplace_back(item.second.at("name"), item.first);
                                    }

                                    return items;
                                });
                        });
                };

                auto listMyItems = [this](Player& player) -> ll::Expected<std::vector<std::pair<std::string, std::string>>> {
                    return this->getItems(player)
                        .and_then([this](const std::vector<std::string>& ids) -> ll::Expected<std::vector<std::pair<std::string, std::string>>> {
                            return this->getItemsData(ids)
                                .transform([](std::unordered_map<std::string, std::unordered_map<std::string, std::string>> data) -> std::vector<std::pair<std::string, std::string>> {
                                    std::vector<std::pair<std::string, std::string>> items;

                                    items.reserve(data.size());
                                    for (auto& item : data)
                                        items.emplace_back(item.second.at("name"), item.first);

                                    return items;
                                });
                        });
                };

                form::GUIManager::getInstance().registerValue("market.players", [listPlayers](Player& player) -> frontend::ArrayRef {
                    auto values = std::make_shared<frontend::ArrayValue>();

                    for (const auto& [name, uuid] : listPlayers(player))
                        values->elements.emplace_back(name);

                    return values;
                });

                form::GUIManager::getInstance().registerValue("market.buy.items", [listBuyItems](Player& player) -> ll::Expected<frontend::ArrayRef> {
                    return listBuyItems(player)
                        .transform([](const std::vector<std::pair<std::string, std::string>>& items) -> frontend::ArrayRef {
                            auto values = std::make_shared<frontend::ArrayValue>();

                            for (const auto& [name, id] : items)
                                values->elements.emplace_back(name);

                            return values;
                        });
                });

                form::GUIManager::getInstance().registerValue("market.my.items", [listMyItems](Player& player) -> ll::Expected<frontend::ArrayRef> {
                    return listMyItems(player)
                        .transform([](const std::vector<std::pair<std::string, std::string>>& items) -> frontend::ArrayRef {
                            auto values = std::make_shared<frontend::ArrayValue>();

                            for (const auto& [name, id] : items)
                                values->elements.emplace_back(name);

                            return values;
                        });
                });

                form::GUIManager::getInstance().registerValue("market.sell.inventory", [listSellableInventory](Player& player) -> ll::Expected<frontend::ArrayRef> {
                    return listSellableInventory(player)
                        .transform([](const std::vector<std::pair<std::string, int>>& items) -> frontend::ArrayRef {
                            auto values = std::make_shared<frontend::ArrayValue>();

                            for (const auto& [name, slot] : items)
                                values->elements.emplace_back(name);

                            return values;
                        });
                });

                form::GUIManager::getInstance().registerValue("market.trade.items", [listSellableInventory](Player& player) -> ll::Expected<frontend::ArrayRef> {
                    return listSellableInventory(player)
                        .transform([](const std::vector<std::pair<std::string, int>>& items) -> frontend::ArrayRef {
                            auto values = std::make_shared<frontend::ArrayValue>();

                            for (const auto& [name, slot] : items)
                                values->elements.emplace_back(name);

                            return values;
                        });
                });

                form::GUIManager::getInstance().registerValue("market.blacklists", [this](Player& player) -> ll::Expected<frontend::ArrayRef> {
                    return this->getBlacklist(player)
                        .transform([](const std::vector<std::string>& blacklists) -> frontend::ArrayRef {
                            auto values = std::make_shared<frontend::ArrayValue>();

                            for (const std::string& id : blacklists)
                                values->elements.emplace_back(id);

                            return values;
                        });
                });

                form::GUIManager::getInstance().registerRequest("market.self.uuid", [](frontend::ArrayRef, Player& player) -> ll::Expected<frontend::ArrayRef> {
                    auto values = std::make_shared<frontend::ArrayValue>();
                    values->elements.emplace_back(player.getUuid().asString());

                    return values;
                });

                form::GUIManager::getInstance().registerRequest("market.isAdmin", [](frontend::ArrayRef, Player& player) -> ll::Expected<frontend::ArrayRef> {
                    auto values = std::make_shared<frontend::ArrayValue>();
                    values->elements.emplace_back(player.getCommandPermissionLevel() >= CommandPermissionLevel::GameDirectors);

                    return values;
                });

                form::GUIManager::getInstance().registerRequest("market.item.info", [this](frontend::ArrayRef args, Player& player) -> ll::Expected<frontend::ArrayRef> {
                    if (args->elements.size() != 1 || !std::holds_alternative<std::string>(args->elements[0]))
                        return ll::makeStringError("market.item.info: must take exactly one string parameter");

                    auto id = std::get<std::string>(args->elements[0]);

                    return LanguagePlugin::getShared()->getLanguage(player)
                        .and_then([this, id, &player](const std::string& language) -> ll::Expected<frontend::ArrayRef> {
                            return this->hasItem(id)
                                .and_then([this, language, id, &player](bool exists) -> ll::Expected<frontend::ArrayRef> {
                                    auto values = std::make_shared<frontend::ArrayValue>();

                                    if (!exists) {
                                        player.sendMessage(tr(language, "market.gui.error"));

                                        return values;
                                    }

                                    auto data = this->getItemData(id);
                                    if (!data.has_value())
                                        return ll::Unexpected(data.error());

                                    values->elements.emplace_back(fmt::format(fmt::runtime(tr(language, "market.gui.item.introduce")),
                                        data.value().at("introduce"),
                                        data.value().at("score"),
                                        data.value().at("data"),
                                        data.value().at("player_name")
                                    ));

                                    return values;
                                });
                        });
                });

                form::GUIManager::getInstance().registerRequest("market.buy.id", [listBuyItems](frontend::ArrayRef args, Player& player) -> ll::Expected<frontend::ArrayRef> {
                    if (args->elements.size() != 1 || !std::holds_alternative<int>(args->elements[0]))
                        return ll::makeStringError("market.buy.id: must take exactly one int parameter");

                    int index = std::get<int>(args->elements[0]);

                    return listBuyItems(player)
                        .and_then([index](const std::vector<std::pair<std::string, std::string>>& items) -> ll::Expected<frontend::ArrayRef> {
                            auto values = std::make_shared<frontend::ArrayValue>();

                            if (index < 0 || index >= static_cast<int>(items.size()))
                                return values;

                            values->elements.emplace_back(items.at(static_cast<size_t>(index)).second);

                            return values;
                        });
                });

                form::GUIManager::getInstance().registerRequest("market.my.item.id", [listMyItems](frontend::ArrayRef args, Player& player) -> ll::Expected<frontend::ArrayRef> {
                    if (args->elements.size() != 1 || !std::holds_alternative<int>(args->elements[0]))
                        return ll::makeStringError("market.my.item.id: must take exactly one int parameter");

                    int index = std::get<int>(args->elements[0]);

                    return listMyItems(player)
                        .and_then([index](const std::vector<std::pair<std::string, std::string>>& items) -> ll::Expected<frontend::ArrayRef> {
                            auto values = std::make_shared<frontend::ArrayValue>();

                            if (index < 0 || index >= static_cast<int>(items.size()))
                                return values;

                            values->elements.emplace_back(items.at(static_cast<size_t>(index)).second);

                            return values;
                        });
                });

                form::GUIManager::getInstance().registerRequest("market.sell.slot", [listSellableInventory](frontend::ArrayRef args, Player& player) -> ll::Expected<frontend::ArrayRef> {
                    if (args->elements.size() != 1 || !std::holds_alternative<int>(args->elements[0]))
                        return ll::makeStringError("market.sell.slot: must take exactly one int parameter");

                    int index = std::get<int>(args->elements[0]);

                    return listSellableInventory(player)
                        .and_then([index](const std::vector<std::pair<std::string, int>>& items) -> ll::Expected<frontend::ArrayRef> {
                            auto values = std::make_shared<frontend::ArrayValue>();

                            if (index < 0 || index >= static_cast<int>(items.size()))
                                return values;

                            values->elements.emplace_back(items.at(static_cast<size_t>(index)).second);

                            return values;
                        });
                });

                form::GUIManager::getInstance().registerRequest("market.players.uuid", [listPlayers](frontend::ArrayRef args, Player& player) -> ll::Expected<frontend::ArrayRef> {
                    if (args->elements.size() != 1 || !std::holds_alternative<int>(args->elements[0]))
                        return ll::makeStringError("market.players.uuid: must take exactly one int parameter");

                    int index = std::get<int>(args->elements[0]);
                    auto players = listPlayers(player);
                    auto values = std::make_shared<frontend::ArrayValue>();

                    if (index < 0 || index >= static_cast<int>(players.size()))
                        return values;

                    values->elements.emplace_back(players.at(static_cast<size_t>(index)).second);

                    return values;
                });

                form::GUIManager::getInstance().registerRequest("market.sell.content.check", [this](frontend::ArrayRef, Player& player) -> ll::Expected<frontend::ArrayRef> {
                    return this->getItems(player)
                        .and_then([this, &player](const std::vector<std::string>& items) -> ll::Expected<frontend::ArrayRef> {
                            auto values = std::make_shared<frontend::ArrayValue>();

                            if (static_cast<int>(items.size()) >= this->getMaximumUpload()) {
                                return LanguagePlugin::getShared()->getLanguage(player)
                                    .and_then([this, &player, values](const std::string& language) -> ll::Expected<frontend::ArrayRef> {
                                        player.sendMessage(fmt::format(fmt::runtime(tr(language, "market.gui.sell.sellItem.tips4")), this->getMaximumUpload()));

                                        values->elements.emplace_back(false);
                                        return values;
                                    });
                            }

                            values->elements.emplace_back(true);
                            return values;
                        });
                });

                form::GUIManager::getInstance().registerRequest("market.blacklist.check", [this](frontend::ArrayRef, Player& player) -> ll::Expected<frontend::ArrayRef> {
                    return this->getBlacklist(player)
                        .and_then([this, &player](const std::vector<std::string>& ids) -> ll::Expected<frontend::ArrayRef> {
                            auto values = std::make_shared<frontend::ArrayValue>();

                            if (static_cast<int>(ids.size()) >= this->getBlacklistUpload()) {
                                return LanguagePlugin::getShared()->getLanguage(player)
                                    .and_then([this, &player, values](const std::string& language) -> ll::Expected<frontend::ArrayRef> {
                                        player.sendMessage(fmt::format(fmt::runtime(tr(language, "market.gui.sell.sellItem.tips5")), this->getBlacklistUpload()));

                                        values->elements.emplace_back(false);
                                        return values;
                                    });
                            }

                            values->elements.emplace_back(true);
                            return values;
                        });
                });

                form::GUIManager::getInstance().registerRequest("market.blacklist.info", [this](frontend::ArrayRef args, Player& player) -> ll::Expected<frontend::ArrayRef> {
                    if (args->elements.size() != 1 || !std::holds_alternative<std::string>(args->elements[0]))
                        return ll::makeStringError("market.blacklist.info: must take exactly one string parameter");

                    auto id = std::get<std::string>(args->elements[0]);

                    return LanguagePlugin::getShared()->getLanguage(player)
                        .and_then([this, id, &player](const std::string& language) -> ll::Expected<frontend::ArrayRef> {
                            return this->hasBlacklist(player, id)
                                .and_then([this, language, id, &player](bool exists) -> ll::Expected<frontend::ArrayRef> {
                                    auto values = std::make_shared<frontend::ArrayValue>();

                                    if (!exists) {
                                        player.sendMessage(tr(language, "market.gui.error"));

                                        return values;
                                    }

                                    auto data = this->getBlacklistData(id);
                                    if (!data.has_value())
                                        return ll::Unexpected(data.error());

                                    values->elements.emplace_back(fmt::format(fmt::runtime(tr(language, "market.gui.sell.blacklist.set.label")),
                                        data.value().at("target"),
                                        data.value().at("name"),
                                        SystemUtils::toFormatTime(data.value().at("time"), "None")
                                    ));

                                    return values;
                                });
                        });
                });

                form::GUIManager::getInstance().registerRequest("market.blacklist.add", [this](frontend::ArrayRef args, Player& player) -> ll::Expected<frontend::ArrayRef> {
                    if (args->elements.size() != 1 || !std::holds_alternative<std::string>(args->elements[0]))
                        return ll::makeStringError("market.blacklist.add: must take exactly one string parameter");

                    auto values = std::make_shared<frontend::ArrayValue>();

                    Player* target = ll::service::getLevel()->getPlayer(mce::UUID::fromString(std::get<std::string>(args->elements[0])));
                    if (!target) {
                        return LanguagePlugin::getShared()->getLanguage(player)
                            .and_then([&player, values](const std::string& language) -> ll::Expected<frontend::ArrayRef> {
                                player.sendMessage(tr(language, "market.gui.error"));

                                values->elements.emplace_back(false);
                                return values;
                            });
                    }

                    return this->addBlacklist(player, *target)
                        .transform([values]() -> frontend::ArrayRef {
                            values->elements.emplace_back(true);
                            return values;
                        });
                });

                form::GUIManager::getInstance().registerRequest("market.sell.submit", [this](frontend::ArrayRef args, Player& player) -> ll::Expected<frontend::ArrayRef> {
                    if (args->elements.size() != 5 ||
                        !std::holds_alternative<int>(args->elements[0]) ||
                        !std::holds_alternative<std::string>(args->elements[1]) ||
                        !std::holds_alternative<std::string>(args->elements[2]) ||
                        !std::holds_alternative<std::string>(args->elements[3]) ||
                        !std::holds_alternative<std::string>(args->elements[4]))
                        return ll::makeStringError("market.sell.submit: must take one int and four string parameters");

                    auto values = std::make_shared<frontend::ArrayValue>();

                    std::string mItemName = std::get<std::string>(args->elements[1]);
                    std::string mItemIcon = std::get<std::string>(args->elements[2]);
                    std::string mItemIntroduce = std::get<std::string>(args->elements[3]);

                    if (mItemName.empty() || mItemIcon.empty() || mItemIntroduce.empty()) {
                        return LanguagePlugin::getShared()->getLanguage(player)
                            .and_then([&player, values](const std::string& language) -> ll::Expected<frontend::ArrayRef> {
                                player.sendMessage(tr(language, "generic.tips.noinput"));

                                values->elements.emplace_back(false);
                                return values;
                            });
                    }

                    if (!this->sellItem(player, std::get<int>(args->elements[0]), mItemName, mItemIcon, mItemIntroduce,
                        SystemUtils::toInt(std::get<std::string>(args->elements[4]), 0))) {
                        return LanguagePlugin::getShared()->getLanguage(player)
                            .and_then([&player, values](const std::string& language) -> ll::Expected<frontend::ArrayRef> {
                                player.sendMessage(tr(language, "market.gui.error"));

                                values->elements.emplace_back(false);
                                return values;
                            });
                    }

                    values->elements.emplace_back(true);
                    return values;
                });

                form::GUIManager::getInstance().registerRequest("market.buy.submit", [this](frontend::ArrayRef args, Player& player) -> ll::Expected<frontend::ArrayRef> {
                    if (args->elements.size() != 1 || !std::holds_alternative<std::string>(args->elements[0]))
                        return ll::makeStringError("market.buy.submit: must take exactly one string parameter");

                    auto values = std::make_shared<frontend::ArrayValue>();

                    return this->buyItem(player, std::get<std::string>(args->elements[0]))
                        .or_else([](ll::Error e) -> ll::Expected<bool> {
                            if (e.isA<ll::ErrorCodeError>() && e.as<ll::ErrorCodeError>().ec == makeErrorCode(MarketPluginErrorCode::ItemNotFound))
                                return false;

                            return ll::Unexpected(e);
                        })
                        .transform([values](bool ok) -> frontend::ArrayRef {
                            values->elements.emplace_back(ok);

                            return values;
                        });
                });

                form::GUIManager::getInstance().registerRequest("market.item.offshelf", [this](frontend::ArrayRef args, Player& player) -> ll::Expected<frontend::ArrayRef> {
                    if (args->elements.size() != 1 || !std::holds_alternative<std::string>(args->elements[0]))
                        return ll::makeStringError("market.item.offshelf: must take exactly one string parameter");

                    auto values = std::make_shared<frontend::ArrayValue>();

                    return this->offshelfItem(player, std::get<std::string>(args->elements[0]), false)
                        .or_else([](ll::Error e) -> ll::Expected<bool> {
                            if (e.isA<ll::ErrorCodeError>() && e.as<ll::ErrorCodeError>().ec == makeErrorCode(MarketPluginErrorCode::ItemNotFound))
                                return false;

                            return ll::Unexpected(e);
                        })
                        .transform([values](bool ok) -> frontend::ArrayRef {
                            values->elements.emplace_back(ok);

                            return values;
                        });
                });

                form::GUIManager::getInstance().registerRequest("market.item.offshelf.return", [this](frontend::ArrayRef args, Player& player) -> ll::Expected<frontend::ArrayRef> {
                    if (args->elements.size() != 1 || !std::holds_alternative<std::string>(args->elements[0]))
                        return ll::makeStringError("market.item.offshelf.return: must take exactly one string parameter");

                    auto values = std::make_shared<frontend::ArrayValue>();

                    return this->offshelfItem(player, std::get<std::string>(args->elements[0]), true)
                        .or_else([](ll::Error e) -> ll::Expected<bool> {
                            if (e.isA<ll::ErrorCodeError>() && e.as<ll::ErrorCodeError>().ec == makeErrorCode(MarketPluginErrorCode::ItemNotFound))
                                return false;

                            return ll::Unexpected(e);
                        })
                        .transform([values](bool ok) -> frontend::ArrayRef {
                            values->elements.emplace_back(ok);

                            return values;
                        });
                });

                form::GUIManager::getInstance().registerRequest("market.trade.request.info", [](frontend::ArrayRef args, Player& player) -> ll::Expected<frontend::ArrayRef> {
                    if (args->elements.size() != 2 ||
                        !std::holds_alternative<std::string>(args->elements[0]) ||
                        !std::holds_alternative<std::string>(args->elements[1]))
                        return ll::makeStringError("market.trade.request.info: must take exactly two string parameters");

                    auto values = std::make_shared<frontend::ArrayValue>();

                    Player* requester = ll::service::getLevel()->getPlayer(mce::UUID::fromString(std::get<std::string>(args->elements[0])));
                    if (!requester)
                        return values;

                    std::string type = std::get<std::string>(args->elements[1]);

                    return LanguagePlugin::getShared()->getLanguage(player)
                        .transform([&requester, type, values](const std::string& language) -> frontend::ArrayRef {
                            values->elements.emplace_back(LOICollectionAPI::APIUtils::getInstance().translate(
                                tr(language, type == "sell" ? "market.sell" : "market.buy"), *requester
                            ));

                            return values;
                        });
                });

                form::GUIManager::getInstance().registerRequest("market.trade.request.accept", [this](frontend::ArrayRef, Player& player) -> ll::Expected<frontend::ArrayRef> {
                    auto values = std::make_shared<frontend::ArrayValue>();

                    return this->acceptRequest(player)
                        .or_else([](ll::Error e) -> ll::Expected<bool> {
                            if (e.isA<ll::ErrorCodeError>() && e.as<ll::ErrorCodeError>().ec == makeErrorCode(MarketPluginErrorCode::RequestNotFound))
                                return false;

                            return ll::Unexpected(e);
                        })
                        .transform([&player, values](bool ok) -> frontend::ArrayRef {
                            if (!ok) {
                                auto language = LanguagePlugin::getShared()->getLanguage(player);
                                if (language.has_value())
                                    player.sendMessage(tr(language.value(), "market.gui.error"));
                            }

                            values->elements.emplace_back(ok);
                            return values;
                        });
                });

                form::GUIManager::getInstance().registerRequest("market.trade.item.info", [](frontend::ArrayRef args, Player& player) -> ll::Expected<frontend::ArrayRef> {
                    if (args->elements.size() != 2 ||
                        !std::holds_alternative<std::string>(args->elements[0]) ||
                        !std::holds_alternative<int>(args->elements[1]))
                        return ll::makeStringError("market.trade.item.info: must take exactly one string and one int parameter");

                    auto values = std::make_shared<frontend::ArrayValue>();

                    Player* seller = ll::service::getLevel()->getPlayer(mce::UUID::fromString(std::get<std::string>(args->elements[0])));
                    if (!seller)
                        return values;

                    int slot = std::get<int>(args->elements[1]);
                    if (slot < 0 || slot >= seller->mInventory->mInventory->getContainerSize())
                        return values;

                    ItemStack mItemStack = seller->mInventory->mInventory->getItem(slot);
                    if (!mItemStack || mItemStack.isNull())
                        return values;

                    return LanguagePlugin::getShared()->getLanguage(player)
                        .transform([&seller, &mItemStack, values](const std::string& language) -> frontend::ArrayRef {
                            values->elements.emplace_back(fmt::format(fmt::runtime(tr(language, "market.gui.trade.introduce")),
                                seller->getRealName(),
                                mItemStack.getName(),
                                mItemStack.mCount,
                                mItemStack.save(*SaveContextFactory::createCloneSaveContext())->toSnbt(SnbtFormat::Minimize, 0)
                            ));

                            return values;
                        });
                });

                form::GUIManager::getInstance().registerRequest("market.trade.confirm.info", [](frontend::ArrayRef args, Player& player) -> ll::Expected<frontend::ArrayRef> {
                    if (args->elements.size() != 3 ||
                        !std::holds_alternative<std::string>(args->elements[0]) ||
                        !std::holds_alternative<int>(args->elements[1]) ||
                        !std::holds_alternative<int>(args->elements[2]))
                        return ll::makeStringError("market.trade.confirm.info: must take one string and two int parameters");

                    auto values = std::make_shared<frontend::ArrayValue>();

                    Player* buyer = ll::service::getLevel()->getPlayer(mce::UUID::fromString(std::get<std::string>(args->elements[0])));
                    if (!buyer)
                        return values;

                    int slot = std::get<int>(args->elements[1]);
                    int score = std::get<int>(args->elements[2]);

                    if (slot < 0 || slot >= player.mInventory->mInventory->getContainerSize())
                        return values;

                    ItemStack mItemStack = player.mInventory->mInventory->getItem(slot);
                    if (!mItemStack || mItemStack.isNull())
                        return values;

                    return LanguagePlugin::getShared()->getLanguage(player)
                        .transform([&buyer, &mItemStack, score, values](const std::string& language) -> frontend::ArrayRef {
                            values->elements.emplace_back(fmt::format(fmt::runtime(tr(language, "market.gui.trade.confirm.introduce")),
                                buyer->getRealName(),
                                mItemStack.getName(),
                                mItemStack.mCount,
                                mItemStack.save(*SaveContextFactory::createCloneSaveContext())->toSnbt(SnbtFormat::Minimize, 0),
                                score
                            ));

                            return values;
                        });
                });

                form::GUIManager::getInstance().registerRequest("market.trade.slot", [listSellableInventory](frontend::ArrayRef args, Player& player) -> ll::Expected<frontend::ArrayRef> {
                    if (args->elements.size() != 1 || !std::holds_alternative<int>(args->elements[0]))
                        return ll::makeStringError("market.trade.slot: must take exactly one int parameter");

                    int index = std::get<int>(args->elements[0]);

                    return listSellableInventory(player)
                        .and_then([index](const std::vector<std::pair<std::string, int>>& items) -> ll::Expected<frontend::ArrayRef> {
                            auto values = std::make_shared<frontend::ArrayValue>();

                            if (index < 0 || index >= static_cast<int>(items.size()))
                                return values;

                            values->elements.emplace_back(items.at(static_cast<size_t>(index)).second);

                            return values;
                        });
                });

                form::GUIManager::getInstance().registerCallback("market.blacklist.remove", [this](frontend::ArrayRef args, Player& player) -> ll::Expected<void> {
                    if (args->elements.size() != 1 || !std::holds_alternative<std::string>(args->elements[0]))
                        return ll::makeStringError("market.blacklist.remove: must take exactly one string parameter");

                    return this->delBlacklist(player, std::get<std::string>(args->elements[0]))
                        .or_else([](ll::Error e) -> ll::Expected<void> {
                            if (e.isA<ll::ErrorCodeError>() && e.as<ll::ErrorCodeError>().ec == makeErrorCode(MarketPluginErrorCode::BlacklistNotFound))
                                return {};

                            return ll::Unexpected(e);
                        });
                });

                form::GUIManager::getInstance().registerCallback("market.trade.request", [this](frontend::ArrayRef args, Player& player) -> ll::Expected<void> {
                    if (args->elements.size() != 2 ||
                        !std::holds_alternative<std::string>(args->elements[0]) ||
                        !std::holds_alternative<std::string>(args->elements[1]))
                        return ll::makeStringError("market.trade.request: must take exactly two string parameters");

                    auto targetUuid = std::get<std::string>(args->elements[0]);
                    std::string type = std::get<std::string>(args->elements[1]);

                    Player* target = ll::service::getLevel()->getPlayer(mce::UUID::fromString(targetUuid));
                    if (!target) {
                        return LanguagePlugin::getShared()->getLanguage(player)
                            .and_then([&player](const std::string& language) -> ll::Expected<void> {
                                player.sendMessage(tr(language, "market.gui.error"));

                                return {};
                            });
                    }

                    return this->hasTrade(player)
                        .and_then([this, &player, target, type](bool exists) -> ll::Expected<void> {
                            if (exists) {
                                return LanguagePlugin::getShared()->getLanguage(player)
                                    .and_then([&player](const std::string& language) -> ll::Expected<void> {
                                        player.sendMessage(tr(language, "market.tips3"));

                                        return {};
                                    });
                            }

                            return this->sendRequest(player, *target, type == "sell" ? MarketTradeType::sell : MarketTradeType::buy)
                                .and_then([&player, target, type]() -> ll::Expected<void> {
                                    auto ctx = std::make_shared<frontend::ArrayValue>();
                                    ctx->elements.emplace_back(player.getUuid().asString());
                                    ctx->elements.emplace_back("");
                                    ctx->elements.emplace_back(-1);
                                    ctx->elements.emplace_back(-1);
                                    ctx->elements.emplace_back(type);

                                    return form::GUIManager::getInstance().open("market.trade", "market.trade.request", form::GUIManagerType::CustomForm, *target, ctx);
                                });
                        });
                });

                form::GUIManager::getInstance().registerCallback("market.trade.request.reject", [this](frontend::ArrayRef, Player& player) -> ll::Expected<void> {
                    return this->rejectRequest(player)
                        .or_else([](ll::Error e) -> ll::Expected<bool> {
                            if (e.isA<ll::ErrorCodeError>() && e.as<ll::ErrorCodeError>().ec == makeErrorCode(MarketPluginErrorCode::RequestNotFound))
                                return false;

                            return ll::Unexpected(e);
                        })
                        .transform([](bool) -> void {});
                });

                form::GUIManager::getInstance().registerCallback("market.trade.open.content", [](frontend::ArrayRef args, Player& player) -> ll::Expected<void> {
                    if (args->elements.size() != 2 ||
                        !std::holds_alternative<std::string>(args->elements[0]) ||
                        !std::holds_alternative<std::string>(args->elements[1]))
                        return ll::makeStringError("market.trade.open.content: must take exactly two string parameters");

                    auto destUuid = std::get<std::string>(args->elements[0]);
                    auto buyerUuid = std::get<std::string>(args->elements[1]);

                    Player* dest = ll::service::getLevel()->getPlayer(mce::UUID::fromString(destUuid));
                    if (!dest) {
                        return LanguagePlugin::getShared()->getLanguage(player)
                            .and_then([&player](const std::string& language) -> ll::Expected<void> {
                                player.sendMessage(tr(language, "market.gui.error"));

                                return {};
                            });
                    }

                    auto ctx = std::make_shared<frontend::ArrayValue>();
                    ctx->elements.emplace_back(buyerUuid);
                    ctx->elements.emplace_back("");
                    ctx->elements.emplace_back(-1);
                    ctx->elements.emplace_back(-1);
                    ctx->elements.emplace_back("");

                    return form::GUIManager::getInstance().open("market.trade", "market.trade.content", form::GUIManagerType::PaginatedForm, *dest, ctx);
                });

                form::GUIManager::getInstance().registerCallback("market.trade.item", [this](frontend::ArrayRef args, Player& player) -> ll::Expected<void> {
                    if (args->elements.size() != 2 ||
                        !std::holds_alternative<std::string>(args->elements[0]) ||
                        !std::holds_alternative<int>(args->elements[1]))
                        return ll::makeStringError("market.trade.item: must take exactly one string and one int parameter");

                    auto buyerUuid = std::get<std::string>(args->elements[0]);
                    int slot = std::get<int>(args->elements[1]);

                    if (slot < 0 || slot >= player.mInventory->mInventory->getContainerSize()) {
                        return this->cancelTrade(player)
                            .or_else([](ll::Error e) -> ll::Expected<bool> {
                                if (e.isA<ll::ErrorCodeError>() && e.as<ll::ErrorCodeError>().ec == makeErrorCode(MarketPluginErrorCode::TradeNotFound))
                                    return false;

                                return ll::Unexpected(e);
                            })
                            .transform([](bool) -> void {});
                    }

                    ItemStack mItemStack = player.mInventory->mInventory->getItem(slot);
                    if (!mItemStack || mItemStack.isNull()) {
                        return this->cancelTrade(player)
                            .or_else([](ll::Error e) -> ll::Expected<bool> {
                                if (e.isA<ll::ErrorCodeError>() && e.as<ll::ErrorCodeError>().ec == makeErrorCode(MarketPluginErrorCode::TradeNotFound))
                                    return false;

                                return ll::Unexpected(e);
                            })
                            .transform([](bool) -> void {});
                    }

                    Player* buyer = ll::service::getLevel()->getPlayer(mce::UUID::fromString(buyerUuid));
                    if (!buyer) {
                        return LanguagePlugin::getShared()->getLanguage(player)
                            .and_then([&player](const std::string& language) -> ll::Expected<void> {
                                player.sendMessage(tr(language, "market.gui.error"));

                                return {};
                            });
                    }

                    auto ctx = std::make_shared<frontend::ArrayValue>();
                    ctx->elements.emplace_back(buyerUuid);
                    ctx->elements.emplace_back(player.getUuid().asString());
                    ctx->elements.emplace_back(slot);
                    ctx->elements.emplace_back(-1);
                    ctx->elements.emplace_back("");

                    return form::GUIManager::getInstance().open("market.trade", "market.trade.item", form::GUIManagerType::CustomForm, *buyer, ctx);
                });

                form::GUIManager::getInstance().registerCallback("market.trade.item.submit", [this](frontend::ArrayRef args, Player& player) -> ll::Expected<void> {
                    if (args->elements.size() != 3 ||
                        !std::holds_alternative<std::string>(args->elements[0]) ||
                        !std::holds_alternative<int>(args->elements[1]) ||
                        !std::holds_alternative<std::string>(args->elements[2]))
                        return ll::makeStringError("market.trade.item.submit: must take two string and one int parameters");

                    auto sellerUuid = std::get<std::string>(args->elements[0]);
                    int slot = std::get<int>(args->elements[1]);
                    int score = SystemUtils::toInt(std::get<std::string>(args->elements[2]), 0);

                    Player* seller = ll::service::getLevel()->getPlayer(mce::UUID::fromString(sellerUuid));
                    if (!seller) {
                        return this->cancelTrade(player)
                            .or_else([](ll::Error e) -> ll::Expected<bool> {
                                if (e.isA<ll::ErrorCodeError>() && e.as<ll::ErrorCodeError>().ec == makeErrorCode(MarketPluginErrorCode::TradeNotFound))
                                    return false;

                                return ll::Unexpected(e);
                            })
                            .transform([](bool) -> void {});
                    }

                    auto ctx = std::make_shared<frontend::ArrayValue>();
                    ctx->elements.emplace_back(player.getUuid().asString());
                    ctx->elements.emplace_back(sellerUuid);
                    ctx->elements.emplace_back(slot);
                    ctx->elements.emplace_back(score);
                    ctx->elements.emplace_back("");

                    return form::GUIManager::getInstance().open("market.trade", "market.trade.confirm", form::GUIManagerType::CustomForm, *seller, ctx);
                });

                form::GUIManager::getInstance().registerCallback("market.trade.accept", [this](frontend::ArrayRef args, Player& player) -> ll::Expected<void> {
                    if (args->elements.size() != 2 ||
                        !std::holds_alternative<int>(args->elements[0]) ||
                        !std::holds_alternative<int>(args->elements[1]))
                        return ll::makeStringError("market.trade.accept: must take exactly two int parameters");

                    int slot = std::get<int>(args->elements[0]);
                    int score = std::get<int>(args->elements[1]);

                    return this->acceptTrade(player, slot, score)
                        .or_else([](ll::Error e) -> ll::Expected<bool> {
                            if (e.isA<ll::ErrorCodeError>() && e.as<ll::ErrorCodeError>().ec == makeErrorCode(MarketPluginErrorCode::TradeNotFound))
                                return false;

                            return ll::Unexpected(e);
                        })
                        .transform([&player](bool ok) -> void {
                            if (!ok) {
                                auto language = LanguagePlugin::getShared()->getLanguage(player);
                                if (language.has_value())
                                    player.sendMessage(tr(language.value(), "market.gui.error"));
                            }
                        });
                });

                form::GUIManager::getInstance().registerCallback("market.trade.cancel", [this](frontend::ArrayRef, Player& player) -> ll::Expected<void> {
                    return this->cancelTrade(player)
                        .or_else([](ll::Error e) -> ll::Expected<bool> {
                            if (e.isA<ll::ErrorCodeError>() && e.as<ll::ErrorCodeError>().ec == makeErrorCode(MarketPluginErrorCode::TradeNotFound))
                                return false;

                            return ll::Unexpected(e);
                        })
                        .transform([](bool) -> void {});
                });
            });
    }

    void MarketPlugin::listenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        this->mImpl->PlayerJoinEventListener = eventBus.emplaceListener<ll::event::PlayerJoinEvent>([this](ll::event::PlayerJoinEvent& event) mutable -> void {
            if (event.self().isSimulatedPlayer())
                return;

            std::string uuid = event.self().getUuid().asString();

            this->mImpl->db2->has("Market", uuid)
                .and_then([this, uuid, name = event.self().getRealName()](bool exists) -> ll::Expected<void> {
                    if (!exists) {
                        std::unordered_map<std::string, std::string> data = {
                            { "name", name },
                            { "score", "0" }
                        };

                        return this->mImpl->db2->set("Market", uuid, data);
                    }

                    return {};
                })
                .or_else(modules::defaultErrorHandler<MarketPlugin>);
            
            this->mImpl->db2->get("Market", uuid, "score")
                .and_then([this, uuid, &event](const std::string& value) -> ll::Expected<void> {
                    int mScore = SystemUtils::toInt(value, 0);
                    if (mScore > 0) {
                        ScoreboardUtils::addScore(event.self(), this->mImpl->options.TargetScoreboard, mScore);

                        return this->mImpl->db2->set("Market", uuid, "score", "0");
                    }

                    return {};
                })
                .or_else(modules::defaultErrorHandler<MarketPlugin>);
        });
    }

    void MarketPlugin::unlistenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(this->mImpl->PlayerJoinEventListener);

        this->mImpl->mTimerManager->cancelAll();
    }

    ll::Expected<bool> MarketPlugin::buyItem(Player& player, const std::string& id) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(MarketPluginErrorCode::Invalid));

        return this->getItemData(id)
            .and_then([this, id, &player]( std::unordered_map<std::string, std::string> data) -> ll::Expected<bool> {
                std::string mScoreboard = this->mImpl->options.TargetScoreboard;

                int mScore = SystemUtils::toInt(data.at("score"), 0);
                if (ScoreboardUtils::getScore(player, mScoreboard) < mScore) {
                    return LanguagePlugin::getShared()->getLanguage(player)
                        .transform([&player](const std::string& language) -> bool {
                            player.sendMessage(tr(language, "market.gui.sell.sellItem.tips3"));

                            return false;
                        });
                }

                ScoreboardUtils::reduceScore(player, mScoreboard, mScore);

                ItemStack mItemStack = ItemStack::fromTag(CompoundTag::fromSnbt(data.at("data"))->mTags);
                InventoryUtils::giveItem(player, mItemStack, static_cast<int>(mItemStack.mCount));

                player.refreshInventory();

                std::string mObject = data.at("player_uuid");
                if (Player* mPlayer = ll::service::getLevel()->getPlayer(mce::UUID::fromString(mObject)); mPlayer) {
                    return LanguagePlugin::getShared()->getLanguage(*mPlayer)
                        .transform([mScoreboard, mScore, &data, &mPlayer](const std::string& language) -> bool {
                            mPlayer->sendMessage(fmt::format(fmt::runtime(tr(language, "market.gui.sell.sellItem.tips1")), data.at("name")));

                            ScoreboardUtils::addScore(*mPlayer, mScoreboard, mScore);

                            return true;
                        });
                } else {
                    return this->mImpl->db2->get("Market", mObject, "Score", "0")
                        .and_then([this, mScore, mObject](const std::string& value) -> ll::Expected<bool> {
                            int mMarketScore = SystemUtils::toInt(value, 0);

                            return this->mImpl->db2->set("Market", mObject, "Score", std::to_string(mMarketScore + mScore))
                                .transform([]() -> bool {
                                    return true;
                                });
                        });
                }

                return this->delItem(id)
                    .transform([this, &data, &player]() -> bool {
                        this->getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "market.log2"), player)), data.at("name"));
    
                        return true;
                    });
            });
    }

    ll::Expected<bool> MarketPlugin::offshelfItem(Player& player, const std::string& id, bool returnItem) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(MarketPluginErrorCode::Invalid));

        return this->getItemData(id)
            .and_then([this, id, returnItem, &player](std::unordered_map<std::string, std::string> data) -> ll::Expected<bool> {
                if (returnItem) {
                    ItemStack mItemStack = ItemStack::fromTag(CompoundTag::fromSnbt(data.at("data"))->mTags);
                    InventoryUtils::giveItem(player, mItemStack, static_cast<int>(mItemStack.mCount));
                }

                return LanguagePlugin::getShared()->getLanguage(player)
                    .and_then([this, id, &data, &player](const std::string& language) -> ll::Expected<void> {
                        player.sendMessage(fmt::format(fmt::runtime(tr(language, "market.gui.sell.sellItem.tips2")), data.at("name")));

                        return this->delItem(id);
                    })
                    .transform([this, &data, &player]() -> bool {
                        this->getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "market.log3"), player)), data.at("name"));

                        return true;
                    });
            });
    }
    
    ll::Expected<bool> MarketPlugin::sellItem(Player& player, int slot, const std::string& name, const std::string& icon, const std::string& intr, int score) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(MarketPluginErrorCode::Invalid));

        ItemStack mItemStack = player.mInventory->mInventory->getItem(slot);
        if (!mItemStack || mItemStack.isNull())
            return false;

        return this->addItem(player, mItemStack, name, icon, intr, score)
            .transform([slot, &player]() -> bool {
                player.mInventory->mInventory->removeItem(slot, 64);
                player.refreshInventory();

                return true;
            });
    }

    ll::Expected<void> MarketPlugin::addBlacklist(Player& player, Player& target) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(MarketPluginErrorCode::Invalid));

        std::string mObject = player.getUuid().asString();
        std::string mTargetObject = target.getUuid().asString();
        std::string mTimestamp = SystemUtils::getCurrentTimestamp();

        std::unordered_map<std::string, std::string> mData = {
            { "name", target.getRealName() },
            { "target", mTargetObject },
            { "author", mObject },
            { "time", mTimestamp }
        };

        return this->getDatabase()->set("Blacklist", mTimestamp, mData)
            .transform([this, mObject, mTargetObject, mTimestamp, &player]() -> void {
                this->getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "market.log4"), player)), mTargetObject);

                if (this->mImpl->BlacklistCache.contains(mObject))
                    this->mImpl->BlacklistCache.update(mObject, [mTimestamp](std::shared_ptr<std::vector<std::string>> mList) -> void {
                        mList->push_back(mTimestamp);
                    });
            });
    }

    ll::Expected<void> MarketPlugin::addItem(Player& player, ItemStack& item, const std::string& name, const std::string& icon, const std::string& intr, int score) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(MarketPluginErrorCode::Invalid));

        std::string mTimestamp = SystemUtils::getCurrentTimestamp();

        std::unordered_map<std::string, std::string> mData = {
            { "name", name },
            { "icon", icon },
            { "introduce", intr },
            { "score", std::to_string(score) },
            { "data", item.save(*SaveContextFactory::createCloneSaveContext())->toSnbt(SnbtFormat::Minimize, 0) },
            { "player_name", player.getRealName() },
            { "player_uuid", player.getUuid().asString() }
        };

        return this->getDatabase()->set("Item", mTimestamp, mData)
            .transform([this, name, &player]() -> void {
                this->getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "market.log2"), player)), name);
            });
    }

    ll::Expected<void> MarketPlugin::delBlacklist(Player& player, const std::string& id) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(MarketPluginErrorCode::Invalid));

        return this->hasBlacklist(player, id)
            .and_then([this, id](bool exists) -> ll::Expected<void> {
                if (!exists) {
                    this->getLogger()->warn(fmt::runtime(tr({}, "console.log.error.object")), this->getName());

                    return ll::makeErrorCodeError(makeErrorCode(MarketPluginErrorCode::BlacklistNotFound));
                }

                return this->getDatabase()->del("Blacklist", id);
            })
            .transform([this, id, &player]() -> void {
                this->getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "market.log5"), player)), id);

                this->mImpl->BlacklistCache.update(player.getUuid().asString(), [id](std::shared_ptr<std::vector<std::string>> mList) -> void {
                    mList->erase(std::remove(mList->begin(), mList->end(), id), mList->end());
                });
            });
    }

    ll::Expected<void> MarketPlugin::delItem(const std::string& id) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(MarketPluginErrorCode::Invalid));

        return this->hasItem(id)
            .and_then([this, id](bool exists) -> ll::Expected<void> {
                if (!exists) {
                    this->getLogger()->warn(fmt::runtime(tr({}, "console.log.error.object")), this->getName());

                    return ll::makeErrorCodeError(makeErrorCode(MarketPluginErrorCode::ItemNotFound));
                }

                return this->getDatabase()->del("Item", id);
            });
    }

    ll::Expected<void> MarketPlugin::setExecutor(const ll::coro::Executor& executor) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(MarketPluginErrorCode::Invalid));

        this->mImpl->mTimerManager->setExecutor(executor);

        return {};
    }

    ll::Expected<bool> MarketPlugin::acceptRequest(Player& player) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(MarketPluginErrorCode::Invalid));

        std::string mObject = player.getUuid().asString();
        if (!this->mImpl->mTradeRequests.contains(mObject))
            return ll::makeErrorCodeError(makeErrorCode(MarketPluginErrorCode::RequestNotFound));

        TradeEntry mEntry = this->mImpl->mTradeRequests.at(mObject);

        Player* sourcePlayer = ll::service::getLevel()->getPlayer(mce::UUID::fromString(mEntry.source));
        if (!sourcePlayer) {
            return LanguagePlugin::getShared()->getLanguage(player)
                .transform([&player](const std::string& language) -> bool {
                    player.sendMessage(tr(language, "market.gui.error"));

                    return false;
                });
        }

        auto& mover = (mEntry.type == MarketTradeType::sell) ? player : *sourcePlayer;

        return LanguagePlugin::getShared()->getLanguage(*sourcePlayer)
            .and_then([&sourcePlayer, &mover](const std::string& language) -> ll::Expected<std::string> {
                sourcePlayer->sendMessage(tr(language, "market.yes.tips"));

                return LanguagePlugin::getShared()->getLanguage(mover);
            })
            .and_then([&mover](const std::string& language) -> ll::Expected<void> {
                mover.sendMessage(tr(language, "market.tips4"));

                return {};
            })
            .and_then([this, &sourcePlayer, &player, type = mEntry.type]() -> ll::Expected<void> { 
                return this->sendTrade(*sourcePlayer, player, type);
            })
            .transform([this, mObject, source = mEntry.source]() -> bool {
                this->mImpl->mTradeRequests.erase(mObject);
                this->mImpl->mTradeRequests.erase(source);

                this->mImpl->mTimerManager->cancel(source);

                return true;
            });
    }

    ll::Expected<bool> MarketPlugin::rejectRequest(Player& player) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(MarketPluginErrorCode::Invalid));

        std::string mObject = player.getUuid().asString();
        if (!this->mImpl->mTradeRequests.contains(mObject))
            return ll::makeErrorCodeError(makeErrorCode(MarketPluginErrorCode::RequestNotFound));

        TradeEntry mEntry = this->mImpl->mTradeRequests.at(mObject);

        if (Player* sourcePlayer = ll::service::getLevel()->getPlayer(mce::UUID::fromString(mEntry.source)); sourcePlayer) {
            auto language = LanguagePlugin::getShared()->getLanguage(*sourcePlayer);
            if (!language.has_value())
                return ll::Unexpected(language.error());

            sourcePlayer->sendMessage(tr(language.value(), "market.no.tips"));
        }
        
        this->mImpl->mTradeRequests.erase(mObject);
        this->mImpl->mTradeRequests.erase(mEntry.source);

        this->mImpl->mTimerManager->cancel(mEntry.source);

        return true;
    }

    ll::Expected<bool> MarketPlugin::cancelRequest(Player& player) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(MarketPluginErrorCode::Invalid));

        std::string mObject = player.getUuid().asString();
        if (!this->mImpl->mTradeRequests.contains(mObject))
            return ll::makeErrorCodeError(makeErrorCode(MarketPluginErrorCode::RequestNotFound));

        TradeEntry mEntry = this->mImpl->mTradeRequests.at(mObject);
        
        this->mImpl->mTradeRequests.erase(mObject);
        this->mImpl->mTradeRequests.erase(mEntry.target);

        this->mImpl->mTimerManager->cancel(mEntry.source);

        return true;
    }

    ll::Expected<bool> MarketPlugin::acceptTrade(Player& player, int slot, int score) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(MarketPluginErrorCode::Invalid));

        std::string mObject = player.getUuid().asString();
        if (!this->mImpl->mTrades.contains(mObject))
            return ll::makeErrorCodeError(makeErrorCode(MarketPluginErrorCode::TradeNotFound));

        TradeEntry mEntry = this->mImpl->mTrades.at(mObject);

        ItemStack mItemStack = player.getInventory().getItem(slot);
        if (!mItemStack || mItemStack.isNull())
            return false;

        Player* mPlayer = (mEntry.source == mObject) ?
            ll::service::getLevel()->getPlayer(mce::UUID::fromString(mEntry.target)) :
            ll::service::getLevel()->getPlayer(mce::UUID::fromString(mEntry.source));

        if (!mPlayer)
            return false;

        if (ScoreboardUtils::getScore(*mPlayer, this->mImpl->options.TargetScoreboard) < score || score <= 0)
            return false;

        player.mInventory->mInventory->removeItem(slot, 64);
        mPlayer->getInventory().addItem(mItemStack);

        player.refreshInventory();
        mPlayer->refreshInventory();

        ScoreboardUtils::addScore(player, this->mImpl->options.TargetScoreboard, score);
        ScoreboardUtils::reduceScore(*mPlayer, this->mImpl->options.TargetScoreboard, score);

        this->mImpl->mTrades.erase(mObject);
        this->mImpl->mTrades.erase(mEntry.target);

        this->mImpl->mTimerManager->cancel(mEntry.source + "_trade");

        return true;
    }

    ll::Expected<bool> MarketPlugin::cancelTrade(Player& player) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(MarketPluginErrorCode::Invalid));

        std::string mObject = player.getUuid().asString();
        if (!this->mImpl->mTrades.contains(mObject))
            return ll::makeErrorCodeError(makeErrorCode(MarketPluginErrorCode::TradeNotFound));

        TradeEntry mEntry = this->mImpl->mTrades.at(mObject);

        Player* mPlayer = (mEntry.source == mObject) ?
            ll::service::getLevel()->getPlayer(mce::UUID::fromString(mEntry.target)) :
            ll::service::getLevel()->getPlayer(mce::UUID::fromString(mEntry.source));
        if (mPlayer) {
            auto language = LanguagePlugin::getShared()->getLanguage(*mPlayer);
            if (!language.has_value())
                return ll::Unexpected(language.error());
            
            mPlayer->sendMessage(tr(language.value(), "market.tips6"));
        }

        this->mImpl->mTrades.erase(mEntry.source);
        this->mImpl->mTrades.erase(mEntry.target);

        this->mImpl->mTimerManager->cancel(mEntry.source + "_trade");

        this->getLogger()->info(fmt::runtime(tr({}, "market.log8")), player.getRealName());

        return true;
    }

    ll::Expected<void> MarketPlugin::sendRequest(Player& player, Player& target, MarketTradeType type) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(MarketPluginErrorCode::Invalid));

        std::string mObject = player.getUuid().asString();
        std::string mTargetObject = target.getUuid().asString();

        TradeEntry mEntry{ mObject, mTargetObject, type };
        this->mImpl->mTradeRequests[mObject] = mEntry;
        this->mImpl->mTradeRequests[mTargetObject] = mEntry;

        this->mImpl->mTimerManager->schedule(mObject, std::chrono::seconds(this->mImpl->options.TradeRequestTimeout), [this, mObject, mTargetObject]() -> void {
            if (!this->mImpl->mTradeRequests.contains(mObject) || !this->mImpl->mTradeRequests.contains(mTargetObject))
                return;

            if (Player* mPlayer = ll::service::getLevel()->getPlayer(mce::UUID::fromString(mObject)); mPlayer) {
                LanguagePlugin::getShared()->getLanguage(*mPlayer)
                    .transform([&mPlayer](const std::string& language) -> void {
                        mPlayer->sendMessage(tr(language, "market.tips2"));
                    })
                    .or_else(modules::defaultErrorHandler<MarketPlugin>);
            }
            
            if (Player* mPlayer = ll::service::getLevel()->getPlayer(mce::UUID::fromString(mTargetObject)); mPlayer) {
                LanguagePlugin::getShared()->getLanguage(*mPlayer)
                    .transform([&mPlayer](const std::string& language) -> void {
                        mPlayer->sendMessage(tr(language, "market.tips2"));
                    })
                    .or_else(modules::defaultErrorHandler<MarketPlugin>);
            }

            this->mImpl->mTradeRequests.erase(mObject);
            this->mImpl->mTradeRequests.erase(mTargetObject);
        });

        return LanguagePlugin::getShared()->getLanguage(player)
            .transform([this, &player, name = target.getRealName()](const std::string& language) -> void {
                player.sendMessage(tr(language, "market.tips1"));

                this->getLogger()->info(fmt::runtime(tr({}, "market.log6")), player.getRealName(), name);
            });
    }

    ll::Expected<void> MarketPlugin::sendTrade(Player& player, Player& target, MarketTradeType type) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(MarketPluginErrorCode::Invalid));

        std::string mObject = player.getUuid().asString();
        std::string mTargetObject = target.getUuid().asString();

        TradeEntry mEntry{ mObject, mTargetObject, type };
        this->mImpl->mTrades[mObject] = mEntry;
        this->mImpl->mTrades[mTargetObject] = mEntry;

        this->mImpl->mTimerManager->schedule(mObject + "_trade", std::chrono::seconds(this->mImpl->options.TradeTimeout), [this, mObject, mTargetObject]() -> void {
            if (!this->mImpl->mTrades.contains(mObject) || !this->mImpl->mTrades.contains(mTargetObject))
                return;

            if (Player* mPlayer = ll::service::getLevel()->getPlayer(mce::UUID::fromString(mObject)); mPlayer) {
                LanguagePlugin::getShared()->getLanguage(*mPlayer)
                    .transform([&mPlayer](const std::string& language) -> void {
                        mPlayer->sendMessage(tr(language, "market.tips5"));
                    })
                    .or_else(modules::defaultErrorHandler<MarketPlugin>);
            }
            
            if (Player* mPlayer = ll::service::getLevel()->getPlayer(mce::UUID::fromString(mTargetObject)); mPlayer) {
                LanguagePlugin::getShared()->getLanguage(*mPlayer)
                    .transform([&mPlayer](const std::string& language) -> void {
                        mPlayer->sendMessage(tr(language, "market.tips5"));
                    })
                    .or_else(modules::defaultErrorHandler<MarketPlugin>);
            }

            this->mImpl->mTrades.erase(mObject);
            this->mImpl->mTrades.erase(mTargetObject);
        });

        this->getLogger()->info(fmt::runtime(tr({}, "market.log7")), player.getRealName(), target.getRealName());

        return {};
    }

    ll::Expected<bool> MarketPlugin::hasTrade(Player& player) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(MarketPluginErrorCode::Invalid));

        return this->mImpl->mTradeRequests.contains(player.getUuid().asString()) || this->mImpl->mTrades.contains(player.getUuid().asString());
    }

    ll::Expected<std::string> MarketPlugin::getBlacklist(Player& player, Player& target) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(MarketPluginErrorCode::Invalid));

        return this->getDatabase()->find("Blacklist", {
            { "target", target.getUuid().asString() },
            { "author", player.getUuid().asString() }
        }, "", SQLiteStorage::FindCondition::AND);
    }

    ll::Expected<std::vector<std::string>> MarketPlugin::getBlacklist(Player& player) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(MarketPluginErrorCode::Invalid));

        return this->getBlacklist(player.getUuid().asString());
    }

    ll::Expected<std::vector<std::string>> MarketPlugin::getBlacklist(const std::string& target) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(MarketPluginErrorCode::Invalid));

        if (this->mImpl->BlacklistCache.contains(target))
            return *this->mImpl->BlacklistCache.get(target).value();

        return this->getDatabase()->find("Blacklist", {
            { "target", target }
        }, SQLiteStorage::FindCondition::AND)
            .transform([this, target](const std::vector<std::string>& ids) -> std::vector<std::string> {
                this->mImpl->BlacklistCache.put(target, ids);

                return ids;
            });
    }

    ll::Expected<std::vector<std::string>> MarketPlugin::getItems() {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(MarketPluginErrorCode::Invalid));

        return this->getDatabase()->list("Item");
    }

    ll::Expected<std::vector<std::string>> MarketPlugin::getItems(Player& player) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(MarketPluginErrorCode::Invalid));

        return this->getDatabase()->find("Item", {
            { "player_uuid", player.getUuid().asString() }
        }, SQLiteStorage::FindCondition::AND);
    }

    ll::Expected<std::unordered_map<std::string, std::string>> MarketPlugin::getItemData(const std::string& id) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(MarketPluginErrorCode::Invalid));

        return this->getDatabase()->get("Item", id);
    }

    ll::Expected<std::unordered_map<std::string, std::string>> MarketPlugin::getBlacklistData(const std::string& id) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(MarketPluginErrorCode::Invalid));

        return this->getDatabase()->get("Blacklist", id);
    }

    ll::Expected<std::unordered_map<std::string, std::unordered_map<std::string, std::string>>> MarketPlugin::getItemsData(const std::vector<std::string>& ids) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(MarketPluginErrorCode::Invalid));

        return this->getDatabase()->get("Item", ids);
    }

    ll::Expected<bool> MarketPlugin::hasItem(const std::string& id) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(MarketPluginErrorCode::Invalid));

        return this->getDatabase()->has("Item", id);
    }

    ll::Expected<bool> MarketPlugin::hasBlacklist(Player& player, const std::string& id) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(MarketPluginErrorCode::Invalid));

        std::string mObject = player.getUuid().asString();
        if (this->mImpl->BlacklistCache.contains(mObject)) {
            auto mKeys = this->mImpl->BlacklistCache.get(mObject).value();
            return std::find(mKeys->begin(), mKeys->end(), id) != mKeys->end();
        }

        return this->getDatabase()->has("Blacklist", id);
    }

    bool MarketPlugin::isValid() {
        return this->getLogger() != nullptr && this->getDatabase() != nullptr && this->mImpl->db2 != nullptr;
    }

    std::vector<std::string> MarketPlugin::getProhibitedItems() {
        return this->mImpl->options.ProhibitedItems;
    }

    int MarketPlugin::getBlacklistUpload() {
        return this->mImpl->options.BlacklistUpload;
    }

    int MarketPlugin::getMaximumUpload() {
        return this->mImpl->options.MaximumUpload;
    }

    std::string MarketPlugin::getName() {
        return "MarketPlugin";
    }

    modules::ModulePriority MarketPlugin::getPriority() {
        return modules::ModulePriority::High;
    }

    ll::Expected<bool> MarketPlugin::load() {
        if (!ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().ServerConfig.Plugins.Market.ModuleEnabled)
            return false;
        
        auto mDataPath = std::filesystem::path(ServiceProvider::getInstance().getService<std::string>("DataPath")->data());

        this->mImpl->db = std::make_shared<SQLiteStorage>((mDataPath / "market.db").string());
        this->mImpl->db2 = ServiceProvider::getInstance().getService<SQLiteStorage>("SettingsDB");
        this->mImpl->logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");
        this->mImpl->options = ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().ServerConfig.Plugins.Market;
        
        auto mGuiPath = std::filesystem::path(ServiceProvider::getInstance().getService<std::string>("GuiPath")->data());

        this->mImpl->mGuiPath = (mGuiPath / "market.lcui").string();
        this->mImpl->mGuiTradePath = (mGuiPath / "market_trade.lcui").string();

        return true;
    }

    ll::Expected<bool> MarketPlugin::unload() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;

        this->mImpl->db.reset();
        this->mImpl->db2.reset();
        this->mImpl->logger.reset();
        this->mImpl->options = {};

        if (this->mImpl->mRegistered.load(std::memory_order_acquire))
            this->unlistenEvent();

        return true;
    }

    ll::Expected<bool> MarketPlugin::registry() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;

        return this->mImpl->db2->create("Market", [](SQLiteStorage::ColumnCallback ctor) -> void {
            ctor("name");
            ctor("score");
        }).and_then([this]() -> ll::Expected<void> {
            return this->getDatabase()->create("Item", [](SQLiteStorage::ColumnCallback ctor) -> void {
                ctor("name");
                ctor("icon");
                ctor("introduce");
                ctor("score");
                ctor("data");
                ctor("player_name");
                ctor("player_uuid");
            });
        }).and_then([this]() -> ll::Expected<void> {
            return this->getDatabase()->create("Blacklist", [](SQLiteStorage::ColumnCallback ctor) -> void {
                ctor("name");
                ctor("target");
                ctor("author");
                ctor("time");
            });
        }).and_then([this]() -> ll::Expected<void> {
            return this->registeryUI();
        }).transform([this]() -> bool {
            this->registeryCommand();
            this->listenEvent();

            this->mImpl->mRegistered.store(true, std::memory_order_release);

            return true;
        }); 
    }

    ll::Expected<bool> MarketPlugin::unregistry() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;

        this->unlistenEvent();

        return this->getDatabase()->exec("VACUUM;")
            .transform([this]() -> bool {
                this->mImpl->mRegistered.store(false, std::memory_order_release);

                return true;
            });
    }
}
