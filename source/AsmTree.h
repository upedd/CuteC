#ifndef ASMTREE_H
#define ASMTREE_H
#include <string>
#include <vector>
#include <variant>

#include "analysis/TypeCheckerPass.h"

namespace ASM {
    struct LongWord {};
    struct QuadWord {};
    struct Double {};
    struct ByteArray {
        int size;
        int alignment;
    };
    using Type = std::variant<LongWord, QuadWord, Double, ByteArray>;

    struct ObjectSymbol {
        Type type;
        bool is_static;
        bool is_constant;
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
        BE,
        NB,
        P,
        NP
    };

    using Operand = std::variant<struct Imm, struct Reg, struct Pseudo, struct Memory, struct Data, struct Indexed, struct PseudoMem>;
    using Instruction = std::variant<struct Mov, struct Ret, struct Unary, struct Binary, struct
        Idiv, struct Div, struct Cdq, struct Cmp, struct Jmp, struct JmpCC, struct SetCC, struct Label,
        struct Push, struct Call, struct Movsx, struct MovZeroExtend, struct Cvttsd2si, struct Cvtsi2sd, struct Lea>;

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

    struct StaticConstant {
        std::string name;
        int alignment;
        Initial initial_value;
    };

    struct Program {
        std::vector<std::variant<Function, StaticVariable, StaticConstant>> items;
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
            SP,
            BP,
            XMM0,
            XMM1,
            XMM2,
            XMM3,
            XMM4,
            XMM5,
            XMM6,
            XMM7,
            XMM14,
            XMM15
        };

        Name name;
    };

    struct Pseudo {
        std::string name;
    };

    struct Memory {
        Reg reg;
        int offset;
    };

    struct Data {
        std::string identifier;
    };

    struct Indexed {
        Reg base;
        Reg index;
        int scale;
    };

    struct PseudoMem {
        std::string identifier;
        int offset;
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
            Not,
            Shr
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
            SHL,
            DIV_DOUBLE
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

    struct Cvttsd2si {
        Type type;
        Operand source;
        Operand destination;
    };

    struct Cvtsi2sd {
        Type type;
        Operand source;
        Operand destination;
    };

    struct Lea {
        Operand source;
        Operand destination;
    };
}

#endif //ASMTREE_H
