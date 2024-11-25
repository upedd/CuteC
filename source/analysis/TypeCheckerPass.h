#ifndef TYPECHECKERPASS_H
#define TYPECHECKERPASS_H

#include "../Ast.h"


struct IntType {
};

struct FunctionType {
    int param_count;
};

using Type = std::variant<IntType, FunctionType>;

struct Symbol {
    Type type;
    bool defined = false;
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
