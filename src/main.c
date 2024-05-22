#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "util/vectors.h"
#include "lexer.h"
#include "parser.h"
#include "ir/ir-gen.h"
#include "ir/cfg.h"
#include "llvm/llvm-gen.h"

// TODO: Set based on current platform
char* DEFAULT_SYSTEM_INCLUDE_DIRECTORIES[2] = {
        "/usr/local/include",
        "/usr/include",
};

typedef struct Options {
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

     /**
      * Write output to file (default: <input>.ll)
      */
     const char* output_file;

     /**
      * --emit-ir
      * Write generated IR to file (default: <input>.ir)
      */
     bool emit_ir;

     string_vector_t input_files;
} options_t;

options_t parse_and_validate_options(int argc, char** argv);
void compile(options_t options, const char* input_file_name);

int main(int argc, char** argv) {
    options_t options = parse_and_validate_options(argc, argv);

    for (size_t i = 0; i < options.input_files.size; i++) {
        const char* input_file_name = options.input_files.buffer[i];
        compile(options, input_file_name);
    }
}

options_t parse_and_validate_options(int argc, char** argv) {
    struct Options options = {
            .additional_include_directories = {NULL, 0, 0},
            .additional_system_include_directories = {NULL, 0, 0},
            .output_file = NULL,
            .emit_ir = false,
            .input_files = {NULL, 0, 0},
    };

    for (int argi = 1; argi < argc; argi++) {
        if (strncmp(argv[argi], "-I", 2) == 0) {
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
                exit(1);
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
                exit(1);
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
                exit(1);
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
                exit(1);
            }
        } else if (strncmp(argv[argi], "-o", 2) == 0) {
            if (argv[argi][2] == '=') {
                options.output_file = argv[argi] + 3;
            } else if (argi + 1 < argc) {
                options.output_file = argv[++argi];
            } else {
                fprintf(stderr, "Missing argument for -o\n");
                exit(1);
            }
        } else if (strcmp(argv[argi], "--emit-ir") == 0) {
            options.emit_ir = true;
        } else if (strcmp(argv[argi], "--help") == 0 || strcmp(argv[argi], "-h") == 0) {
            printf("Usage: %s [options] <input files>\n", argv[0]);
            printf("Options:\n");
            printf("  -I<dir>, --include-directory=<dir>\n");
            printf("                  Add directory to the include search path. These will be\n");
            printf("                  searched in the order they are given before the system\n");
            printf("                  include directories.\n");
            printf("  -isystem<dir>, --system-include-directory=<dir>\n");
            printf("                  Add directory to the system include search path.\n");
            printf("  -o <file>       Write output to <file>\n");
            printf("  --emit-ir       Write generated IR to file\n");
            exit(0);
        } else {
            append_ptr((void***) &options.input_files.buffer,
                       &options.input_files.size,
                       &options.input_files.capacity,
                       argv[argi]);
        }
    }

    if (options.input_files.size == 0) {
        fprintf(stderr, "No input files\n");
        exit(1);
    }

    if (options.output_file != NULL && options.input_files.size > 1) {
        fprintf(stderr, "Cannot specify output file (-o) when generating multiple output files\n");
        exit(1);
    }

    // Add default system include directories to the preprocessor search path.
    for(size_t i = 0; i < sizeof(DEFAULT_SYSTEM_INCLUDE_DIRECTORIES) / sizeof(char*); i++) {
        // TODO: check for duplicates
        append_ptr((void***) &options.additional_system_include_directories.buffer,
                   &options.additional_system_include_directories.size,
                   &options.additional_system_include_directories.capacity,
                   DEFAULT_SYSTEM_INCLUDE_DIRECTORIES[i]);
    }

    return options;
}

void get_output_path(const char *path, const char *extension, char *output, size_t output_size);

void compile(options_t options, const char* input_file_name) {
    FILE* file = fopen(input_file_name, "r");
    if (file == NULL) {
        fprintf(stderr, "Failed to open file: %s\n", input_file_name);
        exit(1);
    }

    char* source_buffer = NULL;
    size_t len = 0;
    ssize_t bytes_read = getdelim( &source_buffer, &len, '\0', file);
    if (bytes_read < 0) {
        fprintf(stderr, "Failed to read file: %s\n", input_file_name);
        exit(1);
    }
    fclose(file);

    lexer_global_context_t global_context = {
            .user_include_paths = &options.additional_include_directories,
            .system_include_paths = &options.additional_system_include_directories,
            .macro_definitions = hash_table_create_string_keys(1024),
    };

    lexer_t lexer = linit(input_file_name,
                          source_buffer,
                          bytes_read,
                          &global_context
    );
    parser_t parser = pinit(lexer);

    translation_unit_t *translation_unit = malloc(sizeof(translation_unit_t));
    if (!parse(&parser, translation_unit)) {
        fprintf(stderr, "Failed to parse file: %s\n",input_file_name);
        for (size_t e = 0; e < parser.errors.size; e++) {
            print_parse_error(stderr, &parser.errors.buffer[e]);
            fprintf(stderr, "\n");
        }
        exit(1);
    }

    // TODO: add flag print IR module
    ir_gen_result_t result = generate_ir(translation_unit);
    if (result.errors.size > 0) {
        fprintf(stderr, "Failed to generate IR for file: %s\n", input_file_name);
        // TODO: print errors
        exit(1);
    }

    ir_module_t *ir_module = result.module;
    if (options.emit_ir) {
        // TODO: write to file (currently just prints to stdout)
        //char output_path[1024];
        //get_output_path(input_file_name, "ir", output_path, sizeof(output_path));
        ir_print_module(stdout, ir_module);
    }

    for (size_t i = 0; i < ir_module->functions.size; i+= 1) {
        ir_function_definition_t *function = ir_module->functions.buffer[i];
        ir_control_flow_graph_t cfg = ir_create_control_flow_graph(function);
        ir_print_control_flow_graph(stdout, &cfg, 1);
    }

    const char *output_file_name;
    if (options.output_file == NULL) {
        size_t file_name_len = strlen(input_file_name);
        char *tmp = malloc(file_name_len + 4);
        get_output_path(input_file_name, "ll", tmp, file_name_len + 4);
        output_file_name = tmp;
    } else {
        output_file_name = options.output_file;
    }

    llvm_gen_module(ir_module, output_file_name);
}

void get_output_path(const char *path, const char *extension, char *output, size_t output_size) {
    const char *extension_start = strrchr(path, '.');
    if (extension_start == NULL || extension_start <= path || extension_start[-1] == '/' || extension_start[-1] == '\\') {
        // No extension found, append the new extension
        snprintf(output, output_size, "%s.%s", path, extension);
    } else {
        size_t bytes = extension_start - path;
        memcpy(output, path, bytes <= output_size ? bytes : output_size);
        snprintf(output + bytes, output_size - bytes, ".%s", extension);
    }
}
