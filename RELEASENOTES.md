# Release Notes

## v0.2.0 — Reference v1: Strict Packages, Audit-Chain, Safe IO

**Datum / Date:** 2026-02-23  
**Schwerpunkt / Focus:** Feature-Cut “Reference v1” (strict schema) + audit + safety enforcement

---

## Deutsch (DE)

### Neu
- **Strict Package Verifikation:** `tablinum verify-package PKGDIR`  
  Prüft Layout, `package.ini` (strict schema), `record.ini`, Manifest-Format, Pfad-Sicherheit, **LF-only** und Fixity (sha256).
- **Package Ingest:** `tablinum ingest-package PKGDIR --config tablinum.ini`  
  Importiert ein verifiziertes Package ins Repo (CAS + records + events), **verify als Vorbedingung**.
- **Tamper-evident Audit-Log (Hash-Kette):** `repo/audit/ops.log`  
  Append-only Ops-Stream mit Hash-Chaining.
- **Audit-Verifikation:** `tablinum verify-audit --config tablinum.ini` (Alias: `audit-verify`)  
  Prüft Hash-Kette und Kanonisierung (LF-only, Format, prev/hash Konsistenz).
- **Job-Eventstream (exportfähig):** `repo/jobs/<jobid>/events.log`  
  Packaging bevorzugt diesen Stream; Fallback auf Legacy-Filter aus `repo/events.log` bleibt möglich.
- **Klarere CLI-Semantik:** `pkg_dir` als eigener Parameter (PKGDIR getrennt von OUTDIR).

### Safety / Robustheit (OpenBSD-Style)
- **Format-stdio Enforcement:** `TBL_FORBID_STDIO_FORMAT` verhindert direkte Nutzung der printf-Family außerhalb `core/safe.h`.
- **Neue Safe-IO-Wrapper:** `tbl_fputs*_ok`, `tbl_fwrite_all_ok`, `tbl_vfprintf_ok`, `tbl_fflush_ok` und konsequente Nutzung im Core.
- **Return-Semantik entschärft:** `_ok`-Konvention für boolsche Helper zur Vermeidung von Rückgabewert-Missverständnissen.

### Determinismus / Abnahme
- Manifest- und Output-Determinismus verbessert (stabile Reihenfolge, Pfadnormierung, LF-only in Metadaten).
- Exitcodes konsistent gemäß Reference-v1-Logik: `0/2/3/4/5/6`.

### Tests
- Neue Tests: `audit_verify_test`, `package_test`, `ingest_package_test` (Golden Path: package → verify-package → ingest-package → verify).
- Neuer Test: `docs_bilingual_test` (Docs müssen DE/EN enthalten und LF-only sein).
- Testdiagnostik verbessert (rc/err sichtbar), keine Format-stdio Nutzung in Tests.

### Dokumentation
- `PACKAGING.md` / `REFERENCE.md` aktualisiert (strict schema, Layout, Exitcodes, Determinismus).
- Neue/erweiterte normative Docs: `AUDIT.md`, `CODING.md` (Safety-Regeln, Helper, Verbote).
- Docs-Struktur vereinheitlicht: explizite DE/EN-Abschnitte oder `**DE:**`/`**EN:**` Marker.

### Hinweise / Migration
- Repos können optional den neuen Job-Eventstream nutzen (`repo/jobs/<jobid>/events.log`). Legacy `repo/events.log` bleibt kompatibel (Fallback/Filter).
- Builds, die noch direkte printf-Family im Core verwenden, schlagen mit aktivem `TBL_FORBID_STDIO_FORMAT` fehl (beabsichtigt).

---

## English (EN)

### Added
- **Strict package verification:** `tablinum verify-package PKGDIR`  
  Verifies layout, strict `package.ini` schema, `record.ini`, manifest format, path safety, **LF-only**, and fixity (sha256).
- **Package ingest:** `tablinum ingest-package PKGDIR --config tablinum.ini`  
  Imports a verified package into the repo (CAS + records + events), **verify as a precondition**.
- **Tamper-evident audit log (hash chain):** `repo/audit/ops.log`  
  Append-only ops stream with hash chaining.
- **Audit verification:** `tablinum verify-audit --config tablinum.ini` (alias: `audit-verify`)  
  Verifies hash chain and canonicalization (LF-only, format, prev/hash consistency).
- **Job event stream (export-friendly):** `repo/jobs/<jobid>/events.log`  
  Packaging prefers this stream; legacy fallback from `repo/events.log` remains available.
- **Clearer CLI semantics:** dedicated `pkg_dir` parameter (separates PKGDIR from OUTDIR).

### Safety / robustness (OpenBSD-style)
- **Format-stdio enforcement:** `TBL_FORBID_STDIO_FORMAT` blocks direct printf-family usage outside `core/safe.h`.
- **New safe IO wrappers:** `tbl_fputs*_ok`, `tbl_fwrite_all_ok`, `tbl_vfprintf_ok`, `tbl_fflush_ok`, used consistently in core.
- **De-risked return semantics:** `_ok` convention for boolean helpers to avoid ambiguity.

### Determinism / acceptance
- Improved deterministic outputs (stable manifest order, path normalization, LF-only metadata).
- Consistent exit codes per Reference-v1 logic: `0/2/3/4/5/6`.

### Tests
- New tests: `audit_verify_test`, `package_test`, `ingest_package_test` (Golden Path: package → verify-package → ingest-package → verify).
- New test: `docs_bilingual_test` (docs must be DE/EN and LF-only).
- Better diagnostics (rc/err), no format-stdio usage in tests.

### Documentation
- Updated `PACKAGING.md` / `REFERENCE.md` (strict schema, layout, exit codes, determinism).
- New/extended normative docs: `AUDIT.md`, `CODING.md` (safety rules, helpers, forbids).
- Standardized doc structure: explicit DE/EN sections or `**DE:**`/`**EN:**` markers.

### Notes / migration
- Repos may adopt the job event stream (`repo/jobs/<jobid>/events.log`). Legacy `repo/events.log` remains compatible (fallback/filter).
- Builds that still use direct printf-family calls in core will fail when `TBL_FORBID_STDIO_FORMAT` is enabled (intended).
