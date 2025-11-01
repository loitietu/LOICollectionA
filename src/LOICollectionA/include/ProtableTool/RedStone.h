#pragma once

#include <memory>

#include "LOICollectionA/base/Macro.h"

namespace ll::io {
    class Logger;
}

namespace LOICollection::ProtableTool{
    class RedStone {
    public:
        static RedStone& getInstance() {
            static RedStone instance;
            return instance;
        }

        LOICOLLECTION_A_NDAPI ll::io::Logger* getLogger();

    public:
        LOICOLLECTION_A_API bool load();
        LOICOLLECTION_A_API bool unload();
        LOICOLLECTION_A_API bool registry();
        LOICOLLECTION_A_API bool unregistry();

    private:
        RedStone();
        ~RedStone();

        void listenEvent();
        void unlistenEvent();

        struct Impl;
        std::unique_ptr<Impl> mImpl;
    };
}