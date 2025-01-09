#ifndef TYPECHECKERPASS_H
#define TYPECHECKERPASS_H

#include <cstdint>
#include <iostream>

#include "../overloaded.h"

#include "../Ast.h"

struct InitialInt {
    int value;
};

struct InitialLong {
    std::int64_t value;
};

struct InitialUInt {
    unsigned int value;
};

struct InitialULong {
    std::uint64_t value;
};

struct InitialDouble {
    double value;
};

using Initial = std::variant<InitialInt, InitialLong, InitialUInt, InitialULong, InitialDouble>;

struct NoInitializer {};
struct Tentative {};

using InitialValue = std::variant<Tentative, Initial, NoInitializer>;

struct FunctionAttributes {
    bool defined;
    bool global;
};

struct StaticAttributes {
    InitialValue initial_value;
    bool global;
};

struct LocalAttributes {};

using IdentifierAttributes = std::variant<FunctionAttributes, StaticAttributes, LocalAttributes>;
inline AST::TypeHandle get_type(const AST::Expr &expr) {
    return std::visit(overloaded {
        [](const auto& expr) {
            return expr.type;
        }
    }, expr);
}


inline void convert_to(AST::ExprHandle& expr, const AST::TypeHandle& type) {
    if (get_type(*expr)->index() == type->index()) return;
    auto cast = AST::CastExpr(type, std::move(expr));
    cast.type = type;
    expr = std::make_unique<AST::Expr>(std::move(cast));
}

inline int get_size_for_type(const AST::TypeHandle& type) {
    return std::visit(overloaded {
        [](const AST::IntType&) {
            return 4;
        },
        [](const AST::UIntType&) {
            return 4;
        },
        [](const AST::LongType&) {
            return 8;
        },
        [](const AST::ULongType&) {
            return 8;
        },
        [](const AST::PointerType&) {
            return 8;
        },
        [](const auto&) {
            return 0; //?
        }
    }, *type);
}


inline bool is_unsigned(const AST::TypeHandle& type) {
    return std::holds_alternative<AST::UIntType>(*type) || std::holds_alternative<AST::ULongType>(*type);
}

struct Symbol {
    AST::TypeHandle type;
    IdentifierAttributes attributes;
};

class TypeCheckerPass {
public:
    class Error {
    public:
        std::string message;
    };

    explicit TypeCheckerPass(AST::Program *program) : program(program) {
    };

    void check_block(std::vector<AST::BlockItem> &block);

    void file_scope_variable_declaration(AST::VariableDecl &decl);

    void local_variable_declaration(AST::VariableDecl &decl);

    void check_function_decl(AST::FunctionDecl &function);

    void check_program(AST::Program &program);

    void run();

    void check_decl(AST::DeclHandle &item);

    void check_expr_and_convert(AST::ExprHandle &expr);

    void check_expr(AST::Expr &expr);


    void check_function_call(AST::FunctionCall &expr);

    void check_variable_expr(AST::VariableExpr &expr);

    void check_stmt(AST::Stmt &item);

    void check_constant_expr(AST::ConstantExpr &expr);

    void check_cast_expr(AST::CastExpr &expr);

    void check_unary( AST::UnaryExpr & expr);

    AST::TypeHandle get_common_pointer_type(const AST::ExprHandle &lhs, const AST::ExprHandle &rhs);

    void check_binary(AST::BinaryExpr &expr);

    void convert_by_assigment(AST::ExprHandle &expr, AST::TypeHandle target);

    void check_assigment(AST::AssigmentExpr & expr);

    void check_conditional(AST::ConditionalExpr & expr);

    void check_dereference(AST::DereferenceExpr & expr);

    void check_address_of(AST::AddressOfExpr & expr);

    std::vector<Error> errors;

    std::unordered_map<std::string, Symbol> symbols;

private:
    std::vector<AST::TypeHandle> current_function_return_type;
    AST::Program *program;
};


#endif //TYPECHECKERPASS_H
