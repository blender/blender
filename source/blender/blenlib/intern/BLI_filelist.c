/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#ifndef WIN32
#  include <dirent.h>
#endif

#include <string.h>
#include <sys/stat.h>
#include <time.h>

#ifdef WIN32
#  include "BLI_winstuff.h"
#  include "utfconv.h"
#  include <direct.h>
#  include <io.h>
#else
#  include <pwd.h>
#  include <sys/ioctl.h>
#  include <unistd.h>
#endif

/* lib includes */
#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"

#include "BLI_fileops.h"
#include "BLI_fileops_types.h"
#include "BLI_listbase.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_string_utils.h"

#include "../imbuf/IMB_imbuf.h"

/*
 * Ordering function for sorting lists of files/directories. Returns -1 if
 * entry1 belongs before entry2, 0 if they are equal, 1 if they should be swapped.
 */
static int direntry_cmp(struct direntry *entry1, struct direntry *entry2)
{
  /* type is equal to stat.st_mode */

  /* directories come before non-directories */
  if (S_ISDIR(entry1->type)) {
    if (S_ISDIR(entry2->type) == 0) {
      return -1;
    }
  }
  else {
    if (S_ISDIR(entry2->type)) {
      return 1;
    }
  }
  /* non-regular files come after regular files */
  if (S_ISREG(entry1->type)) {
    if (S_ISREG(entry2->type) == 0) {
      return -1;
    }
  }
  else {
    if (S_ISREG(entry2->type)) {
      return 1;
    }
  }
  /* arbitrary, but consistent, ordering of different types of non-regular files */
  if ((entry1->type & S_IFMT) < (entry2->type & S_IFMT)) {
    return -1;
  }
  if ((entry1->type & S_IFMT) > (entry2->type & S_IFMT)) {
    return 1;
  }

  /* OK, now we know their S_IFMT fields are the same, go on to a name comparison */
  /* make sure "." and ".." are always first */
  if (FILENAME_IS_CURRENT(entry1->relname)) {
    return -1;
  }
  if (FILENAME_IS_CURRENT(entry2->relname)) {
    return 1;
  }
  if (FILENAME_IS_PARENT(entry1->relname)) {
    return -1;
  }
  if (FILENAME_IS_PARENT(entry2->relname)) {
    return 1;
  }

  return BLI_strcasecmp_natural(entry1->relname, entry2->relname);
}

struct BuildDirCtx {
  struct direntry *files; /* array[files_num] */
  int files_num;
};

/**
 * Scans the directory named *dirname and appends entries for its contents to files.
 */
static void bli_builddir(struct BuildDirCtx *dir_ctx, const char *dirname)
{
  DIR *dir = opendir(dirname);
  if (UNLIKELY(dir == NULL)) {
    fprintf(stderr,
            "Failed to open dir (%s): %s\n",
            errno ? strerror(errno) : "unknown error",
            dirname);
    return;
  }

  ListBase dirbase = {NULL, NULL};
  int newnum = 0;
  const struct dirent *fname;
  bool has_current = false, has_parent = false;

  char dirname_with_slash[FILE_MAXDIR + 1];
  size_t dirname_with_slash_len = BLI_strncpy_rlen(
      dirname_with_slash, dirname, sizeof(dirname_with_slash) - 1);

  if ((dirname_with_slash_len > 0) &&
      (BLI_path_slash_is_native_compat(dirname[dirname_with_slash_len - 1]) == false))
  {
    dirname_with_slash[dirname_with_slash_len++] = SEP;
    dirname_with_slash[dirname_with_slash_len] = '\0';
  }

  while ((fname = readdir(dir)) != NULL) {
    struct dirlink *const dlink = (struct dirlink *)malloc(sizeof(struct dirlink));
    if (dlink != NULL) {
      dlink->name = BLI_strdup(fname->d_name);
      if (FILENAME_IS_PARENT(dlink->name)) {
        has_parent = true;
      }
      else if (FILENAME_IS_CURRENT(dlink->name)) {
        has_current = true;
      }
      BLI_addhead(&dirbase, dlink);
      newnum++;
    }
  }

  if (!has_parent) {
    char pardir[FILE_MAXDIR];

    STRNCPY(pardir, dirname);
    if (BLI_path_parent_dir(pardir) && (BLI_access(pardir, R_OK) == 0)) {
      struct dirlink *const dlink = (struct dirlink *)malloc(sizeof(struct dirlink));
      if (dlink != NULL) {
        dlink->name = BLI_strdup(FILENAME_PARENT);
        BLI_addhead(&dirbase, dlink);
        newnum++;
      }
    }
  }
  if (!has_current) {
    struct dirlink *const dlink = (struct dirlink *)malloc(sizeof(struct dirlink));
    if (dlink != NULL) {
      dlink->name = BLI_strdup(FILENAME_CURRENT);
      BLI_addhead(&dirbase, dlink);
      newnum++;
    }
  }

  if (newnum) {
    if (dir_ctx->files) {
      void *const tmp = MEM_reallocN(dir_ctx->files,
                                     (dir_ctx->files_num + newnum) * sizeof(struct direntry));
      if (tmp) {
        dir_ctx->files = (struct direntry *)tmp;
      }
      else { /* Reallocation may fail. */
        MEM_freeN(dir_ctx->files);
        dir_ctx->files = NULL;
      }
    }

    if (dir_ctx->files == NULL) {
      dir_ctx->files = (struct direntry *)MEM_mallocN(newnum * sizeof(struct direntry), __func__);
    }

    if (UNLIKELY(dir_ctx->files == NULL)) {
      fprintf(stderr, "Couldn't get memory for dir: %s\n", dirname);
      dir_ctx->files_num = 0;
    }
    else {
      struct dirlink *dlink = (struct dirlink *)dirbase.first;
      struct direntry *file = &dir_ctx->files[dir_ctx->files_num];

      while (dlink) {
        memset(file, 0, sizeof(struct direntry));
        file->relname = dlink->name;
        file->path = BLI_string_joinN(dirname_with_slash, dlink->name);
        if (BLI_stat(file->path, &file->s) != -1) {
          file->type = file->s.st_mode;
        }
        else if (FILENAME_IS_CURRPAR(file->relname)) {
          /* Unfortunately a hack around UNC paths on WIN32,
           * which does not support `stat` on `\\SERVER\foo\..`. */
          file->type |= S_IFDIR;
        }
        dir_ctx->files_num++;
        file++;
        dlink = dlink->next;
      }

      qsort(dir_ctx->files,
            dir_ctx->files_num,
            sizeof(struct direntry),
            (int (*)(const void *, const void *))direntry_cmp);
    }

    BLI_freelist(&dirbase);
  }

  closedir(dir);
}

