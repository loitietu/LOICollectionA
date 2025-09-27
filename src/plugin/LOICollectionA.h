#pragma once

#include <ll/api/Mod/NativeMod.h>

#include "ConfigPlugin.h"

namespace LOICollection {
    class A {
    public:
        static A& getInstance() {
            static A instance;
            return instance;
        }

        A() : mSelf(*ll::mod::NativeMod::current()) {}

        [[nodiscard]] ll::mod::NativeMod& getSelf() const { 
            return mSelf;
        }

        bool load();
        bool enable();
        bool disable();

    private:
        ll::mod::NativeMod& mSelf;

        C_Config config;
    };
}
