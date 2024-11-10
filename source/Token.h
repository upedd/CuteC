#ifndef TOKEN_H
#define TOKEN_H
#include <format>
#include <string>
#include <utility>

class Token {
public:
    class Position {
    public:
        int line;
        int offset; // offset from line beginning
    };

    enum class Type {
        IDENTIFIER,
        CONSTANT,
        INT,
        VOID,
        RETURN,
        LEFT_PAREN,
        RIGHT_PAREN,
        LEFT_BRACE,
        RIGHT_BRACE,
        SEMICOLON,
        MINUS,
        MINUS_MINUS,
        TILDE
    };

    static std::string type_to_string(Type type) {
        switch (type) {
            case Type::IDENTIFIER:
                return "Identifier";
            case Type::CONSTANT:
                return "Constant";
            case Type::INT:
                return "Int";
            case Type::VOID:
                return "Void";
            case Type::RETURN:
                return "Return";
            case Type::LEFT_PAREN:
                return "LeftParen";
            case Type::RIGHT_PAREN:
                return "RightParen";
            case Type::LEFT_BRACE:
                return "LeftBrace";
            case Type::RIGHT_BRACE:
                return "RightBrace";
            case Type::SEMICOLON:
                return "Semicolon";
            case Type::MINUS:
                return "Minus";
            case Type::MINUS_MINUS:
                return "MinusMinus";
            case Type::TILDE:
                return "Tilde";
        }
    }

    Token(Type type, Position position, std::string lexeme = {}) : type(type), position(position),
                                                                   lexeme(std::move(lexeme)) {
    }

    Type type;
    Position position;
    std::string lexeme; // possibly intern
};

inline std::ostream &operator<<(std::ostream &os, const Token &token) {
    os << std::format("Token(type={}, position=(line: {}, offset: {}), lexeme={})", Token::type_to_string(token.type),
                      token.position.line, token.position.offset, token.lexeme);
    return os;
}

#endif //TOKEN_H
