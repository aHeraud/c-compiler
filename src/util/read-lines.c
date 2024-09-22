#include <assert.h>
#include <stdio.h>
#include <malloc.h>
#include <string.h>

#include "vectors.h"

char** read_lines(FILE* file, size_t* output_len) {
    assert(file != NULL);
    assert(output_len != NULL);

    size_t lines_len = 0;
    size_t lines_max_len = 1024;
    char** lines = malloc(sizeof(char*) * lines_max_len);

    char* line = NULL;
    size_t line_len = 0;

    char* buffer = malloc(4096);
    size_t buffer_max_len = 4096;
    size_t buffer_len = fread(buffer, 1, 4096, file);

    // Stream the file in 4KB chunks and break it up into lines as we go
    while (buffer_len > 0) {
        int start = 0;
        int end = 0;
        while (end < buffer_len) {
            if (buffer[end] == '\n' || buffer[end] == '\r') {
                if (end + 1 >= buffer_len) {
                    // we need to see the next character to see if it's part of an EOL sequence (CRLF or LFCR)
                    if (buffer_len >= buffer_max_len) {
                        buffer = realloc(buffer, ++buffer_max_len);
                    }
                    int next = fgetc(file);
                    if (EOF != next) {
                        buffer[buffer_len] = (char) next;
                    } else if (ferror(file)) {
                        perror("I/O error when reading");
                        return NULL;
                    }
                }

                if (buffer[end] == '\r' && end + 1 < buffer_len && buffer[end + 1] == '\n' ||
                    buffer[end] == '\n' && end + 1 < buffer_len && buffer[end + 1] == '\r') {
                    end += 1;
                }

                if (line == NULL) {
                    line_len = 1 + end - start;
                    line = malloc(line_len + 1);
                    memcpy(line, buffer + start, line_len);
                } else {
                    size_t len = 1 + end - start;
                    line = realloc(line, line_len + len + 1);
                    memcpy(line + line_len, buffer + start, len);
                    line_len += len;
                }
                line[line_len] = '\0';

                append_ptr((void***) &lines, &lines_len, &lines_max_len, line);

                line = NULL;
                line_len = 0;

                start = ++end;
            }

            end += 1;
        }

        // copy whatever is left in the buffer
        if (line == NULL) {
            line_len = end - start;
            line = malloc(line_len);
            memcpy(line, buffer + start, line_len);
        } else {
            size_t len = end - start;
            line = realloc(line, line_len + len);
            memcpy(line + line_len, buffer + start, len);
            line_len += len;
        }

        buffer_len = fread(buffer, 1, 4096, file);
    }

    // copy the last line if it didn't end with a newline
    if (line != NULL) {
        append_ptr((void***) &lines, &lines_len, &lines_max_len, line);
    }

    if (buffer_len == 0 && ferror(file)) {
        printf("I/O error when reading");
        return NULL;
    }

    free(buffer);

    // shrink the lines vector to the right size
    lines = reallocarray(lines, lines_len, sizeof(char*));
    (*output_len) = lines_len;
    return lines;
}
