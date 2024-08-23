#pragma once

#include <memory>

#include <ll/api/Mod/NativeMod.h>
#include <ll/api/data/KeyValueDB.h>

#include "Utils/JsonUtils.h"
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
        std::unique_ptr<ll::data::KeyValueDB> LanguageDB;
        std::unique_ptr<ll::data::KeyValueDB> BlacklistDB;
        std::unique_ptr<ll::data::KeyValueDB> MuteDB;
        std::unique_ptr<ll::data::KeyValueDB> TpaDB;
        std::unique_ptr<ll::data::KeyValueDB> PvpDB;
        std::unique_ptr<ll::data::KeyValueDB> ChatDB;
        std::unique_ptr<ll::data::KeyValueDB> MarketDB;
        std::unique_ptr<JsonUtils> CdkDB;
        std::unique_ptr<JsonUtils> MenuDB;
        std::unique_ptr<JsonUtils> ShopDB;
        std::unique_ptr<JsonUtils> AnnounCementDB;
    };
}
