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