#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"
#include "parser.h"
#define MAX_SYMBOLS 256
#define MAX_SCOPES 64
#define MAX_PARAMS 32

typedef enum{ SYM_VAR, SYM_FUNC } SymbolKind;

typedef struct{
    char name[256];
    SymbolKind kind;
    SemanticType type;
    int param_count;
    SemanticType param_types[MAX_PARAMS];
} Symbol;

typedef struct{
    Symbol symbols[MAX_SYMBOLS];
    int count;
} Scope;

static Scope scope_stack[MAX_SCOPES];
static int scope_depth = 0;
static void scope_push(void){
    scope_depth++;
    scope_stack[scope_depth].count = 0;
}

static void scope_pop(void){
    if(scope_depth > 0) scope_depth--;
}

static Symbol *scope_lookup(const char *name){
    for(int s = scope_depth; s >= 0; s--){
        Scope *sc = &scope_stack[s];
        for(int i = 0; i < sc->count; i++){
            if(strcmp(sc->symbols[i].name, name) == 0)
                return &sc->symbols[i];
        }
    }
    return NULL;
}

static Symbol *scope_lookup_current(const char *name){
    Scope *sc = &scope_stack[scope_depth];
    for(int i = 0; i < sc->count; i++){
        if(strcmp(sc->symbols[i].name, name) == 0)
            return &sc->symbols[i];
    }
    return NULL;
}

static Symbol *scope_declare(const char *name, SymbolKind kind, SemanticType type){
    Scope *sc = &scope_stack[scope_depth];
    Symbol *s = &sc->symbols[sc->count++];
    strncpy(s->name, name, sizeof(s->name) - 1);
    s->kind = kind;
    s->type = type;
    s->param_count = 0;
    return s;
}

static SemanticType str_to_stype(const char *t){
    if(strcmp(t, "int") == 0) return STYPE_INT;
    if(strcmp(t, "float") == 0) return STYPE_FLOAT;
    if(strcmp(t, "char") == 0) return STYPE_CHAR;
    if(strcmp(t, "void") == 0) return STYPE_VOID;
    if(strcmp(t, "long") == 0) return STYPE_INT;
    if(strcmp(t, "short") == 0) return STYPE_INT;
    if(strcmp(t, "unsigned") == 0) return STYPE_INT;
    return STYPE_UNKNOWN;
}

static const char *stype_name(SemanticType t){
    switch(t){
        case STYPE_INT:    return "int";
        case STYPE_FLOAT:  return "float";
        case STYPE_CHAR:   return "char";
        case STYPE_VOID:   return "void";
        case STYPE_STRING: return "string";
        default:           return "desconhecido";
    }
}

static int types_compatible(SemanticType a, SemanticType b){
    if(a == STYPE_UNKNOWN || b == STYPE_UNKNOWN) return 1;
    if(a == b) return 1;
    if(a == STYPE_VOID || b == STYPE_VOID)   return 0;
    if(a == STYPE_STRING || b == STYPE_STRING) return 0;
    return 1;
}

static Token cur;
static Token prev;
static int error_count;
static int sem_error_count;

static SemanticType current_func_type = STYPE_UNKNOWN;
static int has_return = 0;

static FILE *mips_out = NULL;
static char mips_data_vars[MAX_SYMBOLS][256];
static int mips_data_count = 0;
static int label_count = 0;
static int mips_reg_idx = 0;

void parser_set_mips_output(FILE *out){ mips_out = out; }

static void sem_error(int line, int col, const char *msg){
    fprintf(stderr, "[ERRO SEMÂNTICO] Linha %d, Coluna %d: %s\n", line, col, msg);
    sem_error_count++;
}

