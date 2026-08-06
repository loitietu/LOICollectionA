#include <algorithm>
#include <iterator>
#include <memory>
#include <vector>
#include <functional>
#include <unordered_map>
#include <unordered_set>

#include "LOICollectionA/frontend/ir/ByteCode.h"
#include "LOICollectionA/frontend/Callback.h"

#include "LOICollectionA/frontend/ir/Compiler.h"

namespace LOICollection::frontend::ir {
    namespace {
        class MethodBodyCounter : public ASTVisitor {
        public:
            size_t count = 0;

            void countNode(ASTNode& node) {
                node.accept(*this);
            }

            void countArgs(TemplateNode* args) {
                if (!args)
                    return;

                for (auto& part : args->parts)
                    countNode(*part);
            }

            void countBody(ASTNode* body) {
                if (body)
                    countNode(*body);
            }

            void visit(ValueNode&) override {}
            void visit(VariableNode&) override {}
            void visit(ThisNode&) override {}
            void visit(SuperNode&) override {}

            void visit(AssignmentNode& node) override {
                countNode(*node.target);
                countNode(*node.value);
            }

            void visit(IfNode& node) override {
                countNode(*node.condition);
                countBody(node.trueBranch.get());
                countBody(node.falseBranch.get());
            }

            void visit(CompareNode& node) override {
                countNode(*node.left);
                countNode(*node.right);
            }

            void visit(LogicalNode& node) override {
                countNode(*node.left);
                countNode(*node.right);
            }

            void visit(FunctionNode& node) override {
                countArgs(node.args.get());
            }

            void visit(MacroNode& node) override {
                countArgs(node.args.get());
            }

            void visit(ArithmeticNode& node) override {
                countNode(*node.left);
                countNode(*node.right);
            }

            void visit(UnaryNode& node) override {
                countNode(*node.operand);
            }

            void visit(TemplateNode& node) override {
                for (auto& part : node.parts)
                    countNode(*part);
            }

            void visit(ClassNode& node) override {
                count += node.methods.size();

                for (const auto& method : node.methods)
                    countBody(method.body.get());
            }

            void visit(ReturnNode& node) override {
                countBody(node.value.get());
            }

            void visit(NewNode& node) override {
                countArgs(node.args.get());
            }

            void visit(MemberAccessNode& node) override {
                countNode(*node.target);
            }

            void visit(MethodCallNode& node) override {
                countNode(*node.target);
                countArgs(node.args.get());
            }

            void visit(SuperCallNode& node) override {
                countArgs(node.args.get());
            }

            void visit(InstanceOfNode& node) override {
                countNode(*node.target);
            }

            void visit(FunctionDefNode& node) override {
                count++;
                countBody(node.decl.body.get());
            }

            void visit(FuncCallNode& node) override {
                countArgs(node.args.get());
            }

            void visit(LambdaNode& node) override {
                count++;
                countBody(node.decl.body.get());
            }
        };

        size_t countMethodBodies(ASTNode& root) {
            MethodBodyCounter counter;
            counter.countNode(root);
            return counter.count;
        }
    }

    Compiler::Compiler(DiagnosticEngine& diag) : current(std::ref(chunk)), diagnostics(diag) {}

