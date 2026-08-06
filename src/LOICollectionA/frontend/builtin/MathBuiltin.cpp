#include <cmath>
#include <random>
#include <string>
#include <variant>

#include "LOICollectionA/frontend/Callback.h"

#include "LOICollectionA/utils/core/MathUtils.h"

#include "LOICollectionA/frontend/builtin/MathBuiltin.h"

using namespace LOICollection::frontend;

namespace MathBuiltin {
    LOICollection::frontend::TypedValue abs(const LOICollection::frontend::CallbackTypeValues& args) {
        return std::visit([](auto&& arg) -> LOICollection::frontend::TypedValue {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, int> || std::is_same_v<T, float>)
                return std::abs(arg);

            return {};
        }, args[0]);
    }

    LOICollection::frontend::TypedValue min(const LOICollection::frontend::CallbackTypeValues& args) {
        return std::visit([](auto&& arg1, auto&& arg2) -> LOICollection::frontend::TypedValue {
            using T1 = std::decay_t<decltype(arg1)>;
            using T2 = std::decay_t<decltype(arg2)>;

            if constexpr (
                (std::is_same_v<T1, int> && std::is_same_v<T2, int>) ||
                (std::is_same_v<T1, float> && std::is_same_v<T2, float>)
            ) return std::min(arg1, arg2);

            return {};
        }, args[0], args[1]);
    }

    LOICollection::frontend::TypedValue max(const LOICollection::frontend::CallbackTypeValues& args) {
        return std::visit([](auto&& arg1, auto&& arg2) -> LOICollection::frontend::TypedValue {
            using T1 = std::decay_t<decltype(arg1)>;
            using T2 = std::decay_t<decltype(arg2)>;

            if constexpr (
                (std::is_same_v<T1, int> && std::is_same_v<T2, int>) ||
                (std::is_same_v<T1, float> && std::is_same_v<T2, float>)
            ) return std::max(arg1, arg2);

            return {};
        }, args[0], args[1]);
    }

    LOICollection::frontend::TypedValue sqrt(const LOICollection::frontend::CallbackTypeValues& args) {
        return std::visit([](auto&& arg) -> LOICollection::frontend::TypedValue {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, int> || std::is_same_v<T, float>)
                return static_cast<float>(std::sqrt(arg));

            return {};
        }, args[0]);
    }

    LOICollection::frontend::TypedValue pow(const LOICollection::frontend::CallbackTypeValues& args) {
        return std::visit([](auto&& arg1, auto&& arg2) -> LOICollection::frontend::TypedValue {
            using T1 = std::decay_t<decltype(arg1)>;
            using T2 = std::decay_t<decltype(arg2)>;

            if constexpr ((std::is_same_v<T1, int> && std::is_same_v<T2, int>))
                return static_cast<float>(MathUtils::pow(arg1, arg2));
            else if constexpr ((std::is_same_v<T1, float> && std::is_same_v<T2, float>))
                return std::pow(arg1, arg2);

            return {};
        }, args[0], args[1]);
    }

    LOICollection::frontend::TypedValue log(const LOICollection::frontend::CallbackTypeValues& args) {
        return std::visit([](auto&& arg) -> LOICollection::frontend::TypedValue {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, int> || std::is_same_v<T, float>)
                return static_cast<float>(std::log(arg));

            return {};
        }, args[0]);
    }

    LOICollection::frontend::TypedValue sin(const LOICollection::frontend::CallbackTypeValues& args) {
        return std::visit([](auto&& arg) -> LOICollection::frontend::TypedValue {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, int> || std::is_same_v<T, float>)
                return static_cast<float>(std::sin(arg));

            return {};
        }, args[0]);
    }

    LOICollection::frontend::TypedValue cos(const LOICollection::frontend::CallbackTypeValues& args) {
        return std::visit([](auto&& arg) -> LOICollection::frontend::TypedValue {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, int> || std::is_same_v<T, float>)
                return static_cast<float>(std::cos(arg));

            return {};
        }, args[0]);
    }

    LOICollection::frontend::TypedValue random(const LOICollection::frontend::CallbackTypeValues& args) {
        static std::random_device rd;
        static std::mt19937 gen(rd());

        return std::visit([](auto&& arg1, auto&& arg2) -> LOICollection::frontend::TypedValue {
            using T1 = std::decay_t<decltype(arg1)>;
            using T2 = std::decay_t<decltype(arg2)>;

            if constexpr (std::is_same_v<T1, int> && std::is_same_v<T2, int>) {
                std::uniform_int_distribution<> dis(arg1, arg2);
                return dis(gen);
            } else if constexpr (std::is_same_v<T1, float> && std::is_same_v<T2, float>) {
                std::uniform_real_distribution<> dis(arg1, arg2);
                return static_cast<float>(dis(gen));
            }
                
            return {};
        }, args[0], args[1]);
    }

    void registerFunctions(const std::string& namespaces) {
        FunctionCall& functions = FunctionCall::getInstance();

        functions.registerFunction(namespaces, "abs", abs, { ParamType::INT });
        functions.registerFunction(namespaces, "abs", abs, { ParamType::FLOAT });
        functions.registerFunction(namespaces, "min", min, { ParamType::INT, ParamType::INT });
        functions.registerFunction(namespaces, "min", min, { ParamType::FLOAT, ParamType::FLOAT });
        functions.registerFunction(namespaces, "max", max, { ParamType::INT, ParamType::INT });
        functions.registerFunction(namespaces, "max", max, { ParamType::FLOAT, ParamType::FLOAT });
        functions.registerFunction(namespaces, "sqrt", sqrt, { ParamType::INT });
        functions.registerFunction(namespaces, "sqrt", sqrt, { ParamType::FLOAT });
        functions.registerFunction(namespaces, "pow", pow, { ParamType::INT, ParamType::INT });
        functions.registerFunction(namespaces, "pow", pow, { ParamType::FLOAT, ParamType::FLOAT });
        functions.registerFunction(namespaces, "log", log, { ParamType::INT });
        functions.registerFunction(namespaces, "log", log, { ParamType::FLOAT });
        functions.registerFunction(namespaces, "sin", sin, { ParamType::INT });
        functions.registerFunction(namespaces, "sin", sin, { ParamType::FLOAT });
        functions.registerFunction(namespaces, "cos", cos, { ParamType::INT });
        functions.registerFunction(namespaces, "cos", cos, { ParamType::FLOAT });
        functions.registerFunction(namespaces, "random", random, { ParamType::INT, ParamType::INT });
        functions.registerFunction(namespaces, "random", random, { ParamType::FLOAT, ParamType::FLOAT });
    }
}

REGISTER_CALLBACK(math, MathBuiltin::registerFunctions)
