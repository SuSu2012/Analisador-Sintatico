/*
 * correcao.h - Corrigi gramáticas com recursao a esqurda direta, entre outros.
 */
#ifndef CORRECAO_H
#define CORRECAO_H

#include "gramatica.h"

/* recebe uma gramatica e se precisar, elimina recursao a esquerda e outros
que precisar */
void corrige_ebnf(Gramatica *g);

#endif
