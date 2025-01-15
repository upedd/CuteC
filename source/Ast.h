#ifndef AST_H
#define AST_H
#include <memory>
#include <optional>
#include <unordered_map>
#include <variant>
#include <vector>
#include <cstdint>
#include <map>

#include "overloaded.h"
#include "common/box.h"

// TODO: zone allocation
namespace AST {
    struct Program;

    using Stmt = std::variant<struct ReturnStmt, struct ExprStmt, struct NullStmt, struct IfStmt, struct LabeledStmt,
        struct GoToStmt, struct CompoundStmt, struct BreakStmt, struct ContinueStmt, struct WhileStmt, struct
        DoWhileStmt, struct ForStmt, struct SwitchStmt, struct CaseStmt, struct DefaultStmt>;
    using Expr = std::variant<struct ConstantExpr, struct UnaryExpr, struct BinaryExpr, struct VariableExpr, struct
        AssigmentExpr, struct ConditionalExpr, struct FunctionCall, struct CastExpr, struct DereferenceExpr, struct AddressOfExpr, struct CompoundExpr, struct TemporaryExpr, struct SubscriptExpr>;
    using ExprHandle = std::unique_ptr<Expr>;
    using StmtHandle = std::unique_ptr<Stmt>;

    using Declaration = std::variant<struct VariableDecl, struct FunctionDecl>;
    using DeclHandle = std::unique_ptr<Declaration>;


    using Declarator = std::variant<struct Identifier, struct PointerDeclarator, struct FunctionDeclarator, struct ArrayDeclarator>;
    using DeclaratorHandle = std::unique_ptr<Declarator>;

    using AbstractDeclarator = std::variant<struct AbstractBase, struct AbstractPointer, struct AbstractArray>;
    using AbstractDeclaratorHandle = std::unique_ptr<AbstractDeclarator>;

    struct AbstractBase {};
    struct AbstractPointer {
        AbstractDeclaratorHandle declarator;
    };
    struct AbstractArray {
        AbstractDeclaratorHandle declarator;
        std::uint64_t size;
    };

    using BlockItem = std::variant<StmtHandle, DeclHandle>;

    enum class StorageClass {
        NONE,
        STATIC,
        EXTERN
    };

    using Type = std::variant<struct EmptyType, struct IntType, struct LongType, struct UIntType, struct ULongType, struct DoubleType, struct FunctionType, struct PointerType, struct ArrayType>;
    using TypeHandle = box<Type>;

    using Initializer = std::variant<struct ScalarInit, struct CompoundInit>;
    using InitializerHandle = std::unique_ptr<Initializer>;
    struct ScalarInit {
        ExprHandle value;
        TypeHandle type;
    };

    struct CompoundInit {
        std::vector<InitializerHandle> init;
        TypeHandle type;
    };

    struct Identifier {
        std::string name;
    };

    struct PointerDeclarator {
        DeclaratorHandle declarator;
    };

    struct Param {
        DeclaratorHandle declarator;
        TypeHandle type;
    };

    struct FunctionDeclarator {
        DeclaratorHandle declarator;
        std::vector<Param> params;
    };

    struct ArrayDeclarator {
        DeclaratorHandle declarator;
        std::uint64_t size;
    };

    struct EmptyType {};
    struct IntType {};
    struct LongType {};
    struct UIntType {};
    struct ULongType {};
    struct DoubleType {};
    struct FunctionType {
        std::vector<TypeHandle> parameters_types;
        TypeHandle return_type;
    };
    struct PointerType {
        TypeHandle referenced_type;
    };

    struct ArrayType {
        TypeHandle element_type;
        std::uint64_t size;
    };


    struct VariableDecl {
        std::string name;
        InitializerHandle init;
        TypeHandle type;
        StorageClass storage_class;
    };

    struct FunctionDecl {
        std::string name;
        std::vector<std::string> params;
        std::optional<std::vector<BlockItem> > body;
        TypeHandle type;
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

    struct ConstInt {
        int value;
    };

    struct ConstLong {
        std::int64_t value;
    };

    struct ConstUInt {
        unsigned int value;
    };

    struct ConstULong {
        std::uint64_t value;
    };

    struct ConstDouble {
        double value;
    };

    using Const = std::variant<ConstInt, ConstLong, ConstUInt, ConstULong, ConstDouble>;
    // mess?
    inline bool operator==(const Const& lhs, const Const& rhs) {
        return std::visit(overloaded {
            [&rhs](const auto& l) -> bool {
                return std::visit(overloaded {
                    [&l](const auto& r) {
                        return l.value == r.value;
                    }
                }, rhs);
            }
        }, lhs);
    }
    inline bool operator<(const Const& lhs, const Const& rhs) {
        return std::visit(overloaded {
            [&rhs](const auto& l) -> bool {
                return std::visit(overloaded {
                    [&l](const auto& r) {
                        return l.value < r.value;
                    }
                }, rhs);
            }
        }, lhs);
    }

    struct ConstantExpr {
        Const constant;
        TypeHandle type;
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
        TypeHandle type;
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
        TypeHandle type;
    };

    struct VariableExpr {
        std::string name;
        TypeHandle type;
    };

    struct AssigmentExpr {
        ExprHandle left;
        ExprHandle right;
        TypeHandle type;
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
        TypeHandle type;
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
        std::map<Const, std::string> cases; // will be resolved in semantic analysis
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
        TypeHandle type;
    };

    struct CastExpr {
        TypeHandle target;
        ExprHandle expr;
        TypeHandle type;
    };

    struct DereferenceExpr {
        ExprHandle expr;
        TypeHandle type;
    };

    struct AddressOfExpr {
        ExprHandle expr;
        TypeHandle type;
    };

    // used only while desugaring
    // assume last expr returns
    struct CompoundExpr {
        std::vector<ExprHandle> exprs;
        TypeHandle type;
    };
    // also only used while desugaring
    struct TemporaryExpr {
        std::string identifier;
        ExprHandle init;
        TypeHandle type; // note that it doesn't actually have any type
    };

    struct SubscriptExpr {
        ExprHandle expr;
        ExprHandle index;
        TypeHandle type;
    };
}

#endif //AST_H
