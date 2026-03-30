#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include "lexer.h"

static const char *cursor;
static int cur_line;
static int cur_column;

static const char *reserved_words[] = {
    "auto", "break", "case", "char", "const", "continue", "default",
    "do", "double", "else", "enum", "extern", "float", "for", "goto",
    "if", "inline", "int", "long", "register", "restrict", "return",
    "short", "signed", "sizeof", "static", "struct", "switch", "typedef",
    "union", "unsigned", "void", "volatile", "while",
    NULL // To finish is_reserved loop
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
    { TOKEN_COMMENT,             "^/\\*([^*]|\\*+[^*/])*\\*+/"        },
    { TOKEN_COMMENT,             "^//[^\n]*"                           },
    { TOKEN_STRING,              "^\"([^\"\\\\]|\\\\.)*\""             },
    { TOKEN_CHAR,                "^'([^'\\\\]|\\\\.)*'"                },
    { TOKEN_NUMBER,              "^[0-9]+\\.[0-9]+|^[0-9]+"           },
    { TOKEN_IDENTIFIER,          "^[a-zA-Z_][a-zA-Z0-9_]*"            },
    { TOKEN_ARITHMETIC_OPERATOR, "^(\\+\\+|--|\\+|-|\\*|/|%)"         },
    { TOKEN_LOGIC_OPERATOR,      "^(&&|\\|\\||!=|==|<=|>=|=|!|<|>)"   },
    { TOKEN_SEPARATOR,           "^(\\(|\\)|\\{|\\}|\\[|\\]|;|,|\\.)" },
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
        // Try every rule
        for(size_t i = 0; i < NUM_RULES; i++){
            regex_t re;
            regmatch_t match;
            if(regcomp(&re, rules[i].pattern, REG_EXTENDED) != 0){
                regfree(&re);
                continue;
            }
            if(regexec(&re, cursor, 1, &match, 0) == 0 && match.rm_so == 0){
                int len = (int)match.rm_eo;

                if(len >= (int)sizeof(tok.value))
                    len = (int)sizeof(tok.value) - 1;

                strncpy(tok.value, cursor, len);
                tok.value[len] = '\0';
                tok.type = rules[i].type;
                tok.line = cur_line;
                tok.column = cur_column;

                // Identify reserved words
                if(tok.type == TOKEN_IDENTIFIER && is_reserved(tok.value))
                    tok.type = TOKEN_RESERVED_WORD;

                // Update line/column based on consumed content
                for(int j = 0; j < len; j++){
                    if(tok.value[j] == '\n'){ cur_line++; cur_column = 1; }
                    else cur_column++;
                }
                cursor += match.rm_eo;
                matched = 1;
                regfree(&re);

                // Commentaries are skiped
                if(tok.type == TOKEN_COMMENT) break;
                return tok;
            }
            regfree(&re);
        }
        if(matched) continue;
        // No rules matched
        tok.type = TOKEN_UNKNOWN;
        tok.value[0] = *cursor;
        tok.value[1] = '\0';
        tok.line = cur_line;
        tok.column = cur_column;
        cursor++;
        cur_column++;
        return tok;
    }

    // EOF
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
        case TOKEN_EOF:                 return "EOF";
        default:                        return "???";
    }
}