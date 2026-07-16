#ifndef OPTIMIZER_H
#define OPTIMIZER_H

#include "ast.h"
#include <stdbool.h>

bool optimize_ast(ASTNode* program);
int get_optimization_count(void);

#endif // OPTIMIZER_H
