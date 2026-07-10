#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include <stdbool.h>

ASTNode* parse_program(const char* source);

#endif // PARSER_H
