# ThemeChanger v0.3.21 - Signal firmware support notice

- Show a persistent support notice on Status, Colour and Startup: regular support is firmware 9.00+, with upstream testing on PS5 Pro 13.60.
- Explicitly label the firmware 5.10 compatibility test as experimental and unconfirmed.
- Adjust row spacing to keep all controls below the notice and inside the panel.
- Retain the opt-in compatibility test and restoration controls from v0.3.20.
## v0.3.22 — Firmware 5.10 console LEDs and background effects

- Automatically select the hardware-tested 5.10 single-step LED hold program.
- Keep static Blue / White / Amber mixes, including the visually confirmed purple mix.
- Add an Effects tab: Pulse, Blink, 4 Colours, Colour Wave and Breathe.
- Adjust cycle duration between 500 and 20,000 ms.
- Run effects in the resident Signal payload after the app closes; optionally save the mode for its next start.
- Stop effects before manual colour changes or system-colour restoration; retain the original-state backup.
- Verify colour writes and stop an effect on a device error. Avoid per-frame success logging.
- Add regression coverage for hold programs, effect bounds, persistence, error latching and UI apply/save/stop actions.

The low-level LED protocol was tested on firmware 5.10. This integrated app build and its continuous modes have not yet been validated on console. Existing 9.00+ Signal behaviour remains available; these new continuous modes currently target 5.10.
