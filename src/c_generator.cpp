#include "c_generator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef WIN32
# define sprintf_s sprintf
#endif

static const char *boolToStr(bool b)
{
    if (b) return "true";
    return "false";
}

static void write_c_string(FILE *f, const char *s)
{
    const char *i = s;
    for (; *i; i++) {
        switch (*i) {
        case '\n':
            fprintf(f, "\\n");
            break;
        case '\\':
            fprintf(f, "\\\\");
            break;
        case '\'':
            fprintf(f, "\\'");
            break;
        case '\"':
            fprintf(f, "\\\"");
            break;
        case '\t':
            fprintf(f, "\\t");
            break;
        default:
            fprintf(f, "%c", *i);
        }
    }
}


void c_generator::generate_preamble()
{
    fprintf(output_file, "#include <stdio.h>\n");
    fprintf(output_file, "#include <stdlib.h>\n\n");
    fprintf(output_file, "typedef signed char         s8;\n");
    fprintf(output_file, "typedef signed short       s16;\n");
    fprintf(output_file, "typedef int                s32;\n");
    fprintf(output_file, "typedef long long          s64;\n");
    fprintf(output_file, "typedef unsigned char       u8;\n");
    fprintf(output_file, "typedef unsigned short     u16;\n");
    fprintf(output_file, "typedef unsigned int       u32;\n");
    fprintf(output_file, "typedef unsigned long long u64;\n");
    fprintf(output_file, "typedef float              f32;\n");
    fprintf(output_file, "typedef double             f64;\n\n");
    fprintf(output_file, "struct string {\n");
    fprintf(output_file, "    char *data;\n");
    fprintf(output_file, "    u32 size;\n");
    fprintf(output_file, "};\n");
    fprintf(output_file, "\n");
}

void c_generator::do_ident()
{
    if (ident > 0) {
        fprintf(output_file, "%*s", ident, "");
    }
}

void c_generator::generate_line_info(BaseAST * ast)
{
    if ((last_filename != ast->filename) || (last_linenum != ast->line_num)) {
        last_linenum = ast->line_num;
        last_filename = ast->filename;
        fprintf(output_file, "\n#line %d \"%s\"\n", ast->line_num, ast->filename);
    }
}

void c_generator::generate_dangling_functions()
{
    for (auto decl : dangling_functions) {
        do_ident();
        fprintf(output_file, "%s = %s_implementation;\n", decl->varname, decl->varname);
    }
}

void c_generator::generate_function_prototype(VariableDeclarationAST * decl, bool second_pass)
{
    // no ident since prototypes are always top level

    assert(decl->specified_type->ast_type == AST_FUNCTION_TYPE);

    auto ft = (FunctionTypeAST *)decl->specified_type;

    bool isMain = !strcmp(decl->varname, "main");
    
    if (ft->isForeign) {
        fprintf(output_file, "extern \"C\" ");
    }

    // first print the return type
    if (isMain && isVoidType(ft->return_type)) {
        // main is a special case, to make the C compiler happy, allow void to be int
        fprintf(output_file, "int");
    } else {
        generate_type(ft->return_type);
    }

    if (decl->flags & DECL_FLAG_IS_CONSTANT) {
        fprintf(output_file, " %s ", decl->varname);
    } else if (decl->definition) {
        if (!second_pass && decl->definition->ast_type == AST_FUNCTION_DEFINITION) {

            /* if we have a definition, we need to write it somewhere
               only do this if the definition is a function body, not 
               for example a variable

               The purpose of the second pass variable is to ensure that function pointers
               do get a prototype in the prototype section

            */
            fprintf(output_file, " %s_implementation ", decl->varname);
            dangling_functions.push_back(decl);

        } else {
            fprintf(output_file, " (*%s) ", decl->varname);
        }
    } else {
        fprintf(output_file, " (*%s) ", decl->varname);
    }
    fprintf(output_file, "(");
    // now print the argument declarations here
    bool first = true;
    for (auto arg : ft->arguments) {
        if (!first) fprintf(output_file, ", ");
        generate_argument_declaration(arg);
        first = false;
    }
    if (ft->hasVariableArguments) {
        if (!first) {
            fprintf(output_file, ", ");
        }
        fprintf(output_file, "...");
    }
    fprintf(output_file, ");\n");

    if (!second_pass && decl->definition && decl->definition->ast_type == AST_FUNCTION_DEFINITION &&
        !(decl->flags & DECL_FLAG_IS_CONSTANT) && !isMain) 
        generate_function_prototype(decl, true);
    
}

