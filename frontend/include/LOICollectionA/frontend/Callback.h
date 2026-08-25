#pragma once

#include <any>
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

#include <expected>
#include <system_error>

#include "LOICollectionA/frontend/AST.h"
#include "LOICollectionA/frontend/DiagnosticEngine.h"

#include "LOICollectionA/base/Macro.h"

namespace LOICollection::frontend {
    /* Standalone error value that replaces LeviLamina's ll::Error, so the
     * frontend can be built and tested without the LeviLamina SDK. */
    struct LCUIError {
        std::string text;
        [[nodiscard]] const std::string& message() const noexcept { return text; }
    };

    template <typename T>
    using Expected = std::expected<T, LCUIError>;

    [[nodiscard]] inline std::unexpected<LCUIError> makeStringError(std::string message) {
        return std::unexpected(LCUIError{ std::move(message) });
    }

    [[nodiscard]] inline std::unexpected<LCUIError> makeErrorCodeError(std::error_code code) {
        return std::unexpected(LCUIError{ code.message() });
    }

    enum class ParamType {
        INT,
        FLOAT,
        STRING,
        BOOL,
        OBJECT,
        FUNCTION,
        ARRAY
    };

    using TypedValue = ValueNode::ValueType;

    using CallbackTypeArgs = std::vector<ParamType>;
    using CallbackTypeValues = std::vector<TypedValue>;
    using CallbackTypePlaces = std::unordered_map<int, std::any>;

    using CallbackFunc = std::function<Expected<TypedValue>(const CallbackTypeValues&)>;
    using CallbackFuncCombination = std::function<Expected<TypedValue>(const CallbackTypeValues&, const CallbackTypePlaces&)>;

    using NativeConstructor = std::function<Expected<ObjectRef>(const CallbackTypeValues&)>;
    using NativeConstructorCombination = std::function<Expected<ObjectRef>(const CallbackTypeValues&, const CallbackTypePlaces&)>;
    using NativeMethod = std::function<Expected<TypedValue>(const ObjectRef&, const CallbackTypeValues&)>;
    using NativeMethodCombination = std::function<Expected<TypedValue>(const ObjectRef&, const CallbackTypeValues&, const CallbackTypePlaces&)>;
    using NativeStaticMethod = std::function<Expected<TypedValue>(const CallbackTypeValues&)>;
    using NativeStaticMethodCombination = std::function<Expected<TypedValue>(const CallbackTypeValues&, const CallbackTypePlaces&)>;
    using NativeValueMethod = std::function<Expected<TypedValue>(const TypedValue&, const CallbackTypeValues&)>;
    using NativeOperator = std::function<Expected<TypedValue>(const TypedValue&, const TypedValue&)>;

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

    std::vector<ParamType> valuesToTypes(const CallbackTypeValues& values, DiagnosticEngine& diagnostics, const SourceLocation& loc = {});

    class FunctionCall {
    public:
        LOICOLLECTION_A_NDAPI static FunctionCall& getInstance();

        LOICOLLECTION_A_API   void registerFunction(const std::string& namespaces, const std::string& function, CallbackFunc callback, const CallbackTypeArgs& args);
        LOICOLLECTION_A_API   void registerFunction(const std::string& namespaces, const std::string& function, CallbackFuncCombination callback, const CallbackTypeArgs& args);
        LOICOLLECTION_A_API   void unregisterFunction(const std::string& namespaces, const std::string& function, const CallbackTypeArgs& args, bool isCombination);

        LOICOLLECTION_A_NDAPI bool isRegistered(const std::string& namespaces, const std::string& function, const CallbackTypeArgs& args) const;

        LOICOLLECTION_A_NDAPI Expected<TypedValue> callFunction(const std::string& namespaces, const std::string& function, const CallbackTypeValues& args, const CallbackTypePlaces& placeholders, DiagnosticEngine& diagnostics, const SourceLocation& loc = {});

    private:
        FunctionCall();
        ~FunctionCall();

        struct Impl;
        std::unique_ptr<Impl> mImpl;
    };

    class MacroCall {
    public:
        LOICOLLECTION_A_NDAPI static MacroCall& getInstance();

        LOICOLLECTION_A_API   void registerMacro(const std::string& name, CallbackFunc callback, const CallbackTypeArgs& args);
        LOICOLLECTION_A_API   void registerMacro(const std::string& name, CallbackFuncCombination callback, const CallbackTypeArgs& args);
        LOICOLLECTION_A_API   void unregisterMacro(const std::string& name, const CallbackTypeArgs& args, bool isCombination);

        LOICOLLECTION_A_NDAPI bool isRegistered(const std::string& name, const CallbackTypeArgs& args) const;

        LOICOLLECTION_A_NDAPI Expected<TypedValue> callMacro(const std::string& name, const CallbackTypeValues& args, const CallbackTypePlaces& placeholders, DiagnosticEngine& diagnostics, const SourceLocation& loc = {});

    private:
        MacroCall();
        ~MacroCall();

        struct Impl;
        std::unique_ptr<Impl> mImpl;
    };

