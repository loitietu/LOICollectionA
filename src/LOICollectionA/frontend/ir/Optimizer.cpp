#include <vector>
#include <variant>
#include <unordered_set>

#include "LOICollectionA/frontend/ir/VM.h"

#include "LOICollectionA/frontend/ir/Optimizer.h"

namespace LOICollection::frontend::ir {
    struct TrackedValue {
        ValueNode::ValueType value;
        int producer = -1; 
        bool removable = false;
    };

    using StackEntry = std::variant<TrackedValue, std::monostate>;

    bool isJump(OpCode op) {
        return op == OpCode::JMP || op == OpCode::JMP_IF_FALSE || op == OpCode::JMP_IF_TRUE;
    }

    bool isTerminator(OpCode op) {
        return op == OpCode::JMP || op == OpCode::RETURN || op == OpCode::HALT;
    }

    bool isKnown(const StackEntry& entry) {
        return std::holds_alternative<TrackedValue>(entry);
    }

    const TrackedValue& knownValue(const StackEntry& entry) {
        return std::get<TrackedValue>(entry);
    }

    StackEntry popEntry(std::vector<StackEntry>& stack) {
        if (stack.size() > 1) {
            StackEntry top = std::move(stack.back());
            stack.pop_back();
            return top;
        }

        return std::monostate{};
    }

    std::string arithmeticOpName(OpCode op) {
        switch (op) {
            case OpCode::ADD: return "+";
            case OpCode::SUB: return "-";
            case OpCode::MUL: return "*";
            case OpCode::DIV: return "/";
            case OpCode::MOD: return "%";
            case OpCode::POW: return "^";
            default: return "";
        }
    }

    std::string comparisonOpName(OpCode op) {
        switch (op) {
            case OpCode::CMP_EQ: return "==";
            case OpCode::CMP_NE: return "!=";
            case OpCode::CMP_GT: return ">";
            case OpCode::CMP_LT: return "<";
            case OpCode::CMP_GE: return ">=";
            case OpCode::CMP_LE: return "<=";
            default: return "";
        }
    }

    int addConstant(BytecodeChunk& chunk, const ValueNode::ValueType& value) {
        chunk.constants.push_back(value);
        return static_cast<int>(chunk.constants.size() - 1);
    }

    void emitPush(BytecodeChunk& chunk, std::vector<Instruction>& out, const ValueNode::ValueType& value) {
        OpCode op = OpCode::PUSH_INT;

        switch (value.index()) {
            case 0: op = OpCode::PUSH_INT; break;
            case 1: op = OpCode::PUSH_FLOAT; break;
            case 2: op = OpCode::PUSH_STR; break;
            case 3: op = OpCode::PUSH_BOOL; break;
            default: op = OpCode::PUSH_INT; break;
        }

        out.push_back({ op, addConstant(chunk, value) });
    }

    Optimizer::Stats Optimizer::optimize(BytecodeChunk& chunk) {
        Stats total;

        Stats mainStats = this->optimizeChunk(chunk);
        total.folded += mainStats.folded;
        total.removed += mainStats.removed;

        for (auto& body : chunk.methodBodies) {
            Stats bodyStats = this->optimizeChunk(body);
            total.folded += bodyStats.folded;
            total.removed += bodyStats.removed;
        }

        return total;
    }

