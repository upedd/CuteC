cmake_minimum_required(VERSION 3.29)
set(CMAKE_OSX_ARCHITECTURES x86_64)

project(CuteC)

set(CMAKE_CXX_STANDARD 23)

add_executable(CuteC source/main.cpp
        source/Compiler.cpp
        source/Compiler.h
        source/Lexer.cpp
        source/Lexer.h
        source/Token.h
        source/Ast.h
        source/Parser.cpp
        source/Parser.h
        source/AstPrinter.h
        source/overloaded.h
        source/AsmTree.h
        source/Codegen.cpp
        source/Codegen.h
        source/CodeEmitter.cpp
        source/CodeEmitter.h
        source/IR.h
        source/IRGenerator.cpp
        source/IRGenerator.h
        source/IRPrinter.h
        source/analysis/VariableResolutionPass.cpp
        source/analysis/VariableResolutionPass.h)
