#include <string.h>
#include "errors.h"
#include "ir/codegen/internal.h"
#include "ir/fmt.h"

symbol_t *lookup_symbol(const ir_gen_context_t *context, const char *name) {
    const scope_t *scope = context->current_scope;
    while (scope != NULL) {
        symbol_t *symbol = NULL;
        if (hash_table_lookup(&scope->symbols, name, (void**) &symbol)) {
            return symbol;
        }
        scope = scope->parent;
    }
    return NULL;
}

symbol_t *lookup_symbol_in_current_scope(const ir_gen_context_t *context, const char *name) {
    symbol_t *symbol = NULL;
    if (hash_table_lookup(&context->current_scope->symbols, name, (void**) &symbol)) {
        return symbol;
    }
    return NULL;
}

tag_t *lookup_tag(const ir_gen_context_t *context, const char *name) {
    const scope_t *scope = context->current_scope;
    while (scope != NULL) {
        tag_t *tag = NULL;
        if (hash_table_lookup(&scope->tags, name, (void**) &tag)) return tag;
        scope = scope->parent;
    }
    return NULL;
}

tag_t *lookup_tag_in_current_scope(const ir_gen_context_t *context, const char *name) {
    tag_t *tag = NULL;
    if (hash_table_lookup(&context->current_scope->tags, name, (void**) &tag)) return tag;
    return NULL;
}

tag_t *lookup_tag_by_uid(const ir_gen_context_t *context, const char *uid) {
    tag_t *tag = NULL;
    hash_table_lookup(&context->tag_uid_map, uid, (void**) &tag);
    return tag;
}

void declare_symbol(ir_gen_context_t *context, symbol_t *symbol) {
    assert(context != NULL && symbol != NULL);
    bool inserted = hash_table_insert(&context->current_scope->symbols, symbol->identifier->value, (void*) symbol);
    assert(inserted);
}

void declare_tag(ir_gen_context_t *context, const tag_t *tag) {
    assert(context != NULL && tag != NULL);
    assert(hash_table_insert(&context->current_scope->tags, tag->identifier->value, (void*) tag));
    assert(hash_table_insert(&context->tag_uid_map, tag->uid, (void*) tag));

    // also add the type to the module
    assert(!hash_table_lookup(&context->module->type_map, tag->uid, NULL)); // should be unique
    assert(hash_table_insert(&context->module->type_map, tag->uid, (void*) tag->ir_type));
}

void enter_scope(ir_gen_context_t *context) {
    assert(context != NULL);
    scope_t *scope = malloc(sizeof(scope_t));
    *scope = (scope_t) {
        .symbols = hash_table_create_string_keys(256),
        .tags = hash_table_create_string_keys(256),
        .parent = context->current_scope,
    };
    context->current_scope = scope;
}

void leave_scope(ir_gen_context_t *context) {
    assert(context != NULL);
    scope_t *scope = context->current_scope;
    context->current_scope = scope->parent;
    // TODO: free symbols
    free(scope);
}

void ir_append_function_ptr(ir_function_ptr_vector_t *vec, ir_function_definition_t *function) {
    assert(vec != NULL);
    assert(function != NULL);
    VEC_APPEND(vec, function);
}

void ir_append_global_ptr(ir_global_ptr_vector_t *vec, ir_global_t *global) {
    assert(vec != NULL);
    assert(global != NULL);
    VEC_APPEND(vec, global);
}

/**
* Enter a loop context, which will set the loop break and continue labels
* Also saves and returns the previous context
*/
loop_context_t enter_loop_context(ir_gen_context_t *context, char *break_label, char *continue_label) {
    const loop_context_t prev = {
        .break_label = context->break_label,
        .continue_label = context->continue_label,
    };
    context->break_label = break_label;
    context->continue_label = continue_label;
    return prev;
}

/**
 * Restore the previous loop context
 */
void leave_loop_context(ir_gen_context_t *context, loop_context_t prev) {
    context->break_label = prev.break_label;
    context->continue_label = prev.continue_label;
}

char *global_name(ir_gen_context_t *context) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "@%d", context->global_id_counter++);
    return strdup(buffer);
}

char *temp_name(ir_gen_context_t *context) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%%%d", context->local_id_counter++);
    return strdup(buffer);
}

