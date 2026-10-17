# ThemeChanger - PS5 Homebrew



Native theme chooser for PlayStation 5 homebrew. Reproduces the provided dark blue screenshot as a PS5 native 1920x1080 menu, built with [ps5-homebrew-ui](https://github.com/blackbearreloaded/ps5-homebrew-ui) and the [PS5-DevBridge](https://github.com/Yahballon/PS5-DevBridge) SDK framework.



## LiveShell integration

Uses the documented [LiveShell by ZiZc3](https://github.com/ZiZc3/LiveShell) GPL-3.0 REST endpoints on localhost:9050. It installs selected MP4 clips to `/data/LiveShell/video/ThemeChanger-*.mp4`, reads `/api/state`, and calls `/api/select?i=N`. When LiveShell is not running the app shows a clear message, instead of claiming PS5 firmware was modified. **Since v0.2.0 the release ZIP bundles LiveShell v1.0 and automatically launches it through existing elfldr (127.0.0.1:9021). No manual LiveShell loading is required once elfldr is running.** The bundled LiveShell engine still requires compatible ShellUI privileges and a running jailbreak ELF loader; no separate LiveShell installation is necessary.



## Controls

- D-pad / left stick: browse

- Cross: apply wallpaper (LiveShell must be active)

- Circle: return to backgrounds or exit

- L1: backgrounds; R1: change tabs

- Settings: toggle wallpaper, set dim, enable game pause or system settings suppression



## Files

- `native/main.cpp` native UI

- `native/liveshell_client.*` localhost LiveShell adapter

- `assets/generate.py` builds original previews and sample MP4s

- `app.json` metadata for TESTBUILD manifest



## Build

Push to GitHub `main` and use **Build PS5 ThemeChanger** workflow. Once completed, download the `PPSA99891.zip` release and extract to `/data/homebrew/`. The TESTBUILD manifest automatically discovers a release with tag `themechanger-v0.2.0`.



## Limitations

The preview icons are illustrative, not extracted from the installed PS5 dashboard. LiveShell v1.0 supports **video** backgrounds: still images are packaged as short looping H.264 clips. Actual wallpaper injection is performed by the separate LiveShell payload, not by ThemeChanger alone.



## License

GPL-3.0-or-later for this frontend, GPL-3.0 compatibility with LiveShell. See upstream licenses and credit.



## Integrated engine details



See [LIVESHELL_INTEGRATION.md](LIVESHELL_INTEGRATION.md) for source, license, checksum, installation, requirements, and troubleshooting.



## Online Wallpapers – MotionBGS (v0.3.0)



Browse publicly listed **1920x1080 MP4** wallpapers from [MotionBGS](https://motionbgs.com/) inside the native PS5 ThemeChanger interface. The GitHub Actions build discovers public item pages and official HD download links and packages **only metadata**, not third-party artwork or videos. Users download each video directly from the website to their PS5.



- **R1** cycle into Online Wallpapers; **L2/R2** filter categories (All, Featured, Anime, Games, Nature, Cars, Space, Technology, Fantasy)

- **Triangle** next page; **D-pad** navigate; **X** download/install (or apply when previously installed)

- TLS verified against CA roots, official-domain restrictions, 160MB size cap, progress meter, atomic `.part` downloads, MP4 signature validation

- Stored files: `/data/LiveShell/video/MotionBGS-<slug>.mp4`

- Thumbnails are fetched on-device to `/data/ThemeChanger/cache/`; if they cannot be decoded, show a placeholder

- After download, the LiveShell API automatically receives the video selection

- Built-in original themes and bundled LiveShell v1.0 remain unchanged



**Notes:** The online catalog is a snapshot refreshed during app builds, not an always-live search index. MotionBGS availability, CDN endpoints and copyright permissions may change; users should respect the rights of creators and site terms. LiveShell currently supports 1080p MP4, so 4K links are excluded.



## Controller input fix (v0.3.1)

Both the D-pad and the left analog stick now use edge-triggered input and controlled key repeats instead of treating every analog frame as a fresh press. This prevents unexpected rapid selection jumps when browsing large MotionBGS lists.



## v0.3.2: Gallery, preview troubleshooting and controller access



- Press **R1** once from Backgrounds to open Online Wallpapers, **L1** to return to Backgrounds. The sidebar remains accessible with Left from the first column, then Up/Down and X.

- Downloaded MotionBGS MP4s appear after the built-in themes in **Backgrounds**, including additional 12-item pages. Select with **X** to reapply through LiveShell.

- App diagnostics are written to `/data/ThemeChanger/themechanger.log` (rotates at 2MB to `themechanger.old.log`). Events include catalog, preview URL, download, image-decode, selection and LiveShell states. Send this log when thumbnails remain blank.

- Online previews use still artwork from the source webpage where offered. The build-time catalog adds a fallback for normal JPEG/PNG poster images when metadata is absent; no copyrighted preview files are bundled.



## v0.3.3 – Reliable wallpaper selection and ShellUI diagnostics



- PS5 controller: Uses newest scePadRead sample, preserves held state when the API returns zero samples, avoiding repeated Cross / theme activation after long LiveShell API calls.

- Official bundled LiveShell **v1.1** ELF (SHA-256 pinned). Important: if a previous v1.0 daemon is still running, **restart the PS5, re-enable your jailbreak and start ThemeChanger again** to use the new engine. No forced ShellUI restart is attempted.

- The application verifies LiveShell's `current`, `enabled`, `mainvideo`, and ShellUI `status` fields before reporting selection confirmed. Successful API response is **not** proof that the OS is visually drawing the video.

- Explicit global theme selection enables LiveShell's `mainvideo` when disabled (v1.1 API).

- Select timeout raised from 2 to 20 seconds: LiveShell can wait up to 8 seconds for the old ShellUI video to be released.

- Detailed diagnostics in `/data/ThemeChanger/themechanger.log`: `SELECT_REQUEST`, `SELECT_RESPONSE`, `PLAYBACK_STATE`, `SHELLUI_MONITOR`, `VIDEO_INFO`, `ENGINE_LEGACY`.

- If selecting works according to the REST API but the actual home screen stays unchanged, provide **both** the ThemeChanger log and `/data/LiveShell/liveshell.log`; include `/data/LiveShell/shellui.log` if present. Physical PS5 validation is required.



## v0.3.4 – reliable wallpaper switching and no redundant Media tile



LiveShell's normal `gamepause=1` logic treated ThemeChanger `PPSA99891` as a running game. During a wallpaper change it attempted to stop the previously playing video and sometimes timed out with `The home screen did not release the old video`. Our **source-built LiveShell integration** exempts only ThemeChanger from game detection while preserving game pause for actual games. Fast-switch should then work normally with a shared `/data/LiveShell/video/` mount.



Upstream LiveShell v1.1 by ZiZc3 is cloned during CI and patched in `liveshell_build/patch_liveshell.py`. Its normal `install_tile()` startup call is disabled: ThemeChanger alone manages the engine, so no *new* `LiveShell Engine` Media tile is registered. **Previously installed** `LVSH00001` may be removed once using the PS5 interface; we do not silently delete a registered PS5 app.



After installing v0.3.4 **fully reboot the PS5, enable the jailbreak and start ThemeChanger** to replace the older running daemon. Logs: `/data/ThemeChanger/themechanger.log` and `/data/LiveShell/liveshell.log`.



## v0.3.5 – Wallpaper stays visible when opening PS5 Home



Fixes a separate ShellUI focus bug: on the PS button, PS5 Home can open while ThemeChanger is still marked as the running application (PPSA99891). Although v0.3.4 exempted this title from LiveShell's game-pause detection, upstream **ShellUI** independently faded video to zero whenever FocusLayer.Game remained selected. Our custom GPL LiveShell source now shares the exact ThemeChanger-running state with the injected ShellUI and exempts this title from that fade, without disabling the hide/pause behavior for real games.



To test: apply a wallpaper, press PS to return to Home **without closing the app** and check that the animated background remains. Then reopen ThemeChanger, choose a second wallpaper and return to Home again. Reboot the console and reactivate the jailbreak after installing, to ensure the previous in-memory LiveShell build is unloaded.



Debug information: `/data/ThemeChanger/themechanger.log`, `/data/LiveShell/liveshell.log` (look for `ThemeChanger running app active/closed`) and LiveShell's ShellUI log if still reproducible.



## v0.3.6 – Auto-replace outdated LiveShell runtime



The on-console v0.3.5 logs showed `game=PPSA99891`, `shellui=stopped`, despite a source patch that excludes PPSA99891 from game handling. Root cause: the app previously accepted **any** existing LiveShell instance on port 9050 and never launched its bundled custom engine. The original game-pause logic remained resident, sometimes because another homebrew payload autostarted LiveShell before ThemeChanger.



Our source-built engine now includes `"themechanger_engine":"0.3.6"` in its `/api/state` response. On app start, ThemeChanger checks that exact revision. If an old/foreign daemon is active, the native loader sends our patched `liveshell.elf` to elfldr:9021 even while the old daemon is still online. LiveShell's supported `handover` mechanism stops the old engine and initializes the new version. No silent configuration changes to `gamepause` or force-terminate are used. Once the API confirms version 0.3.6, wallpapers can be selected again. If handover is unavailable or fails, the UI reports it rather than claiming an update.



If this still doesn't work, send `/data/ThemeChanger/themechanger.log`, `/data/LiveShell/liveshell.log`, and `/data/LiveShell/shellui.log`. Check especially `ENGINE_HANDOVER`, `ENGINE_HANDOVER_OK`, and `SHELLUI_MONITOR`.



## v0.3.7 – full MotionBGS catalogue, search and tags



- Reads every eligible desktop wallpaper from MotionBGS's official public sitemap rather than the old 153-item catalogue.

- Catalogue metadata includes available genre/category assignments and popular site tags. Site pagination is indexed at build time for tags, including anime/games/nature/cars and selected sub-tags. Tags count only indexed matches; absent tag assignments are not invented.

- Built-in **controller-friendly search keyboard**: switch to Online Wallpapers, press **Triangle**, move with D-pad or analog stick, **X** types, **Square** backspaces, **R2** clears, **Triangle** accepts. Search updates immediately without network requests.

- **Options** opens Tags with result counts; **L2/R2** pages through tags, **X** chooses, **Square** resets. Category switches with L2/R2 outside overlays.

- Grid: 12 cards per page, D-pad across boundaries; **Square** moves to previous page. Download/Install with **X** remains identical.

- MP4 and JPEG URLs for uncached sitemap entries are resolved *only when needed* from their original item page and kept in `/data/ThemeChanger/meta/` as local metadata. This avoids hammering the source site with 9K full-size downloads during a release and keeps the app ZIP small.

- GPU only keeps 72 JPEG textures in memory. More are reloaded from the thumbnail disk cache when revisiting a page.

- Retains ThemeChanger's integrated LiveShell 0.3.6 engine, so the verified PS5 background switching is unchanged.



Data provenance: MotionBGS public sitemap, metadata and tag listings. No MP4s or poster artwork are bundled in the release. Full list size follows the site's public sitemap at build time; invalid and mobile-only entries are excluded.



## v0.3.8 – Only downloaded backgrounds



The Backgrounds tab is now a personal downloaded-wallpaper library. All twelve original example wallpapers and their associated bundled MP4/PNG assets have been removed from the app UI and distribution ZIP.



Any wallpaper downloaded through Online Wallpapers is immediately added to Backgrounds by a completion event, without restarting ThemeChanger or waiting for a timed disk scan. Wallpapers appear **newest first** and can be reapplied with **X**. The library is paginated in groups of twelve. If nothing is downloaded, switch to Online Wallpapers using **R1**.



Existing MotionBGS downloads persist at `/data/LiveShell/video/MotionBGS-<slug>.mp4`. Old `ThemeChanger-*.mp4` sample clips from previous releases are not deleted from the console automatically, but are not part of this library.



## v0.3.9 – Black background / H.264 compatibility fix



Some MotionBGS Full HD MP4 videos report H.264 **Level 5.0** despite actually being 1920x1080@30fps. This differs from known-good 1080p MotionBGS files reporting Level 4.1 and can exceed the profile understood by the PS5 LiveShell ShellUI video player. The app previously only verified the first `ftyp` bytes and a codec signature; LiveShell's `"ok":true` select response never verified decoded frames.



The new `mp4_compat.hpp` checks the MP4 `avc1` sample entry and H.264 `avcC` plus SPS. On selecting a downloaded wallpaper (including one downloaded with a previous ThemeChanger version), it conditionally adjusts **only** a falsely high Level 5.0 declaration on Full HD H.264 streams to Level 4.2. Encoded frame data and image quality are unchanged, so this is quick even for large clips. Valid Level 4.1/4.2 clips stay untouched; unsupported and UHD codecs are not force-modified and selection returns an explicit compatibility error rather than quietly switching to black.



New logs in `/data/ThemeChanger/themechanger.log`: `MP4_COMPAT` and `MP4_INCOMPATIBLE`. Test by choosing any previously black wallpaper in Backgrounds and then returning to PS5 Home. The source-compatible level fix was validated on **four real MotionBGS 1080p30 Level 5.0 files** with FFprobe (which reads Level 4.2 after adjustment) and a separate Level 4.1 no-op case. Real PS5 decoding must still be confirmed by hardware test. For any remaining black wallpaper, report its exact title plus ThemeChanger and `/data/LiveShell/shellui.log` logs.



## v0.3.10 — Smooth H.264 and persistent library



Some MotionBGS MP4s are tagged H.264 Level 5.0 **because they require eight H.264 reference frames**. Merely changing their declared level (v0.3.9) can make a picture appear but the PS5 home screen decoder may still show very jerky animation. ThemeChanger now includes FFmpeg and x264 in the PS5 app itself, detects the SPS reference count, and re-encodes problem clips **on the PS5** to 1920×1080/30FPS H.264 Baseline with one reference frame. This is an actual re-encode, not a metadata workaround. Video quality may be slightly lower, and conversion can take significantly longer than downloading; it happens only once per clip. Conversion runs in the background instead of blocking navigation and does not overwrite the working MP4 until the new file is finished.



Videos are also archived under `/data/ThemeChanger/wallpapers/MotionBGS-*.mp4` and restored to `/data/LiveShell/video/` before the next library scan, protecting them from LiveShell cache changes or reboot-related disappearance. A console reset still unloads the homebrew payload; start ThemeChanger once after the jailbreak to start the integrated engine.



## v0.3.11 – PS5 crash hotfix and reliable download library



The v0.3.10 native FFmpeg/x264 change correlated with console crashes before a download-complete message. For stability, v0.3.11 does not compile that experimental codec into the PS5 binary; it retains the previous proven MP4 download path and the v0.3.9 H.264 header compatibility fix. This means some reference-heavy animated videos may still stutter until a separately verified converter is ready.



The download worker now has 2 MB of stack and diagnostic transfer-stage logs. Previously, the video scanner allocated 65 KB on a worker stack and could overflow it. Background persistence no longer relies on native `opendir` returning the expected filenames. Wallpaper backups and a filename index are saved under `/data/ThemeChanger/`; catalog-known MP4s are checked and recovered by their exact paths on startup. This fixes the misleading `WALLPAPERS_FOUND 0` condition after a reboot when directory enumeration fails.



After installing v0.3.11, try any new wallpaper and look for `VIDEO_TRANSFER_DONE`, `LIBRARY_SAVED`, and `DOWNLOAD_COMPLETE`. On the next launch check `LIBRARY_RECOVERY` and `WALLPAPERS_FOUND`. Videos interrupted before the successful download can be downloaded again.



## v0.3.12 – PS5 startup recovery



v0.3.10 and v0.3.11 introduced unverified native FFmpeg and automatic filesystem recovery operations, and users reported `system_illegal_function_call` as soon as the app launched. This emergency update **reverts boot and download code to v0.3.9**, the latest version confirmed to open and operate on the user's PS5, while retaining the full searchable MotionBGS library and integrated LiveShell functionality.



No downloaded videos or stored backups are removed. The experimental persistent-recovery feature is temporarily disabled so that a filesystem API failure cannot prevent the application from opening. Data-persistence and video-transcoding changes will be reintroduced only after isolated PS5 validation.



## v0.3.13 – Wallpapers now survive restarts



New wallpapers are downloaded first to `/data/ThemeChanger/wallpapers/MotionBGS-<slug>.mp4` and then atomically deployed to `/data/LiveShell/video/`. The ThemeChanger-owned archive is never replaced by a hardlink to a LiveShell file, and filenames are stored in `/data/ThemeChanger/wallpapers.index` so a restart does not rely on native `readdir` working correctly.



Start-up recovery touches only the saved filenames. Existing v0.3.12 files are found incrementally without blocking PS5 boot. If the app launches before migration finishes, give it some time in Backgrounds; previously downloaded files should populate automatically where they still exist.



**Video playback caveat:** a LiveShell response of `playing` reports file selection, not proof that hardware decoder frames are progressing. Many MotionBGS FullHD MP4s are encoded with H.264 eight reference frames; the PS5 ShellUI player may stall or watchdog-restart those streams. Changing the H.264 level header does not change the number of reference frames. We are retaining the stable media pipeline and awaiting `/data/LiveShell/shellui.log` to identify any further player-specific fault. Experimental on-PS5 transcoding stays off after the v0.3.10 crash.



## v0.3.16 – Signal Companion



Signal Companion is integrated as its own sidebar entry. See [usage, controls and source credits](SIGNAL_COMPANION.md). The service is bundled and starts only when selected.

