#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "LOICollectionA/frontend/Callback.h"

#include "LOICollectionA/frontend/SemanticAnalyzer.h"

namespace LOICollection::frontend {
    TypeInfo typeFromParam(ParamType param) {
        switch (param) {
            case ParamType::INT: return { TypeKind::Int };
            case ParamType::FLOAT: return { TypeKind::Float };
            case ParamType::STRING: return { TypeKind::String };
            case ParamType::BOOL: return { TypeKind::Bool };
            case ParamType::OBJECT: return { TypeKind::Object };
            case ParamType::FUNCTION: return { TypeKind::Function };
        }

        return {};
    }

    bool typeMatchesParam(const TypeInfo& type, ParamType param) {
        if (type.kind == TypeKind::Unknown)
            return true;

        TypeInfo expected = typeFromParam(param);
        if (expected.kind == TypeKind::Object)
            return type.kind == TypeKind::Object;

        return type == expected;
    }

    bool matchesNativeSignature(const CallbackTypeArgs& signature, const std::vector<TypeInfo>& argTypes) {
        if (signature.size() != argTypes.size())
            return false;

        for (size_t i = 0; i < argTypes.size(); ++i) {
            if (!typeMatchesParam(argTypes[i], signature[i]))
                return false;
        }

        return true;
    }

    bool isNativeClass(const std::string& name) {
        return ClassCall::getInstance().isRegistered(name);
    }

    SemanticAnalyzer::SemanticAnalyzer(DiagnosticEngine& diag) : diagnostics(diag) {}

    void SemanticAnalyzer::analyze(ProgramNode& root) {
        this->classes.clear();
        this->orderedClasses.clear();
        this->classByName.clear();
        this->classMethodOrder.clear();
        this->classMethodOrdinals.clear();
        this->functions.clear();
        this->functionsByName.clear();
        this->globalTypes.clear();

        for (auto& part : root.parts) {
            switch (part->getType()) {
                case ASTNode::Type::Class:
                    registerClass(static_cast<ClassNode&>(*part));
                    break;
                case ASTNode::Type::FunctionDef:
                    registerFunction(static_cast<FunctionDefNode&>(*part));
                    break;
                default:
                    break;
            }
        }

        resolveHierarchy();
        buildMethodOrdinals();

        checkTopLevel(root);
        checkClassBodies();
        checkFunctionBodies();
        validateConstructors();
    }

    void SemanticAnalyzer::resolveHierarchy() {
        std::unordered_set<std::string> visiting;
        std::unordered_set<std::string> visited;

        std::function<void(ClassNode&)> visit = [&](ClassNode& cls) {
            if (visited.contains(cls.name))
                return;

            if (visiting.contains(cls.name)) {
                diagnostics.addError(cls.loc,
                    "Circular inheritance involving class '" + cls.name + "'");
                return;
            }

            visiting.insert(cls.name);

            if (!cls.baseClassName.empty()) {
                auto baseOpt = this->findClass(cls.baseClassName);
                if (!baseOpt) {
                    diagnostics.addError(cls.loc, "Unknown base class: " + cls.baseClassName);
                } else {
                    visit(baseOpt->get());
                }
            }

            visiting.erase(cls.name);
            visited.insert(cls.name);
            this->orderedClasses.push_back(std::ref(cls));
        };

        for (auto cls : this->classes)
            visit(cls.get());
    }

    void SemanticAnalyzer::buildMethodOrdinals() {
        for (auto clsRef : this->orderedClasses) {
            ClassNode& cls = clsRef.get();

            std::vector<std::string> order;
            std::unordered_map<std::string, int> ordinals;

            if (!cls.baseClassName.empty()) {
                order = this->classMethodOrder[cls.baseClassName];
                ordinals = this->classMethodOrdinals[cls.baseClassName];
            }

            for (auto& method : cls.methods) {
                if (method.isConstructor)
                    continue;

                std::string signature = this->methodSignature(method);
                if (!ordinals.contains(signature)) {
                    ordinals[signature] = static_cast<int>(order.size());
                    order.push_back(signature);
                }
            }

            this->classMethodOrder[cls.name] = std::move(order);
            this->classMethodOrdinals[cls.name] = std::move(ordinals);
        }
    }

    void SemanticAnalyzer::validateConstructors() {
        for (auto clsRef : this->orderedClasses) {
            ClassNode& cls = clsRef.get();
            if (cls.baseClassName.empty())
                continue;

            auto baseOpt = this->findClass(cls.baseClassName);
            if (!baseOpt)
                continue;

            auto baseCtor = this->findConstructor(baseOpt->get());
            if (!baseCtor || baseCtor->method.get().params.empty())
                continue;

            if (cls.constructorIndex == -1) {
                diagnostics.addError(cls.loc,
                    "Class '" + cls.name + "' must define a constructor to call base constructor with arguments");
            } else if (!cls.methods[cls.constructorIndex].hasSuperCall) {
                diagnostics.addError(cls.loc,
                    "Constructor of class '" + cls.name + "' must call super(...)");
            }
        }
    }

