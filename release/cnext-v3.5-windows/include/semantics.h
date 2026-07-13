#ifndef SEMANTICS_H
#define SEMANTICS_H

#include "ast.h"
#include <stdbool.h>

bool analyze_semantics(ASTNode* program, bool require_main);
void reset_semantics_state(void);

#endif // SEMANTICS_H
