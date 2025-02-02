#ifndef IRGENERATOR_H
#define IRGENERATOR_H
#include <functional>

#include "AsmTree.h"
#include "Ast.h"
#include "IR.h"
#include "overloaded.h"
#include "Parser.h"

// ast -> ir

class IRGenerator {
public:

    struct PlainOperand {
        IR::Value value;
    };

    struct DereferencedPointer {
        IR::Value value;
    };
    using ExprResult = std::variant<PlainOperand, DereferencedPointer>;

    explicit IRGenerator(AST::Program program, std::unordered_map<std::string, Symbol>* symbols) : astProgram(std::move(program)), symbols(symbols) {
    }

    void generate() {
        for (const auto &declaration: astProgram.declarations) {
            if (std::holds_alternative<AST::FunctionDecl>(*declaration)) {
                auto& function_declaration = std::get<AST::FunctionDecl>(*declaration);
                if (!function_declaration.body) continue;
                IRProgram.items.emplace_back(gen_function(function_declaration));
            }
        }

        for (const auto& [name, symbol] : *symbols) {
            if (std::holds_alternative<StaticAttributes>(symbol.attributes)) {
                auto& attributes = std::get<StaticAttributes>(symbol.attributes);
                std::visit(overloaded {
                    [this, &name, &attributes, &symbol](const Initial& init) {
                        IRProgram.items.emplace_back(IR::StaticVariable(name, attributes.global, init, *symbol.type));
                    },
                    [this, &attributes, &name, &symbol](const Tentative& init) {
                        std::visit(overloaded {
                            [this, &name, &attributes, &symbol](const AST::IntType&) {
                                IRProgram.items.emplace_back(IR::StaticVariable(name, attributes.global, Initial{InitialInt(0)}, *symbol.type));
                            },
                            [this, &name, &attributes, &symbol](const AST::LongType&) {
                                IRProgram.items.emplace_back(IR::StaticVariable(name, attributes.global, Initial{InitialLong(0)}, *symbol.type));
                            },
                            [this, &name, &attributes, &symbol](const AST::ULongType&) {
                                IRProgram.items.emplace_back(IR::StaticVariable(name, attributes.global, Initial{InitialULong(0)}, *symbol.type));
                            },
                            [this, &name, &attributes, &symbol](const AST::UIntType&) {
                                IRProgram.items.emplace_back(IR::StaticVariable(name, attributes.global, Initial{InitialUInt(0)}, *symbol.type));
                            },
                            [this, &name, &attributes, &symbol](const AST::DoubleType&) {
                                IRProgram.items.emplace_back(IR::StaticVariable(name, attributes.global, Initial{InitialDouble(0)}, *symbol.type));
                            },
                            [this, &name, &attributes, &symbol](const AST::PointerType&) {
                                IRProgram.items.emplace_back(IR::StaticVariable(name, attributes.global, Initial{InitialULong(0)}, *symbol.type));
                            },
                            [this, &name, &attributes, &symbol](const AST::ArrayType& type) {
                                IRProgram.items.emplace_back(IR::StaticVariable(name, attributes.global, Initial{InitialZero(bytes_for_type(*type.element_type) * type.size)}, *symbol.type));
                            },
                            [](const auto&) {}
                        }, *symbol.type);
                    },
                    [](const NoInitializer&) {}
                }, attributes.initial_value);
            } else if (std::holds_alternative<ConstantAttributes>(symbol.attributes)) {
                auto& attributes = std::get<ConstantAttributes>(symbol.attributes);
                IRProgram.items.emplace_back(IR::StaticConstant(name, *symbol.type, attributes.init));
            }
        }
    }

    IR::Function gen_function(const AST::FunctionDecl &function) {
        std::vector<IR::Instruction> instructions;
        for (const auto &item: *function.body) {
            auto res = gen_block_item(item);
            instructions.insert(instructions.end(), res.begin(), res.end());
        }

        // implicitly add return 0 to the end of the function.
        instructions.emplace_back(IR::Return(IR::Constant(AST::ConstInt(0))));

        auto& attributes = std::get<FunctionAttributes>(symbols->at(function.name).attributes);

        return IR::Function(function.name, attributes.global, function.params, std::move(instructions));
    }

    std::vector<IR::Instruction> gen_block_item(const AST::BlockItem &item) {
        return std::visit(overloaded{
                              [this](const AST::StmtHandle &item) {
                                  return gen_stmt(*item);
                              },
                              [this](const AST::DeclHandle &item) {
                                  return gen_declaration(*item);
                              }
                          }, item);
    }

    std::vector<IR::Instruction> gen_declaration(const AST::Declaration &decl) {
        if (std::holds_alternative<AST::FunctionDecl>(decl)) return {};
        return gen_variable_decl(std::get<AST::VariableDecl>(decl));
    }

    std::vector<IR::Instruction> gen_if_stmt(const AST::IfStmt &stmt) {
        std::vector<IR::Instruction> instructions;
        auto else_label = make_label("else");
        auto end_label = make_label("end");
        auto result = gen_expr_and_convert(*stmt.condition, instructions);
        instructions.emplace_back(IR::JumpIfZero(result, else_label));
        auto then_instructions = gen_stmt(*stmt.then_stmt);
        instructions.insert(instructions.end(), then_instructions.begin(), then_instructions.end());
        instructions.emplace_back(IR::Jump(end_label));
        instructions.emplace_back(IR::Label(else_label));
        if (stmt.else_stmt) {
            auto else_instructions = gen_stmt(*stmt.else_stmt);
            instructions.insert(instructions.end(), else_instructions.begin(), else_instructions.end());
        }
        instructions.emplace_back(IR::Label(end_label));
        return instructions;
    }

