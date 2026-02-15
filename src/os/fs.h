#ifndef TBL_OS_FS_H
#define TBL_OS_FS_H

#include <stddef.h>

/* Callback for directory listing.
   Return 0 to continue, nonzero to stop early. */
typedef int (*tbl_fs_list_cb)(void *ud,
                             const char *name,
                             const char *fullpath,
                             int is_dir);

/* Process id (best effort u32-ish). */
unsigned long tbl_fs_pid_u32(void);

/* Basic queries */
int tbl_fs_exists(const char *path, int *out_exists);
int tbl_fs_is_dir(const char *path, int *out_is_dir);

/* Directory creation */
int tbl_fs_mkdir_one(const char *path);     /* create one level (ok if exists as dir) */
int tbl_fs_mkdir_p(const char *path);       /* create parents like -p (ok if exists) */

/* Atomic-ish rename/move within same filesystem.
   If replace==0, attempt to avoid clobbering existing dst. */
int tbl_fs_rename_atomic(const char *src, const char *dst, int replace);

/* File helpers */
int tbl_fs_write_file(const char *path, const void *data, size_t len);

/* Remove helpers */
int tbl_fs_remove_file(const char *path);
int tbl_fs_remove_dir(const char *path);
int tbl_fs_rm_rf(const char *path); /* recursive delete (best effort) */

/* Directory listing (non-recursive) */
int tbl_fs_list_dir(const char *dirpath, tbl_fs_list_cb cb, void *ud);

#ifdef TBL_FS_IMPLEMENTATION

#include <string.h>
#include <stdio.h>

#include "core/safe.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#ifdef __PLAN9__
#include <u.h>
#include <libc.h>
#else
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#endif
#endif

#ifndef TBL_FS_PATH_MAX
#define TBL_FS_PATH_MAX 4096
#endif

static int tbl_fs_is_sep_char(int c)
{
    return (c == '/' || c == '\\');
}

static char tbl_fs_sep(void)
{
#ifdef _WIN32
    return '\\';
#else
    return '/';
#endif
}

unsigned long tbl_fs_pid_u32(void)
{
#ifdef _WIN32
    return (unsigned long)GetCurrentProcessId();
#else
#ifdef __PLAN9__
    return (unsigned long)getpid();
#else
    return (unsigned long)getpid();
#endif
#endif
}

int tbl_fs_exists(const char *path, int *out_exists)
{
    if (!out_exists) return 1;
    *out_exists = 0;

    if (!path || !path[0]) return 0;

#ifdef _WIN32
    {
        DWORD a = GetFileAttributesA(path);
        if (a == INVALID_FILE_ATTRIBUTES) {
            *out_exists = 0;
            return 0;
        }
        *out_exists = 1;
        return 0;
    }
#else
#ifdef __PLAN9__
    {
        Dir *d = dirstat(path);
        if (d) {
            free(d);
            *out_exists = 1;
        } else {
            *out_exists = 0;
        }
        return 0;
    }
#else
    {
        struct stat st;
        if (stat(path, &st) == 0) {
            *out_exists = 1;
        } else {
            *out_exists = 0;
        }
        return 0;
    }
#endif
#endif
}

