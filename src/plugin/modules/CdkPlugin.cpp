#include <memory>
#include <string>

#include <fmt/core.h>

#include <ll/api/io/Logger.h>
#include <ll/api/io/LoggerRegistry.h>

#include <ll/api/form/ModalForm.h>
#include <ll/api/form/CustomForm.h>
#include <ll/api/form/SimpleForm.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/command/Command.h>
#include <ll/api/command/CommandHandle.h>
#include <ll/api/command/CommandRegistrar.h>
#include <ll/api/utils/StringUtils.h>

#include <mc/nbt/Tag.h>
#include <mc/nbt/CompoundTag.h>

#include <mc/world/item/ItemStack.h>
#include <mc/world/actor/player/Player.h>

#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandPermissionLevel.h>

#include <mc/safety/RedactableString.h>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "include/APIUtils.h"
#include "include/LanguagePlugin.h"
#include "include/ChatPlugin.h"

#include "utils/InventoryUtils.h"
#include "utils/ScoreboardUtils.h"
#include "utils/SystemUtils.h"
#include "utils/I18nUtils.h"

#include "data/JsonStorage.h"

#include "include/CdkPlugin.h"

using I18nUtilsTools::tr;
using LOICollection::Plugins::language::getLanguage;

namespace LOICollection::Plugins::cdk {
    struct CDKOP {
        std::string Id;
    };

    std::unique_ptr<JsonStorage> db;
    std::shared_ptr<ll::io::Logger> logger;

