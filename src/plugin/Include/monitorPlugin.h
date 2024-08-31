#ifndef LOICOLLECTION_A_MONITORPLUGIN_H
#define LOICOLLECTION_A_MONITORPLUGIN_H

#include <map>
#include <vector>
#include <string>

namespace monitorPlugin {
    void registery(std::map<std::string, std::string>& options, std::vector<std::string>& commands);
    void unregistery();
}

#endif