# Packaging / Verpackung (E-ARK-lite v1)

Tablinum erzeugt deterministische, streng validierbare *SIP/AIP*-Pakete (OAIS-light, E-ARK-inspiriert). Fokus: langlebige Archivierung ohne externe Abhängigkeiten, C89-first, fail-fast.  
Tablinum creates deterministic, strictly verifiable *SIP/AIP* packages (OAIS-light, inspired by E-ARK). Focus: long-term preservation without external dependencies, C89-first, fail-fast.

---

## Deutsch (DE)

## Paketlayout (MUST)

Ein Package-Verzeichnis (`PKGDIR`) hat folgende feste Struktur:

```
PKGDIR/
  metadata/
    record.ini
    package.ini
    events.log
    manifest-sha256.txt
  representations/
    rep0/
      data/
        <payload>
```

- Genau diese Verzeichnisse/Dateien **MÜSSEN** existieren.
- Zusätzliche Dateien oder Verzeichnisse sind **nicht** erlaubt (strict schema).

---

## metadata/package.ini (striktes Schema)

### Pflichtschlüssel (MUST)

`package.ini` **MUSS** genau die folgenden Pflichtschlüssel enthalten (zusätzlich zu optionalen Schlüsseln unten):

- `schema_version=1`
- `kind=sip|aip`
- `jobid=<jobid>`
- `created_utc=<unix_epoch_seconds>`
- `tool_version=<tablinum_version>`

### Optionale Schlüssel (MAY)

- `events_source=job|legacy` (optional; wenn vorhanden, strikt geprüft)

`events_source` beschreibt die Herkunft von `metadata/events.log`:
- `job` = per-job Stream `repo/jobs/<jobid>/events.log`
- `legacy` = aus `repo/events.log` gefiltert

- `tool_commit=<revision_or_buildmeta>`

### Regeln (MUST)

- Unbekannte Keys sind **nicht** erlaubt.
- UTF‑8 ist erlaubt; die Datei muss **LF-only** sein (keine CRLF).
- Werte werden als Strings behandelt; `schema_version` muss wörtlich `1` sein.

---

## metadata/record.ini

`record.ini` ist der durable Record (Repository-Quelle), als Copy ins Paket. Er muss konsistent zum Payload sein (insbesondere `payload=` und `sha256=`).

Aktuelle Record-Keys (informativ):
- `status` (für Packaging erwartet: `ok`)
- `job`
- `payload` (Dateiname unter `representations/rep0/data/`)
- `sha256` (64 hex)
- `bytes`
- `stored_at` (unix epoch)
- `reason` (optional)

`record.ini` muss **LF-only** sein.

---

## metadata/events.log

Exportfähiges Job-Events-Log für diesen Job.

Source selection (MUST):
1) prefer: `<repo_root>/jobs/<jobid>/events.log`
2) Fallback: `<repo_root>/events.log` gefiltert nach `job=<jobid>` (Legacy)

- Muss **LF-only** sein (CR ist nicht erlaubt).

---

## metadata/manifest-sha256.txt

Striktes, deterministisches Fixity-Manifest (sha256sum-kompatibel):

- Genau **4 Zeilen** (nicht mehr, nicht weniger).
- Jede Zeile hat das Format:

```
<64-hex-sha256><two spaces><relative/path/with/forward/slashes>\n
```

- Pfade müssen sichere relative Pfade sein (kein `..`, nicht absolut, keine Backslashes).
- Reihenfolge muss exakt sein:

1. `representations/rep0/data/<payload>`
2. `metadata/record.ini`
3. `metadata/package.ini`
4. `metadata/events.log`

`manifest-sha256.txt` selbst muss **LF-only** sein.

---

## Verifikation (verify-package)

### `tablinum verify-package PKGDIR`

Führt eine strikte Verifikation aus (fail-fast):

- Strukturprüfung (dirs/files vorhanden, keine Extras)
- `package.ini` strict schema (Keys/Values, LF-only)
- `record.ini` parsing + Payload-Dateiname konsistent
- Manifest-Parsing (Format, Pfad-Sicherheit, Reihenfolge)
- Fixity (alle referenzierten Dateien neu hashen und vergleichen)
- LF-only Enforcement für alle metadata-Textdateien (inkl. Manifest)

### Exit codes (stable)

- `0` OK
- `2` usage
- `3` not found
- `4` I/O error
- `5` integrity failure
- `6` schema failure

---

## Roundtrip-Import (ingest-package)

### `tablinum ingest-package PKGDIR --config tablinum.ini`

- Läuft dieselbe strikte Verifikation als Vorbedingung.
- Importiert den Payload in CAS, schreibt den durable Record und hängt ein Repo-Event an.
- Schlägt fehl, wenn ein Record für dieselbe `jobid` bereits existiert.

