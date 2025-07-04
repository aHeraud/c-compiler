#include "errors.h"
#include "ir/codegen/internal.h"

const ir_initializer_result_t INITIALIZER_RESULT_ERR = {
    .c_type = NULL,
    .type = NULL,
};

ir_initializer_result_t ir_visit_array_initializer(ir_gen_context_t *context, ir_value_t ptr, const type_t *c_type, const initializer_list_t *initializer) {
    const ir_type_t *type = ir_get_type_of_value(ptr);
    assert(type->kind == IR_TYPE_PTR);
    assert(type->value.ptr.pointee->kind == IR_TYPE_ARRAY);
    const ir_type_t *element_type = type->value.ptr.pointee->value.array.element;
    const ir_type_t *element_ptr_type = get_ir_ptr_type(element_type);
    assert(c_type->kind == TYPE_ARRAY);
    const type_t *c_element_type = c_type->value.array.element_type;

    bool known_size = c_type->value.array.size != NULL;
    if (!known_size) {
        //if size is not known, treat the ir type as a raw pointer
        ir_var_t tmp = temp_var(context, element_ptr_type);
        ir_build_bitcast(context->builder, ptr, tmp);
        ptr = ir_value_for_var(tmp);
    }

    typedef struct IrConstArrayInitializerElement {
        unsigned int index;
        ir_const_t value;
    } ir_const_array_initializer_element;

    VEC_DEFINE(IrConstArrayInitializerList, ir_const_array_initializer_list, ir_const_array_initializer_element);

    bool is_const = true;
    ir_const_array_initializer_list elements = VEC_INIT;


    // TODO: if the initializer is constant and contiguous, we can declare it as a global constant and memcpy it
    // For now this just lazily generates code to initialize the array
    int index = 0; // a designator could change the current index, so we track the array index separately than the index
                   // into the list of initializer list elements
    int inferred_array_length = 0;
    for (size_t i = 0; i < initializer->size; i += 1) {
        initializer_list_element_t element = initializer->buffer[i];
        if (element.designation != NULL && element.designation->size > 0) {
            // Handle the designator top level designator, then recursively visit the initializer
            designator_t designator = element.designation->buffer[0];
            if (designator.kind != DESIGNATOR_INDEX) {
                // TODO: error for invalid designator
                // TODO: source position for designators
                fprintf(stderr, "field designator can only be used to initialize a struct or union type\n");
                exit(1);
            }
            expression_result_t index_expr = ir_visit_constant_expression(context, designator.value.index);
            if (index_expr.kind == EXPR_RESULT_ERR) continue;
            if (index_expr.is_lvalue) index_expr = get_rvalue(context, index_expr);
            if (index_expr.value.constant.kind != IR_CONST_INT) {
                // TODO: array index designator constant must have integer type
                // TODO: source position for designators
                fprintf(stderr, "array index designator must have integer type\n");
                exit(1);
            }

            // the designators modifies the current array index, the next element in the array will start after the new
            // index of the current element
            index = index_expr.value.constant.value.i;
            ir_var_t element_ptr = temp_var(context, element_ptr_type);
            ir_build_get_array_element_ptr(context->builder, ptr, ir_make_const_int(ir_ptr_int_type(context), index), element_ptr);

            if (element.designation->size > 1) {
                // create a new initializer list that just includes this element, with the first designator removed
                designator_list_t nested_designators = VEC_INIT;
                for (int j = 1; j < element.designation->size; j += 1) {
                    VEC_APPEND(&nested_designators, element.designation->buffer[j]);
                }
                initializer_list_t nested_initializer_list = VEC_INIT;
                initializer_list_element_t nested_element = {
                    .designation = &nested_designators,
                    .initializer = element.initializer,
                };
                VEC_APPEND(&nested_initializer_list, nested_element);
                initializer_t nested_initializer = {
                    .kind = INITIALIZER_LIST,
                    .span = element.initializer->span, // TODO: this should include the designator
                    .value.list = &nested_initializer_list,
                };

                // recursively visit it
                ir_initializer_result_t result = ir_visit_initializer(context, ir_value_for_var(element_ptr), c_element_type, &nested_initializer);

                is_const &= result.has_constant_value;
                if (is_const) {
                    ir_const_array_initializer_element element = {
                        .index = index,
                        .value = result.constant_value,
                    };
                    VEC_APPEND(&elements, element);
                }

                // cleanup
                VEC_DESTROY(&nested_initializer_list);
                VEC_DESTROY(&nested_designators);
            } else {
                // visit the initializer
                ir_initializer_result_t result = ir_visit_initializer(context, ir_value_for_var(element_ptr), c_element_type, element.initializer);
                is_const &= result.has_constant_value;
                if (is_const) {
                    ir_const_array_initializer_element element = {
                        .index = index,
                        .value = result.constant_value,
                    };
                    VEC_APPEND(&elements, element);
                }
            }
        } else {
            // For fixed size arrays, we will ignore elements in the initializer list that exceed the length of the
            // array. Arrays without a specified size (e.g. `int a[] = {1, 2, 3};`) are a special case, where we ignore
            // the ir array type length.
            if (known_size && i >= type->value.ptr.pointee->value.array.length) {
                // TODO: warn that initializer is longer than array
                continue;
            }
            ir_value_t index_val = ir_make_const_int(ir_ptr_int_type(context), index);
            ir_var_t element_ptr = temp_var(context, element_ptr_type);
            ir_build_get_array_element_ptr(context->builder, ptr, index_val, element_ptr);
            ir_initializer_result_t result = ir_visit_initializer(context, ir_value_for_var(element_ptr), c_type->value.array.element_type, element.initializer);
            is_const &= result.has_constant_value;
            if (is_const) {
                ir_const_array_initializer_element element = {
                    .index = index,
                    .value = result.constant_value,
                };
                VEC_APPEND(&elements, element);
            }
        }

        // bookkeeping for the array index
        index += 1;
        if (index > inferred_array_length) inferred_array_length = index;
    }

    // Return the type of the array
    // We need to do this to handle arrays without a specified size (e.g. `int a[] = {1, 2, 3};`), as the size is
    // inferred from the initializer, which we hadn't visited yet when we created the symbol.
    unsigned int length = inferred_array_length;
    ir_type_t *ir_type = ir_get_type_of_value(ptr)->value.ptr.pointee;
    if (!known_size) {
        // Create new ir/c types that contain the correct size.
        ir_type = malloc(sizeof(ir_type_t));
        *ir_type = (ir_type_t) {
            .kind = IR_TYPE_ARRAY,
            .value.array = {
                .element = element_type,
                .length = length,
            },
        };

        type_t *new_c_type = malloc(sizeof(type_t));
        *new_c_type = *c_type;
        expression_t *size_expr = malloc(sizeof(expression_t));
        char *size_token_value = malloc(length/10 + 2);
        snprintf(size_token_value, length/10 + 2, "%d", length);
        *size_expr = (expression_t) {
            // TODO: source position
            .kind = EXPRESSION_PRIMARY,
            .value.primary = {
                .kind = PE_CONSTANT,
                .value.token = {
                    .kind = TK_INTEGER_CONSTANT,
                    .value = size_token_value,
                },
            },
        };
        new_c_type->value.array.size = size_expr;
        c_type = new_c_type;
    } else {
        // array lengths are compile time constants
        expression_result_t size_result = ir_visit_constant_expression(context, c_type->value.array.size);
        assert(size_result.kind == EXPR_RESULT_VALUE &&
               size_result.value.kind == IR_VALUE_CONST &&
               size_result.value.constant.kind == IR_CONST_INT);
        length = size_result.value.constant.value.i;
    }

    ir_initializer_result_t result = {
        .c_type = c_type,
        .type = ir_type,
        .constant_value = false,
    };

    if (is_const) {
        ir_const_t *array_elements = malloc(length * sizeof(ir_const_t));

        // zero initialize
        ir_value_t zero_value = ir_get_zero_value(context, element_type);
        assert(zero_value.kind == IR_VALUE_CONST);
        for (int i = 0; i < length; i++) {
            array_elements[i] = zero_value.constant;
        }

        // set the initialized elements
        for (int i = 0; i < elements.size; i += 1) {
            unsigned int element_index = elements.buffer[i].index;
            assert(element_index < length);
            array_elements[element_index] = elements.buffer[i].value;
        }

        ir_const_t const_value = {
            .kind = IR_CONST_ARRAY,
            .type = ir_type,
            .value = {
                .array = {
                    .length = length,
                    .values = array_elements,
                },
            },
        };
        result.has_constant_value = true;
        result.constant_value = const_value;
    }

    return result;
}

