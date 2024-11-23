#ifndef SWITCHRESOLUTIONPASS_H
#define SWITCHRESOLUTIONPASS_H
#include "../Ast.h"


class SwitchResolutionPass {
public:

    class Error {
    public:
        std::string message;
    };

    SwitchResolutionPass(AST::Program* program) : program(program) {};

    void resolve_function(AST::Function &function);

    void resolve_stmt(AST::Stmt &stmt);

    void resolve_block(std::vector<AST::BlockItem> &block);

    void resolve_program(AST::Program &program);

    void run();

    void case_stmt(AST::CaseStmt &stmt);

    void default_stmt(AST::DefaultStmt &stmt);

    std::string make_label(const AST::SwitchStmt &stmt);

    std::vector<Error> errors;
private:
    int cnt = 0;
    std::vector<AST::SwitchStmt*> switches;
    AST::Program* program;
};



#endif //SWITCHRESOLUTIONPASS_H
