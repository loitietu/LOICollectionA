#include <string>

#include <fmt/format.h>
#include <magic_enum/magic_enum.hpp>

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
    void StatisticsGui::open(Player& player, StatisticType type) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        std::string mObjectLabel = tr(mObjectLanguage, "statistics.gui.specific.label");

        ll::form::CustomForm form(tr(mObjectLanguage, "statistics.gui.title"));
        form.appendLabel(fmt::format(fmt::runtime(mObjectLabel), this->mParent.getStatisticName(type), this->mParent.getRankingPlayerCount()));
        
        for (const auto& [index, pair] : std::views::enumerate(this->mParent.getRankingList(type, this->mParent.getRankingPlayerCount()))) {
            form.appendLabel(fmt::format(
                fmt::runtime(tr(mObjectLanguage, "statistics.gui.specific.line")), 
                index + 1,
                this->mParent.getPlayerInfo(pair.first),
                pair.second
            ));
        }

        form.sendTo(player, [this](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
            if (!dt) return this->open(pl);
        });
    }

    void StatisticsGui::open(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        std::vector<std::string> mStatisticNames;
        std::vector<StatisticType> mStatisticTypes;

        for (auto type : magic_enum::enum_entries<StatisticType>()) {
            mStatisticNames.push_back(this->mParent.getStatisticName(type.first));
            mStatisticTypes.push_back(type.first);
        }

        std::shared_ptr<form::PaginatedForm> form = std::make_shared<form::PaginatedForm>(
            tr(mObjectLanguage, "statistics.gui.title"),
            tr(mObjectLanguage, "statistics.gui.label"),
            mStatisticNames
        );
        form->setPreviousButton(tr(mObjectLanguage, "generic.gui.page.previous"));
        form->setNextButton(tr(mObjectLanguage, "generic.gui.page.next"));
        form->setChooseButton(tr(mObjectLanguage, "generic.gui.page.choose"));
        form->setChooseInput(tr(mObjectLanguage, "generic.gui.page.choose.input"));
        form->setCallback([this, mStatisticTypes = std::move(mStatisticTypes)](Player& pl, int index) -> void {
            this->open(pl, mStatisticTypes.at(index));
        });

        form->sendPage(player, 1);
    }
}
