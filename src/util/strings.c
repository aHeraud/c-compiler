#include <stddef.h>
#include "vectors.h"

char* replace_escape_sequences(const char* str) {
    char_vector_t vec = {NULL, 0, 0};
    for (size_t i = 0; str[i] != '\0'; i++) {
        if (str[i] == '\\') {
            switch (str[i + 1]) {
                case 'a':
                    VEC_APPEND((&vec), '\a');
                    i++;
                    break;
                case 'b':
                    VEC_APPEND((&vec), '\b');
                    i++;
                    break;
                case 'f':
                    VEC_APPEND((&vec), '\f');
                    i++;
                    break;
                case 'n':
                    VEC_APPEND((&vec), '\n');
                    i++;
                    break;
                case 'r':
                    VEC_APPEND((&vec), '\r');
                    i++;
                    break;
                case 't':
                    VEC_APPEND((&vec), '\t');
                    i++;
                    break;
                case 'v':
                    VEC_APPEND((&vec), '\v');
                    i++;
                    break;
                case '\\':
                    VEC_APPEND((&vec), '\\');
                    i++;
                    break;
                case '\'':
                    VEC_APPEND((&vec), '\'');
                    i++;
                    break;
                case '"':
                    VEC_APPEND((&vec), '"');
                    i++;
                    break;
                case '?':
                    VEC_APPEND((&vec), '?');
                    i++;
                    break;
                default:
                    VEC_APPEND((&vec), str[i]);
                    break;
            }
        } else {
            VEC_APPEND((&vec), str[i]);
        }
    }
    VEC_APPEND((&vec), '\0'); // null terminate
    shrink_char_vector(&vec.buffer, &vec.size, &vec.capacity);
    return vec.buffer;
}
