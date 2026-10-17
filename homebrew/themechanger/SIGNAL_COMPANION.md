# Signal Companion integration (ThemeChanger v0.3.17)

Open the sidebar and choose **Signal Companion**. Use **L1/R1** for Status, Colour and Startup; **Circle** returns focus to the sidebar. The service is queried only after this page has been opened. Opening the page starts the bundled service once if it is offline. It does not save configuration changes.

## Status

Cross starts the bundled service through the jailbreak ELF loader on localhost port 9021, or restarts an existing compatible service. The status page offers refresh, a beeper test, a blue-light test, stop previews and the optional ps5-autoloader switch. Autoload is changed only on explicit selection, and only when its existing list is accessible.

## Colour

Up/Down selects the control. Left/Right adjusts the blue, white or amber LED channel or the preset/slot. Adjusting a colour enables preview, limited to twice a second. Keep this colour (or Square) persists the mix; Use system colour releases the override. Six personal slots live under /data/ThemeChanger/signal/colours.json. The console has three physical LED channels, not arbitrary RGB output.

## Startup

Triangle switches between beeps and lights. Up/Down chooses a step, Cross chooses its editable field, Left/Right adjusts it, Options adds, R3 removes, Touchpad previews, and Square saves both sequences. Limits match ps5-signal: 24 steps per sequence and at most 10 seconds. Empty sequences disable that startup effect. A colour save preserves startup, and a startup save preserves the kept colour. Saving is blocked until the service config has arrived.

## Provenance and build

GPL-3.0-or-later. Signal Companion backend by Marice, based on ps5-homebrew-ui by BlackBearReloaded, from https://github.com/Marice/ps5-signal-companion at dd34b9917871830547be000fe3e7ae6d932c8abd. The files native/signal/{json,model,service,palette,autoload}.{hpp,cpp} and native/core/save_file.{hpp,cpp} retain upstream notices. ThemeChanger supplies its own panel controller and view.

The build bundles the unmodified loopback-only release ELF from https://github.com/Marice/ps5-signal/releases/tag/v0.1.0. This is byte-for-byte identical to the file inside Signal Companion v00.001.000: 147856 bytes, SHA-256 e39199ab216986442dd4ae66df7fe7ec012c50984a27fc8aff9b42834359e9d8. Both the download and packaged installer are verified against that hash. Source: https://github.com/Marice/ps5-signal/tree/8ed455197a8643302a066b7b0e97972036998304; its GPL license is shipped next to the ELF. It does not use SIGNAL_DEV or expose the service to the LAN. A compatible jailbreak ELF loader is required to start it.

**After updating:** choose Status > Restart ps5-signal once. An app install cannot replace the already-running background service. Optional autoloader controls are hidden if the autoloader is unavailable; this does not block colour control. LED brightness availability is separate from colour availability.

CI checks section-preserving config saves, sequence bounds, offline protection, palette persistence and non-destructive autoload edits against the upstream in-memory service. The native PS5 build and installer asset checks run before publication. Actual console LEDs, beeper, payload handover and firmware compatibility still require a hardware test.


## v0.3.20 experimental compatibility path

This release replaces the bundled service with a modified GPL build, version 0.1.0-tc-compat1, compiled using upstream SDK v0.43. The previous unmodified-release description above applies to v0.3.19. The modifications are reviewable under signal_build/ and opt in only through Status > Enable 5.10 LED test. Read RELEASE_NOTES.md for test and restoration steps. It is not a claim of firmware 5.10+ compatibility.
