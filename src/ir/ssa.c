#include <string.h>
#include "ir/ir.h"
#include "ir/cfg.h"
#include "ir/ssa.h"
#include "ir/fmt.h"
#include "util/hashtable.h"

/// Module for converting a control flow graph into SSA form.
/// Based on the paper "Simple and Efficient Construction of Static Single Assignment Form"
/// by Braun et al. (1999).

typedef struct SsaGenContext {
    // Function of the current CFG
    ir_function_definition_t *function;
    // Map of variable name -> map of ssa block -> variable name
    hash_table_t current_def;
    // Map of variable name -> variable, just a temporary place to keep track ov variables
    hash_table_t variables;
    // Map of basic block -> ssa block
    hash_table_t block_map;
    // Map of ssa phi result name -> original var name
    hash_table_t incomplete_phis;
    // List of blocks in the SSA cfg
    ir_ssa_basic_block_ptr_vector_t blocks;
    // id counter for variable names
    int var_id;
} ssa_gen_context_t;

ir_var_t read_variable_recursive(ssa_gen_context_t *context, ir_var_t var, ir_ssa_basic_block_t *block);
void add_phi_operands(ssa_gen_context_t *context, ir_phi_node_t *phi, ir_var_t var, ir_ssa_basic_block_t *block);
void seal_block(ssa_gen_context_t *context, ir_ssa_basic_block_t *block);

ir_var_t make_variable(ssa_gen_context_t *context, const ir_type_t *type) {
    char name[64];
    snprintf(name, 64, "%%%d", context->var_id++);
    return (ir_var_t) {
        .name = strdup(name),
        .type = type,
    };
}

void write_variable(ssa_gen_context_t *context, ir_var_t variable, ir_ssa_basic_block_t *block, ir_var_t value) {
    // Special handling for global variables, they don't get re-defined for each write
    if (variable.name[0] == '@') return;

    // Add value to the variable map
    if (!hash_table_lookup(&context->variables, value.name, NULL)) {
        ir_var_t *ptr = malloc(sizeof(ir_var_t));
        *ptr = value;
        hash_table_insert(&context->variables, ptr->name, (void*) ptr);
    }

    // current_def[variable][block] = value
    hash_table_t *def_map;
    if (!hash_table_lookup(&context->current_def, variable.name, (void**) &def_map)) {
        def_map = malloc(sizeof(hash_table_t));
        *def_map = hash_table_create_pointer_keys(64);
        hash_table_insert(&context->current_def, variable.name, def_map);
    }
    hash_table_insert(def_map, block, (void*) value.name);
}

ir_var_t read_variable(ssa_gen_context_t *context, ir_var_t variable, ir_ssa_basic_block_t *block) {
    // Special handling for global variables, they are always defined
    // This also handles function names (e.g. in `call printf(...)`)
    if (variable.name[0] != '%') return variable;

    hash_table_t *def_map = NULL;
    const char* name = NULL;

    if (hash_table_lookup(&context->current_def, variable.name, (void**) &def_map) &&
        hash_table_lookup(def_map, block, (void**) &name)
    ) {
        ir_var_t *result;
        assert(hash_table_lookup(&context->variables, name, (void**) &result));
        return *result;
    }

    return read_variable_recursive(context, variable, block);
}

ir_var_t read_variable_recursive(ssa_gen_context_t *context, ir_var_t var, ir_ssa_basic_block_t *block) {
    ir_var_t result;
    if (!block->sealed) {
        result = make_variable(context, var.type);
        ir_phi_node_t phi = (ir_phi_node_t) {
            .var = result,
            .operands = VEC_INIT,
        };
        VEC_APPEND(&block->phi_nodes, phi);
        hash_table_insert(&context->incomplete_phis, result.name, (void*) var.name);
    } else if (block->predecessors.size == 1) {
        result = read_variable(context, var, block->predecessors.buffer[0]);
    } else {
        result = make_variable(context, var.type);
        write_variable(context, var, block, result);
        ir_phi_node_t phi = (ir_phi_node_t) {
            .var = result,
            .operands = VEC_INIT,
        };
        add_phi_operands(context, &phi, var, block);
        VEC_APPEND(&block->phi_nodes, phi);
        // TODO: trivial phi removal
    }
    write_variable(context, var, block, result);
    return result;
}

