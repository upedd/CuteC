#include "Lexer.h"

#include <format>

#include "IR.h"

void Lexer::make_constant(bool fractional) {
    bool is_floating = fractional;
    while (is_digit(peek())) {
        consume();
    }


    if (!fractional && match('.')) {
        is_floating = true;
        while (is_digit(peek())) {
            consume();
        }
    }

    if (match_insensitive('e')) {
        is_floating = true;
        match('+') || match('-');
        if (!is_digit(peek())) {
            errors.emplace_back(std::format("Missing exponent: '{}' at {}:{}", peek(), m_line,
                                        m_linepos));
        }

        while (is_digit(peek())) {
            consume();
        }
    }

    if (is_floating) {
        if (is_invalid_after_constant(peek())) {
            errors.emplace_back(std::format("Unexpected character in number literal: '{}' at {}:{}", peek(), m_line,
                                        m_linepos));
        }
        tokens.emplace_back(Token::Type::FLOATING_POINT_CONSTANT, Token::Position(m_line, m_linepos),
                        m_source.substr(m_start, m_pos - m_start));
        return;
    }


    bool is_long = false;
    bool is_unsigned = false;
    if (match_insensitive('l')) {
        is_long = true;
    } else if (match_insensitive('u')) {
        is_unsigned = true;
        if (match_insensitive('l')) {
            is_long = true;
        }
    }

    if (is_valid_identifier(peek())) {
        errors.emplace_back(std::format("Unexpected character in number literal: '{}' at {}:{}", peek(), m_line,
                                        m_linepos));
    }
    Token::Type type;
    if (is_long) {
        if (is_unsigned) {
            type = Token::Type::UNSIGNED_LONG_CONSTANT;
        } else {
            type = Token::Type::LONG_CONSTANT;
        }
    } else {
        if (is_unsigned) {
            type = Token::Type::UNSIGNED_INT_CONSTANT;
        } else {
            type = Token::Type::CONSTANT;
        }
    }

    tokens.emplace_back(type, Token::Position(m_line, m_linepos),
                        m_source.substr(m_start, m_pos - m_start - is_long - is_unsigned));
}

void Lexer::make_keyword_or_identifier() {
    while (is_valid_identifier(peek())) {
        consume();
    }
    if (is_digit(peek())) {
        errors.emplace_back(std::format("Unexpected character in identifer: '{}' at {}:{}", peek(), m_line, m_linepos));
    }

    std::string lexeme = m_source.substr(m_start, m_pos - m_start);
    if (lexeme == "return") {
        make_token(Token::Type::RETURN);
    } else if (lexeme == "int") {
        make_token(Token::Type::INT);
    } else if (lexeme == "void") {
        make_token(Token::Type::VOID);
    } else if (lexeme == "if") {
        make_token(Token::Type::IF);
    } else if (lexeme == "else") {
        make_token(Token::Type::ELSE);
    } else if (lexeme == "goto") {
        make_token(Token::Type::GOTO);
    } else if (lexeme == "do") {
        make_token(Token::Type::DO);
    } else if (lexeme == "while") {
        make_token(Token::Type::WHILE);
    } else if (lexeme == "for") {
        make_token(Token::Type::FOR);
    } else if (lexeme == "break") {
        make_token(Token::Type::BREAK);
    } else if (lexeme == "continue") {
        make_token(Token::Type::CONTINUE);
    } else if (lexeme == "switch") {
        make_token(Token::Type::SWITCH);
    } else if (lexeme == "case") {
        make_token(Token::Type::CASE);
    } else if (lexeme == "default") {
        make_token(Token::Type::DEFAULT);
    } else if (lexeme == "extern") {
        make_token(Token::Type::EXTERN);
    } else if (lexeme == "static") {
        make_token(Token::Type::STATIC);
    } else if (lexeme == "long") {
        make_token(Token::Type::LONG);
    } else if (lexeme == "unsigned") {
        make_token(Token::Type::UNSIGNED);
    } else if (lexeme == "signed") {
        make_token(Token::Type::SIGNED);
    } else if (lexeme == "double") {
        make_token(Token::Type::DOUBLE);
    } else if (lexeme == "char") {
        make_token(Token::Type::CHAR);
    } else if (lexeme == "sizeof") {
        make_token(Token::Type::SIZEOF);
    } else {
        tokens.emplace_back(Token::Type::IDENTIFIER, Token::Position(m_line, m_linepos), lexeme);
    }
}

