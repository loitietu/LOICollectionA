#pragma once

#include <memory>

#include "LOICollectionA/base/Macro.h"

namespace LOICollection::ProtableTool {
    class BasicHook {
    public:
        ~BasicHook();

        BasicHook(BasicHook const&) = delete;
        BasicHook(BasicHook&&) = delete;
        BasicHook& operator=(BasicHook const&) = delete;
        BasicHook& operator=(BasicHook&&) = delete;

    public:
        LOICOLLECTION_A_NDAPI static BasicHook& getInstance();

    public:
        LOICOLLECTION_A_API bool load();
        LOICOLLECTION_A_API bool unload();
        LOICOLLECTION_A_API bool registry();
        LOICOLLECTION_A_API bool unregistry();

    private:
        BasicHook();

        void listenEvent();
        void unlistenEvent();

        struct Impl;
        std::unique_ptr<Impl> mImpl;
    };
}