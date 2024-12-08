//
// Created by Miłosz Koczorowski on 23/11/2024.
//

#include "TypeCheckerPass.h"

#include <functional>

#include "../overloaded.h"

void TypeCheckerPass::check_block(std::vector<AST::BlockItem> &block) {
    for (auto &item: block) {
        std::visit(overloaded{
                       [this](AST::DeclHandle &item) {
                           check_decl(item);
                       },
                       [this](AST::StmtHandle &item) {
                           check_stmt(*item);
                       }
                   }, item);
    }
}

void TypeCheckerPass::file_scope_variable_declaration(AST::VariableDecl& decl) {
    InitialValue initial_value;
    if (decl.expr && std::holds_alternative<AST::ConstantExpr>(*decl.expr)) {
        initial_value = Initial(std::get<AST::ConstantExpr>(*decl.expr).value);
    } else if (!decl.expr) {
        if (decl.storage_class == AST::StorageClass::EXTERN) {
            initial_value = NoInitializer();
        } else {
            initial_value = Tentative();
        }
    } else {
        errors.emplace_back("Non-constant initializer");
    }

    bool global = decl.storage_class != AST::StorageClass::STATIC;

    if (symbols.contains(decl.name)) {
        auto& old_decl = symbols[decl.name];
        if (old_decl.type.index() != Type(IntType()).index()) {
            errors.emplace_back("Function redeclared as variable");
        }

        auto& attributes = std::get<StaticAttributes>(old_decl.attributes);

        if (decl.storage_class == AST::StorageClass::EXTERN) {
            global = attributes.global;
        } else if (attributes.global != global) {
            errors.emplace_back("Conflicting variable linkage");
        }
        if (std::holds_alternative<Initial>(attributes.initial_value)) {
            if (std::holds_alternative<Initial>(initial_value)) {
                errors.emplace_back("Conflicting file scope variable definitions");
            } else {
                initial_value = attributes.initial_value;
            }
        } else if (!std::holds_alternative<Initial>(initial_value) && std::holds_alternative<Tentative>(attributes.initial_value)) {
            initial_value = Tentative();
        }
    }

    auto attributes = StaticAttributes(initial_value, global);
    symbols[decl.name] = {IntType(), attributes};
}

void TypeCheckerPass::local_variable_declaration(AST::VariableDecl& decl) {
    if (decl.storage_class == AST::StorageClass::EXTERN) {
        if (decl.expr) {
            errors.emplace_back("Initializer on local extern variable declaration");
        }
        if (symbols.contains(decl.name)) {
            auto old_decl = symbols[decl.name];
            if (!std::holds_alternative<IntType>(old_decl.type)) {
                errors.emplace_back("Function redeclared as variable");
            }
        } else {
            symbols[decl.name] = {IntType(), StaticAttributes(NoInitializer(), true)};
        }
    } else if (decl.storage_class == AST::StorageClass::STATIC) {
        InitialValue initial_value;
        if (decl.expr && std::holds_alternative<AST::ConstantExpr>(*decl.expr)) {
            initial_value = Initial(std::get<AST::ConstantExpr>(*decl.expr).value);
        } else if (!decl.expr) {
            initial_value = Initial(0);
        } else {
            errors.emplace_back("Non-constant initializer on local static variable");
        }
        symbols[decl.name] = {IntType(), StaticAttributes(initial_value, false)};
    } else {
        symbols[decl.name] = {IntType(), LocalAttributes()};
        if (decl.expr) {
            check_expr(*decl.expr);
        }
    }
}

