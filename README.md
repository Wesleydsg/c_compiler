# Lexical Analyzer for C

A lexical analyzer for the C language, written in C using regex.

## File Structure

```
.
├── lexer.h     # Token types and function declarations
├── lexer.c     # Lexer implementation
├── main.c      # Entry point
└── input.txt   # Input file for testing
```

## How to Compile and Run

```bash
make run
```

## How It Works

The lexer reads the source file as a string and scans it character by character using a cursor pointer. For each position, it tries to match a list of regex rules in order — the first rule that matches at the current position produces a token. Comments are discarded. Whitespace and newlines are skipped while keeping track of line and column.

Each token is returned with its type, value, and position (`line/column`).

## Tokens

| Token | Examples |
|---|---|
| `RESERVED_WORD` | `int`, `if`, `return`, `while` |
| `IDENTIFIER` | `main`, `x`, `printf` |
| `NUMBER` | `42`, `3.14` |
| `STRING` | `"hello"` |
| `CHAR` | `'A'` |
| `ARITHMETIC_OPERATOR` | `+`, `-`, `*`, `/`, `++`, `--` |
| `LOGIC_OPERATOR` | `==`, `!=`, `&&`, `\|\|`, `>=` |
| `SEPARATOR` | `(`, `)`, `{`, `}`, `;` |
| `COMMENT` | `// ...`, `/* ... */` |