bool is_valid_after_escape(char c) {
    return c == '\'' || c == '"' || c == '?' || c == '\\' || c == 'a' || c == 'b' || c == 'f' || c == 'n' || c == 'r' || c == 't' || c == 'v';
}

void Lexer::make_char() {
    if (match('\\')) {
        if (!is_valid_after_escape(peek())) {
            errors.emplace_back("Invalid character after in escape sequence");
        }
    } else if (match('\n') || match('\'')) {
        errors.emplace_back("Invalid character in char constant");
    }
    consume(); //?
    if (!match('\'')) {
        errors.emplace_back("Multi-character char constants are unsupported");
    } // recover?
    std::string lexeme = m_source.substr(m_start, m_pos - m_start);
    tokens.emplace_back(Token::Type::CHAR_CONSTANT, Token::Position(m_line, m_linepos), lexeme);

}

void Lexer::make_string_literal() {
    char c;
    for (c = consume(); c != '\"' && c != '\0'; c = consume()) {
        if (c == '\\') {
            if (!is_valid_after_escape(peek())) {
                errors.emplace_back("Invalid character after in escape sequence");
            }
            consume();
        } else if (c == '\n') {
            errors.emplace_back("Invalid character in string constant");
        }
    }
    if (c != '\"') {
        errors.emplace_back("Unterminated string literal");
    }
    std::string lexeme = m_source.substr(m_start, m_pos - m_start);
    tokens.emplace_back(Token::Type::STRING_LITERAL, Token::Position(m_line, m_linepos), lexeme);
}


