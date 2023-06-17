// Recursive descent parser, based on the reference c99 grammar: see docs/c99.bnf
// Some rules have been simplified or rewritten to be compatible with a
// recursive descent parser.

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "parser.h"
#include "lexer.h"

parser_t pinit(lexer_t lexer) {
    token_t token = lscan(&lexer);
    return (parser_t) {
            .lexer = lexer,
            .token = token,
    };
}

bool parse(parser_t* parser, ast_node_t* node) {
    return translation_unit(parser, node);
}

bool accept(parser_t* parser, token_kind_t kind, token_t* token_out) {
    if (parser->token.kind == kind) {
        if (token_out != NULL) {
            *token_out = parser->token;
        }
        parser->token = lscan(&parser->lexer); // note: leaks token value
        return true;
    } else {
        return false;
    }
}

bool require(parser_t* parser, token_kind_t kind, token_t* token_out) {
    if (accept(parser, kind, token_out)) {
        return true;
    } else {
        // TODO: recover from error(s)
        fprintf(stderr, "error: expected token of kind %d, got %d\n", kind, parser->token.kind);
        exit(1);
    }
}

// Expressions

bool primary_expression(parser_t* parser, ast_node_t* node) {
    token_t token;
    if (accept(parser, TK_IDENTIFIER, &token)) {
        node->type = AST_PRIMARY_EXPRESSION;
        node->position = token.position;
        node->primary_expression.type = PE_IDENTIFIER;
        node->primary_expression.identifier.name = strdup(token.value);
        return true;
    } else if (accept(parser, TK_INTEGER_CONSTANT, &token)) {
        node->type = AST_PRIMARY_EXPRESSION;
        node->position = token.position;
        node->primary_expression.type = PE_CONSTANT;
        node->primary_expression.constant.type = CONSTANT_INTEGER;
        node->primary_expression.constant.integer = strtoll(token.value, NULL, 10); // can this fail?
        return true;
    } else if (accept(parser, TK_FLOATING_CONSTANT, &token)) {
        return true;
    } else if (accept(parser, TK_CHAR_LITERAL, &token)) {
        node->type = AST_PRIMARY_EXPRESSION;
        node->position = token.position;
        node->primary_expression.type = PE_CONSTANT;
        node->primary_expression.constant.type = CONSTANT_CHARACTER;
        return true;
    } else if (accept(parser, TK_STRING_LITERAL, &token)) {
        node->type = AST_PRIMARY_EXPRESSION;
        node->position = token.position;
        node->primary_expression.type = PE_STRING_LITERAL;
        node->primary_expression.string_literal = strdup(token.value);
        return true;
    } else if (accept(parser, TK_LPAREN, &token)) {
        node->type = AST_PRIMARY_EXPRESSION;
        node->position = token.position;
        ast_node_t child; // TODO
        expression(parser, &child);
        require(parser, TK_RPAREN, NULL);
        return true;
    } else {
        return false;
    }
}

// The <postfix-expression> production as defined in the c99 standard is left recursive.
bool postfix_expression(parser_t* parser, ast_node_t* node) {
    // TODO: Implement
    return primary_expression(parser, node);
}

bool unary_expression(parser_t* parser, ast_node_t* node) {
    // TODO: Implement
    return postfix_expression(parser, node);
}

bool cast_expression(parser_t* parser, ast_node_t* node) {
    // TODO: Implement
    return unary_expression(parser, node);
}

bool multiplicative_expression(parser_t* parser, ast_node_t* node) {
    // TODO: Implement
    return cast_expression(parser, node);
}

bool additive_expression(parser_t* parser, ast_node_t* node) {
    // TODO: Implement
    return multiplicative_expression(parser, node);
}

bool shift_expression(parser_t* parser, ast_node_t* node) {
    // TODO: Implement
    return additive_expression(parser, node);
}

bool relational_expression(parser_t* parser, ast_node_t* node) {
    // TODO: Implement
    return shift_expression(parser, node);
}