char *gen_label(ir_gen_context_t *context) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "l%d", context->label_counter++);
    return strdup(buffer);
}

ir_var_t temp_var(ir_gen_context_t *context, const ir_type_t *type) {
    return (ir_var_t) {
        .type = type,
        .name = temp_name(context),
    };
}

const type_t *c_ptr_uint_type(void) {
    // TODO: arch dependent
    return &UNSIGNED_LONG;
}

const ir_type_t* get_ir_type(ir_gen_context_t *context, const type_t *c_type) {
    assert(c_type != NULL && "C type must not be NULL");

    switch (c_type->kind) {
        case TYPE_INTEGER: {
            if (c_type->value.integer.is_signed) {
                switch (c_type->value.integer.size) {
                    case INTEGER_TYPE_BOOL:
                        return &IR_BOOL;
                    case INTEGER_TYPE_CHAR:
                        return context->arch->schar;
                    case INTEGER_TYPE_SHORT:
                        return context->arch->sshort;
                    case INTEGER_TYPE_INT:
                        return context->arch->sint;
                    case INTEGER_TYPE_LONG:
                        return context->arch->slong;
                    case INTEGER_TYPE_LONG_LONG:
                        return context->arch->slonglong;
                    default:
                        return context->arch->sint;
                }
            } else {
                switch (c_type->value.integer.size) {
                    case INTEGER_TYPE_BOOL:
                        return &IR_BOOL;
                    case INTEGER_TYPE_CHAR:
                        return context->arch->uchar;
                    case INTEGER_TYPE_SHORT:
                        return context->arch->ushort;
                    case INTEGER_TYPE_INT:
                        return context->arch->uint;
                    case INTEGER_TYPE_LONG:
                        return context->arch->ulong;
                    case INTEGER_TYPE_LONG_LONG:
                        return context->arch->ulonglong;
                    default:
                        return context->arch->uint;
                }
            }
        }
        case TYPE_FLOATING: {
            switch (c_type->value.floating) {
                case FLOAT_TYPE_FLOAT:
                    return context->arch->_float;
                case FLOAT_TYPE_DOUBLE:
                    return context->arch->_double;
                case FLOAT_TYPE_LONG_DOUBLE:
                    return context->arch->_long_double;
                default:
                    return context->arch->_double;
            }
        }
        case TYPE_POINTER: {
            const ir_type_t *pointee = get_ir_type(context, c_type->value.pointer.base);
            ir_type_t *ir_type = malloc(sizeof(ir_type_t));
            *ir_type = (ir_type_t) {
                .kind = IR_TYPE_PTR,
                .value.ptr = {
                    .pointee = pointee,
                },
            };
            return ir_type;
        }
        case TYPE_FUNCTION: {
            const ir_type_t *ir_return_type = get_ir_type(context, c_type->value.function.return_type);
            const ir_type_t **ir_param_types = malloc(c_type->value.function.parameter_list->length * sizeof(ir_type_t*));
            for (size_t i = 0; i < c_type->value.function.parameter_list->length; i++) {
                const parameter_declaration_t *param = c_type->value.function.parameter_list->parameters[i];
                const ir_type_t *ir_type = get_ir_type(context,param->type);
                ir_param_types[i] = ir_type->kind == IR_TYPE_ARRAY
                    ? get_ir_ptr_type(ir_type->value.array.element) // array to pointer decay
                    : ir_type;
            }
            ir_type_t *ir_type = malloc(sizeof(ir_type_t));
            *ir_type = (ir_type_t) {
                .kind = IR_TYPE_FUNCTION,
                .value.function = {
                    .return_type = ir_return_type,
                    .params = ir_param_types,
                    .num_params = c_type->value.function.parameter_list->length,
                    .is_variadic = c_type->value.function.parameter_list->variadic
                },
            };
            return ir_type;
        }
        case TYPE_ARRAY: {
            const ir_type_t *element_type = get_ir_type(context,c_type->value.array.element_type);
            size_t length = 0;
            if (c_type->value.array.size != NULL) {
                expression_result_t array_len = ir_visit_expression(context, c_type->value.array.size);
                if (array_len.kind == EXPR_RESULT_ERR) assert(false && "Invalid array size"); // TODO: handle error
                if (array_len.is_lvalue) array_len = get_rvalue(context, array_len);
                ir_value_t length_val = array_len.value;
                if (length_val.kind != IR_VALUE_CONST) {
                    // TODO: handle non-constant array sizes
                    assert(false && "Non-constant array sizes not implemented");
                }
                length = length_val.constant.value.i;
            }

            ir_type_t *ir_type = malloc(sizeof(ir_type_t));
            *ir_type = (ir_type_t) {
                .kind = IR_TYPE_ARRAY,
                .value.array = {
                    .element = element_type,
                    .length = length,
                }
            };
            return ir_type;
        }
        case TYPE_STRUCT_OR_UNION: {
            // This is only for looking up existing struct types, creating a new one should be done through
            // the function get_ir_struct_type
            assert(c_type->value.struct_or_union.identifier != NULL);
            const tag_t *tag = lookup_tag(context, c_type->value.struct_or_union.identifier->value);
            if (tag == NULL) {
                // Any valid declaration that declares a struct also creates the tag (for example: `struct Foo *foo`)
                // If the tag isn't valid here, then there was some other error earlier in the program
                // We will just return a default type
                return &IR_I32;
            }
            assert(tag->ir_type != NULL);
            return tag->ir_type;
        }
        case TYPE_ENUM:
            // TODO: return actual enum type?
            return context->arch->sint;
        default:
            return &IR_VOID;
    }
}

