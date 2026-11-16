/*
 * ebnf_parser.c - Analisador sintatico descendente recursivo da notacao EBNF.
 *
 * Gramatica da propria notacao (escrita em EBNF):
 *
 *   gramatica    = [ secao_tokens ] secao_rules ;
 *   secao_tokens = "TOKENS" ( "EXTERN" LITERAL ";" | { def_token } ) ;
 *   def_token    = NOME_TERM "=" REGEX ";"
 *                | "SKIP" "=" REGEX ";" ;
 *   secao_rules  = "RULES" regra { regra } ;
 *   regra        = NAOTERM "=" expr ";" ;
 *   expr         = seq { "|" seq } ;
 *   seq          = { fator } ;
 *   fator        = NOME_TERM | LITERAL | NAOTERM
 *                | "(" expr ")" | "[" expr "]" | "{" expr "}" ;
 *
 * As secoes TOKENS e RULES e a regra completa (regra) ja estao prontas,
 * incluindo a recuperacao de erros por sincronizacao no ';'.
 *
 * Implementado pelo grupo: parse_expr, parse_seq e parse_fator, que
 * constroem a arvore Expr do lado direito das regras (ver gramatica.h).
 */
#include <stdlib.h>
#include <string.h>
#include "ebnf_parser.h"
#include "ebnf_lexer.h"
#include "util.h"

typedef struct {
    MetaLexer lx;
    MetaToken tok;      /* token corrente (lookahead) */
    Gramatica *g;
} Parser;

/* ------------------------------------------------------------------ */
/* Primitivas                                                         */
/* ------------------------------------------------------------------ */

/* Consome o token corrente. Tokens MT_ERRO (ja reportados pelo
 * lexer) sao descartados. */
static void avancar(Parser *p)
{
    mtok_liberar(&p->tok);
    p->tok = mlex_proximo(&p->lx);
    while (p->tok.tipo == MT_ERRO)
        p->tok = mlex_proximo(&p->lx);
}

static void erro_encontrado(Parser *p, const char *esperado)
{
    if (p->tok.texto != NULL)
        diag_erro(p->tok.linha, p->tok.coluna, "esperado %s, encontrado %s '%s'",
                  esperado, mtipo_nome(p->tok.tipo), p->tok.texto);
    else
        diag_erro(p->tok.linha, p->tok.coluna, "esperado %s, encontrado %s",
                  esperado, mtipo_nome(p->tok.tipo));
}

/* Se o token corrente for do tipo t, consome-o e retorna 1;
 * senao reporta o erro e retorna 0 (sem consumir). */
static int esperar(Parser *p, MetaTipo t)
{
    if (p->tok.tipo == t) {
        avancar(p);
        return 1;
    }
    erro_encontrado(p, mtipo_nome(t));
    return 0;
}

/* Recuperacao de erro: descarta tokens ate depois do proximo ';'
 * (ou ate RULES / fim de arquivo). */
static void sincronizar(Parser *p)
{
    while (p->tok.tipo != MT_PV && p->tok.tipo != MT_FIM && p->tok.tipo != MT_RULES)
        avancar(p);
    if (p->tok.tipo == MT_PV)
        avancar(p);
}

/* ------------------------------------------------------------------ */
/* Expressoes (lado direito das regras)                               */
/* ------------------------------------------------------------------ */

/*
 * Uma funcao por nao terminal, seguindo a gramatica da notacao
 * (cabecalho deste arquivo):
 *
 *   static Expr *parse_expr(Parser *p);   expr  = seq { "|" seq } ;
 *   static Expr *parse_seq(Parser *p);    seq   = { fator } ;
 *   static Expr *parse_fator(Parser *p);  fator = NOME_TERM | LITERAL | ...
 *
 * Observacoes:
 *   - O token corrente esta em p->tok (tipo, texto, linha, coluna);
 *     avancar(p) consome-o e esperar(p, MT_...) consome um token exigido.
 *   - Terminais: gram_terminal(p->g, texto, literal, linha, coluna)
 *     devolve o indice; o no e criado com expr_simbolo(E_TERM, ...).
 *   - Nao terminais: gram_naoterminal(...) e expr_simbolo(E_NAOTERM, ...).
 *   - [ a ] e { a }: expr_unario(E_OPC, a, ...) e expr_unario(E_REP, a, ...).
 *   - ( a ) nao cria no: devolve a propria arvore de a.
 *   - Nao ha nos com um unico filho: uma sequencia com um so fator ou
 *     uma alternativa unica sao devolvidas diretamente.
 *   - Uma sequencia vazia (ex.: "a = ;" ou "b | ") vira E_VAZIO.
 */