void add_phi_operands(ssa_gen_context_t *context, ir_phi_node_t *phi, ir_var_t var, ir_ssa_basic_block_t *block) {
    for (int i = 0; i < block->predecessors.size; i += 1) {
        ir_var_t v = read_variable(context, var, block->predecessors.buffer[i]);
        ir_phi_node_operand_t operand = {
            .name = v.name,
            .block = block->predecessors.buffer[i],
        };
        VEC_APPEND(&phi->operands, operand);
    }

    // TODO: trivial phi removal
    // try_remove_trivial_phi(...);
}

// TODO
//void try_remove_trivial_phi(phi_t *phi) {
//
//}

void seal_block(ssa_gen_context_t *context, ir_ssa_basic_block_t *block) {
    block->sealed = true;
    for (int i = 0; i < block->phi_nodes.size; i += 1) {
        ir_phi_node_t *phi = &block->phi_nodes.buffer[i];
        // if the phi node has no arguments it is incomplete
        if (phi->operands.size == 0) {
            const char* var_name = NULL;
            assert(hash_table_lookup(&context->incomplete_phis, phi->var.name, (void**) &var_name));
            ir_var_t var = (ir_var_t) {
                .name = strdup(var_name),
                .type = phi->var.type,
            };
            add_phi_operands(context, phi, var, block);
        }
    }
}

void fill_block(ssa_gen_context_t *context, ir_basic_block_t *block, ir_ssa_basic_block_t *ssa_block) {
    // Special handling for the entry block, we need to write the function arguments.
    // These will get to keep their original names (unless they are redefined later on)
    if (block->is_entry) {
        for (int i = 0; i < context->function->num_params; i += 1) {
            ir_var_t var = context->function->params[i];
            write_variable(context, var, ssa_block, var);
        }
    }

    for (int i = 0; i < block->instructions.size; i += 1) {
        ir_instruction_t instr = *block->instructions.buffer[i];
        ir_var_t *uses[64];
        size_t num_uses = ir_get_uses(&instr, uses, 64);
        for (int j = 0; j < num_uses; j += 1) {
            ir_var_t var = read_variable(context, *uses[j], ssa_block);
            *uses[j] = var;
        }
        ir_var_t *def = ir_get_def(&instr);
        if (def != NULL) {
            ir_var_t var = make_variable(context, def->type);
            write_variable(context, *def, ssa_block, var);
            *def = var;
        }
        VEC_APPEND(&ssa_block->instructions, instr);
    }
}

ir_ssa_basic_block_t *get_or_create_block(ssa_gen_context_t *context, ir_basic_block_t *block) {
    ir_ssa_basic_block_t *ssa_block;
    if (!hash_table_lookup(&context->block_map, block, (void**) &ssa_block)) {
        ssa_block = malloc(sizeof(ir_ssa_basic_block_t));
        *ssa_block = (ir_ssa_basic_block_t) {
            .id = block->id,
            .label = block->label,
            .is_entry = block->is_entry,
            .phi_nodes = VEC_INIT,
            .instructions = VEC_INIT,
            .fall_through = NULL,
            .predecessors = VEC_INIT,
            .successors = VEC_INIT,
            .sealed = false,
        };
        hash_table_insert(&context->block_map, block, ssa_block);
        VEC_APPEND(&context->blocks, ssa_block);

        if (block->label != NULL) {
            hash_table_insert(&context->variables, block->label, (void*) block);
        }
    }
    return ssa_block;
}

ir_ssa_basic_block_t *visit_block(ssa_gen_context_t *context, ir_basic_block_t *block) {
    ir_ssa_basic_block_t *ssa_block = get_or_create_block(context, block);

    // Rules:
    // - A block is sealed if no predecessors will be added
    // - Only filled blocks my have successors
    //
    // Ordering:
    // - If all predecessors have been filled, seal the block
    // - If the block is not filled, fill the block
    // - If the successors have not been visited, visit them

    bool filled = ssa_block->instructions.size > 0;
    bool sealed = ssa_block->sealed;
    if (filled && sealed) return ssa_block;

    bool all_predecessors_filled = true;
    for (int i = 0; i < block->predecessors.size; i += 1) {
        ir_ssa_basic_block_t *predecessor = NULL;
        if (!hash_table_lookup(&context->block_map, block->predecessors.buffer[i], (void**) &predecessor) ||
            predecessor->instructions.size == 0
            ) {
            all_predecessors_filled = false;
            break;
        }
    }
    if (all_predecessors_filled) seal_block(context, ssa_block);

    if (!filled) {
        fill_block(context, block, ssa_block);
        for (int i = 0; i < block->successors.size; i += 1) {
            ir_ssa_basic_block_t *successor = get_or_create_block(context, block->successors.buffer[i]);
            VEC_APPEND(&ssa_block->successors, successor);
            VEC_APPEND(&successor->predecessors, ssa_block);
        }
        for (int i = 0; i < block->successors.size; i += 1) {
            visit_block(context, block->successors.buffer[i]);
        }
        if (block->fall_through != NULL) {
            ir_ssa_basic_block_t *fall_through = NULL;
            assert(hash_table_lookup(&context->block_map, block->fall_through, (void**) &fall_through));
            ssa_block->fall_through = fall_through;
        }
    }

    return ssa_block;
}