    std::vector<IR::Instruction> gen_labeled_stmt(const AST::LabeledStmt &stmt) {
        std::vector<IR::Instruction> instructions;
        instructions.emplace_back(IR::Label(stmt.label));
        auto stmt_instructions = gen_stmt(*stmt.stmt);
        instructions.insert(instructions.end(), stmt_instructions.begin(), stmt_instructions.end());
        return instructions;
    }

    std::vector<IR::Instruction> gen_goto(const AST::GoToStmt &stmt) {
        std::vector<IR::Instruction> instructions;
        instructions.emplace_back(IR::Jump(stmt.label));
        return instructions;;
    }

    std::vector<IR::Instruction> gen_compound(const AST::CompoundStmt &stmt) {
        std::vector<IR::Instruction> instructions;
        for (const auto &item: stmt.body) {
            auto item_instructions = gen_block_item(item);
            instructions.insert(instructions.end(), item_instructions.begin(), item_instructions.end());
        }
        return instructions;
    }

    std::vector<IR::Instruction> break_stmt(const AST::BreakStmt &stmt) {
        std::vector<IR::Instruction> instructions;
        instructions.emplace_back(IR::Jump(stmt.label + ".break"));
        return instructions;
    }

    std::vector<IR::Instruction> continue_stmt(const AST::ContinueStmt &stmt) {
        std::vector<IR::Instruction> instructions;
        instructions.emplace_back(IR::Jump(stmt.label + ".continue"));
        return instructions;
    }


    std::vector<IR::Instruction> while_stmt(const AST::WhileStmt &stmt) {
        std::vector<IR::Instruction> instructions;
        instructions.emplace_back(IR::Label(stmt.label + ".continue"));
        auto condition = gen_expr_and_convert(*stmt.condition, instructions);
        instructions.emplace_back(IR::JumpIfZero(condition, stmt.label + ".break"));
        auto body_instructions = gen_stmt(*stmt.body);
        instructions.insert(instructions.end(), body_instructions.begin(), body_instructions.end());
        instructions.emplace_back(IR::Jump(stmt.label + ".continue"));
        instructions.emplace_back(IR::Label(stmt.label + ".break"));
        return instructions;
    }


    std::vector<IR::Instruction> do_while_stmt(const AST::DoWhileStmt &stmt) {
        std::vector<IR::Instruction> instructions;
        instructions.emplace_back(IR::Label(stmt.label + ".start"));
        auto body_instructions = gen_stmt(*stmt.body);
        instructions.insert(instructions.end(), body_instructions.begin(), body_instructions.end());
        instructions.emplace_back(IR::Label(stmt.label + ".continue"));
        auto condition = gen_expr_and_convert(*stmt.condition, instructions);
        instructions.emplace_back(IR::JumpIfNotZero(condition, stmt.label + ".start"));
        instructions.emplace_back(IR::Label(stmt.label + ".break"));
        return instructions;
    }

    void gen_compound(std::vector<IR::Instruction>& instructions, const std::string& target, const AST::CompoundInit& compound, int offset) {
        for (const auto& element : compound.init) {
            if (std::holds_alternative<AST::ScalarInit>(*element)) {
                auto& scalar = std::get<AST::ScalarInit>(*element);
                if (std::holds_alternative<AST::StringExpr>(*scalar.value)) { // type checker hopefully already checked that target type is good?
                    // TODO copy entire words as an optimization
                    auto& string = std::get<AST::StringExpr>(*scalar.value).string;
                    for (int i = 0; i < string.size(); ++i) {
                        instructions.emplace_back(IR::CopyToOffset(IR::Value(IR::Constant(AST::ConstChar(string[i]))), target, offset + i));
                    }
                    auto buffer_size = std::get<AST::ArrayType>(*scalar.type).size;
                    for (int i = string.size(); i < buffer_size; ++i) {
                        instructions.emplace_back(IR::CopyToOffset(IR::Value(IR::Constant(AST::ConstChar(0))), target, offset + i));
                    }
                } else {
                    instructions.emplace_back(IR::CopyToOffset(gen_expr_and_convert(*scalar.value, instructions), target, offset));
                }
                offset += bytes_for_type(*std::get<AST::ScalarInit>(*element).type);
            } else {
                gen_compound(instructions, target, std::get<AST::CompoundInit>(*element), offset);
                offset += bytes_for_type(*std::get<AST::CompoundInit>(*element).type);
            }
        }
    }

