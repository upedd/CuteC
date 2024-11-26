#ifndef TYPECHECKERPASS_H
#define TYPECHECKERPASS_H

#include "../Ast.h"


struct IntType {
};

struct FunctionType {
    int param_count;
};

using Type = std::variant<IntType, FunctionType>;



struct Initial {
    int value;
};

struct NoInitializer {};
struct Tentative {};

using InitialValue = std::variant<Tentative, Initial, NoInitializer>

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
    Type type;
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

    void check_block(const std::vector<AST::BlockItem> &block);

    void file_scope_variable_declaration(const AST::FunctionDecl &decl);

    void local_variable_declaration(const AST::VariableDecl &decl);

    void check_function_decl(const AST::FunctionDecl &function);

    void check_program(const AST::Program &program);

    void run();

    void check_decl(const AST::DeclHandle &item);

    void check_expr(const AST::Expr &expr);

    void check_variable_decl(const AST::VariableDecl &decl);

    void check_function_call(const AST::FunctionCall &expr);

    void check_variable_expr(const AST::VariableExpr &expr);

    void check_stmt(const AST::Stmt &item);

    std::vector<Error> errors;

    std::unordered_map<std::string, Symbol> symbols;

private:
    AST::Program *program;
};


#endif //TYPECHECKERPASS_H
