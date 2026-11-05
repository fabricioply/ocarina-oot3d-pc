@echo off
setlocal
rem Ocarina por microfone (PC) v0.6.0 - copie este pacote por cima da pasta PC-OoT3D (veja LEIA-ME.md)
cd /d "%~dp0Jogo"
if not exist "ModMicrofone\TriAevumMicrofone.exe" (
  echo Arquivo ModMicrofone\TriAevumMicrofone.exe nao encontrado.
  pause
  exit /b 1
)
if not exist "ModMicrofone\Microfone.launch.json.modelo" (
  echo Arquivo ModMicrofone\Microfone.launch.json.modelo nao encontrado.
  pause
  exit /b 1
)
rem Gera o perfil de inicializacao apontando para ESTA pasta (nao altera o jogo original)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$j=(Get-Location).Path.Replace('\','/'); (Get-Content -Raw 'ModMicrofone\Microfone.launch.json.modelo').Replace('@JOGO@',$j) | Set-Content -Encoding UTF8 'ModMicrofone\Microfone.launch.json'"
echo Ocarina por microfone v0.6.0 - 12 musicas
echo O microfone e escolhido DENTRO do jogo: tecla Esc, aba "Microfone da Ocarina".
echo A escolha fica salva em Jogo\data\config\ocarina_microfone.json.
rem Opcional: forcar um microfone pelo numero mostrado por
rem   ferramentas\OcarinaMicrofone.exe --list-devices
rem (vale mais que a escolha salva). Tire o "rem" da linha abaixo e troque o 0:
rem set "TRIAEVUM_OCARINA_MIC_DEVICE=0"
set "TRIAEVUM_OCARINA_MIC=1"
set "TRIAEVUM_OCARINA_NATIVE_DIALOG=1"
cd /d "%~dp0Jogo\ModMicrofone"
"TriAevumMicrofone.exe" --launch-profile "%CD%\Microfone.launch.json" 2>> microfone.log
if errorlevel 1 pause
endlocal
