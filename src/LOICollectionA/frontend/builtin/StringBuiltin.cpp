#include <string>
#include <variant>
#include <algorithm>

#include "LOICollectionA/frontend/Callback.h"

#include "LOICollectionA/frontend/builtin/StringBuiltin.h"

using namespace LOICollection::frontend;

namespace StringBuiltin {
    void registerFunctions(const std::string& namespaces) {
        FunctionCall::getInstance().registerFunction(namespaces, "length", length, { ParamType::STRING });
        FunctionCall::getInstance().registerFunction(namespaces, "upper", upper, { ParamType::STRING });
        FunctionCall::getInstance().registerFunction(namespaces, "lower", lower, { ParamType::STRING });
        FunctionCall::getInstance().registerFunction(namespaces, "substr", substr, { ParamType::STRING, ParamType::INT, ParamType::INT });
        FunctionCall::getInstance().registerFunction(namespaces, "trim", trim, { ParamType::STRING });
        FunctionCall::getInstance().registerFunction(namespaces, "replace", replace, { ParamType::STRING, ParamType::STRING, ParamType::STRING });
    }

    std::string length(const LOICollection::frontend::CallbackTypeValues& args) {
        return std::visit([](auto&& arg) -> std::string {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, std::string>)
                return std::to_string(arg.length());
            else
                return {};
        }, args[0]);
    }

    std::string upper(const LOICollection::frontend::CallbackTypeValues& args) {
        return std::visit([](auto&& arg) -> std::string {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, std::string>) {
                std::string result = arg;
                std::transform(result.begin(), result.end(), result.begin(), ::toupper);
                return result;
            } else {
                return {};
            }
        }, args[0]);
    }

    std::string lower(const LOICollection::frontend::CallbackTypeValues& args) {
        return std::visit([](auto&& arg) -> std::string {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, std::string>) {
                std::string result = arg;
                std::transform(result.begin(), result.end(), result.begin(), ::tolower);
                return result;
            } else {
                return {};
            }
        }, args[0]);
    }

    std::string substr(const LOICollection::frontend::CallbackTypeValues& args) {
        return std::visit([](auto&& arg1, auto&& arg2, auto&& arg3) -> std::string {
            using T1 = std::decay_t<decltype(arg1)>;
            using T2 = std::decay_t<decltype(arg2)>;
            using T3 = std::decay_t<decltype(arg3)>;

            if constexpr (std::is_same_v<T1, std::string> && std::is_same_v<T2, int> && std::is_same_v<T3, int>)
                return arg1.substr(arg2, arg3);
            else
                return {};
        }, args[0], args[1], args[2]);
    }

    std::string trim(const LOICollection::frontend::CallbackTypeValues& args) {
        return std::visit([](auto&& arg) -> std::string {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, std::string>) {
                std::string result = arg;
                result.erase(0, result.find_first_not_of(" \t\n\r"));
                result.erase(result.find_last_not_of(" \t\n\r") + 1);
                return result;
            } else {
                return {};
            }
        }, args[0]);
    }

    std::string replace(const LOICollection::frontend::CallbackTypeValues& args) {
        return std::visit([](auto&& arg1, auto&& arg2, auto&& arg3) -> std::string {
            using T1 = std::decay_t<decltype(arg1)>;
            using T2 = std::decay_t<decltype(arg2)>;
            using T3 = std::decay_t<decltype(arg3)>;

            if constexpr (std::is_same_v<T1, std::string> && std::is_same_v<T2, std::string> && std::is_same_v<T3, std::string>) {
                std::string result = arg1;
                size_t pos = 0;
                while ((pos = result.find(arg2, pos)) != std::string::npos) {
                    result.replace(pos, arg2.length(), arg3);

                    pos += arg3.length();
                }

                return result;
            } else {
                return {};
            }
        }, args[0], args[1], args[2]);
    }
}

REGISTER_NAMESPACE("string", StringBuiltin::registerFunctions);
