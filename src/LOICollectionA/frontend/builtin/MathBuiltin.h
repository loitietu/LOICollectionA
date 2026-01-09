#pragma once

#include <string>
#include <vector>

namespace MathBuiltin {
    void registerFunctions(const std::string& namespaces);

    std::string abs(std::vector<std::string>& args);
    std::string min(std::vector<std::string>& args);
    std::string max(std::vector<std::string>& args);
    std::string sqrt(std::vector<std::string>& args);
    std::string pow(std::vector<std::string>& args);
    std::string log(std::vector<std::string>& args);
    std::string sin(std::vector<std::string>& args);
    std::string cos(std::vector<std::string>& args);

    std::string random(std::vector<std::string>& args);
}
