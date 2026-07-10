#ifndef SEMANTICS_H
#define SEMANTICS_H

#include "ast.h"
#include <stdbool.h>

bool analyze_semantics(ASTNode* program, bool require_main);

#endif // SEMANTICS_H
