#include <stdio.h>
#include <stdlib.h>
#include "correcao.h"
#include "util.h"

static int recursao_esq_direta(const Expr *alt, int i) {
    if (alt->tipo == E_SEQ && 
        alt->filhos[0]->tipo == E_NAOTERM && 
        alt->filhos[0]->simbolo == i) {
        return 1;
    }
    return 0;
}

void corrige_ebnf(Gramatica *g) {
    int i, k, a, cont;
    Expr *regra, *alfa, *beta, *aux, *alt, *n_seq, *n_rep;
    for (i = 0; i < g->nnts; i++) {
        cont = 0;
        regra = g->nts[i].regra;
        if (regra == NULL || regra->tipo != E_ALT) {
            continue;
        }
        for (k = 0; k < regra->nfilhos; k++) {
            if (recursao_esq_direta(regra->filhos[k], i)) {
                cont++;
            }
        }
        if (cont > 0 && cont < regra->nfilhos) {
            alfa = expr_criar(E_ALT, regra->linha, regra->coluna);
            beta = expr_criar(E_ALT, regra->linha, regra->coluna);
            for (k = 0; k < regra->nfilhos; k++) {
                if (recursao_esq_direta(regra->filhos[k], i)) {
                    alt = regra->filhos[k];
                    aux = alt->filhos[0];
                    for (a = 0; a < alt->nfilhos - 1; a++) {
                        alt->filhos[a] = alt->filhos[a + 1];
                    }
                    alt->nfilhos--;
                    expr_liberar(aux);
                    if (alt->nfilhos == 1) {
                        aux = alt;
                        alt = alt->filhos[0];
                        free(aux->filhos);
                        free(aux);
                    }
                    expr_adicionar(alfa, alt);
                } else {
                    expr_adicionar(beta, regra->filhos[k]);
                }
            }
            if (alfa->nfilhos == 1) {
                aux = alfa;
                alfa = alfa->filhos[0];
                free(aux->filhos);
                free(aux);
            }
            if (beta->nfilhos == 1) {
                aux = beta;
                beta = beta->filhos[0];
                free(aux->filhos);
                free(aux);
            }
            n_seq = expr_criar(E_SEQ, regra->linha, regra->coluna);
            n_rep = expr_unario(E_REP, alfa, regra->linha, regra->coluna);
            expr_adicionar(n_seq, beta);
            expr_adicionar(n_seq, n_rep);
            g->nts[i].regra = n_seq;
            diag_aviso(regra->linha, regra->coluna, "recursao a esquerda direta corrigida em %s; a regra mudou para:", g->nts[i].nome);
            fprintf(stderr, "%s = ", g->nts[i].nome);
            expr_imprimir(stderr, g, g->nts[i].regra);
            fputs(" ;\n", stderr);
            free(regra->filhos);
            free(regra);
        }
    }
}