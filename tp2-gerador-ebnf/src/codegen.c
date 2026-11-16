/*
 * codegen.c - Geracao do analisador sintatico (etapa 5).
 *
 * Estrutura do arquivo .c gerado:
 *
 *   1. includes e conjuntos constantes (conj_0, conj_1, ...)
 *   2. estado e primitivas: avancar, match, erro_sintatico, arvore
 *   3. modo proprio: analisador lexico gerado a partir de TOKENS
 *      modo Micro C: tokens obtidos de proximo_token() (scanner do TP1)
 *   4. leitura de um arquivo de tokens (opcao --tokens do analisador)
 *   5. um procedimento parse_X() por nao terminal
 *   6. main
 *
 * Fornecido: toda a estrutura acima, os nomes dos tokens, os conjuntos,
 * as primitivas, o lexico (exceto o reconhecimento das expressoes
 * regulares) e a traducao de terminais e nao terminais.
 *
 * Implementado pelo grupo:
 *   - gen_expr: traducao de sequencia, alternativa, [ ] e { }
 *     (esquema da secao 4 do enunciado);
 *   - gen_casamento_regex: converte uma expressao regular de TOKENS em
 *     codigo C (automato finito) que reconhece o maior prefixo;
 *   - gen_token_de_arquivo: leitura do arquivo de tokens do TP1;
 *   - extras: recuperacao de erros (--recuperar) e arvore sintatica
 *     abstrata (--ast) no analisador gerado.
 */
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codegen.h"
#include "microc_mapa.h"
#include "util.h"

/* ------------------------------------------------------------------ */
/* Emissao de texto                                                   */
/* ------------------------------------------------------------------ */

typedef struct {
    FILE *f;
    int nivel;          /* nivel de indentacao (4 espacos por nivel) */
} Emissor;

/* Emite uma linha indentada; fmt segue printf. */
static void emit(Emissor *em, const char *fmt, ...)
{
    int i;
    va_list ap;
    for (i = 0; i < em->nivel; i++)
        fputs("    ", em->f);
    va_start(ap, fmt);
    vfprintf(em->f, fmt, ap);
    va_end(ap);
    fputc('\n', em->f);
}

/* Emite linhas fixas, sem interpretar '%'. A lista termina em NULL. */
static void emit_linhas(FILE *f, const char *const *linhas)
{
    for (; *linhas != NULL; linhas++) {
        fputs(*linhas, f);
        fputc('\n', f);
    }
}

/* Texto dentro de comentario C: evita fechar o comentario por engano. */
static void emitir_comentario(FILE *f, const char *s)
{
    for (; *s != '\0'; s++) {
        if (s[0] == '*' && s[1] == '/') {
            fputs("* /", f);
            s++;
        } else if (*s == '\n') {
            fputc(' ', f);
        } else {
            fputc(*s, f);
        }
    }
}

/* String C entre aspas, com escapes. */
static void emitir_string_c(FILE *f, const char *s)
{
    fputc('"', f);
    for (; *s != '\0'; s++) {
        unsigned char c = (unsigned char) *s;
        if (c == '"' || c == '\\')
            fprintf(f, "\\%c", c);
        else if (c == '\n')
            fputs("\\n", f);
        else if (c == '\t')
            fputs("\\t", f);
        else if (c < 32 || c > 126)
            fprintf(f, "\\%03o", c);
        else
            fputc(c, f);
    }
    fputc('"', f);
}

/* ------------------------------------------------------------------ */
/* Nomes dos tokens no codigo gerado                                  */
/* ------------------------------------------------------------------ */

static const char *const nomes_simbolos[][2] = {
    { "+", "MAIS" }, { "-", "MENOS" }, { "*", "VEZES" }, { "/", "BARRA" },
    { "%", "PORCENTO" }, { "(", "ABRE_PAR" }, { ")", "FECHA_PAR" },
    { "[", "ABRE_COL" }, { "]", "FECHA_COL" }, { "{", "ABRE_CHAVE" },
    { "}", "FECHA_CHAVE" }, { ";", "PV" }, { ",", "VIRGULA" },
    { ".", "PONTO" }, { ":", "DOIS_PONTOS" }, { "=", "IGUAL" },
    { "==", "IGUAL_IGUAL" }, { "!=", "DIFERENTE" }, { "<", "MENOR" },
    { ">", "MAIOR" }, { "<=", "MENOR_IGUAL" }, { ">=", "MAIOR_IGUAL" },
    { "&&", "E_LOGICO" }, { "||", "OU_LOGICO" }, { "!", "NAO" },
    { ":=", "ATRIBUICAO" }, { "->", "SETA" }
};

static int eh_palavra(const char *s)
{
    if (!isalpha((unsigned char) *s) && *s != '_')
        return 0;
    for (; *s != '\0'; s++)
        if (!isalnum((unsigned char) *s) && *s != '_')
            return 0;
    return 1;
}

static int nome_em_uso(const Gramatica *g, int ate, const char *nome)
{
    int i;
    for (i = 0; i < ate; i++)
        if (strcmp(g->terms[i].nome_c, nome) == 0)
            return 1;
    return 0;
}

/* Define g->terms[i].nome_c para todos os terminais. */
static void nomear_terminais(Gramatica *g, int microc)
{
    int i;
    size_t k;
    char buf[256];
    Terminal *t;

    for (i = 0; i < g->nterms; i++) {
        t = &g->terms[i];
        free(t->nome_c);
        if (i == TERM_FIM) {
            t->nome_c = xstrdup(microc ? "END_OF_FILE" : "T_FIM");
            continue;
        }
        if (microc) {
            t->nome_c = xstrdup(t->literal ? microc_nome_literal(t->nome) : t->nome);
            continue;
        }
        buf[0] = '\0';
        if (!t->literal) {
            sprintf(buf, "T_%.200s", t->nome);
        } else if (eh_palavra(t->nome) && strlen(t->nome) < 200) {
            sprintf(buf, "T_%s", t->nome);
            for (k = 2; buf[k] != '\0'; k++)
                buf[k] = (char) toupper((unsigned char) buf[k]);
        } else {
            for (k = 0; k < sizeof(nomes_simbolos) / sizeof(nomes_simbolos[0]); k++)
                if (strcmp(nomes_simbolos[k][0], t->nome) == 0)
                    sprintf(buf, "T_%s", nomes_simbolos[k][1]);
            if (buf[0] == '\0')
                sprintf(buf, "T_LIT_%d", i);
        }
        if (nome_em_uso(g, i, buf))
            sprintf(buf + strlen(buf), "_%d", i);
        t->nome_c = xstrdup(buf);
    }
}

/* ------------------------------------------------------------------ */
/* Conjuntos constantes                                               */
/* ------------------------------------------------------------------ */

#define MAX_COMPARACOES 3   /* ate 3 tokens: tok.tipo == A || ...; acima: pertence */

static Bitset *conjuntos = NULL;
static int nconj = 0, capconj = 0;

static int bitset_igual(const Bitset *a, const Bitset *b)
{
    int i;
    for (i = 0; i < a->n; i++)
        if (bitset_tem(a, i) != bitset_tem(b, i))
            return 0;
    return 1;
}

/* Registra um conjunto e devolve seu numero N (emitido como conj_N).
 * Conjuntos iguais compartilham a mesma constante. */
static int registrar_conjunto(const Bitset *b)
{
    int i;
    for (i = 0; i < nconj; i++)
        if (bitset_igual(&conjuntos[i], b))
            return i;
    if (nconj == capconj) {
        capconj = capconj ? 2 * capconj : 16;
        conjuntos = (Bitset *) xrealloc(conjuntos, (size_t) capconj * sizeof(Bitset));
    }
    bitset_iniciar(&conjuntos[nconj], b->n);
    bitset_copiar(&conjuntos[nconj], b);
    return nconj++;
}

static void liberar_conjuntos(void)
{
    int i;
    for (i = 0; i < nconj; i++)
        bitset_liberar(&conjuntos[i]);
    free(conjuntos);
    conjuntos = NULL;
    nconj = capconj = 0;
}

