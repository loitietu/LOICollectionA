#pragma once

#include <string>
#include <memory>
#include <vector>
#include <functional>

namespace LOICollection::frontend {
    class FunctionCall {
    public:
        using CallBackType = std::string(std::vector<std::string>&);
        using CallBackFunc = std::function<CallBackType>;

        static FunctionCall& getInstance();

        void registerFunction(const std::string& namespaces, const std::string& function, CallBackFunc callback, size_t argsCount);
        
        std::string callFunction(const std::string& namespaces, const std::string& function, std::vector<std::string>& args);

    public:
        [[nodiscard]] bool isInterger(const std::string& str) const;
        [[nodiscard]] bool isFloat(const std::string& str) const;
        [[nodiscard]] bool isString(const std::string& str) const;

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
