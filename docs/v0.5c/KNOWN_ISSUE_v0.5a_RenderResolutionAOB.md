# Resolution status

Resolved in v0.5b: the active build reads GameWindow's internal/output size fields and hooks the exported renderer settings setter, so it no longer relies on the obsolete AOB below. See CHANGES_v0.5b_RenderSettingsHook.md. The in-game hotsampling resize still needs visual confirmation.

# v0.5a test result — render resolution hook not compatible with current game build

The user's in-game screenshot disproves the v0.5a render-resolution claim:

- CONTROL's Display page shows window/output resolution `1280 x 720` and selected Render Resolution `853 x 480` with DLSS enabled. This is approximately `66.7%` on both axes.
- Toolkit reports `Render-size AOB: AOB not found`; therefore the v0.5a code cave never installed.
- Toolkit reports `coreshared::g_vRenderResolution = 1280 x 720` and derives a 100% ratio. In this tested state, that exported vector tracks the output/window dimensions and is not a valid source for the DLSS internal render ratio.
- The displayed `3840 x 2160` expected resolution is consequently based on a bad 100% baseline and must not be treated as meaningful.

The tested game binary is `D:\SteamLibrary\steamapps\common\Control\coregame_rmdwin10_f.dll` (timestamp 2026-07-10, file size 9,437,184 bytes). A read-only scan of both the on-disk DLL and the live module found zero occurrences of the v0.5a signature `89 84 24 84 00 00 00 E8`. Do not retry that signature or ship another build claiming ratio preservation on this binary.

The DLL exports current `GameWindow` methods including `handleAppEvent(WindowResizeEvent)` at RVA `0xAC950`, `setUserRendererSettings` at RVA `0xAD010`, and `updateScreenSize` at RVA `0xAD990`. Their current code differs from the older IGCS AOB path. They are investigation leads only; no new hook should be installed until its input/output dimensions and effect on renderer resource recreation are verified against this build.

## Required follow-up

Identify a current-build source for the selected internal render dimensions (the game UI confirms `853 x 480` here), then trace the renderer's resize/settings path and add a unique, validated AOB for that build. Until that is done, report the shared export as an output-size diagnostic only and do not label its ratio as DLSS/supersampling render scale.
