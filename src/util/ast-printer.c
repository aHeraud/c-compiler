#include <stdbool.h>
#include <stdio.h>
#include <assert.h>

#include "ast-printer.h"
#include "util/vectors.h"
#include "parser.h"

void indent(FILE *__restrict stream, int indent_level) {
    for (int i = 0; i < indent_level; i++) {
        fprintf(stream, "| ");
    }
}

void ppexpr(FILE *__restrict stream, int indent_level, expression_t* expr) {
    assert(expr != NULL);
    indent(stream, indent_level);
    switch (expr->type) {
        case EXPRESSION_PRIMARY:
            fprintf(stream, "- Primary Expression\n");
            switch (expr->primary.type) {
                case PE_IDENTIFIER:
                    indent(stream, indent_level);
                    fprintf(stream, "- Identifier: %s\n", expr->primary.token.value);
                    break;
                case PE_CONSTANT:
                    indent(stream, indent_level);
                    fprintf(stream, "- Constant: ");
                    fprintf(stream, "%s\n", expr->primary.token.value);
                    break;
                case PE_STRING_LITERAL:
                    indent(stream, indent_level + 1);
                    fprintf(stream, "- String Literal: %s\n", expr->primary.token.value);
                case PE_EXPRESSION:
                    fprintf(stream, "- Expression\n");
                    ppexpr(stream, indent_level + 2, expr->primary.expression);
                    break;
            }
            break;
        default:
            fprintf(stderr, "Unknown expression type: %d\n", expr->type);
            assert(false);
    }
}

void ppast(FILE *__restrict stream, expression_t *expr) {
    ppexpr(stream, 0, expr);
}
