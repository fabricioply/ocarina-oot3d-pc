# Ocarina por microfone — Ocarina of Time 3D (port para PC, TriAevum)

Mod para o **port nativo para PC de *The Legend of Zelda: Ocarina of Time 3D*** ([TriAevum — Echoes of Time](https://github.com/coccofresco/TriAevum)). Não funciona no emulador nem no 3DS (para o 3DS há o plugin [ocarina-3ds](https://github.com/fabricioply/ocarina-3ds)).

> Projeto de fã, sem vínculo com a Nintendo, a Grezzo ou o projeto TriAevum. Nenhum jogo, ROM ou dado extraído do jogo é distribuído aqui: é preciso ter a sua própria cópia, já instalada no TriAevum.

Este repositório tem: a **release em ZIP** (o executável do mod + iniciador, pronto para usar), o **código-fonte** do mod (`source/`, espelhando os caminhos do TriAevum, e um `patches/ocarina-microphone.patch`) e a documentação do mecanismo (`docs/`). Veja também o [README em inglês](README.md) para a parte técnica e de compilação.

Toque, cantarole ou assobie uma música de ocarina no microfone e o jogo a executa.
Quando a frase é clara, o jogo toca a música sozinho. Quando há dúvida (por exemplo
Canção da Saria × Canção da Tempestade), aparece **a caixa de diálogo do próprio jogo**
perguntando "<música>?" com Yes/No. Nada é desenhado fora do jogo.

Este pacote contém **apenas o mod**. Ele **não** inclui o jogo, a ROM, nem os dados
extraídos dele.

## O que é preciso ter antes

1. O **TriAevum OoT3D** (a versão PC do jogo) já instalado e funcionando na sua máquina,
   com a pasta de jogo `PC-OoT3D\Jogo` contendo `data\`, `private-plugins\`, `resources\`
   e o seu save. Esses dados são gerados a partir da **sua** cópia do jogo e não podem ser
   distribuídos.
2. Windows 10/11 e um microfone.

## Instalação

1. Copie o conteúdo deste pacote **por cima da pasta `PC-OoT3D`** (o que está em `Jogo\`
   entra em `PC-OoT3D\Jogo\`; `ferramentas\` e `INICIAR_COM_MICROFONE.cmd` ficam em
   `PC-OoT3D\`). A estrutura final deve ser:

   ```
   PC-OoT3D\
     INICIAR_COM_MICROFONE.cmd
     ferramentas\OcarinaMicrofone.exe
     Jogo\
       ModMicrofone\TriAevumMicrofone.exe
       ModMicrofone\Microfone.launch.json.modelo
       data\ private-plugins\ resources\ ...   (do seu TriAevum)
   ```
2. A pasta `fonte\` é só referência (código-fonte); não é necessária para jogar.

## Como usar

1. Feche o jogo se estiver aberto e dê dois cliques em `INICIAR_COM_MICROFONE.cmd`.
   O iniciador **não pergunta mais o microfone**: ele é escolhido dentro do jogo (abaixo).
2. Na primeira vez, aperte **Esc** e abra a aba **Microfone da Ocarina** para escolher o
   microfone (veja "Escolher o microfone"). Sem escolha, vale o padrão do Windows.
3. No jogo, carregue o save e **saque a ocarina** (modo de tocar livremente).
4. Toque/cantarole/assobie uma música. Termine a frase e fique um instante em silêncio.
   - Música clara: o jogo toca sozinho.
   - Dúvida: aparece a caixa "<música>?". Escolha Yes ou No (uma candidata por vez).
5. Notas iguais seguidas precisam de uma pequena pausa entre elas. Dá para transpor a frase
   inteira (importa o desenho dos intervalos, não o tom).
6. Apertar um comando de música manualmente cancela a escuta até fechar e reabrir a ocarina.

## Escolher o microfone (Esc → Microfone da Ocarina)

Aperte **Esc** (ou R3 no controle) para abrir o menu do jogo e vá até a aba
**Microfone da Ocarina** (Q/E ou os gatilhos trocam de aba; o mouse também funciona).

- **Ativar microfone**: liga/desliga o reconhecimento na hora. Desligado, a ocarina
  funciona só com os botões. O `INICIAR_COM_MICROFONE.cmd` liga o microfone a cada
  partida (`TRIAEVUM_OCARINA_MIC=1`), mesmo que ele tenha sido desligado antes.
- **Dispositivo**: a lista dos microfones do Windows, com **Padrão do Windows** primeiro.
  A troca vale **imediatamente** (a captura para e reabre no novo microfone), sem
  reiniciar o jogo.
- **Atualizar lista**: procura de novo os microfones depois de ligar ou desligar um
  (USB, headset, Bluetooth). A lista também é atualizada ao entrar na aba.
- **Em uso / Nível / Última nota**: enquanto a aba está aberta o microfone fica
  aberto só para mostrar o volume (barra e dB) e a última nota ouvida — cantarole ou
  assobie para conferir. Nada é reconhecido nem gravado nesse momento.
- **Estado**: mostra erros (por exemplo, microfone desconectado ou sem permissão).
- **Arquivo**: onde a escolha fica salva.

A escolha (microfone e ligado/desligado) fica salva em
`Jogo\data\config\ocarina_microfone.json`, junto das outras configurações do jogo.
O microfone é guardado pelo **ID do Windows e pelo nome**, não pelo número da lista
(os números mudam quando se conecta/desconecta aparelhos). Se o ID mudar (por exemplo,
depois de atualizar o driver) o mod procura pelo nome.

Se o microfone escolhido sumir (foi desconectado) ou não abrir, o mod passa a usar o
**Padrão do Windows** e mostra o motivo em "Estado". Reconecte e use **Atualizar lista**
para voltar a ele — a escolha salva não é perdida.

**Forçar um microfone sem o menu (opcional, compatibilidade):** a variável
`TRIAEVUM_OCARINA_MIC_DEVICE=<número>` (número mostrado por
`ferramentas\OcarinaMicrofone.exe --list-devices`) vale mais que a escolha salva. No
`INICIAR_COM_MICROFONE.cmd` há uma linha `rem set "TRIAEVUM_OCARINA_MIC_DEVICE=0"`:
tire o `rem` e troque o número. Escolher outro microfone no menu passa por cima dela
naquela partida (e salva a nova escolha). Prioridade na partida:
`TRIAEVUM_OCARINA_MIC_DEVICE` > escolha salva > padrão do Windows.

O mod só reproduz os comandos; o jogo continua aplicando as regras normais (lugar certo,
música aprendida). Ele não desbloqueia músicas nem eventos.

## As 12 músicas (Ré4 = ré grave, Ré5 = ré uma oitava acima)

| Música | Notas |
|---|---|
| Zelda's Lullaby | Si4 Ré5 Lá4 · Si4 Ré5 Lá4 |
| Epona's Song | Ré5 Si4 Lá4 · Ré5 Si4 Lá4 |
| Saria's Song | Fá4 Lá4 Si4 · Fá4 Lá4 Si4 |
| Sun's Song | Lá4 Fá4 Ré5 · Lá4 Fá4 Ré5 |
| Song of Time | Lá4 Ré4 Fá4 · Lá4 Ré4 Fá4 |
| Song of Storms | Ré4 Fá4 Ré5 · Ré4 Fá4 Ré5 |
| Minuet of Forest | Ré4 Ré5 Si4 Lá4 Si4 Lá4 |
| Bolero of Fire | Fá4 Ré4 Fá4 Ré4 Lá4 Fá4 Lá4 Fá4 |
| Serenade of Water | Ré4 Fá4 Lá4 Lá4 Si4 |
| Requiem of Spirit | Ré4 Fá4 Ré4 Lá4 Fá4 Ré4 |
| Nocturne of Shadow | Si4 Lá4 Lá4 Ré4 Si4 Lá4 Fá4 |
| Prelude of Light | Ré5 Lá4 Ré5 Lá4 Si4 Ré5 |

A canção personalizada do Espantalho não está incluída.

## Solução de problemas

- **Barra de nível parada / "Em uso" fechado:** confira **Ativar microfone** e o
  dispositivo na aba Microfone da Ocarina; fale perto do microfone e veja a barra.
  Se continuar parada, troque para outro dispositivo da lista ou use **Atualizar lista**.
- **"Estado" mostra erro ao abrir:** em Configurações do Windows → Privacidade →
  Microfone, permita o acesso para aplicativos da área de trabalho; confira se o
  aparelho está ativo e não está em uso exclusivo por outro programa.
- **Escolhi um microfone e ele voltou para o Padrão do Windows:** ele não foi
  encontrado ou não abriu (desconectado). Reconecte e use **Atualizar lista**.
- **Voltar ao comportamento inicial:** apague `Jogo\data\config\ocarina_microfone.json`
  (com o jogo fechado); o padrão do Windows volta a ser usado.
- **Sem som/nota detectada:** teste o microfone com `ferramentas\OcarinaMicrofone.exe --choose`
  (mostra as notas ouvidas e a música reconhecida, sem controlar o jogo).
- **Mensagens e erros:** `Jogo\ModMicrofone\microfone.log` (e o console do iniciador).
- **O iniciador gera `Microfone.launch.json`** a partir do `.modelo`, apontando para a
  pasta onde você instalou. O original do jogo não é alterado.
- Se o seu TriAevum tiver sido gerado com outro conjunto de dados, os caminhos com hashes
  em `Microfone.launch.json.modelo` (`private-plugins\<hash>`, `data\titles\...`,
  `data\mods\topscreen\<hash>`, `data\cache\...`) precisam ser ajustados para as pastas
  correspondentes da sua instalação.

## Conteúdo do pacote

- `Jogo\ModMicrofone\TriAevumMicrofone.exe` — o executável do mod (v0.6.0).
- `ferramentas\OcarinaMicrofone.exe` — diagnóstico independente do microfone.
- `fonte\real_instrument\` — detector de notas, casamento de músicas e documentação
  (`NATIVE_DIALOG.md` descreve o mecanismo do diálogo nativo).
- `fonte\host_patch\` — arquivos alterados do host TriAevum (caixa nativa e, na v0.6.0,
  a aba Microfone da Ocarina do menu Esc: `oot3d_in_game_menu.*`,
  `oot3d_ocarina_microphone_menu.*`, `oot3d_native_a32_window_*`, `game.cmake`).

Teste de qualidade: validado com áudio sintético e com microfone real no PC; a reprodução
das músicas e a caixa nativa foram confirmadas em jogo (v0.5.17). Na v0.6.0 a lógica de
escolha/salvamento do microfone tem testes automáticos e a troca de dispositivo com
fallback foi testada com os microfones reais do PC; a aba do menu Esc ainda precisa ser
conferida visualmente no jogo.
