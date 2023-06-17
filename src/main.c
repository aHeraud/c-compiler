#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <malloc.h>
#include "util/vectors.h"
#include "lexer.h"
#include "parser.h"
#include "util/ast-printer.h"

struct Options {
    // --ast
    bool print_ast;

    string_vector_t input_files;
};

int main(int argc, char** argv) {
    struct Options options = {
            .print_ast = false,
            .input_files = {NULL, 0, 0},
    };

    for (int argi = 1; argi < argc; argi++) {
        if (strcmp(argv[argi], "--ast") == 0) {
            options.print_ast = true;
        } else if (strcmp(argv[argi], "--help") == 0 || strcmp(argv[argi], "-h") == 0) {
            printf("Usage: %s [options] <input files>\n", argv[0]);
            return 0;
        } else {
            append_ptr((void***) &options.input_files.buffer,
                       &options.input_files.size,
                       &options.input_files.capacity,
                       argv[argi]);
        }
    }

    if (options.input_files.size == 0) {
        fprintf(stderr, "No input files\n");
        return 1;
    }

    for (size_t i = 0; i < options.input_files.size; i++) {
        FILE* file = fopen(options.input_files.buffer[i], "r");
        if (file == NULL) {
            fprintf(stderr, "Failed to open file: %s\n", options.input_files.buffer[i]);
            return 1;
        }

        char* source_buffer = NULL;
        size_t len = 0;
        ssize_t bytes_read = getdelim( &source_buffer, &len, '\0', file);
        if (bytes_read < 0) {
            fprintf(stderr, "Failed to read file: %s\n", options.input_files.buffer[i]);
            return 1;
        }
        fclose(file);

        lexer_t lexer = linit(options.input_files.buffer[i],
                              source_buffer,
                              bytes_read);
        parser_t parser = pinit(lexer);

        ast_node_t* translation_unit = malloc(sizeof(ast_node_t));
        if (!parse(&parser, translation_unit)) {
            fprintf(stderr, "Failed to parse file: %s\n", options.input_files.buffer[i]);
            return 1;
        }

        if (options.print_ast) {
            ppast(stdout, translation_unit);
        }
    }
}
