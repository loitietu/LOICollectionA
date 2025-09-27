#pragma once

#include <memory>

#include "base/Macro.h"

struct ModalFormRequestPacketHook;
struct ModalFormResponsePacketHook;

namespace LOICollection::ProtableTool {
    class OrderedUI {
    public:
        static OrderedUI& getInstance() {
            static OrderedUI instance;
            return instance;
        }

        friend struct ::ModalFormRequestPacketHook;
        friend struct ::ModalFormResponsePacketHook;

    public:
        LOICOLLECTION_A_API bool load();
        LOICOLLECTION_A_API bool registry();
        LOICOLLECTION_A_API bool unregistry();

    private:
        OrderedUI();
        ~OrderedUI();

        struct Impl;
        std::unique_ptr<Impl> mImpl;
    };
}