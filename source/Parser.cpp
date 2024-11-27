#include "Parser.h"

#include "Token.h"
#include "analysis/TypeCheckerPass.h"


static bool is_specifier(Token::Type type) {
    return type == Token::Type::STATIC || type == Token::Type::EXTERN || type == Token::Type::INT;
}

std::pair<Type, AST::StorageClass> Parser::parse_type_and_storage_class() {
    std::vector<Type> types;
    std::vector<AST::StorageClass> storage_classes;
    while (is_specifier(peek().type)) {
        auto specifier = consume();
        if (specifier.type == Token::Type::INT) {
            types.emplace_back(IntType());
        } else if (specifier.type == Token::Type::EXTERN) {
            storage_classes.emplace_back(AST::StorageClass::EXTERN);
        } else if (specifier.type == Token::Type::STATIC) {
            storage_classes.emplace_back(AST::StorageClass::STATIC);
        }
    }

    if (types.size() != 1) {
        errors.emplace_back("Invalid declaration type");
    }
    if (storage_classes.size() > 1) {
        errors.emplace_back("Invalid declaration storage class");
    }

    if (storage_classes.empty()) {
        storage_classes.emplace_back(AST::StorageClass::NONE);
    }
    // TODO: should continue with error!
    return {types[0], storage_classes[0]};
}


void Parser::parse() {
    while (!at_end() && is_specifier(peek().type)) {
        program.declarations.emplace_back(declaration());
    }
    if (m_pos != m_tokens.size() - 1) {
        errors.emplace_back(std::format("Syntax Error: unexpected token at {}:{}", m_tokens[m_pos].position.line,
                                        m_tokens[m_pos].position.offset));
    }
}

AST::FunctionDecl Parser::function_decl(const std::pair<Type, AST::StorageClass>& type_and_storage_class) {
    Token name = expect(Token::Type::IDENTIFIER);
    expect(Token::Type::LEFT_PAREN);
    std::vector<std::string> arguments;
    if (!match(Token::Type::VOID)) {
        do {
            expect(Token::Type::INT);
            auto argument = expect(Token::Type::IDENTIFIER);
            arguments.emplace_back(argument.lexeme);
        } while (match(Token::Type::COMMA));
    }
    expect(Token::Type::RIGHT_PAREN);
    if (match(Token::Type::SEMICOLON)) {
        return AST::FunctionDecl(name.lexeme, std::move(arguments), {}, type_and_storage_class.second);
    }
    expect(Token::Type::LEFT_BRACE);
    return AST::FunctionDecl(name.lexeme, std::move(arguments), block(), type_and_storage_class.second);
}


AST::BlockItem Parser::block_item() {
    if (is_specifier(peek().type)) {
        return declaration();
    }
    return statement();
}

AST::DeclHandle Parser::declaration() {
    auto type_and_storage_class = parse_type_and_storage_class();

    if (peek(1).type == Token::Type::LEFT_PAREN) {
        return std::make_unique<AST::Declaration>(function_decl(type_and_storage_class));
    }
    return std::make_unique<AST::Declaration>(variable_declaration(type_and_storage_class));
}

AST::VariableDecl Parser::variable_declaration(const std::pair<Type, AST::StorageClass>& type_and_storage_class) {
    auto identifier = expect(Token::Type::IDENTIFIER);
    AST::ExprHandle expr;
    if (match(Token::Type::EQUAL)) {
        expr = expression();
    }
    expect(Token::Type::SEMICOLON);
    return AST::VariableDecl(identifier.lexeme, std::move(expr), type_and_storage_class.second);
}

AST::StmtHandle Parser::labeled_stmt(const Token &identifier) {
    expect(Token::Type::COLON);
    auto stmt = statement();
    return std::make_unique<AST::Stmt>(AST::LabeledStmt(identifier.lexeme, std::move(stmt)));
}

AST::StmtHandle Parser::goto_stmt() {
    auto label = expect(Token::Type::IDENTIFIER);
    expect(Token::Type::SEMICOLON);
    return std::make_unique<AST::Stmt>(AST::GoToStmt(label.lexeme));
}