    BytecodeChunk Compiler::compile(ASTNode& root) {
        this->chunk.methodBodies.reserve(countMethodBodies(root));

        if (auto tpl = dynamic_cast<TemplateNode*>(&root)) {
            for (auto& part : tpl->parts) {
                if (auto cls = dynamic_cast<ClassNode*>(part.get()))
                    this->classNodes[cls->name] = cls;
            }

            for (auto& part : tpl->parts) {
                if (auto cls = dynamic_cast<ClassNode*>(part.get()))
                    this->registerClassMeta(*cls);
                else if (auto fn = dynamic_cast<FunctionDefNode*>(part.get()))
                    this->registerFunctionMeta(*fn);
            }

            for (auto* node : this->bodyOrder) {
                if (auto cls = dynamic_cast<ClassNode*>(node))
                    this->compileClassBodies(*cls);
                else if (auto fn = dynamic_cast<FunctionDefNode*>(node))
                    this->compileFunctionBody(*fn);
            }

            size_t lastValuePart = SIZE_MAX;
            for (size_t i = 0; i < tpl->parts.size(); ++i) {
                if (!dynamic_cast<ClassNode*>(tpl->parts[i].get()) &&
                    !dynamic_cast<FunctionDefNode*>(tpl->parts[i].get()))
                    lastValuePart = i;
            }

            for (size_t i = 0; i < tpl->parts.size(); ++i) {
                if (dynamic_cast<ClassNode*>(tpl->parts[i].get()) ||
                    dynamic_cast<FunctionDefNode*>(tpl->parts[i].get()))
                    continue;

                tpl->parts[i]->accept(*this);

                if (i != lastValuePart)
                    this->current.get().emit(OpCode::POP);
            }
        } else {
            root.accept(*this);
        }

        this->current.get().emit(OpCode::HALT);

        return std::move(chunk);
    }

    void Compiler::visit(ValueNode& node) {
        auto val = node.value;

        int idx = this->addConstant(val);
        switch (val.index()) {
            case 0: current.get().emit(OpCode::PUSH_INT, idx); break;
            case 1: current.get().emit(OpCode::PUSH_FLOAT, idx); break;
            case 2: current.get().emit(OpCode::PUSH_STR, idx); break;
            case 3: current.get().emit(OpCode::PUSH_BOOL, idx); break;
            default:
                this->diagnostics.addError({0, 0, 0}, "Unsupported constant value type");
                break;
        }
    }

    void Compiler::visit(VariableNode& node) {
        int idx = this->addConstant(node.name);
        this->current.get().emit(OpCode::LOAD_VAR, idx);
    }

    void Compiler::visit(AssignmentNode& node) {
        node.value->accept(*this);

        this->current.get().emit(OpCode::DUP);

        if (auto var = dynamic_cast<VariableNode*>(node.target.get())) {
            int idx = this->addConstant(var->name);
            this->current.get().emit(OpCode::STORE_VAR, idx);
        } else if (auto member = dynamic_cast<MemberAccessNode*>(node.target.get())) {
            member->target->accept(*this);

            int idx = this->addConstant(member->memberName);
            this->current.get().emit(OpCode::STORE_FIELD, idx);
        } else {
            this->diagnostics.addError(node.loc, "Invalid assignment target");
        }
    }

    void Compiler::visit(IfNode& node) {
        node.condition->accept(*this);

        size_t jmpFalseIdx = this->current.get().emit(OpCode::JMP_IF_FALSE, 0);

        node.trueBranch->accept(*this);

        size_t jmpEndIdx = this->current.get().emit(OpCode::JMP, 0);

        int falseStart = static_cast<int>(this->current.get().currentIP());
        this->current.get().patchJump(jmpFalseIdx, falseStart - static_cast<int>(jmpFalseIdx) - 1);

        node.falseBranch->accept(*this);

        int endPos = static_cast<int>(this->current.get().currentIP());
        this->current.get().patchJump(jmpEndIdx, endPos - static_cast<int>(jmpEndIdx) - 1);
    }

    void Compiler::visit(CompareNode& node) {
        node.left->accept(*this);
        node.right->accept(*this);

        if (node.op == "==") this->current.get().emit(OpCode::CMP_EQ);
        else if (node.op == "!=") this->current.get().emit(OpCode::CMP_NE);
        else if (node.op == ">") this->current.get().emit(OpCode::CMP_GT);
        else if (node.op == "<") this->current.get().emit(OpCode::CMP_LT);
        else if (node.op == ">=") this->current.get().emit(OpCode::CMP_GE);
        else if (node.op == "<=") this->current.get().emit(OpCode::CMP_LE);
        else
            this->diagnostics.addError({0, 0, 0}, "Unknown compare op: " + node.op);
    }

