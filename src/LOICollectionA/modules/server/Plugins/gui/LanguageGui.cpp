#include <string>
#include <ranges>
#include <vector>

#include <fmt/core.h>

#include <ll/api/io/Logger.h>
#include <ll/api/form/CustomForm.h>

#include <mc/world/actor/player/Player.h>

#include "LOICollectionA/include/server/APIUtils.h"
#include "LOICollectionA/include/server/Plugins/LanguagePlugin.h"

#include "LOICollectionA/utils/I18nUtils.h"

#include "LOICollectionA/include/server/Plugins/gui/LanguageGui.h"

using I18nUtilsTools::tr;

namespace LOICollection::server::Plugins {
    void LanguageGui::open(Player& player) {
        std::string mObjectLanguage = this->mParent.getLanguage(player);

        std::vector<std::string> keys = std::views::keys(I18nUtils::getInstance()->data)
            | std::ranges::to<std::vector<std::string>>();
        
        ll::form::CustomForm form(tr(mObjectLanguage, "language.gui.title"));
        form.appendLabel(tr(mObjectLanguage, "language.gui.label"));
        form.appendLabel(fmt::format(fmt::runtime(tr(mObjectLanguage, "language.gui.lang")), tr(mObjectLanguage, "name")));
        form.appendDropdown("dropdown", tr(mObjectLanguage, "language.gui.dropdown"), keys);
        form.sendTo(player, [this](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) mutable -> void {
            if (!dt) return;

            this->mParent.set(pl, std::get<std::string>(dt->at("dropdown")));
            
            this->mParent.getLogger()->info(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "language.log"), pl));
        });
    }
}
