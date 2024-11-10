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
            function.instructions.insert(function.instructions.begin(), ASM::AllocateStack(m_max_offset));
            // TODO: temp

            int idx = 1;
            for (auto &instruction: function.instructions) {
                visit_instruction(instruction, function.instructions, idx);
                idx++;
            }
        }

        void visit_instruction(ASM::Instruction &instruction, std::vector<ASM::Instruction> &instructions, int idx) {
            std::visit(overloaded{
                           [this, &instructions, idx](ASM::Mov &mov) {
                               fix_mov(mov, instructions, idx);
                           },
                           [](auto &) {
                           }
                       }, instruction);
        }

        void fix_mov(ASM::Mov &mov, std::vector<ASM::Instruction> &instructions, int idx) {
            if (std::holds_alternative<ASM::Stack>(mov.src) && std::holds_alternative<ASM::Stack>(mov.dst)) {
                auto src = mov.src;
                mov.src = ASM::Reg(ASM::Reg::Name::R10);
                instructions.insert(instructions.begin() + idx - 1, ASM::Mov{src, ASM::Reg(ASM::Reg::Name::R10)});
            }
        }

    private:
        int m_max_offset;
        ASM::Program *asmProgram;
    };
}


#endif //CODEGEN_H