bool equality_expression(parser_t* parser, ast_node_t* node) {
    // TODO: Implement
    return relational_expression(parser, node);
}

bool and_expression(parser_t* parser, ast_node_t* node) {
    // TODO: Implement
    return equality_expression(parser, node);
}

bool exclusive_or_expression(parser_t* parser, ast_node_t* node) {
    // TODO: Implement
    return and_expression(parser, node);
}

bool inclusive_or_expression(parser_t* parser, ast_node_t* node) {
    // TODO: Implement
    return exclusive_or_expression(parser, node);
}

bool logical_and_expression(parser_t* parser, ast_node_t* node) {
    // TODO: Implement
    return inclusive_or_expression(parser, node);
}

bool logical_or_expression(parser_t* parser, ast_node_t* node) {
    // TODO: Implement
    return logical_and_expression(parser, node);
}

bool conditional_expression(parser_t* parser, ast_node_t* node) {
    // TODO: Implement
    return logical_or_expression(parser, node);
}

bool assignment_expression(parser_t* parser, ast_node_t* node) {
    // TODO: Implement
    return conditional_expression(parser, node);
}

bool expression(parser_t* parser, ast_node_t* node) {
    bool matched = false;
    while(assignment_expression(parser, node)) {
        matched = true;
        if (!accept(parser, TK_COMMA, NULL)) {
            break;
        }
    }
    return matched;
}

// Declarations

bool declaration(parser_t* parser, ast_node_t* node) {
    source_position_t position = parser->token.position;
    ast_node_t* declaration_specifiers_node = malloc(sizeof (ast_node_t));
    ast_node_t* init_declarator_list_node = malloc(sizeof (ast_node_t));

    if (declaration_specifiers(parser, declaration_specifiers_node)) {
        node->type = AST_DECLARATION;
        node->position = position;
        node->declaration.declaration_specifiers = declaration_specifiers_node;

        if (init_declarator_list(parser, init_declarator_list_node)) {
            node->declaration.init_declarators = init_declarator_list_node;
        } else {
            node->declaration.init_declarators = NULL;
            free(init_declarator_list_node);
        }
        require(parser, TK_SEMICOLON, NULL);
        return true;
    } else {
        free(declaration_specifiers_node);
        free(init_declarator_list_node);
        return false;
    }
}

bool declaration_specifiers(parser_t* parser, ast_node_t* node) {
    ast_node_t declaration_specifiers_node;
    declaration_specifiers_node.type = AST_DECLARATION_SPECIFIERS;
    declaration_specifiers_node.position = parser->token.position;

    ast_node_vector_t specifiers_list = {NULL, 0, 0};

    bool matched = false;
    ast_node_t* child = malloc(sizeof (ast_node_t));
    while (storage_class_specifier(parser, child) ||
          type_specifier(parser, child) ||
          type_qualifier(parser, child) ||
          function_specifier(parser, child)
    ) {
        matched = true;
        append_ptr((void***) &specifiers_list.buffer, &specifiers_list.size, &specifiers_list.capacity, child);
        child = malloc(sizeof (ast_node_t));
    }
    free(child);

    if (matched) {
        shrink_ptr_vector((void***) &specifiers_list.buffer, &specifiers_list.size, &specifiers_list.capacity);
        declaration_specifiers_node.declaration_specifiers = specifiers_list;
        *node = declaration_specifiers_node;
    }

    return matched;
}

bool init_declarator_list(parser_t* parser, ast_node_t* node) {
    bool matched = false;
    source_position_t position = parser->token.position;
    ast_node_vector_t init_declarators_list = {NULL, 0, 0};
    ast_node_t* init_declarator_node = malloc(sizeof (ast_node_t));
    while (init_declarator(parser, init_declarator_node)) {
        matched = true;
        append_ptr((void***) &init_declarators_list.buffer, &init_declarators_list.size,
                   &init_declarators_list.capacity, init_declarator_node);
        init_declarator_node = malloc(sizeof (ast_node_t));
        if (!accept(parser, TK_COMMA, NULL)) {
            break;
        }
    }
    free(init_declarator_node);
    if (matched) {
        node->type = AST_INIT_DECLARATOR_LIST;
        node->position = position;
        node->init_declarator_list = init_declarators_list;
    }
    return matched;
}