// This should only be called when creating the declaration/tag
const ir_type_t *get_ir_struct_type(
    ir_gen_context_t *context,
    tag_t *tag,
    const type_t *c_type,
    const char *id
) {
    assert(c_type != NULL && c_type->kind == TYPE_STRUCT_OR_UNION);

    ir_type_t *ir_type = malloc(sizeof(ir_type_t));

    // A bit of a hack to handle nested reference to the struct
    *ir_type = *tag->ir_type;
    tag->ir_type = ir_type;

    // map of field name -> field ptr
    hash_table_t field_map = hash_table_create_string_keys(32);

    // get field list
    ir_struct_field_ptr_vector_t fields = VEC_INIT;
    for (size_t i = 0; i < c_type->value.struct_or_union.fields.size; i++) {
        // TODO: handle illegal definitions where a struct contains a field which has the same type as itself

        const struct_field_t *c_field = c_type->value.struct_or_union.fields.buffer[i];
        assert(c_field->index == i); // assuming they're in order
        ir_struct_field_t *ir_field = malloc(sizeof(ir_struct_field_t));
        const ir_type_t *ir_field_type = NULL;
        ir_field_type = get_ir_type(context, c_field->type);

        *ir_field = (ir_struct_field_t) {
            .name = c_field->identifier->value,
            .type = ir_field_type,
            .index = c_field->index,
        };

        hash_table_insert(&field_map, ir_field->name, ir_field);
        VEC_APPEND(&fields, ir_field);
    }

    ir_type_struct_t definition = {
        .id = id,
        .fields = fields,
        .field_map = field_map,
        .is_union = c_type->value.struct_or_union.is_union,
    };
    if (!c_type->value.struct_or_union.packed && !c_type->value.struct_or_union.is_union) definition = ir_pad_struct(context->arch, &definition);

    *ir_type = (ir_type_t) {
        .kind = IR_TYPE_STRUCT_OR_UNION,
        .value.struct_or_union = definition,
    };
    return ir_type;
}

const ir_type_t *get_ir_ptr_type(const ir_type_t *pointee) {
    // TODO: cache these?
    ir_type_t *ir_type = malloc(sizeof(ir_type_t));
    *ir_type = (ir_type_t) {
        .kind = IR_TYPE_PTR,
        .value.ptr = {
            .pointee = pointee,
        },
    };
    return ir_type;
}

