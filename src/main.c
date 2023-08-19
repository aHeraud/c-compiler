#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <malloc.h>
#include "util/vectors.h"
#include "lexer.h"
#include "parser.h"
#include "util/ast-printer.h"

// TODO: Set based on current platform
char* DEFAULT_SYSTEM_INCLUDE_DIRECTORIES[2] = {
        "/usr/local/include",
        "/usr/include",
};

struct Options {
    /**
     * --ast
     * Print the AST to stdout.
    */
    bool print_ast;

    /**
     * -I<dir>, --include-directory <dir>, --include-directory=<dir>
     * Add <dir> to the user include search path.
     */
    string_vector_t additional_include_directories;

    /**
     * -isystem <dir>, --system-include-directory <dir>, --system-include-directory=<dir>
     * Add <dir> to the system include search path.
     */
     string_vector_t additional_system_include_directories;

    string_vector_t input_files;
};

int main(int argc, char** argv) {
    struct Options options = {
            .print_ast = false,
            .additional_include_directories = {NULL, 0, 0},
            .additional_system_include_directories = {NULL, 0, 0},
            .input_files = {NULL, 0, 0},
    };

    for (int argi = 1; argi < argc; argi++) {
        if (strcmp(argv[argi], "--ast") == 0) {
            options.print_ast = true;
        } else if (strncmp(argv[argi], "-I", 2) == 0) {
            size_t len = strlen(argv[argi]);
            if (len > 2) {
                append_ptr((void ***) &options.additional_include_directories.buffer,
                           &options.additional_include_directories.size,
                           &options.additional_include_directories.capacity,
                           argv[argi] + 2);
            } else if (argi + 1 < argc) {
                append_ptr((void ***) &options.additional_include_directories.buffer,
                           &options.additional_include_directories.size,
                           &options.additional_include_directories.capacity,
                           argv[++argi]);
            } else {
                fprintf(stderr, "Missing argument for -I\n");
                return 1;
            }
        } else if (strncmp(argv[argi], "--include-directory", 19) == 0) {
            if (argv[argi][19] == '=') {
                append_ptr((void ***) &options.additional_include_directories.buffer,
                           &options.additional_include_directories.size,
                           &options.additional_include_directories.capacity,
                           argv[argi] + 20);
            } else if (argi + 1 < argc) {
                append_ptr((void ***) &options.additional_include_directories.buffer,
                           &options.additional_include_directories.size,
                           &options.additional_include_directories.capacity,
                           argv[++argi]);
            } else {
                fprintf(stderr, "Missing argument for --include-directory\n");
                return 1;
            }
        } else if (strncmp(argv[argi], "-isystem", 8) == 0) {
            size_t len = strlen(argv[argi]);
            if (len > 8) {
                append_ptr((void ***) &options.additional_system_include_directories.buffer,
                           &options.additional_system_include_directories.size,
                           &options.additional_system_include_directories.capacity,
                           argv[argi] + 8);
            } else if (argi + 1 < argc) {
                append_ptr((void ***) &options.additional_system_include_directories.buffer,
                           &options.additional_system_include_directories.size,
                           &options.additional_system_include_directories.capacity,
                           argv[++argi]);
            } else {
                fprintf(stderr, "Missing argument for -isystem\n");
                return 1;
            }
        } else if (strncmp(argv[argi], "--system-include-directory", 26) == 0) {
            if (argv[argi][26] == '=') {
                append_ptr((void ***) &options.additional_system_include_directories.buffer,
                           &options.additional_system_include_directories.size,
                           &options.additional_system_include_directories.capacity,
                           argv[argi] + 27);
            } else if (argi + 1 < argc) {
                append_ptr((void ***) &options.additional_system_include_directories.buffer,
                           &options.additional_system_include_directories.size,
                           &options.additional_system_include_directories.capacity,
                           argv[++argi]);
            } else {
                fprintf(stderr, "Missing argument for --system-include-directory\n");
                return 1;
            }
        } else if (strcmp(argv[argi], "--help") == 0 || strcmp(argv[argi], "-h") == 0) {
            printf("Usage: %s [options] <input files>\n", argv[0]);
            printf("Options:\n");
            printf("  --ast           Print the generated AST\n");
            printf("  -I<dir>, --include-directory=<dir>\n");
            printf("                  Add directory to the include search path. These will be\n");
            printf("                  searched in the order they are given before the system\n");
            printf("                  include directories.\n");
            printf("  -isystem<dir>, --system-include-directory=<dir>\n");
            printf("                  Add directory to the system include search path.\n");
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

    for(size_t i = 0; i < sizeof(DEFAULT_SYSTEM_INCLUDE_DIRECTORIES) / sizeof(char*); i++) {
        // TODO: check for duplicates
        append_ptr((void***) &options.additional_system_include_directories.buffer,
                   &options.additional_system_include_directories.size,
                   &options.additional_system_include_directories.capacity,
                   DEFAULT_SYSTEM_INCLUDE_DIRECTORIES[i]);
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

        lexer_global_context_t global_context = {
                .user_include_paths = &options.additional_include_directories,
                .system_include_paths = &options.additional_system_include_directories,
                .macro_definitions = {
                        .size = 0,
                        .num_buckets = 1000,
                        .buckets = calloc(1000, sizeof(hashtable_entry_t*)),
                }
        };

        lexer_t lexer = linit(options.input_files.buffer[i],
                              source_buffer,
                              bytes_read,
                              &global_context
        );
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
