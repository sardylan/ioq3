# OpenGL ES 2.0 renderer

`renderer_opengles2` is an alternate renderer for ioquake3 targeting
OpenGL ES 2.0 hardware, in particular the Raspberry Pi 3, 4 and 5 running
the open source Mesa drivers (`vc4` / `v3d`).

It is derived from the OpenGL 1 renderer, so it looks exactly like vanilla
Quake 3 and is compatible with every mod that works with `opengl1`. The
OpenGL 1.x fixed function pipeline is replaced by a small set of GLSL ES
programs, which makes it both portable to OpenGL ES 2.0 and cheaper to run
on tile based GPUs than the `opengl2` renderer.


## Features

  - Requires only OpenGL ES 2.0, never falls back to desktop OpenGL; uses
    OpenGL ES 3.x features when available.
  - Same output as the `opengl1` renderer: lightmaps, multitexture, dynamic
    lights, fog, sky, portals and mirrors, stencil shadows (`cg_shadows 2`).
  - Warns loudly at startup when the OpenGL ES context is software rendered
    (`llvmpipe` / `softpipe`).
  - 16 bit vertex indexes, as required by the Raspberry Pi 3 GPU.
  - Optional 16 bit textures (`r_texturebits 16`, RGB565 / RGBA4444).
  - Tile based GPU friendly: every buffer is cleared at the start of a
    frame and depth / stencil are discarded before the buffer swap.


## Building

The renderer is not built by default. Enable it with
`-DBUILD_RENDERER_GLES2=ON`:

    cmake -S . -B build -DBUILD_RENDERER_GLES2=ON -DCMAKE_BUILD_TYPE=Release
    cmake --build build

The result is `build/Release/renderer_opengles2.so`, next to the client.
For a static build (`-DUSE_RENDERER_DLOPEN=OFF`) enable only one renderer.

### Raspberry Pi

Packages needed on Raspberry Pi OS / Debian 13 (trixie) to run the game:

    apt install --no-install-recommends libsdl2-2.0-0 libegl1 libgles2 libopenal1

and additionally to build it:

    apt install --no-install-recommends build-essential cmake ninja-build git libsdl2-dev

`libegl1` and `libgles2` pull in the Mesa driver (`mesa-libgallium`).
Everything else (zlib, libjpeg, Ogg/Vorbis/Opus, the curl and OpenAL
headers) is built from `code/thirdparty`. `mesa-utils-bin` provides
`eglinfo`, useful to check the GPU is used:

    eglinfo -B -p gbm      # "OpenGL ES profile renderer: V3D 4.2..." on a Pi 4

`CMakePresets.json` has native build presets for the Raspberry Pi, with the
OpenGL ES 2.0 and OpenGL 1 renderers enabled:

| Preset         | OS             | Runs on       | CPU flags                         |
|----------------|----------------|---------------|-----------------------------------|
| `rpi-aarch64`  | 64-bit         | Pi 3, 4, 5    | `-mcpu=cortex-a53`                |
| `rpi4-aarch64` | 64-bit         | Pi 4, 5       | `-mcpu=cortex-a72`                |
| `rpi5-aarch64` | 64-bit         | Pi 5          | `-mcpu=cortex-a76`                |
| `rpi-armhf`    | 32-bit (armhf) | Pi 3, 4, 5    | `-mcpu=cortex-a53 -mfpu=neon-fp-armv8 -mfloat-abi=hard` |

    cmake --preset rpi-aarch64
    cmake --build --preset rpi-aarch64

The binaries end up in `build/<preset>/Release/`.


## Running

    ./ioquake3 +set cl_renderer opengles2

`cl_renderer` is archived, so it only needs to be set once.

### Raspberry Pi without a desktop (KMSDRM)

The game can run straight from the Linux console, without X11 or Wayland,
through SDL's KMSDRM video driver. SDL picks it automatically when no
display server is running; to force it:

    SDL_VIDEODRIVER=kmsdrm ./ioquake3 +set cl_renderer opengles2

Requirements:

  - The KMS driver must be enabled, which is the default on current
    Raspberry Pi OS: `dtoverlay=vc4-kms-v3d` in `/boot/firmware/config.txt`.
    Without it Mesa falls back to software rendering, and the renderer
    prints a warning naming `llvmpipe`.
  - The user must be in the `video` and `render` groups (GPU) and in the
    `input` group (keyboard and mouse through evdev), plus `audio` for sound.
  - A display must be connected (`cat /sys/class/drm/card*-HDMI*/status`).
  - Run it from the local console. When started over SSH it still uses the
    HDMI output, but keyboard and mouse input come from the Pi.

KMSDRM is always fullscreen. A lower `r_mode` sets a lower display mode and
the monitor does the scaling for free, which is the easiest way to trade
resolution for speed:

    ./ioquake3 +set cl_renderer opengles2 +set r_mode -1 +set r_customwidth 1280 +set r_customheight 720

`r_mode -2` uses the current desktop resolution.


## Cvars

  * `r_vbo` - Vertex submission, latched.
      * 0 - Client side vertex arrays (default). Mesa streams them through
            its own upload buffer, the fastest path on the Raspberry Pi.
      * 1 - Streaming vertex buffer objects. Always used with WebGL.

  * `r_texturebits` - Texture precision.
      * 0 or 32 - 32 bit RGBA (default).
      * 16 - RGB565 / RGBA4444, halves texture memory and bandwidth.
             Lightmaps always stay 32 bit.

Every other cvar behaves as in the `opengl1` renderer.


## Limitations

These need features OpenGL ES doesn't have:

  - Light flares (`r_flares 1`) read back the depth buffer and need
    `GL_NV_read_depth`; without it flares are disabled with a warning.
  - `r_measureOverdraw` reads back the stencil buffer and is not supported.
  - Quad buffered stereo (`r_stereoEnabled`) is not supported; anaglyph
    stereo (`r_anaglyphMode`) works.
  - `r_ext_compressed_textures` has no effect: OpenGL ES can't compress
    textures on upload.


## Performance

`timedemo 1; demo four` (the `four.dm_68` demo from the Quake 3 1.32
patch), fullscreen through KMSDRM, `r_swapInterval 0`, on a Raspberry Pi 4
(8 GB, Raspberry Pi OS / Debian 13, Mesa 26.2, `rpi-aarch64` build):

| Renderer                | 1920x1080 | 1280x720  |
|-------------------------|-----------|-----------|
| `opengles2`             | 79.1 fps  | 140.0 fps |
| `opengles2`, `r_vbo 1`  | 64.0 fps  | 79.9 fps  |
| `opengl1`               | 59.5 fps  | 119.3 fps |
| `opengl2` (*)           | 7.5 fps   |           |

(*) not part of the Raspberry Pi presets, measured with a default
`-DCMAKE_BUILD_TYPE=Release` build.

At 1920x1080 the Pi 4 is limited by fill rate: lowering the resolution is
the most effective way to gain speed.
