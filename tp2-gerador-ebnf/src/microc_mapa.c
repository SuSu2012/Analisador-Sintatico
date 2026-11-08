/*
 * microc_mapa.c - Correspondencia com os tokens de Micro C (ver microc_mapa.h).
 *
 * A tabela de literais segue o tokens.h do livro-texto (secao 2.3.1):
 *
 *   UNDEF, ID, END_OF_FILE, INTEGERCONST, CHARCONST, STRINGCONST,
 *   PLUS, MINUS, MUL, DIV, MOD, EQ, NEQ, LT, GT, LEQ, GEQ, AND, OR, NOT,
 *   ASSIGN, SEMICOLON, COMMA, LPAREN, RPAREN, LBRACE, RBRACE,
 *   LBRACKET, RBRACKET, MAIN, IF, ELSE, FOR, RETURN, INT, CHAR, PRINT
 *
 * Se o seu tokens.h do TP1 usar outros nomes, ajuste a tabela.
 *
 * TODO(aluno): implementar microc_carregar_tokens_h.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "microc_mapa.h"
#include "util.h"

static const char *const literais[][2] = {
    { "+",  "PLUS" },      { "-",  "MINUS" },     { "*",  "MUL" },
    { "/",  "DIV" },       { "%",  "MOD" },
    { "==", "EQ" },        { "!=", "NEQ" },       { "<",  "LT" },
    { ">",  "GT" },        { "<=", "LEQ" },       { ">=", "GEQ" },
    { "&&", "AND" },       { "||", "OR" },        { "!",  "NOT" },
    { "=",  "ASSIGN" },    { ";",  "SEMICOLON" }, { ",",  "COMMA" },
    { "(",  "LPAREN" },    { ")",  "RPAREN" },    { "{",  "LBRACE" },
    { "}",  "RBRACE" },    { "[",  "LBRACKET" },  { "]",  "RBRACKET" },
    { "main", "MAIN" },    { "if", "IF" },        { "else", "ELSE" },
    { "for", "FOR" },      { "return", "RETURN" },{ "int", "INT" },
    { "char", "CHAR" },    { "print", "PRINT" }
};

static char **nomes = NULL;
static int nnomes = 0, capnomes = 0;

const char *microc_nome_literal(const char *literal)
{
    size_t i;
    for (i = 0; i < sizeof(literais) / sizeof(literais[0]); i++)
        if (strcmp(literais[i][0], literal) == 0)
            return literais[i][1];
    return NULL;
}

static void adicionar_nome(const char *s, size_t n)
{
    if (nnomes == capnomes) {
        capnomes = capnomes ? 2 * capnomes : 64;
        nomes = (char **) xrealloc(nomes, (size_t) capnomes * sizeof(char *));
    }
    nomes[nnomes++] = xstrndup(s, n);
}

/* TODO(aluno): abrir o arquivo (ler_arquivo ajuda), localizar a
 * enumeracao "typedef enum { ... } TokenType;" e chamar adicionar_nome
 * para cada identificador da lista, ignorando comentarios e valores
 * explicitos ("X = 3"). Retorne 0 em caso de sucesso.
 * Enquanto nao implementada, a verificacao dos nomes e pulada. */
int microc_carregar_tokens_h(const char *caminho)
{
    char *conteudo;
    const char *p;
    const char *inicio_enum;
    const char *fim_enum;
 
    if (caminho == NULL)
        return -1;
 
    conteudo = ler_arquivo(caminho);
    if (conteudo == NULL)
        return -1;
 
    p = strstr(conteudo, "enum");
    if (p == NULL) {
        free(conteudo);
        return -1;
    }
 
    inicio_enum = strchr(p, '{');
    if (inicio_enum == NULL) {
        free(conteudo);
        return -1;
    }
    inicio_enum++;
 
    fim_enum = strchr(inicio_enum, '}');
    if (fim_enum == NULL) {
        free(conteudo);
        return -1;
    }
 
    p = inicio_enum;
    while (p < fim_enum) {
        while (p < fim_enum && isspace((unsigned char)*p))
            p++;
 
        if (p >= fim_enum)
            break;
 
        if (p[0] == '/' && p[1] == '/') {
            p += 2;
            while (p < fim_enum && *p != '\n')
                p++;
            continue;
        }
 
        /* Ignora comentarios de bloco (abre-barra-asterisco) */
        if (p[0] == '/' && p[1] == '*') {
            p += 2;
            while (p + 1 < fim_enum && !(p[0] == '*' && p[1] == '/'))
                p++;
            if (p + 1 < fim_enum)
                p += 2;
            continue;
        }
 
        if (isalpha((unsigned char)*p) || *p == '_') {
            const char *inicio_id = p;
            size_t tam_id;
 
            while (p < fim_enum && (isalnum((unsigned char)*p) || *p == '_'))
                p++;
            tam_id = (size_t)(p - inicio_id);
 
            adicionar_nome(inicio_id, tam_id);
 
            while (p < fim_enum && isspace((unsigned char)*p))
                p++;
 
            if (p < fim_enum && *p == '=') {
                while (p < fim_enum && *p != ',' && *p != '}')
                    p++;
            }
            if (p < fim_enum && *p == ',')
                p++;
        } else {
            p++;
        }
    }
 
    free(conteudo);
    return 0;
}

int microc_token_existe(const char *nome)
{
    int i;
    for (i = 0; i < nnomes; i++)
        if (strcmp(nomes[i], nome) == 0)
            return 1;
    return 0;
}

void microc_liberar(void)
{
    int i;
    for (i = 0; i < nnomes; i++)
        free(nomes[i]);
    free(nomes);
    nomes = NULL;
    nnomes = capnomes = 0;
}