void c_generator::generate_struct_prototype(VariableDeclarationAST * decl)
{
    if (decl->flags & DECL_FLAG_HAS_PROTOTYPE_GEN) return;

    auto stype = (StructTypeAST *)decl->specified_type;
    
    fprintf(output_file, "struct %s;\n", decl->varname);
    decl->flags |= DECL_FLAG_HAS_PROTOTYPE_GEN;
}

void c_generator::generate_array_type_prototype(ArrayTypeAST *at)
{
    if (at->array_of_type->ast_type == AST_ARRAY_TYPE) {
        auto innerat = (ArrayTypeAST *)at->array_of_type;
        if (!(innerat->flags & ARRAY_TYPE_FLAG_C_GENERATED)) {
            generate_array_type_prototype(innerat);
        }
    } else if (at->array_of_type->ast_type == AST_STRUCT_TYPE) {
        auto st = (StructTypeAST *)at->array_of_type;
        if (!(st->decl->flags & DECL_FLAG_HAS_PROTOTYPE_GEN)) {
            generate_struct_prototype(st->decl);
        }
    }

    if (at->flags & ARRAY_TYPE_FLAG_C_GENERATED) {
        return;
    }

    fprintf(output_file, "struct _generated_array_type_%d {\n", (int)at->s);
    fprintf(output_file, "    ");
    if (at->array_of_type->ast_type == AST_DIRECT_TYPE) {
        auto dt = (DirectTypeAST *)at->array_of_type;
        fprintf(output_file, BasicTypeToStr(dt));
        fprintf(output_file, " *data;\n");
    } else if (at->array_of_type->ast_type == AST_ARRAY_TYPE) {
        auto innerat = (ArrayTypeAST *)at->array_of_type;
        fprintf(output_file, "    _generated_array_type_%d *data;\n", (int)innerat->s);
    } else if (at->array_of_type->ast_type == AST_STRUCT_TYPE) {
        auto st = (StructTypeAST *)at->array_of_type;
        fprintf(output_file, "    %s *data;\n", st->decl->varname);
    } else {
        assert(!"Not implemented!");
    }
    if (isDynamicArray(at)) {
        fprintf(output_file, "    u64 reserved_size;\n");
    }  else if (isSizedArray(at)) {
        fprintf(output_file, "    u64 count;\n");        
    }
    fprintf(output_file, "};\n");
    at->flags |= ARRAY_TYPE_FLAG_C_GENERATED;
}

void c_generator::ensure_deps_are_generated(StructTypeAST *stype)
{
    for (auto mem : stype->struct_scope.decls) {
        DirectTypeAST *dt = nullptr;
        switch (mem->specified_type->ast_type)
        {
        case AST_DIRECT_TYPE: {
            dt = (DirectTypeAST *)mem->specified_type;
            break;
        }
        case AST_ARRAY_TYPE: {
            dt = findFinalDirectType((ArrayTypeAST *)mem->specified_type);
            break;
        }
        case AST_POINTER_TYPE: {
            dt = findFinalDirectType((PointerTypeAST *)mem->specified_type);
            break;
        }
        }
        
        if (dt && dt->custom_type && (dt->custom_type->ast_type == AST_STRUCT_TYPE)) {
            auto st = (StructTypeAST *)dt->custom_type;
            if (st->decl && !(st->decl->flags & DECL_FLAG_HAS_BEEN_C_GENERATED)) {
                generate_variable_declaration(st->decl);
            }
        }

    }
}


