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
                              },
                              [this, &instructions](const AST::BinaryExpr &expr) {
                                  return gen_binary(expr, instructions);
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

    IR::Value gen_binary(const AST::BinaryExpr &expr, std::vector<IR::Instruction> &instructions) {
        static auto convert_op = [](const AST::BinaryExpr::Kind &kind) {
            switch (kind) {
                case AST::BinaryExpr::Kind::ADD:
                    return IR::Binary::Operator::ADD;
                case AST::BinaryExpr::Kind::SUBTRACT:
                    return IR::Binary::Operator::SUBTRACT;
                case AST::BinaryExpr::Kind::MULTIPLY:
                    return IR::Binary::Operator::MULTIPLY;
                case AST::BinaryExpr::Kind::REMAINDER:
                    return IR::Binary::Operator::REMAINDER;
                case AST::BinaryExpr::Kind::DIVIDE:
                    return IR::Binary::Operator::DIVIDE;
                case AST::BinaryExpr::Kind::SHIFT_LEFT:
                    return IR::Binary::Operator::SHIFT_LEFT;
                case AST::BinaryExpr::Kind::SHIFT_RIGHT:
                    return IR::Binary::Operator::SHIFT_RIGHT;
                case AST::BinaryExpr::Kind::BITWISE_AND:
                    return IR::Binary::Operator::BITWISE_AND;
                case AST::BinaryExpr::Kind::BITWISE_OR:
                    return IR::Binary::Operator::BITWISE_OR;
                case AST::BinaryExpr::Kind::BITWISE_XOR:
                    return IR::Binary::Operator::BITWISE_XOR;
            }
        };
        auto left = gen_expr(*expr.left, instructions);
        auto right = gen_expr(*expr.right, instructions);
        auto destination = IR::Variable(make_temporary());
        instructions.emplace_back(IR::Binary(convert_op(expr.kind), left, right, destination));

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
