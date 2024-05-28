#include "ir/ir.h"
#include "ir/cfg.h"

typedef struct IrSsaBasicBlock ir_ssa_basic_block_t;

typedef struct IrPhiNodeOperand {
    const char* name;
    const ir_ssa_basic_block_t *block;
} ir_phi_node_operand_t;

VEC_DEFINE(IrPhiNodeOperandVector, ir_phi_node_operand_vector_t, ir_phi_node_operand_t);

typedef struct IrPhiNode {
    ir_var_t var;
    ir_phi_node_operand_vector_t operands;
} ir_phi_node_t;

typedef struct IrPhiNodeVector {
    ir_phi_node_t *buffer;
    size_t size;
    size_t capacity;
} ir_phi_node_vector_t;

typedef struct IrSsaBasicBlock ir_ssa_basic_block_t;

typedef struct IrSsaBasicBlockPtrVector {
    ir_ssa_basic_block_t **buffer;
    size_t size;
    size_t capacity;
} ir_ssa_basic_block_ptr_vector_t;

/**
 * SSA basic block
 * Similar the the regular basic block, but with a few differences, mainly:
 * - Each block has a list of phi nodes at the beginning
 */
typedef struct IrSsaBasicBlock {
    // Unique identifier for the block
    int id;
    // Label of the block (if any)
    const char* label;
    // If this is the entry block for the function
    bool is_entry;
    // List of phi nodes for this block
    ir_phi_node_vector_t phi_nodes;
    // List of instructions for this block (e.g. body)
    ir_instruction_vector_t instructions;
    ir_ssa_basic_block_t *fall_through;
    // Predecessors of this block
    ir_ssa_basic_block_ptr_vector_t predecessors;
    // Successors of this block
    ir_ssa_basic_block_ptr_vector_t successors;
    // If this block has been sealed
    bool sealed;
} ir_ssa_basic_block_t;

typedef struct IrSsaControlFlowGraph {
    const ir_function_definition_t *function;
    ir_ssa_basic_block_t *entry;
    ir_ssa_basic_block_ptr_vector_t basic_blocks;
    hash_table_t label_to_block_map;
} ir_ssa_control_flow_graph_t;

/**
 * Convert a control flow graph into SSA form.
 * @param cfg Control flow graph to convert
 * @return A SSA control flow graph
 */
ir_ssa_control_flow_graph_t ir_convert_cfg_to_ssa(ir_control_flow_graph_t *cfg);

/**
 * Prints the SSA control flow graph to a file/stream. The output is in the DOT format.
 * @param file file/stream to write to
 * @param function_list list of function ssa control flow graphs to include
 * @param length number of functions in the list
 */
void ir_print_ssa_control_flow_graph(FILE *file, const ir_ssa_control_flow_graph_t *function_list, size_t length);