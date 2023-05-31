/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 * \brief Some types for dealing with directories.
 */

#include <sys/stat.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(WIN32)
typedef unsigned int mode_t;
#endif

#define FILELIST_DIRENTRY_SIZE_LEN 16
#define FILELIST_DIRENTRY_MODE_LEN 4
#define FILELIST_DIRENTRY_OWNER_LEN 16
#define FILELIST_DIRENTRY_TIME_LEN 8
#define FILELIST_DIRENTRY_DATE_LEN 16

struct direntry {
  mode_t type;
  const char *relname;
  const char *path;
#ifdef WIN32 /* keep in sync with the definition of BLI_stat_t in BLI_fileops.h */
#  if defined(_MSC_VER)
  struct _stat64 s;
#  else
  struct _stat s;
#  endif
#else
  struct stat s;
#endif
};

struct dirlink {
  struct dirlink *next, *prev;
  char *name;
};

#ifdef __cplusplus
}
#endif
