# Integração local com OoT 3D no PC

Estado: build completo disponível para teste local. Vers�o 0.4: doze m�sicas padr�o. Oito grupos de testes passam;
inicialização do executável empacotado passou (600 quadros de apresentação).
O usuário confirmou imagem renderizada. Efeitos musicais ao vivo ainda pendentes.

## Caminho dos dados

Microfone WASAPI -> núcleo v0.3 -> música reconhecida -> sequência nativa de
botões da ocarina -> reconhecimento e efeitos do próprio jogo.

`ReadOot3dOcarinaState` usa os leitores de memória do TriAevum, sem escrever
no save ou nos eventos. Só permite a captura na página 12, modo 3, transição
inativa, ação livre e resultado 0xFF. Demonstrações, confirmação, menus do host,
reprodução de timelines e estados desconhecidos suspendem a captura.

As tabelas de notas e músicas precisam coincidir com a família canônica de
execução. As tabelas 0 e 5, seus seis tipos de nota e os índices de flags 96 e
100 foram conferidos no código normalizado derivado localmente do dump USA.
A correspondência de botões nativos é L/R/Y/X/A para D4/F4/A4/B4/D5; ainda exige
validação de execução no jogo para as duas músicas.

## Execução

A função fica desativada no host por padrão. O iniciador do mod deve definir
`TRIAEVUM_OCARINA_MIC=1`. `TRIAEVUM_OCARINA_MIC_DEVICE` seleciona um índice
da listagem do diagnóstico; sem essa variável, utiliza o microfone padrão.

Ao reconhecer uma música aprendida, o host envia a sequ�ncia nativa completa (5 a 8 notas, 200 ms por nota),
com soltura entre os botões. A verificação contextual final continua no jogo.
Um comando manual musical cancela a execução automática até fechar e reabrir
a ocarina. Apenas o som enviado à saída é silenciado durante a escuta, para
reduzir realimentação; preferências e DSP originais permanecem intactos.

Abertura e encerramento do dispositivo usam uma thread própria. O áudio não
é gravado nem enviado. Falha de microfone devolve o áudio e mantém botões
manuais disponíveis. Carregar savestate limpa o reconhecimento e a reprodução.

## Validação

- Aprovados, 8 grupos CTest com Clang: gating, flags de músicas aprendidas, sequência e soltura
  dos botões, cancelamento manual, contexto, relógio, perda de microfone.
- Aprovados: build do host completo e inicialização com o título preparado.
- Pendente: teste no jogo com ocarina equipada, ambas as músicas aprendidas, efeito em
  locais permitidos e rejeição em locais/contextos não permitidos.
- Pendente: comparação com execução pelos botões e reentrada após fechar a ocarina.

Nenhum teste independente substitui o teste do efeito dentro do jogo.


## Refinamento 0.4

- Assobio: limite superior ampliado de 1600 para 4000 Hz; teste reproduziu
  erro de oitava ao cruzar o limite anterior.
- Voz com sopro: busca do fundo do vale YIN evita escolher um m�nimo local
  na borda por causa do ru�do; periodicidade m�nima 0,75 nas tr�s etapas.
  Ru�do branco continua rejeitado. Sensibilidade de amplitude mantida.
- 24 cen�rios PCM sint�ticos (12 m�sicas em voz grave ruidosa e assobio agudo
  com vibrato), al�m das regress�es anteriores de sustain, slide e pull-off.
- Sequ�ncias e flags de aprendizado verificadas para todas as 12 m�sicas;
  replay de Serenade/Nocturne/Bolero cobre 5/7/8 notas respectivamente.
- Can��o personalizada do espantalho ainda n�o inclu�da.
- Usu�rio confirmou voz/assobio funcionando na vers�o anterior; avalia��o
  ao vivo desta revis�o continua necess�ria. Nenhum �udio foi gravado.

## Refinamento 0.4.1 - Saria / Storms

Reprodução RED: [0,3,7,0,3,7] era aceita como Saria pela tolerância global.
Além do ajuste absoluto transposto, agora cada motivo de três notas precisa
preservar sua assinatura: Saria termina com salto pequeno menor que o primeiro;
Storms termina com salto grande maior que o primeiro. As duas metades precisam
concordar. Frases próximas que falham nessa assinatura exibem pedido de repetição
no console, sem ativar; notas dessa tentativa são descartadas para a próxima.

