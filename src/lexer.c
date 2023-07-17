#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>

#include "lexer.h"
#include "util/vectors.h"

struct ReservedWord {
    char* word;
    enum TokenKind kind;
};

struct ReservedWord RESERVED_WORDS[] = {
//        {"auto", TK_AUTO},
        {"break",    TK_BREAK},
        {"case",     TK_CASE},
        {"char",     TK_CHAR},
        {"const",    TK_CONST},
        {"continue", TK_CONTINUE},
        {"default",  TK_DEFAULT},
        {"do",       TK_DO},
        {"double",   TK_DOUBLE},
        {"else",     TK_ELSE},
        {"enum",     TK_ENUM},
        {"extern",   TK_EXTERN},
        {"float",    TK_FLOAT},
        {"for",      TK_FOR},
        {"goto",     TK_GOTO},
        {"if",       TK_IF},
        {"inline",   TK_INLINE},
        {"int",      TK_INT},
        {"long",     TK_LONG},
//        {"register", TK_REGISTER},
//        {"restrict", TK_RESTRICT},
        {"return",   TK_RETURN},
        {"short",    TK_SHORT},
        {"signed",   TK_SIGNED},
        {"sizeof", TK_SIZEOF},
        {"static", TK_STATIC},
        {"struct",   TK_STRUCT},
        {"switch",   TK_SWITCH},
        {"typedef",  TK_TYPEDEF},
        {"union",    TK_UNION},
        {"unsigned", TK_UNSIGNED},
        {"void",     TK_VOID},
//        {"volatile", TK_VOLATILE},
        {"while",    TK_WHILE},
//        {"_Alignas", TK_ALIGNAS},
//        {"_Alignof", TK_ALIGNOF},
//        {"_Atomic", TK_ATOMIC},
//        {"_Bool", TK_BOOL},
//        {"_Complex", TK_COMPLEX},
//        {"_Generic", TK_GENERIC},
//        {"_Imaginary", TK_IMAGINARY},
//        {"_Noreturn", TK_NORETURN},
//        {"_Static_assert", TK_STATIC_ASSERT},
//        {"_Thread_local", TK_THREAD_LOCAL}
};

struct ReservedWord PREPROCESSOR_DIRECTIVES[] = {
        {"include", TK_PP_INCLUDE},
        {"define", TK_PP_DEFINE},
        {"undef", TK_PP_UNDEF},
        {"ifdef", TK_PP_IFDEF},
        {"line", TK_PP_LINE}
};

void string_literal(struct Lexer* lexer, struct Token* token);
void char_literal(struct Lexer* lexer, struct Token* token);
void integer_constant(struct Lexer* lexer, struct Token* token);
void identifier_or_reserved_word(struct Lexer* lexer, struct Token* token);
void comment(struct Lexer* lexer, token_t* token);
void preprocessor_directive(struct Lexer* lexer, token_t* token);
void preprocessor_include(struct Lexer* lexer);

void append_token(token_t** buffer, size_t *size, size_t* capacity, token_t token) {
    if (*size + 1 > *capacity) {
        *capacity > 0 ? (*capacity *= 2) : (*capacity = 1);
        *buffer = realloc(*buffer, *capacity * sizeof(token_t));
        assert(buffer != NULL);
    }

    (*buffer)[*size] = token;
    *size += 1;
}

char ladvance(struct Lexer* lexer) {
    if (lexer->input_offset >= lexer->input_len) {
        return '\0';
    }

    char c0 = lexer->input[lexer->input_offset++];
    char c1 = (c0 == '\0') ? '\0' : (lexer->input)[lexer->input_offset]; // lookahead 1
    char c2 = (c1 == '\0') ? '\0' : (lexer->input)[lexer->input_offset + 1]; // lookahead 2

    if (c0 == '\n' || c0 == '\r') {
        // normalize line endings as '\n'
        if ((c0 == '\n' && c1 == '\r') || (c0 == '\r' && c1 == '\n')) {
            // skip the second character of the line ending
            lexer->input_offset++;
        }
        lexer->position.line++;
        lexer->position.column = 0;
        return '\n';
    } else if (c0 == '\\' && (c1 == '\n' || c1 == '\r')) {
        // Handle line continuations
        if ((c1 == '\n' && c2 == '\r') || (c1 == '\r' && c2 == '\n')) {
            // skip the second character of the line ending
            lexer->input_offset++;
        }
        lexer->position.line++;
        lexer->position.column = 0;
        return ladvance(lexer);
    } else {
        lexer->position.column++;
        return c0;
    }
}