ir_value_t ir_get_zero_value(ir_gen_context_t *context, const ir_type_t *type) {
    if (ir_is_integer_type(type)) {
        return (ir_value_t) {
            .kind = IR_VALUE_CONST,
            .constant = (ir_const_t) {
                .kind = IR_CONST_INT,
                .type = type,
                .value.i = 0,
            }
        };
    } else if (ir_is_float_type(type)) {
        return (ir_value_t) {
            .kind = IR_VALUE_CONST,
            .constant = (ir_const_t) {
                .kind = IR_CONST_FLOAT,
                .type = type,
                .value.f = 0.0,
            }
        };
    } else if (type->kind == IR_TYPE_PTR) {
        ir_value_t zero = ir_get_zero_value(context, get_ir_type(context,c_ptr_uint_type()));
        // ir_var_t result = temp_var(context, type);
        // ir_build_itop(context->builder, zero, result);
        // return ir_value_for_var(result);
        return zero;
    } else if (type->kind == IR_TYPE_ARRAY) {
        unsigned int length = type->value.array.length;
        ir_const_t array = {
            .kind = IR_CONST_ARRAY,
            .type = type,
            .value.array = {
                .length = length,
                .values = malloc(length * sizeof(ir_const_t)),
            },
        };
        for (int i = 0; i < length; i += 1) {
            ir_value_t zero = ir_get_zero_value(context, type->value.array.element);
            assert(zero.kind == IR_VALUE_CONST);
            array.value.array.values[i] = zero.constant;
        }
        return ir_value_for_const(array);
    } else if (type->kind == IR_TYPE_STRUCT_OR_UNION) {
        // TODO: Special handling for unions? e.g. initialize only largest field?
        unsigned int length = type->value.struct_or_union.fields.size;
        ir_const_t value = {
            .kind = IR_CONST_STRUCT,
            .type = type,
            .value._struct = {
                .fields = malloc(sizeof(ir_const_t) * length),
                .length = length,
            },
        };
        for (int i = 0; i < length; i += 1) {
            ir_struct_field_t *field = type->value.struct_or_union.fields.buffer[i];
            ir_value_t zero = ir_get_zero_value(context, field->type);
            assert(zero.kind == IR_VALUE_CONST);
            value.value._struct.fields[i] = zero.constant;
        }
        return ir_value_for_const(value);
    } else {
        // TODO: enums, etc...
        char _type[512];
        ir_fmt_type(_type, 512, type);
        fprintf(stderr, "Unimplemented default value for type %s\n", _type);
        exit(1);
    }
}

expression_result_t get_boolean_value(
    ir_gen_context_t *context,
    ir_value_t value,
    const type_t *c_type,
    const expression_t *expr
) {
    const ir_type_t *ir_type = ir_get_type_of_value(value);
    if (ir_type->kind == IR_TYPE_BOOL) {
        return (expression_result_t) {
            .c_type = &BOOL,
            .is_lvalue = false,
            .value = value,
        };
    }

    if (!ir_is_scalar_type(ir_type)) {
        // The value must have scalar type
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_INVALID_CONVERSION_TO_BOOLEAN,
            .location = expr->span.start,
            .value.invalid_conversion_to_boolean = {
                .type = c_type,
            },
        });
        return EXPR_ERR;
    }

    ir_value_t result;
    if (value.kind == IR_VALUE_CONST) {
        // constant folding
        ir_const_t constant = {
            .kind = IR_CONST_INT,
            .type = &IR_BOOL,
            .value.i =  ir_is_float_type(ir_type) ? value.constant.value.f != 0.0 : value.constant.value.i != 0,
        };
        result = (ir_value_t) {
            .kind = IR_VALUE_CONST,
            .constant = constant,
        };
    } else {
        ir_var_t temp = temp_var(context, &IR_BOOL);
        ir_build_ne(context->builder, value, ir_get_zero_value(context, ir_type), temp);
        result = ir_value_for_var(temp);
    }

    return (expression_result_t) {
        .c_type = &BOOL,
        .is_lvalue = false,
        .value = result,
    };
}

