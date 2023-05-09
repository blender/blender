/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup bli
 *
 * Some really low-level file operations.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include <sys/stat.h>

#if defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__) || defined(__HAIKU__)
/* Other modern unix OS's should probably use this also. */
#  include <sys/statvfs.h>
#  define USE_STATFS_STATVFS
#endif

#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || \
    defined(__DragonFly__)
/* For statfs */
#  include <sys/mount.h>
#  include <sys/param.h>
#endif

#if defined(__linux__) || defined(__hpux) || defined(__GNU__) || defined(__GLIBC__)
#  include <sys/vfs.h>
#endif

#include <fcntl.h>
#include <string.h> /* `strcpy` etc. */

#ifdef WIN32
#  include "BLI_string_utf8.h"
#  include "BLI_winstuff.h"
#  include "utfconv.h"
#  include <ShObjIdl.h>
#  include <direct.h>
#  include <io.h>
#  include <stdbool.h>
#else
#  include <pwd.h>
#  include <sys/ioctl.h>
#  include <unistd.h>
#endif

/* lib includes */
#include "MEM_guardedalloc.h"

#include "BLI_fileops.h"
#include "BLI_linklist.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

char *BLI_current_working_dir(char *dir, const size_t maxncpy)
{
#if defined(WIN32)
  wchar_t path[MAX_PATH];
  if (_wgetcwd(path, MAX_PATH)) {
    if (BLI_strncpy_wchar_as_utf8(dir, path, maxncpy) != maxncpy) {
      return dir;
    }
  }
  return NULL;
#else
  const char *pwd = BLI_getenv("PWD");
  if (pwd) {
    size_t srclen = BLI_strnlen(pwd, maxncpy);
    if (srclen != maxncpy) {
      memcpy(dir, pwd, srclen + 1);
      return dir;
    }
    return NULL;
  }
  return getcwd(dir, maxncpy);
#endif
}

double BLI_dir_free_space(const char *dir)
{
#ifdef WIN32
  DWORD sectorspc, bytesps, freec, clusters;
  char tmp[4];

  tmp[0] = '\\';
  tmp[1] = 0; /* Just a fail-safe. */
  if (ELEM(dir[0], '/', '\\')) {
    tmp[0] = '\\';
    tmp[1] = 0;
  }
  else if (dir[1] == ':') {
    tmp[0] = dir[0];
    tmp[1] = ':';
    tmp[2] = '\\';
    tmp[3] = 0;
  }

  GetDiskFreeSpace(tmp, &sectorspc, &bytesps, &freec, &clusters);

  return (double)(freec * bytesps * sectorspc);
#else

#  ifdef USE_STATFS_STATVFS
  struct statvfs disk;
#  else
  struct statfs disk;
#  endif

  char name[FILE_MAXDIR], *slash;
  int len = strlen(dir);

  if (len >= FILE_MAXDIR) {
    /* path too long */
    return -1;
  }

  strcpy(name, dir);

  if (len) {
    slash = strrchr(name, '/');
    if (slash) {
      slash[1] = 0;
    }
  }
  else {
    strcpy(name, "/");
  }

#  if defined(USE_STATFS_STATVFS)
  if (statvfs(name, &disk)) {
    return -1;
  }
#  elif defined(USE_STATFS_4ARGS)
  if (statfs(name, &disk, sizeof(struct statfs), 0)) {
    return -1;
  }
#  else
  if (statfs(name, &disk)) {
    return -1;
  }
#  endif

  return (((double)disk.f_bsize) * ((double)disk.f_bfree));
#endif
}

int64_t BLI_ftell(FILE *stream)
{
#ifdef WIN32
  return _ftelli64(stream);
#else
  return ftell(stream);
#endif
}

int BLI_fseek(FILE *stream, int64_t offset, int whence)
{
#ifdef WIN32
  return _fseeki64(stream, offset, whence);
#else
  return fseek(stream, offset, whence);
#endif
}

int64_t BLI_lseek(int fd, int64_t offset, int whence)
{
#ifdef WIN32
  return _lseeki64(fd, offset, whence);
#else
  return lseek(fd, offset, whence);
#endif
}