int tbl_fs_is_dir(const char *path, int *out_is_dir)
{
    if (!out_is_dir) return 1;
    *out_is_dir = 0;

    if (!path || !path[0]) return 0;

#ifdef _WIN32
    {
        DWORD a = GetFileAttributesA(path);
        if (a == INVALID_FILE_ATTRIBUTES) return 0;
        *out_is_dir = (a & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0;
        return 0;
    }
#else
#ifdef __PLAN9__
    {
        Dir *d = dirstat(path);
        if (!d) return 0;
        *out_is_dir = (d->mode & DMDIR) ? 1 : 0;
        free(d);
        return 0;
    }
#else
    {
        struct stat st;
        if (stat(path, &st) != 0) return 0;
        *out_is_dir = S_ISDIR(st.st_mode) ? 1 : 0;
        return 0;
    }
#endif
#endif
}

int tbl_fs_mkdir_one(const char *path)
{
    int isdir;
    int ex;

    if (!path || !path[0]) return 1;

    /* already exists as dir is OK */
    if (tbl_fs_exists(path, &ex) == 0 && ex) {
        if (tbl_fs_is_dir(path, &isdir) == 0 && isdir) {
            return 0;
        }
        return 1; /* exists but not dir */
    }

#ifdef _WIN32
    if (CreateDirectoryA(path, NULL)) return 0;
    /* If it appeared between checks: accept as dir */
    if (tbl_fs_is_dir(path, &isdir) == 0 && isdir) return 0;
    return 1;
#else
#ifdef __PLAN9__
    {
        int fd;
        fd = create(path, OREAD, DMDIR | 0775);
        if (fd >= 0) {
            close(fd);
            return 0;
        }
        /* accept as dir if already exists */
        if (tbl_fs_is_dir(path, &isdir) == 0 && isdir) return 0;
        return 1;
    }
#else
    if (mkdir(path, 0775) == 0) return 0;
    if (errno == EEXIST) {
        if (tbl_fs_is_dir(path, &isdir) == 0 && isdir) return 0;
    }
    return 1;
#endif
#endif
}

int tbl_fs_mkdir_p(const char *path)
{
    char tmp[TBL_FS_PATH_MAX];
    size_t n;
    size_t i;
    char sep;

    if (!path || !path[0]) return 1;

    n = strlen(path);
    if (n >= sizeof(tmp)) return 1;

    (void)tbl_strlcpy(tmp, path, sizeof(tmp));

    /* normalize seps to platform sep and collapse repeats */
    sep = tbl_fs_sep();
    {
        size_t r = 0;
        size_t w = 0;
        int prev_sep = 0;
        while (tmp[r] != '\0') {
            char c = tmp[r];
            if (tbl_fs_is_sep_char((int)(unsigned char)c)) {
                if (!prev_sep) {
                    tmp[w++] = sep;
                    prev_sep = 1;
                }
            } else {
                tmp[w++] = c;
                prev_sep = 0;
            }
            r++;
        }
        tmp[w] = '\0';
    }

#ifdef _WIN32
    /* Skip "C:" prefix when creating parents */
    i = 0;
    if (((tmp[0] >= 'A' && tmp[0] <= 'Z') || (tmp[0] >= 'a' && tmp[0] <= 'z')) &&
        tmp[1] == ':') {
        i = 2;
        if (tmp[i] == sep) i++;
    } else if (tmp[0] == sep) {
        /* path like "\foo" */
        i = 1;
    }
#else
    i = 0;
    if (tmp[0] == sep) i = 1; /* keep root "/" intact */
#endif

    /* Create each component */
    for (; tmp[i] != '\0'; ++i) {
        if (tmp[i] == sep) {
            tmp[i] = '\0';
            if (tmp[0] != '\0') {
                if (tbl_fs_mkdir_one(tmp) != 0) return 1;
            }
            tmp[i] = sep;
        }
    }

    /* Final */
    if (tbl_fs_mkdir_one(tmp) != 0) return 1;
    return 0;
}

int tbl_fs_rename_atomic(const char *src, const char *dst, int replace)
{
    int ex;

    if (!src || !src[0] || !dst || !dst[0]) return 1;

    if (!replace) {
        ex = 0;
        (void)tbl_fs_exists(dst, &ex);
        if (ex) return 1;
    }

#ifdef _WIN32
    {
        DWORD flags = 0;
        if (replace) flags |= MOVEFILE_REPLACE_EXISTING;
        if (MoveFileExA(src, dst, flags)) return 0;
        return 1;
    }
#else
#ifdef __PLAN9__
    /* Plan 9 has rename(2) */
    if (!replace) {
        ex = 0;
        (void)tbl_fs_exists(dst, &ex);
        if (ex) return 1;
    }
    if (rename(src, dst) == 0) return 0;
    return 1;
#else
    if (!replace) {
        ex = 0;
        (void)tbl_fs_exists(dst, &ex);
        if (ex) return 1;
    }
    if (rename(src, dst) == 0) return 0;
    return 1;
#endif
#endif
}

int tbl_fs_write_file(const char *path, const void *data, size_t len)
{
    FILE *fp;
    size_t wr;

    if (!path || !path[0]) return 1;

    fp = fopen(path, "wb");
    if (!fp) return 1;

    wr = 0;
    if (len > 0) {
        wr = fwrite(data, 1, len, fp);
        if (wr != len) {
            fclose(fp);
            return 1;
        }
    }

    if (fclose(fp) != 0) return 1;
    return 0;
}

int tbl_fs_remove_file(const char *path)
{
    if (!path || !path[0]) return 1;

#ifdef _WIN32
    if (DeleteFileA(path)) return 0;
    return 1;
#else
#ifdef __PLAN9__
    if (remove(path) == 0) return 0;
    return 1;
#else
    if (unlink(path) == 0) return 0;
    return 1;
#endif
#endif
}

int tbl_fs_remove_dir(const char *path)
{
    if (!path || !path[0]) return 1;

#ifdef _WIN32
    if (RemoveDirectoryA(path)) return 0;
    return 1;
#else
#ifdef __PLAN9__
    /* remove() works for directories too */
    if (remove(path) == 0) return 0;
    return 1;
#else
    if (rmdir(path) == 0) return 0;
    return 1;
#endif
#endif
}

int tbl_fs_list_dir(const char *dirpath, tbl_fs_list_cb cb, void *ud)
{
    if (!dirpath || !dirpath[0]) return 1;
    if (!cb) return 1;

#ifdef _WIN32
    {
        char pat[TBL_FS_PATH_MAX];
        WIN32_FIND_DATAA fd;
        HANDLE h;
        size_t n;

        if (tbl_strlcpy(pat, dirpath, sizeof(pat)) >= sizeof(pat)) return 1;

        n = strlen(pat);
        if (n > 0 && pat[n - 1] != '\\' && pat[n - 1] != '/') {
            if (tbl_strlcat(pat, "\\", sizeof(pat)) >= sizeof(pat)) return 1;
        }
        if (tbl_strlcat(pat, "*", sizeof(pat)) >= sizeof(pat)) return 1;

        h = FindFirstFileA(pat, &fd);
        if (h == INVALID_HANDLE_VALUE) return 1;

        for (;;) {
            const char *name = fd.cFileName;
            char full[TBL_FS_PATH_MAX];
            int isdir;
            int stop;

            if (strcmp(name, ".") != 0 && strcmp(name, "..") != 0) {
                if (tbl_strlcpy(full, dirpath, sizeof(full)) >= sizeof(full)) {
                    FindClose(h);
                    return 1;
                }
                n = strlen(full);
                if (n > 0 && full[n - 1] != '\\' && full[n - 1] != '/') {
                    if (tbl_strlcat(full, "\\", sizeof(full)) >= sizeof(full)) {
                        FindClose(h);
                        return 1;
                    }
                }
                if (tbl_strlcat(full, name, sizeof(full)) >= sizeof(full)) {
                    FindClose(h);
                    return 1;
                }

                isdir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0;
                stop = cb(ud, name, full, isdir);
                if (stop) {
                    FindClose(h);
                    return 0;
                }
            }

            if (!FindNextFileA(h, &fd)) break;
        }

        FindClose(h);
        return 0;
    }
#else
#ifdef __PLAN9__
    {
        int fd;
        Dir *d;
        long n, i;
        char full[TBL_FS_PATH_MAX];

        fd = open(dirpath, OREAD);
        if (fd < 0) return 1;

        d = 0;
        n = dirreadall(fd, &d);
        close(fd);

        if (n < 0) {
            if (d) free(d);
            return 1;
        }

        for (i = 0; i < n; ++i) {
            const char *name = d[i].name;
            int isdir;
            int stop;

            if (!name) continue;
            if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;

            if (tbl_strlcpy(full, dirpath, sizeof(full)) >= sizeof(full)) {
                free(d);
                return 1;
            }
            if (full[0] && full[strlen(full) - 1] != '/') {
                if (tbl_strlcat(full, "/", sizeof(full)) >= sizeof(full)) {
                    free(d);
                    return 1;
                }
            }
            if (tbl_strlcat(full, name, sizeof(full)) >= sizeof(full)) {
                free(d);
                return 1;
            }

            isdir = (d[i].mode & DMDIR) ? 1 : 0;
            stop = cb(ud, name, full, isdir);
            if (stop) {
                free(d);
                return 0;
            }
        }

        free(d);
        return 0;
    }
#else
    {
        DIR *dp;
        struct dirent *de;

        dp = opendir(dirpath);
        if (!dp) return 1;

        for (;;) {
            char full[TBL_FS_PATH_MAX];
            int isdir;
            int stop;
            size_t n;

            de = readdir(dp);
            if (!de) break;

            if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;

            if (tbl_strlcpy(full, dirpath, sizeof(full)) >= sizeof(full)) {
                closedir(dp);
                return 1;
            }
            n = strlen(full);
            if (n > 0 && full[n - 1] != '/') {
                if (tbl_strlcat(full, "/", sizeof(full)) >= sizeof(full)) {
                    closedir(dp);
                    return 1;
                }
            }
            if (tbl_strlcat(full, de->d_name, sizeof(full)) >= sizeof(full)) {
                closedir(dp);
                return 1;
            }

            isdir = 0;
#ifdef DT_DIR
            if (de->d_type == DT_DIR) {
                isdir = 1;
            } else if (de->d_type == DT_UNKNOWN) {
                (void)tbl_fs_is_dir(full, &isdir);
            }
#else
            (void)tbl_fs_is_dir(full, &isdir);
#endif

            stop = cb(ud, de->d_name, full, isdir);
            if (stop) {
                closedir(dp);
                return 0;
            }
        }

        closedir(dp);
        return 0;
    }
#endif
#endif
}

static int tbl_fs_rm_rf_cb(void *ud, const char *name, const char *full, int is_dir)
{
    int *errp;
    (void)name;

    errp = (int *)ud;
    if (*errp) return 1;

    if (is_dir) {
        if (tbl_fs_rm_rf(full) != 0) {
            *errp = 1;
            return 1;
        }
    } else {
        if (tbl_fs_remove_file(full) != 0) {
            *errp = 1;
            return 1;
        }
    }
    return 0;
}

int tbl_fs_rm_rf(const char *path)
{
    int ex;
    int isdir;
    int err;

    if (!path || !path[0]) return 1;

    ex = 0;
    (void)tbl_fs_exists(path, &ex);
    if (!ex) return 0; /* best effort */

    isdir = 0;
    (void)tbl_fs_is_dir(path, &isdir);
    if (!isdir) {
        (void)tbl_fs_remove_file(path);
        return 0;
    }

    err = 0;
    (void)tbl_fs_list_dir(path, tbl_fs_rm_rf_cb, &err);

    (void)tbl_fs_remove_dir(path);
    return err ? 1 : 0;
}

#endif /* TBL_FS_IMPLEMENTATION */
#endif /* TBL_OS_FS_H */
