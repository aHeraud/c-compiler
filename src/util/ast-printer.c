#include <stdbool.h>
#include <stdio.h>
#include <assert.h>

#include "ast-printer.h"
#include "util/vectors.h"
#include "parser.h"

// Example:
//
// input:
// ```
// int foo() {
//     int a = 1;
//     return a;
// }
// ```
//
// output:
// ```
// Translation Unit
// |- Function Definition
// |  |- Declaration Specifiers
// |  |  |- Type Specifier 'int'
// |  |- Declarator
// |  |  |- Identifier: foo
// |  |  |- Parameter Type List
// |  |- Compound Statement
// |  |  |- Declaration
// |  |  |  |- Declaration Specifiers
// |  |  |  |  |- Type Specifier 'int'
// |  |  |  |- Init Declarator List
// |  |  |  |  |- Init Declarator
// |  |  |  |  |  |- Declarator
// |  |  |  |  |  |  |- Identifier: a
// |  |  |  |  |  |- Initializer
// |  |  |  |  |  |  |- Expression
// |  |  |  |  |  |  |  |- Primary Expression
// |  |  |  |  |  |  |  |  |- Constant: 1
// |  |  |- Jump Statement
// |  |  |  |- Return
// |  |  |  |- Expression
// |  |  |  |  |- Primary Expression
// |  |  |  |  |  |- Constant: 1
// ```

void indent(FILE *__restrict stream, int indent_level) {
    for (int i = 0; i < indent_level; i++) {
        fprintf(stream, "| ");
    }
}

void ppidentifier(FILE *__restrict stream, int indent_level, identifier_t identifier) {
    indent(stream, indent_level);
    fprintf(stream, "- Identifier: %s\n", identifier.name);
}

void ppconstant(FILE *__restrict stream, int indent_level, constant_t constant) {
    indent(stream, indent_level);
    fprintf(stream, "- Constant: ");
    switch (constant.type) {
        case CONSTANT_INTEGER:
            fprintf(stream, "%ld\n", constant.integer);
            break;
        case CONSTANT_FLOATING:
            fprintf(stream, "%f\n", constant.floating);
            break;
        case CONSTANT_CHARACTER:
            fprintf(stream, "'%c'\n", constant.character);
            break;
        default:
            fprintf(stderr, "Unknown constant type: %d\n", constant.type);
            assert(false);
    }
}

void ppdirectdeclarator(FILE *__restrict stream, int indent_level, ast_node_t* node) {
    assert(node != NULL);
    assert(node->type == AST_DIRECT_DECLARATOR);

    fprintf(stream, "- Direct Declarator: ");

    ast_node_t* current = node;
    while (current != NULL) {
        assert(current->type == AST_DIRECT_DECLARATOR);
        switch (current->direct_declarator.type) {
            case DECL_ARRAY:
                assert(false); // TODO
            case DECL_IDENTIFIER:
                fprintf(stream, "%s", current->direct_declarator.identifier.name);
                break;
            case DECL_FUNCTION:
                assert(current->direct_declarator.function.param_type_or_ident_list == NULL); // TODO
                fprintf(stream, "()");
        }
        current = current->direct_declarator.next;
    }
    fprintf(stream, "\n");
}

