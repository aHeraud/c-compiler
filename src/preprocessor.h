#ifndef C_COMPILER_PREPROCESSOR_H
#define C_COMPILER_PREPROCESSOR_H

#include "lexer.h"

typedef struct MacroParameters {
    token_vector_t* list;
    size_t size;
    size_t capacity;
} macro_parameters_t;

void preprocessor_directive(struct Lexer* lexer, token_t* token);
void preprocessor_include(struct Lexer* lexer);
void preprocessor_define(struct Lexer* lexer, struct MacroDefinition* macro_definition);
void preprocessor_undefine(lexer_t* lexer, const char* macro_name);
void preprocessor_parse_macro_invocation_parameters(lexer_t* lexer, macro_definition_t* macro_definition, macro_parameters_t* parameters);
void preprocessor_expand_macro(lexer_t* lexer, macro_definition_t* macro_definition, macro_parameters_t parameters);

#endif //C_COMPILER_PREPROCESSOR_H
