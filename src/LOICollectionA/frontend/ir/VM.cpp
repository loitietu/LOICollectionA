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
            else if constexpr (std::is_same_v<std::remove_cv_t<T>, ObjectRef>)
                return "instance of " + arg->className;
            else if constexpr (std::is_same_v<std::remove_cv_t<T>, FunctionRefPtr>)
                return "function";
        }, val);
    }

    ValueNode::ValueType VM::applyArithmetic(const ValueNode::ValueType& left, const ValueNode::ValueType& right, const std::string& op, DiagnosticEngine& diagnostics) {
        return std::visit([&op, &diagnostics](auto&& l, auto&& r) -> ValueNode::ValueType {
            using T = std::decay_t<decltype(l)>;
            using U = std::decay_t<decltype(r)>;

            if constexpr (std::is_arithmetic_v<T> && std::is_arithmetic_v<U>) {
                auto dl = static_cast<double>(l);
                auto dr = static_cast<double>(r);

                if (op == "+") {
                    if constexpr (std::is_integral_v<T> && std::is_integral_v<U>)
                        return l + r;
                    return static_cast<float>(dl + dr);
                }
                if (op == "-") {
                    if constexpr (std::is_integral_v<T> && std::is_integral_v<U>)
                        return l - r;
                    return static_cast<float>(dl - dr);
                }
                if (op == "*") {
                    if constexpr (std::is_integral_v<T> && std::is_integral_v<U>)
                        return l * r;
                    return static_cast<float>(dl * dr);
                }
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
            else if constexpr (std::is_same_v<std::remove_cv_t<T>, ObjectRef>)
                return true;
            else if constexpr (std::is_same_v<std::remove_cv_t<T>, FunctionRefPtr>)
                return true;
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
            } else if constexpr (std::is_same_v<T, ObjectRef> && std::is_same_v<U, ObjectRef>) {
                if (op == "==") return l == r;
                if (op == "!=") return l != r;

                diagnostics.addError({ 0, 0, 0 }, "Unknown comparison op: " + op);
                return false;
            } else if constexpr (std::is_same_v<T, FunctionRefPtr> && std::is_same_v<U, FunctionRefPtr>) {
                if (op == "==") return l == r;
                if (op == "!=") return l != r;

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

    bool VM::isDerived(const BytecodeChunk& chunk, int derivedClassIndex, int baseClassIndex) const {
        int current = derivedClassIndex;
        while (current >= 0 && current < static_cast<int>(chunk.classes.size())) {
            if (current == baseClassIndex)
                return true;

            current = chunk.classes[current].baseClassIndex;
        }

        return false;
    }

    ValueNode::ValueType VM::run(const BytecodeChunk& chunk, const Context& ctx, DiagnosticEngine& diagnostics) {
        this->stack.clear();
        this->frames.clear();
        this->variables.clear();

        this->frames.emplace_back(chunk);

        size_t executed = 0;
        while (true) {
            if (++executed > 1'000'000) {
                diagnostics.addError({ 0, 0, 0 }, "Instruction limit exceeded (possible infinite loop)");
                return ValueNode::ValueType{};
            }

            Frame& frame = this->frames.back();
            const BytecodeChunk& cur = frame.chunk.get();

            if (frame.ip >= cur.code.size()) {
                diagnostics.addError({ 0, 0, 0 }, "Invalid instruction pointer");
                return ValueNode::ValueType{};
            }

            const auto& instr = cur.code[frame.ip++];
            switch (instr.op) {
                case OpCode::PUSH_INT:
                case OpCode::PUSH_FLOAT:
                case OpCode::PUSH_STR:
                case OpCode::PUSH_BOOL:
                    this->push(cur.constants[instr.operand]);
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
                    const auto& name = std::get<std::string>(cur.constants[instr.operand]);

                    auto localIt = frame.locals.find(name);
                    if (localIt != frame.locals.end()) {
                        this->push(localIt->second);
                        break;
                    }

                    if (frame.hasThis) {
                        auto obj = std::get<ObjectRef>(frame.thisObj);

                        if (obj->classIndex >= 0) {
                            const auto& cls = chunk.classes[obj->classIndex];
                            bool isField = std::ranges::find(cls.fieldNames, name) != cls.fieldNames.end();

                            if (isField) {
                                auto fieldIt = obj->fields.find(name);
                                if (fieldIt == obj->fields.end()) {
                                    diagnostics.addError({ 0, 0, 0 }, "Object has no field: " + name);
                                    break;
                                }

                                this->push(fieldIt->second);
                                break;
                            }
                        }

                        auto existingIt = obj->fields.find(name);
                        if (existingIt != obj->fields.end()) {
                            this->push(existingIt->second);
                            break;
                        }
                    }

                    auto globalIt = this->variables.find(name);
                    if (globalIt == this->variables.end()) {
                        diagnostics.addError({ 0, 0, 0 }, "Undefined variable: " + name);
                        break;
                    }

                    this->push(globalIt->second);
                    break;
                }
                case OpCode::STORE_VAR: {
                    const auto& name = std::get<std::string>(cur.constants[instr.operand]);

                    auto val = this->pop(diagnostics);

                    auto localIt = frame.locals.find(name);
                    if (localIt != frame.locals.end()) {
                        localIt->second = val;
                        break;
                    }

                    if (frame.hasThis) {
                        auto obj = std::get<ObjectRef>(frame.thisObj);

                        if (obj->classIndex >= 0) {
                            const auto& cls = chunk.classes[obj->classIndex];
                            bool isField = std::ranges::find(cls.fieldNames, name) != cls.fieldNames.end();

                            if (isField) {
                                obj->fields[name] = val;
                                break;
                            }
                        }

                        auto existingIt = obj->fields.find(name);
                        if (existingIt != obj->fields.end()) {
                            existingIt->second = val;
                            break;
                        }
                    }

                    this->variables[name] = val;
                    break;
                }

                case OpCode::LOAD_FIELD: {
                    const auto& name = std::get<std::string>(cur.constants[instr.operand]);
                    auto objValue = this->pop(diagnostics);

                    if (!std::holds_alternative<ObjectRef>(objValue)) {
                        diagnostics.addError({ 0, 0, 0 }, "Cannot load field '" + name + "' from a non-object value");
                        break;
                    }

                    auto obj = std::get<ObjectRef>(objValue);
                    auto it = obj->fields.find(name);
                    if (it == obj->fields.end()) {
                        diagnostics.addError({ 0, 0, 0 }, "Object has no field: " + name);
                        break;
                    }

                    this->push(it->second);
                    break;
                }
                case OpCode::STORE_FIELD: {
                    const auto& name = std::get<std::string>(cur.constants[instr.operand]);
                    auto objValue = this->pop(diagnostics);
                    auto val = this->pop(diagnostics);

                    if (!std::holds_alternative<ObjectRef>(objValue)) {
                        diagnostics.addError({ 0, 0, 0 }, "Cannot store field '" + name + "' on a non-object value");
                        break;
                    }

                    std::get<ObjectRef>(objValue)->fields[name] = val;
                    break;
                }
                case OpCode::LOAD_THIS: {
                    if (!frame.hasThis) {
                        diagnostics.addError({ 0, 0, 0 }, "'this' is not available in the current context");
                        break;
                    }

                    this->push(frame.thisObj);
                    break;
                }
                case OpCode::MAKE_LAMBDA: {
                    const auto& meta = cur.lambdas[instr.operand];

                    auto func = std::make_shared<FunctionRef>();
                    func->bodyIndex = meta.bodyIndex;
                    func->argCount = meta.argCount;
                    func->paramNames = meta.paramNames;
                    func->hasThis = frame.hasThis;
                    if (frame.hasThis)
                        func->thisObj = std::get<ObjectRef>(frame.thisObj);
                    func->captures = frame.locals;

                    this->push(func);
                    break;
                }
                case OpCode::INSTANCEOF: {
                    const auto& name = std::get<std::string>(cur.constants[instr.operand]);
                    auto value = this->pop(diagnostics);

                    if (!std::holds_alternative<ObjectRef>(value)) {
                        this->push(false);
                        break;
                    }

                    auto obj = std::get<ObjectRef>(value);
                    bool result = (obj->className == name);

                    if (!result && obj->classIndex >= 0) {
                        int targetIdx = -1;
                        for (size_t i = 0; i < chunk.classes.size(); ++i) {
                            if (chunk.classes[i].name == name) {
                                targetIdx = static_cast<int>(i);
                                break;
                            }
                        }

                        if (targetIdx >= 0)
                            result = this->isDerived(chunk, obj->classIndex, targetIdx);
                    }

                    this->push(result);
                    break;
                }

                case OpCode::NEW: {
                    const auto& cls = chunk.classes[instr.operand];

                    std::vector<ValueNode::ValueType> args;

                    if (cls.constructorIndex != -1) {
                        const auto& ctor = chunk.methods[cls.constructorIndex];
                        args.resize(ctor.argCount);

                        for (int i = 0; i < ctor.argCount; ++i)
                            args[ctor.argCount - 1 - i] = this->pop(diagnostics);
                    }

                    auto obj = std::make_shared<Object>();
                    obj->className = cls.name;
                    obj->classIndex = instr.operand;

                    for (size_t i = 0; i < cls.fieldNames.size(); ++i) {
                        if (cls.hasDefault[i])
                            obj->fields[cls.fieldNames[i]] = cls.defaults[i];
                    }

                    if (cls.constructorIndex != -1) {
                        const auto& ctor = chunk.methods[cls.constructorIndex];

                        Frame callee(chunk.methodBodies[ctor.bodyIndex]);
                        callee.hasThis = true;
                        callee.thisObj = obj;
                        callee.hasPending = true;
                        callee.pendingPush = obj;

                        for (int i = 0; i < ctor.argCount; ++i)
                            callee.locals[ctor.paramNames[i]] = args[i];

                        this->frames.push_back(std::move(callee));
                    } else {
                        this->push(obj);
                    }
                    break;
                }

                case OpCode::NEW_NATIVE: {
                    const auto& meta = chunk.nativeCalls[instr.operand];

                    std::vector<ValueNode::ValueType> args(meta.argCount);
                    for (int i = 0; i < meta.argCount; ++i)
                        args[meta.argCount - 1 - i] = this->pop(diagnostics);

                    auto result = ClassCall::getInstance().create(
                        meta.className, args, ctx.params, diagnostics
                    );

                    if (!result.has_value()) {
                        diagnostics.addError({ 0, 0, 0 },
                            "Failed to create native class '" + meta.className + "': " + result.error().message());
                        break;
                    }

                    this->push(result.value());
                    break;
                }

                case OpCode::CALL_METHOD: {
                    const auto& meta = chunk.methods[instr.operand];

                    auto receiver = this->pop(diagnostics);
                    if (!std::holds_alternative<ObjectRef>(receiver)) {
                        diagnostics.addError({ 0, 0, 0 }, "Method call target is not an object");
                        break;
                    }

                    auto obj = std::get<ObjectRef>(receiver);
                    if (!this->isDerived(chunk, obj->classIndex, meta.classIndex)) {
                        diagnostics.addError({ 0, 0, 0 },
                            "Method '" + meta.name + "' does not belong to this object");
                        break;
                    }

                    std::vector<ValueNode::ValueType> args(meta.argCount);
                    for (int i = 0; i < meta.argCount; ++i)
                        args[meta.argCount - 1 - i] = this->pop(diagnostics);

                    Frame callee(chunk.methodBodies[meta.bodyIndex]);
                    callee.hasThis = true;
                    callee.thisObj = receiver;

                    for (int i = 0; i < meta.argCount; ++i)
                        callee.locals[meta.paramNames[i]] = args[i];

                    this->frames.push_back(std::move(callee));
                    break;
                }

                case OpCode::CALL_METHOD_VIRTUAL: {
                    const auto& meta = cur.virtualCalls[instr.operand];

                    auto receiver = this->pop(diagnostics);
                    if (!std::holds_alternative<ObjectRef>(receiver)) {
                        diagnostics.addError({ 0, 0, 0 }, "Method call target is not an object");
                        break;
                    }

                    auto obj = std::get<ObjectRef>(receiver);
                    if (obj->classIndex < 0 || obj->classIndex >= static_cast<int>(chunk.classes.size())) {
                        diagnostics.addError({ 0, 0, 0 }, "Invalid object class index");
                        break;
                    }

                    if (!this->isDerived(chunk, obj->classIndex, meta.classIndex)) {
                        diagnostics.addError({ 0, 0, 0 }, "Method call target is not an instance of the expected class");
                        break;
                    }

                    const auto& actualClass = chunk.classes[obj->classIndex];
                    if (meta.ordinal < 0 || meta.ordinal >= static_cast<int>(actualClass.methods.size())) {
                        diagnostics.addError({ 0, 0, 0 }, "Invalid method ordinal");
                        break;
                    }

                    const auto& method = chunk.methods[actualClass.methods[meta.ordinal]];
                    if (method.argCount != meta.argCount) {
                        diagnostics.addError({ 0, 0, 0 },
                            "Method '" + method.name + "' expects " + std::to_string(method.argCount) +
                            " argument(s), got " + std::to_string(meta.argCount));
                        break;
                    }

                    std::vector<ValueNode::ValueType> args(method.argCount);
                    for (int i = 0; i < method.argCount; ++i)
                        args[method.argCount - 1 - i] = this->pop(diagnostics);

                    Frame callee(chunk.methodBodies[method.bodyIndex]);
                    callee.hasThis = true;
                    callee.thisObj = receiver;

                    for (int i = 0; i < method.argCount; ++i)
                        callee.locals[method.paramNames[i]] = args[i];

                    this->frames.push_back(std::move(callee));
                    break;
                }

                case OpCode::CALL_SUPER_CTOR: {
                    const auto& meta = cur.superCalls[instr.operand];

                    auto receiver = this->pop(diagnostics);
                    if (!std::holds_alternative<ObjectRef>(receiver)) {
                        diagnostics.addError({ 0, 0, 0 }, "Super constructor call target is not an object");
                        break;
                    }

                    if (meta.constructorIndex < 0) {
                        if (meta.argCount != 0) {
                            diagnostics.addError({ 0, 0, 0 }, "Base class has no constructor");
                        } else {
                            this->push(std::string(""));
                        }
                        break;
                    }

                    const auto& ctor = chunk.methods[meta.constructorIndex];
                    if (ctor.argCount != meta.argCount) {
                        diagnostics.addError({ 0, 0, 0 },
                            "Base constructor expects " + std::to_string(ctor.argCount) +
                            " argument(s), got " + std::to_string(meta.argCount));
                        break;
                    }

                    std::vector<ValueNode::ValueType> args(ctor.argCount);
                    for (int i = 0; i < ctor.argCount; ++i)
                        args[ctor.argCount - 1 - i] = this->pop(diagnostics);

                    Frame callee(chunk.methodBodies[ctor.bodyIndex]);
                    callee.hasThis = true;
                    callee.thisObj = receiver;

                    for (int i = 0; i < ctor.argCount; ++i)
                        callee.locals[ctor.paramNames[i]] = args[i];

                    this->frames.push_back(std::move(callee));
                    break;
                }

                case OpCode::CALL_NATIVE_METHOD: {
                    const auto& meta = chunk.nativeCalls[instr.operand];

                    auto receiver = this->pop(diagnostics);
                    if (!std::holds_alternative<ObjectRef>(receiver)) {
                        diagnostics.addError({ 0, 0, 0 }, "Native method call target is not an object");
                        break;
                    }

                    auto obj = std::get<ObjectRef>(receiver);

                    std::vector<ValueNode::ValueType> args(meta.argCount);
                    for (int i = 0; i < meta.argCount; ++i)
                        args[meta.argCount - 1 - i] = this->pop(diagnostics);

                    auto result = ClassCall::getInstance().callMethod(
                        obj->className, meta.name, args, obj, ctx.params, diagnostics
                    );

                    if (!result.has_value()) {
                        diagnostics.addError({ 0, 0, 0 },
                            "Native method call failed '" + meta.className + "::" + meta.name +
                            "': " + result.error().message());
                        break;
                    }

                    this->push(result.value());
                    break;
                }

                case OpCode::CALL_FUNC: {
                    const auto& meta = chunk.methods[instr.operand];

                    std::vector<ValueNode::ValueType> args(meta.argCount);
                    for (int i = 0; i < meta.argCount; ++i)
                        args[meta.argCount - 1 - i] = this->pop(diagnostics);

                    Frame callee(chunk.methodBodies[meta.bodyIndex]);
                    callee.hasThis = false;

                    for (int i = 0; i < meta.argCount; ++i)
                        callee.locals[meta.paramNames[i]] = args[i];

                    this->frames.push_back(std::move(callee));
                    break;
                }

                case OpCode::CALL_LAMBDA: {
                    auto funcValue = this->pop(diagnostics);
                    if (!std::holds_alternative<FunctionRefPtr>(funcValue)) {
                        diagnostics.addError({ 0, 0, 0 }, "Attempted to call a non-function value");
                        break;
                    }

                    auto func = std::get<FunctionRefPtr>(funcValue);
                    if (instr.operand != func->argCount) {
                        diagnostics.addError({ 0, 0, 0 },
                            "Function expects " + std::to_string(func->argCount) +
                            " argument(s), got " + std::to_string(instr.operand));
                        break;
                    }

                    if (func->bodyIndex < 0 ||
                        func->bodyIndex >= static_cast<int>(chunk.methodBodies.size())) {
                        diagnostics.addError({ 0, 0, 0 }, "Invalid function body index");
                        break;
                    }

                    std::vector<ValueNode::ValueType> args(func->argCount);
                    for (int i = 0; i < func->argCount; ++i)
                        args[func->argCount - 1 - i] = this->pop(diagnostics);

                    Frame callee(chunk.methodBodies[func->bodyIndex]);
                    callee.hasThis = func->hasThis;
                    if (func->hasThis)
                        callee.thisObj = func->thisObj;
                    callee.locals = func->captures;

                    for (int i = 0; i < func->argCount; ++i)
                        callee.locals[func->paramNames[i]] = args[i];

                    this->frames.push_back(std::move(callee));
                    break;
                }

                case OpCode::RETURN: {
                    auto result = this->pop(diagnostics);

                    Frame finished = std::move(this->frames.back());
                    this->frames.pop_back();

                    if (this->frames.empty())
                        return result;

                    if (finished.hasPending)
                        this->push(finished.pendingPush);
                    else
                        this->push(result);

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
                    const auto& meta = cur.functions[instr.operand];

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
                    const auto& meta = cur.macros[instr.operand];

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
                        frame.ip += instr.operand;

                    break;
                }
                case OpCode::JMP_IF_TRUE: {
                    auto cond = this->pop(diagnostics);
                    if (VM::valueToBool(cond))
                        frame.ip += instr.operand;
                    
                    break;
                }
                case OpCode::JMP:
                    frame.ip += instr.operand;
                    break;

                case OpCode::HALT: {
                    if (this->stack.empty()) {
                        return std::string("");
                    }

                    return this->stack.back();
                }
            }
        }
    }
}