bool init_declarator(parser_t* parser, ast_node_t* node) {
    source_position_t position = parser->token.position;
    ast_node_t* declarator_node = malloc(sizeof (ast_node_t));
    if (declarator(parser, declarator_node)) {
        node->type = AST_INIT_DECLARATOR;
        node->position = position;
        node->init_declarator.declarator = declarator_node;
        node->init_declarator.initializer = NULL;
        if (accept(parser, TK_ASSIGN, NULL)) {
            ast_node_t* initializer_node = malloc(sizeof (ast_node_t));
            bool hasInitializer = initializer(parser, initializer_node);
            node->init_declarator.initializer = initializer_node;
            assert(hasInitializer); // TODO: Error/Recovery
        }
        return true;
    } else {
        free(declarator_node);
        return false;
    }
}

bool storage_class_specifier(parser_t* parser, ast_node_t* node) {
    ast_node_t temp;
    temp.position = parser->token.position;
    temp.type = AST_STORAGE_CLASS_SPECIFIER;

    if (accept(parser, TK_TYPEDEF, NULL)) {
        temp.storage_class_specifier = STORAGE_CLASS_SPECIFIER_TYPEDEF;
        *node = temp;
        return true;
    } else if (accept(parser, TK_EXTERN, NULL)) {
        temp.storage_class_specifier = STORAGE_CLASS_SPECIFIER_EXTERN;
        *node = temp;
        return true;
    } else if (accept(parser, TK_STATIC, NULL)) {
        temp.storage_class_specifier = STORAGE_CLASS_SPECIFIER_STATIC;
        *node = temp;
        return true;
    } else if (accept(parser, TK_AUTO, NULL)) {
        temp.storage_class_specifier = STORAGE_CLASS_SPECIFIER_AUTO;
        *node = temp;
        return true;
    } else if (accept(parser, TK_REGISTER, NULL)) {
        temp.storage_class_specifier = STORAGE_CLASS_SPECIFIER_REGISTER;
        *node = temp;
        return true;
    } else {
        return false;
    }
}

bool type_specifier(parser_t* parser, ast_node_t* node_out) {
    ast_node_t node;
    node.type = AST_TYPE_SPECIFIER;
    node.position = parser->token.position;

    if (accept(parser, TK_VOID, NULL)) {
        node.type_specifier = TYPE_SPECIFIER_VOID;
        *node_out = node;
        return true;
    } else if (accept(parser, TK_CHAR, NULL)) {
        node.type_specifier = TYPE_SPECIFIER_CHAR;
        *node_out = node;
        return true;
    } else if (accept(parser, TK_SHORT, NULL)) {
        node.type_specifier = TYPE_SPECIFIER_SHORT;
        *node_out = node;
        return true;
    } else if (accept(parser, TK_INT, NULL)) {
        node.type_specifier = TYPE_SPECIFIER_INT;
        *node_out = node;
        return true;
    } else if (accept(parser, TK_LONG, NULL)) {
        node.type_specifier = TYPE_SPECIFIER_LONG;
        *node_out = node;
        return true;
    } else if (accept(parser, TK_FLOAT, NULL)) {
        node.type_specifier = TYPE_SPECIFIER_FLOAT;
        *node_out = node;
        return true;
    } else if (accept(parser, TK_DOUBLE, NULL)) {
        node.type_specifier = TYPE_SPECIFIER_DOUBLE;
        *node_out = node;
        return true;
    } else if (accept(parser, TK_SIGNED, NULL)) {
        node.type_specifier = TYPE_SPECIFIER_SIGNED;
        *node_out = node;
        return true;
    } else if (accept(parser, TK_UNSIGNED, NULL)) {
        node.type_specifier = TYPE_SPECIFIER_UNSIGNED;
        *node_out = node;
        return true;
    } else if (accept(parser, TK_BOOL, NULL)) {
        node.type_specifier = TYPE_SPECIFIER_BOOL;
        *node_out = node;
        return true;
    } else if (accept(parser, TK_COMPLEX, NULL)) {
        node.type_specifier = TYPE_SPECIFIER_COMPLEX;
        *node_out = node;
        return true;
    } else if (accept(parser, TK_STRUCT, NULL)) {
        // TODO: struct declaration
        assert(false);
    } else if (accept(parser, TK_UNION, NULL)) {
        // TODO: union declaration
        assert(false);
    } else if (accept(parser, TK_ENUM, NULL)) {
        // TODO: enum declaration
        assert(false);
    } else {
        // TODO: struct, union, enum, typedef
        return false;
    }
}

