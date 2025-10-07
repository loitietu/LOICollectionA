#pragma once

#include <memory>
#include <string>
#include <vector> 
#include <functional>
#include <unordered_map>

#include "base/Macro.h"

class Vec3;
class SQLiteStorage;

namespace ll::io {
    class Logger;
}

namespace LOICollection::Plugins {
    class BehaviorEventPlugin {
    public:
        static BehaviorEventPlugin& getInstance() {
            static BehaviorEventPlugin instance;
            return instance;
        }

        LOICOLLECTION_A_NDAPI SQLiteStorage* getDatabase();
        LOICOLLECTION_A_NDAPI ll::io::Logger* getLogger();

        LOICOLLECTION_A_NDAPI std::unordered_map<std::string, std::string> getBasicEvent(const std::string& name, const std::string& type, const Vec3& position, int dimension);

        LOICOLLECTION_A_NDAPI std::vector<std::string> getEvents();
        LOICOLLECTION_A_NDAPI std::vector<std::string> getEvents(std::vector<std::pair<std::string, std::string>> conditions, std::function<bool(std::string)> filter = {});
        LOICOLLECTION_A_NDAPI std::vector<std::string> getEventsByPoition(int dimension, std::function<bool(int x, int y, int z)> filter);
        LOICOLLECTION_A_API   std::vector<std::string> filter(std::vector<std::string> ids);

        LOICOLLECTION_A_API   void back(const std::string& id);
        LOICOLLECTION_A_API   void clean(int hours);

        LOICOLLECTION_A_NDAPI bool isValid();
    
    public:
        LOICOLLECTION_A_API bool load();
        LOICOLLECTION_A_API bool registry();
        LOICOLLECTION_A_API bool unregistry();

    private:
        BehaviorEventPlugin();
        ~BehaviorEventPlugin();

        void registeryCommand();
        void listenEvent();
        void unlistenEvent();

        struct operation;

        struct Impl;
        std::unique_ptr<Impl> mImpl;
    };
}