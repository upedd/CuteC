#ifndef IR_H
#define IR_H
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace IR {
    using Value = std::variant<struct Constant, struct Variable>;
    using Instruction = std::variant<struct Return, struct Unary, struct Binary>;

    struct Function {
        std::string name;
        std::vector<Instruction> instructions;
    };

    struct Program {
        Function function;
    };

    struct Constant {
        int value;
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
            BITWISE_OR
        };

        Operator op;
        Value left_source;
        Value right_source;
        Value destination;
    };
}

#endif //IR_H
