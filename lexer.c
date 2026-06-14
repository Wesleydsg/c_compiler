#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include "lexer.h"

static const char *cursor;
static int cur_line;
static int cur_column;

static const char *reserved_words[] = {
    "break", "char", "continue",
    "else", "float", "for",
    "if", "int", "long", "return",
    "short", "struct", "typedef",
    "unsigned", "void", "while",
    NULL
};

static int is_reserved(const char *word){
    for(int i = 0; reserved_words[i] != NULL; i++){
        if(strcmp(word, reserved_words[i]) == 0)
            return 1;
    }
    return 0;
}

typedef struct{
    TokenType type;
    const char *pattern;
} Rule;

static const Rule rules[] = {
    {TOKEN_COMMENT,             "^/\\*([^*]|\\*+[^*/])*\\*+/"        },
    {TOKEN_COMMENT,             "^//[^\n]*"                          },
    {TOKEN_STRING,              "^\"([^\"\\\\]|\\\\.)*\""            },
    {TOKEN_CHAR,                "^'([^'\\\\]|\\\\.)*'"               },
    {TOKEN_NUMBER,              "^[0-9]+\\.[0-9]+|^[0-9]+"           },
    {TOKEN_IDENTIFIER,          "^[a-zA-Z_][a-zA-Z0-9_]*"            },
    {TOKEN_ARITHMETIC_OPERATOR, "^(\\+\\+|--|\\+|-|\\*|/|%)"         },
    {TOKEN_LOGIC_OPERATOR,      "^(&&|\\|\\||!=|==|<=|>=|=|!|<|>)"   },
    {TOKEN_SEPARATOR,           "^(\\(|\\)|\\{|\\}|\\[|\\]|;|,|\\.)" },
};

#define NUM_RULES (sizeof(rules) / sizeof(rules[0]))

void lexer_init(const char *source){
    cursor = source;
    cur_line = 1;
    cur_column = 1;
}

Token lexer_next_token(void){
    Token tok;
    tok.value[0] = '\0';

    while(*cursor != '\0'){
        if(*cursor == ' ' || *cursor == '\r'){
            cur_column++;
            cursor++;
            continue;
        }
        if(*cursor == '\t'){
            cur_column += 4;
            cursor++;
            continue;
        }
        if(*cursor == '\n'){
            cur_line++;
            cur_column = 1;
            cursor++;
            continue;
        }
        int matched = 0;
        for(size_t i = 0; i < NUM_RULES; i++){
            regex_t re;
            regmatch_t match;
            if(regcomp(&re, rules[i].pattern, REG_EXTENDED) != 0){
                regfree(&re);
                continue;
            }
            if(regexec(&re, cursor, 1, &match, 0) == 0 && match.rm_so == 0){
                int len = (int)match.rm_eo;
                strncpy(tok.value, cursor, len);
                tok.value[len] = '\0';
                tok.type = rules[i].type;
                tok.line = cur_line;
                tok.column = cur_column;

                if(tok.type == TOKEN_IDENTIFIER && is_reserved(tok.value))
                    tok.type = TOKEN_RESERVED_WORD;

                if(tok.type == TOKEN_NUMBER){
                    const char *after = cursor + len;
                    if(*after == '.' || (*after >= '0' && *after <= '9')){
                        cursor += len;
                        cur_column += len;
                        int blen = (int)strlen(tok.value);
                        while(*cursor != '\0' && *cursor != ' ' && *cursor != '\n' &&
                              *cursor != '\t' && *cursor != ';' && *cursor != ')' &&
                              *cursor != ',' && blen < 254){
                            tok.value[blen++] = *cursor;
                            cursor++; cur_column++;
                        }
                        tok.value[blen] = '\0';
                        tok.type = TOKEN_ERROR;
                        fprintf(stderr, "[ERRO LÉXICO] Linha %d, Coluna %d: numero malformado '%s'\n",
                                tok.line, tok.column, tok.value);
                        regfree(&re);
                        return tok;
                    }
                }

                for(int j = 0; j < len; j++){
                    if(tok.value[j] == '\n'){ cur_line++; cur_column = 1; }
                    else cur_column++;
                }
                cursor += match.rm_eo;
                matched = 1;
                regfree(&re);

                if(tok.type == TOKEN_COMMENT) break;
                return tok;
            }
            regfree(&re);
        }
        if(matched) continue;

        if(*cursor == '"'){
            tok.type   = TOKEN_ERROR;
            tok.line   = cur_line;
            tok.column = cur_column;
            cursor++;
            while(*cursor != '\0' && *cursor != '\n' && *cursor != '"'){
                cursor++; cur_column++;
            }
            if(*cursor == '"') cursor++;
            strncpy(tok.value, "string nao fechada", sizeof(tok.value) - 1);
            fprintf(stderr, "[ERRO LÉXICO] Linha %d, Coluna %d: string nao fechada\n",
                    tok.line, tok.column);
            cur_column++;
            return tok;
        }

        if(*cursor == '@' || *cursor == '$' || *cursor == '`' || *cursor == '?'){
            tok.type      = TOKEN_ERROR;
            tok.line      = cur_line;
            tok.column    = cur_column;
            tok.value[0]  = *cursor;
            tok.value[1]  = '\0';
            fprintf(stderr, "[ERRO LÉXICO] Linha %d, Coluna %d: caractere invalido '%c'\n",
                    cur_line, cur_column, *cursor);
            cursor++; cur_column++;
            return tok;
        }

        tok.type = TOKEN_UNKNOWN;
        tok.value[0] = *cursor;
        tok.value[1] = '\0';
        tok.line = cur_line;
        tok.column = cur_column;
        cursor++;
        cur_column++;
        return tok;
    }

    tok.type = TOKEN_EOF;
    tok.value[0] = '\0';
    tok.line = cur_line;
    tok.column = cur_column;
    return tok;
}

const char *token_type_name(TokenType type){
    switch(type){
        case TOKEN_IDENTIFIER:          return "IDENTIFIER";
        case TOKEN_NUMBER:              return "NUMBER";
        case TOKEN_STRING:              return "STRING";
        case TOKEN_CHAR:                return "CHAR";
        case TOKEN_RESERVED_WORD:       return "RESERVED_WORD";
        case TOKEN_ARITHMETIC_OPERATOR: return "ARITHMETIC_OPERATOR";
        case TOKEN_LOGIC_OPERATOR:      return "LOGIC_OPERATOR";
        case TOKEN_SEPARATOR:           return "SEPARATOR";
        case TOKEN_COMMENT:             return "COMMENT";
        case TOKEN_UNKNOWN:             return "UNKNOWN";
        case TOKEN_ERROR:               return "ERROR";
        case TOKEN_EOF:                 return "EOF";
        default:                        return "???";
    }
}