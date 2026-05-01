#include <gtest/gtest.h>

#include <vector>

#include "LOICollectionA/frontend/Lexer.h"

using namespace LOICollection::frontend;

TEST(LexerTest, ParseNumbers) {
    Lexer lexer("123 3.14");

    Token t1 = lexer.getNextToken();
    EXPECT_EQ(t1.type, TokenType::TOKEN_INT);
    EXPECT_EQ(t1.value, "123");

    Token t2 = lexer.getNextToken();
    EXPECT_EQ(t2.type, TokenType::TOKEN_FLOAT);
    EXPECT_EQ(t2.value, "3.14");

    EXPECT_EQ(lexer.getNextToken().type, TokenType::TOKEN_EOF);
}

TEST(LexerTest, ParseString) {
    Lexer lexer("\"hello world\"");

    Token t = lexer.getNextToken();
    EXPECT_EQ(t.type, TokenType::TOKEN_STRING);
    EXPECT_EQ(t.value, "hello world");
}

TEST(LexerTest, ParseIdentifiersAndKeywords) {
    Lexer lexer("if true false myVar");

    auto t1 = lexer.getNextToken();
    EXPECT_EQ(t1.type, TokenType::TOKEN_IF);

    auto t2 = lexer.getNextToken();
    EXPECT_EQ(t2.type, TokenType::TOKEN_BOOL_LIT);
    EXPECT_EQ(t2.value, "true");

    auto t3 = lexer.getNextToken();
    EXPECT_EQ(t3.type, TokenType::TOKEN_BOOL_LIT);
    EXPECT_EQ(t3.value, "false");

    auto t4 = lexer.getNextToken();
    EXPECT_EQ(t4.type, TokenType::TOKEN_IDENT);
    EXPECT_EQ(t4.value, "myVar");
}

TEST(LexerTest, ParseOperators) {
    Lexer lexer("+ - * / % ^ == != > < >= <= && || !");
    std::vector<TokenType> expected = {
        TokenType::TOKEN_PLUS,    TokenType::TOKEN_MINUS,   TokenType::TOKEN_MULTIPLY,
        TokenType::TOKEN_DIVIDE,  TokenType::TOKEN_MOD,     TokenType::TOKEN_POWER,
        TokenType::TOKEN_OP,      TokenType::TOKEN_OP,      TokenType::TOKEN_OP,
        TokenType::TOKEN_OP,      TokenType::TOKEN_OP,      TokenType::TOKEN_OP,
        TokenType::TOKEN_BOOL_OP, TokenType::TOKEN_BOOL_OP, TokenType::TOKEN_OP
    };

    for (auto expectedType : expected) {
        auto t = lexer.getNextToken();
        EXPECT_EQ(t.type, expectedType);
    }
}

TEST(LexerTest, ParseColonAndNamespace) {
    Lexer lexer(": ::");

    auto t1 = lexer.getNextToken();
    EXPECT_EQ(t1.type, TokenType::TOKEN_COLON);

    auto t2 = lexer.getNextToken();
    EXPECT_EQ(t2.type, TokenType::TOKEN_NAMESPACE);
    EXPECT_EQ(t2.value, "::");
}

TEST(LexerTest, PeekDoesNotAdvance) {
    Lexer lexer("1 2");

    auto peeked = lexer.peekNextToken();
    EXPECT_EQ(peeked.type, TokenType::TOKEN_INT);

    auto real1 = lexer.getNextToken();
    EXPECT_EQ(real1.type, TokenType::TOKEN_INT);
    
    auto real2 = lexer.getNextToken();
    EXPECT_EQ(real2.type, TokenType::TOKEN_INT);
}