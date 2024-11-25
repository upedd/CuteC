#ifndef ASMTREE_H
#define ASMTREE_H
#include <string>
#include <vector>
#include <variant>

namespace ASM {
    enum class ConditionCode {
        E,
        NE,
        G,
        GE,
        L,
        LE
    };

    using Operand = std::variant<struct Imm, struct Reg, struct Pseudo, struct Stack>;
    using Instruction = std::variant<struct Mov, struct Ret, struct Unary, struct AllocateStack, struct Binary, struct
        Idiv, struct Cdq, struct Cmp, struct Jmp, struct JmpCC, struct SetCC, struct Label, struct DeallocateStack,
        struct Push, struct Call>;

    struct Function {
        std::string name;
        std::vector<Instruction> instructions;
        int stack_size = 0;
    };

    struct Program {
        std::vector<Function> functions;
    };

    struct Imm {
        int value;
    };

    struct Reg {
        enum class Name {
            AX,
            CX,
            DX,
            DI,
            SI,
            R8,
            R9,
            R10,
            R11,
        };

        Name name;
    };

    struct Pseudo {
        std::string name;
    };

    struct Stack {
        int offset;
    };

    struct Mov {
        Operand src;
        Operand dst;
    };

    struct Ret {
    };

    struct Unary {
        enum class Operator {
            Neg,
            Not
        };

        Operator op;
        Operand operand;
    };

    struct AllocateStack {
        int size;
    };

    struct Binary {
        enum class Operator {
            ADD,
            SUB,
            MULT,
            AND,
            OR,
            XOR,
            SHR,
            SHL
        };

        Operator op;
        Operand left;
        Operand right;
    };

    struct Idiv {
        Operand divisor;
    };

    struct Cdq {
    };

    struct Cmp {
        Operand left;
        Operand right;
    };

    struct Jmp {
        std::string target;
    };

    struct JmpCC {
        ConditionCode cond_code;
        std::string target;
    };

    struct SetCC {
        ConditionCode cond_code;
        Operand destination;
    };

    struct Label {
        std::string name;
    };

    struct DeallocateStack {
        int size;
    };

    struct Push {
        Operand value;
    };

    struct Call {
        std::string name;
    };
}

#endif //ASMTREE_H