void c_generator::generate_variable_declaration(VariableDeclarationAST * decl)
{
    // If this is a struct, ensure dependent types are declared first
    if (decl->specified_type->ast_type == AST_STRUCT_TYPE) {        
        auto stype = (StructTypeAST *)decl->specified_type;
        ensure_deps_are_generated(stype);
    }

    if (decl->flags & DECL_FLAG_HAS_BEEN_C_GENERATED) {
        // if we generated this already, nothing to do
        return;
    }

    generate_line_info(decl);
    do_ident();

    if (isDirectTypeVariation(decl->specified_type)) {
        TypeAST *type = decl->specified_type;

        switch (type->ast_type) {
        case AST_DIRECT_TYPE: {
            auto dt = (DirectTypeAST *)type;
            fprintf(output_file, BasicTypeToStr(dt));

            fprintf(output_file, " %s", decl->varname);
            break;
        }
        case AST_POINTER_TYPE: {
            auto pt = (PointerTypeAST *)type;
            auto dt = findFinalDirectType(pt);
            fprintf(output_file, "%s ", BasicTypeToStr(dt));

            while (1) {
                fprintf(output_file, "*");
                if (pt->points_to_type->ast_type == AST_DIRECT_TYPE) {
                    break;
                }
                assert(pt->points_to_type->ast_type == AST_POINTER_TYPE);
                pt = (PointerTypeAST *)pt->points_to_type;
            }
    
            fprintf(output_file, " %s", decl->varname);
            break;
        }
        case AST_ARRAY_TYPE: {
            auto at = (ArrayTypeAST *)type;
            auto dt = findFinalDirectType(at);
            fprintf(output_file, "%s ", BasicTypeToStr(dt));

            fprintf(output_file, " %s", decl->varname);

            while (1) {
                fprintf(output_file, "[");
                if (at->num_elems > 0) {
                    fprintf(output_file, "%d", (int)at->num_elems);
                }
                fprintf(output_file, "]");

                if (at->array_of_type->ast_type == AST_DIRECT_TYPE) {
                    break;
                }
                assert(at->array_of_type->ast_type == AST_ARRAY_TYPE);
                at = (ArrayTypeAST *)at->array_of_type;
            }

            break;
        }
        default:
            assert(!"No other types should be allowed here");
        }
            
        if (decl->definition) {
            fprintf(output_file, " = ");

            if (isStringDefinition(decl->definition)) {
                // special use for strings since they are a struct
                if (isStringDeclaration(decl)) {
                    fprintf(output_file, "{ ");
                }
                generate_expression((ExpressionAST *)decl->definition);
                if (isStringDeclaration(decl)) {
                    fprintf(output_file, ", 0 }");
                }
            } else {
                generate_expression((ExpressionAST *)decl->definition);
            }
        }
        fprintf(output_file, ";\n");
    } else if (decl->specified_type->ast_type == AST_FUNCTION_TYPE) {
        auto ft = (FunctionTypeAST *)decl->specified_type;

        bool isMain = !strcmp(decl->varname, "main");

        // foreign functions are prototype only
        if (ft->isForeign) return;

        // if this is a function ptr in C, it's been defined already in
        // the prototypes section, skip it here
        if (!(decl->flags & DECL_FLAG_IS_CONSTANT)) {
            if (decl->definition && decl->definition->ast_type != AST_FUNCTION_DEFINITION) {
                return;
            }
            if (!decl->definition) {
                // a pure function pointer might not have a definition at all
                return;
            }
            if (decl->definition && decl->definition->ast_type == AST_FUNCTION_DEFINITION) {
                // let's write here the actual implementation, now. 
                // and later we assign it. 
                VariableDeclarationAST impl;
                char func_name[256];
                sprintf_s(func_name, "%s_implementation", decl->varname);
                memcpy(&impl, decl, sizeof(impl));
                impl.flags ^= DECL_FLAG_IS_CONSTANT;
                impl.varname = func_name;
                generate_variable_declaration(&impl);
                return;
            }

        }

        // first print the return type
        if (isMain && isVoidType(ft->return_type)) {
            // main is a special case, to make the C compiler happy, allow void to be int
            fprintf(output_file, "int");
        } else {
            generate_type(ft->return_type);
        }

        if (decl->flags & DECL_FLAG_IS_CONSTANT) {
            fprintf(output_file, " %s ", decl->varname);
        } else {
            fprintf(output_file, " (*%s) ", decl->varname);
        }
        fprintf(output_file, "(");
        // now print the argument declarations here
        bool first = true;
        for (auto arg : ft->arguments) {
            if (!first) fprintf(output_file, ", ");
            generate_argument_declaration(arg);
            first = false;
        }
        fprintf(output_file, ")");

        // now make a decision to end the declaration (prototype, function pointer) 
        // or to make this the full function definition
        if (decl->flags & DECL_FLAG_IS_CONSTANT) {
            fprintf(output_file, "\n");
            // if the function is constant, we can declare it here
            // @TODO: think about prototypes
            assert(decl->definition);
            assert(decl->definition->ast_type == AST_FUNCTION_DEFINITION);
            if (!strcmp(decl->varname, "main")) {
                insert_dangling_funcs = true;
            }
            auto fundef = (FunctionDefinitionAST *)decl->definition;
            generate_statement_block(fundef->function_body);
            fprintf(output_file, "\n");
        } else {
            // even if the function is not constant, for C uses we might need
            // to create an implementation and assign it
            fprintf(output_file, ";\n");

        }
    } else if (decl->specified_type->ast_type == AST_STRUCT_TYPE) {
        auto stype = (StructTypeAST *)decl->specified_type;
        fprintf(output_file, "struct %s {\n", decl->varname);
        ident += 4;
        for (auto mem : stype->struct_scope.decls) {
            generate_variable_declaration(mem);
        }
        ident -= 4;
        do_ident();
        fprintf(output_file, "};\n");
    } else {
        assert(!"Type not suported on C code generation yet");
    }

    decl->flags |= DECL_FLAG_HAS_BEEN_C_GENERATED;
}