    class ClassCall {
    public:
        LOICOLLECTION_A_NDAPI static ClassCall& getInstance();

        LOICOLLECTION_A_API   void registerClass(const std::string& name, const std::vector<std::string>& fields);
        LOICOLLECTION_A_API   void registerConstructor(const std::string& name, NativeConstructor callback, const CallbackTypeArgs& args);
        LOICOLLECTION_A_API   void registerConstructor(const std::string& name, NativeConstructorCombination callback, const CallbackTypeArgs& args);
        LOICOLLECTION_A_API   void registerMethod(const std::string& className, const std::string& method, NativeMethod callback, const CallbackTypeArgs& args);
        LOICOLLECTION_A_API   void registerMethod(const std::string& className, const std::string& method, NativeMethodCombination callback, const CallbackTypeArgs& args);
        LOICOLLECTION_A_API   void registerStaticMethod(const std::string& className, const std::string& method, NativeStaticMethod callback, const CallbackTypeArgs& args);
        LOICOLLECTION_A_API   void registerStaticMethod(const std::string& className, const std::string& method, NativeStaticMethodCombination callback, const CallbackTypeArgs& args);
        LOICOLLECTION_A_API   void registerField(const std::string& className, const std::string& field);
        LOICOLLECTION_A_API   void registerField(const std::string& className, const std::string& field, const TypedValue& defaultValue);
        LOICOLLECTION_A_API   void registerStaticField(const std::string& className, const std::string& field);
        LOICOLLECTION_A_API   void registerStaticField(const std::string& className, const std::string& field, const TypedValue& defaultValue);
        LOICOLLECTION_A_API   void registerValueMethod(const std::string& className, const std::string& method, NativeValueMethod callback, const CallbackTypeArgs& args);
        LOICOLLECTION_A_API   void registerOperator(const std::string& className, const std::string& op, NativeOperator callback);

        LOICOLLECTION_A_NDAPI bool isRegistered(const std::string& name) const;
        LOICOLLECTION_A_NDAPI bool hasField(const std::string& name, const std::string& field) const;
        LOICOLLECTION_A_NDAPI bool hasStaticField(const std::string& name, const std::string& field) const;
        LOICOLLECTION_A_NDAPI bool hasOperator(const std::string& className, const std::string& op) const;

        LOICOLLECTION_A_NDAPI std::vector<std::string> getFields(const std::string& name) const;
        LOICOLLECTION_A_NDAPI std::vector<std::string> getStaticFields(const std::string& name) const;
        LOICOLLECTION_A_NDAPI std::vector<CallbackTypeArgs> getConstructorSignatures(const std::string& name) const;
        LOICOLLECTION_A_NDAPI std::vector<CallbackTypeArgs> getMethodSignatures(const std::string& className, const std::string& method) const;
        LOICOLLECTION_A_NDAPI std::vector<CallbackTypeArgs> getStaticMethodSignatures(const std::string& className, const std::string& method) const;
        LOICOLLECTION_A_NDAPI std::vector<CallbackTypeArgs> getValueMethodSignatures(const std::string& className, const std::string& method) const;

        LOICOLLECTION_A_NDAPI Expected<TypedValue> getStaticField(const std::string& className, const std::string& field) const;

        LOICOLLECTION_A_API   void setStaticField(const std::string& className, const std::string& field, const TypedValue& value);

        LOICOLLECTION_A_NDAPI Expected<ObjectRef> create(
            const std::string& name, const CallbackTypeValues& args, const CallbackTypePlaces& placeholders, DiagnosticEngine& diagnostics, const SourceLocation& loc = {}
        );
        LOICOLLECTION_A_NDAPI Expected<TypedValue> callMethod(
            const std::string& className, const std::string& method, const CallbackTypeValues& args,
            const ObjectRef& object, const CallbackTypePlaces& placeholders, DiagnosticEngine& diagnostics, const SourceLocation& loc = {}
        );
        LOICOLLECTION_A_NDAPI Expected<TypedValue> callStaticMethod(
            const std::string& className, const std::string& method, const CallbackTypeValues& args,
            const CallbackTypePlaces& placeholders, DiagnosticEngine& diagnostics, const SourceLocation& loc = {}
        );
        LOICOLLECTION_A_NDAPI Expected<TypedValue> callValueMethod(
            const std::string& className, const std::string& method, const TypedValue& self, const CallbackTypeValues& args,
            DiagnosticEngine& diagnostics, const SourceLocation& loc = {}
        );
        LOICOLLECTION_A_NDAPI Expected<TypedValue> callOperator(
            const std::string& className, const std::string& op, const TypedValue& left, const TypedValue& right,
            DiagnosticEngine& diagnostics, const SourceLocation& loc = {}
        );

    private:
        ClassCall();
        ~ClassCall();

        struct Impl;
        std::unique_ptr<Impl> mImpl;
    };
}

#define REGISTER_CALLBACK(NAME, BINDER)                     \
const auto NAME##_RegisterHelper = []() -> bool {           \
    BINDER(#NAME);                                          \
    return true;                                            \
}();                                                        \
