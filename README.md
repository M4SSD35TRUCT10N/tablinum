# Tablinum

Tablinum ist ein strikt in C89 geschriebenes, portables *Document Hub* (paperless‑ähnlich) als **Single‑Binary**.  
Ziel: langlebige, in Jahrzehnten noch kompilier- und lesbare Dokumentenablage (Windows, Linux/RPi, 9front) mit **fail‑fast** und sicherem C‑Stil. Build/Test laufen über **tack**.

Tablinum is a strict C89, portable *document hub* (paperless‑style) as a **single binary**.  
Goal: a durable document store that can still be compiled and read decades from now (Windows, Linux/RPi, 9front), using a **fail‑fast** secure C style. Build/tests are driven by **tack**.

## Schnellstart / Quick start

> **DE:** Aktuell ist der Kern‑Workflow **ingest / verify / export** implementiert. Andere Rollen sind Platzhalter und schlagen derzeit fail‑fast mit „not implemented yet“ fehl.  
> **EN:** At this stage, the implemented core workflow is **ingest / verify / export**. Other roles are placeholders and currently fail-fast with “not implemented yet”.

```sh
# show CLI help
tablinum --help

# ingest jobs (polling)
tablinum --config tablinum.ini --role ingest

# verify a stored job by recomputing SHA-256 from CAS
tablinum verify JOBID

# export payload + record (+ manifest) to a directory
tablinum export JOBID ./export_dir

# package as E-ARK inspired folder structure (OAIS-light)
tablinum package JOBID ./aip_dir --format aip

# verify a package (strict schema + fixity)
tablinum verify-package ./aip_dir

# import a package back into a repo
tablinum ingest-package ./aip_dir --config tablinum.ini

# verify ops audit hash-chain
tablinum verify-audit --config tablinum.ini
```

---

## Deutsch (DE)

### Bedeutung des Namens

> Das tablinum ist das repräsentative Arbeitszimmer eines römischen Hauses, das in der typischen Bauweise des Atriumhauses häufig an der hinteren Schmalseite des atrium dem Eingang direkt gegenüber liegt. Es ist nicht zwangsläufig durch eine Wand mit Tür vom atrium getrennt, sondern unterstreicht seinen repräsentativen Charakter dadurch, dass es sich mit der ganzen Seite des Raumes zum atrium hin öffnet, wobei die Öffnung bei Bedarf durch einen Vorhang geschlossen werden kann. An der gegenüberliegenden Seite kann sich das Zimmer auch zum Garten (hortus) hin öffnen.  
>  
> Dem tablinum kommt aufgrund seiner Lage im Haus und seiner meist aufwändigen architektonischen Ausgestaltung nicht die Rolle eines Zimmers zum zurückgezogenen, ungestörten Arbeiten zu. Es dient stattdessen dem Empfang wichtiger Gäste zu politischen oder geschäftlichen Besprechungen, die nicht mit einem Essen verbunden werden.

Im übertragenen Sinn ist Tablinum genau das: ein „repräsentativer Knotenpunkt“ zwischen Eingang (Ingest) und Garten (Archiv/Repository) – offen, aber bei Bedarf klar getrennt und kontrolliert.

### Status

Frühe Bootstrap‑Phase (aber schon funktionsfähig für den Kern‑Workflow).

Referenz v1 (Packaging-Strictness) ist in `docs/REFERENCE.md` definiert.

- strikte C89‑Core‑Utilities (`str`, `safe`, `path`, …)
- robuste Spool/Claim‑Queue (Job‑Verzeichnisse)
- Ingest‑Role: `inbox` → `claim` → `out`/`fail`
- Fixity: SHA‑256
- CAS: Payload wird in `repo/sha256/<ab>/<rest>` abgelegt
- Durable Records: `repo/records/<jobid>.ini`
- Audit‑Trail: append‑only `repo/events.log`
- Ops-Audit: tamper-evident `repo/audit/ops.log` (hash-chain)
- Verify: `tablinum verify <jobid>` (recompute + compare)
- Export: `tablinum export <jobid> <dir>` (DIP‑light: `payload.bin`, `record.ini`, `manifest-sha256.txt`)
- Package: `tablinum package <jobid> <dir> [--format aip|sip]` (E-ARK‑inspiriert: `metadata/` + `representations/`)
- Verify-Package: `tablinum verify-package <pkgdir>` (strict schema + fixity)
- Ingest-Package: `tablinum ingest-package <pkgdir>` (Roundtrip-Import)
- Verify-Audit: `tablinum verify-audit` (prüft Hash-Kette im Ops-Audit)

### Ziele

- eine Binary, mehrere Rollen (All‑in‑One oder verteilt)
- Portabilität zuerst (Windows, Linux/RPi, 9front)
- minimale Abhängigkeiten, langfristige Lesbarkeit
- kleine Angriffsfläche, sicherer C‑Stil, fail‑fast

### Repository Layout

- `include/` – öffentliche Umbrella‑Header
- `src/core/` – plattformneutrale Module (Parser, Queue‑Logik, Helpers)
- `src/os/` – dünne OS‑Abstraktion (Filesystem, Time, …)
- `tests/` – tack‑Tests (`tests/**/*_test.c`)

### Konfiguration

Eine minimale Beispiel‑Konfiguration:
- `tablinum.ini.example`

Lokale Config anlegen:

```bat
copy tablinum.ini.example tablinum.ini
```

### Build

```bat
tack --config .\tack.ini build debug
tack --config .\tack.ini build release
```

### Tests