    std::optional<std::reference_wrapper<ClassNode>> SemanticAnalyzer::findClass(const std::string& name) const {
        auto it = classByName.find(name);
        if (it == classByName.end())
            return std::nullopt;

        return it->second;
    }

    TypeInfo SemanticAnalyzer::typeOfValue(const ValueNode::ValueType& value) const {
        switch (value.index()) {
            case 0: return { TypeKind::Int };
            case 1: return { TypeKind::Float };
            case 2: return { TypeKind::String };
            case 3: return { TypeKind::Bool };
            case 4: return { TypeKind::Object };
            case 5: return { TypeKind::Function };
            default: return {};
        }
    }

    TypeInfo SemanticAnalyzer::typeFromName(const std::string& name, SourceLocation loc, bool reportError) const {
        if (name == "int") return { TypeKind::Int };
        if (name == "float") return { TypeKind::Float };
        if (name == "string") return { TypeKind::String };
        if (name == "bool") return { TypeKind::Bool };
        if (name == "void") return { TypeKind::Void };

        if (auto cls = findClass(name))
            return { TypeKind::Object, cls->get().name };

        if (isNativeClass(name))
            return { TypeKind::Object, name };

        if (reportError)
            diagnostics.addError(loc, "Unknown type: " + name);

        return {};
    }

    std::string SemanticAnalyzer::typeToString(const TypeInfo& type) const {
        switch (type.kind) {
            case TypeKind::Unknown: return "unknown";
            case TypeKind::Int: return "int";
            case TypeKind::Float: return "float";
            case TypeKind::String: return "string";
            case TypeKind::Bool: return "bool";
            case TypeKind::Object: return "class " + type.className;
            case TypeKind::Function: return "function";
            case TypeKind::Void: return "void";
        }

        return "unknown";
    }

    bool SemanticAnalyzer::isNumeric(const TypeInfo& type) const {
        return type.kind == TypeKind::Int || type.kind == TypeKind::Float;
    }

    void SemanticAnalyzer::registerClass(ClassNode& node) {
        if (classByName.find(node.name) != classByName.end()) {
            diagnostics.addError(node.loc, "Duplicate class: " + node.name);
            return;
        }

        classByName.emplace(node.name, std::ref(node));
        classes.push_back(std::ref(node));

        std::unordered_set<std::string> memberNames;
        for (const auto& member : node.members) {
            if (!memberNames.insert(member.name).second)
                diagnostics.addError(member.loc, "Duplicate member variable: " + member.name);
        }

        for (auto& member : node.members) {
            if (member.hasDefault && member.defaultExpr) {
                MethodScope emptyScope;
                member.type = checkExpr(*member.defaultExpr, emptyScope);
            }
        }

        for (auto& method : node.methods) {
            method.paramTypes.resize(method.params.size());

            for (size_t i = 0; i < method.params.size(); ++i) {
                if (method.params[i].hasType)
                    method.paramTypes[i] = typeFromName(method.params[i].typeName, method.loc, true);
            }

            if (method.hasReturnType)
                method.returnType = typeFromName(method.returnTypeName, method.loc, true);
        }
    }

    void SemanticAnalyzer::registerFunction(FunctionDefNode& node) {
        functions.push_back(std::ref(node));
        functionsByName[node.name].push_back(std::ref(node));

        MethodDecl& decl = node.decl;
        decl.paramTypes.resize(decl.params.size());

        for (size_t i = 0; i < decl.params.size(); ++i) {
            if (decl.params[i].hasType)
                decl.paramTypes[i] = typeFromName(decl.params[i].typeName, decl.loc, true);
        }

        if (decl.hasReturnType)
            decl.returnType = typeFromName(decl.returnTypeName, decl.loc, true);
    }

    void SemanticAnalyzer::checkTopLevel(ProgramNode& root) {
        MethodScope emptyScope;

        for (auto& part : root.parts) {
            switch (part->getType()) {
                case ASTNode::Type::Class:
                case ASTNode::Type::FunctionDef:
                    continue;
                default:
                    checkStatement(*part, emptyScope);
            }
        }
    }

    void SemanticAnalyzer::checkClassBodies() {
        for (auto cls : classes)
            checkClassBody(cls.get());
    }

    void SemanticAnalyzer::checkFunctionBodies() {
        for (auto fn : functions)
            checkBody(std::nullopt, fn.get().decl);
    }

    void SemanticAnalyzer::checkClassBody(ClassNode& cls) {
        for (auto& method : cls.methods)
            checkBody(std::ref(cls), method);
    }

