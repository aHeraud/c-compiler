#include "types.h"

// Utility functions for decoding integer and floating point constants

void decode_integer_constant(const token_t *token, unsigned long long *value, const type_t **type);
void decode_float_constant(const token_t *token, long double *value, const type_t **type);
