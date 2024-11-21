#ifndef IRGENERATOR_H
#define IRGENERATOR_H
#include "Ast.h"
#include "IR.h"
#include "overloaded.h"
#include "Parser.h"

// ast -> ir

class IRGenerator {
public:
    explicit IRGenerator(AST::Program program) : astProgram(std::move(program)) {
    }

    void generate() {
        IRProgram = IR::Program(gen_function(astProgram.function));
    }

    IR::Function gen_function(const AST::Function &function) {
        std::vector<IR::Instruction> instructions;
        for (const auto &item: function.body) {
            auto res = gen_block_item(item);
            instructions.insert(instructions.end(), res.begin(), res.end());
        }

        // implicitly add return 0 to the end of the function.
        instructions.emplace_back(IR::Return(IR::Constant(0)));

        return IR::Function(function.name, std::move(instructions));
    }

    std::vector<IR::Instruction> gen_block_item(const AST::BlockItem &item) {
        return std::visit(overloaded{
                              [this](const AST::StmtHandle &item) {
                                  return gen_stmt(*item);
                              },
                              [this](const AST::DeclarationHandle &item) {
                                  return gen_declaration(*item);
                              }
                          }, item);
    }

    std::vector<IR::Instruction> gen_declaration(const AST::Declaration &decl) {
        std::vector<IR::Instruction> instructions;
        if (decl.expr) {
            instructions.emplace_back(IR::Copy(gen_expr(*decl.expr, instructions), IR::Variable(decl.name)));
        }
        return instructions;
    }

    std::vector<IR::Instruction> gen_stmt(const AST::Stmt &stmt) {
        return std::visit(overloaded{
                              [this](const AST::ReturnStmt &stmt) {
                                  return gen_return(stmt);
                              },
                              [this](const AST::ExprStmt &stmt) {
                                  std::vector<IR::Instruction> instructions;
                                  gen_expr(*stmt.expr, instructions);
                                  return instructions;
                              },
                              [this](const auto &) {
                                  return std::vector<IR::Instruction>();
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
                              },
                              [this, &instructions](const AST::VariableExpr &expr) {
                                  return gen_variable(expr, instructions);
                              },
                              [this, &instructions](const AST::AssigmentExpr &expr) {
                                  return gen_assigment(expr, instructions);
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
                case AST::UnaryExpr::Kind::LOGICAL_NOT:
                    return IR::Unary::Operator::LOGICAL_NOT;
            }
        };
        auto source = gen_expr(*expr.expr, instructions);
        // TODO: refactor!
        if (expr.kind == AST::UnaryExpr::Kind::PREFIX_INCREMENT) {
            instructions.emplace_back(IR::Binary(IR::Binary::Operator::ADD, source, IR::Constant(1), source));
            return source;
        }
        if (expr.kind == AST::UnaryExpr::Kind::PREFIX_DECREMENT) {
            instructions.emplace_back(IR::Binary(IR::Binary::Operator::ADD, source, IR::Constant(-1), source));
            return source;
        }
        if (expr.kind == AST::UnaryExpr::Kind::POSTFIX_INCREMENT) {
            auto temp = IR::Variable(make_temporary());
            instructions.emplace_back(IR::Copy(source, temp));
            instructions.emplace_back(IR::Binary(IR::Binary::Operator::ADD, source, IR::Constant(1), source));
            return temp;
        }
        if (expr.kind == AST::UnaryExpr::Kind::POSTFIX_DECREMENT) {
            auto temp = IR::Variable(make_temporary());
            instructions.emplace_back(IR::Copy(source, temp));
            instructions.emplace_back(IR::Binary(IR::Binary::Operator::ADD, source, IR::Constant(-1), source));
            return temp;
        }
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
                case AST::BinaryExpr::Kind::LESS:
                    return IR::Binary::Operator::LESS;
                case AST::BinaryExpr::Kind::GREATER:
                    return IR::Binary::Operator::GREATER;
                case AST::BinaryExpr::Kind::EQUAL:
                    return IR::Binary::Operator::EQUAL;
                case AST::BinaryExpr::Kind::NOT_EQUAL:
                    return IR::Binary::Operator::NOT_EQUAL;
                case AST::BinaryExpr::Kind::GREATER_EQUAL:
                    return IR::Binary::Operator::GREATER_EQUAL;
                case AST::BinaryExpr::Kind::LESS_EQUAL:
                    return IR::Binary::Operator::LESS_EQUAL;
            }
        };
        if (expr.kind == AST::BinaryExpr::Kind::LOGICAL_AND) {
            return gen_logical_and(expr, instructions);
        }
        if (expr.kind == AST::BinaryExpr::Kind::LOGICAL_OR) {
            return gen_logical_or(expr, instructions);
        }