    std::vector<IR::Instruction> gen_variable_decl(const AST::VariableDecl &decl) {
        std::vector<IR::Instruction> instructions;
        if (decl.init && decl.storage_class != AST::StorageClass::STATIC) {
            if (std::holds_alternative<AST::ScalarInit>(*decl.init)) {
                auto& scalar = std::get<AST::ScalarInit>(*decl.init);
                if (std::holds_alternative<AST::StringExpr>(*scalar.value)) { // type checker hopefully already checked that target type is good?
                    // TODO copy entire words as an optimization
                    auto& string = std::get<AST::StringExpr>(*scalar.value).string;
                    for (int i = 0; i < string.size(); ++i) {
                        instructions.emplace_back(IR::CopyToOffset(IR::Value(IR::Constant(AST::ConstChar(string[i]))), decl.name, i));
                    }
                    auto buffer_size = std::get<AST::ArrayType>(*scalar.type).size;
                    for (int i = string.size(); i < buffer_size; ++i) {
                        instructions.emplace_back(IR::CopyToOffset(IR::Value(IR::Constant(AST::ConstChar(0))), decl.name, i));
                    }
                } else {
                    instructions.emplace_back(IR::Copy(gen_expr_and_convert(*scalar.value, instructions), IR::Variable(decl.name)));
                }
            } else { // compound
                gen_compound(instructions, decl.name, std::get<AST::CompoundInit>(*decl.init), 0);
            }
        }
        return instructions;
    }

    std::vector<IR::Instruction> for_stmt(const AST::ForStmt &stmt) {
        std::vector<IR::Instruction> instructions;
        std::visit(overloaded{
                       [this, &instructions](const std::unique_ptr<AST::VariableDecl> &decl) {
                           if (!decl) return;
                           auto decl_instructions = gen_variable_decl(*decl);
                           instructions.insert(instructions.end(), decl_instructions.begin(), decl_instructions.end());
                       },
                       [this, &instructions](const AST::ExprHandle &expr) {
                           if (!expr) return;
                           gen_expr_and_convert(*expr, instructions);
                       }
                   }, stmt.init);
        instructions.emplace_back(IR::Label(stmt.label + ".start"));
        if (stmt.condition) {
            auto condition = gen_expr_and_convert(*stmt.condition, instructions);
            instructions.emplace_back(IR::JumpIfZero(condition, stmt.label + ".break"));
        }
        auto body_instructions = gen_stmt(*stmt.body);
        instructions.insert(instructions.end(), body_instructions.begin(), body_instructions.end());
        instructions.emplace_back(IR::Label(stmt.label + ".continue"));
        if (stmt.post) {
            gen_expr_and_convert(*stmt.post, instructions);
        }
        instructions.emplace_back(IR::Jump(stmt.label + ".start"));
        instructions.emplace_back(IR::Label(stmt.label + ".break"));
        return instructions;
    }

    std::vector<IR::Instruction> case_stmt(const AST::CaseStmt &stmt) {
        std::vector<IR::Instruction> instructions;
        instructions.emplace_back(IR::Label(stmt.label));
        auto stmt_instructions = gen_stmt(*stmt.stmt);
        instructions.insert(instructions.end(), stmt_instructions.begin(), stmt_instructions.end());
        return instructions;
    }

    std::vector<IR::Instruction> default_stmt(const AST::DefaultStmt &stmt) {
        std::vector<IR::Instruction> instructions;
        instructions.emplace_back(IR::Label(stmt.label));
        auto stmt_instructions = gen_stmt(*stmt.stmt);
        instructions.insert(instructions.end(), stmt_instructions.begin(), stmt_instructions.end());
        return instructions;
    }

    std::vector<IR::Instruction> switch_stmt(const AST::SwitchStmt &stmt) {
        std::vector<IR::Instruction> instructions;
        auto result = gen_expr_and_convert(*stmt.expr, instructions);
        for (const auto &[value, label]: stmt.cases) {
            auto cmp = make_variable(get_type(*stmt.expr));
            // TODO: check!
            instructions.emplace_back(IR::Binary(IR::Binary::Operator::EQUAL, result, IR::Constant(value), cmp));
            instructions.emplace_back(IR::JumpIfNotZero(cmp, label));
        }
        if (stmt.has_default) {
            instructions.emplace_back(IR::Jump(stmt.label + ".default"));
        } else {
            instructions.emplace_back(IR::Jump(stmt.label + ".break"));
        }

        auto body_instructions = gen_stmt(*stmt.body);
        instructions.insert(instructions.end(), body_instructions.begin(), body_instructions.end());

        instructions.emplace_back(IR::Label(stmt.label + ".break"));
        return instructions;
    }

    std::vector<IR::Instruction> gen_stmt(const AST::Stmt &stmt) {
        return std::visit(overloaded{
                              [this](const AST::ReturnStmt &stmt) {
                                  return gen_return(stmt);
                              },
                              [this](const AST::ExprStmt &stmt) {
                                  std::vector<IR::Instruction> instructions;
                                  gen_expr_and_convert(*stmt.expr, instructions);
                                  return instructions;
                              },
                              [this](const AST::IfStmt &stmt) {
                                  return gen_if_stmt(stmt);
                              },
                              [this](const AST::LabeledStmt &stmt) {
                                  return gen_labeled_stmt(stmt);
                              },
                              [this](const AST::GoToStmt &stmt) {
                                  return gen_goto(stmt);
                              },
                              [this](const AST::CompoundStmt &stmt) {
                                  return gen_compound(stmt);
                              },
                              [this](const AST::BreakStmt &stmt) {
                                  return break_stmt(stmt);
                              },
                              [this](const AST::ContinueStmt &stmt) {
                                  return continue_stmt(stmt);
                              },
                              [this](const AST::WhileStmt &stmt) {
                                  return while_stmt(stmt);
                              },
                              [this](const AST::DoWhileStmt &stmt) {
                                  return do_while_stmt(stmt);
                              },
                              [this](const AST::ForStmt &stmt) {
                                  return for_stmt(stmt);
                              },
                              [this](const AST::CaseStmt &stmt) {
                                  return case_stmt(stmt);
                              },
                              [this](const AST::DefaultStmt &stmt) {
                                  return default_stmt(stmt);
                              },
                              [this](const AST::SwitchStmt &stmt) {
                                  return switch_stmt(stmt);
                              },
                              [this](const auto &) {
                                  return std::vector<IR::Instruction>();
                              },


                          }, stmt);
    }

