#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"
#include "parser.h"

static Token cur;
static Token prev;
static int error_count;

static ASTNode *ast_new(NodeKind kind, const char *value, int line, int col){
    ASTNode *n = calloc(1, sizeof(ASTNode));
    n->kind = kind;
    n->line = line;
    n->column = col;
    if(value) strncpy(n->value, value, sizeof(n->value) - 1);
    return n;
}

static void ast_add_child(ASTNode *parent, ASTNode *child){
    if(!child) return;
    if(parent->num_children >= AST_MAX_CHILDREN) return;
    parent->children[parent->num_children++] = child;
}

void ast_free(ASTNode *node){
    if(!node) return;
    for(int i = 0; i < node->num_children; i++)
        ast_free(node->children[i]);
    free(node);
}

static void advance(void){
    prev = cur;
    cur = lexer_next_token();
}

static int check_val(const char *v){ return strcmp(cur.value, v) == 0; }
static int check_type(TokenType t){ return cur.type == t; }

static int match_val(const char *v){
    if(check_val(v)){ advance(); return 1; }
    return 0;
}
static int match_type(TokenType t){
    if(check_type(t)){ advance(); return 1; }
    return 0;
}

static void parse_error(const char *msg){
    fprintf(stderr, "[ERRO SINTÁTICO] Linha %d, Coluna %d: %s(token '%s')\n", cur.line, cur.column, msg, cur.value);
    error_count++;
}

static int expect_val(const char *v){
    if(match_val(v)) return 1;
    char msg[128];
    snprintf(msg, sizeof(msg), "esperado '%s'", v);
    parse_error(msg);
    return 0;
}

static void synchronise(void){
    while(cur.type != TOKEN_EOF){
        if(check_val(";")){ advance(); return; }
        if(check_val("}")) return;
        if(cur.type == TOKEN_RESERVED_WORD) return;
        advance();
    }
}

static int is_type(void){
    if(cur.type != TOKEN_RESERVED_WORD) return 0;
    return(strcmp(cur.value, "int") == 0 ||
            strcmp(cur.value, "char") == 0 ||
            strcmp(cur.value, "float") == 0 ||
            strcmp(cur.value, "void") == 0 ||
            strcmp(cur.value, "long") == 0 ||
            strcmp(cur.value, "short") == 0 ||
            strcmp(cur.value, "unsigned") == 0);
}

static ASTNode *parse_statement(void);
static ASTNode *parse_block(void);
static ASTNode *parse_expr(void);

static ASTNode *parse_fator(void){
    int line = cur.line, col = cur.column;
    if(check_type(TOKEN_ERROR)){
        advance();
        return NULL;
    }

    if(check_type(TOKEN_NUMBER)){
        ASTNode *n = ast_new(NODE_NUMBER, cur.value, line, col);
        advance(); return n;
    }
    if(check_type(TOKEN_STRING)){
        ASTNode *n = ast_new(NODE_STRING, cur.value, line, col);
        advance(); return n;
    }
    if(check_type(TOKEN_CHAR)){
        ASTNode *n = ast_new(NODE_CHAR_LIT, cur.value, line, col);
        advance(); return n;
    }
    if(check_type(TOKEN_IDENTIFIER)){
        char nome[256];
        strncpy(nome, cur.value, sizeof(nome) - 1);
        advance();

        if(check_val("(")){
            advance();
            ASTNode *call = ast_new(NODE_CALL, nome, line, col);
            if(!check_val(")")){
                ast_add_child(call, parse_expr());
                while(match_val(","))
                    ast_add_child(call, parse_expr());
            }
            expect_val(")");
            return call;
        }
        return ast_new(NODE_IDENTIFIER, nome, line, col);
    }
    if(check_val("(")){
        advance();
        ASTNode *n = parse_expr();
        expect_val(")");
        return n;
    }

    parse_error("esperado número, identificador ou '('");
    advance();
    return NULL;
}

//termo: fator {('*' | '/' | '%') fator }
static ASTNode *parse_termo(void){
    ASTNode *left = parse_fator();
    while(check_val("*") || check_val("/") || check_val("%")){
        char op[4]; strncpy(op, cur.value, 3); op[3] = '\0';
        int line = cur.line, col = cur.column;
        advance();
        ASTNode *node = ast_new(NODE_BINOP, op, line, col);
        ast_add_child(node, left);
        ast_add_child(node, parse_fator());
        left = node;
    }
    return left;
}

