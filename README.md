# Tablinum

**DE:** Tablinum ist ein strikt in C89 geschriebenes, portables *Document Hub* (paperless‑ähnlich) als **Single‑Binary**.  
Ziel: langlebige, in Jahrzehnten noch kompilier- und lesbare Dokumentenablage (Windows, Linux/RPi, 9front) mit **fail‑fast** und sicherem C‑Stil. Build/Test laufen über **tack**.

**EN:** Tablinum is a strict C89, portable *document hub* (paperless‑style) as a **single binary**.  
Goal: a durable document store that can still be compiled and read decades from now (Windows, Linux/RPi, 9front), using a **fail‑fast** secure C style. Build/tests are driven by **tack**.

---

## Deutsch (DE)

### Bedeutung des Namens

> Das tablinum ist das repräsentative Arbeitszimmer eines römischen Hauses, das in der typischen Bauweise des Atriumhauses häufig an der hinteren Schmalseite des atrium dem Eingang direkt gegenüber liegt. Es ist nicht zwangsläufig durch eine Wand mit Tür vom atrium getrennt, sondern unterstreicht seinen repräsentativen Charakter dadurch, dass es sich mit der ganzen Seite des Raumes zum atrium hin öffnet, wobei die Öffnung bei Bedarf durch einen Vorhang geschlossen werden kann. An der gegenüberliegenden Seite kann sich das Zimmer auch zum Garten (hortus) hin öffnen.  
>  
> Dem tablinum kommt aufgrund seiner Lage im Haus und seiner meist aufwändigen architektonischen Ausgestaltung nicht die Rolle eines Zimmers zum zurückgezogenen, ungestörten Arbeiten zu. Es dient stattdessen dem Empfang wichtiger Gäste zu politischen oder geschäftlichen Besprechungen, die nicht mit einem Essen verbunden werden.

Im übertragenen Sinn ist Tablinum genau das: ein „repräsentativer Knotenpunkt“ zwischen Eingang (Ingest) und Garten (Archiv/Repository) – offen, aber bei Bedarf klar getrennt und kontrolliert.

### Status

Frühe Bootstrap‑Phase:

- strikte C89‑Core‑Utilities
- robuste Spool/Claim‑Queue
- Ingest‑Role (Polling) bewegt Jobs von `inbox` → `claim` → `out`/`fail` und schreibt eine `.meta`‑Sidecar‑Datei

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

### Starten

#### Ingest (Spool‑Queue)

Start:

```bat
tablinum --config tablinum.ini --role ingest
```

Jobs in die Inbox legen:

```bat
echo hello > spool\inbox\job1.txt
echo fail  > spool\inbox\job2.bad
```

Verhalten:

- `job1.txt` wird geclaimt, nach `spool/out/` verschoben und bekommt `job1.txt.meta`
- `job2.bad` wird nach `spool/fail/` verschoben

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

Early bootstrap:

- strict C89 core utilities
- robust spool/claim queue
- ingest role (polling) moves jobs from `inbox` → `claim` → `out`/`fail` and writes a `.meta` sidecar

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

### Run

#### Ingest (spool queue)

Start ingest:

```bat
tablinum --config tablinum.ini --role ingest
```

Drop files into the inbox:

```bat
echo hello > spool\inbox\job1.txt
echo fail  > spool\inbox\job2.bad
```

Behavior:

- `job1.txt` will be claimed, moved to `spool/out/` and gets a `job1.txt.meta` sidecar
- `job2.bad` will be moved to `spool/fail/`

Spool layout:

- `spool/inbox/`  – new jobs
- `spool/claim/`  – claimed (in‑progress)
- `spool/out/`    – processed successfully
- `spool/fail/`   – failed jobs

---

## License

MIT
