# Packaging notes (OAIS-light / E-ARK inspired)

**DE:** Tablinum ist bewusst „OAIS‑light“: Fixity + Audit + Durable Record + CAS.  
Dieses Dokument skizziert eine mögliche, spätere Annäherung an E‑ARK‑artige Paketstrukturen (SIP/AIP/DIP), ohne die CAS‑Vorteile aufzugeben.

**EN:** Tablinum is intentionally “OAIS‑light”: fixity + audit + durable record + CAS.  
This document sketches a future approach towards E‑ARK‑like package structures (SIP/AIP/DIP) without giving up CAS benefits.

---

## Today (already implemented)

### SIP-light (spool jobdir)

```
spool/inbox/<jobid>/
  payload.bin
  job.meta            (optional INI)
```

### AIP-light (repo)

```
repo/
  objects/<sha256>            (CAS payload)
  records/<jobid>.ini         (durable record, references sha256)
  events.log                  (append-only audit trail)
```

### DIP-light (export)

```
<outdir>/
  payload.bin
  record.ini
```

---

## Proposed future (E-ARK inspired)

### Option A: Keep CAS as the payload layer

Create a per-job AIP folder that *references* CAS objects, and adds manifests.

```
repo/aip/<jobid>/
  record.ini
  payload.ref          (e.g. "sha256=<hash>" + size)
  manifest-sha256.txt  (fixity over record.ini + payload.ref + optional meta)
  events.ref           (optional pointer into events.log, or copied subset)
```

Benefits:

- preserves de-duplication and content-addressing
- AIP folder is self-describing

### Option B: Full AIP materialization (bigger, simpler portability)

Materialize a full package for each job:

```
repo/aip/<jobid>/
  payload.bin
  record.ini
  manifest-sha256.txt
  meta/...
```

Benefits:

- “portable by copying one directory”

Trade-off:

- duplicates data (no CAS de-dup)

---

## Manifests (suggested)

- `manifest-sha256.txt`: one line per file, format similar to `sha256sum`:

```
<sha256>  record.ini
<sha256>  payload.ref
```

Later additions:

- a minimal `bagit`-like structure (checksum file + payload folder)
- optional JSON sidecar (for interoperability)
