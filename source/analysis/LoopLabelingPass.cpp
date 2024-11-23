#include "LoopLabelingPass.h"

#include "../overloaded.h"
#include <ranges>

void LoopLabelingPass::label_stmt(AST::Stmt &stmt) {
    std::visit(overloaded {
        [this](AST::IfStmt& stmt) {
            label_stmt(*stmt.then_stmt);
            if (stmt.else_stmt) {
                label_stmt(*stmt.else_stmt);
            }
        },
        [this](AST::LabeledStmt& stmt) {
            label_stmt(*stmt.stmt);
        },
        [this](AST::CompoundStmt& stmt) {
            label_block(stmt.body);
        },
        [this](AST::WhileStmt& stmt) {
            stmt.label = "loop." + std::to_string(cnt++);;
            context_stack.emplace_back(Context::Kind::LOOP, stmt.label);
            label_stmt(*stmt.body);
            context_stack.pop_back();
        },
        [this](AST::DoWhileStmt& stmt) {
            stmt.label = "loop." + std::to_string(cnt++);;
            context_stack.emplace_back(Context::Kind::LOOP, stmt.label);
            label_stmt(*stmt.body);
            context_stack.pop_back();
        },
        [this](AST::ForStmt& stmt) {
            stmt.label = "loop." + std::to_string(cnt++);;
            context_stack.emplace_back(Context::Kind::LOOP, stmt.label);
            label_stmt(*stmt.body);
            context_stack.pop_back();
        },
        [this](AST::BreakStmt& stmt) {
            stmt.label = get_label_for_break();
        },
        [this](AST::ContinueStmt& stmt) {
            stmt.label = get_label_for_continue();
        },
        [this](AST::SwitchStmt& stmt) {
            stmt.label = "switch." + std::to_string(cnt++);
            context_stack.emplace_back(Context::Kind::SWITCH, stmt.label);
            label_stmt(*stmt.body);
            context_stack.pop_back();
        },
        [this](AST::CaseStmt& stmt) {
            label_stmt(*stmt.stmt);
        },
        [this](AST::DefaultStmt& stmt) {
            label_stmt(*stmt.stmt);
        },
        [](auto&) {}
    }, stmt);
}

void LoopLabelingPass::label_block(std::vector<AST::BlockItem> &block) {
    for (auto& item : block) {
        if (std::holds_alternative<AST::StmtHandle>(item)) {
            label_stmt(*std::get<AST::StmtHandle>(item));
        }
    }
}

void LoopLabelingPass::label_function(AST::Function &function) {
    label_block(function.body);
}

void LoopLabelingPass::label_program(AST::Program &program) {
    label_function(program.function);
}

void LoopLabelingPass::run() {
    label_program(*program);
}

std::string LoopLabelingPass::get_label_for_break() {
    if (context_stack.empty()) {
        errors.emplace_back("Break statement outside of loop or switch");
        return "";
    }
    return context_stack.back().label;
}

std::string LoopLabelingPass::get_label_for_continue() {
    for (const auto& context : context_stack | std::views::reverse) {
        if (context.kind == Context::Kind::LOOP) {
            return context.label;
        }
    }

    errors.emplace_back("Continue statement outside of loop");
    return "";
}
