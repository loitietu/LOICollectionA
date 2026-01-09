#include <string>
#include <memory>
#include <vector>
#include <charconv>
#include <stdexcept>
#include <unordered_map>

#include "LOICollectionA/frontend/FunctionCall.h"

namespace LOICollection::frontend {
    struct FunctionCall::Impl {
        std::unordered_map<std::string, std::unordered_map<std::string, CallBackFunc>> mFunctions;
        std::unordered_map<std::string, std::unordered_map<std::string, size_t>> mFunctionArgs;
    };

    FunctionCall::FunctionCall() : mImpl(std::make_unique<Impl>()) {}
    FunctionCall::~FunctionCall() = default;

    FunctionCall& FunctionCall::getInstance() {
        static FunctionCall instance;
        return instance;
    }

    void FunctionCall::registerFunction(const std::string& namespaces, const std::string& function, CallBackFunc callback, size_t argsCount) {
        if (this->mImpl->mFunctions.find(namespaces) == this->mImpl->mFunctions.end()) {
            this->mImpl->mFunctions[namespaces] = std::unordered_map<std::string, CallBackFunc>();
            this->mImpl->mFunctionArgs[namespaces] = std::unordered_map<std::string, size_t>();
        }

        if (this->mImpl->mFunctions[namespaces].find(function) != this->mImpl->mFunctions[namespaces].end())
            throw std::runtime_error("Function already registered");

        this->mImpl->mFunctions[namespaces][function] = callback;
        this->mImpl->mFunctionArgs[namespaces][function] = argsCount;
    }

    std::string FunctionCall::callFunction(const std::string& namespaces, const std::string& function, std::vector<std::string>& args)  {
        if (this->mImpl->mFunctions.find(namespaces) == this->mImpl->mFunctions.end())
            throw std::runtime_error("Namespace not registered");

        if (this->mImpl->mFunctions[namespaces].find(function) == this->mImpl->mFunctions[namespaces].end())
            throw std::runtime_error("Function not registered");

        size_t expectedArgs = this->mImpl->mFunctionArgs[namespaces][function];
        if (args.size() != expectedArgs)
            throw std::runtime_error("Invalid number of arguments");

        return this->mImpl->mFunctions[namespaces][function](args);
    }

    bool FunctionCall::isInteger(const std::string& str) const {
        if (str.empty())
            return false;

        for (char c : str) {
            if (c == '.' || c == 'e' || c == 'E')
                return false;
        }

        int result;
        auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), result);

        return ec == std::errc() && ptr == str.data() + str.size();
    }

    bool FunctionCall::isFloat(const std::string& str) const {
        if (str.empty())
            return false;
        
        for (char c : str) {
            if (c == 'e' || c == 'E')
                return false;
        }

        double result;
        auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), result);

        return ec == std::errc() && ptr == str.data() + str.size();
    }

    bool FunctionCall::isString(const std::string& str) const {
        if (str.empty())
            return false;

        return !isInteger(str) && !isFloat(str);
    }
}
