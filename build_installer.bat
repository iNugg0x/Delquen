@echo off
cd /d "%~dp0"

if not exist delquen.cpp goto nocpp
if not exist lang.inc goto nolang
if not exist delquen.rc goto noart
if not exist delquen.ico goto noart
if not exist delquen_wizard.bmp goto noart
if not exist delquen_small.bmp goto noart
if not exist delquen_setup.iss goto noiss

echo === 1/2: a compilar o delquen.exe ===
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSPATH="
if not exist "%VSWHERE%" goto novs
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if "%VSPATH%"=="" goto nocomp
if not exist "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" goto nocomp

call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat"
cd /d "%~dp0"
taskkill /F /IM delquen.exe >nul 2>&1
taskkill /F /IM gpai.exe >nul 2>&1
timeout /t 1 /nobreak >nul
if exist delquen.exe del delquen.exe
if exist delquen.exe goto locked
if exist delquen.res del delquen.res
rc /nologo delquen.rc
if not exist delquen.res goto errrc
cl /O2 /EHsc /std:c++17 /utf-8 /DUNICODE /D_UNICODE delquen.cpp delquen.res /Fe:delquen.exe /link /SUBSYSTEM:WINDOWS /MANIFEST:EMBED /MANIFESTUAC:"level='requireAdministrator' uiAccess='false'"
if not exist delquen.exe goto errcl

echo.
echo === 2/2: a criar o instalador ===
set "ISCC="
if exist "%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe" set "ISCC=%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
if exist "%ProgramFiles%\Inno Setup 6\ISCC.exe" set "ISCC=%ProgramFiles%\Inno Setup 6\ISCC.exe"
if exist "%LocalAppData%\Programs\Inno Setup 6\ISCC.exe" set "ISCC=%LocalAppData%\Programs\Inno Setup 6\ISCC.exe"
if exist "%ProgramFiles(x86)%\Inno Setup 7\ISCC.exe" set "ISCC=%ProgramFiles(x86)%\Inno Setup 7\ISCC.exe"
if exist "%ProgramFiles%\Inno Setup 7\ISCC.exe" set "ISCC=%ProgramFiles%\Inno Setup 7\ISCC.exe"
if exist "%LocalAppData%\Programs\Inno Setup 7\ISCC.exe" set "ISCC=%LocalAppData%\Programs\Inno Setup 7\ISCC.exe"
if "%ISCC%"=="" for /f "delims=" %%i in ('where ISCC.exe 2^>nul') do set "ISCC=%%i"
if "%ISCC%"=="" goto noinno

echo A usar: %ISCC%
if exist Delquen_Setup.exe del Delquen_Setup.exe
"%ISCC%" delquen_setup.iss
if exist Delquen_Setup.exe goto ok
echo.
echo Houve erros ao criar o instalador. Copia o texto acima e envia-o.
goto fim

:ok
echo.
echo Pronto! O instalador Delquen_Setup.exe foi criado nesta pasta.
goto fim

:locked
echo.
echo Nao consegui substituir o delquen.exe antigo. Fecha o Delquen (icone na bandeja ^> Sair) e tenta outra vez.
goto fim

:errcl
echo.
echo Houve erros na compilacao do delquen.cpp. Copia o texto acima e envia-o.
goto fim

:nocpp
echo Nao encontrei o delquen.cpp nesta pasta: %cd%
goto fim

:nolang
echo Nao encontrei o lang.inc nesta pasta (tem de estar ao lado do delquen.cpp): %cd%
goto fim

:noiss
echo Nao encontrei o delquen_setup.iss nesta pasta: %cd%
goto fim

:novs
echo Nao encontrei o Visual Studio Installer. O Build Tools nao parece estar instalado.
goto fim

:nocomp
echo O Build Tools existe, mas falta o componente de C++.
goto fim

:noinno
echo.
echo Nao encontrei o Inno Setup (ISCC.exe).
echo O delquen.exe foi compilado com sucesso, falta so encontrar o Inno Setup.
echo Procura o ficheiro ISCC.exe no PC e diz-me em que pasta esta.
echo Se ainda nao instalaste, instala o "Inno Setup" em jrsoftware.org e corre este ficheiro outra vez.
goto fim

:errrc
echo.
echo Houve erros ao compilar o icone (delquen.rc). Copia o texto acima e envia-o.
goto fim

:noart
echo Faltam ficheiros do logo nesta pasta (delquen.rc, delquen.ico, delquen_wizard.bmp, delquen_small.bmp).
goto fim

:fim
pause