    void SemanticAnalyzer::checkBody(std::optional<std::reference_wrapper<ClassNode>> cls, MethodDecl& method) {
        MethodScope scope{ cls, std::ref(method) };

        if (method.body)
            checkStatement(*method.body, scope);

        if (method.hasReturnType && !method.isConstructor &&
            method.returnType.kind != TypeKind::Void && !method.hasReturnStatement) {
            diagnostics.addWarning(method.loc,
                "Missing return statement in function '" + method.name + "'");
        }
    }

    void SemanticAnalyzer::checkStatement(ASTNode& node, MethodScope& scope) {
        switch (node.getType()) {
            case ASTNode::Type::Class:
            case ASTNode::Type::FunctionDef:
                return;
            case ASTNode::Type::Return:
                checkReturn(static_cast<ReturnNode&>(node), scope);
                return;
            case ASTNode::Type::Block:
                for (auto& part : static_cast<BlockNode&>(node).parts)
                    checkStatement(*part, scope);
                return;
            default:
                checkExpr(static_cast<ExprNode&>(node), scope);
                return;
        }
    }

    TypeInfo SemanticAnalyzer::checkExpr(ExprNode& node, MethodScope& scope) {
        switch (node.getType()) {
            case ASTNode::Type::Assignment:
                return checkAssignment(static_cast<AssignmentNode&>(node), scope);

            case ASTNode::Type::Value:
                return typeOfValue(static_cast<ValueNode&>(node).value);

            case ASTNode::Type::Variable: {
                auto& var = static_cast<VariableNode&>(node);
                TypeInfo type = lookupName(var.name, scope);

                if (type.kind == TypeKind::Unknown && scope.hasClass()) {
                    if (auto field = findField(scope.classRef(), var.name)) {
                        if (field->member.get().isPrivate && &scope.classRef() != &field->owner.get()) {
                            diagnostics.addError(var.loc,
                                "Cannot access private member '" + var.name + "'");
                        }
                    }
                }

                return type;
            }

            case ASTNode::Type::This: {
                auto& self = static_cast<ThisNode&>(node);
                if (!scope.hasMethod() || !scope.hasClass()) {
                    diagnostics.addError(self.loc, "'this' is only available inside class methods");
                    return {};
                }

                return { TypeKind::Object, scope.classRef().name };
            }

            case ASTNode::Type::Super: {
                auto& superNode = static_cast<SuperNode&>(node);
                if (!scope.hasMethod() || !scope.hasClass()) {
                    diagnostics.addError(superNode.loc, "'super' is only available inside class methods");
                    return {};
                }

                ClassNode& cls = scope.classRef();
                if (cls.baseClassName.empty()) {
                    diagnostics.addError(superNode.loc, "Class '" + cls.name + "' has no base class");
                    return {};
                }

                return { TypeKind::Object, cls.baseClassName };
            }

            case ASTNode::Type::SuperCall:
                return checkSuperCall(static_cast<SuperCallNode&>(node), scope);

            case ASTNode::Type::InstanceOf:
                return checkInstanceOf(static_cast<InstanceOfNode&>(node), scope);

            case ASTNode::Type::New:
                return checkNew(static_cast<NewNode&>(node), scope);

            case ASTNode::Type::MemberAccess:
                return checkMemberAccess(static_cast<MemberAccessNode&>(node), scope);

            case ASTNode::Type::MethodCall:
                return checkMethodCall(static_cast<MethodCallNode&>(node), scope);

            case ASTNode::Type::FuncCall:
                return checkFuncCall(static_cast<FuncCallNode&>(node), scope);

            case ASTNode::Type::Lambda:
                return checkLambda(static_cast<LambdaNode&>(node), scope);

            case ASTNode::Type::If: {
                auto& ifNode = static_cast<IfNode&>(node);
                if (ifNode.condition)
                    checkExpr(*ifNode.condition, scope);
                if (ifNode.trueBranch)
                    checkStatement(*ifNode.trueBranch, scope);
                if (ifNode.falseBranch)
                    checkStatement(*ifNode.falseBranch, scope);

                return {};
            }

            case ASTNode::Type::Arithmetic: {
                auto& arith = static_cast<ArithmeticNode&>(node);
                TypeInfo left = checkExpr(*arith.left, scope);
                TypeInfo right = checkExpr(*arith.right, scope);

                if (arith.op == "+" && (left.kind == TypeKind::String || right.kind == TypeKind::String))
                    return { TypeKind::String };

                if (isNumeric(left) && isNumeric(right)) {
                    if ((arith.op == "+" || arith.op == "-" || arith.op == "*") &&
                        left.kind == TypeKind::Int && right.kind == TypeKind::Int)
                        return { TypeKind::Int };

                    if (arith.op == "%" && left.kind == TypeKind::Int && right.kind == TypeKind::Int)
                        return { TypeKind::Int };

                    return { TypeKind::Float };
                }

                return {};
            }

            case ASTNode::Type::Compare: {
                auto& cmp = static_cast<CompareNode&>(node);
                checkExpr(*cmp.left, scope);
                checkExpr(*cmp.right, scope);
                return { TypeKind::Bool };
            }

            case ASTNode::Type::Logical: {
                auto& logical = static_cast<LogicalNode&>(node);
                checkExpr(*logical.left, scope);
                checkExpr(*logical.right, scope);
                return { TypeKind::Bool };
            }

            case ASTNode::Type::Unary: {
                auto& unary = static_cast<UnaryNode&>(node);
                TypeInfo operand = checkExpr(*unary.operand, scope);
                return unary.op == "!" ? TypeInfo{ TypeKind::Bool } : operand;
            }

            case ASTNode::Type::Function:
                for (auto& arg : static_cast<FunctionNode&>(node).args)
                    checkExpr(*arg, scope);
                return {};

            case ASTNode::Type::Macro:
                for (auto& arg : static_cast<MacroNode&>(node).args)
                    checkExpr(*arg, scope);
                return {};

            default:
                return {};
        }
    }

