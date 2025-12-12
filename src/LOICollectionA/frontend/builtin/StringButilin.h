#pragma once

#include <string>
#include <vector>

namespace StringButilin {
    void registerFunctions(const std::string& namespaces);

    std::string length(std::vector<std::string>& args);
    std::string upper(std::vector<std::string>& args);
    std::string lower(std::vector<std::string>& args);
    std::string substr(std::vector<std::string>& args);
    std::string trim(std::vector<std::string>& args);
    std::string replace(std::vector<std::string>& args);
}
