# CONTROL Toolkit v0.5c — Injector

Start CONTROL in **DX12** mode first (`Control_DX12.exe`), then double-click `Inject-Control.cmd` (or run `ControlToolkitInjector.exe`).

The injector loads only `ControlToolkit.dll` located **beside the injector EXE** into the selected running `Control_DX12.exe` process. Use the same elevation level as the game. It does not launch the game or bypass any game protection.

**Package:** includes the compiled x64 DLL and injector. The overlay and hotsampling functionality must be tested in-game. The injector reports a remote LoadLibrary result, not full overlay initialization. Do not inject twice in one session.