    TypeInfo SemanticAnalyzer::checkAssignment(AssignmentNode& node, MethodScope& scope) {
        TypeInfo rhs = checkExpr(*node.value, scope);

        switch (node.target->getType()) {
            case ASTNode::Type::Variable: {
                auto& var = static_cast<VariableNode&>(*node.target);
                if (scope.hasMethod()) {
                    MethodDecl& method = scope.methodRef();
                    for (size_t i = 0; i < method.params.size(); ++i) {
                        if (method.params[i].name == var.name) {
                            if (method.params[i].hasType)
                                unify(method.paramTypes[i], rhs, node.loc, "parameter '" + var.name + "'");
                            return rhs;
                        }
                    }

                    if (scope.hasClass()) {
                        if (auto field = findField(scope.classRef(), var.name)) {
                            ClassMember& member = field->member.get();
                            if (member.isPrivate && &scope.classRef() != &field->owner.get()) {
                                diagnostics.addError(node.loc,
                                    "Cannot access private member '" + member.name + "'");
                            }

                            if (member.hasDefault)
                                unify(member.type, rhs, node.loc, "member '" + var.name + "'");
                            return rhs;
                        }
                    }
                }

                // 全局变量采用动态类型，直接更新
                if (rhs.kind != TypeKind::Unknown)
                    globalTypes[var.name] = rhs;
                else
                    globalTypes.try_emplace(var.name, rhs);

                return rhs;
            }

            case ASTNode::Type::MemberAccess: {
                auto& member = static_cast<MemberAccessNode&>(*node.target);
                TypeInfo targetType = checkExpr(*member.target, scope);

                if (targetType.kind == TypeKind::Unknown)
                    return rhs;

                if (targetType.kind != TypeKind::Object) {
                    diagnostics.addError(node.loc, "Cannot access member of a non-object value");
                    return rhs;
                }

                auto clsOpt = findClass(targetType.className);
                if (!clsOpt) {
                    if (isNativeClass(targetType.className)) {
                        if (ClassCall::getInstance().hasField(targetType.className, member.memberName)) {
                            return rhs;
                        }

                        diagnostics.addError(node.loc,
                            "Class '" + targetType.className + "' has no member '" + member.memberName + "'");
                        return rhs;
                    }

                    diagnostics.addError(node.loc, "Unknown class: " + targetType.className);
                    return rhs;
                }

                ClassNode& cls = clsOpt->get();
                if (auto field = findField(cls, member.memberName)) {
                    ClassMember& m = field->member.get();
                    if (m.isPrivate && !(scope.hasMethod() && &scope.classRef() == &field->owner.get())) {
                        diagnostics.addError(node.loc, "Cannot access private member '" + m.name + "'");
                    }

                    if (m.hasDefault)
                        unify(m.type, rhs, node.loc, "member '" + m.name + "'");
                    return rhs;
                }

                diagnostics.addError(node.loc,
                    "Class '" + cls.name + "' has no member '" + member.memberName + "'");
                return rhs;
            }

            default:
                diagnostics.addError(node.loc, "Invalid assignment target");
                return rhs;
        }
    }

    TypeInfo SemanticAnalyzer::checkMemberAccess(MemberAccessNode& node, MethodScope& scope) {
        TypeInfo targetType = checkExpr(*node.target, scope);

        if (targetType.kind == TypeKind::Unknown)
            return {};

        if (targetType.kind != TypeKind::Object) {
            diagnostics.addError(node.loc, "Cannot access member of a non-object value");
            return {};
        }

        auto clsOpt = findClass(targetType.className);
        if (!clsOpt) {
            if (isNativeClass(targetType.className)) {
                if (!ClassCall::getInstance().hasField(targetType.className, node.memberName)) {
                    diagnostics.addError(node.loc,
                        "Class '" + targetType.className + "' has no member '" + node.memberName + "'");
                }

                return {};
            }

            diagnostics.addError(node.loc, "Unknown class: " + targetType.className);
            return {};
        }

        ClassNode& cls = clsOpt->get();
        if (auto field = findField(cls, node.memberName)) {
            ClassMember& m = field->member.get();
            if (m.isPrivate && !(scope.hasMethod() && &scope.classRef() == &field->owner.get())) {
                diagnostics.addError(node.loc,
                    "Cannot access private member '" + m.name + "' of class '" + field->owner.get().name + "'");
            }

            return m.type;
        }

        diagnostics.addError(node.loc,
            "Class '" + cls.name + "' has no member '" + node.memberName + "'");
        return {};
    }

