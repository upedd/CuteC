#ifndef CODEGEN_H
#define CODEGEN_H
#include <unordered_map>
#include <algorithm>

#include "AsmTree.h"
#include "Ast.h"
#include "IR.h"
#include "overloaded.h"

namespace codegen {
    class IRToAsmTreePass {
    public:
        IRToAsmTreePass(IR::Program IRProgram) : IRProgram(std::move(IRProgram)) {
        }

        void convert() {
            asmProgram = ASM::Program(convert_function(IRProgram.function));
        }

        ASM::Function convert_function(const IR::Function &function) {
            std::vector<ASM::Instruction> instructions;
            for (auto &instruction: function.instructions) {
                convert_instruction(instruction, instructions);
            }
            return ASM::Function(function.name, std::move(instructions));
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
                           }
                       }, instruction);
        }

        void convert_return(const IR::Return &instruction, std::vector<ASM::Instruction> &instructions) {
            instructions.emplace_back(ASM::Mov(convert_value(instruction.value), ASM::Reg(ASM::Reg::Name::AX)));
            instructions.emplace_back(ASM::Ret());
        }

        ASM::Operand convert_value(const IR::Value &value) {
            return std::visit(overloaded{
                                  [](const IR::Constant &constant) -> ASM::Operand {
                                      return ASM::Imm(constant.value);
                                  },
                                  [](const IR::Variable &variable) -> ASM::Operand {
                                      return ASM::Pseudo(variable.name);
                                  },

                              }, value);
        }

        void convert_unary(const IR::Unary &instruction, std::vector<ASM::Instruction> &instructions) {
            if (instruction.op == IR::Unary::Operator::LOGICAL_NOT) {
                instructions.emplace_back(ASM::Cmp(ASM::Imm(0), convert_value(instruction.source)));
                instructions.emplace_back(ASM::Mov(ASM::Imm(0), convert_value(instruction.destination)));
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

            instructions.emplace_back(ASM::Mov(convert_value(instruction.source),
                                               convert_value(instruction.destination)));
            instructions.emplace_back(ASM::Unary(convert_op(instruction.op), convert_value(instruction.destination)));
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
                    ASM::Mov(convert_value(instruction.left_source), ASM::Reg(ASM::Reg::Name::AX)));
                instructions.emplace_back(ASM::Cdq());
                instructions.emplace_back(ASM::Idiv(convert_value(instruction.right_source)));
                instructions.emplace_back(ASM::Mov(
                    ASM::Reg(instruction.op == IR::Binary::Operator::DIVIDE ? ASM::Reg::Name::AX : ASM::Reg::Name::DX),
                    convert_value(instruction.destination)));
            } else {
                static auto convert_op = [](const IR::Binary::Operator &op) {
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
                            return ASM::Binary::Operator::SHR;
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

                instructions.emplace_back(ASM::Mov(convert_value(instruction.left_source),
                                                   convert_value(instruction.destination)));
                instructions.emplace_back(ASM::Binary(convert_op(instruction.op),
                                                      convert_value(instruction.right_source),
                                                      convert_value(instruction.destination)));
            }
        }

        void convert_relational(const IR::Binary &instruction, std::vector<ASM::Instruction> &instructions) {
            static auto convert_op = [](const IR::Binary::Operator &op) {
                switch (op) {
                    case IR::Binary::Operator::EQUAL:
                        return ASM::ConditionCode::E;
                    case IR::Binary::Operator::GREATER:
                        return ASM::ConditionCode::G;
                    case IR::Binary::Operator::GREATER_EQUAL:
                        return ASM::ConditionCode::GE;
                    case IR::Binary::Operator::LESS:
                        return ASM::ConditionCode::L;
                    case IR::Binary::Operator::LESS_EQUAL:
                        return ASM::ConditionCode::LE;
                    case IR::Binary::Operator::NOT_EQUAL:
                        return ASM::ConditionCode::NE;
                    default:
                        std::unreachable();
                }
            };

            instructions.emplace_back(ASM::Cmp(convert_value(instruction.right_source),
                                               convert_value(instruction.left_source)));
            instructions.emplace_back(ASM::Mov(ASM::Imm(0), convert_value(instruction.destination)));
            instructions.emplace_back(ASM::SetCC(convert_op(instruction.op), convert_value(instruction.destination)));
        }

        void convert_jump(const IR::Jump &instruction, std::vector<ASM::Instruction> &instructions) {
            instructions.emplace_back(ASM::Jmp(instruction.target));
        }

        void convert_jump_if_zero(const IR::JumpIfZero &instruction, std::vector<ASM::Instruction> &instructions) {
            instructions.emplace_back(ASM::Cmp(ASM::Imm(0), convert_value(instruction.condition)));
            instructions.emplace_back(ASM::JmpCC(ASM::ConditionCode::E, instruction.target));
        }

        void convert_jump_if_not_zero(const IR::JumpIfNotZero &instruction,
                                      std::vector<ASM::Instruction> &instructions) {
            instructions.emplace_back(ASM::Cmp(ASM::Imm(0), convert_value(instruction.condition)));
            instructions.emplace_back(ASM::JmpCC(ASM::ConditionCode::NE, instruction.target));
        }

        void convert_label(const IR::Label &instruction, std::vector<ASM::Instruction> &instructions) {
            instructions.emplace_back(ASM::Label(instruction.name));
        }

        void convert_copy(const IR::Copy &instruction, std::vector<ASM::Instruction> &instructions) {
            instructions.emplace_back(ASM::Mov(convert_value(instruction.source),
                                               convert_value(instruction.destination)));
        }

        ASM::Program asmProgram;

    private:
        IR::Program IRProgram;
    };

    class ReplacePseudoRegistersPass {
    public:
        explicit ReplacePseudoRegistersPass(ASM::Program *asmProgram) : asmProgram(asmProgram) {
        }

        void process() {
            visit_function(asmProgram->function);
        }

        void visit_function(ASM::Function &function) {
            for (auto &instruction: function.instructions) {
                visit_instruction(instruction);
            }
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
                           [this](auto &) {
                           }
                       }, instruction);
        }

        int offset = 4;

    private:
        void replace(ASM::Operand &operand) {
            if (!std::holds_alternative<ASM::Pseudo>(operand)) {
                return;
            }
            auto pseudo = std::get<ASM::Pseudo>(operand);
            if (m_offsets.contains(pseudo.name)) {
                operand = ASM::Stack{-m_offsets[pseudo.name]};
            } else {
                m_offsets[pseudo.name] = offset;
                offset += 4;
                operand = ASM::Stack{-m_offsets[pseudo.name]};
            }
        }

        std::unordered_map<std::string, int> m_offsets;
        ASM::Program *asmProgram;
    };

    class FixUpInstructionsPass {
    public:
        explicit FixUpInstructionsPass(ASM::Program *asmProgram, int max_offset) : asmProgram(asmProgram),
            m_max_offset(max_offset) {
        }

        void process() {
            visit_function(asmProgram->function);
        }

        void visit_function(ASM::Function &function) {
            std::vector<ASM::Instruction> fixed_instructions;
            fixed_instructions.emplace_back(ASM::AllocateStack(m_max_offset));

            for (auto &instruction: function.instructions) {
                visit_instruction(instruction, fixed_instructions);
            }

            function.instructions = std::move(fixed_instructions);
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
                           [&output](auto &instruction) {
                               output.emplace_back(instruction);
                           },
                           [this, &output](ASM::Cmp &cmp) {
                               fix_cmp(cmp, output);
                           }
                       }, instruction);
        }

        void fix_mov(ASM::Mov &mov, std::vector<ASM::Instruction> &output) {
            if (std::holds_alternative<ASM::Stack>(mov.src) && std::holds_alternative<ASM::Stack>(mov.dst)) {
                output.emplace_back(ASM::Mov{mov.src, ASM::Reg(ASM::Reg::Name::R10)});
                mov.src = ASM::Reg(ASM::Reg::Name::R10);
            }
            output.emplace_back(mov);
        }

        void fix_binary(ASM::Binary &binary, std::vector<ASM::Instruction> &output) {
            if ((binary.op == ASM::Binary::Operator::SHR || binary.op == ASM::Binary::Operator::SHL) &&
                std::holds_alternative<ASM::Stack>(binary.right)) {
                output.emplace_back(ASM::Mov{binary.left, ASM::Reg(ASM::Reg::Name::CX)});
                binary.left = ASM::Reg(ASM::Reg::Name::CX);
                output.emplace_back(binary);
            } else if (binary.op == ASM::Binary::Operator::MULT && std::holds_alternative<ASM::Stack>(binary.right)) {
                output.emplace_back(ASM::Mov(binary.right, ASM::Reg(ASM::Reg::Name::R11)));
                output.emplace_back(
                    ASM::Binary(binary.op, binary.left, ASM::Reg(ASM::Reg::Name::R11)));
                output.emplace_back(ASM::Mov{ASM::Reg(ASM::Reg::Name::R11), binary.right});
            } else if (std::holds_alternative<ASM::Stack>(binary.left) && std::holds_alternative<ASM::Stack>(
                           binary.right)) {
                output.emplace_back(ASM::Mov{binary.left, ASM::Reg(ASM::Reg::Name::R10)});
                binary.left = ASM::Reg(ASM::Reg::Name::R10);
                output.emplace_back(binary);
            } else {
                output.emplace_back(binary);
            }
        }

        void fix_idiv(ASM::Idiv &idiv, std::vector<ASM::Instruction> &output) {
            if (std::holds_alternative<ASM::Imm>(idiv.divisor)) {
                output.emplace_back(ASM::Mov{idiv.divisor, ASM::Reg(ASM::Reg::Name::R10)});
                idiv.divisor = ASM::Reg(ASM::Reg::Name::R10);
            }
            output.emplace_back(idiv);
        }

        void fix_cmp(ASM::Cmp &cmp, std::vector<ASM::Instruction> &output) {
            if (std::holds_alternative<ASM::Stack>(cmp.left) && std::holds_alternative<ASM::Stack>(cmp.right)) {
                output.emplace_back(ASM::Mov{cmp.left, ASM::Reg(ASM::Reg::Name::R10)});
                cmp.left = ASM::Reg(ASM::Reg::Name::R10);
            }

            if (std::holds_alternative<ASM::Imm>(cmp.right)) {
                output.emplace_back(ASM::Mov{cmp.right, ASM::Reg(ASM::Reg::Name::R11)});
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
