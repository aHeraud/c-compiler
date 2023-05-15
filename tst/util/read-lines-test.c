#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>

#include "CUnit/Basic.h"
#include "../tests.h"

#include "util/read-lines.h"

int write_test_file(FILE** file, char* contents) {
    assert(file != NULL);

    (*file) = tmpfile();
    if ((*file) == NULL) {
        return 1;
    }

    size_t len = strlen(contents);
    if (fwrite(contents, sizeof(char), len, *file) != len) {
        return 1;
    }

    return fseek(*file, 0, SEEK_SET);
}

void empty_file() {
    FILE* file = NULL;
    write_test_file(&file, "");

    size_t lines_len;
    char** lines = read_lines(file, &lines_len);
    CU_ASSERT_EQUAL(lines_len, 0);
}

void single_line_short() {
    FILE* file = NULL;
    write_test_file(&file, "hello world!");

    size_t lines_len;
    char** lines = read_lines(file, &lines_len);

    CU_ASSERT_EQUAL_FATAL(lines_len, 1);
    CU_ASSERT_STRING_EQUAL(lines[0], "hello world!");
}

void single_line_long() {
    FILE* file = NULL;
    char* line = malloc(8192);
    for (int i = 0; i < 8191; i += 1) {
        line[i] = i % 26 + 97; // [a-z]
    }
    line[8191] = '\0';
    write_test_file(&file, line);

    size_t lines_len;
    char** lines = read_lines(file, &lines_len);

    CU_ASSERT_EQUAL_FATAL(lines_len, 1);
    CU_ASSERT_STRING_EQUAL(lines[0], line);
}

void line_endings() {
    FILE* file = NULL;
    char* input = "line 1\nline 2\r\nline 3\n\rline 4\rline 5";
    char* expected[5] = {
        "line 1\n",
        "line 2\r\n",
        "line 3\n\r",
        "line 4\r",
        "line 5"
    };
    write_test_file(&file, input);

    size_t lines_len;
    char** lines = read_lines(file, &lines_len);

    CU_ASSERT_EQUAL_FATAL(lines_len, 5);
    for (int i = 0; i < 5; i += 1) {
        CU_ASSERT_STRING_EQUAL_FATAL(expected[i], lines[i]);
    }
}

int read_lines_test_init_suite() {
    CU_pSuite pSuite = CU_add_suite("read-lines", NULL, NULL);
    if (NULL == CU_add_test(pSuite, "empty file", empty_file) ||
        NULL == CU_add_test(pSuite, "single line, short", single_line_short) ||
        NULL == CU_add_test(pSuite, "single line, long", single_line_long) ||
        NULL == CU_add_test(pSuite, "line endings", line_endings)
    ) {
        CU_cleanup_registry();
        return CU_get_error();
    }
}
