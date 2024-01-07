#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "preprocessor.h"

void preprocessor_directive(struct Lexer* lexer, token_t* token) {
    char c = ladvance(lexer);
    assert(c == '#');

    // skip whitespace between '#' and the directive name
    char c0 = lpeek(lexer, 1);
    while (c0 == ' ' || c0 == '\t') {
        ladvance(lexer);
        c0 = lpeek(lexer, 1);
    }

    char_vector_t directive_name_vec = {malloc(32), 0, 32};
    if (!isalpha(c0) && c0 != '_') {
        fprintf(stderr, "Invalid preprocessor directive name at %s:%d:%d\n",
                lexer->input_path, lexer->position.line, lexer->position.column); // TODO: print the line
        exit(1); // TODO: error recovery
    }

    append_char(&directive_name_vec.buffer, &directive_name_vec.size, &directive_name_vec.capacity, ladvance(lexer));
    while ((c0 = lpeek(lexer, 1)) && (isalnum(c0) || c0 == '_')) {
        append_char(&directive_name_vec.buffer, &directive_name_vec.size, &directive_name_vec.capacity, ladvance(lexer));
    }
    append_char(&directive_name_vec.buffer, &directive_name_vec.size, &directive_name_vec.capacity, '\0');
    shrink_char_vector(&directive_name_vec.buffer, &directive_name_vec.size, &directive_name_vec.capacity);

    for (int i = 0; i < sizeof(PREPROCESSOR_DIRECTIVES) / sizeof(struct ReservedWord); i++) {
        struct ReservedWord directive = PREPROCESSOR_DIRECTIVES[i];
        if (strcmp(directive.word, directive_name_vec.buffer) == 0) {
            token->kind = directive.kind;
            token->value = directive_name_vec.buffer;
            token->position = lexer->position;
            return;
        }
    }

    fprintf(stderr, "%s:%d:%d: Invalid preprocessor directive '%s'\n",
            lexer->input_path, lexer->position.line, lexer->position.column, directive_name_vec.buffer);
    exit(1); // TODO: error recovery
}

// For resolving relative paths in #include directives
void get_file_prefix(const char* path, const size_t len, char buffer[len]) {
    ssize_t pathlen = strlen(path);

    // find the last '/'
    for (ssize_t i = pathlen - 1; i >= 0; i -= 1) {
        if (path[i] == '/') { // TODO: normalize path separators in #include directives
            // copy the prefix into the buffer, including the trailing '/'
            strncpy(buffer, path, i + 1 > len ? len : i + 1);
            return;
        }
        if (i == 0) {
            break;
        }
    }

    // no '/' found
    // because path is the path to the translation unit being processed, we know it is a file, meaning
    // it is in the current working directory, and the prefix is just ""
    buffer[0] = '\0';
}

