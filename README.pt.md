<p align="center">
  <img src="docs/banner.png" alt="Delquen - overlay de FPS leve para Windows" width="100%">
</p>

**Overlay de FPS leve e moderno para Windows.** C++/Win32 nativo — sem Electron, sem Qt, sem Python e sem injetar nada nos jogos.

🇬🇧 [Read in English](README.md)

<!-- Adiciona capturas de ecrã na pasta docs/ e referencia-as aqui, por exemplo:
![Overlay](docs/overlay.png)
![Definições](docs/settings.png)
-->

## Funcionalidades

**Overlay**
- **FPS** grande e com cores por nível (limites configuráveis), mais **média, 1 % Low, 0.1 % Low** e **frame time**
- Gráfico de frame time com **marcadores de stutter**
- **CPU**, **GPU**, **RAM** e **VRAM** em linhas compactas, com **temperaturas da GPU e da CPU** quando o Windows e o driver as fornecem
- **Modo benchmark** (`F8`): mede o jogo que estás a jogar e mostra no overlay a média, 1 % e 0.1 % Low, guardados em `benchmarks.csv`
- **Fundo transparente** a sério (alpha por pixel), sem cartões; modos completo e compacto
- **Arrastar para mover**, com encaixe numa grelha e nas margens do ecrã; bloqueio opcional (os cliques atravessam)
- Tamanho, opacidade, fundo e seis cores dos sensores ajustáveis
- Tema claro, escuro ou do sistema

**Jogos**
- Deteta jogos automaticamente (janelas em ecrã inteiro/borderless) e **só mostra o overlay enquanto há um jogo**
- **Biblioteca de jogos**: lista dos jogos em que o overlay pode aparecer — adiciona jogos à mão, importa do **Steam, Epic, GOG, EA app / Origin, Ubisoft Connect, Battle.net, Rockstar, Bethesda, Riot, itch.io, Amazon Games, Xbox / Microsoft Store / Game Pass** e da lista do Xbox Game Bar, ou procura numa pasta qualquer, e liga o modo *"só jogos desta lista"* (funciona também em janela)
- **Perfis por jogo** (posição, tamanho, sensores)
- Listas de incluir/ignorar para programas detetados por engano (ou não detetados)
- **Histórico de sessões**: um CSV por sessão (FPS, frame time, CPU, GPU, RAM, VRAM) e um `history.csv` com o resumo (média, 1 % / 0.1 % Low, mínimo, máximo)
- Jogos DirectX via DXGI; OpenGL / Vulkan via eventos do kernel *(experimental)*

**Aplicação**
- 22 idiomas, à escolha na instalação e nas Definições
- Ícone na bandeja, corre em segundo plano, arranque opcional com o Windows (tarefa agendada, sem pedido de UAC)
- Instalador com escolha de idioma

## Instalar

Descarrega o `Delquen_Setup.exe` na página **Releases** e executa-o. Escolhe o idioma na primeira página, decide se o Delquen deve correr em segundo plano e arrancar com o Windows, e conclui o assistente.

> O Delquen pede direitos de administrador: ler os eventos de frames do Windows (ETW) em tempo real exige-o.
> O instalador e o executável não estão assinados, por isso o Windows SmartScreen pode mostrar um aviso.

## Começar

1. Abre o Delquen. O overlay aparece no canto superior esquerdo.
2. **Arrasta-o** para onde quiseres. Clique direito (ou o ícone da bandeja) abre o menu; duplo clique abre a janela principal.
3. Inicia um jogo em **ecrã inteiro sem bordas (borderless)** (ou em janela, ver abaixo). O overlay mostra o FPS do jogo detetado.
4. Liga **"Só mostrar durante jogos"** (topo da janela) para o overlay aparecer quando um jogo arranca e desaparecer quando fecha.

| Atalho (por defeito) | Ação |
|---|---|
| `F10` | Mostrar / esconder o overlay |
| `F9`  | Bloquear / desbloquear a posição (cliques atravessam) |
| `F8`  | Iniciar / parar um benchmark do jogo atual |
| `F12` | Sair |

