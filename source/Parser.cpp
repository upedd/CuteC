#include "Parser.h"

#include <algorithm>
#include <ranges>

#include "Token.h"
#include "Token.h"
#include "analysis/TypeCheckerPass.h"

static bool is_type_specifier(Token::Type type) {
    return type == Token::Type::INT || type == Token::Type::LONG || type == Token::Type::UNSIGNED || type == Token::Type::SIGNED || type == Token::Type::DOUBLE;
}

static bool is_specifier(Token::Type type) {
    return type == Token::Type::STATIC || type == Token::Type::EXTERN || is_type_specifier(type);
}

// mess
static bool contains_only_unique_specifiers(const std::vector<Token::Type>& types) {
    auto copy = types;
    std::sort(copy.begin(), copy.end());
    return std::adjacent_find(copy.begin(), copy.end()) == copy.end();
}

AST::Type Parser::parse_type(const std::vector<Token::Type>& types) {
    if (types == std::vector {Token::Type::DOUBLE}) {
        return AST::DoubleType {};
    }

    // mess
    if (std::find(types.begin(), types.end(), Token::Type::DOUBLE) != types.end() ||types.empty() || !contains_only_unique_specifiers(types) || (std::find(types.begin(), types.end(), Token::Type::UNSIGNED) != types.end() && std::find(types.begin(), types.end(), Token::Type::SIGNED) != types.end())) {
        errors.emplace_back("Invalid Type Specifier");
        return AST::IntType {}; // how to handle failure?
    }
    bool is_long = std::find(types.begin(), types.end(), Token::Type::LONG) != types.end();
    bool is_unsigned = std::find(types.begin(), types.end(), Token::Type::UNSIGNED) != types.end();
    if (is_long && is_unsigned) {
        return AST::ULongType {};
    }
    if (is_long ) {
        return AST::LongType {};
    }

    if (is_unsigned ) {
        return AST::UIntType {};
    }
    return AST::IntType {};
}

