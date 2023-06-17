#ifndef C_COMPILER_AST_PRINTER_H
#define C_COMPILER_AST_PRINTER_H

#include <stdio.h>
#include "parser.h"

void ppast(FILE *__restrict stream, ast_node_t* node);

#endif //C_COMPILER_AST_PRINTER_H
