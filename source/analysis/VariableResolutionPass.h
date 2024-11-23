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

    VariableResolutionPass(AST::Program *program) : program(program) {
    }

    void resolve_function(AST::Function &function);

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

    std::string declare(const std::string &name);

    std::optional<std::string> resolve(const std::string &name);

    void resolve_compound(AST::CompoundStmt &stmt);

    void resolve_for(AST::ForStmt & stmt);

    std::vector<Error> errors;

private:
    std::vector<std::unordered_map<std::string, std::string> > variables;
    AST::Program *program;
    int cnt = 0;
};


#endif //VARIABLERESOLUTIONPASS_H
