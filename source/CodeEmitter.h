#ifndef CODEEMITTER_H
#define CODEEMITTER_H
#include <format>

#include "AsmTree.h"
#include "Codegen.h"
#include "overloaded.h"
#include "analysis/TypeCheckerPass.h"

// asm tree -> asm file


class CodeEmitter {
public:
    CodeEmitter(ASM::Program program,
                std::unordered_map<std::string, Symbol> *symbols) : asmProgram(std::move(program)), symbols(symbols) {
    }

    void emit() {
        emit_program(asmProgram);
    }


    void emit_program(const ASM::Program &program) {
        for (const auto &item: program.items) {
            std::visit(overloaded {
                [this](const ASM::Function& item) {
                    emit_function(item);
                },
                [this](const ASM::StaticVariable& item) {
                    emit_static_variable(item);
                }
            }, item);
        }
#if __linux__
        // disable executable stack
        assembly += ".section .note.GNU-stack,\"\",@progbits\n";
#endif
    }

    void emit_function(const ASM::Function &function) {
        // functions names on apple must be prefixed by an underscore
        if (function.global) {
#if __APPLE__
            assembly += "    .globl _" + function.name + "\n";
#else
            assembly += "    .globl " + function.name + "\n";
#endif
        }
#if __APPLE__
        assembly += "_" + function.name + ":\n";
#else
        assembly += function.name + ":\n";
#endif
        assembly += "    .text\n";
        assembly += "    pushq %rbp\n";
        assembly += "    movq %rsp, %rbp\n";
        for (const auto &instruction: function.instructions) {
            emit_instruction(instruction);
        }
    }

    static std::uint64_t get_initial_value(const Initial& initial) {
        return std::visit(overloaded {
            [](const auto& init) -> std::uint64_t {
                return init.value;
            }
        }, initial);
    }

    std::string type_suffix(ASM::Type type) {
        switch (type) {
            case ASM::Type::LongWord:
                return "l";
            case ASM::Type::QuadWord:
                return "q";
        }
    }
    int type_reg_size(ASM::Type type) {
        switch (type) {
            case ASM::Type::LongWord:
                return 4;
            case ASM::Type::QuadWord:
                return 8;
        }
    }

    bool is_initial_zero(const Initial & initial) {
        return std::visit(overloaded {
            [](const InitialInt& init) {
                return init.value == 0;
            },
            [](const InitialLong& init) {
                return init.value == 0l;
            },
            [](const InitialUInt& init) {
                return init.value == 0u;
            },
            [](const InitialULong& init) {
                return init.value == 0ul;
            }
        }, initial);
    }

    int get_aligment(const Initial & initial) {
        return std::visit(overloaded {
            [](const InitialInt&) {
                return 4;
            },
            [](const InitialLong&) {
                return 8;
            },
            [](const InitialUInt&) {
                return 4;
            },
            [](const InitialULong&) {
                return 8;
            }
        }, initial);
    }

    void emit_static_variable(const ASM::StaticVariable & variable) {
        if (variable.global) {
#if __APPLE__
            assembly += "    .globl _" + variable.name + "\n";
#else
            assembly += "    .globl " + variable.name + "\n";
#endif
        }

        if (is_initial_zero(variable.initial_value)) {
            assembly += "    .bss\n";
        } else {
            assembly += "    .data\n";
        }
        assembly += "    .balign " + std::to_string(get_aligment(variable.initial_value)) + " \n";
#if __APPLE__
        assembly += "_" + variable.name + ":\n";
#else
        assembly += variable.name + ":\n";
#endif
        if (get_initial_value(variable.initial_value) == 0) {
            if (std::holds_alternative<InitialInt>(variable.initial_value) || std::holds_alternative<InitialUInt>(variable.initial_value)) {
                assembly += "    .zero 4\n";
            } else {
                assembly += "    .zero 8\n";
            }
        } else {
            std::visit(overloaded {
                [this](const InitialInt& init) {
                    assembly += "    .long " + std::to_string(init.value) + "\n";
                },
                [this](const InitialUInt& init) {
                    assembly += "    .long " + std::to_string(init.value) + "\n";
                },
                [this](const InitialLong& init) {
                    assembly += "    .quad " + std::to_string(init.value) + "\n";
                },
                [this](const InitialULong& init) {
                    assembly += "    .quad " + std::to_string(init.value) + "\n";
                }
            }, variable.initial_value);
        }
    }



