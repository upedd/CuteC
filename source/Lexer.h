#ifndef LEXER_H
#define LEXER_H
#include <string>
#include <utility>
#include <vector>

#include "Token.h"


class Lexer {
public:
    class Error {
    public:
        std::string message;
    };

    Lexer(std::string source) : m_source(std::move(source)) {
    };

    void make_constant();

    void make_keyword_or_identifier();

    void lex();

    std::vector<Token> tokens;
    std::vector<Error> errors;

private:
    char consume();

    char peek();

    bool match(char c);

    bool match_insensitive(char c);

    bool at_end();

    void skip_whitespace();

    bool is_whitespace(char c);

    bool is_digit(char c);

    bool is_valid_identifier(char c);

    void make_token(Token::Type type);

    std::string m_source;
    int m_pos = 0;
    int m_start = 0;
    // used for debugging
    int m_line = 1;
    int m_linepos = 0;
};


#endif //LEXER_H