    std::vector<IR::Instruction> gen_return(const AST::ReturnStmt &stmt) {
        std::vector<IR::Instruction> instructions;
        if (stmt.expr) {
            instructions.emplace_back(IR::Return(gen_expr_and_convert(*stmt.expr, instructions)));
        } else {
            instructions.emplace_back(IR::Return({}));
        }
        return instructions;
    }

    ExprResult gen_conditional(const AST::ConditionalExpr &expr, std::vector<IR::Instruction> &instructions) {
        auto else_label = make_label("else");
        auto end_label = make_label("end");

        auto condition = gen_expr_and_convert(*expr.condition, instructions);
        instructions.emplace_back(IR::JumpIfZero(condition, else_label));

        if (std::holds_alternative<AST::VoidType>(*expr.type)) {
            auto then_result = gen_expr_and_convert(*expr.then_expr, instructions);
            instructions.emplace_back(IR::Jump(end_label));
            instructions.emplace_back(IR::Label(else_label));
            auto else_result = gen_expr_and_convert(*expr.else_expr, instructions);
            instructions.emplace_back(IR::Label(end_label));
            return PlainOperand(IR::Variable("DUMMY")); // mess?
         }
        auto result = make_variable(expr.type);
        auto then_result = gen_expr_and_convert(*expr.then_expr, instructions);
        instructions.emplace_back(IR::Copy(then_result, result));
        instructions.emplace_back(IR::Jump(end_label));
        instructions.emplace_back(IR::Label(else_label));
        auto else_result = gen_expr_and_convert(*expr.else_expr, instructions);
        instructions.emplace_back(IR::Copy(else_result, result));
        instructions.emplace_back(IR::Label(end_label));
        return PlainOperand(result);
    }

    ExprResult gen_function_call(const AST::FunctionCall &expr, std::vector<IR::Instruction> &instructions) {
        std::vector<IR::Value> arguments;
        for (const auto &arg: expr.arguments) {
            arguments.emplace_back(gen_expr_and_convert(*arg, instructions));
        }
        if (std::holds_alternative<AST::VoidType>(*expr.type)) {
            instructions.emplace_back(IR::Call(expr.identifier, std::move(arguments), {}));
            return PlainOperand(IR::Variable("DUMMY")); // mess?
        }
        auto result = make_variable(expr.type);
        instructions.emplace_back(IR::Call(expr.identifier, std::move(arguments), result));
        return PlainOperand(result);
    }

    ExprResult gen_cast_expr(const AST::CastExpr & expr, std::vector<IR::Instruction> & instructions) {
        auto res = gen_expr_and_convert(*expr.expr, instructions);
        if (std::holds_alternative<AST::VoidType>(*expr.target)) {
            return PlainOperand(IR::Variable("DUMMY")); // mess?
        }

        if (get_type(*expr.expr)->index() == expr.target->index()) {
            return PlainOperand(res);
        }

        auto type = get_type(*expr.expr);
        auto destination = make_variable(expr.type);

        if (std::holds_alternative<AST::DoubleType>(*expr.target)) {
            if (std::holds_alternative<AST::IntType>(*type) || std::holds_alternative<AST::LongType>(*type) || std::holds_alternative<AST::CharType>(*type) || std::holds_alternative<AST::SignedCharType>(*type)) {
                instructions.emplace_back(IR::IntToDouble(res, destination));
                return PlainOperand(destination);
            }

            if (std::holds_alternative<AST::UIntType>(*type) || std::holds_alternative<AST::ULongType>(*type) || std::holds_alternative<AST::UCharType>(*type)) {
                instructions.emplace_back(IR::UIntToDouble(res, destination));
                return PlainOperand(destination);
            }
        }

        if (std::holds_alternative<AST::DoubleType>(*type)) {
            if (std::holds_alternative<AST::IntType>(*expr.type) || std::holds_alternative<AST::LongType>(*expr.type) || std::holds_alternative<AST::CharType>(*expr.type) || std::holds_alternative<AST::SignedCharType>(*expr.type)) {
                instructions.emplace_back(IR::DoubleToInt(res, destination));
                return PlainOperand(destination);
            }

            if (std::holds_alternative<AST::UIntType>(*expr.type) || std::holds_alternative<AST::ULongType>(*expr.type) || std::holds_alternative<AST::UCharType>(*expr.type)) {
                instructions.emplace_back(IR::DoubleToUInt(res, destination));
                return PlainOperand(destination);
            }
        }

        if (get_size_for_type(*expr.target) == get_size_for_type(*get_type(*expr.expr))) {
            instructions.emplace_back(IR::Copy(res, destination));
        } else if (get_size_for_type(*expr.target) < get_size_for_type(*get_type(*expr.expr))) {
            instructions.emplace_back(IR::Truncate(res, destination));
        } else if (std::holds_alternative<AST::IntType>(*get_type(*expr.expr)) || std::holds_alternative<AST::LongType>(*get_type(*expr.expr)) || std::holds_alternative<AST::PointerType>(*get_type(*expr.expr))  || std::holds_alternative<AST::CharType>(*get_type(*expr.expr)) || std::holds_alternative<AST::SignedCharType>(*get_type(*expr.expr))) {
            instructions.emplace_back(IR::SignExtend(res, destination));
        } else {
            instructions.emplace_back(IR::ZeroExtend(res, destination));
        }
        return PlainOperand(destination);
    }

