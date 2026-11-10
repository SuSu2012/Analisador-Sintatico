/*
 * verificacao.c - Verificacoes semanticas da gramatica (etapa 2).
 *
 * Ja implementados (use como modelo):
 *   - verif_indefinidos: nao terminais usados mas nao definidos;
 *   - verif_terminais:   terminais nomeados sem declaracao.
 *
 * TODO(aluno):
 *   - verif_inalcancaveis      (aviso)
 *   - verif_improdutivos       (erro)
 *   - verif_recursao_esquerda  (erro; direta e indireta)
 *
 * A duplicidade de definicoes ja e detectada durante a leitura
 * (ebnf_parser.c).
 */
#include <stdlib.h>
#include <string.h>
#include "verificacao.h"
#include "microc_mapa.h"
#include "util.h"

/* ------------------------------------------------------------------ */
/* Fornecido                                                          */
/* ------------------------------------------------------------------ */
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

static int verif_indefinidos(Gramatica *g)
{
    int i, erros = 0;
    for (i = 0; i < g->nnts; i++) {
        if (g->nts[i].regra == NULL) {
            diag_erro(g->nts[i].linha, g->nts[i].coluna,
                      "nao terminal %s usado mas nao definido", g->nts[i].nome);
            erros++;
        }
    }
    return erros;
}

static int verif_terminais(Gramatica *g, int modo_microc)
{
    int i, erros = 0, carregado = 0;
    Terminal *t;

    if (modo_microc)
        carregado = microc_carregar_tokens_h(g->tokens_externos) == 0;

    for (i = 1; i < g->nterms; i++) {
        t = &g->terms[i];
        if (modo_microc) {
            if (t->literal && microc_nome_literal(t->nome) == NULL) {
                diag_erro(t->linha, t->coluna, "literal %s nao corresponde a nenhum "
                          "token de Micro C", t->exibicao);
                erros++;
            } else if (!t->literal && carregado && !microc_token_existe(t->nome)) {
                diag_erro(t->linha, t->coluna, "terminal %s nao existe na enumeracao "
                          "TokenType de %s", t->nome, g->tokens_externos);
                erros++;
            }
        } else if (!t->declarado) {
            diag_erro(t->linha, t->coluna, "terminal %s nao declarado em TOKENS", t->nome);
            erros++;
        }
    }
    return erros;
}

/* ------------------------------------------------------------------ */
/* A implementar                                                      */
/* ------------------------------------------------------------------ */

/* TODO(aluno): emitir um AVISO para cada nao terminal que nao pode ser
 * alcancado a partir do simbolo inicial (g->inicial). Sugestao: marque
 * o inicial e, ate um ponto fixo, marque todo nao terminal que aparece
 * na regra de um nao terminal ja marcado (expr_percorrer ajuda). */
typedef struct {
    int *marca;
    int *mudou;
} CtxAlc;
 
static void marcar_nt(Expr *e, void *ctx)
{
    CtxAlc *c = (CtxAlc *) ctx;
    if (e->tipo == E_NAOTERM && !c->marca[e->simbolo]) {
        c->marca[e->simbolo] = 1;
        *c->mudou = 1;
    }
}

static int verif_inalcancaveis(Gramatica *g)
{
    int i, mudou, *marca;
    CtxAlc ctx;
 
    marca = (int *) xmalloc((size_t) g->nnts * sizeof(int));
    for (i = 0; i < g->nnts; i++)
        marca[i] = 0;
    marca[g->inicial] = 1;
    ctx.marca = marca;
    ctx.mudou = &mudou;
 
    do {
        mudou = 0;
        for (i = 0; i < g->nnts; i++)
            if (marca[i] && g->nts[i].regra != NULL)
                expr_percorrer(g->nts[i].regra, marcar_nt, &ctx);
    } while (mudou);
 
    for (i = 0; i < g->nnts; i++)
        if (!marca[i] && g->nts[i].regra != NULL)
            diag_aviso(g->nts[i].linha_def, 0,
                       "nao terminal %s nao e alcancavel a partir do simbolo inicial",
                       g->nts[i].nome);
 
    free(marca);
    return 0;
}

/* TODO(aluno): emitir um ERRO para cada nao terminal definido que nao
 * deriva nenhuma cadeia de terminais. Ponto fixo: um nao terminal e
 * produtivo se sua regra for produtiva, onde terminais, E_VAZIO, [..] e
 * {..} sao produtivos, uma sequencia exige todos os filhos produtivos
 * e uma alternativa exige ao menos um. Retorne o numero de erros. */
