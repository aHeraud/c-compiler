#include <string.h>
#include "errors.h"
#include "ir/codegen/internal.h"

void visit_enumeration_constants(ir_gen_context_t *context, const enum_specifier_t *enum_specifier) {
    long value = 0;
    for (int i = 0; i < enum_specifier->enumerators.size; i += 1) {
        enumerator_t el = enum_specifier->enumerators.buffer[i];
        if (el.value != NULL) {
            expression_result_t res = ir_visit_constant_expression(context, el.value);
            if (res.kind == EXPR_RESULT_VALUE) {
                if (res.value.constant.kind != IR_CONST_INT) {
                    append_compilation_error(&context->errors, (compilation_error_t) {
                        .kind = ERR_ENUMERATION_CONSTANT_MUST_HAVE_INTEGER_TYPE,
                        .location = el.value->span.start,
                    });
                } else {
                    value = res.value.constant.value.i;
                }
            }
        }

        bool is_global = context->function == NULL;
        const char *name = is_global ? global_name(context) : temp_name(context);

        symbol_t *symbol = malloc(sizeof(symbol_t));
        *symbol = (symbol_t) {
            .identifier = el.identifier,
            .has_const_value = true,
            .ir_type = context->arch->sint,
            .c_type = &INT,
            .ir_ptr = {
                .type = NULL,
                .name = NULL,
            },
            .kind = SYMBOL_ENUMERATION_CONSTANT,
            .const_value = ir_make_const_int(context->arch->sint, value++).constant,
            .name = name,
        };
        declare_symbol(context, symbol);

        if (is_global) {
            ir_global_t *global = malloc(sizeof(ir_global_t));
            *global = (ir_global_t) {
                .initialized = true,
                .value = symbol->const_value,
                .type = symbol->ir_type,
                .name = symbol->name,
            };
            VEC_APPEND(&context->module->globals, global);
        }
    }
}

const tag_t *tag_for_declaration(ir_gen_context_t *context, const type_t *c_type) {
    assert(c_type->kind == TYPE_STRUCT_OR_UNION || c_type->kind == TYPE_ENUM);

    // From section 6.7.2.2 of C99 standard
    // Is this declaring a new tag, modifying a forward declaration, or just referencing an existing one?

    bool incomplete_type;
    const token_t *identifier;
    if (c_type->kind == TYPE_STRUCT_OR_UNION) {
        incomplete_type = !c_type->value.struct_or_union.has_body;
        identifier = c_type->value.struct_or_union.identifier;
    } else {
        assert(c_type->kind == TYPE_ENUM);
        incomplete_type = c_type->value.enum_specifier.enumerators.size == 0;
        identifier = c_type->value.enum_specifier.identifier;
    }

    if (identifier == NULL) {
        // anonymous tag, generate a unique identifier
        char *name = malloc(24);
        snprintf(name, 24, "__anon_tag_%d", context->tag_id_counter++);
        token_t * new_identifier = malloc(sizeof(token_t));
        *new_identifier = (token_t) {
            .kind = TK_IDENTIFIER,
            .value = name,
            // TODO: get position of declaration?
            .position = {
                .path = "",
                .column = 0,
                .line = 0,
            }
        };
        identifier = new_identifier;
    }

    tag_t *tag = lookup_tag_in_current_scope(context, identifier->value);
    // If there was already a tag with this name declared in the current scope. If one or both are incomplete types
    // (e.g. forward declarations, such as `struct Foo;`), then it's ok, otherwise it is a redefinition error.
    if (tag != NULL && !is_tag_incomplete_type(tag) && !incomplete_type) {
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_REDEFINITION_OF_TAG,
            .value.redefinition_of_tag = {
                .redefinition = identifier,
                .previous_definition = tag->identifier,
            },
        });
    }

    if (incomplete_type) {
        // Could be a forward declaration, also could be a reference to an existing tag.
        if (tag == NULL) tag = lookup_tag(context, identifier->value);
        if (tag == NULL) {
            // Declare a new tag
            size_t id_len = strlen(identifier->value) + 7; // max of 5 chars for id (unsigned short), plus 1 for _ and 1 for null terminator
            char *id = malloc(id_len);
            snprintf(id, id_len, "%s_%u", identifier->value, context->global_id_counter++);
            tag = malloc(sizeof(tag_t));
            *tag = (tag_t) {
                .identifier = identifier,
                .uid = id,
                .ir_type = NULL,
                .c_type = NULL, // null = incomplete
            };
            declare_tag(context, tag);
        }
        return tag;
    } else {
        // Defines a new tag
        // First declare an incomplete tag to allow for recursive references (e.g. `struct Foo { struct Foo *next; };`)
        size_t id_len = strlen(identifier->value) + 7; // 5 chars for id counter (unsigned short), 1 for _ and 1 for null terminator
        char *id = malloc(id_len);
        snprintf(id, id_len, "%s_%u", identifier->value, context->global_id_counter++);
        tag = malloc(sizeof(tag_t));
        *tag = (tag_t) {
            .identifier = identifier,
            .uid = id,
            .ir_type = NULL,
            .c_type = NULL, // null = incomplete
        };
        declare_tag(context, tag);

        if (c_type->kind == TYPE_STRUCT_OR_UNION) {
            // Resolve the struct c type
            const type_t *resolved_type = resolve_struct_type(context, c_type);

            // Visit the struct/union body to build the IR type, and update the tag
            const ir_type_t *ir_type = get_ir_struct_type(context, resolved_type, id);
            tag->ir_type = ir_type;
            tag->c_type = resolved_type;
        } else {
            // TODO: smaller enumeration constants based on max value?
            tag->ir_type = context->arch->sint;
            tag->c_type = &INT;

            // Declare identifiers in the current scope for the enumeration constants
            visit_enumeration_constants(context, &c_type->value.enum_specifier);
        }

        return tag;
    }
}

