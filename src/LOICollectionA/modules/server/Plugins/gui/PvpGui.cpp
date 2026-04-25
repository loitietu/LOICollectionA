#include <string>

#include <ll/api/form/SimpleForm.h>

#include "LOICollectionA/include/server/Plugins/LanguagePlugin.h"
#include "LOICollectionA/include/server/Plugins/PvpPlugin.h"

#include "LOICollectionA/utils/I18nUtils.h"

#include "LOICollectionA/include/server/Plugins/gui/PvpGui.h"

using I18nUtilsTools::tr;

namespace LOICollection::server::Plugins {
    void PvpGui::open(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);
        
        ll::form::SimpleForm form(tr(mObjectLanguage, "pvp.gui.title"), tr(mObjectLanguage, "pvp.gui.label"));
        form.appendButton(tr(mObjectLanguage, "pvp.gui.on"), "textures/ui/book_addtextpage_default", "path", [this](Player& pl) -> void {
            this->mParent.enable(pl, true);
        });
        form.appendButton(tr(mObjectLanguage, "pvp.gui.off"), "textures/ui/cancel", "path", [this](Player& pl) -> void {
            this->mParent.enable(pl, false);
        });
        form.sendTo(player);
    }
}
