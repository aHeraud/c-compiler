#include<assert.h>
#include<stdbool.h>
#include<stdio.h>
#include<string.h>

#include "types.h"
#include "ast.h"

// Compare two expression nodes for equality.
bool expression_eq(const expression_t *left, const expression_t *right) {
    if (left == NULL || right == NULL) {
        return left == right;
    }

    // Strip grouping expressions as they are not semantically meaningful in this context.
    while (left->kind == EXPRESSION_PRIMARY && left->value.primary.kind == PE_EXPRESSION) {
        left = left->value.primary.value.expression;
    }
    while (right->kind == EXPRESSION_PRIMARY && right->value.primary.kind == PE_EXPRESSION) {
        right = right->value.primary.value.expression;
    }

    if (left->kind != right->kind) {
        return false;
    }

    switch (left->kind) {
        case EXPRESSION_PRIMARY:
            if (left->value.primary.kind != right->value.primary.kind) {
                return false;
            }
            switch (left->value.primary.kind) {
                case PE_IDENTIFIER:
                case PE_CONSTANT:
                case PE_STRING_LITERAL:
                    return strcmp(left->value.primary.value.token.value, right->value.primary.value.token.value) == 0;
                case PE_EXPRESSION:
                    return expression_eq(left->value.primary.value.expression, right->value.primary.value.expression);
                default:
                    perror("Invalid primary parse_expression type");
                    assert(false);
            }
        case EXPRESSION_BINARY:
            if (left->value.binary.kind != right->value.binary.kind) {
                return false;
            }

            switch (left->value.binary.kind) {
                case BINARY_ARITHMETIC:
                    if (left->value.binary.operator.arithmetic != right->value.binary.operator.arithmetic) {
                        return false;
                    }
                    break;
                case BINARY_BITWISE:
                    if (left->value.binary.operator.bitwise != right->value.binary.operator.bitwise) {
                        return false;
                    }
                    break;
                case BINARY_LOGICAL:
                    if (left->value.binary.operator.logical != right->value.binary.operator.logical) {
                        return false;
                    }
                    break;
                case BINARY_COMPARISON:
                    if (left->value.binary.operator.comparison != right->value.binary.operator.comparison) {
                        return false;
                    }
                    break;
                case BINARY_ASSIGNMENT:
                    if (left->value.binary.operator.assignment != right->value.binary.operator.assignment) {
                        return false;
                    }
                    break;
                case BINARY_COMMA: {}
            }

            if (left->value.binary.operator_token->kind != right->value.binary.operator_token->kind) {
                return false;
            }
            return expression_eq(left->value.binary.left, right->value.binary.left) &&
                   expression_eq(left->value.binary.right, right->value.binary.right);
        case EXPRESSION_UNARY:
            if (left->value.unary.operator != right->value.unary.operator) {
                return false;
            }
            return expression_eq(left->value.unary.operand, right->value.unary.operand);
        case EXPRESSION_TERNARY:
            return expression_eq(left->value.ternary.condition, right->value.ternary.condition) &&
                   expression_eq(left->value.ternary.true_expression, right->value.ternary.true_expression) &&
                   expression_eq(left->value.ternary.false_expression, right->value.ternary.false_expression);
        case EXPRESSION_CALL:
            if (!expression_eq(left->value.call.callee, right->value.call.callee)) {
                return false;
            }
            if (left->value.call.arguments.size != right->value.call.arguments.size) {
                return false;
            }
            for (size_t i = 0; i < left->value.call.arguments.size; i++) {
                if (!expression_eq(left->value.call.arguments.buffer[i], right->value.call.arguments.buffer[i])) {
                    return false;
                }
            }
            return true;
        case EXPRESSION_ARRAY_SUBSCRIPT:
            return expression_eq(left->value.array_subscript.array, right->value.array_subscript.array) &&
                   expression_eq(left->value.array_subscript.index, right->value.array_subscript.index);
        case EXPRESSION_MEMBER_ACCESS:
            if (left->value.member_access.operator.kind != right->value.member_access.operator.kind) {
                return false;
            }
            if (left->value.member_access.member.kind != right->value.member_access.member.kind) {
                return false;
            }
            return expression_eq(left->value.member_access.struct_or_union, right->value.member_access.struct_or_union);
        case EXPRESSION_SIZEOF:
            return types_equal(left->value.type, right->value.type);
        case EXPRESSION_CAST:
            return types_equal(left->value.cast.type, right->value.cast.type) &&
                   expression_eq(left->value.cast.expression, right->value.cast.expression);
        default:
            assert("Invalid expression type" && false);
            return false;
    }
}

