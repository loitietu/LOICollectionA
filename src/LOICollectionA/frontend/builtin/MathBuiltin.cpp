#include <cmath>
#include <random>
#include <string>
#include <vector>

#include "LOICollectionA/frontend/FunctionCall.h"

#include "LOICollectionA/frontend/builtin/MathBuiltin.h"

using namespace LOICollection::frontend;

namespace MathBuiltin {
    void registerFunctions(const std::string& namespaces) {
        FunctionCall::getInstance().registerFunction(namespaces, "abs", abs, 1);
        FunctionCall::getInstance().registerFunction(namespaces, "min", min, 2);
        FunctionCall::getInstance().registerFunction(namespaces, "max", max, 2);
        FunctionCall::getInstance().registerFunction(namespaces, "sqrt", sqrt, 1);
        FunctionCall::getInstance().registerFunction(namespaces, "pow", pow, 2);
        FunctionCall::getInstance().registerFunction(namespaces, "log", log, 1);
        FunctionCall::getInstance().registerFunction(namespaces, "sin", sin, 1);
        FunctionCall::getInstance().registerFunction(namespaces, "cos", cos, 1);
        FunctionCall::getInstance().registerFunction(namespaces, "random", random, 2);
    }

    std::string abs(std::vector<std::string>& args) {
        const std::string& arg = args[0];

        if (FunctionCall::getInstance().isInteger(arg))
            return std::to_string(std::abs(std::stoi(arg)));
        
        return std::to_string(std::abs(std::stof(arg)));
    }

    std::string min(std::vector<std::string>& args) {
        const std::string& arg1 = args[0];
        const std::string& arg2 = args[1];

        if (FunctionCall::getInstance().isInteger(arg1) && FunctionCall::getInstance().isInteger(arg2))
            return std::to_string(std::min(std::stoi(arg1), std::stoi(arg2)));
        
        return std::to_string(std::min(std::stof(arg1), std::stof(arg2)));
    }

    std::string max(std::vector<std::string>& args) {
        const std::string& arg1 = args[0];
        const std::string& arg2 = args[1];

        if (FunctionCall::getInstance().isInteger(arg1) && FunctionCall::getInstance().isInteger(arg2))
            return std::to_string(std::max(std::stoi(arg1), std::stoi(arg2)));
        
        return std::to_string(std::max(std::stof(arg1), std::stof(arg2)));
    }

    std::string sqrt(std::vector<std::string>& args) {
        const std::string& arg = args[0];

        if (FunctionCall::getInstance().isInteger(arg))
            return std::to_string(std::sqrt(std::stoi(arg)));
        
        return std::to_string(std::sqrt(std::stof(arg)));
    }

    std::string pow(std::vector<std::string>& args) {
        const std::string& arg1 = args[0];
        const std::string& arg2 = args[1];

        if (FunctionCall::getInstance().isInteger(arg1) && FunctionCall::getInstance().isInteger(arg2))
            return std::to_string(std::pow(std::stoi(arg1), std::stoi(arg2)));
        
        return std::to_string(std::pow(std::stof(arg1), std::stof(arg2)));
    }

    std::string log(std::vector<std::string>& args) {
        const std::string& arg = args[0];

        if (FunctionCall::getInstance().isInteger(arg))
            return std::to_string(std::log(std::stoi(arg)));
        
        return std::to_string(std::log(std::stof(arg)));
    }

    std::string sin(std::vector<std::string>& args) {
        const std::string& arg = args[0];

        if (FunctionCall::getInstance().isInteger(arg))
            return std::to_string(std::sin(std::stoi(arg)));
        
        return std::to_string(std::sin(std::stof(arg)));
    }

    std::string cos(std::vector<std::string>& args) {
        const std::string& arg = args[0];

        if (FunctionCall::getInstance().isInteger(arg))
            return std::to_string(std::cos(std::stoi(arg)));
        
        return std::to_string(std::cos(std::stof(arg)));
    }

    std::string random(std::vector<std::string>& args) {
        static std::random_device rd;
        static std::mt19937 gen(rd());

        const std::string& arg1 = args[0];
        const std::string& arg2 = args[1];

        if (FunctionCall::getInstance().isInteger(arg1) && FunctionCall::getInstance().isInteger(arg2)) {
            std::uniform_int_distribution<> dis(std::stoi(arg1), std::stoi(arg2));
            return std::to_string(dis(gen));
        }

        std::uniform_real_distribution<> dis(std::stof(arg1), std::stof(arg2));
        return std::to_string(dis(gen));
    }
}

REGISTER_NAMESPACE("math", MathBuiltin::registerFunctions)
