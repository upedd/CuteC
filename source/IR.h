#ifndef IR_H
#define IR_H
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "analysis/TypeCheckerPass.h"

namespace IR {
    using Value = std::variant<struct Constant, struct Variable>;
    using Instruction = std::variant<struct Return, struct Unary, struct Binary, struct Copy, struct Jump, struct
        JumpIfZero, struct JumpIfNotZero, struct Label, struct Call, struct SignExtend, struct Truncate, struct ZeroExtend, struct DoubleToInt, struct DoubleToUInt, struct IntToDouble, struct UIntToDouble, struct GetAddress, struct Load, struct Store, struct AddPtr, struct CopyToOffset>;

    struct Function {
        std::string name;
        bool global;
        std::vector<std::string> params;
        std::vector<Instruction> instructions;
    };

    struct StaticVariable {
        std::string name;
        bool global;
        Initial initial;
        AST::TypeHandle type;
    };

    struct StaticConstant {
        std::string name;
        AST::TypeHandle type;
        Initial initial;
    };

    struct Program {
        std::vector<std::variant<Function, StaticVariable, StaticConstant>> items;
    };

    struct Constant {
        AST::Const constant;
    };

    struct Variable {
        std::string name;
    };

    struct Return {
        Value value;
    };

    struct Unary {
        enum class Operator {
            NEGATE,
            COMPLEMENT,
            LOGICAL_NOT,
        };

        Unary(Operator op, Value source, Value destination)
            : op(op),
              source(std::move(source)),
              destination(std::move(destination)) {
        }

        Operator op;
        Value source;
        Value destination;
    };

    struct Binary {
        enum class Operator {
            ADD,
            SUBTRACT,
            MULTIPLY,
            DIVIDE,
            REMAINDER,
            SHIFT_LEFT,
            SHIFT_RIGHT,
            BITWISE_AND,
            BITWISE_XOR,
            BITWISE_OR,
            EQUAL,
            NOT_EQUAL,
            LESS,
            LESS_EQUAL,
            GREATER,
            GREATER_EQUAL,
        };

        Operator op;
        Value left_source;
        Value right_source;
        Value destination;
    };

    struct Copy {
        Value source;
        Value destination;
    };

    struct Jump {
        std::string target;
    };

    struct JumpIfZero {
        Value condition;
        std::string target;
    };

    struct JumpIfNotZero {
        Value condition;
        std::string target;
    };

    struct Label {
        std::string name;
    };

    struct Call {
        std::string name;
        std::vector<Value> arguments;
        Value destination;
    };

    struct SignExtend {
        Value source;
        Value destination;
    };

    struct Truncate {
        Value source;
        Value destination;
    };

    struct ZeroExtend {
        Value source;
        Value destination;
    };

    struct DoubleToInt {
        Value source;
        Value destination;
    };

    struct DoubleToUInt {
        Value source;
        Value destination;
    };

    struct IntToDouble {
        Value source;
        Value destination;
    };

    struct UIntToDouble {
        Value source;
        Value destination;
    };

    struct GetAddress {
        Value source;
        Value destination;
    };

    struct Load {
        Value source_ptr;
        Value destination;
    };

    struct Store {
        Value source;
        Value destination_ptr;
    };

    struct AddPtr {
        Value ptr;
        Value index;
        int scale;
        Value destination;
    };

    struct CopyToOffset {
        Value source;
        std::string destination;
        int offset;
    };
}

#endif //IR_H
