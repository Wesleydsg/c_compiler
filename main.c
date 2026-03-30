#include <stdio.h>
#include <stdlib.h>
#include "lexer.h"

int main(int argc, char *argv[]){
    if(argc < 2){
        fprintf(stderr, "Uso: %s <arquivo.c>\n", argv[0]);
        return 1;
    }

    FILE *f = fopen(argv[1], "r");
    if(!f){
        perror("Erro ao abrir arquivo");
        return 1;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    char *source = malloc(size + 1);
    if(!source){
        fprintf(stderr, "Erro de memória\n");
        fclose(f);
        return 1;
    }

    fread(source, 1, size, f);
    source[size] = '\0';
    fclose(f);

    lexer_init(source);

    printf("%-10s %-20s %s\n", "L/C", "TIPO", "VALOR");
    printf("%-10s %-20s %s\n", "---", "----", "-----");

    Token tok;
    while((tok = lexer_next_token()).type != TOKEN_EOF){
        char lc[16];
        snprintf(lc, sizeof(lc), "%d/%d", tok.line, tok.column);
        printf("%-10s %-20s %s\n", lc, token_type_name(tok.type), tok.value);
    }

    free(source);
    return 0;
}