---

## Determinismus-Garantien (MUST/SHOULD)

Tablinum zielt auf deterministische Outputs:

- fixes Pfadlayout und Naming
- Manifest-Reihenfolge ist fix und wird verifiziert
- alle metadata-Textdateien sind LF-only
- relative Pfade verwenden Forward Slashes

Payload-Bytes bleiben unverändert.

---

## Event-Quelle (Reference v1)

- Prefer: `repo/jobs/<jobid>/events.log` (exportfähige Job Events)
- Fallback: `repo/events.log` gefiltert nach `job=<jobid>` (Legacy)

Packaging darf **nicht** an die Job Events anhängen.

---

## English (EN)

## Paketlayout (MUST)

A package directory (`PKGDIR`) has this fixed structure:

```
PKGDIR/
  metadata/
    record.ini
    package.ini
    events.log
    manifest-sha256.txt
  representations/
    rep0/
      data/
        <payload>
```

- Exactly these directories/files MUST exist.
- No extra files or directories are allowed (strict schema).

---

## metadata/package.ini (striktes Schema)

### Pflichtschlüssel (MUST)

`package.ini` MUST contain exactly the following required keys (plus the optional keys below):

- `schema_version=1`
- `kind=sip|aip`
- `jobid=<jobid>`
- `created_utc=<unix_epoch_seconds>`
- `tool_version=<tablinum_version>`

### Optionale Schlüssel (MAY)

- `events_source=job|legacy` (optional; strict if present)

`events_source` specifies where `metadata/events.log` comes from:
- `job` = per-job stream `repo/jobs/<jobid>/events.log`
- `legacy` = filtered from `repo/events.log`

- `tool_commit=<revision_or_buildmeta>`

### Regeln (MUST)

- Unknown keys are NOT allowed.
- UTF-8 is allowed; the file MUST be **LF-only** (no CRLF).
- Values are treated as strings; `schema_version` MUST be the literal `1`.

---

## metadata/record.ini

`record.ini` is the durable record (repository source), copied into the package. It must be consistent with the payload (especially `payload=` and `sha256=`).

Tablinum’s current record keys (informative):
- `status` (expected `ok` for packaging)
- `job`
- `payload` (filename under `representations/rep0/data/`)
- `sha256` (64 hex)
- `bytes`
- `stored_at` (unix epoch)
- `reason` (optional)

`record.ini` MUST be **LF-only**.

---

## metadata/events.log

Exportable job events log for this job.

Source selection (MUST):
1) prefer: `<repo_root>/jobs/<jobid>/events.log`
2) fallback: `<repo_root>/events.log` filtered by `job=<jobid>` (legacy)

- MUST be **LF-only** (CR not allowed).

---

## metadata/manifest-sha256.txt

A strict, deterministic fixity manifest (sha256sum-compatible):

- Exactly **4 lines** (no more, no less).
- Each line format MUST be:

```
<64-hex-sha256><two spaces><relative/path/with/forward/slashes>\n
```

- Paths MUST be safe relative paths (no `..`, no absolute paths, no backslashes).
- Line order MUST be exactly:

1. `representations/rep0/data/<payload>`
2. `metadata/record.ini`
3. `metadata/package.ini`
4. `metadata/events.log`

`manifest-sha256.txt` itself MUST be **LF-only**.

---

## Verification

### `tablinum verify-package PKGDIR`

Performs a strict verification (fail-fast):

- Structure check (required dirs/files, no extras)
- `package.ini` strict schema (keys, values, LF-only)
- `record.ini` parsing + payload filename consistency
- Manifest parsing (format, safe paths, order)
- Fixity check (re-hash all referenced files and compare)
- LF-only enforcement for all metadata text files (including manifest)

### Exit codes (stable)

- `0` OK
- `2` usage
- `3` not found
- `4` I/O error
- `5` integrity failure
- `6` schema failure

---

## Roundtrip import

### `tablinum ingest-package PKGDIR --config tablinum.ini`

- Runs the same strict verification logic as a precondition.
- Imports the payload into CAS, writes the durable record, and appends a repository event.
- Fails if a record for the same `jobid` already exists.

---

## Determinism guarantees

Tablinum aims for deterministic outputs:

- Fixed path layout and naming
- Manifest order is fixed and verified
- All metadata text files are written LF-only
- Relative paths use forward slashes

Payload bytes are preserved verbatim.

---

## Event-Quelle (Reference v1)

- Prefer: `repo/jobs/<jobid>/events.log` (exportable job events)
- Fallback: `repo/events.log` filtered by `job=<jobid>` (legacy)

Packaging MUST NOT append to job events.
