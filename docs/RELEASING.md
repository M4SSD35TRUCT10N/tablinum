# Releasing / Version discipline

Kurz und strikt: Tablinum soll auch in Jahren noch reproduzierbar baubar sein. Daher bleibt die Versionierung bewusst simpel und „handgemacht“.  
Short and strict: Tablinum aims to remain reproducible years from now. Therefore versioning is intentionally simple and manual.

---

## Deutsch (DE)

## Policy

### SemVer-ish
- `MAJOR`: Breaking Changes (vorerst bleibt das wahrscheinlich länger `0`)
- `MINOR`: größere Meilensteine / Features (Feature-Cuts)
- `PATCH`: inkrementelle Features + Bugfixes

**Source of truth:** `src/core/version.h`.

### Dev vs release
- **Development Builds** behalten ein Suffix, standardmäßig `-dev` (`TBL_VERSION_SUFFIX`).
- **Release Builds** setzen das Suffix auf leer: `""`.

Optional: SemVer build metadata über `TBL_BUILD_META` (z.B. `+g<hash>`).

---

## Empfohlener Ablauf

### 1) Release vorbereiten
1. Zahlen in `src/core/version.h` aktualisieren (`MAJOR/MINOR/PATCH`).
2. `TBL_VERSION_SUFFIX` für Release auf `""` setzen.
3. Clean Build + Tests laufen lassen (debug + release).

### 2) Commit-Disziplin
Empfohlene Commit Message:
- `tablinum: release vX.Y.Z`

---

## English (EN)

## Policy

### SemVer-ish
- `MAJOR`: breaking changes (for now this likely stays `0` for a while)
- `MINOR`: bigger milestones / features (feature cuts)
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
