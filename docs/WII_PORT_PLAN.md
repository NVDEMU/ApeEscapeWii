# ApeEscapeWii: Native Wii/vWii Port Plan

This is the experimental native PowerPC port. The goal is to reuse
PSXRecomp's generated game logic while replacing its desktop runtime services
with Wii/vWii homebrew services.

## Current milestone

- [x] Add a devkitPro/devkitPPC CI smoke build.
- [x] Pin the PSXRecomp framework fork at
      [NVDEMU/psxrecompwii](https://github.com/NVDEMU/psxrecompwii).
- [x] Add a libogc Wii Remote/Nunchuk input adapter.
- [x] Add an initial GX ARGB frame presenter using tiled RGB565 textures.
- [x] Add DOL padding and validation so 32-byte-aligned section reads stay
      within the executable file.
- [x] Build and upload the controller + GP0 + software-renderer diagnostic `.dol`:
      [successful build](https://github.com/NVDEMU/ApeEscapeWii/actions/runs/38756604488).
- [ ] Test the diagnostic DOL in Dolphin and on Wii/vWii hardware.
- [x] Link the existing PS1 software rasterizer (`gpu_sw_renderer.c`) into the Wii diagnostic.
- [x] Draw a low-resolution PS1-VRAM scene with PS1 renderer primitives and present it through GX.
- [x] Add a Wii GP0 command-stream adapter for fill/flat rectangles, flat and
      Gouraud triangles/quads/lines, and basic draw-area/offset/mask commands.
- [x] Change the diagnostic scene to submit GP0 packets before scanout.
- [ ] Test the GP0-renderer DOL in Dolphin and validate scene/colors/controller response.
- [ ] Expand GP0 coverage to textured primitives, polylines, VRAM read/write/copy
      commands and command-state edge cases.
- [ ] Connect the complete PS1 GPU state/scanout path to `psx_wii_video_present_argb()`.
- [ ] Add a PS1 pad-state bridge matching the framework runtime API.
- [ ] Create a Wii runtime target that excludes SDL, desktop GL/Vulkan and the
      desktop launcher/host-only services.
- [ ] Compile representative generated game-code units for PowerPC and resolve
      ABI/compiler assumptions.
- [ ] Bring up BIOS reset execution, RAM/BIOS loading and deterministic timing.
- [ ] Add Wii audio, CD-ROM/disc loading and statically built overlay coverage.
- [ ] Validate gameplay, saves, audio, cutscenes and performance on real hardware.

## Verified CI result

The successful build uses the `NVDEMU/psxrecompwii` submodule and the official
`devkitpro/devkitppc` Docker image. The GP0-renderer build is linked here:
[GitHub Actions run](https://github.com/NVDEMU/ApeEscapeWii/actions/runs/38756604488).
Download `ape-escape-wii-smoke-dol` from that run's Artifacts section, or use
the direct [boot.dol artifact](https://github.com/NVDEMU/ApeEscapeWii/actions/runs/38756604488/artifacts/11983887363).

The DOL links `gpu_sw_renderer.c`, `gpu_vram_dirty.c`, a Wii GX presenter and a
small GP0 packet decoder. It generates a 320x240 platforming-style scene by
sending GP0 packets for flat rectangles, triangles and lines into PS1-style VRAM,
then converts it with `sw_render_display()` and presents the result through GX.
Nunchuk input moves the placeholder actor and the Wii Remote pointer aims a
net line. This is a procedural GPU-pipeline test, not original game content.

It has **not** yet been tested in Dolphin after this code change. The next
important test is to confirm visible scene output, correct colors, and
controller response in the emulator.

The initial DOL lacked the final section's 32-byte-aligned padding; the Makefile
now pads the DOL after conversion, and CI checks aligned section bounds.

The first GX bring-up reached a green screen but Dolphin reported a GatherPipe
FIFO overflow. The presenter originally called `GX_Init(NULL, 0)`; it now
allocates a 32-byte-aligned 256 KiB FIFO and passes it to `GX_Init`. That
corrected DOL booted successfully in Dolphin. The later GP0-renderer DOL builds
and passes DOL-header/padding validation, but still needs an in-emulator retest.

**Ape Escape itself does not run on Wii yet.** Current GP0 coverage is a subset
for untextured primitives and basic state; it ignores unsupported opcodes rather
than acting as a complete GPU model. The CPU execution loop, full GPU/display
state, textured commands, audio, BIOS/disc loading and generated game overlays
still need integration.

## Architecture

The original game logic is machine-translated from MIPS to C by PSXRecomp.
This project aims to compile that C to native PowerPC for Wii/vWii while
implementing the required PS1 hardware/runtime services using Wii homebrew APIs.
This is not a ROM launched by a Wii PS1 emulator, and it is not a high-level
rewrite of the game's engine.

## Development rules

- Do not commit PS1 disc images, retail BIOS files, extracted executables,
  generated copyrighted game C, memory cards, or runtime dumps.
- Keep the upstream desktop build working; isolate Wii-specific changes behind
  a separate platform target.
- Validate compile/link success separately from game boot, gameplay and
  real-hardware performance.
- Keep the same Wii DOL target compatible with original Wii and Wii U vWii
  wherever the homebrew APIs permit it.
