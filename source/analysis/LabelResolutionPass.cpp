
#include "LabelResolutionPass.h"

#include "../overloaded.h"

// ensure all stmts are visited!

void LabelResolutionPass::resolve_stmt(const AST::Stmt &stmt) {
    std::visit(overloaded{
                   [this](const AST::GoToStmt &stmt) {
                       resolve_goto(stmt);
                   },
                   [this](const AST::LabeledStmt &stmt) {
                       resolve_labeled(stmt);
                   },
                   [this](const AST::IfStmt &stmt) {
                       resolve_stmt(*stmt.then_stmt);
                       if (stmt.else_stmt) {
                           resolve_stmt(*stmt.else_stmt);
                       }
                   },
                   [this](const AST::CompoundStmt &stmt) {
                       resolve_block(stmt.body);
                   },
                    [this](const AST::WhileStmt &stmt) {
                        resolve_stmt(*stmt.body);
                    },
                    [this](const AST::ForStmt &stmt) {
                        resolve_stmt(*stmt.body);
                    },
                    [this](const AST::DoWhileStmt& stmt) {
                        resolve_stmt(*stmt.body);
                    },
        [this](const AST::SwitchStmt &stmt) {
            resolve_stmt(*stmt.body);
        },
        [this](const AST::CaseStmt& stmt) {
            resolve_stmt(*stmt.stmt);
        },
        [this](const AST::DefaultStmt &stmt) {
            resolve_stmt(*stmt.stmt);
        },
                   [](const auto &) {
                   }
               }, stmt);
}

void LabelResolutionPass::resolve_function(const AST::Function &function) {
    resolve_block(function.body);
}

void LabelResolutionPass::resolve_block(const std::vector<AST::BlockItem> &block) {
    for (const auto &item: block) {
        if (std::holds_alternative<AST::StmtHandle>(item)) {
            resolve_stmt(*std::get<AST::StmtHandle>(item));
        }
    }
}

void LabelResolutionPass::resolve_program(const AST::Program &program) {
    resolve_function(program.function);
}

void LabelResolutionPass::run() {
    resolve_program(*program);
    for (const auto &label: unresolved_labels) {
        errors.emplace_back("Label \"" + label + "\" could not be resolved");
    }
}

void LabelResolutionPass::resolve_goto(const AST::GoToStmt &stmt) {
    if (!labels.contains(stmt.label)) {
        unresolved_labels.insert(stmt.label);
    }
}

void LabelResolutionPass::resolve_labeled(const AST::LabeledStmt &stmt) {
    if (labels.contains(stmt.label)) {
        errors.emplace_back("The label '" + stmt.label + "' is already in use.");
    }
    if (unresolved_labels.contains(stmt.label)) {
        unresolved_labels.erase(stmt.label);
    }
    labels.insert(stmt.label);
    resolve_stmt(*stmt.stmt);
}
