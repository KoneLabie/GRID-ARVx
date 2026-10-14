# Recording encoder

The 0.4.18 standard Windows installer does not bundle an encoder. Select FFmpeg
in Settings to record, or enable the optional `bundle_encoder` GitHub Actions
input. The remaining build/source details below describe that optional path;
the source build must complete on a runner before it can be treated as verified.

GRID-ARVx sends rendered RGB frames through a pipe to a separate FFmpeg executable. It does not link FFmpeg libraries into the application or plugins. Recording contains visuals only; audio processing continues independently.

`scripts/encoder.py` builds from the official FFmpeg 7.1.5 release archive and FFmpeg's NVIDIA codec-header tag n12.2.72.0. Both source archives are pinned by SHA-256. The build disables GPL, nonfree, network protocols and external media libraries, and enables only the media paths needed for recording and hardware capability checks. macOS builds both Intel and Apple silicon binaries and joins them with `lipo`; its build uses software encoding. Windows and Linux enable NVIDIA's dynamically loaded NVENC path. A compatible GPU and driver are still required.

The encoder's source archives, license texts, build script, configure commands and binary hash accompany each installer under the shared `tools/Source` folder. This follows the source-distribution approach described at https://ffmpeg.org/legal.html. FFmpeg remains the work of its upstream authors and is distributed under its own license. No libx264 or other GPL codec is enabled by this build.

Developer requirements: Python 3.12+, GCC or Clang, GNU make, pkg-config and nasm on x86. On Windows use MSYS2 MINGW64's Python/GCC/pkgconf packages; this is a build dependency, not an end-user requirement. Build the encoder before staging products:

```sh
python scripts/encoder.py
```

The script tests RGB input to MP4 and MKV before allowing staging. The workflow performs these checks on every target OS. End users get the executable automatically through their installer. The app can also accept a custom encoder path in Settings for developer troubleshooting.

The new source-build workflow is not yet validated. See REVISION-STATUS.md before treating the installer build as ready for distribution.
