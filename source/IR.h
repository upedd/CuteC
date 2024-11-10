#ifndef IR_H
#define IR_H
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace IR {
    using Value = std::variant<struct Constant, struct Variable>;
    using Instruction = std::variant<struct Return, struct Unary>;

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
}

#endif //IR_H
