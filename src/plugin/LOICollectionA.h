#pragma once

#include <memory>

#include <ll/api/Mod/NativeMod.h>

#include "data/JsonStorage.h"
#include "data/SQLiteStorage.h"

#include "ConfigPlugin.h"

namespace LOICollection {
    class A {
    public:
        static A& getInstance();
        A(ll::mod::NativeMod& self) : mSelf(self) {}
        [[nodiscard]] ll::mod::NativeMod& getSelf() const { return mSelf; }
        bool load();
        bool enable();
        bool disable();

    private:
        ll::mod::NativeMod& mSelf;

        C_Config config;
        std::shared_ptr<SQLiteStorage> SettingsDB;
        std::unique_ptr<SQLiteStorage> BlacklistDB;
        std::unique_ptr<SQLiteStorage> MuteDB;
        std::unique_ptr<SQLiteStorage> ChatDB;
        std::unique_ptr<SQLiteStorage> MarketDB;
        std::unique_ptr<JsonStorage> AnnounCementDB;
        std::unique_ptr<JsonStorage> CdkDB;
        std::unique_ptr<JsonStorage> MenuDB;
        std::unique_ptr<JsonStorage> ShopDB;
    };
}
