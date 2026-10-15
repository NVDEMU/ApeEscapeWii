# Browser-only Ape Escape asset preparation

This workflow prepares your own PS1 disc image in GitHub Actions, so you do
not need to run Terminal or upload the file to ChatGPT.

## Keep the large disc image outside Git history

Do **not** commit a retail game image or BIOS into the public ApeEscapeWii
repository. The public workflow intentionally skips public repositories. Use
a separate **private** GitHub repository for staging.

1. Create a private repository, for example `ApeEscapeWii-PrivateAssets`.
2. In that private repository, create
   `.github/workflows/prepare-private-assets.yml` by copying the workflow
   from [ApeEscapeWii](https://github.com/NVDEMU/ApeEscapeWii/blob/master/.github/workflows/prepare-private-assets.yml).
   The workflow downloads the public helper script itself, so no other source
   files are needed.
3. Create a release in the **private** repository with tag `disc-input`.
   Publish it (not draft) and attach your `Ape Escape (USA).chd` file as a
   release asset. GitHub release assets can be up to 2 GiB each, so the
   ~111 MB CHD fits; ordinary browser uploads to a repository's Git tree are
   limited to 25 MiB per file.
4. Open **Actions → Prepare private Ape Escape disc assets → Run workflow**.
5. When it succeeds, download the `ape-escape-private-disc-prep` artifact.
   It contains:
   - `Ape Escape (USA).cue`
   - `Ape Escape (USA).bin`
   - `SCUS_944.23` (the executable referenced by `game.toml`)

The workflow validates the CHD, converts it with `chdman`, extracts the data
track, reads `SYSTEM.CNF`, and extracts the boot executable. The output artifact
expires after one day. It does **not** contain or require a retail BIOS.

Keep the staging repository private, and do not transfer the BIN/CUE or
executable into the public repository. A file deleted from a commit may remain
in Git history; a private release avoids committing the large game image in the
first place.
