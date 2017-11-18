#include "bytecode_generator.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

const u64 stack_size = 10 * 1024;

union __xxx_to_u64 {
    f64 f;
    s64 s;
    u64 u;
    void *p;
};

static u64 straight_convert(f64 f) {
    __xxx_to_u64 x;
    x.f = f;
    return x.u;
}

static u64 straight_convert(s64 s) {
    __xxx_to_u64 x;
    x.s = s;
    return x.u;
}

static u64 straight_convert(u64 u) {
    return u;
}

static u64 straight_convert(void *p) {
    __xxx_to_u64 x;
    x.p = p;
    return x.u;
}

static inline u8 truncate_op_size(s64 bits)
{
    s64 bytes = bits /= 8;
    if (bytes < 8) return (u8)bytes;
    return 8;
}

static inline u64 getVariableSize(VariableDeclarationAST *decl)
{
    if ((decl->flags & DECL_FLAG_IS_CONSTANT) &&
        isFunctionDeclaration(decl)) {
        // constant functions do not need space
        return 0;
    }
    
    return decl->specified_type->size_in_bits / 8;
}

static inline bool isGlobal(BaseAST *ast)
{
    assert(ast);
    assert(ast->scope);
    return ast->scope->parent == nullptr;
}

static s16 reserveRegistersForSize(bytecode_program *program, u64 size_in_bits)
{
    // @TODO: Possible optimization, for large structs or arrays,
    // store them in the stack and just copy here pointers to that
    u64 count = size_in_bits / 64;
    if (size_in_bits %64 != 0) count ++;
    return program->reserve_register(count);
}

static u64 getScopeVariablesSize(Scope *scope)
{
    u64 total_size = 0;
    
    // this only works for the top scope for now due to computing the total_size
    // might work for stack... who knows
    assert(scope->parent == nullptr);

    for (auto decl : scope->decls) {
        assert(decl->specified_type);
        assert(decl->specified_type->size_in_bits > 0);
        assert((decl->specified_type->size_in_bits % 8) == 0);
        
        u64 var_size = getVariableSize(decl);

        if (var_size > 0) {
            // the offset for global variables is the accumulated size in bytes
            decl->bc_mem_offset = total_size;
            
            total_size += decl->specified_type->size_in_bits / 8;
        }
    }
    return total_size;
}

static inline u64 roundToPage(u64 size, u64 page_size)
{
    size = (page_size - 1)&size ? ((size + page_size) & ~(page_size - 1)) : size;
    return size;
}

#define CASE_BC_OPCODE(a) case a: return #a
const char *bc_opcode_to_str(BytecodeInstructionOpcode opcode)
{
    switch(opcode)
    {
        CASE_BC_OPCODE(BC_UNINITIALIZED);
        CASE_BC_OPCODE(BC_ZERO_REG);
        CASE_BC_OPCODE(BC_LOAD_BIG_CONSTANT_TO_REG);
        CASE_BC_OPCODE(BC_STORE_TO_STACK_PLUS_CONSTANT);
        CASE_BC_OPCODE(BC_STORE_TO_BSS_PLUS_CONSTANT);
        default:
            return "UNKNOWN OPCODE";
    }
}

void print_instruction(BCI *inst)
{
    printf("  op: %s src: %d dst: %d size: %d big_const: %lu\n",
           bc_opcode_to_str(inst->opcode), (int)inst->src_reg,
           (int)inst->dst_reg, inst->op_size, inst->big_const);
}

void print_bc_function(bytecode_function *func)
{
    if (func->function_name) {
        printf("Function : %s ", func->function_name);
    } else {
        printf("Function <unnamed> ");
    }
    printf(" local var size: %d num_instructions: %d\n",
           func->local_variables_size, func->instructions.size());
    for(auto inst: func->instructions) {
        print_instruction(inst);
    }
}

void print_bc_program(bytecode_program *program)
{
    printf("Preamble function:\n");
    print_bc_function(&program->preamble_function);

    printf("Start function: ");
    if (program->start_function) {
        printf("%s\n", program->start_function->function_name);
    } else {
        printf(" <none>\n");
    }
    
    for (auto func:program->functions) {
        print_bc_function(func);
    }
}

