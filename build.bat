@echo off
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSPATH="
set "VCVARS="

if not exist "%VSWHERE%" goto novs

for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"

if "%VSPATH%"=="" goto nocomp

set "VCVARS=%VSPATH%\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" goto nocomp

call "%VCVARS%"
cd /d "%~dp0"

if not exist delquen.cpp goto nocpp
if not exist lang.inc goto nolang
if not exist delquen.rc goto noart
if not exist delquen.ico goto noart
taskkill /F /IM delquen.exe >nul 2>&1
taskkill /F /IM gpai.exe >nul 2>&1
timeout /t 1 /nobreak >nul
if exist delquen.exe del delquen.exe
if exist delquen.exe goto locked

if exist delquen.res del delquen.res
rc /nologo delquen.rc
if not exist delquen.res goto errrc
cl /O2 /EHsc /std:c++17 /utf-8 /DUNICODE /D_UNICODE delquen.cpp delquen.res /Fe:delquen.exe /link /SUBSYSTEM:WINDOWS /MANIFEST:EMBED /MANIFESTUAC:"level='requireAdministrator' uiAccess='false'"

if exist delquen.exe goto ok
echo.
echo Houve erros na compilacao. Copia o texto acima e envia-o.
goto fim

:ok
echo.
echo Pronto! O delquen.exe foi criado nesta pasta.
goto fim

:novs
echo Nao encontrei o Visual Studio Installer. O Build Tools nao parece estar instalado.
goto fim

:nocomp
echo O Build Tools existe, mas falta o componente de C++.
echo Abre o Visual Studio Installer, clica em Modificar e marca:
echo Desenvolvimento para ambiente de trabalho com C++
goto fim

:nocpp
echo Nao encontrei o delquen.cpp nesta pasta.
goto fim

:locked
echo.
echo Nao consegui substituir o delquen.exe antigo. Fecha o Delquen (icone na bandeja ^> Sair) e tenta outra vez.
goto fim

:nolang
echo Nao encontrei o lang.inc nesta pasta (tem de estar ao lado do delquen.cpp).
goto fim

:errrc
echo.
echo Houve erros ao compilar o icone (delquen.rc). Copia o texto acima e envia-o.
goto fim

:noart
echo Faltam ficheiros do logo nesta pasta (delquen.rc, delquen.ico).
goto fim

:fim
pause