bool specifier_qualifier_list(parser_t* parser, ast_node_t* node) {
    bool matched = false;
    while (type_specifier(parser, node) || type_qualifier(parser, node)) {
        matched = true;
    }
    return matched;
}

bool type_qualifier(parser_t* parser, ast_node_t* node) {
    ast_node_t temp;
    temp.position = parser->token.position;
    temp.type = AST_TYPE_QUALIFIER;

    if (accept(parser, TK_CONST, NULL)) {
        temp.type_qualifier = TYPE_QUALIFIER_CONST;
        *node = temp;
        return true;
    } else if (accept(parser, TK_RESTRICT, NULL)) {
        temp.type_qualifier = TYPE_QUALIFIER_RESTRICT;
        *node = temp;
        return true;
    } else if (accept(parser, TK_VOLATILE, NULL)) {
        temp.type_qualifier = TYPE_QUALIFIER_VOLATILE;
        *node = temp;
        return true;
    } else {
        return false;
    }
}

bool function_specifier(parser_t* parser, ast_node_t* node) {
    source_position_t position = parser->token.position;
    if (accept(parser, TK_INLINE, NULL)) {
        node->type = AST_FUNCTION_SPECIFIER;
        node->function_specifier = FUNCTION_SPECIFIER_INLINE;
        node->position = position;
        return true;
    } else {
        return false;
    }
}

bool declarator(parser_t* parser, ast_node_t* node) {
    node->position = parser->token.position;

    ast_node_t* pointer_node = malloc(sizeof (ast_node_t));
    if (!pointer(parser, pointer_node)) {
        free(pointer_node);
        pointer_node = NULL;
    }

    ast_node_t* direct_declarator_node = malloc(sizeof (ast_node_t));
    if (!direct_declarator(parser, direct_declarator_node)) {
        free(direct_declarator_node);
        return false;
    }

    node->type = AST_DECLARATOR;
    node->declarator.pointer = pointer_node;
    node->declarator.direct_declarator = direct_declarator_node;
    return true;
}

// The rule as defined in the c99 standard is left recursive (see docs/c99.bnf), we will redefine it here to remove
// the left recursion. The associativity of the rule is also changed to right associative, and we will later reverse the
// list of direct_declarator' nodes, so it matches the expected order defined by the grammar.
//
// <direct-declarator> ::= <identifier> <direct-declarator'>? | '(' <declarator> ')' <direct-declarator'>*
// <direct-declarator'> ::= '[' <type-qualifier-list>? <assignment-expression>? ']'
//                        | '[' 'static' <type-qualifier-list>? <assignment-expression> ']'
//                        | '[' <type-qualifier-list> 'static' <assignment-expression> ']'
//                        | '[' <type-qualifier-list>? '*' ']'
//                        | '(' <parameter-type-list> ')'
//                        | '(' <identifier-list>? ')'

