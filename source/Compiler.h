#ifndef COMPILER_H
#define COMPILER_H
#include <iostream>
#include <ostream>
#include <string>

#include "AstPrinter.h"
#include "CodeEmitter.h"
#include "Codegen.h"
#include "IRGenerator.h"
#include "IRPrinter.h"
#include "Lexer.h"
#include "Parser.h"
#include "analysis/LabelResolutionPass.h"
#include "analysis/LoopLabelingPass.h"
#include "analysis/SwitchResolutionPass.h"
#include "analysis/TypeCheckerPass.h"
#include "analysis/VariableResolutionPass.h"


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

        VariableResolutionPass variable_resolution_pass(&parser.program);
        variable_resolution_pass.run();
        if (!variable_resolution_pass.errors.empty()) {
            std::cerr << "variable resolution failed!" << '\n';
            for (const auto &error: variable_resolution_pass.errors) {
                std::cerr << error.message << '\n';
            }
            return {};
        }

        TypeCheckerPass type_checker(&parser.program);\
        type_checker.run();
        if (!type_checker.errors.empty()) {
            std::cerr << "type check failed!" << '\n';
            for (const auto &error: type_checker.errors) {
                std::cerr << error.message << '\n';
            }
            return {};
        }

        LabelResolutionPass label_resolution_pass(&parser.program);
        label_resolution_pass.run();
        if (!label_resolution_pass.errors.empty()) {
            std::cerr << "label resolution failed!" << '\n';
            for (const auto &error: label_resolution_pass.errors) {
                std::cerr << error.message << '\n';
            }
            return {};
        }

        LoopLabelingPass loop_labeling_pass(&parser.program);
        loop_labeling_pass.run();
        if (!loop_labeling_pass.errors.empty()) {
            std::cerr << "loop labeling failed!" << '\n';
            for (const auto &error: loop_labeling_pass.errors) {
                std::cerr << error.message << '\n';
            }
            return {};
        }

        SwitchResolutionPass switch_resolution_pass(&parser.program);
        switch_resolution_pass.run();
        if (!switch_resolution_pass.errors.empty()) {
            std::cerr << "switch resolution failed!" << '\n';
            for (const auto &error: switch_resolution_pass.errors) {
                std::cerr << error.message << '\n';
            }
            return {};
        }

        if (only_analysis) {
            return "";
        }

        IRGenerator generator(std::move(parser.program));
        generator.generate();

        IRPrinter ir_printer(&generator.IRProgram);
        ir_printer.print();
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

        CodeEmitter emitter(std::move(asm_tree), &type_checker.symbols);
        emitter.emit();

        return emitter.assembly;
    }

    bool only_lex = false;
    bool only_parse = false;
    bool only_codegen = false;
    bool only_ir = false;
    bool only_analysis = false;
};


#endif //COMPILER_H