// TODO: This is a bit of a mess, and duplicates a lot of code from ir_visit_declaration.
//       They should probably be combined into a single function.
void ir_visit_global_declaration(ir_gen_context_t *context, const declaration_t *declaration) {
    assert(context != NULL && "Context must not be NULL");
    assert(declaration != NULL && "Declaration must not be NULL");

    // Typedef-name resolution is handled by the parser. This is a no-op.
    if (declaration->type->storage_class == STORAGE_CLASS_TYPEDEF) {
        return;
    }

    // Does this declare or reference a tag? (TODO: also support enums)
    const tag_t *tag = NULL;
    if (declaration->type->kind == TYPE_STRUCT_OR_UNION || declaration->type->kind == TYPE_ENUM) {
        tag = tag_for_declaration(context, declaration->type);
    }

    if (declaration->identifier == NULL) {
        // this only declares a tag
        return;
    }

    const type_t *c_type;
    const ir_type_t *ir_type;
    if (tag != NULL) {
        c_type = tag->c_type;
        ir_type = tag->ir_type;
    } else {
        c_type = resolve_type(context, declaration->type);
        ir_type = get_ir_type(context, c_type);
    }

    // Create the symbol for the variable declared by this declaration
    symbol_t *symbol = lookup_symbol_in_current_scope(context, declaration->identifier->value);
    ir_global_t *global = NULL;
    if (symbol != NULL) {
        // Global scope is a bit special. Re-declarations are allowed if the types match, however if
        // the global was previously given a value (e.g. has an initializer or is a function definition),
        // then it is a re-definition error.

        if (c_type->kind == TYPE_FUNCTION) {
            // Check if we've already processed a function definition with the same name
            if (hash_table_lookup(&context->function_definition_map, declaration->identifier->value, NULL)) {
                append_compilation_error(&context->errors, (compilation_error_t) {
                    .location = declaration->identifier->position,
                    .kind = ERR_REDEFINITION_OF_SYMBOL,
                    .value.redefinition_of_symbol = {
                        .redefinition = declaration->identifier,
                        .previous_definition = symbol->identifier,
                    },
                });
            }
            // Check if the types match. Re-declaration is allowed if the types match.
            if (!ir_types_equal(symbol->ir_type, ir_type)) {
                append_compilation_error(&context->errors, (compilation_error_t) {
                    .location = declaration->identifier->position,
                    .kind = ERR_REDEFINITION_OF_SYMBOL,
                    .value.redefinition_of_symbol = {
                        .redefinition = declaration->identifier,
                        .previous_definition = symbol->identifier,
                    },
                });
            }
            return;
        } else {
            // Look up the global in the module's global list.
            assert(hash_table_lookup(&context->global_map, declaration->identifier->value, (void**) &global));
            assert(global != NULL);
            // If the types are not equal, or the global has already been initialized, it is a redefinition error.
            if (!ir_types_equal(global->type, ir_type) || global->initialized) {
                append_compilation_error(&context->errors, (compilation_error_t) {
                    .location = declaration->identifier->position,
                    .kind = ERR_REDEFINITION_OF_SYMBOL,
                    .value.redefinition_of_symbol = {
                        .redefinition = declaration->identifier,
                        .previous_definition = symbol->identifier,
                    },
                });
                return;
            }
        }
    } else {
        // Create a new global symbol
        symbol = malloc(sizeof(symbol_t));

        bool is_function = declaration->type->kind == TYPE_FUNCTION;

        char* name;
        if (is_function) {
            size_t len = strlen(declaration->identifier->value) + 2;
            name = malloc(len);
            snprintf(name, len, "%s", declaration->identifier->value);
        } else {
            name = global_name(context);
        }

        *symbol = (symbol_t) {
            .kind = is_function ? SYMBOL_FUNCTION : SYMBOL_GLOBAL_VARIABLE,
            .identifier = declaration->identifier,
            .name = declaration->identifier->value,
            .c_type = c_type,
            .ir_type = ir_type,
            .ir_ptr = (ir_var_t) {
                .name = name,
                .type = is_function ? ir_type : get_ir_ptr_type(ir_type),
            },
            .has_const_value = false,
        };
        declare_symbol(context, symbol);

        // Add the global to the module's global list
        // *Function declarations are not IR globals*
        if (c_type->kind != TYPE_FUNCTION) {
            global = malloc(sizeof(ir_global_t));
            *global = (ir_global_t) {
                .name = symbol->ir_ptr.name,
                .type = symbol->ir_ptr.type,
                .initialized = declaration->initializer != NULL,
            };

            hash_table_insert(&context->global_map, symbol->name, global);
            ir_append_global_ptr(&context->module->globals, global);
        }
    }

    // Visit the initializer if present
    if (declaration->initializer != NULL) {
        assert(global != NULL);

        // Create a dummy fake instruction context and a function builder (visit_expression will attempt to generate
        // instructions if this expression isn't actually a compile time constant).
        ir_function_definition_t *cur_fn = context->function;
        ir_function_builder_t *cur_builder = context->builder;
        context->function = & (ir_function_definition_t) {
                .name = "__gen_global_initializer",
                .type = NULL,
                .num_params = 0,
                .params = NULL,
                .is_variadic = false,
                .body = NULL,
        };
        context->builder = ir_builder_create();

        ir_initializer_result_t initializer_result =
                ir_visit_initializer(context, ir_value_for_var(symbol->ir_ptr), symbol->c_type, declaration->initializer);

        // Delete the builder, throw away any generated instructions, and restore whatever the previous values were
        ir_builder_destroy(context->builder);
        context->function = NULL;
        context->function = cur_fn;
        context->builder = cur_builder;

        if (!initializer_result.has_constant_value) {
            // The initializer must be a constant expression
            append_compilation_error(&context->errors, (compilation_error_t) {
                    .kind = ERR_GLOBAL_INITIALIZER_NOT_CONSTANT,
                    .location = declaration->initializer->span.start,
                    .value.global_initializer_not_constant = {
                            .declaration = declaration,
                    },
            });
            return;
        }

        global->value = initializer_result.constant_value;
        symbol->has_const_value = true;
        symbol->const_value = initializer_result.constant_value;
        // If this is an array whose length is inferred from the initializer, the type information needs to be
        // updated based on the initializer list result.
        global->type = get_ir_ptr_type(initializer_result.type);
        symbol->c_type = initializer_result.c_type;
        symbol->ir_type = get_ir_ptr_type(initializer_result.type);
        symbol->ir_ptr.type = symbol->ir_type;
    } else if (global != NULL) {
        // Default value for uninitialized global variables
        if (is_floating_type(declaration->type)) {
            global->value = (ir_const_t) {
                .kind = IR_CONST_FLOAT,
                .type = symbol->ir_type,
                .value.f = 0.0,
            };
        } else {
            global->value = (ir_const_t) {
                .kind = IR_CONST_INT,
                .type = symbol->ir_type,
                .value.i = 0,
            };
        }
    }
}

