import ply.lex as lex

# TOKENS

tokens = (
    'IDENTIFIER',
    'NUMBER',
    'STRING',
    'CHAR',
    'RESERVED_WORD',
    'ARITHMETIC_OPERATOR',
    'LOGIC_OPERATOR',
    'SEPARATOR',
)

# RESERVED WORDS

reserved_words = {
    'auto', 'break', 'case', 'char', 'const', 'continue', 'default',
    'do', 'double', 'else', 'enum', 'extern', 'float', 'for', 'goto',
    'if', 'inline', 'int', 'long', 'register', 'restrict', 'return',
    'short', 'signed', 'sizeof', 'static', 'struct', 'switch', 'typedef',
    'union', 'unsigned', 'void', 'volatile', 'while',
}

# RULES

def t_COMMENT_BLOCK(t):
    r'/\*(.|\n)*?\*/'
    pass  # ignora comentários de bloco
 
def t_COMMENT_LINE(t):
    r'//.*'
    pass  # ignora comentários de linha
 
def t_STRING(t):
    r'"([^"\\]|\\.)*"'
    return t
 
def t_CHAR(t):
    r"'([^'\\]|\\.)'"
    return t
 
def t_NUMBER(t):
    r'\d+\.\d+|\d+'
    return t
 
def t_IDENTIFIER(t):
    r'[a-zA-Z_][a-zA-Z0-9_]*'
    if t.value in reserved_words:
        t.type = 'RESERVED_WORD'
    return t
 
def t_ARITHMETIC_OPERATOR(t):
    r'\+\+|--|[+\-*/%]'
    return t
 
def t_LOGIC_OPERATOR(t):
    r'=|&&|\|\||!=|==|<=|>=|[!<>]'
    return t

def t_SEPARATOR(t):
    r'[(){}\[\];,.]'
    return t

# NEW LINE/ERROR

def t_newline(t):
    r'\n+'
    t.lexer.lineno += len(t.value)
 
t_ignore = ' \t\r'
 
def t_error(t):
    print(f"[ERRO] Caractere desconhecido '{t.value[0]}' na linha {t.lexer.lineno}")
    t.lexer.skip(1)

lexer = lex.lex()