#include "VariableResolutionPass.h"

#include <format>

#include "../overloaded.h"
#include <ranges>

void VariableResolutionPass::resolve_function(AST::Function &function) {
    variables.emplace_back();
    resolve_block(function.body);
    variables.pop_back();
}

void VariableResolutionPass::resolve_block(std::vector<AST::BlockItem> &block) {
    for (auto &item: block) {
        std::visit(overloaded{
                       [this](AST::DeclarationHandle &item) {
                           resolve_declaration(*item);
                       },
                       [this](AST::StmtHandle &item) {
                           resolve_statement(*item);
                       }
                   }, item);
    }
}

void VariableResolutionPass::resolve_program(AST::Program &program) {
    resolve_function(program.function);
}

void VariableResolutionPass::run() {
    resolve_program(*program);
}

std::string VariableResolutionPass::make_temporary(const std::string &original_name) {
    return std::format("local.{}.{}", original_name, cnt++);
}

void VariableResolutionPass::resolve_declaration(AST::Declaration &decl) {
    decl.name = declare(decl.name);
    if (decl.expr) {
        resolve_expression(*decl.expr);
    }
}

void VariableResolutionPass::resolve_statement(AST::Stmt &stmt) {
    std::visit(overloaded{
                   [this](AST::ReturnStmt &stmt) {
                       resolve_expression(*stmt.expr);
                   },
                   [this](AST::ExprStmt &stmt) {
                       resolve_expression(*stmt.expr);
                   },
                   [this](AST::IfStmt &stmt) {
                       resolve_expression(*stmt.condition);
                       resolve_statement(*stmt.then_stmt);
                       if (stmt.else_stmt) {
                           resolve_statement(*stmt.else_stmt);
                       }
                   },
                   [this](AST::LabeledStmt &stmt) {
                       resolve_statement(*stmt.stmt);
                   },
                   [this](AST::CompoundStmt &stmt) {
                       resolve_compound(stmt);
                   },
                   [](auto &) {
                   }
               }, stmt);
}

void VariableResolutionPass::resolve_expression(AST::Expr &expr) {
    std::visit(overloaded{
                   [this](AST::UnaryExpr &expr) {
                       resolve_unary(expr);
                   },
                   [this](AST::BinaryExpr &expr) {
                       resolve_binary(expr);
                   },
                   [this](AST::AssigmentExpr &expr) {
                       resolve_assigment(expr);
                   },
                   [this](AST::VariableExpr &expr) {
                       resolve_variable(expr);
                   },
                   [this](AST::ConditionalExpr &expr) {
                       resolve_conditional(expr);
                   },
                   [this](auto &) {
                   }
               }, expr);
}

void VariableResolutionPass::resolve_conditional(AST::ConditionalExpr &expr) {
    resolve_expression(*expr.condition);
    resolve_expression(*expr.else_expr);
    resolve_expression(*expr.then_expr);
}

void VariableResolutionPass::resolve_unary(AST::UnaryExpr &expr) {
    if (expr.kind == AST::UnaryExpr::Kind::POSTFIX_DECREMENT || expr.kind == AST::UnaryExpr::Kind::POSTFIX_INCREMENT ||
        expr.kind == AST::UnaryExpr::Kind::PREFIX_INCREMENT || expr.kind == AST::UnaryExpr::Kind::PREFIX_DECREMENT) {
        if (!std::holds_alternative<AST::VariableExpr>(*expr.expr)) {
            errors.emplace_back("Expected an lvalue.");
        }
    }

    resolve_expression(*expr.expr);
}

void VariableResolutionPass::resolve_binary(AST::BinaryExpr &expr) {
    resolve_expression(*expr.left);
    resolve_expression(*expr.right);
}

void VariableResolutionPass::resolve_assigment(AST::AssigmentExpr &expr) {
    if (!std::holds_alternative<AST::VariableExpr>(*expr.left)) {
        errors.emplace_back("Expected an lvalue.");
    }
    resolve_expression(*expr.left);
    resolve_expression(*expr.right);
}

void VariableResolutionPass::resolve_variable(AST::VariableExpr &expr) {
    if (auto resolved = resolve(expr.name); !resolved) {
        errors.emplace_back("Undeclared variable: \"" + expr.name + "\"!");
    } else {
        expr.name = *resolved;
    }
}

std::string VariableResolutionPass::declare(const std::string &name) {
    if (variables.back().contains(name)) {
        errors.emplace_back("Variable already declared in current scope: \"" + name + "\"!");
        // should fail?
    }
    return variables.back()[name] = make_temporary(name);
}

std::optional<std::string> VariableResolutionPass::resolve(const std::string &name) {
    for (auto &scope: variables | std::views::reverse) {
        if (scope.contains(name)) {
            return scope[name];
        }
    }
    return {};
}

void VariableResolutionPass::resolve_compound(AST::CompoundStmt &stmt) {
    variables.emplace_back();
    resolve_block(stmt.body);
    variables.pop_back();
}
