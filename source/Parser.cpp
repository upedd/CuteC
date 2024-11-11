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

static bool is_binary_operator(Token::Type type) {
    switch (type) {
        case Token::Type::PLUS:
        case Token::Type::MINUS:
        case Token::Type::ASTERISK:
        case Token::Type::PERCENT:
        case Token::Type::FORWARD_SLASH:
        case Token::Type::LESS_LESS:
        case Token::Type::GREATER_GREATER:
        case Token::Type::AND:
        case Token::Type::CARET:
        case Token::Type::BAR:
        case Token::Type::LESS:
        case Token::Type::LESS_EQUAL:
        case Token::Type::GREATER:
        case Token::Type::GREATER_EQUAL:
        case Token::Type::BANG_EQUAL:
        case Token::Type::EQUAL_EQUAL:
        case Token::Type::AND_AND:
        case Token::Type::BAR_BAR:
            return true;
        default:
            return false;
    }
}

static Parser::Precedence get_precedence(Token::Type type) {
    switch (type) {
        case Token::Type::ASTERISK:
        case Token::Type::PERCENT:
        case Token::Type::FORWARD_SLASH:
            return Parser::Precedence::FACTOR;
        case Token::Type::PLUS:
        case Token::Type::MINUS:
            return Parser::Precedence::TERM;
        case Token::Type::LESS_LESS:
        case Token::Type::GREATER_GREATER:
            return Parser::Precedence::BITWISE_SHIFT;
        case Token::Type::LESS:
        case Token::Type::LESS_EQUAL:
        case Token::Type::GREATER:
        case Token::Type::GREATER_EQUAL:
            return Parser::Precedence::RELATIONAL;
        case Token::Type::EQUAL_EQUAL:
        case Token::Type::BANG_EQUAL:
            return Parser::Precedence::EQUALITY;
        case Token::Type::AND:
            return Parser::Precedence::BITWISE_AND;
        case Token::Type::CARET:
            return Parser::Precedence::BITWISE_XOR;
        case Token::Type::BAR:
            return Parser::Precedence::BITWISE_OR;
        case Token::Type::AND_AND:
            return Parser::Precedence::LOGICAL_AND;
        case Token::Type::BAR_BAR:
            return Parser::Precedence::LOGICAL_OR;
    }
}


AST::ExprHandle Parser::expression(Precedence min_precedence) {
    auto left = factor();
    auto token = peek();
    while (is_binary_operator(token.type) && get_precedence(token.type) >= min_precedence) {
        auto op = binary_operator();
        auto right = expression(get_precedence(token.type) + 1);
        left = std::make_unique<AST::Expr>(AST::BinaryExpr(op, std::move(left), std::move(right)));
        token = peek();
    }
    return left;
}


AST::BinaryExpr::Kind Parser::binary_operator() {
    switch (Token token = consume(); token.type) {
        case Token::Type::PLUS:
            return AST::BinaryExpr::Kind::ADD;
        case Token::Type::MINUS:
            return AST::BinaryExpr::Kind::SUBTRACT;
        case Token::Type::ASTERISK:
            return AST::BinaryExpr::Kind::MULTIPLY;
        case Token::Type::PERCENT:
            return AST::BinaryExpr::Kind::REMAINDER;
        case Token::Type::FORWARD_SLASH:
            return AST::BinaryExpr::Kind::DIVIDE;
        case Token::Type::LESS_LESS:
            return AST::BinaryExpr::Kind::SHIFT_LEFT;
        case Token::Type::GREATER_GREATER:
            return AST::BinaryExpr::Kind::SHIFT_RIGHT;
        case Token::Type::AND:
            return AST::BinaryExpr::Kind::BITWISE_AND;
        case Token::Type::CARET:
            return AST::BinaryExpr::Kind::BITWISE_XOR;
        case Token::Type::BAR:
            return AST::BinaryExpr::Kind::BITWISE_OR;
        case Token::Type::LESS:
            return AST::BinaryExpr::Kind::LESS;
        case Token::Type::LESS_EQUAL:
            return AST::BinaryExpr::Kind::LESS_EQUAL;
        case Token::Type::GREATER:
            return AST::BinaryExpr::Kind::GREATER;
        case Token::Type::GREATER_EQUAL:
            return AST::BinaryExpr::Kind::GREATER_EQUAL;
        case Token::Type::BANG_EQUAL:
            return AST::BinaryExpr::Kind::NOT_EQUAL;
        case Token::Type::EQUAL_EQUAL:
            return AST::BinaryExpr::Kind::EQUAL;
        case Token::Type::BAR_BAR:
            return AST::BinaryExpr::Kind::LOGICAL_OR;
        case Token::Type::AND_AND:
            return AST::BinaryExpr::Kind::LOGICAL_AND;
    }
}

AST::ExprHandle Parser::factor() {
    Token token = consume();
    if (token.type == Token::Type::CONSTANT) {
        return std::make_unique<AST::Expr>(AST::ConstantExpr(std::stol(token.lexeme)));
    }
    if (token.type == Token::Type::MINUS) {
        auto expr = factor();
        return std::make_unique<AST::Expr>(AST::UnaryExpr(AST::UnaryExpr::Kind::NEGATE, std::move(expr)));
    }
    if (token.type == Token::Type::TILDE) {
        auto expr = factor();
        return std::make_unique<AST::Expr>(AST::UnaryExpr(AST::UnaryExpr::Kind::COMPLEMENT, std::move(expr)));
    }
    if (token.type == Token::Type::BANG) {
        auto expr = factor();
        return std::make_unique<AST::Expr>(AST::UnaryExpr(AST::UnaryExpr::Kind::LOGICAL_NOT, std::move(expr)));
    }
    if (token.type == Token::Type::LEFT_PAREN) {
        auto expr = expression();
        expect(Token::Type::RIGHT_PAREN);
        return expr;
    }
    errors.emplace_back(std::format("Expected an expression at {}:{}", token.position.line, token.position.offset));
    return {};
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

