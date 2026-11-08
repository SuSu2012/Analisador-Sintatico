%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "tokens.h"

YYSTYPE microc_yylval;
#define TAM_BUFFER 1024
int termina_expr = 0; /* variavel para tratar ambiguidade de inteiros negativos */

int linha_atual = 1;

char buffer_string[TAM_BUFFER];
int pos_buffer = 0;

char buffer_erro[TAM_BUFFER];

void clear_string_buffer(void) {
    int i;
    for (i = 0; i < TAM_BUFFER - 1; i++) {
        buffer_string[i] = '\0';
    }
}

void add_char_string(char c) {
    if (pos_buffer < TAM_BUFFER - 1) {
        buffer_string[pos_buffer++] = c;
    }
}

#define P 10
#define TAM 256

unsigned long retorna_hash(char *texto, int p) {
    double A = 0.6180339887;
    int x = 0;
    char *ptr = texto;
    double m;
    while (*ptr != '\0') {
        x = x + (*ptr);
        ptr++;
    }
    m = pow(2, p);
    return floor(m * (x * A - floor(x * A)));
}

unsigned int retorna_pos(unsigned long val, int tam) {
    return val % tam;
}

typedef struct NoIdentificador {
    char *identificador;
    int tipo;
    struct NoIdentificador *proximo;
} NoIdentificador;

NoIdentificador *identificadores[TAM];

NoIdentificador* busca_tabela_id(char *texto, int tipo, int hash_pos) {
    NoIdentificador *ptr = identificadores[hash_pos];
    while (ptr != NULL) {
        if ((strcmp(texto, ptr->identificador) == 0) && ptr->tipo == tipo) {
            return ptr;
        }
        ptr = ptr->proximo;
    }
    return NULL;
}

NoIdentificador* inserir_tabela_id(char *texto, int tipo, int hash_pos) {
    NoIdentificador *ptr = identificadores[hash_pos];
    NoIdentificador *ptr_ant = NULL;
    NoIdentificador *novo = (NoIdentificador *) calloc(1, sizeof(NoIdentificador));
    while (ptr != NULL) {
        ptr_ant = ptr;
        ptr = ptr->proximo;
    }
    if (ptr_ant == NULL) {
        identificadores[hash_pos] = novo;
    }
    novo->identificador = strdup(texto);
    novo->tipo = tipo;
    novo->proximo = NULL;
    if (ptr_ant != NULL) {
        ptr_ant->proximo = novo;
    }
    return novo;
}

static void guarda_lexema(char *texto, int tipo) {
    unsigned long hash_val = retorna_hash(texto, P);
    unsigned int hash_pos = retorna_pos(hash_val, TAM);
    NoIdentificador *novo = busca_tabela_id(texto, tipo, hash_pos);
    if (novo == NULL) {
        novo = inserir_tabela_id(texto, tipo, hash_pos);
    }
    if (novo != NULL) {
        microc_yylval.symbol = novo->identificador;
    }
}

void liberar_tabela_hash(void) {
    int i;
    for (i = 0; i < TAM; i++) {
        NoIdentificador *ptr = identificadores[i];
        while (ptr != NULL) {
            NoIdentificador *tmp = ptr;
            ptr = ptr->proximo;
            free(tmp->identificador);
            free(tmp);
        }
        identificadores[i] = NULL;
    }
}
%}

%option prefix="microc_yy"
%option noyywrap

DIGIT       [0-9]
LETRA       [a-zA-Z_]
ALFANUM     [a-zA-Z0-9_]

%x COMMENT
%x STRING

%%

<INITIAL><<EOF>>            { return END_OF_FILE; }

\n                          { linha_atual++; }
[ \t\r]+                    { /* ignora espacos em branco */ }

"//".*                      { /* comentario de linha */ }
<COMMENT><<EOF>>            {
                                microc_yylval.error_msg = "EOF em comentario";
                                BEGIN(INITIAL);
                                return UNDEF;
                            }
"/*"                        { BEGIN(COMMENT); }
<COMMENT>"*/"               { BEGIN(INITIAL); }
<COMMENT>\n                { linha_atual++; }
<COMMENT>.                  { /* consome caracteres */ }

"*/"                        {
                                microc_yylval.error_msg = "Comentario nao iniciado";
                                return UNDEF;
                            }

{LETRA}{ALFANUM}*           {
                                if (strcmp(yytext, "main") == 0) { termina_expr = 0; return MAIN; }
                                else if (strcmp(yytext, "if") == 0) { termina_expr = 0; return IF; }
                                else if (strcmp(yytext, "else") == 0) { termina_expr = 0; return ELSE; }
                                else if (strcmp(yytext, "for") == 0) { termina_expr = 0; return FOR; }
                                else if (strcmp(yytext, "return") == 0) { termina_expr = 0; return RETURN; }
                                else if (strcmp(yytext, "int") == 0) { termina_expr = 0; return INT; }
                                else if (strcmp(yytext, "char") == 0) { termina_expr = 0; return CHAR; }
                                else if (strcmp(yytext, "print") == 0) { termina_expr = 0; return PRINT; }
                                else {
                                    guarda_lexema(yytext, ID);
                                    termina_expr = 1;
                                    return ID;
                                }
                            }

