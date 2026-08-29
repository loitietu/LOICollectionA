#include <gtest/gtest.h>

#include "common/frontend/CommonTest.h"

using namespace LOICollection::frontend;

namespace {
    FunctionRefPtr functionRefOf(const std::string& input) {
        DiagnosticEngine diagnostics;
        auto chunk = compile(input, diagnostics);
        if (!chunk)
            throw std::runtime_error(diagnostics.getErrorMessage());

        ir::VM vm(diagnostics);
        auto result = vm.run(chunk, {});
        if (diagnostics.hasErrors())
            throw std::runtime_error(diagnostics.getErrorMessage());

        return std::get<FunctionRefPtr>(result);
    }
}

TEST(LambdaTest, AssignAndCall) {
    EXPECT_EQ(eval("f = func (a: int) -> int { return a + 1; }; f(1)"), "2");
}

TEST(LambdaTest, PassClassObject) {
    EXPECT_EQ(eval(
        "class Point { "
        "public: "
        "x = 0; "
        "y = 0; "
        "Point(x, y) { this.x = x; this.y = y; } "
        "} "
        "f = func (p: Point) -> int { return p.x + p.y; }; "
        "f(new Point(3, 4))"),
        "7");
}

TEST(LambdaTest, PassClassObjectUntyped) {
    EXPECT_EQ(eval(
        "class Point { "
        "public: "
        "x = 0; "
        "Point(x) { this.x = x; } "
        "} "
        "f = func (p) { return p.x; }; "
        "f(new Point(9))"),
        "9");
}

TEST(LambdaTest, PassAsArgument) {
    EXPECT_EQ(eval(
        "func apply(f, v: int) -> int { return f(v); } "
        "apply(func (x: int) -> int { return x * 2; }, 5)"),
        "10");
}

TEST(LambdaTest, CaptureOuterParameter) {
    EXPECT_EQ(eval(
        "func outer(a: int) -> int { "
        "f = func (b: int) -> int { return a + b; }; "
        "return f(10); "
        "} "
        "outer(5)"),
        "15");
}

TEST(LambdaTest, CaptureThis) {
    EXPECT_EQ(eval(
        "class A { "
        "public: "
        "x = 3; "
        "func make() -> int { "
        "f = func () -> int { return this.x; }; "
        "return f(); "
        "} "
        "} "
        "a = new A(); "
        "a.make()"),
        "3");
}

TEST(LambdaTest, WrongArgumentCount) {
    EXPECT_THROW(eval("f = func (a: int) -> int { return a; }; f()"), std::runtime_error);
}

TEST(LambdaTest, CallFunctionRefFromNative) {
    DiagnosticEngine diagnostics;

    Lexer lexer("f = func (a: int) -> int { return a + 1; }; f", diagnostics);
    Parser parser(lexer, diagnostics);
    auto ast = parser.parse();
    ASSERT_FALSE(diagnostics.hasErrors());

    if (ast->getType() == ASTNode::Type::Program) {
        SemanticAnalyzer analyzer(diagnostics);
        analyzer.analyze(static_cast<ProgramNode&>(*ast));
    }
    ASSERT_FALSE(diagnostics.hasErrors());

    ir::Compiler compiler(diagnostics);
    auto chunk = std::make_shared<ir::BytecodeChunk>(compiler.compile(*ast));
    ASSERT_FALSE(diagnostics.hasErrors());

    ir::VM vm(diagnostics);
    auto result = vm.run(chunk, {});
    ASSERT_FALSE(diagnostics.hasErrors());
    ASSERT_TRUE(std::holds_alternative<FunctionRefPtr>(result));

    auto func = std::get<FunctionRefPtr>(result);
    auto called = ir::VM::callFunctionRef(func, CallbackTypeValues{ 2 }, {}, diagnostics);
    ASSERT_FALSE(diagnostics.hasErrors());
    EXPECT_EQ(ir::VM::valueToString(called), "3");
}

TEST(LambdaTest, CallFunctionRefKeepsCaptures) {
    DiagnosticEngine diagnostics;

    Lexer lexer("a = 5; f = func (b: int) -> int { return a + b; }; f", diagnostics);
    Parser parser(lexer, diagnostics);
    auto ast = parser.parse();
    ASSERT_FALSE(diagnostics.hasErrors());

    if (ast->getType() == ASTNode::Type::Program) {
        SemanticAnalyzer analyzer(diagnostics);
        analyzer.analyze(static_cast<ProgramNode&>(*ast));
    }
    ASSERT_FALSE(diagnostics.hasErrors());

    ir::Compiler compiler(diagnostics);
    auto chunk = std::make_shared<ir::BytecodeChunk>(compiler.compile(*ast));
    ASSERT_FALSE(diagnostics.hasErrors());

    ir::VM vm(diagnostics);
    auto result = vm.run(chunk, {});
    ASSERT_FALSE(diagnostics.hasErrors());
    ASSERT_TRUE(std::holds_alternative<FunctionRefPtr>(result));

    auto func = std::get<FunctionRefPtr>(result);
    auto called = ir::VM::callFunctionRef(func, CallbackTypeValues{ 10 }, {}, diagnostics);
    ASSERT_FALSE(diagnostics.hasErrors());
    EXPECT_EQ(ir::VM::valueToString(called), "15");
}