static void emitir_conjuntos(FILE *f, const Gramatica *g)
{
    int i, t;
    for (i = 0; i < nconj; i++) {
        fprintf(f, "static const int conj_%d[] = { ", i);
        for (t = bitset_proximo(&conjuntos[i], 0); t >= 0;
             t = bitset_proximo(&conjuntos[i], t + 1))
            fprintf(f, "%s, ", g->terms[t].nome_c);
        fputs("-1 };\n", f);
    }
}

/* Escreve a condicao "o token corrente pertence a b":
 *   tok.tipo == T_A                     (um token)
 *   tok.tipo == T_A || tok.tipo == T_B  (ate MAX_COMPARACOES)
 *   pertence(tok.tipo, conj_N)          (mais tokens) */
static void emitir_teste(FILE *f, const Gramatica *g, const Bitset *b)
{
    int t, n = bitset_tamanho(b), primeiro = 1;
    if (n == 0) {
        fputs("0", f);
    } else if (n <= MAX_COMPARACOES) {
        for (t = bitset_proximo(b, 0); t >= 0; t = bitset_proximo(b, t + 1)) {
            fprintf(f, "%stok.tipo == %s", primeiro ? "" : " || ", g->terms[t].nome_c);
            primeiro = 0;
        }
    } else {
        fprintf(f, "pertence(tok.tipo, conj_%d)", registrar_conjunto(b));
    }
}

/* Linha "<antes><teste><depois>", ex.: emit_teste(em, "while (", g, b, ") {") */
static void emit_teste(Emissor *em, const char *antes, const Gramatica *g,
                       const Bitset *b, const char *depois)
{
    int i;
    for (i = 0; i < em->nivel; i++)
        fputs("    ", em->f);
    fputs(antes, em->f);
    emitir_teste(em->f, g, b);
    fputs(depois, em->f);
    fputc('\n', em->f);
}

/* Linha "erro_sintatico(conj_N);" com N = conjunto dos tokens esperados. */
static void emit_erro(Emissor *em, const Bitset *esperados)
{
    emit(em, "erro_sintatico(conj_%d);", registrar_conjunto(esperados));
}

/* ------------------------------------------------------------------ */
/* Traducao das expressoes (esquema da secao 4 do enunciado)          */
/* ------------------------------------------------------------------ */

static void gen_expr(Emissor *em, const Gramatica *g, const Expr *e)
{
    /*
     * Traducao de cada construcao (secao 4):
     *
     *   E_TERM  t              ->  match(t)
     *   E_NAOTERM B            ->  parse_B()
     *   E_SEQ   a1 a2 ... an   ->  gen_expr de cada filho, em ordem
     *   E_ALT   a1 | ... | an  ->  if (FIRST(a1)) {...} else if ... else erro
     *   E_OPC   [ a ]          ->  if (FIRST(a)) { ... }
     *   E_REP   { a }          ->  while (FIRST(a)) { ... }
     *   E_VAZIO                ->  nada
     *
     * Funcoes usadas:
     *   emit(em, fmt, ...)                 uma linha indentada
     *   em->nivel++ / em->nivel--          indentacao dos blocos
     *   emit_teste(em, "if (", g, &x->first, ") {")
     *   emit_erro(em, &e->first)           erro_sintatico com os esperados
     * Os conjuntos FIRST de cada no foram calculados em conjuntos.c.
     * Uma alternativa anulavel nao tem teste: ela fica no else final,
     * no lugar do erro.
     */
    int i;
    Expr *no_anulavel = NULL;
    int primeiro = 1;

    switch (e->tipo) {
    case E_TERM:
        emit(em, "match(%s);", g->terms[e->simbolo].nome_c);
        break;
    case E_NAOTERM:
        emit(em, "parse_%s();", g->nts[e->simbolo].nome);
        break;
    case E_SEQ:
        /* sequencia (a1 a2 ... an): traduz cada filho em ordem */
        for (i = 0; i < e->nfilhos; i++)
            gen_expr(em, g, e->filhos[i]);
        break;
    case E_ALT:
        /* alternativa (a1 | a2 | ... | an):
         * procura se ha alguma alternativa anulavel */
        for (i = 0; i < e->nfilhos; i++) {
            if (e->filhos[i]->anulavel) {
                no_anulavel = e->filhos[i];
                break;
            }
        }
        /* emite os testes if (...) / else if (...) para as alternativas nao-anulaveis */
        for (i = 0; i < e->nfilhos; i++) {
            Expr *filho = e->filhos[i];
            if (filho == no_anulavel)
                continue;
            if (primeiro)   {
                emit_teste(em, "if (", g, &filho->first, ") {");
                primeiro = 0;
            } else {
                emit_teste(em, "} else if (", g, &filho->first, ") {");
            }
            em->nivel++;
            gen_expr(em, g, filho);
            em->nivel--;
        }
        /* trata o bloco else final */
        if (primeiro) {
            /* se nao houve nenhuma alternativa nao-anulavel */
            if (no_anulavel != NULL) {
                gen_expr(em, g, no_anulavel);
            } else {
                emit_erro(em, &e->first);
            }
        } else {
            emit(em, "} else {");
            em->nivel++;
            if (no_anulavel != NULL) {
                gen_expr(em, g, no_anulavel);
            } else {
                emit_erro(em, &e->first);
            }
            em->nivel--;
            emit(em, "}");
        }
        break;
    case E_OPC:
        /* opcional [ a ]: if (FIRST(a)) { T(a) } */
        if (e->nfilhos > 0) {
            emit_teste(em, "if (", g, &e->filhos[0]->first, ") {");
            em->nivel++;
            gen_expr(em, g, e->filhos[0]);
            em->nivel--;
            emit(em, "}");
        }
        break;
    case E_REP:
        /* repeticao { a }: while (FIRST(a)) { T(a) } */
        if (e->nfilhos > 0) {
            emit_teste(em, "while (", g, &e->filhos[0]->first, ") {");
            em->nivel++;
            gen_expr(em, g, e->filhos[0]);
            em->nivel--;
            emit(em, "}");
        }
        break;
    case E_VAZIO:
        /* cadeia vazia */
        break;
    default:
        emit(em, "/* construcao desconhecida */");
        (void) emit_teste;
        (void) emit_erro;
        break;
    }
}

/* ------------------------------------------------------------------ */
/* Expressoes regulares de TOKENS (modo proprio)                      */
/* ------------------------------------------------------------------ */

/* Cada expressao regular de TOKENS vira um automato finito
 * deterministico: a expressao e analisada, vira um AFN pela construcao
 * de Thompson e o AFN e determinizado pela construcao de subconjuntos.
 * O automato e emitido como codigo C: um switch sobre o estado, com as
 * transicoes por faixas de caracteres, guardando a ultima posicao em
 * que o automato passou por um estado final (maior prefixo). Aceita
 * concatenacao, |, *, +, ?, parenteses, classes [a-z] e [^...], o
 * ponto e escapes como \n, \t, \\ e \/. */

/* ------------------------------------------------------------------ */
/* Expressoes regulares de TOKENS (modo proprio)                      */
/* Automato Finito Deterministico (AFN -> AFD)                        */
/* ------------------------------------------------------------------ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_NFA_STATES 1024
#define MAX_DFA_STATES 512

typedef struct {
    int type; /* 0: CHAR, 1: EPSILON, 2: ACCEPT */
    unsigned char match[256];
    int out1;
    int out2;
} NFAState;

typedef struct {
    NFAState states[MAX_NFA_STATES];
    int nstates;
} NFA;

typedef struct {
    int start;
    int end;
} Frag;

typedef struct {
    const char *str;
    int pos;
    int error;
} RegexParser;

typedef struct {
    unsigned char nfa_set[MAX_NFA_STATES];
    int is_accept;
    int transitions[256];
} DFAState;

typedef struct {
    DFAState states[MAX_DFA_STATES];
    int nstates;
} DFA;

static Frag frag_invalid(void)
{
    Frag f;
    f.start = -1;
    f.end = -1;
    return f;
}

