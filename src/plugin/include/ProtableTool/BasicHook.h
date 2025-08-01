#pragma once

#include <string>

#include "base/Macro.h"

namespace LOICollection::ProtableTool::BasicHook {
    LOICOLLECTION_A_API void registery(const std::string& fakeSeed);
    LOICOLLECTION_A_API void unregistery();
}