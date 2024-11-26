#ifndef AST_H
#define AST_H
#include <memory>
#include <optional>
#include <unordered_map>
#include <variant>
#include <vector>

// TODO: zone allocation
namespace AST {
    struct Program;

    using Stmt = std::variant<struct ReturnStmt, struct ExprStmt, struct NullStmt, struct IfStmt, struct LabeledStmt,
        struct GoToStmt, struct CompoundStmt, struct BreakStmt, struct ContinueStmt, struct WhileStmt, struct
        DoWhileStmt, struct ForStmt, struct SwitchStmt, struct CaseStmt, struct DefaultStmt>;
    using Expr = std::variant<struct ConstantExpr, struct UnaryExpr, struct BinaryExpr, struct VariableExpr, struct
        AssigmentExpr, struct ConditionalExpr, struct FunctionCall>;
    using ExprHandle = std::unique_ptr<Expr>;
    using StmtHandle = std::unique_ptr<Stmt>;

    using Declaration = std::variant<struct VariableDecl, struct FunctionDecl>;
    using DeclHandle = std::unique_ptr<Declaration>;

    using BlockItem = std::variant<StmtHandle, DeclHandle>;

    enum class StorageClass {
        NONE,
        STATIC,
        EXTERN
    };

    struct VariableDecl {
        std::string name;
        ExprHandle expr;
        StorageClass storage_class;
    };

    struct FunctionDecl {
        std::string name;
        std::vector<std::string> params;
        std::optional<std::vector<BlockItem> > body;
        StorageClass storage_class;
    };



    struct Program {
        std::vector<DeclHandle> declarations;
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

    struct BreakStmt {
        std::string label;
    };

    struct ContinueStmt {
        std::string label;
    };

    struct WhileStmt {
        ExprHandle condition;
        StmtHandle body;
        std::string label;
    };

    struct DoWhileStmt {
        ExprHandle condition;
        StmtHandle body;
        std::string label;
    };

    struct ForStmt {
        std::variant<std::unique_ptr<VariableDecl>, ExprHandle> init;
        ExprHandle condition;
        ExprHandle post;
        StmtHandle body;
        std::string label;
    };

    struct SwitchStmt {
        ExprHandle expr;
        StmtHandle body;
        std::string label;
        std::unordered_map<int, std::string> cases; // will be resolved in semantic analysis
        bool has_default = false;
    };

    struct CaseStmt {
        ExprHandle value;
        StmtHandle stmt;
        std::string label;
    };

    struct DefaultStmt {
        StmtHandle stmt;
        std::string label;
    };

    struct FunctionCall {
        std::string identifier;
        std::vector<ExprHandle> arguments;
    };
}

#endif //AST_H
