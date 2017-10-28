#include "AST.h"
#include <stdio.h>

static const char *BasicTypeToStr(BasicType t)
{
    switch (t)
    {
    case BASIC_TYPE_BOOL:  return "BOOL";
    case BASIC_TYPE_STRING:  return "STRING";
    case BASIC_TYPE_S8:  return "S8";
    case BASIC_TYPE_S16: return "S16";
    case BASIC_TYPE_S32: return "S32";
    case BASIC_TYPE_S64: return "S64";
    case BASIC_TYPE_U8:  return "U8";
    case BASIC_TYPE_U16: return "U16";
    case BASIC_TYPE_U32: return "U32";
    case BASIC_TYPE_U64: return "U64";
    case BASIC_TYPE_F32: return "F32";
    case BASIC_TYPE_F64: return "F64";
    }
    return "UNKNOWN";
}

void printAST(const BaseAST *ast, int ident)
{
    if (ast == nullptr) return;

    switch (ast->ast_type) {
    case AST_CONSTANT_NUMBER: {
        const ConstantNumberAST *c = (const ConstantNumberAST *)ast;
        printf("%*sConstNumAST type: %s", ident, "", BasicTypeToStr(c->type));
        switch (c->type)
        {
        case BASIC_TYPE_F32:
            printf(" %f", c->pl.pf32);
            break;
        case BASIC_TYPE_F64:
            printf(" %lf", c->pl.pf64);
            break;
        case BASIC_TYPE_U32:
            printf(" %d", c->pl.pu32);
            break;
        case BASIC_TYPE_U64:
            printf(" %lld", c->pl.pu64);
            break;
        }
        printf("\n");
        break;
    }
    case AST_BINARY_OPERATION: {
        const BinaryOperationAST *b = (const BinaryOperationAST *)ast;
        printf("%*sBinOpAST op: %s\n", ident, "", TokenTypeToStr(b->op));
        printf("%*s LHS:\n", ident, "");
        printAST(b->lhs, ident + 3);
        printf("%*s RHS:\n", ident, "");
        printAST(b->rhs, ident + 3);
        break;
    }
    case AST_ASSIGNMENT: {
        const AssignmentAST *a = (const AssignmentAST *)ast;
        printf("%*sAssignAST op: %s\n", ident, "", TokenTypeToStr(a->op));
        printf("%*s LHS:\n", ident, "");
        printAST(a->lhs, ident + 3);
        printf("%*s RHS:\n", ident, "");
        printAST(a->rhs, ident + 3);
        break;
    }
    case AST_DECLARATION: {
        const DeclarationAST *a = (const DeclarationAST *)ast;
        printf("%*sDeclAST varname: [%s] flags: %x\n", ident, "", a->varname,
            a->flags);
        printf("%*s SpecifiedType: ", ident, "");
        if (a->specified_type) {
            printf("\n");
            printAST(a->specified_type, ident + 3);
        } else {
            printf(" NONE\n");
        }
        printf("%*s DefinitionAST: ", ident, "");
        if (a->definition) {
            printf("\n");
            printAST(a->definition, ident + 3);
        } else {
            printf(" NONE\n");
        }
        break;
    }
    case AST_DIRECT_TYPE: {
        const DirectTypeAST *a = (const DirectTypeAST *)ast;
        printf("%*sDirectTypeAST name: [%s]", ident, "", a->name);
        printf(" type: %s\n", BasicTypeToStr(a->type));
        break;
    }
    case AST_ARGUMENT_DECLARATION: {
        const ArgumentDeclarationAST *a = (const ArgumentDeclarationAST *)ast;
        printf("%*sArgumentDeclarationAST name: %s\n", ident, "", a->name);
        printAST(a->type, ident + 3);
        break;
    }
    case AST_FUNCTION_DECLARATION: {
        const FunctionDeclarationAST *a = (const FunctionDeclarationAST *)ast;
        printf("%*sFunctionDeclarationAST with %d arguments\n", ident, "", (int)a->arguments.size());
        for (const auto & arg : a->arguments) printAST(arg, ident + 3);
        if (a->return_type) {
            printf(" and return type:\n");
            printAST(a->return_type, ident + 3);
        } else {
            printf(" and no return type, void inferred\n");
        }
        break;
    }
    case AST_STATEMENT_BLOCK: {
        const StatementBlockAST* a = (const StatementBlockAST *)ast;
        printf("%*sStatementBlockAST with %d statements\n", ident, "", (int)a->statements.size());
        for (const auto & stmt : a->statements) printAST(stmt, ident + 3);
        break;
    }
    case AST_RETURN_STATEMENT: {
        const ReturnStatementAST *a = (const ReturnStatementAST *)ast;
        printf("%*sReturnStatementAST\n", ident, "");
        printAST(a->ret, ident + 3);
        break;
    }
    case AST_FUNCTION_DEFINITION: {
        const FunctionDefinitionAST *a = (const FunctionDefinitionAST *)ast;
        printf("%*sFunctionDefinitionAST\n", ident, "");
        printAST(a->declaration, ident + 3);
        printAST(a->function_body, ident + 3);
        break;
    }
    case AST_IDENTIFIER: {
        const IdentifierAST *a = (const IdentifierAST *)ast;
        printf("%*sIdentifierAST name: [%s]\n", ident, "", a->name);
        break;
    }
    case AST_CONSTANT_STRING: {
        const ConstantStringAST *a = (const ConstantStringAST *)ast;
        printf("%*sConstantStringAST name: [%s]\n", ident, "", a->str);
        break;
    }
    case AST_FUNCTION_CALL: {
        const FunctionCallAST *a = (const FunctionCallAST *)ast;
        printf("%*sFunctionCall: %s with %d arguments\n", ident, "", a->function_name, (int)a->args.size());
        for (auto arg : a->args) printAST(arg, ident + 3);
        break;
    }
    case AST_FILE: {
        const FileAST *a = (const FileAST *)ast;
        for (const auto &it : a->items) printAST(it, ident);
        break;
    }
    default : 
        printf("%*sUnknown AST type\n", ident, "");
    }
}

