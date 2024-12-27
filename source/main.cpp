#include <iostream>
#include <filesystem>
#include <format>
#include <sstream>
#include <fstream>

#include "Compiler.h"


int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <filename>" << std::endl;
    }

    std::string file_path_string;

    Compiler compiler;
    bool generate_object_file = false;
    bool generate_asm = false;
    std::string args_to_assembler;
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--lex") {
            compiler.only_lex = true;
        } else if (std::string(argv[i]) == "--parse") {
            compiler.only_parse = true;
        } else if (std::string(argv[i]) == "--codegen") {
            compiler.only_codegen = true;
        } else if (std::string(argv[i]) == "-S") {
            generate_asm = true;
        } else if (std::string(argv[i]) == "--tacky") {
            compiler.only_ir = true;
        } else if (std::string(argv[i]) == "--validate") {
            compiler.only_analysis = true;
        } else if (std::string(argv[i]) == "-c") {
            generate_object_file = true;
        } else if (std::string(argv[i]).starts_with("-l")) {
            args_to_assembler += std::string(argv[i]) + " ";
        } else {
            file_path_string = argv[i];
        }
    }

    std::filesystem::path file_path = file_path_string;
    auto preprocessed_path = std::filesystem::path(file_path).replace_extension(".i");
    system(std::format("gcc -E -P {} -o {}", file_path.string(), preprocessed_path.string()).c_str());

    std::ifstream input_file(preprocessed_path);
    std::stringstream stream;
    stream << input_file.rdbuf();
    std::string content = stream.str();

    std::filesystem::remove(preprocessed_path);

    auto compiled = compiler.compile(content);

    if (!compiled) {
        return -1;
    }

    if (compiler.only_lex || compiler.only_parse || compiler.only_codegen || compiler.only_ir) {
        return 0;
    }
    auto assembly_path = std::filesystem::path(file_path).replace_extension(".s");
    std::ofstream assembly_file(assembly_path);
    assembly_file << *compiled;
    assembly_file.close();
    if (generate_asm) {
        return 0;
    }

    if (generate_object_file) {
        auto output_path = std::filesystem::path(file_path).replace_extension(".o");
        // temp arch only in macOS?
#if __APPLE__
        system(std::format("arch -x86_64 gcc -c {} -o {}", assembly_path.string(), output_path.string()).c_str());
#else
        system(std::format("gcc -c {} -o {}", assembly_path.string(), output_path.string()).c_str());
#endif
    } else {
        auto output_path = std::filesystem::path(file_path).replace_extension("");
        // temp arch only in macOS?
#if __APPLE__
        system(std::format("arch -x86_64 gcc {} -o {} {}", assembly_path.string(), output_path.string(), args_to_assembler).c_str());
#else
        system(std::format("gcc {} -o {} {}", assembly_path.string(), output_path.string(), args_to_assembler).c_str());

#endif
    }

    std::filesystem::remove(assembly_path);

    return 0;
}