void preprocessor_include(lexer_t* lexer) {
    // skip whitespace between '#include' and the file specifier
    while (lpeek(lexer, 1) == ' ' || lpeek(lexer, 1) == '\t') {
        ladvance(lexer);
    }

    source_position_t filename_start = lexer->position;
    char start = lpeek(lexer, 1);
    char end;
    if (start == '"') {
        end = '"';
    } else if (start == '<') {
        end = '>';
    } else {
        fprintf(stderr, "%s:%d:%d: error: expected \"FILE\" or <FILE> following '#include' directive\n",
                lexer->input_path, lexer->position.line, lexer->position.column); // TODO: print the line
        exit(1); // TODO: error recovery
    }

    ladvance(lexer); // consume the opening symbol ('"' or '<')

    char_vector_t filename = {malloc(32), 0, 32};
    char c0;
    while ((c0 = lpeek(lexer, 1)) && c0 != end && c0 != '\n') {
        append_char(&filename.buffer, &filename.size, &filename.capacity, ladvance(lexer));
    }
    append_char(&filename.buffer, &filename.size, &filename.capacity, '\0');
    shrink_char_vector(&filename.buffer, &filename.size, &filename.capacity);

    if (c0 != end) {
        fprintf(stderr, "%s:%d:%d: error: missing terminating '%c' character\n",
                lexer->input_path, lexer->position.line, lexer->position.column, end);
        exit(1); // TODO: error recovery
    } else {
        ladvance(lexer); // consume the closing symbol ('"' or '>')
    }

    // TODO: consume the rest of the line, error if there's anything other than whitespace or a comment

    // #include path resolution:
    // 1. If double-quoted, search in the directory containing the current file
    // 2. Search in additional include directories included as command-line arguments (if any were supplied)
    // 3. Search in the standard system include directories

    bool search_current_dir = end == '"';
    FILE *fp = NULL;
    if (search_current_dir) {
        char prefix[256];
        get_file_prefix(lexer->input_path, 256, prefix);
        char_vector_t path = {malloc(32), 0, 32};
        append_chars(&path.buffer, &path.size, &path.capacity, prefix);
        append_chars(&path.buffer, &path.size, &path.capacity, filename.buffer);
        shrink_char_vector(&path.buffer, &path.size, &path.capacity);
        fp = fopen(path.buffer, "r");
    }

    lexer_global_context_t* global_context = lexer->global_context;
    for(int i = 0; global_context->user_include_paths != NULL && i < global_context->user_include_paths->size; i += 1) {
        if (fp != NULL) {
            break;
        }

        char_vector_t path = {malloc(32), 0, 32};
        append_chars(&path.buffer, &path.size, &path.capacity, lexer->global_context->user_include_paths->buffer[i]);
        if (path.size > 0 && path.buffer[path.size - 1] != '/') {
            append_char(&path.buffer, &path.size, &path.capacity, '/');
        }
        append_chars(&path.buffer, &path.size, &path.capacity, filename.buffer);
        fp = fopen(path.buffer, "r");
    }

    for(int i = 0; global_context->system_include_paths != NULL && i < global_context->system_include_paths->size; i += 1) {
        if (fp != NULL) {
            break;
        }

        char_vector_t path = {malloc(32), 0, 32};
        append_chars(&path.buffer, &path.size, &path.capacity, lexer->global_context->system_include_paths->buffer[i]);
        if (path.size > 0 && path.buffer[path.size - 1] != '/') {
            append_char(&path.buffer, &path.size, &path.capacity, '/');
        }
        append_chars(&path.buffer, &path.size, &path.capacity, filename.buffer);
        fp = fopen(path.buffer, "r");
    }

    if (fp == NULL) {
        fprintf(stderr, "%s:%d:%d: Failed to open file: %s\n",
                filename_start.path, filename_start.line, filename_start.column, filename.buffer);
        exit(1);
    }

    // File inclusion is handled recursively by creating a new nested lexer for the included file
    char* source_buffer = NULL;
    size_t len = 0;
    ssize_t bytes_read = getdelim( &source_buffer, &len, '\0', fp);
    if (bytes_read < 0) {
        fprintf(stderr, "%s:%d:%d: No such file or directory: %s\n",
                filename_start.path, filename_start.line, filename_start.column, filename.buffer);
        exit(1);
    }
    fclose(fp);

    lexer_t* child = malloc(sizeof(lexer_t));
    *child = linit(filename.buffer, source_buffer, bytes_read, lexer->global_context);
    lexer->child = child;
}

void identifier(struct Lexer* lexer, struct Token* token) {
    struct SourcePosition match_start = {lexer->position.path, lexer->position.line, lexer->position.column};
    struct CharVector buffer = {malloc(32), 0, 32};

    char c = ladvance(lexer);
    assert(isalpha(c) || c == '_');
    append_char(&buffer.buffer, &buffer.size, &buffer.capacity, c);

    while ((c = lpeek(lexer, 1)) && (isalnum(c) || c == '_' )) {
        append_char(&buffer.buffer, &buffer.size, &buffer.capacity, ladvance(lexer));
    }
    append_char(&buffer.buffer, &buffer.size, &buffer.capacity, '\0');

    token->kind = TK_IDENTIFIER;
    token->value = realloc(buffer.buffer, buffer.size);
    token->position = match_start;
}