void bytecode_generator::createStoreInstruction(VariableDeclarationAST *decl, s16 reg)
{
    BCI *bci;
    BytecodeInstructionOpcode opcode;
    if (isGlobal(decl)) {
        opcode = BC_STORE_TO_BSS_PLUS_CONSTANT;
    } else {
        opcode = BC_STORE_TO_STACK_PLUS_CONSTANT;
    }
    
    bci = create_instruction(opcode, reg, -1, decl->bc_mem_offset);
    bci->op_size = truncate_op_size(decl->specified_type->size_in_bits);
    issue_instruction(bci);
    if (decl->specified_type->size_in_bits > 64) {
        s64 bits = decl->specified_type->size_in_bits;
        u64 offset = decl->bc_mem_offset + 8;
        bits -= 64;
        reg++;
        do {
            BCI *bci = create_instruction(opcode, reg, -1, offset);
            issue_instruction(bci);
            bci->op_size = truncate_op_size(bits);
            reg++;
            bits -= 64;
            offset += 8;
        } while (bits > 0);
    }
}

BCI * bytecode_generator::create_instruction(BytecodeInstructionOpcode opcode, s16 src_reg, s16 dst_reg, u64 big_const)
{
    BCI *bci = new (pool) BCI;
    bci->opcode = opcode;
    bci->src_reg = src_reg;
    bci->dst_reg = dst_reg;
    bci->big_const = big_const;

    return bci;
}

void bytecode_generator::issue_instruction(BCI * bci)
{
    bci->inst_index = current_function->instructions.push_back(bci);
}

bytecode_program * bytecode_generator::compileToBytecode(FileAST * root)
{
    bytecode_program *bp = new (pool) bytecode_program;
    this->program = bp;

    // First step, have space in the bss for the global variables (non functions)
    u64 bss_size = getScopeVariablesSize(&root->global_scope);
    // ensure we get page aligned memory chunks
    bss_size = roundToPage(bss_size, 4 * 1024);

    bp->bss.initMem((u8 *)pool->alloc(bss_size + stack_size),
        bss_size, stack_size);
    
    // set the correct value in the bss area for vars (bytecode instruction)
    current_function = &program->preamble_function;
    initializeVariablesInScope(&root->global_scope);
    current_function = nullptr;
    
    // Update function variable addresses?
    
    return bp;
}

void bytecode_generator::generate_function(TextType name, FunctionDefinitionAST * fundef)
{
    if (fundef->declaration->isForeign) return; // foreign functions just get called

    bytecode_function *old_current = current_function;
    bytecode_function *func = new (pool) bytecode_function;
    func->function_name = name;
    func->function_id = fundef->s;
    current_function = func;
	
    // Creating a function is the same as processing its statementBlock
    generate_statement_block(fundef->function_body);
	
	current_function = old_current;
    if (!strcmp(name, "main") && (fundef->scope->parent == nullptr)) {
        program->start_function = func;
    }
    program->functions.push_back(func);
}

void bytecode_generator::generate_statement_block(StatementBlockAST *block)
{
    // first thing, allocate space for the variables, and process implicit
    // initialization
    for (auto stmt: block->statements) {
        switch (stmt->ast_type) {
            case AST_VARIABLE_DECLARATION: {
                auto decl = (VariableDeclarationAST *)stmt;
                u64 var_size = getVariableSize(decl);
                if (var_size > 0) {
                    // reserve space and figure out where it is
                    decl->bc_mem_offset = current_function->local_variables_size;
                    current_function->local_variables_size += var_size;
                }
                initializeVariable(decl);
                break;
            }
            case AST_ASSIGNMENT: {
                auto assign = (AssignmentAST *)stmt;
                s16 mark = program->reg_mark();
                s16 reg = reserveRegistersForSize(program, assign->lhs->expr_type->size_in_bits);
                computeExpressionIntoRegister(assign->rhs, reg);

                if (assign->rhs->ast_type == AST_IDENTIFIER) {
                    auto iden = (IdentifierAST *)assign->rhs;
                    createStoreInstruction(iden->decl, reg);
                } else {
                    assert (! "We do not support anything else than an identifier for lhs for now");
                }
                
                program->pop_mark(mark);
                break;
            }
            case AST_FUNCTION_CALL: {
                assert(false);
                break;
            }
            case AST_RETURN_STATEMENT: {
                assert(false);
                break;
            }
            case AST_STATEMENT_BLOCK: {
                assert(false);
                break;
            }
            default:
                assert(false);
        }
    }
}

