# Tablinum Audit Logging (Reference v1)

---

## Deutsch (DE)

> Ziel: reproduzierbare, effiziente und ISO-/Audit-taugliche Nachvollziehbarkeit ohne Side-Effects beim Packaging.

### 1. Begriffe
- **Repo (System of Record):** Persistenter Speicher von Tablinum (CAS + Records + Logs).
- **Package (Delivery Artifact):** PKGDIR im E-ARK-lite v1 Layout (SIP/AIP), abgeleitet aus dem Repo.
- **Job Events (exportfähig):** Ereignisstrom, der in Packages als `metadata/events.log` mitgegeben werden darf.
- **Ops Audit (betriebsintern):** Operative Telemetrie/Fehler/Tool-Läufe, die *nicht* in Packages einfließen dürfen.

### 2. Leitprinzipien (normativ)

#### MUST
- **Append-only:** Logs werden nie überschrieben oder „aufgeräumt“ (kein Truncate).
- **Kanonisches Format:** UTF‑8 (oder ASCII), LF-only (`\n`), keine CR (`\r`).
- **Side-Effect-Freiheit:** `tablinum package` darf keine Repo-Inputs verändern, die wieder im Package landen.
- **Trennung:** Ops Audit und Job Events sind getrennte Ströme.

#### SHOULD
- **Manipulationsnachweis:** Ops Audit nutzt Hash-Kette (tamper-evident).
- **Determinismus:** Package-Ausgaben sind bei identischem Input byte-identisch (Manifest-Reihenfolge, Pfadnormierung, LF-only).

### 3. Verzeichnislayout (empfohlen)

```
<repo_root>/
  records/<jobid>.ini              # durable record
  cas/<sha256...>                  # content-addressed storage

  jobs/<jobid>/events.log          # Job Events (exportfähig)
  audit/ops.log                    # Ops Audit (tamper-evident, hash-chained)

  events.log                       # LEGACY: kombinierter Event-Stream (optional, Übergang)
```

### 4. Job Events (exportfähig)

**Zweck:** Fachliche/technische Ereignisse, die zur Provenienz des Archivobjekts gehören und in Packages als `metadata/events.log` erscheinen dürfen.

#### MUST
- Speicherung pro Job: `<repo_root>/jobs/<jobid>/events.log`
- Zeilenformat (kanonisch, feldweise):
  - `ts=<unix-utc> event=<name> job=<jobid> status=<...> sha256=<...> reason=<...>`
- LF-only

#### SHOULD
- Nur Ereignisse, die für das Archivobjekt relevant sind (ingest/verify/export usw.).
- Keine „Run-Telemetrie“ von `package` (siehe Ops Audit).

### 5. Ops Audit (betriebsintern, tamper-evident)

**Zweck:** Operative Nachvollziehbarkeit und ISO-/Audit-Fragen („wer/was/wann/warum“), ohne Packages zu beeinflussen.

#### Hash-Kette (MUST für Reference v1 Audit-Mode)
Jeder Eintrag enthält:
- `prev=<64hex>` Hash des vorherigen Eintrags (für den ersten Eintrag: 64×`0`)
- `hash=<64hex>` Hash des aktuellen Eintrags

**Berechnung (kanonisch):**
- `canonical = "ts=... event=... job=... status=... sha256=... reason=..."` (ohne `prev=` und `hash=`)
- `hash = sha256(prev + "\n" + canonical)`

**Logzeile:**
- `prev=<prev> hash=<hash> <canonical>\n`

#### MUST
- Pfad: `<repo_root>/audit/ops.log`
- Append-only, LF-only, deterministische Feldreihenfolge wie oben.

#### SHOULD
- Periodische Checkpoints (z.B. täglich) über den letzten `hash=` Wert.
- Später: Signatur (HSM/KMS), sobald verfügbar.

### 6. Beziehung zum Packaging

#### MUST
- `tablinum package` schreibt **nicht** in Job Events.
- `tablinum package` kann (optional) in Ops Audit schreiben, **aber** Ops Audit ist kein Input für das Package.

#### Package Events Source
Beim Erzeugen von `metadata/events.log` gilt:
1) **Prefer:** `<repo_root>/jobs/<jobid>/events.log`
2) **Fallback:** `<repo_root>/events.log` gefiltert nach `job=<jobid>` (Legacy-Modus)

