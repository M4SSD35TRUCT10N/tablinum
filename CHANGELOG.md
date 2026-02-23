# Changelog

## [0.2.0] — 2026-02-23

---

## Deutsch (DE)

### Hinzugefügt
- `verify-package` strikter PKGDIR-Verifier (Schema/Layout/Manifest/Fixity/LF-only)
- `ingest-package` (verify als Vorbedingung; Import in CAS + records + events)
- `verify-audit` / `audit-verify` (Prüfung der Ops-Audit-Hash-Kette)
- Ops-Audit-Stream `repo/audit/ops.log` (tamper-evident)
- Job-Eventstream `repo/jobs/<jobid>/events.log` (Export bevorzugt; Legacy-Fallback)
- Safe-IO-Helper + Format-stdio Enforcement (`TBL_FORBID_STDIO_FORMAT`)
- Tests: `audit_verify_test`, `package_test`, `ingest_package_test`, `docs_bilingual_test`
- Doku: `AUDIT.md`, `CODING.md` + Updates in `PACKAGING.md`/`REFERENCE.md`

### Geändert
- deterministische Packaging-Outputs (Manifest-Reihenfolge, Pfadnormierung, LF-only)
- CLI/Config: dedizierte `pkg_dir` Semantik (PKGDIR vs OUTDIR)
- Docs-Disziplin: DE/EN-Markierung als MUST + Linter-Test

### Behoben
- strikte INI-Callback-Accept-Semantik (0=accept)
- sha256 Hex-Encode-Check-Logik + verbesserte Fehlerweitergabe
- Build-Hygiene (Includes, NUL-Bytes, Exitcode-Mapping konsistent)

---

## English (EN)

### Added
- `verify-package` strict PKGDIR verifier (schema/layout/manifest/fixity/LF-only)
- `ingest-package` (verify precondition; import into CAS + records + events)
- `verify-audit` / `audit-verify` (ops audit hash-chain verification)
- ops audit stream `repo/audit/ops.log` (tamper-evident)
- job event stream `repo/jobs/<jobid>/events.log` (export preferred; legacy fallback)
- safe IO helpers + format-stdio enforcement (`TBL_FORBID_STDIO_FORMAT`)
- tests: `audit_verify_test`, `package_test`, `ingest_package_test`, `docs_bilingual_test`
- docs: `AUDIT.md`, `CODING.md` + updates to `PACKAGING.md`/`REFERENCE.md`

### Changed
- deterministic packaging outputs (manifest order, path normalization, LF-only)
- CLI config adds dedicated `pkg_dir` semantics (PKGDIR vs OUTDIR)
- docs discipline: DE/EN markers as MUST + linter test

### Fixed
- strict INI callback accept semantics (0=accept)
- sha256 hex encode check logic + improved error propagation
- build hygiene fixes (includes, NUL bytes, exitcode mapping consistency)
