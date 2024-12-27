#ifndef CODEGEN_H
#define CODEGEN_H
#include <unordered_map>
#include <algorithm>
#include <limits>
#include <ranges>

#include "AsmTree.h"
#include "Ast.h"
#include "IR.h"
#include "overloaded.h"
#include "analysis/TypeCheckerPass.h"

namespace codegen {
    class IRToAsmTreePass {
    public:
        IRToAsmTreePass(IR::Program IRProgram, std::unordered_map<std::string, Symbol>* symbols) : IRProgram(std::move(IRProgram)), symbols(symbols) {
        }

        ASM::Type get_type_for_identifier(const std::string& name) {
            return std::visit(overloaded {
                        [](const AST::IntType&) {
                            return ASM::Type::LongWord;
                        },
                        [](const AST::UIntType&) {
                            return ASM::Type::LongWord;
                        },
                        [](const AST::LongType&) {
                            return ASM::Type::QuadWord;
                        },
                        [](const AST::ULongType&) {
                            return ASM::Type::QuadWord;
                        },
                        [](const AST::DoubleType&) {
                            return ASM::Type::Double;
                        },
                        [](const auto&) -> ASM::Type {
                            std::unreachable();
                        }
                    }, *(*symbols)[name].type);
        }

        ASM::Type get_type_for_value(const IR::Value& value) {
            return std::visit(overloaded {
                [](const IR::Constant& val) {
                    return std::visit(overloaded {
                        [](const AST::ConstInt&) {
                            return ASM::Type::LongWord;
                        },
                        [](const AST::ConstUInt&) {
                            return ASM::Type::LongWord;
                        },
                        [](const AST::ConstLong&) {
                            return ASM::Type::QuadWord;
                        },
                        [](const AST::ConstULong&) {
                            return ASM::Type::QuadWord;
                        },
                        [](const AST::ConstDouble&) {
                            return ASM::Type::Double;
                        }
                        }, val.constant);
                },
                [this](const IR::Variable& val) {
                    return get_type_for_identifier(val.name);
                }
            }, value);
        }

        bool is_unsigned(const IR::Value& value) {
            return std::visit(overloaded {
                [](const IR::Constant& val) {
                    return std::holds_alternative<AST::ConstUInt>(val.constant) || std::holds_alternative<AST::ConstULong>(val.constant);
                },
                [this](const IR::Variable& val) {
                    auto type = (*symbols)[val.name].type;
                    return std::holds_alternative<AST::UIntType>(*type) || std::holds_alternative<AST::ULongType>(*type);
                }
            }, value);
        }

        void convert() {
            for (const auto &item: IRProgram.items) {
                std::visit(overloaded {
                    [this](const IR::Function& function) {
                        asmProgram.items.emplace_back(convert_function(function));
                    },
                    [this](const IR::StaticVariable& variable) {
                        asmProgram.items.emplace_back(convert_static_variable(variable));
                    }
                }, item);
            }

            for (const auto& [name, symbol] : *symbols) {
                asmSymbols[name] = std::visit(overloaded {
                    [&symbol](const AST::IntType&) -> ASM::Symbol {
                        return ASM::ObjectSymbol {ASM::Type::LongWord, std::holds_alternative<StaticAttributes>(symbol.attributes)};
                    },
                    [&symbol](const AST::LongType&) -> ASM::Symbol {
                        return ASM::ObjectSymbol {ASM::Type::QuadWord, std::holds_alternative<StaticAttributes>(symbol.attributes)};
                    },
                    [&symbol](const AST::ULongType&) -> ASM::Symbol {
                        return ASM::ObjectSymbol {ASM::Type::QuadWord, std::holds_alternative<StaticAttributes>(symbol.attributes)};
                    },
                    [&symbol](const AST::UIntType&) -> ASM::Symbol {
                        return ASM::ObjectSymbol {ASM::Type::LongWord, std::holds_alternative<StaticAttributes>(symbol.attributes)};
                    },
                    [&symbol](const AST::DoubleType&) -> ASM::Symbol {
                        return ASM::ObjectSymbol {ASM::Type::Double, std::holds_alternative<StaticAttributes>(symbol.attributes)};
                    },
                    [&symbol](const AST::FunctionType&) -> ASM::Symbol {
                        return ASM::FunctionSymbol {std::get<FunctionAttributes>(symbol.attributes).defined};
                    },
                    [](const auto&) -> ASM::Symbol {
                        std::unreachable();
                    }
                }, *symbol.type);
            }

        }

        ASM::StaticVariable convert_static_variable(const IR::StaticVariable & variable) {
            return std::visit(overloaded {
                [&variable](const AST::IntType&) -> ASM::StaticVariable {
                    return {variable.name, variable.global, 4, variable.initial};
                },
                [&variable](const AST::LongType&) -> ASM::StaticVariable {
                    return {variable.name, variable.global, 8, variable.initial};
                },
                [&variable](const AST::ULongType&) -> ASM::StaticVariable {
                    return {variable.name, variable.global, 8, variable.initial};
                },
                [&variable](const AST::UIntType&) -> ASM::StaticVariable {
                    return {variable.name, variable.global, 4, variable.initial};
                },
                [&variable](const AST::DoubleType&) -> ASM::StaticVariable {
                    return {variable.name, variable.global, 8, variable.initial};
                },
                [](const auto&) -> ASM::StaticVariable {
                    std::unreachable();
                }
            }, *variable.type);
        }


