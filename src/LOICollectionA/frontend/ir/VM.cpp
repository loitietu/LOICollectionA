#include <cmath>
#include <ranges>
#include <string>
#include <vector>
#include <algorithm>

#include <ll/api/Expected.h>

#include "LOICollectionA/frontend/Callback.h"

#include "LOICollectionA/utils/core/MathUtils.h"

#include "LOICollectionA/frontend/ir/VM.h"

namespace LOICollection::frontend::ir {
    std::string VM::valueToString(const ValueNode::ValueType& val) {
        return std::visit([](auto&& arg) -> std::string {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<std::remove_cv_t<T>, int>)
                return std::to_string(arg);
            else if constexpr (std::is_same_v<std::remove_cv_t<T>, float>) {
                std::string result = std::to_string(arg);

                result.erase(result.find_last_not_of('0') + 1, std::string::npos);
                if (result.back() == '.')
                    result.pop_back();
                
                return result;
            }
            else if constexpr (std::is_same_v<std::remove_cv_t<T>, std::string>)
                return arg;
            else if constexpr (std::is_same_v<std::remove_cv_t<T>, bool>)
                return arg ? "true" : "false";
        }, val);
    }

    ValueNode::ValueType VM::applyArithmetic(const ValueNode::ValueType& left, const ValueNode::ValueType& right, const std::string& op, DiagnosticEngine& diagnostics) {
        return std::visit([&op, &diagnostics](auto&& l, auto&& r) -> ValueNode::ValueType {
            using T = std::decay_t<decltype(l)>;
            using U = std::decay_t<decltype(r)>;

            if constexpr (std::is_arithmetic_v<T> && std::is_arithmetic_v<U>) {
                auto dl = static_cast<double>(l);
                auto dr = static_cast<double>(r);

                if (op == "+") return static_cast<float>(dl + dr);
                if (op == "-") return static_cast<float>(dl - dr);
                if (op == "*") return static_cast<float>(dl * dr);
                if (op == "/") return static_cast<float>(dl / dr);
                if (op == "^") return static_cast<float>(MathUtils::pow(dl, dr));
                if (op == "%") {
                    if constexpr (std::is_integral_v<T> && std::is_integral_v<U>)
                        return l % r;
                        
                    diagnostics.addError({ 0, 0, 0 }, "Modulo requires integral types");
                    return 0;
                }

                diagnostics.addError({ 0, 0, 0 }, "Unknown arithmetic op: " + op);
                return 0;
            } else {
                if (op == "+") return VM::valueToString(l) + VM::valueToString(r);

                diagnostics.addError({ 0, 0, 0 }, "Type mismatch in arithmetic");
                return 0;
            }
        }, left, right);
    }

    ValueNode::ValueType VM::applyUnary(const ValueNode::ValueType& operand, const std::string& op, DiagnosticEngine& diagnostics) {
        return std::visit([&op, &diagnostics](auto&& arg) -> ValueNode::ValueType {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_arithmetic_v<T>) {
                if (op == "+") return arg;
                if (op == "-") return -arg;
            }
            if (op == "!") return !VM::valueToBool(arg);

            diagnostics.addError({ 0, 0, 0 }, "Unknown unary op: " + op);
            return 0;
        }, operand);
    }

    bool VM::valueToBool(const ValueNode::ValueType& val) {
        return std::visit([](auto&& arg) -> bool {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<std::remove_cv_t<T>, int>)
                return arg != 0;
            else if constexpr (std::is_same_v<std::remove_cv_t<T>, float>)
                return std::abs(arg) > std::numeric_limits<float>::epsilon();
            else if constexpr (std::is_same_v<std::remove_cv_t<T>, std::string>) {
                auto lower = arg | std::views::transform(::tolower) | std::ranges::to<std::string>();
                if (lower == "false") return false;
                if (lower == "true") return true;
                
                return !arg.empty();
            }
            else if constexpr (std::is_same_v<std::remove_cv_t<T>, bool>)
                return arg;
        }, val);
    }

    bool VM::applyComparison(const ValueNode::ValueType& left, const ValueNode::ValueType& right, const std::string& op, DiagnosticEngine& diagnostics) {
        return std::visit([&op, &diagnostics](auto&& l, auto&& r) -> bool {
            using T = std::decay_t<decltype(l)>;
            using U = std::decay_t<decltype(r)>;

            if constexpr (std::is_arithmetic_v<T> && std::is_arithmetic_v<U>) {
                auto cmp = static_cast<double>(l) <=> static_cast<double>(r);

                if (op == "==") return cmp == 0;
                if (op == "!=") return cmp != 0;
                if (op == ">") return cmp > 0;
                if (op == "<") return cmp < 0;
                if (op == ">=") return cmp >= 0;
                if (op == "<=") return cmp <= 0;

                diagnostics.addError({ 0, 0, 0 }, "Unknown comparison op: " + op);
                return false;
            } else if constexpr (std::is_same_v<T, U>) {
                auto cmp = l <=> r;

                if (op == "==") return cmp == 0;
                if (op == "!=") return cmp != 0;
                if (op == ">") return cmp > 0;
                if (op == "<") return cmp < 0;
                if (op == ">=") return cmp >= 0;
                if (op == "<=") return cmp <= 0;

                diagnostics.addError({ 0, 0, 0 }, "Unknown comparison op: " + op);
                return false;
            } else {
                diagnostics.addError({ 0, 0, 0 }, "Type mismatch in comparison");
                return false;
            }
        }, left, right);
    }

    void VM::push(const ValueNode::ValueType& v) {
        this->stack.push_back(v);
    }

    ValueNode::ValueType VM::pop(DiagnosticEngine& diagnostics) {
        if (this->stack.empty()) {
            diagnostics.addError({ 0, 0, 0 }, "Stack underflow");
            return ValueNode::ValueType{};
        }

        auto v = this->stack.back();
        this->stack.pop_back();

        return v;
    }

    ValueNode::ValueType VM::run(const BytecodeChunk& chunk, const Context& ctx, DiagnosticEngine& diagnostics) {
        this->ip = 0;
        this->stack.clear();

        size_t executed = 0;
        while (true) {
            if (++executed > 1'000'000) {
                diagnostics.addError({ 0, 0, 0 }, "Instruction limit exceeded (possible infinite loop)");
                return ValueNode::ValueType{};
            }

            const auto& instr = chunk.code[ip++];
            switch (instr.op) {
                case OpCode::PUSH_INT:
                case OpCode::PUSH_FLOAT:
                case OpCode::PUSH_STR:
                case OpCode::PUSH_BOOL:
                    this->push(chunk.constants[instr.operand]);
                    break;
                case OpCode::POP:
                    this->pop(diagnostics);
                    break;

                case OpCode::DUP: {
                    if (this->stack.empty()) {
                        diagnostics.addError({ 0, 0, 0 }, "Stack underflow during DUP");
                        break;
                    }

                    this->stack.push_back(this->stack.back());
                    break;
                }

                case OpCode::LOAD_VAR: {
                    const auto& name = std::get<std::string>(chunk.constants[instr.operand]);

                    auto it = this->variables.find(name);
                    if (it == this->variables.end()) {
                        diagnostics.addError({ 0, 0, 0 }, "Undefined variable: " + name);
                        break;
                    }

                    this->push(it->second);
                    break;
                }
                case OpCode::STORE_VAR: {
                    const auto& name = std::get<std::string>(chunk.constants[instr.operand]);

                    auto val = this->pop(diagnostics);

                    this->variables[name] = val;
                    break;
                }

                case OpCode::ADD: {
                    auto r = this->pop(diagnostics);
                    auto l = this->pop(diagnostics);

                    this->push(VM::applyArithmetic(l, r, "+", diagnostics));
                    break;
                }
                case OpCode::SUB: {
                    auto r = this->pop(diagnostics);
                    auto l = this->pop(diagnostics);

                    this->push(VM::applyArithmetic(l, r, "-", diagnostics));
                    break;
                }
                case OpCode::MUL: {
                    auto r = this->pop(diagnostics);
                    auto l = this->pop(diagnostics);

                    this->push(VM::applyArithmetic(l, r, "*", diagnostics));
                    break;
                }
                case OpCode::DIV: {
                    auto r = this->pop(diagnostics);
                    auto l = this->pop(diagnostics);

                    this->push(VM::applyArithmetic(l, r, "/", diagnostics));
                    break;
                }
                case OpCode::MOD: {
                    auto r = this->pop(diagnostics);
                    auto l = this->pop(diagnostics);

                    this->push(VM::applyArithmetic(l, r, "%", diagnostics));
                    break;
                }
                case OpCode::POW: {
                    auto r = this->pop(diagnostics);
                    auto l = this->pop(diagnostics);

                    this->push(VM::applyArithmetic(l, r, "^", diagnostics));
                    break;
                }

                case OpCode::CMP_EQ: {
                    auto r = this->pop(diagnostics);
                    auto l = this->pop(diagnostics);

                    this->push(VM::applyComparison(l, r, "==", diagnostics));
                    break;
                }
                case OpCode::CMP_NE: {
                    auto r = this->pop(diagnostics);
                    auto l = this->pop(diagnostics);

                    this->push(VM::applyComparison(l, r, "!=", diagnostics));
                    break;
                }
                case OpCode::CMP_GT: {
                    auto r = this->pop(diagnostics);
                    auto l = this->pop(diagnostics);

                    this->push(VM::applyComparison(l, r, ">", diagnostics));
                    break;
                }
                case OpCode::CMP_LT: {
                    auto r = this->pop(diagnostics);
                    auto l = this->pop(diagnostics);

                    this->push(VM::applyComparison(l, r, "<", diagnostics));
                    break;
                }
                case OpCode::CMP_GE: {
                    auto r = this->pop(diagnostics);
                    auto l = this->pop(diagnostics);

                    this->push(VM::applyComparison(l, r, ">=", diagnostics));
                    break;
                }
                case OpCode::CMP_LE: {
                    auto r = this->pop(diagnostics);
                    auto l = this->pop(diagnostics);

                    this->push(VM::applyComparison(l, r, "<=", diagnostics));
                    break;
                }

                case OpCode::LOGIC_AND: {
                    auto r = this->pop(diagnostics);
                    auto l = this->pop(diagnostics);

                    this->push(VM::valueToBool(l) && VM::valueToBool(r));
                    break;
                }
                case OpCode::LOGIC_OR: {
                    auto r = this->pop(diagnostics);
                    auto l = this->pop(diagnostics);

                    this->push(VM::valueToBool(l) || VM::valueToBool(r));
                    break;
                }

                case OpCode::NEG: {
                    auto v = this->pop(diagnostics);

                    this->push(VM::applyUnary(v, "-", diagnostics));
                    break;
                }
                case OpCode::NOT: {
                    auto v = this->pop(diagnostics);

                    this->push(!VM::valueToBool(v));
                    break;
                }

                case OpCode::CALL: {
                    const auto& meta = chunk.functions[instr.operand];

                    CallbackTypeValues args;
                    args.reserve(meta.argCount);
                    for (int i = 0; i < meta.argCount; ++i)
                        args.push_back(this->pop(diagnostics));

                    std::ranges::reverse(args);

                    auto ns = meta.name.substr(0, meta.name.find("::"));
                    auto func = meta.name.substr(meta.name.find("::") + 2);
                    auto result = FunctionCall::getInstance().callFunction(
                        ns, func, args, ctx.params, diagnostics
                    );
                    
                    if (!result.has_value()) {
                        diagnostics.addError({ 0, 0, 0 }, result.error().message());
                        break;
                    }

                    this->push(result.value());
                    break;
                }
                case OpCode::CALL_MACRO: {
                    const auto& meta = chunk.macros[instr.operand];

                    CallbackTypeValues args;
                    args.reserve(meta.argCount);
                    for (int i = 0; i < meta.argCount; ++i)
                        args.push_back(this->pop(diagnostics));

                    std::ranges::reverse(args);
                    
                    auto result = MacroCall::getInstance().callMacro(
                        meta.name, args, ctx.params, diagnostics
                    );

                    if (!result.has_value()) {
                        diagnostics.addError({ 0, 0, 0 }, result.error().message());
                        break;
                    }

                    this->push(result.value());
                    break;
                }

                case OpCode::JMP_IF_FALSE: {
                    auto cond = this->pop(diagnostics);
                    if (!VM::valueToBool(cond))
                        this->ip += instr.operand;

                    break;
                }
                case OpCode::JMP_IF_TRUE: {
                    auto cond = this->pop(diagnostics);
                    if (VM::valueToBool(cond))
                        this->ip += instr.operand;
                    
                    break;
                }
                case OpCode::JMP:
                    this->ip += instr.operand;
                    break;

                case OpCode::HALT:
                    return this->stack.back();
            }
        }
    }
}