char lpeek(struct Lexer* lexer, unsigned int count) {
    assert(count > 0);
    source_position_t pos = lexer->position; // save position
    size_t input_offset = lexer->input_offset; // save input offset
    char c;
    for (unsigned int i = 0; i < count; i++) {
        c = ladvance(lexer);
        if (c == '\0') {
            break;
        }
    }
    lexer->position = pos; // rewind
    lexer->input_offset = input_offset; // rewind
    return c;
}

lexer_t linit(
        const char* input_path,
        const char* input,
        size_t input_len,
        string_vector_t* user_include_paths,
        string_vector_t* system_include_paths
) {
    lexer_t lexer = {
        .input_path = input_path,
        .input = input,
        .input_offset = 0,
        .input_len = input_len,
        .position = {
            .path = input_path,
            .line = 1,
            .column = 0,
        },
        .user_include_paths = user_include_paths,
        .system_include_paths = system_include_paths,
    };

    if (lexer.user_include_paths == NULL) {
        lexer.user_include_paths = malloc(sizeof(string_vector_t));
        lexer.user_include_paths->buffer = NULL;
        lexer.user_include_paths->size = 0;
        lexer.user_include_paths->capacity = 0;
    }

    if (lexer.system_include_paths == NULL) {
        lexer.system_include_paths = malloc(sizeof(string_vector_t));
        lexer.system_include_paths->buffer = NULL;
        lexer.system_include_paths->size = 0;
        lexer.system_include_paths->capacity = 0;
    }

    return lexer;
}

// TODO: rename this
void lfree(lexer_t* lexer) {
    // TODO: free allocated memory for input path/input
    lexer->input = NULL;
}

token_t lscan(struct Lexer* lexer) {
    if (lexer->child != NULL) {
        token_t token = lscan(lexer->child);
        if (token.kind == TK_NONE || token.kind == TK_EOF) {
            // child lexer is done, clean it up
            lfree(lexer->child);
            free(lexer->child);
            lexer->child = NULL;
        } else {
            return token;
        }
    }

    bool start_of_line = lexer->position.column == 0;

    // skip whitespace
    char c0 = lpeek(lexer, 1);
    while (c0 == ' ' || c0 == '\t' || c0 == '\n') {
        ladvance(lexer);
        c0 = lpeek(lexer, 1);
    }

    struct Token token;
    token.kind = TK_NONE;
    token.position = lexer->position;

    c0 = lpeek(lexer, 1); // lookahead 1
    char c1 = lpeek(lexer, 2); // lookahead 2
    switch (c0) {
//        case '\r':
//        case '\n':
//            ladvance(lexer);
//            token.kind = TK_NEWLINE;
//            token.value = "\n";
//            break;
        case ';':
            ladvance(lexer);
            token.kind = TK_SEMICOLON;
            token.value = ";";
            break;
        case ':':
            ladvance(lexer);
            token.kind = TK_COLON;
            token.value = ":";
            break;
        case ',':
            ladvance(lexer);
            token.kind = TK_COMMA;
            token.value = ",";
            break;
        case '(':
            ladvance(lexer);
            token.kind = TK_LPAREN;
            token.value = "(";
            break;
        case ')':
            ladvance(lexer);
            token.kind = TK_RPAREN;
            token.value = ")";
            break;
        case '{':
            ladvance(lexer);
            token.kind = TK_LBRACE;
            token.value = "{";
            break;
        case '}':
            ladvance(lexer);
            token.kind = TK_RBRACE;
            token.value = "}";
            break;
        case '[':
            ladvance(lexer);
            token.kind = TK_LBRACKET;
            token.value = "[";
            break;
        case ']':
            ladvance(lexer);
            token.kind = TK_RBRACKET;
            token.value = "]";
            break;
        case '/':
            if (c1 == '/' || c1 == '*') {
                // comment
                comment(lexer, &token);
            } else {
                ladvance(lexer);
                token.kind = TK_SLASH;
                token.value = "/";
            }
            break;
        case '\'':
            char_literal(lexer, &token);
            break;
        case '"':
            string_literal(lexer, &token);
            break;
        case '.':
            ladvance(lexer);
            token.kind = TK_DOT;
            token.value = ".";
            break;
        case '+':
            ladvance(lexer);
            token.kind = TK_PLUS;
            token.value = "+";
            break;
        case '-':
            ladvance(lexer);
            if (lpeek(lexer, 1) == '>') {
                ladvance(lexer);
                token.kind = TK_ARROW;
                token.value = "->";
            } else {
                token.kind = TK_MINUS;
                token.value = "-";
            }
            break;
        case '*':
            ladvance(lexer);
            token.kind = TK_STAR;
            token.value = "*";
            break;
        case '=':
            ladvance(lexer);
            if (c1 == '=') {
                ladvance(lexer);
                token.kind = TK_EQUALS;
                token.value = "==";
            } else {
                token.kind = TK_ASSIGN;
                token.value = "=";
            }
            break;
        case '#':
            if (start_of_line) {
                // preprocessor directive
                token_t directive_name;
                preprocessor_directive(lexer, &directive_name);
                switch (directive_name.kind) {
                    case TK_PP_INCLUDE:
                        preprocessor_include(lexer);
                        return lscan(lexer); // scan next token
                    default:
                        fprintf(stderr, "%s:%d:%d: Unknown preprocessor directive '%s'\n",
                                directive_name.position.path, directive_name.position.line, directive_name.position.column, directive_name.value);
                        exit(1); // TODO: error recovery
                }
            } else {
                fprintf(stderr, "%s:%d:%d: Unexpected character '#' in input\n",
                        lexer->position.path, lexer->position.line, lexer->position.column);
                exit(1); // TODO: error recovery
            }
            break;
        default:
            if (isalpha(c0) || c0 == '_') {
                identifier_or_reserved_word(lexer, &token);
            } else if (isdigit(c0)) {
                // TODO: handle floats generally
                // TODO: handle floating point numbers beginning with '.' (e.g. .5f)
                // TODO: handle floats in scientific notation (e.g. 1.0e-5f)
                integer_constant(lexer, &token);
            } else if (c0 == '\0') {
                token.kind = TK_EOF;
                token.value = "EOF";
            } else {
                fprintf(stderr, "Unexpected character '%c' at %d:%d\n",
                        c0, lexer->position.line, lexer->position.column);
                exit(1);
            }
            break;
    }

    return token;
}

