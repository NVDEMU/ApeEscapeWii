# ApeEscapeWii: Native Wii/vWii Port Plan

This branch is the experimental native PowerPC port. The initial goal is to
prove the Wii homebrew toolchain and continuous-integration pipeline before
trying to compile the full recompiled game runtime.

## Current milestone

- [x] Add a devkitPro/devkitPPC CI smoke build.
- [x] Build and publish a standalone Wii homebrew `boot.dol` artifact.
- [x] Confirm the smoke workflow succeeds on GitHub Actions:
      [successful run](https://github.com/NVDEMU/ApeEscapeWii/actions/runs/38749436453).
- [ ] Test the smoke-test DOL in Dolphin and on Wii/vWii hardware.
- [ ] Fork/pin the PSXRecomp framework for Wii-specific runtime work.
- [ ] Audit the pinned PSXRecomp runtime for desktop-only dependencies.
- [ ] Compile a representative generated game-code translation unit for
      PowerPC and resolve architecture/compiler assumptions.
- [ ] Design Wii-compatible PS1 GPU, SPU, GTE, timing, threading and memory
      backends.
- [ ] Build a minimal game-runtime executable.
- [ ] Add Wii Remote/Nunchuk input mapping to the game runtime.
- [ ] Expand statically compiled coverage for dynamically loaded game overlays.
- [ ] Validate gameplay, saves, audio, cutscenes and performance.

## Verified CI result

The CI smoke test builds `wii-smoke/boot.dol` using the official
`devkitpro/devkitppc` Docker image. The successful run uploaded the
`ape-escape-wii-smoke-dol` artifact (about 154 KB). It initializes Wii video
and polls Wii Remote buttons.

This DOL is only a toolchain/controller smoke test. It does not run Ape Escape,
does not include the PS1 game runtime, and has not yet been tested on physical
Wii/vWii hardware.

## Architecture

The original game logic is machine-translated from MIPS to C by PSXRecomp.
This project aims to compile that generated C to native PowerPC for Wii/vWii
while implementing the required PS1 hardware/runtime services using Wii
homebrew APIs. It is not a ROM launched by a Wii PS1 emulator, and it is not
yet a high-level rewrite of the game's engine.

The first `wii-smoke` program is intentionally independent of PSXRecomp. It
exists only to verify that GitHub Actions can produce a valid homebrew DOL
with devkitPPC/libogc. A successful smoke build does not mean the game's
generated code or runtime has been ported.

## Development rules

- Do not commit PS1 disc images, retail BIOS files, extracted executables,
  generated copyrighted game C, memory cards, or runtime dumps.
- Keep upstream PC builds working while Wii-specific build changes are
  isolated behind a new target/platform layer.
- Validate compile and link success separately from game boot, gameplay and
  real-hardware performance.
- Keep the same Wii DOL target compatible with original Wii and Wii U vWii
  wherever the homebrew APIs permit it.

## Toolchain

The CI smoke test uses the official `devkitpro/devkitppc` Docker image and
the standard `DEVKITPPC/wii_rules` build rules. It uploads `boot.dol` as a
GitHub Actions artifact for download and testing.