    void Compiler::visit(LogicalNode& node) {
        bool isAnd = (node.op == "&&");

        if (node.left->getType() == ASTNode::Type::Value) {
            auto& leftVal = static_cast<const ValueNode&>(*node.left).value;
            if (std::holds_alternative<bool>(leftVal)) {
                bool leftBool = std::get<bool>(leftVal);
                if (isAnd) {
                    if (leftBool) {
                        int idxTrue = this->addConstant(true);
                        this->current.get().emit(OpCode::PUSH_BOOL, idxTrue);

                        node.right->accept(*this);

                        this->current.get().emit(OpCode::LOGIC_AND);
                    } else {
                        int idxFalse = this->addConstant(false);
                        this->current.get().emit(OpCode::PUSH_BOOL, idxFalse);
                    }
                } else {
                    if (leftBool) {
                        int idxTrue = this->addConstant(true);
                        this->current.get().emit(OpCode::PUSH_BOOL, idxTrue);
                    } else {
                        int idxFalse = this->addConstant(false);
                        this->current.get().emit(OpCode::PUSH_BOOL, idxFalse);

                        node.right->accept(*this);
                        
                        this->current.get().emit(OpCode::LOGIC_OR);
                    }
                }
                return;
            }
        }

        node.left->accept(*this);
        this->current.get().emit(OpCode::DUP);

        size_t jumpToShort = this->current.get().emit(isAnd ? OpCode::JMP_IF_FALSE : OpCode::JMP_IF_TRUE, 0);

        node.right->accept(*this);
        this->current.get().emit(isAnd ? OpCode::LOGIC_AND : OpCode::LOGIC_OR);

        size_t jumpToEnd = this->current.get().emit(OpCode::JMP, 0);

        int shortStart = static_cast<int>(this->current.get().currentIP());

        int shortIdx = this->addConstant(isAnd ? false : true);
        this->current.get().emit(OpCode::POP); 
        this->current.get().emit(OpCode::PUSH_BOOL, shortIdx);

        this->current.get().patchJump(jumpToShort, shortStart - static_cast<int>(jumpToShort) - 1);

        int endPos = static_cast<int>(this->current.get().currentIP());
        this->current.get().patchJump(jumpToEnd, endPos - static_cast<int>(jumpToEnd) - 1);
    }

    void Compiler::visit(FunctionNode& node) {
        int argCount = 0;
        if (node.args) {
            for (auto& part : node.args->parts) {
                part->accept(*this);
                argCount++;
            }
        }

        int metaIdx = this->addFunction(node.namespaces + "::" + node.name, argCount);
        this->current.get().emit(OpCode::CALL, metaIdx);
    }

    void Compiler::visit(MacroNode& node) {
        int argCount = 0;
        if (node.args) {
            for (auto& part : node.args->parts) {
                part->accept(*this);
                argCount++;
            }
        }

        int metaIdx = addMacro(node.name, argCount);
        this->current.get().emit(OpCode::CALL_MACRO, metaIdx);
    }

    void Compiler::visit(ArithmeticNode& node) {
        node.left->accept(*this);
        node.right->accept(*this);

        if (node.op == "+") this->current.get().emit(OpCode::ADD);
        else if (node.op == "-") this->current.get().emit(OpCode::SUB);
        else if (node.op == "*") this->current.get().emit(OpCode::MUL);
        else if (node.op == "/") this->current.get().emit(OpCode::DIV);
        else if (node.op == "%") this->current.get().emit(OpCode::MOD);
        else if (node.op == "^") this->current.get().emit(OpCode::POW);
        else
            this->diagnostics.addError({0, 0, 0}, "Unknown arithmetic op: " + node.op);
    }

    void Compiler::visit(UnaryNode& node) {
        node.operand->accept(*this);

        if (node.op == "-") this->current.get().emit(OpCode::NEG);
        else if (node.op == "!") this->current.get().emit(OpCode::NOT);
        else if (node.op == "+") {}
        else
            this->diagnostics.addError({0, 0, 0}, "Unknown unary op: " + node.op);
    }