    TypeInfo SemanticAnalyzer::checkMethodCall(MethodCallNode& node, MethodScope& scope) {
        TypeInfo targetType = checkExpr(*node.target, scope);

        if (targetType.kind != TypeKind::Object) {
            diagnostics.addError(node.loc, "Method call target is not an object");
            return {};
        }

        size_t argCount = node.args.size();

        std::vector<TypeInfo> argTypes;
        argTypes.reserve(argCount);
        for (auto& arg : node.args)
            argTypes.push_back(checkExpr(*arg, scope));

        auto clsOpt = findClass(targetType.className);
        if (!clsOpt) {
            if (isNativeClass(targetType.className)) {
                std::vector<CallbackTypeArgs> signatures =
                    ClassCall::getInstance().getMethodSignatures(targetType.className, node.methodName);

                for (const auto& signature : signatures) {
                    if (matchesNativeSignature(signature, argTypes)) {
                        node.className = targetType.className;
                        node.methodOrdinal = -1;
                        return {};
                    }
                }

                diagnostics.addError(node.loc,
                    "No matching method '" + node.methodName + "' with " +
                    std::to_string(argCount) + " argument(s) in native class '" +
                    targetType.className + "'");
                return {};
            }

            diagnostics.addError(node.loc, "Unknown class: " + targetType.className);
            return {};
        }

        ClassNode& cls = clsOpt->get();

        std::vector<std::pair<std::reference_wrapper<ClassNode>, std::reference_wrapper<MethodDecl>>> candidates;
        std::optional<std::reference_wrapper<ClassNode>> walk = std::ref(cls);
        for (size_t step = 0; step <= this->classes.size() && walk; ++step) {
            ClassNode& current = walk->get();
            for (auto& method : current.methods) {
                if (method.isConstructor || method.name != node.methodName)
                    continue;
                if (method.params.size() != argCount)
                    continue;
                if (method.isPrivate &&
                    !(scope.hasMethod() && &scope.classRef() == &current)) {
                    continue;
                }

                bool match = true;
                for (size_t j = 0; j < argCount; ++j) {
                    const TypeInfo& param = method.paramTypes[j];
                    if (param.kind != TypeKind::Unknown &&
                        argTypes[j].kind != TypeKind::Unknown &&
                        !this->isTypeCompatible(param, argTypes[j])) {
                        match = false;
                        break;
                    }
                }

                if (match)
                    candidates.emplace_back(std::ref(current), std::ref(method));
            }

            if (current.baseClassName.empty())
                break;

            auto baseOpt = this->findClass(current.baseClassName);
            if (!baseOpt)
                break;

            walk = baseOpt;
        }

        if (candidates.empty()) {
            diagnostics.addError(node.loc,
                "No matching method '" + node.methodName + "' with " +
                std::to_string(argCount) + " argument(s) in class '" + cls.name + "'");
            return {};
        }

        auto depthOf = [&](const ClassNode& owner) {
            int depth = 0;
            std::optional<std::reference_wrapper<ClassNode>> cur = std::ref(cls);
            for (size_t step = 0; step <= this->classes.size() && cur; ++step) {
                ClassNode& current = cur->get();
                if (&current == &owner)
                    return depth;

                depth++;

                if (current.baseClassName.empty())
                    return std::numeric_limits<int>::max();

                auto baseOpt = this->findClass(current.baseClassName);
                if (!baseOpt)
                    return std::numeric_limits<int>::max();

                cur = baseOpt;
            }

            return std::numeric_limits<int>::max();
        };

        auto best = candidates[0];
        int bestDepth = depthOf(best.first.get());
        size_t bestScore = knownParamCount(best.second.get());

        for (size_t i = 1; i < candidates.size(); ++i) {
            int depth = depthOf(candidates[i].first.get());
            size_t score = knownParamCount(candidates[i].second.get());

            if (depth < bestDepth || (depth == bestDepth && score > bestScore)) {
                best = candidates[i];
                bestDepth = depth;
                bestScore = score;
            }
        }

        MethodDecl& method = best.second.get();

        node.className = cls.name;
        node.methodOrdinal = this->methodOrdinal(cls.name, this->methodSignature(method));

        return method.returnType;
    }

