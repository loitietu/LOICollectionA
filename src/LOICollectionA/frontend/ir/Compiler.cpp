#include <memory>
#include <stdexcept>

#include "LOICollectionA/frontend/ir/ByteCode.h"

#include "LOICollectionA/frontend/ir/Compiler.h"

namespace LOICollection::frontend::ir {
    BytecodeChunk Compiler::compile(ASTNode& root) {
        root.accept(*this);

        this->chunk.emit(OpCode::HALT);
        return std::move(chunk);
    }

    void Compiler::visit(ValueNode& node) {
        auto val = node.value;

        int idx = this->addConstant(val);
        switch (val.index()) {
            case 0: chunk.emit(OpCode::PUSH_INT, idx); break;
            case 1: chunk.emit(OpCode::PUSH_FLOAT, idx); break;
            case 2: chunk.emit(OpCode::PUSH_STR, idx); break;
            case 3: chunk.emit(OpCode::PUSH_BOOL, idx); break;
        }
    }

    void Compiler::visit(VariableNode& node) {
        int idx = this->addConstant(node.name);
        this->chunk.emit(OpCode::LOAD_VAR, idx);
    }

    void Compiler::visit(AssignmentNode& node) {
        node.value->accept(*this);

        this->chunk.emit(OpCode::DUP);

        int idx = this->addConstant(node.varName);
        this->chunk.emit(OpCode::STORE_VAR, idx);
    }

    void Compiler::visit(IfNode& node) {
        node.condition->accept(*this);

        size_t jmpFalseIdx = this->chunk.emit(OpCode::JMP_IF_FALSE, 0);

        node.trueBranch->accept(*this);

        size_t jmpEndIdx = this->chunk.emit(OpCode::JMP, 0);

        int falseStart = static_cast<int>(this->chunk.currentIP());
        this->chunk.patchJump(jmpFalseIdx, falseStart - static_cast<int>(jmpFalseIdx) - 1);

        node.falseBranch->accept(*this);

        int endPos = static_cast<int>(this->chunk.currentIP());
        this->chunk.patchJump(jmpEndIdx, endPos - static_cast<int>(jmpEndIdx) - 1);
    }

    void Compiler::visit(CompareNode& node) {
        node.left->accept(*this);
        node.right->accept(*this);

        if (node.op == "==") this->chunk.emit(OpCode::CMP_EQ);
        else if (node.op == "!=") this->chunk.emit(OpCode::CMP_NE);
        else if (node.op == ">") this->chunk.emit(OpCode::CMP_GT);
        else if (node.op == "<") this->chunk.emit(OpCode::CMP_LT);
        else if (node.op == ">=") this->chunk.emit(OpCode::CMP_GE);
        else if (node.op == "<=") this->chunk.emit(OpCode::CMP_LE);
        else throw std::runtime_error("Unknown compare op");
    };

    void Compiler::visit(LogicalNode& node) {
        bool isAnd = (node.op == "&&");

        if (node.left->getType() == ASTNode::Type::Value) {
            auto& leftVal = static_cast<const ValueNode&>(*node.left).value;
            if (std::holds_alternative<bool>(leftVal)) {
                bool leftBool = std::get<bool>(leftVal);
                if (isAnd) {
                    if (leftBool) {
                        int idxTrue = this->addConstant(true);
                        this->chunk.emit(OpCode::PUSH_BOOL, idxTrue);

                        node.right->accept(*this);

                        this->chunk.emit(OpCode::LOGIC_AND);
                    } else {
                        int idxFalse = this->addConstant(false);
                        this->chunk.emit(OpCode::PUSH_BOOL, idxFalse);
                    }
                } else {
                    if (leftBool) {
                        int idxTrue = this->addConstant(true);
                        this->chunk.emit(OpCode::PUSH_BOOL, idxTrue);
                    } else {
                        int idxFalse = this->addConstant(false);
                        this->chunk.emit(OpCode::PUSH_BOOL, idxFalse);

                        node.right->accept(*this);
                        
                        this->chunk.emit(OpCode::LOGIC_OR);
                    }
                }
                return;
            }
        }

        node.left->accept(*this);
        this->chunk.emit(OpCode::DUP);

        size_t jumpToShort = this->chunk.emit(isAnd ? OpCode::JMP_IF_FALSE : OpCode::JMP_IF_TRUE, 0);

        node.right->accept(*this);
        this->chunk.emit(isAnd ? OpCode::LOGIC_AND : OpCode::LOGIC_OR);

        size_t jumpToEnd = this->chunk.emit(OpCode::JMP, 0);

        int shortIdx = this->addConstant(isAnd ? false : true);
        this->chunk.emit(OpCode::POP); 
        this->chunk.emit(OpCode::PUSH_BOOL, shortIdx);

        int shortStart = static_cast<int>(this->chunk.currentIP());
        this->chunk.patchJump(jumpToShort, shortStart - static_cast<int>(jumpToShort) - 1);

        int endPos = static_cast<int>(this->chunk.currentIP());
        this->chunk.patchJump(jumpToEnd, endPos - static_cast<int>(jumpToEnd) - 1);
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
        this->chunk.emit(OpCode::CALL, metaIdx);
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
        chunk.emit(OpCode::CALL_MACRO, metaIdx);
    }

    void Compiler::visit(ArithmeticNode& node) {
        node.left->accept(*this);
        node.right->accept(*this);

        if (node.op == "+") this->chunk.emit(OpCode::ADD);
        else if (node.op == "-") this->chunk.emit(OpCode::SUB);
        else if (node.op == "*") this->chunk.emit(OpCode::MUL);
        else if (node.op == "/") this->chunk.emit(OpCode::DIV);
        else if (node.op == "%") this->chunk.emit(OpCode::MOD);
        else if (node.op == "^") this->chunk.emit(OpCode::POW);
        else throw std::runtime_error("Unknown arithmetic op");
    }

    void Compiler::visit(UnaryNode& node) {
        node.operand->accept(*this);

        if (node.op == "-") this->chunk.emit(OpCode::NEG);
        else if (node.op == "!") this->chunk.emit(OpCode::NOT);
        else if (node.op == "+") {}
        else throw std::runtime_error("Unknown unary op");
    }

    void Compiler::visit(TemplateNode& node) {
        if (node.parts.empty()) {
            int idx = this->addConstant(std::string(""));
            this->chunk.emit(OpCode::PUSH_STR, idx);

            return;
        }

        for (size_t i = 0; i < node.parts.size(); ++i) {
            node.parts[i]->accept(*this);
            
            if (i != node.parts.size() - 1)
                this->chunk.emit(OpCode::POP);
        }
    }

    int Compiler::addConstant(const ValueNode::ValueType& val) {
        this->chunk.constants.push_back(val);
        return static_cast<int>(this->chunk.constants.size() - 1);
    }

    int Compiler::addFunction(const std::string& name, int argCount) {
        this->chunk.functions.push_back({name, argCount});
        return static_cast<int>(this->chunk.functions.size() - 1);
    }

    int Compiler::addMacro(const std::string& name, int argCount) {
        this->chunk.macros.push_back({name, argCount});
        return static_cast<int>(this->chunk.macros.size() - 1);
    }
}