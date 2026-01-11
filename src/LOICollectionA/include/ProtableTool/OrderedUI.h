#pragma once

#include <memory>

#include "LOICollectionA/base/Macro.h"

namespace LOICollection::ProtableTool {
    class OrderedUI {
    public:
        ~OrderedUI();

        OrderedUI(OrderedUI const&) = delete;
        OrderedUI(OrderedUI&&) = delete;
        OrderedUI& operator=(OrderedUI const&) = delete;
        OrderedUI& operator=(OrderedUI&&) = delete;

    public:
        LOICOLLECTION_A_NDAPI static OrderedUI& getInstance();

    public:
        LOICOLLECTION_A_API bool load();
        LOICOLLECTION_A_API bool unload();
        LOICOLLECTION_A_API bool registry();
        LOICOLLECTION_A_API bool unregistry();

    private:
        OrderedUI();

        void listenEvent();
        void unlistenEvent();

        struct Impl;
        std::unique_ptr<Impl> mImpl;
    };
}