Teste de recuperação inicialmente detectou Lullaby na junção entre tentativa
ambígua e repetição correta. A limpeza de notas após ambiguidade corrigiu essa
reprodução. Testes cobrem frase ambígua, metades conflitantes, afinação levemente
imprecisa e transposição, recuperação sem reabrir e as 12 músicas em PCM.
Oito grupos CTest aprovados, host e diagnóstico recompilados, product-info
empacotado executou com código 0. Validação ao vivo desta revisão pendente.

## Refinamento 0.4.2 - separacao das notas

Relato do usuário: apenas Time e Lullaby funcionam. Processo em execução
confirmado como v0.4.1; saves de origem e teste têm todas as flags musicais.
Não atribuir o relato a versão antiga ou músicas não aprendidas.

Os testes anteriores de todas as músicas usavam pausas de 120 ms entre notas.
Novo teste PCM usa notas ligadas de 210 ms (transição de 30 ms + corpo de 180 ms)
e articulação de 40 ms apenas entre alturas iguais. Reproduziu perda de notas
em Minuet, Epona, Nocturne, Saria e Serenade.

Causas reproduzidas: candidato iniciado entre alturas mantinha o debounce de
semitom mesmo chegando a dois semitons; silêncio de 40 ms era engolido pelo
debounce de perda de periodicidade de 60 ms. Corrigido: considerar a chegada
atual para decidir proximidade e distinguir silêncio energético (25 ms) de
perda de confiança com som ainda presente (60 ms). Filtros de slides, pull-offs,
semitom breve e nota sustentada continuam nos testes.

Oito grupos aprovados, incluindo as 12 novas execuções ligadas e os 24 cenários
anteriores. Host/diagnóstico compilados; pacote v0.4.2 selecionado nos iniciadores.
Ainda não comprova o relato ao vivo resolvido: console e log local derivado
microfone-notas-v0.4.2.log agora incluem alturas, trechos, músicas reconhecidas,
flags aprendidas e decisão de envio ao jogo. Não grava PCM/áudio.

## v0.6.0 - microfone escolhido no menu do jogo

O host cria `real_instrument::MicController` (em `oot3d_native_a32_window_setup.cpp`)
com o arquivo `<config>/ocarina_microfone.json` (mesma pasta de `game_language.json`)
e o liga ao menu Esc pela interface `InGameMenuMicrophone`
(`oot3d_ocarina_microphone_menu.cpp`): aba "Microfone da Ocarina" com "Ativar
microfone", "Dispositivo", "Atualizar lista" e as linhas ao vivo Em uso / Nível /
Última nota / Estado / Arquivo. É configuração do PC; a confirmação de música
continua sendo a caixa de texto nativa do jogo (NATIVE_DIALOG.md, sem mudanças).

- Ligado na partida: `TRIAEVUM_OCARINA_MIC=1` (iniciador) liga, `=0` desliga; sem a
  variável vale a caixa salva; sem arquivo, desligado. O menu liga/desliga na hora.
- Dispositivo: `TRIAEVUM_OCARINA_MIC_DEVICE=<índice>` (compatibilidade) > escolha
  salva (ID do Windows, depois nome) > padrão do Windows. Índice inválido ou
  dispositivo salvo ausente viram aviso na linha "Estado".
- `GameMicrophone::SetDevice` troca o dispositivo na thread de captura: para a
  captura atual, abre a nova; falha ao abrir ou desconexão durante o uso caem para o
  padrão do Windows com a mensagem de erro. `SetMonitoring` mantém a captura aberta
  só para o medidor enquanto a aba está visível (sem reconhecer nada).
- A enumeração roda numa thread própria (o apartamento COM da thread do jogo não é
  alterado); `ComScope` também tolera um apartamento já iniciado.
- Testes novos: `test_mic_settings` (arquivo, prioridade, lista, resolução) e
  `test_mic_controller` (dono com enumeração real e arquivo temporário; abre o
  microfone padrão por um instante para o teste de fallback).
