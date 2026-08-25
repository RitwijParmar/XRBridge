# XRBridge Unity package

This UPM package binds Unity 2022.3 or newer to the XRBridge native library.
Build the native target for the editor platform, then copy `xrbridge.dll`,
`libxrbridge.dylib`, or `libxrbridge.so` into a `Runtime/Plugins/<platform>`
folder in the installed package or host project. Unity selects it through the
`xrbridge` P/Invoke library name.

Import **XRBridge Demo** from Package Manager > Samples. The recorded trace is
synthetic and requires no headset. The scene creates headset and controller
proxies, trails, and an on-screen status panel at runtime.

The package includes EditMode and PlayMode assemblies. The scene and Unity tests
must be run in a licensed Unity editor; they are not substitutes for native CTest.
