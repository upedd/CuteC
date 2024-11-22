#ifndef AST_H
#define AST_H
#include <memory>
#include <variant>
#include <vector>

// TODO: zone allocation
namespace AST {
    struct Function;
    struct Program;

    using Stmt = std::variant<struct ReturnStmt, struct ExprStmt, struct NullStmt, struct IfStmt, struct LabeledStmt,
        struct GoToStmt, struct CompoundStmt>;
    using Expr = std::variant<struct ConstantExpr, struct UnaryExpr, struct BinaryExpr, struct VariableExpr, struct
        AssigmentExpr, struct ConditionalExpr>;
    using ExprHandle = std::unique_ptr<Expr>;
    using StmtHandle = std::unique_ptr<Stmt>;
    using DeclarationHandle = std::unique_ptr<struct Declaration>;

    using BlockItem = std::variant<StmtHandle, DeclarationHandle>;

    struct Declaration {
        std::string name;
        ExprHandle expr;
    };

    struct Function {
        std::string name;
        std::vector<BlockItem> body;
    };

    struct Program {
        Function function;
    };

    struct ReturnStmt {
        ExprHandle expr;
    };

    struct ExprStmt {
        ExprHandle expr;
    };

    struct NullStmt {
    };

    struct ConstantExpr {
        int value;
    };

    struct UnaryExpr {
        enum class Kind {
            NEGATE,
            COMPLEMENT,
            LOGICAL_NOT,
            PREFIX_INCREMENT,
            PREFIX_DECREMENT,
            POSTFIX_INCREMENT,
            POSTFIX_DECREMENT
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

    struct VariableExpr {
        std::string name;
    };

    struct AssigmentExpr {
        // used for compound assigment
        enum class Operator {
            NONE,
            ADD,
            SUBTRACT,
            MULTIPLY,
            DIVIDE,
            REMAINDER,
            SHIFT_LEFT,
            SHIFT_RIGHT,
            BITWISE_AND,
            BITWISE_OR,
            BITWISE_XOR
        };

        Operator op;
        ExprHandle left;
        ExprHandle right;
    };

    struct IfStmt {
        ExprHandle condition;
        StmtHandle then_stmt;
        StmtHandle else_stmt;
    };

    struct ConditionalExpr {
        ExprHandle condition;
        ExprHandle then_expr;
        ExprHandle else_expr;
    };

    struct LabeledStmt {
        std::string label;
        StmtHandle stmt;
    };

    struct GoToStmt {
        std::string label;
    };

    struct CompoundStmt {
        std::vector<BlockItem> body;
    };
}

#endif //AST_H
