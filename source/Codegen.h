#ifndef CODEGEN_H
#define CODEGEN_H
#include <unordered_map>
#include <algorithm>
#include <limits>

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
                [](const auto&) -> ASM::StaticVariable {
                    std::unreachable();
                }
            }, *variable.type);
        }


        ASM::Function convert_function(const IR::Function &function) {
            std::vector<ASM::Instruction> instructions;
            static std::vector param_registers = {
                ASM::Reg::Name::DI, ASM::Reg::Name::SI, ASM::Reg::Name::DX, ASM::Reg::Name::CX, ASM::Reg::Name::R8,
                ASM::Reg::Name::R9
            };
            for (int i = 0; i < std::min(function.params.size(), param_registers.size()); ++i) {
                instructions.emplace_back(ASM::Mov(get_type_for_identifier(function.params[i]), ASM::Reg(param_registers[i]), ASM::Pseudo(function.params[i])));
            }

            for (int i = param_registers.size(); i < function.params.size(); ++i) {
                instructions.emplace_back(ASM::Mov(get_type_for_identifier(function.params[i]),ASM::Stack(16 + (i - param_registers.size()) * 8),
                                                   ASM::Pseudo(function.params[i])));
            }

            for (auto &instruction: function.instructions) {
                convert_instruction(instruction, instructions);
            }
            return ASM::Function(function.name, function.global, std::move(instructions));
        }

        void convert_call(const IR::Call &call, std::vector<ASM::Instruction> &instructions) {
            static std::vector arg_registers = {
                ASM::Reg::Name::DI, ASM::Reg::Name::SI, ASM::Reg::Name::DX, ASM::Reg::Name::CX, ASM::Reg::Name::R8,
                ASM::Reg::Name::R9
            };

            int stack_padding = call.arguments.size() % 2 == 0 ? 0 : 8;
            if (stack_padding != 0) {
                instructions.emplace_back(ASM::Binary(ASM::Binary::Operator::SUB, ASM::Type::QuadWord, ASM::Imm(stack_padding), ASM::Reg(ASM::Reg::Name::SP)));
            }

            // register passed arguments
            for (int i = 0; i < std::min(arg_registers.size(), call.arguments.size()); i++) {
                instructions.emplace_back(ASM::Mov(get_type_for_value(call.arguments[i]),convert_value(call.arguments[i]), ASM::Reg(arg_registers[i])));
            }

            // stack passed arguments
            for (int i = call.arguments.size() - 1; i >= static_cast<int>(arg_registers.size()); --i) {
                auto arg = convert_value(call.arguments[i]);
                if (std::holds_alternative<ASM::Imm>(arg) || std::holds_alternative<ASM::Reg>(arg) || get_type_for_value(call.arguments[i]) == ASM::Type::LongWord) {
                    instructions.emplace_back(ASM::Push(arg));
                } else {
                    instructions.emplace_back(ASM::Mov(get_type_for_value(call.arguments[i]), arg, ASM::Reg(ASM::Reg::Name::AX)));
                    instructions.emplace_back(ASM::Push(ASM::Reg(ASM::Reg::Name::AX)));
                }
            }

            instructions.emplace_back(ASM::Call(call.name));

            int bytes_to_remove = 8 * std::max(
                                      0, static_cast<int>(call.arguments.size()) - static_cast<int>(arg_registers.
                                             size())) + stack_padding;

            if (bytes_to_remove != 0) {
                instructions.emplace_back(ASM::Binary(ASM::Binary::Operator::ADD, ASM::Type::QuadWord, ASM::Imm(bytes_to_remove), ASM::Reg(ASM::Reg::Name::SP)));
            }

            instructions.emplace_back(ASM::Mov(get_type_for_value(call.destination), ASM::Reg(ASM::Reg::Name::AX), convert_value(call.destination)));
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
                }
                       }, instruction);
        }

        void convert_return(const IR::Return &instruction, std::vector<ASM::Instruction> &instructions) {
            instructions.emplace_back(ASM::Mov(get_type_for_value(instruction.value), convert_value(instruction.value), ASM::Reg(ASM::Reg::Name::AX)));
            instructions.emplace_back(ASM::Ret());
        }

        ASM::Operand convert_value(const IR::Value &value) {
            return std::visit(overloaded{
                                  [](const IR::Constant &constant) -> ASM::Operand {
                                      return std::visit(overloaded {
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
                instructions.emplace_back(ASM::Cmp(get_type_for_value(instruction.source), ASM::Imm(0), convert_value(instruction.source)));
                instructions.emplace_back(ASM::Mov(get_type_for_value(instruction.destination), ASM::Imm(0), convert_value(instruction.destination)));
                instructions.emplace_back(ASM::SetCC(ASM::ConditionCode::E, convert_value(instruction.destination)));
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

            if (instruction.op == IR::Binary::Operator::DIVIDE || instruction.op == IR::Binary::Operator::REMAINDER) {
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
            bool op_unsigned = is_unsigned(instruction.left_source);
             auto convert_op = [op_unsigned](const IR::Binary::Operator &op) {
                switch (op) {
                    case IR::Binary::Operator::EQUAL:
                        return ASM::ConditionCode::E;
                    case IR::Binary::Operator::GREATER:
                        return op_unsigned ? ASM::ConditionCode::A : ASM::ConditionCode::G;
                    case IR::Binary::Operator::GREATER_EQUAL:
                        return op_unsigned ? ASM::ConditionCode::AE : ASM::ConditionCode::GE;
                    case IR::Binary::Operator::LESS:
                        return op_unsigned ? ASM::ConditionCode::B : ASM::ConditionCode::L;
                    case IR::Binary::Operator::LESS_EQUAL:
                        return op_unsigned ? ASM::ConditionCode::BE : ASM::ConditionCode::LE;
                    case IR::Binary::Operator::NOT_EQUAL:
                        return ASM::ConditionCode::NE;
                    default:
                        std::unreachable();
                }
            };

            instructions.emplace_back(ASM::Cmp(get_type_for_value(instruction.left_source), convert_value(instruction.right_source),
                                               convert_value(instruction.left_source)));
            instructions.emplace_back(ASM::Mov(get_type_for_value(instruction.destination), ASM::Imm(0), convert_value(instruction.destination)));
            instructions.emplace_back(ASM::SetCC(convert_op(instruction.op), convert_value(instruction.destination)));
        }

        void convert_jump(const IR::Jump &instruction, std::vector<ASM::Instruction> &instructions) {
            instructions.emplace_back(ASM::Jmp(instruction.target));
        }

        void convert_jump_if_zero(const IR::JumpIfZero &instruction, std::vector<ASM::Instruction> &instructions) {
            instructions.emplace_back(ASM::Cmp(get_type_for_value(instruction.condition), ASM::Imm(0), convert_value(instruction.condition)));
            instructions.emplace_back(ASM::JmpCC(ASM::ConditionCode::E, instruction.target));
        }

        void convert_jump_if_not_zero(const IR::JumpIfNotZero &instruction,
                                      std::vector<ASM::Instruction> &instructions) {
            instructions.emplace_back(ASM::Cmp(get_type_for_value(instruction.condition), ASM::Imm(0), convert_value(instruction.condition)));
            instructions.emplace_back(ASM::JmpCC(ASM::ConditionCode::NE, instruction.target));
        }

        void convert_label(const IR::Label &instruction, std::vector<ASM::Instruction> &instructions) {
            instructions.emplace_back(ASM::Label(instruction.name));
        }

        void convert_copy(const IR::Copy &instruction, std::vector<ASM::Instruction> &instructions) {
            instructions.emplace_back(ASM::Mov(get_type_for_value(instruction.destination),convert_value(instruction.source),
                                               convert_value(instruction.destination)));
        }

        ASM::Program asmProgram;
        std::unordered_map<std::string, ASM::Symbol> asmSymbols;

    private:
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
                output.emplace_back(ASM::Mov{mov.type, mov.src, ASM::Reg(ASM::Reg::Name::R10)});
                mov.src = ASM::Reg(ASM::Reg::Name::R10);
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

    private:
        int m_max_offset;
        ASM::Program *asmProgram;
    };
}


#endif //CODEGEN_H
