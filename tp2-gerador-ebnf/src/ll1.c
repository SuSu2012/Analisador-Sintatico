/*
 * ll1.c - Verificacao da condicao LL(1) (etapa 4).
 *
 * Condicoes (secao 3 do enunciado):
 *   1. Em a1 | a2 | ... | an, os FIRST das alternativas sao disjuntos dois
 *      a dois e no maximo uma alternativa e anulavel.
 *   2. Se uma alternativa ai e anulavel, FOLLOW(alternativa) e disjunto
 *      dos FIRST das demais.
 *   3. Em [ a ] e { a }, a nao e anulavel e FIRST(a) e disjunto do
 *      FOLLOW da construcao.
 *
 * Violacoes de 1, 2 e da primeira parte de 3 sao ERROS. A violacao de
 * FIRST(a) inter FOLLOW nao vazio em [ a ] ou { a } e reportada como
 * AVISO e resolvida de forma gulosa: o analisador gerado entra na
 * construcao sempre que o token estiver em FIRST(a). E o caso classico
 * do "else pendente":  if = "if" "(" expr ")" cmd [ "else" cmd ] ;
 * em que o else fica com o if mais proximo, como em C.
 *
 * Fornecido: reportar (formata as mensagens).
 * Implementado pelo grupo: verificar_expr e ll1_verificar.
 */
#include <stdarg.h>
#include <stdio.h>
#include "ll1.h"
#include "util.h"

/* Reporta um conflito na regra do nao terminal nt, na posicao de e.
 * comum (pode ser NULL) e o conjunto de tokens em conflito. */
static void reportar(const Gramatica *g, int nt, const Expr *e, int eh_aviso,
                     const Bitset *comum, const char *fmt, ...)
{
    va_list ap;
    fprintf(stderr, "%s:%d:%d: %s: conflito LL(1) em %s: ", g->arquivo, e->linha,
            e->coluna, eh_aviso ? "aviso" : "erro", g->nts[nt].nome);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    if (comum != NULL) {
        fprintf(stderr, " ");
        gram_imprimir_conjunto(stderr, g, comum);
    }
    fputc('\n', stderr);
}

/* Percorre as regras e, para cada no E_ALT, E_OPC e E_REP, verifica as
 * condicoes do cabecalho com bitset_intersecao/bitset_vazio e reporta
 * com reportar(...). Exemplo de mensagem:
 *   expr.ebnf:7:10: erro: conflito LL(1) em stmt: alternativas 1 e 2 comecam com {ID}
 * Retorna o numero de ERROS (avisos nao contam). */

/* Percorre recursivamente a arvore de expressoes de uma regra checando
 * as condicoes LL(1) em nos E_ALT, E_OPC e E_REP. */
static int verificar_expr(const Gramatica *g, int nt, const Expr *e)
{
    int i, j, erros = 0;
    Bitset comum;

    if (e == NULL)
        return 0;

    bitset_iniciar(&comum, g->nterms);

    switch (e->tipo) {
    case E_ALT: {
        int num_anulaveis = 0;
        /* condicao 1a: FIRST(ai) intersecao FIRST(aj) == vazio para i != j */
        for (i = 0; i < e->nfilhos; i++) {
            if (e->filhos[i]->anulavel)
                num_anulaveis++;

            for (j = i + 1; j < e->nfilhos; j++) {
                bitset_intersecao(&comum, &e->filhos[i]->first, &e->filhos[j]->first);
                if (!bitset_vazio(&comum)) {
                    reportar(g, nt, e, 0, &comum,
                             "alternativas %d e %d comecam com", i + 1, j + 1);
                    erros++;
                }
            }
        }
        /* condicao 1b: No maximo uma alternativa e anulavel */
        if (num_anulaveis > 1) {
            reportar(g, nt, e, 0, NULL, "mais de uma alternativa e anulavel");
            erros++;
        }
        /* condicao 2: Se ai e anulavel, FOLLOW(e) intersecao FIRST(aj) == vazio para j != i */
        for (i = 0; i < e->nfilhos; i++) {
            if (e->filhos[i]->anulavel) {
                for (j = 0; j < e->nfilhos; j++) {
                    if (i == j)
                        continue;
                    bitset_intersecao(&comum, &e->follow, &e->filhos[j]->first);
                    if (!bitset_vazio(&comum)) {
                        reportar(g, nt, e, 0, &comum,
                                 "alternativa %d e anulavel e seu FOLLOW conflita com FIRST da alternativa %d em",
                                 i + 1, j + 1);
                        erros++;
                    }
                }
            }
        }
        break;
    }
    case E_OPC:
    case E_REP: {
        if (e->nfilhos > 0) {
            const Expr *filho = e->filhos[0];
            const char *nome_const = (e->tipo == E_OPC) ? "opcional" : "repeticao";
            /* condicao 3a: O filho de [ a ] ou { a } nao pode ser anulavel */
            if (filho->anulavel) {
                reportar(g, nt, e, 0, NULL, "%s contem expressao anulavel", nome_const);
                erros++;
            }
            /* condicao 3b: FIRST(filho) intersecao FOLLOW(e) == vazio (AVISO) */
            bitset_intersecao(&comum, &filho->first, &e->follow);
            if (!bitset_vazio(&comum)) {
                /* Reporta como aviso; nao incrementa contagem de erros */
                reportar(g, nt, e, 1, &comum, "FIRST e FOLLOW de %s se intersectam (resolvido de forma gulosa) em", e->tipo == E_OPC ? "[ ]" : "{ }");
            }
        }
        break;
    }
    default:
        break;
    }
    bitset_liberar(&comum);
    /* percorre todos os filhos da subarvore */
    for (i = 0; i < e->nfilhos; i++) {
        erros += verificar_expr(g, nt, e->filhos[i]);
    }
    return erros;
}

/*
 * ll1_verificar: percorre todas as regras dos nao-terminais da gramatica.
 * Retorna o numero de erros encontrados (avisos nao impedem a geracao).
 */
int ll1_verificar(const Gramatica *g)
{
    int i, erros = 0;
    for (i = 0; i < g->nnts; i++) {
        if (g->nts[i].regra != NULL) {
            erros += verificar_expr(g, i, g->nts[i].regra);
        }
    }
    return erros;
}