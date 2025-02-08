#pragma once

#include <map>
#include <string>

#include "base/Macro.h"

namespace LOICollection::ProtableTool::BasicHook {
    LOICOLLECTION_A_API void registery(std::map<std::string, std::string>& options);
    LOICOLLECTION_A_API void unregistery();
}