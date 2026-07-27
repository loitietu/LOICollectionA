#pragma once

#include <memory>

#include <ll/api/Expected.h>

#include "LOICollectionA/base/Macro.h"

#include "LOICollectionA/include/ModuleBase.h"
#include "LOICollectionA/include/ModManager.h"

namespace LOICollection::server::ProtableTool {
    class BasicHook : public std::enable_shared_from_this<BasicHook>,
                      public modules::ModuleBase,
                      public modules::AutoRegister<BasicHook> {
    public:
        ~BasicHook();

        BasicHook(BasicHook const&) = delete;
        BasicHook(BasicHook&&) = delete;
        BasicHook& operator=(BasicHook const&) = delete;
        BasicHook& operator=(BasicHook&&) = delete;

    public:
        LOICOLLECTION_A_NDAPI static std::shared_ptr<BasicHook> getShared();

    public:
        LOICOLLECTION_A_NDAPI std::string getName() override;

        LOICOLLECTION_A_NDAPI modules::ModulePriority getPriority() override;

        LOICOLLECTION_A_API   ll::Expected<bool> load() override;
        LOICOLLECTION_A_API   ll::Expected<bool> unload() override;
        LOICOLLECTION_A_API   ll::Expected<bool> registry() override;
        LOICOLLECTION_A_API   ll::Expected<bool> unregistry() override;

    private:
        BasicHook();

        void listenEvent();
        void unlistenEvent();

        struct Impl;
        std::unique_ptr<Impl> mImpl;
    };
}