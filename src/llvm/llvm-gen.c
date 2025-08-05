#include <string.h>
#include <ctype.h>

#include "llvm-c/Analysis.h"
#include "llvm-c/Core.h"

#include "ir/ir.h"
#include "llvm-gen.h"
#include "ir/cfg.h"
#include "ir/ssa.h"

typedef struct IncompletePhiNode {
    const ir_phi_node_t *phi;
    const ir_ssa_basic_block_t *block;
    LLVMValueRef llvm_phi;
} incomplete_phi_node_t;

VEC_DEFINE(IncompletePhiNodeVector, incomplete_phi_node_vector_t, incomplete_phi_node_t)

typedef struct LLVMGenContext {
    LLVMModuleRef llvm_module;
    LLVMValueRef llvm_function;
    LLVMBasicBlockRef llvm_block;
    LLVMBuilderRef llvm_builder;

    // Target information
    const target_t *target;

    // Map of ir struct id to llvm type
    hash_table_t llvm_struct_types_map;

    // Mapping of IR function names to LLVM values
    hash_table_t llvm_function_map;
    // Mapping of IR global variables to LLVM values
    hash_table_t global_var_map;
    // Mapping of IR variables to LLVM values
    hash_table_t local_var_map;
    // Mapping of IR basic block IDs to LLVM basic blocks
    hash_table_t block_map;
    // List of incomplete phi nodes
    incomplete_phi_node_vector_t incomplete_phi_nodes;
    ir_ssa_control_flow_graph_t *ir_cfg;
} llvm_gen_context_t;

LLVMTypeRef ir_to_llvm_type(llvm_gen_context_t *context, const ir_type_t *type);
LLVMValueRef ir_to_llvm_value(llvm_gen_context_t *context, const ir_value_t *value);
LLVMValueRef llvm_get_or_add_function(llvm_gen_context_t *context, const char* name, LLVMTypeRef fn_type);

void llvm_gen_visit_function(llvm_gen_context_t *context, const ir_function_definition_t *function);
void llvm_gen_visit_basic_block(llvm_gen_context_t *context, const ir_ssa_basic_block_t *block);
void llvm_gen_visit_instruction(llvm_gen_context_t *context, const ir_instruction_t *instr, const ir_ssa_basic_block_t *ir_block);

LLVMBasicBlockRef llvm_get_or_create_basic_block(llvm_gen_context_t *context, const ir_ssa_basic_block_t *ir_block) {
    char tmp[256];
    snprintf(tmp, sizeof(tmp), "block_%d", ir_block->id);

    LLVMBasicBlockRef llvm_block;
    if (!hash_table_lookup(&context->block_map, tmp, (void**)&llvm_block)) {
        const char* block_name = strdup(tmp);
        llvm_block = LLVMAppendBasicBlock(context->llvm_function, block_name);
        assert(hash_table_insert(&context->block_map, block_name, llvm_block));
    }

    return llvm_block;
}

void llvm_gen_module(const ir_module_t *module, const target_t *target, const char* output_filename) {
    // init
    llvm_gen_context_t context = {
        .llvm_module = LLVMModuleCreateWithName(module->name),
        .llvm_builder = LLVMCreateBuilder(),
        .target = target,
        .llvm_function_map = hash_table_create_string_keys(128),
        .llvm_struct_types_map = hash_table_create_string_keys(128),
        .global_var_map = hash_table_create_string_keys(128),
        .incomplete_phi_nodes = VEC_INIT,
    };

    // global variables
    for (size_t i = 0; i < module->globals.size; i += 1) {
        const ir_global_t *global = module->globals.buffer[i];
        // Get the actual type. The globals IR type is a pointer to the actual type.
        assert(global->type->kind == IR_TYPE_PTR);
        const ir_type_t *ir_type = global->type->value.ptr.pointee;
        const char *name = global->name;
        if (name[0] == '@') {
            if (isdigit(name[1])) {
                // Anonymous global variable (e.g. string literal)
                name = "";
            } else {
                name += 1;
            }
        }

        LLVMValueRef llvm_global;
        if (global->initialized) {
            LLVMValueRef value;
            switch (global->value.kind) {
                case IR_CONST_STRING: {
                    value = LLVMConstString(global->value.value.s, strlen(global->value.value.s), false);
                    break;
                }
                case IR_CONST_INT: {
                    value = LLVMConstInt(ir_to_llvm_type(&context, ir_type), global->value.value.i, true);
                    break;
                }
                case IR_CONST_FLOAT: {
                    value = LLVMConstReal(ir_to_llvm_type(&context, ir_type), global->value.value.f);
                    break;
                }
                case IR_CONST_ARRAY: {
                    LLVMTypeRef element_type = ir_to_llvm_type(&context, global->value.type->value.array.element);
                    int len = global->value.value.array.length;
                    LLVMValueRef *elements = malloc(sizeof(LLVMValueRef) * len);
                    for (int i = 0; i < len; i += 1) {
                        ir_value_t element = { .kind = IR_VALUE_CONST, .constant = global->value.value.array.values[i] };
                        elements[i] = ir_to_llvm_value(&context, &element);
                    }
                    value = LLVMConstArray(element_type, elements, len);
                    break;
                }
                case IR_CONST_STRUCT: {
                    ir_value_t ir_value = {
                        .kind = IR_VALUE_CONST,
                        .constant = global->value,
                    };
                    value = ir_to_llvm_value(&context, &ir_value);
                    break;
                }
                case IR_CONST_GLOBAL_POINTER: {
                    ir_value_t ir_value = {
                        .kind = IR_VALUE_CONST,
                        .constant = global->value,
                    };
                    value = ir_to_llvm_value(&context, &ir_value);
                    break;
                }
                default:
                    assert(false);
                    fprintf(stderr, "%s:%d: Invalid IR global kind", __FILE__, __LINE__);
                    exit(1);
            }
            LLVMTypeRef llvm_type = LLVMTypeOf(value);
            llvm_global = LLVMAddGlobal(context.llvm_module, llvm_type, name);
            LLVMSetInitializer(llvm_global, value);
        } else {
            // Zero initialize
            LLVMTypeRef llvm_type = ir_to_llvm_type(&context, ir_type);
            llvm_global = LLVMAddGlobal(context.llvm_module, llvm_type, name);
            LLVMSetInitializer(llvm_global, LLVMConstNull(llvm_type));
        }
        hash_table_insert(&context.global_var_map, global->name, llvm_global);
    }

    // codegen
    for (size_t i = 0; i < module->functions.size; i += 1) {
        llvm_gen_visit_function(&context, module->functions.buffer[i]);
    }

    // finalize - validate the module and write it to a file
    char *message;
    //LLVMVerifyModule(context.llvm_module, LLVMAbortProcessAction, &message);
    //LLVMDisposeMessage(message);
    LLVMPrintModuleToFile(context.llvm_module, output_filename, &message);
    LLVMDisposeModule(context.llvm_module);

    // Cleanup
    LLVMDisposeBuilder(context.llvm_builder);
    hash_table_destroy(&context.llvm_function_map);
    hash_table_destroy(&context.global_var_map);
}

