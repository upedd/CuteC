#ifndef PARSER_H
#define PARSER_H
#include <memory>
#include <utility>
#include <vector>

#include "Ast.h"
#include "Token.h"
#include "analysis/TypeCheckerPass.h"


class Parser {
public:
    class Error {
    public:
        std::string message;
    };

    explicit Parser(std::vector<Token> tokens) : m_tokens(std::move(tokens)) {
    }

    AST::Type parse_type(const std::vector<Token::Type> &types);

    std::pair<AST::Type, AST::StorageClass> parse_type_and_storage_class();

    void parse();

    AST::FunctionDecl function_decl(const std::string &name, const AST::Type &type, AST::StorageClass storage_class, const std::vector<std::string> &params);

    AST::BlockItem block_item();

    AST::DeclaratorHandle simple_declarator();

    std::vector<AST::Param> parse_param_list();

    AST::DeclaratorHandle parse_direct_declarator();

    AST::DeclaratorHandle parse_declarator();

    std::tuple<std::string, AST::TypeHandle, std::vector<std::string>> process_declarator(
        const AST::Declarator &declarator, const AST::Type &type);

    AST::DeclHandle declaration();

    std::unique_ptr<AST::Initializer> initializer();

    AST::VariableDecl variable_declaration(const std::string &name, const AST::Type &type, AST::StorageClass storage_class);

    AST::StmtHandle labeled_stmt(const Token &identifer);

    AST::StmtHandle goto_stmt();

    std::vector<AST::BlockItem> block();

    AST::StmtHandle while_stmt();

    AST::StmtHandle do_while_stmt();

    AST::StmtHandle for_stmt();

    AST::StmtHandle case_stmt();

    AST::StmtHandle switch_stmt();

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

    std::optional<AST::BinaryExpr::Kind> compound_operator();

    AST::BinaryExpr::Kind binary_operator();

    AST::ExprHandle constant(const Token &token);

    AST::AbstractDeclaratorHandle parse_abstract_declarator();

    AST::TypeHandle process_abstract_declarator(const AST::AbstractDeclarator &declarator, const AST::Type &type);

    AST::ExprHandle primary(const Token &token);

    std::vector<AST::ExprHandle> arguments_list();

    AST::ExprHandle unary();

    AST::ExprHandle factor(const Token &token);

    AST::Program program{};
    std::vector<Error> errors{};

    // used for variables used in desugaring
    std::string make_temp_name() {
        return "desugar." + std::to_string(temp_cnt++);
    }

    int temp_cnt = 0;

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
