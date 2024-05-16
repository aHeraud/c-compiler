#include <string.h>
#include <ctype.h>
#include "llvm-c/Core.h"
#include "llvm-c/Analysis.h"

#include "ir/ir.h"
#include "llvm-gen.h"
#include "ir/cfg.h"

typedef struct LLVMGenContext {
    LLVMModuleRef llvm_module;
    LLVMValueRef llvm_function;
    LLVMBasicBlockRef llvm_block;
    LLVMBuilderRef llvm_builder;

    // Mapping of IR function names to LLVM values
    hash_table_t llvm_function_map;
    // Mapping of IR global variables to LLVM values
    hash_table_t global_var_map;
    // Mapping of IR variables to LLVM values
    hash_table_t local_var_map;
    // Mapping of IR basic block IDs to LLVM basic blocks
    hash_table_t block_map;

    ir_control_flow_graph_t ir_cfg;
} llvm_gen_context_t;

LLVMTypeRef ir_to_llvm_type(const ir_type_t *type);
LLVMValueRef ir_to_llvm_value(const llvm_gen_context_t *context, const ir_value_t *value);
LLVMValueRef llvm_get_or_add_function(llvm_gen_context_t *context, const char* name, LLVMTypeRef fn_type);

void llvm_gen_visit_function(llvm_gen_context_t *context, const ir_function_definition_t *function);
void llvm_gen_visit_basic_block(llvm_gen_context_t *context, const ir_basic_block_t *block);
void llvm_gen_visit_instruction(llvm_gen_context_t *context, const ir_instruction_t *instr, const ir_basic_block_t *ir_block);