void ir_visit_declaration(ir_gen_context_t *context, const declaration_t *declaration) {
    assert(context != NULL && "Context must not be NULL");
    assert(declaration != NULL && "Declaration must not be NULL");

    if (declaration->type->storage_class == STORAGE_CLASS_TYPEDEF) {
        // Typedefs are a no-op
        // The actual typedef-name resolution happens in the parser.
        return;
    }

    // Does this declare or reference a tag?
    const tag_t *tag = NULL;
    if (declaration->type->kind == TYPE_STRUCT_OR_UNION || declaration->type->kind == TYPE_ENUM) {
        tag = tag_for_declaration(context, declaration->type);
    }

    if (declaration->identifier == NULL) {
        // this only declares a tag
        return;
    }

    // Verify that this declaration is not a redeclaration of an existing symbol
    symbol_t *symbol = lookup_symbol_in_current_scope(context, declaration->identifier->value);
    if (symbol != NULL) {
        // Symbols in the same scope must have unique names, redefinition is not allowed.
        append_compilation_error(&context->errors, (compilation_error_t) {
            .location = declaration->identifier->position,
            .kind = ERR_REDEFINITION_OF_SYMBOL,
            .value.redefinition_of_symbol = {
                .redefinition = declaration->identifier,
                .previous_definition = symbol->identifier,
            },
        });
        return;
    }

    const type_t *c_type;
    const ir_type_t *ir_type;
    if (tag == NULL) {
        c_type = resolve_type(context, declaration->type);
        ir_type = get_ir_type(context, c_type);
    } else {
        c_type = tag->c_type;
        ir_type = tag->ir_type;
    }

    // Create a new symbol for this declaration and add it to the current scope
    symbol = malloc(sizeof(symbol_t));
    *symbol = (symbol_t) {
        .kind = SYMBOL_LOCAL_VARIABLE, // TODO: handle global/static variables
        .identifier = declaration->identifier,
        .name = declaration->identifier->value,
        .c_type = c_type,
        .ir_type = ir_type,
        .ir_ptr = (ir_var_t) {
            .name = temp_name(context),
            .type = get_ir_ptr_type(ir_type),
        },
        .has_const_value = false,
    };
    declare_symbol(context, symbol);

    // Allocate storage space for the variable
    ir_instruction_node_t *alloca_node = insert_alloca(context, ir_type, symbol->ir_ptr);

    // Evaluate the initializer if present, and store the result in the allocated storage
    if (declaration->initializer != NULL) {
        ir_initializer_result_t initializer_result =
                ir_visit_initializer(context, ir_value_for_var(symbol->ir_ptr), symbol->c_type, declaration->initializer);
        const ir_type_t *value_type = initializer_result.type;

        // If the variable was an array with a length inferred from the initializer list (e.g. `int a[] = {1, 2, 3};`),
        // we need to go back and update the symbol type and alloca parameter now that we know the size.
        // TODO: this seems like a hack, maybe there's a better way to do this?
        if (value_type != NULL && c_type->kind == TYPE_ARRAY && c_type->value.array.size == NULL) {
            // update the symbol
            symbol->ir_type = value_type;
            symbol->ir_ptr.type = get_ir_ptr_type(value_type);
            // update the alloca instruction
            ir_instruction_t *alloca_instr = ir_builder_get_instruction(alloca_node);
            alloca_instr->value.alloca.type = value_type;
            alloca_instr->value.alloca.result = symbol->ir_ptr;
        }

        // If this variable has a constant type, and the initializer is a constant, then we can treat this as a compile
        // time constant (e.g. for constant propagation, or for use in something requiring a constant expression).
        // TODO: does this work correctly for pointers (e.g. `const int *foo = &bar`)?
        if (symbol->c_type->is_const && initializer_result.has_constant_value) {
            symbol->has_const_value = true;
            symbol->const_value = initializer_result.constant_value;
        }
    }
}