static Expr *parse_expr(Parser *p);
static Expr *parse_seq(Parser *p);
static Expr *parse_fator(Parser *p);

/* fator = NOME_TERM | LITERAL | NAOTERM | "(" expr ")" | "[" expr "]" | "{" expr "}" ; */
static Expr *parse_fator(Parser *p)
{
    int l = p->tok.linha, c = p->tok.coluna;
    int idx;
    Expr *e;

    switch (p->tok.tipo) {
    case MT_NOME_TERM:
        idx = gram_terminal(p->g, p->tok.texto, 0, l, c);
        avancar(p);
        return expr_simbolo(E_TERM, idx, l, c);

    case MT_LITERAL:
        idx = gram_terminal(p->g, p->tok.texto, 1, l, c);
        avancar(p);
        return expr_simbolo(E_TERM, idx, l, c);

    case MT_NAOTERM:
        idx = gram_naoterminal(p->g, p->tok.texto, l, c);
        avancar(p);
        return expr_simbolo(E_NAOTERM, idx, l, c);

    case MT_ABRE_PAR:
        avancar(p);
        e = parse_expr(p);
        esperar(p, MT_FECHA_PAR);
        return e;

    case MT_ABRE_COL:
        avancar(p);
        e = parse_expr(p);
        esperar(p, MT_FECHA_COL);
        return expr_unario(E_OPC, e, l, c);

    case MT_ABRE_CHAVE:
        avancar(p);
        e = parse_expr(p);
        esperar(p, MT_FECHA_CHAVE);
        return expr_unario(E_REP, e, l, c);

    default:
        erro_encontrado(p, "fator");
        return expr_criar(E_VAZIO, l, c);
    }
}
/* seq = { fator } ; */
static Expr *parse_seq(Parser *p)
{
    int l = p->tok.linha, c = p->tok.coluna;
    Expr *seq = expr_criar(E_SEQ, l, c);
    Expr *f, *unico;

    while (p->tok.tipo == MT_NOME_TERM || p->tok.tipo == MT_LITERAL ||
           p->tok.tipo == MT_NAOTERM   || p->tok.tipo == MT_ABRE_PAR ||
           p->tok.tipo == MT_ABRE_COL  || p->tok.tipo == MT_ABRE_CHAVE) {
        f = parse_fator(p);
        expr_adicionar(seq, f);
    }

    if (seq->nfilhos == 0) {
        expr_liberar(seq);
        return expr_criar(E_VAZIO, l, c);
    }
    if (seq->nfilhos == 1) {
        unico = seq->filhos[0];
        free(seq->filhos);
        free(seq);
        return unico;
    }
    return seq;
}
/* expr = seq { "|" seq } ; */
static Expr *parse_expr(Parser *p)
{
    int l = p->tok.linha, c = p->tok.coluna;
    Expr *s = parse_seq(p);
    Expr *alt, *s2, *unico;

    if (p->tok.tipo != MT_BARRA)
        return s;

    alt = expr_criar(E_ALT, l, c);
    expr_adicionar(alt, s);

    while (p->tok.tipo == MT_BARRA) {
        avancar(p);
        s2 = parse_seq(p);
        expr_adicionar(alt, s2);
    }

    if (alt->nfilhos == 1) {
        unico = alt->filhos[0];
        free(alt->filhos);
        free(alt);
        return unico;
    }
    return alt;
}

/* ------------------------------------------------------------------ */
/* Secoes e regras (fornecido)                                        */
/* ------------------------------------------------------------------ */

