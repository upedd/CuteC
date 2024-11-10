#ifndef ASMTREE_H
#define ASMTREE_H
#include <string>
#include <vector>
#include <variant>

namespace ASM {
    using Operand = std::variant<struct Imm, struct Reg, struct Pseudo, struct Stack>;
    using Instruction = std::variant<struct Mov, struct Ret, struct Unary, struct AllocateStack>;

    struct Function {
        std::string name;
        std::vector<Instruction> instructions;
    };

    struct Program {
        Function function;
    };

    struct Imm {
        int value;
    };

    struct Reg {
        enum class Name {
            AX,
            R10
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
}

#endif //ASMTREE_H
