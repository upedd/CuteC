#include "SwitchResolutionPass.h"

#include <utility>

#include "TypeCheckerPass.h"
#include "../overloaded.h"

void SwitchResolutionPass::resolve_function(AST::FunctionDecl &function) {
    if (function.body) {
        resolve_block(*function.body);
    }
}

void SwitchResolutionPass::resolve_stmt(AST::Stmt &stmt) {
    std::visit(overloaded{
                   [this](AST::IfStmt &stmt) {
                       resolve_stmt(*stmt.then_stmt);
                       if (stmt.else_stmt) {
                           resolve_stmt(*stmt.else_stmt);
                       }
                   },
                   [this](AST::LabeledStmt &stmt) {
                       resolve_stmt(*stmt.stmt);
                   },
                   [this](AST::CompoundStmt &stmt) {
                       resolve_block(stmt.body);
                   },
                   [this](AST::WhileStmt &stmt) {
                       resolve_stmt(*stmt.body);
                   },
                   [this](AST::DoWhileStmt &stmt) {
                       resolve_stmt(*stmt.body);
                   },
                   [this](AST::ForStmt &stmt) {
                       resolve_stmt(*stmt.body);
                   },
                   [this](AST::SwitchStmt &stmt) {
                       switches.emplace_back(&stmt);
                       resolve_stmt(*stmt.body);
                       switches.pop_back();
                   },
                   [this](AST::CaseStmt &stmt) {
                       case_stmt(stmt);
                   },
                   [this](AST::DefaultStmt &stmt) {
                       default_stmt(stmt);
                   },
                   [this](auto &) {
                   }
               }, stmt);
}

void SwitchResolutionPass::resolve_block(std::vector<AST::BlockItem> &block) {
    for (auto &item: block) {
        if (std::holds_alternative<AST::StmtHandle>(item)) {
            resolve_stmt(*std::get<AST::StmtHandle>(item));
        }
    }
}

void SwitchResolutionPass::resolve_program(AST::Program &program) {
    for (auto &declaration: program.declarations) {
        if (std::holds_alternative<AST::FunctionDecl>(*declaration)) {
            resolve_function(std::get<AST::FunctionDecl>(*declaration));
        }
    }
}

void SwitchResolutionPass::run() {
    resolve_program(*program);
}

static void convert_const(AST::Const &constant, AST::TypeHandle &type) {
    std::visit(overloaded{
                   [&constant](const AST::IntType &) {
                       std::visit(overloaded{
                                      [&constant](const auto &c) {
                                          constant = AST::ConstInt(c.value);
                                      }
                                  }, constant);
                   },
                   [&constant](const AST::UIntType &) {
                       std::visit(overloaded{
                                      [&constant](const auto &c) {
                                          constant = AST::ConstUInt(c.value);
                                      }
                                  }, constant);
                   },
                   [&constant](const AST::LongType &) {
                       std::visit(overloaded{
                                      [&constant](const auto &c) {
                                          constant = AST::ConstLong(c.value);
                                      }
                                  }, constant);
                   },
                   [&constant](const AST::ULongType &) {
                       std::visit(overloaded{
                                      [&constant](const auto &c) {
                                          constant = AST::ConstULong(c.value);
                                      }
                                  }, constant);
                   },
                   [](const auto &) {
                       std::unreachable();
                   }
               }, *type);
}

void SwitchResolutionPass::case_stmt(AST::CaseStmt &stmt) {
    if (switches.empty()) {
        errors.emplace_back("Case statement outside of switch block");
        return;
    }
    if (!std::holds_alternative<AST::ConstantExpr>(*stmt.value)) {
        errors.emplace_back("Unsupported expression as case value");
        return;
    }
    auto &current_switch = *switches.back();
    auto expr_type = get_type(*current_switch.expr);
    auto &value = std::get<AST::ConstantExpr>(*stmt.value).constant;
    convert_const(value, expr_type);
    if (current_switch.cases.contains(value)) {
        errors.emplace_back("Duplicate case in switch");
        return;
    }
    stmt.label = make_label(current_switch);
    current_switch.cases[value] = stmt.label;
    resolve_stmt(*stmt.stmt);
}

void SwitchResolutionPass::default_stmt(AST::DefaultStmt &stmt) {
    if (switches.empty()) {
        errors.emplace_back("Default statement outside of switch block");
        return;
    }
    auto &current_switch = *switches.back();
    if (current_switch.has_default) {
        errors.emplace_back("Duplicate default case in switch");
        return;
    }
    stmt.label = current_switch.label + ".default";
    current_switch.has_default = true;
    resolve_stmt(*stmt.stmt);
}

std::string SwitchResolutionPass::make_label(const AST::SwitchStmt &stmt) {
    return stmt.label + ".case." + std::to_string(cnt++);
}
