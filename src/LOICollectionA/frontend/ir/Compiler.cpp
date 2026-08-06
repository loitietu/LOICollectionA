#include <memory>
#include <vector>
#include <functional>
#include <unordered_map>
#include <unordered_set>

#include "LOICollectionA/frontend/ir/ByteCode.h"
#include "LOICollectionA/frontend/Callback.h"

#include "LOICollectionA/frontend/ir/Compiler.h"

namespace LOICollection::frontend::ir {
    Compiler::Compiler(DiagnosticEngine& diag) : current(std::ref(chunk)), diagnostics(diag) {}

    BytecodeChunk Compiler::compile(ASTNode& root) {
        if (auto tpl = dynamic_cast<TemplateNode*>(&root)) {
            for (auto& part : tpl->parts) {
                if (auto cls = dynamic_cast<ClassNode*>(part.get()))
                    this->registerClassMeta(*cls);
                else if (auto fn = dynamic_cast<FunctionDefNode*>(part.get()))
                    this->registerFunctionMeta(*fn);
            }

            for (auto& part : tpl->parts) {
                if (auto cls = dynamic_cast<ClassNode*>(part.get()))
                    this->compileClassBodies(*cls);
                else if (auto fn = dynamic_cast<FunctionDefNode*>(part.get()))
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
        if (classIndices.find(node.name) != classIndices.end()) {
            this->diagnostics.addError(node.loc, "Duplicate class: " + node.name);
            return;
        }

        int classIdx = static_cast<int>(this->chunk.classes.size());
        this->classIndices[node.name] = classIdx;
        this->registeredClasses.insert(classIdx);

        ir::ClassMeta meta;
        meta.name = node.name;

        for (const auto& member : node.members) {
            meta.fieldNames.push_back(member.name);
            meta.hasDefault.push_back(member.hasDefault);

            if (member.hasDefault) {
                if (auto literal = dynamic_cast<const ValueNode*>(member.defaultExpr.get())) {
                    meta.defaults.push_back(literal->value);
                } else {
                    this->diagnostics.addError(node.loc, "Member default value of '" + member.name + "' must be a constant literal");

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
                meta.methods.push_back(methodIdx);

                this->classMethodIndices[node.name].push_back(methodIdx);
            }
        }

        this->chunk.classes.push_back(std::move(meta));
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
            this->current.get().emit(OpCode::CALL_METHOD, it->second[node.methodOrdinal]);
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

    void Compiler::visit(FunctionDefNode& node) {
        this->registerFunctionMeta(node);
        this->compileFunctionBody(node);
    }

    void Compiler::registerFunctionMeta(FunctionDefNode& node) {
        int funcIdx = static_cast<int>(methodCount++);
        this->functionIndices[node.name].push_back(funcIdx);
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
}
