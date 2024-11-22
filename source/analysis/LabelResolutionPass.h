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

 void resolve_stmt(const AST::Stmt &stmt);

 void resolve_function(const AST::Function &function);

 void resolve_program(const AST::Program &program);

 void run();

 void resolve_goto(const AST::GoToStmt &stmt);

 void resolve_labeled(const AST::LabeledStmt &stmt);


 std::vector<Error> errors;

private:
 std::unordered_set<std::string> labels;
 std::unordered_set<std::string> unresolved_labels;
 AST::Program *program;
};


#endif //LABELRESOLUTIONPASS_H