static ASTNode *parse_expr(void){
    ASTNode *left = parse_termo();
    while(check_type(TOKEN_ARITHMETIC_OPERATOR) || check_type(TOKEN_LOGIC_OPERATOR)){
        char op[8]; strncpy(op, cur.value, 7); op[7] = '\0';
        int line = cur.line, col = cur.column;
        advance();
        ASTNode *node = ast_new(NODE_BINOP, op, line, col);
        ast_add_child(node, left);
        ast_add_child(node, parse_termo());
        left = node;
    }
    return left;
}

static ASTNode *parse_var_decl(const char *tipo, int line, int col){
    ASTNode *node = ast_new(NODE_VAR_DECL, tipo, line, col);
    if(!check_type(TOKEN_IDENTIFIER)){
        parse_error("esperado identificador após tipo");
        synchronise();
        return node;
    }
    ast_add_child(node, ast_new(NODE_IDENTIFIER, cur.value, cur.line, cur.column));
    advance();

    if(match_val("="))
        ast_add_child(node, parse_expr());

    expect_val(";");
    return node;
}

static ASTNode *parse_statement(void){
    int line = cur.line, col = cur.column;
    if(check_val("{"))
        return parse_block();

    if(check_val("if")){
        advance();
        ASTNode *node = ast_new(NODE_IF, "if", line, col);
        expect_val("(");
        ast_add_child(node, parse_expr());
        expect_val(")");
        ast_add_child(node, parse_statement());
        if(check_val("else")){ advance(); ast_add_child(node, parse_statement()); }
        return node;
    }

    if(check_val("while")){
        advance();
        ASTNode *node = ast_new(NODE_WHILE, "while", line, col);
        expect_val("(");
        ast_add_child(node, parse_expr());
        expect_val(")");
        ast_add_child(node, parse_statement());
        return node;
    }

    if(check_val("for")){
        advance();
        ASTNode *node = ast_new(NODE_FOR, "for", line, col);
        expect_val("(");
        if(is_type()){
            char tipo[64]; strncpy(tipo, cur.value, 63); advance();
            ast_add_child(node, parse_var_decl(tipo, line, col));
        } else if(check_val(";")){
            advance();
        } else {
            ASTNode *s = ast_new(NODE_EXPR_STMT, "", line, col);
            ast_add_child(s, parse_expr()); ast_add_child(node, s);
            expect_val(";");
        }
        //condição
        if(!check_val(";")) ast_add_child(node, parse_expr());
        expect_val(";");
        //passo
        if(!check_val(")")) ast_add_child(node, parse_expr());
        expect_val(")");
        ast_add_child(node, parse_statement());
        return node;
    }

    if(check_val("return")){
        advance();
        ASTNode *node = ast_new(NODE_RETURN, "return", line, col);
        if(!check_val(";")) ast_add_child(node, parse_expr());
        expect_val(";");
        return node;
    }

    if(check_val("break")){
        advance(); expect_val(";");
        return ast_new(NODE_BREAK, "break", line, col);
    }

    if(check_val("continue")){
        advance(); expect_val(";");
        return ast_new(NODE_CONTINUE, "continue", line, col);
    }

    if(is_type()){
        char tipo[64]; strncpy(tipo, cur.value, 63); advance();
        return parse_var_decl(tipo, line, col);
    }

    if(check_type(TOKEN_IDENTIFIER)){
        char nome[256]; strncpy(nome, cur.value, 255); advance();
        if(check_val("=")){
            advance();
            ASTNode *node = ast_new(NODE_ASSIGN, "=", line, col);
            ast_add_child(node, ast_new(NODE_IDENTIFIER, nome, line, col));
            ast_add_child(node, parse_expr());
            expect_val(";");
            return node;
        }

        if(check_val("(")){
            advance();
            ASTNode *call = ast_new(NODE_CALL, nome, line, col);
            if(!check_val(")")){
                ast_add_child(call, parse_expr());
                while(match_val(",")) ast_add_child(call, parse_expr());
            }
            expect_val(")");
            ASTNode *stmt = ast_new(NODE_EXPR_STMT, "", line, col);
            ast_add_child(stmt, call);
            expect_val(";");
            return stmt;
        }

        parse_error("esperado '=' ou '(' após identificador");
        synchronise();
        return NULL;
    }

    parse_error("comando inválido");
    synchronise();
    return NULL;
}

static ASTNode *parse_block(void){
    int line = cur.line, col = cur.column;
    ASTNode *node = ast_new(NODE_BLOCK, "block", line, col);
    expect_val("{");
    while(!check_val("}") && !check_type(TOKEN_EOF)){
        if(check_type(TOKEN_ERROR)){ advance(); continue; }
        ASTNode *s = parse_statement();
        if(s) ast_add_child(node, s);
        else synchronise();
    }
    expect_val("}");
    return node;
}

