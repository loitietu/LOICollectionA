#pragma once

#include <memory>

#include "LOICollectionA/base/Macro.h"

namespace LOICollection::ProtableTool {
    class BasicHook {
    public:
        static BasicHook& getInstance() {
            static BasicHook instance;
            return instance;
        }

    public:
        LOICOLLECTION_A_API bool load();
        LOICOLLECTION_A_API bool unload();
        LOICOLLECTION_A_API bool registry();
        LOICOLLECTION_A_API bool unregistry();

    private:
        BasicHook();
        ~BasicHook();

        void listenEvent();
        void unlistenEvent();

        struct Impl;
        std::unique_ptr<Impl> mImpl;
    };
}