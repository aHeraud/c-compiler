#include <stdio.h>
#include "ir/ir.h"

const char* ir_fmt_type(char *buffer, size_t size, const ir_type_t *type);
const char* ir_fmt_const(char *buffer, size_t size, ir_const_t constant);
const char* ir_fmt_var(char *buffer, size_t size, ir_var_t var);
const char* ir_fmt_val(char *buffer, size_t size, ir_value_t value);
const char* ir_fmt_instr(char *buffer, size_t size, const ir_instruction_t *instruction);
void ir_print_module(FILE *file, const ir_module_t *module);
