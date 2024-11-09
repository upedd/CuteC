#ifndef CODEEMITTER_H
#define CODEEMITTER_H
#include <format>

#include "AsmTree.h"
#include "overloaded.h"

// asm tree -> asm file

class CodeEmitter {
public:
    CodeEmitter(ASM::Program program) : asmProgram(std::move(program)) {}

    void emit() {
        emit_program(asmProgram);
    }

    void emit_program(const ASM::Program& program) {
        emit_function(program.function);
#if __linux__
        // disable executable stack
        assembly += "   .section .note.GNU-stack,"",@progbits";
#endif
    }

    void emit_function(const ASM::Function& function) {
        // functions names on apple must be prefixed by an underscore
#if __APPLE__
        assembly += "    .global _" + function.name + "\n";
        assembly += "_" + function.name + ":\n";
#elif
        assembly += "    .global " + function.name + "\n";
        assembly += function.name + ":\n";
#endif
        for (const auto& instruction : function.instructions) {
            emit_instruction(instruction);
        }
    }



    void emit_instruction(const ASM::Instruction& instruction) {
        std::visit(overloaded {
            [this](const ASM::Mov& ins) {
                emit_mov(ins);
            },
            [this](const ASM::Ret& ins) {
                emit_ret(ins);
            }
        }, instruction);
    }

    void emit_mov(const ASM::Mov& ins) {
        assembly += "    movl ";
        emit_operand(ins.src);
        assembly += ", ";
        emit_operand(ins.dst);
        assembly += "\n";
    }

    void emit_ret(const ASM::Ret& ins) {
        assembly += "    ret\n";
    }

    void emit_operand(const ASM::Operand& operand) {
        std::visit(overloaded {
            [this](const ASM::Imm& operand) {
                emit_imm(operand);
            },
            [this](const ASM::Register& operand) {
                emit_register(operand);
            }
        }, operand);
    }

    void emit_imm(const ASM::Imm& operand) {
        assembly += std::format("${}", operand.value);
    }

    void emit_register(const ASM::Register& operand) {
        assembly += "%eax";
    }

    std::string assembly;
private:
    ASM::Program asmProgram;
};

#endif //CODEEMITTER_H