    void Compiler::visit(TemplateNode& node) {
        if (node.parts.empty()) {
            int idx = this->addConstant(std::string(""));
            this->current.get().emit(OpCode::PUSH_STR, idx);

            return;
        }

        for (size_t i = 0; i < node.parts.size(); ++i) {
            node.parts[i]->accept(*this);

            bool producesValue = node.parts[i]->getType() != ASTNode::Type::Class
                && node.parts[i]->getType() != ASTNode::Type::Return;

            if (i != node.parts.size() - 1 && producesValue)
                this->current.get().emit(OpCode::POP);
        }
    }

    void Compiler::visit(ClassNode& node) {
        this->registerClassMeta(node);
        this->compileClassBodies(node);
    }

    void Compiler::registerClassMeta(ClassNode& node) {
        if (classIndices.contains(node.name))
            return;

        int baseIdx = -1;
        if (!node.baseClassName.empty()) {
            if (this->registeringClasses.contains(node.name)) {
                this->diagnostics.addError(node.loc,
                    "Circular inheritance involving class '" + node.name + "'");
                return;
            }

            this->registeringClasses.insert(node.name);

            auto baseIt = this->classNodes.find(node.baseClassName);
            if (baseIt == this->classNodes.end()) {
                this->diagnostics.addError(node.loc, "Unknown base class: " + node.baseClassName);
                this->registeringClasses.erase(node.name);
                return;
            }

            if (!this->classIndices.contains(node.baseClassName))
                this->registerClassMeta(*baseIt->second);

            baseIdx = this->classIndices[node.baseClassName];
            this->registeringClasses.erase(node.name);
        }

        int classIdx = static_cast<int>(this->chunk.classes.size());
        this->classIndices[node.name] = classIdx;
        this->registeredClasses.insert(classIdx);

        ir::ClassMeta meta;
        meta.name = node.name;
        meta.baseClassIndex = baseIdx;

        if (baseIdx >= 0) {
            const auto& base = this->chunk.classes[baseIdx];
            meta.fieldNames = base.fieldNames;
            meta.defaults = base.defaults;
            meta.hasDefault = base.hasDefault;
            meta.constructorIndex = base.constructorIndex;
            meta.methods = base.methods;
            meta.methodSignatures = base.methodSignatures;
        }

        for (const auto& member : node.members) {
            auto fieldIt = std::ranges::find(meta.fieldNames, member.name);
            if (fieldIt != meta.fieldNames.end()) {
                size_t fieldIdx = static_cast<size_t>(std::distance(meta.fieldNames.begin(), fieldIt));
                meta.hasDefault[fieldIdx] = member.hasDefault;

                if (member.hasDefault) {
                    if (auto literal = dynamic_cast<const ValueNode*>(member.defaultExpr.get())) {
                        meta.defaults[fieldIdx] = literal->value;
                    } else {
                        this->diagnostics.addError(node.loc,
                            "Member default value of '" + member.name + "' must be a constant literal");
                        meta.defaults[fieldIdx] = ValueNode::ValueType{};
                    }
                } else {
                    meta.defaults[fieldIdx] = ValueNode::ValueType{};
                }

                continue;
            }

            meta.fieldNames.push_back(member.name);
            meta.hasDefault.push_back(member.hasDefault);

            if (member.hasDefault) {
                if (auto literal = dynamic_cast<const ValueNode*>(member.defaultExpr.get())) {
                    meta.defaults.push_back(literal->value);
                } else {
                    this->diagnostics.addError(node.loc,
                        "Member default value of '" + member.name + "' must be a constant literal");
                    meta.defaults.emplace_back();
                }
            } else {
                meta.defaults.emplace_back();
            }
        }

        for (size_t i = 0; i < node.methods.size(); ++i) {
            int methodIdx = static_cast<int>(methodCount++);

            if (node.methods[i].isConstructor)
                meta.constructorIndex = methodIdx;
            else {
                std::string signature = this->methodSignature(node.methods[i]);
                auto sigIt = std::ranges::find(meta.methodSignatures, signature);
                if (sigIt != meta.methodSignatures.end()) {
                    size_t ordinal = static_cast<size_t>(std::distance(meta.methodSignatures.begin(), sigIt));
                    meta.methods[ordinal] = methodIdx;
                } else {
                    meta.methodSignatures.push_back(signature);
                    meta.methods.push_back(methodIdx);
                }

            }
        }

        this->classMethodIndices[node.name] = meta.methods;
        this->chunk.classes.push_back(std::move(meta));
        this->bodyOrder.push_back(&node);
    }

