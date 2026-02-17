# Releasing / Version discipline

**DE:** Kurz und strikt: Tablinum soll auch in Jahren noch reproduzierbar baubar sein. Daher bleibt die Versionierung bewusst simpel und „handgemacht“.

**EN:** Short and strict: Tablinum aims to remain reproducible years from now. Therefore versioning is intentionally simple and manual.

---

## Policy

### SemVer-ish

- `MAJOR`: breaking changes (for now this likely stays `0` for a while)
- `MINOR`: bigger milestones / features
- `PATCH`: incremental features + bugfixes

Source of truth: `src/core/version.h`.

### Dev vs release

- **Development builds** keep a suffix, default `-dev` (`TBL_VERSION_SUFFIX`).
- **Release builds** set the suffix to empty: `""`.

Optional: add SemVer build metadata via `TBL_BUILD_META` (e.g. `+g<hash>`).

---

## Suggested workflow

### 1) Prepare a release

1. Update numbers in `src/core/version.h` (`MAJOR/MINOR/PATCH`).
2. Set `TBL_VERSION_SUFFIX` to `""` for release.
3. Run a clean build + tests (debug + release).

### 2) Commit discipline

Recommended commit message:

- `tablinum: release vX.Y.Z`

Then tag the release in your VCS (Git/Fossil).

### 3) Back to dev

After tagging:

1. Set `TBL_VERSION_SUFFIX` back to `-dev`.
2. (Optional) bump `PATCH` immediately to the next development version.

---

## Build metadata (optional)

If you want `tablinum --version` to include a short revision id, define:

- `TBL_BUILD_META` as a string that already contains the leading `+` (e.g. `"+gabcdef0"`).

How you pass that depends on your build system. With a C compiler it typically looks like:

```sh
cc ... -DTBL_BUILD_META=\"+gabcdef0\" ...
```
