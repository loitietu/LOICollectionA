#pragma once

#include <any>
#include <memory>
#include <string>
#include <vector>
#include <variant>
#include <functional>
#include <unordered_map>

#include "LOICollectionA/base/Macro.h"

namespace LOICollection::frontend {
    enum class ParamType {
        INT,
        FLOAT,
        STRING,
        BOOL
    };

    using TypedValue = std::variant<int, float, std::string, bool>;

    using CallbackTypeArgs = std::vector<ParamType>;
    using CallbackTypeValues = std::vector<TypedValue>;
    using CallbackTypePlaces = std::unordered_map<int, std::any>;

    using CallbackFunc = std::function<std::string(const CallbackTypeValues&)>;
    using CallbackFuncCombination = std::function<std::string(const CallbackTypeValues&, const CallbackTypePlaces&)>;

    struct Signature {
        std::string name;
        size_t argsCount;
        CallbackTypeArgs args;
        bool isCombination;

        bool operator==(const Signature& other) const {
            return name == other.name && argsCount == other.argsCount && args == other.args && isCombination == other.isCombination;
        }
    };

    struct SignatureHasher {
        std::size_t operator()(const Signature& sig) const {
            std::size_t h = std::hash<std::string>()(sig.name);
            h ^= std::hash<size_t>()(sig.argsCount) + 0x9e3779b9 + (h << 6) + (h >> 2);
            for (const auto& arg : sig.args)
                h ^= std::hash<int>{}(static_cast<int>(arg)) + 0x9e3779b9 + (h << 6) + (h >> 2);

            h ^= std::hash<bool>()(sig.isCombination) + 0x9e3779b9 + (h << 6) + (h >> 2);
            return h;
        }
    };

    std::vector<ParamType> valuesToTypes(const CallbackTypeValues& values);

    class FunctionCall {
    public:
        LOICOLLECTION_A_NDAPI static FunctionCall& getInstance();

        LOICOLLECTION_A_API   void registerFunction(const std::string& namespaces, const std::string& function, CallbackFunc callback, CallbackTypeArgs args);
        LOICOLLECTION_A_API   void registerFunction(const std::string& namespaces, const std::string& function, CallbackFuncCombination callback, CallbackTypeArgs args);
        
        LOICOLLECTION_A_NDAPI bool isRegistered(const std::string& namespaces, const std::string& function, CallbackTypeArgs args) const;

        LOICOLLECTION_A_NDAPI std::string callFunction(const std::string& namespaces, const std::string& function, const CallbackTypeValues& args, const CallbackTypePlaces& placeholders = {});

    private:
        FunctionCall();
        ~FunctionCall();

        struct Impl;
        std::unique_ptr<Impl> mImpl;
    };

    class MacroCall {
    public:
        LOICOLLECTION_A_NDAPI static MacroCall& getInstance();

        LOICOLLECTION_A_API   void registerMacro(const std::string& name, CallbackFunc callback, CallbackTypeArgs args);
        LOICOLLECTION_A_API   void registerMacro(const std::string& name, CallbackFuncCombination callback, CallbackTypeArgs args);

        LOICOLLECTION_A_NDAPI bool isRegistered(const std::string& name, CallbackTypeArgs args) const;

        LOICOLLECTION_A_NDAPI std::string callMacro(const std::string& name, const CallbackTypeValues& args, const CallbackTypePlaces& placeholders = {});

    private:
        MacroCall();
        ~MacroCall();

        struct Impl;
        std::unique_ptr<Impl> mImpl;
    };
}

#define REGISTER_NAMESPACE(NAMESPACE, BINDER)   \
const auto RegisterHelper = []() -> bool {      \
    BINDER(NAMESPACE);                          \
    return true;                                \
}();                                            \
