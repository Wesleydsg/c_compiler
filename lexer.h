#ifndef LEXER_H
#define LEXER_H

typedef enum{
    TOKEN_IDENTIFIER,
    TOKEN_NUMBER,
    TOKEN_STRING,
    TOKEN_CHAR,
    TOKEN_RESERVED_WORD,
    TOKEN_ARITHMETIC_OPERATOR,
    TOKEN_LOGIC_OPERATOR,
    TOKEN_SEPARATOR,
    TOKEN_COMMENT,
    TOKEN_UNKNOWN,
    TOKEN_EOF,
} TokenType;

typedef struct{
    TokenType type;
    char value[256];
    int line;
    int column;
} Token;

void  lexer_init(const char *source);
Token lexer_next_token(void);
const char *token_type_name(TokenType type);

#endif