        ASM::Function convert_function(const IR::Function &function) {
            std::vector<ASM::Instruction> instructions;
            // duplication with call
            static std::vector int_registers = {
                ASM::Reg::Name::DI, ASM::Reg::Name::SI, ASM::Reg::Name::DX, ASM::Reg::Name::CX, ASM::Reg::Name::R8,
                ASM::Reg::Name::R9
            };
            static std::vector double_registers = {
                ASM::Reg::Name::XMM0, ASM::Reg::Name::XMM1, ASM::Reg::Name::XMM2, ASM::Reg::Name::XMM3, ASM::Reg::Name::XMM4, ASM::Reg::Name::XMM5, ASM::Reg::Name::XMM6, ASM::Reg::Name::XMM7
            };
            std::vector<std::string> int_params;
            std::vector<std::string> double_params;
            std::vector<std::string> stack_params;

            for (const auto& param : function.params) {
                    if (get_type_for_identifier(param) == ASM::Type::Double) {
                        if (double_params.size() < double_registers.size()) {
                            double_params.push_back(param);
                        } else {
                            stack_params.push_back(param);
                        }
                    } else {
                        if (int_params.size() < int_registers.size()) {
                            int_params.push_back(param);
                        } else {
                            stack_params.push_back(param);
                        }
                    }
            }

            for (int i = 0; i < int_params.size(); ++i) {
                instructions.emplace_back(ASM::Mov(get_type_for_identifier(int_params[i]), ASM::Reg(int_registers[i]), ASM::Pseudo(int_params[i])));
            }

            for (int i = 0; i < double_params.size(); ++i) {
                instructions.emplace_back(ASM::Mov(get_type_for_identifier(double_params[i]), ASM::Reg(double_registers[i]), ASM::Pseudo(double_params[i])));
            }

            for (int i = 0; i < stack_params.size(); ++i) {
                instructions.emplace_back(ASM::Mov(get_type_for_identifier(stack_params[i]),ASM::Stack(16 + i * 8),
                                                   ASM::Pseudo(stack_params[i])));
            }

            for (auto &instruction: function.instructions) {
                convert_instruction(instruction, instructions);
            }
            return ASM::Function(function.name, function.global, std::move(instructions));
        }


        void convert_call(const IR::Call &call, std::vector<ASM::Instruction> &instructions) {
            static std::vector int_registers = {
                ASM::Reg::Name::DI, ASM::Reg::Name::SI, ASM::Reg::Name::DX, ASM::Reg::Name::CX, ASM::Reg::Name::R8,
                ASM::Reg::Name::R9
            };
            static std::vector double_registers = {
                ASM::Reg::Name::XMM0, ASM::Reg::Name::XMM1, ASM::Reg::Name::XMM2, ASM::Reg::Name::XMM3, ASM::Reg::Name::XMM4, ASM::Reg::Name::XMM5, ASM::Reg::Name::XMM6, ASM::Reg::Name::XMM7
            };
            std::vector<IR::Value> int_args;
            std::vector<IR::Value> double_args;
            std::vector<IR::Value> stack_args;

            for (const auto &arg: call.arguments) {
                if (get_type_for_value(arg) == ASM::Type::Double) {
                    if (double_args.size() < double_registers.size()) {
                        double_args.push_back(arg);
                    } else {
                        stack_args.push_back(arg);
                    }
                } else {
                    if (int_args.size() < int_registers.size()) {
                        int_args.push_back(arg);
                    } else {
                        stack_args.push_back(arg);
                    }
                }
            }


            int stack_padding = stack_args.size() % 2 == 0 ? 0 : 8;
            if (stack_padding != 0) {
                instructions.emplace_back(ASM::Binary(ASM::Binary::Operator::SUB, ASM::Type::QuadWord, ASM::Imm(stack_padding), ASM::Reg(ASM::Reg::Name::SP)));
            }

            // register passed arguments
            for (int i = 0; i < int_args.size(); i++) {
                instructions.emplace_back(ASM::Mov(get_type_for_value(int_args[i]),convert_value(int_args[i]), ASM::Reg(int_registers[i])));
            }

            for (int i = 0; i < double_args.size(); i++) {
                instructions.emplace_back(ASM::Mov(get_type_for_value(double_args[i]), convert_value(double_args[i]), ASM::Reg(double_registers[i])));
            }

            // stack passed arguments
            for (auto& stack_arg : stack_args | std::views::reverse) {
                auto arg = convert_value(stack_arg);
                auto type = get_type_for_value(stack_arg);
                if (std::holds_alternative<ASM::Imm>(arg) || std::holds_alternative<ASM::Reg>(arg) || type == ASM::Type::LongWord || type == ASM::Type::Double) {
                    instructions.emplace_back(ASM::Push(arg));
                } else {
                    instructions.emplace_back(ASM::Mov(type, arg, ASM::Reg(ASM::Reg::Name::AX)));
                    instructions.emplace_back(ASM::Push(ASM::Reg(ASM::Reg::Name::AX)));
                }
            }

            instructions.emplace_back(ASM::Call(call.name));

            int bytes_to_remove = 8 * stack_args.size() + stack_padding;

            if (bytes_to_remove != 0) {
                instructions.emplace_back(ASM::Binary(ASM::Binary::Operator::ADD, ASM::Type::QuadWord, ASM::Imm(bytes_to_remove), ASM::Reg(ASM::Reg::Name::SP)));
            }

            if (get_type_for_value(call.destination) == ASM::Type::Double) {
                instructions.emplace_back(ASM::Mov(get_type_for_value(call.destination), ASM::Reg(ASM::Reg::Name::XMM0), convert_value(call.destination)));
            } else {
                instructions.emplace_back(ASM::Mov(get_type_for_value(call.destination), ASM::Reg(ASM::Reg::Name::AX), convert_value(call.destination)));
            }
        }

        void convert_sign_extend(const IR::SignExtend & instruction, std::vector<ASM::Instruction> & instructions) {
            instructions.emplace_back(ASM::Movsx(convert_value(instruction.source), convert_value(instruction.destination)));
        }

        void convert_trunctate(const IR::Truncate & instruction, std::vector<ASM::Instruction> & instructions) {
            instructions.emplace_back(ASM::Mov(ASM::Type::LongWord, convert_value(instruction.source), convert_value(instruction.destination)));
        }

        void convert_zero_extend(const IR::ZeroExtend & instruction, std::vector<ASM::Instruction> & instructions) {
            instructions.emplace_back(ASM::MovZeroExtend(convert_value(instruction.source), convert_value(instruction.destination)));
        }

        void convert_double_to_int(const IR::DoubleToInt & instruction, std::vector<ASM::Instruction> & instructions) {
            instructions.emplace_back(ASM::Cvttsd2si(get_type_for_value(instruction.destination),convert_value(instruction.source), convert_value(instruction.destination)));
        }