bool statement_eq(const statement_t *left, const statement_t *right) {
    if (left == NULL || right == NULL) {
        return left == right;
    }

    if (left->kind != right->kind) {
        return false;
    }

    switch (left->kind) {
        case STATEMENT_EMPTY:
            return true;
        case STATEMENT_EXPRESSION:
            return expression_eq(left->value.expression, right->value.expression);
        case STATEMENT_COMPOUND:
            if (left->value.compound.block_items.size != right->value.compound.block_items.size) {
                return false;
            }
            for (size_t i = 0; i < left->value.compound.block_items.size; i++) {
                block_item_t *left_item = left->value.compound.block_items.buffer[i];
                block_item_t *right_item = right->value.compound.block_items.buffer[i];

                if (left_item->kind != right_item->kind) {
                    return false;
                }

                if (left_item->kind == BLOCK_ITEM_STATEMENT) {
                    if (!statement_eq(left_item->value.statement, right_item->value.statement)) {
                        return false;
                    }
                } else {
                    if (!declaration_eq(left_item->value.declaration, right_item->value.declaration)) {
                        return false;
                    }
                }
            }
            return true;
        case STATEMENT_IF:
            assert(left->value.if_.keyword != NULL && right->value.if_.keyword != NULL);
            if (left->value.if_.keyword->kind != right->value.if_.keyword->kind) {
                return false;
            }
            if (!expression_eq(left->value.if_.condition, right->value.if_.condition)) {
                return false;
            }
            if (!statement_eq(left->value.if_.true_branch, right->value.if_.true_branch)) {
                return false;
            }
            if (left->value.if_.false_branch == NULL || right->value.if_.false_branch == NULL) {
                return left->value.if_.false_branch == right->value.if_.false_branch;
            } else {
                return statement_eq(left->value.if_.false_branch, right->value.if_.false_branch);
            }
        case STATEMENT_RETURN:
            assert(left->value.return_.keyword != NULL && right->value.return_.keyword != NULL);
            if (left->value.return_.keyword->kind != right->value.return_.keyword->kind) {
                return false;
            }
            return expression_eq(left->value.return_.expression, right->value.return_.expression);
        case STATEMENT_WHILE:
            return expression_eq(left->value.while_.condition, right->value.while_.condition) &&
                statement_eq(left->value.while_.body, right->value.while_.body);
        case STATEMENT_FOR:
            return left->value.for_.initializer.kind == right->value.for_.initializer.kind && // TODO: check rest of initializer
                expression_eq(left->value.for_.condition, right->value.for_.condition) &&
                expression_eq(left->value.for_.post, right->value.for_.post) &&
                statement_eq(left->value.for_.body, right->value.for_.body);
        case STATEMENT_BREAK:
            return true;
        case STATEMENT_CONTINUE:
            return true;
        case STATEMENT_GOTO:
            return right->kind == STATEMENT_GOTO &&
                strcmp(left->value.goto_.identifier->value, right->value.goto_.identifier->value) == 0;
        case STATEMENT_LABEL:
            return right->kind == STATEMENT_LABEL &&
                strcmp(left->value.label_.identifier->value, right->value.label_.identifier->value) == 0 &&
                statement_eq(left->value.label_.statement, right->value.label_.statement);
    }
}

bool initializer_eq(const initializer_t *left, initializer_t *right) {
    if (left == NULL || right == NULL) {
        return left == right;
    }

    if (left->kind != right->kind) {
        return false;
    }

    if (left->kind == INITIALIZER_LIST) {
        if (left->value.list->size != right->value.list->size) {
            return false;
        }

        for (size_t i = 0; i < left->value.list->size; i++) {
            initializer_list_element_t *left_element = &left->value.list->buffer[i];
            initializer_list_element_t *right_element = &right->value.list->buffer[i];

            // TODO: compare designators
            return initializer_eq(left_element->initializer, right_element->initializer);
        }
    } else {
        return expression_eq(left->value.expression, right->value.expression);
    }
    return false;
}

bool declaration_eq(const declaration_t *left, const declaration_t *right) {
    if (left == NULL || right == NULL) {
        return left == right;
    }

    if (!types_equal(left->type, right->type)) {
        return false;
    }

    if (left->identifier == NULL || right->identifier == NULL) {
        return left->identifier == right->identifier;
    }

    if (strcmp(left->identifier->value, right->identifier->value) != 0) {
        return false;
    }

    return initializer_eq(left->initializer, right->initializer);
}
