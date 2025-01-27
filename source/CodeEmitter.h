#ifndef CODEEMITTER_H
#define CODEEMITTER_H
#include <format>
#include <cstring> // for memcpy

#include "AsmTree.h"
#include "Codegen.h"
#include "overloaded.h"
#include "analysis/TypeCheckerPass.h"

// asm tree -> asm file


class CodeEmitter {
public:
    CodeEmitter(ASM::Program program,
                std::unordered_map<std::string, ASM::Symbol> *symbols) : asmProgram(std::move(program)), symbols(symbols) {
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
                },
                [this](const ASM::StaticConstant& item) {
                    emit_static_constant(item);
                }
            }, item);
        }
#if __linux__
        // disable executable stack
        assembly += ".section .note.GNU-stack,\"\",@progbits\n";
#endif
    }

    void emit_function(const ASM::Function &function) {
        assembly += "    .text\n";
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
        assembly += "    pushq %rbp\n";
        assembly += "    movq %rsp, %rbp\n";
        for (const auto &instruction: function.instructions) {
            emit_instruction(instruction);
        }
    }

    std::string type_suffix(ASM::Type type) {
        return std::visit(overloaded {
            [](const ASM::LongWord) {
                return "l";
            },
            [](const ASM::QuadWord) {
                return "q";
            },
            [](const ASM::Double) {
                return "sd";
            },
            [](const ASM::Byte) {
                return "b";
            },
            [](const ASM::ByteArray) {
                return "?"; //?
            }
        }, type);
    }
    int type_reg_size(ASM::Type type) {
        return std::visit(overloaded {
            [](const ASM::LongWord) {
                return 4;
            },
            [](const ASM::QuadWord) {
                return 8;
            },
            [](const ASM::Double) {
                return 8;
            },
            [](const ASM::Byte) {
                return 1;
            },
            [](const ASM::ByteArray) {
                return 0; //?
            }
        }, type);
    }
    // Note: this function actually check whether the binary representation of a number is all zeros
    // but does always return false for floating point values because there are two zero value 0.0 and -0.0
    bool is_initial_zero(const Initial & initial) {
        if (initial.size() != 1) return false;
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
            },
            [](const InitialDouble&) {
                return false;
            },
            [](const InitialZero&) {
                return true;
            },
            [](const InitialChar& init) {
                return init.value == 0;
            },
            [](const InitialUChar& init) {
                return init.value == 0;
            },
            [](const InitialString&) {
                return false; //?
            },
            [](const InitialPointer&) {
                return false; //?
            }
        }, initial[0]);
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
        assembly += "    .balign " + std::to_string(variable.alignment) + " \n";
#if __APPLE__
        assembly += "_" + variable.name + ":\n";
#else
        assembly += variable.name + ":\n";
