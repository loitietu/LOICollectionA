#ifndef LOICOLLECTION_A_MONITORPLUGIN_H
#define LOICOLLECTION_A_MONITORPLUGIN_H

#include <map>
#include <string>
#include <vector>
#include <variant>

namespace monitorPlugin {
    void registery(std::map<std::string, std::variant<std::string, std::vector<std::string>>>& options);
    void unregistery();
}

#endif