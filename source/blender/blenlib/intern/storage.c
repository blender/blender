/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 * Reorganised mar-01 nzc
 * Some really low-level file thingies.
 */

/** \file
 * \ingroup bli
 */

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/stat.h>

#if defined(__NetBSD__) || defined(__DragonFly__) || defined(__HAIKU__)
/* Other modern unix os's should probably use this also */
#  include <sys/statvfs.h>
#  define USE_STATFS_STATVFS
#endif

#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || \
    defined(__DragonFly__)
/* For statfs */
#  include <sys/param.h>
#  include <sys/mount.h>
#endif

#if defined(__linux__) || defined(__hpux) || defined(__GNU__) || defined(__GLIBC__)
#  include <sys/vfs.h>
#endif

#include <fcntl.h>
#include <string.h> /* strcpy etc.. */

#ifdef WIN32
#  include <io.h>
#  include <direct.h>
#  include <stdbool.h>
#  include "BLI_winstuff.h"
#  include "BLI_string_utf8.h"
#  include "utfconv.h"
#else
#  include <sys/ioctl.h>
#  include <unistd.h>
#  include <pwd.h>
#endif

/* lib includes */
#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_linklist.h"
#include "BLI_string.h"
#include "BLI_fileops.h"
#include "BLI_path_util.h"

/**
 * Copies the current working directory into *dir (max size maxncpy), and
 * returns a pointer to same.
 *
 * \note can return NULL when the size is not big enough
 */
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
    else {
      return NULL;
    }
  }
  return getcwd(dir, maxncpy);
#endif
}

/**
 * Returns the number of free bytes on the volume containing the specified pathname. */
/* Not actually used anywhere.
 */
double BLI_dir_free_space(const char *dir)
{
#ifdef WIN32
  DWORD sectorspc, bytesps, freec, clusters;
  char tmp[4];

  tmp[0] = '\\';
  tmp[1] = 0; /* Just a failsafe */
  if (dir[0] == '/' || dir[0] == '\\') {
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

/**
 * Returns the file size of an opened file descriptor.
 */
size_t BLI_file_descriptor_size(int file)
{
  struct stat st;
  if ((file < 0) || (fstat(file, &st) == -1)) {
    return -1;
  }
  return st.st_size;
}

/**
 * Returns the size of a file.
 */
size_t BLI_file_size(const char *path)
{
  BLI_stat_t stats;
  if (BLI_stat(path, &stats) == -1) {
    return -1;
  }
  return stats.st_size;
}

/**
 * Returns the st_mode from stat-ing the specified path name, or 0 if stat fails
 * (most likely doesn't exist or no access).
 */
int BLI_exists(const char *name)
{
#if defined(WIN32)
  BLI_stat_t st;
  wchar_t *tmp_16 = alloc_utf16_from_8(name, 1);
  int len, res;
  unsigned int old_error_mode;

  len = wcslen(tmp_16);
  /* in Windows #stat doesn't recognize dir ending on a slash
   * so we remove it here */
  if (len > 3 && (tmp_16[len - 1] == L'\\' || tmp_16[len - 1] == L'/')) {
    tmp_16[len - 1] = '\0';
  }
  /* two special cases where the trailing slash is needed:
   * 1. after the share part of a UNC path
   * 2. after the C:\ when the path is the volume only
   */
  if ((len >= 3) && (tmp_16[0] == L'\\') && (tmp_16[1] == L'\\')) {
    BLI_cleanup_unc_16(tmp_16);
  }

  if ((tmp_16[1] == L':') && (tmp_16[2] == L'\0')) {
    tmp_16[2] = L'\\';
    tmp_16[3] = L'\0';
  }

  /* change error mode so user does not get a "no disk in drive" popup
   * when looking for a file on an empty CD/DVD drive */
  old_error_mode = SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);

  res = BLI_wstat(tmp_16, &st);

  SetErrorMode(old_error_mode);

  free(tmp_16);
  if (res == -1) {
    return (0);
  }
#else
  struct stat st;
  BLI_assert(!BLI_path_is_rel(name));
  if (stat(name, &st)) {
    return (0);
  }
#endif
  return (st.st_mode);
}

#ifdef WIN32
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
int BLI_stat(const char *path, struct stat *buffer)
{
  return stat(path, buffer);
}
#endif

/**
 * Does the specified path point to a directory?
 * \note Would be better in fileops.c except that it needs stat.h so add here
 */
bool BLI_is_dir(const char *file)
{
  return S_ISDIR(BLI_exists(file));
}

/**
 * Does the specified path point to a non-directory?
 */
bool BLI_is_file(const char *path)
{
  const int mode = BLI_exists(path);
  return (mode && !S_ISDIR(mode));
}

void *BLI_file_read_text_as_mem(const char *filepath, size_t pad_bytes, size_t *r_size)
{
  FILE *fp = BLI_fopen(filepath, "r");
  void *mem = NULL;

  if (fp) {
    fseek(fp, 0L, SEEK_END);
    const long int filelen = ftell(fp);
    if (filelen == -1) {
      goto finally;
    }
    fseek(fp, 0L, SEEK_SET);

    mem = MEM_mallocN(filelen + pad_bytes, __func__);
    if (mem == NULL) {
      goto finally;
    }

    const long int filelen_read = fread(mem, 1, filelen, fp);
    if ((filelen_read < 0) || ferror(fp)) {
      MEM_freeN(mem);
      mem = NULL;
      goto finally;
    }

    if (filelen_read < filelen) {
      mem = MEM_reallocN(mem, filelen_read + pad_bytes);
      if (mem == NULL) {
        goto finally;
      }
    }

    *r_size = filelen_read;

  finally:
    fclose(fp);
  }

  return mem;
}

void *BLI_file_read_binary_as_mem(const char *filepath, size_t pad_bytes, size_t *r_size)
{
  FILE *fp = BLI_fopen(filepath, "rb");
  void *mem = NULL;

  if (fp) {
    fseek(fp, 0L, SEEK_END);
    const long int filelen = ftell(fp);
    if (filelen == -1) {
      goto finally;
    }
    fseek(fp, 0L, SEEK_SET);

    mem = MEM_mallocN(filelen + pad_bytes, __func__);
    if (mem == NULL) {
      goto finally;
    }

    const long int filelen_read = fread(mem, 1, filelen, fp);
    if ((filelen_read != filelen) || ferror(fp)) {
      MEM_freeN(mem);
      mem = NULL;
      goto finally;
    }

    *r_size = filelen_read;

  finally:
    fclose(fp);
  }

  return mem;
}

/**
 * Reads the contents of a text file and returns the lines in a linked list.
 */
LinkNode *BLI_file_read_as_lines(const char *name)
{
  FILE *fp = BLI_fopen(name, "r");
  LinkNodePair lines = {NULL, NULL};
  char *buf;
  size_t size;

  if (!fp) {
    return NULL;
  }

  fseek(fp, 0, SEEK_END);
  size = (size_t)ftell(fp);
  fseek(fp, 0, SEEK_SET);

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

/*
 * Frees memory from a previous call to BLI_file_read_as_lines.
 */
void BLI_file_free_lines(LinkNode *lines)
{
  BLI_linklist_freeN(lines);
}

/** is file1 older then file2 */
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
