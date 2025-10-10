#include <string.h>

#include "target.h"
#include "builtins.h"

const type_t *get_x86_64_linux_va_list_type(void) {
    // type = struct { i32, i32, ptr, ptr }

    token_t *identifier = make_identifier_token(BUILTIN_VA_LIST_NAME, BUILTIN_SOURCE_POS, malloc(sizeof(token_t)));
    type_t *type = malloc(sizeof(type_t));
    *type = (type_t) {
        .kind = TYPE_STRUCT_OR_UNION,
        .is_const = false,
        .is_volatile = false,
        .value.struct_or_union = {
            .is_union = false,
            .identifier = identifier,
            .fields = (field_ptr_vector_t) VEC_INIT,
            .field_map = hash_table_create_string_keys(1),
            .has_body = true,
            .packed = true,
        },
    };

    struct_field_t *field = malloc(sizeof(struct_field_t));
    *field = (struct_field_t) {
        .index = 0,
        .type = &INT,
        .identifier = make_identifier_token("0", BUILTIN_SOURCE_POS, malloc(sizeof(token_t))),
    };
    VEC_APPEND(&type->value.struct_or_union.fields, field);

    field = malloc(sizeof(struct_field_t));
    *field = (struct_field_t) {
        .index = 1,
        .type = &INT,
        .identifier = make_identifier_token("1", BUILTIN_SOURCE_POS, malloc(sizeof(token_t))),
    };
    VEC_APPEND(&type->value.struct_or_union.fields, field);

    field = malloc(sizeof(struct_field_t));
    *field = (struct_field_t) {
        .index = 2,
        .type = get_ptr_type(&VOID_PTR),
        .identifier = make_identifier_token("2", BUILTIN_SOURCE_POS, malloc(sizeof(token_t))),
    };
    VEC_APPEND(&type->value.struct_or_union.fields, field);

    field = malloc(sizeof(struct_field_t));
    *field = (struct_field_t) {
        .index = 3,
        .type = get_ptr_type(&VOID_PTR),
        .identifier = make_identifier_token("3", BUILTIN_SOURCE_POS, malloc(sizeof(token_t))),
    };
    VEC_APPEND(&type->value.struct_or_union.fields, field);

    return type;
}

const type_t *get_va_list_type(const target_t *target) {
    if (strcmp(target->name, TARGET_X86_64_UNKNOWN_LINUX_GNU.name) == 0) {
        return get_x86_64_linux_va_list_type();
    }
    // This should be unreachable
    fprintf(stderr, "%s:%d (%s): Error: va_list type not defined for target %s\n", __FILE__, __LINE__, __func__, target->name);
    exit(1);
}