        void convert_double_to_uint(const IR::DoubleToUInt & instruction, std::vector<ASM::Instruction> & instructions) {
            // no asm instruction for this conversion!

            if (get_type_for_value(instruction.destination) == ASM::Type::LongWord) {
                // when converting to unsigned int we can emit conversion to unsigned long and truncate the result
                instructions.emplace_back(ASM::Cvttsd2si(ASM::Type::QuadWord,convert_value(instruction.source), ASM::Reg(ASM::Reg::Name::AX)));
                instructions.emplace_back(ASM::Mov(ASM::Type::LongWord, ASM::Reg(ASM::Reg::Name::AX), convert_value(instruction.destination)));
            } else {
                // when converting to unsigned long we first check if the result will fit into signed long
                ASM::Operand max_value;
                if (constants.contains(std::make_pair(std::numeric_limits<std::int64_t>::max() + 1, 8))) {
                    max_value = ASM::Data(constants[std::make_pair(static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) + 1, 8)]);
                } else {
                    auto label = gen_const_label();
                    asmProgram.items.emplace_back(ASM::StaticConstant(label, 8, InitialDouble(static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())+ 1)));
                    max_value = ASM::Data(label);
                }
                auto end_label = gen_jump_label("end");
                auto out_of_range_label = gen_jump_label("out_of_range_label");
                instructions.emplace_back(ASM::Cmp(ASM::Type::Double, max_value, convert_value(instruction.source)));
                instructions.emplace_back(ASM::JmpCC(ASM::ConditionCode::AE, out_of_range_label));

                // if result fits into signed long we can convert it normally
                instructions.emplace_back(ASM::Cvttsd2si(ASM::Type::QuadWord,convert_value(instruction.source), convert_value(instruction.destination)));
                instructions.emplace_back(ASM::Jmp(end_label));

                // if result does not fit into signed long we need to subtract LONG_MAX + 1 before conversion and add it after
                instructions.emplace_back(ASM::Label(out_of_range_label));
                instructions.emplace_back(ASM::Mov(ASM::Type::Double, convert_value(instruction.source), ASM::Reg(ASM::Reg::Name::XMM1)));
                instructions.emplace_back(ASM::Binary(ASM::Binary::Operator::SUB, ASM::Type::Double, max_value, ASM::Reg(ASM::Reg::Name::XMM1)));
                instructions.emplace_back(ASM::Cvttsd2si(ASM::Type::QuadWord, ASM::Reg(ASM::Reg::Name::XMM1), convert_value(instruction.destination)));
                instructions.emplace_back(ASM::Mov(ASM::Type::QuadWord, ASM::Imm(std::numeric_limits<std::int64_t>::max() + 1), ASM::Reg(ASM::Reg::Name::DX)));
                instructions.emplace_back(ASM::Binary(ASM::Binary::Operator::ADD, ASM::Type::QuadWord, ASM::Reg(ASM::Reg::Name::DX), convert_value(instruction.destination)));