expression_result_t convert_to_type(
        ir_gen_context_t *context,
        ir_value_t value,
        const type_t *from_type,
        const type_t *to_type
) {
    const ir_type_t *result_type = get_ir_type(context,to_type);
    const ir_type_t *source_type;
    if (value.kind == IR_VALUE_CONST) {
        source_type = value.constant.type;
    } else {
        source_type = value.var.type;
    }

    if (ir_types_equal(source_type, result_type)) {
        // No conversion necessary
        return (expression_result_t) {
            .kind = EXPR_RESULT_VALUE,
            .c_type = to_type,
            .is_lvalue = false,
            .value = value,
        };
    }

    ir_var_t result = {
        .name = temp_name(context),
        .type = result_type,
    };

    if (ir_is_integer_type(result_type)) {
        if (ir_is_integer_type(source_type)) {
            if (value.kind == IR_VALUE_CONST) {
                // constant -> constant conversion
                ir_value_t constant = (ir_value_t) {
                    .kind = IR_VALUE_CONST,
                    .constant = (ir_const_t) {
                        .kind = IR_CONST_INT,
                        .type = result_type,
                        .value.i = value.constant.value.i,
                    }
                };
                return (expression_result_t) {
                    .kind = EXPR_RESULT_VALUE,
                    .c_type = to_type,
                    .is_lvalue = false,
                    .value = constant,
                };
            }

            // int -> int conversion
            if (ir_size_of_type_bits(context->arch, source_type) > ir_size_of_type_bits(context->arch, result_type)) {
                // Truncate
                ir_build_trunc(context->builder, value, result);
            } else if (ir_size_of_type_bits(context->arch, source_type) < ir_size_of_type_bits(context->arch, result_type)) {
                // Extend
                ir_build_ext(context->builder, value, result);
            } else {
                // Sign/unsigned integer conversion
                ir_build_bitcast(context->builder, value, result);
            }
        } else if (ir_is_float_type(source_type)) {
            // constant conversion
            if (value.kind == IR_VALUE_CONST) {
                ir_value_t constant = (ir_value_t) {
                    .kind = IR_VALUE_CONST,
                    .constant = (ir_const_t) {
                        .kind = IR_CONST_INT,
                        .type = result_type,
                        .value.i = (long long)value.constant.value.f,
                    },
                };
                return (expression_result_t) {
                    .kind = EXPR_RESULT_VALUE,
                    .c_type = to_type,
                    .is_lvalue = false,
                    .value = constant,
                };
            }

            // float -> int
            ir_build_ftoi(context->builder, value, result);
        } else if (source_type->kind == IR_TYPE_PTR) {
            if (value.kind == IR_VALUE_CONST) {
                ir_value_t constant = (ir_value_t) {
                    .kind = IR_VALUE_CONST,
                    .constant = (ir_const_t) {
                        .kind = IR_CONST_INT,
                        .type = result_type,
                        .value.i = value.constant.value.i,
                    }
                };
                return (expression_result_t) {
                    .kind = EXPR_RESULT_VALUE,
                    .c_type = to_type,
                    .is_lvalue = false,
                    .value = constant,
                };
            }

            // ptr -> int
            ir_build_ptoi(context->builder, value, result);
        } else {
            // TODO, other conversions, proper error handling
            char _type1[512], _type2[512];
            ir_fmt_type(_type1, 512, source_type);
            ir_fmt_type(_type2, 512, result_type);
            fprintf(stderr, "Unimplemented type conversion from %s to %s\n", _type1, _type2);
            return EXPR_ERR;
        }
    } else if (ir_is_float_type(result_type)) {
        if (ir_is_float_type(source_type)) {
            // constant conversion
            if (value.kind == IR_VALUE_CONST) {
                ir_value_t constant = (ir_value_t) {
                    .kind = IR_VALUE_CONST,
                    .constant = (ir_const_t) {
                        .kind = IR_CONST_FLOAT,
                        .type = result_type,
                        .value.f = value.constant.value.f,
                    }
                };
                return (expression_result_t) {
                    .kind = EXPR_RESULT_VALUE,
                    .c_type = to_type,
                    .is_lvalue = false,
                    .value = constant,
                };
            }

            // float -> float conversion
            if (ir_size_of_type_bits(context->arch, source_type) > ir_size_of_type_bits(context->arch, result_type)) {
                // Truncate
                ir_build_trunc(context->builder, value, result);
            } else if (ir_size_of_type_bits(context->arch, source_type) < ir_size_of_type_bits(context->arch, result_type)) {
                // Extend
                ir_build_ext(context->builder, value, result);
            } else {
                // No conversion necessary
                ir_build_assign(context->builder, value, result);
            }
        } else if (ir_is_integer_type(source_type)) {
            // constant conversion
            if (value.kind == IR_VALUE_CONST) {
                ir_value_t constant = (ir_value_t) {
                    .kind = IR_VALUE_CONST,
                    .constant = (ir_const_t) {
                        .kind = IR_CONST_FLOAT,
                        .type = result_type,
                        .value.f = (double)value.constant.value.i,
                    }
                };
                return (expression_result_t) {
                    .kind = EXPR_RESULT_VALUE,
                    .c_type = to_type,
                    .is_lvalue = false,
                    .value = constant,
                };
            }

            // int -> float
            ir_build_itof(context->builder, value, result);
        } else {
            // TODO: proper error handling
            char _type1[512], _type2[512];
            ir_fmt_type(_type1, 512, source_type);
            ir_fmt_type(_type2, 512, result_type);
            fprintf(stderr, "Unimplemented type conversion from %s to %s\n", _type1, _type2);
            return EXPR_ERR;
        }
    } else if (result_type->kind == IR_TYPE_PTR) {
        if (source_type->kind == IR_TYPE_PTR) {
            // constant conversion
            if (value.kind == IR_VALUE_CONST && value.constant.kind == IR_CONST_INT) {
                ir_value_t constant = (ir_value_t) {
                    .kind = IR_VALUE_CONST,
                    .constant = (ir_const_t) {
                        .kind = IR_CONST_INT,
                        .type = result_type,
                        .value.i = value.constant.value.i,
                    }
                };
                return (expression_result_t) {
                    .kind = EXPR_RESULT_VALUE,
                    .c_type = to_type,
                    .is_lvalue = false,
                    .value = constant,
                };
            }

            // ptr -> ptr conversion
            ir_build_bitcast(context->builder, value, result);
        } else if (ir_is_integer_type(source_type)) {
            // constant conversion
            if (value.kind == IR_VALUE_CONST) {
                ir_value_t constant = (ir_value_t) {
                    .kind = IR_VALUE_CONST,
                    .constant = (ir_const_t) {
                        .kind = IR_CONST_INT,
                        .type = result_type,
                        .value.i = value.constant.value.i,
                    }
                };
                return (expression_result_t) {
                    .kind = EXPR_RESULT_VALUE,
                    .c_type = to_type,
                    .is_lvalue = false,
                    .value = constant,
                };
            }

            // int -> ptr
            // If the source is smaller than the target, we need to extend it
            if (ir_size_of_type_bits(context->arch, source_type) < ir_size_of_type_bits(context->arch, get_ir_type(context, c_ptr_uint_type()))) {
                ir_var_t temp = temp_var(context, get_ir_type(context,c_ptr_uint_type()));
                ir_build_ext(context->builder, value, temp);
                value = ir_value_for_var(temp);
            }
            ir_build_itop(context->builder, value, result);
        } else if (ir_is_float_type(source_type)) {
            // float -> ptr
            // TODO: is this allowed? Seems like it's an invalid conversion
            const ir_type_t* int_type = source_type->kind == IR_TYPE_F64 ? &IR_I64 : &IR_I32;
            ir_var_t temp = temp_var(context, int_type);
            ir_build_bitcast(context->builder, value, temp);
            ir_build_itop(context->builder, ir_value_for_var(temp), result);
        } else if (source_type->kind == IR_TYPE_ARRAY) {
            // TODO
            char _type1[512], _type2[512];
            ir_fmt_type(_type1, 512, source_type);
            ir_fmt_type(_type2, 512, result_type);
            fprintf(stderr, "Unimplemented type conversion from %s to %s\n", _type1, _type2);
            return EXPR_ERR;
        }
    } else {
        char _type1[512], _type2[512];
        ir_fmt_type(_type1, 512, source_type);
        ir_fmt_type(_type2, 512, result_type);
        fprintf(stderr, "Unimplemented type conversion from %s to %s\n", _type1, _type2);
        return EXPR_ERR;
    }

    return (expression_result_t) {
        .kind = EXPR_RESULT_VALUE,
        .c_type = to_type,
        .is_lvalue = false,
        .value = ir_value_for_var(result),
    };
}

