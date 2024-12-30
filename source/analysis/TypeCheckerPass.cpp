//
// Created by Miłosz Koczorowski on 23/11/2024.
//

#include "TypeCheckerPass.h"

#include <functional>
#include <iostream>
#include <utility>

#include "../IR.h"
#include "../overloaded.h"


static bool types_match(const AST::TypeHandle& a, const AST::TypeHandle& b) {
    if (a->index() != b->index()) return false;
    if (std::holds_alternative<AST::FunctionType>(*a)) {
        auto& fun_a = std::get<AST::FunctionType>(*a);
        auto& fun_b = std::get<AST::FunctionType>(*b);
        bool match = types_match(fun_a.return_type, fun_b.return_type) && fun_a.parameters_types.size() == fun_b.parameters_types.size();
        for (int i = 0; i < fun_a.parameters_types.size(); i++) {
            match = match && types_match(fun_a.parameters_types[i], fun_b.parameters_types[i]);
        }
        return match;
    }
 if (std::holds_alternative<AST::PointerType>(*a)) {
     auto& ptr_a = std::get<AST::PointerType>(*a);
     auto& ptr_b = std::get<AST::PointerType>(*b);
     return types_match(ptr_a.referenced_type, ptr_b.referenced_type);
 }
    return true;
}

// wrong for 32-bit systems!

static AST::TypeHandle get_common_type(const AST::TypeHandle &lhs, const AST::TypeHandle &rhs) {
    if (types_match(lhs, rhs)) {
        return lhs;
    }
    if (std::holds_alternative<AST::DoubleType>(*lhs) ) {
        return lhs;
    }
    if (std::holds_alternative<AST::DoubleType>(*rhs)) {
        return rhs;
    }
    if (get_size_for_type(lhs) == get_size_for_type(rhs)) {
        if (std::holds_alternative<AST::IntType>(*lhs) || std::holds_alternative<AST::LongType>(*lhs)) {
            return rhs;
        }
        return lhs;
    }
    if (get_size_for_type(lhs) > get_size_for_type(rhs)) {
        return lhs;
    }
    return rhs;
}
// todo: pointers in file scope variables!

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
        auto& constant = std::get<AST::ConstantExpr>(*decl.expr);
        std::visit(overloaded {
            [&initial_value, &constant](const AST::IntType&) {
                    std::visit(overloaded {
                        [&initial_value](const auto& c) {
                            initial_value = InitialInt(c.value);
                        }
                    }, constant.constant);
            },
            [&initial_value, &constant](const AST::LongType&) {
                std::visit(overloaded {
                        [&initial_value](const auto& c) {
                            initial_value = InitialLong(c.value);
                        }
                    }, constant.constant);
            },
            [&initial_value, &constant](const AST::ULongType&) {
                std::visit(overloaded {
                        [&initial_value](const auto& c) {
                            initial_value = InitialULong(c.value);
                        }
                    }, constant.constant);
            },
            [&initial_value, &constant](const AST::UIntType&) {
                std::visit(overloaded {
                        [&initial_value](const auto& c) {
                            initial_value = InitialUInt(c.value);
                        }
                    }, constant.constant);
            },
            [&initial_value, &constant](const AST::DoubleType&) {
                std::visit(overloaded {
                        [&initial_value](const auto& c) {
                            initial_value = InitialDouble(c.value);
                        }
                    }, constant.constant);
            },
            [&initial_value, &constant, this](const AST::PointerType&) {
                std::visit(overloaded {
                        [this](const AST::ConstDouble&) {
                            errors.emplace_back("Invalid static pointer initializer");
                        },
                        [&initial_value, this](const auto& c) {
                            if (c.value != 0) {
                                errors.emplace_back("Invalid static pointer initializer");
                            }
                            initial_value = InitialULong(c.value);
                        }
                    }, constant.constant);
            },
            [](const auto&) {
                std::unreachable();
            }
        }, *decl.type);
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
        if (!types_match(old_decl.type, decl.type)) {
            errors.emplace_back("Conflicting types!");
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
    symbols[decl.name] = {decl.type, attributes};
}

