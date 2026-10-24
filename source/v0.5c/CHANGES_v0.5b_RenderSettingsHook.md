# v0.5b — preserve CONTROL internal render ratio during hotsampling

- Read output and internal render dimensions from the live GameWindow singleton (`+0xF4/+0xF8` output, `+0xFC/+0x100` internal), verified against the active game build and its Display page.
- Replace the obsolete render-size AOB with a MinHook detour on the exported `GameWindow::setUserRendererSettings` method.
- When the renderer commits settings after a hotsampling resize, copy the settings and recompute internal dimensions from the captured initial output-to-render ratio. Ratios within 0.0015 of common 1/12 increments are normalized, so 853/1280 resolves to 2/3 and stays at 66.7% on other output sizes. This applies equally to reduced render ratios and supersampling ratios.
- The hook changes only the local settings copy and then calls the original setter, allowing the engine's own renderer/resource recreation path to run.
- Compiled Release DLL and injector. A live resize ratio still needs visual confirmation in-game.