#pragma once

#include <string>
#include <vector>

namespace StringBuiltin {
    void registerFunctions(const std::string& namespaces);

    std::string length(const std::vector<std::string>& args);
    std::string upper(const std::vector<std::string>& args);
    std::string lower(const std::vector<std::string>& args);
    std::string substr(const std::vector<std::string>& args);
    std::string trim(const std::vector<std::string>& args);
    std::string replace(const std::vector<std::string>& args);
}