void TypeCheckerPass::local_variable_declaration(AST::VariableDecl& decl) {
    if (decl.storage_class == AST::StorageClass::EXTERN) {
        if (decl.expr) {
            errors.emplace_back("Initializer on local extern variable declaration");
        }
        if (symbols.contains(decl.name)) {
            auto old_decl = symbols[decl.name];
            if (!types_match(old_decl.type, decl.type)) {
                errors.emplace_back("Conflicting types");
            }
        } else {
            symbols[decl.name] = {decl.type, StaticAttributes(NoInitializer(), true)};
        }
    } else if (decl.storage_class == AST::StorageClass::STATIC) {
        InitialValue initial_value;
        if (decl.expr && std::holds_alternative<AST::ConstantExpr>(*decl.expr)) {
            std::visit(overloaded {
            [&initial_value](const AST::ConstInt &constant) {
                    initial_value = InitialInt(constant.value);
            },
            [&initial_value](const AST::ConstLong &constant) {
                initial_value = InitialLong(constant.value);
            },
            [&initial_value](const AST::ConstULong &constant) {
                initial_value = InitialULong(constant.value);
            },
            [&initial_value](const AST::ConstUInt &constant) {
                initial_value = InitialUInt(constant.value);
            }
                ,[&initial_value](const AST::ConstDouble &constant) {
                initial_value = InitialDouble(constant.value);
            },
        }, std::get<AST::ConstantExpr>(*decl.expr).constant);
        } else if (!decl.expr) {
            std::visit(overloaded {
                [&initial_value](const AST::IntType &) {
                  initial_value = InitialInt(0);
                },
                [&initial_value](const AST::LongType &) {
                    initial_value = InitialLong(0);
                },
                [&initial_value](const AST::ULongType &) {
                    initial_value = InitialULong(0);
                },
                [&initial_value](const AST::UIntType &) {
                    initial_value = InitialUInt(0);
                },
                [&initial_value](const AST::DoubleType &) {
                    initial_value = InitialDouble(0);
                },
                [&initial_value](const AST::PointerType &) {
                    initial_value = InitialULong(0);
                },
                [](const auto&) {}
            }, *decl.type);
        } else {
            errors.emplace_back("Non-constant initializer on local static variable");
        }
        symbols[decl.name] = {decl.type, StaticAttributes(initial_value, false)};
    } else {
        symbols[decl.name] = {decl.type, LocalAttributes()};
        if (decl.expr) {
            check_expr(*decl.expr);
            convert_by_assigment(decl.expr, decl.type);
        }
    }
}

