#ifndef TYPECHECKERPASS_H
#define TYPECHECKERPASS_H

#include <array>
#include <array>
#include <cstdint>
#include <iostream>
#include <utility>

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

struct InitialZero {
    int bytes;
};

struct InitialChar {
    char value;
};

struct InitialUChar {
    unsigned char value;
};

struct InitialString {
    std::string value;
    bool null_terminated;
};

struct InitialPointer {
    std::string name;
};

using InitialElement = std::variant<InitialInt, InitialLong, InitialUInt, InitialULong, InitialDouble, InitialZero,
    InitialChar, InitialUChar, InitialString, InitialPointer>;

using Initial = std::vector<InitialElement>;

struct NoInitializer {
};

struct Tentative {
};

using InitialValue = std::variant<Tentative, Initial, NoInitializer>;

struct FunctionAttributes {
    bool defined;
    bool global;
};

struct StaticAttributes {
    InitialValue initial_value;
    bool global;
};

struct ConstantAttributes {
    Initial init;
};

struct LocalAttributes {
};

using IdentifierAttributes = std::variant<FunctionAttributes, StaticAttributes, ConstantAttributes, LocalAttributes>;

inline AST::TypeHandle get_type(const AST::Expr &expr) {
    return std::visit(overloaded{
                          [](const auto &expr) {
                              return expr.type;
                          }
                      }, expr);
}

inline std::uint64_t bytes_for_type(const AST::Type &type) {
    return std::visit(overloaded{
                          [](const AST::IntType &)-> std::uint64_t {
                              return 4;
                          },
                          [](const AST::UIntType &)-> std::uint64_t {
                              return 4;
                          },
                          [](const AST::LongType &)-> std::uint64_t {
                              return 8;
                          },
                          [](const AST::ULongType &)-> std::uint64_t {
                              return 8;
                          },
                          [](const AST::PointerType &)-> std::uint64_t {
                              return 8;
                          },
                          [](const AST::DoubleType &)-> std::uint64_t {
                              return 8;
                          },
                          [](const AST::CharType &)-> std::uint64_t {
                              return 1;
                          },
                          [](const AST::UCharType &)-> std::uint64_t {
                              return 1;
                          },
                          [](const AST::SignedCharType &)-> std::uint64_t {
                              return 1;
                          },
                          [](const AST::ArrayType &array) -> std::uint64_t {
                              return bytes_for_type(*array.element_type) * array.size;
                          },
                          [](const auto &) -> std::uint64_t {
                              std::unreachable();
                          }
                      }, type);
}

inline void convert_to(AST::ExprHandle &expr, const AST::TypeHandle &type) {
    if (get_type(*expr)->index() == type->index()) return;
    auto cast = AST::CastExpr(type, std::move(expr));
    cast.type = type;
    expr = std::make_unique<AST::Expr>(std::move(cast));
}

inline int get_size_for_type(const AST::Type &type) {
    return std::visit(overloaded{
                          [](const AST::IntType &) {
                              return 4;
                          },
                          [](const AST::UIntType &) {
                              return 4;
                          },
                          [](const AST::LongType &) {
                              return 8;
                          },
                          [](const AST::ULongType &) {
                              return 8;
                          },
                          [](const AST::DoubleType &) {
                              return 8;
                          },
                          [](const AST::PointerType &) {
                              return 8;
                          },
                          [](const AST::CharType &) {
                              return 1;
                          },
                          [](const AST::UCharType &) {
                              return 1;
                          },
                          [](const AST::SignedCharType &) {
                              return 1;
                          },
                          [](const auto &) {
                              return 0; //?
                          }
                      }, type);
}


inline bool is_unsigned(const AST::TypeHandle &type) {
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

    Initial convert_scalar_to_initial(const AST::Type &target_type, AST::ScalarInit &init);

    void convert_compound_to_initial(Initial &initial, const AST::Type &target_type, AST::CompoundInit &compound);

    void file_scope_variable_declaration(AST::VariableDecl &decl);

    void local_variable_declaration(AST::VariableDecl &decl);

    void check_init(const AST::TypeHandle &target_type, AST::Initializer &init);

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

    void check_unary(AST::UnaryExpr &expr);

    AST::TypeHandle get_common_pointer_type(const AST::ExprHandle &lhs, const AST::ExprHandle &rhs);

    void check_binary(AST::BinaryExpr &expr);

    void convert_by_assigment(AST::ExprHandle &expr, AST::TypeHandle target);

    void check_assigment(AST::AssigmentExpr &expr);

    void check_conditional(AST::ConditionalExpr &expr);

    void check_dereference(AST::DereferenceExpr &expr);

    void check_address_of(AST::AddressOfExpr &expr);

    void check_subscript(AST::SubscriptExpr &expr);

    AST::InitializerHandle zero_initializer(const AST::Type &type);

    void check_string(AST::StringExpr &expr);

    void check_type(const AST::Type &type);

    std::vector<Error> errors;

    std::unordered_map<std::string, Symbol> symbols;

private:
    int cnt = 0;
    std::vector<AST::TypeHandle> current_function_return_type;
    AST::Program *program;
};


#endif //TYPECHECKERPASS_H
