#include <string>
#include <vector>
#include <utility>

#include <nlohmann/json.hpp>

#include <ll/api/Expected.h>
#include <ll/api/io/Logger.h>
#include <ll/api/form/ModalForm.h>
#include <ll/api/form/CustomForm.h>
#include <ll/api/form/SimpleForm.h>

#include <mc/world/actor/player/Player.h>

#include "LOICollectionA/include/server/APIUtils.h"
#include "LOICollectionA/include/server/Plugins/MenuPlugin.h"

#include "LOICollectionA/utils/mc-server/CommandUtils.h"

#include "LOICollectionA/data/JsonStorage.h"

#include "LOICollectionA/include/server/Plugins/gui/MenuGui.h"

namespace LOICollection::server::Plugins {
    ll::Expected<void> MenuGui::custom(Player& player, const std::string& id) {
        nlohmann::ordered_json mCustomData{};

        auto data = this->mParent.getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id).value_or(nlohmann::ordered_json{});

        ll::form::CustomForm form(LOICollectionAPI::APIUtils::getInstance().translate(data.value("title", ""), player));
        
        for (nlohmann::ordered_json& customize : data.value("customize", nlohmann::ordered_json())) {
            switch (ll::hash_utils::doHash(customize.value("type", ""))) {
                case ll::hash_utils::doHash("header"):
                    form.appendHeader(LOICollectionAPI::APIUtils::getInstance().translate(customize.value("title", ""), player));
                    break;
                case ll::hash_utils::doHash("Label"):
                    form.appendLabel(LOICollectionAPI::APIUtils::getInstance().translate(customize.value("title", ""), player));
                    break;
                case ll::hash_utils::doHash("divider"):
                    form.appendDivider();
                    break;
                case ll::hash_utils::doHash("Input"): {
                    form.appendInput(
                        customize.value("id", "ID"),
                        LOICollectionAPI::APIUtils::getInstance().translate(customize.value("title", ""), player), 
                        customize.value("placeholder", ""),
                        customize.value("defaultValue", ""),
                        customize.value("tooltip", "")
                    );

                    mCustomData[customize.value("id", "ID")] = customize.value("defaultValue", "");
                    break;
                }
                case ll::hash_utils::doHash("Dropdown"): {
                    std::vector<std::string> mOptions = customize.value("options", std::vector<std::string>());
                    if (mOptions.empty())
                        break;

                    form.appendDropdown(
                        customize.value("id", "ID"),
                        LOICollectionAPI::APIUtils::getInstance().translate(customize.value("title", ""), player), 
                        mOptions,
                        customize.value("defaultValue", 0),
                        customize.value("tooltip", "")
                    );

                    mCustomData[customize.value("id", "ID")] = mOptions.at(customize.value("defaultValue", 0));
                    break;
                }
                case ll::hash_utils::doHash("Toggle"): {
                    form.appendToggle(
                        customize.value("id", "ID"),
                        LOICollectionAPI::APIUtils::getInstance().translate(customize.value("title", ""), player),
                        customize.value("defaultValue", false),
                        customize.value("tooltip", "")
                    );

                    mCustomData[customize.value("id", "ID")] = customize.value("defaultValue", false);
                    break;
                }
                case ll::hash_utils::doHash("Slider"): {
                    form.appendSlider(
                        customize.value("id", "ID"), 
                        LOICollectionAPI::APIUtils::getInstance().translate(customize.value("title", ""), player),
                        customize.value("min", 0),
                        customize.value("max", 100),
                        customize.value("step", 1),
                        customize.value("defaultValue", 0),
                        customize.value("tooltip", "")
                    );

                    mCustomData[customize.value("id", "ID")] = customize.value("defaultValue", 0);
                    break;
                }
                case ll::hash_utils::doHash("StepSlider"): {
                    std::vector<std::string> mOptions = customize.value("options", std::vector<std::string>());
                    if (mOptions.size() < 2)
                        break;

                    form.appendStepSlider(
                        customize.value("id", "ID"),
                        LOICollectionAPI::APIUtils::getInstance().translate(customize.value("title", ""), player),
                        mOptions,
                        customize.value("defaultValue", 0),
                        customize.value("tooltip", "")
                    );

                    mCustomData[customize.value("id", "ID")] = mOptions.at(customize.value("defaultValue", 0));
                    break;
                }
            }
        }

