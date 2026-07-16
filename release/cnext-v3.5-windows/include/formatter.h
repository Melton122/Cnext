#ifndef FORMATTER_H
#define FORMATTER_H

#include <stdbool.h>

bool format_file(const char* input_path, const char* output_path, bool in_place);
bool format_source(const char* source, char** output);

#endif // FORMATTER_H