    void Compiler::compileClassBodies(ClassNode& node) {
        auto classIt = this->classIndices.find(node.name);
        if (classIt == this->classIndices.end() || !this->registeredClasses.contains(classIt->second))
            return;

        int classIdx = classIt->second;

        for (const auto& method : node.methods) {
            ir::MethodMeta mm;
            mm.name = method.name;
            mm.classIndex = classIdx;
            mm.argCount = static_cast<int>(method.params.size());
            for (const auto& param : method.params)
                mm.paramNames.push_back(param.name);

            int bodyIdx = static_cast<int>(this->chunk.methodBodies.size());
            this->chunk.methodBodies.push_back(BytecodeChunk{});

            std::reference_wrapper<BytecodeChunk> saved = this->current;
            this->current = std::ref(chunk.methodBodies.back());

            if (method.isConstructor && !node.baseClassName.empty() && !method.hasSuperCall) {
                int ctorIdx = -1;
                int walkIdx = this->classIndices[node.baseClassName];
                while (walkIdx >= 0) {
                    const auto& walkCls = this->chunk.classes[walkIdx];
                    if (walkCls.constructorIndex != -1) {
                        ctorIdx = walkCls.constructorIndex;
                        break;
                    }
                    walkIdx = walkCls.baseClassIndex;
                }

                int argCount = ctorIdx >= 0 ? this->chunk.methods[ctorIdx].argCount : 0;
                int superIdx = this->addSuperCall(ctorIdx, argCount);
                this->current.get().emit(OpCode::LOAD_THIS);
                this->current.get().emit(OpCode::CALL_SUPER_CTOR, superIdx);
                this->current.get().emit(OpCode::POP);
            }

            if (method.body)
                method.body->accept(*this);

            this->current.get().emit(OpCode::POP);

            int emptyIdx = this->addConstant(std::string(""));
            this->current.get().emit(OpCode::PUSH_STR, emptyIdx);
            this->current.get().emit(OpCode::RETURN);

            this->current = saved;
            mm.bodyIndex = bodyIdx;

            this->chunk.methods.push_back(std::move(mm));
        }
    }

    void Compiler::visit(ReturnNode& node) {
        if (node.value) {
            node.value->accept(*this);
        } else {
            int idx = this->addConstant(std::string(""));
            this->current.get().emit(OpCode::PUSH_STR, idx);
        }

        this->current.get().emit(OpCode::RETURN);
    }

    void Compiler::visit(NewNode& node) {
        if (node.args) {
            for (auto& part : node.args->parts)
                part->accept(*this);
        }

        auto it = this->classIndices.find(node.className);
        if (it == this->classIndices.end()) {
            if (ClassCall::getInstance().isRegistered(node.className)) {
                int argCount = node.args ? static_cast<int>(node.args->parts.size()) : 0;
                int metaIdx = this->addNativeCall(node.className, "", argCount);

                this->current.get().emit(OpCode::NEW_NATIVE, metaIdx);
                return;
            }

            this->diagnostics.addError(node.loc, "Unknown class: " + node.className);
            return;
        }

        this->current.get().emit(OpCode::NEW, it->second);
    }

    void Compiler::visit(MemberAccessNode& node) {
        node.target->accept(*this);

        int idx = this->addConstant(node.memberName);
        this->current.get().emit(OpCode::LOAD_FIELD, idx);
    }