size_t BLI_file_descriptor_size(int file)
{
  BLI_stat_t st;
  if ((file < 0) || (BLI_fstat(file, &st) == -1)) {
    return -1;
  }
  return st.st_size;
}

size_t BLI_file_size(const char *path)
{
  BLI_stat_t stats;
  if (BLI_stat(path, &stats) == -1) {
    return -1;
  }
  return stats.st_size;
}

/* Return file attributes. Apple version of this function is defined in storage_apple.mm */
#ifndef __APPLE__
eFileAttributes BLI_file_attributes(const char *path)
{
  int ret = 0;

#  ifdef WIN32

  if (BLI_path_extension_check(path, ".lnk")) {
    return FILE_ATTR_ALIAS;
  }

  WCHAR wline[FILE_MAXDIR];
  if (conv_utf_8_to_16(path, wline, ARRAY_SIZE(wline)) != 0) {
    return ret;
  }
  DWORD attr = GetFileAttributesW(wline);
  if (attr & FILE_ATTRIBUTE_READONLY) {
    ret |= FILE_ATTR_READONLY;
  }
  if (attr & FILE_ATTRIBUTE_HIDDEN) {
    ret |= FILE_ATTR_HIDDEN;
  }
  if (attr & FILE_ATTRIBUTE_SYSTEM) {
    ret |= FILE_ATTR_SYSTEM;
  }
  if (attr & FILE_ATTRIBUTE_ARCHIVE) {
    ret |= FILE_ATTR_ARCHIVE;
  }
  if (attr & FILE_ATTRIBUTE_COMPRESSED) {
    ret |= FILE_ATTR_COMPRESSED;
  }
  if (attr & FILE_ATTRIBUTE_ENCRYPTED) {
    ret |= FILE_ATTR_ENCRYPTED;
  }
  if (attr & FILE_ATTRIBUTE_TEMPORARY) {
    ret |= FILE_ATTR_TEMPORARY;
  }
  if (attr & FILE_ATTRIBUTE_SPARSE_FILE) {
    ret |= FILE_ATTR_SPARSE_FILE;
  }
  if (attr & FILE_ATTRIBUTE_OFFLINE || attr & FILE_ATTRIBUTE_RECALL_ON_OPEN ||
      attr & FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS) {
    ret |= FILE_ATTR_OFFLINE;
  }
  if (attr & FILE_ATTRIBUTE_REPARSE_POINT) {
    ret |= FILE_ATTR_REPARSE_POINT;
  }

#  else

  UNUSED_VARS(path);

  /* TODO:
   * If Immutable set FILE_ATTR_READONLY
   * If Archived set FILE_ATTR_ARCHIVE
   */
#  endif
  return ret;
}
#endif

/* Return alias/shortcut file target. Apple version is defined in storage_apple.mm */
#ifndef __APPLE__
bool BLI_file_alias_target(const char *filepath,
                           /* This parameter can only be `const` on Linux since
                            * redirections are not supported there.
                            * NOLINTNEXTLINE: readability-non-const-parameter. */
                           char r_targetpath[/*FILE_MAXDIR*/])
{
#  ifdef WIN32
  if (!BLI_path_extension_check(filepath, ".lnk")) {
    return false;
  }

  HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
  if (FAILED(hr)) {
    return false;
  }

  IShellLinkW *Shortcut = NULL;
  hr = CoCreateInstance(
      &CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW, (LPVOID *)&Shortcut);

  bool success = false;
  if (SUCCEEDED(hr)) {
    IPersistFile *PersistFile;
    hr = Shortcut->lpVtbl->QueryInterface(Shortcut, &IID_IPersistFile, (LPVOID *)&PersistFile);
    if (SUCCEEDED(hr)) {
      WCHAR path_utf16[FILE_MAXDIR] = {0};
      if (conv_utf_8_to_16(filepath, path_utf16, ARRAY_SIZE(path_utf16)) == 0) {
        hr = PersistFile->lpVtbl->Load(PersistFile, path_utf16, STGM_READ);
        if (SUCCEEDED(hr)) {
          hr = Shortcut->lpVtbl->Resolve(Shortcut, 0, SLR_NO_UI | SLR_UPDATE);
          if (SUCCEEDED(hr)) {
            wchar_t target_utf16[FILE_MAXDIR] = {0};
            hr = Shortcut->lpVtbl->GetPath(Shortcut, target_utf16, FILE_MAXDIR, NULL, 0);
            if (SUCCEEDED(hr)) {
              success = (conv_utf_16_to_8(target_utf16, r_targetpath, FILE_MAXDIR) == 0);
            }
          }
          PersistFile->lpVtbl->Release(PersistFile);
        }
      }
    }
    Shortcut->lpVtbl->Release(Shortcut);
  }

  CoUninitialize();
  return (success && r_targetpath[0]);
#  else
  UNUSED_VARS(r_targetpath, filepath);
  /* File-based redirection not supported. */
  return false;
#  endif
}
#endif