void Lexer::lex() {
    while (true) {
        skip_whitespace();
        if (at_end()) {
            break;
        }
        m_start = m_pos;
        switch (char c = consume(); c) {
            case '(':
                make_token(Token::Type::LEFT_PAREN);
                break;
            case ')':
                make_token(Token::Type::RIGHT_PAREN);
                break;
            case '{':
                make_token(Token::Type::LEFT_BRACE);
                break;
            case '}':
                make_token(Token::Type::RIGHT_BRACE);
                break;
            case ';':
                make_token(Token::Type::SEMICOLON);
                break;
            case '-':
                if (match('-')) {
                    make_token(Token::Type::MINUS_MINUS);
                } else if (match('=')) {
                    make_token(Token::Type::MINUS_EQUAL);
                } else {
                    make_token(Token::Type::MINUS);
                }
                break;
            case '&':
                if (match('&')) {
                    make_token(Token::Type::AND_AND);
                } else if (match('=')) {
                    make_token(Token::Type::AND_EQUAL);
                } else {
                    make_token(Token::Type::AND);
                }
                break;
            case '|':
                if (match('|')) {
                    make_token(Token::Type::BAR_BAR);
                } else if (match('=')) {
                    make_token(Token::Type::BAR_EQUAL);
                } else {
                    make_token(Token::Type::BAR);
                }
                break;
            case '<':
                if (match('<')) {
                    if (match('=')) {
                        make_token(Token::Type::LESS_LESS_EQUAL);
                    } else {
                        make_token(Token::Type::LESS_LESS);
                    }
                } else if (match('=')) {
                    make_token(Token::Type::LESS_EQUAL);
                } else {
                    make_token(Token::Type::LESS);
                }
                break;
            case '>':
                if (match('>')) {
                    if (match('=')) {
                        make_token(Token::Type::GREATER_GREATER_EQUAL);
                    } else {
                        make_token(Token::Type::GREATER_GREATER);
                    }
                } else if (match('=')) {
                    make_token(Token::Type::GREATER_EQUAL);
                } else {
                    make_token(Token::Type::GREATER);
                }
                break;
            case '~':
                make_token(Token::Type::TILDE);
                break;
            case '+':
                if (match('=')) {
                    make_token(Token::Type::PLUS_EQUAL);
                } else if (match('+')) {
                    make_token(Token::Type::PLUS_PLUS);
                } else {
                    make_token(Token::Type::PLUS);
                }
                break;
            case '/':
                if (match('=')) {
                    make_token(Token::Type::SLASH_EQUAL);
                } else {
                    make_token(Token::Type::FORWARD_SLASH);
                }
                break;
            case '*':
                if (match('=')) {
                    make_token(Token::Type::ASTERISK_EQUAL);
                } else {
                    make_token(Token::Type::ASTERISK);
                }
                break;
            case '%':
                if (match('=')) {
                    make_token(Token::Type::PERCENT_EQUAL);
                } else {
                    make_token(Token::Type::PERCENT);
                }
                break;
            case '^':
                if (match('=')) {
                    make_token(Token::Type::CARET_EQUAL);
                } else {
                    make_token(Token::Type::CARET);
                }
                break;
            case '!':
                if (match('=')) {
                    make_token(Token::Type::BANG_EQUAL);
                } else {
                    make_token(Token::Type::BANG);
                }
                break;
            case '=':
                if (match('=')) {
                    make_token(Token::Type::EQUAL_EQUAL);
                } else {
                    make_token(Token::Type::EQUAL);
                }
                break;
            case ':':
                make_token(Token::Type::COLON);
                break;
            case '?':
                make_token(Token::Type::QUESTION_MARK);
                break;
            case ',':
                make_token(Token::Type::COMMA);
                break;
            case '[':
                make_token(Token::Type::LEFT_BRACKET);
                break;
            case ']':
                make_token(Token::Type::RIGHT_BRACKET);
                break;
            case '.':
                if (is_digit(peek())) {
                    make_constant(true);
                } else {
                    std::unreachable(); // TODO
                }
                break;
            case '\'':
                make_char();
                break;
            case '"':
                make_string_literal();
                break;
            default: {
                if (is_digit(c)) {
                    make_constant();
                } else if (is_valid_identifier(c)) {
                    make_keyword_or_identifier();
                } else {
                    errors.emplace_back(std::format("Unexpected character: '{}' at {}:{}", c, m_line, m_linepos));
                }
            }
        }
    }

    make_token(Token::Type::END);
}
char Lexer::consume() {
    if (at_end()) {
        return '\0';
    }
    m_linepos++;
    return m_source[m_pos++];
}

char Lexer::peek() {
    if (at_end()) {
        return '\0';
    }
    return m_source[m_pos];
}

bool Lexer::match(char c) {
    if (peek() == c) {
        consume();
        return true;
    }
    return false;
}

bool Lexer::match_insensitive(char c) {
    // assert c is in lowercase
    if (peek() == c || peek() == c - 32) {
        consume();
        return true;
    }
    return false;
}

bool Lexer::at_end() {
    return m_pos >= m_source.size();
}

void Lexer::skip_whitespace() {
    while (is_whitespace(peek())) {
        if (char c = consume(); c == '\n') {
            m_line++;
            m_linepos = 0;
        }
    }
}

bool Lexer::is_whitespace(char c) {
    return c == ' ' || c == '\n' || c == '\t' || c == '\r';
}

bool Lexer::is_digit(char c) {
    return c >= '0' && c <= '9';
}

bool Lexer::is_valid_identifier(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || (c >= '0' && c <= '9');
}

bool Lexer::is_invalid_after_constant(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '.';
}

void Lexer::make_token(Token::Type type) {
    tokens.emplace_back(type, Token::Position(m_line, m_linepos));
}
