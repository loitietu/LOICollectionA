#pragma once

#include <memory>

#include "LOICollectionA/base/Macro.h"

namespace LOICollection::ProtableTool {
    class OrderedUI {
    public:
        static OrderedUI& getInstance() {
            static OrderedUI instance;
            return instance;
        }

    public:
        LOICOLLECTION_A_API bool load();
        LOICOLLECTION_A_API bool unload();
        LOICOLLECTION_A_API bool registry();
        LOICOLLECTION_A_API bool unregistry();

    private:
        OrderedUI();
        ~OrderedUI();

        void listenEvent();
        void unlistenEvent();

        struct Impl;
        std::unique_ptr<Impl> mImpl;
    };
}