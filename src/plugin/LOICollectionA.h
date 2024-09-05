#pragma once

#include <memory>

#include <ll/api/Mod/NativeMod.h>

#include "Utils/JsonUtils.h"
#include "Utils/SQLiteStorage.h"

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
        std::unique_ptr<SQLiteStorage> LanguageDB;
        std::unique_ptr<SQLiteStorage> BlacklistDB;
        std::unique_ptr<SQLiteStorage> MuteDB;
        std::unique_ptr<SQLiteStorage> TpaDB;
        std::unique_ptr<SQLiteStorage> PvpDB;
        std::unique_ptr<SQLiteStorage> ChatDB;
        std::unique_ptr<SQLiteStorage> MarketDB;
        std::unique_ptr<JsonUtils> CdkDB;
        std::unique_ptr<JsonUtils> MenuDB;
        std::unique_ptr<JsonUtils> ShopDB;
        std::unique_ptr<JsonUtils> AnnounCementDB;
    };
}