void llvm_gen_visit_function(llvm_gen_context_t *context, const ir_function_definition_t *function) {
    // Create the function
    context->llvm_function =
        llvm_get_or_add_function(context, function->name, ir_to_llvm_type(context, function->type));
    LLVMSetLinkage(context->llvm_function, LLVMExternalLinkage);

    context->local_var_map = hash_table_create_string_keys(128);
    context->block_map = hash_table_create_string_keys(128);

    // The IR refers to the parameters by name, so we need to set up the mapping
    for (int i = 0; i < function->num_params; i += 1) {
        LLVMValueRef param = LLVMGetParam(context->llvm_function, i);
        hash_table_insert(&context->local_var_map, function->params[i].name, param);
    }

    // Get the control flow graph for the IR function
    ir_control_flow_graph_t cfg = ir_create_control_flow_graph(function);
    ir_ssa_control_flow_graph_t ssa_cfg = ir_convert_cfg_to_ssa(&cfg);
    //ir_print_ssa_control_flow_graph(stdout, &ssa_cfg, 1);
    context->ir_cfg = &ssa_cfg;

    // Visit each basic block in the CFG
    for (int i = 0; i < context->ir_cfg->basic_blocks.size; i += 1) {
        ir_ssa_basic_block_t *block = context->ir_cfg->basic_blocks.buffer[i];
        llvm_gen_visit_basic_block(context, block);
    }

    // Add phi node arguments now that we've filled all the blocks
    for (int i = 0; i < context->incomplete_phi_nodes.size; i += 1) {
        incomplete_phi_node_t *incomplete_phi = &context->incomplete_phi_nodes.buffer[i];
        const ir_phi_node_t *phi = context->incomplete_phi_nodes.buffer[i].phi;

        LLVMValueRef *incoming_values = malloc(phi->operands.size * sizeof(LLVMValueRef));
        LLVMBasicBlockRef *incoming_blocks = malloc(phi->operands.size * sizeof(LLVMBasicBlockRef));
        for (int j = 0; j < phi->operands.size; j += 1) {
            ir_phi_node_operand_t *operand = &phi->operands.buffer[j];
            LLVMValueRef incoming_value;
            assert(hash_table_lookup(&context->local_var_map, operand->name, (void**) &incoming_value));
            const ir_ssa_basic_block_t *incoming_block = operand->block;
            incoming_values[j] = incoming_value;
            incoming_blocks[j] = llvm_get_or_create_basic_block(context, incoming_block);
        }

        LLVMAddIncoming(incomplete_phi->llvm_phi, incoming_values, incoming_blocks, phi->operands.size);
    }

    // The entry block must not have predecessors
    ir_ssa_basic_block_t *entry_block = context->ir_cfg->basic_blocks.buffer[0];
    if (entry_block != NULL && entry_block->predecessors.size > 0){
        // The entry basic block has at least 1 predecessor. It's probably the start of a loop, or a label that is the
        // destination of a goto statement.
        // We will add a new basic block as the entry of the function that just contains a jump instruction to go to
        // the previous entry block.
        LLVMBasicBlockRef llvm_entry_block = llvm_get_or_create_basic_block(context, entry_block);
        LLVMBasicBlockRef llvm_basic_block = LLVMInsertBasicBlock(llvm_entry_block, "");
        LLVMPositionBuilderAtEnd(context->llvm_builder, llvm_basic_block);
        LLVMBuildBr(context->llvm_builder, llvm_entry_block);
    }

    // Cleanup
    context->ir_cfg = NULL;
    context->incomplete_phi_nodes.size = 0;
    hash_table_destroy(&context->local_var_map);
    hash_table_destroy(&context->block_map);
}

void llvm_gen_visit_basic_block(llvm_gen_context_t *context, const ir_ssa_basic_block_t *block) {
    char tmp[256];
    snprintf(tmp, sizeof(tmp), "block_%d", block->id);

    context->llvm_block = llvm_get_or_create_basic_block(context, block);
    LLVMPositionBuilderAtEnd(context->llvm_builder, context->llvm_block);

    // Add phi nodes
    for (int i = 0; i < block->phi_nodes.size; i += 1) {
        ir_phi_node_t *phi = &block->phi_nodes.buffer[i];
        LLVMValueRef llvm_phi = LLVMBuildPhi(context->llvm_builder, ir_to_llvm_type(context, phi->var.type), "");
        hash_table_insert(&context->local_var_map, phi->var.name, llvm_phi);
        // We need to add the phi node arguments later, since we may not have filled the incoming blocks yet.
        VEC_APPEND(&context->incomplete_phi_nodes, ((incomplete_phi_node_t) { .phi = phi, .block = block, .llvm_phi = llvm_phi }));
    }

    for (size_t i = 0; i < block->instructions.size; i += 1) {
        const ir_instruction_t *instr = &block->instructions.buffer[i];
        llvm_gen_visit_instruction(context, instr, block);
    }
}

