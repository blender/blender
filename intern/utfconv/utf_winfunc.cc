/* SPDX-FileCopyrightText: 2012 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup intern_utf_conv
 */

#ifndef _WIN32_IE
#  define _WIN32_IE 0x0501
#endif

#include "utf_winfunc.hh"
#include "utfconv.hh"
#include <cwchar>
#include <io.h>
#include <windows.h>

FILE *ufopen(const char *filename, const char *mode)
{
  FILE *f = nullptr;
  UTF16_ENCODE(filename);
  UTF16_ENCODE(mode);

  if (filename_16 && mode_16) {
    f = _wfopen(filename_16, mode_16);
  }

  UTF16_UN_ENCODE(mode);
  UTF16_UN_ENCODE(filename);

  if (!f) {
    if ((f = fopen(filename, mode))) {
      printf("WARNING: %s is not utf path. Please update it.\n", filename);
    }
  }

  return f;
}

int uopen(const char *filename, int oflag, int pmode)
{
  int f = -1;
  UTF16_ENCODE(filename);

  if (filename_16) {
    f = _wopen(filename_16, oflag, pmode);
  }

  UTF16_UN_ENCODE(filename);

  if (f == -1) {
    if ((f = open(filename, oflag, pmode)) != -1) {
      printf("WARNING: %s is not utf path. Please update it.\n", filename);
    }
  }

  return f;
}

int uaccess(const char *filename, int mode)
{
  int r = -1;
  UTF16_ENCODE(filename);

  if (filename_16) {
    r = _waccess(filename_16, mode);
  }

  UTF16_UN_ENCODE(filename);

  return r;
}

int urename(const char *oldname, const char *newname, const bool do_replace)
{
  int r = -1;
  UTF16_ENCODE(oldname);
  UTF16_ENCODE(newname);

  if (oldname_16 && newname_16) {
    /* Closer to UNIX `rename` behavior, as it at least allows to replace an existing file.
     * Return value logic is inverted however (returns non-zero on sucess, 0 on failure).
     * Note that the operation will still fail if the 'newname' existing file is opened anywhere.
     */
    r = (MoveFileExW(oldname_16, newname_16, do_replace ? MOVEFILE_REPLACE_EXISTING : 0) == 0);
  }

  UTF16_UN_ENCODE(newname);
  UTF16_UN_ENCODE(oldname);
  return r;
}

int umkdir(const char *pathname)
{

  BOOL r = 0;
  UTF16_ENCODE(pathname);

  if (pathname_16) {
    r = CreateDirectoryW(pathname_16, NULL);
  }

  UTF16_UN_ENCODE(pathname);

  return r ? 0 : -1;
}

char *u_alloc_getenv(const char *varname)
{
  char *r = nullptr;
  wchar_t *str;
  UTF16_ENCODE(varname);
  if (varname_16) {
    str = _wgetenv(varname_16);
    r = alloc_utf_8_from_16(str, 0);
  }
  UTF16_UN_ENCODE(varname);

  return r;
}
void u_free_getenv(char *val)
{
  free(val);
}

int uput_getenv(const char *varname, char *value, size_t buffsize)
{
  int r = 0;
  wchar_t *str;

  if (!buffsize) {
    return r;
  }

  UTF16_ENCODE(varname);
  if (varname_16) {
    str = _wgetenv(varname_16);
    conv_utf_16_to_8(str, value, buffsize);
    r = 1;
  }
  UTF16_UN_ENCODE(varname);

  if (!r) {
    value[0] = 0;
  }

  return r;
}

int uputenv(const char *name, const char *value)
{
  int r = -1;
  UTF16_ENCODE(name);

  if (value) {
    /* set */
    UTF16_ENCODE(value);

    if (name_16 && value_16) {
      r = (SetEnvironmentVariableW(name_16, value_16) != 0) ? 0 : -1;
    }
    UTF16_UN_ENCODE(value);
  }
  else {
    /* clear */
    if (name_16) {
      r = (SetEnvironmentVariableW(name_16, NULL) != 0) ? 0 : -1;
    }
  }

  UTF16_UN_ENCODE(name);

  return r;
}
