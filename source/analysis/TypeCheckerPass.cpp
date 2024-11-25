//
// Created by Miłosz Koczorowski on 23/11/2024.
//

#include "TypeCheckerPass.h"

#include <functional>

#include "../overloaded.h"

void TypeCheckerPass::check_block(const std::vector<AST::BlockItem> &block) {
    for (const auto &item: block) {
        std::visit(overloaded{
                       [this](const AST::DeclHandle &item) {
                           check_decl(item);
                       },
                       [this](const AST::StmtHandle &item) {
                           check_stmt(*item);
                       }
                   }, item);
    }
}

void TypeCheckerPass::check_function_decl(const AST::FunctionDecl &function) {
    auto type = FunctionType(function.params.size());
    bool already_defined = false;
    if (symbols.contains(function.name)) {
        auto old_decl = symbols[function.name];
        if (old_decl.type.index() != Type(type).index() || std::get<FunctionType>(old_decl.type).param_count != function
            .params.size()) {
            errors.emplace_back("Incompatible function declaration: " + function.name);
        }
        already_defined = old_decl.defined;
        if (already_defined && function.body) {
            errors.emplace_back("Function redefinition: " + function.name);
        }
    }
    symbols[function.name] = Symbol(type, already_defined || function.body);

    if (function.body) {
        for (const auto &param: function.params) {
            symbols[param] = Symbol(IntType());
        }
        check_block(*function.body);
    }
}

void TypeCheckerPass::check_program(const AST::Program &program) {
    for (const auto &function: program.functions) {
        check_function_decl(function);
    }
}

void TypeCheckerPass::run() {
    check_program(*program);
}

void TypeCheckerPass::check_decl(const AST::DeclHandle &item) {
    std::visit(overloaded{
                   [this](const AST::VariableDecl &decl) {
                       check_variable_decl(decl);
                   },
                   [this](const AST::FunctionDecl &decl) {
                       check_function_decl(decl);
                   }
               }, *item);
}

void TypeCheckerPass::check_expr(const AST::Expr &expr) {
    std::visit(overloaded{
                   [this](const AST::FunctionCall &expr) {
                       check_function_call(expr);
                   },
                   [this](const AST::VariableExpr &expr) {
                       check_variable_expr(expr);
                   },
                   [this](const AST::UnaryExpr &expr) {
                       check_expr(*expr.expr);
                   },
                   [this](const AST::BinaryExpr &expr) {
                       check_expr(*expr.left);
                       check_expr(*expr.right);
                   },
                   [this](const AST::AssigmentExpr &expr) {
                       check_expr(*expr.left);
                       check_expr(*expr.right);
                   },
                   [this](const AST::ConditionalExpr &expr) {
                       check_expr(*expr.condition);
                       check_expr(*expr.then_expr);
                       check_expr(*expr.else_expr);
                   },
                   [this](const AST::ConstantExpr &) {
                   }
               }, expr);
}

void TypeCheckerPass::check_variable_decl(const AST::VariableDecl &decl) {
    symbols[decl.name] = Symbol(IntType());
    if (decl.expr) {
        check_expr(*decl.expr);
    }
}

void TypeCheckerPass::check_function_call(const AST::FunctionCall &expr) {
    auto type = symbols[expr.identifier].type;
    if (type.index() == Type(IntType()).index()) {
        errors.emplace_back("Variable used as function name");
    }
    if (std::get<FunctionType>(type).param_count != expr.arguments.size()) {
        errors.emplace_back("Function called with wrong number of arguments");
    }
    for (const auto &arg: expr.arguments) {
        check_expr(*arg);
    }
}

void TypeCheckerPass::check_variable_expr(const AST::VariableExpr &expr) {
    if (symbols[expr.name].type.index() != Type(IntType()).index()) {
        errors.emplace_back("Function used as variable");
    }
}

void TypeCheckerPass::check_stmt(const AST::Stmt &item) {
    std::visit(overloaded{
                   [this](const AST::ReturnStmt &stmt) {
                       check_expr(*stmt.expr);
                   },
                   [this](const AST::ExprStmt &stmt) {
                       check_expr(*stmt.expr);
                   },
                   [this](const AST::NullStmt &) {
                   },
                   [this](const AST::IfStmt &stmt) {
                       check_expr(*stmt.condition);
                       check_stmt(*stmt.then_stmt);
                       if (stmt.else_stmt) {
                           check_stmt(*stmt.else_stmt);
                       }
                   },
                   [this](const AST::LabeledStmt &stmt) {
                       check_stmt(*stmt.stmt);
                   },
                   [this](const AST::GoToStmt &) {
                   },
                   [this](const AST::CompoundStmt &stmt) {
                       check_block(stmt.body);
                   },
                   [this](const AST::BreakStmt &) {
                   },
                   [this](const AST::ContinueStmt &) {
                   },
                   [this](const AST::WhileStmt &stmt) {
                       check_expr(*stmt.condition);
                       check_stmt(*stmt.body);
                   },
                   [this](const AST::DoWhileStmt &stmt) {
                       check_expr(*stmt.condition);
                       check_stmt(*stmt.body);
                   },
                   [this](const AST::ForStmt &stmt) {
                       std::visit(overloaded{
                                      [this](const std::unique_ptr<AST::VariableDecl> &decl) {
                                          if (!decl) return;
                                          check_variable_decl(*decl);
                                      },
                                      [this](const AST::ExprHandle &expr) {
                                          if (!expr) return;
                                          check_expr(*expr);
                                      }
                                  }, stmt.init);
                       if (stmt.condition) {
                           check_expr(*stmt.condition);
                       }
                       if (stmt.post) {
                           check_expr(*stmt.post);
                       }
                       check_stmt(*stmt.body);
                   },
                   [this](const AST::SwitchStmt &stmt) {
                       check_expr(*stmt.expr);
                       check_stmt(*stmt.body);
                   },
                   [this](const AST::CaseStmt &stmt) {
                       check_stmt(*stmt.stmt);
                   },
                   [this](const AST::DefaultStmt &stmt) {
                       check_stmt(*stmt.stmt);
                   }
               }, item);
}