        auto left = gen_expr(*expr.left, instructions);
        auto right = gen_expr(*expr.right, instructions);
        auto destination = IR::Variable(make_temporary());
        instructions.emplace_back(IR::Binary(convert_op(expr.kind), left, right, destination));

        return destination;
    }

    IR::Value gen_logical_and(const AST::BinaryExpr &expr, std::vector<IR::Instruction> &instructions) {
        auto false_label = make_label("false");
        auto end_label = make_label("end");

        auto left = gen_expr(*expr.left, instructions);
        instructions.emplace_back(IR::JumpIfZero(left, false_label));
        auto right = gen_expr(*expr.right, instructions);
        instructions.emplace_back(IR::JumpIfZero(right, false_label));
        auto result = IR::Variable(make_temporary());
        instructions.emplace_back(IR::Copy(IR::Constant(1), result));
        instructions.emplace_back(IR::Jump(end_label));
        instructions.emplace_back(IR::Label(false_label));
        instructions.emplace_back(IR::Copy(IR::Constant(0), result));
        instructions.emplace_back(IR::Label(end_label));
        return result;
    }

    IR::Value gen_logical_or(const AST::BinaryExpr &expr, std::vector<IR::Instruction> &instructions) {
        auto true_label = make_label("true");
        auto end_label = make_label("end");

        auto left = gen_expr(*expr.left, instructions);
        instructions.emplace_back(IR::JumpIfNotZero(left, true_label));
        auto right = gen_expr(*expr.right, instructions);
        instructions.emplace_back(IR::JumpIfNotZero(right, true_label));
        auto result = IR::Variable(make_temporary());
        instructions.emplace_back(IR::Copy(IR::Constant(0), result));
        instructions.emplace_back(IR::Jump(end_label));
        instructions.emplace_back(IR::Label(true_label));
        instructions.emplace_back(IR::Copy(IR::Constant(1), result));
        instructions.emplace_back(IR::Label(end_label));
        return result;
    }

    IR::Value gen_variable(const AST::VariableExpr &expr, std::vector<IR::Instruction> &instructions) {
        return IR::Variable(expr.name);
    }

    IR::Value gen_assigment(const AST::AssigmentExpr &expr, std::vector<IR::Instruction> &instructions) {
        static auto convert_op = [](const AST::AssigmentExpr::Operator &op) {
            switch (op) {
                case AST::AssigmentExpr::Operator::ADD:
                    return IR::Binary::Operator::ADD;
                case AST::AssigmentExpr::Operator::SUBTRACT:
                    return IR::Binary::Operator::SUBTRACT;
                case AST::AssigmentExpr::Operator::MULTIPLY:
                    return IR::Binary::Operator::MULTIPLY;
                case AST::AssigmentExpr::Operator::DIVIDE:
                    return IR::Binary::Operator::DIVIDE;
                case AST::AssigmentExpr::Operator::REMAINDER:
                    return IR::Binary::Operator::REMAINDER;
                case AST::AssigmentExpr::Operator::BITWISE_AND:
                    return IR::Binary::Operator::BITWISE_AND;
                case AST::AssigmentExpr::Operator::BITWISE_OR:
                    return IR::Binary::Operator::BITWISE_OR;
                case AST::AssigmentExpr::Operator::BITWISE_XOR:
                    return IR::Binary::Operator::BITWISE_XOR;
                case AST::AssigmentExpr::Operator::SHIFT_LEFT:
                    return IR::Binary::Operator::SHIFT_LEFT;
                case AST::AssigmentExpr::Operator::SHIFT_RIGHT:
                    return IR::Binary::Operator::SHIFT_RIGHT;
                default:
                    std::unreachable();
            }
        };

        auto result = gen_expr(*expr.right, instructions);
        auto lhs = IR::Variable(std::get<AST::VariableExpr>(*expr.left).name);
        if (expr.op != AST::AssigmentExpr::Operator::NONE) {
            //auto temp = IR::Variable(make_temporary());
            instructions.emplace_back(IR::Binary(convert_op(expr.op), lhs, result, lhs));
            //result = temp;
        } else {
            instructions.emplace_back(IR::Copy(result, lhs));
        }
        return lhs;
    }

    std::string make_temporary() {
        return "tmp." + std::to_string(m_tmp_counter++);
    }

    std::string make_label(const std::string &name = "tmp") {
        return name + "." + std::to_string(m_label_counter++);
    }

    IR::Program IRProgram;

private:
    int m_tmp_counter = 0;
    int m_label_counter = 0;

    AST::Program astProgram;
};

#endif //IRGENERATOR_H
