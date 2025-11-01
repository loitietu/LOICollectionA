#pragma once

#include <memory>

#include "LOICollectionA/base/Macro.h"

namespace LOICollection::Plugins {
    class MonitorPlugin {
    public:
        static MonitorPlugin& getInstance() {
            static MonitorPlugin instance;
            return instance;
        }

    public:
        LOICOLLECTION_A_API bool load();
        LOICOLLECTION_A_API bool unload();
        LOICOLLECTION_A_API bool registry();
        LOICOLLECTION_A_API bool unregistry();

    private:
        MonitorPlugin();
        ~MonitorPlugin();

        void listenEvent();
        void unlistenEvent();

        struct Impl;
        std::unique_ptr<Impl> mImpl;
    };
}