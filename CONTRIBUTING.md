# Contributing to Delquen

Thanks for helping! Bug reports, game-compatibility reports, translations and code are all welcome.

## Reporting a bug or a game that does not work

Please open an issue and include:

1. Windows version and GPU (vendor + model)
2. The game and how it runs: **fullscreen, borderless or windowed**, and the API (DirectX / OpenGL / Vulkan) if you know it
3. What you expected and what happened
4. The last lines of `%APPDATA%\Delquen\debug.log` (press `Win+R`, paste the path, Enter)
5. For a game that shows `--` or is not detected: open the **Games** tab with the game focused (alt-tab to Delquen) and copy the diagnostic line, e.g. `javaw.exe: DXGI 0 | kernel 0 / 0 / 60 / 0 / 0 (2 s)`

## Building

See the *Build from source* section of the [README](README.md). In short:

```bat
build.bat
```

Requirements: Visual Studio Build Tools (C++ workload). Inno Setup 6 only for the installer.

## Code guidelines

- Keep it **small and dependency-free**: C++17, Win32 API only, no third-party libraries, no runtime frameworks.
- Memory and CPU use are part of the product: avoid per-frame heap allocations, keep timers coarse, do no work while the overlay is hidden and no game is running.
- Never inject into, hook or read the memory of other processes.
- User-visible text goes through `TR(S_...)` and `lang.inc`, never as a string literal in `delquen.cpp`.
- Source files are UTF-8 (without BOM) and compiled with `/utf-8`. `delquen_setup.iss` must keep its UTF-8 BOM and CRLF line endings.

## Logo and artwork

The logo is drawn by `tools/make_logo.py` (Pillow + numpy) and mirrored by hand in `docs/logo.svg`. After changing the design run `python tools/make_logo.py` and commit the regenerated `delquen.ico`, `delquen_wizard.bmp`, `delquen_small.bmp` and the images in `docs/`.

## Translations

Edit `lang.inc`. Each row is `LSTR(NAME, 22 strings)` in the column order documented at the top of the file (`en, pt, pt-BR, fr, es, ar, pl, el, tr, sv, ru, uk, de, it, nl, ja, ko, zh, cs, ro, hu, id`).

- Keep exactly 22 strings per row, keep `\n` and any `%` markers.
- Short phrases matter: buttons and labels have a fixed width — if a text is cut off in your language, shorten it.

To add a language: add a column to every row, add the name to `LANG_NAME` and `NLANG` in `delquen.cpp`, map the Windows language in `DetectLang()` and add the entry to the language page in `delquen_setup.iss`.

## Pull requests

- One change per pull request, with a short description of what and why
- Test the overlay with at least one DirectX game in borderless mode
- Update `CHANGELOG.md` for user-visible changes