void TypeCheckerPass::check_function_decl(AST::FunctionDecl &function) {
    bool already_defined = false;
    bool global = function.storage_class != AST::StorageClass::STATIC;
    if (symbols.contains(function.name)) {
        auto& old_decl = symbols[function.name];
        if (!types_match(old_decl.type, function.type)) {
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
    symbols[function.name] = Symbol(function.type, attributes);

    if (function.body) {
        for (int i = 0; i < function.params.size(); i++) {
            symbols[function.params[i]] = Symbol(std::get<AST::FunctionType>(*function.type).parameters_types[i]);
        }
        current_function_return_type.emplace_back(std::get<AST::FunctionType>(*function.type).return_type);
        check_block(*function.body);
        current_function_return_type.pop_back();
    }
}

void TypeCheckerPass::check_program(AST::Program &program) {
    for (const auto &declaration: program.declarations) {
        std::visit(overloaded {
            [this](AST::VariableDecl& decl) {
                file_scope_variable_declaration(decl);
            },
            [this](AST::FunctionDecl& decl) {
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
                   [this](AST::VariableDecl &decl) {
                       local_variable_declaration(decl);
                   },
                   [this](AST::FunctionDecl &decl) {
                       check_function_decl(decl);
                   }
               }, *item);
}

void TypeCheckerPass::check_expr(AST::Expr &expr) {
    std::visit(overloaded{
                   [this](AST::FunctionCall &expr) {
                       check_function_call(expr);
                   },
                   [this](AST::VariableExpr &expr) {
                       check_variable_expr(expr);
                   },
                   [this](AST::UnaryExpr &expr) {
                       check_unary(expr);
                   },
                   [this](AST::BinaryExpr &expr) {
                       check_binary(expr);
                   },
                   [this](AST::AssigmentExpr &expr) {
                       check_assigment(expr);
                   },
                   [this](AST::ConditionalExpr &expr) {
                       check_conditional(expr);
                   },
                   [this](AST::ConstantExpr & expr) {
                       check_constant_expr(expr);
                   },
                    [this](AST::CastExpr &expr) {
                        check_cast_expr(expr);
                    },
            [this](AST::DereferenceExpr &expr) {
                check_dereference(expr);
            },
        [this](AST::AddressOfExpr& expr) {
            check_address_of(expr);
        },
        [this](AST::TemporaryExpr& expr) {
            if (expr.init) {
                check_expr(*expr.init);
                expr.type = get_type(*expr.init);
                symbols[expr.identifier] = Symbol(expr.type, LocalAttributes{});
            }
        },
        [this](AST::CompoundExpr& expr) {
            for (auto& ex : expr.exprs) {
                check_expr(*ex);
            }
            expr.type = get_type(*expr.exprs.back());
         }
               }, expr);
}


void TypeCheckerPass::check_function_call(AST::FunctionCall &expr) {
    auto type = symbols[expr.identifier].type;
    if (!std::holds_alternative<AST::FunctionType>(*type)) {
        errors.emplace_back("Variable used as function name");
    }
    auto& function_type = std::get<AST::FunctionType>(*type);
    if (function_type.parameters_types.size() != expr.arguments.size()) {
        errors.emplace_back("Function called with wrong number of arguments");
    }
    for (int i = 0; i < function_type.parameters_types.size(); i++) {
        check_expr(*expr.arguments[i]);
        convert_by_assigment(expr.arguments[i], function_type.parameters_types[i]);
    }
    expr.type = function_type.return_type;
}

void TypeCheckerPass::check_variable_expr(AST::VariableExpr &expr) {
    const auto symbol_type = symbols[expr.name].type;
    if (std::holds_alternative<AST::FunctionType>(*symbol_type)) {
        errors.emplace_back("Function used as variable");
    }
    expr.type = *symbol_type;
}

void TypeCheckerPass::check_stmt(AST::Stmt &item) {
    std::visit(overloaded{
                   [this](AST::ReturnStmt &stmt) {
                       if (stmt.expr) {
                           check_expr(*stmt.expr);
                           convert_by_assigment(stmt.expr, current_function_return_type.back());
                       }
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
                       if (std::holds_alternative<AST::DoubleType>(*get_type(*stmt.expr)) || std::holds_alternative<AST::PointerType>(*get_type(*stmt.expr))) {
                           errors.emplace_back("Switch controlling value must be an integer");
                       }
                   },
                   [this](AST::CaseStmt &stmt) {
                       check_stmt(*stmt.stmt);
                       check_expr(*stmt.value);
                       if (std::holds_alternative<AST::DoubleType>(*get_type(*stmt.value)) || std::holds_alternative<AST::DoubleType>(*get_type(*stmt.value))) {
                           errors.emplace_back("Case value must be an integer");
                       }
                   },
                   [this](AST::DefaultStmt &stmt) {
                       check_stmt(*stmt.stmt);
                   }
               }, item);
}

void TypeCheckerPass::check_constant_expr(AST::ConstantExpr& expr) {
    std::visit(overloaded {
        [&expr](const AST::ConstInt&) {
            expr.type = AST::Type(AST::IntType());
        },
        [&expr](const AST::ConstLong&) {
            expr.type = AST::Type(AST::LongType());
        },
        [&expr](const AST::ConstUInt&) {
            expr.type = AST::Type(AST::UIntType());
        },
        [&expr](const AST::ConstULong&) {
            expr.type = AST::Type(AST::ULongType());
        },
        [&expr](const AST::ConstDouble&) {
            expr.type = AST::Type(AST::DoubleType());
        }
    }, expr.constant);
}

bool is_lvalue(const AST::Expr& expr) {
    return std::holds_alternative<AST::VariableExpr>(expr) || std::holds_alternative<AST::DereferenceExpr>(expr);
}



void TypeCheckerPass::check_cast_expr(AST::CastExpr &expr) {
    check_expr(*expr.expr);
    if (std::holds_alternative<AST::PointerType>(*expr.target) && std::holds_alternative<AST::DoubleType>(*get_type(*expr.expr))) {
        errors.emplace_back("Casting double to pointer type is disallowed");
    } else if (std::holds_alternative<AST::DoubleType>(*expr.target) && std::holds_alternative<AST::PointerType>(*get_type(*expr.expr))) {
        errors.emplace_back("Casting pointer to double type is disallowed");
    }
    expr.type = expr.target;
}

void TypeCheckerPass::check_unary(AST::UnaryExpr &expr) {
    check_expr(*expr.expr);
    if ((expr.kind == AST::UnaryExpr::Kind::POSTFIX_DECREMENT || expr.kind == AST::UnaryExpr::Kind::POSTFIX_INCREMENT || expr.kind == AST::UnaryExpr::Kind::PREFIX_DECREMENT || expr.kind == AST::UnaryExpr::Kind::PREFIX_INCREMENT) && !is_lvalue(*expr.expr)) {
        errors.emplace_back("Expected an lvalue");
    }

    if (expr.kind == AST::UnaryExpr::Kind::COMPLEMENT && std::holds_alternative<AST::DoubleType>(*get_type(*expr.expr))) {
        errors.emplace_back("Bitwise complement operator operand must have integer type");
    }

    if ((expr.kind == AST::UnaryExpr::Kind::COMPLEMENT || expr.kind == AST::UnaryExpr::Kind::NEGATE) && std::holds_alternative<AST::PointerType>(*get_type(*expr.expr))) {
        errors.emplace_back("Invalid unary operation on pointer type");
    }

    if (expr.kind == AST::UnaryExpr::Kind::LOGICAL_NOT) {
        expr.type = AST::Type(AST::IntType());
    } else {
        expr.type = get_type(*expr.expr);
    }
}

bool is_null_pointer_constant(const AST::Expr& expr) {
    if (!std::holds_alternative<AST::ConstantExpr>(expr)) {
        return false;
    }

    return std::visit(overloaded {
        [](const AST::ConstInt& c) {
            return c.value == 0;
        },
        [](const AST::ConstUInt& c) {
            return c.value == 0;
        },
        [](const AST::ConstLong& c) {
            return c.value == 0;
        },
        [](const AST::ConstULong& c) {
            return c.value == 0;
        },
        [](const auto&) {
            return false;
        }
    }, std::get<AST::ConstantExpr>(expr).constant);
}

AST::TypeHandle TypeCheckerPass::get_common_pointer_type(const AST::ExprHandle& lhs, const AST::ExprHandle& rhs) {
    auto lhs_type = get_type(*lhs);
    auto rhs_type = get_type(*rhs);
    if (types_match(lhs_type, rhs_type)) {
        return lhs_type;
    }
    if (is_null_pointer_constant(*lhs)) {
        return rhs_type;
    }
    if (is_null_pointer_constant(*rhs)) {
        return lhs_type;
    }

    // TODO: handle more gracefully?
    errors.emplace_back("Incompatible types");
    return {};
}

void TypeCheckerPass::check_binary(AST::BinaryExpr &expr) {
    check_expr(*expr.left);
    check_expr(*expr.right);

    auto lhs_type = get_type(*expr.left);
    auto rhs_type = get_type(*expr.right);

    if ((expr.kind == AST::BinaryExpr::Kind::REMAINDER || expr.kind == AST::BinaryExpr::Kind::BITWISE_OR || expr.kind == AST::BinaryExpr::Kind::BITWISE_AND || expr.kind == AST::BinaryExpr::Kind::BITWISE_XOR || expr.kind == AST::BinaryExpr::Kind::SHIFT_LEFT || expr.kind == AST::BinaryExpr::Kind::SHIFT_RIGHT) && (std::holds_alternative<AST::DoubleType>(*lhs_type) || std::holds_alternative<AST::DoubleType>(*rhs_type))) {
        errors.emplace_back("operator operands must have integer types");
    }

    if ((expr.kind == AST::BinaryExpr::Kind::DIVIDE || expr.kind == AST::BinaryExpr::Kind::MULTIPLY || expr.kind == AST::BinaryExpr::Kind::REMAINDER || expr.kind == AST::BinaryExpr::Kind::BITWISE_OR || expr.kind == AST::BinaryExpr::Kind::BITWISE_AND || expr.kind == AST::BinaryExpr::Kind::BITWISE_XOR || expr.kind == AST::BinaryExpr::Kind::SHIFT_LEFT || expr.kind == AST::BinaryExpr::Kind::SHIFT_RIGHT) && (std::holds_alternative<AST::PointerType>(*lhs_type) || std::holds_alternative<AST::PointerType>(*rhs_type))) {
        errors.emplace_back("invalid operand for operator"); // better errors?
    }

    if (expr.kind == AST::BinaryExpr::Kind::LOGICAL_AND || expr.kind == AST::BinaryExpr::Kind::LOGICAL_OR) {
        expr.type = AST::Type(AST::IntType());
        return;
    }
    if (expr.kind  == AST::BinaryExpr::Kind::SHIFT_LEFT || expr.kind == AST::BinaryExpr::Kind::SHIFT_RIGHT) {
        auto lhs_type = get_type(*expr.left);
        convert_to(expr.right, lhs_type);
        expr.type = lhs_type;
        return;
    }

    AST::TypeHandle common_type;

    if (std::holds_alternative<AST::PointerType>(*lhs_type) || std::holds_alternative<AST::PointerType>(*rhs_type)) {
        common_type = get_common_pointer_type(expr.left, expr.right);
    } else {
        common_type = get_common_type(lhs_type, rhs_type);
    }
    convert_to(expr.left, common_type);
    convert_to(expr.right, common_type);
    if (expr.kind == AST::BinaryExpr::Kind::ADD || expr.kind == AST::BinaryExpr::Kind::SUBTRACT || expr.kind == AST::BinaryExpr::Kind::MULTIPLY || expr.kind == AST::BinaryExpr::Kind::DIVIDE || expr.kind == AST::BinaryExpr::Kind::REMAINDER || expr.kind == AST::BinaryExpr::Kind::BITWISE_OR || expr.kind == AST::BinaryExpr::Kind::BITWISE_AND || expr.kind == AST::BinaryExpr::Kind::BITWISE_XOR) {
        expr.type = common_type;
    } else {
        expr.type = AST::Type(AST::IntType());
    }
}


bool is_type_arithmetic(const AST::TypeHandle& type) {
    return std::holds_alternative<AST::IntType>(*type) || std::holds_alternative<AST::UIntType>(*type) || std::holds_alternative<AST::LongType>(*type) || std::holds_alternative<AST::ULongType>(*type) || std::holds_alternative<AST::DoubleType>(*type);
}

void TypeCheckerPass::convert_by_assigment(AST::ExprHandle& expr, AST::TypeHandle target) {
    auto expr_type = get_type(*expr);
    if (types_match(target, expr_type)) {
        return;
    }
    if (is_type_arithmetic(target) && is_type_arithmetic(expr_type)) {
        convert_to(expr, target);
        return;
    }

    if (is_null_pointer_constant(*expr) && std::holds_alternative<AST::PointerType>(*target)) {
        convert_to(expr, target);
        return;
    }
    errors.emplace_back("Cannot convert types for assigment");

}

void TypeCheckerPass::check_assigment(AST::AssigmentExpr &expr) {
    if (!is_lvalue(*expr.left)) {
        errors.emplace_back("Lvalue expected");
    }
    check_expr(*expr.left);
    expr.type = get_type(*expr.left);
    check_expr(*expr.right);
    convert_by_assigment(expr.right, expr.type);
}

void TypeCheckerPass::check_conditional(AST::ConditionalExpr &expr) {
    check_expr(*expr.condition);
    check_expr(*expr.then_expr);
    check_expr(*expr.else_expr);
    auto lhs_type = get_type(*expr.then_expr);
    auto rhs_type = get_type(*expr.else_expr);
    AST::TypeHandle common_type;
    if (std::holds_alternative<AST::PointerType>(*lhs_type) || std::holds_alternative<AST::PointerType>(*rhs_type)) {
        common_type = get_common_pointer_type(expr.then_expr, expr.else_expr);
    } else {
        common_type = get_common_type(lhs_type, rhs_type);
    }
    convert_to(expr.then_expr, common_type);
    convert_to(expr.else_expr, common_type);
    expr.type = common_type;
}

void TypeCheckerPass::check_dereference(AST::DereferenceExpr &expr) {
    check_expr(*expr.expr);
    if (!std::holds_alternative<AST::PointerType>(*get_type(*expr.expr))) {
        errors.emplace_back("Cannot dereference a non-pointer type");
    }
    expr.type = std::get<AST::PointerType>(*get_type(*expr.expr)).referenced_type;
}


void TypeCheckerPass::check_address_of(AST::AddressOfExpr &expr) {
    if (!is_lvalue(*expr.expr)) {
        errors.emplace_back("Cannot take address of a non-lvalue");
    }
    check_expr(*expr.expr);
    expr.type = {AST::PointerType(*get_type(*expr.expr))};
}
