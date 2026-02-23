# Reference v1 (acceptance checklist)

Diese Datei definiert, was für Tablinum als „Referenzimplementierung v1“ gilt. Sie ist absichtlich kurz, normativ (MUST/SHOULD) und prüfbar.  
This file defines what counts as “Reference implementation v1” for Tablinum. It is intentionally short, normative (MUST/SHOULD) and testable.

---

## Deutsch (DE)

## 1. Scope

Fokus ist das E-ARK-lite v1 Packaging inkl. strikter Verifikation.

---

## 2. Normative Anforderungen (MUST)

### 2.1 Strict package schema
Ein Package **MUSS** dem Layout in `docs/PACKAGING.md` folgen.

- Pflichtverzeichnisse/-dateien **MÜSSEN** existieren.
- Zusätzliche Dateien/Verzeichnisse sind **nicht** erlaubt.

### 2.2 Deterministic outputs
`tablinum package` **MUSS** deterministische Package-Inhalte erzeugen:

- Manifest-Zeilenreihenfolge **MUSS** stabil sein und verifiziert werden.
- Relative Pfade **MÜSSEN** kanonisch sein (Forward Slashes).
- Metadata-Textdateien **MÜSSEN** LF-only sein.

### 2.3 Stable exit codes
Die CLI **MUSS** diese stabilen Exitcodes für

- `verify-package`
- `ingest-package`

verwenden:

- `0` OK
- `2` usage
- `3` not found
- `4` I/O error
- `5` integrity failure
- `6` schema failure

### 2.4 Package verification command
`tablinum verify-package PKGDIR` **MUSS**:

- die Struktur validieren
- `package.ini` strikt prüfen (schema/layout, LF-only)
- `package.ini` Pflichtkeys für `schema_version=1` erzwingen: `schema_version`, `kind`, `jobid`, `created_utc`, `tool_version`
- optional `events_source=job|legacy` akzeptieren (wenn vorhanden: strikt), als Herkunftsangabe für `metadata/events.log`
- Manifestformat und -reihenfolge validieren
- Hashes für alle Manifest-Referenzen prüfen
- LF-only für metadata-Textdateien erzwingen

### 2.5 Package ingest command
`tablinum ingest-package PKGDIR --config tablinum.ini` **MUSS**:

- `verify-package` als harte Vorbedingung behandeln
- Payload in CAS importieren
- den durable Record erstellen
- ein Repo-Event zum Import anhängen
- fehlschlagen, wenn für dieselbe `jobid` bereits ein Record existiert

### 2.6 Ops audit verification command
`tablinum verify-audit --config tablinum.ini` **MUSS**:

- LF-only und striktes Zeilenformat von `<repo_root>/audit/ops.log` prüfen
- Hash-Ketten-Kontinuität (`prev` Feld) prüfen
- die Hash-Regel `sha256(prev + "\n" + canonical)` für jeden Eintrag prüfen
- stabile Exitcodes liefern: `0 ok / 3 missing / 4 io / 5 integrity`

### 2.7 Coding safety (C89/OpenBSD/fail-fast)
Core-Code **MUSS** `docs/CODING.md` folgen, insbesondere:

- keine direkte printf-family (`printf/sprintf/fprintf/vfprintf/vsprintf`) im Core-Code (nur innerhalb `core/safe.h`)
- boolsche Helper **MÜSSEN** über `_ok`-Namen (oder `_ok`-Aliase) genutzt werden
- compile-time Enforcement via `TBL_STRICT_NAMES` und `TBL_FORBID_STDIO_FORMAT`

---

## 3. SHOULD-Anforderungen (empfohlen)

- `package.ini` **SOLLTE** `tool_commit` enthalten (Build-Metadaten / VCS-Revision), wenn verfügbar.
- Golden-Path E2E **SOLLTE** durch Tests abgedeckt sein:
  - ingest → package → verify-package → export (Payload-Hash identisch)

---

## 4. Tests (MUST)

Das Test-Harness **MUSS** mindestens enthalten:

