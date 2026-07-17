#include <memory>
#include <stdexcept>

#include "LOICollectionA/frontend/ir/ByteCode.h"

#include "LOICollectionA/frontend/ir/Compiler.h"

namespace LOICollection::frontend::ir {
    BytecodeChunk Compiler::compile(const ASTNode& root) {
        this->compileNode(root);
        this->chunk.emit(OpCode::HALT);
        return std::move(chunk);
    }

    void Compiler::compileNode(const ASTNode& node) {
        switch (node.getType()) {
            case ASTNode::Type::Template:
                this->compileTemplate(static_cast<const TemplateNode&>(node), false);
                break;
            case ASTNode::Type::Value:
            case ASTNode::Type::If:
            case ASTNode::Type::Function:
            case ASTNode::Type::Macro:
            case ASTNode::Type::Compare:
            case ASTNode::Type::Logical:
            case ASTNode::Type::Arithmetic:
            case ASTNode::Type::Unary:
                this->compileExpr(static_cast<const ExprNode&>(node));
                break;
            default:
                throw std::runtime_error("Unsupported AST node for IR compilation");
        }
    }

    void Compiler::compileExpr(const ExprNode& expr) {
        switch (expr.getType()) {
            case ASTNode::Type::Value: {
                auto& val = static_cast<const ValueNode&>(expr).value;

                int idx = this->addConstant(val);
                switch (val.index()) {
                    case 0: chunk.emit(OpCode::PUSH_INT, idx); break;
                    case 1: chunk.emit(OpCode::PUSH_FLOAT, idx); break;
                    case 2: chunk.emit(OpCode::PUSH_STR, idx); break;
                    case 3: chunk.emit(OpCode::PUSH_BOOL, idx); break;
                }

                break;
            }
            case ASTNode::Type::If: {
                auto& ifNode = static_cast<const IfNode&>(expr);

                this->compileExpr(*ifNode.condition);

                size_t jmpFalseIdx = this->chunk.emit(OpCode::JMP_IF_FALSE, 0);

                this->compileNode(*ifNode.trueBranch);

                size_t jmpEndIdx = this->chunk.emit(OpCode::JMP, 0);

                int falseStart = static_cast<int>(this->chunk.currentIP());

                this->chunk.patchJump(jmpFalseIdx, falseStart - static_cast<int>(jmpFalseIdx) - 1);

                this->compileNode(*ifNode.falseBranch);

                int endPos = static_cast<int>(this->chunk.currentIP());
                this->chunk.patchJump(jmpEndIdx, endPos - static_cast<int>(jmpEndIdx) - 1);

                break;
            }
            case ASTNode::Type::Function: {
                auto& func = static_cast<const FunctionNode&>(expr);

                int argCount = 0;
                if (func.args) {
                    auto& tpl = static_cast<const TemplateNode&>(*func.args);
                    for (auto& part : tpl.parts) {
                        this->compileNode(*part);
                        argCount++;
                    }
                }

                int metaIdx = addFunction(func.namespaces + "::" + func.name, argCount);
                this->chunk.emit(OpCode::CALL, metaIdx);
                
                break;
            }
            case ASTNode::Type::Macro: {
                auto& macro = static_cast<const MacroNode&>(expr);

                int argCount = 0;
                if (macro.args) {
                    auto& tpl = static_cast<const TemplateNode&>(*macro.args);
                    for (auto& part : tpl.parts) {
                        this->compileNode(*part);
                        argCount++;
                    }
                }

                int metaIdx = addMacro(macro.name, argCount);
                chunk.emit(OpCode::CALL_MACRO, metaIdx);
                
                break;
            }
            case ASTNode::Type::Compare: {
                auto& cmp = static_cast<const CompareNode&>(expr);

                this->compileExpr(*cmp.left);
                this->compileExpr(*cmp.right);

                if (cmp.op == "==") this->chunk.emit(OpCode::CMP_EQ);
                else if (cmp.op == "!=") this->chunk.emit(OpCode::CMP_NE);
                else if (cmp.op == ">") this->chunk.emit(OpCode::CMP_GT);
                else if (cmp.op == "<") this->chunk.emit(OpCode::CMP_LT);
                else if (cmp.op == ">=") this->chunk.emit(OpCode::CMP_GE);
                else if (cmp.op == "<=") this->chunk.emit(OpCode::CMP_LE);
                else throw std::runtime_error("Unknown compare op");

                break;
            }
            case ASTNode::Type::Logical: {
                auto& log = static_cast<const LogicalNode&>(expr);

                bool isAnd = (log.op == "&&");

                if (log.left->getType() == ASTNode::Type::Value) {
                    auto& leftVal = static_cast<const ValueNode&>(*log.left).value;
                    if (std::holds_alternative<bool>(leftVal)) {
                        bool leftBool = std::get<bool>(leftVal);
                        if (isAnd) {
                            if (leftBool) {
                                int idxTrue = this->addConstant(true);
                                this->chunk.emit(OpCode::PUSH_BOOL, idxTrue);

                                this->compileExpr(*log.right);

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

                                this->compileExpr(*log.right);
                                
                                this->chunk.emit(OpCode::LOGIC_OR);
                            }
                        }
                        break;
                    }
                }

                this->compileExpr(*log.left); 
                this->chunk.emit(OpCode::DUP);

                size_t jumpToShort = this->chunk.emit(isAnd ? OpCode::JMP_IF_FALSE : OpCode::JMP_IF_TRUE, 0);

                this->compileExpr(*log.right);
                this->chunk.emit(isAnd ? OpCode::LOGIC_AND : OpCode::LOGIC_OR);

                size_t jumpToEnd = this->chunk.emit(OpCode::JMP, 0);

                int shortIdx = this->addConstant(isAnd ? false : true);
                this->chunk.emit(OpCode::POP); 
                this->chunk.emit(OpCode::PUSH_BOOL, shortIdx);

                int shortStart = static_cast<int>(this->chunk.currentIP());
                this->chunk.patchJump(jumpToShort, shortStart - static_cast<int>(jumpToShort) - 1);

                int endPos = static_cast<int>(this->chunk.currentIP());
                this->chunk.patchJump(jumpToEnd, endPos - static_cast<int>(jumpToEnd) - 1);

                break;
            }
            case ASTNode::Type::Arithmetic: {
                auto& arith = static_cast<const ArithmeticNode&>(expr);

                this->compileExpr(*arith.left);
                this->compileExpr(*arith.right);

                if (arith.op == "+") this->chunk.emit(OpCode::ADD);
                else if (arith.op == "-") this->chunk.emit(OpCode::SUB);
                else if (arith.op == "*") this->chunk.emit(OpCode::MUL);
                else if (arith.op == "/") this->chunk.emit(OpCode::DIV);
                else if (arith.op == "%") this->chunk.emit(OpCode::MOD);
                else if (arith.op == "^") this->chunk.emit(OpCode::POW);
                else throw std::runtime_error("Unknown arithmetic op");

                break;
            }
            case ASTNode::Type::Unary: {
                auto& unary = static_cast<const UnaryNode&>(expr);
                
                this->compileExpr(*unary.operand);

                if (unary.op == "-") this->chunk.emit(OpCode::NEG);
                else if (unary.op == "!") this->chunk.emit(OpCode::NOT);
                else if (unary.op == "+") {}
                else throw std::runtime_error("Unknown unary op");

                break;
            }
            default:
                throw std::runtime_error("Unsupported ExprNode type for IR");
        }
    }

    void Compiler::compileTemplate(const TemplateNode& tpl, bool pushAsMultiple) {
        if (tpl.parts.empty()) {
            int idx = this->addConstant(std::string(""));
            this->chunk.emit(OpCode::PUSH_STR, idx);

            return;
        }

        if (pushAsMultiple) {
            for (auto& part : tpl.parts)
                this->compileNode(*part);

            return;
        }

        if (tpl.parts.size() == 1) {
            this->compileNode(*tpl.parts[0]);
            return;
        }

        this->compileNode(*tpl.parts[0]);
        for (size_t i = 1; i < tpl.parts.size(); ++i) {
            this->compileNode(*tpl.parts[i]);
            this->chunk.emit(OpCode::ADD);
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