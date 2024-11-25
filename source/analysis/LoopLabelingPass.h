#ifndef LOOPLABELINGPASS_H
#define LOOPLABELINGPASS_H
#include "../Ast.h"


class LoopLabelingPass {
public:
    class Error {
    public:
        std::string message;
    };

    explicit LoopLabelingPass(AST::Program *program) : program(program) {
    };

    void label_stmt(AST::Stmt &stmt);

    void label_block(std::vector<AST::BlockItem> &body);

    void label_function(AST::FunctionDecl &function);

    void label_program(AST::Program &program);

    void run();

    std::string get_label_for_break();

    std::string get_label_for_continue();

    std::vector<Error> errors;

private:
    struct Context {
        enum class Kind {
            LOOP,
            SWITCH
        };

        Kind kind;
        std::string label;
    };

    int cnt = 0;
    std::vector<Context> context_stack;
    AST::Program *program;
};


#endif //LOOPLABELINGPASS_H
