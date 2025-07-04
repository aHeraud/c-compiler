#ifndef CODEGEN_INTERNAL_H
#define CODEGEN_INTERNAL_H

#include "ast.h"
#include "ir/ir.h"
#include "ir/ir-builder.h"

struct Scope;
struct Symbol;

VEC_DEFINE(StatementPtrVector, statement_ptr_vector_t, statement_t*)

typedef struct IrGenContext {
    ir_module_t *module;
    const ir_arch_t *arch;

    hash_table_t global_map;
    hash_table_t function_definition_map;
    hash_table_t tag_uid_map;

    // State for the current function being visited
    ir_function_definition_t *function;
    const function_definition_t *c_function;
    ir_function_builder_t *builder;
    ir_instruction_node_t *alloca_tail;
    hash_table_t label_map; // map of c label -> ir label
    hash_table_t label_exists; // set of c label that actually exist, for validating the goto statements
    statement_ptr_vector_t goto_statements; // goto statements that need to be validated at the end of the fn

    // Switch instruction node (in a switch statement)
    ir_instruction_node_t *switch_node;
    // Break label (if in a loop/switch case statement)
    char* break_label;
    // Continue label (if in a loop)
    char* continue_label;

    // List of compilation errors encountered during semantic analysis
    compilation_error_vector_t errors;
    // The current lexical scope
    struct Scope *current_scope;
    // Counter for generating unique global variable names
    // These should be unique over the entire module
    unsigned short global_id_counter;
    // Counter for generating unique local variable names
    // These are only unique within the current function
    unsigned short local_id_counter;
    // Counter for generating unique labels
    unsigned short label_counter;
    // Counter for generating unique tag suffixes
    unsigned short tag_id_counter;
} ir_gen_context_t;

typedef enum ExpressionResultKind {
    EXPR_RESULT_ERR,
    EXPR_RESULT_VALUE,
    EXPR_RESULT_INDIRECTION,
} expression_result_kind_t;

struct Symbol;
struct ExpressionResult;
typedef struct ExpressionResult {
    expression_result_kind_t kind;
    const type_t *c_type;
    bool is_lvalue;
    bool addr_of;
    bool is_string_literal;
    // Non-null if this was the result of a primary expression which was an identifier
    const struct Symbol *symbol;
    // only 1 of these is initialized, depending on the value of kind
    ir_value_t value;
    struct ExpressionResult *indirection_inner;
} expression_result_t;

typedef struct Scope {
    hash_table_t symbols;
    hash_table_t tags; // separate namespace for struct/union/enum declarations
    struct Scope *parent;
} scope_t;

enum SymbolKind {
    SYMBOL_ENUMERATION_CONSTANT,
    SYMBOL_LOCAL_VARIABLE,
    SYMBOL_GLOBAL_VARIABLE,
    SYMBOL_FUNCTION,
};

typedef struct Symbol {
    enum SymbolKind kind;
    // The token containing the name of the symbol as it appears in the source.
    const token_t *identifier;
    // The name of the symbol as it appears in the IR.
    const char *name;
    // The C type of this symbol.
    const type_t *c_type;
    // The IR type of this symbol.
    const ir_type_t *ir_type;
    // Pointer to the memory location where this symbol is stored (variables only).
    ir_var_t ir_ptr;
    // True if this has a constant value (e.g. constant storage class)
    bool has_const_value;
    // Constant value for this symbol, only valid if has_constant_value == true
    ir_const_t const_value;
} symbol_t;

typedef struct Tag {
    const token_t *identifier;
    const char* uid; // unique-id (module) for the tag
    const type_t *c_type;
    const ir_type_t *ir_type;
} tag_t;

extern const expression_result_t EXPR_ERR;

symbol_t *lookup_symbol(const ir_gen_context_t *context, const char *name);
symbol_t *lookup_symbol_in_current_scope(const ir_gen_context_t *context, const char *name);

tag_t *lookup_tag(const ir_gen_context_t *context, const char *name);
tag_t *lookup_tag_in_current_scope(const ir_gen_context_t *context, const char *name);
tag_t *lookup_tag_by_uid(const ir_gen_context_t *context, const char *uid);

void declare_symbol(ir_gen_context_t *context, symbol_t *symbol);
void declare_tag(ir_gen_context_t *context, const tag_t *tag);

char *global_name(ir_gen_context_t *context);
char *temp_name(ir_gen_context_t *context);
ir_var_t temp_var(ir_gen_context_t *context, const ir_type_t *type);
char *gen_label(ir_gen_context_t *context);