#endif
        if (is_initial_zero(variable.initial_value)) {
            if (std::holds_alternative<InitialZero>(variable.initial_value[0])) {
                assembly += "    .zero " + std::to_string(std::get<InitialZero>(variable.initial_value[0]).bytes) + " \n";
            } else if (std::holds_alternative<InitialChar>(variable.initial_value[0]) || std::holds_alternative<InitialUChar>(variable.initial_value[0])) {
                assembly += "    .zero 1\n";
            } else if (std::holds_alternative<InitialInt>(variable.initial_value[0]) || std::holds_alternative<InitialUInt>(variable.initial_value[0])) {
                assembly += "    .zero 4\n";
            } else {
                assembly += "    .zero 8\n";
            }
        } else {
            for (const auto& element : variable.initial_value) {
                emit_init(element);
            }
        }
    }

    void emit_init(const InitialElement& initial_value) {
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
                },
                [this](const InitialDouble& init) {
                    // hack to get double binary representation
                    std::uint64_t u;
                    std::memcpy(&u, &init.value, sizeof(init.value));
                    assembly += "    .quad " + std::to_string(u) + "\n";
                },
                [this](const InitialZero& init) {
                    assembly += "    .zero " + std::to_string(init.bytes) + "\n";
                },
                [this](const InitialChar& init) {
                    assembly += "    .byte " + std::to_string(init.value) + "\n";
                },
            [this](const InitialUChar& init) {
                    assembly += "    .byte " + std::to_string(init.value) + "\n";
                },
                [this](const InitialString& init) {
                    if (init.null_terminated) {
                        assembly += "    .asciz ";
                    } else {
                        assembly += "    .ascii ";
                    }
                    assembly += '\"';
                    for (char c : init.value) {
                        if (c == '\n') {
                            assembly += "\\n";
                        } else if (c == '\\') {
                            assembly += "\\\\";
                        } else if (c == '\"') {
                            assembly += "\\\"";
                        } else {
                            assembly += c;
                        }
                    }
                    assembly += "\"\n";
                },
                [this](const InitialPointer& init) {
                    assembly += "    .quad " + with_local_label(init.name);
                }

            }, initial_value);
    }

    std::string with_local_label(std::string x) {
#ifdef __APPLE__
        return "L" + x;
#else
        return ".L" + x;
#endif
    }

    void emit_static_constant(const ASM::StaticConstant & item) {
#ifdef __APPLE__
        if (std::holds_alternative<InitialString>(item.initial_value[0])) {
            assembly += "    .cstring\n";
        } else if (item.alignment == 8) {
            assembly += "    .literal8\n";
            assembly += "    .balign 8\n";
        } else { // must be 16
            assembly += "    .literal16\n";
            assembly += "    .balign 16\n";
        }
#else
        assembly += "    .section .rodata\n";
        assembly += "    .balign " + std::to_string(item.alignment) + "\n";
#endif
        assembly += with_local_label(item.name) + ":\n";
        for (const auto& element : item.initial_value) {
            emit_init(element);
        }
#if __APPLE__
        if (item.alignment == 16) {
            assembly += "    .quad 0\n";
        }
#endif
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
        assembly += "    movs" + type_suffix(ins.source_type) + type_suffix(ins.destination_type) + " ";
        emit_operand(ins.source, type_reg_size(ins.source_type));
        assembly += ", ";
        emit_operand(ins.destination, type_reg_size(ins.destination_type));
        assembly += "\n";
    }

    void emit_div(const ASM::Div & ins) {
        assembly += "    div" + type_suffix(ins.type) + " ";
        emit_operand(ins.divisor, type_reg_size(ins.type));
        assembly += "\n";
    }

    void emit_cvtsi2sd(const ASM::Cvtsi2sd & ins) {
        assembly += "    cvtsi2sd" + type_suffix(ins.type) + " ";
        emit_operand(ins.source, type_reg_size(ins.type));
        assembly += ", ";
        emit_operand(ins.destination, type_reg_size(ins.type));
        assembly += "\n";
    }

    void emit_cvttsd2si(const ASM::Cvttsd2si & ins) {
        assembly += "    cvttsd2si" + type_suffix(ins.type) + " ";
        emit_operand(ins.source, type_reg_size(ins.type));
        assembly += ", ";
        emit_operand(ins.destination, type_reg_size(ins.type));
        assembly += "\n";
    }

    void emit_lea(const ASM::Lea & ins) {
        assembly += "    leaq ";
        emit_operand(ins.source, 8);
        assembly += ", ";
        emit_operand(ins.destination, 8);
        assembly += "\n";
    }

    void emit_mov_zero_extend(const ASM::MovZeroExtend & ins) {
        assembly += "    movz" + type_suffix(ins.source_type) + type_suffix(ins.destination_type) + " ";
        emit_operand(ins.source, type_reg_size(ins.source_type));
        assembly += ", ";
        emit_operand(ins.destination, type_reg_size(ins.destination_type));
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
            [this](const ASM::Cvtsi2sd& ins) {
                emit_cvtsi2sd(ins);
            },
            [this](const ASM::Cvttsd2si& ins) {
                emit_cvttsd2si(ins);
            },
            [this](const ASM::Lea& ins) {
                emit_lea(ins);
            },
             [this](const ASM::MovZeroExtend &ins) {
                 emit_mov_zero_extend(ins);
             }
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
            case ASM::Unary::Operator::Shr:
                assembly += "    shr";
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
                if (std::holds_alternative<ASM::Double>(ins.type)) {
                    assembly += "    mul";
                } else {
                    assembly += "    imul";
                }
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
                if (std::holds_alternative<ASM::Double>(ins.type)) {
                    assembly += "    xorpd ";
                } else {
                    assembly += "    xor";
                }
                break;
            case ASM::Binary::Operator::OR:
                assembly += "    or";
                break;
            case ASM::Binary::Operator::DIV_DOUBLE:
                assembly += "    div";
                break;
        }
        if (!std::holds_alternative<ASM::Double>(ins.type) || ins.op != ASM::Binary::Operator::XOR) { // do not emit type suffix for xor of doubles
            assembly += type_suffix(ins.type) + " ";
        }
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
        if (std::holds_alternative<ASM::LongWord>(ins.type)) {
            assembly += "    cdq\n";
        } else {
            assembly += "    cqo\n";
        }
    }

    void emit_cmp(const ASM::Cmp &ins) {
        if (std::holds_alternative<ASM::Double>(ins.type)) {
            assembly += "    comisd ";
        } else {
            assembly += "    cmp" + type_suffix(ins.type) + " ";
        }
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
            case ASM::ConditionCode::NB:
                assembly += "nb";
            break;
            case ASM::ConditionCode::P:
                assembly += "p";
            break;
            case ASM::ConditionCode::NP:
                assembly += "np";
            break;
        }
    }

    void emit_jmp(const ASM::Jmp &ins) {
        assembly += "    jmp " + with_local_label(ins.target) + "\n";
    }

    void emit_jmpcc(const ASM::JmpCC &ins) {
        assembly += "    j";
        emit_condition_code(ins.cond_code);
        assembly += " " + with_local_label(ins.target) + "\n";
    }

    void emit_setcc(const ASM::SetCC &ins) {
        assembly += "    set";
        emit_condition_code(ins.cond_code);
        assembly += " ";
        emit_operand(ins.destination, 1);
        assembly += "\n";
    }

    void emit_label(const ASM::Label &ins) {
        assembly += with_local_label(ins.name) + ":\n";
    }
    // duplicate!
    bool is_sse_register(const ASM::Reg& reg) {
        switch (reg.name) {
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


    void emit_sse_register(const ASM::Reg& reg) {
        switch (reg.name) {
            case ASM::Reg::Name::XMM0:
                assembly += "%xmm0";
                break;
            case ASM::Reg::Name::XMM1:
                assembly += "%xmm1";
                break;
            case ASM::Reg::Name::XMM2:
                assembly += "%xmm2";
            break;
            case ASM::Reg::Name::XMM3:
                assembly += "%xmm3";
            break;
            case ASM::Reg::Name::XMM4:
                assembly += "%xmm4";
            break;
            case ASM::Reg::Name::XMM5:
                assembly += "%xmm5";
            break;
            case ASM::Reg::Name::XMM6:
                assembly += "%xmm6";
            break;
            case ASM::Reg::Name::XMM7:
                assembly += "%xmm7";
            break;
            case ASM::Reg::Name::XMM14:
                assembly += "%xmm14";
            break;
            case ASM::Reg::Name::XMM15:
                assembly += "%xmm15";
            break;
        }
    }

    void emit_indexed(const ASM::Indexed & operand) {
        assembly += "(";
        emit_8byte_register(operand.base);
        assembly += ", ";
        emit_8byte_register(operand.index);
        assembly += ", " + std::to_string(operand.scale) + ")";
    }

    void emit_operand(const ASM::Operand &operand, int reg_size = 4) {
        std::visit(overloaded{
                       [this](const ASM::Imm &operand) {
                           emit_imm(operand);
                       },
                       [this, reg_size](const ASM::Reg &operand) {
                           if (is_sse_register(operand)) {
                               emit_sse_register(operand);
                           } else if (reg_size == 1) {
                               emit_1byte_register(operand);
                           } else if (reg_size == 8) {
                               emit_8byte_register(operand);
                           } else {
                               emit_4byte_register(operand);
                           }
                       },
                       [this](const ASM::Memory &operand) {
                           emit_memory(operand);
                       },
                       [this](const ASM::Data& operand) {
                           emit_data(operand);
                       },
                       [this](const ASM::Indexed& operand) {
                           emit_indexed(operand);
                       } ,
                       [this](auto &) {
                           // TODO: panic
                       }
                   }, operand);
    }

    void emit_imm(const ASM::Imm &operand) {
        assembly += std::format("${}", operand.value);
    }
    void emit_data(const ASM::Data & data) {
        if (symbols->contains(data.identifier)) {
            auto& symbol = symbols->at(data.identifier);
            if (std::holds_alternative<ASM::ObjectSymbol>(symbol) && std::get<ASM::ObjectSymbol>(symbol).is_constant) {
                assembly += with_local_label(data.identifier) + "(%rip)";
                return;
            }
        }
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
            case ASM::Reg::Name::BP:
                assembly += "%rbp";
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
            case ASM::Reg::Name::BP:
                assembly += "%ebp";
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
            case ASM::Reg::Name::BP:
                assembly += "%bp";
            break;
            case ASM::Reg::Name::SI:
                assembly += "%sil";
            break;
        }
    }

    void emit_memory(const ASM::Memory &operand) {
        assembly += std::to_string(operand.offset);
        assembly += "(";
        emit_8byte_register(operand.reg);
        assembly += ")";
    }


    std::string assembly;

private:
    ASM::Program asmProgram;
    std::unordered_map<std::string, ASM::Symbol> *symbols;
};

#endif //CODEEMITTER_H
