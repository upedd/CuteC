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
        GREATER_EQUAL,
        END,
        PLUS_EQUAL,
        MINUS_EQUAL,
        ASTERISK_EQUAL,
        SLASH_EQUAL,
        PERCENT_EQUAL,
        AND_EQUAL,
        BAR_EQUAL,
        CARET_EQUAL,
        LESS_LESS_EQUAL,
        GREATER_GREATER_EQUAL,
        PLUS_PLUS,
        IF,
        ELSE,
        QUESTION_MARK,
        COLON,
        GOTO,
        DO,
        WHILE,
        FOR,
        BREAK,
        CONTINUE,
        SWITCH,
        CASE,
        DEFAULT,
        COMMA,
        STATIC,
        EXTERN,
        LONG,
        LONG_CONSTANT,
        UNSIGNED,
        SIGNED,
        UNSIGNED_INT_CONSTANT,
        UNSIGNED_LONG_CONSTANT,
        DOUBLE,
        FLOATING_POINT_CONSTANT
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
            case Type::END:
                return "End";
            case Type::PLUS_EQUAL:
                return "PlusEqual";
            case Type::MINUS_EQUAL:
                return "MinusEqual";
            case Type::ASTERISK_EQUAL:
                return "AsteriskEqual";
            case Type::SLASH_EQUAL:
                return "SlashEqual";
            case Type::PERCENT_EQUAL:
                return "PercentEqual";
            case Type::AND_EQUAL:
                return "AndEqual";
            case Type::BAR_EQUAL:
                return "BarEqual";
            case Type::CARET_EQUAL:
                return "CaretEqual";
            case Type::LESS_LESS_EQUAL:
                return "LessLessEqual";
            case Type::GREATER_GREATER_EQUAL:
                return "GreaterGreaterEqual";
            case Type::PLUS_PLUS:
                return "PlusPlus";
            case Type::IF:
                return "If";
            case Type::ELSE:
                return "Else";
            case Type::QUESTION_MARK:
                return "QuestionMark";
            case Type::COLON:
                return "Colon";
            case Type::GOTO:
                return "Goto";
            case Type::DO:
                return "Do";
            case Type::WHILE:
                return "While";
            case Type::FOR:
                return "For";
            case Type::BREAK:
                return "Break";
            case Type::CONTINUE:
                return "Continue";
            case Type::SWITCH:
                return "Switch";
            case Type::CASE:
                return "Case";
            case Type::DEFAULT:
                return "Default";
            case Type::COMMA:
                return "Comma";
            case Type::STATIC:
                return "Static";
            case Type::EXTERN:
                return "Extern";
            case Type::LONG:
                return "Long";
            case Type::LONG_CONSTANT:
                return "LongConstant";
            case Type::UNSIGNED:
                return "Unsigned";
            case Type::SIGNED:
                return "Signed";
            case Type::UNSIGNED_INT_CONSTANT:
                return "UnsignedIntegerConstant";
            case Type::UNSIGNED_LONG_CONSTANT:
                return "UnsignedLongConstant";
            case Type::DOUBLE:
                return "Double";
            case Type::FLOATING_POINT_CONSTANT:
                return "FloatingPointConstant";
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
