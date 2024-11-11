#ifndef CODEEMITTER_H
#define CODEEMITTER_H
#include <format>

#include "AsmTree.h"
#include "Codegen.h"
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
                       },
                       [this](const ASM::Binary &ins) {
                           emit_binary(ins);
                       },
                       [this](const ASM::Idiv &ins) {
                           emit_idiv(ins);
                       },
                       [this](const ASM::Cdq &ins) {
                           emit_cdq(ins);
                       },
                       [this](const ASM::Cmp &ins) {
                           emit_cmp(ins);
                       },
                       [this](const ASM::Jmp &ins) {
                           emit_jmp(ins);
                       },
                       [this](const ASM::JmpCC &ins) {
                           emit_jmpcc(ins);
                       },
                       [this](const ASM::SetCC &ins) {
                           emit_setcc(ins);
                       },
                       [this](const ASM::Label &ins) {
                           emit_label(ins);
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

    void emit_binary(const ASM::Binary &ins) {
        switch (ins.op) {
            case ASM::Binary::Operator::ADD:
                assembly += "    addl ";
                break;
            case ASM::Binary::Operator::SUB:
                assembly += "    subl ";
                break;
            case ASM::Binary::Operator::MULT:
                assembly += "    imull ";
                break;
            case ASM::Binary::Operator::SHL:
                assembly += "    shll ";
                break;
            case ASM::Binary::Operator::SHR:
                assembly += "    sarl ";
                break;
            case ASM::Binary::Operator::AND:
                assembly += "    andl ";
                break;
            case ASM::Binary::Operator::XOR:
                assembly += "    xorl ";
                break;
            case ASM::Binary::Operator::OR:
                assembly += "    orl ";
                break;
        }
        if (ins.op == ASM::Binary::Operator::SHR || ins.op == ASM::Binary::Operator::SHL) {
            emit_operand(ins.left, true);
        } else {
            emit_operand(ins.left, false);
        }
        assembly += ", ";
        emit_operand(ins.right);
        assembly += "\n";
    }

    void emit_idiv(const ASM::Idiv &ins) {
        assembly += "    idivl ";
        emit_operand(ins.divisor);
        assembly += "\n";
    }

    void emit_cdq(const ASM::Cdq &) {
        assembly += "    cdq\n";
    }

    void emit_cmp(const ASM::Cmp &ins) {
        assembly += "    cmpl ";
        emit_operand(ins.left);
        assembly += ", ";
        emit_operand(ins.right);
        assembly += "\n";
    }


    void emit_condition_code(const ASM::ConditionCode &code) {
        switch (code) {
            case ASM::ConditionCode::E:
                assembly += "e";
                break;
            case ASM::ConditionCode::NE:
                assembly += "ne";
                break;
            case ASM::ConditionCode::L:
                assembly += "l";
                break;
            case ASM::ConditionCode::GE:
                assembly += "ge";
                break;
            case ASM::ConditionCode::LE:
                assembly += "le";
                break;
            case ASM::ConditionCode::G:
                assembly += "g";
                break;
        }
    }

    void emit_jmp(const ASM::Jmp &ins) {
        assembly += "    jmp ";
        emit_label(ins.target);
        assembly += "\n";
    }

    void emit_jmpcc(const ASM::JmpCC &ins) {
        assembly += "    j";
        emit_condition_code(ins.cond_code);
        assembly += " ";
        emit_label(ins.target);
        assembly += "\n";
    }

    void emit_setcc(const ASM::SetCC &ins) {
        assembly += "    set";
        emit_condition_code(ins.cond_code);
        assembly += " ";
        emit_operand(ins.destination, true);
        assembly += "\n";
    }

    void emit_label(const ASM::Label &ins) {
        emit_label(ins.name);
        assembly += ":\n";
    }

    void emit_label(const std::string &name) {
#ifdef __APPLE__
        assembly += "L" + name;
#else
        assembly += ".L" + name;
#endif
    }

    void emit_operand(const ASM::Operand &operand, bool emit_1byte_registers = false) {
        std::visit(overloaded{
                       [this](const ASM::Imm &operand) {
                           emit_imm(operand);
                       },
                       [this, emit_1byte_registers](const ASM::Reg &operand) {
                           if (emit_1byte_registers) {
                               emit_1byte_register(operand);
                           } else {
                               emit_4byte_register(operand);
                           }
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

    void emit_4byte_register(const ASM::Reg &operand) {
        switch (operand.name) {
            case ASM::Reg::Name::AX:
                assembly += "%eax";
                break;
            case ASM::Reg::Name::DX:
                assembly += "%edx";
                break;
            case ASM::Reg::Name::R10:
                assembly += "%r10d";
                break;
            case ASM::Reg::Name::R11:
                assembly += "%r11d";
                break;
            case ASM::Reg::Name::CX:
                assembly += "%ecx";
                break;
        }
    }

    void emit_1byte_register(const ASM::Reg &operand) {
        switch (operand.name) {
            case ASM::Reg::Name::AX:
                assembly += "%al";
                break;
            case ASM::Reg::Name::DX:
                assembly += "%dl";
                break;
            case ASM::Reg::Name::R10:
                assembly += "%r10b";
                break;
            case ASM::Reg::Name::R11:
                assembly += "%r11b";
                break;
            case ASM::Reg::Name::CX:
                assembly += "%cl";
                break;
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
