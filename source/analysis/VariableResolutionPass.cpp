#include "VariableResolutionPass.h"

#include <format>

#include "../overloaded.h"
#include <ranges>

void VariableResolutionPass::resolve_block(std::vector<AST::BlockItem> &block) {
    for (auto &item: block) {
        std::visit(overloaded{
                       [this](AST::DeclHandle &item) {
                           resolve_declaration(*item);
                       },
                       [this](AST::StmtHandle &item) {
                           resolve_statement(*item);
                       }
                   }, item);
    }
}

void VariableResolutionPass::resolve_program(AST::Program &program) {
    variables.emplace_back();
    for (auto &declaration: program.declarations) {

        std::visit(overloaded {
            [this](AST::FunctionDecl& decl) {
                resolve_function_declaration(decl);
            },
            [this](AST::VariableDecl& decl) {
                resolve_file_scope_variable_declaration(decl);
            }

        }, *declaration);
        //resolve_function_declaration(function);
    }
    variables.pop_back();
}


void VariableResolutionPass::resolve_file_scope_variable_declaration(AST::VariableDecl& decl) {
    decl.name = declare(Identifier(decl.name, true)).name;
}


void VariableResolutionPass::run() {
    resolve_program(*program);
}

std::string VariableResolutionPass::make_temporary(const std::string &original_name) {
    return std::format("local.{}.{}", original_name, cnt++);
}

void VariableResolutionPass::resolve_declaration(AST::Declaration &decl) {
    std::visit(overloaded{
                   [this](AST::VariableDecl &decl) {
                       resolve_variable_decl(decl);
                   },
                   [this](AST::FunctionDecl &decl) {
                       if (decl.body) {
                           errors.emplace_back("Only top-level function definitions are allowed");
                       }
                       if (decl.storage_class == AST::StorageClass::STATIC) {
                           errors.emplace_back("Function declarations with static specifier outside of file scope is disallowed");
                       }
                       resolve_function_declaration(decl);
                   }
               }, decl);
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
                   [this](AST::WhileStmt &stmt) {
                       resolve_expression(*stmt.condition);
                       resolve_statement(*stmt.body);
                   },
                   [this](AST::DoWhileStmt &stmt) {
                       resolve_expression(*stmt.condition);
                       resolve_statement(*stmt.body);
                   },
                   [this](AST::ForStmt &stmt) {
                       resolve_for(stmt);
                   },
                   [this](AST::SwitchStmt &stmt) {
                       resolve_expression(*stmt.expr);
                       resolve_statement(*stmt.body);
                   },
                   [this](AST::CaseStmt &stmt) {
                       resolve_statement(*stmt.stmt);
                   },
                   [this](AST::DefaultStmt &stmt) {
                       resolve_statement(*stmt.stmt);
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
                   [this](AST::FunctionCall &expr) {
                       resolve_function_call(expr);
                   },
                    [this](AST::CastExpr& expr) {
                        resolve_expression(*expr.expr);
                    },
                    [this](AST::DereferenceExpr &expr) {
                        resolve_expression(*expr.expr);
                    },
                    [this](AST::AddressOfExpr& expr) {
                        resolve_expression(*expr.expr);
                    },
                    [this](AST::TemporaryExpr& expr) {
                        expr.identifier = declare(Identifier(expr.identifier, false)).name;
                        if (expr.init) {
                            resolve_expression(*expr.init);
                        }
                    },
                    [this](AST::CompoundExpr& expr) {
                        for (auto& ex : expr.exprs) {
                            resolve_expression(*ex);
                        }
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
    resolve_expression(*expr.expr);
}

void VariableResolutionPass::resolve_binary(AST::BinaryExpr &expr) {
    resolve_expression(*expr.left);
    resolve_expression(*expr.right);
}

void VariableResolutionPass::resolve_assigment(AST::AssigmentExpr &expr) {
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

VariableResolutionPass::Identifier VariableResolutionPass::declare(const Identifier &identifier) {
    auto &current_scope = variables.back();
    if (current_scope.contains(identifier.name) && !(
            current_scope[identifier.name].is_external && identifier.is_external)) {
        errors.emplace_back("Identifer already declared in current scope: \"" + identifier.name + "\"!");
        // should fail?
    }
    if (identifier.is_external) {
        return current_scope[identifier.name] = identifier;
    }
    return current_scope[identifier.name] = Identifier(make_temporary(identifier.name), false);
}


std::optional<std::string> VariableResolutionPass::resolve(const std::string &name) {
    for (auto &scope: variables | std::views::reverse) {
        if (scope.contains(name)) {
            return scope[name].name;
        }
    }
    return {};
}

void VariableResolutionPass::resolve_compound(AST::CompoundStmt &stmt) {
    variables.emplace_back();
    resolve_block(stmt.body);
    variables.pop_back();
}

void VariableResolutionPass::resolve_for(AST::ForStmt &stmt) {
    variables.emplace_back();
    std::visit(overloaded{
                   [this](std::unique_ptr<AST::VariableDecl> &decl) {
                       if (!decl) return;
                       resolve_variable_decl(*decl);
                   },
                   [this](AST::ExprHandle &expr) {
                       if (!expr) return;
                       resolve_expression(*expr);
                   }
               }, stmt.init);
    if (stmt.condition) {
        resolve_expression(*stmt.condition);
    }
    if (stmt.post) {
        resolve_expression(*stmt.post);
    }

    resolve_statement(*stmt.body);

    variables.pop_back();
}

void VariableResolutionPass::resolve_function_call(AST::FunctionCall &expr) {
    if (auto resolved = resolve(expr.identifier); !resolved) {
        errors.emplace_back("Unresolved identifier: \"" + expr.identifier + "\"!");
    } else {
        expr.identifier = *resolved;
    }

    for (auto &arg: expr.arguments) {
        resolve_expression(*arg);
    }
}

void VariableResolutionPass::resolve_function_declaration(AST::FunctionDecl &decl) {
    decl.name = declare(Identifier(decl.name, true)).name;
    variables.emplace_back();
    for (auto &param: decl.params) {
        param = declare(Identifier(param, false)).name;
    }
    if (decl.body) {
        resolve_block(*decl.body);
    }
    variables.pop_back();
}

void VariableResolutionPass::resolve_variable_decl(AST::VariableDecl &decl) {
    decl.name = declare(Identifier(decl.name, decl.storage_class == AST::StorageClass::EXTERN)).name;
    if (decl.expr) {
        if (decl.storage_class == AST::StorageClass::EXTERN) {
            errors.emplace_back("Initialization of external variables inside of blocks is disallowed.");
        }

        resolve_expression(*decl.expr);
    }
}
