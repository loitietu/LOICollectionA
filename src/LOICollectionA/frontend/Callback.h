#pragma once

#include <any>
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

#include "LOICollectionA/base/Macro.h"

namespace LOICollection::frontend {
    using CallbackTypeArgs = std::vector<std::string>;
    using CallbackTypePlaces = std::unordered_map<int, std::any>;
    using CallbackFunc = std::function<std::string(const CallbackTypeArgs&)>;
    using CallbackFuncCombination = std::function<std::string(const CallbackTypeArgs&, const CallbackTypePlaces&)>;

    class FunctionCall {
    public:
        LOICOLLECTION_A_NDAPI static FunctionCall& getInstance();

        LOICOLLECTION_A_API   void registerFunction(const std::string& namespaces, const std::string& function, CallbackFunc callback, size_t argsCount);
        LOICOLLECTION_A_API   void registerFunction(const std::string& namespaces, const std::string& function, CallbackFuncCombination callback, size_t argsCount);
        
        LOICOLLECTION_A_NDAPI bool isRegistered(const std::string& namespaces, const std::string& function) const;

        LOICOLLECTION_A_NDAPI std::string callFunction(const std::string& namespaces, const std::string& function, const CallbackTypeArgs& args, const CallbackTypePlaces& placeholders = {});

    public:
        LOICOLLECTION_A_NDAPI bool isInteger(const std::string& str) const;
        LOICOLLECTION_A_NDAPI bool isFloat(const std::string& str) const;
        LOICOLLECTION_A_NDAPI bool isString(const std::string& str) const;

    private:
        FunctionCall();
        ~FunctionCall();

        struct Impl;
        std::unique_ptr<Impl> mImpl;
    };

    class MacroCall {
    public:
        LOICOLLECTION_A_NDAPI static MacroCall& getInstance();

        LOICOLLECTION_A_API   void registerMacro(const std::string& name, CallbackFunc callback);
        LOICOLLECTION_A_API   void registerMacro(const std::string& name, CallbackFuncCombination callback);

        LOICOLLECTION_A_NDAPI bool isRegistered(const std::string& name) const;

        LOICOLLECTION_A_NDAPI std::string callMacro(const std::string& name, const CallbackTypeArgs& args, const CallbackTypePlaces& placeholders = {});

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
