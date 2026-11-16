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
./build/expr_bin --ast exemplo.txt      # mostra a árvore sintática abstrata
```

Opções do gerador:

- `-o prefixo`: nome dos arquivos de saída (`prefixo.c` e `prefixo.h`);
- `--sets`: mostra os conjuntos anulável, FIRST e FOLLOW;
- `--debug`: mostra a gramática do jeito que foi lida;
- `--tree`: inclui no analisador a opção de imprimir a árvore de derivação;
- `--scanner microc`: usa o scanner do TP1 em vez de gerar um léxico.

O analisador gerado lê o programa do arquivo indicado ou da entrada padrão. Ele termina com código 0 quando aceita a entrada e com 1 quando encontra erro.

Opções do analisador gerado:

- `--tree`: mostra a árvore de derivação durante a análise. O analisador precisa ter sido gerado com `--tree`;
- `--ast`: mostra a árvore sintática abstrata no fim da análise. Também exige gerar com `--tree` (veja "Extras implementados");
- `--recuperar`: não para no primeiro erro sintático e continua a análise (veja "Extras implementados");
- `--tokens arquivo`: lê os tokens de um arquivo no formato do TP1 (`Token: tipo = 33, lexema = 'int', linha = 1`) em vez de ler o programa.

## O que foi implementado

Todas as partes que o código-base deixava para os alunos foram feitas. A versão com os comentários `TODO(aluno)` originais, que marcam essas partes, está na branch `branch-todo`.

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
- `ebnf.ebnf`: a própria notação EBNF do gerador (válida). É o extra descrito abaixo;
- `recursao_esquerda.ebnf`: deve ser recusada por recursão à esquerda;
- `conflito_ll1.ebnf`: deve ser recusada por conflito LL(1).

Cada gramática válida tem pelo menos 3 programas aceitos e 3 rejeitados. No total são 33 testes, e todos passam.

## Extras implementados

### Notação EBNF descrita em EBNF (+0,5)

O arquivo `testes/gramaticas/ebnf.ebnf` descreve, na própria notação EBNF, como se escreve um arquivo `.ebnf`. A gramática é a mesma que está no comentário do início de `src/ebnf_parser.c`. A seção TOKENS define por expressões regulares o que o léxico do gerador (`src/ebnf_lexer.c`) reconhece à mão:

| Token | Expressão regular | Exemplo |
| --- | --- | --- |
| `NOME_TERM` (terminal nomeado) | `/[A-Z][A-Z0-9_]*/` | `NUM`, `STRINGCONST` |
| `NAOTERM` (não terminal) | `/[a-z][a-z0-9_]*/` | `expr`, `secao_rules` |
| `LITERAL` | `/("[^"']+"\|'[^"']+')/` | `"while"`, `'+'` |
| `REGEX` | `/\/([^\/\\]\|\\.)+\//` | `/[0-9]+/`, `/a\/b/` |
| espaços (SKIP) | `/[ \t\r\n]+/` | |
| comentários (SKIP) | `/\(\*[^*]*\*\)/` | `(* comentario *)` |

O conteúdo de `REGEX` é uma repetição de duas peças: um caractere que não é `/` nem `\`, ou uma `\` seguida de qualquer caractere. Assim `\/` é lido como parte da expressão e não fecha o token, como faz o léxico do gerador.

As palavras `TOKENS`, `RULES`, `SKIP` e `EXTERN` aparecem como literais nas regras. Elas também casam com a expressão de `NOME_TERM`, mas, quando há empate, o literal vence. Assim elas funcionam como palavras reservadas.

O gerador aceita a gramática sem conflitos LL(1), e o analisador gerado a partir dela verifica se um arquivo `.ebnf` está bem escrito:

```
./gerador testes/gramaticas/ebnf.ebnf -o build/ebnf
gcc -ansi -pedantic -Wall -Wextra -o build/ebnf_bin build/ebnf.c
./build/ebnf_bin testes/gramaticas/expr.ebnf      # entrada aceita
./build/ebnf_bin testes/gramaticas/ebnf.ebnf      # entrada aceita: a gramática lê a si mesma
```

Esse analisador verifica só a **escrita** da gramática. Por isso ele aceita `recursao_esquerda.ebnf` e `conflito_ll1.ebnf`: as duas estão bem escritas, e os problemas delas (recursão à esquerda e conflito LL(1)) só são encontrados pelas verificações do gerador.

Os testes ficam em `testes/programas/ebnf/`. Os programas aceitos são cópias de `expr.ebnf`, `gram.ebnf`, `microc.ebnf` e do próprio `ebnf.ebnf`. Os rejeitados têm um parêntese sem fechar, um não terminal escrito com maiúscula e uma regra sem `;` no final.

Limitações (formas que o gerador aceita, mas esta gramática não):

- comentário com `*` no meio, como `(* a * b *)`;
- não terminal com letra maiúscula depois da primeira letra, como `minhaRegra`;
- literal com a outra aspa dentro, como `"'"`, ou com escape, como `"\""`.

### Continuar a análise após um erro (+1,0)

Sem opção nenhuma, o analisador gerado continua igual: mostra o primeiro erro e termina. Com a opção `--recuperar`, ele mostra o erro e segue analisando o resto da entrada, para encontrar outros erros na mesma execução:

```
$ printf "x = 'a' + 1;\ny = 2 + ;\n" | ./build/gram_bin --recuperar
linha 1, coluna 11: encontrado '1', esperado CHA
linha 2, coluna 9: encontrado ';', esperado NUM
```

No fim, se houve algum erro, o analisador não imprime `entrada aceita` e termina com código 1. A opção funciona no modo próprio e no modo Micro C, e também junto com `--tree`.

A técnica usada é a recuperação em **modo pânico**. Depois de um erro, o analisador fica "perdido": não sabe mais em que ponto da gramática a entrada está. Se continuasse como se nada tivesse acontecido, mostraria vários erros falsos, que são só consequência do primeiro. Por isso a recuperação tem três partes:

1. **Ao achar um erro**, `erro_sintatico` mostra a mensagem e liga a variável `modo_panico`. Enquanto ela está ligada, novos erros não são mostrados. O `match` que falha não consome o token: só reporta e volta.
2. **No fim de cada procedimento `parse_X`**, se `modo_panico` está ligada, o analisador descarta tokens até achar um que esteja no FOLLOW de X, ou até o fim do arquivo. O FOLLOW de X é o que pode vir logo depois de um X, então quem chamou `parse_X` consegue continuar dali. O gerador escreve esse laço no fim de todo procedimento, com o FOLLOW calculado na etapa 3:

   ```c
   while (modo_panico && !(tok.tipo == T_PV) && tok.tipo != TOKEN_FIM) {
       avancar();
   }
   ```

3. **Quando um `match` casa um token**, `modo_panico` é desligada. É o sinal de que o analisador e a entrada voltaram a concordar, e os próximos erros voltam a ser mostrados.

Quando falta um token, nada é descartado. Em `x = 5 y = 6;`, o `;` que falta é reportado, o `y` já está no FOLLOW da atribuição, e a análise continua em `y = 6;`. Quando sobra um token, ele é descartado. Em `(1 2) + 3`, o `2` é pulado e só um erro é mostrado.

Onde está no código: as variáveis `recupera_erros`, `contador_erros` e `modo_panico` (em `rt_estado`), `erro_sintatico` e `match` (em `rt_primitivas`), o laço de descarte (em `gerar_procedimentos`) e a leitura da opção e o código de saída (em `gerar_main`), todos em `src/codegen.c`.

Limitações:

- a recuperação é aproximada. O FOLLOW diz o que pode vir depois de X em algum lugar da gramática, não necessariamente no ponto do erro. Por isso, às vezes o analisador descarta um trecho que tinha outro erro, e esse segundo erro não é mostrado;
- erros léxicos (como um caractere inválido ou uma string não terminada) continuam encerrando a análise, mesmo com `--recuperar`;
- os testes de `make test` rodam os analisadores sem opção, então não cobrem a recuperação. Ela foi testada à mão, com as gramáticas `gram`, `expr` e `microc`.

### Árvore sintática abstrata (+1,0)

Com a opção `--ast`, o analisador gerado monta a árvore na memória durante a análise e, no fim, simplifica e imprime essa árvore, depois de `entrada aceita`. A diferença para o `--tree`: o `--tree` imprime a árvore de derivação completa enquanto analisa; a `--ast` deixa de fora o que não tem significado.

Exemplo com `(1 + 2) * x` na gramática `expr`. Árvore de derivação (`--tree`):

```
expr
  term
    factor
      '('
      expr
        term
          factor
            NUM '1'
        '+'
        term
          factor
            NUM '2'
      ')'
    '*'
    factor
      ID 'x'
```

Árvore abstrata (`--ast`):

```
term
  expr
    NUM '1'
    '+'
    NUM '2'
  '*'
  ID 'x'
```

A simplificação usa duas regras:

1. **Pontuação sai da árvore.** As folhas `(`, `)`, `[`, `]`, `{`, `}`, `;` e `,` são descartadas. Os parênteses não fazem falta: o agrupamento já aparece no formato da árvore, com o `1 + 2` num ramo abaixo do `*`.
2. **Não terminal com um filho só é trocado por esse filho.** É isso que tira as cadeias como `expr`, `term`, `factor`, `NUM`, que existem só por causa da forma da gramática.

Operadores e palavras-chave ficam na árvore. Sem o `'+'` não daria para saber qual é a operação, e sem o `'if'` ou o `'while'` não daria para saber qual é o comando. Um exemplo em Micro C:

```
$ ./build/microc/parser_microc --ast exemplo.mc
entrada aceita
programa
  'int'
  'main'
  bloco
    declaracao
      'int'
      ID 'x'
    atribuicao_interna
      ID 'x'
      '='
      termo
        expressao_aritmetica
          INTEGERCONST '3'
          '+'
          INTEGERCONST '4'
        '*'
        INTEGERCONST '2'
    comando_if
      'if'
      expressao_relacional
        ID 'x'
        '>'
        INTEGERCONST '10'
      comando_print
        'print'
        ID 'x'
```

(programa: `int main() { int x; x = (3 + 4) * 2; if (x > 10) print(x); }`)

Como funciona:

- Cada nó é uma `struct No` com o nome (não terminal ou token), o lexema, um vetor de filhos que cresce com `realloc` e um ponteiro para o pai.
- A árvore é montada nos mesmos pontos que o `--tree` usa. `arvore_entra`, no início de cada `parse_X`, cria um nó para X, coloca esse nó como filho do nó atual e passa a usá-lo como atual. `match` cria uma folha para o token que casou. `arvore_sai`, no fim de `parse_X`, volta para o pai. Por isso a `--ast` só funciona se o analisador foi gerado com `--tree`; sem isso, o analisador mostra um aviso e não imprime a árvore.
- No fim, se não houve erro, `simplificar_ast` percorre a árvore de baixo para cima aplicando as duas regras, e `imprimir_ast` mostra o resultado. A árvore é sempre liberada com `liberar_ast`, com erro ou sem erro. Conferimos com a ferramenta `leaks` do macOS que não sobra memória sem liberar.

Onde está no código, tudo em `src/codegen.c`: a `struct No` e as variáveis `raiz_ast`, `atual_ast` e `imprime_ast` (em `rt_estado`); as funções `criar_ast`, `adicionar_filho_ast`, `imprimir_ast`, `liberar_ast`, `eh_pontuacao` e `simplificar_ast`, além das mudanças em `arvore_entra`, `arvore_sai` e `match` (em `rt_primitivas`); a leitura da opção, o aviso, a simplificação, a impressão e a liberação (em `gerar_main`).

Limitações:

- os operadores ficam no mesmo nível dos operandos (`1`, `+`, `2` como irmãos), e não como pai deles. Para isso o gerador precisaria saber quais literais são operadores, e isso muda de uma gramática para outra;
- a lista de pontuação é fixa. Se uma gramática usar outro símbolo só como separador, ele continua na árvore;
- sem a pontuação, parte da informação fica na posição dos nós. Em `v[0] = 50`, por exemplo, o índice é o que aparece entre o `ID` e o `=`;
- a árvore só é impressa quando a entrada é aceita. Com `--recuperar` e algum erro, ela estaria incompleta, então não é mostrada;
- assim como a recuperação, a `--ast` não é coberta pelo `make test`. Foi testada à mão, com as gramáticas `expr` e `microc`.