TEST(LambdaTest, CaptureClauseEmpty) {
    EXPECT_EQ(eval("f = func[] (a: int) -> int { return a + 1; }; f(1)"), "2");
}

TEST(LambdaTest, CaptureClauseByValueDefault) {
    EXPECT_EQ(eval(
        "func outer(a: int) -> int { "
        "f = func[=] (b: int) -> int { return a + b; }; "
        "return f(10); "
        "} "
        "outer(5)"),
        "15");
}

TEST(LambdaTest, CaptureClauseByRefDefault) {
    EXPECT_EQ(eval(
        "func outer(a: int) -> int { "
        "f = func[&] () -> void { a = 42; }; "
        "f(); "
        "return a; "
        "} "
        "outer(1)"),
        "42");
}

TEST(LambdaTest, CaptureClauseExplicitByRef) {
    EXPECT_EQ(eval(
        "func outer(a: int, b: int) -> int { "
        "f = func[&a] () -> void { a = 42; }; "
        "f(); "
        "return a + b; "
        "} "
        "outer(1, 2)"),
        "44");
}

TEST(LambdaTest, CaptureClauseExplicitByValue) {
    EXPECT_EQ(eval(
        "func outer(a: int) -> int { "
        "f = func[a] () -> void { a = 42; }; "
        "f(); "
        "return a; "
        "} "
        "outer(1)"),
        "1");
}

TEST(LambdaTest, CaptureClauseMixedDefaults) {
    EXPECT_EQ(eval(
        "func outer(a: int, b: int) -> int { "
        "f = func[=, &b] () -> void { a = 7; b = 42; }; "
        "f(); "
        "return a + b; "
        "} "
        "outer(1, 2)"),
        "43");
    EXPECT_EQ(eval(
        "func outer(a: int, b: int) -> int { "
        "f = func[&, a] () -> void { a = 7; b = 42; }; "
        "f(); "
        "return a + b; "
        "} "
        "outer(1, 2)"),
        "43");
}

TEST(LambdaTest, CaptureClauseByRefSharesCell) {
    EXPECT_EQ(eval(
        "func outer(x: int) -> int { "
        "set = func[&x] (v: int) -> void { x = v; }; "
        "get = func[&x] () -> int { return x; }; "
        "set(99); "
        "return get(); "
        "} "
        "outer(1)"),
        "99");
}

TEST(LambdaTest, CaptureClauseByRefOutlivesFrame) {
    EXPECT_EQ(eval(
        "g = func () -> int { return 0; }; "
        "func make(x: int) -> void { "
        "s = func[&x] (v: int) -> void { x = v; }; "
        "g = func[&x] () -> int { return x; }; "
        "s(7); "
        "} "
        "func use() -> int { return g(); } "
        "make(1); "
        "use()"),
        "7");
}

TEST(LambdaTest, CaptureClauseEmptyDropsThis) {
    EXPECT_THROW(eval(
        "class A { "
        "public: "
        "v = 5; "
        "func bad() -> int { "
        "f = func[] () -> int { return this.v; }; "
        "return f(); "
        "} "
        "} "
        "a = new A(); "
        "a.bad()"),
        std::runtime_error);
}

TEST(LambdaTest, CaptureClauseExplicitThis) {
    EXPECT_EQ(eval(
        "class A { "
        "public: "
        "v = 5; "
        "func ok() -> int { "
        "f = func[this] () -> int { return this.v; }; "
        "return f(); "
        "} "
        "} "
        "a = new A(); "
        "a.ok()"),
        "5");
}

TEST(LambdaTest, CaptureClauseHidesUncapturedNames) {
    EXPECT_THROW(eval(
        "func outer(a: int) -> int { "
        "g = func[=] () -> int { "
        "h = func[] () -> int { return a; }; "
        "return h(); "
        "}; "
        "return g(); "
        "} "
        "outer(5)"),
        std::runtime_error);
}

TEST(LambdaTest, CaptureClauseUnknownName) {
    EXPECT_THROW(eval(
        "func outer(a: int) -> int { "
        "f = func[zz] () -> int { return 1; }; "
        "return f(); "
        "} "
        "outer(1)"),
        std::runtime_error);
}

TEST(LambdaTest, CaptureClauseDuplicateName) {
    EXPECT_THROW(eval(
        "func outer(a: int) -> int { "
        "f = func[a, a] () -> int { return 1; }; "
        "return f(); "
        "} "
        "outer(1)"),
        std::runtime_error);
}

TEST(LambdaTest, CallFunctionRefSharesGlobals) {
    DiagnosticEngine diagnostics;
    auto func = functionRefOf("g = 0; f = func () -> int { g = g + 1; return g; }; f");

    EXPECT_EQ(ir::VM::valueToString(ir::VM::callFunctionRef(func, CallbackTypeValues{}, {}, diagnostics)), "1");
    EXPECT_EQ(ir::VM::valueToString(ir::VM::callFunctionRef(func, CallbackTypeValues{}, {}, diagnostics)), "2");
    EXPECT_EQ(ir::VM::valueToString(ir::VM::callFunctionRef(func, CallbackTypeValues{}, {}, diagnostics)), "3");
    EXPECT_FALSE(diagnostics.hasErrors());
}