bool direct_declarator_prime(parser_t* parser, ast_node_t* node) {
    source_position_t position = parser->token.position;
    if (accept(parser, TK_LBRACKET, NULL)) {
        // TODO: Implement array declarator(s)
        assert(false);
    } else if (accept(parser, TK_LPAREN, NULL)) {
        ast_node_t* pt_i_list = malloc(sizeof(ast_node_t));
        parameter_type_list(parser, pt_i_list) || identifier_list(parser, pt_i_list) || (pt_i_list = NULL);
        node->type = AST_DIRECT_DECLARATOR;
        node->position = position;
        node->direct_declarator.type = DECL_FUNCTION;
        node->direct_declarator.function.param_type_or_ident_list = pt_i_list;
        require(parser, TK_RPAREN, NULL);
        return true;
    } else {
        return false;
    }
}

bool direct_declarator(parser_t* parser, ast_node_t* node) {
    node->position = parser->token.position;
    token_t token;

    ast_node_t* temp = malloc(sizeof(ast_node_t));
    if (accept(parser, TK_IDENTIFIER, &token)) {
        temp->type = AST_DIRECT_DECLARATOR;
        temp->direct_declarator.type = DECL_IDENTIFIER;
        identifier_t identifier = {token.value};
        temp->direct_declarator.identifier = identifier;

        // reverse the list of direct_declarator nodes
        ast_node_t* prev = malloc(sizeof(ast_node_t));
        while (direct_declarator_prime(parser, prev)) {
            temp->direct_declarator.prev = prev;
            prev->direct_declarator.next = temp;
            temp = prev;
            prev = malloc(sizeof(ast_node_t));
        }
        free(prev);

        *node = *temp;

        return true;
    } else if (accept(parser, TK_LPAREN, NULL)) {
        // declarator(parser, node);
        // require(parser, TK_RPAREN, NULL);
        assert(false); // TODO: Implement '(' <declarator> ')' <direct_declarator'>?
        return true;
    } else {
        free(temp);
        return false;
    }
}

bool pointer(parser_t* parser, ast_node_t* node) {
    if (accept(parser, TK_ASTERISK, NULL)) {
        ast_node_t temp;
        type_qualifier_list(parser, &temp);
        pointer(parser, &temp);
        return true;
    } else {
        return false;
    }
}

bool type_qualifier_list(parser_t* parser, ast_node_t* node) {
    bool matched = false;
    while (type_qualifier(parser, node)) {
        matched = true;
    }
    return matched;
}

// Combined with <parameter-list> to remove need for lookahead.
bool parameter_type_list(parser_t* parser, ast_node_t* node) {
    return parameter_list(parser, node);
}

bool parameter_list(parser_t* parser, ast_node_t* node) {
    if (!parameter_declaration(parser, node)) {
        return false;
    };

    while (accept(parser, TK_COMMA, NULL)) {
        if (accept(parser, TK_ELLIPSIS, NULL)) {
            break;
        } else if (parameter_declaration(parser, node)) {
            continue;
        } else {
            assert(false); // TODO: Error
        }
    }

    return true;
}

bool parameter_declaration(parser_t* parser, ast_node_t* node) {
    if (declaration_specifiers(parser, node)) {
        declarator(parser, node) || abstract_declarator(parser, node);
        return true;
    } else {
        return false;
    }
}

bool identifier_list(parser_t* parser, ast_node_t* node) {
    if (accept(parser, TK_IDENTIFIER, NULL)) {
        while (accept(parser, TK_COMMA, NULL)) {
            require(parser, TK_IDENTIFIER, NULL);
        }
        return true;
    } else {
        return false;
    }
}

bool type_name(parser_t* parser, ast_node_t* node) {
    if (specifier_qualifier_list(parser, node)) {
        abstract_declarator(parser, node);
        return true;
    } else {
        return false;
    }
}