    IR::Value convert_expr_result(const ExprResult& result, const AST::Type& type, std::vector<IR::Instruction> &instructions) {
        return std::visit(overloaded{
            [](const PlainOperand& operand) {
                return operand.value;
            },
            [this, &type, &instructions](const DereferencedPointer& operand) {
                auto dst = make_variable(type);
                instructions.emplace_back(IR::Load(operand.value, dst));
                return dst;
            }
            }, result);
    }

    IR::Value gen_expr_and_convert(const AST::Expr &expr, std::vector<IR::Instruction> &instructions) {
        auto result = gen_expr(expr, instructions);
        return convert_expr_result(result, *get_type(expr), instructions);
    }



    ExprResult gen_dereference_expr(const AST::DereferenceExpr & expr, std::vector<IR::Instruction> & instructions) {
        auto result = gen_expr_and_convert(*expr.expr, instructions);
        return DereferencedPointer(result);
    }

    ExprResult gen_address_of_expr(const AST::AddressOfExpr & expr, std::vector<IR::Instruction> & instructions) {
        auto result = gen_expr(*expr.expr, instructions);
        return std::visit(overloaded {
            [&expr, this, &instructions](const PlainOperand& operand) {
                auto dst = make_variable(expr.type);
                instructions.emplace_back(IR::GetAddress(operand.value, dst));
                return PlainOperand(dst);
            },
            [](const DereferencedPointer& operand) {
                return PlainOperand(operand.value);
            }
            }, result);
    }

    ExprResult gen_compound_expr(const AST::CompoundExpr & expr, std::vector<IR::Instruction> & instructions) {
        ExprResult last;
        for (const auto& ex : expr.exprs) {
            last = gen_expr(*ex, instructions);
        }
        return last;
    }

    ExprResult gen_temporary_expr(const AST::TemporaryExpr & expr, std::vector<IR::Instruction> & instructions) {
        if (expr.init) {
            auto res = gen_expr_and_convert(*expr.init, instructions);
            instructions.emplace_back(IR::Copy(res, IR::Variable(expr.identifier)));
            return PlainOperand(res);
        }
        std::unreachable();
    }

    ExprResult gen_subscript_expr(const AST::SubscriptExpr& expr, std::vector<IR::Instruction>& instructions) {
        auto left = gen_expr_and_convert(*expr.expr, instructions);
        auto right = gen_expr_and_convert(*expr.index, instructions);
        if (std::holds_alternative<AST::PointerType>(*get_type(*expr.expr))) {
            auto destination = make_variable(*get_type(*expr.expr));
            instructions.emplace_back(IR::AddPtr(left, right, bytes_for_type(*std::get<AST::PointerType>(*get_type(*expr.expr)).referenced_type), destination));
            return DereferencedPointer(destination);
        }
         if (std::holds_alternative<AST::PointerType>(*get_type(*expr.index))) {
            auto destination = make_variable(*get_type(*expr.index));
            instructions.emplace_back(IR::AddPtr(right, left, bytes_for_type(*std::get<AST::PointerType>(*get_type(*expr.index)).referenced_type), destination));
            return DereferencedPointer(destination);
        }
    }

    ExprResult gen_string_expr(const AST::StringExpr & expr, std::vector<IR::Instruction> & instructions) {
        auto identifier = make_temporary();
        (*symbols)[identifier] = {{AST::ArrayType{{AST::CharType{}}, expr.string.size() + 1}}, ConstantAttributes{Initial{InitialString{expr.string, true}}}};
        return PlainOperand(IR::Variable(identifier));
    }

    ExprResult gen_sizeof_type_expr(const AST::SizeOfTypeExpr & expr,  std::vector<IR::Instruction> & instructions) {
        return PlainOperand(IR::Constant(AST::ConstULong(bytes_for_type(*expr.referenced))));
    }

    ExprResult get_sizeof_expr(const AST::SizeOfExpr & expr, std::vector<IR::Instruction> & instructions) {
        return PlainOperand(IR::Constant(AST::ConstULong(bytes_for_type(*get_type(*expr.expr)))));
    }

