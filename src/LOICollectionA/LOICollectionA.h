#pragma once

#include <ll/api/Mod/NativeMod.h>

#include "LOICollectionA/ConfigPlugin.h"

namespace LOICollection {
    class A {
    public:
        static A& getInstance();

        A() : mSelf(*ll::mod::NativeMod::current()) {}

        [[nodiscard]] ll::mod::NativeMod& getSelf() const { 
            return mSelf;
        }

        bool load();
        bool unload();
        bool enable();
        bool disable();

    private:
        ll::mod::NativeMod& mSelf;

        C_Config config;
    };
}
