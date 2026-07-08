#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"
#include "parser.h"

static char *read_file(const char *path){
    FILE *f = fopen(path, "r");
    if(!f){ perror("Erro ao abrir arquivo"); return NULL; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    char *src = malloc(size + 1);
    if(!src){ fprintf(stderr, "Erro de memória\n"); fclose(f); return NULL; }
    fread(src, 1, size, f);
    src[size] = '\0';
    fclose(f);
    return src;
}

int main(int argc, char *argv[]){
    int print_tokens = 0;
    int print_ast = 1;
    const char *filename = NULL;

    for(int i = 1; i < argc; i++){
        if(strcmp(argv[i], "--tokens") == 0) print_tokens = 1;
        else if(strcmp(argv[i], "--no-ast") == 0) print_ast = 0;
        else filename = argv[i];
    }

    if(!filename){
        fprintf(stderr, "Uso: %s [--tokens] [--no-ast] <arquivo.c>\n", argv[0]);
        return 1;
    }

    char *source = read_file(filename);
    if(!source) return 1;

    if(print_tokens){
        printf("═══════════════ TOKENS ═══════════════\n");
        printf("%-10s %-22s %s\n", "L/C", "TIPO", "VALOR");
        printf("%-10s %-22s %s\n", "---", "----", "-----");
        lexer_init(source);
        Token tok;
        while((tok = lexer_next_token()).type != TOKEN_EOF){
            char lc[16];
            snprintf(lc, sizeof(lc), "%d/%d", tok.line, tok.column);
            printf("%-10s %-22s %s\n", lc, token_type_name(tok.type), tok.value);
        }
        printf("\n");
    }

    printf("═══════════════ ERROS ═══════════════\n");
    lexer_init(source);
    parser_init();
    
    FILE *mips_out = fopen("saida.txt", "w");
    if(mips_out) {
        parser_set_mips_output(mips_out);
    }
    
    ASTNode *ast = parser_parse();

    int syn_errs = parser_error_count();
    int sem_errs = semantic_error_count();

    if(syn_errs == 0)
        printf("Sem erros sintáticos.\n");
    else
        printf("%d erro(s) sintático(s) encontrado(s).\n", syn_errs);

    printf("\n═══════════════ ANÁLISE SEMÂNTICA ═══════════════\n");
    if(sem_errs == 0)
        printf("Sem erros semânticos.\n");
    else
        printf("%d erro(s) semântico(s) encontrado(s).\n", sem_errs);

    if(print_ast && ast){
        printf("\n═══════════════ AST ═══════════════\n");
        ast_print(ast, 0);
    }

    if(mips_out){
        fclose(mips_out);
        if(syn_errs == 0 && sem_errs == 0){
            printf("\nCódigo MIPS gerado com sucesso em 'saida.txt'\n");
        } else {
            printf("\nErro na compilação. Código MIPS gerado pode estar incompleto.\n");
        }
    }

    ast_free(ast);
    free(source);
    return(syn_errs > 0 || sem_errs > 0) ? 1 : 0;
}