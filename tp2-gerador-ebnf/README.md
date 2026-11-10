aluno: 202519050060
aluno: 202519050044
uso_ia: sim

# Compiladores I - Trabalho Prático 2: Gerador de Analisadores Sintáticos

Integrantes:

| Nome | Matrícula |
| --- | --- |
| Suelen Farias Paulino | 202519050060 |
| João Pedro Pinheiro | 202519050044 |

O programa `gerador` lê uma gramática escrita em EBNF e escreve, em ANSI C, um analisador sintático descendente recursivo para a linguagem dessa gramática. Depois é só compilar o `.c` gerado e usar o analisador para verificar programas.

O trabalho partiu do código-base fornecido pelo professor. O uso de IA está registrado em `USO_IA.md`.

## Como compilar

```
make            # compila o gerador e o analisador de Micro C
make test       # roda todos os testes
make microc     # gera e compila só o analisador de Micro C
make clean      # apaga a pasta build/ e o executável gerador
```

O código compila com `gcc -ansi -pedantic -Wall -Wextra` sem avisos. A única exceção é o `lex.yy.c`, que é gerado pelo flex.

## Como usar

Gerar o analisador de uma gramática, compilar e rodar sobre um programa:

```
mkdir -p build
./gerador testes/gramaticas/expr.ebnf -o build/expr --tree
gcc -ansi -pedantic -Wall -Wextra -o build/expr_bin build/expr.c
./build/expr_bin exemplo.txt
./build/expr_bin --tree exemplo.txt     # mostra a árvore de derivação
```

Opções do gerador:

- `-o prefixo`: nome dos arquivos de saída (`prefixo.c` e `prefixo.h`);
- `--sets`: mostra os conjuntos anulável, FIRST e FOLLOW;
- `--debug`: mostra a gramática do jeito que foi lida;
- `--tree`: inclui no analisador a opção de imprimir a árvore de derivação;
- `--scanner microc`: usa o scanner do TP1 em vez de gerar um léxico.

O analisador gerado lê o programa do arquivo indicado ou da entrada padrão. Ele termina com código 0 quando aceita a entrada e com 1 quando encontra erro. Com `--tokens arquivo`, ele lê os tokens de um arquivo no formato do TP1 (`Token: tipo = 33, lexema = 'int', linha = 1`) em vez de ler o programa.

## O que foi implementado

Todas as partes marcadas com `TODO(aluno)` no código-base foram feitas. Deixamos os comentários `TODO(aluno)` no código para lembrar o que era nosso e o que já veio pronto.

| Etapa | Arquivo | O que faz |
| --- | --- | --- |
| 1. Leitura da EBNF | `src/ebnf_parser.c` | `parse_expr`, `parse_seq` e `parse_fator` montam a árvore de cada regra |
| 2. Verificações | `src/verificacao.c` | aviso para não terminal inalcançável; erro para não terminal improdutivo e para recursão à esquerda (direta, indireta e através de partes que podem ser vazias) |
| 3. Conjuntos | `src/conjuntos.c` | cálculo de anulável, FIRST e FOLLOW por ponto fixo |
| 4. LL(1) | `src/ll1.c` | verifica as três condições LL(1). O conflito de `[ ]` e `{ }` com o FOLLOW vira aviso e é resolvido de forma gulosa |
| 5. Tradução das regras | `src/codegen.c` (`gen_expr`) | gera `if`, `while` e chamadas de função para sequência, alternativa, `[ ]` e `{ }` |
| 6. Léxico próprio | `src/codegen.c` (`gen_casamento_regex`) | transforma cada expressão regular de TOKENS em um autômato (expressão regular, depois AFN, depois AFD) e gera o código C que reconhece o maior prefixo |
| 7. Arquivo de tokens | `src/codegen.c` (`gen_token_de_arquivo`) | leitura do arquivo de tokens do TP1 (opção `--tokens`) |
| 8. Micro C | `src/microc_mapa.c`, `microc/scanner.c` | leitura dos nomes do `TokenType` no `tokens.h` e ligação com o scanner do TP1 |

## Scanner do TP1 (modo Micro C)

A pasta `microc/` tem o scanner que entregamos no TP1, adaptado para ser chamado pelo analisador gerado:

- `microc.flex`: o scanner do TP1;
- `tokens.h`: o `TokenType`, o `Token` e o `YYSTYPE`;
- `scanner.c` e `scanner.h`: fazem a ponte entre `microc_yylex()` e a função `proximo_token()`, que o analisador chama.

A gramática `testes/gramaticas/microc.ebnf` gera um aviso de conflito LL(1) no `else`. Esse aviso é esperado: é o caso do "else pendente", e o `else` fica com o `if` mais próximo, como em C.

### Correções feitas no scanner desde o TP1

- **Linha errada no erro "String nao terminada".** No TP1, a regra que encontra a quebra de linha dentro de uma string somava 1 na linha antes de devolver o erro. Assim o erro aparecia na linha seguinte. Agora a regra devolve a quebra de linha para a entrada com `yyless(0)` e a linha é contada depois, pela regra normal de `\n`.
- **Vazamento de memória no caractere inválido.** No TP1, a mensagem de erro era criada com `strdup` e nunca era liberada. Agora a mensagem é montada num vetor fixo (`buffer_erro`), sem alocar memória. A mensagem também ficou mais clara: `caractere invalido: @`.
- **Liberação da tabela de identificadores.** Foi criada a função `liberar_tabela_hash()`, chamada no fim da análise por `scanner_finalizar()`.
- **Adaptação ao TP2.** O tipo dos tokens e o `YYSTYPE` saíram do arquivo do flex e foram para o `tokens.h`. O `main()` de teste e a função `yywrap()` foram retirados. Foram incluídos `%option prefix="microc_yy"` e `%option noyywrap`. O arquivo passou a se chamar `microc.flex`.
- **ANSI C.** Os comentários `//` viraram `/* */`, a inicialização `char res[2] = {yytext[1], '\0'}` foi trocada por atribuições, e foram retiradas variáveis que não eram usadas.
- **Biblioteca matemática.** A função de hash usa `pow` e `floor`, então o Makefile passou a ligar com `-lm` (`LDLIBS_MICROC = -lm`). Sem isso, a compilação falha no Linux.
- **`scanner.c`.** O lexema de ID e das constantes vem da tabela de strings (`microc_yylval.symbol`), e o token UNDEF traz a mensagem de erro do TP1 (`microc_yylval.error_msg`).

## Testes

O `make test` roda o gerador sobre cada gramática de `testes/gramaticas/` e compara a saída com `testes/esperado/`. Depois gera o analisador de cada gramática válida, roda os programas de `testes/programas/<gramatica>/` e compara com os arquivos `.esperado`. A última linha de cada caso é o código de saída (`status 0` ou `status 1`).

Gramáticas de teste:

- `expr.ebnf`: expressões aritméticas (válida);
- `microc.ebnf`: Micro C, usando o scanner do TP1 (válida);
- `gram.ebnf`: uma ou mais atribuições do tipo `x = 1 + 2;` ou `c = 'a' + 'b';` (válida). Uma expressão só pode juntar valores do mesmo tipo: só números (com `+`, `-`, `*`, `/`) ou só caracteres entre aspas simples (com `+`, `-`). Misturar os dois, como em `x = 'a' + 1;`, é erro;
- `recursao_esquerda.ebnf`: deve ser recusada por recursão à esquerda;
- `conflito_ll1.ebnf`: deve ser recusada por conflito LL(1).

Cada gramática válida tem pelo menos 3 programas aceitos e 3 rejeitados. No total são 25 testes, e todos passam.
