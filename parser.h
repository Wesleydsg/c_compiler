#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"

typedef enum {
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

typedef struct ASTNode {
    NodeKind kind;
    char value[256];
    int line;
    int column;
    struct ASTNode *children[AST_MAX_CHILDREN];
    int num_children;
} ASTNode;

void parser_init(void);
ASTNode *parser_parse(void);
void ast_print(const ASTNode *node, int depth);
void ast_free(ASTNode *node);
int parser_error_count(void);

#endif