#include <gtest/gtest.h>

#include <string>

#include <ll/api/service/Bedrock.h>

#include <mc/world/level/Level.h>
#include <mc/world/actor/player/Player.h>

#include <mc/server/SimulatedPlayer.h>

#include "LOICollectionA/base/ServiceProvider.h"

#include "LOICollectionA/data/JsonStorage.h"
#include "LOICollectionA/data/SQLiteStorage.h"

#include "LOICollectionA/include/server/Plugins/NoticePlugin.h"

using namespace LOICollection::server::Plugins;

class NoticePluginTest : public testing::Test {
protected:
    void SetUp() override {
        if (!NoticePlugin::getInstance().isValid())
            GTEST_SKIP() << "NoticePlugin is not valid";
    }

    void TearDown() override {
        NoticePlugin::getInstance().getDatabase()->write({});
        NoticePlugin::getInstance().getDatabase()->save();

        ServiceProvider::getInstance().getService<SQLiteStorage>("SettingsDB")->exec("DELETE FROM Notice;");
    }
};

TEST_F(NoticePluginTest, CreateNotice) {
    NoticePlugin::getInstance().create("test_notice", "Test Notice", 1, false);

    EXPECT_TRUE(NoticePlugin::getInstance().has("test_notice"));
}

TEST_F(NoticePluginTest, RemoveNotice) {
    NoticePlugin::getInstance().create("test_notice", "Test Notice", 1, false);

    EXPECT_TRUE(NoticePlugin::getInstance().has("test_notice"));

    NoticePlugin::getInstance().remove("test_notice");

    EXPECT_FALSE(NoticePlugin::getInstance().has("test_notice"));
}

TEST_F(NoticePluginTest, SettingCloseStatus) {
    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    NoticePlugin::getInstance().setClose(*sp, true);
    EXPECT_TRUE(NoticePlugin::getInstance().isClose(*sp));

    NoticePlugin::getInstance().setClose(*sp, false);
    EXPECT_FALSE(NoticePlugin::getInstance().isClose(*sp));
}