Damit bleibt Reference v1 kompatibel, aber unterstützt die saubere Trennung.

### 7. Audit-Chain Verifikation (CLI)

**Command:**
```sh
tablinum verify-audit --config tablinum.ini
```

**Prüft (fail-fast):**
- LF-only (kein `\r`)
- Format pro Zeile: `prev=<64hex> hash=<64hex> <canonical>`
- `prev`-Verkettung über alle Zeilen
- Hash-Regel: `sha256(prev + "\n" + canonical)`

**Exitcodes:**
- `0` ok
- `3` audit log fehlt
- `4` I/O Fehler
- `5` Integrität/Format gebrochen

---

## English (EN)

> Goal: reproducible, efficient, ISO/audit-friendly traceability without packaging side effects.

### 1. Terms
- **Repo (system of record):** persistent Tablinum store (CAS + records + logs).
- **Package (delivery artifact):** PKGDIR in E-ARK-lite v1 layout (SIP/AIP), derived from the repo.
- **Job events (exportable):** event stream allowed to be included as `metadata/events.log`.
- **Ops audit (internal):** operational telemetry/errors/tool runs that must not influence packages.

### 2. Principles (normative)

#### MUST
- **Append-only:** no truncation/overwrite.
- **Canonical format:** UTF‑8/ASCII, LF-only (`\n`), no CR (`\r`).
- **No packaging side effects:** `tablinum package` must not mutate repo inputs that appear in packages.
- **Separation:** ops audit and job events are separate streams.

#### SHOULD
- **Tamper evidence:** ops audit uses a hash chain.
- **Determinism:** identical input yields byte-identical packages (manifest order, path canonicalization, LF-only).

### 3. Layout (recommended)

```
<repo_root>/
  records/<jobid>.ini
  cas/<sha256...>

  jobs/<jobid>/events.log
  audit/ops.log

  events.log   # legacy (optional)
```

### 4. Job events (exportable)

**Purpose:** provenance-relevant events that may be shipped as `metadata/events.log` inside packages.

#### MUST
- Path: `<repo_root>/jobs/<jobid>/events.log`
- Canonical line format (field-wise):
  - `ts=<unix-utc> event=<name> job=<jobid> status=<...> sha256=<...> reason=<...>`
- LF-only

#### SHOULD
- Only events relevant to the archived object (ingest/verify/export, etc.).
- No packaging run telemetry here (see ops audit).

### 5. Ops audit (internal, tamper-evident)

**Purpose:** operational traceability for ISO/audit questions (“who/what/when/why”) without affecting packages.

#### Hash chain (MUST for Reference v1 audit mode)
Each entry contains:
- `prev=<64hex>` hash of the previous entry (first entry: 64×`0`)
- `hash=<64hex>` hash of the current entry

**Canonical hash rule:**
- `canonical = "ts=... event=... job=... status=... sha256=... reason=..."` (without `prev=` and `hash=`)
- `hash = sha256(prev + "\n" + canonical)`

**Line format:**
- `prev=<prev> hash=<hash> <canonical>\n`

#### MUST
- Path: `<repo_root>/audit/ops.log`
- Append-only, LF-only, deterministic field order as above.

#### SHOULD
- Periodic checkpoints (e.g., daily) based on the last `hash=` value.
- Later: signatures (HSM/KMS) when available.

### 6. Packaging relationship

#### MUST
- `tablinum package` must not write to job events.
- It may write to ops audit, which is not a package input.

#### Package events source
When building `metadata/events.log`:
1) **Prefer:** `<repo_root>/jobs/<jobid>/events.log`
2) **Fallback:** `<repo_root>/events.log` filtered by `job=<jobid>` (legacy mode)

This keeps Reference v1 compatible while supporting clean separation.

### 7. Verify audit chain (CLI)

**Command:**
```sh
tablinum verify-audit --config tablinum.ini
```

**Checks (fail-fast):**
- LF-only (no `\r`)
- Per-line format: `prev=<64hex> hash=<64hex> <canonical>`
- `prev` chain continuity
- Hash rule: `sha256(prev + "\n" + canonical)`

**Exit codes:**
- `0` ok
- `3` audit log missing
- `4` I/O error
- `5` integrity/format failure
