#include "VariableResolutionPass.h"

#include <format>

#include "../overloaded.h"

void VariableResolutionPass::resolve_function(AST::Function &function) {
    for (auto &block_item: function.body) {
        std::visit(overloaded{
                       [this](AST::DeclarationHandle &item) {
                           resolve_declaration(*item);
                       },
                       [this](AST::StmtHandle &item) {
                           resolve_statement(*item);
                       }
                   }, block_item);
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
    if (variables.contains(decl.name)) {
        errors.emplace_back("Duplicate variable declaration: \"" + decl.name + "\"!");
        return; // should fail?
    }
    std::string unique_name = make_temporary(decl.name);
    variables.emplace(decl.name, unique_name);
    if (decl.expr) {
        resolve_expression(*decl.expr);
    }
    decl.name = unique_name;
}

void VariableResolutionPass::resolve_statement(AST::Stmt &stmt) {
    std::visit(overloaded{
                   [this](AST::ReturnStmt &stmt) {
                       resolve_expression(*stmt.expr);
                   },
                   [this](AST::ExprStmt &stmt) {
                       resolve_expression(*stmt.expr);
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
                   [this](auto &) {
                   }
               }, expr);
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
    if (!variables.contains(expr.name)) {
        errors.emplace_back("Undeclared variable: \"" + expr.name + "\"!");
    } else {
        expr.name = variables[expr.name];
    }
}