std::vector<AST::BlockItem> Parser::block() {
    std::vector<AST::BlockItem> items;
    while (!at_end() && peek().type != Token::Type::RIGHT_BRACE) {
        items.emplace_back(block_item());
    }
    expect(Token::Type::RIGHT_BRACE);
    return items;
}

AST::StmtHandle Parser::while_stmt() {
    expect(Token::Type::LEFT_PAREN);
    auto condition = expression();
    expect(Token::Type::RIGHT_PAREN);
    auto body = statement();
    return std::make_unique<AST::Stmt>(AST::WhileStmt(std::move(condition), std::move(body)));
}

AST::StmtHandle Parser::do_while_stmt() {
    auto body = statement();
    expect(Token::Type::WHILE);
    expect(Token::Type::LEFT_PAREN);
    auto condition = expression();
    expect(Token::Type::RIGHT_PAREN);
    expect(Token::Type::SEMICOLON);
    return std::make_unique<AST::Stmt>(AST::DoWhileStmt(std::move(condition), std::move(body)));
}

AST::StmtHandle Parser::for_stmt() {
    expect(Token::Type::LEFT_PAREN);
    std::variant<std::unique_ptr<AST::VariableDecl>, AST::ExprHandle> init;
    if (match(Token::Type::INT)) {
        init = std::make_unique<AST::VariableDecl>(variable_declaration({IntType(), AST::StorageClass::NONE}));
    } else if (!match(Token::Type::SEMICOLON)) {
        init = expression();
        expect(Token::Type::SEMICOLON);
    }
    AST::ExprHandle condition;
    if (!match(Token::Type::SEMICOLON)) {
        condition = expression();
        expect(Token::Type::SEMICOLON);
    }

    AST::ExprHandle post;
    if (!match(Token::Type::RIGHT_PAREN)) {
        post = expression();
        expect(Token::Type::RIGHT_PAREN);
    }

    auto body = statement();

    return std::make_unique<AST::Stmt>(AST::ForStmt(std::move(init), std::move(condition), std::move(post),
                                                    std::move(body)));
}

AST::StmtHandle Parser::case_stmt() {
    auto value = expression();
    expect(Token::Type::COLON);
    auto stmt = statement();
    return std::make_unique<AST::Stmt>(AST::CaseStmt(std::move(value), std::move(stmt)));
}

AST::StmtHandle Parser::switch_stmt() {
    expect(Token::Type::LEFT_PAREN);
    auto expr = expression();
    expect(Token::Type::RIGHT_PAREN);
    auto body = statement();
    return std::make_unique<AST::Stmt>(AST::SwitchStmt(std::move(expr), std::move(body)));
}

AST::StmtHandle Parser::statement() {
    if (peek().type == Token::Type::IDENTIFIER && peek(1).type == Token::Type::COLON) {
        auto identifier = consume();
        return labeled_stmt(identifier);
    }

    if (match(Token::Type::CASE)) {
        return case_stmt();
    }

    if (match(Token::Type::DEFAULT)) {
        expect(Token::Type::COLON);
        auto stmt = statement();
        return std::make_unique<AST::Stmt>(AST::DefaultStmt(std::move(stmt)));
    }

    if (match(Token::Type::SWITCH)) {
        return switch_stmt();
    }

    if (match(Token::Type::BREAK)) {
        expect(Token::Type::SEMICOLON);
        return std::make_unique<AST::Stmt>(AST::BreakStmt());
    }

    if (match(Token::Type::CONTINUE)) {
        expect(Token::Type::SEMICOLON);
        return std::make_unique<AST::Stmt>(AST::ContinueStmt());
    }

    if (match(Token::Type::WHILE)) {
        return while_stmt();
    }

    if (match(Token::Type::DO)) {
        return do_while_stmt();
    }

    if (match(Token::Type::FOR)) {
        return for_stmt();
    }

    if (match(Token::Type::LEFT_BRACE)) {
        return std::make_unique<AST::Stmt>(AST::CompoundStmt(block()));
    }

    if (match(Token::Type::GOTO)) {
        return goto_stmt();
    }

    if (match(Token::Type::IF)) {
        return if_stmt();
    }
    if (match(Token::Type::RETURN)) {
        auto expr = expression();
        expect(Token::Type::SEMICOLON);
        return std::make_unique<AST::Stmt>(AST::ReturnStmt(std::move(expr)));
    }
    if (match(Token::Type::SEMICOLON)) {
        return std::make_unique<AST::Stmt>(AST::NullStmt());
    }
    auto expr = expression();
    expect(Token::Type::SEMICOLON);
    return std::make_unique<AST::Stmt>(AST::ExprStmt(std::move(expr)));
}