static ASTNode *ast_new(NodeKind kind, const char *value, int line, int col){
    ASTNode *n = calloc(1, sizeof(ASTNode));
    n->kind = kind;
    n->line = line;
    n->column = col;
    n->stype  = STYPE_UNKNOWN;
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

static void parse_error(const char *msg){
    fprintf(stderr, "[ERRO SINTÁTICO] Linha %d, Coluna %d: %s (token '%s')\n",
            cur.line, cur.column, msg, cur.value);
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

static SemanticType promote(SemanticType a, SemanticType b, int line, int col){
    if(a == STYPE_UNKNOWN || b == STYPE_UNKNOWN) return STYPE_UNKNOWN;
    if(a == STYPE_STRING || b == STYPE_STRING || a == STYPE_VOID || b == STYPE_VOID){
        char msg[512];
        snprintf(msg, sizeof(msg), "operação inválida entre '%s' e '%s'", stype_name(a), stype_name(b));
        sem_error(line, col, msg);
        return STYPE_UNKNOWN;
    }
    if(a == STYPE_FLOAT || b == STYPE_FLOAT) return STYPE_FLOAT;
    if(a == STYPE_INT || b == STYPE_INT) return STYPE_INT;
    if(a == STYPE_CHAR && b == STYPE_CHAR) return STYPE_CHAR;

    char msg[512];
    snprintf(msg, sizeof(msg), "operação inválida entre '%s' e '%s'", stype_name(a), stype_name(b));
    sem_error(line, col, msg);
    return STYPE_UNKNOWN;
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
        n->stype = (strchr(cur.value, '.') != NULL) ? STYPE_FLOAT : STYPE_INT;
        if(mips_out && !error_count && !sem_error_count) {
            fprintf(mips_out, "  li $t%d, %s\n", mips_reg_idx++, cur.value);
        }
        advance(); return n;
    }

    if(check_type(TOKEN_STRING)){
        ASTNode *n = ast_new(NODE_STRING, cur.value, line, col);
        n->stype = STYPE_STRING;
        advance(); return n;
    }

    if(check_type(TOKEN_CHAR)){
        ASTNode *n = ast_new(NODE_CHAR_LIT, cur.value, line, col);
        n->stype = STYPE_CHAR;
        advance(); return n;
    }

    if(check_type(TOKEN_IDENTIFIER)){
        char nome[256];
        strncpy(nome, cur.value, sizeof(nome) - 1);
        int id_line = cur.line, id_col = cur.column;
        advance();

        if(check_val("(")){
            advance();
            ASTNode *call = ast_new(NODE_CALL, nome, id_line, id_col);
            int arg_count = 0;
            SemanticType arg_types[MAX_PARAMS];
            if(!check_val(")")){
                ASTNode *arg = parse_expr();
                if(arg){
                    arg_types[arg_count++] = arg->stype;
                    ast_add_child(call, arg);
                }
                while(match_val(",")){
                    arg = parse_expr();
                    if(arg){
                        if(arg_count < MAX_PARAMS)
                            arg_types[arg_count++] = arg->stype;
                        ast_add_child(call, arg);
                    }
                }
            }
            expect_val(")");

            Symbol *sym = scope_lookup(nome);
            if(!sym){
                char msg[512];
                snprintf(msg, sizeof(msg), "função '%s' não declarada", nome);
                sem_error(id_line, id_col, msg);
                call->stype = STYPE_UNKNOWN;
            } else if(sym->kind != SYM_FUNC){
                char msg[512];
                snprintf(msg, sizeof(msg), "'%s' não é uma função", nome);
                sem_error(id_line, id_col, msg);
                call->stype = STYPE_UNKNOWN;
            }else{
                call->stype = sym->type;
                if(arg_count != sym->param_count){
                    char msg[512];
                    snprintf(msg, sizeof(msg), "função '%s' espera %d argumento(s), mas recebeu %d", nome, sym->param_count, arg_count);
                    sem_error(id_line, id_col, msg);
                }

                int check_n = (arg_count < sym->param_count) ? arg_count : sym->param_count;
                for(int i = 0; i < check_n; i++){
                    if(!types_compatible(arg_types[i], sym->param_types[i])){
                        char msg[512];
                        snprintf(msg, sizeof(msg), "argumento %d de '%s': tipo '%s' incompatível com '%s'", i + 1, nome, stype_name(arg_types[i]), stype_name(sym->param_types[i]));
                        sem_error(id_line, id_col, msg);
                    }
                }
            }
            return call;
        }

        ASTNode *id_node = ast_new(NODE_IDENTIFIER, nome, id_line, id_col);
        Symbol *sym = scope_lookup(nome);
        if(!sym){
            char msg[512];
            snprintf(msg, sizeof(msg), "variável '%s' usada sem ser declarada", nome);
            sem_error(id_line, id_col, msg);
            id_node->stype = STYPE_UNKNOWN;
        }else{
            id_node->stype = sym->type;
            if(mips_out && !error_count && !sem_error_count) {
                fprintf(mips_out, "  lw $t%d, %s\n", mips_reg_idx++, nome);
            }
        }
        return id_node;
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

static ASTNode *parse_termo(void){
    ASTNode *left = parse_fator();
    while(check_val("*") || check_val("/") || check_val("%")){
        char op[4]; strncpy(op, cur.value, 3); op[3] = '\0';
        int line = cur.line, col = cur.column;
        advance();
        ASTNode *right = parse_fator();
        
        if(mips_out && !error_count && !sem_error_count) {
            int r = mips_reg_idx - 1;
            int l = mips_reg_idx - 2;
            if(strcmp(op, "*") == 0) fprintf(mips_out, "  mul $t%d, $t%d, $t%d\n\n", l, l, r);
            else if(strcmp(op, "/") == 0) fprintf(mips_out, "  div $t%d, $t%d\n  mflo $t%d\n\n", l, r, l);
            else if(strcmp(op, "%") == 0) fprintf(mips_out, "  div $t%d, $t%d\n  mfhi $t%d\n\n", l, r, l);
            mips_reg_idx--;
        }

        ASTNode *node = ast_new(NODE_BINOP, op, line, col);
        ast_add_child(node, left);
        ast_add_child(node, right);

        SemanticType lt = left  ? left->stype : STYPE_UNKNOWN;
        SemanticType rt = right ? right->stype : STYPE_UNKNOWN;
        node->stype = promote(lt, rt, line, col);
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
        ASTNode *right = parse_termo();
        
        if(mips_out && !error_count && !sem_error_count) {
            int r = mips_reg_idx - 1;
            int l = mips_reg_idx - 2;
            if(strcmp(op, "+") == 0) fprintf(mips_out, "  add $t%d, $t%d, $t%d\n\n", l, l, r);
            else if(strcmp(op, "-") == 0) fprintf(mips_out, "  sub $t%d, $t%d, $t%d\n\n", l, l, r);
            else if(strcmp(op, "==") == 0) fprintf(mips_out, "  seq $t%d, $t%d, $t%d\n\n", l, l, r);
            else if(strcmp(op, "!=") == 0) fprintf(mips_out, "  sne $t%d, $t%d, $t%d\n\n", l, l, r);
            else if(strcmp(op, "<") == 0) fprintf(mips_out, "  slt $t%d, $t%d, $t%d\n\n", l, l, r);
            else if(strcmp(op, ">") == 0) fprintf(mips_out, "  sgt $t%d, $t%d, $t%d\n\n", l, l, r);
            else if(strcmp(op, "<=") == 0) fprintf(mips_out, "  sle $t%d, $t%d, $t%d\n\n", l, l, r);
            else if(strcmp(op, ">=") == 0) fprintf(mips_out, "  sge $t%d, $t%d, $t%d\n\n", l, l, r);
            mips_reg_idx--;
        }

        ASTNode *node = ast_new(NODE_BINOP, op, line, col);
        ast_add_child(node, left);
        ast_add_child(node, right);
        SemanticType lt = left  ? left->stype : STYPE_UNKNOWN;
        SemanticType rt = right ? right->stype : STYPE_UNKNOWN;
        node->stype = promote(lt, rt, line, col);
        left = node;
    }
    return left;
}

static ASTNode *parse_var_decl(const char *tipo, int line, int col){
    ASTNode *node = ast_new(NODE_VAR_DECL, tipo, line, col);
    SemanticType var_type = str_to_stype(tipo);
    node->stype = var_type;
    if(!check_type(TOKEN_IDENTIFIER)){
        parse_error("esperado identificador após tipo");
        synchronise();
        return node;
    }

    char var_name[256];
    strncpy(var_name, cur.value, sizeof(var_name) - 1);
    var_name[sizeof(var_name) - 1] = '\0';
    int vline = cur.line, vcol = cur.column;

    if(scope_lookup_current(var_name)){
        char msg[512];
        snprintf(msg, sizeof(msg), "variável '%s' já declarada neste escopo", var_name);
        sem_error(vline, vcol, msg);
    }else{
        scope_declare(var_name, SYM_VAR, var_type);
        if(mips_out){
            strncpy(mips_data_vars[mips_data_count++], var_name, 255);
        }
    }

    ast_add_child(node, ast_new(NODE_IDENTIFIER, var_name, vline, vcol));
    advance();

    if(match_val("=")){
        ASTNode *init = parse_expr();
        if(mips_out && !error_count && !sem_error_count && init) {
            fprintf(mips_out, "  sw $t0, %s\n\n", var_name);
            mips_reg_idx = 0;
        }
        if(init){
            if(!types_compatible(var_type, init->stype) &&
               init->stype != STYPE_UNKNOWN){
                char msg[512];
                snprintf(msg, sizeof(msg), "tipo incompatível na inicialização de '%s': " "esperado '%s', obtido '%s'", var_name, stype_name(var_type), stype_name(init->stype));
                sem_error(vline, vcol, msg);
            }
            ast_add_child(node, init);
        }
    }

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
        
        int l_else = ++label_count;
        int l_fim = ++label_count;
        if(mips_out && !error_count && !sem_error_count) {
            fprintf(mips_out, "  beq $t0, $zero, L_else_%d\n", l_else);
            mips_reg_idx = 0;
        }

        ast_add_child(node, parse_statement());
        
        if(mips_out && !error_count && !sem_error_count) {
            fprintf(mips_out, "  j L_fim_%d\n", l_fim);
            fprintf(mips_out, "\nL_else_%d:\n", l_else);
        }
        
        if(check_val("else")){ advance(); ast_add_child(node, parse_statement()); }
        
        if(mips_out && !error_count && !sem_error_count) {
            fprintf(mips_out, "\nL_fim_%d:\n", l_fim);
        }
        return node;
    }

    if(check_val("while")){
        advance();
        ASTNode *node = ast_new(NODE_WHILE, "while", line, col);
        expect_val("(");
        
        int l_start = ++label_count;
        int l_end = ++label_count;
        if(mips_out && !error_count && !sem_error_count) {
            fprintf(mips_out, "\nL_start_%d:\n", l_start);
        }
        
        ast_add_child(node, parse_expr());
        expect_val(")");
        
        if(mips_out && !error_count && !sem_error_count) {
            fprintf(mips_out, "  beq $t0, $zero, L_end_%d\n", l_end);
            mips_reg_idx = 0;
        }

        ast_add_child(node, parse_statement());
        
        if(mips_out && !error_count && !sem_error_count) {
            fprintf(mips_out, "  j L_start_%d\n", l_start);
            fprintf(mips_out, "\nL_end_%d:\n", l_end);
        }
        return node;
    }

    if(check_val("for")){
        advance();
        ASTNode *node = ast_new(NODE_FOR, "for", line, col);
        expect_val("(");

        scope_push();
        if(is_type()){
            char tipo[64]; strncpy(tipo, cur.value, 63); advance();
            ast_add_child(node, parse_var_decl(tipo, line, col));
        } else if(check_val(";")){
            advance();
        }else{
            ASTNode *s = ast_new(NODE_EXPR_STMT, "", line, col);
            ast_add_child(s, parse_expr());
            ast_add_child(node, s);
            expect_val(";");
        }

        if(!check_val(";")) ast_add_child(node, parse_expr());
        expect_val(";");

        if(!check_val(")")) ast_add_child(node, parse_expr());
        expect_val(")");
        ast_add_child(node, parse_statement());
        scope_pop();
        return node;
    }

    if(check_val("return")){
        advance();
        ASTNode *node = ast_new(NODE_RETURN, "return", line, col);
        has_return = 1;

        if(!check_val(";")){
            ASTNode *ret_expr = parse_expr();
            if(ret_expr){
                node->stype = ret_expr->stype;
                if(current_func_type == STYPE_VOID){
                    sem_error(line, col, "função void não pode retornar valor");
                }else if(!types_compatible(current_func_type, ret_expr->stype) && ret_expr->stype != STYPE_UNKNOWN){
                    char msg[512];
                    snprintf(msg, sizeof(msg), "tipo de retorno incompatível: esperado '%s', obtido '%s'", stype_name(current_func_type), stype_name(ret_expr->stype));
                    sem_error(line, col, msg);
                }
                ast_add_child(node, ret_expr);
            }
        }else{
            node->stype = STYPE_VOID;
            if(current_func_type != STYPE_VOID && current_func_type != STYPE_UNKNOWN){
                char msg[512];
                snprintf(msg, sizeof(msg), "função deve retornar valor do tipo '%s'", stype_name(current_func_type));
                sem_error(line, col, msg);
            }
        }
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
            int eq_line = cur.line, eq_col = cur.column;
            advance();
            ASTNode *rside  = parse_expr();
            
            if(mips_out && !error_count && !sem_error_count) {
                fprintf(mips_out, "  sw $t0, %s\n\n", nome);
                mips_reg_idx = 0;
            }
            
            ASTNode *node = ast_new(NODE_ASSIGN, "=", line, col);
            Symbol *sym = scope_lookup(nome);
            ASTNode *lside = ast_new(NODE_IDENTIFIER, nome, line, col);
            if(!sym){
                char msg[512];
                snprintf(msg, sizeof(msg), "variável '%s' usada sem ser declarada", nome);
                sem_error(line, col, msg);
                lside->stype = STYPE_UNKNOWN;
            }else{
                lside->stype = sym->type;
                if(rside && rside->stype != STYPE_UNKNOWN &&
                   !types_compatible(sym->type, rside->stype)){
                    char msg[512];
                    snprintf(msg, sizeof(msg), "atribuição incompatível para '%s': " "esperado '%s', obtido '%s'", nome, stype_name(sym->type), stype_name(rside->stype));
                    sem_error(eq_line, eq_col, msg);
                }
            }
            ast_add_child(node, lside);
            ast_add_child(node, rside);
            expect_val(";");
            return node;
        }

        if(check_val("(")){
            advance();
            ASTNode *call = ast_new(NODE_CALL, nome, line, col);
            int arg_count = 0;
            SemanticType arg_types[MAX_PARAMS];
            if(!check_val(")")){
                ASTNode *arg = parse_expr();
                if(arg){
                    arg_types[arg_count++] = arg->stype;
                    ast_add_child(call, arg);
                }
                while(match_val(",")){
                    arg = parse_expr();
                    if(arg){
                        if(arg_count < MAX_PARAMS)
                            arg_types[arg_count++] = arg->stype;
                        ast_add_child(call, arg);
                    }
                }
            }
            expect_val(")");

            Symbol *sym = scope_lookup(nome);
            if(!sym){
                char msg[512];
                snprintf(msg, sizeof(msg), "função '%s' não declarada", nome);
                sem_error(line, col, msg);
            } else if(sym->kind != SYM_FUNC){
                char msg[512];
                snprintf(msg, sizeof(msg), "'%s' não é uma função", nome);
                sem_error(line, col, msg);
            }else{
                call->stype = sym->type;
                if(arg_count != sym->param_count){
                    char msg[512];
                    snprintf(msg, sizeof(msg), "função '%s' espera %d argumento(s), mas recebeu %d", nome, sym->param_count, arg_count);
                    sem_error(line, col, msg);
                }
                int check_n = (arg_count < sym->param_count) ? arg_count : sym->param_count;
                for(int i = 0; i < check_n; i++){
                    if(!types_compatible(arg_types[i], sym->param_types[i])){
                        char msg[512];
                        snprintf(msg, sizeof(msg), "argumento %d de '%s': tipo '%s' incompatível com '%s'", i + 1, nome, stype_name(arg_types[i]), stype_name(sym->param_types[i]));
                        sem_error(line, col, msg);
                    }
                }
            }

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
    scope_push();
    expect_val("{");
    while(!check_val("}") && !check_type(TOKEN_EOF)){
        if(check_type(TOKEN_ERROR)){ advance(); continue; }
        ASTNode *s = parse_statement();
        if(s) ast_add_child(node, s);
        else synchronise();
    }
    expect_val("}");
    scope_pop();
    return node;
}

static ASTNode *parse_func_decl(const char *tipo, const char *nome, int line, int col){
    ASTNode *node = ast_new(NODE_FUNC_DECL, nome, line, col);
    SemanticType ret_type = str_to_stype(tipo);
    node->stype = ret_type;
    ast_add_child(node, ast_new(NODE_IDENTIFIER, tipo, line, col));

    Symbol *fsym = scope_lookup_current(nome);
    if(fsym){
        char msg[512];
        snprintf(msg, sizeof(msg), "função '%s' já declarada", nome);
        sem_error(line, col, msg);
    }else{
        fsym = scope_declare(nome, SYM_FUNC, ret_type);
    }

    expect_val("(");
    scope_push();

    ASTNode *params = ast_new(NODE_PROGRAM, "params", cur.line, cur.column);
    ast_add_child(node, params);

    if(!check_val(")")){
        do {
            if(!is_type()){
                parse_error("esperado tipo do parâmetro");
                while(!check_val(")") && !check_val("{") && !check_type(TOKEN_EOF))
                    advance();
                break;
            }
            char ptipo[64]; strncpy(ptipo, cur.value, 63); advance();
            SemanticType ptype = str_to_stype(ptipo);
            const char *pname = (cur.type == TOKEN_IDENTIFIER) ? cur.value : "(sem nome)";
            int pline = cur.line, pcol = cur.column;

            if(cur.type == TOKEN_IDENTIFIER){
                if(scope_lookup_current(pname)){
                    char msg[512];
                    snprintf(msg, sizeof(msg), "parâmetro '%s' duplicado", pname);
                    sem_error(pline, pcol, msg);
                }else{
                    scope_declare(pname, SYM_VAR, ptype);
                }
            }

            if(fsym && fsym->param_count < MAX_PARAMS){
                fsym->param_types[fsym->param_count++] = ptype;
            }

            ASTNode *param = ast_new(NODE_PARAM, pname, pline, pcol);
            ast_add_child(param, ast_new(NODE_IDENTIFIER, ptipo, pline, pcol));
            if(cur.type == TOKEN_IDENTIFIER) advance();
            ast_add_child(params, param);

        }while(match_val(","));
    }
    expect_val(")");

    SemanticType saved_ret = current_func_type;
    int saved_has = has_return;
    current_func_type = ret_type;
    has_return = 0;

    if(check_val(";")){
        advance();
        scope_pop();
    }else{
        int bline = cur.line, bcol = cur.column;
        ASTNode *body = ast_new(NODE_BLOCK, "block", bline, bcol);
        expect_val("{");
        while(!check_val("}") && !check_type(TOKEN_EOF)){
            if(check_type(TOKEN_ERROR)){ advance(); continue; }
            ASTNode *s = parse_statement();
            if(s) ast_add_child(body, s);
            else synchronise();
        }
        expect_val("}");
        scope_pop();
        ast_add_child(node, body);

        if(ret_type != STYPE_VOID && ret_type != STYPE_UNKNOWN && !has_return){
            char msg[512];
            snprintf(msg, sizeof(msg), "função '%s' deve retornar '%s' mas não tem return", nome, stype_name(ret_type));
            sem_error(line, col, msg);
        }
    }

    current_func_type = saved_ret;
    has_return = saved_has;
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

    SemanticType gtype = str_to_stype(tipo);
    if(scope_lookup_current(nome)){
        char msg[512];
        snprintf(msg, sizeof(msg), "variável global '%s' já declarada", nome);
        sem_error(line, col, msg);
    }else{
        scope_declare(nome, SYM_VAR, gtype);
        if(mips_out){
            strncpy(mips_data_vars[mips_data_count++], nome, 255);
        }
    }

    ASTNode *node = ast_new(NODE_VAR_DECL, nome, line, col);
    node->stype = gtype;
    ast_add_child(node, ast_new(NODE_IDENTIFIER, tipo, line, col));
    if(match_val("=")){
        ASTNode *init = parse_expr();
        if(mips_out && !error_count && !sem_error_count && init) {
            fprintf(mips_out, "  sw $t0, %s\n\n", nome);
            mips_reg_idx = 0;
        }
        if(init && init->stype != STYPE_UNKNOWN && !types_compatible(gtype, init->stype)){
            char msg[512];
            snprintf(msg, sizeof(msg), "tipo incompatível na inicialização de '%s': " "esperado '%s', obtido '%s'", nome, stype_name(gtype), stype_name(init->stype));
            sem_error(line, col, msg);
        }
        ast_add_child(node, init);
    }
    expect_val(";");
    return node;
}

static ASTNode *parse_program(void){
    ASTNode *root = ast_new(NODE_PROGRAM, "program", 1, 1);
    
    if(mips_out){
        fprintf(mips_out, ".text\n.globl main\nmain:\n");
    }
    
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
    
    if(mips_out && !error_count && !sem_error_count){
        fprintf(mips_out, "  li $v0, 10\n  syscall\n");
        if(mips_data_count > 0){
            fprintf(mips_out, "\n.data\n");
            for(int i = 0; i < mips_data_count; i++){
                fprintf(mips_out, "%s: .word 0\n", mips_data_vars[i]);
            }
        }
    }
    
    return root;
}

void parser_init(void){
    error_count = 0;
    sem_error_count = 0;
    scope_depth = 0;
    scope_stack[0].count = 0;
    current_func_type = STYPE_UNKNOWN;
    has_return = 0;
    advance();
}

ASTNode *parser_parse(void){
    return parse_program();
}

int parser_error_count(void){ return error_count; }
int semantic_error_count(void){ return sem_error_count; }

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
    if(node->stype != STYPE_UNKNOWN)
        printf(" :%s", stype_name(node->stype));
    printf("  <%d:%d>\n", node->line, node->column);
    for(int i = 0; i < node->num_children; i++)
        ast_print(node->children[i], depth + 1);
}