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
- [x] Generate the Ape Escape main executable as C in the private staging workflow.
- [x] Syntax-check and compile all 55 generated C translation units to Wii PowerPC
      object files in private CI artifacts. No generated game code or disc assets
      are committed to this public repository.
- [x] Add Wii-only compatibility hooks for optional desktop debug/mod services,
      native-width culling, and a safe no-fast-path fallback for mutable guest text.
- [x] Compile the main-executable game code, real GTE/memory/GPU/interrupt/
      syscall services and Wii compatibility hooks into a private native DOL.
- [x] Add a Wii CUE/BIN sector reader and include the PS1 memory-card subsystem.
- [x] Add a Wii cooperative BIOS-thread fiber adapter backed by libogc.
- [x] Build and validate the first private native-game DOL (5,988,320 bytes):
      [GitHub Actions run](https://github.com/NVDEMU/ApeEscapeWii-privateassets/actions/runs/38784049209).
- [ ] Boot this DOL in Dolphin and confirm the game reaches at least its startup/logo/title screen.
- [ ] Bring up authentic PS1 BIOS reset/CD boot instead of the current direct PS-X EXE entry path.
- [ ] Validate CD-ROM audio tracks, dynamic overlay loading, controller mapping and save-card behavior.
- [ ] Validate gameplay, saves, audio, cutscenes and performance on real Wii/vWii hardware.

## Generated game-code cross-compile validation

The private preparation workflow now extracts `SCUS_944.23`, runs PSXRecomp to
generate the main executable's C translation units, and keeps those sources
private. A separate private Wii PowerPC check successfully syntax-checked and
compiled all 55 generated C units to object files. An unresolved-symbol audit
then found 28 external/runtime symbols remaining after accounting for symbols
defined by other generated objects.

A Wii-only compatibility layer now supplies optional debug/mod hooks, native
4:3 culling behavior, and a conservative refusal to execute unvalidated mutable
guest text through a native fast path. With that layer included in the private
symbol audit, 15 real runtime symbols remain: GTE command/register handling,
PS1 RAM reads and load timing, interrupt delivery, syscall/dispatch handling,
and VSync/HLE support.

The first private native-game build now links successfully after including the
real PS1 GTE, memory, GPU, interrupt, syscall and CD-ROM device code, plus the
Wii-specific ISO/CUE reader, memory cards and cooperative guest-thread adapter.
The generated C, executable image and raw disc files were staged only in the
private CI workspace; none are committed to this public repository.

The resulting private DOL is structurally validated, but it has **not yet been
boot-tested** in Dolphin or on a physical Wii/vWii. Its current entry path loads
the PS-X EXE header/payload into PS1 RAM and jumps to that executable's entry
point; this is a first boot attempt, not an authentic boot through the PS1 BIOS
reset vector and CD boot sequence. A successful link therefore does not yet
prove the game reaches a logo or title screen. Retail BIOS, disc data, generated
game C and compiled game objects must remain outside this public repository.

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

**Ape Escape has not yet been verified booting on Wii.** The private native-game
DOL now builds, but its first launch still needs a Dolphin or hardware test.
The current GP0 diagnostic remains a separate public test app; the private game
DOL instead uses PSXRecomp-generated native code and the PS1 runtime. Accurate
BIOS reset boot, remaining GPU command coverage, CD audio, runtime overlays,
and real-game compatibility are still incomplete.

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