ir_value_t ir_value_for_var(ir_var_t var) {
    return (ir_value_t) {
        .kind = IR_VALUE_VAR,
        .var = var,
    };
}

ir_value_t ir_value_for_const(ir_const_t constant) {
    return (ir_value_t) {
        .kind = IR_VALUE_CONST,
        .constant = constant,
    };
}

ir_value_t get_indirect_ptr(ir_gen_context_t *context, expression_result_t res) {
    assert(res.kind == EXPR_RESULT_INDIRECTION && "Expected indirection expression");

    // We may need to load the value from a pointer.
    // However, there may be multiple levels of indirection, each requiring a load.
    expression_result_t *e = &res;
    int indirection_level = 0;
    do {
        assert(e->indirection_inner != NULL);
        e = e->indirection_inner;
        indirection_level += 1;
    } while (e->kind == EXPR_RESULT_INDIRECTION);

    if (!e->is_lvalue) {
        // value has already been loaded
        indirection_level -= 1;
    }

    // Starting at the base pointer, repeatedly load the new pointer
    ir_value_t ptr = e->value;
    for (int i = 0; i < indirection_level; i += 1) {
        ir_var_t temp = temp_var(context, ir_get_type_of_value(ptr)->value.ptr.pointee);
        ir_build_load(context->builder, ptr, temp);
        ptr = ir_value_for_var(temp);
    }

    return ptr;
}

