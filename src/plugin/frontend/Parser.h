#pragma once

#include <string>
#include <stdexcept>
#include <variant>

#include "frontend/Lexer.h"

namespace LOICollection::frontend {
    class Parser {
        Lexer& lexer;
        Token current_token;

    public:
        Parser(Lexer& l) : lexer(l) {
            current_token = lexer.getNextToken();
        }

        std::string parse();
    private:
        std::string parseIfStatement();

        bool parseBoolExpression();
        bool parseOrExpression();
        bool parseAndExpression();
        bool parsePrimary();
        bool parseComparison();

        struct Value {
            std::variant<int, std::string> data;
            
            bool operator>(const Value& other) const {
                if (auto l = std::get_if<int>(&data)) {
                    if (auto r = std::get_if<int>(&other.data))
                        return *l > *r;
                }
                throw std::runtime_error("> operator between incompatible types");
            }

            bool operator<(const Value& other) const {
                if (auto l = std::get_if<int>(&data)) {
                    if (auto r = std::get_if<int>(&other.data))
                        return *l < *r;
                }
                throw std::runtime_error("< operator between incompatible types");
            }

            bool operator==(const Value& other) const {
                return data == other.data;
            }

            bool operator!=(const Value& other) const { return !(*this == other); }
            bool operator>=(const Value& other) const { return *this > other || *this == other; }
            bool operator<=(const Value& other) const { return *this < other || *this == other; }
        };

        Value parseValue();

        std::string parseResult();

        void eat(TokenType expected);

        std::string getTokenName(TokenType type);
    };
}