    TypeInfo SemanticAnalyzer::checkFuncCall(FuncCallNode& node, MethodScope& scope) {
        size_t argCount = node.args.size();

        std::vector<TypeInfo> argTypes;
        argTypes.reserve(argCount);
        for (auto& arg : node.args)
            argTypes.push_back(checkExpr(*arg, scope));

        auto it = functionsByName.find(node.name);
        if (it == functionsByName.end() || it->second.empty()) {
            if (this->isNameDefined(node.name, scope)) {
                TypeInfo variableType = this->lookupName(node.name, scope);

                if (variableType.kind != TypeKind::Unknown &&
                    variableType.kind != TypeKind::Function) {
                    diagnostics.addError(node.loc,
                        "Value '" + node.name + "' is not callable");
                    return {};
                }

                node.isCallable = true;
                node.resolvedName = node.name;
                return {};
            }

            diagnostics.addError(node.loc,
                "No matching function '" + node.name + "' with " +
                std::to_string(argCount) + " argument(s)");
            return {};
        }

        std::vector<size_t> candidates;
        for (size_t i = 0; i < it->second.size(); ++i) {
            const auto& decl = it->second[i].get().decl;
            if (decl.params.size() != argCount)
                continue;

            bool match = true;
            for (size_t j = 0; j < argCount; ++j) {
                const TypeInfo& param = decl.paramTypes[j];
                if (param.kind != TypeKind::Unknown &&
                    argTypes[j].kind != TypeKind::Unknown &&
                    !this->isTypeCompatible(param, argTypes[j])) {
                    match = false;
                    break;
                }
            }

            if (match)
                candidates.push_back(i);
        }

        if (candidates.empty()) {
            diagnostics.addError(node.loc,
                "No matching function '" + node.name + "' with " +
                std::to_string(argCount) + " argument(s)");
            return {};
        }

        size_t best = candidates[0];
        size_t bestScore = knownParamCount(it->second[best].get().decl);
        for (size_t i = 1; i < candidates.size(); ++i) {
            size_t score = knownParamCount(it->second[candidates[i]].get().decl);
            if (score > bestScore) {
                best = candidates[i];
                bestScore = score;
            }
        }

        MethodDecl& decl = it->second[best].get().decl;

        node.resolvedName = node.name;
        node.functionOrdinal = static_cast<int>(best);

        return decl.returnType;
    }

    TypeInfo SemanticAnalyzer::checkNew(NewNode& node, MethodScope& scope) {
        size_t argCount = node.args.size();

        std::vector<TypeInfo> argTypes;
        argTypes.reserve(argCount);
        for (auto& arg : node.args)
            argTypes.push_back(checkExpr(*arg, scope));

        auto clsOpt = findClass(node.className);
        if (!clsOpt) {
            if (isNativeClass(node.className)) {
                std::vector<CallbackTypeArgs> signatures =
                    ClassCall::getInstance().getConstructorSignatures(node.className);

                if (signatures.empty()) {
                    if (argCount != 0) {
                        diagnostics.addError(node.loc,
                            "Native class '" + node.className + "' has no constructor");
                    }

                    return { TypeKind::Object, node.className };
                }

                for (const auto& signature : signatures) {
                    if (matchesNativeSignature(signature, argTypes))
                        return { TypeKind::Object, node.className };
                }

                diagnostics.addError(node.loc,
                    "No matching constructor for native class '" + node.className + "' with " +
                    std::to_string(argCount) + " argument(s)");
                return { TypeKind::Object, node.className };
            }

            diagnostics.addError(node.loc, "Unknown class: " + node.className);
            return {};
        }

        ClassNode& cls = clsOpt->get();
        auto ctorRef = this->findConstructor(cls);

        if (!ctorRef) {
            if (argCount != 0) {
                diagnostics.addError(node.loc,
                    "Class '" + cls.name + "' has no constructor");
            }

            return { TypeKind::Object, cls.name };
        }

        MethodDecl& ctor = ctorRef->method.get();

        if (ctor.params.size() != argCount) {
            diagnostics.addError(node.loc,
                "Constructor of class '" + cls.name + "' expects " +
                std::to_string(ctor.params.size()) + " argument(s), got " +
                std::to_string(argCount));

            return { TypeKind::Object, cls.name };
        }

        for (size_t j = 0; j < argCount; ++j) {
            if (ctor.paramTypes[j].kind == TypeKind::Unknown ||
                argTypes[j].kind == TypeKind::Unknown)
                continue;

            if (!this->isTypeCompatible(ctor.paramTypes[j], argTypes[j])) {
                diagnostics.addError(node.loc,
                    "Type mismatch for constructor parameter '" + ctor.params[j].name +
                    "': expected " + typeToString(ctor.paramTypes[j]) +
                    ", got " + typeToString(argTypes[j]));
            }
        }

        return { TypeKind::Object, cls.name };
    }

