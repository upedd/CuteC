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
            for (const auto &decl: program.declarations) {
                visit_decl(*decl);
            }
        });
        println(")");
    }

    void function(const FunctionDecl &function) {
        println("FunctionDecl(");
        with_indent([this, &function] {
            println(std::format("name=\"{}\",", function.name));
            if (function.body) {
                println("body=[");
                with_indent([this, &function] {
                    for (const auto &item: *function.body) {
                        block_item(item);
                    }
                });
                println("]");
            }
        });
        println(")");
    }

    void block_item(const BlockItem &item) {
        std::visit(overloaded{
                       [this](const StmtHandle &item) {
                           visit_stmt(*item);
                       },
                       [this](const DeclHandle &item) {
                           visit_decl(*item);
                       }
                   }, item);
    }


    void visit_decl(const Declaration &decl) {
        std::visit(overloaded{
                       [this](const VariableDecl &decl) {
                           visit_variable_decl(decl);
                       },
                       [this](const FunctionDecl &decl) {
                           function(decl);
                       }
                   }, decl);
    }

    void visit_variable_decl(const VariableDecl &decl) {
        println("VariableDeclaration(");
        with_indent([this, &decl] {
            println(std::format("name={},", decl.name));
            println("value=");
            if (decl.expr) {
                visit_expr(*decl.expr);
            } else {
                println("null");
            }
        });
        println(")");
    }

    void if_stmt(const AST::IfStmt &stmt) {
        println("IfStmt(");
        with_indent([this, &stmt] {
            println("condition=");
            with_indent([this, &stmt] {
                visit_expr(*stmt.condition);
            });
            println("then=");
            with_indent([this, &stmt] {
                visit_stmt(*stmt.then_stmt);
            });
            println("else=");
            with_indent([this, &stmt] {
                if (stmt.else_stmt) {
                    visit_stmt(*stmt.else_stmt);
                } else {
                    println("null");
                }
            });
        });
        println(")");
    }

    void labeled_stmt(const AST::LabeledStmt &stmt) {
        println("LabeledStmt(");
        with_indent([this, &stmt] {
            println("label=" + stmt.label);
            println("stmt=");
            with_indent([this, &stmt] {
                visit_stmt(*stmt.stmt);
            });
        });
        println(")");
    }

    void goto_stmt(const AST::GoToStmt &stmt) {
        println("GoToStmt(label=" + stmt.label + ")");
    }

    void compound_stmt(const AST::CompoundStmt &stmt) {
        println("CompoundStmt(");
        with_indent([this, &stmt] {
            for (const auto &item: stmt.body) {
                block_item(item);
            }
        });
        println(")");
    }

    void while_stmt(const WhileStmt &stmt) {
        println("WhileStmt(");
        with_indent([this, &stmt] {
            println("condition=");
            with_indent([this, &stmt] {
                visit_expr(*stmt.condition);
            });
            println("body=");
            with_indent([this, &stmt] {
                visit_stmt(*stmt.body);
            });
        });
        println(")");
    }

    void do_while_stmt(const DoWhileStmt &stmt) {
        println("DoWhileStmt(");
        with_indent([this, &stmt] {
            println("condition=");
            with_indent([this, &stmt] {
                visit_expr(*stmt.condition);
            });
            println("body=");
            with_indent([this, &stmt] {
                visit_stmt(*stmt.body);
            });
        });
        println(")");
    }

    void for_stmt(const ForStmt &stmt) {
        println("ForStmt(");
        with_indent([this, &stmt] {
            println("init=");
            with_indent([this, &stmt] {
                std::visit(overloaded{
                               [this](const std::unique_ptr<VariableDecl> &decl) {
                                   if (decl) {
                                       visit_variable_decl(*decl);
                                   } else {
                                       println("null");
                                   }
                               },
                               [this](const ExprHandle &expr) {
                                   if (expr) {
                                       visit_expr(*expr);
                                   } else {
                                       println("null");
                                   }
                               },
                           }, stmt.init);
            });
            println("condition=");
            with_indent([this, &stmt] {
                if (stmt.condition) {
                    visit_expr(*stmt.condition);
                } else {
                    println("null");
                }
            });
            println("condition=");
            with_indent([this, &stmt] {
                if (stmt.post) {
                    visit_expr(*stmt.post);
                } else {
                    println("null");
                }
            });
            println("body=");
            with_indent([this, &stmt] {
                visit_stmt(*stmt.body);
            });
        });
        println(")");
    }

    void switch_stmt(const AST::SwitchStmt &stmt) {
        println("SwitchStmt(");
        with_indent([this, &stmt] {
            println("expr=");
            with_indent([this, &stmt] {
                visit_expr(*stmt.expr);
            });
            println("body=");
            with_indent([this, &stmt] {
                visit_stmt(*stmt.body);
            });
        });
        println(")");
    }

    void case_stmt(const AST::CaseStmt &stmt) {
        println("CaseStmt(");
        with_indent([this, &stmt] {
            println("value=");
            with_indent([this, &stmt] {
                visit_expr(*stmt.value);
            });
            println("stmt=");
            with_indent([this, &stmt] {
                visit_stmt(*stmt.stmt);
            });
        });
        println(")");
    }

    void default_stmt(const AST::DefaultStmt &stmt) {
        println("DefaultStmt(");
        with_indent([this, &stmt] {
            println("stmt=");
            with_indent([this, &stmt] {
                visit_stmt(*stmt.stmt);
            });
        });
        println(")");
    }

    void visit_stmt(const Stmt &stmt) {
        std::visit(overloaded{
                       [this](const ReturnStmt &stmt) {
                           return_stmt(stmt);
                       },
                       [this](const ExprStmt &stmt) {
                           visit_expr(*stmt.expr);
                       },
                       [this](const NullStmt &stmt) {
                           println("NullStmt");
                       },
                       [this](const IfStmt &stmt) {
                           if_stmt(stmt);
                       },
                       [this](const LabeledStmt &stmt) {
                           labeled_stmt(stmt);
                       },
                       [this](const GoToStmt &stmt) {
                           goto_stmt(stmt);
                       },
                       [this](const CompoundStmt &stmt) {
                           compound_stmt(stmt);
                       },
                       [this](const WhileStmt &stmt) {
                           while_stmt(stmt);
                       },
                       [this](const DoWhileStmt &stmt) {
                           do_while_stmt(stmt);
                       },
                       [this](const ForStmt &stmt) {
                           for_stmt(stmt);
                       },
                       [this](const BreakStmt &stmt) {
                           println("BreakStmt()");
                       },
                       [this](const ContinueStmt &stmt) {
                           println("ContinueStmt()");
                       },
                       [this](const SwitchStmt &stmt) {
                           switch_stmt(stmt);
                       },
                       [this](const CaseStmt &stmt) {
                           case_stmt(stmt);
                       },
                       [this](const DefaultStmt &stmt) {
                           default_stmt(stmt);
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

    void conditional_expr(const AST::ConditionalExpr &expr) {
        println("ConditionalExpr(");
        with_indent([this, &expr] {
            println("condition=");
            with_indent([this, &expr] {
                visit_expr(*expr.condition);
            });
            println("then=");
            with_indent([this, &expr] {
                visit_expr(*expr.then_expr);
            });
            println("else=");
            with_indent([this, &expr] {
                visit_expr(*expr.else_expr);
            });
        });
        println(")");
    }

    void function_call_expr(const AST::FunctionCall &expr) {
        println("FunctionCallExpr(");
        with_indent([this, &expr] {
            println("name=" + expr.identifier);
            println("args=[");
            with_indent([this, &expr] {
                for (const auto &arg: expr.arguments) {
                    visit_expr(*arg);
                }
            });
        });
        println(")");
    }

    void address_of_expr(const AST::AddressOfExpr & expr) {
        println("AddressOfExpr(");
        with_indent([this, &expr] {
            println("expr=");
            with_indent([this, &expr] {
                visit_expr(*expr.expr);
            });
        });
        println(")");
    }

    void dereference_expr(const AST::DereferenceExpr & expr) {
        println("DereferenceExpr(");
        with_indent([this, &expr] {
            println("expr=");
            with_indent([this, &expr] {
                visit_expr(*expr.expr);
            });
        });
        println(")");
    }

    void compound_expr(const AST::CompoundExpr & expr) {
        println("CompoundExpr(");
        with_indent([this, &expr] {
            for (const auto &expr: expr.exprs) {
                visit_expr(*expr);
            }
        });
        println(")");
    }

    void temporary_expr(const AST::TemporaryExpr & expr) {
        println("TemporaryExpr(");
        with_indent([this, &expr] {
            println("name=" + expr.identifier);
            if (expr.init) {
                println("expr=");
                with_indent([this, &expr] {
                    visit_expr(*expr.init);
                });
            }
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
                       },
                       [this](const VariableExpr &expr) {
                           variable_expr(expr);
                       },
                       [this](const AssigmentExpr &expr) {
                           assignment_expr(expr);
                       },
                       [this](const ConditionalExpr &expr) {
                           conditional_expr(expr);
                       },
                       [this](const FunctionCall &expr) {
                           function_call_expr(expr);
                       },
                        [this](const AddressOfExpr& expr) {
                            address_of_expr(expr);
                        },
                        [this](const DereferenceExpr &expr) {
                            dereference_expr(expr);
                        },
                        [this](const CompoundExpr& expr) {
                            compound_expr(expr);
                        },
                        [this](const TemporaryExpr& expr) {
                            temporary_expr(expr);
                        },
                        [this](const CastExpr& expr) {}
                   }, expr);
    }

    void variable_expr(const VariableExpr &expr) {
        println(std::format("VariableExpr(name={})", expr.name));
    }

    void assignment_expr(const AssigmentExpr &expr) {
        println("AssigmentExpr(");
        with_indent([this, &expr] {
            println("left=");
            visit_expr(*expr.left);
            println("right=");
            visit_expr(*expr.right);
        });
        println(")");
    }

    void constant_expr(const ConstantExpr &expr) {
        println(std::format("Constant({})", std::visit(overloaded{[](const auto& c) -> std::int64_t {return c.value;}}, expr.constant)));
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
                case UnaryExpr::Kind::PREFIX_INCREMENT:
                    return "prefix increment";
                case UnaryExpr::Kind::PREFIX_DECREMENT:
                    return "prefix decrement";
                case UnaryExpr::Kind::POSTFIX_DECREMENT:
                    return "postfix decrement";
                case UnaryExpr::Kind::POSTFIX_INCREMENT:
                    return "postfix increment";
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