```bat
tack test debug -j 8
tack test release -j 8
```

### Version

```bat
tablinum --version
```

### Starten

#### Ingest (Spool‑Queue)

```bat
tablinum --config tablinum.ini --role ingest
```

#### Verify (Fixity)

```bat
tablinum verify JOBID --config tablinum.ini
```

#### Export (Payload + Record)

```bat
tablinum export JOBID OUTDIR --config tablinum.ini
```

#### Package (E-ARK inspired, OAIS-light)

```bat
tablinum package JOBID OUTDIR --format aip --config tablinum.ini
```

#### Verify-Package (strict schema + fixity)

```bat
tablinum verify-package OUTDIR
```

#### Ingest-Package (Roundtrip-Import)

```bat
tablinum ingest-package OUTDIR --config tablinum.ini
```

Jobs in die Inbox legen:

```bat
mkdir spool\inbox\job1
mkdir spool\inbox\job2

echo hello > spool\inbox\job1\payload.bin
rem optional metadata (INI):
echo [job] > spool\inbox\job1\job.meta

echo fail  > spool\inbox\job2\payload.bin
```

Verhalten:
- `job1/` wird geclaimt, verarbeitet und im Repository als CAS + `records/<jobid>.ini` + `events.log` abgelegt; danach nach `spool/out/` verschoben
- `job2/` wird nach `spool/fail/` verschoben

Spool‑Layout:
- `spool/inbox/`  – neue Jobs
- `spool/claim/`  – geclaimt (in Arbeit)
- `spool/out/`    – erfolgreich verarbeitet
- `spool/fail/`   – fehlgeschlagen

---

## English (EN)

### Name meaning

> The *tablinum* is the representative office in a Roman atrium house, typically opposite the entrance.  
> It often opens fully towards the atrium (sometimes closed by a curtain) and may also open towards the garden (*hortus*).  
> Due to its prominent position and elaborate design it was not meant for secluded work, but for receiving important guests for political or business meetings.

In that sense, Tablinum is a controlled “hub” between the entrance (ingest) and the garden (archive/repository) — open by design, but clearly separated when needed.

### Status

Early bootstrap phase (but the core workflow already works).

Reference v1 (packaging strictness) is defined in `docs/REFERENCE.md`.

- strict C89 core utilities (`str`, `safe`, `path`, ...)
- robust spool/claim queue (job directories)
- ingest role: `inbox` → `claim` → `out`/`fail`
- fixity: SHA‑256
- CAS: payload stored as `repo/sha256/<ab>/<rest>`
- durable records: `repo/records/<jobid>.ini`
- audit trail: append‑only `repo/events.log`
- ops audit: tamper-evident `repo/audit/ops.log` (hash-chain)
- verify: `tablinum verify <jobid>` (recompute + compare)
- export: `tablinum export <jobid> <dir>` (DIP‑light: `payload.bin`, `record.ini`, `manifest-sha256.txt`)
- package: `tablinum package <jobid> <dir> [--format aip|sip]` (E-ARK inspired: `metadata/` + `representations/`)
- verify-package: `tablinum verify-package <pkgdir>` (strict schema + fixity)
- ingest-package: `tablinum ingest-package <pkgdir>` (roundtrip import)
- verify-audit: `tablinum verify-audit` (verifies ops audit hash-chain)

### Goals

- one binary, multiple roles (all‑in‑one or distributed)
- portability first (Windows, Linux/RPi, 9front)
- minimal dependencies, long‑term readability
- small attack surface, secure C style, fail‑fast

### Repository Layout

- `include/` – public umbrella headers
- `src/core/` – platform‑neutral modules (parsers, queue logic, helpers)
- `src/os/` – thin OS abstraction layer (filesystem, time, …)
- `tests/` – tack tests (`tests/**/*_test.c`)

### Configuration

A minimal example config is provided:
- `tablinum.ini.example`

Create your local config:

```bat
copy tablinum.ini.example tablinum.ini
```

### Build

```bat
tack --config .\tack.ini build debug
tack --config .\tack.ini build release
```

### Tests

```bat
tack test debug -j 8
tack test release -j 8
```

### Version

```bat
tablinum --version
```

### Run

#### Ingest (spool queue)

```bat
tablinum --config tablinum.ini --role ingest
```

#### Verify (fixity)

```bat
tablinum verify JOBID --config tablinum.ini
```

#### Export (payload + record)

```bat
tablinum export JOBID OUTDIR --config tablinum.ini
```

#### Package (E-ARK inspired, OAIS-light)

```bat
tablinum package JOBID OUTDIR --format aip --config tablinum.ini
```

#### Verify-Package (strict schema + fixity)

```bat
tablinum verify-package OUTDIR
```

#### Ingest-Package (roundtrip import)

```bat
tablinum ingest-package OUTDIR --config tablinum.ini
```

Drop files into the inbox:

```bat
mkdir spool\inbox\job1
mkdir spool\inbox\job2

echo hello > spool\inbox\job1\payload.bin
rem optional metadata (INI):
echo [job] > spool\inbox\job1\job.meta

echo fail  > spool\inbox\job2\payload.bin
```

Behavior:
- `job1/` will be claimed, processed and recorded as CAS + `records/<jobid>.ini` + `events.log`; then moved to `spool/out/`
- `job2/` will be moved to `spool/fail/`

Spool layout:
- `spool/inbox/`  – new jobs
- `spool/claim/`  – claimed (in‑progress)
- `spool/out/`    – processed successfully
- `spool/fail/`   – failed jobs

---

## License

MIT
