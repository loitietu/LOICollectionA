#include <string>

#include <fmt/format.h>
#include <magic_enum/magic_enum.hpp>

#include <ll/api/Expected.h>
#include <ll/api/form/CustomForm.h>
#include <ll/api/form/SimpleForm.h>

#include <mc/world/actor/player/Player.h>

#include "LOICollectionA/include/server/Plugins/LanguagePlugin.h"
#include "LOICollectionA/include/server/Plugins/StatisticsPlugin.h"

#include "LOICollectionA/include/form/PaginatedForm.h"

#include "LOICollectionA/utils/I18nUtils.h"

#include "LOICollectionA/include/server/Plugins/gui/StatisticsGui.h"

using I18nUtilsTools::tr;

namespace LOICollection::server::Plugins {
    ll::Expected<void> StatisticsGui::open(Player& player, StatisticType type) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .and_then([this, type, &player](const std::string& language) -> ll::Expected<void> { 
                auto names = this->mParent.getStatisticName(type);
                if (!names.has_value())
                    return ll::Unexpected(names.error());

                auto lists = this->mParent.getRankingList(type, this->mParent.getRankingPlayerCount());
                if (!lists.has_value())
                    return ll::Unexpected(lists.error());

                ll::form::CustomForm form(tr(language, "statistics.gui.title"));
                form.appendLabel(fmt::format(
                    fmt::runtime(tr(language, "statistics.gui.pecific.label")),
                    names.value(),
                    this->mParent.getRankingPlayerCount()
                ));
                
                for (const auto& [index, pair] : std::views::enumerate(lists.value())) {
                    auto name = this->mParent.getPlayerInfo(pair.first);
                    if (!name.has_value())
                        return ll::Unexpected(name.error());

                    form.appendLabel(fmt::format(
                        fmt::runtime(tr(language, "statistics.gui.specific.line")), 
                        index + 1,
                        name.value(),
                        pair.second
                    ));
                }

                form.sendTo(player, [this](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                    if (!dt)
                        this->open(pl).or_else(modules::defaultErrorHandler<StatisticsPlugin>);
                });

                return {};
            });
    }

    ll::Expected<void> StatisticsGui::open(Player& player) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .and_then([this, &player](const std::string& language) -> ll::Expected<void> {
                std::vector<std::string> mStatisticNames;
                std::vector<StatisticType> mStatisticTypes;

                for (auto type : magic_enum::enum_entries<StatisticType>()) {
                    auto result = this->mParent.getStatisticName(type.first);
                    if (!result.has_value())
                        return ll::Unexpected(result.error());

                    mStatisticNames.push_back(result.value());
                    mStatisticTypes.push_back(type.first);
                }

                std::shared_ptr<form::PaginatedForm> form = std::make_shared<form::PaginatedForm>(
                    tr(language, "statistics.gui.title"),
                    tr(language, "statistics.gui.label"),
                    mStatisticNames
                );
                form->setPreviousButton(tr(language, "generic.gui.page.previous"));
                form->setNextButton(tr(language, "generic.gui.page.next"));
                form->setChooseButton(tr(language, "generic.gui.page.choose"));
                form->setChooseInput(tr(language, "generic.gui.page.choose.input"));
                form->setCallback([this, mStatisticTypes = std::move(mStatisticTypes)](Player& pl, int index) -> void {
                    this->open(pl, mStatisticTypes.at(index)).or_else(modules::defaultErrorHandler<StatisticsPlugin>);
                });

                form->sendPage(player, 1);

                return {};
            });
    }
}