    ExprResult gen_expr(const AST::Expr &expr, std::vector<IR::Instruction> &instructions) {
        return std::visit(overloaded{
                              [this](const AST::ConstantExpr &expr) {
                                  return gen_constant(expr);
                              },
                              [this, &instructions](const AST::UnaryExpr &expr) {
                                  return gen_unary(expr, instructions);
                              },
                              [this, &instructions](const AST::BinaryExpr &expr) {
                                  return gen_binary(expr, instructions);
                              },
                              [this, &instructions](const AST::VariableExpr &expr) {
                                  return gen_variable(expr, instructions);
                              },
                              [this, &instructions](const AST::AssigmentExpr &expr) {
                                  return gen_assigment(expr, instructions);
                              },
                              [this, &instructions](const AST::ConditionalExpr &expr) {
                                  return gen_conditional(expr, instructions);
                              },
                              [this, &instructions](const AST::FunctionCall &expr) {
                                  return gen_function_call(expr, instructions);
                              },
                              [this, &instructions](const AST::CastExpr &expr) {
                                  return gen_cast_expr(expr, instructions);
                              },
                              [this, &instructions](const AST::DereferenceExpr &expr) {
                                  return gen_dereference_expr(expr, instructions);
                              },
                              [this, &instructions](const AST::AddressOfExpr& expr) {
                                  return gen_address_of_expr(expr, instructions);
                              },
                              [this, &instructions](const AST::SubscriptExpr& expr) {
                                  return gen_subscript_expr(expr, instructions);
                              },
                                [this, &instructions](const AST::StringExpr& expr) {
                                  return gen_string_expr(expr, instructions);
                              },
                                [this, &instructions](const AST::SizeOfTypeExpr& expr) {
                                    return gen_sizeof_type_expr(expr, instructions);
                                },
                                [this, &instructions](const AST::SizeOfExpr& expr) {
                                    return get_sizeof_expr(expr, instructions);
                                },
                              [this, &instructions](const AST::CompoundExpr& expr) {
                                  return gen_compound_expr(expr, instructions);
                              },
                                [this, &instructions](const AST::TemporaryExpr& expr) {
                                    return gen_temporary_expr(expr, instructions);
                                }
                          }, expr);
    }

    IR::Value make_variable(AST::TypeHandle type) {
        auto name = make_temporary();
        (*symbols)[name] = Symbol {type, LocalAttributes{}};
        return IR::Variable(name);
    }

    ExprResult gen_constant(const AST::ConstantExpr &expr) {
        return PlainOperand(IR::Constant(expr.constant));
    }

    ExprResult gen_unary(const AST::UnaryExpr &expr, std::vector<IR::Instruction> &instructions) {
        static auto convert_op = [](const AST::UnaryExpr::Kind &kind) {
            switch (kind) {
                case AST::UnaryExpr::Kind::NEGATE:
                    return IR::Unary::Operator::NEGATE;
                case AST::UnaryExpr::Kind::COMPLEMENT:
                    return IR::Unary::Operator::COMPLEMENT;
                case AST::UnaryExpr::Kind::LOGICAL_NOT:
                    return IR::Unary::Operator::LOGICAL_NOT;
            }
        };
        auto lvalue_type = get_type(*expr.expr);
        auto lvalue = gen_expr(*expr.expr, instructions);
        auto value = convert_expr_result(lvalue, *lvalue_type, instructions);

        IR::Value constant;
        if (std::holds_alternative<AST::DoubleType>(*lvalue_type)) {
            constant = IR::Constant(AST::ConstDouble(1.0));
        } else {
            constant = IR::Constant(AST::ConstInt(1));
        }
        auto constant_for_decrement_pointer = IR::Constant(AST::ConstInt(-1));
        bool is_pointer = std::holds_alternative<AST::PointerType>(*lvalue_type);

        // TODO: refactor!!!
        if (expr.kind == AST::UnaryExpr::Kind::PREFIX_INCREMENT) {
                return std::visit(overloaded {
                [&instructions, &value, &lvalue, &constant, is_pointer, &lvalue_type](const PlainOperand& operand) -> ExprResult {
                    if (is_pointer) {
                        instructions.emplace_back(IR::AddPtr(value, constant, bytes_for_type(*std::get<AST::PointerType>(*lvalue_type).referenced_type), operand.value));
                    } else {
                        instructions.emplace_back(IR::Binary(IR::Binary::Operator::ADD, value, constant, operand.value));
                    }
                    return lvalue;
                },
                [&instructions, &value, &constant, &lvalue_type, is_pointer](const DereferencedPointer& operand) -> ExprResult {
                    if (is_pointer) {
                        instructions.emplace_back(IR::AddPtr(value, constant, bytes_for_type(*std::get<AST::PointerType>(*lvalue_type).referenced_type), value));
                    } else {
                        instructions.emplace_back(IR::Binary(IR::Binary::Operator::ADD, value, constant, value));
                    }
                    instructions.emplace_back(IR::Store(value, operand.value));
                    return PlainOperand(value);
                }
            }, lvalue);
        }
        if (expr.kind == AST::UnaryExpr::Kind::PREFIX_DECREMENT) {
            return std::visit(overloaded {
                [&instructions, &value, &lvalue, &constant, &lvalue_type, is_pointer, &constant_for_decrement_pointer](const PlainOperand& operand) -> ExprResult {
                    if (is_pointer) {
                        instructions.emplace_back(IR::AddPtr(value, constant_for_decrement_pointer, bytes_for_type(*std::get<AST::PointerType>(*lvalue_type).referenced_type), operand.value));
                    } else {
                        instructions.emplace_back(IR::Binary(IR::Binary::Operator::SUBTRACT, value, constant, operand.value));
                    }
                    return lvalue;
                },
                [&instructions, &value, &constant, &lvalue_type, &constant_for_decrement_pointer, is_pointer](const DereferencedPointer& operand) -> ExprResult {
                    if (is_pointer) {
                        instructions.emplace_back(IR::AddPtr(value, constant_for_decrement_pointer, bytes_for_type(*std::get<AST::PointerType>(*lvalue_type).referenced_type), value));
                    } else {
                        instructions.emplace_back(IR::Binary(IR::Binary::Operator::SUBTRACT, value, constant, value));
                    }
                    instructions.emplace_back(IR::Store(value, operand.value));
                    return PlainOperand(value);
                }
            }, lvalue);
        }
        // TODO: check!
        if (expr.kind == AST::UnaryExpr::Kind::POSTFIX_INCREMENT) {
            auto temp = make_variable(expr.type);
            instructions.emplace_back(IR::Copy(value, temp));
            std::visit(overloaded {
                [&instructions, &value, &constant, is_pointer, lvalue_type](const PlainOperand& operand) {
                    if (is_pointer) {
                        instructions.emplace_back(IR::AddPtr(value, constant, bytes_for_type(*std::get<AST::PointerType>(*lvalue_type).referenced_type), operand.value));
                    } else {
                        instructions.emplace_back(IR::Binary(IR::Binary::Operator::ADD, value, constant, operand.value));
                    }
                },
                [&instructions, &value, &constant, is_pointer, lvalue_type](const DereferencedPointer& operand) {
                    if (is_pointer) {
                        instructions.emplace_back(IR::AddPtr(value, constant, bytes_for_type(*std::get<AST::PointerType>(*lvalue_type).referenced_type), value));
                    } else {
                        instructions.emplace_back(IR::Binary(IR::Binary::Operator::ADD, value, constant, value));
                    }
                    instructions.emplace_back(IR::Store(value, operand.value));
                }
            }, lvalue);
            return PlainOperand(temp);
        }
        if (expr.kind == AST::UnaryExpr::Kind::POSTFIX_DECREMENT) {
            auto temp = make_variable(expr.type);
            instructions.emplace_back(IR::Copy(value, temp));
            std::visit(overloaded {
                [&instructions, &value, &constant, &lvalue_type, &constant_for_decrement_pointer, is_pointer](const PlainOperand& operand) {
                    if (is_pointer) {
                        instructions.emplace_back(IR::AddPtr(value, constant_for_decrement_pointer, bytes_for_type(*std::get<AST::PointerType>(*lvalue_type).referenced_type), operand.value));
                    } else {
                        instructions.emplace_back(IR::Binary(IR::Binary::Operator::SUBTRACT, value, constant, operand.value));
                    }
                },
                [&instructions, &value, &constant, &lvalue_type, &constant_for_decrement_pointer, is_pointer](const DereferencedPointer& operand) {
                    if (is_pointer) {
                        instructions.emplace_back(IR::AddPtr(value, constant_for_decrement_pointer, bytes_for_type(*std::get<AST::PointerType>(*lvalue_type).referenced_type), value));
                    } else {
                        instructions.emplace_back(IR::Binary(IR::Binary::Operator::SUBTRACT, value, constant, value));
                    }
                    instructions.emplace_back(IR::Store(value, operand.value));
                }
            }, lvalue);
            return PlainOperand(temp);
        }
        auto destination = make_variable(expr.type);
        instructions.emplace_back(IR::Unary(convert_op(expr.kind), value, destination));
        return PlainOperand(destination);
    }

