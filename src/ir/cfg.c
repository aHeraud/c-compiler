#include <stdio.h>
#include "ir/cfg.h"
#include "ir/fmt.h"

void bb_append_instr(ir_basic_block_t *bb, const ir_instruction_t *instr) {
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
        case IR_BR_COND:
            return instr->branch.label;
        default:
            return NULL;
    }
}

ir_basic_block_t create_basic_block(int *id_counter) {
    int id = *id_counter;
    *id_counter += 1;
    ir_basic_block_t bb = {
        .id = id,
        .label = NULL,
        .is_entry = false,
        .fall_through = NULL,
        .predecessors = (ir_basic_block_ptr_vector_t) { .buffer = NULL, .size = 0, .capacity = 0 },
        .successors = (ir_basic_block_ptr_vector_t) { .buffer = NULL, .size = 0, .capacity = 0 },
        .instructions = (ir_instruction_ptr_vector_t) { .buffer = NULL, .size = 0, .capacity = 0 }
    };
    return bb;
}

ir_control_flow_graph_t ir_create_control_flow_graph(const ir_function_definition_t *function) {
    // IDs are only unique within a function
    int id_counter = 0;

    ir_control_flow_graph_t cfg = (ir_control_flow_graph_t) {
        .function = function,
        .entry = NULL,
        .basic_blocks = (ir_basic_block_ptr_vector_t) { .buffer = NULL, .size = 0, .capacity = 0 }
    };

    // Map of label -> basic block that contains it
    hash_table_t label_to_block = hash_table_create_string_keys(64);

    // Create the basic block for the function entry point
    ir_basic_block_t *entry = malloc(sizeof(ir_basic_block_t));
    *entry = create_basic_block(&id_counter);
    entry->is_entry = true;

    // Add the entry block to the CFG
    cfg.entry = entry;
    cfg_append_basic_block(&cfg, entry);

    // Iterate over the instructions, creating basic blocks as necessary
    ir_basic_block_t *current_block = entry;
    for (size_t i = 0; i < function->body.size; i += 1) {
        const ir_instruction_t *instr = &function->body.buffer[i];

        if (instr->label != NULL) {
            hash_table_insert(&label_to_block, instr->label, current_block);
            current_block->label = instr->label;
        }

        // Append the instruction to the current block
        bb_append_instr(current_block, instr);

        bool split_block = split_block_after(instr) ||
            (i + 1 < function->body.size && split_block_before(&function->body.buffer[i + 1]));

        if (split_block) {
            ir_basic_block_t *new_block = malloc(sizeof(ir_basic_block_t));
            *new_block = create_basic_block(&id_counter);
            cfg_append_basic_block(&cfg, new_block);
            if (fall_through(instr)) {
                bb_append_predecessor(new_block, current_block);
                bb_append_successor(current_block, new_block);
                current_block->fall_through = new_block;
            }
            current_block = new_block;
        }
    }

    // If the last block is empty remove it, unless it is the entry block
    if (current_block->instructions.size == 0 && !current_block->is_entry) {
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

    cfg.label_to_block_map = label_to_block;

    return cfg;
}

ir_instruction_vector_t ir_linearize_cfg(const ir_control_flow_graph_t *cfg) {
    ir_instruction_vector_t instructions = { .buffer = NULL, .size = 0, .capacity = 0 };

    hash_table_t visited_table = hash_table_create_pointer_keys(64);
    ir_basic_block_ptr_vector_t stack = { .buffer = NULL, .size = 0, .capacity = 0 };

    VEC_APPEND(&stack, cfg->entry);
    while (stack.size > 0) {
        // Peek the top block from the stack
        ir_basic_block_t *block = stack.buffer[stack.size - 1];

        if (hash_table_lookup(&visited_table, block, NULL)) {
            // We have already visited this block
            stack.size -= 1;
            continue;
        }

        // If this block is the fall-through of another block, and we haven't visited the predecessor yet,
        // push the predecessor onto the stack, so we can visit it first.
        bool should_skip = false;
        for (size_t i = 0; i < block->predecessors.size; i += 1) {
            ir_basic_block_t *pred = block->predecessors.buffer[i];
            if (pred->fall_through == block && !hash_table_lookup(&visited_table, pred, NULL)) {
                VEC_APPEND(&stack, pred);
                should_skip = true;
            }
        }
        if (should_skip) continue;

        // Pop the block from the stack
        stack.size -= 1;

        // Append the instructions from the block to the linearized list
        for (size_t i = 0; i < block->instructions.size; i += 1) {
            VEC_APPEND(&instructions, *block->instructions.buffer[i]);
        }

        // Mark the block as visited
        hash_table_insert(&visited_table, block, NULL);

        // Push the successors onto the stack. If this block falls through to another block, we must visit
        // the fall-through block first, so we will push it last.
        for (size_t i = 0; i < block->successors.size; i += 1) {
            ir_basic_block_t *succ = block->successors.buffer[i];
            if (block->fall_through == succ) {
                continue;
            }

            if (!hash_table_lookup(&visited_table, succ, NULL)) {
                VEC_APPEND(&stack, succ);
            }
        }
        if (block->fall_through != NULL) {
            // We should not have visited the fall-through block yet
            assert(!hash_table_lookup(&visited_table, block->fall_through, NULL));
            VEC_APPEND(&stack, block->fall_through);
        }
    }

    hash_table_destroy(&visited_table);
    VEC_SHRINK(&instructions, ir_instruction_t);
    if (stack.buffer != NULL) {
        free(stack.buffer);
    }
    return instructions;
}

void ir_prune_control_flow_graph(ir_control_flow_graph_t *cfg) {
    assert(cfg != NULL);
    if (cfg->basic_blocks.size == 0) {
        return;
    }

    // Fixed point algorithm to remove unreachable blocks.
    // Could be optimized with a work list to keep track of modified blocks but this is the easiest way to do it.
    ir_basic_block_t *block;
    bool modified = false;
    do {
        modified = false;
        for (size_t i = cfg->basic_blocks.size - 1; i > 0; i -= 1) {
            block = cfg->basic_blocks.buffer[i];
            if (block->predecessors.size == 0 && !block->is_entry) {
                modified = true;

                // If the block has a label, remove it from the label map
                if (block->label != NULL) {
                    hash_table_remove(&cfg->label_to_block_map, block->label, NULL);
                }

                // Swap the last element into the current position, then decrement the size to remove the last element
                cfg->basic_blocks.buffer[i] = cfg->basic_blocks.buffer[cfg->basic_blocks.size - 1];
                cfg->basic_blocks.size -= 1;

                // Remove references to this block from its successors
                for (size_t j = 0; j < block->successors.size; j += 1) {
                    ir_basic_block_t *successor = block->successors.buffer[j];
                    for (size_t k = 0; k < successor->predecessors.size; k += 1) {
                        if (successor->predecessors.buffer[k] == block) {
                            // Swap the last element into the current position, then decrement the size
                            successor->predecessors.buffer[k] = successor->predecessors.buffer[
                                successor->predecessors.size - 1];
                            successor->predecessors.size -= 1;
                            break;
                        }
                    }
                }

                // Free the block
                free(block);
            }
        }
    } while (modified);
}

void ir_print_control_flow_graph(FILE *file, const ir_control_flow_graph_t *function_list, size_t length) {
    fprintf(file, "digraph G {\n");
    for (size_t i = 0; i < length; i += 1) {
        const ir_control_flow_graph_t cfg = function_list[i];
        fprintf(file, "  subgraph cluster_%s {\n", cfg.function->name);
        fprintf(file, "    label=\"%s\";\n", cfg.function->name);
        for (size_t j = 0; j < cfg.basic_blocks.size; j += 1) {
            ir_basic_block_t *bb = cfg.basic_blocks.buffer[j];
            fprintf(file, "    %s_block_%d [\n      shape=box\n      label=\n", cfg.function->name, bb->id);
            for (size_t k = 0; k < bb->instructions.size; k += 1) {
                char buffer[512];
                ir_fmt_instr(buffer, 512, bb->instructions.buffer[k]);
                fprintf(file, "        \"%s\\l\"%s\n", buffer, k + 1 < bb->instructions.size ? " +" : "");
            }
            fprintf(file, "    ];\n");

            for (size_t k = 0; k < bb->successors.size; k += 1) {
                ir_basic_block_t *succ = bb->successors.buffer[k];
                fprintf(file, "    %s_block_%d -> %s_block_%d;\n", cfg.function->name, bb->id, cfg.function->name, succ->id);
            }
        }
        fprintf(file, "  }\n");
    }
    fprintf(file, "}\n");
}