bool abstract_declarator(parser_t* parser, ast_node_t* node) {
    if (pointer(parser, node)) {
        direct_abstract_declarator(parser, node);
        return true;
    } else {
        return direct_abstract_declarator(parser, node);
    }
}

// The rule as defined in the c99 standard is left-recursive, and a bit ambiguous.
// Fixing it looks hard so leaving this for later.
bool direct_abstract_declarator(parser_t* parser, ast_node_t* node) {
    assert(false); // TODO: Implement
}

bool tyedef_name(parser_t* parser, ast_node_t* node) {
    if (accept(parser, TK_IDENTIFIER, NULL)) {
        return true;
    } else {
        return false;
    }
}

bool initializer(parser_t* parser, ast_node_t* node) {
    node->position = parser->token.position;

    ast_node_t* temp = malloc(sizeof(ast_node_t));
    if (accept(parser, TK_LBRACE, NULL)) {
        node->type = AST_INITIALIZER;
        node->initializer.type = INITIALIZER_LIST;
        initializer_list(parser, temp); // TODO: error if not matched
        node->initializer.initializer_list = temp;
        require(parser, TK_RBRACE, NULL);
        return true;
    } else if (assignment_expression(parser, temp)) {
        node->type = AST_INITIALIZER;
        node->initializer.type = INITIALIZER_EXPRESSION;
        node->initializer.expression = temp;
        return true;
    } else {
        free(temp);
        return false;
    }
}

bool initializer_list(parser_t* parser, ast_node_t* node) {
    assert(false); // TODO: Implement
}

bool designation(parser_t* parser, ast_node_t* node) {
    assert(false); // TODO: Implement
}

bool designator_list(parser_t* parser, ast_node_t* node) {
    assert(false); // TODO: Implement
}

bool designator(parser_t* parser, ast_node_t* node) {
    assert(false); // TODO: Implement
}

// Statements

bool statement(parser_t* parser, ast_node_t* node) {
    if (labeled_statement(parser, node)) {
        return true;
    } else if (compound_statement(parser, node)) {
        return true;
    } else if (expression_statement(parser, node)) {
        return true;
    } else if (selection_statement(parser, node)) {
        return true;
    } else if (iteration_statement(parser, node)) {
        return true;
    } else if (jump_statement(parser, node)) {
        return true;
    } else {
        return false;
    }
}

bool labeled_statement(parser_t* parser, ast_node_t* node) {
    if (accept(parser, TK_IDENTIFIER, NULL)) {
        require(parser, TK_COLON, NULL);
        statement(parser, node);
        return true;
    } else if (accept(parser, TK_CASE, NULL)) {
        expression(parser, node);
        require(parser, TK_COLON, NULL);
        statement(parser, node);
        return true;
    } else if (accept(parser, TK_DEFAULT, NULL)) {
        require(parser, TK_COLON, NULL);
        statement(parser, node);
        return true;
    } else {
        return false;
    }
}

bool compound_statement(parser_t* parser, ast_node_t* node) {
    node->position = parser->token.position;
    if (accept(parser, TK_LBRACE, NULL)) {
        ast_node_vector_t block_items = {NULL, 0, 0};
        ast_node_t* block_item_node = malloc(sizeof(ast_node_t));
        while (block_item(parser, block_item_node)) {
            append_ptr((void***)&block_items.buffer,
                       &block_items.size,
                       &block_items.capacity,
                       block_item_node);
            block_item_node = malloc(sizeof(ast_node_t));
        }
        shrink_ptr_vector((void***)&block_items.buffer,
                          &block_items.size,
                          &block_items.capacity);
        free(block_item_node);

        node->type = AST_COMPOUND_STATEMENT;
        node->compound_statement.block_items = block_items;

        require(parser, TK_RBRACE, NULL);
        return true;
    } else {
        return false;
    }
}

bool block_item_list(parser_t* parser, ast_node_t* node) {
    if (block_item(parser, node)) {
        block_item_list(parser, node);
        return true;
    } else {
        return false;
    }
}

