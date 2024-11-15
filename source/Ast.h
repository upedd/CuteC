#ifndef AST_H
#define AST_H
#include <memory>
#include <variant>

// TODO: zone allocation
namespace AST {
    struct Function;
    struct Program;

    using Stmt = std::variant<struct ReturnStmt>;
    using Expr = std::variant<struct ConstantExpr, struct UnaryExpr, struct BinaryExpr>;
    using ExprHandle = std::unique_ptr<Expr>;
    using StmtHandle = std::unique_ptr<Stmt>;

    struct Function {
        std::string name;
        StmtHandle body;
    };

    struct Program {
        Function function;
    };

    struct ReturnStmt {
        ExprHandle expr;
    };

    struct ConstantExpr {
        int value;
    };

    struct UnaryExpr {
        enum class Kind {
            NEGATE,
            COMPLEMENT,
            LOGICAL_NOT
        };

        Kind kind;
        ExprHandle expr;
    };

    struct BinaryExpr {
        enum class Kind {
            ADD,
            SUBTRACT,
            MULTIPLY,
            DIVIDE,
            REMAINDER,
            SHIFT_LEFT,
            SHIFT_RIGHT,
            BITWISE_AND,
            BITWISE_OR,
            BITWISE_XOR,
            LOGICAL_AND,
            LOGICAL_OR,
            EQUAL,
            NOT_EQUAL,
            LESS,
            LESS_EQUAL,
            GREATER,
            GREATER_EQUAL
        };

        Kind kind;
        ExprHandle left;
        ExprHandle right;
    };
}

#endif //AST_H