void string_literal(struct Lexer* lexer, struct Token* token) {
    struct SourcePosition match_start = {lexer->position.path, lexer->position.line, lexer->position.column};
    struct CharVector buffer = {malloc(512), 0, 512};
    char c = ladvance(lexer);
    assert(c == '"');
    append_char(&buffer.buffer, &buffer.size, &buffer.capacity, c);
    while ((c = ladvance(lexer)) && c != '"') {
        if (c == '\r' || c == '\n') {
            // Illegal newline in string literal
            // TODO: better error handling
            fprintf(stderr, "Illegal newline in string literal at %s:%d:%d\n",
                    lexer->input_path, lexer->position.line, lexer->position.column);
            exit(1);
        } else if (c == '\\' && lpeek(lexer, 1) == '"') {
            // Special handling for escaped double quote
            append_char(&buffer.buffer, &buffer.size, &buffer.capacity, c);
            append_char(&buffer.buffer, &buffer.size, &buffer.capacity, ladvance(lexer));
        } else {
            append_char(&buffer.buffer, &buffer.size, &buffer.capacity, c);
        }
    }

    if (c != '"') {
        // TODO: better error handling
        fprintf(stderr, "Unterminated string literal at %s:%d:%d\n",
                lexer->input_path, lexer->position.line, lexer->position.column);
        exit(1);
    }
    append_char(&buffer.buffer, &buffer.size, &buffer.capacity, c);
    token->kind = TK_STRING_LITERAL;
    token->position = match_start;
    token->value = realloc(buffer.buffer, buffer.size);
}

void char_literal(struct Lexer* lexer, struct Token* token) {
    struct SourcePosition match_start = {lexer->position.path, lexer->position.line, lexer->position.column};
    struct CharVector buffer = {malloc(4), 0, 4};
    char c = ladvance(lexer);
    assert(c == '\'');
    append_char(&buffer.buffer, &buffer.size, &buffer.capacity, c);
    while ((c = ladvance(lexer)) && c != '\'') {
        if (c == '\r' || c == '\n') {
            // Illegal newline in character literal
            // TODO: better error handling
            fprintf(stderr, "Illegal newline in character literal at %s:%d:%d\n",
                    lexer->input_path, lexer->position.line, lexer->position.column);
            exit(1);
        } else if (c == '\\' && lpeek(lexer, 1) == '\'') {
            // Special handling for escaped single quote
            append_char(&buffer.buffer, &buffer.size, &buffer.capacity, c);
            append_char(&buffer.buffer, &buffer.size, &buffer.capacity, ladvance(lexer));
        } else {
            append_char(&buffer.buffer, &buffer.size, &buffer.capacity, c);
        }
    }

    if (c != '\'') {
        // TODO: better error handling
        fprintf(stderr, "Unterminated character literal at %s:%d:%d\n",
                lexer->input_path, lexer->position.line, lexer->position.column);
        exit(1);
    }
    append_char(&buffer.buffer, &buffer.size, &buffer.capacity, c);

    token->position = match_start;
    token->kind = TK_CHAR_LITERAL;
    token->value = realloc(buffer.buffer, buffer.size);
}

