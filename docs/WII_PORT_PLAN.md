# ApeEscapeWii: Native Wii/vWii Port Plan

This branch is the experimental native PowerPC port. The goal is to reuse
PSXRecomp's generated game logic while replacing its desktop runtime services
with Wii/vWii homebrew services.

## Current milestone

- [x] Add a devkitPro/devkitPPC CI smoke build.
- [x] Build and publish a standalone Wii homebrew `boot.dol` artifact.
- [x] Confirm the initial Wii toolchain build succeeds on GitHub Actions.
- [x] Add a provisional Nunchuk movement / IR-aim / DualShock-button adapter.
- [x] Fork and pin the PSXRecomp framework at
      [NVDEMU/psxrecompwii](https://github.com/NVDEMU/psxrecompwii).
- [x] Add an initial GX frame presenter to the framework using libogc.
- [ ] Confirm the updated input + GX source set builds in CI:
      [latest run](https://github.com/NVDEMU/ApeEscapeWii/actions/runs/38751716108).
- [ ] Test the diagnostic DOL in Dolphin and on Wii/vWii hardware.
- [ ] Connect `sw_render_display()` to `psx_wii_video_present_argb()`.
- [ ] Add a Wii-specific PS1 pad-state bridge matching the framework runtime API.
- [ ] Create a Wii runtime target that excludes SDL, desktop GL/Vulkan, and the
      desktop launcher/host-only services.
- [ ] Compile representative generated game-code units for PowerPC and resolve
      ABI/compiler assumptions.
- [ ] Bring up BIOS reset execution, RAM/BIOS loading and deterministic timing.
- [ ] Add Wii audio, CD-ROM/disc loading and statically built overlay coverage.
- [ ] Validate gameplay, saves, audio, cutscenes and performance on real hardware.

## Verified CI result

The first smoke DOL was built with the official `devkitpro/devkitppc` Docker
image and uploaded as the `ape-escape-wii-smoke-dol` artifact. It initializes
video and reports Nunchuk movement, Wii Remote IR aim, and provisional
PS1-style button mappings.

The framework fork now contains `runtime/platform/wii/psx_wii_input.c` and
`psx_wii_video.c`. The controller module was successfully compiled and linked
into the prior diagnostic DOL. The latest run is checking the new GX presenter
as well; its result is linked above.

**Ape Escape itself does not run on Wii yet.** The DOL is only a toolchain and
platform-module smoke test; neither the PSXRecomp runtime nor the game itself
has been integrated or tested on hardware.

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
