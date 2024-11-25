#ifndef LABELRESOLUTIONPASS_H
#define LABELRESOLUTIONPASS_H
#include <unordered_map>
#include <unordered_set>

#include "../Ast.h"


/**
 * Resolves goto labels
 * TODO: when functions will get added this will need to be reworked!
 */
class LabelResolutionPass {
public:
 class Error {
 public:
  std::string message;
 };

 LabelResolutionPass(AST::Program *program) : program(program) {
 }

 void resolve_stmt(AST::Stmt &stmt);

 void resolve_function(AST::FunctionDecl &function);

 void resolve_block(std::vector<AST::BlockItem> &block);

 void resolve_program(AST::Program &program);

 void run();

 void resolve_goto(AST::GoToStmt &stmt);

 void resolve_labeled(AST::LabeledStmt &stmt);


 std::vector<Error> errors;

private:
 std::string current_function_name;
 std::unordered_map<std::string, std::string> labels;
 std::unordered_set<std::string> unresolved_labels;
 AST::Program *program;
};


#endif //LABELRESOLUTIONPASS_H