void c_generator::generate_argument_declaration(VariableDeclarationAST * arg)
{
    assert(isDirectTypeVariation(arg->specified_type));

    /*
    C declaration of types is pretty confusing, with pointers in one side, 
    arrays in the other, and no way to specify precedence. 
    It would be best said that we do not support arguments with very complex
    types, or then we have to do the typedef route. 
    */

    generate_type(arg->specified_type);

    fprintf(output_file, " %s", arg->varname);
}

void c_generator::generate_statement_block(StatementBlockAST * block)
{
    generate_line_info(block);
    do_ident();
    fprintf(output_file, "{\n");
    generate_line_info(block);
    ident += 4;

    if (insert_dangling_funcs) {
        generate_dangling_functions();
        insert_dangling_funcs = false;
    }

    for (auto stmt : block->statements) generate_statement(stmt);

    ident -= 4;
    do_ident();
    fprintf(output_file, "}\n");
}

void c_generator::generate_statement(StatementAST * stmt)
{
    switch (stmt->ast_type) {
    case AST_VARIABLE_DECLARATION: {
        generate_variable_declaration((VariableDeclarationAST *)stmt);
        break;
    }
    case AST_RETURN_STATEMENT: {
        generate_return_statement((ReturnStatementAST *)stmt);
        break;
    }
    case AST_ASSIGNMENT: {
        generate_line_info(stmt);
        do_ident();
        generate_assignment((AssignmentAST *)stmt);
        fprintf(output_file, ";\n");
        break;
    }
    case AST_FUNCTION_CALL: {
        generate_line_info(stmt);
        do_ident();
        generate_function_call((FunctionCallAST *)stmt);
        fprintf(output_file, ";\n");
        break;
    }
    case AST_IF_STATEMENT: {
        generate_if_statement((IfStatementAST *)stmt);
        break;
    }
    case AST_STATEMENT_BLOCK: {
        generate_statement_block((StatementBlockAST *)stmt);
        break;
    }
    default:
        assert(!"Not implemented yet");
    }
}

