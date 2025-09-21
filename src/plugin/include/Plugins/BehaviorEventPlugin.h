#pragma once

#include <string>
#include <vector> 
#include <functional>
#include <unordered_map>

#include "base/Macro.h"

class Vec3;

namespace LOICollection::Plugins::behaviorevent {
    LOICOLLECTION_A_NDAPI std::unordered_map<std::string, std::string> getBasicEvent(const std::string& name, const std::string& type, const Vec3& position, int dimension);

    LOICOLLECTION_A_NDAPI std::vector<std::string> getEvents();
    LOICOLLECTION_A_NDAPI std::vector<std::string> getEvents(std::vector<std::pair<std::string, std::string>> conditions, std::function<bool(std::string)> filter = {});
    LOICOLLECTION_A_NDAPI std::vector<std::string> getEventsByPoition(int dimension, std::function<bool(int x, int y, int z)> filter);

    LOICOLLECTION_A_API   void back(const std::string& id);
    LOICOLLECTION_A_API   void clean(int hours);

    LOICOLLECTION_A_NDAPI bool isValid();

    LOICOLLECTION_A_API   void registery(void* database);
    LOICOLLECTION_A_API   void unregistery();
}