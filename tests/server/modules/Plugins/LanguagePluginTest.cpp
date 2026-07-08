#include <gtest/gtest.h>

#include <string>

#include <ll/api/service/Bedrock.h>

#include <mc/world/level/Level.h>
#include <mc/world/actor/player/Player.h>

#include <mc/server/SimulatedPlayer.h>

#include "LOICollectionA/data/SQLiteStorage.h"

#include "LOICollectionA/base/ServiceProvider.h"

#include "LOICollectionA/include/server/Plugins/LanguagePlugin.h"

using namespace LOICollection::server::Plugins;

class LanguagePluginTest : public testing::Test {
protected:
    void TearDown() override {
        ServiceProvider::getInstance().getService<SQLiteStorage>("SettingsDB")->exec("DELETE FROM Language;");
    }
};

TEST_F(LanguagePluginTest, SetAndGetLanguage) {
    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    LanguagePlugin::getInstance().set(*sp, "en_US");

    std::string langcode = LanguagePlugin::getInstance().getLanguage(*sp);
    EXPECT_FALSE(langcode.empty());
    EXPECT_TRUE(langcode == "en_US");
}
