#include <stdio.h>
#include "ir/cfg.h"

void bb_append_instr(ir_basic_block_t *bb, ir_instruction_t *instr) {
    ir_instruction_ptr_vector_t *instructions = &bb->instructions;
    VEC_APPEND(instructions, instr);
}

void bb_append_predecessor(ir_basic_block_t *bb, ir_basic_block_t *predecessor) {
    ir_basic_block_ptr_vector_t *predecessors = &bb->predecessors;
    VEC_APPEND(predecessors, predecessor);
}

void bb_append_successor(ir_basic_block_t *bb, ir_basic_block_t *successor) {
    ir_basic_block_ptr_vector_t *successors = &bb->successors;
    VEC_APPEND(successors, successor);
}

void cfg_append_basic_block(ir_control_flow_graph_t *cfg, ir_basic_block_t *bb) {
    ir_basic_block_ptr_vector_t *basic_blocks = &cfg->basic_blocks;
    VEC_APPEND(basic_blocks, bb);
}

/**
 * Returns true if the instruction should be the last in a basic block.
 * @param instr
 * @return true if the instruction should be the last in a basic block
 */
bool split_block_after(const ir_instruction_t *instr) {
    // Instructions that modify control flow should be the last in a basic block
    switch (instr->opcode) {
        case IR_BR:
        case IR_BR_COND:
        case IR_RET:
            return true;
        default:
            return false;
    }
}

/**
 * Returns true if the instruction should be the first in a basic block.
 * @param instr
 * @return true if the instruction should be the first in a basic block
 */
bool split_block_before(const ir_instruction_t *instr) {
    // Labeled instructions should be the first in a basic block, since they may have multiple predecessors.
    return instr->label != NULL;
}

/**
 * Returns true if the instruction can transfer control linearly to the next instruction.
 */
bool fall_through(const ir_instruction_t *instr) {
    switch (instr->opcode) {
        case IR_BR:
        case IR_RET:
            return false;
        default:
            return true;
    }
}

const char* jump_target(const ir_instruction_t *instr) {
    switch (instr->opcode) {
        case IR_BR:
            return instr->br.label;
        case IR_BR_COND:
            return instr->br_cond.label;
        default:
            return NULL;
    }
}

ir_basic_block_t create_basic_block() {
    static int id = 0;
    ir_basic_block_t bb = {
        .id = id++,
        .predecessors = (ir_basic_block_ptr_vector_t) { .buffer = NULL, .size = 0, .capacity = 0 },
        .successors = (ir_basic_block_ptr_vector_t) { .buffer = NULL, .size = 0, .capacity = 0 },
        .instructions = (ir_instruction_ptr_vector_t) { .buffer = NULL, .size = 0, .capacity = 0 }
    };
    return bb;
}

ir_control_flow_graph_t ir_create_control_flow_graph(ir_function_definition_t *function) {
    ir_control_flow_graph_t cfg = (ir_control_flow_graph_t) {
        .function = function,
        .entry = NULL,
        .basic_blocks = (ir_basic_block_ptr_vector_t) { .buffer = NULL, .size = 0, .capacity = 0 }
    };

    // Map of label -> basic block that contains it
    hash_table_t label_to_block = hash_table_create(64);

    // Create the basic block for the function entry point
    ir_basic_block_t *entry = malloc(sizeof(ir_basic_block_t));
    *entry = create_basic_block();

    // Add the entry block to the CFG
    cfg.entry = entry;
    cfg_append_basic_block(&cfg, entry);

    // Iterate over the instructions, creating basic blocks as necessary
    ir_basic_block_t *current_block = entry;
    for (size_t i = 0; i < function->num_instructions; i += 1) {
        ir_instruction_t *instr = &function->instructions[i];

        if (split_block_before(instr)) {
            // Split the current block before this instruction
            ir_basic_block_t *new_block = malloc(sizeof(ir_basic_block_t));
            *new_block = create_basic_block();
            cfg_append_basic_block(&cfg, new_block);
            bb_append_predecessor(new_block, current_block);
            bb_append_successor(current_block, new_block);
            current_block = new_block;
        }

        if (instr->label != NULL) {
            hash_table_insert(&label_to_block, instr->label, current_block);
        }

        // Append the instruction to the current block
        bb_append_instr(current_block, instr);

        if (split_block_after(instr)) {
            ir_basic_block_t *new_block = malloc(sizeof(ir_basic_block_t));
            *new_block = create_basic_block();
            cfg_append_basic_block(&cfg, new_block);
            if (fall_through(instr)) {
                bb_append_predecessor(new_block, current_block);
                bb_append_successor(current_block, new_block);
            }
            current_block = new_block;
        }
    }

    // If the last block is empty, remove it
    if (current_block->instructions.size == 0) {
        // We need to remove references to this block from its predecessors
        for (size_t i = 0; i < current_block->predecessors.size; i += 1) {
            ir_basic_block_t *pred = current_block->predecessors.buffer[i];
            for (size_t j = 0; j < pred->successors.size; j += 1) {
                if (pred->successors.buffer[j] == current_block) {
                    // Remove the successor from the predecessor
                    // Swap the last element into the current position, then decrement the size
                    pred->successors.buffer[j] = pred->successors.buffer[pred->successors.size - 1];
                    pred->successors.size -= 1;
                    break;
                }
            }
        }
        cfg.basic_blocks.size -= 1;
        free(current_block);
    }

    // At this point, the blocks only have predecessors/successors based on control flow fall-through. We need
    // to add predecessor/successor blocks based on branch instructions.
    for (size_t i = 0; i < cfg.basic_blocks.size; i += 1) {
        ir_basic_block_t *bb = cfg.basic_blocks.buffer[i];
        if (bb->instructions.size == 0) {
            continue;
        }

        ir_instruction_t *last_instr = bb->instructions.buffer[bb->instructions.size - 1];
        const char *target = jump_target(last_instr);
        if (target != NULL) {
            // The last instruction is a branch, we need to find the basic block that contains the target label.
            ir_basic_block_t *target_block = NULL;
            if (hash_table_lookup(&label_to_block, target, (void**) &target_block)) {
                bb_append_successor(bb, target_block);
                bb_append_predecessor(target_block, bb);
            }
        }
    }

    return cfg;
}


void ir_print_control_flow_graph(FILE *file, const ir_control_flow_graph_t *function_list, size_t length) {
    fprintf(file, "digraph G {\n");
    for (size_t i = 0; i < length; i += 1) {
        const ir_control_flow_graph_t cfg = function_list[i];
        fprintf(file, "  subgraph cluster_%s {\n", cfg.function->name);
        fprintf(file, "    label=\"%s\";\n", cfg.function->name);
        for (size_t j = 0; j < cfg.basic_blocks.size; j += 1) {
            ir_basic_block_t *bb = cfg.basic_blocks.buffer[j];
            fprintf(file, "    block_%d [\n      shape=box\n      label=\n", bb->id);
            for (size_t k = 0; k < bb->instructions.size; k += 1) {
                char buffer[512];
                ir_fmt_instr(buffer, 512, bb->instructions.buffer[k]);
                fprintf(file, "        \"%s\\l\"%s\n", buffer, k + 1 < bb->instructions.size ? " +" : "");
            }
            fprintf(file, "    ];\n");

            for (size_t k = 0; k < bb->successors.size; k += 1) {
                ir_basic_block_t *succ = bb->successors.buffer[k];
                fprintf(file, "    block_%d -> block_%d;\n", bb->id, succ->id);
            }
        }
        fprintf(file, "  }\n");
    }
    fprintf(file, "}\n");
}
