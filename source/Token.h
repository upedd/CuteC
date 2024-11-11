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
        TILDE,
        PLUS,
        ASTERISK,
        FORWARD_SLASH,
        PERCENT,
        AND,
        AND_AND,
        BAR,
        BAR_BAR,
        LESS,
        LESS_LESS,
        GREATER,
        GREATER_GREATER,
        CARET,
        BANG,
        EQUAL,
        EQUAL_EQUAL,
        BANG_EQUAL,
        LESS_EQUAL,
        GREATER_EQUAL
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
            case Type::AND:
                return "And";
            case Type::AND_AND:
                return "AndAnd";
            case Type::BAR:
                return "Bar";
            case Type::BAR_BAR:
                return "BarBar";
            case Type::LESS:
                return "Less";
            case Type::LESS_LESS:
                return "LessLess";
            case Type::GREATER:
                return "Greater";
            case Type::GREATER_GREATER:
                return "GreaterGreater";
            case Type::CARET:
                return "Caret";
            case Type::BANG:
                return "Bang";
            case Type::EQUAL:
                return "Equal";
            case Type::EQUAL_EQUAL:
                return "EqualEqual";
            case Type::BANG_EQUAL:
                return "BangEqual";
            case Type::LESS_EQUAL:
                return "LessEqual";
            case Type::GREATER_EQUAL:
                return "GreaterEqual";
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
