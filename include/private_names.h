#ifndef PRIVATE_NAMES_H
#define PRIVATE_NAMES_H

/* Per-file visibility for local imports.

   Every file in a program that uses `import "./..."` gets its own copy of this
   rewrite: `export` is removed from top-level declarations, and the names of
   top-level declarations that are *not* exported are suffixed with the file's
   module tag (`helper` -> `helper__m3`). Suffixed names live only inside the
   merged translation unit, so two files may each define a private `helper`
   without colliding, and a reference from another file no longer finds the
   private declaration.

   `module_tag` < 0 keeps the original names and only removes `export` (used
   when a program has no local imports, where visibility cannot matter).
   Line numbers are preserved: replacements never add or remove newlines. */
char* module_privatize_source(const char* source, int module_tag);

/* True when `source` contains a top-level `import "./file.cn"` declaration
   (comments and string literals are ignored). */
int source_has_local_import(const char* source);

#endif /* PRIVATE_NAMES_H */