int BLI_exists(const char *path)
{
#if defined(WIN32)
  BLI_stat_t st;
  wchar_t *tmp_16 = alloc_utf16_from_8(path, 1);
  int len, res;

  len = wcslen(tmp_16);
  /* in Windows #stat doesn't recognize dir ending on a slash
   * so we remove it here */
  if ((len > 3) && ELEM(tmp_16[len - 1], L'\\', L'/')) {
    tmp_16[len - 1] = '\0';
  }
  /* two special cases where the trailing slash is needed:
   * 1. after the share part of a UNC path
   * 2. after the C:\ when the path is the volume only
   */
  if ((len >= 3) && (tmp_16[0] == L'\\') && (tmp_16[1] == L'\\')) {
    BLI_path_normalize_unc_16(tmp_16);
  }

  if ((tmp_16[1] == L':') && (tmp_16[2] == L'\0')) {
    tmp_16[2] = L'\\';
    tmp_16[3] = L'\0';
  }

  res = BLI_wstat(tmp_16, &st);

  free(tmp_16);
  if (res == -1) {
    return 0;
  }
#else
  struct stat st;
  BLI_assert(!BLI_path_is_rel(path));
  if (stat(path, &st)) {
    return 0;
  }
#endif
  return (st.st_mode);
}

#ifdef WIN32
int BLI_fstat(int fd, BLI_stat_t *buffer)
{
#  if defined(_MSC_VER)
  return _fstat64(fd, buffer);
#  else
  return _fstat(fd, buffer);
#  endif
}

int BLI_stat(const char *path, BLI_stat_t *buffer)
{
  int r;
  UTF16_ENCODE(path);

  r = BLI_wstat(path_16, buffer);

  UTF16_UN_ENCODE(path);
  return r;
}

int BLI_wstat(const wchar_t *path, BLI_stat_t *buffer)
{
#  if defined(_MSC_VER)
  return _wstat64(path, buffer);
#  else
  return _wstat(path, buffer);
#  endif
}
#else
int BLI_fstat(int fd, struct stat *buffer)
{
  return fstat(fd, buffer);
}

int BLI_stat(const char *path, struct stat *buffer)
{
  return stat(path, buffer);
}
#endif

bool BLI_is_dir(const char *file)
{
  return S_ISDIR(BLI_exists(file));
}

bool BLI_is_file(const char *path)
{
  const int mode = BLI_exists(path);
  return (mode && !S_ISDIR(mode));
}

/**
 * Use for both text and binary file reading.
 */
static void *file_read_data_as_mem_impl(FILE *fp,
                                        bool read_size_exact,
                                        size_t pad_bytes,
                                        size_t *r_size)
{
  BLI_stat_t st;
  if (BLI_fstat(fileno(fp), &st) == -1) {
    return NULL;
  }
  if (S_ISDIR(st.st_mode)) {
    return NULL;
  }
  if (BLI_fseek(fp, 0L, SEEK_END) == -1) {
    return NULL;
  }
  /* Don't use the 'st_size' because it may be the symlink. */
  const long int filelen = BLI_ftell(fp);
  if (filelen == -1) {
    return NULL;
  }
  if (BLI_fseek(fp, 0L, SEEK_SET) == -1) {
    return NULL;
  }

  void *mem = MEM_mallocN(filelen + pad_bytes, __func__);
  if (mem == NULL) {
    return NULL;
  }

  const long int filelen_read = fread(mem, 1, filelen, fp);
  if ((filelen_read < 0) || ferror(fp)) {
    MEM_freeN(mem);
    return NULL;
  }

  if (read_size_exact) {
    if (filelen_read != filelen) {
      MEM_freeN(mem);
      return NULL;
    }
  }
  else {
    if (filelen_read < filelen) {
      mem = MEM_reallocN(mem, filelen_read + pad_bytes);
      if (mem == NULL) {
        return NULL;
      }
    }
  }

  *r_size = filelen_read;

  return mem;
}

