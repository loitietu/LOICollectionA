#pragma once

#include <memory>

#include "base/Macro.h"

struct RedStoneWireBlockHook;
struct RedStoneTorchBlockHook;
struct DiodeBlockHook;
struct ComparatorBlockHook;
struct ObserverBlockHook;

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

        friend struct ::RedStoneWireBlockHook;
        friend struct ::RedStoneTorchBlockHook;
        friend struct ::DiodeBlockHook;
        friend struct ::ComparatorBlockHook;
        friend struct ::ObserverBlockHook;

        LOICOLLECTION_A_NDAPI ll::io::Logger* getLogger();

    public:
        LOICOLLECTION_A_API bool load();
        LOICOLLECTION_A_API bool registry();
        LOICOLLECTION_A_API bool unregistry();

    private:
        RedStone();
        ~RedStone();

        struct Impl;
        std::unique_ptr<Impl> mImpl;
    };
}