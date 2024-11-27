#ifndef IRPRINTER_H
#define IRPRINTER_H
#include <iostream>

#include "IR.h"
#include "overloaded.h"

class IRPrinter {
public:
    explicit IRPrinter(IR::Program *program)
        : program(program) {
    }

    void print_static_variable(const IR::StaticVariable & item) {
        std::cout << "StaticVariable[" << item.name << "]" << "(global=" << (item.global ? "true" : "false") << ", initial_value=" << item.initial_value << ")\n";
    }

    void print() {
        for (const auto &item: program->items) {
            std::visit(overloaded {
                [this](const IR::Function& item) {
                    print_function(item);
                },
                [this](const IR::StaticVariable& item) {
                    print_static_variable(item);
                }
            }, item);
        }
    }

    void print_function(const IR::Function &function) {
        std::cout << "Function[" << function.name << ", global=" << function.global << "](";
        for (const auto &param: function.params) {
            std::cout << param << " ";
        }
        std::cout << ")" << '\n';
        for (const auto &ins: function.instructions) {
            print_instruction(ins);
        }
    }

    void print_call(const IR::Call &ins) {
        print_indent();
        print_value(ins.destination);
        std::cout << " = " << ins.name << " (";
        for (const auto &arg: ins.arguments) {
            print_value(arg);
            std::cout << " ";
        }
        std::cout << ")\n";
    }

    void print_instruction(const IR::Instruction &instruction) {
        std::visit(overloaded{
                       [this](const IR::Return &ins) {
                           print_return(ins);
                       },
                       [this](const IR::Unary &ins) {
                           print_unary(ins);
                       },
                       [this](const IR::Binary &ins) {
                           print_binary(ins);
                       },
                       [this](const IR::Copy &ins) {
                           print_copy(ins);
                       },
                       [this](const IR::Jump &ins) {
                           print_jump(ins);
                       },
                       [this](const IR::JumpIfZero &ins) {
                           print_jump_if_zero(ins);
                       },
                       [this](const IR::JumpIfNotZero &ins) {
                           print_jump_if_not_zero(ins);
                       },
                       [this](const IR::Label &ins) {
                           print_label(ins);
                       },
                       [this](const IR::Call &ins) {
                           print_call(ins);
                       }
                   }, instruction);
    }

    void print_value(const IR::Value &value) {
        std::visit(overloaded{
                       [this](const IR::Constant &ins) {
                           std::cout << ins.value;
                       },
                       [this](const IR::Variable &ins) {
                           std::cout << ins.name;
                       }
                   }, value);
    }

    void print_indent() {
        std::cout << "   ";
    }

    void print_return(const IR::Return &ins) {
        std::cout << "   return ";
        print_value(ins.value);
        std::cout << '\n';
    }

    void print_unary(const IR::Unary &ins) {
        print_indent();
        print_value(ins.destination);
        std::cout << " = ";
        switch (ins.op) {
            case IR::Unary::Operator::NEGATE:
                std::cout << '-';
                break;
            case IR::Unary::Operator::COMPLEMENT:
                std::cout << '~';
                break;
            case IR::Unary::Operator::LOGICAL_NOT:
                std::cout << '!';
                break;
        }
        print_value(ins.source);
        std::cout << '\n';
    }

    void print_binary(const IR::Binary &ins) {
        print_indent();
        print_value(ins.destination);
        std::cout << " = ";
        print_value(ins.left_source);
        switch (ins.op) {
            case IR::Binary::Operator::ADD:
                std::cout << '+';
                break;
            case IR::Binary::Operator::SUBTRACT:
                std::cout << '-';
                break;
            case IR::Binary::Operator::MULTIPLY:
                std::cout << '*';
                break;
            case IR::Binary::Operator::DIVIDE:
                std::cout << '/';
                break;
            case IR::Binary::Operator::SHIFT_LEFT:
                std::cout << "<<";
                break;
            case IR::Binary::Operator::SHIFT_RIGHT:
                std::cout << ">>";
                break;
            case IR::Binary::Operator::BITWISE_AND:
                std::cout << '&';
                break;
            case IR::Binary::Operator::BITWISE_OR:
                std::cout << '|';
                break;
            case IR::Binary::Operator::BITWISE_XOR:
                std::cout << '^';
                break;
            case IR::Binary::Operator::EQUAL:
                std::cout << "==";
                break;
            case IR::Binary::Operator::NOT_EQUAL:
                std::cout << "!=";
                break;
            case IR::Binary::Operator::GREATER:
                std::cout << ">";
                break;
            case IR::Binary::Operator::GREATER_EQUAL:
                std::cout << ">=";
                break;
            case IR::Binary::Operator::LESS:
                std::cout << "<";
                break;
            case IR::Binary::Operator::LESS_EQUAL:
                std::cout << "<=";
                break;
            case IR::Binary::Operator::REMAINDER:
                std::cout << "%";
                break;
        }
        print_value(ins.right_source);
        std::cout << '\n';
    }

    void print_copy(const IR::Copy &ins) {
        print_indent();
        print_value(ins.destination);
        std::cout << " = ";
        print_value(ins.source);
        std::cout << '\n';
    }

    void print_jump(const IR::Jump &ins) {
        std::cout << "    jmp @" << ins.target << '\n';
    }

    void print_jump_if_zero(const IR::JumpIfZero &ins) {
        std::cout << "    jmpz ";
        print_value(ins.condition);
        std::cout << " @" << ins.target << '\n';
    }

    void print_jump_if_not_zero(const IR::JumpIfNotZero &ins) {
        std::cout << "    jmpnz ";
        print_value(ins.condition);
        std::cout << " @" << ins.target << '\n';
    }

    void print_label(const IR::Label &ins) {
        std::cout << "@" << ins.name << '\n';
    }

private:
    IR::Program *program;
};
#endif //IRPRINTER_H
