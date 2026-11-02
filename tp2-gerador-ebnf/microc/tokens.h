#ifndef TOKENS_H
#define TOKENS_H

typedef enum {
    UNDEF,          /* 0: token indefinido */
    ID,             /* 1: identificador */
    END_OF_FILE,    /* 2: fim de arquivo */

    /* constantes literais */
    INTEGERCONST,
    CHARCONST,
    STRINGCONST,

    /* operadores aritmeticos */
    PLUS, MINUS, MUL, DIV, MOD,

    /* operadores relacionais e logicos */
    EQ, NEQ, LT, GT, LEQ, GEQ, AND, OR, NOT,

    /* simbolos de atribuicao e pontuacao */
    ASSIGN, SEMICOLON, COMMA, LPAREN, RPAREN,
    LBRACE, RBRACE, LBRACKET, RBRACKET,

    /* palavras reservadas */
    MAIN, IF, ELSE, FOR, RETURN, INT, CHAR, PRINT
} TokenType;

typedef struct {
    TokenType tipo;
    char lexema[100];
    int linha;
} Token;

typedef struct {
    char *symbol;      /* lexema para ID, INTEGERCONST, CHARCONST, STRINGCONST */
    char *error_msg;   /* mensagem de erro para UNDEF */
} YYSTYPE;

extern YYSTYPE microc_yylval;

/* função para desalocar a tabela hash no encerramento */
void liberar_tabela_hash(void);

#endif