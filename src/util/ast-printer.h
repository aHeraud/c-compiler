#ifndef C_COMPILER_AST_PRINTER_H
#define C_COMPILER_AST_PRINTER_H

#include <stdio.h>
#include "parser.h"

void format_expression(FILE *__restrict stream, expression_t *node);
void format_statement(FILE *__restrict stream, statement_t *node);

#endif //C_COMPILER_AST_PRINTER_H
