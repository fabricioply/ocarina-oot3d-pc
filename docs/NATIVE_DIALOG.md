# Confirmação nativa (caixa de texto do jogo) — mecanismo para PC e 3DS

Objetivo: quando uma frase cantada/tocada se parece com várias músicas, o jogo pergunta
"<música>?" com **sua própria caixa de texto Sim/Não** (fonte, janela e cursor originais),
uma candidata por vez. Nada de UI do host: tudo é estado e código do próprio OoT3D,
portanto portável para o 3DS (os endereços abaixo são da imagem "canonical EUR").

## Fatos verificados (por emulação ARM do dump + testes no jogo)

| Fato | Valor |
| --- | --- |
| Contexto de mensagem (global) | `0x005C1878` |
| `+0x08` | ponteiro do PlayState (0 fora de uma cena) |
| `+0x0C/+0x0D/+0x0E` | flags de desenho; `MessageDraw` (0x42CBCC) só desenha se C≠0, D≠0, E≠0 e estado ∉ {0,1,14,15} |
| `+0xF38` (byte) | estado: 0 livre · 1 abrindo · 7 **escolha** · 14 fechando · (6 = sessão da ocarina) |
| `+0xFA4` | próximo estado; `+0xFA0` ator |
| `+0xFC4` | índice da opção destacada (0 = primeira = "Sim") |
| `+0xF3C` | índice do idioma |
| Tabela de mensagens | `[ctx+0x20]` = base do arquivo `us.qm`; `[ctx+0x24]` = registros de 0x60 bytes ordenados por id |
| Registro | id, 3 palavras, depois 10 entradas (offset,tamanho) por idioma |
| Texto | bytes UTF‑8 com códigos `7F xx`: `1C` nova linha, `1A FFFFFFFF` início de escolha, `1D <cor>` cor, `00` fim |
| Caixas nativas | no máximo **2 opções** (por isso perguntamos uma candidata por vez) |

## Sequência (ver `oot3d_ocarina_dialog_runtime.cpp`)

1. **Salvar** os `0x1090` bytes do contexto de mensagem (a ocarina guarda estado ali).
2. Se o contexto estiver ocupado por uma mensagem **terminada** (estado 6), fechá-la como
   `Message_CloseTextbox` (`0x2E9A1C`): `next=3; state=0xE`.
3. **Trocar o texto** de um registro Sim/Não existente (id `0x1A`, "Do you want to play
   again?") em memória, pelo texto
   `"<música>?" 7F1C 7F1C 7F1A FFFFFFFF 7F1D 'B' "Yes" 7F1C "No" 7F1D00 7F00`
   (ajustando só o tamanho da entrada do idioma atual). Restaurado ao fim.
4. **Abrir** sem o reset de HUD de `Message_StartTextbox`: chamar o carregador `0x2CD2B4(ctx, 0x1A)`
   e então escrever `ctx+0xFA0=0; ctx+0xF38=1; ctx+0xFA4=2; ctx+0xD=1`
   (o que `0x4C0BC8` faz), **sem** `0x340BDC(play,0)`, que fecha a tela de ocarina.
5. O jogo avança sozinho: estados 1→2→3→4→7. Em 7, o botão A confirma e o índice em
   `+0xFC4` diz Sim/Não. O próprio jogo leva o estado a 14→0.
6. **Restaurar o núcleo** do contexto salvo: `[0x00,0x20)`, `[0x164,0x168)`, `[0xF38,0xFD0)`.
   Restaurar tudo recupera a ocarina mas ressuscita dados velhos e derruba o jogo; não
   restaurar nada deixa a ocarina sem reconhecer a música.
7. Esperar ~1,5 s e repetir a frase como botões (replay). A tecla A que confirmou a caixa
   também chega à ocarina como nota; a espera deixa o jogo esquecê-la.

## Pontos de integração no PC (TriAevum, execução whole‑AOT)

* O desvio de execução em `0x0041E988` só sai do código compilado quando
  `OcarinaDialog->NeedsExecution()` está na condição de `oot3d_native_a32_projection_policy.cpp`
  (sem isso, **qualquer** chamada ao jogo corrompe a execução — foi a causa das quedas).
* Enquanto a caixa está aberta o overlay da ocarina do TopScreen é omitido
  (`SetOcarinaOverlaySuppressed`), senão cobre a caixa.
* Entradas A/D‑pad durante a caixa pertencem à caixa: o microfone não trata como "manual".

## Para o 3DS

Os passos 1–7 são só leituras/escritas de memória e uma chamada de função do jogo
(`0x2CD2B4`), sem nenhuma dependência do host. Em hardware, aplicar o mesmo como um patch
de código chamado no ponto de atualização de entrada do jogo (o equivalente de `0x41E988`),
mantendo o replay por botões idêntico ao do PC.

## Variáveis de ambiente (PC)

* `TRIAEVUM_OCARINA_NATIVE_DIALOG=1` liga o diálogo (padrão do launcher normal).
* `TRIAEVUM_OCARINA_DIALOG_PROBE=1` log detalhado + teclas F8/F9/F10 de teste.
* `TRIAEVUM_OCARINA_DIALOG_RESTORE=full` e `…_NORESTORE=1` para comparar restaurações.
* `TRIAEVUM_OCARINA_REPLAY_DELAY=<s>` espera antes do replay (padrão 1,5).
