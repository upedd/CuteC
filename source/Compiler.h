#ifndef COMPILER_H
#define COMPILER_H
#include <iostream>
#include <ostream>
#include <string>

#include "AstPrinter.h"
#include "CodeEmitter.h"
#include "Codegen.h"
#include "Lexer.h"
#include "Parser.h"


class Compiler {
public:
    std::optional<std::string> compile(const std::string& input) {
        Lexer lexer(input);
        lexer.lex();
        if (!lexer.errors.empty()) {
            std::cerr << "lexing failed!" << '\n';
            for (const auto& error : lexer.errors) {
                std::cerr << error.message << '\n';
            }
            return {};
        }


        // for (const auto& token: lexer.tokens) {
        //     std::cout << token << '\n';
        // }
        Parser parser(lexer.tokens);
        parser.parse();
        if (!parser.errors.empty()) {
            std::cerr << "parsing failed!" << '\n';
            for (const auto& error : parser.errors) {
                std::cerr << error.message << '\n';
            }
            return {};
        }

        AstPrinter printer;
        printer.program(parser.program);

        Codegen codegen(std::move(parser.program));
        codegen.generate();

        CodeEmitter emitter(std::move(codegen.asmProgram));
        emitter.emit();

        // stub
        return emitter.assembly;
    }


    bool only_lex = false;
    bool only_parse = false;
    bool only_codegen = false;

};



#endif //COMPILER_H
