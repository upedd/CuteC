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
        IRToAsmTreePass(IR::Program IRProgram,
                        std::unordered_map<std::string, Symbol> *symbols) : IRProgram(std::move(IRProgram)),
                                                                            symbols(symbols) {
        }

        ASM::Type get_type_for_identifier(const std::string &name) {
            return std::visit(overloaded{
                                  [](const AST::IntType &) -> ASM::Type {
                                      return ASM::LongWord();
                                  },
                                  [](const AST::UIntType &) -> ASM::Type {
                                      return ASM::LongWord();
                                  },
                                  [](const AST::LongType &) -> ASM::Type {
                                      return ASM::QuadWord();
                                  },
                                  [](const AST::ULongType &) -> ASM::Type {
                                      return ASM::QuadWord();
                                  },
                                    [](const AST::CharType&) -> ASM::Type {
                                        return ASM::Byte();
                                    },
                                    [](const AST::UCharType&) -> ASM::Type {
                                        return ASM::Byte();
                                    },
                                    [](const AST::SignedCharType&) -> ASM::Type {
                                        return ASM::Byte();
                                    },
                                  [](const AST::DoubleType &) -> ASM::Type {
                                      return ASM::Double();
                                  },
                                  [](const AST::PointerType &) -> ASM::Type {
                                      return ASM::QuadWord();
                                  },
                                  [](const AST::ArrayType &) -> ASM::Type {
                                      return ASM::ByteArray(); // TODO
                                  },
                                  [](const auto &) -> ASM::Type {
                                      std::unreachable();
                                  }
                              }, *(*symbols)[name].type);
        }

        ASM::Type get_type_for_value(const IR::Value &value) {
            return std::visit(overloaded{
                                  [](const IR::Constant &val) {
                                      return std::visit(overloaded{
                                                            [](const AST::ConstInt &) -> ASM::Type {
                                                                return ASM::LongWord();
                                                            },
                                                            [](const AST::ConstUInt &) -> ASM::Type {
                                                                return ASM::LongWord();
                                                            },
                                                            [](const AST::ConstLong &) -> ASM::Type {
                                                                return ASM::QuadWord();
                                                            },
                                                            [](const AST::ConstULong &) -> ASM::Type {
                                                                return ASM::QuadWord();
                                                            },
                                                            [](const AST::ConstDouble &) -> ASM::Type {
                                                                return ASM::Double();
                                                            },
                                                            [](const AST::ConstChar&) -> ASM::Type {
                                                                return ASM::Byte();
                                                            },
                                                            [](const AST::ConstUChar&) -> ASM::Type {
                                                                return ASM::Byte();
                                                            }
                                                        }, val.constant);
                                  },
                                  [this](const IR::Variable &val) {
                                      return get_type_for_identifier(val.name);
                                  }
                              }, value);
        }

        bool is_unsigned(const IR::Value &value) {
            return std::visit(overloaded{
                                  [](const IR::Constant &val) {
                                      return std::holds_alternative<AST::ConstUInt>(val.constant) ||
                                             std::holds_alternative<AST::ConstULong>(val.constant) ||
                                                 std::holds_alternative<AST::ConstUChar>(val.constant);
                                  },
                                  [this](const IR::Variable &val) {
                                      auto type = (*symbols)[val.name].type;
                                      return std::holds_alternative<AST::UIntType>(*type) || std::holds_alternative<
                                                 AST::ULongType>(*type) || std::holds_alternative<AST::UCharType>(*type);
                                  }
                              }, value);
        }

        bool is_pointer(const IR::Value& value) {
            return std::visit(overloaded{
                                 [](const IR::Constant &val) {
                                     return false; // never pointer constants?
                                 },
                                 [this](const IR::Variable &val) {
                                     auto type = (*symbols)[val.name].type;
                                     return std::holds_alternative<AST::PointerType>(*type);
                                 }
                             }, value);
        }



        void convert() {
            for (const auto &item: IRProgram.items) {
                std::visit(overloaded{
                               [this](const IR::Function &function) {
                                   asmProgram.items.emplace_back(convert_function(function));
                               },
                               [this](const IR::StaticVariable &variable) {
                                   asmProgram.items.emplace_back(convert_static_variable(variable));
                               },
                               [this](const IR::StaticConstant &constant) {
                                   asmProgram.items.emplace_back(convert_static_constant(constant));
                               }
                           }, item);
            }

            for (const auto &[name, symbol]: *symbols) {
                bool is_constant = std::holds_alternative<ConstantAttributes>(symbol.attributes);
                asmSymbols[name] = std::visit(overloaded{
                                                  [&symbol, is_constant](const AST::IntType &) -> ASM::Symbol {
                                                      return ASM::ObjectSymbol{
                                                          ASM::LongWord(),
                                                          std::holds_alternative<StaticAttributes>(symbol.attributes) || is_constant,
                                                          is_constant
                                                      };
                                                  },
                                                  [&symbol, is_constant](const AST::LongType &) -> ASM::Symbol {
                                                      return ASM::ObjectSymbol{
                                                          ASM::QuadWord(),
                                                          std::holds_alternative<StaticAttributes>(symbol.attributes) || is_constant,
                                                          is_constant
                                                      };
                                                  },
                                                  [&symbol, is_constant](const AST::ULongType &) -> ASM::Symbol {
                                                      return ASM::ObjectSymbol{
                                                          ASM::QuadWord(),
                                                          std::holds_alternative<StaticAttributes>(symbol.attributes) || is_constant,
                                                          is_constant
                                                      };
                                                  },
                                                  [&symbol, is_constant](const AST::UIntType &) -> ASM::Symbol {
                                                      return ASM::ObjectSymbol{
                                                          ASM::LongWord(),
                                                          std::holds_alternative<StaticAttributes>(symbol.attributes) || is_constant,
                                                          is_constant
                                                      };
                                                  },
                                                  [&symbol, is_constant](const AST::DoubleType &) -> ASM::Symbol {
                                                      return ASM::ObjectSymbol{
                                                          ASM::Double(),
                                                          std::holds_alternative<StaticAttributes>(symbol.attributes) || is_constant,
                                                          is_constant
                                                      };
                                                  },
                    [&symbol, is_constant](const AST::CharType &) -> ASM::Symbol {
                                                      return ASM::ObjectSymbol{
                                                          ASM::Byte(),
                                                          std::holds_alternative<StaticAttributes>(symbol.attributes) || is_constant,
                                                          is_constant
                                                      };
                                                  },
                    [&symbol, is_constant](const AST::UCharType &) -> ASM::Symbol {
                                                      return ASM::ObjectSymbol{
                                                          ASM::Byte(),
                                                          std::holds_alternative<StaticAttributes>(symbol.attributes) || is_constant,
                                                          is_constant
                                                      };
                                                  },
                    [&symbol, is_constant](const AST::SignedCharType &) -> ASM::Symbol {
                                                      return ASM::ObjectSymbol{
                                                          ASM::Byte(),
                                                          std::holds_alternative<StaticAttributes>(symbol.attributes) || is_constant,
                                                          is_constant
                                                      };
                                                  },

                                                  [&symbol](const AST::FunctionType &) -> ASM::Symbol {
                                                      return ASM::FunctionSymbol{
                                                          std::get<FunctionAttributes>(symbol.attributes).defined
                                                      };
                                                  },
                                                  [&symbol, is_constant](const AST::PointerType &) -> ASM::Symbol {
                                                      return ASM::ObjectSymbol{
                                                          ASM::QuadWord(),
                                                          std::holds_alternative<StaticAttributes>(symbol.attributes) || is_constant,
                                                          is_constant
                                                      };
                                                  },
                                          [&symbol, is_constant, this](const AST::ArrayType& type) -> ASM::Symbol {
                                                      auto size = bytes_for_type(*type.element_type) * type.size;
                                                      return ASM::ObjectSymbol{
                                                          ASM::ByteArray(size, get_alignment_for_type(*symbol.type)),
                                                          std::holds_alternative<StaticAttributes>(symbol.attributes) || is_constant,
                                                          is_constant
                                                      };
                                                  },
                                                  [](const auto &) -> ASM::Symbol {
                                                      std::unreachable();
                                                  }
                                              }, *symbol.type);
            }
        }

        int get_alignment_for_type(const AST::Type &type) {
            return std::visit(overloaded {
                [](const AST::IntType&) {
                    return 4;
                },
                [](const AST::LongType&) {
                    return 8;
                },
                [](const AST::ULongType&) {
                    return 8;
                },
                [](const AST::UIntType&) {
                    return 4;
                },
                [](const AST::DoubleType&) {
                    return 8;
                },
                [](const AST::CharType&) {
                    return 1;
                },
                [](const AST::UCharType&) {
                    return 1;
                },
                [](const AST::SignedCharType&) {
                    return 1;
                },
                [](const AST::PointerType&) {
                    return 8;
                },
                [this](const AST::ArrayType& type) {
                    auto size = bytes_for_type(*type.element_type) * type.size;
                     return size < 16ull ? get_alignment_for_type(*type.element_type) : 16;
                },
                [](const auto&) -> int {
                    std::unreachable();
                }
            }, type);
        }

        ASM::StaticVariable convert_static_variable(const IR::StaticVariable &variable) {
            return std::visit(overloaded{
                                  [&variable, this](const AST::IntType &) -> ASM::StaticVariable {
                                      return {variable.name, variable.global, get_alignment_for_type(*variable.type), variable.initial};
                                  },
                                  [&variable, this](const AST::LongType &) -> ASM::StaticVariable {
                                      return {variable.name, variable.global, get_alignment_for_type(*variable.type), variable.initial};
                                  },
                                  [&variable, this](const AST::ULongType &) -> ASM::StaticVariable {
                                      return {variable.name, variable.global, get_alignment_for_type(*variable.type), variable.initial};
                                  },
                                  [&variable, this](const AST::UIntType &) -> ASM::StaticVariable {
                                      return {variable.name, variable.global, get_alignment_for_type(*variable.type), variable.initial};
                                  },
                                  [&variable, this](const AST::DoubleType &) -> ASM::StaticVariable {
                                      return {variable.name, variable.global, get_alignment_for_type(*variable.type), variable.initial};
                                  },
                [&variable, this](const AST::CharType &) -> ASM::StaticVariable {
                  return {variable.name, variable.global, get_alignment_for_type(*variable.type), variable.initial};
              },
                [&variable, this](const AST::UCharType &) -> ASM::StaticVariable {
                                      return {variable.name, variable.global, get_alignment_for_type(*variable.type), variable.initial};
                                  },
                [&variable, this](const AST::SignedCharType &) -> ASM::StaticVariable {
                                      return {variable.name, variable.global, get_alignment_for_type(*variable.type), variable.initial};
                                  },
                                  [&variable, this](const AST::PointerType &) -> ASM::StaticVariable {
                                      return {variable.name, variable.global, get_alignment_for_type(*variable.type), variable.initial};
                                  },
                                    [&variable, this](const AST::ArrayType & type) -> ASM::StaticVariable {
                                        return {variable.name, variable.global, get_alignment_for_type(*variable.type), variable.initial};
                                    },
                                  [](const auto &) -> ASM::StaticVariable {
                                      std::unreachable();
                                  }
                              }, *variable.type);
        }

        ASM::StaticConstant convert_static_constant(const IR::StaticConstant & constant) {
            return ASM::StaticConstant{constant.name, bytes_for_type(*constant.type), constant.initial};
        }


        ASM::Function convert_function(const IR::Function &function) {
            std::vector<ASM::Instruction> instructions;
            // duplication with call
            static std::vector int_registers = {
                ASM::Reg::Name::DI, ASM::Reg::Name::SI, ASM::Reg::Name::DX, ASM::Reg::Name::CX, ASM::Reg::Name::R8,
                ASM::Reg::Name::R9
            };
            static std::vector double_registers = {
                ASM::Reg::Name::XMM0, ASM::Reg::Name::XMM1, ASM::Reg::Name::XMM2, ASM::Reg::Name::XMM3,
                ASM::Reg::Name::XMM4, ASM::Reg::Name::XMM5, ASM::Reg::Name::XMM6, ASM::Reg::Name::XMM7
            };
            std::vector<std::string> int_params;
            std::vector<std::string> double_params;
            std::vector<std::string> stack_params;

            for (const auto &param: function.params) {
                if (std::holds_alternative<ASM::Double>(get_type_for_identifier(param))) {
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
                instructions.emplace_back(ASM::Mov(get_type_for_identifier(int_params[i]), ASM::Reg(int_registers[i]),
                                                   ASM::Pseudo(int_params[i])));
            }

            for (int i = 0; i < double_params.size(); ++i) {
                instructions.emplace_back(ASM::Mov(get_type_for_identifier(double_params[i]),
                                                   ASM::Reg(double_registers[i]), ASM::Pseudo(double_params[i])));
            }

            for (int i = 0; i < stack_params.size(); ++i) {
                instructions.emplace_back(ASM::Mov(get_type_for_identifier(stack_params[i]),
                                                   ASM::Memory(ASM::Reg(ASM::Reg::Name::BP), 16 + i * 8),
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
                ASM::Reg::Name::XMM0, ASM::Reg::Name::XMM1, ASM::Reg::Name::XMM2, ASM::Reg::Name::XMM3,
                ASM::Reg::Name::XMM4, ASM::Reg::Name::XMM5, ASM::Reg::Name::XMM6, ASM::Reg::Name::XMM7
            };
            std::vector<IR::Value> int_args;
            std::vector<IR::Value> double_args;
            std::vector<IR::Value> stack_args;

            for (const auto &arg: call.arguments) {
                if (std::holds_alternative<ASM::Double>(get_type_for_value(arg))) {
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
                instructions.emplace_back(ASM::Binary(ASM::Binary::Operator::SUB, ASM::QuadWord(),
                                                      ASM::Imm(stack_padding), ASM::Reg(ASM::Reg::Name::SP)));
            }

            // register passed arguments
            for (int i = 0; i < int_args.size(); i++) {
                instructions.emplace_back(ASM::Mov(get_type_for_value(int_args[i]), convert_value(int_args[i]),
                                                   ASM::Reg(int_registers[i])));
            }

            for (int i = 0; i < double_args.size(); i++) {
                instructions.emplace_back(ASM::Mov(get_type_for_value(double_args[i]), convert_value(double_args[i]),
                                                   ASM::Reg(double_registers[i])));
            }

            // stack passed arguments
            for (auto &stack_arg: stack_args | std::views::reverse) {
                auto arg = convert_value(stack_arg);
                auto type = get_type_for_value(stack_arg);
                if (std::holds_alternative<ASM::Imm>(arg) || std::holds_alternative<ASM::Reg>(arg) ||
                    std::holds_alternative<ASM::LongWord>(type) || std::holds_alternative<ASM::Double>(type)) {
                    instructions.emplace_back(ASM::Push(arg));
                } else {
                    instructions.emplace_back(ASM::Mov(type, arg, ASM::Reg(ASM::Reg::Name::AX)));
                    instructions.emplace_back(ASM::Push(ASM::Reg(ASM::Reg::Name::AX)));
                }
            }

            instructions.emplace_back(ASM::Call(call.name));

            int bytes_to_remove = 8 * stack_args.size() + stack_padding;

            if (bytes_to_remove != 0) {
                instructions.emplace_back(ASM::Binary(ASM::Binary::Operator::ADD, ASM::QuadWord(),
                                                      ASM::Imm(bytes_to_remove), ASM::Reg(ASM::Reg::Name::SP)));
            }

            if (call.destination) {
                if (std::holds_alternative<ASM::Double>(get_type_for_value(*call.destination))) {
                    instructions.emplace_back(ASM::Mov(get_type_for_value(*call.destination), ASM::Reg(ASM::Reg::Name::XMM0),
                                                       convert_value(*call.destination)));
                } else {
                    instructions.emplace_back(ASM::Mov(get_type_for_value(*call.destination), ASM::Reg(ASM::Reg::Name::AX),
                                                       convert_value(*call.destination)));
                }
            }
        }

        void convert_sign_extend(const IR::SignExtend &instruction, std::vector<ASM::Instruction> &instructions) {
            instructions.emplace_back(ASM::Movsx(get_type_for_value(instruction.source), get_type_for_value(instruction.destination), convert_value(instruction.source),
                                                 convert_value(instruction.destination)));
        }

        void convert_trunctate(const IR::Truncate &instruction, std::vector<ASM::Instruction> &instructions) {
            instructions.emplace_back(ASM::Mov(get_type_for_value(instruction.destination), convert_value(instruction.source),
                                               convert_value(instruction.destination)));
        }

        void convert_zero_extend(const IR::ZeroExtend &instruction, std::vector<ASM::Instruction> &instructions) {
            instructions.emplace_back(ASM::MovZeroExtend(get_type_for_value(instruction.source), get_type_for_value(instruction.destination), convert_value(instruction.source),
                                                         convert_value(instruction.destination)));
        }

        void convert_double_to_int(const IR::DoubleToInt &instruction, std::vector<ASM::Instruction> &instructions) {
            if (std::holds_alternative<ASM::Byte>(get_type_for_value(instruction.destination))) {
                instructions.emplace_back(ASM::Cvttsd2si(ASM::LongWord(), convert_value(instruction.source),
                                                         ASM::Reg(ASM::Reg::Name::AX)));
                instructions.emplace_back(ASM::Mov(ASM::Byte(), ASM::Reg(ASM::Reg::Name::AX),
                                                   convert_value(instruction.destination)));
            } else {
                instructions.emplace_back(ASM::Cvttsd2si(get_type_for_value(instruction.destination),
                                                     convert_value(instruction.source),
                                                     convert_value(instruction.destination)));
            }
        }

        void convert_double_to_uint(const IR::DoubleToUInt &instruction, std::vector<ASM::Instruction> &instructions) {
            // no asm instruction for this conversion!
            if (std::holds_alternative<ASM::Byte>(get_type_for_value(instruction.destination))) {
                instructions.emplace_back(ASM::Cvttsd2si(ASM::LongWord(), convert_value(instruction.source),
                                                         ASM::Reg(ASM::Reg::Name::AX)));
                instructions.emplace_back(ASM::Mov(ASM::Byte(), ASM::Reg(ASM::Reg::Name::AX),
                                                   convert_value(instruction.destination)));
            } else if (std::holds_alternative<ASM::LongWord>(get_type_for_value(instruction.destination))) {
                // when converting to unsigned int we can emit conversion to unsigned long and truncate the result
                instructions.emplace_back(ASM::Cvttsd2si(ASM::QuadWord(), convert_value(instruction.source),
                                                         ASM::Reg(ASM::Reg::Name::AX)));
                instructions.emplace_back(ASM::Mov(ASM::LongWord(), ASM::Reg(ASM::Reg::Name::AX),
                                                   convert_value(instruction.destination)));
            } else {
                // when converting to unsigned long we first check if the result will fit into signed long
                ASM::Operand max_value;
                if (constants.contains(std::make_pair(std::numeric_limits<std::int64_t>::max() + 1, 8))) {
                    max_value = ASM::Data(
                        constants[std::make_pair(
                            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) + 1, 8)]);
                } else {
                    auto label = gen_const_label();
                    asmProgram.items.emplace_back(ASM::StaticConstant(label, 8, Initial{
                                                                          InitialDouble(
                                                                              static_cast<std::uint64_t>(
                                                                                  std::numeric_limits<
                                                                                      std::int64_t>::max()) + 1)
                                                                      }));
                    max_value = ASM::Data(label);
                }
                auto end_label = gen_jump_label("end");
                auto out_of_range_label = gen_jump_label("out_of_range_label");
                instructions.emplace_back(ASM::Cmp(ASM::Double(), max_value, convert_value(instruction.source)));
                instructions.emplace_back(ASM::JmpCC(ASM::ConditionCode::AE, out_of_range_label));

                // if result fits into signed long we can convert it normally
                instructions.emplace_back(ASM::Cvttsd2si(ASM::QuadWord(), convert_value(instruction.source),
                                                         convert_value(instruction.destination)));
                instructions.emplace_back(ASM::Jmp(end_label));

                // if result does not fit into signed long we need to subtract LONG_MAX + 1 before conversion and add it after
                instructions.emplace_back(ASM::Label(out_of_range_label));
                instructions.emplace_back(ASM::Mov(ASM::Double(), convert_value(instruction.source),
                                                   ASM::Reg(ASM::Reg::Name::XMM1)));
                instructions.emplace_back(ASM::Binary(ASM::Binary::Operator::SUB, ASM::Double(), max_value,
                                                      ASM::Reg(ASM::Reg::Name::XMM1)));
                instructions.emplace_back(ASM::Cvttsd2si(ASM::QuadWord(), ASM::Reg(ASM::Reg::Name::XMM1),
                                                         convert_value(instruction.destination)));
                instructions.emplace_back(ASM::Mov(ASM::QuadWord(),
                                                   ASM::Imm(std::numeric_limits<std::int64_t>::max() + 1),
                                                   ASM::Reg(ASM::Reg::Name::DX)));
                instructions.emplace_back(ASM::Binary(ASM::Binary::Operator::ADD, ASM::QuadWord(),
                                                      ASM::Reg(ASM::Reg::Name::DX),
                                                      convert_value(instruction.destination)));

                instructions.emplace_back(ASM::Label(end_label));
            }
        }

        void convert_int_to_double(const IR::IntToDouble &instruction, std::vector<ASM::Instruction> &instructions) {

            if (std::holds_alternative<ASM::Byte>(get_type_for_value(instruction.source))) {
                instructions.emplace_back(
                    ASM::Movsx(ASM::Byte(), ASM::LongWord(), convert_value(instruction.source), ASM::Reg(ASM::Reg::Name::AX)));
                instructions.emplace_back(ASM::Cvtsi2sd(ASM::LongWord(), ASM::Reg(ASM::Reg::Name::AX),
                                                        convert_value(instruction.destination)));
            } else {
                instructions.emplace_back(ASM::Cvtsi2sd(get_type_for_value(instruction.source),
                                                    convert_value(instruction.source),
                                                    convert_value(instruction.destination)));
            }

        }


        void convert_uint_to_double(const IR::UIntToDouble &instruction, std::vector<ASM::Instruction> &instructions) {
            // no asm instruction for this conversion

            if (std::holds_alternative<ASM::Byte>(get_type_for_value(instruction.source))) {
                instructions.emplace_back(
                    ASM::MovZeroExtend(ASM::Byte(), ASM::LongWord(), convert_value(instruction.source), ASM::Reg(ASM::Reg::Name::AX)));
                instructions.emplace_back(ASM::Cvtsi2sd(ASM::LongWord(), ASM::Reg(ASM::Reg::Name::AX),
                                                        convert_value(instruction.destination)));
            } else if (std::holds_alternative<ASM::LongWord>(get_type_for_value(instruction.source))) {
                // when converting an unsigned integer we can zero extend it and emit conversion from signed long to double

                instructions.emplace_back(
                    ASM::MovZeroExtend(ASM::LongWord(), ASM::QuadWord(), convert_value(instruction.source), ASM::Reg(ASM::Reg::Name::AX)));
                instructions.emplace_back(ASM::Cvtsi2sd(ASM::QuadWord(), ASM::Reg(ASM::Reg::Name::AX),
                                                        convert_value(instruction.destination)));
            } else {
                // when converting an unsigned long we first check if we can fit it into signed long
                auto end_label = gen_jump_label("end");
                auto out_of_range_label = gen_jump_label("out_of_range");

                instructions.emplace_back(ASM::Cmp(ASM::QuadWord(), ASM::Imm(0), convert_value(instruction.source)));
                instructions.emplace_back(ASM::JmpCC(ASM::ConditionCode::L, out_of_range_label));

                // if source fits into signed long we convert it normally
                instructions.emplace_back(ASM::Cvtsi2sd(ASM::QuadWord(), convert_value(instruction.source),
                                                        convert_value(instruction.destination)));
                instructions.emplace_back(ASM::Jmp(end_label));

                // if source does not fit we divide it by two before conversion and multiply by 2 after conversion
                instructions.emplace_back(ASM::Label(out_of_range_label));
                instructions.emplace_back(ASM::Mov(ASM::QuadWord(), convert_value(instruction.source),
                                                   ASM::Reg(ASM::Reg::Name::AX)));
                instructions.emplace_back(ASM::Mov(ASM::QuadWord(), ASM::Reg(ASM::Reg::Name::AX),
                                                   ASM::Reg(ASM::Reg::Name::DX)));
                instructions.emplace_back(ASM::Unary(ASM::Unary::Operator::Shr, ASM::QuadWord(),
                                                     ASM::Reg(ASM::Reg::Name::DX)));
                // round to odd to avoid double rounding error
                instructions.emplace_back(ASM::Binary(ASM::Binary::Operator::AND, ASM::QuadWord(), ASM::Imm(1),
                                                      ASM::Reg(ASM::Reg::Name::AX)));
                instructions.emplace_back(ASM::Binary(ASM::Binary::Operator::OR, ASM::QuadWord(),
                                                      ASM::Reg(ASM::Reg::Name::AX), ASM::Reg(ASM::Reg::Name::DX)));

                instructions.emplace_back(ASM::Cvtsi2sd(ASM::QuadWord(), ASM::Reg(ASM::Reg::Name::DX),
                                                        convert_value(instruction.destination)));
                instructions.emplace_back(ASM::Binary(ASM::Binary::Operator::ADD, ASM::Double(),
                                                      convert_value(instruction.destination),
                                                      convert_value(instruction.destination)));

                instructions.emplace_back(ASM::Label(end_label));
            }
        }

        void convert_load(const IR::Load &load, std::vector<ASM::Instruction> &instructions) {
            instructions.emplace_back(ASM::Mov(ASM::QuadWord(), convert_value(load.source_ptr),
                                               ASM::Reg(ASM::Reg::Name::AX)));
            instructions.emplace_back(ASM::Mov(get_type_for_value(load.destination),
                                               ASM::Memory(ASM::Reg(ASM::Reg::Name::AX), 0),
                                               convert_value(load.destination)));
        }

        void convert_store(const IR::Store &store, std::vector<ASM::Instruction> &instructions) {
            instructions.emplace_back(ASM::Mov(ASM::QuadWord(), convert_value(store.destination_ptr),
                                               ASM::Reg(ASM::Reg::Name::AX)));
            instructions.emplace_back(ASM::Mov(get_type_for_value(store.source), convert_value(store.source),
                                               ASM::Memory(ASM::Reg(ASM::Reg::Name::AX), 0)));
        }

        void convert_get_address(const IR::GetAddress &instruction, std::vector<ASM::Instruction> &instructions) {
            instructions.emplace_back(ASM::Lea(convert_value(instruction.source),
                                               convert_value(instruction.destination)));
        }

        void convert_copy_to_offset(const IR::CopyToOffset &instruction, std::vector<ASM::Instruction> &instructions) {
            instructions.emplace_back(ASM::Mov(get_type_for_value(instruction.source),
                                               convert_value(instruction.source),
                                               ASM::PseudoMem(instruction.destination, instruction.offset)));
        }

        void convert_add_ptr(const IR::AddPtr &instruction, std::vector<ASM::Instruction> &instructions) {
            instructions.emplace_back(ASM::Mov(ASM::QuadWord(), convert_value(instruction.ptr), ASM::Reg(ASM::Reg::Name::AX)));
            if (std::holds_alternative<IR::Constant>(instruction.index)) {
                instructions.emplace_back(ASM::Lea(ASM::Memory(ASM::Reg(ASM::Reg::Name::AX), std::get<ASM::Imm>(convert_value(instruction.index)).value * instruction.scale), convert_value(instruction.destination)));
                return;
            }
            instructions.emplace_back(ASM::Mov(ASM::QuadWord(), convert_value(instruction.index), ASM::Reg(ASM::Reg::Name::DX)));
            if (instruction.scale == 1 || instruction.scale == 2 || instruction.scale == 4 || instruction.scale == 8) {
                instructions.emplace_back(ASM::Lea(ASM::Indexed(ASM::Reg(ASM::Reg::Name::AX), ASM::Reg(ASM::Reg::Name::DX), instruction.scale), convert_value(instruction.destination)));
            } else {
                instructions.emplace_back(ASM::Binary(ASM::Binary::Operator::MULT, ASM::QuadWord(), ASM::Imm(instruction.scale), ASM::Reg(ASM::Reg::Name::DX)));
                instructions.emplace_back(ASM::Lea(ASM::Indexed(ASM::Reg(ASM::Reg::Name::AX), ASM::Reg(ASM::Reg::Name::DX), 1), convert_value(instruction.destination)));
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
                           [this, &instructions](const IR::SignExtend &instruction) {
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
                           [this, &instructions](const IR::Load &instruction) {
                               convert_load(instruction, instructions);
                           },
                           [this, &instructions](const IR::Store &instruction) {
                               convert_store(instruction, instructions);
                           },
                           [this, &instructions](const IR::GetAddress &instruction) {
                               convert_get_address(instruction, instructions);
                           },
                           [this, &instructions](const IR::CopyToOffset &instruction) {
                               convert_copy_to_offset(instruction, instructions);
                           },
                           [this, &instructions](const IR::AddPtr &instruction) {
                               convert_add_ptr(instruction, instructions);
                           }

                       }, instruction);
        }

        void convert_return(const IR::Return &instruction, std::vector<ASM::Instruction> &instructions) {
            if (instruction.value) {
                if (std::holds_alternative<ASM::Double>(get_type_for_value(*instruction.value))) {
                    instructions.emplace_back(ASM::Mov(get_type_for_value(*instruction.value),
                                                       convert_value(*instruction.value), ASM::Reg(ASM::Reg::Name::XMM0)));
                } else {
                    instructions.emplace_back(ASM::Mov(get_type_for_value(*instruction.value),
                                                       convert_value(*instruction.value), ASM::Reg(ASM::Reg::Name::AX)));
                }
            }
            instructions.emplace_back(ASM::Ret());
        }

        ASM::Operand convert_value(const IR::Value &value) {
            return std::visit(overloaded{
                                  [this](const IR::Constant &constant) -> ASM::Operand {
                                      return std::visit(overloaded{
                                                            [this](const AST::ConstDouble &c) -> ASM::Operand {
                                                                if (constants.contains(std::make_pair(c.value, 8))) {
                                                                    return ASM::Data(
                                                                        constants[std::make_pair(c.value, 8)]);
                                                                }
                                                                auto label = gen_const_label();
                                                                asmProgram.items.emplace_back(
                                                                    ASM::StaticConstant(
                                                                        label, 8, Initial{InitialDouble(c.value)}));
                                                                return ASM::Data(label);
                                                            },
                                                            [](const auto &c) -> ASM::Operand {
                                                                return ASM::Imm(c.value);
                                                            }
                                                        }, constant.constant);
                                  },
                                  [this](const IR::Variable &variable) -> ASM::Operand {
                                      if (std::holds_alternative<
                                          ASM::ByteArray>(get_type_for_identifier(variable.name))) {
                                          return ASM::PseudoMem(variable.name, 0);
                                      }
                                      return ASM::Pseudo(variable.name);
                                  },

                              }, value);
        }

        void convert_unary(const IR::Unary &instruction, std::vector<ASM::Instruction> &instructions) {
            if (instruction.op == IR::Unary::Operator::LOGICAL_NOT) {
                if (std::holds_alternative<ASM::Double>(get_type_for_value(instruction.source))) {
                    instructions.emplace_back(ASM::Binary(ASM::Binary::Operator::XOR, ASM::Double(),
                                                          ASM::Reg(ASM::Reg::Name::XMM0),
                                                          ASM::Reg(ASM::Reg::Name::XMM0)));
                    instructions.emplace_back(ASM::Cmp(get_type_for_value(instruction.source),
                                                       convert_value(instruction.source),
                                                       ASM::Reg(ASM::Reg::Name::XMM0)));
                } else {
                    instructions.emplace_back(ASM::Cmp(get_type_for_value(instruction.source), ASM::Imm(0),
                                                       convert_value(instruction.source)));
                }
                instructions.emplace_back(ASM::Mov(get_type_for_value(instruction.destination), ASM::Imm(0),
                                                   convert_value(instruction.destination)));
                instructions.emplace_back(ASM::SetCC(ASM::ConditionCode::E, convert_value(instruction.destination)));
                return;
            }

            if (instruction.op == IR::Unary::Operator::NEGATE && std::holds_alternative<ASM::Double>(
                    get_type_for_value(instruction.source))) {
                // no asm instruction for negation of double instead xor it with -0.0
                ASM::Operand mask;
                if (constants.contains(std::make_pair(-0.0, 16))) {
                    mask = ASM::Data(constants[std::make_pair(-0.0, 16)]);
                } else {
                    auto label = gen_const_label();
                    asmProgram.items.emplace_back(ASM::StaticConstant(label, 16, Initial{InitialDouble(-0.0)}));
                    mask = ASM::Data(label);
                }

                instructions.emplace_back(ASM::Mov(get_type_for_value(instruction.source),
                                                   convert_value(instruction.source),
                                                   convert_value(instruction.destination)));
                instructions.emplace_back(ASM::Binary(ASM::Binary::Operator::XOR,
                                                      get_type_for_value(instruction.source), mask,
                                                      convert_value(instruction.destination)));
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

            instructions.emplace_back(ASM::Mov(get_type_for_value(instruction.source),
                                               convert_value(instruction.source),
                                               convert_value(instruction.destination)));
            instructions.emplace_back(ASM::Unary(convert_op(instruction.op),
                                                 get_type_for_value(instruction.destination),
                                                 convert_value(instruction.destination)));
        }

        void convert_binary(const IR::Binary &instruction, std::vector<ASM::Instruction> &instructions) {
            if (instruction.op == IR::Binary::Operator::EQUAL || instruction.op == IR::Binary::Operator::GREATER ||
                instruction.op == IR::Binary::Operator::GREATER_EQUAL || instruction.op == IR::Binary::Operator::LESS ||
                instruction.op == IR::Binary::Operator::LESS_EQUAL || instruction.op ==
                IR::Binary::Operator::NOT_EQUAL) {
                convert_relational(instruction, instructions);
                return;
            }

            if ((instruction.op == IR::Binary::Operator::DIVIDE && !std::holds_alternative<
                     ASM::Double>(get_type_for_value(instruction.left_source))) || instruction.op ==
                IR::Binary::Operator::REMAINDER) {
                instructions.emplace_back(
                    ASM::Mov(get_type_for_value(instruction.left_source), convert_value(instruction.left_source),
                             ASM::Reg(ASM::Reg::Name::AX)));
                if (is_unsigned(instruction.left_source)) {
                    instructions.emplace_back(ASM::Mov(get_type_for_value(instruction.left_source), ASM::Imm(0),
                                                       ASM::Reg(ASM::Reg::Name::DX)));
                    instructions.emplace_back(ASM::Div(get_type_for_value(instruction.left_source),
                                                       convert_value(instruction.right_source)));
                } else {
                    instructions.emplace_back(ASM::Cdq(get_type_for_value(instruction.left_source)));
                    instructions.emplace_back(ASM::Idiv(get_type_for_value(instruction.left_source),
                                                        convert_value(instruction.right_source)));
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
                            return op_unsigned ? ASM::Binary::Operator::SHR : ASM::Binary::Operator::SAR;
                        // arithmetic shift when performed on signed integers
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

                instructions.emplace_back(ASM::Mov(get_type_for_value(instruction.left_source),
                                                   convert_value(instruction.left_source),
                                                   convert_value(instruction.destination)));
                instructions.emplace_back(ASM::Binary(convert_op(instruction.op),
                                                      get_type_for_value(instruction.left_source),
                                                      convert_value(instruction.right_source),
                                                      convert_value(instruction.destination)));
            }
        }

        void convert_relational(const IR::Binary &instruction, std::vector<ASM::Instruction> &instructions) {
            // add comment!
            bool use_alt_instructions = is_unsigned(instruction.left_source) || std::holds_alternative<ASM::Double>(
                                            get_type_for_value(instruction.left_source)) || is_pointer(instruction.left_source);
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
            bool is_double = std::holds_alternative<ASM::Double>(get_type_for_value(instruction.left_source));
            auto op = convert_op(instruction.op);
            // TODO: cleanup
            if (is_double && op == ASM::ConditionCode::B) {
                // swap right and left
                instructions.emplace_back(ASM::Cmp(get_type_for_value(instruction.left_source),
                                                   convert_value(instruction.left_source),
                                                   convert_value(instruction.right_source)));

                op = ASM::ConditionCode::A;
                instructions.emplace_back(ASM::Mov(get_type_for_value(instruction.destination), ASM::Imm(0),
                                                   convert_value(instruction.destination)));
                instructions.emplace_back(ASM::SetCC(op, convert_value(instruction.destination)));
            } else if (is_double && op == ASM::ConditionCode::BE) {
                instructions.emplace_back(ASM::Cmp(get_type_for_value(instruction.left_source),
                                                   convert_value(instruction.left_source),
                                                   convert_value(instruction.right_source)));

                op = ASM::ConditionCode::AE;
                instructions.emplace_back(ASM::Mov(get_type_for_value(instruction.destination), ASM::Imm(0),
                                                   convert_value(instruction.destination)));
                instructions.emplace_back(ASM::SetCC(op, convert_value(instruction.destination)));
            } else if (is_double && op == ASM::ConditionCode::E) {
                // TODO: gcc handles this more efficently
                auto end_label = gen_jump_label("end");
                instructions.emplace_back(ASM::Cmp(get_type_for_value(instruction.left_source),
                                                   convert_value(instruction.right_source),
                                                   convert_value(instruction.left_source)));
                // set to zero if parity is 1
                instructions.emplace_back(ASM::Mov(get_type_for_value(instruction.destination), ASM::Imm(0),
                                                   convert_value(instruction.destination)));
                instructions.emplace_back(ASM::SetCC(ASM::ConditionCode::NP, convert_value(instruction.destination)));
                instructions.emplace_back(ASM::JmpCC(ASM::ConditionCode::P, end_label));
                instructions.emplace_back(ASM::SetCC(ASM::ConditionCode::E, convert_value(instruction.destination)));
                instructions.emplace_back(ASM::Label(end_label));
            } else if (is_double && op == ASM::ConditionCode::NE) {
                auto end_label = gen_jump_label("end");
                instructions.emplace_back(ASM::Cmp(get_type_for_value(instruction.left_source),
                                                   convert_value(instruction.right_source),
                                                   convert_value(instruction.left_source)));
                // set to zero if parity is 1
                instructions.emplace_back(ASM::Mov(get_type_for_value(instruction.destination), ASM::Imm(0),
                                                   convert_value(instruction.destination)));
                instructions.emplace_back(ASM::SetCC(ASM::ConditionCode::P, convert_value(instruction.destination)));
                instructions.emplace_back(ASM::JmpCC(ASM::ConditionCode::P, end_label));
                instructions.emplace_back(ASM::SetCC(ASM::ConditionCode::NE, convert_value(instruction.destination)));
                instructions.emplace_back(ASM::Label(end_label));
            } else {
                instructions.emplace_back(ASM::Cmp(get_type_for_value(instruction.left_source),
                                                   convert_value(instruction.right_source),
                                                   convert_value(instruction.left_source)));
                instructions.emplace_back(ASM::Mov(get_type_for_value(instruction.destination), ASM::Imm(0),
                                                   convert_value(instruction.destination)));
                instructions.emplace_back(ASM::SetCC(op, convert_value(instruction.destination)));
            }
        }

        void convert_jump(const IR::Jump &instruction, std::vector<ASM::Instruction> &instructions) {
            instructions.emplace_back(ASM::Jmp(instruction.target));
        }

        void convert_jump_if_zero(const IR::JumpIfZero &instruction, std::vector<ASM::Instruction> &instructions) {
            if (std::holds_alternative<ASM::Double>(get_type_for_value(instruction.condition))) {
                instructions.emplace_back(ASM::Binary(ASM::Binary::Operator::XOR, ASM::Double(),
                                                      ASM::Reg(ASM::Reg::Name::XMM0), ASM::Reg(ASM::Reg::Name::XMM0)));
                instructions.emplace_back(ASM::Cmp(get_type_for_value(instruction.condition),
                                                   convert_value(instruction.condition),
                                                   ASM::Reg(ASM::Reg::Name::XMM0)));
            } else {
                instructions.emplace_back(ASM::Cmp(get_type_for_value(instruction.condition), ASM::Imm(0),
                                                   convert_value(instruction.condition)));
            }
            instructions.emplace_back(ASM::JmpCC(ASM::ConditionCode::E, instruction.target));
        }

        void convert_jump_if_not_zero(const IR::JumpIfNotZero &instruction,
                                      std::vector<ASM::Instruction> &instructions) {
            if (std::holds_alternative<ASM::Double>(get_type_for_value(instruction.condition))) {
                instructions.emplace_back(ASM::Binary(ASM::Binary::Operator::XOR, ASM::Double(),
                                                      ASM::Reg(ASM::Reg::Name::XMM0), ASM::Reg(ASM::Reg::Name::XMM0)));
                instructions.emplace_back(ASM::Cmp(get_type_for_value(instruction.condition),
                                                   convert_value(instruction.condition),
                                                   ASM::Reg(ASM::Reg::Name::XMM0)));
            } else {
                instructions.emplace_back(ASM::Cmp(get_type_for_value(instruction.condition), ASM::Imm(0),
                                                   convert_value(instruction.condition)));
            }
            instructions.emplace_back(ASM::JmpCC(ASM::ConditionCode::NE, instruction.target));
        }

        void convert_label(const IR::Label &instruction, std::vector<ASM::Instruction> &instructions) {
            instructions.emplace_back(ASM::Label(instruction.name));
        }

        void convert_copy(const IR::Copy &instruction, std::vector<ASM::Instruction> &instructions) {
            instructions.emplace_back(ASM::Mov(get_type_for_value(instruction.destination),
                                               convert_value(instruction.source),
                                               convert_value(instruction.destination)));
        }

        std::string gen_const_label() {
            auto label = "const." + std::to_string(cnt++);
            asmSymbols[label] = ASM::ObjectSymbol(ASM::Double(), true, true);
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
        std::unordered_map<std::string, Symbol> *symbols;
    };

    class ReplacePseudoRegistersPass {
    public:
        explicit ReplacePseudoRegistersPass(ASM::Program *asmProgram,
                                            std::unordered_map<std::string, ASM::Symbol> *
                                            symbols) : asmProgram(asmProgram), symbols(symbols) {
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
                           [this](ASM::Div &div) {
                               replace(div.divisor);
                           },
                           [this](ASM::MovZeroExtend &movz) {
                               replace(movz.destination);
                               replace(movz.source);
                           },
                           [this](ASM::Cvtsi2sd &cvtsi2sd) {
                               replace(cvtsi2sd.destination);
                               replace(cvtsi2sd.source);
                           },
                           [this](ASM::Cvttsd2si &cvttsd2si) {
                               replace(cvttsd2si.destination);
                               replace(cvttsd2si.source);
                           },
                           [this](ASM::Lea &lea) {
                               replace(lea.destination);
                               replace(lea.source);
                           },
                           [this](auto &) {
                           }
                       }, instruction);
        }

        int offset = -4;

    private:
        void replace(ASM::Operand &operand) {
            std::string name;
            int add_offset = 0;
            if (std::holds_alternative<ASM::Pseudo>(operand)) {
                name = std::get<ASM::Pseudo>(operand).name;
            } else if (std::holds_alternative<ASM::PseudoMem>(operand)) {
                name = std::get<ASM::PseudoMem>(operand).identifier;
                add_offset = std::get<ASM::PseudoMem>(operand).offset;
            } else {
                return;
            }

            if (symbols->contains(name) && std::get<ASM::ObjectSymbol>((*symbols)[name]).is_static) {
                operand = ASM::Data(name);
            } else {
                if (m_offsets.contains(name)) {
                    operand = ASM::Memory(ASM::Reg(ASM::Reg::Name::BP), m_offsets[name] + add_offset);
                } else {
                    m_offsets[name] = offset;
                    // why we only round down in case of 8 bytes and above?
                    auto& type = std::get<ASM::ObjectSymbol>((*symbols)[name]).type;
                    if (std::holds_alternative<ASM::Byte>(type)) {
                        offset -= 1;
                    } else if (std::holds_alternative<ASM::LongWord>(type)) {
                        offset -= 4;
                    } else {
                        if (std::holds_alternative<ASM::ByteArray>(type)) {
                            offset -= std::get<ASM::ByteArray>(type).size;
                        } else {
                            offset -= 8;
                        }
                        // is it good alignment always?
                        offset = ((offset - 15) / 16) * 16;
                    }
                    operand = ASM::Memory(ASM::Reg(ASM::Reg::Name::BP), (m_offsets[name] = offset) + add_offset);
                }
            }
        }

        std::unordered_map<std::string, ASM::Symbol> *symbols;
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
            fixed_instructions.emplace_back(ASM::Binary(ASM::Binary::Operator::SUB, ASM::QuadWord(),
                                                        ASM::Imm(rounded_stack_size), ASM::Reg(ASM::Reg::Name::SP)));

            for (auto &instruction: function.instructions) {
                visit_instruction(instruction, fixed_instructions);
            }

            function.instructions = std::move(fixed_instructions);
        }

        void fix_movsx(ASM::Movsx &mov, std::vector<ASM::Instruction> &output) {
            if (std::holds_alternative<ASM::Imm>(mov.source)) {
                auto source = mov.source;
                mov.source = ASM::Reg(ASM::Reg::Name::R10);
                output.emplace_back(ASM::Mov(mov.source_type, source, mov.source));
            }
            if (is_memory_address(mov.destination)) {
                auto destination = mov.destination;
                mov.destination = ASM::Reg(ASM::Reg::Name::R11);
                output.emplace_back(mov);
                output.emplace_back(ASM::Mov(mov.destination_type, mov.destination, destination));
            } else {
                output.emplace_back(mov);
            }
        }

        bool is_xmm_reg(ASM::Reg::Name name) {
            switch (name) {
                case ASM::Reg::Name::XMM0:
                case ASM::Reg::Name::XMM1:
                case ASM::Reg::Name::XMM2:
                case ASM::Reg::Name::XMM3:
                case ASM::Reg::Name::XMM4:
                case ASM::Reg::Name::XMM5:
                case ASM::Reg::Name::XMM6:
                case ASM::Reg::Name::XMM7:
                case ASM::Reg::Name::XMM14:
                case ASM::Reg::Name::XMM15:
                    return true;
                default:
                    return false;
            }
        }

        void fix_push(ASM::Push &push, std::vector<ASM::Instruction> &output) {
            if (should_fix_imm(push.value)) {
                output.emplace_back(ASM::Mov(ASM::QuadWord(), push.value, ASM::Reg(ASM::Reg::Name::R10)));
                push.value = ASM::Reg(ASM::Reg::Name::R10);
            }

            if (std::holds_alternative<ASM::Reg>(push.value) && is_xmm_reg(std::get<ASM::Reg>(push.value).name)) {
                output.emplace_back(ASM::Binary(ASM::Binary::Operator::SUB, ASM::QuadWord(), ASM::Imm(8),
                                                ASM::Reg(ASM::Reg::Name::SP)));
                output.emplace_back(ASM::Mov(ASM::QuadWord(), push.value,
                                             ASM::Memory(ASM::Reg(ASM::Reg::Name::SP), 0)));
            } else {
                output.emplace_back(push);
            }
        }

        void fix_mov_zero_extend(ASM::MovZeroExtend &mov, std::vector<ASM::Instruction> &output) {
            if (std::holds_alternative<ASM::Byte>(mov.source_type)) {
                if (std::holds_alternative<ASM::Imm>(mov.source)) {
                    std::get<ASM::Imm>(mov.source).value &= 0xff; // truncate to 1 byte
                    output.emplace_back(ASM::Mov(ASM::Byte(), mov.source, ASM::Reg(ASM::Reg::Name::R10)));
                    mov.source = ASM::Reg(ASM::Reg::Name::R10);
                }

                if (!std::holds_alternative<ASM::Reg>(mov.destination)) {
                    auto dest = mov.destination;
                    mov.destination = ASM::Reg(ASM::Reg::Name::R11);
                    output.emplace_back(mov);
                    output.emplace_back(ASM::Mov(mov.destination_type, ASM::Reg(ASM::Reg::Name::R11), dest));
                } else {
                    output.emplace_back(mov);
                }
            } else {
                if (std::holds_alternative<ASM::Reg>(mov.destination)) {
                    output.emplace_back(ASM::Mov(ASM::LongWord(), mov.source, mov.destination));
                } else if (is_memory_address(mov.destination)) {
                    output.emplace_back(ASM::Mov(ASM::LongWord(), mov.source, ASM::Reg(ASM::Reg::Name::R11)));
                    output.emplace_back(ASM::Mov(ASM::QuadWord(), ASM::Reg(ASM::Reg::Name::R11), mov.destination));
                }
            }
        }

        void fix_cvtsi2d(ASM::Cvtsi2sd &cvtsi2sd, std::vector<ASM::Instruction> &output) {
            if (std::holds_alternative<ASM::Imm>(cvtsi2sd.source)) {
                output.emplace_back(ASM::Mov(cvtsi2sd.type, cvtsi2sd.source, ASM::Reg(ASM::Reg::Name::R10)));
                cvtsi2sd.source = ASM::Reg(ASM::Reg::Name::R10);
            }

            if (!std::holds_alternative<ASM::Reg>(cvtsi2sd.destination)) {
                auto destination = cvtsi2sd.destination;
                cvtsi2sd.destination = ASM::Reg(ASM::Reg::Name::XMM15);
                output.emplace_back(cvtsi2sd);
                output.emplace_back(ASM::Mov(ASM::Double(), ASM::Reg(ASM::Reg::Name::XMM15), destination));
            } else {
                output.emplace_back(cvtsi2sd);
            }
        }

        void fix_cvttsd2si(ASM::Cvttsd2si &cvttsd2si, std::vector<ASM::Instruction> &output) {
            if (!std::holds_alternative<ASM::Reg>(cvttsd2si.destination)) {
                auto destination = cvttsd2si.destination;
                cvttsd2si.destination = ASM::Reg(ASM::Reg::Name::R11);
                output.emplace_back(cvttsd2si);
                output.emplace_back(ASM::Mov(cvttsd2si.type, ASM::Reg(ASM::Reg::Name::R11), destination));
            } else {
                output.emplace_back(cvttsd2si);
            }
        }

        void fix_lea(ASM::Lea &lea, std::vector<ASM::Instruction> &output) {
            if (!std::holds_alternative<ASM::Reg>(lea.destination)) {
                auto destination = lea.destination;
                lea.destination = ASM::Reg(ASM::Reg::Name::R11);
                output.emplace_back(lea);
                output.emplace_back(ASM::Mov(ASM::QuadWord(), ASM::Reg(ASM::Reg::Name::R11), destination));
            } else {
                output.emplace_back(lea);
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
                           [this, &output](ASM::Push &push) {
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
                           [this, &output](ASM::Cvttsd2si &cvttsd2si) {
                               fix_cvttsd2si(cvttsd2si, output);
                           },
                           [this, &output](ASM::Lea &lea) {
                               fix_lea(lea, output);
                           },
                           [&output](auto &instruction) {
                               output.emplace_back(instruction);
                           },
                       }, instruction);
        }

        bool is_memory_address(const ASM::Operand &operand) {
            return std::holds_alternative<ASM::Memory>(operand) || std::holds_alternative<ASM::Data>(operand) || std::holds_alternative<ASM::Indexed>(operand);
        }

        void fix_mov(ASM::Mov &mov, std::vector<ASM::Instruction> &output) {
            if (is_memory_address(mov.src) && is_memory_address(mov.dst)) {
                if (std::holds_alternative<ASM::Double>(mov.type)) {
                    output.emplace_back(ASM::Mov{mov.type, mov.src, ASM::Reg(ASM::Reg::Name::XMM14)});
                    mov.src = ASM::Reg(ASM::Reg::Name::XMM14);
                } else {
                    output.emplace_back(ASM::Mov{mov.type, mov.src, ASM::Reg(ASM::Reg::Name::R10)});
                    mov.src = ASM::Reg(ASM::Reg::Name::R10);
                }
            }
            if (should_fix_imm(mov.src)) {
                if (std::holds_alternative<ASM::LongWord>(mov.type)) {
                    std::get<ASM::Imm>(mov.src).value &= 0xffffffff; // truncate to 32-bit
                } else if (is_memory_address(mov.dst)) {
                    output.emplace_back(ASM::Mov{mov.type, mov.src, ASM::Reg(ASM::Reg::Name::R10)});
                    mov.src = ASM::Reg(ASM::Reg::Name::R10);
                }
            }
            output.emplace_back(mov);
        }

        bool should_fix_imm(ASM::Operand &operand) {
            return std::holds_alternative<ASM::Imm>(operand) && std::get<ASM::Imm>(operand).value > std::numeric_limits<
                       int>::max();
        }

        void fix_binary(ASM::Binary &binary, std::vector<ASM::Instruction> &output) {
            // mess!!!!
            // does this even work!
            if (std::holds_alternative<ASM::Double>(binary.type)) {
                if (binary.op == ASM::Binary::Operator::ADD || binary.op == ASM::Binary::Operator::SUB || binary.op ==
                    ASM::Binary::Operator::MULT || binary.op == ASM::Binary::Operator::DIV_DOUBLE || binary.op ==
                    ASM::Binary::Operator::XOR) {
                    if (!std::holds_alternative<ASM::Reg>(binary.right)) {
                        auto destination = binary.right;
                        binary.right = ASM::Reg(ASM::Reg::Name::XMM15);
                        output.emplace_back(ASM::Mov(ASM::Double(), destination, ASM::Reg(ASM::Reg::Name::XMM15)));
                        output.emplace_back(binary);
                        output.emplace_back(ASM::Mov(ASM::Double(), ASM::Reg(ASM::Reg::Name::XMM15), destination));
                    } else {
                        output.emplace_back(binary);
                    }
                } else {
                    std::abort();
                }
            } else {
                if ((binary.op == ASM::Binary::Operator::SHR || binary.op == ASM::Binary::Operator::SHL || binary.op ==
                     ASM::Binary::Operator::SAR) &&
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

        void fix_div(ASM::Div &div, std::vector<ASM::Instruction> &output) {
            if (std::holds_alternative<ASM::Imm>(div.divisor)) {
                output.emplace_back(ASM::Mov{div.type, div.divisor, ASM::Reg(ASM::Reg::Name::R10)});
                div.divisor = ASM::Reg(ASM::Reg::Name::R10);
            }
            output.emplace_back(div);
        }

        void fix_cmp(ASM::Cmp &cmp, std::vector<ASM::Instruction> &output) {
            if (std::holds_alternative<ASM::Double>(cmp.type)) {
                if (!std::holds_alternative<ASM::Reg>(cmp.right)) {
                    auto destination = cmp.right;
                    cmp.right = ASM::Reg(ASM::Reg::Name::XMM15);
                    output.emplace_back(ASM::Mov(ASM::Double(), destination, ASM::Reg(ASM::Reg::Name::XMM15)));
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