void integer_constant(struct Lexer* lexer, struct Token* token) {
    struct SourcePosition match_start = {lexer->position.path, lexer->position.line, lexer->position.column};
    struct CharVector buffer = {malloc(32), 0, 32};

    // TODO: handle hex, octal, and binary literals
    char c = ladvance(lexer);
    assert(isdigit(c));

    do {
        append_char(&buffer.buffer, &buffer.size, &buffer.capacity, c);
    } while ((c = lpeek(lexer, 1)) && isdigit(c));

    append_char(&buffer.buffer, &buffer.size, &buffer.capacity, '\0');

    token->kind = TK_INTEGER_CONSTANT;
    token->position = match_start;
    token->value = realloc(buffer.buffer, buffer.size);
}

void identifier_or_reserved_word(struct Lexer* lexer, struct Token* token) {
    struct SourcePosition match_start = {lexer->position.path, lexer->position.line, lexer->position.column};
    struct CharVector buffer = {malloc(32), 0, 32};

    char c = ladvance(lexer);
    assert(isalpha(c) || c == '_');
    append_char(&buffer.buffer, &buffer.size, &buffer.capacity, c);

    while ((c = lpeek(lexer, 1)) && (isalnum(c) || c == '_' )) {
        append_char(&buffer.buffer, &buffer.size, &buffer.capacity, ladvance(lexer));
    }
    append_char(&buffer.buffer, &buffer.size, &buffer.capacity, '\0');

    // Is this a reserved word?
    for(int i = 0; i < sizeof(RESERVED_WORDS) / sizeof(struct ReservedWord); i++) {
        struct ReservedWord reserved_word = RESERVED_WORDS[i];
        char* word = reserved_word.word;
        if (strcmp(buffer.buffer, word) == 0) {
            token->kind = reserved_word.kind;
            token->value = buffer.buffer;
            token->position = match_start;
            return;
        }
    }

    token->kind = TK_IDENTIFIER;
    token->value = realloc(buffer.buffer, buffer.size);
    token->position = match_start;
}

void comment(struct Lexer* lexer, token_t* token) {
    char_vector_t buffer = {malloc(64), 0, 64};

    char c = ladvance(lexer);
    assert(c == '/');
    append_char(&buffer.buffer, &buffer.size, &buffer.capacity, c);
    c = ladvance(lexer);
    assert(c == '/' || c == '*');
    append_char(&buffer.buffer, &buffer.size, &buffer.capacity, c);

    if (c == '*') {
        while ((c = ladvance(lexer)) && c != '*' && lpeek(lexer, 1) != '/') {
            append_char(&buffer.buffer, &buffer.size, &buffer.capacity, c);
        }

        if (c != '*' || lpeek(lexer, 1) != '/') {
            // error
        }
        append_char(&buffer.buffer, &buffer.size, &buffer.capacity, c);
        append_char(&buffer.buffer, &buffer.size, &buffer.capacity, ladvance(lexer));
    } else {
        while ((c = lpeek(lexer, 1)) && c != '\n') {
            append_char(&buffer.buffer, &buffer.size, &buffer.capacity, ladvance(lexer));
        }
    }

    append_char(&buffer.buffer, &buffer.size, &buffer.capacity, '\0');

    token->kind = TK_COMMENT;
    token->value = realloc(buffer.buffer, buffer.size);
}

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
            lexer->input_path, lexer->position.line, lexer->position.column, directive_name_vec.buffer); // TODO: print the line
    exit(1); // TODO: error recovery
}

// For resolving relative paths in #include directives
void get_file_prefix(const char* path, const size_t len, char buffer[len]) {
    ssize_t pathlen = strlen(path);

    // find the last '/'
    for (size_t i = pathlen - 1; i >= 0; i -= 1) {
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
    //    * ['/usr/local/include', '/usr/include'] on Linux
    //    * TODO for other platforms
    //

    // TODO: resolve include path for system headers
    // TODO: custom user/system include folders

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

    for(int i = 0; i < lexer->user_include_paths->size; i += 1) {
        if (fp != NULL) {
            break;
        }

        char_vector_t path = {malloc(32), 0, 32};
        append_chars(&path.buffer, &path.size, &path.capacity, lexer->user_include_paths->buffer[i]);
        if (path.size > 0 && path.buffer[path.size - 1] != '/') {
            append_char(&path.buffer, &path.size, &path.capacity, '/');
        }
        append_chars(&path.buffer, &path.size, &path.capacity, filename.buffer);
        fp = fopen(path.buffer, "r");
    }

    for(int i = 0; i < lexer->system_include_paths->size; i += 1) {
        if (fp != NULL) {
            break;
        }

        char_vector_t path = {malloc(32), 0, 32};
        append_chars(&path.buffer, &path.size, &path.capacity, lexer->system_include_paths->buffer[i]);
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
    *child = linit(filename.buffer, source_buffer, bytes_read, lexer->user_include_paths, lexer->system_include_paths);
    lexer->child = child;
}