Os dois primeiros podem ser alterados em **Definições**.

### Biblioteca de jogos (recomendado)
Abre o separador **Biblioteca** para veres os jogos que o Delquen conhece. Carrega em **Importar jogos instalados** para juntar os jogos do Steam, Epic, GOG, EA app / Origin, Ubisoft Connect, Battle.net, Rockstar, Bethesda, Riot, itch.io, Amazon Games, Xbox / Microsoft Store / Game Pass e da lista do Xbox Game Bar. Para jogos sem lançador usa **Procurar numa pasta…**, ou **Adicionar jogo…** para escolher um `.exe`. O Minecraft Java corre como `javaw.exe`, por isso adiciona esse à mão. Os lançadores são encontrados pelo registo e pelas pastas por defeito, por isso jogos em locais personalizados podem precisar de **Procurar numa pasta…**. Liga **"Mostrar o overlay só nos jogos desta lista"** e o Delquen ignora tudo o resto — sem deteções erradas, e os jogos em janela também funcionam.

### Jogos em janela e deteções erradas
Sem o modo de lista, por defeito só janelas em ecrã inteiro / borderless (não maximizadas) são tratadas como jogos, para não medir navegadores e outras apps. Se jogas em janela, ativa **Jogos → "Detetar também jogos em janela"**. Se um programa for detetado por engano (ou ignorado), passa para ele, volta ao Delquen, abre **Jogos** e carrega em **"Não é um jogo (ignorar)"** / **"É um jogo (incluir)"**.

## Como funciona

- **FPS**: uma sessão ETW em tempo real escuta os eventos de apresentação do `Microsoft-Windows-DXGI` (DirectX 9–12) e, como alternativa para OpenGL / Vulkan, alguns eventos `Microsoft-Windows-DxgKrnl` — a mesma abordagem do PresentMon. **Nada é injetado nem intercetado nos jogos.**
- **Hardware**: CPU/RAM via Win32, GPU NVIDIA via NVML, outras GPUs via contadores de desempenho do Windows (PDH) e DXGI. Com duas GPUs (integrada + dedicada) o Delquen segue a mais ocupada. A temperatura da GPU usa os dados de desempenho do próprio driver (`D3DKMT`); a da CPU usa as zonas térmicas do Windows.
- **Overlay**: janela em camadas, sempre no topo e sem tirar o foco, desenhada com GDI num DIB de 32 bits com alpha por pixel.
- A pegada de memória é pequena (uma versão inicial mediu cerca de 2,7 MB de conjunto de trabalho privado no Gestor de Tarefas; cresce um pouco com as funcionalidades).

Embora o Delquen não toque nos processos dos jogos, nenhum overlay pode garantir ser aceite por todos os sistemas anti-cheat. Usa por tua conta e risco em jogos online.

## Limitações conhecidas

- O **ecrã inteiro exclusivo** (por exemplo o ecrã inteiro clássico de alguns jogos como o Minecraft 1.8.8 ou o Brawlhalla) esconde qualquer janela de overlay. O Delquen deteta-o e avisa na linha de estado: passa o jogo para borderless / janela.
- O suporte a **OpenGL / Vulkan** é experimental; alguns jogos podem mostrar `--` no FPS. Reporta-os (ver Contribuir).
- A **temperatura da CPU** vem das zonas térmicas do Windows: pode ser aproximada, ou não existir em alguns PCs (ler a temperatura real dos núcleos exige um driver de kernel, que o Delquen não usa). A **temperatura da GPU** depende do driver.
- As traduções não foram revistas por falantes nativos — correções são bem-vindas.
- Requer **Windows 10 / 11 de 64 bits**.

## Idiomas