/**
 * Macro definitions
 *
 * Macro definitions have two forms:
 * 1. Parameterless macros: '#define' <identifier> <tokens>*
 * 2. Parameterized macros: '#define' <identifier> <parameter-list> <tokens>*
 *    - If there is a space between the macro identifier and the opening parenthesis, it is interpreted to be a
 *      parameterless macro.
 *    - <parameter-list> is a comma-separated list of identifiers (can be empty)
 *       - <parameter-list> ::= '(' <identifier-list> ')'
 *                            |  '(' <identifier-list> ',' '...' ')' // variadic macro
 *                            |  '(' '...' ')' // variadic macro
 *         <identifier-list> ::= <identifier> | <identifier-list> ',' <identifier>
 *
 * Special operators:
 * 1. '#' - stringification operator - converts a token into a string literal (escapes quotes and backslashes in the token)
 *    Example: #define STRINGIFY(x) #x -> STRINGIFY(printf("hello world")) -> "printf(\"hello world\")"
 * 2. '##' - token pasting operator - concatenates two tokens into a single token
 *    Example: #define PASTE(x, y) x ## y -> PASTE(print, f("hello world")) -> printf("hello world")
 *
 * @param lexer
 */
void preprocessor_define(lexer_t* lexer, macro_definition_t* macro_definition) {
    lexer->global_context->disable_macro_expansion = true;

    // skip whitespace between '#define' and the macro name
    while (lpeek(lexer, 1) == ' ' || lpeek(lexer, 1) == '\t') {
        ladvance(lexer);
    }

    token_t macro_name;
    identifier(lexer, &macro_name);

    bool variadic = false;
    token_vector_t parameter_list = {NULL, 0, 0};
    if (lpeek(lexer, 1) == '(') {
        // parameterized macro, parse the parameter list
        token_t token = lscan(lexer); // consume the '('

        while ((token = lscan(lexer)).kind != TK_EOF) {
            if (token.kind == TK_RPAREN) {
                break;
            }

            if (variadic) {
                fprintf(stderr, "%s:%d:%d: error: '...' must be the final token in the parameter list for a variadic macro\n",
                        lexer->input_path, lexer->position.line, lexer->position.column); // TODO: print the line
                exit(1); // TODO: error recovery
            }

            if (token.kind == TK_ELLIPSIS) {
                variadic = true;
            } else if (token.kind == TK_IDENTIFIER) {
                append_token(&parameter_list.buffer, &parameter_list.size, &parameter_list.capacity, token);
                token = lscan(lexer);
                if (token.kind == TK_COMMA) {
                    continue;
                } else if (token.kind == TK_RPAREN) {
                    break;
                } else {
                    fprintf(stderr, "%s:%d:%d: error: unexpected token '%s' following identifier in macro parameter list\n",
                            lexer->input_path, lexer->position.line, lexer->position.column, token.value); // TODO: print the line
                    exit(1); // TODO: error recovery
                }
            } else {
                fprintf(stderr, "%s:%d:%d: error: unexpected token '%s' in macro parameter list\n",
                        lexer->input_path, lexer->position.line, lexer->position.column, token.value); // TODO: print the line
                exit(1); // TODO: error recovery
            }
        }
    }

    // Consume the rest of the line
    // TODO: does this handle line continuations?
    token_vector_t tokens = {NULL, 0, 0};
    while (lpeek(lexer, 1) != '\n') {
        token_t token = lscan(lexer);
        append_token(&tokens.buffer, &tokens.size, &tokens.capacity, token);
    }

    macro_definition->name = macro_name.value;
    macro_definition->parameters = parameter_list;
    macro_definition->tokens = tokens;
    macro_definition->variadic = variadic;

    lexer->global_context->disable_macro_expansion = false;
}

