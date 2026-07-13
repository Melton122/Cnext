#ifndef CODEGEN_H
#define CODEGEN_H

#include "ast.h"
#include "sourcemap.h"
#include <stdio.h>

bool generate_c_code(ASTNode* program, const char* output_filename, bool test_mode);
void reset_codegen_state(void);
void set_codegen_profile_mode(bool enabled);
void set_codegen_sourcemap(SourceMap* map);

#endif // CODEGEN_H
