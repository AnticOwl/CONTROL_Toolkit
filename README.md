# CONTROL Toolkit v0.5c

An in-game toolkit for **CONTROL (DX12)**.

## Download

- [Windows release package](releases/ControlToolkit_0.5c.zip)
- [v0.5c source code](source/v0.5c/)
- [About IGCSConnector](docs/v0.5c/IGCSDOF_SETUP.md)
- [v0.5b renderer settings notes](docs/v0.5c/CHANGES_v0.5b_RenderSettingsHook.md)
- [v0.5c interface notes](docs/v0.5c/CHANGES_v0.5c_CleanHotsamplingUI.md)

## Install

1. Extract the release ZIP.
2. Copy the included `IGCSRelay.addon64` into the same ReShade add-ons folder as your existing `IgcsConnector.addon64`.
3. Start CONTROL in DX12 mode and load into the game.
4. Run `ControlToolkitInjector.exe` and press **F10** to show or hide the toolkit.

Keep `ControlToolkit.dll` beside the injector. Inject once per game session.

## About IGCSConnector

Frans' IGCSDOF requires IGCS Relay alongside the IGCS Connector. Enable both ReShade add-ons, enable the named-pipe provider in the toolkit's **IGCSDOF** tab, then start the DOF session from **IGCS Connector → Depth of Field control**. The Relay file is included in the release ZIP.

## Included features

Hotsampling, post-processing, native Northlight light controls, camera paths, and IGCSDOF provider support through IGCS Relay.

## Build from source

The [v0.5c source](source/v0.5c/) includes the CMake project, injector, and build scripts. Build with CMake and a Windows C++ toolchain. CMake fetches the pinned Dear ImGui and MinHook versions during configuration.
