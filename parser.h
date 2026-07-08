#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"

typedef enum{
    STYPE_UNKNOWN = 0,
    STYPE_INT,
    STYPE_FLOAT,
    STYPE_CHAR,
    STYPE_VOID,
    STYPE_STRING,
} SemanticType;

typedef enum{
    NODE_PROGRAM,
    NODE_FUNC_DECL,
    NODE_VAR_DECL,
    NODE_PARAM,

    NODE_BLOCK,
    NODE_IF,
    NODE_WHILE,
    NODE_FOR,
    NODE_RETURN,
    NODE_BREAK,
    NODE_CONTINUE,
    NODE_EXPR_STMT,

    NODE_ASSIGN,
    NODE_BINOP,
    NODE_CALL,

    NODE_IDENTIFIER,
    NODE_NUMBER,
    NODE_STRING,
    NODE_CHAR_LIT,
} NodeKind;

#define AST_MAX_CHILDREN 64

typedef struct ASTNode{
    NodeKind kind;
    char value[256];
    int line;
    int column;
    SemanticType stype;
    struct ASTNode *children[AST_MAX_CHILDREN];
    int num_children;
} ASTNode;

#include <stdio.h>

void parser_init(void);
void parser_set_mips_output(FILE *out);
ASTNode *parser_parse(void);
void ast_print(const ASTNode *node, int depth);
void ast_free(ASTNode *node);
int parser_error_count(void);
int semantic_error_count(void);

#endif