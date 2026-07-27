#pragma once

#include <memory>

#include <ll/api/Expected.h>

#include "LOICollectionA/base/Macro.h"

#include "LOICollectionA/include/ModuleBase.h"
#include "LOICollectionA/include/ModManager.h"

namespace ll::io {
    class Logger;
}

namespace LOICollection::server::ProtableTool{
    class RedStone : public std::enable_shared_from_this<RedStone>,
                     public modules::ModuleBase,
                     public modules::AutoRegister<RedStone> {
    public:
        ~RedStone();

        RedStone(RedStone const&) = delete;
        RedStone(RedStone&&) = delete;
        RedStone& operator=(RedStone const&) = delete;
        RedStone& operator=(RedStone&&) = delete;
    
    public:
        LOICOLLECTION_A_NDAPI static std::shared_ptr<RedStone> getShared();

        LOICOLLECTION_A_NDAPI std::shared_ptr<ll::io::Logger> getLogger();

    public:
        LOICOLLECTION_A_NDAPI std::string getName() override;

        LOICOLLECTION_A_NDAPI modules::ModulePriority getPriority() override;

        LOICOLLECTION_A_API   ll::Expected<bool> load() override;
        LOICOLLECTION_A_API   ll::Expected<bool> unload() override;
        LOICOLLECTION_A_API   ll::Expected<bool> registry() override;
        LOICOLLECTION_A_API   ll::Expected<bool> unregistry() override;

    private:
        RedStone();

        void listenEvent();
        void unlistenEvent();

        struct Impl;
        std::unique_ptr<Impl> mImpl;
    };
}