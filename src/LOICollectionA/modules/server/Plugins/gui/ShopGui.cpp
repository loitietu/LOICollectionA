#include <string>
#include <vector>
#include <utility>
#include <memory>

#include <nlohmann/json.hpp>

#include <ll/api/Expected.h>
#include <ll/api/io/Logger.h>
#include <ll/api/form/ModalForm.h>
#include <ll/api/form/CustomForm.h>
#include <ll/api/form/SimpleForm.h>

#include <mc/world/actor/player/Player.h>

#include "LOICollectionA/include/server/APIUtils.h"
#include "LOICollectionA/include/server/Plugins/LanguagePlugin.h"
#include "LOICollectionA/include/server/Plugins/ShopPlugin.h"

#include "LOICollectionA/include/form/PaginatedForm.h"

#include "LOICollectionA/utils/I18nUtils.h"
#include "LOICollectionA/utils/mc-server/CommandUtils.h"
#include "LOICollectionA/utils/core/SystemUtils.h"

#include "LOICollectionA/data/JsonStorage.h"

#include "LOICollectionA/include/server/Plugins/gui/ShopGui.h"

using I18nUtilsTools::tr;

namespace LOICollection::server::Plugins {
    ll::Expected<void> ShopGui::menu(Player& player, const std::string& id, ShopType type) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .transform([this,id, type, &player](const std::string& language) -> void {
                auto data = this->mParent.getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id).value_or(nlohmann::ordered_json{});

                std::vector<std::pair<std::string, std::string>> mItems;
                std::vector<nlohmann::ordered_json> mItemIds;

                for (auto& item : data.value("classiflcation", nlohmann::ordered_json::array())) {
                    mItems.emplace_back(LOICollectionAPI::APIUtils::getInstance().translate(item.value("title", ""), player), item.value("image", ""));
                    mItemIds.emplace_back(item);
                }

                std::shared_ptr<form::PaginatedForm> form = std::make_shared<form::PaginatedForm>(
                    LOICollectionAPI::APIUtils::getInstance().translate(data.value("title", ""), player),
                    LOICollectionAPI::APIUtils::getInstance().translate(data.value("content", ""), player),
                    mItems
                );
                form->setPreviousButton(tr(language, "generic.gui.page.previous"));
                form->setNextButton(tr(language, "generic.gui.page.next"));
                form->setChooseButton(tr(language, "generic.gui.page.choose"));
                form->setChooseInput(tr(language, "generic.gui.page.choose.input"));
                form->setCallback([this, type, id, mItemIds = std::move(mItemIds)](Player& pl, int index) -> void {
                    const nlohmann::ordered_json& item = mItemIds.at(index);

                    switch (ll::hash_utils::doHash(item.value("type", ""))) {
                        case ll::hash_utils::doHash("commodity"):
                            this->commodity(pl, index, id, type).or_else(modules::defaultErrorHandler<ShopPlugin>);
                            break;
                        case ll::hash_utils::doHash("title"):
                            this->title(pl, index, id, type).or_else(modules::defaultErrorHandler<ShopPlugin>);
                            break;
                        case ll::hash_utils::doHash("from"):
                            this->open(pl, item).or_else(modules::defaultErrorHandler<ShopPlugin>);
                            break;
                        default:
                            modules::defaultErrorHandler<ShopPlugin>(ll::makeErrorCodeError(ShopPlugin::makeErrorCode(ShopPluginErrorCode::UnknownType)));
                            break;
                    };
                });
                form->setCloseCallback([data](Player& pl) -> void {
                    CommandUtils::executeCommand(pl, data.value("info", nlohmann::ordered_json()).value("exit", ""));
                });

                form->sendPage(player, 1);
            });
    }

    ll::Expected<void> ShopGui::commodity(Player& player, int index, const std::string& id, ShopType type) {
        auto original = this->mParent.getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id).value_or(nlohmann::ordered_json{});
        auto data = original.at("classiflcation").at(index);

        ll::form::CustomForm form(LOICollectionAPI::APIUtils::getInstance().translate(data.value("title", ""), player));
        form.appendLabel(LOICollectionAPI::APIUtils::getInstance().translate(data.value("introduce", ""), player));
        form.appendInput("Input", LOICollectionAPI::APIUtils::getInstance().translate(data.value("number", ""), player), "", "1");
        form.sendTo(player, [this, original = std::move(original), data = std::move(data), type](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
            if (!dt) return CommandUtils::executeCommand(pl, original.value("info", nlohmann::ordered_json()).value("exit", ""));

            int mNumber = SystemUtils::toInt(std::get<std::string>(dt->at("Input")), 0);
            if (mNumber > 2304 || mNumber <= 0)
                return;

            if (!this->mParent.commodity(pl, mNumber, data, type))
                return CommandUtils::executeCommand(pl, original.value("info", nlohmann::ordered_json()).value(type == ShopType::buy ? "score" : "item", ""));
        });

        return {};
    }

    ll::Expected<void> ShopGui::title(Player& player, int index, const std::string& id, ShopType type) {
        auto original = this->mParent.getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id).value_or(nlohmann::ordered_json{});
        auto data = original.at("classiflcation").at(index);

        ll::form::ModalForm form(
            LOICollectionAPI::APIUtils::getInstance().translate(data.value("title", ""), player),
            LOICollectionAPI::APIUtils::getInstance().translate(data.value("introduce", ""), player),
            LOICollectionAPI::APIUtils::getInstance().translate(data.value("confirmButton", "confirm"), player),
            LOICollectionAPI::APIUtils::getInstance().translate(data.value("cancelButton", "cancel"), player)
        );
        form.sendTo(player, [this, original = std::move(original), data = std::move(data), type](Player& pl, ll::form::ModalFormResult result, ll::form::FormCancelReason) -> void {
            if (result == ll::form::ModalFormSelectedButton::Upper) {
                if (!this->mParent.title(pl, data, type))
                    return CommandUtils::executeCommand(pl, original.value("info", nlohmann::ordered_json()).value(type == ShopType::buy ? "score" : "title", ""));
            }
        });

        return {};
    }

    ll::Expected<void> ShopGui::open(Player& player, const std::string& id) {
        if (this->mParent.getDatabase()->has(id)) {
            auto data = this->mParent.getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id).value_or(nlohmann::ordered_json{});

            if (data.empty())
                return {};
            
            switch (ll::hash_utils::doHash(data.value("type", ""))) {
                case ll::hash_utils::doHash("buy"): return this->menu(player, id, ShopType::buy);
                case ll::hash_utils::doHash("sell"): return this->menu(player, id, ShopType::sell);
            };

            return ll::makeErrorCodeError(ShopPlugin::makeErrorCode(ShopPluginErrorCode::UnknownType));
        }

        return ll::makeErrorCodeError(ShopPlugin::makeErrorCode(ShopPluginErrorCode::NotFound));
    }
}