    void Compiler::visit(MethodCallNode& node) {
        if (node.args) {
            for (auto& part : node.args->parts)
                part->accept(*this);
        }

        node.target->accept(*this);

        auto it = classMethodIndices.find(node.className);
        if (it != classMethodIndices.end() && node.methodOrdinal >= 0 &&
            static_cast<size_t>(node.methodOrdinal) < it->second.size()) {
            int argCount = node.args ? static_cast<int>(node.args->parts.size()) : 0;

            if (dynamic_cast<SuperNode*>(node.target.get())) {
                this->current.get().emit(OpCode::CALL_METHOD, it->second[node.methodOrdinal]);
            } else {
                int classIdx = this->classIndices[node.className];
                int metaIdx = this->addVirtualCall(classIdx, node.methodOrdinal, argCount);
                this->current.get().emit(OpCode::CALL_METHOD_VIRTUAL, metaIdx);
            }
            return;
        }

        if (ClassCall::getInstance().isRegistered(node.className)) {
            int argCount = node.args ? static_cast<int>(node.args->parts.size()) : 0;
            int metaIdx = this->addNativeCall(node.className, node.methodName, argCount);

            this->current.get().emit(OpCode::CALL_NATIVE_METHOD, metaIdx);
            return;
        }

        this->diagnostics.addError(node.loc, "Unresolved method call: " + node.methodName);
    }

    void Compiler::visit(ThisNode&) {
        this->current.get().emit(OpCode::LOAD_THIS);
    }

    void Compiler::visit(SuperNode&) {
        this->current.get().emit(OpCode::LOAD_THIS);
    }

    void Compiler::visit(SuperCallNode& node) {
        if (node.args) {
            for (auto& part : node.args->parts)
                part->accept(*this);
        }

        this->current.get().emit(OpCode::LOAD_THIS);

        int constructorIndex = -1;
        if (node.constructorIndex >= 0) {
            auto classIt = this->classIndices.find(node.className);
            if (classIt != this->classIndices.end())
                constructorIndex = this->chunk.classes[classIt->second].constructorIndex;
        }

        int argCount = node.args ? static_cast<int>(node.args->parts.size()) : 0;
        int metaIdx = this->addSuperCall(constructorIndex, argCount);
        this->current.get().emit(OpCode::CALL_SUPER_CTOR, metaIdx);
    }

    void Compiler::visit(InstanceOfNode& node) {
        node.target->accept(*this);

        int nameIdx = this->addConstant(node.className);
        this->current.get().emit(OpCode::INSTANCEOF, nameIdx);
    }

    void Compiler::visit(FunctionDefNode& node) {
        this->registerFunctionMeta(node);
        this->compileFunctionBody(node);
    }

    void Compiler::registerFunctionMeta(FunctionDefNode& node) {
        int funcIdx = static_cast<int>(methodCount++);
        this->functionIndices[node.name].push_back(funcIdx);
        this->bodyOrder.push_back(&node);
    }

    void Compiler::compileFunctionBody(FunctionDefNode& node) {
        ir::MethodMeta mm;
        mm.name = node.name;
        mm.classIndex = -1;
        mm.argCount = static_cast<int>(node.decl.params.size());
        for (const auto& param : node.decl.params)
            mm.paramNames.push_back(param.name);

        int bodyIdx = static_cast<int>(this->chunk.methodBodies.size());
        this->chunk.methodBodies.push_back(BytecodeChunk{});

        std::reference_wrapper<BytecodeChunk> saved = this->current;
        this->current = std::ref(this->chunk.methodBodies.back());

        if (node.decl.body)
            node.decl.body->accept(*this);

        this->current.get().emit(OpCode::POP);

        int emptyIdx = this->addConstant(std::string(""));
        this->current.get().emit(OpCode::PUSH_STR, emptyIdx);
        this->current.get().emit(OpCode::RETURN);

        this->current = saved;
        mm.bodyIndex = bodyIdx;

        this->chunk.methods.push_back(std::move(mm));
    }

