# Frans' IGCSDOF setup — CONTROL Toolkit v0.5c

Frans' DOF workflow needs IGCS Relay installed beside the IGCS Connector ReShade add-on.

1. Extract the CONTROL Toolkit ZIP.
2. Copy `IGCSRelay.addon64` from the ZIP into the same ReShade add-ons folder as `IgcsConnector.addon64`.
3. Make sure both **IGCS Connector** and **IGCSRelay** are enabled in ReShade.
4. Start CONTROL in DX12 mode, enter the game, then run `ControlToolkitInjector.exe`.
5. In the toolkit's **IGCSDOF** tab, enable the named-pipe provider.
6. In ReShade, open **IGCS Connector → Depth of Field control** and start the DOF session.

Put the Relay in the Connector's ReShade add-ons folder, not beside the toolkit DLL or injector.