static int expr_produtiva(const Expr *e, const int *prod)
{
    int i;
    if (e == NULL)
        return 1;
    switch (e->tipo) {
    case E_TERM:
    case E_VAZIO:
    case E_OPC:
    case E_REP:
        return 1;
    case E_NAOTERM:
        return prod[e->simbolo];
    case E_SEQ:
        for (i = 0; i < e->nfilhos; i++)
            if (!expr_produtiva(e->filhos[i], prod))
                return 0;
        return 1;
    case E_ALT:
        for (i = 0; i < e->nfilhos; i++)
            if (expr_produtiva(e->filhos[i], prod))
                return 1;
        return 0;
    }
    return 1;
}

static int verif_improdutivos(Gramatica *g)
{
    int i, mudou, erros = 0, *prod;
 
    prod = (int *) xmalloc((size_t) g->nnts * sizeof(int));
    for (i = 0; i < g->nnts; i++)
        prod[i] = 0;
 
    do {
        mudou = 0;
        for (i = 0; i < g->nnts; i++) {
            if (!prod[i] && g->nts[i].regra != NULL &&
                expr_produtiva(g->nts[i].regra, prod)) {
                prod[i] = 1;
                mudou = 1;
            }
        }
    } while (mudou);
 
    for (i = 0; i < g->nnts; i++) {
        if (g->nts[i].regra != NULL && !prod[i]) {
            diag_erro(g->nts[i].linha_def, 0,
                      "nao terminal %s nao deriva nenhuma cadeia de terminais",
                      g->nts[i].nome);
            erros++;
        }
    }
 
    free(prod);
    return erros;
}

/* TODO(aluno): detectar recursao a esquerda direta (a = a ...) e
 * indireta (a = b ... ; b = a ...), inclusive atraves de prefixos
 * anulaveis (a = [ x ] a ...). Use o campo anulavel de Expr, ja
 * calculado quando esta funcao e chamada. Reporte um erro por nao
 * terminal recursivo e retorne o numero de erros. */
static void nts_a_esquerda(const Expr *e, char *esq)
{
    int i;
    if (e == NULL)
        return;
    switch (e->tipo) {
    case E_NAOTERM:
        esq[e->simbolo] = 1;
        break;
    case E_SEQ:
        for (i = 0; i < e->nfilhos; i++) {
            nts_a_esquerda(e->filhos[i], esq);
            if (!e->filhos[i]->anulavel)
                break;
        }
        break;
    case E_ALT:
        for (i = 0; i < e->nfilhos; i++)
            nts_a_esquerda(e->filhos[i], esq);
        break;
    case E_OPC:
    case E_REP:
        if (e->nfilhos > 0)
            nts_a_esquerda(e->filhos[0], esq);
        break;
    case E_TERM:
    case E_VAZIO:
        break;
    }
}

int verif_recursao_esquerda(Gramatica *g)
{
    int n = g->nnts, i, j, k, erros = 0;
    char *dir, *alc;
 
    dir = (char *) xmalloc((size_t) n * (size_t) n);
    alc = (char *) xmalloc((size_t) n * (size_t) n);
    memset(dir, 0, (size_t) n * (size_t) n);
 
    for (i = 0; i < n; i++)
        if (g->nts[i].regra != NULL)
            nts_a_esquerda(g->nts[i].regra, dir + (size_t) i * (size_t) n);
 
    memcpy(alc, dir, (size_t) n * (size_t) n);
    /* fecho transitivo (Warshall) */
    for (k = 0; k < n; k++)
        for (i = 0; i < n; i++)
            if (alc[(size_t) i * n + k])
                for (j = 0; j < n; j++)
                    if (alc[(size_t) k * n + j])
                        alc[(size_t) i * n + j] = 1;
 
    for (i = 0; i < n; i++) {
        if (alc[(size_t) i * n + i]) {
            diag_erro(g->nts[i].linha_def, 0,
                      "nao terminal %s tem recursao a esquerda %s",
                      g->nts[i].nome,
                      dir[(size_t) i * n + i] ? "direta" : "indireta");
            erros++;
        }
    }
 
    free(dir);
    free(alc);
    return erros;
}

int verif_basica(Gramatica *g, int modo_microc)
{
    int erros = 0;
    if (g->inicial < 0) {
        diag_erro(0, 0, "a gramatica nao tem nenhuma regra");
        return 1;
    }
    erros += verif_indefinidos(g);
    erros += verif_terminais(g, modo_microc);
    erros += verif_inalcancaveis(g);
    if (erros == 0)
        erros += verif_improdutivos(g);
    return erros;
}