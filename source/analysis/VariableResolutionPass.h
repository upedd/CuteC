#ifndef VARIABLERESOLUTIONPASS_H
#define VARIABLERESOLUTIONPASS_H
#include <optional>
#include <unordered_map>

#include "../Ast.h"


class VariableResolutionPass {
public:
    class Error {
    public:
        std::string message;
    };


    struct Identifier {
        std::string name;
        bool is_external = false;
    };

    VariableResolutionPass(AST::Program *program) : program(program) {
    }


    void resolve_block(std::vector<AST::BlockItem> &block);

    void resolve_program(AST::Program &program);

    void run();

    std::string make_temporary(const std::string &original_name);

    void resolve_declaration(AST::Declaration &decl);

    void resolve_statement(AST::Stmt &stmt);

    void resolve_expression(AST::Expr &expr);

    void resolve_conditional(AST::ConditionalExpr &expr);

    void resolve_unary(AST::UnaryExpr &expr);

    void resolve_binary(AST::BinaryExpr &expr);

    void resolve_assigment(AST::AssigmentExpr &expr);

    void resolve_variable(AST::VariableExpr &expr);

    VariableResolutionPass::Identifier declare(const Identifier &identifier);

    std::optional<std::string> resolve(const std::string &name);

    void resolve_compound(AST::CompoundStmt &stmt);

    void resolve_for(AST::ForStmt &stmt);

    void resolve_function_call(AST::FunctionCall &expr);

    void resolve_function_declaration(AST::FunctionDecl &decl);

    void resolve_variable_decl(AST::VariableDecl &decl);

    std::vector<Error> errors;

private:
    std::vector<std::unordered_map<std::string, Identifier> > variables;
    AST::Program *program;
    int cnt = 0;
};


#endif //VARIABLERESOLUTIONPASS_H
