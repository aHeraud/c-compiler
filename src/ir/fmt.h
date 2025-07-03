#include <stdio.h>
#include "ir/ir.h"

/**
 * Format an ir type as a string
 * @param buffer  pointer to output buffer
 * @param size    size of the buffer, in bytes
 * @param type    ir type to format
 * @return The number of characters that would have been written to the buffer, if it had been sufficiently large.
 *         A return value = size signifies that the buffer was too small to fit the entire formatted string.
 *         Returns -1 on error.
 */
int ir_fmt_type(char *buffer, size_t size, const ir_type_t *type);

/**
 * Format an ir constant as a string.
 * @param buffer    pointer to output buffer
 * @param size      size of the buffer, in bytes
 * @param constant  ir constant to format
 * @return The number of characters that would have been written to the buffer, if it had been sufficiently large.
 *         A return value = size signifies that the buffer was too small to fit the entire formatted string.
 *         Returns -1 on error.
 */
int ir_fmt_const(char *buffer, size_t size, ir_const_t constant);

/**
 * Format an ir variable as a string.
 * @param buffer  pointer to output buffer
 * @param size    size of the buffer, in bytes
 * @param var     ir variable to format
 * @return The number of characters that would have been written to the buffer, if it had been sufficiently large.
 *         A return value = size signifies that the buffer was too small to fit the entire formatted string.
 *         Returns -1 on error.
 */
int ir_fmt_var(char *buffer, size_t size, ir_var_t var);

/**
 * Format an ir value as a string.
 * @param buffer  pointer to output buffer
 * @param size    size of the buffer, in bytes
 * @param value   ir variable to format
 * @return The number of characters that would have been written to the buffer, if it had been sufficiently large.
 *         A return value = size signifies that the buffer was too small to fit the entire formatted string.
 *         Returns -1 on error.
 */
int ir_fmt_val(char *buffer, size_t size, ir_value_t value);

/**
 * Format an ir instruction as a string.
 * @param buffer       pointer to output buffer
 * @param size         size of the buffer, in bytes
 * @param instruction  pointer to the ir instruction to format
 * @return The number of characters that would have been written to the buffer, if it had been sufficiently large.
 *         A return value = size signifies that the buffer was too small to fit the entire formatted string.
 *         Returns -1 on error.
 */
int ir_fmt_instr(char *buffer, size_t size, const ir_instruction_t *instruction);

void ir_print_module(FILE *file, const ir_module_t *module);
