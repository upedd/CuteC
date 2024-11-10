#ifndef COMPILER_H
#define COMPILER_H
#include <iostream>
#include <ostream>
#include <string>

#include "AstPrinter.h"
#include "CodeEmitter.h"
#include "Codegen.h"
#include "IRGenerator.h"
#include "Lexer.h"
#include "Parser.h"


class Compiler {
public:
    std::optional<std::string> compile(const std::string &input) {
        Lexer lexer(input);
        lexer.lex();
        if (!lexer.errors.empty()) {
            std::cerr << "lexing failed!" << '\n';
            for (const auto &error: lexer.errors) {
                std::cerr << error.message << '\n';
            }
            return {};
        }
        if (only_lex) {
            return "";
        }

        Parser parser(lexer.tokens);
        parser.parse();
        if (!parser.errors.empty()) {
            std::cerr << "parsing failed!" << '\n';
            for (const auto &error: parser.errors) {
                std::cerr << error.message << '\n';
            }
            return {};
        }

        AstPrinter printer;
        printer.program(parser.program);
        if (only_parse) {
            return "";
        }

        IRGenerator generator(std::move(parser.program));
        generator.generate();
        if (only_ir) {
            return "";
        }

        codegen::IRToAsmTreePass ir_to_asm_tree_pass(std::move(generator.IRProgram));
        ir_to_asm_tree_pass.convert();
        auto &asm_tree = ir_to_asm_tree_pass.asmProgram;
        codegen::ReplacePseudoRegistersPass replace_pseudo_registers_pass(&asm_tree);
        replace_pseudo_registers_pass.process();

        int max_offset = replace_pseudo_registers_pass.offset;

        codegen::FixUpInstructionsPass fix_up_instructions_pass(&asm_tree, max_offset);
        fix_up_instructions_pass.process();
        if (only_codegen) {
            return "";
        }

        CodeEmitter emitter(std::move(asm_tree));
        emitter.emit();

        return emitter.assembly;
    }

    bool only_lex = false;
    bool only_parse = false;
    bool only_codegen = false;
    bool only_ir = false;
};


#endif //COMPILER_H
