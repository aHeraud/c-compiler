#ifndef C_COMPILER_CFG_H
#define C_COMPILER_CFG_H

#include <stddef.h>
#include <stdio.h>

#include "util/vectors.h"
#include "util/hashtable.h"
#include "ir/ir.h"

struct IrBasicBlock;

VEC_DEFINE(IrBasicBlockPtrVector, ir_basic_block_ptr_vector_t, struct IrBasicBlock*);

typedef struct IrBasicBlock {
    // Unique identifier for the block
    int id;
    // Label of the first instruction in the block, if any
    const char* label;
    bool is_entry;
    struct IrBasicBlock *fall_through;
    ir_basic_block_ptr_vector_t predecessors;
    ir_basic_block_ptr_vector_t successors;
    ir_instruction_ptr_vector_t instructions;
} ir_basic_block_t;

typedef struct IrControlFlowGraph {
    const ir_function_definition_t *function;
    ir_basic_block_t *entry;
    ir_basic_block_ptr_vector_t basic_blocks;
    hash_table_t label_to_block_map;
} ir_control_flow_graph_t;

/**
 * Converts linear IR code into a control flow graph.
 * @param function Function definition
 * @return control flow graph for the supplied function
 */
ir_control_flow_graph_t ir_create_control_flow_graph(const ir_function_definition_t *function);

/**
 * Converts a control flow graph into linear IR code.
 * @param cfg
 */
ir_instruction_vector_t ir_linearize_cfg(const ir_control_flow_graph_t *cfg);

/**
 * Removes basic blocks that are unreachable from the entry block.
 * @param cfg
 */
void ir_prune_control_flow_graph(ir_control_flow_graph_t *cfg);

/**
 * Prints the control flow graph to a file/stream. The output is in the DOT format.
 * @param file file/stream to write to
 * @param function_list list of function control flow graphs to include
 * @param length number of functions in the list
 */
void ir_print_control_flow_graph(FILE *file, const ir_control_flow_graph_t *function_list, size_t length);

#endif //C_COMPILER_CFG_H
