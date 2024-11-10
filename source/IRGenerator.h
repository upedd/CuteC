#ifndef IRGENERATOR_H
#define IRGENERATOR_H
#include "Ast.h"
#include "IR.h"
#include "overloaded.h"

// ast -> ir

class IRGenerator {
public:
    explicit IRGenerator(AST::Program program) : astProgram(std::move(program)) {
    }

    void generate() {
        IRProgram = IR::Program(gen_function(astProgram.function));
    }

    IR::Function gen_function(const AST::Function &function) {
        return IR::Function(function.name, gen_stmt(*function.body));
    }

    std::vector<IR::Instruction> gen_stmt(const AST::Stmt &stmt) {
        return std::visit(overloaded{
                              [this](const AST::ReturnStmt &stmt) {
                                  return gen_return(stmt);
                              }
                          }, stmt);
    }

    std::vector<IR::Instruction> gen_return(const AST::ReturnStmt &stmt) {
        std::vector<IR::Instruction> instructions;
        instructions.emplace_back(IR::Return(gen_expr(*stmt.expr, instructions)));
        return instructions;
    }

    IR::Value gen_expr(const AST::Expr &expr, std::vector<IR::Instruction> &instructions) {
        return std::visit(overloaded{
                              [this](const AST::ConstantExpr &expr) {
                                  return gen_constant(expr);
                              },
                              [this, &instructions](const AST::UnaryExpr &expr) {
                                  return gen_unary(expr, instructions);
                              }
                          }, expr);
    }

    IR::Value gen_constant(const AST::ConstantExpr &expr) {
        return IR::Constant(expr.value);
    }

    IR::Value gen_unary(const AST::UnaryExpr &expr, std::vector<IR::Instruction> &instructions) {
        static auto convert_op = [](const AST::UnaryExpr::Kind &kind) {
            switch (kind) {
                case AST::UnaryExpr::Kind::NEGATE:
                    return IR::Unary::Operator::NEGATE;
                case AST::UnaryExpr::Kind::COMPLEMENT:
                    return IR::Unary::Operator::COMPLEMENT;
            }
        };

        auto source = gen_expr(*expr.expr, instructions);
        auto destination = IR::Variable(make_temporary());
        instructions.emplace_back(IR::Unary(convert_op(expr.kind), source, destination));
        return destination;
    }

    std::string make_temporary() {
        return "tmp." + (m_tmp_counter++);
    }

    IR::Program IRProgram;

private:
    int m_tmp_counter = 0;

    AST::Program astProgram;
};

#endif //IRGENERATOR_H