/* def_token = NOME_TERM "=" REGEX ";" | "SKIP" "=" REGEX ";" ; */
static void parse_def_token(Parser *p)
{
    int skip = p->tok.tipo == MT_SKIP, l = p->tok.linha, c = p->tok.coluna, i = -1;

    if (!skip) {
        i = gram_busca_terminal(p->g, p->tok.texto, 0);
        if (i >= 0 && p->g->terms[i].declarado) {
            diag_erro(l, c, "terminal %s declarado mais de uma vez (primeira declaracao "
                      "na linha %d)", p->tok.texto, p->g->terms[i].linha);
            sincronizar(p);
            return;
        }
        i = gram_terminal(p->g, p->tok.texto, 0, l, c);
    }
    avancar(p);
    if (!esperar(p, MT_IGUAL)) {
        sincronizar(p);
        return;
    }
    if (p->tok.tipo != MT_REGEX) {
        erro_encontrado(p, "expressao regular /.../");
        sincronizar(p);
        return;
    }
    if (skip) {
        gram_add_skip(p->g, p->tok.texto);
    } else {
        p->g->terms[i].regex = xstrdup(p->tok.texto);
        p->g->terms[i].declarado = 1;
    }
    avancar(p);
    if (!esperar(p, MT_PV))
        sincronizar(p);
}

/* secao_tokens = "TOKENS" ( "EXTERN" LITERAL ";" | { def_token } ) ; */
static void parse_secao_tokens(Parser *p)
{
    avancar(p); /* TOKENS */
    if (p->tok.tipo == MT_EXTERN) {
        avancar(p);
        if (p->tok.tipo != MT_LITERAL) {
            erro_encontrado(p, "caminho do cabecalho entre aspas (ex.: \"tokens.h\")");
            sincronizar(p);
            return;
        }
        p->g->tokens_externos = xstrdup(p->tok.texto);
        avancar(p);
        if (!esperar(p, MT_PV))
            sincronizar(p);
        return;
    }
    while (p->tok.tipo == MT_NOME_TERM || p->tok.tipo == MT_SKIP)
        parse_def_token(p);
}

/* regra = NAOTERM "=" expr ";" ; */
static void parse_regra(Parser *p)
{
    int l = p->tok.linha, c = p->tok.coluna, nt, erros_antes = diag_num_erros();
    Expr *e;

    nt = gram_naoterminal(p->g, p->tok.texto, l, c);
    avancar(p);
    if (!esperar(p, MT_IGUAL)) {
        sincronizar(p);
        return;
    }
    e = parse_expr(p);
    if (diag_num_erros() > erros_antes || !esperar(p, MT_PV)) {
        expr_liberar(e);
        sincronizar(p);
        return;
    }
    if (p->g->nts[nt].regra != NULL) {
        diag_erro(l, c, "nao terminal %s definido mais de uma vez (primeira definicao "
                  "na linha %d)", p->g->nts[nt].nome, p->g->nts[nt].linha_def);
        expr_liberar(e);
        return;
    }
    p->g->nts[nt].regra = e;
    p->g->nts[nt].linha_def = l;
    if (p->g->inicial < 0)
        p->g->inicial = nt;
}

/* secao_rules = "RULES" regra { regra } ; */
static void parse_secao_rules(Parser *p)
{
    if (!esperar(p, MT_RULES))
        return;
    if (p->tok.tipo != MT_NAOTERM)
        erro_encontrado(p, "ao menos uma regra (nao terminal)");
    while (p->tok.tipo != MT_FIM) {
        if (p->tok.tipo == MT_NAOTERM) {
            parse_regra(p);
        } else {
            erro_encontrado(p, "nao terminal no inicio de uma regra");
            avancar(p);
            sincronizar(p);
        }
    }
}

int ebnf_analisar(Gramatica *g, const char *fonte)
{
    Parser p;
    p.g = g;
    mlex_iniciar(&p.lx, fonte);
    p.tok.texto = NULL;
    avancar(&p);

    if (p.tok.tipo == MT_TOKENS)
        parse_secao_tokens(&p);
    if (p.tok.tipo != MT_RULES) {
        erro_encontrado(&p, "RULES (ou uma definicao de token)");
        while (p.tok.tipo != MT_RULES && p.tok.tipo != MT_FIM)
            avancar(&p);
    }
    parse_secao_rules(&p);
    mtok_liberar(&p.tok);
    return diag_num_erros() == 0;
}
