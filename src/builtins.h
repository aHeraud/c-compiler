#ifndef _C_COMPILER_BUILTINS_H
#define _C_COMPILER_BUILTINS_H

#define BUILTIN_SOURCE_POS (source_position_t) { .path = "<builtin>", .line = 0, .column = 0 }

#define BUILTIN_VA_LIST_NAME "__builtin_va_list"

static const type_t RAW_BUILTIN_VA_LIST_TYPE = {
    .kind = TYPE_BUILTIN,
    .is_const = false,
    .is_volatile = false,
    .storage_class = STORAGE_CLASS_AUTO,
    .value = {
        .builtin_name = BUILTIN_VA_LIST_NAME,
    },
};

const type_t *BUILTIN_TYPES[] = {
    &RAW_BUILTIN_VA_LIST_TYPE,
};

#endif