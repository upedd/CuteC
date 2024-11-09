#ifndef AST_H
#define AST_H
#include <memory>
#include <variant>

// TODO: zone allocation
namespace AST {
    struct Function;
    struct Program;

    using Stmt = std::variant<struct ReturnStmt>;
    using Expr = std::variant<struct ConstantExpr>;
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
}

#endif //AST_H
