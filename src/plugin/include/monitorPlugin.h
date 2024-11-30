#ifndef LOICOLLECTION_A_MONITORPLUGIN_H
#define LOICOLLECTION_A_MONITORPLUGIN_H

#include <map>
#include <string>
#include <vector>
#include <variant>

#include "ExportLib.h"

namespace LOICollection::Plugins::monitor {
    LOICOLLECTION_A_API void registery(std::map<std::string, std::variant<std::string, std::vector<std::string>, int, bool>>& options);
    LOICOLLECTION_A_API void unregistery();
}

#endif