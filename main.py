import lexer as lexer_mod

if __name__ == "__main__":
    with open("input.txt", "r") as file:
        code = file.read()

    lex = lexer_mod.lexer
    lex.input(code)

    for tok in lex:
        print(tok)
