#pragma once

#include <memory>

#include "LOICollectionA/base/Macro.h"

struct ServerStartGamePacketHook;

namespace LOICollection::ProtableTool {
    class BasicHook {
    public:
        static BasicHook& getInstance() {
            static BasicHook instance;
            return instance;
        }

        friend struct ::ServerStartGamePacketHook;

    public:
        LOICOLLECTION_A_API bool load();
        LOICOLLECTION_A_API bool registry();
        LOICOLLECTION_A_API bool unregistry();

    private:
        BasicHook();
        ~BasicHook();

        struct Impl;
        std::unique_ptr<Impl> mImpl;
    };
}