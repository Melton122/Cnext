#ifndef CODEGEN_H
#define CODEGEN_H

#include "ast.h"
#include <stdio.h>

bool generate_c_code(ASTNode* program, const char* output_filename, bool test_mode);

#endif // CODEGEN_H