static ASTNode *parse_func_decl(const char *tipo, const char *nome, int line, int col){
    ASTNode *node = ast_new(NODE_FUNC_DECL, nome, line, col);
    ast_add_child(node, ast_new(NODE_IDENTIFIER, tipo, line, col));
    expect_val("(");
    ASTNode *params = ast_new(NODE_PROGRAM, "params", cur.line, cur.column);
    ast_add_child(node, params);

    if(!check_val(")")){
        do{
            if(!is_type()){
                parse_error("esperado tipo do parâmetro");
                // pula até o ')' para evitar erros em cascata
                while(!check_val(")") && !check_val("{") && !check_type(TOKEN_EOF))
                    advance();
                break;
            }
            char ptipo[64]; strncpy(ptipo, cur.value, 63); advance();
            ASTNode *param = ast_new(NODE_PARAM, cur.type == TOKEN_IDENTIFIER ? cur.value : "(sem nome)", cur.line, cur.column);
            ast_add_child(param, ast_new(NODE_IDENTIFIER, ptipo, cur.line, cur.column));
            if(check_type(TOKEN_IDENTIFIER)) advance();
            ast_add_child(params, param);
        } while(match_val(","));
    }
    expect_val(")");

    if(check_val(";")){ advance(); }
    else ast_add_child(node, parse_block());

    return node;
}

static ASTNode *parse_decl(void){
    if(!is_type()){
        parse_error("esperado tipo");
        synchronise();
        return NULL;
    }
    char tipo[64]; strncpy(tipo, cur.value, 63);
    int line = cur.line, col = cur.column;
    advance();

    if(!check_type(TOKEN_IDENTIFIER)){
        parse_error("esperado identificador após tipo");
        synchronise();
        return NULL;
    }
    char nome[256]; strncpy(nome, cur.value, 255);
    advance();

    if(check_val("("))
        return parse_func_decl(tipo, nome, line, col);

    //variável global
    ASTNode *node = ast_new(NODE_VAR_DECL, nome, line, col);
    ast_add_child(node, ast_new(NODE_IDENTIFIER, tipo, line, col));
    if(match_val("=")) ast_add_child(node, parse_expr());
    expect_val(";");
    return node;
}

static ASTNode *parse_program(void){
    ASTNode *root = ast_new(NODE_PROGRAM, "program", 1, 1);
    while(!check_type(TOKEN_EOF)){
        if(check_type(TOKEN_ERROR)){ advance(); continue; }
        if(cur.type == TOKEN_UNKNOWN && cur.value[0] == '#'){
            int ln = cur.line;
            while(!check_type(TOKEN_EOF) && cur.line == ln) advance();
            continue;
        }
        ASTNode *d = parse_decl();
        if(d) ast_add_child(root, d);
        else synchronise();
    }
    return root;
}

void parser_init(void){
    error_count = 0;
    advance();
}

ASTNode *parser_parse(void){
    return parse_program();
}

int parser_error_count(void){ return error_count; }

static const char *kind_name(NodeKind k){
    switch(k){
        case NODE_PROGRAM:   return "PROGRAM";
        case NODE_FUNC_DECL: return "FUNC_DECL";
        case NODE_VAR_DECL:  return "VAR_DECL";
        case NODE_PARAM:     return "PARAM";
        case NODE_BLOCK:     return "BLOCK";
        case NODE_IF:        return "IF";
        case NODE_WHILE:     return "WHILE";
        case NODE_FOR:       return "FOR";
        case NODE_RETURN:    return "RETURN";
        case NODE_BREAK:     return "BREAK";
        case NODE_CONTINUE:  return "CONTINUE";
        case NODE_EXPR_STMT: return "EXPR_STMT";
        case NODE_ASSIGN:    return "ASSIGN";
        case NODE_BINOP:     return "BINOP";
        case NODE_CALL:      return "CALL";
        case NODE_IDENTIFIER:return "IDENTIFIER";
        case NODE_NUMBER:    return "NUMBER";
        case NODE_STRING:    return "STRING";
        case NODE_CHAR_LIT:  return "CHAR_LIT";
        default:             return "???";
    }
}


void ast_print(const ASTNode *node, int depth){
    if(!node) return;
    for(int i = 0; i < depth; i++) printf("  ");
    printf("[%s]", kind_name(node->kind));
    if(node->value[0]) printf(" \"%s\"", node->value);
    printf("  <%d:%d>\n", node->line, node->column);
    for(int i = 0; i < node->num_children; i++)
        ast_print(node->children[i], depth + 1);
}