English · Português (Portugal) · Português (Brasil) · Français · Español · العربية · Polski · Ελληνικά · Türkçe · Svenska · Русский · Українська · Deutsch · Italiano · Nederlands · 日本語 · 한국어 · 简体中文 · Čeština · Română · Magyar · Bahasa Indonesia

O árabe usa um layout de janela espelhado (da direita para a esquerda).

## Compilar a partir do código

Requisitos:
- Windows 10/11 x64
- **Visual Studio Build Tools** com a carga de trabalho *Desenvolvimento para ambiente de trabalho com C++* (MSVC, C++17)
- *(opcional, para o instalador)* **Inno Setup 6**

```bat
build.bat              :: compila o delquen.exe
build_installer.bat    :: compila o delquen.exe e cria o Delquen_Setup.exe
```

O `build.bat` fecha um Delquen em execução, encontra o Visual Studio com o `vswhere` e executa:

```bat
cl /O2 /EHsc /std:c++17 /utf-8 /DUNICODE /D_UNICODE delquen.cpp /Fe:delquen.exe ^
   /link /SUBSYSTEM:WINDOWS /MANIFEST:EMBED /MANIFESTUAC:"level='requireAdministrator' uiAccess='false'"
```

Não há dependências de terceiros. O `delquen.cpp` e o `lang.inc` têm de estar na mesma pasta.

### Compilações e releases automáticas
O fluxo em `.github/workflows/build.yml` compila o executável e o instalador no GitHub a cada envio. Para publicar uma release, envia uma etiqueta de versão:

```bat
git tag v2.1.0
git push origin v2.1.0
```

O GitHub compila então o `Delquen_Setup.exe` e junta-o a uma nova Release automaticamente.

## Ficheiros e dados

| Caminho | Função |
|---|---|
| `%APPDATA%\Delquen\config.ini` | Definições, perfis por jogo (`[game:<exe>]`), listas de incluir / ignorar |
| `%APPDATA%\Delquen\history.csv` | Uma linha de resumo por sessão de jogo |
| `%APPDATA%\Delquen\sessions\*.csv` | Dados por segundo de cada sessão |
| `%APPDATA%\Delquen\benchmarks.csv` | Uma linha de resumo por benchmark (`F8`) |
| `%APPDATA%\Delquen\debug.log` | Registo de deteção de jogos / arranque (útil em relatórios de erros) |

As versões antigas chamavam-se *GPAI*; a pasta antiga `%APPDATA%\GPAI` é movida automaticamente no primeiro arranque.

## Estrutura do projeto

```
delquen.cpp          aplicação (um único ficheiro de código)
lang.inc             traduções (88 frases x 22 idiomas)
delquen.rc           recursos do Windows (ícone, versão)
delquen.ico          ícone da aplicação (gerado, ver tools/make_logo.py)
delquen_setup.iss    script do instalador (Inno Setup)
delquen_wizard.bmp   imagens do instalador (geradas)
delquen_small.bmp    imagens do instalador (geradas)
build.bat            compila o executável
build_installer.bat  compila o executável e o instalador
.github/workflows/   compilação automática (e release nas etiquetas de versão)
tools/make_logo.py   regenera o logo, o ícone, as imagens do instalador e os banners (precisa de Pillow + numpy)
docs/                logo.svg, logo.png, banner.png, social-preview.png
```

## Traduzir

Todo o texto está no `lang.inc`. Cada linha é `LSTR(NOME, <22 traduções>)`, por esta ordem de colunas:

`en, pt, pt-BR, fr, es, ar, pl, el, tr, sv, ru, uk, de, it, nl, ja, ko, zh, cs, ro, hu, id`

Para corrigir uma tradução edita o texto na coluna certa. Mantém 22 traduções por linha e conserva os marcadores e as quebras de linha (`\n`). Se faltar uma tradução, usa-se o inglês.

## Contribuir

Vê o [CONTRIBUTING.md](CONTRIBUTING.md). Relatórios de erros e de compatibilidade com jogos são muito bem-vindos.

## Licença

[MIT](LICENSE)
