# Packaging (E-ARK inspired, OAIS-light)

Tablinum keeps the *durable truth* in the repository (CAS + record.ini + append-only events.log).
Packaging is a **reconstruction step** that emits a self-contained folder package.

This is **E-ARK inspired** (CSIP/AIP concepts) but intentionally minimal (“OAIS-light”).
No XML, no external tooling, no bagit dependency — just a stable directory layout + SHA-256 fixity.

---

## Package kinds

- **AIP** (Archival Information Package): export from durable record (`status=ok`).
- **SIP** (Submission Information Package): same layout for now, but `kind=sip` in `metadata/package.ini`.

CLI:

```text
tablinum package JOBID OUTDIR [--format aip|sip]
```

`OUTDIR` is the **package root directory** (will be created if missing).

---

## Layout v1 (eark-lite)

Package root:

```text
OUTDIR/
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

### `representations/rep0/data/<payload>`
- The reconstructed payload file.
- File name defaults to `payload.bin`, or uses `record.payload` if set.

### `metadata/record.ini`
- Exact durable record copied from `repo/records/<jobid>.ini`.

### `metadata/events.log`
- Filtered lines from `repo/events.log` containing `job=<jobid>`.
- If the repository has no `events.log`, an empty file is created.

### `metadata/package.ini`
Small self-description (INI):

```ini
[package]
schema = tablinum.package.v1
kind = aip
jobid = jobOK
created_ts = 1700000000
tool = tablinum 0.1.0
payload_name = payload.bin
payload_sha256 = <sha256>
record_sha256 = <sha256>
cas_sha256 = <sha256>
events_lines = 3
```

### `metadata/manifest-sha256.txt`
sha256sum-compatible fixity list for the package contents:

```text
<sha256>  representations/rep0/data/payload.bin
<sha256>  metadata/record.ini
<sha256>  metadata/package.ini
<sha256>  metadata/events.log
```

---

## Deutsch (DE)

Tablinum hält die „dauerhafte Wahrheit“ im Repository (CAS + record.ini + append-only events.log).
Packaging ist ein **Rekonstruktions-Schritt**, der ein selbstenthaltendes Ordnerpaket erzeugt.

Das ist **E-ARK inspiriert** (CSIP/AIP-Idee), aber bewusst minimal („OAIS-light“):
keine XML, keine externen Tools, keine BagIt-Abhängigkeit — nur stabiles Layout + SHA-256 Fixity.

CLI:

```text
tablinum package JOBID OUTDIR [--format aip|sip]
```

`OUTDIR` ist das **Package-Root-Verzeichnis**.

Layout siehe oben.