{DIGIT}+                    {
                                guarda_lexema(yytext, INTEGERCONST);
                                termina_expr = 1;
                                return INTEGERCONST;
                            }
-{DIGIT}+                   {
                                if (termina_expr) {
                                    yyless(1);
                                    termina_expr = 0;
                                    return MINUS;
                                } else {
                                    guarda_lexema(yytext, INTEGERCONST);
                                    termina_expr = 1;
                                    return INTEGERCONST;
                                }
                            }

\'.\'                       {
                                char res[2];
                                res[0] = yytext[1];
                                res[1] = '\0';
                                guarda_lexema(res, CHARCONST);
                                termina_expr = 1;
                                return CHARCONST;
                            }
\'[^'\n]*                   {
                                microc_yylval.error_msg = "Constante de caractere nao fechada";
                                return UNDEF;
                            }
\'([^\'\\\n]|\\.)\'         {
                                char res[2];
                                unsigned char c2 = (unsigned char) yytext[2];
                                res[0] = 0;
                                res[1] = '\0';

                                if (yytext[1] == '\\') {
                                    if (c2 == 'n')      res[0] = '\n';
                                    else if (c2 == 't') res[0] = '\t';
                                    else if (c2 == '\\') res[0] = '\\';
                                    else if (c2 == '\'') res[0] = '\'';
                                    else if (c2 == '0')  res[0] = '\0';
                                    else                 res[0] = c2;
                                } else {
                                    res[0] = yytext[1];
                                }

                                guarda_lexema(res, CHARCONST);
                                termina_expr = 1;
                                return CHARCONST;
                            }

\"                          {
                                BEGIN(STRING);
                                pos_buffer = 0;
                                buffer_string[0] = '\0';
                            }

<STRING>\"                  { 
                                BEGIN(INITIAL);
                                buffer_string[pos_buffer] = '\0';
                                guarda_lexema(buffer_string, STRINGCONST);
                                termina_expr = 1;
                                return STRINGCONST;
                            }

<STRING>\\n                 { add_char_string('\n'); }
<STRING>\\t                 { add_char_string('\t'); }
<STRING>\\\"                { add_char_string('"'); }
<STRING>\\\\                { add_char_string('\\'); }
<STRING>\\0                 { add_char_string('\0'); }

<STRING>\n                  {
                                microc_yylval.error_msg = "String nao terminada";
                                BEGIN(INITIAL);
                                yyless(0);
                                return UNDEF;
                            }

<STRING>\0                  {
                                microc_yylval.error_msg = "String contem caractere nulo";
                                BEGIN(INITIAL);
                                return UNDEF;
                            }

<STRING><<EOF>>             {
                                microc_yylval.error_msg = "EOF em string";
                                BEGIN(INITIAL);
                                return UNDEF;
                            }

<STRING>.                   { 
                                add_char_string(yytext[0]);
                            }

"=="                        { termina_expr = 0; return EQ; }
"="                         { termina_expr = 0; return ASSIGN; }
"!="                        { termina_expr = 0; return NEQ; }
"!"                         { termina_expr = 0; return NOT; }
"<="                        { termina_expr = 0; return LEQ; }
"<"                         { termina_expr = 0; return LT; }
">="                        { termina_expr = 0; return GEQ; }
">"                         { termina_expr = 0; return GT; }
"&&"                        { termina_expr = 0; return AND; }
"||"                        { termina_expr = 0; return OR; }

"+"                         { termina_expr = 0; return PLUS; }
"-"                         { termina_expr = 0; return MINUS; }
"*"                         { termina_expr = 0; return MUL; }
"/"                         { termina_expr = 0; return DIV; }
"%"                         { termina_expr = 0; return MOD; }
";"                         { termina_expr = 0; return SEMICOLON; }
","                         { termina_expr = 0; return COMMA; }
"("                         { termina_expr = 0; return LPAREN; }
")"                         { termina_expr = 1; return RPAREN; }
"{"                         { return LBRACE; }
"}"                         { return RBRACE; }
"["                         { termina_expr = 0; return LBRACKET; }
"]"                         { termina_expr = 1; return RBRACKET; }

.                           {
                                snprintf(buffer_erro, sizeof(buffer_erro), "caractere invalido: ");
                                buffer_erro[20] = yytext[0];
                                buffer_erro[21] = '\0';
                                microc_yylval.error_msg = buffer_erro;
                                return UNDEF;
                            }

%%