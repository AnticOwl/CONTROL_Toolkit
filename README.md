# CONTROL Toolkit v0.5c

An in-game toolkit for **CONTROL (DX12)**.

## Download

- [Windows release package](releases/ControlToolkit_0.5c.zip)
- [Source code for v0.5c](source/v0.5c/)
- [v0.5b renderer settings notes](docs/v0.5c/CHANGES_v0.5b_RenderSettingsHook.md)
- [v0.5c interface notes](docs/v0.5c/CHANGES_v0.5c_CleanHotsamplingUI.md)

## Install and run

1. Extract the release ZIP.
2. Start CONTROL in DX12 mode and load into the game.
3. Run `Inject-Control.cmd` (or `ControlToolkitInjector.exe`) from the extracted folder.
4. Press **F10** to show or hide the toolkit.

The injector and DLL must remain in the same folder. Inject once per game session.

## Included features

- Hotsampling controls for window and internal render resolution. The renderer settings hook preserves the game's current render scale, including reduced and supersampling ratios, when the window is resized.
- Post-processing and native Northlight light controls.
- Camera path editing and configurable shortcuts.
- IGCSDOF provider communication through the IGCS Relay named-pipe protocol.

## Build from source

The v0.5c source is in [source/v0.5c](source/v0.5c), including the CMake project, injector, and build scripts. Configure and build with CMake and a Windows C++ toolchain. CMake fetches the pinned Dear ImGui and MinHook versions during configuration.

The source tree does not include generated build files or third-party dependency sources.
