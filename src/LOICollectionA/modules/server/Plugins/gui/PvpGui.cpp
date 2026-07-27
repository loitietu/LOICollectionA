#include <string>

#include <ll/api/Expected.h>
#include <ll/api/form/SimpleForm.h>

#include "LOICollectionA/include/server/Plugins/LanguagePlugin.h"
#include "LOICollectionA/include/server/Plugins/PvpPlugin.h"

#include "LOICollectionA/utils/I18nUtils.h"

#include "LOICollectionA/include/server/Plugins/gui/PvpGui.h"

using I18nUtilsTools::tr;

namespace LOICollection::server::Plugins {
    ll::Expected<void> PvpGui::open(Player& player) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .transform([this, &player](const std::string& language) -> void {
                ll::form::SimpleForm form(tr(language, "pvp.gui.title"), tr(language, "pvp.gui.label"));
                form.appendButton(tr(language, "pvp.gui.on"), "textures/ui/book_addtextpage_default", "path", [this](Player& pl) -> void {
                    this->mParent.enable(pl, true).or_else(modules::defaultErrorHandler<PvpPlugin>);
                });
                form.appendButton(tr(language, "pvp.gui.off"), "textures/ui/cancel", "path", [this](Player& pl) -> void {
                    this->mParent.enable(pl, false).or_else(modules::defaultErrorHandler<PvpPlugin>);
                });
                form.sendTo(player);
            });
    }
}
