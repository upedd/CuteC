#ifndef PARSER_H
#define PARSER_H
#include <memory>
#include <utility>
#include <vector>

#include "Ast.h"
#include "Token.h"


class Parser {
public:
    class Error {
    public:
        std::string message;
    };

    explicit Parser(std::vector<Token> tokens) : m_tokens(std::move(tokens)) {
    }

    void parse();

    AST::Function function();

    AST::StmtHandle statement();

    enum class Precedence {
        NONE,
        BITWISE_OR,
        BITWISE_XOR,
        BITWISE_AND,
        BITWISE_SHIFT,
        TERM,
        FACTOR,
    };

    AST::ExprHandle expression(Precedence min_precedence = Precedence::NONE);

    AST::BinaryExpr::Kind binary_operator();

    AST::ExprHandle factor();

    AST::Program program{};
    std::vector<Error> errors{};

private:
    Token peek();

    Token consume();

    Token expect(Token::Type type);

    bool match(Token::Type type);

    int m_pos = 0;
    std::vector<Token> m_tokens;
};

inline Parser::Precedence operator+(Parser::Precedence lhs, int rhs) {
    return static_cast<Parser::Precedence>(static_cast<int>(lhs) + rhs);
}

#endif //PARSER_H
