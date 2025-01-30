#include "Parser.h"

#include <algorithm>
#include <ranges>

#include "Token.h"
#include "Token.h"
#include "analysis/TypeCheckerPass.h"

static bool is_type_specifier(Token::Type type) {
    return type == Token::Type::INT || type == Token::Type::LONG || type == Token::Type::UNSIGNED || type ==
           Token::Type::SIGNED || type == Token::Type::DOUBLE || type == Token::Type::CHAR || type == Token::Type::VOID;
}

static bool is_specifier(Token::Type type) {
    return type == Token::Type::STATIC || type == Token::Type::EXTERN || is_type_specifier(type);
}

// mess
static bool contains_only_unique_specifiers(const std::vector<Token::Type> &types) {
    auto copy = types;
    std::sort(copy.begin(), copy.end());
    return std::adjacent_find(copy.begin(), copy.end()) == copy.end();
}

AST::Type Parser::parse_type(const std::vector<Token::Type> &types) {
    if (types == std::vector{Token::Type::DOUBLE}) {
        return AST::DoubleType{};
    }
    if (std::find(types.begin(), types.end(), Token::Type::VOID) != types.end()) {
        if (types.size() > 1) {
            errors.emplace_back("Invalid type specifier");
        }
        return AST::VoidType{};
    }
    // mess!
    if (std::find(types.begin(), types.end(), Token::Type::CHAR) != types.end()) {
        if (std::find(types.begin(), types.end(), Token::Type::UNSIGNED) != types.end()) {
            if (types.size() != 2) {
                errors.emplace_back("Invalid type specifier");
            }
            return AST::UCharType{};
        }
        if (std::find(types.begin(), types.end(), Token::Type::SIGNED) != types.end()) {
            if (types.size() != 2) {
                errors.emplace_back("Invalid type specifier");
            }
            return AST::SignedCharType{};
        }
        if (types.size() != 1) {
            errors.emplace_back("Invalid type specifier");
        }
        return AST::CharType{};
    }

    // mess
    if (std::find(types.begin(), types.end(), Token::Type::DOUBLE) != types.end() || types.empty() || !
        contains_only_unique_specifiers(types) || (
            std::find(types.begin(), types.end(), Token::Type::UNSIGNED) != types.end() && std::find(
                types.begin(), types.end(), Token::Type::SIGNED) != types.end())) {
        errors.emplace_back("Invalid Type Specifier");
        return AST::IntType{}; // how to handle failure?
    }
    bool is_long = std::find(types.begin(), types.end(), Token::Type::LONG) != types.end();
    bool is_unsigned = std::find(types.begin(), types.end(), Token::Type::UNSIGNED) != types.end();
    if (is_long && is_unsigned) {
        return AST::ULongType{};
    }
    if (is_long) {
        return AST::LongType{};
    }

    if (is_unsigned) {
        return AST::UIntType{};
    }
    return AST::IntType{};
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
    return {std::move(type), storage_classes[0]};
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

AST::FunctionDecl Parser::function_decl(const std::string &name, const AST::Type &type,
                                        const AST::StorageClass storage_class, const std::vector<std::string> &params) {
    if (match(Token::Type::SEMICOLON)) {
        return AST::FunctionDecl(name, params, {}, type, storage_class);
    }
    expect(Token::Type::LEFT_BRACE);
    return AST::FunctionDecl(name, params, block(), type, storage_class);
}


AST::BlockItem Parser::block_item() {
    if (is_specifier(peek().type)) {
        return declaration();
    }
    return statement();
}

AST::DeclaratorHandle Parser::simple_declarator() {
    if (match(Token::Type::LEFT_PAREN)) {
        auto declarator = parse_declarator();
        expect(Token::Type::RIGHT_PAREN);
        return declarator;
    }
    auto identifier = expect(Token::Type::IDENTIFIER);
    return std::make_unique<AST::Declarator>(AST::Identifier(identifier.lexeme));
}

// TODO: use elsewhere!
std::vector<AST::Param> Parser::parse_param_list() {
    if (peek().type == Token::Type::VOID && peek(1).type == Token::Type::RIGHT_PAREN) {
        consume(); // void
        consume(); // right paren
        return {};
    }
    std::vector<AST::Param> params;
    do {
        std::vector<Token::Type> types;
        while (is_type_specifier(peek().type)) {
            types.emplace_back(consume().type);
        }
        auto type = parse_type(types);
        auto declarator = parse_declarator();
        params.emplace_back(std::move(declarator), AST::TypeHandle(std::move(type)));
    } while (match(Token::Type::COMMA));
    expect(Token::Type::RIGHT_PAREN);
    return params;
}

AST::DeclaratorHandle Parser::parse_direct_declarator() {
    auto declarator = simple_declarator();
    if (match(Token::Type::LEFT_PAREN)) {
        auto params = parse_param_list();
        return std::make_unique<AST::Declarator>(AST::FunctionDeclarator(std::move(declarator), std::move(params)));
    }
    if (match(Token::Type::LEFT_BRACKET)) {
        do {
            if (peek().type == Token::Type::CONSTANT || peek().type == Token::Type::LONG_CONSTANT || peek().type ==
                Token::Type::UNSIGNED_INT_CONSTANT || peek().type == Token::Type::UNSIGNED_LONG_CONSTANT || peek().type
                == Token::Type::CHAR_CONSTANT) {
                auto c = constant(consume()); // TODO: refactor and get constant directly
                expect(Token::Type::RIGHT_BRACKET);
                std::uint64_t value;
                std::visit(overloaded{
                               [](const AST::ConstDouble &) {
                                   std::unreachable();
                               },
                               [&value](const auto &c) {
                                   value = c.value;
                               }
                           }, std::get<AST::ConstantExpr>(*c).constant);
                declarator = std::make_unique<AST::Declarator>(AST::ArrayDeclarator(std::move(declarator), value));
            } else {
                errors.emplace_back("Expected an integer constant in a array size declaration");
            }
        } while (match(Token::Type::LEFT_BRACKET));
    }
    return std::move(declarator);
}

AST::DeclaratorHandle Parser::parse_declarator() {
    if (match(Token::Type::ASTERISK)) {
        return std::make_unique<AST::Declarator>(AST::PointerDeclarator(parse_declarator()));
    }
    return parse_direct_declarator();
}

std::tuple<std::string, AST::TypeHandle, std::vector<std::string> > Parser::process_declarator(
    const AST::Declarator &declarator, const AST::Type &type) {
    return std::visit(overloaded{
                          [&type](
                      const AST::Identifier &identifier) -> std::tuple<std::string, AST::TypeHandle, std::vector<
                      std::string> > {
                              return {identifier.name, type, {}};
                          },
                          [&type, this](
                      const AST::PointerDeclarator &pointer) -> std::tuple<std::string, AST::TypeHandle, std::vector<
                      std::string> > {
                              return process_declarator(*pointer.declarator, AST::PointerType(type));
                          },
                          [&type, this](
                      const AST::FunctionDeclarator &function) -> std::tuple<std::string, AST::TypeHandle, std::vector<
                      std::string> > {
                              if (!std::holds_alternative<AST::Identifier>(*function.declarator)) {
                                  errors.emplace_back("Invalid function declarator");
                              }
                              std::vector<std::string> param_names;
                              std::vector<AST::TypeHandle> param_types;
                              for (const auto &param: function.params) {
                                  auto [param_name, param_type, _] = process_declarator(*param.declarator, *param.type);
                                  if (std::holds_alternative<AST::FunctionType>(*param_type)) {
                                      errors.emplace_back("Function pointers in parameters aren't supported!");
                                  }
                                  param_names.emplace_back(param_name);
                                  param_types.emplace_back(param_type);
                              }

                              auto name = std::get<AST::Identifier>(*function.declarator).name;
                              auto fun_type = AST::FunctionType{param_types, type};
                              return {name, {fun_type}, param_names};
                          },
                          [&type, this](const AST::ArrayDeclarator &array) {
                              return process_declarator(*array.declarator, AST::ArrayType(type, array.size));
                          }
                      }, declarator);
}

AST::DeclHandle Parser::declaration() {
    auto type_and_storage_class = parse_type_and_storage_class();
    auto declarator = parse_declarator();
    auto [name, type, params] = process_declarator(*declarator, type_and_storage_class.first);
    if (std::holds_alternative<AST::FunctionType>(*type)) {
        return std::make_unique<AST::Declaration>(function_decl(name, *type, type_and_storage_class.second, params));
    }
    return std::make_unique<AST::Declaration>(variable_declaration(name, *type, type_and_storage_class.second));
}

AST::InitializerHandle Parser::initializer() {
    if (match(Token::Type::LEFT_BRACE)) {
        std::vector<AST::InitializerHandle> init;
        do {
            if (!init.empty() && peek().type == Token::Type::RIGHT_BRACE) break; // trailling comma
            init.emplace_back(initializer());
        } while (match(Token::Type::COMMA));
        expect(Token::Type::RIGHT_BRACE);
        return std::make_unique<AST::Initializer>(AST::CompoundInit(std::move(init)));
    }
    return std::make_unique<AST::Initializer>(AST::ScalarInit(expression()));
}

AST::VariableDecl Parser::variable_declaration(const std::string &name, const AST::Type &type,
                                               const AST::StorageClass storage_class) {
    AST::InitializerHandle expr;
    if (match(Token::Type::EQUAL)) {
        expr = initializer();
    }
    expect(Token::Type::SEMICOLON);
    return AST::VariableDecl(name, std::move(expr), type, storage_class);
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
        AST::ExprHandle expr;
        if (peek().type != Token::Type::SEMICOLON) {
            expr = expression();
        }
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
    auto left = unary();
    auto token = peek();
    while (is_binary_operator(token.type) && get_precedence(token.type) >= min_precedence) {
        if (get_precedence(token.type) == Precedence::ASSIGMENT) {
            auto op = compound_operator();
            auto right = expression(get_precedence(token.type));
            if (op) {
                // TODO: move desugaring into separate pass
                std::vector<AST::ExprHandle> build;
                // only dereference and variable allowed as lhs
                if (std::holds_alternative<AST::DereferenceExpr>(*left)) {
                    auto &dereference_expr = std::get<AST::DereferenceExpr>(*left);
                    auto temp_var = make_temp_name();
                    build.emplace_back(
                        std::make_unique<AST::Expr>(AST::TemporaryExpr(temp_var, std::move(dereference_expr.expr))));
                    auto lvalue = std::make_unique<AST::Expr>(
                        AST::DereferenceExpr(std::make_unique<AST::Expr>(AST::VariableExpr(temp_var))));
                    auto lhs = std::make_unique<AST::Expr>(
                        AST::DereferenceExpr(std::make_unique<AST::Expr>(AST::VariableExpr(temp_var))));
                    auto binary = std::make_unique<AST::Expr>(AST::BinaryExpr(*op, std::move(lhs), std::move(right)));
                    build.emplace_back(
                        std::make_unique<AST::Expr>(AST::AssigmentExpr(std::move(lvalue), std::move(binary))));
                } else if (std::holds_alternative<AST::VariableExpr>(*left)) {
                    auto &var_expr = std::get<AST::VariableExpr>(*left);
                    auto lhs = std::make_unique<AST::Expr>(AST::VariableExpr(var_expr.name));
                    auto binary = std::make_unique<AST::Expr>(AST::BinaryExpr(*op, std::move(lhs), std::move(right)));
                    build.emplace_back(
                        std::make_unique<AST::Expr>(AST::AssigmentExpr(std::move(left), std::move(binary))));
                } else if (std::holds_alternative<AST::SubscriptExpr>(*left)) {
                    auto address_of_expr = std::make_unique<AST::Expr>(AST::AddressOfExpr(std::move(left)));
                    auto temp_var = make_temp_name();
                    build.emplace_back(
                        std::make_unique<AST::Expr>(AST::TemporaryExpr(temp_var, std::move(address_of_expr))));
                    auto lvalue = std::make_unique<AST::Expr>(
                        AST::DereferenceExpr(std::make_unique<AST::Expr>(AST::VariableExpr(temp_var))));
                    auto lhs = std::make_unique<AST::Expr>(
                        AST::DereferenceExpr(std::make_unique<AST::Expr>(AST::VariableExpr(temp_var))));
                    auto binary = std::make_unique<AST::Expr>(AST::BinaryExpr(*op, std::move(lhs), std::move(right)));
                    build.emplace_back(
                        std::make_unique<AST::Expr>(AST::AssigmentExpr(std::move(lvalue), std::move(binary))));
                }
                left = std::make_unique<AST::Expr>(AST::CompoundExpr(std::move(build)));
            } else {
                left = std::make_unique<AST::Expr>(AST::AssigmentExpr(std::move(left), std::move(right)));
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

char get_escape_sequence_value(char c) {
    switch (c) {
        case '\'':
            return '\'';
        case '"':
            return '"';
        case '?':
            return '?';
        case '\\':
            return '\\';
        case 'a':
            return '\a';
        case 'b':
            return '\b';
        case 'f':
            return '\f';
        case 'n':
            return '\n';
        case 'r':
            return '\r';
        case 't':
            return '\t';
        case 'v':
            return '\v';
        default:
            std::unreachable();
    }
}

AST::ExprHandle Parser::constant(const Token &token) {
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
        } catch (const std::out_of_range &) {
            errors.emplace_back("Constant too large!");
            // graceful errors?
        }
    } else if (token.type == Token::Type::UNSIGNED_INT_CONSTANT) {
        try {
            std::uint64_t val = std::stoull(token.lexeme);
            if (val <= std::numeric_limits<std::uint32_t>::max()) {
                return std::make_unique<AST::Expr>(AST::ConstantExpr(AST::ConstUInt(val)));
            }
            return std::make_unique<AST::Expr>(AST::ConstantExpr(AST::ConstULong(val)));
        } catch (const std::out_of_range &) {
            errors.emplace_back("Constant too large!");
            // graceful errors?
        }
    } else if (token.type == Token::Type::LONG_CONSTANT) {
        try {
            return std::make_unique<AST::Expr>(AST::ConstantExpr(AST::ConstLong(std::stoll(token.lexeme))));
        } catch (const std::out_of_range &) {
            errors.emplace_back("Constant too large!");
            // graceful errors?
        }
    } else if (token.type == Token::Type::CHAR_CONSTANT) {
        if (token.lexeme[1] == '\\') {
            return std::make_unique<AST::Expr>(
                AST::ConstantExpr(AST::ConstInt(get_escape_sequence_value(token.lexeme[2]))));
        }
        return std::make_unique<AST::Expr>(AST::ConstantExpr(AST::ConstInt(token.lexeme[1])));
    } else {
        try {
            return std::make_unique<AST::Expr>(AST::ConstantExpr(AST::ConstInt(std::stoi(token.lexeme))));
        } catch (const std::out_of_range &) {
            try {
                return std::make_unique<AST::Expr>(AST::ConstantExpr(AST::ConstLong(std::stoll(token.lexeme))));
            } catch (const std::out_of_range &) {
                errors.emplace_back("Constant too large!");
                // graceful errors?
            }
        }
    }
}

AST::AbstractDeclaratorHandle Parser::parse_abstract_declarator() {
    if (match(Token::Type::ASTERISK)) {
        return std::make_unique<AST::AbstractDeclarator>(AST::AbstractPointer(parse_abstract_declarator()));
    }
    auto abstract_declarator = std::make_unique<AST::AbstractDeclarator>(AST::AbstractBase());
    if (match(Token::Type::LEFT_PAREN)) {
        abstract_declarator = parse_abstract_declarator();
        expect(Token::Type::RIGHT_PAREN);
    }
    // overlap!
    if (match(Token::Type::LEFT_BRACKET)) {
        do {
            if (peek().type == Token::Type::CONSTANT || peek().type == Token::Type::LONG_CONSTANT || peek().type ==
                Token::Type::UNSIGNED_INT_CONSTANT || peek().type == Token::Type::UNSIGNED_LONG_CONSTANT || peek().type
                == Token::Type::CHAR_CONSTANT) {
                auto c = constant(consume()); // TODO: refactor and get constant directly
                expect(Token::Type::RIGHT_BRACKET);
                std::uint64_t value;
                std::visit(overloaded{
                               [](const AST::ConstDouble &) {
                                   std::unreachable();
                               },
                               [&value](const auto &c) {
                                   value = c.value;
                               }
                           }, std::get<AST::ConstantExpr>(*c).constant);
                abstract_declarator = std::make_unique<AST::AbstractDeclarator>(
                    AST::AbstractArray(std::move(abstract_declarator), value));
            } else {
                errors.emplace_back("Expected an integer constant in a array size declaration");
            }
        } while (match(Token::Type::LEFT_BRACKET));
    }
    return abstract_declarator;
}

AST::TypeHandle Parser::process_abstract_declarator(const AST::AbstractDeclarator &declarator, const AST::Type &type) {
    return std::visit(overloaded{
                          [&type, this](const AST::AbstractPointer &ptr) {
                              return process_abstract_declarator(*ptr.declarator, AST::PointerType(type));
                          },
                          [&type](const AST::AbstractBase &) {
                              return AST::TypeHandle{type};
                          },
                          [&type, this](const AST::AbstractArray &array) {
                              return process_abstract_declarator(*array.declarator, AST::ArrayType(type, array.size));
                          }
                      }, declarator);
}

std::string parse_string_literal(const Token &token) {
    // assert token.type == STRING_LITERAL
    std::string result;
    for (int i = 1; i < token.lexeme.size() - 1; ++i) {
        // skip quotes
        if (token.lexeme[i] == '\\') {
            result += get_escape_sequence_value(token.lexeme[++i]);
        } else {
            result += token.lexeme[i];
        }
    }

    return result;
}

AST::ExprHandle Parser::primary(const Token &token) {
    if (token.type == Token::Type::IDENTIFIER) {
        if (match(Token::Type::LEFT_PAREN)) {
            return std::make_unique<AST::Expr>(
                AST::FunctionCall(token.lexeme, arguments_list()));
        }
        return std::make_unique<AST::Expr>(AST::VariableExpr(token.lexeme));
    }
    if (token.type == Token::Type::CONSTANT || token.type == Token::Type::LONG_CONSTANT || token.type ==
        Token::Type::UNSIGNED_INT_CONSTANT || token.type == Token::Type::UNSIGNED_LONG_CONSTANT || token.type ==
        Token::Type::FLOATING_POINT_CONSTANT || token.type == Token::Type::CHAR_CONSTANT) {
        return constant(token);
    }
    if (token.type == Token::Type::STRING_LITERAL) {
        std::string string = parse_string_literal(token);
        while (peek().type == Token::Type::STRING_LITERAL) {
            string += parse_string_literal(peek());
            consume();
        }
        return std::make_unique<AST::Expr>(AST::StringExpr(string));
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

AST::ExprHandle Parser::unary() {
    Token token = consume();
    // TODO: correct?
    if (token.type == Token::Type::LEFT_PAREN && is_type_specifier(peek().type)) {
        std::vector<Token::Type> types;
        while (is_type_specifier(peek().type)) {
            types.emplace_back(consume().type);
        }
        auto base_type = parse_type(types);
        auto declarator = parse_abstract_declarator();
        auto type = process_abstract_declarator(*declarator, base_type);
        expect(Token::Type::RIGHT_PAREN);
        auto expr = unary();
        return std::make_unique<AST::Expr>(AST::CastExpr(type, std::move(expr)));
    }
    return factor(token);
}

AST::ExprHandle Parser::factor(const Token &token) {
    // TODO: refactor!

    if (token.type == Token::Type::MINUS) {
        auto expr = unary();
        return std::make_unique<AST::Expr>(AST::UnaryExpr(AST::UnaryExpr::Kind::NEGATE, std::move(expr)));
    }
    if (token.type == Token::Type::TILDE) {
        auto expr = unary();
        return std::make_unique<AST::Expr>(AST::UnaryExpr(AST::UnaryExpr::Kind::COMPLEMENT, std::move(expr)));
    }
    if (token.type == Token::Type::BANG) {
        auto expr = unary();
        return std::make_unique<AST::Expr>(AST::UnaryExpr(AST::UnaryExpr::Kind::LOGICAL_NOT, std::move(expr)));
    }

    if (token.type == Token::Type::ASTERISK) {
        auto expr = unary();
        return std::make_unique<AST::Expr>(AST::DereferenceExpr(std::move(expr)));
    }

    if (token.type == Token::Type::AND) {
        auto expr = unary();
        return std::make_unique<AST::Expr>(AST::AddressOfExpr(std::move(expr)));
    }

    if (token.type == Token::Type::PLUS_PLUS) {
        auto expr = unary();
        return std::make_unique<AST::Expr>(AST::UnaryExpr(AST::UnaryExpr::Kind::PREFIX_INCREMENT, std::move(expr)));
    }
    if (token.type == Token::Type::MINUS_MINUS) {
        auto expr = unary();
        return std::make_unique<AST::Expr>(AST::UnaryExpr(AST::UnaryExpr::Kind::PREFIX_DECREMENT, std::move(expr)));
    }


    if (token.type == Token::Type::SIZEOF) {
        if (peek().type == Token::Type::LEFT_PAREN && is_type_specifier(peek(1).type)) {
            consume();
            // duplicate from cast!
            std::vector<Token::Type> types;
            while (is_type_specifier(peek().type)) {
                types.emplace_back(consume().type);
            }
            auto base_type = parse_type(types);
            auto declarator = parse_abstract_declarator();
            auto type = process_abstract_declarator(*declarator, base_type);
            expect(Token::Type::RIGHT_PAREN);
            return std::make_unique<AST::Expr>(AST::SizeOfTypeExpr(type));
        }
        auto t = consume();
        auto expr = factor(t);
        return std::make_unique<AST::Expr>(AST::SizeOfExpr(std::move(expr)));
    }

    auto primary_expr = primary(token);
    if (primary_expr) {
        if (match(Token::Type::PLUS_PLUS)) {
            primary_expr = std::make_unique<AST::Expr>(
                AST::UnaryExpr(AST::UnaryExpr::Kind::POSTFIX_INCREMENT, std::move(primary_expr)));
        }
        if (match(Token::Type::MINUS_MINUS)) {
            primary_expr = std::make_unique<AST::Expr>(
                AST::UnaryExpr(AST::UnaryExpr::Kind::POSTFIX_DECREMENT, std::move(primary_expr)));
        }
        if (match(Token::Type::LEFT_BRACKET)) {
            do {
                auto expr = expression();
                expect(Token::Type::RIGHT_BRACKET);
                primary_expr = std::make_unique<
                    AST::Expr>(AST::SubscriptExpr(std::move(primary_expr), std::move(expr)));
            } while (match(Token::Type::LEFT_BRACKET));
        }
        // mess!
        if (match(Token::Type::PLUS_PLUS)) {
            primary_expr = std::make_unique<AST::Expr>(
                AST::UnaryExpr(AST::UnaryExpr::Kind::POSTFIX_INCREMENT, std::move(primary_expr)));
        }
        if (match(Token::Type::MINUS_MINUS)) {
            primary_expr = std::make_unique<AST::Expr>(
                AST::UnaryExpr(AST::UnaryExpr::Kind::POSTFIX_DECREMENT, std::move(primary_expr)));
        }
        return primary_expr;
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