void TypeCheckerPass::check_function_decl(AST::FunctionDecl &function) {
    auto type = FunctionType(function.params.size());
    bool already_defined = false;
    bool global = function.storage_class != AST::StorageClass::STATIC;
    if (symbols.contains(function.name)) {
        auto& old_decl = symbols[function.name];
        if (old_decl.type.index() != Type(type).index() || std::get<FunctionType>(old_decl.type).param_count != function
            .params.size()) {
            errors.emplace_back("Incompatible function declaration: " + function.name);
        }
        auto& function_attributes = std::get<FunctionAttributes>(old_decl.attributes);
        already_defined = function_attributes.defined;
        if (already_defined && function.body) {
            errors.emplace_back("Function redefinition: " + function.name);
        }

        if (function_attributes.global && function.storage_class == AST::StorageClass::STATIC) {
            errors.emplace_back("Static function declaration follows non-static");
        }

        global = function_attributes.global;
    }
    auto attributes = FunctionAttributes(already_defined || function.body, global);
    symbols[function.name] = Symbol(type, attributes);

    if (function.body) {
        for (const auto &param: function.params) {
            symbols[param] = Symbol(IntType());
        }
        check_block(*function.body);
    }
}

void TypeCheckerPass::check_program(AST::Program &program) {
    for (const auto &declaration: program.declarations) {
        std::visit(overloaded {
            [this](const AST::VariableDecl& decl) {
                file_scope_variable_declaration(decl);
            },
            [this](const AST::FunctionDecl& decl) {
                check_function_decl(decl);
            }
        }, *declaration);
    }
}

void TypeCheckerPass::run() {
    check_program(*program);
}

void TypeCheckerPass::check_decl(AST::DeclHandle &item) {
    std::visit(overloaded{
                   [this](const AST::VariableDecl &decl) {
                       local_variable_declaration(decl);
                   },
                   [this](const AST::FunctionDecl &decl) {
                       check_function_decl(decl);
                   }
               }, *item);
}

void TypeCheckerPass::check_expr(AST::Expr &expr) {
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


void TypeCheckerPass::check_function_call(AST::FunctionCall &expr) {
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

void TypeCheckerPass::check_variable_expr(AST::VariableExpr &expr) {
    auto* symbol_type = symbols[expr.name].type;
    if (std::holds_alternative<AST::FunctionType>(*symbol_type)) {
        errors.emplace_back("Function used as variable");
    }
    expr.type = std::make_unique<AST::Type>(*symbol_type);
}

void TypeCheckerPass::check_stmt(AST::Stmt &item) {
    std::visit(overloaded{
                   [this](AST::ReturnStmt &stmt) {
                       check_expr(*stmt.expr);
                   },
                   [this](AST::ExprStmt &stmt) {
                       check_expr(*stmt.expr);
                   },
                   [this](AST::NullStmt &) {
                   },
                   [this](AST::IfStmt &stmt) {
                       check_expr(*stmt.condition);
                       check_stmt(*stmt.then_stmt);
                       if (stmt.else_stmt) {
                           check_stmt(*stmt.else_stmt);
                       }
                   },
                   [this](AST::LabeledStmt &stmt) {
                       check_stmt(*stmt.stmt);
                   },
                   [this](AST::GoToStmt &) {
                   },
                   [this](AST::CompoundStmt &stmt) {
                       check_block(stmt.body);
                   },
                   [this](AST::BreakStmt &) {
                   },
                   [this](AST::ContinueStmt &) {
                   },
                   [this](AST::WhileStmt &stmt) {
                       check_expr(*stmt.condition);
                       check_stmt(*stmt.body);
                   },
                   [this](AST::DoWhileStmt &stmt) {
                       check_expr(*stmt.condition);
                       check_stmt(*stmt.body);
                   },
                   [this](AST::ForStmt &stmt) {
                       std::visit(overloaded{
                                      [this](std::unique_ptr<AST::VariableDecl> &decl) {
                                          if (!decl) return;
                                          local_variable_declaration(*decl);
                                      },
                                      [this](AST::ExprHandle &expr) {
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
                   [this](AST::SwitchStmt &stmt) {
                       check_expr(*stmt.expr);
                       check_stmt(*stmt.body);
                   },
                   [this](AST::CaseStmt &stmt) {
                       check_stmt(*stmt.stmt);
                   },
                   [this](AST::DefaultStmt &stmt) {
                       check_stmt(*stmt.stmt);
                   }
               }, item);
}