expression_result_t get_rvalue(ir_gen_context_t *context, expression_result_t res) {
    assert(res.is_lvalue && "Expected lvalue");
    if (res.kind == EXPR_RESULT_VALUE) {
        assert(ir_get_type_of_value(res.value)->kind == IR_TYPE_PTR && "Expected pointer type");
        if (res.symbol != NULL && res.symbol->c_type->is_const && res.symbol->has_const_value) {
            // TODO: not quite sure this is correct for const pointers (e.g. `const int *foo = bar`)
            // This value is a compile time constant. Use the constant value instead of loading from memory.
            return (expression_result_t) {
                .kind = EXPR_RESULT_VALUE,
                .c_type = res.c_type,
                .is_lvalue = false,
                .is_string_literal = false,
                .addr_of = false,
                .value = ir_value_for_const(res.symbol->const_value),
            };
        }

        ir_var_t temp = temp_var(context, ir_get_type_of_value(res.value)->value.ptr.pointee);
        ir_value_t ir_ptr = res.value;
        ir_build_load(context->builder, ir_ptr, temp);
        return (expression_result_t) {
            .kind = EXPR_RESULT_VALUE,
            .c_type = res.c_type,
            .is_lvalue = false,
            .value = ir_value_for_var(temp),
        };
    } else if(res.kind == EXPR_RESULT_INDIRECTION) {
        ir_value_t ptr = get_indirect_ptr(context, res);

        // Then finally, load the result
        ir_var_t result = temp_var(context, ir_get_type_of_value(ptr)->value.ptr.pointee);
        ir_build_load(context->builder, ptr, result);
        return (expression_result_t) {
            .kind = EXPR_RESULT_VALUE,
            .c_type = res.c_type,
            .is_lvalue = false,
            .addr_of = false,
            .is_string_literal = false,
            .value = ir_value_for_var(result)
        };
    } else {
        return EXPR_ERR;
    }
}

ir_instruction_node_t *insert_alloca(ir_gen_context_t *context, const ir_type_t *ir_type, ir_var_t result) {
    // save the current position of the builder
    ir_instruction_node_t *position = ir_builder_get_position(context->builder);
    bool should_restore = position != NULL && position != context->alloca_tail;

    ir_builder_position_after(context->builder, context->alloca_tail);
    ir_instruction_node_t *alloca_node = ir_build_alloca(context->builder, ir_type, result);
    context->alloca_tail = alloca_node;

    // restore the builder position
    if (should_restore) {
        ir_builder_position_after(context->builder, position);
    }

    return alloca_node;
}

const ir_type_t *ir_ptr_int_type(const ir_gen_context_t *context) {
    return context->arch->ptr_int_type;
}

ir_value_t ir_make_const_int(const ir_type_t *type, long long value) {
    return (ir_value_t) {
        .kind = IR_VALUE_CONST,
        .constant = {
            .kind = IR_CONST_INT,
            .type = type,
            .value.i = value,
        }
    };
}

ir_value_t ir_make_const_float(const ir_type_t *type, double value) {
    return (ir_value_t) {
        .kind = IR_VALUE_CONST,
        .constant = {
            .kind = IR_CONST_FLOAT,
            .type = type,
            .value.f = value,
        },
    };
}

bool is_tag_incomplete_type(const tag_t *tag) {
    assert(tag != NULL);
    return tag->incomplete;
}

