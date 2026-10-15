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
- [x] Build and upload the live controller/GX diagnostic `.dol`:
      [workflow runs and artifacts](https://github.com/NVDEMU/ApeEscapeWii/actions/workflows/wii-smoke.yml).
- [ ] Test the diagnostic DOL in Dolphin and on Wii/vWii hardware.
- [ ] Connect `sw_render_display()` to `psx_wii_video_present_argb()`.
- [ ] Add a PS1 pad-state bridge matching the framework runtime API.
- [ ] Create a Wii runtime target that excludes SDL, desktop GL/Vulkan and the
      desktop launcher/host-only services.
- [ ] Compile representative generated game-code units for PowerPC and resolve
      ABI/compiler assumptions.
- [ ] Bring up BIOS reset execution, RAM/BIOS loading and deterministic timing.
- [ ] Add Wii audio, CD-ROM/disc loading and statically built overlay coverage.
- [ ] Validate gameplay, saves, audio, cutscenes and performance on real hardware.

## Verified CI result

The successful smoke build uses the `NVDEMU/psxrecompwii` submodule and the
official `devkitpro/devkitppc` Docker image. GitHub Actions verified and
uploaded `boot.dol` as the `ape-escape-wii-smoke-dol` artifact (about 170 KB).

The diagnostic DOL calls the GX presenter and draws live bars reflecting
Nunchuk movement, IR aiming and buttons. It has **not** been run in Dolphin or
on physical hardware in CI, so actual display output and input behavior still
need testing.

The first DOL was rejected by Dolphin's loader because `elf2dol` left the
final data section 12 bytes short of its 32-byte-aligned read boundary. The
Makefile now pads the DOL after conversion, and CI checks the aligned section
bounds before uploading an artifact.

**Ape Escape itself does not run on Wii yet.** The DOL is only a platform
diagnostic; the PSXRecomp runtime and game have not been integrated.

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
