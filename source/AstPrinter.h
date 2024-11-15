#ifndef ASTPRINTER_H
#define ASTPRINTER_H
#include <format>
#include <iostream>

#include "Ast.h"
#include "overloaded.h"

using namespace AST;

class AstPrinter {
public:
    void program(const Program &program) {
        println("Program(");
        with_indent([this, &program] {
            function(program.function);
        });
        println(")");
    }

    void function(const Function &function) {
        println("Function(");
        with_indent([this, &function] {
            println(std::format("name=\"{}\",", function.name));
            println("body=");
            with_indent([this, &function] {
                visit_stmt(*function.body);
            });
        });
        println(")");
    }

    void visit_stmt(const Stmt &stmt) {
        std::visit(overloaded{
                       [this](const ReturnStmt &stmt) {
                           return_stmt(stmt);
                       }
                   }, stmt);
    }

    void return_stmt(const ReturnStmt &stmt) {
        println("Return(");
        with_indent([this, &stmt] {
            visit_expr(*stmt.expr);
        });
        println(")");
    }

    void visit_expr(const Expr &expr) {
        std::visit(overloaded{
                       [this](const ConstantExpr &expr) {
                           constant_expr(expr);
                       },
                       [this](const UnaryExpr &expr) {
                           unary_expr(expr);
                       },
                       [this](const BinaryExpr &expr) {
                           binary_expr(expr);
                       }
                   }, expr);
    }

    void constant_expr(const ConstantExpr &expr) {
        println(std::format("Constant({})", expr.value));
    }

    void unary_expr(const UnaryExpr &expr) {
        static auto operator_to_string = [](const UnaryExpr::Kind &kind) {
            switch (kind) {
                case UnaryExpr::Kind::NEGATE:
                    return "negate";
                case UnaryExpr::Kind::COMPLEMENT:
                    return "complement";
                case UnaryExpr::Kind::LOGICAL_NOT:
                    return "logical-not";
            }
        };
        println("Unary(");
        with_indent([this, &expr] {
            println(std::format("operator={}", operator_to_string(expr.kind)));
            println("expr=");
            with_indent([this, &expr] {
                visit_expr(*expr.expr);
            });
        });
        println(")");
    }


    void binary_expr(const BinaryExpr &expr) {
        static auto operator_to_string = [](const BinaryExpr::Kind &kind) {
            switch (kind) {
                case BinaryExpr::Kind::ADD:
                    return "add";
                case BinaryExpr::Kind::SUBTRACT:
                    return "subtract";
                case BinaryExpr::Kind::MULTIPLY:
                    return "multiply";
                case BinaryExpr::Kind::DIVIDE:
                    return "divide";
                case BinaryExpr::Kind::REMAINDER:
                    return "remainder";
                case BinaryExpr::Kind::SHIFT_LEFT:
                    return "shift-left";
                case BinaryExpr::Kind::SHIFT_RIGHT:
                    return "shift-right";
                case BinaryExpr::Kind::BITWISE_AND:
                    return "bitwise-and";
                case BinaryExpr::Kind::BITWISE_OR:
                    return "bitwise-or";
                case BinaryExpr::Kind::BITWISE_XOR:
                    return "bitwise-xor";
                case BinaryExpr::Kind::LOGICAL_AND:
                    return "logical-and";
                case BinaryExpr::Kind::LOGICAL_OR:
                    return "logical-or";
                case BinaryExpr::Kind::EQUAL:
                    return "equal";
                case BinaryExpr::Kind::NOT_EQUAL:
                    return "not-equal";
                case BinaryExpr::Kind::GREATER:
                    return "greater";
                case BinaryExpr::Kind::GREATER_EQUAL:
                    return "greater-equal";
                case BinaryExpr::Kind::LESS:
                    return "less";
                case BinaryExpr::Kind::LESS_EQUAL:
                    return "less_equal";
            }
        };
        println("Binary(");
        with_indent([this, &expr] {
            println(std::format("operator={}", operator_to_string(expr.kind)));
            println("left=");
            with_indent([this, &expr] {
                visit_expr(*expr.left);
            });
            println("right=");
            with_indent([this, &expr] {
                visit_expr(*expr.right);
            });
        });
        println(")");
    }

private:
    void with_indent(const auto &fn) {
        m_depth++;
        fn();
        m_depth--;
    }

    void print(const std::string &str) {
        for (int i = 0; i < m_depth; i++) {
            std::cout << "  ";
        }
        std::cout << str;
    }

    void println(const std::string &str) {
        print(str);
        print("\n");
    }

    int m_depth = 0;
};

#endif //ASTPRINTER_H