        if (data.contains("submit"))
            form.setSubmitButton(LOICollectionAPI::APIUtils::getInstance().translate(data.value("submit", ""), player));

        form.sendTo(player, [mCustomData = std::move(mCustomData), data = std::move(data)](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
            if (!dt) return CommandUtils::executeCommand(pl, data.value("info", nlohmann::ordered_json{}).value("exit", ""));

            nlohmann::ordered_json mCustom;
            for (auto& item : mCustomData.items()) {
                if (item.value().is_string()) {
                    std::string result = std::get<std::string>(dt->at(item.key()));
                    if (!result.empty())
                        mCustom[item.key()] = result;
                }

                if (item.value().is_boolean()) mCustom[item.key()] = std::get<uint64>(dt->at(item.key())) ? "true" : "false";
                if (item.value().is_number_integer()) mCustom[item.key()] = std::to_string(static_cast<int>(std::get<double>(dt->at(item.key()))));
            }

            for (const auto& c_it : data.value("run", nlohmann::ordered_json())) {
                std::string result = c_it.get<std::string>();
                for (auto& item : mCustom.items()) {
                    if (result.find("{" + item.key() + "}") == std::string::npos)
                        continue;

                    ll::string_utils::replaceAll(result, "{" + item.key() + "}", item.value().get<std::string>());
                }

                CommandUtils::executeCommand(pl, result);
            }
        });

