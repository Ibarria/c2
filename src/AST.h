#pragma once

#include "Array.h"
#include "mytypes.h"
#include "TokenType.h"
#include "TextType.h"

struct BaseAST;
struct TypeAST;
struct ExpressionAST;
struct VariableDeclarationAST;

struct Scope {
    Scope *parent;
    Array<VariableDeclarationAST *>decls;
};

enum BasicType {
    BASIC_TYPE_BOOL,
    BASIC_TYPE_STRING,
    BASIC_TYPE_INTEGER,
    BASIC_TYPE_FLOATING
};

enum AST_CLASS_TYPE {
    AST_UNKNOWN,
    AST_FILE,
    AST_STATEMENT,
    AST_DEFINITION,
    AST_TYPE,
    AST_ARGUMENT_DECLARATION,
    AST_FUNCTION_TYPE,
    AST_STATEMENT_BLOCK,
    AST_RETURN_STATEMENT,
    AST_FUNCTION_DEFINITION,
    AST_EXPRESSION,
    AST_FUNCTION_CALL,
    AST_DIRECT_TYPE,
    AST_ARRAY_TYPE,
    AST_IDENTIFIER,
    AST_LITERAL,
    AST_BINARY_OPERATION,
    AST_UNARY_OPERATION,
    AST_ASSIGNMENT,
    AST_VARIABLE_DECLARATION,
    AST_RUN_DIRECTIVE
};

struct BaseAST
{
    AST_CLASS_TYPE ast_type;
    BaseAST() { ast_type = AST_UNKNOWN; }
    TextType filename = nullptr;
    Scope *scope = nullptr;
    u32 line_num = 0;
    u32 char_num = 0;
	u64 s = 0; // This is a unique ID for this AST element
};

struct FileAST : BaseAST
{
    FileAST() { ast_type = AST_FILE; }
    Array<BaseAST *>items;
    Scope global_scope;
};

struct StatementAST : BaseAST
{
};

struct DefinitionAST : StatementAST
{
    virtual bool needsSemiColon() const { return true; }
};

struct TypeAST : BaseAST
{
    u32 size_in_bits = 0;
};

struct ArgumentDeclarationAST : BaseAST
{
    ArgumentDeclarationAST() { ast_type = AST_ARGUMENT_DECLARATION; }
    TextType name = nullptr;
    TypeAST *type = nullptr;
};

struct FunctionTypeAST : TypeAST
{
    FunctionTypeAST() { ast_type = AST_FUNCTION_TYPE; size_in_bits = 64; }
    Array<ArgumentDeclarationAST *> arguments;
    TypeAST *return_type = nullptr;
    bool isForeign = false;
    bool hasVariableArguments = false;
};

struct StatementBlockAST : StatementAST
{
    StatementBlockAST() { ast_type = AST_STATEMENT_BLOCK; }
    Array<StatementAST *> statements;
    Scope block_scope;
};

struct ReturnStatementAST: StatementAST
{
    ReturnStatementAST() { ast_type = AST_RETURN_STATEMENT; }
    ExpressionAST *ret = nullptr;
};

struct FunctionDefinitionAST : DefinitionAST
{
    FunctionDefinitionAST() { ast_type = AST_FUNCTION_DEFINITION; }
    FunctionTypeAST *declaration = nullptr;
    StatementBlockAST *function_body = nullptr;
    virtual bool needsSemiColon() const { return false; }
    u32 size_in_bits = 64;
};

struct ExpressionAST : DefinitionAST
{
    TypeAST *expr_type = nullptr;
};

struct RunDirectiveAST : ExpressionAST
{
    RunDirectiveAST() { ast_type = AST_RUN_DIRECTIVE; }
    ExpressionAST *expr = nullptr;
};

struct FunctionCallAST : ExpressionAST
{
    FunctionCallAST() { ast_type = AST_FUNCTION_CALL; }
    Array<ExpressionAST *>args;
    TextType function_name = nullptr;
    FunctionDefinitionAST *fundef = nullptr;
};

struct DirectTypeAST : TypeAST
{
    DirectTypeAST() { ast_type = AST_DIRECT_TYPE; }

	BasicType basic_type;
    bool isSigned = false;
	bool isArray = false;
	bool isPointer = false;
    bool isLiteral = false;
	TextType name = nullptr;
};

struct ArrayTypeAST : TypeAST
{
    ArrayTypeAST() { ast_type = AST_ARRAY_TYPE; }
    TypeAST *contained_type = nullptr;
    u64 num_elems = 0;
    bool isDynamic = false;
};

struct IdentifierAST : ExpressionAST
{
    IdentifierAST() { ast_type = AST_IDENTIFIER; }
    VariableDeclarationAST *decl = nullptr;
    TextType name = nullptr;
};

struct LiteralAST : ExpressionAST
{
    LiteralAST() { ast_type = AST_LITERAL; }
    u64 _u64 = 0;
    s64 _s64 = 0;
    f64 _f64 = 0.0;
    bool _bool = false;
    TextType str = nullptr;
    DirectTypeAST typeAST;
};

struct BinaryOperationAST : ExpressionAST
{
    BinaryOperationAST() { ast_type = AST_BINARY_OPERATION; }
    ExpressionAST *lhs = nullptr;
    ExpressionAST *rhs = nullptr;
    TOKEN_TYPE op = TK_INVALID;
};

struct UnaryOperationAST : ExpressionAST
{
    UnaryOperationAST() { ast_type = AST_UNARY_OPERATION; }
    TOKEN_TYPE op = TK_INVALID;
    ExpressionAST *expr = nullptr;
};

struct AssignmentAST : ExpressionAST
{
    AssignmentAST() { ast_type = AST_ASSIGNMENT; }
    ExpressionAST *lhs = nullptr;
    ExpressionAST *rhs = nullptr;
    TOKEN_TYPE op = TK_INVALID;
};

#define DECL_FLAG_IS_CONSTANT          0x1
#define DECL_FLAG_HAS_BEEN_INFERRED    0x2
#define DECL_FLAG_HAS_BEEN_GENERATED   0x4

struct VariableDeclarationAST : StatementAST
{
    VariableDeclarationAST() { ast_type = AST_VARIABLE_DECLARATION; }
    TextType varname = nullptr;
	TypeAST *specified_type = nullptr;
    DefinitionAST *definition = nullptr;
    u32 flags = 0;
    u64 bc_mem_offset = 0;
};

void printAST(const BaseAST*ast, int ident);
const char *BasicTypeToStr(const DirectTypeAST* t);
bool isFunctionDeclaration(VariableDeclarationAST *decl);