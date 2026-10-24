# v0.5c — clean user interface and camera path location

- Hotsampling shows only the current window output resolution and internal render resolution. Removed hook/AOB, swapchain, baseline, ratio diagnostics, and resolution history from the user interface.
- Kept target window width/height, Apply, and Restore controls.
- Removed IGCSDOF pipe names, protocol/transport details, and the raw camera pointer from the visible interface.
- Camera path files now save and load beside `ControlToolkit.dll`; the UI shows the resolved full path and the status confirms successful Save/Load.