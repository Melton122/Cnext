#ifndef DOCGEN_H
#define DOCGEN_H

#include <stdbool.h>

bool generate_docs(const char* file_path, const char* output_dir);
bool generate_docs_from_source(const char* source, const char* name, char** output);

#endif // DOCGEN_H