void llvm_gen_visit_instruction(
    llvm_gen_context_t *context,
    const ir_instruction_t *instr,
    const ir_ssa_basic_block_t *ir_block
) {
    // This currently only works if the input IR is already in SSA form. The IR generated
    // by the first pass of the AST is in SSA form, since all variables that live across basic
    // block boundaries are just stored on the stack.

    bool is_last_instr_in_block = &ir_block->instructions.buffer[ir_block->instructions.size - 1] == instr;
    bool is_terminator = instr->opcode == IR_RET || instr->opcode == IR_BR || instr->opcode == IR_BR_COND ||
                         instr->opcode == IR_SWITCH;

    switch (instr->opcode) {
        case IR_NOP:
            break;
        case IR_ADD: {
            LLVMValueRef lhs = ir_to_llvm_value(context, &instr->value.binary_op.left);
            LLVMValueRef rhs = ir_to_llvm_value(context, &instr->value.binary_op.right);
            LLVMValueRef result;
            if (ir_is_float_type(instr->value.binary_op.result.type)) {
                result = LLVMBuildFAdd(context->llvm_builder, lhs, rhs, "");
            } else {
                result = LLVMBuildAdd(context->llvm_builder, lhs, rhs, "");
            }
            hash_table_insert(&context->local_var_map, instr->value.binary_op.result.name, result);
            break;
        }
        case IR_SUB: {
            LLVMValueRef lhs = ir_to_llvm_value(context, &instr->value.binary_op.left);
            LLVMValueRef rhs = ir_to_llvm_value(context, &instr->value.binary_op.right);
            LLVMValueRef result;
            if (ir_is_float_type(instr->value.binary_op.result.type)) {
                result = LLVMBuildFSub(context->llvm_builder, lhs, rhs, "");
            } else {
                result = LLVMBuildSub(context->llvm_builder, lhs, rhs, "");
            }
            hash_table_insert(&context->local_var_map, instr->value.binary_op.result.name, result);
            break;
        }
        case IR_MUL: {
            LLVMValueRef lhs = ir_to_llvm_value(context, &instr->value.binary_op.left);
            LLVMValueRef rhs = ir_to_llvm_value(context, &instr->value.binary_op.right);
            LLVMValueRef result;
            if (ir_is_float_type(instr->value.binary_op.result.type)) {
                result = LLVMBuildFMul(context->llvm_builder, lhs, rhs, "");
            } else {
                result = LLVMBuildMul(context->llvm_builder, lhs, rhs, "");
            }
            hash_table_insert(&context->local_var_map, instr->value.binary_op.result.name, result);
            break;
        }
        case IR_DIV: {
            LLVMValueRef lhs = ir_to_llvm_value(context, &instr->value.binary_op.left);
            LLVMValueRef rhs = ir_to_llvm_value(context, &instr->value.binary_op.right);
            LLVMValueRef result;
            if (ir_is_float_type(instr->value.binary_op.result.type)) {
                result = LLVMBuildFDiv(context->llvm_builder, lhs, rhs, "");
            } else if (ir_is_signed_integer_type(instr->value.binary_op.result.type)) {
                result = LLVMBuildSDiv(context->llvm_builder, lhs, rhs, "");
            } else {
                result = LLVMBuildUDiv(context->llvm_builder, lhs, rhs, "");
            }
            hash_table_insert(&context->local_var_map, instr->value.binary_op.result.name, result);
            break;
        }
        case IR_MOD: {
            LLVMValueRef lhs = ir_to_llvm_value(context, &instr->value.binary_op.left);
            LLVMValueRef rhs = ir_to_llvm_value(context, &instr->value.binary_op.right);
            LLVMValueRef result;
            if (ir_is_float_type(instr->value.binary_op.result.type)) {
                result = LLVMBuildFRem(context->llvm_builder, lhs, rhs, "");
            } else {
                result = LLVMBuildSRem(context->llvm_builder, lhs, rhs, "");
            }
            hash_table_insert(&context->local_var_map, instr->value.binary_op.result.name, result);
            break;
        }
        case IR_ASSIGN: {
            LLVMValueRef value = ir_to_llvm_value(context, &instr->value.assign.value);
            hash_table_insert(&context->local_var_map, instr->value.assign.result.name, value);
            break;
        }
        case IR_AND: {
            LLVMValueRef lhs = ir_to_llvm_value(context, &instr->value.binary_op.left);
            LLVMValueRef rhs = ir_to_llvm_value(context, &instr->value.binary_op.right);
            LLVMValueRef result = LLVMBuildAnd(context->llvm_builder, lhs, rhs, "");
            hash_table_insert(&context->local_var_map, instr->value.binary_op.result.name, result);
            break;
        }
        case IR_OR: {
            LLVMValueRef lhs = ir_to_llvm_value(context, &instr->value.binary_op.left);
            LLVMValueRef rhs = ir_to_llvm_value(context, &instr->value.binary_op.right);
            LLVMValueRef result = LLVMBuildOr(context->llvm_builder, lhs, rhs, "");
            hash_table_insert(&context->local_var_map, instr->value.binary_op.result.name, result);
            break;
        }
        case IR_SHL: {
            LLVMValueRef lhs = ir_to_llvm_value(context, &instr->value.binary_op.left);
            LLVMValueRef rhs = ir_to_llvm_value(context, &instr->value.binary_op.right);
            LLVMValueRef result = LLVMBuildShl(context->llvm_builder, lhs, rhs, "");
            hash_table_insert(&context->local_var_map, instr->value.binary_op.result.name, result);
            break;
        }
        case IR_SHR: {
            LLVMValueRef lhs = ir_to_llvm_value(context, &instr->value.binary_op.left);
            LLVMValueRef rhs = ir_to_llvm_value(context, &instr->value.binary_op.right);
            LLVMValueRef result;
            if (ir_is_signed_integer_type(ir_get_type_of_value(instr->value.binary_op.left))) {
                result = LLVMBuildAShr(context->llvm_builder, lhs, rhs, "");
            } else {
                result = LLVMBuildLShr(context->llvm_builder, lhs, rhs, "");
            }
            hash_table_insert(&context->local_var_map, instr->value.binary_op.result.name, result);
            break;
        }
        case IR_XOR: {
            LLVMValueRef lhs = ir_to_llvm_value(context, &instr->value.binary_op.left);
            LLVMValueRef rhs = ir_to_llvm_value(context, &instr->value.binary_op.right);
            LLVMValueRef result = LLVMBuildXor(context->llvm_builder, lhs, rhs, "");
            hash_table_insert(&context->local_var_map, instr->value.binary_op.result.name, result);
            break;
        }
        case IR_NOT: {
            LLVMValueRef operand = ir_to_llvm_value(context, &instr->value.unary_op.operand);
            LLVMValueRef result = LLVMBuildNot(context->llvm_builder, operand, "");
            hash_table_insert(&context->local_var_map, instr->value.unary_op.result.name, result);
            break;
        }
        case IR_EQ: {
            LLVMValueRef lhs = ir_to_llvm_value(context, &instr->value.binary_op.left);
            LLVMValueRef rhs = ir_to_llvm_value(context, &instr->value.binary_op.right);
            LLVMValueRef result;
            if (ir_is_float_type(ir_get_type_of_value(instr->value.binary_op.left))) {
                result = LLVMBuildFCmp(context->llvm_builder, LLVMRealOEQ, lhs, rhs, "");
            } else {
                result = LLVMBuildICmp(context->llvm_builder, LLVMIntEQ, lhs, rhs, "");
            }
            hash_table_insert(&context->local_var_map, instr->value.binary_op.result.name, result);
            break;
        }
        case IR_NE: {
            LLVMValueRef lhs = ir_to_llvm_value(context, &instr->value.binary_op.left);
            LLVMValueRef rhs = ir_to_llvm_value(context, &instr->value.binary_op.right);
            LLVMValueRef result;
            if (ir_is_float_type(ir_get_type_of_value(instr->value.binary_op.left))) {
                result = LLVMBuildFCmp(context->llvm_builder, LLVMRealONE, lhs, rhs, "");
            } else {
                result = LLVMBuildICmp(context->llvm_builder, LLVMIntNE, lhs, rhs, "");
            }
            hash_table_insert(&context->local_var_map, instr->value.binary_op.result.name, result);
            break;
        }
        case IR_LT: {
            LLVMValueRef lhs = ir_to_llvm_value(context, &instr->value.binary_op.left);
            LLVMValueRef rhs = ir_to_llvm_value(context, &instr->value.binary_op.right);
            LLVMValueRef result;
            if (ir_is_float_type(ir_get_type_of_value(instr->value.binary_op.left))) {
                result = LLVMBuildFCmp(context->llvm_builder, LLVMRealOLT, lhs, rhs, "");
            } else if (ir_is_signed_integer_type(ir_get_type_of_value(instr->value.binary_op.left))) {
                result = LLVMBuildICmp(context->llvm_builder, LLVMIntSLT, lhs, rhs, "");
            } else {
                result = LLVMBuildICmp(context->llvm_builder, LLVMIntULT, lhs, rhs, "");
            }
            hash_table_insert(&context->local_var_map, instr->value.binary_op.result.name, result);
            break;
        }
        case IR_LE: {
            LLVMValueRef lhs = ir_to_llvm_value(context, &instr->value.binary_op.left);
            LLVMValueRef rhs = ir_to_llvm_value(context, &instr->value.binary_op.right);
            LLVMValueRef result;
            if (ir_is_float_type(ir_get_type_of_value(instr->value.binary_op.left))) {
                result = LLVMBuildFCmp(context->llvm_builder, LLVMRealOLE, lhs, rhs, "");
            } else if (ir_is_signed_integer_type(ir_get_type_of_value(instr->value.binary_op.left))) {
                result = LLVMBuildICmp(context->llvm_builder, LLVMIntSLE, lhs, rhs, "");
            } else {
                result = LLVMBuildICmp(context->llvm_builder, LLVMIntULE, lhs, rhs, "");
            }
            hash_table_insert(&context->local_var_map, instr->value.binary_op.result.name, result);
            break;
        }
        case IR_GT: {
            LLVMValueRef lhs = ir_to_llvm_value(context, &instr->value.binary_op.left);
            LLVMValueRef rhs = ir_to_llvm_value(context, &instr->value.binary_op.right);
            LLVMValueRef result;
            if (ir_is_float_type(ir_get_type_of_value(instr->value.binary_op.left))) {
                result = LLVMBuildFCmp(context->llvm_builder, LLVMRealOGT, lhs, rhs, "");
            } else if (ir_is_signed_integer_type(ir_get_type_of_value(instr->value.binary_op.left))) {
                result = LLVMBuildICmp(context->llvm_builder, LLVMIntSGT, lhs, rhs, "");
            } else {
                result = LLVMBuildICmp(context->llvm_builder, LLVMIntUGT, lhs, rhs, "");
            }
            hash_table_insert(&context->local_var_map, instr->value.binary_op.result.name, result);
            break;
        }
        case IR_GE: {
            LLVMValueRef lhs = ir_to_llvm_value(context, &instr->value.binary_op.left);
            LLVMValueRef rhs = ir_to_llvm_value(context, &instr->value.binary_op.right);
            LLVMValueRef result;
            if (ir_is_float_type(ir_get_type_of_value(instr->value.binary_op.left))) {
                result = LLVMBuildFCmp(context->llvm_builder, LLVMRealOGE, lhs, rhs, "");
            } else if (ir_is_signed_integer_type(ir_get_type_of_value(instr->value.binary_op.left))) {
                result = LLVMBuildICmp(context->llvm_builder, LLVMIntSGE, lhs, rhs, "");
            } else {
                result = LLVMBuildICmp(context->llvm_builder, LLVMIntUGE, lhs, rhs, "");
            }
            hash_table_insert(&context->local_var_map, instr->value.binary_op.result.name, result);
            break;
        }
        case IR_BR: {
            const char* label = instr->value.branch.label;
            const ir_ssa_basic_block_t *target_block;
            assert(hash_table_lookup(&context->ir_cfg->label_to_block_map, label, (void**)&target_block));
            LLVMBasicBlockRef llvm_block = llvm_get_or_create_basic_block(context, target_block);
            LLVMBuildBr(context->llvm_builder, llvm_block);
            break;
        }
        case IR_BR_COND: {
            const char* label = instr->value.branch.label;
            const ir_ssa_basic_block_t *ir_true_block;
            assert(hash_table_lookup(&context->ir_cfg->label_to_block_map, label, (void**)&ir_true_block));
            const ir_ssa_basic_block_t *ir_false_block = ir_block->fall_through;
            assert(ir_false_block != NULL);

            LLVMValueRef cond = ir_to_llvm_value(context, &instr->value.branch.cond);
            LLVMBasicBlockRef true_block = llvm_get_or_create_basic_block(context, ir_true_block);
            LLVMBasicBlockRef false_block = llvm_get_or_create_basic_block(context, ir_false_block);
            LLVMBuildCondBr(context->llvm_builder, cond, true_block, false_block);
            break;
        }
        case IR_CALL: {
            LLVMTypeRef fn_type = ir_to_llvm_type(context, ir_get_type_of_value(instr->value.call.function));
            // TODO: call things that aren't named functions
            const char *fn_name;
            if (instr->value.call.function.kind == IR_VALUE_VAR) {
                fn_name = instr->value.call.function.var.name;
            } else {
                assert(instr->value.call.function.kind == IR_VALUE_CONST);
                assert(instr->value.call.function.constant.kind == IR_CONST_GLOBAL_POINTER);
                fn_name = instr->value.call.function.constant.value.global_name;
            }
            LLVMValueRef fn = llvm_get_or_add_function(context, fn_name, fn_type);
            LLVMValueRef *args = malloc(instr->value.call.num_args * sizeof(LLVMValueRef));
            for (int i = 0; i < instr->value.call.num_args; i += 1) {
                args[i] = ir_to_llvm_value(context, &instr->value.call.args[i]);
            }
            LLVMValueRef result = LLVMBuildCall2(context->llvm_builder, fn_type, fn, args, instr->value.call.num_args, "");
            if (instr->value.call.result != NULL) {
                hash_table_insert(&context->local_var_map, instr->value.call.result->name, result);
            }
            break;
        }
        case IR_RET: {
            if (!instr->value.ret.has_value) {
                LLVMBuildRetVoid(context->llvm_builder);
            } else {
                LLVMValueRef value = ir_to_llvm_value(context, &instr->value.ret.value);
                LLVMBuildRet(context->llvm_builder, value);
            }
            break;
        }
        case IR_ALLOCA: {
            LLVMValueRef result = LLVMBuildAlloca(context->llvm_builder, ir_to_llvm_type(context, instr->value.alloca.type), "");
            hash_table_insert(&context->local_var_map, instr->value.alloca.result.name, result);
            break;
        }
        case IR_LOAD: {
            ir_value_t ptr = instr->value.unary_op.operand;
            const ir_type_t *ptr_type = ir_get_type_of_value(ptr);
            LLVMValueRef result = LLVMBuildLoad2(
                context->llvm_builder,
                ir_to_llvm_type(context, ptr_type->value.ptr.pointee),
                ir_to_llvm_value(context, &ptr),
                ""
            );
            hash_table_insert(&context->local_var_map, instr->value.unary_op.result.name, result);
            break;
        }
        case IR_STORE:
            LLVMBuildStore(context->llvm_builder,
                           ir_to_llvm_value(context, &instr->value.store.value),
                           ir_to_llvm_value(context, &instr->value.store.ptr));
            break;
        case IR_MEMCPY: {
            // TODO: alignment for more efficient memcpy?
            const int align = 1;
            LLVMBuildMemCpy(context->llvm_builder,
                            ir_to_llvm_value(context, &instr->value.memcpy.dest),
                            align,
                            ir_to_llvm_value(context, &instr->value.memcpy.src),
                            align,
                            ir_to_llvm_value(context, &instr->value.memcpy.length));
            break;
        }
        case IR_GET_ARRAY_ELEMENT_PTR: {
            ir_value_t ptr = instr->value.binary_op.left;
            const ir_type_t *ptr_type = ir_get_type_of_value(ptr);
            assert(ptr_type->kind == IR_TYPE_PTR);
            const ir_type_t *var_type = ptr_type->value.ptr.pointee;
            LLVMValueRef llvm_ptr = ir_to_llvm_value(context, &instr->value.binary_op.left);
            LLVMValueRef index = ir_to_llvm_value(context, &instr->value.binary_op.right);
            LLVMValueRef result;
            if (var_type->kind == IR_TYPE_ARRAY) {
                LLVMValueRef indices[2] = {
                    LLVMConstInt(LLVMInt64Type(), 0, false), // dereference the array address
                    index // second index is the index of the element we want
                };
                result = LLVMBuildGEP2(context->llvm_builder, ir_to_llvm_type(context, var_type), llvm_ptr, indices, 2, "");
            } else {
                LLVMValueRef indices[1] = { index };
                result = LLVMBuildGEP2(context->llvm_builder, ir_to_llvm_type(context, var_type), llvm_ptr, indices, 1, "");
            }
            hash_table_insert(&context->local_var_map, instr->value.binary_op.result.name, result);
            break;
        }
        case IR_GET_STRUCT_MEMBER_PTR: {
            ir_value_t ptr = instr->value.binary_op.left;
            const ir_type_t *ptr_type = ir_get_type_of_value(ptr);
            assert(ptr_type->kind == IR_TYPE_PTR && ptr_type->value.ptr.pointee->kind == IR_TYPE_STRUCT_OR_UNION);
            const ir_type_t *struct_type = ptr_type->value.ptr.pointee;
            LLVMValueRef llvm_ptr = ir_to_llvm_value(context, &instr->value.binary_op.left);
            ir_value_t index_value = instr->value.binary_op.right;
            assert(index_value.kind == IR_VALUE_CONST && index_value.constant.kind == IR_CONST_INT);
            int index = index_value.constant.value.i;

            LLVMValueRef result = NULL;
            if (struct_type->value.struct_or_union.is_union) {
                // This is a union, so the field we want to access always has an offset of 0
                // We just need to cast the pointer to the type of the selected field
                // Note: This seems to only be required for older versions of LLVM, as in newer versions pointers are
                //       untyped
                const ir_struct_field_t *field = struct_type->value.struct_or_union.fields.buffer[index];
                LLVMTypeRef llvm_field_type = ir_to_llvm_type(context, field->type);
                result = LLVMBuildPointerCast(context->llvm_builder, llvm_ptr, LLVMPointerType(llvm_field_type, 0), "");
            } else {
                result = LLVMBuildStructGEP2(context->llvm_builder, ir_to_llvm_type(context, struct_type), llvm_ptr, index, "");
            }
            hash_table_insert(&context->local_var_map, instr->value.binary_op.result.name, result);
            break;
        }
        case IR_TRUNC: {
            LLVMValueRef result;
            if (ir_is_float_type(ir_get_type_of_value(instr->value.unary_op.operand))) {
                result = LLVMBuildFPTrunc(context->llvm_builder,
                    ir_to_llvm_value(context, &instr->value.unary_op.operand),
                    ir_to_llvm_type(context, instr->value.unary_op.result.type),
                    "");
            } else {
                result = LLVMBuildTrunc(context->llvm_builder,
                    ir_to_llvm_value(context, &instr->value.unary_op.operand),
                    ir_to_llvm_type(context, instr->value.unary_op.result.type),
                    "");
            }
            hash_table_insert(&context->local_var_map, instr->value.unary_op.result.name, result);
            break;
        }
        case IR_EXT: {
            LLVMValueRef result;
            if (ir_is_float_type(ir_get_type_of_value(instr->value.unary_op.operand))) {
                result = LLVMBuildFPExt(context->llvm_builder,
                    ir_to_llvm_value(context, &instr->value.unary_op.operand),
                    ir_to_llvm_type(context, instr->value.unary_op.result.type),
                    "");
            } else {
                if (ir_is_signed_integer_type(ir_get_type_of_value(instr->value.unary_op.operand))) {
                    result = LLVMBuildSExt(context->llvm_builder,
                        ir_to_llvm_value(context, &instr->value.unary_op.operand),
                        ir_to_llvm_type(context, instr->value.unary_op.result.type),
                        "");
                } else {
                    result = LLVMBuildZExt(context->llvm_builder,
                        ir_to_llvm_value(context, &instr->value.unary_op.operand),
                        ir_to_llvm_type(context, instr->value.unary_op.result.type),
                        "");
                }
            }
            hash_table_insert(&context->local_var_map, instr->value.unary_op.result.name, result);
            break;
        }
        case IR_FTOI: {
            LLVMValueRef operand = ir_to_llvm_value(context, &instr->value.unary_op.operand);
            LLVMValueRef result;
            if (ir_is_signed_integer_type(instr->value.unary_op.result.type)) {
                result = LLVMBuildFPToSI(context->llvm_builder, operand, ir_to_llvm_type(context, instr->value.unary_op.result.type), "");
            } else {
                result = LLVMBuildFPToUI(context->llvm_builder, operand, ir_to_llvm_type(context, instr->value.unary_op.result.type), "");
            }
            hash_table_insert(&context->local_var_map, instr->value.unary_op.result.name, result);
            break;
        }
        case IR_ITOF: {
            LLVMValueRef operand = ir_to_llvm_value(context, &instr->value.unary_op.operand);
            LLVMValueRef result;
            if (ir_is_signed_integer_type(ir_get_type_of_value(instr->value.unary_op.operand))) {
                result = LLVMBuildSIToFP(context->llvm_builder, operand, ir_to_llvm_type(context, instr->value.unary_op.result.type), "");
            } else {
                result = LLVMBuildUIToFP(context->llvm_builder, operand, ir_to_llvm_type(context, instr->value.unary_op.result.type), "");
            }
            hash_table_insert(&context->local_var_map, instr->value.unary_op.result.name, result);
            break;
        }
        case IR_PTOI: {
            LLVMValueRef result = LLVMBuildPtrToInt(context->llvm_builder,
                ir_to_llvm_value(context, &instr->value.unary_op.operand),
                ir_to_llvm_type(context, instr->value.unary_op.result.type),
                "");
            hash_table_insert(&context->local_var_map, instr->value.unary_op.result.name, result);
            break;
        }
        case IR_ITOP: {
            LLVMValueRef result = LLVMBuildIntToPtr(context->llvm_builder,
                ir_to_llvm_value(context, &instr->value.unary_op.operand),
                ir_to_llvm_type(context, instr->value.unary_op.result.type),
                "");
            hash_table_insert(&context->local_var_map, instr->value.unary_op.result.name, result);
            break;
        }
        case IR_BITCAST: {
            LLVMValueRef result = LLVMBuildBitCast(context->llvm_builder,
                ir_to_llvm_value(context, &instr->value.unary_op.operand),
                ir_to_llvm_type(context, instr->value.unary_op.result.type),
                "");
            hash_table_insert(&context->local_var_map, instr->value.unary_op.result.name, result);
            break;
        }
        case IR_SWITCH: {
            const char *default_label = instr->value.switch_.default_label;
            ir_ssa_basic_block_t *default_block = NULL;
            for (int i = 0; i < ir_block->successors.size; i += 1) {
                if (strcmp(ir_block->successors.buffer[i]->label, default_label) == 0) {
                    default_block = ir_block->successors.buffer[i];
                    break;
                }
            }
            assert(default_block != NULL && "Expected to find a block for the default/fall-through switch case");

            LLVMValueRef llvm_switch = LLVMBuildSwitch(
                context->llvm_builder,
                ir_to_llvm_value(context, &instr->value.switch_.value),
                llvm_get_or_create_basic_block(context, default_block),
                instr->value.switch_.cases.size
            );

            // hashtable of label -> basic block
            hash_table_t successors = hash_table_create_string_keys(128);
            for (int i = 0; i < ir_block->successors.size; i += 1) {
                ir_ssa_basic_block_t *succ = ir_block->successors.buffer[i];
                hash_table_insert(&successors, succ->label, succ);
            }

            for (int i = 0; i < instr->value.switch_.cases.size; i += 1) {
                ir_switch_case_t switch_case = instr->value.switch_.cases.buffer[i];
                ir_ssa_basic_block_t *succ = NULL;
                hash_table_lookup(&successors, switch_case.label, (void**) &succ);
                assert(succ != NULL);
                assert(switch_case.const_val.kind == IR_CONST_INT);

                LLVMValueRef llvm_case_value =
                    LLVMConstInt(ir_to_llvm_type(context, switch_case.const_val.type), switch_case.const_val.value.i, false);
                LLVMAddCase(llvm_switch, llvm_case_value, llvm_get_or_create_basic_block(context, succ));
            }

            hash_table_destroy(&successors); // clean up
            break;
        }
        default:
            assert(false && "Unrecognized opcode");
    }

    if (is_last_instr_in_block && !is_terminator) {
        // If the last instruction in the block isn't a terminator, there must be an explicit branch
        // to the next block.
        assert(ir_block->fall_through != NULL && "Expected a fall-through block");
        LLVMBuildBr(context->llvm_builder, llvm_get_or_create_basic_block(context, ir_block->fall_through));
    }
}

