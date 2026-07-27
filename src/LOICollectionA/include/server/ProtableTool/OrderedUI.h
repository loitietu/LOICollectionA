#pragma once

#include <memory>

#include <ll/api/Expected.h>

#include "LOICollectionA/base/Macro.h"

#include "LOICollectionA/include/ModuleBase.h"
#include "LOICollectionA/include/ModManager.h"

namespace LOICollection::server::ProtableTool {
    class OrderedUI : public std::enable_shared_from_this<OrderedUI>,
                      public modules::ModuleBase,
                      public modules::AutoRegister<OrderedUI> {
    public:
        ~OrderedUI();

        OrderedUI(OrderedUI const&) = delete;
        OrderedUI(OrderedUI&&) = delete;
        OrderedUI& operator=(OrderedUI const&) = delete;
        OrderedUI& operator=(OrderedUI&&) = delete;

    public:
        LOICOLLECTION_A_NDAPI static std::shared_ptr<OrderedUI> getShared();

    public:
        LOICOLLECTION_A_NDAPI std::string getName() override;

        LOICOLLECTION_A_NDAPI modules::ModulePriority getPriority() override;

        LOICOLLECTION_A_API   ll::Expected<bool> load() override;
        LOICOLLECTION_A_API   ll::Expected<bool> unload() override;
        LOICOLLECTION_A_API   ll::Expected<bool> registry() override;
        LOICOLLECTION_A_API   ll::Expected<bool> unregistry() override;

    private:
        OrderedUI();

        void listenEvent();
        void unlistenEvent();

        struct Impl;
        std::unique_ptr<Impl> mImpl;
    };
}