        return {};
    }

    ll::Expected<void> MenuGui::simple(Player& player, const std::string& id) {
        auto data = this->mParent.getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id).value_or(nlohmann::ordered_json{});

        ll::form::SimpleForm form(LOICollectionAPI::APIUtils::getInstance().translate(data.value("title", ""), player), LOICollectionAPI::APIUtils::getInstance().translate(data.value("content", ""), player));
        for (nlohmann::ordered_json& customize : data.value("customize", nlohmann::ordered_json())) {
            switch (ll::hash_utils::doHash(customize.value("type", ""))) {
                case ll::hash_utils::doHash("button"):
                case ll::hash_utils::doHash("from"):
                    form.appendButton(LOICollectionAPI::APIUtils::getInstance().translate(customize.value("title", ""), player), customize.value("image", ""), "path", [this, data, customize](Player& pl) -> void {
                        this->mParent.handleAction(pl, customize, data)
                            .or_else([](ll::Error e) -> ll::Expected<void> {
                                if (e.isA<ll::ErrorCodeError>()
                                    && (e.as<ll::ErrorCodeError>().ec == MenuPlugin::makeErrorCode(MenuPluginErrorCode::InsufficientScore)
                                        || e.as<ll::ErrorCodeError>().ec == MenuPlugin::makeErrorCode(MenuPluginErrorCode::PermissionDenied)))
                                    return {};

                                return ll::Unexpected(e);
                            })
                            .or_else(modules::defaultErrorHandler<MenuPlugin>);
                    });
                    break;
                case ll::hash_utils::doHash("header"):
                    form.appendHeader(LOICollectionAPI::APIUtils::getInstance().translate(customize.value("title", ""), player));
                    break;
                case ll::hash_utils::doHash("label"): 
                    form.appendLabel(LOICollectionAPI::APIUtils::getInstance().translate(customize.value("title", ""), player));
                    break;
                case ll::hash_utils::doHash("divider"):
                    form.appendDivider();
                    break;
            }
        }
        form.sendTo(player, [data = std::move(data)](Player& pl, int id, ll::form::FormCancelReason) -> void {
            if (id == -1) return CommandUtils::executeCommand(pl, data.value("info", nlohmann::ordered_json{}).value("exit", ""));
        });

        return {};
    }

    ll::Expected<void> MenuGui::modal(Player& player, const std::string& id) {
        auto data = this->mParent.getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id).value_or(nlohmann::ordered_json{});

        nlohmann::ordered_json mConfirmButton = data.value("confirmButton", nlohmann::ordered_json());
        nlohmann::ordered_json mCancelButton = data.value("cancelButton", nlohmann::ordered_json());
        if (mCancelButton.empty() || mConfirmButton.empty())
            return {};

        ll::form::ModalForm form(
            LOICollectionAPI::APIUtils::getInstance().translate(data.value("title", ""), player),
            LOICollectionAPI::APIUtils::getInstance().translate(data.value("content", ""), player),
            LOICollectionAPI::APIUtils::getInstance().translate(mConfirmButton.value("title", ""), player),
            LOICollectionAPI::APIUtils::getInstance().translate(mCancelButton.value("title", ""), player)
        );
        form.sendTo(player, [this, data = std::move(data), mConfirmButton = std::move(mConfirmButton), mCancelButton = std::move(mCancelButton)](Player& pl, ll::form::ModalFormResult result, ll::form::FormCancelReason) -> void {
            if (result == ll::form::ModalFormSelectedButton::Upper) {
                this->mParent.handleAction(pl, mConfirmButton, data)
                    .or_else([](ll::Error e) -> ll::Expected<void> {
                        if (e.isA<ll::ErrorCodeError>()
                            && (e.as<ll::ErrorCodeError>().ec == MenuPlugin::makeErrorCode(MenuPluginErrorCode::InsufficientScore)
                                || e.as<ll::ErrorCodeError>().ec == MenuPlugin::makeErrorCode(MenuPluginErrorCode::PermissionDenied)))
                            return {};

                        return ll::Unexpected(e);
                    })
                    .or_else(modules::defaultErrorHandler<MenuPlugin>);
            }
            
            this->mParent.handleAction(pl, mCancelButton, data)
                .or_else([](ll::Error e) -> ll::Expected<void> {
                    if (e.isA<ll::ErrorCodeError>()
                        && (e.as<ll::ErrorCodeError>().ec == MenuPlugin::makeErrorCode(MenuPluginErrorCode::InsufficientScore)
                            || e.as<ll::ErrorCodeError>().ec == MenuPlugin::makeErrorCode(MenuPluginErrorCode::PermissionDenied)))
                        return {};

                    return ll::Unexpected(e);
                })
                .or_else(modules::defaultErrorHandler<MenuPlugin>);
        });

        return {};
    }

    ll::Expected<void> MenuGui::open(Player& player, const std::string& id) {
        if (this->mParent.getDatabase()->has(id)) {
            auto data = this->mParent.getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id).value_or(nlohmann::ordered_json{});
            
            if (data.empty()) return {};
            if (data.contains("permission")) {
                if (static_cast<int>(player.getCommandPermissionLevel()) < data.value("permission", 0)) {
                    CommandUtils::executeCommand(player, data.value("info", nlohmann::ordered_json{}).value("permission", ""));

                    return ll::makeErrorCodeError(MenuPlugin::makeErrorCode(MenuPluginErrorCode::PermissionDenied));
                }
            }
            
            switch (ll::hash_utils::doHash(data.value("type", ""))) {
                case ll::hash_utils::doHash("Custom"): return this->custom(player, id);
                case ll::hash_utils::doHash("Simple"): return this->simple(player, id);
                case ll::hash_utils::doHash("Modal"): return this->modal(player, id);
            }

            return ll::makeErrorCodeError(MenuPlugin::makeErrorCode(MenuPluginErrorCode::UnknownType));
        }

        return ll::makeErrorCodeError(MenuPlugin::makeErrorCode(MenuPluginErrorCode::NotFound));
    }
}
