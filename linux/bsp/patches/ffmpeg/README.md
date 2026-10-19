# FFmpeg stateful decoder BSP adaptation

The default rootfs builders install complete distribution packages with the
stateful V4L2 decoder fixes:

| Target | Source and patch series | Package builder |
| --- | --- | --- |
| Debian GNOME / KDE | FFmpeg 7.1.5, `series.json` | `tools/build_ffmpeg_packages.py` |
| SteamOS | FFmpeg 7.1.5, `series.json` | `tools/build_arch_ffmpeg.py` |
| Nyarch | FFmpeg 9.0.2, `series-9.json` | `tools/build_arch_ffmpeg.py` |

Arch packages preserve the installed codec configuration, dependencies,
epoch and library ABI. A different upstream ABI needs its own patch port.
Source archives and patches are checksum-pinned and applied without fuzz.
Stage a copy with `tools/prepare_ffmpeg_source.py --output build/<new-directory>`;
add `--series linux/bsp/patches/ffmpeg/series-9.json` for FFmpeg 9.

Real Iris hardware has decoded and encoded the 12-frame H.264 sample with
both versions. The full FFmpeg 9 package was built and installed on Nyarch.
Resolution-change playback still needs the capture-pool lifetime patches in
`drafts/` and the companion Iris kernel fixes; these are not enabled by either
series yet. See [the codec development notes](../../../../docs/devel/piano-video-codec.md).
