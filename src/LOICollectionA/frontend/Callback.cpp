#include <string>
#include <memory>
#include <vector>
#include <stdexcept>
#include <unordered_map>

#include "LOICollectionA/frontend/Callback.h"

namespace LOICollection::frontend {
    std::vector<ParamType> valuesToTypes(const CallbackTypeValues& values) {
        std::vector<ParamType> argTypes;
        for (const auto& arg : values) {
            std::visit([&argTypes](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;

                if constexpr (std::is_same_v<T, int>)
                    argTypes.push_back(ParamType::INT);
                else if constexpr (std::is_same_v<T, float>)               
                    argTypes.push_back(ParamType::FLOAT);
                else if constexpr (std::is_same_v<T, std::string>)
                    argTypes.push_back(ParamType::STRING);
                else if constexpr (std::is_same_v<T, bool>)
                    argTypes.push_back(ParamType::BOOL);
                else
                    throw std::runtime_error("Unsupported argument type");
            }, arg);
        }

        return argTypes;
    }

    struct FunctionCall::Impl {
        std::unordered_map<std::string, std::unordered_map<Signature, CallbackFunc, SignatureHasher>> mFunctions;
        std::unordered_map<std::string, std::unordered_map<Signature, CallbackFuncCombination, SignatureHasher>> mFunctionCombinations;
    };

    FunctionCall::FunctionCall() : mImpl(std::make_unique<Impl>()) {}
    FunctionCall::~FunctionCall() = default;

    FunctionCall& FunctionCall::getInstance() {
        static FunctionCall instance;
        return instance;
    }

    void FunctionCall::registerFunction(const std::string& namespaces, const std::string& function, CallbackFunc callback, CallbackTypeArgs args) {
        if (this->isRegistered(namespaces, function, args))
            throw std::runtime_error("Function already registered");

        Signature sig{ function, args.size(), std::move(args), false };

        this->mImpl->mFunctions[namespaces][sig] = std::move(callback);
    }

    void FunctionCall::registerFunction(const std::string& namespaces, const std::string& function, CallbackFuncCombination callback, CallbackTypeArgs args) {
        if (this->isRegistered(namespaces, function, args))
            throw std::runtime_error("Function already registered");

        Signature sig{ function, args.size(), std::move(args), true };

        this->mImpl->mFunctionCombinations[namespaces][sig] = std::move(callback);
    }

    bool FunctionCall::isRegistered(const std::string& namespaces, const std::string& function, CallbackTypeArgs args) const {
        Signature sig{ function, args.size(), std::move(args), false };
        bool result = this->mImpl->mFunctions[namespaces].find(sig) != this->mImpl->mFunctions[namespaces].end();

        sig.isCombination = true;
        return result || this->mImpl->mFunctionCombinations[namespaces].find(sig) != this->mImpl->mFunctionCombinations[namespaces].end();
    }

    std::string FunctionCall::callFunction(const std::string& namespaces, const std::string& function, const CallbackTypeValues& args, const CallbackTypePlaces& placeholders)  {
        std::vector<ParamType> argTypes = valuesToTypes(args);

        if (!this->isRegistered(namespaces, function, argTypes))
            throw std::runtime_error("Function not registered");

        Signature sig{ function, argTypes.size(), std::move(argTypes), false };
        if (this->mImpl->mFunctions[namespaces].find(sig) != this->mImpl->mFunctions[namespaces].end())
            return this->mImpl->mFunctions[namespaces][sig](args);

        sig.isCombination = true;
        return this->mImpl->mFunctionCombinations[namespaces][sig](args, placeholders);
    }
    
    struct MacroCall::Impl {
        std::unordered_map<Signature, CallbackFunc, SignatureHasher> mMacros;
        std::unordered_map<Signature, CallbackFuncCombination, SignatureHasher> mMacroCombinations;
    };

    MacroCall::MacroCall() : mImpl(std::make_unique<Impl>()) {}
    MacroCall::~MacroCall() = default;

    MacroCall& MacroCall::getInstance() {
        static MacroCall instance;
        return instance;
    }

    void MacroCall::registerMacro(const std::string& name, CallbackFunc callback, CallbackTypeArgs args) {
        if (this->isRegistered(name, args))
            throw std::runtime_error("Macro already registered");

        Signature sig{ name, args.size(), std::move(args), false };

        this->mImpl->mMacros[sig] = callback;
    }

    void MacroCall::registerMacro(const std::string& name, CallbackFuncCombination callback, CallbackTypeArgs args) {
        if (this->isRegistered(name, args))
            throw std::runtime_error("Macro already registered");

        Signature sig{ name, args.size(), std::move(args), true };

        this->mImpl->mMacroCombinations[sig] = callback;
    }

    bool MacroCall::isRegistered(const std::string& name, CallbackTypeArgs args) const {
        Signature sig{ name, args.size(), std::move(args), false };
        bool result = this->mImpl->mMacros.find(sig) != this->mImpl->mMacros.end();

        sig.isCombination = true;
        return result || this->mImpl->mMacroCombinations.find(sig) != this->mImpl->mMacroCombinations.end();
    }

    std::string MacroCall::callMacro(const std::string& name, const CallbackTypeValues& args, const CallbackTypePlaces& placeholders) {
        std::vector<ParamType> argTypes = valuesToTypes(args);

        if (!this->isRegistered(name, argTypes))
            throw std::runtime_error("Macro not registered");
        
        Signature sig{ name, argTypes.size(), std::move(argTypes), false };
        if (this->mImpl->mMacros.find(sig) != this->mImpl->mMacros.end())
            return this->mImpl->mMacros[sig](args);

        sig.isCombination = true;
        return this->mImpl->mMacroCombinations[sig](args, placeholders);
    }
}
