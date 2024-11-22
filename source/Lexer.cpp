#include "Lexer.h"

#include <format>

#include "IR.h"

void Lexer::make_constant() {
    while (is_digit(peek())) {
        consume();
    }
    if (is_valid_identifier(peek())) {
        errors.emplace_back(std::format("Unexpected character in number literal: '{}' at {}:{}", peek(), m_line,
                                        m_linepos));
    }

    tokens.emplace_back(Token::Type::CONSTANT, Token::Position(m_line, m_linepos),
                        m_source.substr(m_start, m_pos - m_start));
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
    } else {
        tokens.emplace_back(Token::Type::IDENTIFIER, Token::Position(m_line, m_linepos), lexeme);
    }
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

void Lexer::make_token(Token::Type type) {
    tokens.emplace_back(type, Token::Position(m_line, m_linepos));
}
