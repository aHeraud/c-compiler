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
    while (left->type == EXPRESSION_PRIMARY && left->primary.type == PE_EXPRESSION) {
        left = left->primary.expression;
    }
    while (right->type == EXPRESSION_PRIMARY && right->primary.type == PE_EXPRESSION) {
        right = right->primary.expression;
    }

    if (left->type != right->type) {
        return false;
    }

    switch (left->type) {
        case EXPRESSION_PRIMARY:
            if (left->primary.type != right->primary.type) {
                return false;
            }
            switch (left->primary.type) {
                case PE_IDENTIFIER:
                case PE_CONSTANT:
                case PE_STRING_LITERAL:
                    return strcmp(left->primary.token.value, right->primary.token.value) == 0;
                case PE_EXPRESSION:
                    return expression_eq(left->primary.expression, right->primary.expression);
                default:
                    perror("Invalid primary parse_expression type");
                    assert(false);
            }
        case EXPRESSION_BINARY:
            if (left->binary.type != right->binary.type) {
                return false;
            }

            switch (left->binary.type) {
                case BINARY_ARITHMETIC:
                    if (left->binary.arithmetic_operator != right->binary.arithmetic_operator) {
                        return false;
                    }
                    break;
                case BINARY_BITWISE:
                    if (left->binary.bitwise_operator != right->binary.bitwise_operator) {
                        return false;
                    }
                    break;
                case BINARY_LOGICAL:
                    if (left->binary.logical_operator != right->binary.logical_operator) {
                        return false;
                    }
                    break;
                case BINARY_COMPARISON:
                    if (left->binary.comparison_operator != right->binary.comparison_operator) {
                        return false;
                    }
                    break;
                case BINARY_ASSIGNMENT:
                    if (left->binary.assignment_operator != right->binary.assignment_operator) {
                        return false;
                    }
                    break;
                case BINARY_COMMA: {}
            }

            if (left->binary.operator->kind != right->binary.operator->kind) {
                return false;
            }
            return expression_eq(left->binary.left, right->binary.left) &&
                   expression_eq(left->binary.right, right->binary.right);
        case EXPRESSION_UNARY:
            if (left->unary.operator != right->unary.operator) {
                return false;
            }
            return expression_eq(left->unary.operand, right->unary.operand);
        case EXPRESSION_TERNARY:
            return expression_eq(left->ternary.condition, right->ternary.condition) &&
                   expression_eq(left->ternary.true_expression, right->ternary.true_expression) &&
                   expression_eq(left->ternary.false_expression, right->ternary.false_expression);
        case EXPRESSION_CALL:
            if (!expression_eq(left->call.callee, right->call.callee)) {
                return false;
            }
            if (left->call.arguments.size != right->call.arguments.size) {
                return false;
            }
            for (size_t i = 0; i < left->call.arguments.size; i++) {
                if (!expression_eq(left->call.arguments.buffer[i], right->call.arguments.buffer[i])) {
                    return false;
                }
            }
            return true;
        case EXPRESSION_ARRAY_SUBSCRIPT:
            return expression_eq(left->array_subscript.array, right->array_subscript.array) &&
                   expression_eq(left->array_subscript.index, right->array_subscript.index);
        case EXPRESSION_MEMBER_ACCESS:
            if (left->member_access.operator.kind != right->member_access.operator.kind) {
                return false;
            }
            if (left->member_access.member.kind != right->member_access.member.kind) {
                return false;
            }
            return expression_eq(left->member_access.struct_or_union, right->member_access.struct_or_union);
        case EXPRESSION_SIZEOF:
            return types_equal(left->sizeof_type, right->sizeof_type);
        case EXPRESSION_CAST:
            return types_equal(left->cast.type, right->cast.type) &&
                   expression_eq(left->cast.expression, right->cast.expression);
        default:
            assert("Invalid expression type" && false);
            return false;
    }
}

bool statement_eq(const statement_t *left, const statement_t *right) {
    if (left == NULL || right == NULL) {
        return left == right;
    }

    if (left->type != right->type) {
        return false;
    }

    switch (left->type) {
        case STATEMENT_EMPTY:
            return true;
        case STATEMENT_EXPRESSION:
            return expression_eq(left->expression, right->expression);
        case STATEMENT_COMPOUND:
            if (left->compound.block_items.size != right->compound.block_items.size) {
                return false;
            }
            for (size_t i = 0; i < left->compound.block_items.size; i++) {
                block_item_t *left_item = left->compound.block_items.buffer[i];
                block_item_t *right_item = right->compound.block_items.buffer[i];

                if (left_item->type != right_item->type) {
                    return false;
                }

                if (left_item->type == BLOCK_ITEM_STATEMENT) {
                    if (!statement_eq(left_item->statement, right_item->statement)) {
                        return false;
                    }
                } else {
                    if (!declaration_eq(left_item->declaration, right_item->declaration)) {
                        return false;
                    }
                }
            }
            return true;
        case STATEMENT_IF:
            assert(left->if_.keyword != NULL && right->if_.keyword != NULL);
            if (left->if_.keyword->kind != right->if_.keyword->kind) {
                return false;
            }
            if (!expression_eq(left->if_.condition, right->if_.condition)) {
                return false;
            }
            if (!statement_eq(left->if_.true_branch, right->if_.true_branch)) {
                return false;
            }
            if (left->if_.false_branch == NULL || right->if_.false_branch == NULL) {
                return left->if_.false_branch == right->if_.false_branch;
            } else {
                return statement_eq(left->if_.false_branch, right->if_.false_branch);
            }
        case STATEMENT_RETURN:
            assert(left->return_.keyword != NULL && right->return_.keyword != NULL);
            if (left->return_.keyword->kind != right->return_.keyword->kind) {
                return false;
            }
            return expression_eq(left->return_.expression, right->return_.expression);
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
        if (left->list->size != right->list->size) {
            return false;
        }

        for (size_t i = 0; i < left->list->size; i++) {
            initializer_list_element_t *left_element = &left->list->buffer[i];
            initializer_list_element_t *right_element = &right->list->buffer[i];

            // TODO: compare designators
            return initializer_eq(left_element->initializer, right_element->initializer);
        }
    } else {
        return expression_eq(left->expression, right->expression);
    }
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