static int new_nfa_state(NFA *nfa, int type)
{
    int id;
    if (nfa->nstates >= MAX_NFA_STATES)
        return -1;
    id = nfa->nstates++;
    memset(&nfa->states[id], 0, sizeof(NFAState));
    nfa->states[id].type = type;
    nfa->states[id].out1 = -1;
    nfa->states[id].out2 = -1;
    return id;
}

static Frag frag_char(NFA *nfa, const unsigned char match[256])
{
    Frag f;
    int s_char = new_nfa_state(nfa, 0);
    int s_end = new_nfa_state(nfa, 1);
    if (s_char < 0 || s_end < 0)
        return frag_invalid();

    memcpy(nfa->states[s_char].match, match, 256);
    nfa->states[s_char].out1 = s_end;
    f.start = s_char;
    f.end = s_end;
    return f;
}

static Frag frag_cat(NFA *nfa, Frag f1, Frag f2)
{
    Frag f;
    if (f1.start < 0 || f2.start < 0)
        return frag_invalid();
    if (f1.end >= 0) {
        nfa->states[f1.end].out1 = f2.start;
    }
    f.start = f1.start;
    f.end = f2.end;
    return f;
}

static Frag frag_alt(NFA *nfa, Frag f1, Frag f2)
{
    Frag f;
    int s_start, s_end;
    if (f1.start < 0 || f2.start < 0)
        return frag_invalid();

    s_start = new_nfa_state(nfa, 1);
    s_end = new_nfa_state(nfa, 1);
    if (s_start < 0 || s_end < 0)
        return frag_invalid();

    nfa->states[s_start].out1 = f1.start;
    nfa->states[s_start].out2 = f2.start;
    if (f1.end >= 0) nfa->states[f1.end].out1 = s_end;
    if (f2.end >= 0) nfa->states[f2.end].out1 = s_end;
    f.start = s_start;
    f.end = s_end;
    return f;
}

static Frag frag_star(NFA *nfa, Frag f_in)
{
    Frag f;
    int s_start, s_end;
    if (f_in.start < 0)
        return frag_invalid();

    s_start = new_nfa_state(nfa, 1);
    s_end = new_nfa_state(nfa, 1);
    if (s_start < 0 || s_end < 0)
        return frag_invalid();

    nfa->states[s_start].out1 = f_in.start;
    nfa->states[s_start].out2 = s_end;
    if (f_in.end >= 0) {
        nfa->states[f_in.end].out1 = f_in.start;
        nfa->states[f_in.end].out2 = s_end;
    }
    f.start = s_start;
    f.end = s_end;
    return f;
}

static Frag frag_plus(NFA *nfa, Frag f_in)
{
    Frag f;
    int s_end;
    if (f_in.start < 0)
        return frag_invalid();

    s_end = new_nfa_state(nfa, 1);
    if (s_end < 0)
        return frag_invalid();

    if (f_in.end >= 0) {
        nfa->states[f_in.end].out1 = f_in.start;
        nfa->states[f_in.end].out2 = s_end;
    }
    f.start = f_in.start;
    f.end = s_end;
    return f;
}

static Frag frag_question(NFA *nfa, Frag f_in)
{
    Frag f;
    int s_start, s_end;
    if (f_in.start < 0)
        return frag_invalid();

    s_start = new_nfa_state(nfa, 1);
    s_end = new_nfa_state(nfa, 1);
    if (s_start < 0 || s_end < 0)
        return frag_invalid();

    nfa->states[s_start].out1 = f_in.start;
    nfa->states[s_start].out2 = s_end;
    if (f_in.end >= 0) {
        nfa->states[f_in.end].out1 = s_end;
    }
    f.start = s_start;
    f.end = s_end;
    return f;
}

static char rx_peek(RegexParser *p)
{
    return p->str[p->pos];
}

static char rx_get(RegexParser *p)
{
    if (p->str[p->pos] == '\0')
        return '\0';
    return p->str[p->pos++];
}

static int parse_escape(RegexParser *p)
{
    char c;
    if (rx_peek(p) == '\0') {
        p->error = 1;
        return 0;
    }

    c = rx_get(p);
    if (c == 'n') return '\n';
    if (c == 't') return '\t';
    if (c == 'r') return '\r';
    return (unsigned char) c;
}

static Frag parse_regex(RegexParser *p, NFA *nfa);
static Frag parse_concat(RegexParser *p, NFA *nfa);
static Frag parse_quant(RegexParser *p, NFA *nfa);
static Frag parse_atom(RegexParser *p, NFA *nfa);
static Frag parse_class(RegexParser *p, NFA *nfa);

static Frag parse_regex(RegexParser *p, NFA *nfa)
{
    Frag f1 = parse_concat(p, nfa);
    while (rx_peek(p) == '|' && !p->error) {
        Frag f2;
        rx_get(p);
        f2 = parse_concat(p, nfa);
        f1 = frag_alt(nfa, f1, f2);
    }
    if (f1.start < 0) p->error = 1;
    return f1;
}

static Frag parse_concat(RegexParser *p, NFA *nfa)
{
    Frag f1 = parse_quant(p, nfa);
    while (rx_peek(p) != '\0' && rx_peek(p) != '|' && rx_peek(p) != ')' && !p->error) {
        Frag f2 = parse_quant(p, nfa);
        f1 = frag_cat(nfa, f1, f2);
    }
    if (f1.start < 0) p->error = 1;
    return f1;
}

static Frag parse_quant(RegexParser *p, NFA *nfa)
{
    Frag f = parse_atom(p, nfa);
    char ch = rx_peek(p);
    if (p->error || f.start < 0) return frag_invalid();

    if (ch == '*') {
        rx_get(p);
        f = frag_star(nfa, f);
    } else if (ch == '+') {
        rx_get(p);
        f = frag_plus(nfa, f);
    } else if (ch == '?') {
        rx_get(p);
        f = frag_question(nfa, f);
    }
    if (f.start < 0) p->error = 1;
    return f;
}

static Frag parse_class(RegexParser *p, NFA *nfa)
{
    unsigned char match[256];
    int negate = 0;
    int k;
    Frag f;

    rx_get(p); /* consome '[' */
    memset(match, 0, 256);
    if (rx_peek(p) == '^') {
        negate = 1;
        rx_get(p);
    }
    while (rx_peek(p) != ']' && rx_peek(p) != '\0' && !p->error) {
        int c1, c2;
        if (rx_peek(p) == '\\') {
            rx_get(p);
            c1 = parse_escape(p);
        } else {
            c1 = (unsigned char) rx_get(p);
        }

        if (p->error) break;

        if (rx_peek(p) == '-' && p->str[p->pos + 1] != ']' && p->str[p->pos + 1] != '\0') {
            rx_get(p); /* consome '-' */
            if (rx_peek(p) == '\\') {
                rx_get(p);
                c2 = parse_escape(p);
            } else {
                c2 = (unsigned char) rx_get(p);
            }

            if (p->error) break;

            if (c1 <= c2) {
                for (k = c1; k <= c2; k++)
                    match[k] = 1;
            }
        } else {
            match[c1] = 1;
        }
    }

    if (p->error) return frag_invalid();

    if (rx_peek(p) == ']') {
        rx_get(p);
    } else {
        p->error = 1;
        return frag_invalid();
    }

    if (negate) {
        /* CORRECAO: inverte TODOS os bytes (inclusive '\n'); apenas o
         * byte 0 (terminador da string) nunca casa. */
        for (k = 1; k < 256; k++)
            match[k] = !match[k];
        match[0] = 0;
    }

    f = frag_char(nfa, match);
    if (f.start < 0) p->error = 1;
    return f;
}

