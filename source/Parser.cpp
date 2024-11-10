#include "Parser.h"

void Parser::parse() {
    program = AST::Program(function());
    if (m_pos != m_tokens.size()) {
        errors.emplace_back(std::format("Syntax Error: unexpected token at {}:{}", m_tokens[m_pos].position.line,
                                        m_tokens[m_pos].position.offset));
    }
}

AST::Function Parser::function() {
    expect(Token::Type::INT);
    Token name = expect(Token::Type::IDENTIFIER);
    expect(Token::Type::LEFT_PAREN);
    expect(Token::Type::VOID);
    expect(Token::Type::RIGHT_PAREN);
    expect(Token::Type::LEFT_BRACE);
    auto stmt = statement();
    expect(Token::Type::RIGHT_BRACE);
    return AST::Function(name.lexeme, std::move(stmt));
}

AST::StmtHandle Parser::statement() {
    expect(Token::Type::RETURN);
    auto expr = expression();
    expect(Token::Type::SEMICOLON);
    return std::make_unique<AST::Stmt>(AST::ReturnStmt(std::move(expr)));
}

AST::ExprHandle Parser::expression() {
    Token token = consume();
    if (token.type == Token::Type::CONSTANT) {
        return std::make_unique<AST::Expr>(AST::ConstantExpr(std::stol(token.lexeme)));
    }
    if (token.type == Token::Type::MINUS) {
        auto expr = expression();
        return std::make_unique<AST::Expr>(AST::UnaryExpr(AST::UnaryExpr::Kind::NEGATE, std::move(expr)));
    }
    if (token.type == Token::Type::TILDE) {
        auto expr = expression();
        return std::make_unique<AST::Expr>(AST::UnaryExpr(AST::UnaryExpr::Kind::COMPLEMENT, std::move(expr)));
    }
    if (token.type == Token::Type::LEFT_PAREN) {
        auto expr = expression();
        expect(Token::Type::RIGHT_PAREN);
        return expr;
    }
    errors.emplace_back(std::format("Expected an expression at {}:{}", token.position.line, token.position.offset));
}

Token Parser::peek() {
    return m_tokens[m_pos];
}

Token Parser::consume() {
    return m_tokens[m_pos++];
}

Token Parser::expect(Token::Type type) {
    auto token = consume();
    if (token.type != type) {
        errors.emplace_back(std::format("Expected '{}' but got '{}' at {}:{})", Token::type_to_string(type),
                                        Token::type_to_string(token.type), token.position.line, token.position.offset));
    }
    return token;
}

bool Parser::match(Token::Type type) {
    if (peek().type == type) {
        consume();
        return true;
    }
    return false;
}

