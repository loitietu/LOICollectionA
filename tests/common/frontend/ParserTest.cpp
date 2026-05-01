#include <gtest/gtest.h>

#include "common/frontend/CommonTest.h"

using namespace LOICollection::frontend;

TEST(ParserEvalTest, LiteralValues) {
    EXPECT_EQ(eval("42"), "42");
    EXPECT_EQ(eval("3.14000"), "3.14");
    EXPECT_EQ(eval("true"), "true");
    EXPECT_EQ(eval("false"), "false");
    EXPECT_EQ(eval("\"hello\""), "hello");
}

TEST(ParserEvalTest, Arithmetic) {
    EXPECT_EQ(eval("1+2"), "3");
    EXPECT_EQ(eval("2*3+1"), "7");
    EXPECT_EQ(eval("10/3"), "3");
    EXPECT_EQ(eval("2^3"), "8");
    EXPECT_EQ(eval("10%3"), "1");
}

TEST(ParserEvalTest, Unary) {
    EXPECT_EQ(eval("-5"), "-5");
    EXPECT_EQ(eval("--3"), "3");
    EXPECT_EQ(eval("!true"), "false");
    EXPECT_EQ(eval("!false"), "true");
}

TEST(ParserEvalTest, Comparison) {
    EXPECT_EQ(eval("5>3"), "true");
    EXPECT_EQ(eval("5<3"), "false");
    EXPECT_EQ(eval("5==5"), "true");
    EXPECT_EQ(eval("5!=5"), "false");
    EXPECT_EQ(eval("5>=5"), "true");
    EXPECT_EQ(eval("5<=4"), "false");
}

TEST(ParserEvalTest, Logical) {
    EXPECT_EQ(eval("true && false"), "false");
    EXPECT_EQ(eval("true || false"), "true");
    EXPECT_EQ(eval("!true || 0"), "false");
    EXPECT_EQ(eval("true && 1"), "true");
}

TEST(ParserEvalTest, IfStatement) {
    EXPECT_EQ(eval("if(true)[1:2]"), "1");
    EXPECT_EQ(eval("if(false)[1:2]"), "2");
    EXPECT_EQ(eval("if(2>1)[yes:no]"), "yes");
}

TEST(ParserEvalTest, IfStatementNested) {
    EXPECT_EQ(eval("if(6<7)[if(true)[1:2]:2]"), "1");
    EXPECT_EQ(eval("if(6<7)[if(false)[1:2]:2]"), "2");
    EXPECT_EQ(eval("if(6>7)[if(true)[yes:no]:if(false)['yes2':'no2']]"), "no2");
}

TEST(ParserEvalTest, TemplateConcatenation) {
    EXPECT_EQ(eval("hello world"), "helloworld");
    EXPECT_EQ(eval("abc 123"), "abc123");
}

TEST(ParserEvalTest, Transpile) {
    EXPECT_EQ(eval("$raw text}"), "rawtext}");
    EXPECT_EQ(eval("prefix $inner}' suffix'"), "prefixinner} suffix");
}

TEST(ParserEvalTest, SyntaxError) {
    EXPECT_THROW(eval("if("), std::runtime_error);
    EXPECT_THROW(eval("(1+2"), std::runtime_error);
}

TEST(ParserEvalTest, NestedExpressions) {
    EXPECT_EQ(eval("(1+2)*3"), "9");
    EXPECT_EQ(eval("if(3>2)[10:20] + 5"), "15");
    EXPECT_EQ(eval("if(3>2)['10':'20'] + '5'"), "105");
}

TEST(ParserEvalTest, StringConcatenation) {
    EXPECT_EQ(eval("\"Value: \" + '42'"), "Value: 42");
}
