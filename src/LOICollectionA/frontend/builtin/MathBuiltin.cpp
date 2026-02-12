#include <cmath>
#include <random>
#include <string>
#include <variant>

#include "LOICollectionA/frontend/Callback.h"

#include "LOICollectionA/utils/core/MathUtils.h"

#include "LOICollectionA/frontend/builtin/MathBuiltin.h"

using namespace LOICollection::frontend;

namespace MathBuiltin {
    void registerFunctions(const std::string& namespaces) {
        FunctionCall::getInstance().registerFunction(namespaces, "abs", abs, { ParamType::INT });
        FunctionCall::getInstance().registerFunction(namespaces, "abs", abs, { ParamType::FLOAT });
        FunctionCall::getInstance().registerFunction(namespaces, "min", min, { ParamType::INT, ParamType::INT });
        FunctionCall::getInstance().registerFunction(namespaces, "min", min, { ParamType::FLOAT, ParamType::FLOAT });
        FunctionCall::getInstance().registerFunction(namespaces, "max", max, { ParamType::INT, ParamType::INT });
        FunctionCall::getInstance().registerFunction(namespaces, "max", max, { ParamType::FLOAT, ParamType::FLOAT });
        FunctionCall::getInstance().registerFunction(namespaces, "sqrt", sqrt, { ParamType::INT });
        FunctionCall::getInstance().registerFunction(namespaces, "sqrt", sqrt, { ParamType::FLOAT });
        FunctionCall::getInstance().registerFunction(namespaces, "pow", pow, { ParamType::INT, ParamType::INT });
        FunctionCall::getInstance().registerFunction(namespaces, "pow", pow, { ParamType::FLOAT, ParamType::FLOAT });
        FunctionCall::getInstance().registerFunction(namespaces, "log", log, { ParamType::INT });
        FunctionCall::getInstance().registerFunction(namespaces, "log", log, { ParamType::FLOAT });
        FunctionCall::getInstance().registerFunction(namespaces, "sin", sin, { ParamType::INT });
        FunctionCall::getInstance().registerFunction(namespaces, "sin", sin, { ParamType::FLOAT });
        FunctionCall::getInstance().registerFunction(namespaces, "cos", cos, { ParamType::INT });
        FunctionCall::getInstance().registerFunction(namespaces, "cos", cos, { ParamType::FLOAT });
        FunctionCall::getInstance().registerFunction(namespaces, "random", random, { ParamType::INT, ParamType::INT });
        FunctionCall::getInstance().registerFunction(namespaces, "random", random, { ParamType::FLOAT, ParamType::FLOAT });
    }

    std::string abs(const LOICollection::frontend::CallbackTypeValues& args) {
        return std::visit([](auto&& arg) -> std::string {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, int> || std::is_same_v<T, float>)
                return std::to_string(std::abs(arg));
            else
                return {};
        }, args[0]);
    }

    std::string min(const LOICollection::frontend::CallbackTypeValues& args) {
        return std::visit([](auto&& arg1, auto&& arg2) -> std::string {
            using T1 = std::decay_t<decltype(arg1)>;
            using T2 = std::decay_t<decltype(arg2)>;

            if constexpr (
                (std::is_same_v<T1, int> && std::is_same_v<T2, int>) ||
                (std::is_same_v<T1, float> && std::is_same_v<T2, float>)
            ) {
                return std::to_string(std::min(arg1, arg2));
            } else {
                return {};
            }
        }, args[0], args[1]);
    }

    std::string max(const LOICollection::frontend::CallbackTypeValues& args) {
        return std::visit([](auto&& arg1, auto&& arg2) -> std::string {
            using T1 = std::decay_t<decltype(arg1)>;
            using T2 = std::decay_t<decltype(arg2)>;

            if constexpr (
                (std::is_same_v<T1, int> && std::is_same_v<T2, int>) ||
                (std::is_same_v<T1, float> && std::is_same_v<T2, float>)
            ) {
                return std::to_string(std::max(arg1, arg2));
            } else {
                return {};
            }
        }, args[0], args[1]);
    }

    std::string sqrt(const LOICollection::frontend::CallbackTypeValues& args) {
        return std::visit([](auto&& arg) -> std::string {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, int> || std::is_same_v<T, float>)
                return std::to_string(std::sqrt(arg));
            else
                return {};
        }, args[0]);
    }

    std::string pow(const LOICollection::frontend::CallbackTypeValues& args) {
        return std::visit([](auto&& arg1, auto&& arg2) -> std::string {
            using T1 = std::decay_t<decltype(arg1)>;
            using T2 = std::decay_t<decltype(arg2)>;

            if constexpr ((std::is_same_v<T1, int> && std::is_same_v<T2, int>))
                return std::to_string(MathUtils::pow(arg1, arg2));
            else if constexpr ((std::is_same_v<T1, float> && std::is_same_v<T2, float>))
                return std::to_string(std::pow(arg1, arg2));
            else
                return {};
        }, args[0], args[1]);
    }

    std::string log(const LOICollection::frontend::CallbackTypeValues& args) {
        return std::visit([](auto&& arg) -> std::string {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, int> || std::is_same_v<T, float>)
                return std::to_string(std::log(arg));
            else
                return {};
        }, args[0]);
    }

    std::string sin(const LOICollection::frontend::CallbackTypeValues& args) {
        return std::visit([](auto&& arg) -> std::string {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, int> || std::is_same_v<T, float>)
                return std::to_string(std::sin(arg));
            else
                return {};
        }, args[0]);
    }

    std::string cos(const LOICollection::frontend::CallbackTypeValues& args) {
        return std::visit([](auto&& arg) -> std::string {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, int> || std::is_same_v<T, float>)
                return std::to_string(std::cos(arg));
            else
                return {};
        }, args[0]);
    }

    std::string random(const LOICollection::frontend::CallbackTypeValues& args) {
        static std::random_device rd;
        static std::mt19937 gen(rd());

        return std::visit([](auto&& arg1, auto&& arg2) -> std::string {
            using T1 = std::decay_t<decltype(arg1)>;
            using T2 = std::decay_t<decltype(arg2)>;

            if constexpr (std::is_same_v<T1, int> && std::is_same_v<T2, int>) {
                std::uniform_int_distribution<> dis(arg1, arg2);
                return std::to_string(dis(gen));
            } else if constexpr (std::is_same_v<T1, float> && std::is_same_v<T2, float>) {
                std::uniform_real_distribution<> dis(arg1, arg2);
                return std::to_string(dis(gen));
            } else {
                return {};
            }
        }, args[0], args[1]);
    }
}

REGISTER_NAMESPACE("math", MathBuiltin::registerFunctions)
