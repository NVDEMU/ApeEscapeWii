# Browser-only Ape Escape asset preparation

This workflow converts a locally supplied CHD in GitHub Actions, so a local
Terminal is not required.

## Keep the assets private

Do **not** commit a retail game image or BIOS into the public ApeEscapeWii
repository. This workflow intentionally skips in public repositories. Create a
separate **private** GitHub repository for temporary staging, copy this workflow
and `tools/extract_psx_boot_exe.py` there, then upload the legally obtained
CHD to `assets/Ape Escape (USA).chd`. There is no need to upload a BIOS for
this extraction step.

In the private staging repository, use **Actions → Prepare private Ape Escape
disc assets → Run workflow**. After it finishes, download the
`ape-escape-private-disc-prep` artifact. It contains the CUE/BIN disc pair and
the extracted `SCUS_944.23` executable that matches the existing `game.toml`
path.

The artifact expires after one day. Keep the staging repository private even
after deleting files: deleting files from a commit does not remove old blobs
from Git history. Do not copy the artifact into the public repository; put
the extracted files into a local, ignored `apeescape/` directory when you
eventually have a way to transfer the artifact to your development environment.

The workflow validates and converts the submitted image, but it does not
publish the game image or BIOS as release assets. The BIOS is not needed to
extract the disc's boot executable.