ir_value_t ir_make_const_int(const ir_type_t *type, long long value);
ir_value_t ir_make_const_float(const ir_type_t *type, double value);
ir_value_t ir_get_zero_value(ir_gen_context_t *context, const ir_type_t *type);

/**
 * Helper to insert alloca instructions for local variables at the top of the function.
 */
ir_instruction_node_t *insert_alloca(ir_gen_context_t *context, const ir_type_t *ir_type, ir_var_t result);

/**
 * Get the C integer type that is the same width as a pointer.
 */
const type_t *c_ptr_uint_type(void);

/**
 * Get the IR integer type that is the same width as a pointer.
 */
const ir_type_t *ir_ptr_int_type(const ir_gen_context_t *context);

/**
 * Convert an IR value from one type to another.
 * Will generate conversion instructions if necessary, and store the result in a new variable,
 * with the exception of trivial conversions or constant values.
 * @param context The IR generation context
 * @param value   The value to convert
 * @param from_type The C type of the value
 * @param to_type  The C type to convert the value to
 * @return The resulting ir value and its corresponding c type
 */
expression_result_t convert_to_type(ir_gen_context_t *context, ir_value_t value, const type_t *from_type, const type_t *to_type);

expression_result_t get_boolean_value(
    ir_gen_context_t *context,
    ir_value_t value,
    const type_t *c_type,
    const expression_t *expr
);

/**
 * Get the IR type that corresponds to a specific C type.
 * @param c_type
 * @return corresponding IR type
 */
const ir_type_t* get_ir_type(ir_gen_context_t *context, const type_t *c_type);

/**
 * Get the IR type that corresponds to a C struct/union type.
 * This should only be called when creating the declaration/tag
 * @param context
 * @param c_type
 * @param id
 * @return New struct/union type
 */
const ir_type_t *get_ir_struct_type(ir_gen_context_t *context, const type_t *c_type, const char *id);

/**
 * Get the IR type that is a pointer to the specified IR type
 * @param pointee The type that the pointer points to
 * @return The pointer type
 */
const ir_type_t *get_ir_ptr_type(const ir_type_t *pointee);

ir_value_t ir_value_for_var(ir_var_t var);
ir_value_t ir_value_for_const(ir_const_t constant);

ir_value_t get_indirect_ptr(ir_gen_context_t *context, expression_result_t res);

expression_result_t get_rvalue(ir_gen_context_t *context, expression_result_t res);

void enter_scope(ir_gen_context_t *context);
void leave_scope(ir_gen_context_t *context);

void ir_append_function_ptr(ir_function_ptr_vector_t *vec, ir_function_definition_t *function);
void ir_append_global_ptr(ir_global_ptr_vector_t *vec, ir_global_t *global);

typedef struct LoopContext {
    char *break_label;
    char *continue_label;
} loop_context_t;

/**
* Enter a loop context, which will set the loop break and continue labels
* Also saves and returns the previous context
*/
loop_context_t enter_loop_context(ir_gen_context_t *context, char *break_label, char *continue_label);

/**
 * Restore the previous loop context
 */
void leave_loop_context(ir_gen_context_t *context, loop_context_t prev);

expression_result_t get_boolean_value(
    ir_gen_context_t *context,
    ir_value_t value,
    const type_t *c_type,
    const expression_t *expr
);

bool is_tag_incomplete_type(const tag_t *tag);
const tag_t *tag_for_declaration(ir_gen_context_t *context, const type_t *c_type);

/**
 * Recursively resolve a struct type.
 * Needed to avoid incorrectly resolving the types of fields if a new struct or enum type with the same name as one
 * referenced by a field has been declared between the struct definition and its use.
 * Example:
 * ```
 * struct Bar { float a; float b; };
 * enum Baz { A, B, C };
 * struct Foo { struct Bar a; enum Baz b; };
 * if (c) {
 *     struct Bar { int a; int b; };
 *     struct Foo foo;               // <--- foo.a should have the type struct { float, float }
 *                                   //      but if we wait to look up what the type of tag Bar is at this point,
 *                                   //      we will choose the wrong one (struct { int, int })
 * }
 * ```
 * @param context Codegen context
 * @param c_type Type to resolve (must be a struct)
 * @return Resolved C type
 */
const type_t *resolve_struct_type(ir_gen_context_t *context, const type_t *c_type);

typedef struct InitializerResult {
    const type_t *c_type;
    const ir_type_t *type;
    bool has_constant_value;
    ir_const_t constant_value;
} ir_initializer_result_t;

