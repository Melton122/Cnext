# Cnext EBNF Grammar

```ebnf
program        ::= { statement }

statement      ::= class_decl
                 | func_decl
                 | main_block
                 | var_decl
                 | assign_stmt
                 | if_stmt
                 | while_stmt
                 | for_stmt
                 | return_stmt
                 | expr_stmt
                 | block

block          ::= "{" { statement } "}"

class_decl     ::= "class" IDENTIFIER "{" { var_decl } "}"

func_decl      ::= "func" IDENTIFIER "(" [ param_list ] ")" [ "->" type ] block

main_block     ::= "main" block

param_list     ::= param { "," param }
param          ::= type IDENTIFIER

var_decl       ::= type IDENTIFIER [ "=" expr ] [ ";" ]

assign_stmt    ::= IDENTIFIER ( "=" | "+=" | "-=" | "*=" | "/=" ) expr [ ";" ]

if_stmt        ::= "if" expr block [ "else" block ]

while_stmt     ::= "while" expr block

for_stmt       ::= "for" var_decl expr ";" assign_stmt block

return_stmt    ::= "return" [ expr ] [ ";" ]

expr_stmt      ::= expr [ ";" ]

type           ::= "int" | "float" | "str" | "bool" | "char" | IDENTIFIER [ "[]" ]

expr           ::= logic_or

logic_or       ::= logic_and { "||" logic_and }
logic_and      ::= equality { "&&" equality }
equality       ::= comparison { ( "==" | "!=" ) comparison }
comparison     ::= term { ( "<" | "<=" | ">" | ">=" ) term }
term           ::= factor { ( "+" | "-" ) factor }
factor         ::= unary { ( "*" | "/" ) unary }

unary          ::= ( "!" | "-" ) unary
                 | primary

primary        ::= NUMBER
                 | FLOAT_LITERAL
                 | STRING_LITERAL
                 | CHAR_LITERAL
                 | "true" | "false"
                 | IDENTIFIER
                 | array_init
                 | func_call
                 | "(" expr ")"

array_init     ::= "{" [ expr_list ] "}"
func_call      ::= IDENTIFIER "(" [ expr_list ] ")"

expr_list      ::= expr { "," expr }
```
