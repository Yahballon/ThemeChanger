# Signal integration — ThemeChanger v0.3.22

Open Signal Companion in the sidebar. L1/R1 select Status, Colour, Startup
and Effects. Circle returns to the sidebar. Opening the section starts the
bundled loopback-only Signal service if it is offline.

## Firmware 5.10

The payload detects firmware 0x05100023, validates the 130-byte indicator state,
and saves the original state before changing a light. Each 42-byte channel
program is replaced with `{channel,1,2,level,2,1,0}`. This format was tested
through DevBridge: blue, amber, purple (32/0/255) and repeated colour changes
were visually confirmed, with successful restoration. Other firmware keeps the
original Signal colour path. New continuous effects currently target 5.10;
integrated console validation of v0.3.22 remains pending.

After updating choose Status → Restart ps5-signal once. Installing the app
does not replace an already running service.

## Colours and effects

Colour provides Blue / White / Amber sliders, presets, six personal slots and
Keep this colour. These are not independent RGB channels; red and green have
not been established by the hardware tests.

Effects provides Pulse, Blink, 4 Colours, Colour Wave and Breathe, with a
500–20,000 ms cycle. Pulse/Blink/Breathe use the Colour mix. Four Colours cycles
blue, white, amber and purple; Colour Wave interpolates that palette.

Apply runs the mode in the resident payload after ThemeChanger closes.
Save writes `/data/ThemeChanger/signal-effect.txt` atomically and resumes the
mode on the next payload start. Enable Start with ps5-autoloader to reload it
after reboot. Saving alone does not install an autostart service.
Manual colours cancel the saved effect. Stop/restore cancels its worker and
restores the captured system program. A failed write latches the effect off.
Successful animation frames are not logged individually.

No LED firmware or NVS is flashed. Original-state backup:
`/data/ThemeChanger/signal-led.before.bin`; hardware failures:
`/data/ThemeChanger/signal-hardware.log`.

## Startup and existing features

Startup edits upstream beep/light sequences (24 steps, 10 seconds total).
Colour saves preserve startup; startup saves preserve the kept colour.
The existing palette and autoloader controls remain available.

## Provenance and validation

GPL-3.0-or-later. Signal Companion backend by Marice, based on ps5-homebrew-ui
by BlackBearReloaded. Companion source: Marice/ps5-signal-companion at
dd34b9917871830547be000fe3e7ae6d932c8abd. The bundled modified Signal payload
is built from Marice/ps5-signal at 8ed455197a8643302a066b7b0e97972036998304
plus the reviewable signal_build files. Its version is 0.1.0-tc-led510. It binds
only to 127.0.0.1:9150, never SIGNAL_DEV/LAN. The GPL license ships beside it.

CI covers exact hold bytes, layout validation, backup/restore, failure latching,
effect bounds, persisted restart, serialized stop, UI actions, config section
preservation and HTTP transport. Native build/package checks are separate from
physical console validation.
