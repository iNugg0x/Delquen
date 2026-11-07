<p align="center">
  <img src="docs/banner.png" alt="Delquen - lightweight FPS overlay for Windows" width="100%">
</p>

**Lightweight, modern FPS overlay for Windows.** Native C++/Win32 — no Electron, no Qt, no Python, no game injection.

🇵🇹 [Leia em português](README.pt.md)

<!-- Add screenshots to the docs/ folder and reference them here, e.g.:
![Overlay](docs/overlay.png)
![Settings](docs/settings.png)
-->

## Features

**Overlay**
- Large, colour-coded **FPS** (good / medium / low thresholds are configurable), plus **average, 1 % low, 0.1 % low** and **frame time**
- Frame-time graph with **stutter markers**
- **CPU**, **GPU**, **RAM** and **VRAM** on compact lines; GPU temperature on NVIDIA
- Truly **transparent background** (per-pixel alpha), no cards; full and compact layouts
- **Drag to move** with grid and screen-edge snapping; optional click-through lock
- Size, opacity, background and six sensor colours are all adjustable
- Light, dark or system theme

**Games**
- Detects games automatically (borderless / fullscreen-style windows) and **shows the overlay only while a game is running**
- **Per-game profiles** (position, size, sensors)
- Include / ignore lists for programs that are wrongly detected (or missed)
- **Session history**: one CSV per session (FPS, frame time, CPU, GPU, RAM, VRAM) plus a `history.csv` summary (average, 1 % / 0.1 % low, min, max)
- DirectX games via DXGI; OpenGL / Vulkan via kernel present events *(experimental)*

**App**
- 22 languages, switchable at install time and in Settings (see below)
- Tray icon, runs in the background, optional start with Windows (scheduled task, no UAC prompt)
- Installer with language selection

## Install

Download `Delquen_Setup.exe` from the **Releases** page and run it. Pick your language on the first page, choose whether Delquen should run in the background and start with Windows, and finish the wizard.

> Delquen asks for administrator rights: reading frame events from Windows (ETW) in real time requires it.
> The installer and executable are not code-signed, so Windows SmartScreen may show a warning.

## Quick start

1. Open Delquen. The overlay appears in the top-left corner of the screen.
2. **Drag** it where you want it. Right-click it (or use the tray icon) for the menu; double-click it to open the main window.
3. Start a game in **fullscreen borderless** (or windowed, see below). The overlay shows the FPS of the detected game.
4. Turn on **"Show only during games"** (top of the window) to have it appear when a game starts and hide when it closes.

| Shortcut (default) | Action |
|---|---|
| `F10` | Show / hide the overlay |
| `F9`  | Lock / unlock the position (click-through) |
| `F12` | Quit |

Shortcuts for the first two can be changed in **Settings**.

### Windowed games and wrong detections
By default only fullscreen / borderless (non-maximised) windows are treated as games, so browsers and other apps are not measured. If you play in a window, enable **Games → "Also detect windowed games"**. If a program is detected by mistake (or missed), switch to it, return to Delquen, open **Games** and press **"Not a game (ignore)"** / **"It's a game (include)"**.

## How it works

- **FPS**: a real-time ETW session listens to `Microsoft-Windows-DXGI` present events (DirectX 9–12) and, as a fallback for OpenGL / Vulkan, a few `Microsoft-Windows-DxgKrnl` present events — the same approach as PresentMon. **Nothing is injected or hooked into the game.**
- **Hardware**: CPU/RAM via Win32, NVIDIA GPU via NVML, other GPUs via Windows performance counters (PDH) and DXGI.
- **Overlay**: a layered, topmost, non-activating window drawn with GDI into a 32-bit DIB with per-pixel alpha.
- Memory footprint is small (an early build measured about 2.7 MB private working set in Task Manager; it grows a little with features).

Although Delquen does not touch game processes, no overlay can be guaranteed to be accepted by every anti-cheat system. Use it at your own risk in online games.

## Known limitations

- **Exclusive fullscreen** (e.g. classic fullscreen in old games such as Minecraft 1.8.8) hides any overlay window. Use borderless / windowed mode.
- **OpenGL / Vulkan** support is experimental; some games may show `--` for FPS. Please report them (see Contributing).
- **CPU temperature** is not available (Windows needs a kernel driver for it). **GPU temperature** is NVIDIA-only. On AMD / Intel only GPU usage (and VRAM when reported) is shown.
- Translations were not reviewed by native speakers — corrections are welcome.
- Requires **Windows 10 / 11, 64-bit**.

## Languages

English · Português (Portugal) · Português (Brasil) · Français · Español · العربية · Polski · Ελληνικά · Türkçe · Svenska · Русский · Українська · Deutsch · Italiano · Nederlands · 日本語 · 한국어 · 简体中文 · Čeština · Română · Magyar · Bahasa Indonesia

Arabic uses a mirrored (right-to-left) window layout.

## Build from source

Requirements:
- Windows 10/11 x64
- **Visual Studio Build Tools** with the *Desktop development with C++* workload (MSVC, C++17)
- *(optional, for the installer)* **Inno Setup 6**

```bat
build.bat              :: compiles delquen.exe
build_installer.bat    :: compiles delquen.exe and builds Delquen_Setup.exe
```

`build.bat` closes a running Delquen, finds Visual Studio with `vswhere` and runs:

```bat
cl /O2 /EHsc /std:c++17 /utf-8 /DUNICODE /D_UNICODE delquen.cpp /Fe:delquen.exe ^
   /link /SUBSYSTEM:WINDOWS /MANIFEST:EMBED /MANIFESTUAC:"level='requireAdministrator' uiAccess='false'"
```

There are no third-party dependencies. `delquen.cpp` and `lang.inc` must stay in the same folder.

## Files and data

| Path | Purpose |
|---|---|
| `%APPDATA%\Delquen\config.ini` | Settings, per-game profiles (`[game:<exe>]`), include / ignore lists |
| `%APPDATA%\Delquen\history.csv` | One summary line per game session |
| `%APPDATA%\Delquen\sessions\*.csv` | Per-second data of each session |
| `%APPDATA%\Delquen\debug.log` | Game detection / start-up log (useful for bug reports) |

Older versions used the name *GPAI*; the old `%APPDATA%\GPAI` folder is moved automatically on first start.

## Project layout

```
delquen.cpp          application (single source file)
lang.inc             translations (77 phrases x 22 languages)
delquen.rc           Windows resources (icon, version info)
delquen.ico          application icon (generated, see tools/make_logo.py)
delquen_setup.iss    Inno Setup installer script
delquen_wizard.bmp   installer artwork (generated)
delquen_small.bmp    installer artwork (generated)
build.bat            build the executable
build_installer.bat  build the executable and the installer
tools/make_logo.py   regenerates the logo, icon, installer art and banners (needs Pillow + numpy)
docs/                logo.svg, logo.png, banner.png, social-preview.png
```

## Translating

All text lives in `lang.inc`. Each line is `LSTR(NAME, <22 translations>)`, in this column order:

`en, pt, pt-BR, fr, es, ar, pl, el, tr, sv, ru, uk, de, it, nl, ja, ko, zh, cs, ro, hu, id`

Fix a translation by editing the string in the right column. Keep the number of translations per row at 22 and keep placeholders/line breaks (`\n`) as they are. A missing translation falls back to English.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). Bug reports and game-compatibility reports are very welcome.

## License

[MIT](LICENSE)
