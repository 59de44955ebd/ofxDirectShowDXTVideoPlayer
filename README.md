# ofxDirectShowDXTVideoPlayer

A simple video player addon for openFrameworks that plays HAP-encoded videos natively on Windows using DirectShow video playback tools.

HAP is a video codec that is decoded on the GPU. Some of its benefits include fast decompression and low CPU usage. It was written by Tom Butterworth for VidVox/VDMX. For further information on HAP, see http://vdmx.vidvox.net/blog/hap.

This addon is based on code of ofxDSHapVideoPlayer by secondstory:
https://github.com/secondstory/ofxDSHapVideoPlayer

But the original code was changed quite a bit, and this addon works differently. It uses the LAV Splitter Source Filter as splitter, and the HapDecoder filter as decoder. It receives DXT compressed texture frames from HapDecoder, and uploads them to the GPU via OpenGL.

Unlike ofxDSHapVideoPlayer, this addon supports various containers (AVI, MOV, MKV), and also supports HAP videos encoded by FFmpeg.


*Requirements*

- the addon is preconfigured for openFrameworks v0.10.x and VS2017, but it also works with openFrameworks v0.9.x and VS2015. It was so far tested with v0.10.0 and v0.9.8 in Win 8.1 x64.

- The addon depends on two DirectShow filters, LAVSplitter.ax and HapDecoder.ax. Those must be registered in the system for the current platform (Win32 and/or x64). Binaries are included in folder "setup", just run batch script "__register_run_as_admin.bat" as admin (Explorer -> context menu -> Run as administrator" to register them. Otherwise the official sources for those filters are here:
https://github.com/Nevcairiel/LAVFilters
https://github.com/59de44955ebd/HapDecoder


*Usage*

see example/src/ofApp.cpp