AST::StmtHandle Parser::if_stmt() {
    expect(Token::Type::LEFT_PAREN);
    auto condition = expression();
    expect(Token::Type::RIGHT_PAREN);
    auto then_stmt = statement();
    AST::StmtHandle else_stmt;
    if (match(Token::Type::ELSE)) {
        else_stmt = statement();
    }

    return std::make_unique<AST::Stmt>(AST::IfStmt(std::move(condition), std::move(then_stmt), std::move(else_stmt)));
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
        case Token::Type::EQUAL:
        case Token::Type::PLUS_EQUAL:
        case Token::Type::MINUS_EQUAL:
        case Token::Type::ASTERISK_EQUAL:
        case Token::Type::SLASH_EQUAL:
        case Token::Type::GREATER_GREATER_EQUAL:
        case Token::Type::LESS_LESS_EQUAL:
        case Token::Type::AND_EQUAL:
        case Token::Type::BAR_EQUAL:
        case Token::Type::CARET_EQUAL:
        case Token::Type::PERCENT_EQUAL:
        case Token::Type::QUESTION_MARK:
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
        case Token::Type::EQUAL:
        case Token::Type::PLUS_EQUAL:
        case Token::Type::MINUS_EQUAL:
        case Token::Type::ASTERISK_EQUAL:
        case Token::Type::SLASH_EQUAL:
        case Token::Type::GREATER_GREATER_EQUAL:
        case Token::Type::LESS_LESS_EQUAL:
        case Token::Type::AND_EQUAL:
        case Token::Type::BAR_EQUAL:
        case Token::Type::PERCENT_EQUAL:
        case Token::Type::CARET_EQUAL:
            return Parser::Precedence::ASSIGMENT;
        case Token::Type::QUESTION_MARK:
            return Parser::Precedence::CONDITIONAL;
    }
}


AST::ExprHandle Parser::expression(Precedence min_precedence) {
    auto left = factor();
    auto token = peek();
    while (is_binary_operator(token.type) && get_precedence(token.type) >= min_precedence) {
        if (get_precedence(token.type) == Precedence::ASSIGMENT) {
            auto op = compound_operator();
            auto right = expression(get_precedence(token.type));
            left = std::make_unique<AST::Expr>(AST::AssigmentExpr(op, std::move(left), std::move(right)));
        } else if (token.type == Token::Type::QUESTION_MARK) {
            consume();
            auto then_expr = expression();
            expect(Token::Type::COLON);
            auto else_expr = expression(get_precedence(token.type));
            left = std::make_unique<AST::Expr>(
                AST::ConditionalExpr(std::move(left), std::move(then_expr), std::move(else_expr)));
        } else {
            auto op = binary_operator();
            auto right = expression(get_precedence(token.type) + 1);
            left = std::make_unique<AST::Expr>(AST::BinaryExpr(op, std::move(left), std::move(right)));
        }
        token = peek();
    }
    return left;
}