ir_initializer_result_t ir_visit_struct_initializer(ir_gen_context_t *context, ir_value_t ptr, const type_t *c_type, const initializer_list_t *initializer_list) {
    assert(c_type->kind == TYPE_STRUCT_OR_UNION);
    const ir_type_t *ir_ptr_type = ir_get_type_of_value(ptr);
    assert(ir_ptr_type->kind == IR_TYPE_PTR);
    const ir_type_t *ir_struct_type = ir_ptr_type->value.ptr.pointee;
    assert(ir_struct_type->kind == IR_TYPE_STRUCT_OR_UNION);

    const field_ptr_vector_t *fields = &c_type->value.struct_or_union.fields;
    const hash_table_t *field_map = &c_type->value.struct_or_union.field_map;

    const ir_struct_field_ptr_vector_t *ir_fields = &ir_struct_type->value.struct_or_union.fields;
    const hash_table_t *ir_field_map = &ir_struct_type->value.struct_or_union.field_map;

    int field_index = 0;
    for (int i = 0; i < initializer_list->size; i += 1) {
        initializer_list_element_t element = initializer_list->buffer[i];
        if (element.designation != NULL && element.designation->size > 0) {
            // Handle the designator top level designator, then recursively visit the initializer
            designator_t designator = element.designation->buffer[0];
            if (designator.kind != DESIGNATOR_FIELD) {
                // TODO: error for invalid designator
                // TODO: source position for designators
                fprintf(stderr, "Index designator can only be used to initialize an array element\n");
                exit(1);
            }

            // Look up the field
            const token_t *field_name = designator.value.field;
            struct_field_t *field = NULL;
            if (!hash_table_lookup(field_map, field_name->value, (void**) &field)) {
                append_compilation_error(&context->errors, (compilation_error_t) {
                    .kind = ERR_INVALID_STRUCT_FIELD_REFERENCE,
                    .location = field_name->position,
                    .value.invalid_struct_field_reference = {
                        .field = *field_name,
                        .type = c_type,
                    },
                });

                // TODO: how to handle errors processing the initializer list?
                continue;
            }
            field_index = field->index;

            // Get the IR field (may have a different index due to padding)
            ir_struct_field_t *ir_field = NULL;
            assert(hash_table_lookup(ir_field_map, field_name->value, (void**) &ir_field));

            // Get a pointer to the field
            ir_var_t element_ptr = temp_var(context, get_ir_ptr_type(ir_field->type));
            ir_build_get_struct_member_ptr(context->builder, ptr, ir_field->index, element_ptr);

            if (element.designation->size > 1) {
                // create a new initializer list that just includes this element, with the first designator removed
                designator_list_t nested_designators = VEC_INIT;
                for (int j = 1; j < element.designation->size; j += 1) {
                    VEC_APPEND(&nested_designators, element.designation->buffer[j]);
                }
                initializer_list_t nested_initializer_list = VEC_INIT;
                initializer_list_element_t nested_element = {
                        .designation = &nested_designators,
                        .initializer = element.initializer,
                };
                VEC_APPEND(&nested_initializer_list, nested_element);
                initializer_t nested_initializer = {
                        .kind = INITIALIZER_LIST,
                        .span = element.initializer->span, // TODO: this should include the designator
                        .value.list = &nested_initializer_list,
                };
                // recursively visit it
                ir_initializer_result_t element_initializer_result =
                    ir_visit_initializer(context, ir_value_for_var(element_ptr), field->type, &nested_initializer);
                VEC_DESTROY(&nested_initializer_list);
                VEC_DESTROY(&nested_designators);
            } else {
                // visit the initializer
                ir_visit_initializer(context, ir_value_for_var(element_ptr), field->type, element.initializer);
            }
        } else {
            // No designator, this just refers to the current field index (either the first field, or the field following
            // the last field we visited in the list).
            // e.g. `struct Foo foo = { 1, 2, 3 };`
            if (field_index >= fields->size) {
                // TODO: warn about too many elements in struct initializer
                continue;
            }

            const struct_field_t *field = fields->buffer[field_index];

            // Get the IR field, it may have a different index after adding padding, so look it up by name.
            const ir_struct_field_t *ir_field = NULL;
            assert(hash_table_lookup(ir_field_map, field->identifier->value, (void**) &ir_field));

            // Get a pointer to the field
            ir_var_t element_ptr = temp_var(context, get_ir_ptr_type(ir_field->type));
            ir_build_get_struct_member_ptr(context->builder, ptr, ir_field->index, element_ptr);

            // visit the initializer
            ir_visit_initializer(context, ir_value_for_var(element_ptr), field->type, element.initializer);
        }
        field_index += 1;
    }

    return (ir_initializer_result_t) {
        .c_type = c_type,
        .type = ir_struct_type,
        .has_constant_value = false,
    };
}