static Frag parse_atom(RegexParser *p, NFA *nfa)
{
    char ch = rx_peek(p);
    unsigned char match[256];
    Frag f;

    if (ch == '(') {
        rx_get(p);
        f = parse_regex(p, nfa);
        if (rx_peek(p) == ')') {
            rx_get(p);
        } else {
            p->error = 1;
            return frag_invalid();
        }
        return f;
    } else if (ch == '[') {
        return parse_class(p, nfa);
    } else if (ch == '.') {
        rx_get(p);
        memset(match, 1, 256);
        match[0] = 0;
        match['\n'] = 0; /* o ponto nao casa newline (convencional) */
        f = frag_char(nfa, match);
        if (f.start < 0) p->error = 1;
        return f;
    } else if (ch == '\\') {
        int c;
        rx_get(p);
        c = parse_escape(p);
        if (p->error) return frag_invalid();
        memset(match, 0, 256);
        match[c] = 1;
        f = frag_char(nfa, match);
        if (f.start < 0) p->error = 1;
        return f;
    } else if (ch == '\0' || ch == '|' || ch == ')' || ch == '*' || ch == '+' || ch == '?') {
        p->error = 1;
        return frag_invalid();
    } else {
        int c = (unsigned char) rx_get(p);
        memset(match, 0, 256);
        match[c] = 1;
        f = frag_char(nfa, match);
        if (f.start < 0) p->error = 1;
        return f;
    }
}

static void epsilon_closure(const NFA *nfa, unsigned char *set)
{
    int stack[MAX_NFA_STATES];
    int top = 0;
    int i;
    for (i = 0; i < nfa->nstates; i++) {
        if (set[i]) {
            stack[top++] = i;
        }
    }

    while (top > 0) {
        int u = stack[--top];
        if (nfa->states[u].type == 1) { /* EPSILON */
            int o1 = nfa->states[u].out1;
            int o2 = nfa->states[u].out2;
            if (o1 >= 0 && !set[o1]) {
                set[o1] = 1;
                stack[top++] = o1;
            }
            if (o2 >= 0 && !set[o2]) {
                set[o2] = 1;
                stack[top++] = o2;
            }
        }
    }
}

static int find_or_add_dfa_state(DFA *dfa, const NFA *nfa, const unsigned char *set)
{
    int i, k;
    for (i = 0; i < dfa->nstates; i++) {
        if (memcmp(dfa->states[i].nfa_set, set, (size_t) nfa->nstates) == 0)
            return i;
    }
    if (dfa->nstates >= MAX_DFA_STATES)
        return -1;

    i = dfa->nstates++;
    memset(&dfa->states[i], 0, sizeof(DFAState));
    memcpy(dfa->states[i].nfa_set, set, (size_t) nfa->nstates);
    for (k = 0; k < 256; k++)
        dfa->states[i].transitions[k] = -1;

    for (k = 0; k < nfa->nstates; k++) {
        if (set[k] && nfa->states[k].type == 2) { /* ACCEPT */
            dfa->states[i].is_accept = 1;
            break;
        }
    }
    return i;
}

static int build_dfa(DFA *dfa, const NFA *nfa, int start_nfa)
{
    unsigned char initial_set[MAX_NFA_STATES];
    int queue[MAX_DFA_STATES];
    int q_head = 0, q_tail = 0;
    int d_start;

    dfa->nstates = 0;
    memset(initial_set, 0, (size_t) nfa->nstates);
    if (start_nfa >= 0)
        initial_set[start_nfa] = 1;
    epsilon_closure(nfa, initial_set);

    d_start = find_or_add_dfa_state(dfa, nfa, initial_set);
    if (d_start < 0) return -1;
    queue[q_tail++] = d_start;

    while (q_head < q_tail) {
        int u_dfa = queue[q_head++];
        int c;
        for (c = 0; c < 256; c++) {
            unsigned char next_set[MAX_NFA_STATES];
            int k, has_next = 0;
            memset(next_set, 0, (size_t) nfa->nstates);

            for (k = 0; k < nfa->nstates; k++) {
                if (dfa->states[u_dfa].nfa_set[k]) {
                    if (nfa->states[k].type == 0 && nfa->states[k].match[c]) {
                        int out = nfa->states[k].out1;
                        if (out >= 0) {
                            next_set[out] = 1;
                            has_next = 1;
                        }
                    }
                }
            }

            if (has_next) {
                int prev_count = dfa->nstates;
                int v_dfa;
                epsilon_closure(nfa, next_set);
                v_dfa = find_or_add_dfa_state(dfa, nfa, next_set);
                if (v_dfa < 0) return -1;
                dfa->states[u_dfa].transitions[c] = v_dfa;
                if (dfa->nstates > prev_count) {
                    if (q_tail >= MAX_DFA_STATES) return -1;
                    queue[q_tail++] = v_dfa;
                }
            }
        }
    }
    return 0;
}

static void emit_single_char_cond(FILE *f, int c)
{
    if (c == '\'')
        fputs("c == '\\''", f);
    else if (c == '\\')
        fputs("c == '\\\\'", f);
    else if (c == '\n')
        fputs("c == '\\n'", f);
    else if (c == '\t')
        fputs("c == '\\t'", f);
    else if (c == '\r')
        fputs("c == '\\r'", f);
    else if (c >= 32 && c <= 126)
        fprintf(f, "c == '%c'", c);
    else
        fprintf(f, "c == %d", c);
}

static void emit_range_cond(FILE *f, int start_c, int end_c)
{
    if (start_c == end_c) {
        emit_single_char_cond(f, start_c);
    } else if (start_c >= '0' && end_c <= '9') {
        fprintf(f, "c >= '%c' && c <= '%c'", start_c, end_c);
    } else if (start_c >= 'a' && end_c <= 'z') {
        fprintf(f, "c >= '%c' && c <= '%c'", start_c, end_c);
    } else if (start_c >= 'A' && end_c <= 'Z') {
        fprintf(f, "c >= '%c' && c <= '%c'", start_c, end_c);
    } else {
        fprintf(f, "c >= %d && c <= %d", start_c, end_c);
    }
}

/* Emite a funcao
 *     static size_t <nome>(const char *s)
 * que devolve o comprimento do MAIOR prefixo de s reconhecido pela
 * expressao regular, ou 0 se nenhum prefixo for reconhecido.
 * Retorna 0 se a expressao for valida (senao reporta com diag_erro). */