    Optimizer::Stats Optimizer::optimizeChunk(BytecodeChunk& chunk) {
        Stats stats;

        const auto& code = chunk.code;
        if (code.empty())
            return stats;

        std::unordered_set<int> targets;
        for (size_t i = 0; i < code.size(); ++i) {
            if (isJump(code[i].op)) {
                int target = static_cast<int>(i) + 1 + code[i].operand;
                if (target >= 0 && target < static_cast<int>(code.size()))
                    targets.insert(target);
            }
        }

        std::vector<Instruction> foldedCode;
        std::vector<int> newToOld;
        std::vector<int> oldToNew(code.size(), -1);
        std::vector<bool> dropped(code.size(), false);
        std::vector<StackEntry> stack{ std::monostate{} };

        for (size_t i = 0; i < code.size(); ++i) {
            const auto& instr = code[i];
            const int oldIdx = static_cast<int>(i);
            int emittedAt = -1;

            if (targets.contains(oldIdx))
                stack.assign(1, std::monostate{});

            switch (instr.op) {
                case OpCode::PUSH_INT:
                case OpCode::PUSH_FLOAT:
                case OpCode::PUSH_STR:
                case OpCode::PUSH_BOOL:
                    emittedAt = static_cast<int>(foldedCode.size());
                    foldedCode.push_back(instr);
                    stack.emplace_back(TrackedValue{
                        chunk.constants[instr.operand], emittedAt, !targets.contains(oldIdx)
                    });
                    
                    break;

                case OpCode::POP: {
                    if (!targets.contains(oldIdx) && stack.size() > 1 &&
                        isKnown(stack.back()) && knownValue(stack.back()).removable) {
                        dropped[knownValue(stack.back()).producer] = true;
                        stack.pop_back();
                        stats.removed++;
                    } else {
                        popEntry(stack);
                        emittedAt = static_cast<int>(foldedCode.size());
                        foldedCode.push_back(instr);
                    }

                    break;
                }

                case OpCode::DUP: {
                    StackEntry top = stack.back();
                    if (auto* known = std::get_if<TrackedValue>(&top))
                        known->removable = false;

                    stack.push_back(top);
                    emittedAt = static_cast<int>(foldedCode.size());
                    foldedCode.push_back(instr);

                    break;
                }

                case OpCode::ADD:
                case OpCode::SUB:
                case OpCode::MUL:
                case OpCode::DIV:
                case OpCode::MOD:
                case OpCode::POW: {
                    StackEntry right = popEntry(stack);
                    StackEntry left = popEntry(stack);

                    if (isKnown(left) && isKnown(right) &&
                        knownValue(left).removable && knownValue(right).removable) {
                        dropped[knownValue(left).producer] = true;
                        dropped[knownValue(right).producer] = true;

                        DiagnosticEngine foldDiag;
                        ValueNode::ValueType result = VM::applyArithmetic(
                            knownValue(left).value, knownValue(right).value, arithmeticOpName(instr.op), foldDiag);

                        if (!foldDiag.hasErrors()) {
                            emitPush(chunk, foldedCode, result);
                            emittedAt = static_cast<int>(foldedCode.size()) - 1;
                            stack.emplace_back(TrackedValue{ result, emittedAt, !targets.contains(oldIdx) });
                            stats.folded++;
                        } else {
                            emittedAt = static_cast<int>(foldedCode.size());
                            foldedCode.push_back(instr);
                            stack.emplace_back(std::monostate{});
                        }
                    } else {
                        emittedAt = static_cast<int>(foldedCode.size());
                        foldedCode.push_back(instr);
                        stack.emplace_back(std::monostate{});
                    }

                    break;
                }

                case OpCode::CMP_EQ:
                case OpCode::CMP_NE:
                case OpCode::CMP_GT:
                case OpCode::CMP_LT:
                case OpCode::CMP_GE:
                case OpCode::CMP_LE: {
                    StackEntry right = popEntry(stack);
                    StackEntry left = popEntry(stack);

                    if (isKnown(left) && isKnown(right) &&
                        knownValue(left).removable && knownValue(right).removable) {
                        dropped[knownValue(left).producer] = true;
                        dropped[knownValue(right).producer] = true;

                        DiagnosticEngine foldDiag;
                        bool result = VM::applyComparison(
                            knownValue(left).value, knownValue(right).value, comparisonOpName(instr.op), foldDiag);

                        if (!foldDiag.hasErrors()) {
                            emitPush(chunk, foldedCode, result);
                            emittedAt = static_cast<int>(foldedCode.size()) - 1;
                            stack.emplace_back(TrackedValue{ result, emittedAt, !targets.contains(oldIdx) });
                            stats.folded++;
                        } else {
                            emittedAt = static_cast<int>(foldedCode.size());
                            foldedCode.push_back(instr);
                            stack.emplace_back(std::monostate{});
                        }
                    } else {
                        emittedAt = static_cast<int>(foldedCode.size());
                        foldedCode.push_back(instr);
                        stack.emplace_back(std::monostate{});
                    }

                    break;
                }

                case OpCode::LOGIC_AND:
                case OpCode::LOGIC_OR: {
                    StackEntry right = popEntry(stack);
                    StackEntry left = popEntry(stack);

                    if (isKnown(left) && isKnown(right) &&
                        knownValue(left).removable && knownValue(right).removable) {
                        dropped[knownValue(left).producer] = true;
                        dropped[knownValue(right).producer] = true;

                        bool l = VM::valueToBool(knownValue(left).value);
                        bool r = VM::valueToBool(knownValue(right).value);
                        bool result = (instr.op == OpCode::LOGIC_AND) ? (l && r) : (l || r);
                        emitPush(chunk, foldedCode, result);
                        emittedAt = static_cast<int>(foldedCode.size()) - 1;
                        stack.emplace_back(TrackedValue{ result, emittedAt, !targets.contains(oldIdx) });
                        stats.folded++;
                    } else {
                        emittedAt = static_cast<int>(foldedCode.size());
                        foldedCode.push_back(instr);
                        stack.emplace_back(std::monostate{});
                    }
                    break;
                }

                case OpCode::MAKE_ARRAY: {
                    int count = instr.operand;
                    bool foldable = count == 0 || static_cast<int>(stack.size()) > count;
                    std::vector<int> producers;
                    std::vector<ValueNode::ValueType> elements;

                    if (foldable && count > 0) {
                        elements.resize(count);
                        producers.resize(count);

                        for (int i = count - 1; i >= 0; --i) {
                            StackEntry entry = popEntry(stack);
                            if (!isKnown(entry) || !knownValue(entry).removable) {
                                foldable = false;
                                break;
                            }

                            elements[i] = knownValue(entry).value;
                            producers[i] = knownValue(entry).producer;
                        }
                    }

                    if (foldable) {
                        for (int producer : producers)
                            dropped[producer] = true;

                        auto arr = std::make_shared<ArrayValue>();
                        arr->elements = std::move(elements);

                        emitPush(chunk, foldedCode, arr);
                        emittedAt = static_cast<int>(foldedCode.size()) - 1;
                        stack.emplace_back(TrackedValue{ arr, emittedAt, !targets.contains(oldIdx) });
                        stats.folded++;
                    } else {
                        emittedAt = static_cast<int>(foldedCode.size());
                        foldedCode.push_back(instr);
                        stack.assign(1, std::monostate{});
                    }

                    break;
                }

                case OpCode::LOAD_INDEX: {
                    StackEntry indexEntry = popEntry(stack);
                    StackEntry targetEntry = popEntry(stack);

                    bool foldable = isKnown(indexEntry) && isKnown(targetEntry) &&
                        knownValue(indexEntry).removable && knownValue(targetEntry).removable &&
                        std::holds_alternative<int>(knownValue(indexEntry).value) &&
                        std::holds_alternative<ArrayRef>(knownValue(targetEntry).value);

                    int index = 0;
                    if (foldable) {
                        index = std::get<int>(knownValue(indexEntry).value);
                        const auto& target = std::get<ArrayRef>(knownValue(targetEntry).value);
                        foldable = index >= 0 && index < static_cast<int>(target->elements.size()) &&
                            !std::holds_alternative<ArrayRef>(target->elements[index]);
                    }

                    if (foldable) {
                        dropped[knownValue(indexEntry).producer] = true;
                        dropped[knownValue(targetEntry).producer] = true;

                        const auto& element =
                            std::get<ArrayRef>(knownValue(targetEntry).value)->elements[index];
                        emitPush(chunk, foldedCode, element);
                        emittedAt = static_cast<int>(foldedCode.size()) - 1;
                        stack.emplace_back(TrackedValue{ element, emittedAt, !targets.contains(oldIdx) });
                        stats.folded++;
                    } else {
                        emittedAt = static_cast<int>(foldedCode.size());
                        foldedCode.push_back(instr);
                        stack.emplace_back(std::monostate{});
                    }

                    break;
                }

                case OpCode::STORE_INDEX: {
                    popEntry(stack);
                    popEntry(stack);
                    popEntry(stack);

                    emittedAt = static_cast<int>(foldedCode.size());
                    foldedCode.push_back(instr);
                    break;
                }

                case OpCode::NEG:
                case OpCode::NOT: {
                    StackEntry operand = popEntry(stack);

                    if (isKnown(operand) && knownValue(operand).removable) {
                        dropped[knownValue(operand).producer] = true;

                        if (instr.op == OpCode::NEG) {
                            DiagnosticEngine foldDiag;
                            ValueNode::ValueType result = VM::applyUnary(knownValue(operand).value, "-", foldDiag);

                            if (!foldDiag.hasErrors()) {
                                emitPush(chunk, foldedCode, result);
                                emittedAt = static_cast<int>(foldedCode.size()) - 1;
                                stack.emplace_back(TrackedValue{ result, emittedAt, !targets.contains(oldIdx) });
                            } else {
                                emittedAt = static_cast<int>(foldedCode.size());
                                foldedCode.push_back(instr);
                                stack.emplace_back(std::monostate{});
                            }
                        } else {
                            bool result = !VM::valueToBool(knownValue(operand).value);
                            emitPush(chunk, foldedCode, result);
                            emittedAt = static_cast<int>(foldedCode.size()) - 1;
                            stack.emplace_back(TrackedValue{ result, emittedAt, !targets.contains(oldIdx) });
                        }
                        stats.folded++;
                    } else {
                        emittedAt = static_cast<int>(foldedCode.size());
                        foldedCode.push_back(instr);
                        stack.emplace_back(std::monostate{});
                    }
                    break;
                }

                case OpCode::JMP_IF_FALSE:
                case OpCode::JMP_IF_TRUE: {
                    StackEntry cond = popEntry(stack);

                    if (!targets.contains(oldIdx) && isKnown(cond) && knownValue(cond).removable) {
                        dropped[knownValue(cond).producer] = true;

                        bool value = VM::valueToBool(knownValue(cond).value);
                        bool alwaysJump = (instr.op == OpCode::JMP_IF_FALSE) ? !value : value;

                        if (alwaysJump) {
                            emittedAt = static_cast<int>(foldedCode.size());
                            foldedCode.push_back({ OpCode::JMP, instr.operand });
                        } else {
                            stats.removed++;
                        }
                        
                        stats.folded++;
                    } else {
                        emittedAt = static_cast<int>(foldedCode.size());
                        foldedCode.push_back(instr);
                    }

                    stack.assign(1, std::monostate{});
                    break;
                }

                case OpCode::JMP:
                case OpCode::RETURN:
                case OpCode::HALT:
                default:
                    emittedAt = static_cast<int>(foldedCode.size());
                    foldedCode.push_back(instr);
                    stack.assign(1, std::monostate{});
                    break;
            }

            if (emittedAt >= 0) {
                oldToNew[oldIdx] = emittedAt;
                newToOld.push_back(oldIdx);
            }
        }

        {
            std::vector<Instruction> compactCode;
            std::vector<int> compactNewToOld;
            std::vector<int> compactOldToNew(code.size(), -1);

            for (size_t j = 0; j < foldedCode.size(); ++j) {
                if (dropped[j])
                    continue;

                compactOldToNew[newToOld[j]] = static_cast<int>(compactCode.size());
                compactCode.push_back(foldedCode[j]);
                compactNewToOld.push_back(newToOld[j]);
            }

            foldedCode = std::move(compactCode);
            newToOld = std::move(compactNewToOld);
            oldToNew = std::move(compactOldToNew);
        }

        auto foldedTargetOf = [&](int j) -> int {
            if (j < 0 || j >= static_cast<int>(newToOld.size()))
                return -1;

            int oldTarget = newToOld[j] + 1 + foldedCode[j].operand;
            if (oldTarget < 0 || oldTarget >= static_cast<int>(oldToNew.size()))
                return -1;

            return oldToNew[oldTarget];
        };

        auto resolveTarget = [&](int j) -> int {
            int target = foldedTargetOf(j);
            int guard = 0;

            while (target >= 0 && target < static_cast<int>(foldedCode.size()) &&
                   foldedCode[target].op == OpCode::JMP) {
                if (++guard > static_cast<int>(foldedCode.size()))
                    break;

                int next = foldedTargetOf(target);
                if (next == target || next < 0)
                    break;

                target = next;
            }

            return target;
        };

        std::vector<bool> reachable(foldedCode.size(), false);
        std::vector<int> queue;
        reachable[0] = true;
        queue.push_back(0);

        while (!queue.empty()) {
            int j = queue.back();
            queue.pop_back();

            auto mark = [&](int target) {
                if (target >= 0 && target < static_cast<int>(foldedCode.size()) && !reachable[target]) {
                    reachable[target] = true;
                    queue.push_back(target);
                }
            };

            if (isJump(foldedCode[j].op))
                mark(resolveTarget(j));

            if (!isTerminator(foldedCode[j].op))
                mark(j + 1);
        }

        std::vector<Instruction> finalCode;
        std::vector<int> finalToFolded;
        std::vector<int> foldedToFinal(foldedCode.size(), -1);

        for (size_t j = 0; j < foldedCode.size(); ++j) {
            if (!reachable[j]) {
                stats.removed++;
                continue;
            }

            if (isJump(foldedCode[j].op) && resolveTarget(static_cast<int>(j)) == static_cast<int>(j) + 1) {
                stats.removed++;
                continue;
            }

            foldedToFinal[j] = static_cast<int>(finalCode.size());
            finalToFolded.push_back(static_cast<int>(j));
            finalCode.push_back(foldedCode[j]);
        }

        for (size_t f = 0; f < finalCode.size(); ++f) {
            auto& instr = finalCode[f];
            if (!isJump(instr.op))
                continue;

            int foldedIdx = finalToFolded[f];
            int target = resolveTarget(foldedIdx);
            int finalTarget = (target >= 0 && target < static_cast<int>(foldedToFinal.size()))
                ? foldedToFinal[target] : static_cast<int>(f) + 1;

            instr.operand = finalTarget - static_cast<int>(f) - 1;
        }

        chunk.code = std::move(finalCode);
        return stats;
    }
}