    IR::Binary::Operator convert_ast_binary_kind_to_ir(const AST::BinaryExpr::Kind &kind) {
        switch (kind) {
            case AST::BinaryExpr::Kind::ADD:
                return IR::Binary::Operator::ADD;
            case AST::BinaryExpr::Kind::SUBTRACT:
                return IR::Binary::Operator::SUBTRACT;
            case AST::BinaryExpr::Kind::MULTIPLY:
                return IR::Binary::Operator::MULTIPLY;
            case AST::BinaryExpr::Kind::REMAINDER:
                return IR::Binary::Operator::REMAINDER;
            case AST::BinaryExpr::Kind::DIVIDE:
                return IR::Binary::Operator::DIVIDE;
            case AST::BinaryExpr::Kind::SHIFT_LEFT:
                return IR::Binary::Operator::SHIFT_LEFT;
            case AST::BinaryExpr::Kind::SHIFT_RIGHT:
                return IR::Binary::Operator::SHIFT_RIGHT;
            case AST::BinaryExpr::Kind::BITWISE_AND:
                return IR::Binary::Operator::BITWISE_AND;
            case AST::BinaryExpr::Kind::BITWISE_OR:
                return IR::Binary::Operator::BITWISE_OR;
            case AST::BinaryExpr::Kind::BITWISE_XOR:
                return IR::Binary::Operator::BITWISE_XOR;
            case AST::BinaryExpr::Kind::LESS:
                return IR::Binary::Operator::LESS;
            case AST::BinaryExpr::Kind::GREATER:
                return IR::Binary::Operator::GREATER;
            case AST::BinaryExpr::Kind::EQUAL:
                return IR::Binary::Operator::EQUAL;
            case AST::BinaryExpr::Kind::NOT_EQUAL:
                return IR::Binary::Operator::NOT_EQUAL;
            case AST::BinaryExpr::Kind::GREATER_EQUAL:
                return IR::Binary::Operator::GREATER_EQUAL;
            case AST::BinaryExpr::Kind::LESS_EQUAL:
                return IR::Binary::Operator::LESS_EQUAL;
        }
    };

