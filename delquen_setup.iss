; Instalador do Delquen (Inno Setup 6)
; Coloca na mesma pasta: delquen.exe (compilado), delquen_setup.iss e, se quiseres, nada mais.
; Compila com build_installer.bat ou abrindo este ficheiro no Inno Setup.

[Setup]
AppName=Delquen
AppVersion=2.1.0
AppPublisher=Delquen
DefaultDirName={autopf}\Delquen
DefaultGroupName=Delquen
UninstallDisplayIcon={app}\delquen.exe
OutputDir=.
OutputBaseFilename=Delquen_Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
SetupIconFile=delquen.ico
WizardImageFile=delquen_wizard.bmp
WizardSmallImageFile=delquen_small.bmp
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
CloseApplications=yes
ShowLanguageDialog=no
LanguageDetectionMethod=uilanguage

; Idioma do instalador: segue o idioma do Windows (os ficheiros .isl que existirem no teu Inno Setup).
; O idioma da APLICAÇÃO é escolhido na página "Language / Idioma" (22 idiomas).
[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"
#ifexist AddBackslash(CompilerPath) + "Languages\Portuguese.isl"
Name: "pt"; MessagesFile: "compiler:Languages\Portuguese.isl"
#endif
#ifexist AddBackslash(CompilerPath) + "Languages\BrazilianPortuguese.isl"
Name: "ptbr"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"
#endif
#ifexist AddBackslash(CompilerPath) + "Languages\French.isl"
Name: "fr"; MessagesFile: "compiler:Languages\French.isl"
#endif
#ifexist AddBackslash(CompilerPath) + "Languages\Spanish.isl"
Name: "es"; MessagesFile: "compiler:Languages\Spanish.isl"
#endif
#ifexist AddBackslash(CompilerPath) + "Languages\Polish.isl"
Name: "pl"; MessagesFile: "compiler:Languages\Polish.isl"
#endif
#ifexist AddBackslash(CompilerPath) + "Languages\Turkish.isl"
Name: "tr"; MessagesFile: "compiler:Languages\Turkish.isl"
#endif
#ifexist AddBackslash(CompilerPath) + "Languages\Russian.isl"
Name: "ru"; MessagesFile: "compiler:Languages\Russian.isl"
#endif
#ifexist AddBackslash(CompilerPath) + "Languages\Ukrainian.isl"
Name: "uk"; MessagesFile: "compiler:Languages\Ukrainian.isl"
#endif
#ifexist AddBackslash(CompilerPath) + "Languages\German.isl"
Name: "de"; MessagesFile: "compiler:Languages\German.isl"
#endif
#ifexist AddBackslash(CompilerPath) + "Languages\Italian.isl"
Name: "it"; MessagesFile: "compiler:Languages\Italian.isl"
#endif
#ifexist AddBackslash(CompilerPath) + "Languages\Dutch.isl"
Name: "nl"; MessagesFile: "compiler:Languages\Dutch.isl"
#endif
#ifexist AddBackslash(CompilerPath) + "Languages\Japanese.isl"
Name: "ja"; MessagesFile: "compiler:Languages\Japanese.isl"
#endif
#ifexist AddBackslash(CompilerPath) + "Languages\Czech.isl"
Name: "cs"; MessagesFile: "compiler:Languages\Czech.isl"
#endif
#ifexist AddBackslash(CompilerPath) + "Languages\Hungarian.isl"
Name: "hu"; MessagesFile: "compiler:Languages\Hungarian.isl"
#endif

[Tasks]
Name: "background"; Description: "Keep Delquen running in the background and start with Windows"; GroupDescription: "Background / Segundo plano:"
Name: "autoshow"; Description: "Show the overlay automatically when a game starts"; GroupDescription: "Background / Segundo plano:"
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Shortcuts / Atalhos:"; Flags: unchecked

[Files]
Source: "delquen.exe"; DestDir: "{app}"; Flags: ignoreversion

[Dirs]
Name: "{userappdata}\Delquen"

[Icons]
Name: "{group}\Delquen"; Filename: "{app}\delquen.exe"
Name: "{group}\Uninstall Delquen"; Filename: "{uninstallexe}"
Name: "{autodesktop}\Delquen"; Filename: "{app}\delquen.exe"; Tasks: desktopicon

; as escolhas do instalador ficam na configuração do programa (a pergunta da primeira execução deixa de aparecer)
[INI]
Filename: "{userappdata}\Delquen\config.ini"; Section: "overlay"; Key: "asked"; String: "1"
Filename: "{userappdata}\Delquen\config.ini"; Section: "overlay"; Key: "language"; String: "{code:GetLangIndex}"
Filename: "{userappdata}\Delquen\config.ini"; Section: "overlay"; Key: "autostart"; String: "1"; Tasks: background
Filename: "{userappdata}\Delquen\config.ini"; Section: "overlay"; Key: "autostart"; String: "0"; Tasks: not background
Filename: "{userappdata}\Delquen\config.ini"; Section: "overlay"; Key: "trayClose"; String: "1"; Tasks: background
Filename: "{userappdata}\Delquen\config.ini"; Section: "overlay"; Key: "autoShow"; String: "1"; Tasks: autoshow
Filename: "{userappdata}\Delquen\config.ini"; Section: "overlay"; Key: "autoShow"; String: "0"; Tasks: not autoshow

[Run]
; limpeza: tarefa agendada da versão antiga (GPAI), se existir
Filename: "schtasks"; Parameters: "/Delete /F /TN GPAI"; Flags: runhidden
; tarefa agendada com privilégios elevados: arranca com o Windows sem pedir UAC
Filename: "schtasks"; Parameters: "/Create /F /TN Delquen /SC ONLOGON /RL HIGHEST /TR ""\""{app}\delquen.exe\"" /tray"""; Flags: runhidden; Tasks: background
Filename: "{app}\delquen.exe"; Description: "Start Delquen now"; Flags: nowait postinstall skipifsilent shellexec

[UninstallRun]
Filename: "schtasks"; Parameters: "/Delete /F /TN Delquen"; Flags: runhidden; RunOnceId: "DelTask"
Filename: "taskkill"; Parameters: "/F /IM delquen.exe"; Flags: runhidden; RunOnceId: "KillApp"

[Code]
var
  LangCombo: TNewComboBox;

// índice do idioma da app (mesma ordem do lang.inc) a partir do idioma do Windows
function DetectLangIndex: Integer;
var
  L: Integer;
begin
  L := GetUILanguage;
  Result := 0;
  case L and $3FF of
    $09: Result := 0;
    $16: if L = $0416 then Result := 2 else Result := 1;
    $0C: Result := 3;
    $0A: Result := 4;
    $01: Result := 5;
    $15: Result := 6;
    $08: Result := 7;
    $1F: Result := 8;
    $1D: Result := 9;
    $19: Result := 10;
    $22: Result := 11;
    $07: Result := 12;
    $10: Result := 13;
    $13: Result := 14;
    $11: Result := 15;
    $12: Result := 16;
    $04: Result := 17;
    $05: Result := 18;
    $18: Result := 19;
    $0E: Result := 20;
    $21: Result := 21;
  end;
end;

procedure InitializeWizard;
var
  Page: TWizardPage;
begin
  Page := CreateCustomPage(wpWelcome, 'Language / Idioma / Langue / Sprache / Язык / 言語',
    'Choose the language of Delquen. You can change it later in Settings.');
  LangCombo := TNewComboBox.Create(Page);
  LangCombo.Parent := Page.Surface;
  LangCombo.Style := csDropDownList;
  LangCombo.SetBounds(0, ScaleY(8), Page.SurfaceWidth, ScaleY(21));
  LangCombo.Items.Add('English');
  LangCombo.Items.Add('Português (Portugal)');
  LangCombo.Items.Add('Português (Brasil)');
  LangCombo.Items.Add('Français');
  LangCombo.Items.Add('Español');
  LangCombo.Items.Add('العربية');
  LangCombo.Items.Add('Polski');
  LangCombo.Items.Add('Ελληνικά');
  LangCombo.Items.Add('Türkçe');
  LangCombo.Items.Add('Svenska');
  LangCombo.Items.Add('Русский');
  LangCombo.Items.Add('Українська');
  LangCombo.Items.Add('Deutsch');
  LangCombo.Items.Add('Italiano');
  LangCombo.Items.Add('Nederlands');
  LangCombo.Items.Add('日本語');
  LangCombo.Items.Add('한국어');
  LangCombo.Items.Add('简体中文');
  LangCombo.Items.Add('Čeština');
  LangCombo.Items.Add('Română');
  LangCombo.Items.Add('Magyar');
  LangCombo.Items.Add('Bahasa Indonesia');
  LangCombo.ItemIndex := DetectLangIndex;
end;

function GetLangIndex(Param: String): String;
begin
  Result := IntToStr(LangCombo.ItemIndex);
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  R: Integer;
begin
  if CurStep = ssInstall then
    begin
    Exec('taskkill', '/F /IM delquen.exe', '', SW_HIDE, ewWaitUntilTerminated, R);   // fecha uma versão em execução
    Exec('taskkill', '/F /IM gpai.exe', '', SW_HIDE, ewWaitUntilTerminated, R);
  end;
end;