- `package_test` (Package-Erzeugung + verify-package)
- `ingest_package_test` (Roundtrip-Import)
- `audit_verify_test` (Ops-Audit Hash-Chain Verify + Tamper-Detection)
- `docs_bilingual_test` (Docs bilingual + LF-only sanity check)

---

## 5. Nicht-Ziele (v1)

- OCR, Volltextsuche, Multi-File-Payloads, DB-Index, Web-UI

(Diese Themen kommen in späteren Meilensteinen.)

---

## Audit (Reference v1)

Siehe `docs/AUDIT.md` für die normative Trennung von Job Events vs. Ops Audit und das hash-chained Ops-Log.

---

## English (EN)

## 1. Scope

Focus is E-ARK-lite v1 packaging including strict verification.

---

## 2. Normative Anforderungen (MUST)

### 2.1 Strict package schema
A package MUST follow the layout described in `docs/PACKAGING.md`.

- Required directories/files MUST exist.
- No extra files/directories are allowed.

### 2.2 Deterministic outputs
`tablinum package` MUST generate deterministic package content:

- Manifest line order MUST be stable and verified.
- Relative paths MUST be canonical (forward slashes).
- Metadata text files MUST be LF-only.

### 2.3 Stable exit codes
The CLI MUST use these stable exit codes for:

- `verify-package`
- `ingest-package`

Exit codes:

- `0` OK
- `2` usage
- `3` not found
- `4` I/O error
- `5` integrity failure
- `6` schema failure

### 2.4 Package verification command
`tablinum verify-package PKGDIR` MUST:

- validate structure
- validate `package.ini` strict schema
- enforce `package.ini` required keys for `schema_version=1`: `schema_version`, `kind`, `jobid`, `created_utc`, `tool_version`
- accept optional `events_source=job|legacy` (if present, strict), describing the origin of `metadata/events.log`
- validate manifest format and order
- verify hashes for all files referenced by the manifest
- enforce LF-only for metadata text files

### 2.5 Package ingest command
`tablinum ingest-package PKGDIR --config tablinum.ini` MUST:

- treat `verify-package` as a hard precondition
- import payload into CAS
- create the durable record
- append a repository event describing the import
- fail if a record for the same `jobid` already exists

### 2.6 Ops audit verification command
`tablinum verify-audit --config tablinum.ini` MUST:

- verify LF-only and strict per-line format of `<repo_root>/audit/ops.log`
- verify hash-chain continuity (`prev` field)
- verify the hash rule `sha256(prev + "\n" + canonical)` for every entry
- return stable exit codes: `0 ok / 3 missing / 4 io / 5 integrity`

### 2.7 Coding safety (C89/OpenBSD/fail-fast)
Core code MUST follow `docs/CODING.md`, in particular:

- no direct `printf()`/`sprintf()`/`fprintf()`/`vfprintf()`/`vsprintf()` in core code (printf-family only inside `core/safe.h`)
- boolean helpers MUST be used via `_ok` names (or `_ok` aliases)
- compile-time enforcement via `TBL_STRICT_NAMES` and `TBL_FORBID_STDIO_FORMAT`

---

## 3. SHOULD-Anforderungen (empfohlen)

- `package.ini` SHOULD contain `tool_commit` (build metadata / VCS revision) when available.
- Golden-path E2E SHOULD be covered by tests:
  - ingest → package → verify-package → export (payload hash equality)

---

## 4. Tests (MUST)

The test harness MUST include at least:

- `package_test` (package generation + verify-package)
- `ingest_package_test` (roundtrip import)
- `audit_verify_test` (ops audit hash-chain verify + tamper detection)
- `docs_bilingual_test` (docs bilingual + LF-only sanity check)

---

## 5. Nicht-Ziele (v1)

- OCR, full-text search, multi-file payloads, database index, web UI

(These can be addressed in later milestones.)

---

## Audit (Reference v1)

See `docs/AUDIT.md` for the normative separation of Job Events vs. Ops Audit and the hash-chained ops log.