void *BLI_file_read_text_as_mem(const char *filepath, size_t pad_bytes, size_t *r_size)
{
  FILE *fp = BLI_fopen(filepath, "r");
  void *mem = NULL;
  if (fp) {
    mem = file_read_data_as_mem_impl(fp, false, pad_bytes, r_size);
    fclose(fp);
  }
  return mem;
}

void *BLI_file_read_binary_as_mem(const char *filepath, size_t pad_bytes, size_t *r_size)
{
  FILE *fp = BLI_fopen(filepath, "rb");
  void *mem = NULL;
  if (fp) {
    mem = file_read_data_as_mem_impl(fp, true, pad_bytes, r_size);
    fclose(fp);
  }
  return mem;
}

void *BLI_file_read_text_as_mem_with_newline_as_nil(const char *filepath,
                                                    bool trim_trailing_space,
                                                    size_t pad_bytes,
                                                    size_t *r_size)
{
  char *mem = BLI_file_read_text_as_mem(filepath, pad_bytes, r_size);
  if (mem != NULL) {
    char *mem_end = mem + *r_size;
    if (pad_bytes != 0) {
      *mem_end = '\0';
    }
    for (char *p = mem, *p_next; p != mem_end; p = p_next) {
      p_next = memchr(p, '\n', mem_end - p);
      if (p_next != NULL) {
        if (trim_trailing_space) {
          for (char *p_trim = p_next - 1; p_trim > p && ELEM(*p_trim, ' ', '\t'); p_trim--) {
            *p_trim = '\0';
          }
        }
        *p_next = '\0';
        p_next++;
      }
      else {
        p_next = mem_end;
      }
    }
  }
  return mem;
}

LinkNode *BLI_file_read_as_lines(const char *filepath)
{
  FILE *fp = BLI_fopen(filepath, "r");
  LinkNodePair lines = {NULL, NULL};
  char *buf;
  size_t size;

  if (!fp) {
    return NULL;
  }

  BLI_fseek(fp, 0, SEEK_END);
  size = (size_t)BLI_ftell(fp);
  BLI_fseek(fp, 0, SEEK_SET);

  if (UNLIKELY(size == (size_t)-1)) {
    fclose(fp);
    return NULL;
  }

  buf = MEM_mallocN(size, "file_as_lines");
  if (buf) {
    size_t i, last = 0;

    /*
     * size = because on win32 reading
     * all the bytes in the file will return
     * less bytes because of `CRNL` changes.
     */
    size = fread(buf, 1, size, fp);
    for (i = 0; i <= size; i++) {
      if (i == size || buf[i] == '\n') {
        char *line = BLI_strdupn(&buf[last], i - last);
        BLI_linklist_append(&lines, line);
        last = i + 1;
      }
    }

    MEM_freeN(buf);
  }

  fclose(fp);

  return lines.list;
}

void BLI_file_free_lines(LinkNode *lines)
{
  BLI_linklist_freeN(lines);
}

bool BLI_file_older(const char *file1, const char *file2)
{
#ifdef WIN32
  struct _stat st1, st2;

  UTF16_ENCODE(file1);
  UTF16_ENCODE(file2);

  if (_wstat(file1_16, &st1)) {
    return false;
  }
  if (_wstat(file2_16, &st2)) {
    return false;
  }

  UTF16_UN_ENCODE(file2);
  UTF16_UN_ENCODE(file1);
#else
  struct stat st1, st2;

  if (stat(file1, &st1)) {
    return false;
  }
  if (stat(file2, &st2)) {
    return false;
  }
#endif
  return (st1.st_mtime < st2.st_mtime);
}