    void emit_call(const ASM::Call &ins) {
#if __APPLE__
        assembly += "    call _" + ins.name + "\n";
#else
        assembly += "    call " + ins.name + (symbols->contains(ins.name) ? "" : "@PLT") + "\n";
#endif
    }


    void emit_push(const ASM::Push &ins) {
        assembly += "    pushq ";
        emit_operand(ins.value, 8);
        assembly += "\n";
    }

    void emit_movsx(const ASM::Movsx & ins) {
        assembly += "    movslq ";
        emit_operand(ins.source, 4);
        assembly += ", ";
        emit_operand(ins.destination, 8);
        assembly += "\n";
    }

    void emit_div(const ASM::Div & ins) {
        assembly += "    div" + type_suffix(ins.type) + " ";
        emit_operand(ins.divisor, type_reg_size(ins.type));
        assembly += "\n";
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
                       },
                       [this](const ASM::Call &ins) {
                           emit_call(ins);
                       },
                       [this](const ASM::Push &ins) {
                           emit_push(ins);
                       },
                       [this](const ASM::Movsx& ins) {
                           emit_movsx(ins);
                       },
                [this](const ASM::Div &ins) {
                    emit_div(ins);
                },
            [this](const ASM::MovZeroExtend &ins) {}
                   }, instruction);
    }

    void emit_mov(const ASM::Mov &ins) {
        assembly += "    mov" + type_suffix(ins.type) + " ";
        emit_operand(ins.src, type_reg_size(ins.type));
        assembly += ", ";
        emit_operand(ins.dst, type_reg_size(ins.type));
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
                assembly += "    not";
                break;
            case ASM::Unary::Operator::Neg:
                assembly += "    neg";
                break;
        }
        assembly += type_suffix(ins.type) + " ";
        emit_operand(ins.operand, type_reg_size(ins.type));
        assembly += "\n";
    }


    void emit_binary(const ASM::Binary &ins) {
        switch (ins.op) {
            case ASM::Binary::Operator::ADD:
                assembly += "    add";
                break;
            case ASM::Binary::Operator::SUB:
                assembly += "    sub";
                break;
            case ASM::Binary::Operator::MULT:
                assembly += "    imul";
                break;
            case ASM::Binary::Operator::SHL:
                assembly += "    shl";
                break;
            case ASM::Binary::Operator::SAR:
                assembly += "    sar";
                break;
            case ASM::Binary::Operator::SHR:
                assembly += "    shr";
            break;
            case ASM::Binary::Operator::AND:
                assembly += "    and";
                break;
            case ASM::Binary::Operator::XOR:
                assembly += "    xor";
                break;
            case ASM::Binary::Operator::OR:
                assembly += "    or";
                break;
        }
        assembly += type_suffix(ins.type) + " ";
        if (ins.op == ASM::Binary::Operator::SHR || ins.op == ASM::Binary::Operator::SHL || ins.op == ASM::Binary::Operator::SAR) {
            emit_operand(ins.left, 1);
        } else {
            emit_operand(ins.left, type_reg_size(ins.type));
        }
        assembly += ", ";
        emit_operand(ins.right, type_reg_size(ins.type));
        assembly += "\n";
    }

    void emit_idiv(const ASM::Idiv &ins) {
        assembly += "    idiv" + type_suffix(ins.type) + " ";
        emit_operand(ins.divisor, type_reg_size(ins.type));
        assembly += "\n";
    }

    void emit_cdq(const ASM::Cdq & ins) {
        if (ins.type == ASM::Type::LongWord) {
            assembly += "    cdq\n";
        } else {
            assembly += "    cqo\n";
        }
    }

    void emit_cmp(const ASM::Cmp &ins) {
        assembly += "    cmp" + type_suffix(ins.type) + " ";
        emit_operand(ins.left, type_reg_size(ins.type));
        assembly += ", ";
        emit_operand(ins.right, type_reg_size(ins.type));
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
            case ASM::ConditionCode::A:
                assembly += "a";
                break;
            case ASM::ConditionCode::AE:
                assembly += "ae";
                break;
            case ASM::ConditionCode::B:
                assembly += "b";
                break;
            case ASM::ConditionCode::BE:
                assembly += "be";
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
        emit_operand(ins.destination, 1);
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


    void emit_operand(const ASM::Operand &operand, int reg_size = 4) {
        std::visit(overloaded{
                       [this](const ASM::Imm &operand) {
                           emit_imm(operand);
                       },
                       [this, reg_size](const ASM::Reg &operand) {
                           if (reg_size == 1) {
                               emit_1byte_register(operand);
                           } else if (reg_size == 8) {
                               emit_8byte_register(operand);
                           } else {
                               emit_4byte_register(operand);
                           }
                       },
                       [this](const ASM::Stack &operand) {
                           emit_stack(operand);
                       },
                       [this](const ASM::Data& operand) {
                           emit_data(operand);
                       },
                       [this](auto &) {
                           // TODO: panic
                       }
                   }, operand);
    }

    void emit_imm(const ASM::Imm &operand) {
        assembly += std::format("${}", operand.value);
    }
    void emit_data(const ASM::Data & data) {
#ifdef __APPLE__
        assembly += "_" + data.identifier + "(%rip)";
#else
        assembly += data.identifier + "(%rip)";
#endif
    }

    void emit_8byte_register(const ASM::Reg &reg) {
        switch (reg.name) {
            case ASM::Reg::Name::AX:
                assembly += "%rax";
                break;
            case ASM::Reg::Name::DX:
                assembly += "%rdx";
                break;
            case ASM::Reg::Name::CX:
                assembly += "%rcx";
                break;
            case ASM::Reg::Name::DI:
                assembly += "%rdi";
                break;
            case ASM::Reg::Name::SI:
                assembly += "%rsi";
                break;
            case ASM::Reg::Name::R8:
                assembly += "%r8";
                break;
            case ASM::Reg::Name::R9:
                assembly += "%r9";
                break;
            case ASM::Reg::Name::R10:
                assembly += "%r10";
                break;
            case ASM::Reg::Name::R11:
                assembly += "%r11";
                break;
            case ASM::Reg::Name::SP:
                assembly += "%rsp";
                break;
        }
    }

    void emit_4byte_register(const ASM::Reg &operand) {
        switch (operand.name) {
            case ASM::Reg::Name::AX:
                assembly += "%eax";
                break;
            case ASM::Reg::Name::DX:
                assembly += "%edx";
                break;
            case ASM::Reg::Name::CX:
                assembly += "%ecx";
                break;
            case ASM::Reg::Name::DI:
                assembly += "%edi";
                break;
            case ASM::Reg::Name::SI:
                assembly += "%esi";
                break;
            case ASM::Reg::Name::R8:
                assembly += "%r8d";
                break;
            case ASM::Reg::Name::R9:
                assembly += "%r9d";
                break;
            case ASM::Reg::Name::R10:
                assembly += "%r10d";
                break;
            case ASM::Reg::Name::R11:
                assembly += "%r11d";
                break;
            case ASM::Reg::Name::SP:
                assembly += "%esp";
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
            case ASM::Reg::Name::CX:
                assembly += "%cl";
                break;
            case ASM::Reg::Name::DI:
                assembly += "%dil";
                break;
            case ASM::Reg::Name::R8:
                assembly += "%r8b";
                break;
            case ASM::Reg::Name::R9:
                assembly += "%r9b";
                break;
            case ASM::Reg::Name::R10:
                assembly += "%r10b";
                break;
            case ASM::Reg::Name::R11:
                assembly += "%r11b";
                break;
            case ASM::Reg::Name::SP:
                assembly += "%sp";
                break;
        }
    }

    void emit_stack(const ASM::Stack &operand) {
        assembly += std::format("{}(%rbp)", operand.offset);
    }


    std::string assembly;

private:
    ASM::Program asmProgram;
    std::unordered_map<std::string, Symbol> *symbols;
};

#endif //CODEEMITTER_H
