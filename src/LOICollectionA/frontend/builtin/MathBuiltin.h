#pragma once

#include <string>
#include <vector>

namespace MathBuiltin {
    void registerFunctions(const std::string& namespaces);

    std::string abs(const std::vector<std::string>& args);
    std::string min(const std::vector<std::string>& args);
    std::string max(const std::vector<std::string>& args);
    std::string sqrt(const std::vector<std::string>& args);
    std::string pow(const std::vector<std::string>& args);
    std::string log(const std::vector<std::string>& args);
    std::string sin(const std::vector<std::string>& args);
    std::string cos(const std::vector<std::string>& args);

    std::string random(const std::vector<std::string>& args);
}