void preprocessor_undefine(lexer_t* lexer, const char* macro_name) {
    hash_table_t* macro_definitions = &lexer->global_context->macro_definitions;
    void* value;
    if (hash_table_remove(macro_definitions, macro_name, &value)) {
        macro_definition_t* macro_definition = value;
        free(macro_definition->parameters.buffer);
        free(macro_definition->tokens.buffer);
        free(macro_definition);
    }
}

void append_macro_parameter(macro_parameters_t* parameters, token_vector_t parameter) {
    if (parameters->size + 1 >= parameters->capacity) {
        parameters->capacity > 0 ? (parameters->capacity *= 2) : (parameters->capacity = 1);
        parameters->list = realloc(parameters->list, parameters->capacity * sizeof(token_vector_t));
        assert(parameters->list != NULL);
    }

    parameters->list[parameters->size] = parameter;
    parameters->size += 1;
}

/**
 * Parse the parameter list of a macro invocation.
 *
 * @param lexer Lexer context
 * @param macro_definition Macro definition being invoked
 * @param parameters Output parameter list
 */
void preprocessor_parse_macro_invocation_parameters(lexer_t* lexer, macro_definition_t* macro_definition, macro_parameters_t* parameters) {
    assert(parameters != NULL);
    *parameters = (macro_parameters_t) { .list = NULL, .size = 0, .capacity = 0};

    if (lpeek(lexer, 1) != '(' || (macro_definition->parameters.size == 0 && macro_definition->variadic)) {
        // no parameters
        return;
    }

    token_t token = lscan(lexer); // consume the '('
    int lp_count = 1; // number of '(' tokens seen so far (to handle nested parentheses)

    token_vector_t parameter = {NULL, 0, 0};
    while ((token = lscan(lexer)).kind != TK_EOF) {
        if (token.kind == TK_LPAREN) {
            lp_count += 1;
        } else if (token.kind == TK_RPAREN) {
            if (lp_count <= 1) {
                // end of the parameter list
                append_macro_parameter(parameters, parameter);
                break;
            } else {
                // the token is part of the current argument
                lp_count -= 1;
            }
        } else if (token.kind == TK_COMMA && lp_count == 1) {
            // end of a parameter
            append_macro_parameter(parameters, parameter);
            parameter = (token_vector_t) {NULL, 0, 0};
            continue;
        }

        // part of the current parameter
        append_token(&parameter.buffer, &parameter.size, &parameter.capacity, token);
    }
}

/**
 * Pre-processor macro replacement/expansion
 *
 * Special pre-processor tokens:
 * 1. '#' - stringification operator - converts a token into a string literal
 * 2. '##' - token pasting operator - concatenates two tokens into a single token
 *
 * Macro expansion is performed in the following order:
 * 1. Stringification
 * 2. Parameter replacement
 *   - If the token is a macro parameter, it is replaced with the corresponding argument
 *   - If the token is the special variadic macro parameter '__VA_ARGS__', it is replaced with the remaining arguments
 *     that appear after the last named parameter (including commas).
 * 3. Concatenation
 * 4. Tokens originating from parameters are expanded
 * 5. The result is rescanned for more macro invocations
 *
 * @param lexer Lexer context
 * @param macro_definition Macro definition to expand
 * @param parameters Macro parameters (if any)
 * @return
 */