ir_initializer_result_t ir_visit_initializer_list(ir_gen_context_t *context, ir_value_t ptr, const type_t *c_type, const initializer_list_t *initializer_list) {
    const ir_type_t *ir_type = ir_get_type_of_value(ptr);
    assert(ir_type->kind == IR_TYPE_PTR);
    switch (ir_type->value.ptr.pointee->kind) {
        case IR_TYPE_ARRAY:
            return ir_visit_array_initializer(context, ptr, c_type, initializer_list);
        case IR_TYPE_STRUCT_OR_UNION: {
            return ir_visit_struct_initializer(context, ptr, c_type, initializer_list);
        }
        default: {
            fprintf(stderr, "%s:%d: Invalid type for initializer list\n", __FILE__, __LINE__);
            exit(1);
        }
    }
}

ir_initializer_result_t ir_visit_initializer(ir_gen_context_t *context, ir_value_t ptr, const type_t *var_ctype, const initializer_t *initializer) {
    switch (initializer->kind) {
        case INITIALIZER_EXPRESSION: {
            expression_result_t result =  ir_visit_expression(context, initializer->value.expression);

            // Error occurred while evaluating the initializer
            if (result.kind == EXPR_RESULT_ERR) return INITIALIZER_RESULT_ERR;

            // If the initializer is an lvalue, load the value
            // TODO: not sure that this is correct
            if (result.is_lvalue) result = get_rvalue(context, result);

            // Verify that the types are compatible, convert if necessary
            result = convert_to_type(context, result.value, result.c_type, var_ctype);
            if (result.kind == EXPR_RESULT_ERR) return INITIALIZER_RESULT_ERR;

            // Store the result in the allocated storage
            ir_build_store(context->builder, ptr, result.value);

            return (ir_initializer_result_t) {
                .type = ir_get_type_of_value(result.value),
                .c_type = var_ctype,
                .has_constant_value = result.value.kind == IR_VALUE_CONST,
                .constant_value = result.value.constant,
            };
        }
        case INITIALIZER_LIST: {
            return ir_visit_initializer_list(context, ptr, var_ctype, initializer->value.list);
        }
        default: {
            fprintf(stderr, "%s:%d: Invalid initializer kind", __FILE__, __LINE__);
            exit(1);
        }
    }
}
