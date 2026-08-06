#pragma once

#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <unordered_map>

#include "LOICollectionA/frontend/AST.h"
#include "LOICollectionA/frontend/DiagnosticEngine.h"

#include "LOICollectionA/base/Macro.h"

namespace LOICollection::frontend {
    class SemanticAnalyzer {
    public:
        LOICOLLECTION_A_API SemanticAnalyzer(DiagnosticEngine& diag);

        LOICOLLECTION_A_API void analyze(TemplateNode& root);

    private:
        struct MethodScope {
            std::optional<std::reference_wrapper<ClassNode>> cls;
            std::optional<std::reference_wrapper<MethodDecl>> method;

            [[nodiscard]] bool hasClass() const { return cls.has_value(); }
            [[nodiscard]] bool hasMethod() const { return method.has_value(); }
            [[nodiscard]] ClassNode& classRef() const { return cls->get(); }
            [[nodiscard]] MethodDecl& methodRef() const { return method->get(); }
        };

        DiagnosticEngine& diagnostics;

        std::vector<std::reference_wrapper<ClassNode>> classes;
        std::vector<std::reference_wrapper<ClassNode>> orderedClasses;
        std::unordered_map<std::string, std::reference_wrapper<ClassNode>> classByName;
        std::unordered_map<std::string, std::vector<std::string>> classMethodOrder;
        std::unordered_map<std::string, std::unordered_map<std::string, int>> classMethodOrdinals;
        std::vector<std::reference_wrapper<FunctionDefNode>> functions;
        std::unordered_map<std::string, std::vector<std::reference_wrapper<FunctionDefNode>>> functionsByName;
        std::unordered_map<std::string, TypeInfo> globalTypes;
        std::vector<std::reference_wrapper<NewNode>> pendingNewSites;

        [[nodiscard]] std::optional<std::reference_wrapper<ClassNode>> findClass(const std::string& name) const;

        [[nodiscard]] TypeInfo typeOfValue(const ValueNode::ValueType& value) const;
        [[nodiscard]] TypeInfo typeFromName(const std::string& name, SourceLocation loc, bool reportError) const;
        [[nodiscard]] std::string typeToString(const TypeInfo& type) const;
        [[nodiscard]] bool isNumeric(const TypeInfo& type) const;
        [[nodiscard]] bool isNameDefined(const std::string& name, MethodScope& scope) const;

        void registerClass(ClassNode& node);
        void resolveHierarchy();
        void buildMethodOrdinals();
        void validateConstructors();
        void registerFunction(FunctionDefNode& node);
        void checkTopLevel(TemplateNode& root);
        void checkClassBodies();
        void checkFunctionBodies();
        void checkClassBody(ClassNode& cls);
        void checkBody(std::optional<std::reference_wrapper<ClassNode>> cls, MethodDecl& method);

        void checkStatement(ASTNode& node, MethodScope& scope);
        TypeInfo checkExpr(ExprNode& node, MethodScope& scope);
        TypeInfo checkAssignment(AssignmentNode& node, MethodScope& scope);
        TypeInfo checkMemberAccess(MemberAccessNode& node, MethodScope& scope);
        TypeInfo checkMethodCall(MethodCallNode& node, MethodScope& scope);
        TypeInfo checkFuncCall(FuncCallNode& node, MethodScope& scope);
        TypeInfo checkNew(NewNode& node, MethodScope& scope);
        TypeInfo checkSuperCall(SuperCallNode& node, MethodScope& scope);
        TypeInfo checkInstanceOf(InstanceOfNode& node, MethodScope& scope);
        TypeInfo checkReturn(ReturnNode& node, MethodScope& scope);
        TypeInfo checkLambda(LambdaNode& node, MethodScope& scope);

        TypeInfo lookupName(const std::string& name, MethodScope& scope);
        void unify(TypeInfo& target, const TypeInfo& from, SourceLocation loc, const std::string& what);

        [[nodiscard]] size_t knownParamCount(const MethodDecl& method) const;
        [[nodiscard]] std::string methodSignature(const MethodDecl& method) const;
        [[nodiscard]] int methodOrdinal(const std::string& className, const std::string& signature) const;
        [[nodiscard]] bool isDerived(const std::string& derivedName, const std::string& baseName) const;
        [[nodiscard]] bool isTypeCompatible(const TypeInfo& target, const TypeInfo& from) const;
        [[nodiscard]] ClassMember* findField(ClassNode& cls, const std::string& name, ClassNode*& owner) const;
        [[nodiscard]] MethodDecl* findConstructor(ClassNode& cls, ClassNode*& owner) const;

        void finalizePending();
    };
}