LLVMBasicBlockRef llvm_get_or_create_basic_block(llvm_gen_context_t *context, const ir_basic_block_t *ir_block) {
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

void llvm_gen_module(const ir_module_t *module, const char* output_filename) {
    // init
    llvm_gen_context_t context = {
        .llvm_module = LLVMModuleCreateWithName(module->name),
        .llvm_builder = LLVMCreateBuilder(),
        .llvm_function_map = hash_table_create(128),
        .global_var_map = hash_table_create(128),
    };

    // global variables
    for (size_t i = 0; i < module->globals.size; i += 1) {
        const ir_global_t *global = module->globals.buffer[i];
        // Get the actual type. The globals IR type is a pointer to the actual type.
        assert(global->type->kind == IR_TYPE_PTR);
        const ir_type_t *ir_type = global->type->ptr.pointee;
        char *name = global->name;
        if (name[0] == '@') {
            if (isdigit(name[1])) {
                // Anonymous global variable (e.g. string literal)
                name = "";
            } else {
                name += 1;
            }
        }
        LLVMValueRef llvm_global = LLVMAddGlobal(context.llvm_module, ir_to_llvm_type(ir_type), name);
        if (global->value.kind == IR_CONST_STRING) {
            LLVMSetInitializer(llvm_global, LLVMConstString(global->value.s, strlen(global->value.s), false));
        }
        hash_table_insert(&context.global_var_map, global->name, llvm_global);
    }

    // codegen
    for (size_t i = 0; i < module->functions.size; i += 1) {
        llvm_gen_visit_function(&context, module->functions.buffer[i]);
    }

    // finalize - validate the module and write it to a file
    char *message;
    LLVMVerifyModule(context.llvm_module, LLVMAbortProcessAction, &message);
    LLVMDisposeMessage(message);
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
        LLVMAddFunction(context->llvm_module, function->name, ir_to_llvm_type(function->type));
    LLVMSetLinkage(context->llvm_function, LLVMExternalLinkage);

    context->local_var_map = hash_table_create(128);
    context->block_map = hash_table_create(128);

    // The IR refers to the parameters by name, so we need to set up the mapping
    for (int i = 0; i < function->num_params; i += 1) {
        LLVMValueRef param = LLVMGetParam(context->llvm_function, i);
        hash_table_insert(&context->local_var_map, function->params[i].name, param);
    }

    // Get the control flow graph for the IR function
    context->ir_cfg = ir_create_control_flow_graph(function);

    // Visit each basic block in the CFG
    for (int i = 0; i < context->ir_cfg.basic_blocks.size; i += 1) {
        ir_basic_block_t *block = context->ir_cfg.basic_blocks.buffer[i];
        llvm_gen_visit_basic_block(context, block);
    }
}

void llvm_gen_visit_basic_block(llvm_gen_context_t *context, const ir_basic_block_t *block) {
    char tmp[256];
    snprintf(tmp, sizeof(tmp), "block_%d", block->id);

    context->llvm_block = llvm_get_or_create_basic_block(context, block);
    LLVMPositionBuilderAtEnd(context->llvm_builder, context->llvm_block);

    for (size_t i = 0; i < block->instructions.size; i += 1) {
        const ir_instruction_t *instr = block->instructions.buffer[i];
        llvm_gen_visit_instruction(context, instr, block);
    }
}

void llvm_gen_visit_instruction(
    llvm_gen_context_t *context,
    const ir_instruction_t *instr,
    const ir_basic_block_t *ir_block
) {
    // This currently only works if the input IR is already in SSA form. The IR generated
    // by the first pass of the AST is in SSA form, since all variables that live across basic
    // block boundaries are just stored on the stack.

    bool is_last_instr_in_block = ir_block->instructions.buffer[ir_block->instructions.size - 1] == instr;
    bool is_terminator = instr->opcode == IR_RET || instr->opcode == IR_BR || instr->opcode == IR_BR_COND;

    switch (instr->opcode) {
        case IR_NOP:
            break;
        case IR_ADD: {
            LLVMValueRef lhs = ir_to_llvm_value(context, &instr->binary_op.left);
            LLVMValueRef rhs = ir_to_llvm_value(context, &instr->binary_op.right);
            LLVMValueRef result;
            if (ir_is_float_type(instr->binary_op.result.type)) {
                result = LLVMBuildFAdd(context->llvm_builder, lhs, rhs, "");
            } else {
                result = LLVMBuildAdd(context->llvm_builder, lhs, rhs, "");
            }
            hash_table_insert(&context->local_var_map, instr->binary_op.result.name, result);
            break;
        }
        case IR_SUB: {
            LLVMValueRef lhs = ir_to_llvm_value(context, &instr->binary_op.left);
            LLVMValueRef rhs = ir_to_llvm_value(context, &instr->binary_op.right);
            LLVMValueRef result;
            if (ir_is_float_type(instr->binary_op.result.type)) {
                result = LLVMBuildFSub(context->llvm_builder, lhs, rhs, "");
            } else {
                result = LLVMBuildSub(context->llvm_builder, lhs, rhs, "");
            }
            hash_table_insert(&context->local_var_map, instr->binary_op.result.name, result);
            break;
        }
        case IR_MUL:
            assert(false && "Not implemented");
            break;
        case IR_DIV:
            assert(false && "Not implemented");
            break;
        case IR_MOD:
            assert(false && "Not implemented");
            break;
        case IR_ASSIGN: {
            LLVMValueRef value = ir_to_llvm_value(context, &instr->assign.value);
            hash_table_insert(&context->local_var_map, instr->assign.result.name, value);
            break;
        }
        case IR_AND:
            assert(false && "Not implemented");
            break;
        case IR_OR:
            assert(false && "Not implemented");
            break;
        case IR_SHL:
            assert(false && "Not implemented");
            break;
        case IR_SHR:
            assert(false && "Not implemented");
            break;
        case IR_XOR:
            assert(false && "Not implemented");
            break;
        case IR_NOT:
            assert(false && "Not implemented");
            break;
        case IR_EQ: {
            LLVMValueRef lhs = ir_to_llvm_value(context, &instr->binary_op.left);
            LLVMValueRef rhs = ir_to_llvm_value(context, &instr->binary_op.right);
            LLVMValueRef result;
            if (ir_is_float_type(ir_get_type_of_value(instr->binary_op.left))) {
                result = LLVMBuildFCmp(context->llvm_builder, LLVMRealOEQ, lhs, rhs, "");
            } else {
                result = LLVMBuildICmp(context->llvm_builder, LLVMIntEQ, lhs, rhs, "");
            }
            hash_table_insert(&context->local_var_map, instr->binary_op.result.name, result);
            break;
        }
        case IR_NE:
            assert(false && "Not implemented");
            break;
        case IR_LT:
            assert(false && "Not implemented");
            break;
        case IR_LE:
            assert(false && "Not implemented");
            break;
        case IR_GT:
            assert(false && "Not implemented");
            break;
        case IR_GE:
            assert(false && "Not implemented");
            break;
        case IR_BR:
            assert(false && "Not implemented");
            break;
        case IR_BR_COND: {
            const char* label = instr->branch.label;
            const ir_basic_block_t *ir_true_block;
            assert(hash_table_lookup(&context->ir_cfg.label_to_block_map, label, (void**)&ir_true_block));
            const ir_basic_block_t *ir_false_block = ir_block->fall_through;
            assert(ir_false_block != NULL);

            LLVMValueRef cond = ir_to_llvm_value(context, &instr->branch.cond);
            LLVMBasicBlockRef true_block = llvm_get_or_create_basic_block(context, ir_true_block);
            LLVMBasicBlockRef false_block = llvm_get_or_create_basic_block(context, ir_false_block);
            LLVMBuildCondBr(context->llvm_builder, cond, true_block, false_block);
            break;
        }
        case IR_CALL: {
            LLVMTypeRef fn_type = ir_to_llvm_type(instr->call.function.type);
            LLVMValueRef fn = llvm_get_or_add_function(context, instr->call.function.name, fn_type);
            LLVMValueRef *args = malloc(instr->call.num_args * sizeof(LLVMValueRef));
            for (int i = 0; i < instr->call.num_args; i += 1) {
                args[i] = ir_to_llvm_value(context, &instr->call.args[i]);
            }
            LLVMValueRef result = LLVMBuildCall2(context->llvm_builder, fn_type, fn, args, instr->call.num_args, "");
            if (instr->call.result != NULL) {
                hash_table_insert(&context->local_var_map, instr->call.result->name, result);
            }
            break;
        }
        case IR_RET: {
            if (!instr->ret.has_value) {
                LLVMBuildRetVoid(context->llvm_builder);
            } else {
                LLVMValueRef value = ir_to_llvm_value(context, &instr->ret.value);
                LLVMBuildRet(context->llvm_builder, value);
            }
            break;
        }
        case IR_ALLOCA: {
            LLVMValueRef result = LLVMBuildAlloca(context->llvm_builder, ir_to_llvm_type(instr->alloca.type), "");
            hash_table_insert(&context->local_var_map, instr->alloca.result.name, result);
            break;
        }
        case IR_LOAD: {
            ir_value_t ptr = instr->unary_op.operand;
            const ir_type_t *ptr_type = ir_get_type_of_value(ptr);
            LLVMValueRef result = LLVMBuildLoad2(
                context->llvm_builder,
                ir_to_llvm_type(ptr_type->ptr.pointee),
                ir_to_llvm_value(context, &ptr),
                ""
            );
            hash_table_insert(&context->local_var_map, instr->unary_op.result.name, result);
            break;
        }
        case IR_STORE:
            LLVMBuildStore(context->llvm_builder,
                           ir_to_llvm_value(context, &instr->store.value),
                           ir_to_llvm_value(context, &instr->store.ptr));
            break;
        case IR_MEMCPY:
            assert(false && "Not implemented");
            break;
        case IR_TRUNC: {
            LLVMValueRef result;
            if (ir_is_float_type(ir_get_type_of_value(instr->unary_op.operand))) {
                result = LLVMBuildFPTrunc(context->llvm_builder,
                    ir_to_llvm_value(context, &instr->unary_op.operand),
                    ir_to_llvm_type(instr->unary_op.result.type),
                    "");
            } else {
                result = LLVMBuildTrunc(context->llvm_builder,
                    ir_to_llvm_value(context, &instr->unary_op.operand),
                    ir_to_llvm_type(instr->unary_op.result.type),
                    "");
            }
            hash_table_insert(&context->local_var_map, instr->unary_op.result.name, result);
            break;
        }
        case IR_EXT: {
            LLVMValueRef result;
            if (ir_is_float_type(ir_get_type_of_value(instr->unary_op.operand))) {
                result = LLVMBuildFPExt(context->llvm_builder,
                    ir_to_llvm_value(context, &instr->unary_op.operand),
                    ir_to_llvm_type(instr->unary_op.result.type),
                    "");
            } else {
                if (ir_is_signed_integer_type(ir_get_type_of_value(instr->unary_op.operand))) {
                    result = LLVMBuildSExt(context->llvm_builder,
                        ir_to_llvm_value(context, &instr->unary_op.operand),
                        ir_to_llvm_type(instr->unary_op.result.type),
                        "");
                } else {
                    result = LLVMBuildZExt(context->llvm_builder,
                        ir_to_llvm_value(context, &instr->unary_op.operand),
                        ir_to_llvm_type(instr->unary_op.result.type),
                        "");
                }
            }
            hash_table_insert(&context->local_var_map, instr->unary_op.result.name, result);
            break;
        }
        case IR_FTOI:
            assert(false && "Not implemented");
            break;
        case IR_ITOF:
            assert(false && "Not implemented");
            break;
        case IR_PTOI: {
            LLVMValueRef result = LLVMBuildPtrToInt(context->llvm_builder,
                ir_to_llvm_value(context, &instr->unary_op.operand),
                ir_to_llvm_type(instr->unary_op.result.type),
                "");
            hash_table_insert(&context->local_var_map, instr->unary_op.result.name, result);
            break;
        }
        case IR_ITOP: {
            LLVMValueRef result = LLVMBuildIntToPtr(context->llvm_builder,
                ir_to_llvm_value(context, &instr->unary_op.operand),
                ir_to_llvm_type(instr->unary_op.result.type),
                "");
            hash_table_insert(&context->local_var_map, instr->unary_op.result.name, result);
            break;
        }
        case IR_BITCAST: {
            LLVMValueRef result = LLVMBuildBitCast(context->llvm_builder,
                ir_to_llvm_value(context, &instr->unary_op.operand),
                ir_to_llvm_type(instr->unary_op.result.type),
                "");
            hash_table_insert(&context->local_var_map, instr->unary_op.result.name, result);
            break;
        }
    }

    if (is_last_instr_in_block && !is_terminator) {
        // If the last instruction in the block isn't a terminator, there must be an explicit branch
        // to the next block.
        assert(ir_block->fall_through != NULL && "Expected a fall-through block");
        LLVMBuildBr(context->llvm_builder, llvm_get_or_create_basic_block(context, ir_block->fall_through));
    }
}

LLVMTypeRef ir_to_llvm_type(const ir_type_t *type) {
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
        case IR_TYPE_PTR:
            return LLVMPointerType(ir_to_llvm_type(type->ptr.pointee), 0);
        case IR_TYPE_ARRAY:
            return LLVMArrayType(ir_to_llvm_type(type->array.element), type->array.length);
        case IR_TYPE_STRUCT:
            assert(false && "Not implemented");
            break;
        case IR_TYPE_FUNCTION: {
            LLVMTypeRef *param_types = malloc(type->function.num_params * sizeof(LLVMTypeRef));
            for (int i = 0; i < type->function.num_params; i += 1) {
                param_types[i] = ir_to_llvm_type(type->function.params[i]);
            }
            return LLVMFunctionType(ir_to_llvm_type(type->function.return_type), param_types, type->function.num_params, type->function.is_variadic);
        }
    }
}

LLVMValueRef ir_to_llvm_value(const llvm_gen_context_t *context, const ir_value_t *value) {
    switch (value->kind) {
        case IR_VALUE_CONST: {
            const ir_type_t *ir_type = value->constant.type;
            switch (value->constant.kind) {
                case IR_CONST_INT:
                    return LLVMConstInt(ir_to_llvm_type(ir_type), value->constant.i, false);
                case IR_CONST_FLOAT:
                    return LLVMConstReal(ir_to_llvm_type(ir_type), value->constant.f);
                case IR_CONST_STRING:
                    assert(false && "Not implemented");
                    break;
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