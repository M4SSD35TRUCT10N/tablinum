# Coding rules (Reference v1)

---

## Deutsch (DE)

Diese Datei ist normativ für den Tablinum-Core. Ziel: **striktes C89/ANSI‑C**, **fail‑fast**, **OpenBSD‑Safety**, **Header‑Implementations**.

### 1) Säulen
- **C89/ANSI‑C:** keine C99-Features als Designvoraussetzung.
- **Fail‑fast:** wenn etwas falsch ist, sofort stoppen – mit präziser Fehlermeldung.
- **OpenBSD‑Safety:** begrenzte Operationen, keine stillen Trunkierungen, unsichere APIs vermeiden.
- **Header‑Implementations:** Module sind `*.h` + eine `*.c`, die `TBL_*_IMPLEMENTATION` setzt und den Header inkludiert.

### 2) Keine direkte printf-family (MUST)
- Im Core dürfen **keine** direkten Aufrufe der printf-family vorkommen:
  `printf()`/`sprintf()`/`fprintf()`/`vfprintf()`/`vsprintf()`.
- printf-family ist ausschließlich **innerhalb** von `src/core/safe.h` erlaubt.
- Für FILE‑Ausgaben sind die sicheren Helfer aus `src/core/safe.h` zu verwenden:
  - `tbl_fputs_ok`, `tbl_fputs2_ok` … `tbl_fputs5_ok`, `tbl_fputc_ok`, `tbl_fwrite_all_ok`, `tbl_vfprintf_ok`, `tbl_fflush_ok`
- Für Zahlen: `tbl_u32_to_dec_ok`, `tbl_ul_to_dec_ok`.
- Für String‑Ops: `tbl_strlcpy_ok`, `tbl_strlcat_ok` (oder `tbl_strl*` wenn Trunkierung nicht relevant ist).

### 3) `_ok`-Namenskonvention für boolsche Helper (MUST)
Funktionen, die **1 = Erfolg / 0 = Fehler** zurückgeben, müssen ein `_ok`‑Suffix haben (oder es muss ein `_ok`‑Alias existieren und der Core nutzt den Alias).

Beispiele:
- `tbl_parse_u32_ok(...)`
- `tbl_u32_to_dec_ok(...)`
- `tbl_ul_to_dec_ok(...)`
- `tbl_sha256_hex_ok(...)`

### 4) Compile-time Enforcement (MUST)
In Debug/Release werden die Regeln per Defines erzwungen:
- `TBL_STRICT_NAMES=1` → verbietet ambige Helper ohne `_ok`‑Suffix.
- `TBL_FORBID_STDIO_FORMAT=1` → verbietet printf-family in Projektcode (printf-family nur in `core/safe.h`).

### 5) Header-Implementation Pattern (MUST)
Jedes Core‑Modul ist `*.h` + eine `*.c` Datei, die nur `#define TBL_*_IMPLEMENTATION` setzt und den Header inkludiert.

### 6) Build Hygiene (SHOULD)
Wenn tack Header‑Änderungen nicht zuverlässig als Abhängigkeit erkennt, ist nach größeren Header‑Refactorings ein `tack clobber` sinnvoll, damit keine stale Objects getestet werden.

### 7) Dokumentation ist bilingual (MUST)
Dokumentation im Repo muss **Deutsch (DE)** und **English (EN)** enthalten (entweder als Abschnitte `## Deutsch (DE)` / `## English (EN)` oder als Marker `**DE:**` / `**EN:**`).

Diese Regel wird durch den Test `docs_bilingual_test` enforced.


---

## English (EN)

This file is normative for the Tablinum core. Goal: **strict C89/ANSI‑C**, **fail‑fast**, **OpenBSD safety**, **header implementations**.

### 1) Pillars
- **C89/ANSI‑C:** no C99 features required by design.
- **Fail‑fast:** if something is wrong, stop early with a precise error.
- **OpenBSD safety:** bounded operations, avoid silent truncation, avoid unsafe APIs.
- **Header implementations:** modules are `*.h` plus a `*.c` that defines `TBL_*_IMPLEMENTATION` and includes the header.

### 2) No direct printf-family calls (MUST)
- Core code MUST NOT call printf-family functions directly:
  `printf()`/`sprintf()`/`fprintf()`/`vfprintf()`/`vsprintf()`.
- printf-family is allowed **only** inside `src/core/safe.h`.
- Use `src/core/safe.h` helpers for file output:
  - `tbl_fputs_ok`, `tbl_fputs2_ok` … `tbl_fputs5_ok`, `tbl_fputc_ok`, `tbl_fwrite_all_ok`, `tbl_vfprintf_ok`, `tbl_fflush_ok`
- For numbers: `tbl_u32_to_dec_ok`, `tbl_ul_to_dec_ok`.
- For string ops: `tbl_strlcpy_ok`, `tbl_strlcat_ok` (or `tbl_strl*` if truncation is irrelevant).

### 3) `_ok` naming for boolean helpers (MUST)
Functions that return **1 = success / 0 = failure** MUST have an `_ok` suffix (or a `_ok` alias must exist and the core uses that alias).

Examples:
- `tbl_parse_u32_ok(...)`
- `tbl_u32_to_dec_ok(...)`
- `tbl_ul_to_dec_ok(...)`
- `tbl_sha256_hex_ok(...)`

### 4) Compile-time enforcement (MUST)
In Debug/Release the rules are enforced via defines:
- `TBL_STRICT_NAMES=1` → forbids ambiguous helpers without `_ok`.
- `TBL_FORBID_STDIO_FORMAT=1` → forbids printf-family in project code (printf-family allowed only inside `core/safe.h`).

### 5) Header-implementation pattern (MUST)
Each core module is a `*.h` plus a `*.c` file that only defines `TBL_*_IMPLEMENTATION` and includes the header.

### 6) Build hygiene (SHOULD)
If tack does not reliably detect header changes as dependencies, run `tack clobber` after larger header refactors to avoid testing stale objects.

### 7) Documentation must be bilingual (MUST)
Repository documentation must contain **Deutsch (DE)** and **English (EN)** (either as sections `## Deutsch (DE)` / `## English (EN)` or as markers `**DE:**` / `**EN:**`).

This rule is enforced by the `docs_bilingual_test`.