void c_generator::generate_if_statement(IfStatementAST * ifst)
{
    generate_line_info(ifst);
    do_ident();
    fprintf(output_file, "if (");
    generate_expression(ifst->condition);
    fprintf(output_file, ") ");
    generate_statement(ifst->then_branch);
    if (ifst->else_branch) {
        generate_line_info(ifst->else_branch);
        do_ident();
        fprintf(output_file, "else ");
        generate_statement(ifst->else_branch);
    }
}

void c_generator::generate_return_statement(ReturnStatementAST * ret)
{
    generate_line_info(ret);
    do_ident();
    fprintf(output_file, "return ");
    generate_expression(ret->ret);
    fprintf(output_file, ";\n");
}

void c_generator::generate_assignment(AssignmentAST * assign)
{
    generate_expression(assign->lhs);

    fprintf(output_file, TokenTypeToCOP(assign->op));

    generate_expression(assign->rhs);
}

void c_generator::generate_expression(ExpressionAST * expr)
{
    if (!expr) return;
    switch (expr->ast_type) {
    case AST_LITERAL: {
        auto lit = (LiteralAST *)expr;
        switch (lit->typeAST->basic_type) {
        case BASIC_TYPE_FLOATING:
            fprintf(output_file, "%f", lit->_f64);
            break;
        case BASIC_TYPE_BOOL:
            fprintf(output_file, "%s", boolToStr(lit->_bool));
            break;
        case BASIC_TYPE_STRING:
            fprintf(output_file, "\"");
            write_c_string(output_file, lit->str);
            fprintf(output_file, "\", %" U64FMT "u", strlen(lit->str));
            break;
        case BASIC_TYPE_INTEGER:
            if (lit->typeAST->isSigned) fprintf(output_file, "%" U64FMT "d", lit->_s64);
            else fprintf(output_file, "%" U64FMT "u", lit->_u64);
            break;
        }

        break;
    }
    case AST_IDENTIFIER: {
        auto iden = (IdentifierAST *)expr;
        fprintf(output_file, "%s", iden->name);
        generate_expression(iden->next);
        break;
    }
    case AST_BINARY_OPERATION: {
        auto binop = (BinaryOperationAST *)expr;
        generate_expression(binop->lhs);

        fprintf(output_file, TokenTypeToCOP(binop->op));

        generate_expression(binop->rhs);
        break;
    }
    case AST_FUNCTION_CALL: {
        generate_function_call((FunctionCallAST *)expr);
        break;
    }
    case AST_UNARY_OPERATION: {
        auto unop = (UnaryOperationAST *)expr;
        // Special case for two operators with different meaning
        if (unop->op == TK_STAR) {
            fprintf(output_file, "&");
        } else if (unop->op == TK_LSHIFT) {
            fprintf(output_file, "*");
        } else {
            fprintf(output_file, TokenTypeToCOP(unop->op));
        }

        generate_expression(unop->expr);
        break;
    }
    case AST_RUN_DIRECTIVE: {
        // we should never get here, do something crappy for now
        fprintf(output_file, "0");
        break;
    } 
    case AST_STRUCT_ACCESS: {
        auto sac = (StructAccessAST *)expr;

        if (sac->decl->specified_type->ast_type == AST_POINTER_TYPE) {
            fprintf(output_file, "->");
        } else {
            fprintf(output_file, ".");
        }
        fprintf(output_file, "%s", sac->name);
        generate_expression(sac->next);
        break;
    }
    case AST_ARRAY_ACCESS: {
        auto ac = (ArrayAccessAST *)expr;
        fprintf(output_file, "[");
        generate_expression(ac->array_exp);
        fprintf(output_file, "]");
        generate_expression(ac->next);
        break;
    }
    default:
        assert(!"We should never get here");
    }
}

