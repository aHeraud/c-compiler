#include "errors.h"
#include "util/vectors.h"

void append_compilation_error(compilation_error_vector_t *errors, compilation_error_t error) {
    VEC_APPEND(errors, error);
}
