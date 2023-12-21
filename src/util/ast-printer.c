#include <stdbool.h>
#include <stdio.h>
#include <assert.h>

#include "ast-printer.h"
#include "util/vectors.h"
#include "parser.h"


void _format_statement(FILE *__restrict stream, int indent_level, statement_t *stmt);
void _format_expression(FILE *__restrict stream, int indent_level, expression_t *stmt);

void indent(FILE *__restrict stream, int indent_level) {
    for (int i = 0; i < indent_level; i++) {
        fprintf(stream, "| ");
    }
}

void _format_statement(FILE *__restrict stream, int indent_level, statement_t *stmt) {
    assert(stmt != NULL);
    indent(stream, indent_level);
    switch (stmt->type) {
        case STATEMENT_EXPRESSION:
            fprintf(stream, "- Expression Statement\n");
            _format_expression(stream, indent_level + 1, stmt->expression);
            break;
        case STATEMENT_EMPTY:
            fprintf(stream, "- Empty Statement\n");
            break;
        default:
            fprintf(stderr, "Unknown statement type: %d\n", stmt->type);
            assert(false);
    }
}

void _format_expression(FILE *__restrict stream, int indent_level, expression_t *expr) {
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
                    _format_expression(stream, indent_level + 2, expr->primary.expression);
                    break;
            }
            break;
        case EXPRESSION_BINARY:
            fprintf(stream, "- Binary Expression\n");
            indent(stream, indent_level + 1);
            fprintf(stream, "- Operator: %s\n", expr->binary.operator->value);
            indent(stream, indent_level + 1);
            fprintf(stream, "- Left\n");
            _format_expression(stream, indent_level + 2, expr->binary.left);
            indent(stream, indent_level + 1);
            fprintf(stream, "- Right\n");
            _format_expression(stream, indent_level + 2, expr->binary.right);
            break;
        case EXPRESSION_UNARY:
            fprintf(stream, "- Unary Expression\n");
            indent(stream, indent_level + 1);
            fprintf(stream, "- Operator: %d\n", expr->unary.operator); // TODO: Print operator name
            indent(stream, indent_level + 1);
            fprintf(stream, "- Operand\n");
            _format_expression(stream, indent_level + 2, expr->unary.operand);
            break;
        case EXPRESSION_TERNARY:
            fprintf(stream, "- Ternary Expression\n");
            indent(stream, indent_level + 1);
            fprintf(stream, "- Condition\n");
            _format_expression(stream, indent_level + 2, expr->ternary.condition);
            indent(stream, indent_level + 1);
            fprintf(stream, "- True Expression\n");
            _format_expression(stream, indent_level + 2, expr->ternary.true_expression);
            indent(stream, indent_level + 1);
            fprintf(stream, "- False Expression\n");
            _format_expression(stream, indent_level + 2, expr->ternary.false_expression);
            break;
        case EXPRESSION_CALL:
            fprintf(stream, "- Call Expression\n");
            indent(stream, indent_level + 1);
            fprintf(stream, "- Callee\n");
            _format_expression(stream, indent_level + 2, expr->call.callee);
            indent(stream, indent_level + 1);
            fprintf(stream, "- Arguments\n");
            for (size_t i = 0; i < expr->call.arguments.size; i++) {
                _format_expression(stream, indent_level + 2, expr->call.arguments.buffer[i]);
            }
            break;
        case EXPRESSION_ARRAY_SUBSCRIPT:
            fprintf(stream, "- Array Subscript Expression\n");
            indent(stream, indent_level + 1);
            fprintf(stream, "- Array\n");
            _format_expression(stream, indent_level + 2, expr->array_subscript.array);
            indent(stream, indent_level + 1);
            fprintf(stream, "- Index\n");
            _format_expression(stream, indent_level + 2, expr->array_subscript.index);
            break;
        case EXPRESSION_MEMBER_ACCESS:
            fprintf(stream, "- Member Access Expression\n");
            indent(stream, indent_level + 1);
            fprintf(stream, "- Struct or Union\n");
            _format_expression(stream, indent_level + 2, expr->member_access.struct_or_union);
            indent(stream, indent_level + 1);
            fprintf(stream, "- Operator: %s\n", expr->member_access.operator.value);
            indent(stream, indent_level + 1);
            fprintf(stream, "- Member: %s\n", expr->member_access.member.value);
            break;
        default:
            fprintf(stderr, "Unknown expression type: %d\n", expr->type);
            assert(false);
    }
}

void format_statement(FILE *__restrict stream, statement_t *stmt) {
    _format_statement(stream, 0, stmt);
}

void format_expression(FILE *__restrict stream, expression_t *expr) {
    _format_expression(stream, 0, expr);
}