LLVMTypeRef ir_to_llvm_type(llvm_gen_context_t *context, const ir_type_t *type) {
    switch (type->kind) {
        case IR_TYPE_VOID:
            return LLVMVoidType();
        case IR_TYPE_BOOL:
            return LLVMInt1Type();
        case IR_TYPE_I8:
        case IR_TYPE_U8:
            return LLVMInt8Type();
        case IR_TYPE_I16:
        case IR_TYPE_U16:
            return LLVMInt16Type();
        case IR_TYPE_I32:
        case IR_TYPE_U32:
            return LLVMInt32Type();
        case IR_TYPE_I64:
        case IR_TYPE_U64:
            return LLVMInt64Type();
        case IR_TYPE_F32:
            return LLVMFloatType();
        case IR_TYPE_F64:
            return LLVMDoubleType();
        case IR_TYPE_PTR: {
            // This causes a stack overflow when handling structs which are self-referential
            // Instead just return an opaque pointer.
            // return LLVMPointerType(ir_to_llvm_type(context, type->value.ptr.pointee), 0);
            return LLVMPointerType(LLVMVoidType(), 0);

        }
        case IR_TYPE_ARRAY:
            return LLVMArrayType(ir_to_llvm_type(context, type->value.array.element), type->value.array.length);
        case IR_TYPE_STRUCT_OR_UNION: {
            // If we've already seen this type then it should be in the struct type map
            LLVMTypeRef llvm_type = NULL;
            if (hash_table_lookup(&context->llvm_struct_types_map, type->value.struct_or_union.id, (void**) &llvm_type)) {
                return llvm_type;
            }

            // We need to create the LLVM type
            if (type->value.struct_or_union.is_union) {
                // If the type is a union, we will just represent it as an array of bytes, where the size is equal
                // to the size of the largest field
                int size = ir_size_of_type_bytes(context->target->arch->ir_arch, type);
                llvm_type = LLVMArrayType(LLVMInt8Type(), size);
            } else {
                // Build the LLVM struct type
                int element_count = type->value.struct_or_union.fields.size;
                LLVMTypeRef *element_types = malloc(element_count * sizeof(element_types));
                for (int i = 0; i < element_count; i += 1) {
                    element_types[i] = ir_to_llvm_type(context, type->value.struct_or_union.fields.buffer[i]->type);
                }
                // Note: packed = true here because the IR struct definition has already had padding applied
                llvm_type = LLVMStructType(element_types, element_count, true);
            }

            // Add the new type to the map
            hash_table_insert(&context->llvm_struct_types_map, type->value.struct_or_union.id, llvm_type);

            return llvm_type;
        }
        case IR_TYPE_FUNCTION: {
            LLVMTypeRef *param_types = malloc(type->value.function.num_params * sizeof(LLVMTypeRef));
            for (int i = 0; i < type->value.function.num_params; i += 1) {
                param_types[i] = ir_to_llvm_type(context, type->value.function.params[i]);
            }
            return LLVMFunctionType(ir_to_llvm_type(context, type->value.function.return_type), param_types, type->value.function.num_params, type->value.function.is_variadic);
        }
    }
}