    void Compiler::visit(FuncCallNode& node) {
        if (node.isCallable) {
            if (node.args) {
                for (auto& part : node.args->parts)
                    part->accept(*this);
            }

            int idx = this->addConstant(node.resolvedName);
            this->current.get().emit(OpCode::LOAD_VAR, idx);

            int argCount = node.args ? static_cast<int>(node.args->parts.size()) : 0;
            this->current.get().emit(OpCode::CALL_LAMBDA, argCount);
            return;
        }

        if (node.args) {
            for (auto& part : node.args->parts)
                part->accept(*this);
        }

        auto it = this->functionIndices.find(node.resolvedName);
        if (it == this->functionIndices.end() || node.functionOrdinal < 0 ||
            static_cast<size_t>(node.functionOrdinal) >= it->second.size()) {
            this->diagnostics.addError(node.loc, "Unresolved function call: " + node.name);
            return;
        }

        this->current.get().emit(OpCode::CALL_FUNC, it->second[node.functionOrdinal]);
    }

    void Compiler::visit(LambdaNode& node) {
        int bodyIdx = static_cast<int>(this->chunk.methodBodies.size());
        this->chunk.methodBodies.push_back(BytecodeChunk{});

        std::reference_wrapper<BytecodeChunk> saved = this->current;
        this->current = std::ref(this->chunk.methodBodies.back());

        if (node.decl.body)
            node.decl.body->accept(*this);

        this->current.get().emit(OpCode::POP);

        int emptyIdx = this->addConstant(std::string(""));
        this->current.get().emit(OpCode::PUSH_STR, emptyIdx);
        this->current.get().emit(OpCode::RETURN);

        this->current = saved;

        std::vector<std::string> paramNames;
        paramNames.reserve(node.decl.params.size());
        for (const auto& param : node.decl.params)
            paramNames.push_back(param.name);

        int lambdaIdx = this->addLambda(bodyIdx, static_cast<int>(node.decl.params.size()), paramNames);
        this->current.get().emit(OpCode::MAKE_LAMBDA, lambdaIdx);
    }

    int Compiler::addNativeCall(const std::string& className, const std::string& name, int argCount) {
        for (size_t i = 0; i < this->chunk.nativeCalls.size(); ++i) {
            const auto& meta = this->chunk.nativeCalls[i];
            if (meta.className == className && meta.name == name && meta.argCount == argCount)
                return static_cast<int>(i);
        }

        this->chunk.nativeCalls.push_back({ className, name, argCount });
        return static_cast<int>(this->chunk.nativeCalls.size() - 1);
    }

    int Compiler::addConstant(const ValueNode::ValueType& val) {
        this->current.get().constants.push_back(val);
        return static_cast<int>(this->current.get().constants.size() - 1);
    }

    int Compiler::addFunction(const std::string& name, int argCount) {
        this->current.get().functions.push_back({name, argCount});
        return static_cast<int>(this->current.get().functions.size() - 1);
    }

    int Compiler::addMacro(const std::string& name, int argCount) {
        this->current.get().macros.push_back({name, argCount});
        return static_cast<int>(this->current.get().macros.size() - 1);
    }

    int Compiler::addLambda(int bodyIndex, int argCount, const std::vector<std::string>& paramNames) {
        this->current.get().lambdas.push_back({bodyIndex, argCount, paramNames});
        return static_cast<int>(this->current.get().lambdas.size() - 1);
    }

    int Compiler::addVirtualCall(int classIndex, int ordinal, int argCount) {
        this->current.get().virtualCalls.push_back({classIndex, ordinal, argCount});
        return static_cast<int>(this->current.get().virtualCalls.size() - 1);
    }

    int Compiler::addSuperCall(int constructorIndex, int argCount) {
        this->current.get().superCalls.push_back({constructorIndex, argCount});
        return static_cast<int>(this->current.get().superCalls.size() - 1);
    }

    std::string Compiler::methodSignature(const MethodDecl& method) const {
        std::string signature = method.name + "(";
        for (size_t i = 0; i < method.paramTypes.size(); ++i) {
            if (i != 0)
                signature += ",";

            const std::string& typeName = method.params[i].typeName;
            if (!method.params[i].hasType) {
                signature += "unknown";
            } else if (typeName == "int" || typeName == "float" || typeName == "string" ||
                       typeName == "bool" || typeName == "void") {
                signature += typeName;
            } else {
                signature += "class " + typeName;
            }
        }
        signature += ")";
        return signature;
    }
}
