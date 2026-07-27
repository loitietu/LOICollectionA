#include <string>
#include <ranges>
#include <vector>

#include <fmt/core.h>

#include <ll/api/Expected.h>
#include <ll/api/io/Logger.h>
#include <ll/api/form/CustomForm.h>

#include <mc/world/actor/player/Player.h>

#include "LOICollectionA/include/server/APIUtils.h"
#include "LOICollectionA/include/server/Plugins/LanguagePlugin.h"

#include "LOICollectionA/utils/I18nUtils.h"

#include "LOICollectionA/include/server/Plugins/gui/LanguageGui.h"

using I18nUtilsTools::tr;

namespace LOICollection::server::Plugins {
    ll::Expected<void> LanguageGui::open(Player& player) {
        return this->mParent.getLanguage(player)
            .transform([this, &player](const std::string& language) -> void {
                std::vector<std::string> keys = std::views::keys(I18nUtils::getInstance()->data)
                    | std::ranges::to<std::vector<std::string>>();
                
                ll::form::CustomForm form(tr(language, "language.gui.title"));
                form.appendLabel(tr(language, "language.gui.label"));
                form.appendLabel(fmt::format(fmt::runtime(tr(language, "language.gui.lang")), tr(language, "name")));
                form.appendDropdown("dropdown", tr(language, "language.gui.dropdown"), keys);
                form.sendTo(player, [this](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) mutable -> void {
                    if (!dt) return;

                    this->mParent.set(pl, std::get<std::string>(dt->at("dropdown"))).or_else(modules::defaultErrorHandler<LanguagePlugin>);
                    
                    this->mParent.getLogger()->info(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "language.log"), pl));
                });
            });
    }
}
