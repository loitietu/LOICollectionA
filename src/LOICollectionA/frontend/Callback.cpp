#include <string>
#include <memory>
#include <charconv>
#include <stdexcept>
#include <unordered_map>

#include "LOICollectionA/frontend/Callback.h"

namespace LOICollection::frontend {
    struct FunctionCall::Impl {
        std::unordered_map<std::string, std::unordered_map<std::string, CallbackFunc>> mFunctions;
        std::unordered_map<std::string, std::unordered_map<std::string, CallbackFuncCombination>> mFunctionCombinations;
        std::unordered_map<std::string, std::unordered_map<std::string, size_t>> mFunctionArgs;
    };

    FunctionCall::FunctionCall() : mImpl(std::make_unique<Impl>()) {}
    FunctionCall::~FunctionCall() = default;

    FunctionCall& FunctionCall::getInstance() {
        static FunctionCall instance;
        return instance;
    }

    void FunctionCall::registerFunction(const std::string& namespaces, const std::string& function, CallbackFunc callback, size_t argsCount) {
        if (this->isRegistered(namespaces, function))
            throw std::runtime_error("Function already registered");

        this->mImpl->mFunctions[namespaces][function] = callback;
        this->mImpl->mFunctionArgs[namespaces][function] = argsCount;
    }

    void FunctionCall::registerFunction(const std::string& namespaces, const std::string& function, CallbackFuncCombination callback, size_t argsCount) {
        if (this->isRegistered(namespaces, function))
            throw std::runtime_error("Function already registered");

        this->mImpl->mFunctionCombinations[namespaces][function] = callback;
        this->mImpl->mFunctionArgs[namespaces][function] = argsCount;
    }

    bool FunctionCall::isRegistered(const std::string& namespaces, const std::string& function) const {
        return this->mImpl->mFunctions[namespaces].find(function) != this->mImpl->mFunctions[namespaces].end() ||
            this->mImpl->mFunctionCombinations[namespaces].find(function) != this->mImpl->mFunctionCombinations[namespaces].end();
    }

    std::string FunctionCall::callFunction(const std::string& namespaces, const std::string& function, const CallbackTypeArgs& args, const CallbackTypePlaces& placeholders)  {
        if (!this->isRegistered(namespaces, function))
            throw std::runtime_error("Function not registered");

        size_t expectedArgs = this->mImpl->mFunctionArgs[namespaces][function];
        if (args.size() != expectedArgs)
            throw std::runtime_error("Invalid number of arguments");

        if (this->mImpl->mFunctions[namespaces].find(function) != this->mImpl->mFunctions[namespaces].end())
            return this->mImpl->mFunctions[namespaces][function](args);

        return this->mImpl->mFunctionCombinations[namespaces][function](args, placeholders);
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
    
    struct MacroCall::Impl {
        std::unordered_map<std::string, CallbackFunc> mMacros;
        std::unordered_map<std::string, CallbackFuncCombination> mMacroCombinations;
    };

    MacroCall::MacroCall() : mImpl(std::make_unique<Impl>()) {}
    MacroCall::~MacroCall() = default;

    MacroCall& MacroCall::getInstance() {
        static MacroCall instance;
        return instance;
    }

    void MacroCall::registerMacro(const std::string& name, CallbackFunc callback) {
        if (this->isRegistered(name))
            throw std::runtime_error("Macro already registered");

        this->mImpl->mMacros[name] = callback;
    }

    void MacroCall::registerMacro(const std::string& name, CallbackFuncCombination callback) {
        if (this->isRegistered(name))
            throw std::runtime_error("Macro already registered");

        this->mImpl->mMacroCombinations[name] = callback;
    }

    bool MacroCall::isRegistered(const std::string& name) const {
        return this->mImpl->mMacros.find(name) != this->mImpl->mMacros.end() ||
            this->mImpl->mMacroCombinations.find(name) != this->mImpl->mMacroCombinations.end();
    }

    std::string MacroCall::callMacro(const std::string& name, const CallbackTypeArgs& args, const CallbackTypePlaces& placeholders) {
        if (!this->isRegistered(name))
            throw std::runtime_error("Macro not registered");

        if (this->mImpl->mMacros.find(name) != this->mImpl->mMacros.end())
            return this->mImpl->mMacros[name](args);

        return this->mImpl->mMacroCombinations[name](args, placeholders);
    }
}
