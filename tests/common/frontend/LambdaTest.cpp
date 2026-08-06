#include <gtest/gtest.h>

#include "common/frontend/CommonTest.h"

using namespace LOICollection::frontend;

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