                instructions.emplace_back(ASM::Label(end_label));
            }
        }

        void convert_int_to_double(const IR::IntToDouble & instruction, std::vector<ASM::Instruction> & instructions) {
            instructions.emplace_back(ASM::Cvtsi2sd(get_type_for_value(instruction.source), convert_value(instruction.source), convert_value(instruction.destination)));
        }


        void convert_uint_to_double(const IR::UIntToDouble & instruction, std::vector<ASM::Instruction> & instructions) {
            // no asm instruction for this conversion

            // when converting an unsigned integer we can zero extend it and emit conversion from signed long to double
            if (get_type_for_value(instruction.source) == ASM::Type::LongWord) {
                instructions.emplace_back(ASM::MovZeroExtend(convert_value(instruction.source), ASM::Reg(ASM::Reg::Name::AX)));
                instructions.emplace_back(ASM::Cvtsi2sd(ASM::Type::QuadWord, ASM::Reg(ASM::Reg::Name::AX),  convert_value(instruction.destination)));
            } else {
                // when converting an unsigned long we first check if we can fit it into signed long
                auto end_label = gen_jump_label("end");
                auto out_of_range_label = gen_jump_label("out_of_range");

                instructions.emplace_back(ASM::Cmp(ASM::Type::QuadWord, ASM::Imm(0), convert_value(instruction.source)));
                instructions.emplace_back(ASM::JmpCC(ASM::ConditionCode::L,out_of_range_label));

                // if source fits into signed long we convert it normally
                instructions.emplace_back(ASM::Cvtsi2sd(ASM::Type::QuadWord, convert_value(instruction.source), convert_value(instruction.destination)));
                instructions.emplace_back(ASM::Jmp(end_label));

                // if source does not fit we divide it by two before conversion and multiply by 2 after conversion
                instructions.emplace_back(ASM::Label(out_of_range_label));
                instructions.emplace_back(ASM::Mov(ASM::Type::QuadWord, convert_value(instruction.source), ASM::Reg(ASM::Reg::Name::AX)));
                instructions.emplace_back(ASM::Mov(ASM::Type::QuadWord,ASM::Reg(ASM::Reg::Name::AX), ASM::Reg(ASM::Reg::Name::DX)));
                instructions.emplace_back(ASM::Unary(ASM::Unary::Operator::Shr, ASM::Type::QuadWord, ASM::Reg(ASM::Reg::Name::DX)));
                // round to odd to avoid double rounding error
                instructions.emplace_back(ASM::Binary(ASM::Binary::Operator::AND, ASM::Type::QuadWord, ASM::Imm(1), ASM::Reg(ASM::Reg::Name::AX)));
                instructions.emplace_back(ASM::Binary(ASM::Binary::Operator::OR, ASM::Type::QuadWord, ASM::Reg(ASM::Reg::Name::AX), ASM::Reg(ASM::Reg::Name::DX)));

                instructions.emplace_back(ASM::Cvtsi2sd(ASM::Type::QuadWord, ASM::Reg(ASM::Reg::Name::DX), convert_value(instruction.destination)));
                instructions.emplace_back(ASM::Binary(ASM::Binary::Operator::ADD, ASM::Type::Double, convert_value(instruction.destination), convert_value(instruction.destination)));

                instructions.emplace_back(ASM::Label(end_label));
            }

        }

        void convert_instruction(const IR::Instruction &instruction, std::vector<ASM::Instruction> &instructions) {
            std::visit(overloaded{
                           [this, &instructions](const IR::Return &instruction) {
                               convert_return(instruction, instructions);
                           },
                           [this, &instructions](const IR::Unary &instruction) {
                               convert_unary(instruction, instructions);
                           },
                           [this, &instructions](const IR::Binary &instruction) {
                               convert_binary(instruction, instructions);
                           },
                           [this, &instructions](const IR::Jump &instruction) {
                               convert_jump(instruction, instructions);
                           },
                           [this, &instructions](const IR::JumpIfZero &instruction) {
                               convert_jump_if_zero(instruction, instructions);
                           },
                           [this, &instructions](const IR::JumpIfNotZero &instruction) {
                               convert_jump_if_not_zero(instruction, instructions);
                           },
                           [this, &instructions](const IR::Copy &instruction) {
                               convert_copy(instruction, instructions);
                           },
                           [this, &instructions](const IR::Label &instruction) {
                               convert_label(instruction, instructions);
                           },
                           [this, &instructions](const IR::Call &instruction) {
                               convert_call(instruction, instructions);
                           },
                            [this, &instructions](const IR::SignExtend& instruction) {
                                convert_sign_extend(instruction, instructions);
                            },
                        [this, &instructions](const IR::Truncate &instruction) {
                            convert_trunctate(instruction, instructions);
                        },
                [this, &instructions](const IR::ZeroExtend &instruction) {
                    convert_zero_extend(instruction, instructions);
                },
                [this, &instructions](const IR::DoubleToInt &instruction) {
                    convert_double_to_int(instruction, instructions);
                },
                [this, &instructions](const IR::DoubleToUInt &instruction) {
                    convert_double_to_uint(instruction, instructions);
                },
                [this, &instructions](const IR::IntToDouble &instruction) {
                    convert_int_to_double(instruction, instructions);
                },
                [this, &instructions](const IR::UIntToDouble &instruction) {
                    convert_uint_to_double(instruction, instructions);
                },

                       }, instruction);
        }

        void convert_return(const IR::Return &instruction, std::vector<ASM::Instruction> &instructions) {
            if (get_type_for_value(instruction.value) == ASM::Type::Double) {
                instructions.emplace_back(ASM::Mov(get_type_for_value(instruction.value), convert_value(instruction.value), ASM::Reg(ASM::Reg::Name::XMM0)));
            } else {
                instructions.emplace_back(ASM::Mov(get_type_for_value(instruction.value), convert_value(instruction.value), ASM::Reg(ASM::Reg::Name::AX)));
            }
            instructions.emplace_back(ASM::Ret());
        }

        ASM::Operand convert_value(const IR::Value &value) {
            return std::visit(overloaded{
                                  [this](const IR::Constant &constant) -> ASM::Operand {
                                      return std::visit(overloaded {
                                          [this](const AST::ConstDouble& c) -> ASM::Operand {

                                              if (constants.contains(std::make_pair(c.value, 8))) {
                                                  return ASM::Data(constants[std::make_pair(c.value, 8)]);
                                              }
                                              auto label = gen_const_label();
                                              asmProgram.items.emplace_back(ASM::StaticConstant(label, 8, InitialDouble(c.value)));
                                              return ASM::Data(label);
                                          },
                                          [](const auto& c) -> ASM::Operand {
                                              return ASM::Imm(c.value);
                                          }
                                      }, constant.constant);
                                  },
                                  [](const IR::Variable &variable) -> ASM::Operand {
                                      return ASM::Pseudo(variable.name);
                                  },

                              }, value);
        }

        void convert_unary(const IR::Unary &instruction, std::vector<ASM::Instruction> &instructions) {
            if (instruction.op == IR::Unary::Operator::LOGICAL_NOT) {
                if (get_type_for_value(instruction.source) == ASM::Type::Double) {
                    instructions.emplace_back(ASM::Binary(ASM::Binary::Operator::XOR, ASM::Type::Double, ASM::Reg(ASM::Reg::Name::XMM0), ASM::Reg(ASM::Reg::Name::XMM0)));
                    instructions.emplace_back(ASM::Cmp(get_type_for_value(instruction.source),  convert_value(instruction.source), ASM::Reg(ASM::Reg::Name::XMM0)));
                } else {
                    instructions.emplace_back(ASM::Cmp(get_type_for_value(instruction.source), ASM::Imm(0), convert_value(instruction.source)));
                }
                instructions.emplace_back(ASM::Mov(get_type_for_value(instruction.destination), ASM::Imm(0), convert_value(instruction.destination)));
                instructions.emplace_back(ASM::SetCC(ASM::ConditionCode::E, convert_value(instruction.destination)));
                return;
            }

            if (instruction.op == IR::Unary::Operator::NEGATE && get_type_for_value(instruction.source) == ASM::Type::Double) {
                // no asm instruction for negation of double instead xor it with -0.0
                ASM::Operand mask;
                if (constants.contains(std::make_pair(-0.0, 16))) {
                    mask = ASM::Data(constants[std::make_pair(-0.0, 16)]);
                } else {
                    auto label = gen_const_label();
                    asmProgram.items.emplace_back(ASM::StaticConstant(label, 16, InitialDouble(-0.0)));
                    mask = ASM::Data(label);
                }

                instructions.emplace_back(ASM::Mov(get_type_for_value(instruction.source), convert_value(instruction.source),
                                               convert_value(instruction.destination)));
                instructions.emplace_back(ASM::Binary(ASM::Binary::Operator::XOR, get_type_for_value(instruction.source), mask, convert_value(instruction.destination)));
                return;
            }

            static auto convert_op = [](const IR::Unary::Operator &op) {
                switch (op) {
                    case IR::Unary::Operator::NEGATE:
                        return ASM::Unary::Operator::Neg;
                    case IR::Unary::Operator::COMPLEMENT:
                        return ASM::Unary::Operator::Not;
                }
            };

            instructions.emplace_back(ASM::Mov(get_type_for_value(instruction.source), convert_value(instruction.source),
                                               convert_value(instruction.destination)));
            instructions.emplace_back(ASM::Unary(convert_op(instruction.op),get_type_for_value(instruction.destination), convert_value(instruction.destination)));
        }

        void convert_binary(const IR::Binary &instruction, std::vector<ASM::Instruction> &instructions) {
            if (instruction.op == IR::Binary::Operator::EQUAL || instruction.op == IR::Binary::Operator::GREATER ||
                instruction.op == IR::Binary::Operator::GREATER_EQUAL || instruction.op == IR::Binary::Operator::LESS ||
                instruction.op == IR::Binary::Operator::LESS_EQUAL || instruction.op ==
                IR::Binary::Operator::NOT_EQUAL) {
                convert_relational(instruction, instructions);
                return;
            }

            if ((instruction.op == IR::Binary::Operator::DIVIDE && get_type_for_value(instruction.left_source) != ASM::Type::Double) || instruction.op == IR::Binary::Operator::REMAINDER) {
                instructions.emplace_back(
                    ASM::Mov(get_type_for_value(instruction.left_source), convert_value(instruction.left_source), ASM::Reg(ASM::Reg::Name::AX)));
                if (is_unsigned(instruction.left_source)) {
                    instructions.emplace_back(ASM::Mov(get_type_for_value(instruction.left_source), ASM::Imm(0), ASM::Reg(ASM::Reg::Name::DX)));
                    instructions.emplace_back(ASM::Div(get_type_for_value(instruction.left_source), convert_value(instruction.right_source)));
                } else {
                    instructions.emplace_back(ASM::Cdq(get_type_for_value(instruction.left_source)));
                    instructions.emplace_back(ASM::Idiv(get_type_for_value(instruction.left_source), convert_value(instruction.right_source)));
                }
                instructions.emplace_back(ASM::Mov(
                    get_type_for_value(instruction.left_source),
                    ASM::Reg(instruction.op == IR::Binary::Operator::DIVIDE ? ASM::Reg::Name::AX : ASM::Reg::Name::DX),
                    convert_value(instruction.destination)));
            } else {
                bool op_unsigned = is_unsigned(instruction.left_source);
                auto convert_op = [op_unsigned](const IR::Binary::Operator &op) {
                    switch (op) {
                        case IR::Binary::Operator::ADD:
                            return ASM::Binary::Operator::ADD;
                        case IR::Binary::Operator::SUBTRACT:
                            return ASM::Binary::Operator::SUB;
                        case IR::Binary::Operator::MULTIPLY:
                            return ASM::Binary::Operator::MULT;
                        case IR::Binary::Operator::SHIFT_LEFT:
                            return ASM::Binary::Operator::SHL;
                        case IR::Binary::Operator::SHIFT_RIGHT:
                            return op_unsigned ? ASM::Binary::Operator::SHR : ASM::Binary::Operator::SAR; // arithmetic shift when performed on signed integers
                        case IR::Binary::Operator::BITWISE_AND:
                            return ASM::Binary::Operator::AND;
                        case IR::Binary::Operator::BITWISE_OR:
                            return ASM::Binary::Operator::OR;
                        case IR::Binary::Operator::BITWISE_XOR:
                            return ASM::Binary::Operator::XOR;
                        case IR::Binary::Operator::DIVIDE:
                            return ASM::Binary::Operator::DIV_DOUBLE; // only double division at this point
                        default:
                            // panic!
                            std::unreachable();
                    }
                };

                instructions.emplace_back(ASM::Mov(get_type_for_value(instruction.left_source), convert_value(instruction.left_source),
                                                   convert_value(instruction.destination)));
                instructions.emplace_back(ASM::Binary( convert_op(instruction.op),get_type_for_value(instruction.left_source),
                                                      convert_value(instruction.right_source),
                                                      convert_value(instruction.destination)));
            }
        }

        void convert_relational(const IR::Binary &instruction, std::vector<ASM::Instruction> &instructions) {
            // add comment!
            bool use_alt_instructions = is_unsigned(instruction.left_source) || get_type_for_value(instruction.left_source) == ASM::Type::Double;
            auto convert_op = [use_alt_instructions](const IR::Binary::Operator &op) {
                switch (op) {
                    case IR::Binary::Operator::EQUAL:
                        return ASM::ConditionCode::E;
                    case IR::Binary::Operator::GREATER:
                        return use_alt_instructions ? ASM::ConditionCode::A : ASM::ConditionCode::G;
                    case IR::Binary::Operator::GREATER_EQUAL:
                        return use_alt_instructions ? ASM::ConditionCode::AE : ASM::ConditionCode::GE;
                    case IR::Binary::Operator::LESS:
                        return use_alt_instructions ? ASM::ConditionCode::B : ASM::ConditionCode::L;
                    case IR::Binary::Operator::LESS_EQUAL:
                        return use_alt_instructions ? ASM::ConditionCode::BE : ASM::ConditionCode::LE;
                    case IR::Binary::Operator::NOT_EQUAL:
                        return ASM::ConditionCode::NE;
                    default:
                        std::unreachable();
                }
            };
            // fixes for nan
            // currently copied from gcc output
            // todo: explain how it works
            bool is_double = get_type_for_value(instruction.left_source) == ASM::Type::Double;
            auto op = convert_op(instruction.op);
            // TODO: cleanup
            if (is_double && op == ASM::ConditionCode::B) {
                // swap right and left
                instructions.emplace_back(ASM::Cmp(get_type_for_value(instruction.left_source),
                                               convert_value(instruction.left_source), convert_value(instruction.right_source)));

                op = ASM::ConditionCode::A;
                instructions.emplace_back(ASM::Mov(get_type_for_value(instruction.destination), ASM::Imm(0), convert_value(instruction.destination)));
                instructions.emplace_back(ASM::SetCC(op, convert_value(instruction.destination)));
            } else if (is_double && op == ASM::ConditionCode::BE) {
                instructions.emplace_back(ASM::Cmp(get_type_for_value(instruction.left_source),
                                                convert_value(instruction.left_source), convert_value(instruction.right_source)));

                op = ASM::ConditionCode::AE;
                instructions.emplace_back(ASM::Mov(get_type_for_value(instruction.destination), ASM::Imm(0), convert_value(instruction.destination)));
                instructions.emplace_back(ASM::SetCC(op, convert_value(instruction.destination)));
            } else if (is_double && op == ASM::ConditionCode::E) {
                // TODO: gcc handles this more efficently
                auto end_label = gen_jump_label("end");
                instructions.emplace_back(ASM::Cmp(get_type_for_value(instruction.left_source), convert_value(instruction.right_source),
                                               convert_value(instruction.left_source)));
                // set to zero if parity is 1
                instructions.emplace_back(ASM::Mov(get_type_for_value(instruction.destination), ASM::Imm(0), convert_value(instruction.destination)));
                instructions.emplace_back(ASM::SetCC(ASM::ConditionCode::NP, convert_value(instruction.destination)));
                instructions.emplace_back(ASM::JmpCC(ASM::ConditionCode::P, end_label));
                instructions.emplace_back(ASM::SetCC(ASM::ConditionCode::E, convert_value(instruction.destination)));
                instructions.emplace_back(ASM::Label(end_label));
            } else if (is_double && op == ASM::ConditionCode::NE) {
                auto end_label = gen_jump_label("end");
                instructions.emplace_back(ASM::Cmp(get_type_for_value(instruction.left_source), convert_value(instruction.right_source),
                                               convert_value(instruction.left_source)));
                // set to zero if parity is 1
                instructions.emplace_back(ASM::Mov(get_type_for_value(instruction.destination), ASM::Imm(0), convert_value(instruction.destination)));
                instructions.emplace_back(ASM::SetCC(ASM::ConditionCode::P, convert_value(instruction.destination)));
                instructions.emplace_back(ASM::JmpCC(ASM::ConditionCode::P, end_label));
                instructions.emplace_back(ASM::SetCC(ASM::ConditionCode::NE, convert_value(instruction.destination)));
                instructions.emplace_back(ASM::Label(end_label));
            } else {
                instructions.emplace_back(ASM::Cmp(get_type_for_value(instruction.left_source), convert_value(instruction.right_source),
                                               convert_value(instruction.left_source)));
                instructions.emplace_back(ASM::Mov(get_type_for_value(instruction.destination), ASM::Imm(0), convert_value(instruction.destination)));
                instructions.emplace_back(ASM::SetCC(op, convert_value(instruction.destination)));
            }



        }

        void convert_jump(const IR::Jump &instruction, std::vector<ASM::Instruction> &instructions) {
            instructions.emplace_back(ASM::Jmp(instruction.target));
        }

        void convert_jump_if_zero(const IR::JumpIfZero &instruction, std::vector<ASM::Instruction> &instructions) {
            if (get_type_for_value(instruction.condition) == ASM::Type::Double) {
                instructions.emplace_back(ASM::Binary(ASM::Binary::Operator::XOR, ASM::Type::Double, ASM::Reg(ASM::Reg::Name::XMM0), ASM::Reg(ASM::Reg::Name::XMM0)));
                instructions.emplace_back(ASM::Cmp(get_type_for_value(instruction.condition),  convert_value(instruction.condition), ASM::Reg(ASM::Reg::Name::XMM0)));
            } else {
                instructions.emplace_back(ASM::Cmp(get_type_for_value(instruction.condition), ASM::Imm(0), convert_value(instruction.condition)));
            }
            instructions.emplace_back(ASM::JmpCC(ASM::ConditionCode::E, instruction.target));
        }

        void convert_jump_if_not_zero(const IR::JumpIfNotZero &instruction,
                                      std::vector<ASM::Instruction> &instructions) {
            if (get_type_for_value(instruction.condition) == ASM::Type::Double) {
                instructions.emplace_back(ASM::Binary(ASM::Binary::Operator::XOR, ASM::Type::Double, ASM::Reg(ASM::Reg::Name::XMM0), ASM::Reg(ASM::Reg::Name::XMM0)));
                instructions.emplace_back(ASM::Cmp(get_type_for_value(instruction.condition),  convert_value(instruction.condition), ASM::Reg(ASM::Reg::Name::XMM0)));
            } else {
                instructions.emplace_back(ASM::Cmp(get_type_for_value(instruction.condition), ASM::Imm(0), convert_value(instruction.condition)));
            }
            instructions.emplace_back(ASM::JmpCC(ASM::ConditionCode::NE, instruction.target));
        }

        void convert_label(const IR::Label &instruction, std::vector<ASM::Instruction> &instructions) {
            instructions.emplace_back(ASM::Label(instruction.name));
        }

        void convert_copy(const IR::Copy &instruction, std::vector<ASM::Instruction> &instructions) {
            instructions.emplace_back(ASM::Mov(get_type_for_value(instruction.destination),convert_value(instruction.source),
                                               convert_value(instruction.destination)));
        }

        std::string gen_const_label() {
            auto label = "const." + std::to_string(cnt++);
            asmSymbols[label] = ASM::ObjectSymbol(ASM::Type::Double, true, true);
            return label;
        }
        std::string gen_jump_label(std::string label) {
            // codegen prefix needed?
            return "codegen." + label + std::to_string(cnt++);
        }

        ASM::Program asmProgram;
        std::unordered_map<std::string, ASM::Symbol> asmSymbols;

    private:
        // value, aligment
        std::map<std::pair<double, int>, std::string> constants; // TODO: support other value types;
        int cnt = 0;
        IR::Program IRProgram;
        std::unordered_map<std::string, Symbol>* symbols;
    };

    class ReplacePseudoRegistersPass {
    public:
        explicit ReplacePseudoRegistersPass(ASM::Program *asmProgram, std::unordered_map<std::string, ASM::Symbol>* symbols) : asmProgram(asmProgram), symbols(symbols) {
        }

        void process() {
            for (auto &item: asmProgram->items) {
                if (std::holds_alternative<ASM::Function>(item)) {
                    visit_function(std::get<ASM::Function>(item));
                }
            }
        }

        void visit_function(ASM::Function &function) {
            for (auto &instruction: function.instructions) {
                visit_instruction(instruction);
            }
            function.stack_size = offset;
            offset = -4;
            m_offsets.clear();
        }

        void visit_instruction(ASM::Instruction &instruction) {
            std::visit(overloaded{
                           [this](ASM::Mov &mov) {
                               replace(mov.dst);
                               replace(mov.src);
                           },
                           [this](ASM::Unary &unary) {
                               replace(unary.operand);
                           },
                           [this](ASM::Binary &binary) {
                               replace(binary.left);
                               replace(binary.right);
                           },
                           [this](ASM::Idiv &idiv) {
                               replace(idiv.divisor);
                           },
                           [this](ASM::Cmp &cmp) {
                               replace(cmp.left);
                               replace(cmp.right);
                           },
                           [this](ASM::SetCC &mov) {
                               replace(mov.destination);
                           },
                           [this](ASM::Push &push) {
                               replace(push.value);
                           },
                           [this](ASM::Movsx &mov) {
                               replace(mov.destination);
                               replace(mov.source);
                           },
                            [this](ASM::Div& div) {
                                replace(div.divisor);
                            },
                [this](ASM::MovZeroExtend& movz) {
                    replace(movz.destination);
                    replace(movz.source);
                },
                [this](ASM::Cvtsi2sd &cvtsi2sd) {
                    replace(cvtsi2sd.destination);
                    replace(cvtsi2sd.source);
                },
                [this](ASM::Cvttsd2si& cvttsd2si) {
                    replace(cvttsd2si.destination);
                    replace(cvttsd2si.source);
                },
                           [this](auto &) {
                           }
                       }, instruction);
        }

        int offset = -4;

    private:
        void replace(ASM::Operand &operand) {
            if (!std::holds_alternative<ASM::Pseudo>(operand)) {
                return;
            }

            auto pseudo = std::get<ASM::Pseudo>(operand);

            if (symbols->contains(pseudo.name) && std::get<ASM::ObjectSymbol>((*symbols)[pseudo.name]).is_static) {
                operand = ASM::Data(pseudo.name);
            } else {
                if (m_offsets.contains(pseudo.name)) {
                    operand = ASM::Stack{m_offsets[pseudo.name]};
                } else {
                    m_offsets[pseudo.name] = offset;
                    if (std::get<ASM::ObjectSymbol>((*symbols)[pseudo.name]).type == ASM::Type::LongWord) {
                        offset -= 4;
                    } else {
                        offset -= 8;
                        offset = ((offset - 15) / 16) * 16;
                    }
                    operand = ASM::Stack{m_offsets[pseudo.name] = offset};
                }
            }
        }

        std::unordered_map<std::string, ASM::Symbol>* symbols;
        std::unordered_map<std::string, int> m_offsets;
        ASM::Program *asmProgram;
    };

    class FixUpInstructionsPass {
    public:
        explicit FixUpInstructionsPass(ASM::Program *asmProgram, int max_offset) : asmProgram(asmProgram),
            m_max_offset(max_offset) {
        }

        void process() {
            for (auto &item: asmProgram->items) {
                if (std::holds_alternative<ASM::Function>(item)) {
                    visit_function(std::get<ASM::Function>(item));
                }
            }
        }

        void visit_function(ASM::Function &function) {
            std::vector<ASM::Instruction> fixed_instructions;
            int rounded_stack_size = ((-function.stack_size + 15) / 16) * 16;
            fixed_instructions.emplace_back(ASM::Binary(ASM::Binary::Operator::SUB, ASM::Type::QuadWord, ASM::Imm(rounded_stack_size), ASM::Reg(ASM::Reg::Name::SP)));

            for (auto &instruction: function.instructions) {
                visit_instruction(instruction, fixed_instructions);
            }

            function.instructions = std::move(fixed_instructions);
        }

        void fix_movsx(ASM::Movsx & mov, std::vector<ASM::Instruction> & output) {
            if (std::holds_alternative<ASM::Imm>(mov.source)) {
                auto source = mov.source;
                mov.source = ASM::Reg(ASM::Reg::Name::R10);
                output.emplace_back(ASM::Mov(ASM::Type::LongWord, source,mov.source));
            }
            if (is_memory_address(mov.destination)) {
                auto destination = mov.destination;
                mov.destination = ASM::Reg(ASM::Reg::Name::R11);
                output.emplace_back(mov);
                output.emplace_back(ASM::Mov(ASM::Type::QuadWord, mov.destination, destination));
            } else {
                output.emplace_back(mov);
            }
        }

        void fix_push(ASM::Push & push, std::vector<ASM::Instruction> & output) {
            if (should_fix_imm(push.value)) {
                output.emplace_back(ASM::Mov(ASM::Type::QuadWord, push.value, ASM::Reg(ASM::Reg::Name::R10)));
                push.value= ASM::Reg(ASM::Reg::Name::R10);
            }
            output.emplace_back(push);
        }

        void fix_mov_zero_extend(const ASM::MovZeroExtend & mov, std::vector<ASM::Instruction> & output) {
            if (std::holds_alternative<ASM::Reg>(mov.destination)) {
                output.emplace_back(ASM::Mov(ASM::Type::LongWord, mov.source, mov.destination));
            } else if (is_memory_address(mov.destination)) {
                output.emplace_back(ASM::Mov(ASM::Type::LongWord, mov.source, ASM::Reg(ASM::Reg::Name::R11)));
                output.emplace_back(ASM::Mov(ASM::Type::QuadWord, ASM::Reg(ASM::Reg::Name::R11), mov.destination));
            }
        }

        void fix_cvtsi2d(ASM::Cvtsi2sd & cvtsi2sd, std::vector<ASM::Instruction> & output) {
            if (std::holds_alternative<ASM::Imm>(cvtsi2sd.source)) {
                output.emplace_back(ASM::Mov(cvtsi2sd.type, cvtsi2sd.source, ASM::Reg(ASM::Reg::Name::R10)));
                cvtsi2sd.source = ASM::Reg(ASM::Reg::Name::R10);
            }

            if (!std::holds_alternative<ASM::Reg>(cvtsi2sd.destination)) {
                auto destination = cvtsi2sd.destination;
                cvtsi2sd.destination = ASM::Reg(ASM::Reg::Name::XMM15);
                output.emplace_back(cvtsi2sd);
                output.emplace_back(ASM::Mov(ASM::Type::Double, ASM::Reg(ASM::Reg::Name::XMM15), destination));
            } else {
                output.emplace_back(cvtsi2sd);
            }
        }

        void fix_cvttsd2si(ASM::Cvttsd2si & cvttsd2si, std::vector<ASM::Instruction> & output) {
            if (!std::holds_alternative<ASM::Reg>(cvttsd2si.destination)) {
                auto destination = cvttsd2si.destination;
                cvttsd2si.destination = ASM::Reg(ASM::Reg::Name::R11);
                output.emplace_back(cvttsd2si);
                output.emplace_back(ASM::Mov(cvttsd2si.type, ASM::Reg(ASM::Reg::Name::R11), destination));
            } else {
                output.emplace_back(cvttsd2si);
            }
        }

        void visit_instruction(ASM::Instruction &instruction, std::vector<ASM::Instruction> &output) {
            std::visit(overloaded{
                           [this, &output](ASM::Mov &mov) {
                               fix_mov(mov, output);
                           },
                           [this, &output](ASM::Binary &binary) {
                               fix_binary(binary, output);
                           },
                           [this, &output](ASM::Idiv &idiv) {
                               fix_idiv(idiv, output);
                           },
                            [this, &output](ASM::Cmp &cmp) {
                                fix_cmp(cmp, output);
                            },
                             [this, &output](ASM::Movsx &mov) {
                               fix_movsx(mov, output);
                             },
                            [this, &output](ASM::Push& push) {
                                fix_push(push, output);
                            },
                [this, &output](ASM::Div &div) {
                    fix_div(div, output);
                },
                [this, &output](ASM::MovZeroExtend &mov) {
                    fix_mov_zero_extend(mov, output);
                },
                [this, &output](ASM::Cvtsi2sd &cvtsi2sd) {
                    fix_cvtsi2d(cvtsi2sd, output);
                },
                [this, &output](ASM::Cvttsd2si& cvttsd2si) {
                    fix_cvttsd2si(cvttsd2si, output);
                },
                           [&output](auto &instruction) {
                               output.emplace_back(instruction);
                           },
                       }, instruction);
        }

        bool is_memory_address(const ASM::Operand& operand) {
            return std::holds_alternative<ASM::Stack>(operand) || std::holds_alternative<ASM::Data>(operand);
        }

        void fix_mov(ASM::Mov &mov, std::vector<ASM::Instruction> &output) {
            if (is_memory_address(mov.src) && is_memory_address(mov.dst)) {
                if (mov.type == ASM::Type::Double) {
                    output.emplace_back(ASM::Mov{mov.type, mov.src, ASM::Reg(ASM::Reg::Name::XMM14)});
                    mov.src = ASM::Reg(ASM::Reg::Name::XMM14);
                } else {
                    output.emplace_back(ASM::Mov{mov.type, mov.src, ASM::Reg(ASM::Reg::Name::R10)});
                    mov.src = ASM::Reg(ASM::Reg::Name::R10);
                }

            }
            if (should_fix_imm(mov.src) ) {
                if (mov.type == ASM::Type::LongWord) {
                    std::get<ASM::Imm>(mov.src).value &= 0xffffffff; // truncate to 32-bit
                } else if (is_memory_address(mov.dst)) {
                    output.emplace_back(ASM::Mov{mov.type, mov.src, ASM::Reg(ASM::Reg::Name::R10)});
                    mov.src = ASM::Reg(ASM::Reg::Name::R10);
                }
            }
            output.emplace_back(mov);
        }

        bool should_fix_imm(ASM::Operand &operand) {
            return std::holds_alternative<ASM::Imm>(operand) && std::get<ASM::Imm>(operand).value > std::numeric_limits<int>::max();
        }

        void fix_binary(ASM::Binary &binary, std::vector<ASM::Instruction> &output) {
            // mess!!!!
            // does this even work!
            if (binary.type == ASM::Type::Double) {
                if (binary.op == ASM::Binary::Operator::ADD || binary.op == ASM::Binary::Operator::SUB || binary.op == ASM::Binary::Operator::MULT || binary.op == ASM::Binary::Operator::DIV_DOUBLE || binary.op == ASM::Binary::Operator::XOR) {
                    if (!std::holds_alternative<ASM::Reg>(binary.right)) {

                        auto destination = binary.right;
                        binary.right = ASM::Reg(ASM::Reg::Name::XMM15);
                        output.emplace_back(ASM::Mov(ASM::Type::Double, destination, ASM::Reg(ASM::Reg::Name::XMM15)));
                        output.emplace_back(binary);
                        output.emplace_back(ASM::Mov(ASM::Type::Double, ASM::Reg(ASM::Reg::Name::XMM15), destination));
                    } else {
                        output.emplace_back(binary);
                    }
                } else {
                    std::abort();
                }
            } else {
                if ((binary.op == ASM::Binary::Operator::SHR || binary.op == ASM::Binary::Operator::SHL || binary.op == ASM::Binary::Operator::SAR) &&
                is_memory_address(binary.right)) {
                    output.emplace_back(ASM::Mov{binary.type, binary.left, ASM::Reg(ASM::Reg::Name::CX)});
                    binary.left = ASM::Reg(ASM::Reg::Name::CX);
                    output.emplace_back(binary);
                } else if (binary.op == ASM::Binary::Operator::MULT && is_memory_address(binary.right)) {
                    if (should_fix_imm(binary.left)) {
                        output.emplace_back(ASM::Mov(binary.type, binary.left, ASM::Reg(ASM::Reg::Name::R10)));
                        binary.left = ASM::Reg(ASM::Reg::Name::R10);
                    }

                    output.emplace_back(ASM::Mov(binary.type, binary.right, ASM::Reg(ASM::Reg::Name::R11)));
                    output.emplace_back(
                        ASM::Binary(binary.op, binary.type, binary.left, ASM::Reg(ASM::Reg::Name::R11)));
                    output.emplace_back(ASM::Mov{binary.type, ASM::Reg(ASM::Reg::Name::R11), binary.right});
                } else if ((is_memory_address(binary.left) && is_memory_address(
                               binary.right)) || should_fix_imm(binary.left)) {
                    output.emplace_back(ASM::Mov{binary.type, binary.left, ASM::Reg(ASM::Reg::Name::R10)});
                    binary.left = ASM::Reg(ASM::Reg::Name::R10);
                    output.emplace_back(binary);
                               } else {
                                   output.emplace_back(binary);
                               }
            }


        }

        void fix_idiv(ASM::Idiv &idiv, std::vector<ASM::Instruction> &output) {
            if (std::holds_alternative<ASM::Imm>(idiv.divisor)) {
                output.emplace_back(ASM::Mov{idiv.type, idiv.divisor, ASM::Reg(ASM::Reg::Name::R10)});
                idiv.divisor = ASM::Reg(ASM::Reg::Name::R10);
            }
            output.emplace_back(idiv);
        }

        void fix_div(ASM::Div& div, std::vector<ASM::Instruction> &output) {
            if (std::holds_alternative<ASM::Imm>(div.divisor)) {
                output.emplace_back(ASM::Mov{div.type, div.divisor, ASM::Reg(ASM::Reg::Name::R10)});
                div.divisor = ASM::Reg(ASM::Reg::Name::R10);
            }
            output.emplace_back(div);
        }

        void fix_cmp(ASM::Cmp &cmp, std::vector<ASM::Instruction> &output) {
            if (cmp.type == ASM::Type::Double) {
                if (!std::holds_alternative<ASM::Reg>(cmp.right)) {
                    auto destination = cmp.right;
                    cmp.right = ASM::Reg(ASM::Reg::Name::XMM15);
                    output.emplace_back(ASM::Mov(ASM::Type::Double, destination, ASM::Reg(ASM::Reg::Name::XMM15)));
                    output.emplace_back(cmp);
                } else {
                    output.emplace_back(cmp);
                }
            } else {
                if (is_memory_address(cmp.left) && is_memory_address(cmp.right)) {
                    output.emplace_back(ASM::Mov{cmp.type, cmp.left, ASM::Reg(ASM::Reg::Name::R10)});
                    cmp.left = ASM::Reg(ASM::Reg::Name::R10);
                }

                if (should_fix_imm(cmp.left)) {
                    output.emplace_back(ASM::Mov(cmp.type, cmp.left, ASM::Reg(ASM::Reg::Name::R10)));
                    cmp.left = ASM::Reg(ASM::Reg::Name::R10);
                }

                if (std::holds_alternative<ASM::Imm>(cmp.right)) {
                    output.emplace_back(ASM::Mov{cmp.type, cmp.right, ASM::Reg(ASM::Reg::Name::R11)});
                    cmp.right = ASM::Reg(ASM::Reg::Name::R11);
                }
                output.emplace_back(cmp);
            }
        }

    private:
        int m_max_offset;
        ASM::Program *asmProgram;
    };
}


#endif //CODEGEN_H