    TypeInfo SemanticAnalyzer::checkSuperCall(SuperCallNode& node, MethodScope& scope) {
        if (!scope.hasMethod() || !scope.hasClass()) {
            diagnostics.addError(node.loc, "'super(...)' is only available inside class methods");
            return {};
        }

        MethodDecl& method = scope.methodRef();
        if (!method.isConstructor) {
            diagnostics.addError(node.loc, "'super(...)' is only available inside constructors");
            return {};
        }

        ClassNode& cls = scope.classRef();
        if (cls.baseClassName.empty()) {
            diagnostics.addError(node.loc, "Class '" + cls.name + "' has no base class");
            return {};
        }

        method.hasSuperCall = true;

        size_t argCount = node.args.size();
        std::vector<TypeInfo> argTypes;
        argTypes.reserve(argCount);
        for (auto& arg : node.args)
            argTypes.push_back(checkExpr(*arg, scope));

        auto baseOpt = this->findClass(cls.baseClassName);
        if (!baseOpt) {
            diagnostics.addError(node.loc, "Unknown base class: " + cls.baseClassName);
            return {};
        }

        auto ctorRef = this->findConstructor(baseOpt->get());

        if (!ctorRef) {
            if (argCount != 0) {
                diagnostics.addError(node.loc, "Base class has no constructor");
            }

            node.constructorIndex = -1;
            return {};
        }

        ClassNode& ctorOwner = ctorRef->owner.get();
        MethodDecl& ctor = ctorRef->method.get();
        node.className = ctorOwner.name;
        for (size_t i = 0; i < ctorOwner.methods.size(); ++i) {
            if (&ctorOwner.methods[i] == &ctor) {
                node.constructorIndex = static_cast<int>(i);
                break;
            }
        }

        if (ctor.params.size() != argCount) {
            diagnostics.addError(node.loc,
                "Base constructor of class '" + cls.baseClassName + "' expects " +
                std::to_string(ctor.params.size()) + " argument(s), got " +
                std::to_string(argCount));
            return {};
        }

        for (size_t j = 0; j < argCount; ++j) {
            if (ctor.paramTypes[j].kind == TypeKind::Unknown ||
                argTypes[j].kind == TypeKind::Unknown)
                continue;

            if (!this->isTypeCompatible(ctor.paramTypes[j], argTypes[j])) {
                diagnostics.addError(node.loc,
                    "Type mismatch for base constructor parameter '" + ctor.params[j].name +
                    "': expected " + typeToString(ctor.paramTypes[j]) +
                    ", got " + typeToString(argTypes[j]));
            }
        }

        return {};
    }

    TypeInfo SemanticAnalyzer::checkInstanceOf(InstanceOfNode& node, MethodScope& scope) {
        checkExpr(*node.target, scope);

        if (!this->findClass(node.className) && !isNativeClass(node.className)) {
            diagnostics.addError(node.loc, "Unknown class: " + node.className);
        }

        return { TypeKind::Bool };
    }

    TypeInfo SemanticAnalyzer::checkReturn(ReturnNode& node, MethodScope& scope) {
        if (!scope.hasMethod()) {
            diagnostics.addError(node.loc, "'return' is only available inside functions or class methods");
            return {};
        }

        MethodDecl& method = scope.methodRef();
        method.hasReturnStatement = true;

        TypeInfo valueType;
        if (node.value) {
            valueType = checkExpr(*node.value, scope);

            if (method.returnType.kind == TypeKind::Void) {
                diagnostics.addError(node.loc,
                    "Void function '" + method.name + "' cannot return a value");
                return valueType;
            }

            if (method.hasReturnType &&
                method.returnType.kind != TypeKind::Unknown &&
                valueType.kind != TypeKind::Unknown &&
                !this->isTypeCompatible(method.returnType, valueType)) {
                diagnostics.addError(node.loc,
                    "Return type mismatch in function '" + method.name +
                    "': expected " + typeToString(method.returnType) +
                    ", got " + typeToString(valueType));
            }
        }

        return valueType;
    }

    TypeInfo SemanticAnalyzer::checkLambda(LambdaNode& node, MethodScope& scope) {
        MethodDecl& decl = node.decl;
        decl.paramTypes.resize(decl.params.size());

        for (size_t i = 0; i < decl.params.size(); ++i) {
            if (decl.params[i].hasType)
                decl.paramTypes[i] = typeFromName(decl.params[i].typeName, decl.loc, true);
        }

        if (decl.hasReturnType)
            decl.returnType = typeFromName(decl.returnTypeName, decl.loc, true);

        MethodScope lambdaScope;
        if (scope.hasClass())
            lambdaScope.cls = scope.cls;
        lambdaScope.method = std::ref(decl);

        if (decl.body)
            checkStatement(*decl.body, lambdaScope);

        if (decl.hasReturnType &&
            decl.returnType.kind != TypeKind::Void && !decl.hasReturnStatement) {
            diagnostics.addWarning(decl.loc,
                "Missing return statement in anonymous function");
        }

        return { TypeKind::Function };
    }

