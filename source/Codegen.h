#ifndef CODEGEN_H
#define CODEGEN_H
#include "AsmTree.h"
#include "Ast.h"
#include "overloaded.h"

// ast -> asm tree

class Codegen {
public:
    explicit Codegen(AST::Program program) : astProgram(std::move(program)) {}

    void generate() {
        asmProgram = program(astProgram);
    }

    ASM::Program program(const AST::Program& program) {
        return ASM::Program(function(program.function));
    }

    ASM::Function function(const AST::Function& function) {
        return ASM::Function(function.name, visit_stmt(*function.body));
    }

    std::vector<ASM::Instruction> visit_stmt(const AST::Stmt& stmt) {
        return std::visit(overloaded {
            [this](const AST::ReturnStmt& stmt) {
                return return_stmt(stmt);
            }
        }, stmt);
    }

    std::vector<ASM::Instruction> return_stmt(const AST::ReturnStmt& stmt) {
        return {ASM::Mov(visit_expr(*stmt.expr), ASM::Register()), ASM::Ret()};
    }

    ASM::Operand visit_expr(const AST::Expr& expr) {
        return std::visit(overloaded {
            [this](const AST::ConstantExpr& expr) {
                return constant_expr(expr);
            }
            }, expr);
    }

    ASM::Operand constant_expr(const AST::ConstantExpr& expr) {
        return ASM::Imm(expr.value);
    }

    ASM::Program asmProgram;
private:
    AST::Program astProgram;
};



#endif //CODEGEN_H
