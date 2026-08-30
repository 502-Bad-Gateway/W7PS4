# W7PS4 publication identity

This repository is the dedicated publication home for the shadPS4 Graphics Lab project.

Its initial source snapshot was generated from the locally verified foundation tree whose source
identities are recorded in `BASELINE-IDENTITY.txt`. The exact historical Git import was attempted,
and both Graphics Lab commit hashes were reconstructed successfully, but GitHub rejected the branch
push because the historical commits contain Actions workflow files and the Actions token cannot
import those files through Git push.

To keep the new repository clean and prevent unrelated or duplicate builds:

- the complete emulator, launcher, Graphics Lab modules, profiles, tests and documentation are
  included in the snapshot;
- the historical workflow directory is excluded from the snapshot publication;
- only `.github/workflows/graphics-lab-plugins.yml` is recreated through GitHub's authorized
  contents API;
- `502-Bad-Gateway/shadPS4_test_7` remains untouched and is only the recorded Build 11 source.

This publication choice changes the new repository's root commit identity, not the imported source
content or the recorded Build 11 baseline. The original foundation history is preserved separately
in `shadPS4_GraphicsLab_history_78f73fd1.bundle`.