AST::AssigmentExpr::Operator Parser::compound_operator() {
    switch (Token token = consume(); token.type) {
        case Token::Type::EQUAL:
            return AST::AssigmentExpr::Operator::NONE;
        case Token::Type::PLUS_EQUAL:
            return AST::AssigmentExpr::Operator::ADD;
        case Token::Type::MINUS_EQUAL:
            return AST::AssigmentExpr::Operator::SUBTRACT;
        case Token::Type::ASTERISK_EQUAL:
            return AST::AssigmentExpr::Operator::MULTIPLY;
        case Token::Type::PERCENT_EQUAL:
            return AST::AssigmentExpr::Operator::REMAINDER;
        case Token::Type::SLASH_EQUAL:
            return AST::AssigmentExpr::Operator::DIVIDE;
        case Token::Type::GREATER_GREATER_EQUAL:
            return AST::AssigmentExpr::Operator::SHIFT_RIGHT;
        case Token::Type::LESS_LESS_EQUAL:
            return AST::AssigmentExpr::Operator::SHIFT_LEFT;
        case Token::Type::AND_EQUAL:
            return AST::AssigmentExpr::Operator::BITWISE_AND;
        case Token::Type::CARET_EQUAL:
            return AST::AssigmentExpr::Operator::BITWISE_XOR;
        case Token::Type::BAR_EQUAL:
            return AST::AssigmentExpr::Operator::BITWISE_OR;
        default:
            std::unreachable();
    }
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
        default:
            std::unreachable();
    }
}

AST::ExprHandle Parser::primary(const Token &token) {
    if (token.type == Token::Type::IDENTIFIER) {
        return std::make_unique<AST::Expr>(AST::VariableExpr(token.lexeme));
    }
    if (token.type == Token::Type::CONSTANT) {
        return std::make_unique<AST::Expr>(AST::ConstantExpr(std::stol(token.lexeme)));
    }

    if (token.type == Token::Type::LEFT_PAREN) {
        auto expr = expression();
        expect(Token::Type::RIGHT_PAREN);
        return expr;
    }

    return {};
}

std::vector<AST::ExprHandle> Parser::arguments_list() {
    std::vector<AST::ExprHandle> arguments;
    if (!match(Token::Type::RIGHT_PAREN)) {
        do {
            arguments.emplace_back(expression());
        } while (match(Token::Type::COMMA));
        expect(Token::Type::RIGHT_PAREN);
    }
    return arguments;
}


AST::ExprHandle Parser::factor() {
    Token token = consume();
    // TODO: refactor!
    auto primary_expr = primary(token);
    if (primary_expr) {
        if (match(Token::Type::PLUS_PLUS)) {
            return std::make_unique<AST::Expr>(
                AST::UnaryExpr(AST::UnaryExpr::Kind::POSTFIX_INCREMENT, std::move(primary_expr)));
        }
        if (match(Token::Type::MINUS_MINUS)) {
            return std::make_unique<AST::Expr>(
                AST::UnaryExpr(AST::UnaryExpr::Kind::POSTFIX_DECREMENT, std::move(primary_expr)));
        }
        if (std::holds_alternative<AST::VariableExpr>(*primary_expr) && match(Token::Type::LEFT_PAREN)) {
            return std::make_unique<AST::Expr>(
                AST::FunctionCall(std::get<AST::VariableExpr>(*primary_expr).name, arguments_list()));
        }
        return primary_expr;
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

    if (token.type == Token::Type::PLUS_PLUS) {
        auto expr = factor();
        return std::make_unique<AST::Expr>(AST::UnaryExpr(AST::UnaryExpr::Kind::PREFIX_INCREMENT, std::move(expr)));
    }
    if (token.type == Token::Type::MINUS_MINUS) {
        auto expr = factor();
        return std::make_unique<AST::Expr>(AST::UnaryExpr(AST::UnaryExpr::Kind::PREFIX_DECREMENT, std::move(expr)));
    }

    errors.emplace_back(std::format("Expected an expression at {}:{}", token.position.line, token.position.offset));

    return {};
}

// Safety!

Token Parser::peek(int n) {
    return m_tokens[m_pos + n];
}

Token Parser::consume() {
    return m_tokens[m_pos++];
}

bool Parser::at_end() {
    return m_pos >= m_tokens.size() - 1;
}

Token Parser::expect(Token::Type type) {
    auto token = consume();
    if (token.type != type) {
        errors.emplace_back(std::format("Expected '{}' but got '{}' at {}:{})", Token::type_to_string(type),
                                        Token::type_to_string(token.type), token.position.line, token.position.offset));
    }
    return token;
}

// TODO: prob unsafe!
void Parser::go_back() {
    --m_pos;
}

bool Parser::match(Token::Type type) {
    if (peek().type == type) {
        consume();
        return true;
    }
    return false;
}

