#ifndef TYPECHECKERPASS_H
#define TYPECHECKERPASS_H

#include "../Ast.h"

struct Initial {
    int value;
};

struct NoInitializer {};
struct Tentative {};

using InitialValue = std::variant<Tentative, Initial, NoInitializer>;

struct FunctionAttributes {
    bool defined;
    bool global;
};

struct StaticAttributes {
    InitialValue initial_value;
    bool global;
};

struct LocalAttributes {};

using IdentifierAttributes = std::variant<FunctionAttributes, StaticAttributes, LocalAttributes>;

struct Symbol {
    AST::Type* type;
    IdentifierAttributes attributes;
};

class TypeCheckerPass {
public:
    class Error {
    public:
        std::string message;
    };

    explicit TypeCheckerPass(AST::Program *program) : program(program) {
    };

    void check_block(std::vector<AST::BlockItem> &block);

    void file_scope_variable_declaration(AST::VariableDecl &decl);

    void local_variable_declaration(AST::VariableDecl &decl);

    void check_function_decl(AST::FunctionDecl &function);

    void check_program(AST::Program &program);

    void run();

    void check_decl(AST::DeclHandle &item);

    void check_expr(AST::Expr &expr);


    void check_function_call(AST::FunctionCall &expr);

    void check_variable_expr(AST::VariableExpr &expr);

    void check_stmt(AST::Stmt &item);

    std::vector<Error> errors;

    std::unordered_map<std::string, Symbol> symbols;

private:
    AST::Program *program;
};


#endif //TYPECHECKERPASS_H
