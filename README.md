# ThemeChanger

PS5 wallpaper manager by **YAHBALLON**.

9660+ Live Wallpapers 4K

![ThemeChanger](homebrew/themechanger/assets/icon0.png)

## Download

Get `PPSA99891.zip` from [Releases](https://github.com/Yahballon/ThemeChanger/releases/latest). Title ID: `PPSA99891`.

## Features

- Download and manage MotionBGS animated wallpapers.
- Integrated LiveShell engine, preview, About and Settings.
- Delete all downloaded wallpapers from Settings.
- Signal Companion controls and diagnostics.

## Compatibility

Signal's regular firmware support is **9.00+**; upstream testing was on PS5 Pro 13.60. The included firmware 5.10 LED test is experimental and unconfirmed.

Some H.264 Level 5.0 videos still stall in the PS5 ShellUI decoder. Unmodified smooth Level 5.0 playback and automatic transcoding are **not fixed** in v0.3.21.

## Source and builds

The application, native tests, integration patches and release notes are in [homebrew/themechanger](homebrew/themechanger). GitHub Actions builds the PS5 installer and publishes `PPSA99891.zip` to Releases after successful checks. Push changes to `main`, or run **Build PS5 ThemeChanger** manually. Bump the app version, content version and workflow release tag for a new release.

Initial source snapshot: [tmp_lap commit 0ddf9f2](https://github.com/Yahballon/tmp_lap/commit/0ddf9f22428ecf0027299775af998ac2a7cc2659).

## Credits and license

GPL-3.0. Includes integrations with [ZiZc3/LiveShell](https://github.com/ZiZc3/LiveShell), [Marice/ps5-signal](https://github.com/Marice/ps5-signal), [Marice/ps5-signal-companion](https://github.com/Marice/ps5-signal-companion), and [blackbearreloaded/ps5-homebrew-ui](https://github.com/blackbearreloaded/ps5-homebrew-ui). See the integration documents for attribution and build details.