std::pair<AST::Type, AST::StorageClass> Parser::parse_type_and_storage_class() {
    std::vector<Token::Type> types;
    std::vector<AST::StorageClass> storage_classes;
    while (is_specifier(peek().type)) {
        auto specifier = consume();
        if (is_type_specifier(specifier.type)) {
            types.emplace_back(specifier.type);
        } else if (specifier.type == Token::Type::EXTERN) {
            storage_classes.emplace_back(AST::StorageClass::EXTERN);
        } else if (specifier.type == Token::Type::STATIC) {
            storage_classes.emplace_back(AST::StorageClass::STATIC);
        }
    }
    auto type = parse_type(types);


    if (storage_classes.size() > 1) {
        errors.emplace_back("Invalid declaration storage class");
    }

    if (storage_classes.empty()) {
        storage_classes.emplace_back(AST::StorageClass::NONE);
    }
    // TODO: should continue with error!
    return  {std::move(type), storage_classes[0]};
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

AST::FunctionDecl Parser::function_decl(std::pair<AST::Type, AST::StorageClass>& type_and_storage_class) {
    Token name = expect(Token::Type::IDENTIFIER);
    expect(Token::Type::LEFT_PAREN);
    std::vector<std::string> arguments;
    std::vector<AST::TypeHandle> argument_types;
    if (!match(Token::Type::VOID)) {
        do {
            std::vector<Token::Type> types;
            while (is_type_specifier(peek().type)) {
                types.emplace_back(consume().type);
            }
            argument_types.emplace_back(parse_type(types));
            auto argument = expect(Token::Type::IDENTIFIER);
            arguments.emplace_back(argument.lexeme);
        } while (match(Token::Type::COMMA));
    }
    expect(Token::Type::RIGHT_PAREN);
    if (match(Token::Type::SEMICOLON)) {
        return AST::FunctionDecl(name.lexeme, std::move(arguments), {}, AST::TypeHandle(AST::FunctionType {std::move(argument_types), std::move(type_and_storage_class.first) }), type_and_storage_class.second);
    }
    expect(Token::Type::LEFT_BRACE);
    return AST::FunctionDecl(name.lexeme, std::move(arguments), block(), AST::TypeHandle(AST::FunctionType {std::move(argument_types), std::move(type_and_storage_class.first) }), type_and_storage_class.second);
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

AST::VariableDecl Parser::variable_declaration(std::pair<AST::Type, AST::StorageClass>& type_and_storage_class) {
    auto identifier = expect(Token::Type::IDENTIFIER);
    AST::ExprHandle expr;
    if (match(Token::Type::EQUAL)) {
        expr = expression();
    }
    expect(Token::Type::SEMICOLON);
    return AST::VariableDecl(identifier.lexeme, std::move(expr), std::move(type_and_storage_class.first), type_and_storage_class.second);
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
    if (is_specifier(peek().type)) {
        auto decl = declaration();


        auto var_decl = std::get<AST::VariableDecl>(std::move(*decl));
        if (var_decl.storage_class != AST::StorageClass::NONE) {
            errors.emplace_back("For loop initializer must have automatic storage duration.");
        }
        // mess
        init = std::make_unique<AST::VariableDecl>(std::move(var_decl));
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
            if (!std::holds_alternative<AST::VariableExpr>(*left)) {
                errors.emplace_back("Left operand of assigment must be a lvalue");
            }
            auto var_expr = std::get<AST::VariableExpr>(*left);
            if (op) {
                auto binary = std::make_unique<AST::Expr>(AST::BinaryExpr(*op, std::make_unique<AST::Expr>(AST::VariableExpr(var_expr.name)), std::move(right)));
                left  = std::make_unique<AST::Expr>(AST::AssigmentExpr(std::move(left), std::move(binary)));
            } else {
                left  = std::make_unique<AST::Expr>(AST::AssigmentExpr(std::move(left), std::move(right)));
            }
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

std::optional<AST::BinaryExpr::Kind> Parser::compound_operator() {
    switch (Token token = consume(); token.type) {
        case Token::Type::EQUAL:
            return {};
        case Token::Type::PLUS_EQUAL:
            return AST::BinaryExpr::Kind::ADD;
        case Token::Type::MINUS_EQUAL:
            return AST::BinaryExpr::Kind::SUBTRACT;
        case Token::Type::ASTERISK_EQUAL:
            return AST::BinaryExpr::Kind::MULTIPLY;
        case Token::Type::PERCENT_EQUAL:
            return AST::BinaryExpr::Kind::REMAINDER;
        case Token::Type::SLASH_EQUAL:
            return AST::BinaryExpr::Kind::DIVIDE;
        case Token::Type::GREATER_GREATER_EQUAL:
            return AST::BinaryExpr::Kind::SHIFT_RIGHT;
        case Token::Type::LESS_LESS_EQUAL:
            return AST::BinaryExpr::Kind::SHIFT_LEFT;
        case Token::Type::AND_EQUAL:
            return AST::BinaryExpr::Kind::BITWISE_AND;
        case Token::Type::CARET_EQUAL:
            return AST::BinaryExpr::Kind::BITWISE_XOR;
        case Token::Type::BAR_EQUAL:
            return AST::BinaryExpr::Kind::BITWISE_OR;
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

AST::ExprHandle Parser::constant(const Token& token) {
    // mess
    std::string value = token.lexeme;
    if (token.type == Token::Type::FLOATING_POINT_CONSTANT) {
        // i absolutely love c++
        // we cannot use std::from_chars or std::stod as they don't distinguish between underflow and overflow
        // however strtod correctly handles the case of rounding out of range values either to zero or infinity
        // this probably is implementation defined so we can only pray this is implemented consistently
        // TODO: maybe find more bullet-proof way
        double output = std::strtod(value.c_str(), nullptr);
        return std::make_unique<AST::Expr>(AST::ConstantExpr(AST::ConstDouble(output)));
    } else if (token.type == Token::Type::UNSIGNED_LONG_CONSTANT) {
        try {
            return std::make_unique<AST::Expr>(AST::ConstantExpr(AST::ConstULong(std::stoull(token.lexeme))));
        } catch (const std::out_of_range&) {
            errors.emplace_back("Constant too large!");
            // graceful errors?
        }
    } else if (token.type == Token::Type::UNSIGNED_INT_CONSTANT) {
        try {
            std::uint64_t val = std::stoull(token.lexeme);
            if (val < std::numeric_limits<std::uint32_t>::max()) {
                return std::make_unique<AST::Expr>(AST::ConstantExpr(AST::ConstUInt(val)));
            }
            return std::make_unique<AST::Expr>(AST::ConstantExpr(AST::ConstULong(val)));
        } catch (const std::out_of_range&) {
                errors.emplace_back("Constant too large!");
                // graceful errors?
        }
    } else if (token.type == Token::Type::LONG_CONSTANT) {
        try {
            return std::make_unique<AST::Expr>(AST::ConstantExpr(AST::ConstLong(std::stoll(token.lexeme))));
        } catch (const std::out_of_range&) {
            errors.emplace_back("Constant too large!");
            // graceful errors?
        }
    } else {
        try {
            return std::make_unique<AST::Expr>(AST::ConstantExpr(AST::ConstInt(std::stoi(token.lexeme))));
        } catch (const std::out_of_range&) {
            try {
                return std::make_unique<AST::Expr>(AST::ConstantExpr(AST::ConstLong(std::stoll(token.lexeme))));
            } catch (const std::out_of_range&) {
                errors.emplace_back("Constant too large!");
                // graceful errors?
            }
        }
    }
}

AST::ExprHandle Parser::primary(const Token &token) {
    if (token.type == Token::Type::IDENTIFIER) {
        return std::make_unique<AST::Expr>(AST::VariableExpr(token.lexeme));
    }
    if (token.type == Token::Type::CONSTANT || token.type == Token::Type::LONG_CONSTANT || token.type == Token::Type::UNSIGNED_INT_CONSTANT || token.type == Token::Type::UNSIGNED_LONG_CONSTANT || token.type == Token::Type::FLOATING_POINT_CONSTANT) {
        return constant(token);
    }
    // TODO: correct?
    if (token.type == Token::Type::LEFT_PAREN) {
        if (is_type_specifier(peek().type)) {
            std::vector<Token::Type> types;
            while(is_type_specifier(peek().type)) {
                types.emplace_back(consume().type);
            }
            auto type = parse_type(types);
            expect(Token::Type::RIGHT_PAREN);
            auto expr = factor();
            return std::make_unique<AST::Expr>(AST::CastExpr(type, std::move(expr)));
        }
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