    TypeInfo SemanticAnalyzer::lookupName(const std::string& name, MethodScope& scope) {
        if (scope.hasMethod()) {
            MethodDecl& method = scope.methodRef();
            for (size_t i = 0; i < method.params.size(); ++i) {
                if (method.params[i].name == name)
                    return method.paramTypes[i];
            }

            if (scope.hasClass()) {
                if (auto field = findField(scope.classRef(), name)) {
                    if (field->member.get().isPrivate && &scope.classRef() != &field->owner.get())
                        return {};

                    return field->member.get().type;
                }
            }
        }

        auto it = globalTypes.find(name);
        if (it != globalTypes.end())
            return it->second;

        return {};
    }

    bool SemanticAnalyzer::isNameDefined(const std::string& name, MethodScope& scope) const {
        if (scope.hasMethod()) {
            const MethodDecl& method = scope.methodRef();
            for (const auto& param : method.params) {
                if (param.name == name)
                    return true;
            }

            if (scope.hasClass() && findField(scope.classRef(), name))
                return true;
        }

        return globalTypes.find(name) != globalTypes.end();
    }

    void SemanticAnalyzer::unify(TypeInfo& target, const TypeInfo& from, SourceLocation loc, const std::string& what) {
        if (from.kind == TypeKind::Unknown || from.kind == TypeKind::Void)
            return;

        if (target.kind == TypeKind::Unknown) {
            target = from;
            return;
        }

        if (!this->isTypeCompatible(target, from)) {
            diagnostics.addError(loc,
                "Type mismatch for " + what + ": expected " +
                typeToString(target) + ", got " + typeToString(from));
        }
    }

    size_t SemanticAnalyzer::knownParamCount(const MethodDecl& method) const {
        size_t count = 0;
        for (const auto& type : method.paramTypes) {
            if (type.kind != TypeKind::Unknown)
                count++;
        }
        return count;
    }

    std::string SemanticAnalyzer::methodSignature(const MethodDecl& method) const {
        std::string signature = method.name + "(";
        for (size_t i = 0; i < method.paramTypes.size(); ++i) {
            if (i != 0)
                signature += ",";

            TypeInfo paramType = method.params[i].hasType
                ? this->typeFromName(method.params[i].typeName, method.loc, false)
                : TypeInfo{};
            signature += this->typeToString(paramType);
        }
        signature += ")";
        return signature;
    }

    int SemanticAnalyzer::methodOrdinal(const std::string& className, const std::string& signature) const {
        auto classIt = this->classMethodOrdinals.find(className);
        if (classIt == this->classMethodOrdinals.end())
            return -1;

        auto sigIt = classIt->second.find(signature);
        if (sigIt == classIt->second.end())
            return -1;

        return sigIt->second;
    }

    bool SemanticAnalyzer::isDerived(const std::string& derivedName, const std::string& baseName) const {
        if (derivedName == baseName)
            return true;

        std::string current = derivedName;
        for (size_t step = 0; step <= this->classes.size() && !current.empty(); ++step) {
            auto clsOpt = this->findClass(current);
            if (!clsOpt)
                return false;

            ClassNode& cls = clsOpt->get();
            if (cls.baseClassName == baseName)
                return true;

            current = cls.baseClassName;
        }

        return false;
    }

    bool SemanticAnalyzer::isTypeCompatible(const TypeInfo& target, const TypeInfo& from) const {
        if (target == from)
            return true;

        if (target.kind == TypeKind::Object && from.kind == TypeKind::Object)
            return this->isDerived(from.className, target.className);

        return false;
    }

    std::optional<SemanticAnalyzer::FieldRef> SemanticAnalyzer::findField(ClassNode& cls, const std::string& name) const {
        std::optional<std::reference_wrapper<ClassNode>> current = std::ref(cls);
        for (size_t step = 0; step <= this->classes.size() && current; ++step) {
            ClassNode& clsRef = current->get();
            for (auto& member : clsRef.members) {
                if (member.name == name) {
                    return FieldRef{ std::ref(member), std::ref(clsRef) };
                }
            }

            if (clsRef.baseClassName.empty())
                break;

            auto baseOpt = this->findClass(clsRef.baseClassName);
            if (!baseOpt)
                break;

            current = baseOpt;
        }

        return std::nullopt;
    }

    std::optional<SemanticAnalyzer::ConstructorRef> SemanticAnalyzer::findConstructor(ClassNode& cls) const {
        std::optional<std::reference_wrapper<ClassNode>> current = std::ref(cls);
        for (size_t step = 0; step <= this->classes.size() && current; ++step) {
            ClassNode& clsRef = current->get();
            for (auto& method : clsRef.methods) {
                if (method.isConstructor) {
                    return ConstructorRef{ std::ref(method), std::ref(clsRef) };
                }
            }

            if (clsRef.baseClassName.empty())
                break;

            auto baseOpt = this->findClass(clsRef.baseClassName);
            if (!baseOpt)
                break;

            current = baseOpt;
        }

        return std::nullopt;
    }

}