extern const ir_initializer_result_t INITIALIZER_RESULT_ERR;

ir_initializer_result_t ir_visit_initializer(ir_gen_context_t *context, ir_value_t ptr, const type_t *var_ctype, const initializer_t *initializer);
ir_initializer_result_t ir_visit_initializer_list(ir_gen_context_t *context, ir_value_t ptr, const type_t *c_type, const initializer_list_t *initializer_list);

void ir_visit_translation_unit(ir_gen_context_t *context, const translation_unit_t *translation_unit);
void ir_visit_function(ir_gen_context_t *context, const function_definition_t *function);
void ir_visit_statement(ir_gen_context_t *context, const statement_t *statement);
void ir_visit_labeled_statement(ir_gen_context_t *context, const statement_t *statement);
void ir_visit_if_statement(ir_gen_context_t *context, const statement_t *statement);
void ir_visit_return_statement(ir_gen_context_t *context, const statement_t *statement);
void ir_visit_loop_statement(ir_gen_context_t *context, const statement_t *statement);
void ir_visit_break_statement(ir_gen_context_t *context, const statement_t *statement);
void ir_visit_continue_statement(ir_gen_context_t *context, const statement_t *statement);
void ir_visit_goto_statement(ir_gen_context_t *context, const statement_t *statement);
void ir_visit_switch_statement(ir_gen_context_t *context, const statement_t *statement);
void ir_visit_case_statement(ir_gen_context_t *context, const statement_t *statement);
void ir_visit_global_declaration(ir_gen_context_t *context, const declaration_t *declaration);
void ir_visit_declaration(ir_gen_context_t *context, const declaration_t *declaration);
expression_result_t ir_visit_expression(ir_gen_context_t *context, const expression_t *expression);
expression_result_t ir_visit_constant_expression(ir_gen_context_t *context, const expression_t *expression);
expression_result_t ir_visit_array_subscript_expression(ir_gen_context_t *context, const expression_t *expr);
expression_result_t ir_visit_member_access_expression(ir_gen_context_t *context, const expression_t *expr);
expression_result_t ir_visit_primary_expression(ir_gen_context_t *context, const expression_t *expr);
expression_result_t ir_visit_constant(ir_gen_context_t *context, const expression_t *expr);
expression_result_t ir_visit_call_expression(ir_gen_context_t *context, const expression_t *expr);
expression_result_t ir_visit_cast_expression(ir_gen_context_t *context, const expression_t *expr);
expression_result_t ir_visit_binary_expression(ir_gen_context_t *context, const expression_t *expr);
expression_result_t ir_visit_additive_binexpr(ir_gen_context_t *context, const expression_t *expr, expression_result_t lhs, expression_result_t rhs);
expression_result_t ir_visit_assignment_binexpr(ir_gen_context_t *context, const expression_t *expr);
expression_result_t ir_visit_bitwise_binexpr(ir_gen_context_t *context, const expression_t *expr, expression_result_t lhs, expression_result_t rhs);
expression_result_t ir_visit_comparison_binexpr(ir_gen_context_t *context, const expression_t *expr);
expression_result_t ir_visit_multiplicative_binexpr(ir_gen_context_t *context, const expression_t *expr, expression_result_t lhs, expression_result_t rhs);
expression_result_t ir_visit_sizeof_expression(ir_gen_context_t *context, const expression_t *expr);
expression_result_t ir_visit_ternary_expression(ir_gen_context_t *context, const expression_t *expr);
expression_result_t ir_visit_unary_expression(ir_gen_context_t *context, const expression_t *expr);
expression_result_t ir_visit_bitwise_not_unexpr(ir_gen_context_t *context, const expression_t *expr);
expression_result_t ir_visit_logical_not_unexpr(ir_gen_context_t *context, const expression_t *expr);
expression_result_t ir_visit_address_of_unexpr(ir_gen_context_t *context, const expression_t *expr);
expression_result_t ir_visit_indirection_unexpr(ir_gen_context_t *context, const expression_t *expr);
expression_result_t ir_visit_sizeof_unexpr(ir_gen_context_t *context, const expression_t *expr);
expression_result_t ir_visit_increment_decrement(ir_gen_context_t *context, const expression_t *expr, bool pre, bool incr);
expression_result_t ir_visit_logical_expression(ir_gen_context_t *context, const expression_t *expr);
expression_result_t ir_visit_compound_literal(ir_gen_context_t *context, const expression_t *expr);

#endif //CODEGEN_INTERNAL_H