bool block_item(parser_t* parser, ast_node_t* node) {
    if (declaration(parser, node)) {
        return true;
    } else if (statement(parser, node)) {
        return true;
    } else {
        return false;
    }
}

bool expression_statement(parser_t* parser, ast_node_t* node) {
    if (accept(parser, TK_SEMICOLON, NULL)) {
        return true;
    } else if (expression(parser, node)) {
        require(parser, TK_SEMICOLON, NULL);
        return true;
    } else {
        return false;
    }
}

bool selection_statement(parser_t* parser, ast_node_t* node) {
    if (accept(parser, TK_IF, NULL)) {
        require(parser, TK_LPAREN, NULL);
        expression(parser, node);
        require(parser, TK_RPAREN, NULL);
        statement(parser, node);
        if (accept(parser, TK_ELSE, NULL)) {
            statement(parser, node);
        }
        return true;
    } else if (accept(parser, TK_SWITCH, NULL)) {
        require(parser, TK_LPAREN, NULL);
        expression(parser, node);
        require(parser, TK_RPAREN, NULL);
        statement(parser, node);
        return true;
    } else {
        return false;
    }
}

bool iteration_statement(parser_t* parser, ast_node_t* node) {
    if (accept(parser, TK_WHILE, NULL)) {
        require(parser, TK_LPAREN, NULL);
        expression(parser, node);
        require(parser, TK_RPAREN, NULL);
        statement(parser, node);
        return true;
    } else if (accept(parser, TK_DO, NULL)) {
        statement(parser, node);
        require(parser, TK_WHILE, NULL);
        require(parser, TK_LPAREN, NULL);
        expression(parser, node);
        require(parser, TK_RPAREN, NULL);
        require(parser, TK_SEMICOLON, NULL);
        return true;
    } else if (accept(parser, TK_FOR, NULL)) {
        require(parser, TK_LPAREN, NULL);
        if (expression(parser, node)) {
            require(parser, TK_SEMICOLON, NULL);
        } else {
            if(!declaration(parser, node)) {
                // TODO: error
                assert(false);
            }
        }
        if (expression(parser, node)) {
            require(parser, TK_SEMICOLON, NULL);
        }
        if (expression(parser, node)) {
            require(parser, TK_SEMICOLON, NULL);
        }
        require(parser, TK_RPAREN, NULL);
        statement(parser, node);
        return true;
    } else {
        return false;
    }
}

bool jump_statement(parser_t* parser, ast_node_t* node) {
    node->position = parser->token.position;
    if (accept(parser, TK_GOTO, NULL)) {
        token_t ident;
        require(parser, TK_IDENTIFIER, &ident);
        require(parser, TK_SEMICOLON, NULL);
        node->type = AST_JUMP_STATEMENT;
        node->jump_statement.type = JMP_GOTO;
        node->jump_statement._goto.identifier.name = ident.value;
        return true;
    } else if (accept(parser, TK_CONTINUE, NULL)) {
        require(parser, TK_SEMICOLON, NULL);
        node->type = AST_JUMP_STATEMENT;
        node->jump_statement.type = JMP_CONTINUE;
        return true;
    } else if (accept(parser, TK_BREAK, NULL)) {
        require(parser, TK_SEMICOLON, NULL);
        node->type = AST_JUMP_STATEMENT;
        node->jump_statement.type = JMP_BREAK;
        return true;
    } else if (accept(parser, TK_RETURN, NULL)) {
        ast_node_t* expression_node = malloc(sizeof (ast_node_t));
        if (!expression(parser, expression_node)) {
            free(expression_node);
            expression_node = NULL;
        }
        require(parser, TK_SEMICOLON, NULL);
        node->type = AST_JUMP_STATEMENT;
        node->jump_statement.type = JMP_RETURN;
        node->jump_statement._return.expression = expression_node;
        return true;
    } else {
        return false;
    }
}

// External definitions

