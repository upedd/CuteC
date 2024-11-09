#ifndef ASMTREE_H
#define ASMTREE_H
#include <string>
#include <vector>
#include <variant>

namespace ASM {
    using Operand = std::variant<struct Imm, struct Register>;
    using Instruction = std::variant<struct Mov, struct Ret>;


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

    struct Register {};

    struct Mov {
        Operand src;
        Operand dst;
    };

    struct Ret {};

}

#endif //ASMTREE_H
