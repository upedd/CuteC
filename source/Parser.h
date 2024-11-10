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

    AST::ExprHandle expression();

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


#endif //PARSER_H