void preprocessor_expand_macro(lexer_t* lexer, macro_definition_t* macro_definition, macro_parameters_t parameters) {
    assert(macro_definition != NULL);
    if (macro_definition->variadic) {
        assert(parameters.size > macro_definition->parameters.size);
    } else {
        assert(macro_definition->parameters.size > 0 ?
               parameters.size == macro_definition->parameters.size : parameters.size == 0);
    }

    if (macro_definition->tokens.size == 0) {
        // empty macro definition, so nothing to expand/replace
        return;
    }

    // Convert the macro definition's tokens into a doubly-linked list to simplify stringification and concatenation.
    token_node_t* head = malloc(sizeof(token_node_t));
    *head = (token_node_t) {.token = macro_definition->tokens.buffer[0], .prev = NULL, .next = NULL};
    token_node_t* cur = head;
    for (int i = 1; i < macro_definition->tokens.size; i += 1) {
        token_t token = macro_definition->tokens.buffer[i];
        token_node_t* node = malloc(sizeof(token_node_t));
        *node = (token_node_t) {.token = token, .prev = cur, .next = NULL};
        cur->next = node;
        cur = node;
    }

    // Step 1: Stringification
    // The token following a '#' is converted to a string literal, the token to be stringified must be a macro parameter
    // If there is no token following the '#', then it is a compilation error
    // Any quotes or backslashes in the token are escaped
    cur = head;
    while (cur != NULL) {
        if (cur->token.kind == TK_HASH) {
            if (cur->next == NULL || cur->next->token.kind != TK_IDENTIFIER) {
                // TODO: error recovery
                fprintf(stderr, "%s:%d:%d: error: '#' must be followed by a macro parameter name\n",
                        cur->token.position.path, cur->token.position.line, cur->token.position.column);
                exit(1);
            }

            token_t parameter = cur->next->token;
            int parameter_index = -1;
            for(int j = 0; j < macro_definition->parameters.size; j += 1) {
                if (strcmp(macro_definition->parameters.buffer[j].value, parameter.value) == 0) {
                    parameter_index = j;
                    break;
                }
            }
            if (parameter_index == -1) {
                // TODO: error recovery
                fprintf(stderr, "%s:%d:%d: error: '#' must be followed by a macro parameter\n",
                        cur->token.position.path, cur->token.position.line, cur->token.position.column);
                exit(1);
            }

            token_vector_t parameter_value = parameters.list[parameter_index];
            char_vector_t vec = {malloc(32), 0, 32};
            for (size_t token_index = 0; token_index < parameter_value.size; token_index += 1) {
                token_t token = parameter_value.buffer[token_index];
                for (int j = 0; j < strlen(token.value); j += 1) {
                    char c = token.value[j];
                    if (c == '"' || c == '\\') {
                        append_char(&vec.buffer, &vec.size, &vec.capacity, '\\');
                    }
                    append_char(&vec.buffer, &vec.size, &vec.capacity, c);
                }
            }
            append_char(&vec.buffer, &vec.size, &vec.capacity, '\0');
            shrink_char_vector(&vec.buffer, &vec.size, &vec.capacity);

            // replace cur ('#') and next (macro parameter) with the stringified parameter
            token_node_t* parent = cur->prev;
            token_node_t* rest = cur->next->next; // can be null
            token_node_t* replacement = malloc(sizeof(token_node_t));
            *replacement = (token_node_t) {.token = (token_t) {.kind = TK_STRING_LITERAL, .value = vec.buffer, .position = parameter.position}, .prev = parent, .next = NULL};

            if (parent != NULL) {
                parent->next = replacement;
            } else {
                head = replacement;
            }

            if (rest != NULL) {
                rest->prev = replacement;
            }

            free(cur->next);
            free(cur);
            cur = rest;
        } else {
            cur = cur->next;
        }
    }

    // Step 2: Parameter replacement
    cur = head;
    while (cur != NULL) {
        if (cur->token.kind == TK_IDENTIFIER) {
            bool is_variadic = false;
            token_vector_t parameter_value;
            if (strcmp("__VA_ARGS__", cur->token.value) == 0) {
                if (!macro_definition->variadic) {
                    fprintf(stderr, "%s:%d:%d: error: '__VA_ARGS__' can only be used in a variadic macro\n",
                            cur->token.position.path, cur->token.position.line, cur->token.position.column);
                    exit(1);
                }
                is_variadic = true;
                parameter_value = (token_vector_t) {.buffer = NULL, .size = 0, .capacity = 0};
                for (size_t i = macro_definition->parameters.size; i < parameters.size; i += 1) {
                    if (parameter_value.size > 0) {
                        append_token(&parameter_value.buffer, &parameter_value.size, &parameter_value.capacity, (token_t) {.kind = TK_COMMA, .value = ",", .position = cur->token.position});
                    }
                    append_token(&parameter_value.buffer, &parameter_value.size, &parameter_value.capacity, parameters.list[i].buffer[0]);
                }
            } else {
                int parameter_index = -1;
                for(int j = 0; j < macro_definition->parameters.size; j += 1) {
                    if (strcmp(macro_definition->parameters.buffer[j].value, cur->token.value) == 0) {
                        parameter_index = j;
                        break;
                    }
                }
                if (parameter_index == -1) {
                    cur = cur->next;
                    continue;
                } else {
                    parameter_value = parameters.list[parameter_index];
                }
            }

            cur->token = parameter_value.buffer[0]; // replace the parameter with the first token in the list
            for (size_t token_index = 1; token_index < parameter_value.size; token_index += 1) {
                // insert any remaining tokens into the list
                token_t token = parameter_value.buffer[token_index];
                token_node_t* node = malloc(sizeof(token_node_t));
                *node = (token_node_t) {.token = token, .prev = cur, .next = cur->next};
                cur->next = node;
                cur = node;
            }

            if (cur->next != NULL) {
                cur->next->prev = cur;
            }
        }
        cur = cur->next;
    }

    // Step 3: Concatenation
    cur = head;
    while (cur != NULL) {
        if (cur->token.kind == TK_DOUBLE_HASH) {
            if (cur->prev == NULL || cur->next == NULL) {
                fprintf(stderr, "%s:%d:%d: error: '##' cannot be at the beginning or end of a macro definition\n",
                        cur->token.position.path, cur->token.position.line, cur->token.position.column);
                exit(1);
            }

            // concat prev and next together, and then re-lex the result
            char_vector_t vec = {malloc(32), 0, 32};
            append_chars(&vec.buffer, &vec.size, &vec.capacity, cur->prev->token.value);
            append_chars(&vec.buffer, &vec.size, &vec.capacity, cur->next->token.value);
            shrink_char_vector(&vec.buffer, &vec.size, &vec.capacity);

            lexer_t child_lexer = linit("<macro expansion>", vec.buffer, vec.size, lexer->global_context);
            token_t token = lscan(&child_lexer);
            if (lscan(&child_lexer).kind != TK_EOF) {
                fprintf(stderr, "%s:%d:%d: error: concatenating \"%s\" and \"%s\" does not result in a valid token\n",
                        cur->token.position.path, cur->token.position.line, cur->token.position.column, cur->prev->token.value, cur->next->token.value);
                exit(1);
            }

            // remove prev, current ("##"), and next tokens from the list, and replace with the new token
            cur->prev->token = token;
            cur->prev->next = cur->next->next;
            cur = cur->next->next;
        } else {
            cur = cur->next;
        }
    }

    // Step 4: Parameter expansion
    // TODO

    // Step 5: Rescan
    // TODO

    // Store the resulting tokens in the lexer
    lexer->pending_tokens = head;
}

/**
 * Pre-processor __FILE__ substitution.
 * Expands to the path of the translation unit being processed.
 * @param lexer Lexer context
 * @param token The token containing the __FILE__ directive.
 * @return A new token containing the path of the translation unit being processed.
 */
token_t preprocessor_file_replacement(lexer_t *lexer, token_t *token) {
    return (token_t) {
        .position = token->position,
        .kind = TK_STRING_LITERAL,
        .value = lexer->input_path
    };
}

/**
 * Pre-processor __LINE__ substitution.
 * Expands to the line number of the current token.
 * @param lexer Lexer context
 * @param token The token containing the __LINE__ directive.
 * @return A new token containing the line number of the current token.
 */
token_t preprocessor_line_replacement(lexer_t *lexer, token_t *token) {
    char_vector_t vec = {malloc(32), 0, 32};
    snprintf(vec.buffer, 32, "%d", lexer->position.line);
    vec.size = strlen(vec.buffer);
    shrink_char_vector(&vec.buffer, &vec.size, &vec.capacity);
    return (token_t) {
        .position = token->position,
        .kind = TK_INTEGER_CONSTANT,
        .value = vec.buffer,
    };
}