uint BLI_filelist_dir_contents(const char *dirname, struct direntry **r_filelist)
{
  struct BuildDirCtx dir_ctx;

  dir_ctx.files_num = 0;
  dir_ctx.files = NULL;

  bli_builddir(&dir_ctx, dirname);

  if (dir_ctx.files) {
    *r_filelist = dir_ctx.files;
  }
  else {
    /* Keep Blender happy. Blender stores this in a variable
     * where 0 has special meaning..... */
    *r_filelist = MEM_mallocN(sizeof(**r_filelist), __func__);
  }

  return dir_ctx.files_num;
}

void BLI_filelist_entry_size_to_string(const struct stat *st,
                                       const uint64_t st_size_fallback,
                                       /* Used to change MB -> M, etc. - is that really useful? */
                                       const bool UNUSED(compact),
                                       char r_size[FILELIST_DIRENTRY_SIZE_LEN])
{
  /*
   * Seems st_size is signed 32-bit value in *nix and Windows.  This
   * will buy us some time until files get bigger than 4GB or until
   * everyone starts using __USE_FILE_OFFSET64 or equivalent.
   */
  double size = (double)(st ? st->st_size : st_size_fallback);
#ifdef WIN32
  BLI_str_format_byte_unit(r_size, size, false);
#else
  BLI_str_format_byte_unit(r_size, size, true);
#endif
}

void BLI_filelist_entry_mode_to_string(const struct stat *st,
                                       const bool UNUSED(compact),
                                       char r_mode1[FILELIST_DIRENTRY_MODE_LEN],
                                       char r_mode2[FILELIST_DIRENTRY_MODE_LEN],
                                       char r_mode3[FILELIST_DIRENTRY_MODE_LEN])
{
  const char *types[8] = {"---", "--x", "-w-", "-wx", "r--", "r-x", "rw-", "rwx"};

#ifdef WIN32
  UNUSED_VARS(st);
  BLI_strncpy(r_mode1, types[0], sizeof(*r_mode1) * FILELIST_DIRENTRY_MODE_LEN);
  BLI_strncpy(r_mode2, types[0], sizeof(*r_mode2) * FILELIST_DIRENTRY_MODE_LEN);
  BLI_strncpy(r_mode3, types[0], sizeof(*r_mode3) * FILELIST_DIRENTRY_MODE_LEN);
#else
  const int mode = st->st_mode;

  BLI_strncpy(r_mode1, types[(mode & 0700) >> 6], sizeof(*r_mode1) * FILELIST_DIRENTRY_MODE_LEN);
  BLI_strncpy(r_mode2, types[(mode & 0070) >> 3], sizeof(*r_mode2) * FILELIST_DIRENTRY_MODE_LEN);
  BLI_strncpy(r_mode3, types[(mode & 0007)], sizeof(*r_mode3) * FILELIST_DIRENTRY_MODE_LEN);

  if (((mode & S_ISGID) == S_ISGID) && (r_mode2[2] == '-')) {
    r_mode2[2] = 'l';
  }

  if (mode & (S_ISUID | S_ISGID)) {
    if (r_mode1[2] == 'x') {
      r_mode1[2] = 's';
    }
    else {
      r_mode1[2] = 'S';
    }

    if (r_mode2[2] == 'x') {
      r_mode2[2] = 's';
    }
  }

  if (mode & S_ISVTX) {
    if (r_mode3[2] == 'x') {
      r_mode3[2] = 't';
    }
    else {
      r_mode3[2] = 'T';
    }
  }
#endif
}