LLVMValueRef ir_to_llvm_value(llvm_gen_context_t *context, const ir_value_t *value) {
    switch (value->kind) {
        case IR_VALUE_CONST: {
            const ir_type_t *ir_type = value->constant.type;
            switch (value->constant.kind) {
                case IR_CONST_INT: {
                    LLVMTypeRef llvm_type = ir_type->kind == IR_TYPE_PTR
                        ? ir_to_llvm_type(context, context->target->arch->ir_arch->ptr_int_type)
                        : ir_to_llvm_type(context, ir_type);
                    return LLVMConstInt(llvm_type, value->constant.value.i, false);
                }
                case IR_CONST_FLOAT:
                    return LLVMConstReal(ir_to_llvm_type(context, ir_type), value->constant.value.f);
                case IR_CONST_STRING:
                    // This is _probably_ unreachable, since this should be handled when visiting the ir globals
                    fprintf(stderr, "%s:%d: LLVM codegen for IR constant strings not implemented\n", __FILE__, __LINE__);
                    exit(1);
                case IR_CONST_ARRAY: {
                    LLVMTypeRef element_type = ir_to_llvm_type(context, ir_type->value.array.element);
                    const size_t len = ir_type->value.array.length;
                    LLVMValueRef *elements = malloc(sizeof(LLVMValueRef) * len);
                    for (int i = 0; i < len; i += 1) {
                        ir_value_t element = { .kind = IR_VALUE_CONST, .constant = value->constant.value.array.values[i] };
                        elements[i] = ir_to_llvm_value(context, &element);
                    }
                    return LLVMConstArray(element_type, elements, len);
                }
                case IR_CONST_STRUCT: {
                    if (value->constant.value._struct.is_union) {
                        LLVMValueRef constant_values[2];

                        // field value
                        const size_t field_index = value->constant.value._struct.union_field_index;
                        ir_value_t field_value = {
                            .kind = IR_VALUE_CONST,
                            .constant = value->constant.value._struct.fields[field_index],
                        };
                        constant_values[0] = ir_to_llvm_value(context, &field_value);

                        // padding
                        ir_struct_field_t *field = value->constant.type->value.struct_or_union.fields.buffer[field_index];
                        size_t union_size = ir_size_of_type_bytes(context->target->arch->ir_arch, value->constant.type);
                        size_t field_size = ir_size_of_type_bytes(context->target->arch->ir_arch, field->type);
                        assert(union_size >= field_size);
                        size_t padding_bytes = union_size - field_size;

                        // TODO: is there a way to not have to zero initialize the individual elements
                        LLVMValueRef zero = LLVMConstInt(LLVMInt8Type(), 0, false);
                        LLVMValueRef *padding = malloc(padding_bytes * sizeof(LLVMValueRef));
                        for (int i = 0; i < padding_bytes; i += 1) {
                            padding[i] = zero;
                        }
                        constant_values[1] = LLVMConstArray(LLVMInt8Type(), padding, padding_bytes);
                        return LLVMConstStruct(constant_values, 2, true);
                    }

                    LLVMValueRef *constant_values = malloc(sizeof(LLVMValueRef) * value->constant.value._struct.length);
                    for (int i = 0; i < value->constant.value._struct.length; i += 1) {
                        ir_value_t field_value = {
                            .kind = IR_VALUE_CONST,
                            .constant = value->constant.value._struct.fields[i]
                        };
                        constant_values[i] = ir_to_llvm_value(context, &field_value);
                    }
                    // Note: packed = true, since the ir generator creates new padding fields
                    return LLVMConstStruct(constant_values, value->constant.value._struct.length, true);
                }
                case IR_CONST_GLOBAL_POINTER: {
                    const char *ir_name = value->constant.value.global_name;
                    assert(ir_name != NULL);
                    LLVMValueRef llvm_value = NULL;
                    assert(hash_table_lookup(&context->global_var_map, ir_name, (void**) &llvm_value));
                    return llvm_value;
                }
            }
        }
        case IR_VALUE_VAR: {
            const char* ir_name = value->var.name;
            LLVMValueRef llvm_value = NULL;
            if (ir_name[0] == '@') {
                assert(hash_table_lookup(&context->global_var_map, ir_name, (void**)&llvm_value));
            } else {
                assert(hash_table_lookup(&context->local_var_map, ir_name, (void**)&llvm_value));
            }
            return llvm_value;
        }
    }
}

LLVMValueRef llvm_get_or_add_function(llvm_gen_context_t *context, const char* name, LLVMTypeRef fn_type) {
    LLVMValueRef fn;

    if (hash_table_lookup(&context->llvm_function_map, name, (void**)&fn)) {
        // We've already added this function to the module
        return fn;
    } else {
        // Add the function to the module
        fn = LLVMAddFunction(context->llvm_module, name, fn_type);
        assert(hash_table_insert(&context->llvm_function_map, name, fn));
        return fn;
    }
}
