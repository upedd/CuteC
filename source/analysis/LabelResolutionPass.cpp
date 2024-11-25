
#include "LabelResolutionPass.h"

#include "../overloaded.h"

// ensure all stmts are visited!

void LabelResolutionPass::resolve_stmt(AST::Stmt &stmt) {
    std::visit(overloaded{
                   [this](AST::GoToStmt &stmt) {
                       resolve_goto(stmt);
                   },
                   [this](AST::LabeledStmt &stmt) {
                       resolve_labeled(stmt);
                   },
                   [this](AST::IfStmt &stmt) {
                       resolve_stmt(*stmt.then_stmt);
                       if (stmt.else_stmt) {
                           resolve_stmt(*stmt.else_stmt);
                       }
                   },
                   [this](AST::CompoundStmt &stmt) {
                       resolve_block(stmt.body);
                   },
                   [this](AST::WhileStmt &stmt) {
                       resolve_stmt(*stmt.body);
                   },
                   [this](AST::ForStmt &stmt) {
                       resolve_stmt(*stmt.body);
                   },
                   [this](AST::DoWhileStmt &stmt) {
                       resolve_stmt(*stmt.body);
                   },
                   [this](AST::SwitchStmt &stmt) {
                       resolve_stmt(*stmt.body);
                   },
                   [this](AST::CaseStmt &stmt) {
                       resolve_stmt(*stmt.stmt);
                   },
                   [this](AST::DefaultStmt &stmt) {
                       resolve_stmt(*stmt.stmt);
                   },
                   [](const auto &) {
                   }
               }, stmt);
}

void LabelResolutionPass::resolve_function(AST::FunctionDecl &function) {
    auto prev = current_function_name;
    current_function_name = function.name;
    if (function.body) {
        resolve_block(*function.body);
    }
    current_function_name = prev;
}

void LabelResolutionPass::resolve_block(std::vector<AST::BlockItem> &block) {
    for (const auto &item: block) {
        if (std::holds_alternative<AST::StmtHandle>(item)) {
            resolve_stmt(*std::get<AST::StmtHandle>(item));
        }
    }
}

void LabelResolutionPass::resolve_program(AST::Program &program) {
    for (auto &function: program.functions) {
        resolve_function(function);
        for (const auto &label: unresolved_labels) {
            errors.emplace_back("Label \"" + label + "\" could not be resolved");
        }
        unresolved_labels.clear();
        labels.clear();
    }
}

void LabelResolutionPass::run() {
    resolve_program(*program);
}

void LabelResolutionPass::resolve_goto(AST::GoToStmt &stmt) {
    if (!labels.contains(stmt.label)) {
        unresolved_labels.insert(stmt.label);
    }
    stmt.label = current_function_name + "." + stmt.label;
}

void LabelResolutionPass::resolve_labeled(AST::LabeledStmt &stmt) {
    if (labels.contains(stmt.label)) {
        errors.emplace_back("The label '" + stmt.label + "' is already in use.");
    }
    if (unresolved_labels.contains(stmt.label)) {
        unresolved_labels.erase(stmt.label);
    }
    labels[stmt.label] = current_function_name + "." + stmt.label;
    stmt.label = current_function_name + "." + stmt.label;
    resolve_stmt(*stmt.stmt);
}