void c_generator::generate_function_call(FunctionCallAST * call)
{
    fprintf(output_file, "%s(", call->function_name);
    bool first = true;
    for (auto arg : call->args) {
        if (!first) fprintf(output_file, ", ");
        generate_expression(arg);
        first = false;
    }
    fprintf(output_file, ")");
}

void c_generator::generate_type(BaseAST * ast)
{
    switch (ast->ast_type) {
    case AST_DIRECT_TYPE: {
        auto dt = (DirectTypeAST *)ast;
        if (dt->basic_type == BASIC_TYPE_STRING) {            
            fprintf(output_file, "char * rand_%d, unsigned long", (int)ast->s);
        }
        else {
            fprintf(output_file, BasicTypeToStr(dt));
        }
        break;
    }
    case AST_POINTER_TYPE: {
        TypeAST *type = (TypeAST *)ast;
        auto pt = (PointerTypeAST *)ast;
        auto final = findFinalDirectType(pt);

        fprintf(output_file, BasicTypeToStr(final));

        do {
            if (type->ast_type == AST_POINTER_TYPE) {
                auto pt = (PointerTypeAST *)type;
                fprintf(output_file, "*");
                type = pt->points_to_type;
            } else {
                assert(type->ast_type == AST_DIRECT_TYPE);
                break;
            }
        } while (1);
        break;
    }
    case AST_ARRAY_TYPE: {
        assert(!"Array C generation is unimplemented");
        break;
    }
    case AST_FUNCTION_TYPE: {
        assert(!"Not implemented yet");
        break;
    }
    default:
        // we might want to support functions here... we should
        assert(!"No other types should be allowed here");
    }
}


void c_generator::generate_c_file(FileObject &filename, FileAST * root)
{
    CPU_SAMPLE("generate C");

#ifdef WIN32
    fopen_s(&output_file, filename.getFilename(), "w");
#else
    output_file = fopen(filename.getFilename(), "w");
#endif      
    ident = 0;
    last_filename = nullptr;
    last_linenum = 0;
    dangling_functions.reset();
    insert_dangling_funcs = false;
    // dangling functions have an issue with possible local functions
    // also named main... but it is remote


    generate_preamble();

    // when we have them, place custom types here, before prototypes
    // maybe type prototypes (forward decls)

    // write function prototypes
    for (auto &ast : root->items) {
        if (ast->ast_type == AST_VARIABLE_DECLARATION) {
            auto decl = (VariableDeclarationAST *)ast;
            if (decl->specified_type->ast_type == AST_FUNCTION_TYPE) {
                generate_function_prototype(decl);
            } else if (decl->specified_type->ast_type == AST_STRUCT_TYPE) {
                generate_struct_prototype(decl);
            } else if (decl->specified_type->ast_type == AST_ARRAY_TYPE) {
                auto at = (ArrayTypeAST *)decl->specified_type;
                generate_array_type_prototype(at);
            }
        }            
    };
    fprintf(output_file, "\n\n");

    for (auto &ast : root->items) {
        switch (ast->ast_type) {
        case AST_VARIABLE_DECLARATION: {
            generate_variable_declaration((VariableDeclarationAST *)ast);
            break;
        }
        default:
            printf("  C Generation: AST type not supported at the top level: %d\n", ast->ast_type);
        }
    }

    fclose(output_file);
    output_file = nullptr;
}
