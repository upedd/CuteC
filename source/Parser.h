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

    AST::BlockItem block_item();

    AST::DeclarationHandle declaration();

    AST::StmtHandle labeled_stmt(const Token &identifer);

    AST::StmtHandle goto_stmt();

    std::vector<AST::BlockItem> block();

    AST::StmtHandle statement();

    AST::StmtHandle if_stmt();

    enum class Precedence {
        NONE,
        ASSIGMENT,
        CONDITIONAL,
        LOGICAL_OR,
        LOGICAL_AND,
        BITWISE_OR,
        BITWISE_XOR,
        BITWISE_AND,
        EQUALITY,
        RELATIONAL,
        BITWISE_SHIFT,
        TERM,
        FACTOR,
    };

    AST::ExprHandle expression(Precedence min_precedence = Precedence::NONE);

    AST::UnaryExpr::Kind postfix_operator();

    AST::AssigmentExpr::Operator compound_operator();

    AST::BinaryExpr::Kind binary_operator();

    AST::ExprHandle primary(const Token &token);

    AST::ExprHandle factor();

    AST::Program program{};
    std::vector<Error> errors{};

private:
    Token peek(int n = 0);

    Token consume();

    bool at_end();

    Token expect(Token::Type type);

    void go_back();

    bool match(Token::Type type);

    int m_pos = 0;
    std::vector<Token> m_tokens;
};

inline Parser::Precedence operator+(Parser::Precedence lhs, int rhs) {
    return static_cast<Parser::Precedence>(static_cast<int>(lhs) + rhs);
}

#endif //PARSER_H
