# Bundled LiveShell engine

ThemeChanger is a native PS5 app, and ZiZc3's LiveShell Engine is a separate, privileged `liveshell.elf` payload. They **cannot simply be merged into the same process**, because the engine injects `liveshell_shellui.elf` into the separately running `SceShellUI` process.

To make installation one-step, ThemeChanger v0.2.0 packages the original **LiveShell Engine v1.1** binary into the PS5 app and automatically streams that binary to the jailbreak's existing local **elfldr** on TCP port **9021** when it starts. If the engine is already running, ThemeChanger reuses it. Users can also request a retry by pressing Square.

- Upstream source: https://github.com/ZiZc3/LiveShell
- Upstream source commit inspected: `2bb061b8fd3d2e4c7ea2e81fbc90281debefaf8e`
- Distribution: https://github.com/ZiZc3/LiveShell/releases/tag/v1.1
- Pinned SHA-256: `771abd65fe1dc96d35662eceddbe42dfe40f2c126ae960e6be5f43247627d248`
- Upstream license: GNU GPL 3.0 (see https://github.com/ZiZc3/LiveShell/blob/main/LICENSE).
- UI: BlackBearReloaded `ps5-homebrew-ui` / PS5-DevBridge, GPL-3.0-or-later.
- No special utility on Windows or separate ELF upload required after installing ThemeChanger.
- A jailbreak with a running localhost ELF loader at `127.0.0.1:9021` **is still required**. The app does not bypass kernel protection or jailbreak the console.

Starting v0.3.4, LiveShell is **compiled from upstream GPL-3.0 sources during CI** with a small, published PS5 ThemeChanger integration patch. The patch exempts PPSA99891 from game-pause detection and suppresses the redundant Media tile registration; ShellUI injection and video playback remain upstream implementations. LiveShell is still a privileged standalone payload automatically launched by ThemeChanger.

Status and troubleshooting: `/data/LiveShell/liveshell.log` and `/data/LiveShell/inject.log`. ShellUI injection carries a risk of UI crash and may vary by firmware and jailbreak environment. Firmware compatibility must be tested on-console.

## Upgrade notice

Version 0.3.3 packages LiveShell 1.1. A previously running daemon will not be overwritten while active: reboot the console and re-enable the jailbreak to load the new bundle. ShellUI and decoder failures require the upstream LiveShell logs, and the frontend does not force kernel-level restarts.

## v0.3.4 modified LiveShell source

Build instructions: `homebrew/themechanger/liveshell_build/patch_liveshell.py` and `.github/workflows/build-themechanger.yml`. Upstream copyright and GNU GPL v3 apply to the patched engine. Previous packaged engine version (v1.1) SHA256 refers to the unmodified release; the newly compiled integrated engine has its own build-dependent checksum.

## v0.3.5 – Separate ShellUI focus exception

Upstream ShellUI hides its video whenever the focus layer remains `Game`. Pressing PS to open Home from PPSA99891 can leave that value unchanged while ThemeChanger remains running. The custom daemon writes `/system_tmp/liveshell/themechanger_active` only when PPSA99891 is the reported running app, and clears it on exit. The custom ShellUI checks the marker periodically and suppresses the Game-layer fade only for that title. Real games still fade normally; no PS5 game or system app is modified. The engine does not install a separate Media tile.
