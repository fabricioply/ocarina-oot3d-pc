# Teste local de instrumento e voz

Primeiro incremento do mod de PC: um detector independente, sem controle do jogo.
Versão 0.3: reconhece as frases de seis notas da Song of Time e Zelda's Lullaby.
Modo assistido por padrão.
Todos os dados ficam em memória; não grava áudio nem usa rede.

## Testar

Abra `E:\OcarinaMod\TestarOcarina\INICIAR.cmd`. Escolha o número do microfone.
Prefira o microfone físico direto, pois filtros de voz como NVIDIA Broadcast
podem remover sons de instrumentos ou distorcer o cantarolar.

Toque ou cantarole **Lá–Ré–Fá, Lá–Ré–Fá**. Ré desce sete semitons a partir do Lá;
Fá sobe três a partir do Ré. Pode transpor a frase inteira. Comece com notas de
0,3–0,7 s e pausas de aproximadamente 0,1–0,2 s, e faça uma pausa após a última nota.
Para Zelda's Lullaby, toque **Si–Ré–Lá, Si–Ré–Lá**: Ré sobe três semitons
desde o Si e Lá desce cinco desde o Ré. O ritmo é livre; pode sustentar as notas.
O teste mostra `NOTA ESTAVEL #N`, o trecho mais próximo e o nome da música reconhecida.
`Ouvindo` e `Sustentando` são medições; não acrescentam notas diretamente à melodia.
As notas estáveis são candidatas: o reconhecedor compara a frase inteira e pode
desconsiderar até dois enfeites compatíveis com slide, repetição de um trecho
sustentado ou uma nota vizinha breve. Ele preserva a chegada do slide/pull-off.
Não aceita qualquer sequência como uma música: exige seis notas correspondentes,
uma pontuação mínima e separação entre candidatos de músicas diferentes.
Use R para reiniciar e Q/ESC para sair. Fechar o teste encerra a captura.

Não espere reconhecimento de acordes, fala, acompanhamento ou áudio misturado.
A faixa do detector é 60–1600 Hz, incluindo o Ré2 para cantarolar mais grave; ruído, eco,
oitavas instáveis e timbres complexos ainda exigem avaliação com áudio real.
A indicação de confiança é uma pontuação heurística, não uma probabilidade.

## Configurar

`OcarinaMicrofone.exe --list-devices`: listar os microfones ativos.

`OcarinaMicrofone.exe --device 2`: selecionar um índice listado nessa execução.

`OcarinaMicrofone.exe --mode precise`: menor tolerância à desafinação.

`OcarinaMicrofone.exe --noise-floor 0.01`: rejeitar mais ruído. Reduzir para 0.002
se sons fracos não forem capturados; elevar se ruído virar notas. Valor padrão 0.005.
O índice do dispositivo pode mudar ao desconectar ou conectar equipamentos.
No pacote de teste, o executável se chama `OcarinaMicrofone-v0.3.exe`. Use
`INICIAR_SENSIVEL.cmd` para sons baixos; o limite menor agora vale em todas as etapas.

Se o microfone não abrir, confira a permissão de microfone para aplicativos de
desktop no Windows e se o dispositivo está ativo. O programa informa erros;
para trocar ou reconectar o microfone, encerre e abra novamente.

### No jogo (v0.6.0): Esc > Microfone da Ocarina

No mod do PC o microfone é escolhido no menu do jogo (tecla Esc, ou R3 no controle),
aba **Microfone da Ocarina**: ativar/desativar, lista de dispositivos ("Padrão do
Windows" primeiro), "Atualizar lista", nível de entrada ao vivo e última nota ouvida.
A troca vale na hora (a captura para e reabre no novo dispositivo) e fica salva em
`<Jogo>\data\config\ocarina_microfone.json`, pelo ID do Windows e pelo nome.
Prioridade na partida: `TRIAEVUM_OCARINA_MIC_DEVICE=<índice>` > escolha salva >
padrão do Windows. Se o dispositivo some ou não abre, o padrão do Windows assume e a
linha "Estado" mostra o erro. A lógica pura (arquivo, prioridade, lista, resolução
por ID e depois nome) está em `mic_settings.cpp` e é testada por `test_mic_settings`;
`mic_controller.cpp` é o dono usado pelo host (`test_mic_controller`).

## Compilar e verificar

Com CMake e MSVC instalados:

```powershell
cmake -S tools/real_instrument -B build-recognition -G "Visual Studio 17 2022" -A x64
cmake --build build-recognition --config Release
ctest --test-dir build-recognition -C Release --output-on-failure
```

Os testes cobrem altura, silêncio/ruído, vibrato, repetição, transposição,
desafinação, ambiguidade, sessões, interrupções e um dispositivo inválido.
A sessão inteira também é exercitada com áudio sintético da melodia.
Na versão 0.2, incluem sustentação com volume decrescente, ataque vocal que
estabiliza, sinais baixos e Lullaby conectada no ritmo, com durações diferentes.
Na versão 0.3, também cobrem Ré–Ré#–Ré durante uma sustentação, slide contínuo,
pull-off, uma mudança de semitom intencional, alinhamento com enfeites, notas
conectadas de 150 ms e áudio sintético contínuo de Song of Time com slide e bend.
A janela de análise foi reduzida para aproximadamente 43 ms a 48 kHz, mantendo
o teste de Ré2. Pequenas oscilações precisam de mais tempo para virar nova nota
do que uma chegada clara em outro intervalo.

## Estado

Sem integração com TriAevum ou OoT original ainda. Não ativa efeitos no jogo,
não verifica canções aprendidas nem altera o áudio dos jogos. Os testes de canto
e instrumentos pelo usuário, a avaliação de dez minutos de áudio negativo e as
integrações continuam pendentes. Nenhuma compilação de 3DS foi realizada.
