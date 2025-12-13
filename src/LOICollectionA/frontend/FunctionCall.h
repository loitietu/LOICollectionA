#pragma once

#include <string>
#include <memory>
#include <vector>
#include <functional>

#include "LOICollectionA/base/Macro.h"

namespace LOICollection::frontend {
    class FunctionCall {
    public:
        using CallBackType = std::string(std::vector<std::string>&);
        using CallBackFunc = std::function<CallBackType>;

    public:
        LOICOLLECTION_A_NDAPI static FunctionCall& getInstance();

        LOICOLLECTION_A_API   void registerFunction(const std::string& namespaces, const std::string& function, CallBackFunc callback, size_t argsCount);
        
        LOICOLLECTION_A_NDAPI std::string callFunction(const std::string& namespaces, const std::string& function, std::vector<std::string>& args);

    public:
        LOICOLLECTION_A_NDAPI bool isInterger(const std::string& str) const;
        LOICOLLECTION_A_NDAPI bool isFloat(const std::string& str) const;
        LOICOLLECTION_A_NDAPI bool isString(const std::string& str) const;

    private:
        FunctionCall();
        ~FunctionCall();

        struct Impl;
        std::unique_ptr<Impl> mImpl;
    };
}

#define REGISTER_NAMESPACE(NAMESPACE, BINDER)   \
const auto RegisterHelper = []() -> bool {      \
    BINDER(NAMESPACE);                          \
    return true;                                \
}();                                            \