static int gen_casamento_regex(FILE *f, const char *nome, const char *regex)
{
    NFA *nfa = NULL;
    DFA *dfa = NULL;
    RegexParser parser;
    Frag final_frag;
    int d, c;

    if (regex == NULL || regex[0] == '\0') {
        diag_erro(0, 0, "expressao regular vazia ou nula");
        return -1;
    }

    nfa = (NFA *) malloc(sizeof(NFA));
    dfa = (DFA *) malloc(sizeof(DFA));

    if (nfa == NULL || dfa == NULL) {
        diag_erro(0, 0, "memoria insuficiente para alocar automato da regex '%s'", regex);
        free(nfa);
        free(dfa);
        return -1;
    }

    memset(nfa, 0, sizeof(NFA));
    memset(dfa, 0, sizeof(DFA));

    parser.str = regex;
    parser.pos = 0;
    parser.error = 0;

    final_frag = parse_regex(&parser, nfa);
    if (parser.error || parser.str[parser.pos] != '\0' || final_frag.start < 0) {
        diag_erro(0, 0, "expressao regular invalida '%s'", regex);
        free(nfa);
        free(dfa);
        return -1;
    }

    nfa->states[final_frag.end].type = 2; /* ACCEPT */

    if (build_dfa(dfa, nfa, final_frag.start) != 0) {
        diag_erro(0, 0, "falha ao construir automato para '%s'", regex);
        free(nfa);
        free(dfa);
        return -1;
    }

    fprintf(f, "static size_t %s(const char *s)\n{\n", nome);
    fputs("    size_t i = 0;\n", f);
    fputs("    size_t melhor = 0;\n", f);
    fputs("    int estado = 0;\n\n", f);
    fputs("    if (s == NULL)\n        return 0;\n\n", f);
    fputs("    while (s[i] != '\\0') {\n", f);
    fputs("        unsigned char c = (unsigned char) s[i];\n", f);
    fputs("        int prox = -1;\n", f);
    fputs("        switch (estado) {\n", f);

    for (d = 0; d < dfa->nstates; d++) {
        int first_if = 1;
        fprintf(f, "        case %d:\n", d);
        c = 0;
        while (c < 256) {
            int target = dfa->states[d].transitions[c];
            if (target != -1) {
                int start_c = c;
                int end_c;
                while (c + 1 < 256 && dfa->states[d].transitions[c + 1] == target) {
                    c++;
                }
                end_c = c;
                if (first_if) {
                    fputs("            if (", f);
                    first_if = 0;
                } else {
                    fputs(" else if (", f);
                }
                emit_range_cond(f, start_c, end_c);
                fprintf(f, ") prox = %d;", target);
            }
            c++;
        }
        if (first_if) {
            fputs("            prox = -1;\n", f);
        } else {
            fputs("\n", f);
        }
        fputs("            break;\n", f);
    }

    fputs("        default:\n", f);
    fputs("            break;\n", f);
    fputs("        }\n", f);
    fputs("        if (prox == -1)\n", f);
    fputs("            break;\n", f);
    fputs("        estado = prox;\n", f);
    fputs("        i++;\n", f);

    {
        int first_acc = 1;
        for (d = 0; d < dfa->nstates; d++) {
            if (dfa->states[d].is_accept) {
                if (first_acc) {
                    fprintf(f, "        if (estado == %d", d);
                    first_acc = 0;
                } else {
                    fprintf(f, " || estado == %d", d);
                }
            }
        }
        if (!first_acc) {
            fputs(")\n            melhor = i;\n", f);
        }
    }

    fputs("    }\n", f);
    fputs("    return melhor;\n}\n\n", f);

    free(nfa);
    free(dfa);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Leitura do arquivo de tokens (opcao --tokens do analisador gerado) */
/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ */
/* Leitura do arquivo de tokens (opcao --tokens do analisador gerado) */
/* ------------------------------------------------------------------ */

/* Emite "static Token token_de_arquivo(void)", que le o proximo token
 * do arquivo aberto em arquivo_tokens, no formato gravado pelo lexer do
 * TP1 (livro-texto, secao 3.3.2):
 *
 *   Token: tipo = <numero>, lexema = '<texto>', linha = <numero>
 *
 * Ao chegar ao fim do arquivo devolve um token TOKEN_FIM. */
static void gen_token_de_arquivo(FILE *f, int microc)
{
    static const char *const linhas[] = {
        "static Token token_de_arquivo(void)",
        "{",
        "    static int linha_tok = 0;",
        "    char buf[1024];",
        "    Token t;",
        "    memset(&t, 0, sizeof t);",
        "",
        "    while (fgets(buf, sizeof buf, arquivo_tokens) != NULL) {",
        "        char *p = buf;",
        "        char *lex_start, *linha_start;",
        "        int tipo_val, linha_val;",
        "        size_t len;",
        "",
        "        linha_tok++;",
        "        while (*p == ' ' || *p == '\\t' || *p == '\\r' || *p == '\\n')",
        "            p++;",
        "        if (*p == '\\0')",
        "            continue;",
        "",
        "        if (sscanf(p, \"Token: tipo = %d\", &tipo_val) != 1) {",
        "            fprintf(stderr, \"linha %d: linha malformada no arquivo de tokens\\n\", linha_tok);",
        "            exit(EXIT_FAILURE);",
        "        }",
        "        lex_start = strstr(p, \", lexema = '\");",
        "        if (lex_start == NULL) {",
        "            fprintf(stderr, \"linha %d: linha malformada no arquivo de tokens\\n\", linha_tok);",
        "            exit(EXIT_FAILURE);",
        "        }",
        "        lex_start += 12; /* tamanho de \", lexema = '\" */",
        "        linha_start = strstr(lex_start, \"', linha = \");",
        "        if (linha_start == NULL) {",
        "            fprintf(stderr, \"linha %d: linha malformada no arquivo de tokens\\n\", linha_tok);",
        "            exit(EXIT_FAILURE);",
        "        }",
        "        if (sscanf(linha_start + 11, \"%d\", &linha_val) != 1) {",
        "            fprintf(stderr, \"linha %d: linha malformada no arquivo de tokens\\n\", linha_tok);",
        "            exit(EXIT_FAILURE);",
        "        }",
        "        len = (size_t)(linha_start - lex_start);",
        "        if (len >= sizeof t.lexema)",
        "            len = sizeof t.lexema - 1;",
        "        memcpy(t.lexema, lex_start, len);",
        "        t.lexema[len] = '\\0';",
        "        t.tipo = tipo_val;",
        "        t.linha = linha_val;",
        "        return t;",
        "    }",
        "    t.tipo = TOKEN_FIM;",
        "    t.lexema[0] = '\\0';",
        "    return t;",
        "}",
        "",
        NULL
    };
    (void) microc;
    emit_linhas(f, linhas);
}

/* ------------------------------------------------------------------ */
/* Partes fixas do codigo gerado                                      */
/* ------------------------------------------------------------------ */

static const char *const rt_estado[] = {
    "/* ---- estado do analisador ---- */",
    "",
    "static Token tok;                  /* token corrente (lookahead) */",
    "static int imprimir_arvore = 0;    /* opcao --tree */",
    "static int nivel_arvore = 0;",
    "static FILE *arquivo_tokens = NULL; /* opcao --tokens */",
    "",
    "static Token token_de_arquivo(void);",
    "",
    "static int recupera_erros = 0; /* opcao --recuperar */",
    "",
    "static int contador_erros = 0;",
    "",
    "static int modo_panico = 0;",
    "",
    "typedef struct No {",
    "    const char *nome;",
    "    char lexema[100];",
    "    struct No **filhos;",
    "    struct No *pai;",
    "    int n_filhos;",
    "    int tam_total;",
    "} No;",
    "",
    "static No *raiz_ast = NULL;",
    "static No *atual_ast = NULL;",
    "",
    "static int imprime_ast = 0; /* opcao --ast */",
    NULL
};

static const char *const rt_primitivas[] = {
    "/* ---- primitivas ---- */",
    "",
    "int pertence(int tipo, const int *conjunto)",
    "{",
    "    for (; *conjunto != -1; conjunto++)",
    "        if (*conjunto == tipo)",
    "            return 1;",
    "    return 0;",
    "}",
    "",
    "/* Reporta o erro com os tokens esperados (lista terminada em -1) e encerra caso o modo --recuperar esteja desligado. */",
    "void erro_sintatico(const int *esperados)",
    "{",    
    "    int i;",
    "    if (modo_panico == 0) {",
    "        modo_panico = 1;",
    "        escrever_posicao();",
    "        if ((int) tok.tipo == TOKEN_FIM)",
    "            fprintf(stderr, \"encontrado fim de arquivo, \");",
    "        else",
    "            fprintf(stderr, \"encontrado '%s', \", tok.lexema);",
    "        if (esperados[0] != -1 && esperados[1] == -1) {",
    "            fprintf(stderr, \"esperado %s\\n\", nome_token(esperados[0]));",
    "        } else {",
    "            fprintf(stderr, \"esperado um de {\");",
    "            for (i = 0; esperados[i] != -1; i++)",
    "                fprintf(stderr, \"%s%s\", i > 0 ? \", \" : \"\", nome_token(esperados[i]));",
    "            fprintf(stderr, \"}\\n\");",
    "        }",
    "    }",
    "    contador_erros += 1;",
    "    if (recupera_erros == 0) {",
    "        exit(EXIT_FAILURE);",
    "    }",
    "}",
    "",
    "No *criar_ast(const char *nome, const char *lexema)",
    "{",
    "    No* no_ast = calloc(1, sizeof(No));",
    "    if (no_ast == NULL) {",
    "        fprintf(stderr, \"nao foi possivel alocar espaco para o no ast\\n\");",
    "        exit(EXIT_FAILURE);",
    "    }",
    "    no_ast->nome = nome;",
    "    strcpy(no_ast->lexema, lexema);",
    "    no_ast->filhos = NULL;",
    "    no_ast->pai = NULL;",
    "    no_ast->n_filhos = 0;",
    "    no_ast->tam_total = 0;",
    "    return no_ast;",
    "}",
    "",
    "void adicionar_filho_ast(No *pai, No *filho)",
    "{",
    "    int novo_tam = 0;",
    "    if (pai->n_filhos == pai->tam_total) {",
    "        if (pai->tam_total == 0) {",
    "            novo_tam = 4;",
    "            pai->filhos = realloc(pai->filhos, novo_tam*sizeof(No *));",
    "            if (pai->filhos == NULL) {",
    "                fprintf(stderr, \"memoria insuficiente\\n\");",
    "                exit(EXIT_FAILURE);",
    "            }",
    "            pai->tam_total = novo_tam;",
    "        } else {",
    "            novo_tam = 2*pai->tam_total;",
    "            pai->filhos = realloc(pai->filhos, novo_tam*sizeof(No *));",
    "            if (pai->filhos == NULL) {",
    "                fprintf(stderr, \"memoria insuficiente\\n\");",
    "                exit(EXIT_FAILURE);",
    "            }",
    "            pai->tam_total = novo_tam;}",
    "    }",
    "    pai->filhos[pai->n_filhos] = filho;",
    "    pai->n_filhos++;",
    "    filho->pai = pai;",
    "}",
    "",
    "void imprimir_ast(No *no_ast, int nivel)",
    "{",
    "    int i = 0;",
    "    for (i = 0; i < 2*nivel; i++) { printf(\" \"); }",
    "    printf(\"%s\", no_ast->nome);",
    "    if (no_ast->lexema[0] != \'\\0\') { printf(\" \'%s\'\", no_ast->lexema); }",
    "    printf(\"\\n\");",
    "    for (i = 0; i < no_ast->n_filhos; i++) {",
    "        imprimir_ast(no_ast->filhos[i], nivel+1);",
    "    }",
    "}",
    "",
    "void liberar_ast(No *no_ast)",
    "{",
    "    int i = 0;",
    "    if (no_ast == NULL) {",
    "        return ;}",
    "    for (i = 0; i < no_ast->n_filhos; i++) {;",
    "        liberar_ast(no_ast->filhos[i]);}",
    "    free(no_ast->filhos);",
    "    free(no_ast);",
    "}",
    "",
    "int eh_pontuacao(const char *nome)",
    "{",
    "    static const char *const descarta[] = {\"'('\", \"')'\", \"'['\", \"']'\", \"'{'\", \"'}'\", \"';'\", \"','\", NULL};",
    "    int i = 0;",
    "    while (descarta[i] != NULL) {",
    "        if (strcmp(nome, descarta[i++]) == 0) {",
    "            return 1;",
    "        }",
    "    }",
    "    return 0;",
    "}",
    "",
    "No *simplificar_ast(No *no_ast)",
    "{",
    "    int i = 0;",
    "    int k = 0;",
    "    No *no_valido;",
    "    No *filho_unico;",
    "    if (no_ast == NULL) {",
    "        return no_ast;",
    "    }",
    "    if (no_ast->n_filhos == 0) {",
    "        if (eh_pontuacao(no_ast->nome)) {",
    "            liberar_ast(no_ast);",
    "            return NULL;",
    "        } else {",
    "            return no_ast ;",
    "        }",
    "    }",
    "    for (i = 0; i < no_ast->n_filhos; i++) {",
    "        no_valido = simplificar_ast(no_ast->filhos[i]);",
    "        if (no_valido != NULL) {",
    "            no_ast->filhos[k] = no_valido;",
    "            no_valido->pai = no_ast;",
    "            k++;",
    "        }",
    "    }",
    "    no_ast->n_filhos = k;",
    "    if (k == 1) {",
    "        filho_unico = no_ast->filhos[0];",
    "        filho_unico->pai = no_ast->pai;",
    "        free(no_ast->filhos);",
    "        free(no_ast);",
    "        return filho_unico;",
    "    }",
    "    return no_ast ;"
    "}",
    "",
    "static void recuo(void)",
    "{",
    "    int i;",
    "    for (i = 0; i < nivel_arvore; i++)",
    "        fputs(\"  \", stdout);",
    "}",
    "",
    "void arvore_entra(const char *nao_terminal)",
    "{",
    "    if (imprimir_arvore) {",
    "        recuo();",
    "        printf(\"%s\\n\", nao_terminal);",
    "    }",
    "    if (imprime_ast) {",
    "        No *no = criar_ast(nao_terminal, \"\");",
    "        if (atual_ast == NULL) {",
    "            raiz_ast = no ;",
    "        } else {",
    "             adicionar_filho_ast(atual_ast, no);",
    "        }",
    "        atual_ast = no;",
    "    }",
    "    nivel_arvore++;",
    "}",
    "",
    "void arvore_sai(void)",
    "{",
    "    if (imprime_ast) {",
    "        if (atual_ast != NULL) {",
    "            atual_ast = atual_ast->pai ;",
    "        }",
    "    }",
    "    nivel_arvore--;",
    "}",
    "",
    "/* Consome o token corrente se for do tipo esperado; senao, erro. */",
    "void match(int esperado)",
    "{",
    "    int lista[2];",
    "    char lex[100];",
    "    if ((int) tok.tipo != esperado) {",
    "        lista[0] = esperado;",
    "        lista[1] = -1;",
    "        erro_sintatico(lista);",
    "        return ;",
    "    }",
    "    if (SUPORTE_ARVORE && imprimir_arvore) {",
    "        recuo();",
    "        if (nome_token(tok.tipo)[0] == '\\'')",
    "            printf(\"%s\\n\", nome_token(tok.tipo));",
    "        else",
    "            printf(\"%s '%s'\\n\", nome_token(tok.tipo), tok.lexema);",
    "    }",
    "    modo_panico = 0;",
    "    if (imprime_ast && atual_ast != NULL) {",
    "        No *no;",
    "        if (nome_token(tok.tipo)[0] == '\\'')",
    "            strcpy(lex, \"\");",
    "        else",
    "            strcpy(lex, tok.lexema);",
    "        no = criar_ast(nome_token(tok.tipo), lex);",
    "        adicionar_filho_ast(atual_ast, no);",
    "    }",
    "    avancar();",
    "}",
    "",
    NULL
};

/* Lexico proprio: partes fixas antes e depois das funcoes de regex. */
static const char *const rt_lexico_estado[] = {
    "/* ---- analisador lexico gerado a partir de TOKENS ---- */",
    "",
    "static char *fonte = NULL;",
    "static size_t pos = 0;",
    "static int linha = 1, coluna = 1;",
    "",
    "static char *ler_tudo(FILE *f)",
    "{",
    "    size_t cap = 4096, n = 0, lidos;",
    "    char *buf = (char *) malloc(cap), *novo;",
    "    if (buf == NULL) {",
    "        fprintf(stderr, \"memoria insuficiente\\n\");",
    "        exit(EXIT_FAILURE);",
    "    }",
    "    while ((lidos = fread(buf + n, 1, cap - n - 1, f)) > 0) {",
    "        n += lidos;",
    "        if (cap - n - 1 == 0) {",
    "            cap *= 2;",
    "            novo = (char *) realloc(buf, cap);",
    "            if (novo == NULL) {",
    "                fprintf(stderr, \"memoria insuficiente\\n\");",
    "                exit(EXIT_FAILURE);",
    "            }",
    "            buf = novo;",
    "        }",
    "    }",
    "    buf[n] = '\\0';",
    "    return buf;",
    "}",
    "",
    "static void consumir(size_t n)",
    "{",
    "    for (; n > 0 && fonte[pos] != '\\0'; n--, pos++) {",
    "        if (fonte[pos] == '\\n') {",
    "            linha++;",
    "            coluna = 1;",
    "        } else {",
    "            coluna++;",
    "        }",
    "    }",
    "}",
    "",
    NULL
};

static void escrever_cabecalho_c(FILE *f, const OpcoesGeracao *op)
{
    fputs("/*\n * Analisador sintatico descendente recursivo gerado a partir de ", f);
    emitir_comentario(f, op->origem);
    fputs(".\n * Arquivo gerado automaticamente: nao edite.\n */\n", f);
}

/* ------------------------------------------------------------------ */
/* Arquivo .h                                                         */
/* ------------------------------------------------------------------ */

static int gerar_h(const Gramatica *g, const OpcoesGeracao *op)
{
    char *caminho = (char *) xmalloc(strlen(op->prefixo) + 3);
    FILE *f;
    int i;

    sprintf(caminho, "%s.h", op->prefixo);
    f = fopen(caminho, "w");
    if (f == NULL) {
        diag_erro(0, 0, "nao foi possivel criar %s", caminho);
        free(caminho);
        return -1;
    }
    escrever_cabecalho_c(f, op);
    fputs("#ifndef PARSER_GERADO_H\n#define PARSER_GERADO_H\n\n", f);
    if (op->microc) {
        fputs("#include \"scanner.h\"   /* scanner do TP1: Token, TokenType, proximo_token */\n\n", f);
        fputs("#define TOKEN_FIM END_OF_FILE\n\n", f);
    } else {
        fputs("typedef enum {\n", f);
        for (i = 0; i < g->nterms; i++) {
            fprintf(f, "    %s%s   /* ", g->terms[i].nome_c, i + 1 < g->nterms ? "," : "");
            emitir_comentario(f, g->terms[i].exibicao);
            fputs(" */\n", f);
        }
        fputs("} TipoToken;\n\n", f);
        fputs("typedef struct {\n    int tipo;\n    char lexema[100];\n"
              "    int linha;\n    int coluna;\n} Token;\n\n", f);
        fputs("#define TOKEN_FIM T_FIM\n\n", f);
    }
    fputs("const char *nome_token(int tipo);\n", f);
    fputs("int  pertence(int tipo, const int *conjunto);\n", f);
    fputs("void avancar(void);\n", f);
    fputs("void match(int esperado);\n", f);
    fputs("void erro_sintatico(const int *esperados);\n", f);
    fputs("void arvore_entra(const char *nao_terminal);\n", f);
    fputs("void arvore_sai(void);\n\n", f);
    for (i = 0; i < g->nnts; i++)
        if (g->nts[i].regra != NULL)
            fprintf(f, "void parse_%s(void);\n", g->nts[i].nome);
    fputs("\n#endif\n", f);
    fclose(f);
    free(caminho);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Arquivo .c                                                         */
/* ------------------------------------------------------------------ */

static void gerar_nome_token(FILE *f, const Gramatica *g)
{
    int i;
    fputs("const char *nome_token(int tipo)\n{\n    switch (tipo) {\n", f);
    for (i = 0; i < g->nterms; i++) {
        fprintf(f, "    case %s: return ", g->terms[i].nome_c);
        emitir_string_c(f, i == TERM_FIM ? "fim de arquivo" : g->terms[i].exibicao);
        fputs(";\n", f);
    }
    fputs("    default: return \"token desconhecido\";\n    }\n}\n\n", f);
}

static void gerar_posicao_e_avancar(FILE *f, int microc)
{
    fputs("static void escrever_posicao(void)\n{\n", f);
    if (microc)
        fputs("    fprintf(stderr, \"linha %d: \", tok.linha);\n", f);
    else
        fputs("    fprintf(stderr, \"linha %d, coluna %d: \", tok.linha, tok.coluna);\n", f);
    fputs("}\n\n", f);

    fputs("/* Le o proximo token para tok. */\nvoid avancar(void)\n{\n", f);
    fputs("    if (arquivo_tokens != NULL)\n        tok = token_de_arquivo();\n    else\n", f);
    if (microc) {
        fputs("        tok = proximo_token();\n", f);
        fputs("    if (tok.tipo == UNDEF) {\n", f);
        fputs("        fprintf(stderr, \"linha %d: erro lexico: %s\\n\", tok.linha, tok.lexema);\n", f);
        fputs("        exit(EXIT_FAILURE);\n    }\n", f);
    } else {
        fputs("        tok = proximo_token_gerado();\n", f);
    }
    fputs("}\n\n", f);
}

static int gerar_lexico(FILE *f, const Gramatica *g)
{
    int i, nlit = 0, erros = 0;
    char nome[300];

    emit_linhas(f, rt_lexico_estado);

    for (i = 0; i < g->nskips; i++) {
        sprintf(nome, "casa_skip_%d", i);
        fputs("/* SKIP: ", f);
        emitir_comentario(f, g->skips[i]);
        fputs(" */\n", f);
        if (gen_casamento_regex(f, nome, g->skips[i]) != 0)
            erros++;
    }
    for (i = 1; i < g->nterms; i++) {
        if (g->terms[i].literal) {
            nlit++;
            continue;
        }
        sprintf(nome, "casa_%.250s", g->terms[i].nome_c);
        fprintf(f, "/* %s: ", g->terms[i].nome);
        emitir_comentario(f, g->terms[i].regex ? g->terms[i].regex : "");
        fputs(" */\n", f);
        if (gen_casamento_regex(f, nome, g->terms[i].regex ? g->terms[i].regex : "")
            != 0)
            erros++;
    }

    /* literais: comparacao direta, com prioridade em caso de empate */
    fputs("static const char *const texto_literais[] = {\n", f);
    for (i = 1; i < g->nterms; i++)
        if (g->terms[i].literal) {
            fputs("    ", f);
            emitir_string_c(f, g->terms[i].nome);
            fputs(",\n", f);
        }
    fputs("    NULL\n};\n\nstatic const int tipo_literais[] = {\n", f);
    for (i = 1; i < g->nterms; i++)
        if (g->terms[i].literal)
            fprintf(f, "    %s,\n", g->terms[i].nome_c);
    fputs("    -1\n};\n\n", f);

    fputs("/* Reconhece o maior prefixo da entrada; em caso de empate vence\n"
          " * o literal (palavras reservadas tem prioridade sobre ID). */\n", f);
    fputs("static Token proximo_token_gerado(void)\n{\n", f);
    fputs("    Token t;\n    size_t n, melhor;\n    int i, tipo, pulou;\n\n", f);
    fputs("    do {\n        pulou = 0;\n", f);
    for (i = 0; i < g->nskips; i++)
        fprintf(f, "        if ((n = casa_skip_%d(fonte + pos)) > 0) {\n"
                   "            consumir(n);\n            pulou = 1;\n        }\n", i);
    fputs("    } while (pulou);\n\n", f);
    fputs("    memset(&t, 0, sizeof t);\n", f);
    fputs("    t.linha = linha;\n    t.coluna = coluna;\n", f);
    fputs("    if (fonte[pos] == '\\0') {\n        t.tipo = T_FIM;\n        return t;\n    }\n", f);
    fputs("    melhor = 0;\n    tipo = -1;\n", f);
    fputs("    for (i = 0; texto_literais[i] != NULL; i++) {\n", f);
    fputs("        n = strlen(texto_literais[i]);\n", f);
    fputs("        if (n > melhor && strncmp(fonte + pos, texto_literais[i], n) == 0) {\n", f);
    fputs("            melhor = n;\n            tipo = tipo_literais[i];\n        }\n    }\n", f);
    for (i = 1; i < g->nterms; i++)
        if (!g->terms[i].literal)
            fprintf(f, "    if ((n = casa_%s(fonte + pos)) > melhor) {\n"
                       "        melhor = n;\n        tipo = %s;\n    }\n",
                    g->terms[i].nome_c, g->terms[i].nome_c);
    fputs("    if (melhor == 0) {\n", f);
    fputs("        fprintf(stderr, \"linha %d, coluna %d: erro lexico: caractere "
          "invalido '%c'\\n\",\n                linha, coluna, fonte[pos]);\n", f);
    fputs("        exit(EXIT_FAILURE);\n    }\n", f);
    fputs("    n = melhor < sizeof t.lexema ? melhor : sizeof t.lexema - 1;\n", f);
    fputs("    memcpy(t.lexema, fonte + pos, n);\n    t.lexema[n] = '\\0';\n", f);
    fputs("    t.tipo = tipo;\n    consumir(melhor);\n    return t;\n}\n\n", f);
    (void) nlit;
    return erros;
}

static void gerar_procedimentos(FILE *f, const Gramatica *g, const OpcoesGeracao *op)
{
    int i;
    Emissor em;
    em.f = f;
    em.nivel = 0;

    fputs("/* ---- procedimentos (um por nao terminal) ---- */\n\n", f);
    for (i = 0; i < g->nnts; i++) {
        if (g->nts[i].regra == NULL)
            continue;
        fprintf(f, "/* %s = ", g->nts[i].nome);
        {
            /* a regra em EBNF, sem "* /" */
            FILE *tmp = tmpfile();
            int c;
            if (tmp != NULL) {
                char buf[2] = { 0, 0 };
                expr_imprimir(tmp, g, g->nts[i].regra);
                rewind(tmp);
                while ((c = fgetc(tmp)) != EOF) {
                    buf[0] = (char) c;
                    emitir_comentario(f, buf);
                }
                fclose(tmp);
            }
        }
        fputs(" ; */\n", f);
        fprintf(f, "void parse_%s(void)\n{\n", g->nts[i].nome);
        em.nivel = 1;
        if (op->arvore)
            emit(&em, "arvore_entra(\"%s\");", g->nts[i].nome);
        gen_expr(&em, g, g->nts[i].regra);
        emit_teste(&em, "while (modo_panico && !(", g, &g->nts[i].follow, ") && tok.tipo != TOKEN_FIM) {");
        em.nivel++;
        emit(&em, "avancar();");
        em.nivel--;
        emit(&em, "}");
        if (op->arvore)
            emit(&em, "arvore_sai();");
        fputs("}\n\n", f);
    }
}

static void gerar_main(FILE *f, const Gramatica *g, const OpcoesGeracao *op)
{
    fputs("/* ---- programa principal ---- */\n\n", f);
    fputs("int main(int argc, char **argv)\n{\n", f);
    fputs("    int i, fim[2];\n    const char *arquivo = NULL, *tokens = NULL;\n", f);
    fputs("    FILE *entrada = stdin;\n\n", f);
    fputs("    for (i = 1; i < argc; i++) {\n", f);
    fputs("        if (strcmp(argv[i], \"--tree\") == 0) {\n", f);
    fputs("            imprimir_arvore = 1;\n", f);
    fputs("        } else if (strcmp(argv[i], \"--tokens\") == 0 && i + 1 < argc) {\n", f);
    fputs("            tokens = argv[++i];\n", f);
    fputs("        } else if (strcmp(argv[i], \"--recuperar\") == 0) {\n", f);
    fputs("            recupera_erros = 1;\n", f);
    fputs("        } else if (strcmp(argv[i], \"--ast\") == 0) {\n", f);
    fputs("            imprime_ast = 1;\n", f);
    fputs("        } else if (argv[i][0] == '-' && argv[i][1] != '\\0') {\n", f);
    fputs("            fprintf(stderr, \"uso: %s [--tree] [--recuperar] [--ast] [--tokens arquivo.tok | "
          "programa]\\n\", argv[0]);\n", f);
    fputs("            return 2;\n        } else {\n            arquivo = argv[i];\n"
          "        }\n    }\n", f);
    fputs("    if (imprimir_arvore && !SUPORTE_ARVORE)\n", f);
    fputs("        fprintf(stderr, \"aviso: gere o analisador com --tree para "
          "imprimir a arvore\\n\");\n\n", f);
    fputs("    if (imprime_ast && !SUPORTE_ARVORE)\n", f);
    fputs("        fprintf(stderr, \"aviso: gere o analisador com --tree antes para "
          "imprimir a arvore sintatica depois com --ast\\n\");\n\n", f);
    fputs("    if (tokens != NULL) {\n", f);
    fputs("        arquivo_tokens = fopen(tokens, \"r\");\n", f);
    fputs("        if (arquivo_tokens == NULL) {\n", f);
    fputs("            fprintf(stderr, \"nao foi possivel abrir %s\\n\", tokens);\n", f);
    fputs("            return 2;\n        }\n    } else {\n", f);
    fputs("        if (arquivo != NULL && (entrada = fopen(arquivo, \"r\")) == NULL) {\n", f);
    fputs("            fprintf(stderr, \"nao foi possivel abrir %s\\n\", arquivo);\n", f);
    fputs("            return 2;\n        }\n", f);
    if (op->microc)
        fputs("        scanner_iniciar(entrada);\n", f);
    else
        fputs("        fonte = ler_tudo(entrada);\n", f);
    fputs("    }\n\n", f);
    fputs("    avancar();\n", f);
    fprintf(f, "    parse_%s();\n", g->nts[g->inicial].nome);
    fputs("    if (tok.tipo != TOKEN_FIM) {\n", f);
    fputs("        fim[0] = TOKEN_FIM;\n        fim[1] = -1;\n", f);
    fputs("        erro_sintatico(fim);\n    }\n", f);
    fputs("    if (contador_erros == 0) {\n", f);
    fputs("        printf(\"entrada aceita\\n\");\n", f);
    fputs("    }\n", f);
    fputs("    if (imprime_ast == 1 && contador_erros == 0 && raiz_ast != NULL) {\n", f);
    fputs("        raiz_ast = simplificar_ast(raiz_ast);\n", f);    
    fputs("        imprimir_ast(raiz_ast, 0);\n", f);
    fputs("    }\n", f);
    fputs("    if (arquivo_tokens != NULL) {\n        fclose(arquivo_tokens);\n", f);
    fputs("    } else {\n", f);
    if (op->microc)
        fputs("        scanner_finalizar();\n", f);
    else
        fputs("        free(fonte);\n", f);
    fputs("        if (entrada != stdin)\n            fclose(entrada);\n    }\n", f);
    fputs("    liberar_ast(raiz_ast);\n", f);
    fputs("    if (contador_erros > 0) {\n        return 1;\n    }\n", f);
    fputs("    return 0;\n", f);
    fputs("}\n", f);
}

static const char *nome_base(const char *caminho)
{
    const char *b = strrchr(caminho, '/');
    return b != NULL ? b + 1 : caminho;
}

static int gerar_c(const Gramatica *g, const OpcoesGeracao *op)
{
    char *caminho = (char *) xmalloc(strlen(op->prefixo) + 3);
    FILE *f, *corpo;
    int c, erros = 0;

    /* Os procedimentos sao gerados primeiro num arquivo temporario porque
     * registram os conjuntos (conj_N), que precisam vir antes no .c. */
    corpo = tmpfile();
    if (corpo == NULL) {
        diag_erro(0, 0, "nao foi possivel criar arquivo temporario");
        free(caminho);
        return -1;
    }
    gerar_procedimentos(corpo, g, op);

    sprintf(caminho, "%s.c", op->prefixo);
    f = fopen(caminho, "w");
    if (f == NULL) {
        diag_erro(0, 0, "nao foi possivel criar %s", caminho);
        fclose(corpo);
        free(caminho);
        return -1;
    }
    escrever_cabecalho_c(f, op);
    fputs("#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n", f);
    fprintf(f, "#include \"%s.h\"\n\n", nome_base(op->prefixo));
    fprintf(f, "#define SUPORTE_ARVORE %d\n\n", op->arvore ? 1 : 0);

    fputs("/* ---- conjuntos de tokens ---- */\n\n", f);
    emitir_conjuntos(f, g);
    fputc('\n', f);

    emit_linhas(f, rt_estado);
    if (!op->microc)
        erros += gerar_lexico(f, g);
    gerar_nome_token(f, g);
    gerar_posicao_e_avancar(f, op->microc);
    emit_linhas(f, rt_primitivas);
    gen_token_de_arquivo(f, op->microc);

    rewind(corpo);
    while ((c = fgetc(corpo)) != EOF)
        fputc(c, f);
    fclose(corpo);

    gerar_main(f, g, op);
    fclose(f);
    free(caminho);
    return erros == 0 ? 0 : -1;
}

int codegen_gerar(Gramatica *g, const OpcoesGeracao *op)
{
    int r;
    nomear_terminais(g, op->microc);
    r = gerar_h(g, op);
    if (r == 0)
        r = gerar_c(g, op);
    liberar_conjuntos();
    return r;
}