    ExprResult gen_binary(const AST::BinaryExpr &expr, std::vector<IR::Instruction> &instructions) {
        if (expr.kind == AST::BinaryExpr::Kind::LOGICAL_AND) {
            return gen_logical_and(expr, instructions);
        }
        if (expr.kind == AST::BinaryExpr::Kind::LOGICAL_OR) {
            return gen_logical_or(expr, instructions);
        }
        auto destination = make_variable(expr.type);
        auto left = gen_expr_and_convert(*expr.left, instructions);
        auto right = gen_expr_and_convert(*expr.right, instructions);
        if (std::holds_alternative<AST::PointerType>(*get_type(*expr.left)) && expr.kind == AST::BinaryExpr::Kind::ADD) {
            instructions.emplace_back(IR::AddPtr(left, right, bytes_for_type(*std::get<AST::PointerType>(*get_type(*expr.left)).referenced_type), destination));
        } else if (std::holds_alternative<AST::PointerType>(*get_type(*expr.right)) && expr.kind == AST::BinaryExpr::Kind::ADD) {
            instructions.emplace_back(IR::AddPtr(right, left, bytes_for_type(*std::get<AST::PointerType>(*get_type(*expr.right)).referenced_type), destination));
        } else if (std::holds_alternative<AST::PointerType>(*get_type(*expr.left)) && std::holds_alternative<AST::PointerType>(*get_type(*expr.right)) && expr.kind == AST::BinaryExpr::Kind::SUBTRACT) {
            auto difference = make_variable(expr.type);
            instructions.emplace_back(IR::Binary(IR::Binary::Operator::SUBTRACT, left, right, difference));
            instructions.emplace_back(IR::Binary(IR::Binary::Operator::DIVIDE, difference, IR::Constant(AST::ConstULong(bytes_for_type(*std::get<AST::PointerType>(*get_type(*expr.left)).referenced_type))), destination));
        } else if (std::holds_alternative<AST::PointerType>(*get_type(*expr.left)) && expr.kind == AST::BinaryExpr::Kind::SUBTRACT) {
            auto negated = make_variable(get_type(*expr.right));
            instructions.emplace_back(IR::Unary(IR::Unary::Operator::NEGATE, right, negated));
            instructions.emplace_back(IR::AddPtr(left,  negated, bytes_for_type(*std::get<AST::PointerType>(*get_type(*expr.left)).referenced_type), destination));
        } else {
            instructions.emplace_back(IR::Binary(convert_ast_binary_kind_to_ir(expr.kind), left, right, destination));
        }
        return PlainOperand(destination);
    }

    ExprResult gen_logical_and(const AST::BinaryExpr &expr, std::vector<IR::Instruction> &instructions) {
        auto false_label = make_label("false");
        auto end_label = make_label("end");

        auto left = gen_expr_and_convert(*expr.left, instructions);
        instructions.emplace_back(IR::JumpIfZero(left, false_label));
        auto right = gen_expr_and_convert(*expr.right, instructions);
        instructions.emplace_back(IR::JumpIfZero(right, false_label));
        auto result = make_variable(expr.type);
        instructions.emplace_back(IR::Copy(IR::Constant(AST::ConstInt(1)), result));
        instructions.emplace_back(IR::Jump(end_label));
        instructions.emplace_back(IR::Label(false_label));
        instructions.emplace_back(IR::Copy(IR::Constant(AST::ConstInt(0)), result));
        instructions.emplace_back(IR::Label(end_label));
        return PlainOperand(result);
    }

    ExprResult gen_logical_or(const AST::BinaryExpr &expr, std::vector<IR::Instruction> &instructions) {
        auto true_label = make_label("true");
        auto end_label = make_label("end");

        auto left = gen_expr_and_convert(*expr.left, instructions);
        instructions.emplace_back(IR::JumpIfNotZero(left, true_label));
        auto right = gen_expr_and_convert(*expr.right, instructions);
        instructions.emplace_back(IR::JumpIfNotZero(right, true_label));
        auto result = make_variable(expr.type);
        instructions.emplace_back(IR::Copy(IR::Constant(AST::ConstInt(0)), result));
        instructions.emplace_back(IR::Jump(end_label));
        instructions.emplace_back(IR::Label(true_label));
        instructions.emplace_back(IR::Copy(IR::Constant(AST::ConstInt(1)), result));
        instructions.emplace_back(IR::Label(end_label));
        return PlainOperand(result);
    }

    ExprResult gen_variable(const AST::VariableExpr &expr, std::vector<IR::Instruction> &instructions) {
        return PlainOperand(IR::Variable(expr.name));
    }

    ExprResult gen_assigment(const AST::AssigmentExpr &expr, std::vector<IR::Instruction> &instructions) {
            auto lhs = gen_expr(*expr.left, instructions);
            auto result = gen_expr_and_convert(*expr.right, instructions);
            return std::visit(overloaded {
                [&instructions, &result, &lhs](const PlainOperand& operand) -> ExprResult {
                    instructions.emplace_back(IR::Copy(result, operand.value));
                    return lhs;
                },
                [&instructions, &result](const DereferencedPointer& operand) -> ExprResult {
                    instructions.emplace_back(IR::Store(result, operand.value));
                    return PlainOperand(result);
                }
            }, lhs);
    }

    std::string make_temporary() {
        return "tmp." + std::to_string(m_tmp_counter++);
    }

    std::string make_label(const std::string &name = "tmp") {
        return name + "." + std::to_string(m_label_counter++);
        return name + "." + std::to_string(m_label_counter++);
    }

    IR::Program IRProgram;

private:
    int m_tmp_counter = 0;
    int m_label_counter = 0;

    AST::Program astProgram;
    std::unordered_map<std::string, Symbol>* symbols;
};

#endif //IRGENERATOR_H
