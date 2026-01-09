#include <string>
#include <vector>
#include <algorithm>

#include "LOICollectionA/frontend/FunctionCall.h"

#include "LOICollectionA/frontend/builtin/StringBuiltin.h"

using namespace LOICollection::frontend;

namespace StringBuiltin {
    void registerFunctions(const std::string& namespaces) {
        FunctionCall::getInstance().registerFunction(namespaces, "length", length, 1);
        FunctionCall::getInstance().registerFunction(namespaces, "upper", upper, 1);
        FunctionCall::getInstance().registerFunction(namespaces, "lower", lower, 1);
        FunctionCall::getInstance().registerFunction(namespaces, "substr", substr, 3);
        FunctionCall::getInstance().registerFunction(namespaces, "trim", trim, 1);
        FunctionCall::getInstance().registerFunction(namespaces, "replace", replace, 3);
    }

    std::string length(std::vector<std::string>& args) {
        const std::string& arg = args[0];

        return std::to_string(arg.length());
    }

    std::string upper(std::vector<std::string>& args) {
        const std::string& arg = args[0];

        std::string result = arg;
        std::transform(result.begin(), result.end(), result.begin(), ::toupper);
        
        return result;
    }

    std::string lower(std::vector<std::string>& args) {
        const std::string& arg = args[0];

        std::string result = arg;
        std::transform(result.begin(), result.end(), result.begin(), ::tolower);

        return result;
    }

    std::string substr(std::vector<std::string>& args) {
        const std::string& arg = args[0];
        const int start = std::stoi(args[1]);
        const int length = std::stoi(args[2]);

        return arg.substr(start, length);
    }

    std::string trim(std::vector<std::string>& args) {
        const std::string& arg = args[0];

        std::string result = arg;
        
        size_t start = result.find_first_not_of(" \t\n\r");
        if (start == std::string::npos)
            return "";

        size_t end = result.find_last_not_of(" \t\n\r");
        return result.substr(start, end - start + 1);
    }

    std::string replace(std::vector<std::string>& args) {
        const std::string& arg = args[0];
        const std::string& oldStr = args[1];
        const std::string& newStr = args[2];

        std::string result = arg;

        size_t pos = 0;
        while ((pos = result.find(oldStr, pos)) != std::string::npos) {
            result.replace(pos, oldStr.length(), newStr);
            
            pos += newStr.length();
        }

        return result;
    }
}

REGISTER_NAMESPACE("string", StringBuiltin::registerFunctions);