bool translation_unit(parser_t* parser, ast_node_t* node) {
    node->position = parser->token.position;

    ast_node_vector_t external_declarations = {NULL, 0, 0};
    ast_node_t* external_declaration_node = malloc(sizeof (ast_node_t));
    while (external_declaration(parser, external_declaration_node)) {
        append_ptr((void***) &external_declarations.buffer,
                   &external_declarations.size,
                   &external_declarations.capacity,
                   external_declaration_node);
        external_declaration_node = malloc(sizeof (ast_node_t));
    }
    free(external_declaration_node);

    node->type = AST_TRANSLATION_UNIT;
    node->translation_unit.external_declarations = external_declarations;

    return true;
}

// This production is ambiguous. A function definition is just a specific type of declaration
// followed by an optional declaration list and then a compound statement (instead of a semicolon).
//
// <external-declaration> ::= <function-definition> | <declaration>
bool external_declaration(parser_t* parser, ast_node_t* node) {
    node->position = parser->token.position;

    ast_node_t* declaration_specifiers_node = malloc(sizeof (ast_node_t));
    if (!declaration_specifiers(parser, declaration_specifiers_node)) {
        free(declaration_specifiers_node);
        return false;
    }

    ast_node_t* init_declarator_list_node = malloc(sizeof (ast_node_t));
    if (!init_declarator_list(parser, init_declarator_list_node)) {
        // This is a declaration with no declarators (e.g. "int;").
        free(init_declarator_list_node);
        require(parser, TK_SEMICOLON, NULL);
        node->type = AST_DECLARATION;
        node->declaration.declaration_specifiers = declaration_specifiers_node;
        node->declaration.init_declarators = NULL;
        return true;
    }

    if (accept(parser, TK_SEMICOLON, NULL)) {
        // This is a declaration
        node->type = AST_DECLARATION;
        node->declaration.declaration_specifiers = declaration_specifiers_node;
        node->declaration.init_declarators = init_declarator_list_node;
        return true;
    }

    // This is a function definition (or an error).
    // Verify that the declarator is a function declarator.
    assert(init_declarator_list_node->type == AST_INIT_DECLARATOR_LIST);
    if (init_declarator_list_node->init_declarator_list.size != 1) {
        // Parse error, expected <declarator>
        fprintf(stderr, "Parse error: expected <declarator> in function definition\n");
        exit(1); // TODO: error handling
    }
    if (init_declarator_list_node->init_declarator_list.buffer[0]->init_declarator.initializer != NULL) {
        // Expected just a <declarator>, not a full <init-declarator>
        fprintf(stderr, "Parse error: unexpected initializer in function definition\n");
        exit(1);
    }
    ast_node_t* declarator_node = init_declarator_list_node->init_declarator_list.buffer[0]->init_declarator.declarator;
    free(init_declarator_list_node->init_declarator_list.buffer);
    free(init_declarator_list_node);
    if (declarator_node->declarator.direct_declarator->direct_declarator.type != DECL_FUNCTION) {
        fprintf(stderr, "Parse error: expected function declarator in function definition\n");
        exit(1);
    }
    // TODO: more verification

    // TODO: add support for declaration-list in function definition
    ast_node_t* declaration_list_node = NULL;
    // declaration_list(parser, node);

    ast_node_t* compound_statement_node = malloc(sizeof (ast_node_t));
    if (!compound_statement(parser, compound_statement_node)) {
        free(compound_statement_node);
        fprintf(stderr, "Parse error: expected <compound-statement> in function definition\n");
        exit(1);
    }

    node->type = AST_FUNCTION_DEFINITION;
    node->function_definition.declaration_specifiers = declaration_specifiers_node;
    node->function_definition.declarator = declarator_node;
    node->function_definition.declaration_list = declaration_list_node;
    node->function_definition.compound_statement = compound_statement_node;
    return true;
}

bool declaration_list(parser_t* parser, ast_node_t* node) {
    // TODO: implement
    return false;
}