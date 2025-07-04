#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "errors.h"
#include "target.h"
#include "ir/arch.h"
#include "util/vectors.h"
#include "parser/lexer.h"
#include "parser/parser.h"
#include "ir/codegen/codegen.h"
#include "ir/cfg.h"
#include "ir/fmt.h"
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
     * Target-triplet, defaults to "native".
     * Components are <machine>-<vendor>-<operating
     */
    const char* target;

    /**
     * List supported targets and exit.
     */
    bool list_targets;

    /**
     * Write output to file (default: <input>.ll)
     */
    const char* output_file;

    /**
     * --emit-ir
     * Write generated IR to file (default: <input>.ir)
     */
    bool emit_ir;

    /**
     * --emit-ir-cfg
     */
    bool emit_ir_cfg;

    string_vector_t input_files;
} options_t;

options_t parse_and_validate_options(int argc, char** argv);
void compile(options_t options, const char* input_file_name);
void print_ir_cfg(FILE *file, const ir_module_t *module);

int main(int argc, char** argv) {
    options_t options = parse_and_validate_options(argc, argv);

    for (size_t i = 0; i < options.input_files.size; i += 1) {
        const char* input_file_name = options.input_files.buffer[i];
        compile(options, input_file_name);
    }
}

options_t parse_and_validate_options(int argc, char** argv) {
    struct Options options = {
            .additional_include_directories = {NULL, 0, 0},
            .additional_system_include_directories = {NULL, 0, 0},
            .output_file = NULL,
            .target = NULL,
            .list_targets = false,
            .emit_ir = false,
            .emit_ir_cfg = false,
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
        } else if (strcmp(argv[argi], "--emit-ir-cfg") == 0) {
            options.emit_ir_cfg = true;
        } else if (strncmp(argv[argi], "--target", 8) == 0) {
            if (argv[argi][8] == '=') {
                options.target = argv[argi] + 9;
            } else if (argi + 1 < argc) {
                options.target = argv[argi++];
            } else {
                fprintf(stderr, "Missing value for argument --target\n");
                exit(1);
            }
        } else if (strcmp(argv[argi], "--list-targets") == 0) {
            options.list_targets = true;
        } else if (strcmp(argv[argi], "--help") == 0 || strcmp(argv[argi], "-h") == 0) {
            printf("Usage: %s [options] <input files>\n", argv[0]);
            printf("Options:\n");
            printf("  -I<dir>, --include-directory=<dir>\n");
            printf("                  Add directory to the include search path. These will be\n");
            printf("                  searched in the order they are given before the system\n");
            printf("                  include directories.\n");
            printf("  -isystem<dir>, --system-include-directory=<dir>\n");
            printf("                  Add directory to the system include search path.\n");
            printf("  --target        Target tripple, defaults to host platform if not specified\n");
            printf("  --list-targets  List the supported targets and exit\n");
            printf("  -o <file>       Write output to <file>\n");
            printf("  --emit-ir       Write generated IR to file\n");
            printf("  --emit-ir-cfg   Write generated IR control flow graphs to file in graphviz format\n");
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

const char *get_output_path(const char *path, const char *extension);

void compile(options_t options, const char* input_file_name) {
    if (options.target == NULL || strcmp(options.target, "native") == 0) options.target = get_native_target();
    const target_t *target = get_target(options.target);
    if (target == NULL) {
        fprintf(stderr, "Target %s not supported, run with --list-targets to list all supported targets\n",
            options.target);
        exit(1);
    }

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

    ir_gen_result_t result = generate_ir(translation_unit, target->arch->ir_arch);
    if (result.errors.size > 0) {
        for (size_t i = 0; i < result.errors.size; i++) {
            compilation_error_t error = result.errors.buffer[i];
            print_compilation_error(&error);
        }
        fprintf(stderr, "Compilation failed, %zu errors\n", result.errors.size);
        exit(1);
    }

    ir_module_t *ir_module = result.module;
    if (options.emit_ir) {
        const char *output_path = get_output_path(input_file_name, "ir");
        FILE* output = fopen(output_path, "w");
        if (output == NULL) {
            fprintf(stderr, "Failed to open output file: %s\n", output_path);
            exit(1);
        }
        ir_print_module(output, ir_module);
        return;
    }

    if (options.emit_ir_cfg) {
        const char *output_path = get_output_path(input_file_name, "dot");
        FILE* output = fopen(output_path, "w");
        if (output == NULL) {
            fprintf(stderr, "Failed to open output file: %s\n", output_path);
            exit(1);
        }
        print_ir_cfg(output, ir_module);
        return;
    }

    const char *output_file_name;
    if (options.output_file == NULL) {
        output_file_name = get_output_path(input_file_name, "ll");
    } else {
        output_file_name = options.output_file;
    }

    llvm_gen_module(ir_module, target, output_file_name);
}

const char *get_output_path(const char *path, const char *extension) {
    // Strip the path to get just the file name
    // Handle both Unix and Windows path separators
    const char *file_name = strrchr(path, '/');
    if (file_name == NULL) {
        file_name = strrchr(path, '\\');
    }
    if (file_name == NULL) {
        file_name = path;
    } else {
        file_name += 1;
    }

    size_t output_size = strlen(file_name) + strlen(extension) + 2;
    char *output = malloc(output_size);

    // Strip the extension, replace it with the new extension
    const char *extension_start = strrchr(file_name, '.');
    if (extension_start == NULL || extension_start <= file_name || extension_start[-1] == '/' || extension_start[-1] == '\\') {
        // No extension found, append the new extension
        snprintf(output, output_size, "%s.%s", file_name, extension);
    } else {
        size_t bytes = extension_start - file_name;
        memcpy(output, file_name, bytes <= output_size ? bytes : output_size);
        snprintf(output + bytes, output_size - bytes, ".%s", extension);
    }

    return output;
}

void print_ir_cfg(FILE *file, const ir_module_t *module) {
    ir_control_flow_graph_t *cfgs = malloc(sizeof(ir_control_flow_graph_t) * module->functions.size);

    for (size_t i = 0; i < module->functions.size; i++) {
        cfgs[i] = ir_create_control_flow_graph(module->functions.buffer[i]);
    }

    ir_print_control_flow_graph(file, cfgs, module->functions.size);
}