/**
 * Some types (structs/enums) reference type definitions that occur elsewhere, which need to be looked up.
 * Other types (arrays, pointers, structs) can reference these as inner types, so they also need to be handled
 * specially.
 * @param context Codegen context
 * @param c_type  C type to resolve
 * @return Resolved c type
 */
const type_t* resolve_type(ir_gen_context_t *context, const type_t *c_type) {
    switch (c_type->kind) {
        case TYPE_ARRAY: {
            const type_t *element_type = c_type->value.array.element_type;
            const type_t *resolved_element_type = resolve_type(context, element_type);
            if (resolved_element_type != element_type) {
                type_t *resolved_type = malloc(sizeof(type_t));
                *resolved_type = *c_type;
                resolved_type->value.array.element_type = resolved_element_type;
                return resolved_type;
            }
            return c_type;
        }
        case TYPE_ENUM: {
            // Is there a better way to check if this has been resolved?
            if (c_type->value.enum_specifier.enumerators.size > 0) return c_type;
            // Look up the tag starting at the current scope
            tag_t *tag = lookup_tag(context, c_type->value.enum_specifier.identifier->value);
            assert(tag != NULL);
            return tag->c_type;
        }
        case TYPE_POINTER: {
            const type_t *element_type = c_type->value.pointer.base;
            const type_t *resolved_element_type = resolve_type(context, element_type);
            if (resolved_element_type != element_type) {
                type_t *resolved_type = malloc(sizeof(type_t));
                *resolved_type = *c_type;
                resolved_type->value.pointer.base = resolved_element_type;
                return resolved_type;
            }
            return c_type;
        }
        case TYPE_STRUCT_OR_UNION: {
            const tag_t *tag = lookup_tag(context, c_type->value.struct_or_union.identifier->value);
            assert(tag != NULL);
            return tag->c_type;
        }
        default:
            // Scalar types don't reference other types, so don't need to be resolved
            return c_type;
    }
}

/**
 * Recursively resolve a struct type.
 * Needed to avoid incorrectly resolving the types of fields if a new struct or enum type with the same name as one
 * referenced by a field has been declared between the struct definition and its use.
 * Example:
 * ```
 * struct Bar { float a; float b; };
 * enum Baz { A, B, C };
 * struct Foo { struct Bar a; enum Baz b; };
 * if (c) {
 *     struct Bar { int a; int b; };
 *     struct Foo foo;               // <--- foo.a should have the type struct { float, float }
 *                                   //      but if we wait to look up what the type of tag Bar is at this point,
 *                                   //      we will choose the wrong one (struct { int, int })
 * }
 * ```
 * @param context Codegen context
 * @param c_type Type to resolve (must be a struct)
 * @return Resolved C type
 */
const type_t *resolve_struct_type(ir_gen_context_t *context, const type_t *c_type) {
    assert(c_type->kind == TYPE_STRUCT_OR_UNION);

    // TODO: this needlessly makes copies of every struct type

    type_t *resolved = malloc(sizeof(type_t));
    *resolved = *c_type;
    resolved->value.struct_or_union.field_map = hash_table_create_string_keys(64);
    resolved->value.struct_or_union.fields = (field_ptr_vector_t) VEC_INIT;

    for (int i = 0; i < c_type->value.struct_or_union.fields.size; i += 1) {
        const struct_field_t *field = c_type->value.struct_or_union.fields.buffer[i];
        const type_t *field_type = field->type;
        // TODO: this should also apply to enums?
        if (field->type->kind == TYPE_STRUCT_OR_UNION) {
            if (!field->type->value.struct_or_union.has_body || lookup_tag_in_current_scope(context, field->type->value.struct_or_union.identifier->value) == NULL) {
                // incomplete type we should try to resolve, or tag we haven't created yet
                const tag_t *tag = tag_for_declaration(context, field_type);
                field_type = tag->c_type;
            }
            field_type = resolve_struct_type(context, field_type);
            struct_field_t *new_field = malloc(sizeof(struct_field_t));
            *new_field = *field;
            new_field->type = field_type;
            field = new_field;
        }
        VEC_APPEND(&resolved->value.struct_or_union.fields, (struct_field_t*) field);
        hash_table_insert(&resolved->value.struct_or_union.field_map, field->identifier->value, (void*) field);
    }

    return resolved;
}