    namespace MainGui {
        void convert(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::CustomForm form(tr(mObjectLanguage, "cdk.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "cdk.gui.label"));
            form.appendInput("Input", tr(mObjectLanguage, "cdk.gui.convert.input"), tr(mObjectLanguage, "cdk.gui.convert.input.placeholder"));
            form.sendTo(player, [mObjectLanguage](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                if (!dt) return;

                std::string mCdk = std::get<std::string>(dt->at("Input"));

                if (mCdk.empty()) {
                    pl.sendMessage(tr(mObjectLanguage, "generic.tips.noinput"));
                    return;
                }

                cdk::convert(pl, mCdk);
            });
        }

        void cdkNew(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::CustomForm form(tr(mObjectLanguage, "cdk.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "cdk.gui.label"));
            form.appendInput("Input1", tr(mObjectLanguage, "cdk.gui.new.input1"), tr(mObjectLanguage, "cdk.gui.new.input1.placeholder"));
            form.appendToggle("Toggle", tr(mObjectLanguage, "cdk.gui.new.switch"));
            form.appendInput("Input2", tr(mObjectLanguage, "cdk.gui.new.input2"), tr(mObjectLanguage, "cdk.gui.new.input2.placeholder"));
            form.sendTo(player, [mObjectLanguage](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                if (!dt) return MainGui::open(pl);

                std::string mObjectCdk = std::get<std::string>(dt->at("Input1"));

                if (mObjectCdk.empty()) {
                    pl.sendMessage(tr(mObjectLanguage, "generic.tips.noinput"));
                    return MainGui::open(pl);
                }

                if (!db->has(mObjectCdk)) {
                    int time = SystemUtils::toInt(std::get<std::string>(dt->at("Input2")), 0);
                    std::string mObjectTime = time ? SystemUtils::timeCalculate(SystemUtils::getNowTime(), time, "0") : "0";

                    nlohmann::ordered_json dataList = {
                        { "personal", (bool)std::get<uint64>(dt->at("Toggle")) },
                        { "player", nlohmann::ordered_json::array() },
                        { "scores", nlohmann::ordered_json::object() },
                        { "item", nlohmann::ordered_json::array() },
                        { "title", nlohmann::ordered_json::object() },
                        { "time", mObjectTime }
                    };

                    db->set(mObjectCdk, dataList);
                    db->save();
                }

                logger->info(ll::string_utils::replaceAll(tr({}, "cdk.log1"), "${cdk}", mObjectCdk));
            });
        }

        void cdkRemoveInfo(Player& player, const std::string& id) {
            std::string mObjectLanguage = getLanguage(player);

            std::string mObjectContent = tr(mObjectLanguage, "cdk.gui.remove.content");
            ll::string_utils::replaceAll(mObjectContent, "${cdk}", id);

            ll::form::ModalForm form(tr(mObjectLanguage, "cdk.gui.title"), mObjectContent,
                tr(mObjectLanguage, "cdk.gui.remove.yes"), tr(mObjectLanguage, "cdk.gui.remove.no")
            );
            form.sendTo(player, [id](Player& pl, ll::form::ModalFormResult result, ll::form::FormCancelReason) -> void {
                if (result != ll::form::ModalFormSelectedButton::Upper) 
                    return MainGui::open(pl);

                db->remove(id);
                db->save();

                logger->info(ll::string_utils::replaceAll(tr({}, "cdk.log2"), "${cdk}", id));
            });
        }

        void cdkRemove(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::SimpleForm form(tr(mObjectLanguage, "cdk.gui.title"), tr(mObjectLanguage, "cdk.gui.remove.label"));
            for (std::string& mItem : db->keys()) {
                form.appendButton(mItem, [mItem](Player& pl) {
                    MainGui::cdkRemoveInfo(pl, mItem);
                });
            }
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) -> void {
                if (id == -1) MainGui::open(pl);
            });
        }

        void cdkAwardScore(Player& player, const std::string& id) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::CustomForm form(tr(mObjectLanguage, "cdk.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "cdk.gui.label"));
            form.appendInput("Input1", tr(mObjectLanguage, "cdk.gui.award.score.input1"), tr(mObjectLanguage, "cdk.gui.award.score.input1.placeholder"));
            form.appendInput("Input2", tr(mObjectLanguage, "cdk.gui.award.score.input2"), tr(mObjectLanguage, "cdk.gui.award.score.input2.placeholder"));
            form.sendTo(player, [mObjectLanguage, id](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                if (!dt) return MainGui::cdkAward(pl);

                std::string mObjective = std::get<std::string>(dt->at("Input1"));

                if (mObjective.empty() || !ScoreboardUtils::hasScoreboard(mObjective)) {
                    pl.sendMessage(tr(mObjectLanguage, "generic.tips.noinput"));
                    return MainGui::cdkAwardInfo(pl, id);
                }

                int mScore = SystemUtils::toInt(std::get<std::string>(dt->at("Input2")), 0);

                db->set_ptr("/" + id + "/scores/" + mObjective, mScore);
                db->save(); 
            });
        }

        void cdkAwardItem(Player& player, const std::string& id) {
            std::string mObjectLanguage = getLanguage(player);
            
            ll::form::CustomForm form(tr(mObjectLanguage, "cdk.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "cdk.gui.label"));
            form.appendInput("Input1", tr(mObjectLanguage, "cdk.gui.award.item.input1"), tr(mObjectLanguage, "cdk.gui.award.item.input1.placeholder"));
            form.appendInput("Input2", tr(mObjectLanguage, "cdk.gui.award.item.input2"), tr(mObjectLanguage, "cdk.gui.award.item.input2.placeholder"));
            form.appendInput("Input3", tr(mObjectLanguage, "cdk.gui.award.item.input3"), tr(mObjectLanguage, "cdk.gui.award.item.input3.placeholder"));
            form.appendInput("Input4", tr(mObjectLanguage, "cdk.gui.award.item.input4"), tr(mObjectLanguage, "cdk.gui.award.item.input4.placeholder"));
            form.appendDropdown("dropdown2", tr(mObjectLanguage, "cdk.gui.award.item.dropdown"), { "universal", "nbt" });
            form.sendTo(player, [mObjectLanguage, id](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                if (!dt) return MainGui::cdkAward(pl);

                std::string mObjectId = std::get<std::string>(dt->at("Input1"));
                std::string mObjectName = std::get<std::string>(dt->at("Input2"));

                if (mObjectId.empty() || mObjectName.empty()) {
                    pl.sendMessage(tr(mObjectLanguage, "generic.tips.noinput"));
                    return MainGui::cdkAwardInfo(pl, id);
                }

                std::string mObjectType = std::get<std::string>(dt->at("dropdown2"));

                nlohmann::ordered_json mItemData = {
                    { "id", mObjectId },
                    { "type", mObjectType }
                };
                if (mObjectType == "universal") {
                    mItemData["name"] = mObjectName;
                    mItemData["quantity"] = SystemUtils::toInt(std::get<std::string>(dt->at("Input3")), 1);
                    mItemData["specialvalue"] = SystemUtils::toInt(std::get<std::string>(dt->at("Input4")), 0);
                }

                int mIndex = (int) db->get_ptr<nlohmann::ordered_json>("/" + id + "/item").size();

                db->set_ptr("/" + id + "/item/" + std::to_string(mIndex), mItemData);
                db->save();
            });
        }

        void cdkAwardTitle(Player& player, const std::string& id) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::CustomForm form(tr(mObjectLanguage, "cdk.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "cdk.gui.label"));
            form.appendInput("Input1", tr(mObjectLanguage, "cdk.gui.award.title.input1"), tr(mObjectLanguage, "cdk.gui.award.title.input1.placeholder"));
            form.appendInput("Input2", tr(mObjectLanguage, "cdk.gui.award.title.input2"), tr(mObjectLanguage, "cdk.gui.award.title.input2.placeholder"));
            form.sendTo(player, [mObjectLanguage, id](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                if (!dt) return MainGui::cdkAward(pl);

                std::string mObjectTitle = std::get<std::string>(dt->at("Input1"));

                if (mObjectTitle.empty()) {
                    pl.sendMessage(tr(mObjectLanguage, "generic.tips.noinput"));
                    return MainGui::cdkAwardInfo(pl, id);
                }

                int mObjectData = SystemUtils::toInt(std::get<std::string>(dt->at("Input2")), 0);
                
                db->set_ptr("/" + id + "/title/" + mObjectTitle, mObjectData);
                db->save();
            });
        }

        void cdkAwardInfo(Player& player, const std::string& id) {
            std::string mObjectLanguage = getLanguage(player);

            std::string mObjectLabel = tr(mObjectLanguage, "cdk.gui.award.info.label");
            ll::string_utils::replaceAll(mObjectLabel, "${cdk}", id);
            ll::string_utils::replaceAll(mObjectLabel, "${personal}", db->get_ptr<bool>("/" + id + "/personal", false) ? "true" : "false");
            ll::string_utils::replaceAll(mObjectLabel, "${time}", SystemUtils::formatDataTime(db->get_ptr<std::string>("/" + id + "/time", "None"), "None"));

            ll::form::SimpleForm form(tr(mObjectLanguage, "cdk.gui.title"), mObjectLabel);
            form.appendButton(tr(mObjectLanguage, "cdk.gui.award.score"), "textures/items/diamond_sword", "path", [id](Player& pl) -> void {
                MainGui::cdkAwardScore(pl, id);
            });
            form.appendButton(tr(mObjectLanguage, "cdk.gui.award.item"), "textures/items/diamond", "path", [id](Player& pl) -> void {
                MainGui::cdkAwardItem(pl, id);
            });
            form.appendButton(tr(mObjectLanguage, "cdk.gui.award.title"), "textures/ui/backup_replace", "path", [id](Player& pl) -> void {
                MainGui::cdkAwardTitle(pl, id);
            });
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) -> void {
                if (id == -1) MainGui::open(pl);
            });
        }

        void cdkAward(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::SimpleForm form(tr(mObjectLanguage, "cdk.gui.title"), tr(mObjectLanguage, "cdk.gui.award.label"));
            for (std::string& mItem : db->keys()) {
                form.appendButton(mItem, [mItem](Player& pl) {
                    MainGui::cdkAwardInfo(pl, mItem);
                });
            }
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) -> void {
                if (id == -1) MainGui::open(pl);
            });
        }

        void open(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::SimpleForm form(tr(mObjectLanguage, "cdk.gui.title"), tr(mObjectLanguage, "cdk.gui.label"));
            form.appendButton(tr(mObjectLanguage, "cdk.gui.addCdk"), "textures/ui/book_addtextpage_default", "path", [](Player& pl) -> void {
                MainGui::cdkNew(pl);
            });
            form.appendButton(tr(mObjectLanguage, "cdk.gui.removeCdk"), "textures/ui/cancel", "path", [](Player& pl) -> void {
                MainGui::cdkRemove(pl);
            });
            form.appendButton(tr(mObjectLanguage, "cdk.gui.addAward"), "textures/ui/color_picker", "path", [](Player& pl) -> void {
                MainGui::cdkAward(pl);
            });
            form.sendTo(player);
        }
    }

    namespace {
        void registerCommand() {
            ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance()
                .getOrCreateCommand("cdk", tr({}, "commands.cdk.description"), CommandPermissionLevel::Any);
            command.overload<CDKOP>().text("convert").required("Id").execute(
                [](CommandOrigin const& origin, CommandOutput& output, CDKOP const& param) -> void {
                Actor* entity = origin.getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return output.error(tr({}, "commands.generic.target"));
                Player& player = *static_cast<Player*>(entity);

                convert(player, param.Id);
                
                output.success(fmt::runtime(tr({}, "commands.cdk.success.convert")), player.getRealName(), param.Id);
            });
            command.overload().text("gui").execute([](CommandOrigin const& origin, CommandOutput& output) -> void {
                Actor* entity = origin.getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return output.error(tr({}, "commands.generic.target"));
                Player& player = *static_cast<Player*>(entity);

                MainGui::convert(player);

                output.success(fmt::runtime(tr({}, "commands.generic.ui")), player.getRealName());
            });
            command.overload().text("edit").execute([](CommandOrigin const& origin, CommandOutput& output) -> void {
                if (origin.getPermissionsLevel() < CommandPermissionLevel::GameDirectors)
                    return output.error(tr({}, "commands.generic.permission"));
                
                Actor* entity = origin.getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return output.error(tr({}, "commands.generic.target"));
                Player& player = *static_cast<Player*>(entity);

                MainGui::open(player);

                output.success(fmt::runtime(tr({}, "commands.generic.ui")), player.getRealName());
            });
        }
    }

    void convert(Player& player, const std::string& id) {
        if (!isValid()) 
            return;

        auto data = db->get<nlohmann::ordered_json>(id);

        std::string mObjectLanguage = getLanguage(player);

        if (data.is_null()) {
            player.sendMessage(tr(mObjectLanguage, "cdk.convert.tips1"));
            return;
        }

        if (SystemUtils::isReach(data.value("time", ""))) {
            db->remove(id);
            db->save();

            player.sendMessage(tr(mObjectLanguage, "cdk.convert.tips1"));
            return;
        }

        std::string mUuid = player.getUuid().asString();
        if (auto it = data.value<nlohmann::ordered_json>("player", {}); std::find(it.begin(), it.end(), mUuid) != it.end()) {
            player.sendMessage(tr(mObjectLanguage, "cdk.convert.tips2"));
            return;
        }

        for (auto elements = data.value<nlohmann::ordered_json>("title", {}); auto& element : elements.items())
            chat::addTitle(player, element.key(), element.value());

        for (auto elements = data.value<nlohmann::ordered_json>("scores", {}); auto& element : elements.items())
            ScoreboardUtils::addScore(player, element.key(), element.value());

        for (auto& value : data.value<nlohmann::ordered_json>("item", {})) {
            if (value.value("type", "") == "nbt") {
                ItemStack itemStack = ItemStack::fromTag(CompoundTag::fromSnbt(value.value("id", ""))->mTags);
                InventoryUtils::giveItem(player, itemStack, (int)itemStack.mCount);
            } else {
                Bedrock::Safety::RedactableString mRedactableString;
                mRedactableString.mUnredactedString = value.value("name", "");
                
                ItemStack itemStack(value.value("id", ""), 1, value.value("specialvalue", 0), nullptr);
                itemStack.setCustomName(mRedactableString);
                
                InventoryUtils::giveItem(player, itemStack, value.value("quantity", 1));
            }
        }

        player.refreshInventory();
        player.sendMessage(tr(mObjectLanguage, "cdk.convert.tips3"));

        data.value("personal", false) ? (void)db->remove(id) : data.at("player").push_back(mUuid);

        db->set(id, data);
        db->save();

        logger->info(LOICollectionAPI::getVariableString(
            ll::string_utils::replaceAll(tr({}, "cdk.log3"), "${cdk}", id), player
        ));
    }

    bool isValid() {
        return logger != nullptr && db != nullptr;
    }

    void registery(void* database) {
        db = std::move(*static_cast<std::unique_ptr<JsonStorage>*>(database));
        logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");

        registerCommand();
    }

    void unregistery() {
        db->save();
    }
}