void _ppast(FILE *__restrict stream, ast_node_t* node, int indent_level) {
    assert(node != NULL);
    indent(stream, indent_level);
    switch (node->type) {
        case AST_PRIMARY_EXPRESSION:
            fprintf(stream, "- Primary Expression\n");switch (node->primary_expression.type) {
                case PE_IDENTIFIER:
                    ppidentifier(stream, indent_level + 1, node->primary_expression.identifier);
                    break;
                case PE_CONSTANT:
                    ppconstant(stream, indent_level + 1, node->primary_expression.constant);
                    break;
                case PE_STRING_LITERAL:
                    indent(stream, indent_level + 1);
                    // TODO: truncate string literals that are too long
                    fprintf(stream, "- String Literal: %s\n", node->primary_expression.string_literal);
                    break;
                case PE_EXPRESSION:
                    // TODO
                    assert(false);
            }
            break;
        case AST_DECLARATION:
            fprintf(stream, "- Declaration\n");
            _ppast(stream, node->declaration.declaration_specifiers, indent_level + 1);
            _ppast(stream, node->declaration.init_declarators, indent_level + 1);
            break;
        case AST_DECLARATION_SPECIFIERS:
            fprintf(stream, "- Declaration Specifiers\n");
            for (size_t i = 0; i < node->declaration_specifiers.size; i++) {
                _ppast(stream, node->declaration_specifiers.buffer[i], indent_level + 1);
            }
            break;
        case AST_TYPE_SPECIFIER:
            fprintf(stream, "- Type Specifier: %s\n",
                    type_specifier_names[node->type_specifier]);
            break;
        case AST_TYPE_QUALIFIER:
            fprintf(stream, "- Type Qualifier: %s\n",
                    type_qualifier_names[node->type_qualifier]);
            break;
        case AST_STORAGE_CLASS_SPECIFIER:
            fprintf(stream, "- Storage Class Specifier: %s\n",
                    storage_class_specifier_names[node->storage_class_specifier]);
            break;
        case AST_FUNCTION_SPECIFIER:
            fprintf(stream, "- Function Specifier: %s\n",
                    function_specifier_names[node->function_specifier]);
            break;
        case AST_INIT_DECLARATOR_LIST:
            fprintf(stream, "- Init Declarator List\n");
            for (size_t i = 0; i < node->init_declarator_list.size; i++) {
                _ppast(stream, node->init_declarator_list.buffer[i], indent_level + 1);
            }
            break;
        case AST_INIT_DECLARATOR:
            fprintf(stream, "- Init Declarator\n");
            _ppast(stream, node->init_declarator.declarator, indent_level + 1);
            if (node->init_declarator.initializer != NULL) {
                _ppast(stream, node->init_declarator.initializer, indent_level + 1);
            }
            break;
        case AST_DECLARATOR:
            fprintf(stream, "- Declarator\n");
            // _ppast(stream, node->declarator.pointer, indent_level + 1); // TODO
            _ppast(stream, node->declarator.direct_declarator, indent_level + 1);
            break;
        case AST_DIRECT_DECLARATOR:
            ppdirectdeclarator(stream, indent_level, node);
            break;
        case AST_INITIALIZER:
            fprintf(stream, "- Initializer\n");
            if (node->initializer.type == INITIALIZER_EXPRESSION) {
                _ppast(stream, node->initializer.expression, indent_level + 1);
            } else if (node->initializer.type == INITIALIZER_LIST) {
                assert(false); // TODO
            } else {
                fprintf(stderr, "Unknown initializer type: %d\n", node->initializer.type);
                assert(false);
            }
            break;
        case AST_COMPOUND_STATEMENT:
            fprintf(stream, "- Compound Statement\n");
            for (size_t i = 0; i < node->compound_statement.block_items.size; i++) {
                _ppast(stream, node->compound_statement.block_items.buffer[i], indent_level + 1);
            }
            break;
        case AST_FUNCTION_DEFINITION:
            fprintf(stream, "- Function Definition\n");
            _ppast(stream, node->function_definition.declaration_specifiers, indent_level + 1);
            _ppast(stream, node->function_definition.declarator, indent_level + 1);
            if (node->function_definition.declaration_list != NULL) {
                _ppast(stream, node->function_definition.declaration_list, indent_level + 1);
            }
            _ppast(stream, node->function_definition.compound_statement, indent_level + 1);
            break;
        case AST_TRANSLATION_UNIT:
            fprintf(stream, "- Translation Unit\n");
            for (int i = 0; i < node->translation_unit.external_declarations.size; i += 1) {
                _ppast(stream, node->translation_unit.external_declarations.buffer[i], indent_level + 1);
            }
            break;
        case AST_JUMP_STATEMENT:
            switch (node->jump_statement.type) {
                case JMP_RETURN:
                    fprintf(stream, "- Jump Statement: return\n");
                    if (node->jump_statement._return.expression != NULL) {
                        _ppast(stream, node->jump_statement._return.expression, indent_level + 1);
                    }
                    break;
                case JMP_BREAK:
                    fprintf(stream, "- Jump Statement: break\n");
                    break;
                case JMP_CONTINUE:
                    fprintf(stream, "- Jump Statement: continue\n");
                    break;
                case JMP_GOTO:
                    fprintf(stream, "- Jump Statement - goto\n");
                    if (node->jump_statement._goto.identifier.name != NULL) {
                        indent(stream, indent_level + 1);
                        fprintf(stream,"- Identifier: %s", node->jump_statement._goto.identifier.name);
                    }
                    break;
            }
            break;
        default:
            fprintf(stderr, "Unknown AST node type: %d\n", node->type);
            assert(false);
    }
}

void ppast(FILE *__restrict stream, ast_node_t* node) {
    _ppast(stream, node, 0);
}
