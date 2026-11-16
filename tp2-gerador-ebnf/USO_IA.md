## USO_IA.md

    Ferramenta: Claude (Claude Code), modelo Claude Opus 5.5
    Trecho:     microc/microc.flex e Makefile
    Finalidade: tirar dúvidas sobre os problemas apontados no scanner: por que o
                erro "String nao terminada" saía na linha errada, como usar o
                yyless, como evitar o vazamento do strdup com um vetor fixo e por
                que faltava o -lm.
    O que fiz:  entendi as explicações e escrevi as correções eu mesmo. A IA
                revisou o que eu fiz e apontou erros, que eu corrigi.

    Ferramenta: Claude (Claude Code), modelo Claude Opus 5.5
    Trecho:     src/ e testes/
    Finalidade: pedi para revisar a versão com todas as etapas implementadas,
                rodar os testes e testar casos que os testes do professor não
                cobrem (mensagens de erro do manual, expressões regulares mais
                complicadas, opção --tokens, vazamentos de memória).
    O que fiz:  A IA foi consultada apenas, apontou avisos de compilação e diferenças
                nas mensagens; o que corrigimos, corrigimos nós mesmos.

    Ferramenta: Claude (Claude Code), modelo Claude Opus 5.5
    Trecho:     README.md
    Finalidade: pedi para formatar o README.md de maneira estética e segundo as instruções de entrega.

    Ferramenta: Claude, modelo Claude Sonnet 5.5
    Trecho:     src/codegen.c (gen_casamento_regex e gen_token_de_arquivo)
    Finalidade: pedi para conferir se as minhas soluções estavam corretas. A IA
                apontou que a classe negada [^...] não invertia o '\n' (o que
                quebraria comentários e strings) e uma declaração fora do
                início de bloco (C89).
    O que fiz:  apliquei a correção da classe negada escrita por mim.

    Ferramenta: Claude, modelo Claude Sonnet 5.5
    Trecho:     src/verificacao.c
    Finalidade: pedi para conferir se as minhas soluções estavam corretas.
    O que fiz:  conferi juntamente com os meus testes se estavam batendo com o resultado.

    Ferramenta: Claude (Claude Code), modelo Claude Opus 5.5
    Trecho:     testes/gramaticas/gram.ebnf e testes/programas/gram/
    Finalidade: tirar dúvidas ao escrever a 5ª gramática: como representar
                vazio e repetição em EBNF sem recursão à esquerda, a diferença
                entre duas expressões regulares para identificador, por que o
                token CHA nunca era reconhecido (empate com ID).
    O que fiz:  escrevi a gramática e os programas de teste eu mesmo. A IA
                testou as versões e apontou os problemas;

    Ferramenta: Claude (Claude Code), modelo Claude Opus 5.5
    Trecho:     extras (enunciado tp2_gerador_ebnf-1.pdf)
    Finalidade: pedi para ler o enunciado, listar os extras opcionais e
                explicar o que precisava ser feito em cada um.
    O que fiz:  apenas consultado. Usei a explicação para planejar os extras.

    Ferramenta: Claude (Claude Code), modelo Claude Opus 5.5
    Trecho:     testes/gramaticas/ebnf.ebnf (extra: EBNF em EBNF)
    Finalidade: revisar as expressões regulares da seção TOKENS. A IA testou o
                léxico gerado com várias entradas e apontou os erros.
    O que fiz:  escrevi e corrigi as expressões regulares e a seção RULES eu
                mesmo, a partir das explicações.

    Ferramenta: Claude (Claude Code), modelo Claude Opus 5.5
    Trecho:     src/codegen.c (extra: continuar a análise após um erro)
    Finalidade: entender como o codegen.c escreve o analisador (rt_estado,
                rt_primitivas, gerar_procedimentos, gerar_main), entender a
                recuperação em modo pânico e revisar cada passo. A IA explicou
                as funções emit e emit_teste e mostrou o formato da linha do
                laço de descarte que precisava aparecer no código gerado.
    O que fiz:  escrevi o código eu mesmo, passo a passo; a IA me ajudou com os testes, apontando erros (= no lugar de ==, chave faltando,
                -> no lugar de ., indentação do código gerado).

    Ferramenta: Claude (Claude Code), modelo Claude Opus 5.5
    Trecho:     src/codegen.c (extra: árvore sintática abstrata, opção --ast)
    Finalidade: entender a diferença entre o --tree do gerador e o do
                analisador, planejar a estrutura do nó e as funções da
                árvore, e entender a lógica da simplificação. A IA explicou
                cada parte em palavras e, na simplificar_ast, mostrou um
                pseudocódigo da recursão. Também explicou o uso de static e
                o escape em duas camadas (\\n, \\0, '\\'') nas strings do
                gerador.
    O que fiz:  escrevi o código eu mesmo; a IA me ajudou a testar cada versão (incluindo
                vazamentos de memória).

