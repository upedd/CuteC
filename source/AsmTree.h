#ifndef ASMTREE_H
#define ASMTREE_H
#include <string>
#include <vector>
#include <variant>

#include "analysis/TypeCheckerPass.h"

namespace ASM {
    enum class Type {
        LongWord,
        QuadWord,
    };

    struct ObjectSymbol {
        Type type;
        bool is_static;
    };

    struct FunctionSymbol {
        bool defined;
    };

    using Symbol = std::variant<ObjectSymbol, FunctionSymbol>;

    enum class ConditionCode {
        E,
        NE,
        G,
        GE,
        L,
        LE,
        A,
        AE,
        B,
        BE
    };

    using Operand = std::variant<struct Imm, struct Reg, struct Pseudo, struct Stack, struct Data>;
    using Instruction = std::variant<struct Mov, struct Ret, struct Unary, struct Binary, struct
        Idiv, struct Div, struct Cdq, struct Cmp, struct Jmp, struct JmpCC, struct SetCC, struct Label,
        struct Push, struct Call, struct Movsx, struct MovZeroExtend>;

    struct Function {
        std::string name;
        bool global;
        std::vector<Instruction> instructions;
        int stack_size = 0;
    };

    struct StaticVariable {
        std::string name;
        bool global;
        int alignment;
        Initial initial_value;
    };

    struct Program {
        std::vector<std::variant<Function, StaticVariable>> items;
    };

    struct Imm {
        std::uint64_t value;
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
            SP
        };

        Name name;
    };

    struct Pseudo {
        std::string name;
    };

    struct Stack {
        int offset;
    };

    struct Data {
        std::string identifier;
    };

    struct Mov {
        Type type;
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
        Type type;
        Operand operand;
    };

    struct Binary {
        enum class Operator {
            ADD,
            SUB,
            MULT,
            AND,
            OR,
            XOR,
            SAR, // arithmetic shift
            SHR, // logical shift
            SHL
        };

        Operator op;
        Type type;
        Operand left;
        Operand right;
    };

    struct Idiv {
        Type type;
        Operand divisor;
    };

    struct Div {
        Type type;
        Operand divisor;
    };

    struct Cdq {
        Type type;
    };

    struct Cmp {
        Type type;
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

    struct Push {
        Operand value;
    };

    struct Call {
        std::string name;
    };

    struct Movsx {
        Operand source;
        Operand destination;
    };

    struct MovZeroExtend {
        Operand source;
        Operand destination;
    };
}

#endif //ASMTREE_H