void bytecode_generator::initializeVariablesInScope(Scope * scope)
{
    for (auto decl : scope->decls) {
        initializeVariable(decl);
    }
}

void bytecode_generator::initializeVariable(VariableDeclarationAST * decl)
{
    if (!decl->definition) return; // nothing to do for this var
    // @TODO: initialize it to 0 by default
    
    if (decl->definition->ast_type == AST_FUNCTION_DEFINITION) {
        auto fundef = (FunctionDefinitionAST *)decl->definition;
        generate_function(decl->varname, fundef);
        // assert(false);
        return;
    }
    // @TODO: we do not support function pointers yet

    // ok, it is an expression (operation, literal (num, string) )
    // or god forbid, another variable (which means load its value
    s16 mark = program->reg_mark();
    s16 reg = reserveRegistersForSize(program, decl->specified_type->size_in_bits);
    
    computeExpressionIntoRegister((ExpressionAST *)decl->definition, reg);
    assert(decl->scope->parent == nullptr);
    createStoreInstruction(decl, reg);
    
    program->pop_mark(mark);
}

void bytecode_generator::computeExpressionIntoRegister(ExpressionAST * expr, s16 reg)
{
    switch (expr->ast_type) {
    case AST_LITERAL: {
        auto lit = (LiteralAST *)expr;

        switch (lit->typeAST.basic_type) {
        case BASIC_TYPE_INTEGER: {
            u64 val;
            if (lit->typeAST.isSigned) val = straight_convert(lit->_s64);
            else val = lit->_u64;
            BCI *bci = create_instruction(BC_LOAD_BIG_CONSTANT_TO_REG, -1, reg, val);
            bci->op_size = lit->typeAST.size_in_bits / 8;
            issue_instruction(bci);
            break;
        }
        case BASIC_TYPE_STRING: {
            // Hacky version, the string data will use the pointer in the AST... 
            // @TODO: think of a better version to handle strings, such as:
            /*
               - Strings in some data segment (all the ones in the program?)
               - allocate memory for all the strings, malloc style
               - Inline the strings (if known size), right after the pointer or such
               - Are strings mutable? How does allocation work? Do we care?
            */
            u64 val = straight_convert(lit->str);
            BCI *bci = create_instruction(BC_LOAD_BIG_CONSTANT_TO_REG, -1, reg, val);
            bci->op_size = 8;
            issue_instruction(bci);
            val = strlen(lit->str);
            bci = create_instruction(BC_LOAD_BIG_CONSTANT_TO_REG, -1, reg+1, val);
            bci->op_size = 8;
            issue_instruction(bci);
            break;
        }
        default:
            assert(false);
                                 // @TODO: add here float, bool... 
        }
        break;
    }
    case AST_IDENTIFIER: {
        auto id = (IdentifierAST *)expr;
        break;
    }
    case AST_UNARY_OPERATION: {
        auto unop = (UnaryOperationAST *)expr;
        break;
    }
    case AST_BINARY_OPERATION: {
        auto binop = (BinaryOperationAST *)expr;
        break;
    }
    case AST_ASSIGNMENT: {
        assert(!"Assignment expression bytecode not implemented");
        break;
    }
    default:
        assert(!"Unknown expression AST for bytecode");
    }
}

void bc_base_memory::initMem(u8 * basemem, u64 bss_size, u64 stack_size)
{
    mem = basemem;
    alloc_size = bss_size + stack_size;
    used_size = 0;
    stack_base = mem + bss_size;
    stack_index = 0;
    this->stack_size = stack_size;
}