void BLI_filelist_entry_owner_to_string(const struct stat *st,
                                        const bool UNUSED(compact),
                                        char r_owner[FILELIST_DIRENTRY_OWNER_LEN])
{
#ifdef WIN32
  UNUSED_VARS(st);
  BLI_strncpy(r_owner, "unknown", FILELIST_DIRENTRY_OWNER_LEN);
#else
  struct passwd *pwuser = getpwuid(st->st_uid);

  if (pwuser) {
    BLI_strncpy(r_owner, pwuser->pw_name, sizeof(*r_owner) * FILELIST_DIRENTRY_OWNER_LEN);
  }
  else {
    BLI_snprintf(r_owner, sizeof(*r_owner) * FILELIST_DIRENTRY_OWNER_LEN, "%u", st->st_uid);
  }
#endif
}

void BLI_filelist_entry_datetime_to_string(const struct stat *st,
                                           const int64_t ts,
                                           const bool compact,
                                           char r_time[FILELIST_DIRENTRY_TIME_LEN],
                                           char r_date[FILELIST_DIRENTRY_DATE_LEN],
                                           bool *r_is_today,
                                           bool *r_is_yesterday)
{
  int today_year = 0;
  int today_yday = 0;
  int yesterday_year = 0;
  int yesterday_yday = 0;

  if (r_is_today || r_is_yesterday) {
    /* `localtime()` has only one buffer so need to get data out before called again. */
    const time_t ts_now = time(NULL);
    struct tm *today = localtime(&ts_now);

    today_year = today->tm_year;
    today_yday = today->tm_yday;
    /* Handle a yesterday that spans a year */
    today->tm_mday--;
    mktime(today);
    yesterday_year = today->tm_year;
    yesterday_yday = today->tm_yday;

    if (r_is_today) {
      *r_is_today = false;
    }
    if (r_is_yesterday) {
      *r_is_yesterday = false;
    }
  }

  const time_t ts_mtime = ts;
  const struct tm *tm = localtime(st ? &st->st_mtime : &ts_mtime);
  const time_t zero = 0;

  /* Prevent impossible dates in windows. */
  if (tm == NULL) {
    tm = localtime(&zero);
  }

  if (r_time) {
    strftime(r_time, sizeof(*r_time) * FILELIST_DIRENTRY_TIME_LEN, "%H:%M", tm);
  }

  if (r_date) {
    strftime(r_date,
             sizeof(*r_date) * FILELIST_DIRENTRY_DATE_LEN,
             compact ? "%d/%m/%y" : "%d %b %Y",
             tm);
  }

  if (r_is_today && (tm->tm_year == today_year) && (tm->tm_yday == today_yday)) {
    *r_is_today = true;
  }
  else if (r_is_yesterday && (tm->tm_year == yesterday_year) && (tm->tm_yday == yesterday_yday)) {
    *r_is_yesterday = true;
  }
}

void BLI_filelist_entry_duplicate(struct direntry *dst, const struct direntry *src)
{
  *dst = *src;
  if (dst->relname) {
    dst->relname = MEM_dupallocN(src->relname);
  }
  if (dst->path) {
    dst->path = MEM_dupallocN(src->path);
  }
}

void BLI_filelist_duplicate(struct direntry **dest_filelist,
                            struct direntry *const src_filelist,
                            const uint nrentries)
{
  uint i;

  *dest_filelist = MEM_mallocN(sizeof(**dest_filelist) * (size_t)(nrentries), __func__);
  for (i = 0; i < nrentries; i++) {
    struct direntry *const src = &src_filelist[i];
    struct direntry *dst = &(*dest_filelist)[i];
    BLI_filelist_entry_duplicate(dst, src);
  }
}

void BLI_filelist_entry_free(struct direntry *entry)
{
  if (entry->relname) {
    MEM_freeN((void *)entry->relname);
  }
  if (entry->path) {
    MEM_freeN((void *)entry->path);
  }
}

void BLI_filelist_free(struct direntry *filelist, const uint nrentries)
{
  uint i;
  for (i = 0; i < nrentries; i++) {
    BLI_filelist_entry_free(&filelist[i]);
  }

  if (filelist != NULL) {
    MEM_freeN(filelist);
  }
}
