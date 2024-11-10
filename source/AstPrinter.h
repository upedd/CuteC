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
