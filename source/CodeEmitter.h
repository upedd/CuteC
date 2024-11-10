#ifndef CODEEMITTER_H
#define CODEEMITTER_H
#include <format>

#include "AsmTree.h"
#include "overloaded.h"

// asm tree -> asm file

class CodeEmitter {
public:
    CodeEmitter(ASM::Program program) : asmProgram(std::move(program)) {
    }

    void emit() {
        emit_program(asmProgram);
    }

    void emit_program(const ASM::Program &program) {
        emit_function(program.function);
#if __linux__
        // disable executable stack
        assembly += "   .section .note.GNU-stack,"",@progbits";
#endif
    }

    void emit_function(const ASM::Function &function) {
        // functions names on apple must be prefixed by an underscore
#if __APPLE__
        assembly += "    .global _" + function.name + "\n";
        assembly += "_" + function.name + ":\n";
#elif
        assembly += "    .global " + function.name + "\n";
        assembly += function.name + ":\n";
#endif
        assembly += "    pushq %rbp\n";
        assembly += "    movq %rsp, %rbp\n";
        for (const auto &instruction: function.instructions) {
            emit_instruction(instruction);
        }
    }


    void emit_instruction(const ASM::Instruction &instruction) {
        std::visit(overloaded{
                       [this](const ASM::Mov &ins) {
                           emit_mov(ins);
                       },
                       [this](const ASM::Ret &ins) {
                           emit_ret(ins);
                       },
                       [this](const ASM::Unary &ins) {
                           emit_unary(ins);
                       },
                       [this](const ASM::AllocateStack &ins) {
                           emit_allocate_stack(ins);
                       }
                   }, instruction);
    }

    void emit_mov(const ASM::Mov &ins) {
        assembly += "    movl ";
        emit_operand(ins.src);
        assembly += ", ";
        emit_operand(ins.dst);
        assembly += "\n";
    }

    void emit_ret(const ASM::Ret &ins) {
        assembly += "    movq %rbp, %rsp\n";
        assembly += "    popq %rbp\n";
        assembly += "    ret\n";
    }

    void emit_unary(const ASM::Unary &ins) {
        switch (ins.op) {
            case ASM::Unary::Operator::Not:
                assembly += "    notl ";
                break;
            case ASM::Unary::Operator::Neg:
                assembly += "    negl ";
                break;
        }
        emit_operand(ins.operand);
        assembly += "\n";
    }

    void emit_allocate_stack(const ASM::AllocateStack &ins) {
        assembly += std::format("    subq ${}, %rsp\n", ins.size);
    }

    void emit_operand(const ASM::Operand &operand) {
        std::visit(overloaded{
                       [this](const ASM::Imm &operand) {
                           emit_imm(operand);
                       },
                       [this](const ASM::Reg &operand) {
                           emit_register(operand);
                       },
                       [this](const ASM::Stack &operand) {
                           emit_stack(operand);
                       },
                       [this](auto &) {
                           // TODO: panic
                       }
                   }, operand);
    }

    void emit_imm(const ASM::Imm &operand) {
        assembly += std::format("${}", operand.value);
    }

    void emit_register(const ASM::Reg &operand) {
        switch (operand.name) {
            case ASM::Reg::Name::AX:
                assembly += "%eax";
                break;
            case ASM::Reg::Name::R10:
                assembly += "%r10d";
        }
    }

    void emit_stack(const ASM::Stack &operand) {
        assembly += std::format("{}(%rbp)", operand.offset);
    }

    std::string assembly;

private:
    ASM::Program asmProgram;
};

#endif //CODEEMITTER_H