// TODO: currently leaks the blocks/instructions from the original CFG
ir_ssa_control_flow_graph_t ir_convert_cfg_to_ssa(ir_control_flow_graph_t *cfg) {
    ssa_gen_context_t context = {
        .function = cfg->function,
        .variables = hash_table_create_string_keys(256),
        .current_def = hash_table_create_string_keys(256),
        .block_map = hash_table_create_pointer_keys(256),
        .incomplete_phis = hash_table_create_string_keys(256),
        .var_id = 0,
    };

    ir_ssa_basic_block_t *entry = visit_block(&context, cfg->entry);

    // TODO: free the original CFG

    // TODO: size hash table correctly
    hash_table_t label_to_block_map = hash_table_create_string_keys(64);
    for (int i = 0; i < context.blocks.size; i += 1) {
        ir_ssa_basic_block_t *block = context.blocks.buffer[i];
        if (block->label != NULL) {
            hash_table_insert(&label_to_block_map, block->label, block);
        }
    }

    hash_table_destroy(&context.variables);
    hash_table_destroy(&context.current_def); // TODO: free the inner hash tables
    hash_table_destroy(&context.block_map);
    hash_table_destroy(&context.incomplete_phis);

    return (ir_ssa_control_flow_graph_t) {
        .function = cfg->function,
        .entry = entry,
        .basic_blocks = context.blocks,
        .label_to_block_map = label_to_block_map,
    };
}

const char* fmt_phi(char *buffer, size_t size, const ir_phi_node_t *phi) {
    char *ptr = buffer;
    ptr += snprintf(ptr, size, "%s = phi ", phi->var.name);
    for (int i = 0; i < phi->operands.size; i += 1) {
        ir_phi_node_operand_t *operand = &phi->operands.buffer[i];
        ptr += snprintf(ptr, size, "[%s, block_%d]", operand->name, operand->block->id);
        if (i + 1 < phi->operands.size) {
            ptr += snprintf(ptr, size, ", ");
        }
    }
    return buffer;
}

void ir_print_ssa_control_flow_graph(FILE *file, const ir_ssa_control_flow_graph_t *function_list, size_t length) {
    fprintf(file, "digraph G {\n");
    for (size_t i = 0; i < length; i += 1) {
        const ir_ssa_control_flow_graph_t cfg = function_list[i];
        fprintf(file, "  subgraph cluster_%s {\n", cfg.function->name);
        fprintf(file, "    label=\"%s\";\n", cfg.function->name);
        for (size_t j = 0; j < cfg.basic_blocks.size; j += 1) {
            ir_ssa_basic_block_t *bb = cfg.basic_blocks.buffer[j];
            fprintf(file, "    block_%d [\n      shape=box\n      label=\n", bb->id);
            for (size_t k = 0; k < bb->phi_nodes.size; k += 1) {
                ir_phi_node_t *phi = &bb->phi_nodes.buffer[k];
                char buffer[1024];
                fmt_phi(buffer, 512, phi);
                fprintf(file, "        \"%s\\l\" +\n", buffer);
            }
            for (size_t k = 0; k < bb->instructions.size; k += 1) {
                char buffer[512];
                ir_fmt_instr(buffer, 512, &bb->instructions.buffer[k]);
                fprintf(file, "        \"%s\\l\"%s\n", buffer, k + 1 < bb->instructions.size ? " +" : "");
            }
            fprintf(file, "    ];\n");

            for (size_t k = 0; k < bb->successors.size; k += 1) {
                ir_ssa_basic_block_t *succ = bb->successors.buffer[k];
                fprintf(file, "    block_%d -> block_%d;\n", bb->id, succ->id);
            }
        }
        fprintf(file, "  }\n");
    }
    fprintf(file, "}\n");
}
