/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 * Various string, file, list operations.
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "DNA_listBase.h"

#include "BLI_fileops.h"
#include "BLI_fnmatch.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.h"
#include "BLI_utildefines.h"

#ifdef WIN32
#  include "utf_winfunc.h"
#  include "utfconv.h"
#  include <io.h>
#  ifdef _WIN32_IE
#    undef _WIN32_IE
#  endif
#  define _WIN32_IE 0x0501
#  include "BLI_alloca.h"
#  include "BLI_winstuff.h"
#  include <shlobj.h>
#  include <windows.h>
#else
#  include <unistd.h>
#endif /* WIN32 */

#include "MEM_guardedalloc.h"

/* Declarations. */

static int BLI_path_unc_prefix_len(const char *path);

#ifdef WIN32
static bool BLI_path_is_abs_win32(const char *path);
static int BLI_path_win32_prefix_len(const char *path);
#endif /* WIN32 */

/**
 * The maximum number of `#` characters expanded for #BLI_path_frame & #BLI_path_frame_range
 * Typically 12 is enough and even 16 is very large.
 * Use a much larger value so hitting the upper limit is not an issue.
 * Exceeding this limit won't fail either, it will just not insert so many leading zeros.
 */
#define FILENAME_FRAME_CHARS_MAX FILE_MAX

int BLI_path_sequence_decode(const char *path,
                             char *head,
                             const size_t head_maxncpy,
                             char *tail,
                             const size_t tail_maxncpy,
                             ushort *r_digits_len)
{
  if (head) {
    BLI_string_debug_size(head, head_maxncpy);
  }
  if (tail) {
    BLI_string_debug_size(tail, tail_maxncpy);
  }

  uint nums = 0, nume = 0;
  int i;
  bool found_digit = false;
  const char *const lslash = BLI_path_slash_rfind(path);
  const char *const extension = BLI_path_extension_or_end(lslash ? lslash : path);
  const uint lslash_len = lslash != NULL ? (int)(lslash - path) : 0;
  const uint name_end = (uint)(extension - path);

  for (i = name_end - 1; i >= (int)lslash_len; i--) {
    if (isdigit(path[i])) {
      if (found_digit) {
        nums = i;
      }
      else {
        nume = i;
        nums = i;
        found_digit = true;
      }
    }
    else {
      if (found_digit) {
        break;
      }
    }
  }

  if (found_digit) {
    const long long int ret = strtoll(&(path[nums]), NULL, 10);
    if (ret >= INT_MIN && ret <= INT_MAX) {
      if (tail) {
        BLI_strncpy(tail, &path[nume + 1], tail_maxncpy);
      }
      if (head) {
        BLI_strncpy(head, path, MIN2(head_maxncpy, nums + 1));
      }
      if (r_digits_len) {
        *r_digits_len = nume - nums + 1;
      }
      return (int)ret;
    }
  }

  if (tail) {
    BLI_strncpy(tail, path + name_end, tail_maxncpy);
  }
  if (head) {
    /* Name_end points to last character of head,
     * make it +1 so null-terminator is nicely placed. */
    BLI_strncpy(head, path, MIN2(head_maxncpy, name_end + 1));
  }
  if (r_digits_len) {
    *r_digits_len = 0;
  }
  return 0;
}

void BLI_path_sequence_encode(char *path,
                              const size_t path_maxncpy,
                              const char *head,
                              const char *tail,
                              ushort numlen,
                              int pic)
{
  BLI_string_debug_size(path, path_maxncpy);

  BLI_snprintf(path, path_maxncpy, "%s%.*d%s", head, numlen, MAX2(0, pic), tail);
}

/**
 * Implementation for #BLI_path_normalize & #BLI_path_normalize_native.
 * \return The path length.
 */
static int path_normalize_impl(char *path, bool check_blend_relative_prefix)
{
  const char *path_orig = path;
  int path_len = strlen(path);

  /*
   * Skip absolute prefix.
   * ---------------------
   */
  if (check_blend_relative_prefix && (path[0] == '/' && path[1] == '/')) {
    path = path + 2; /* Leave the initial `//` untouched. */
    path_len -= 2;

    /* Strip leading slashes, as they will interfere with the absolute/relative check
     * (besides being redundant). */
    int i = 0;
    while (path[i] == SEP) {
      i++;
    }

    if (i != 0) {
      memmove(path, path + i, (path_len - i) + 1);
      path_len -= i;
    }
    BLI_assert(path_len == strlen(path));
  }

#ifdef WIN32
  /* Skip to the first slash of the drive or UNC path,
   * so additional slashes are treated as doubles. */
  if (path_orig == path) {
    int path_unc_len = BLI_path_unc_prefix_len(path);
    if (path_unc_len) {
      path_unc_len -= 1;
      BLI_assert(path_unc_len > 0 && path[path_unc_len] == SEP);
      path += path_unc_len;
      path_len -= path_unc_len;
    }
    else if (BLI_path_is_win32_drive(path)) { /* Check for `C:` (2 characters only). */
      path += 2;
      path_len -= 2;
    }
  }
#endif /* WIN32 */
  /* Works on WIN32 as well, because the drive component is skipped. */
  const bool is_relative = path[0] && (path[0] != SEP);

  /*
   * Strip redundant path components.
   * --------------------------------
   */

  /* NOTE(@ideasman42):
   *   `memmove(start, eind, strlen(eind) + 1);`
   * is the same as
   *   `BLI_strncpy(start, eind, ...);`
   * except string-copy should not be used because there is overlap,
   * so use `memmove` 's slightly more obscure syntax. */

  /* Inline replacement:
   * - `/./` -> `/`.
   * - `//` -> `/`.
   * Performed until no more replacements can be made. */
  if (path_len > 1) {
    for (int i = path_len - 1; i > 0; i--) {
      /* Calculate the redundant slash span (if any). */
      if (path[i] == SEP) {
        const int i_end = i;
        do {
          /* Stepping over elements assumes 'i' references a separator. */
          BLI_assert(path[i] == SEP);
          if (path[i - 1] == SEP) {
            i -= 1; /* Found `//`, replace with `/`. */
          }
          else if (i >= 2 && path[i - 1] == '.' && path[i - 2] == SEP) {
            i -= 2; /* Found `/./`, replace with `/`. */
          }
          else {
            break;
          }
        } while (i > 0);

        if (i < i_end) {
          memmove(path + i, path + i_end, (path_len - i_end) + 1);
          path_len -= i_end - i;
          BLI_assert(strlen(path) == path_len);
        }
      }
    }
  }
  /* Remove redundant `./` prefix as it's redundant & complicates collapsing directories. */
  if (is_relative) {
    if ((path_len > 2) && (path[0] == '.') && (path[1] == SEP)) {
      memmove(path, path + 2, (path_len - 2) + 1);
      path_len -= 2;
    }
  }

  /*
   * Collapse Parent Directories.
   * ----------------------------
   *
   * Example: `<parent>/<child>/../` -> `<parent>/`
   *
   * Notes:
   * - Leading `../` are skipped as they cannot be collapsed (see `start_base`).
   * - Multiple parent directories are handled at once to reduce number of `memmove` calls.
   */

#define IS_PARENT_DIR(p) ((p)[0] == '.' && (p)[1] == '.' && ELEM((p)[2], SEP, '\0'))

  /* First non prefix path component. */
  char *path_first_non_slash_part = path;
  while (*path_first_non_slash_part && *path_first_non_slash_part == SEP) {
    path_first_non_slash_part++;
  }

  /* Maintain a pointer to the end of leading `..` component.
   * Skip leading parent directories because logically they cannot be collapsed. */
  char *start_base = path_first_non_slash_part;
  while (IS_PARENT_DIR(start_base)) {
    start_base += 3;
  }

  /* It's possible the entire path is made of up `../`,
   * in this case there is nothing to do. */
  if (start_base < path + path_len) {
    /* Step over directories, always starting out on the character after the slash. */
    char *start = start_base;
    char *start_temp;
    while ((start_temp = strstr(start, SEP_STR ".." SEP_STR)) ||
           /* Check if the string ends with `/..` & assign when found, else NULL. */
           (start_temp = ((start <= &path[path_len - 3]) &&
                          STREQ(&path[path_len - 3], SEP_STR "..")) ?
                             &path[path_len - 3] :
                             NULL))
    {
      start = start_temp + 1; /* Skip the `/`. */
      BLI_assert(start_base != start);

      /* Step `end_all` forwards (over all `..`). */
      char *end_all = start;
      do {
        BLI_assert(IS_PARENT_DIR(end_all));
        end_all += 3;
        BLI_assert(end_all <= path + path_len + 1);
      } while (IS_PARENT_DIR(end_all));

      /* Step `start` backwards (until `end` meets `end_all` or `start` meets `start_base`). */
      char *end = start;
      do {
        BLI_assert(start_base < start);
        BLI_assert(*(start - 1) == SEP);
        /* Step `start` backwards one. */
        do {
          start--;
        } while (start_base < start && *(start - 1) != SEP);
        BLI_assert(*start != SEP);         /* Ensure the loop ran at least once. */
        BLI_assert(!IS_PARENT_DIR(start)); /* Clamping by `start_base` prevents this. */
        end += 3;
      } while ((start != start_base) && (end < end_all));

      if (end > path + path_len) {
        BLI_assert(*(end - 1) == '\0');
        end--;
        end_all--;
      }
      BLI_assert(start < end && start >= start_base);
      const size_t start_len = path_len - (end - path);
      memmove(start, end, start_len + 1);
      path_len -= end - start;
      BLI_assert(strlen(path) == path_len);
      /* Other `..` directories may have been moved to the front, step `start_base` past them. */
      if (UNLIKELY(start == start_base && (end != end_all))) {
        start_base += (end_all - end);
        start = (start_base < path + path_len) ? start_base : start_base - 1;
      }
    }
  }

  BLI_assert(strlen(path) == path_len);
  /* Characters before the `start_base` must *only* be `../../../` (multiples of 3). */
  BLI_assert((start_base - path_first_non_slash_part) % 3 == 0);
  /* All `..` ahead of `start_base` were collapsed (including trailing `/..`). */
  BLI_assert(!(start_base < path + path_len) ||
             (!strstr(start_base, SEP_STR ".." SEP_STR) &&
              !(path_len >= 3 && STREQ(&path[path_len - 3], SEP_STR ".."))));

  /*
   * Final Prefix Cleanup.
   * ---------------------
   */
  if (is_relative) {
    if (path_len == 0 && (path == path_orig)) {
      path[0] = '.';
      path[1] = '\0';
      path_len = 1;
    }
  }
  else {
    /* Support for odd paths: eg `/../home/me` --> `/home/me`
     * this is a valid path in blender but we can't handle this the usual way below
     * simply strip this prefix then evaluate the path as usual.
     * Python's `os.path.normpath()` does this. */
    if (start_base != path_first_non_slash_part) {
      char *start = start_base > path + path_len ? start_base - 1 : start_base;
      /* As long as `start` is set correctly, it should never begin with `../`
       * as these directories are expected to be skipped. */
      BLI_assert(!IS_PARENT_DIR(start));
      const size_t start_len = path_len - (start - path);
      BLI_assert(strlen(start) == start_len);
      memmove(path_first_non_slash_part, start, start_len + 1);
      path_len -= start - path_first_non_slash_part;
      BLI_assert(strlen(path) == path_len);
    }
  }

  BLI_assert(strlen(path) == path_len);

#undef IS_PARENT_DIR

  return (path - path_orig) + path_len;
}

int BLI_path_normalize(char *path)
{
  return path_normalize_impl(path, true);
}

int BLI_path_normalize_native(char *path)
{
  return path_normalize_impl(path, false);
}

int BLI_path_normalize_dir(char *dir, size_t dir_maxncpy)
{
  /* Would just create an unexpected "/" path, just early exit entirely. */
  if (dir[0] == '\0') {
    return 0;
  }

  int dir_len = BLI_path_normalize(dir);
  return BLI_path_slash_ensure_ex(dir, dir_maxncpy, dir_len);
}

int BLI_path_canonicalize_native(char *path, int path_maxncpy)
{
  BLI_path_abs_from_cwd(path, path_maxncpy);
  /* As these are system level paths, only convert slashes
   * if the alternate direction is accepted as a slash. */
  if (BLI_path_slash_is_native_compat(ALTSEP)) {
    BLI_path_slash_native(path);
  }
  int path_len = BLI_path_normalize_native(path);
  /* Strip trailing slash but don't strip `/` away to nothing. */
  if (path_len > 1 && path[path_len - 1] == SEP) {
#ifdef WIN32
    /* Don't strip `C:\` -> `C:` as this is no longer a valid directory. */
    if (BLI_path_win32_prefix_len(path) + 1 < path_len)
#endif
    {
      path_len -= 1;
      path[path_len] = '\0';
    }
  }
  return path_len;
}

bool BLI_path_make_safe_filename_ex(char *filename, bool allow_tokens)
{
#define INVALID_CHARS \
  "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f" \
  "\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f" \
  "/\\?*:|\""
#define INVALID_TOKENS "<>"

  const char *invalid = allow_tokens ? INVALID_CHARS : INVALID_CHARS INVALID_TOKENS;

#undef INVALID_CHARS
#undef INVALID_TOKENS

  char *fn;
  bool changed = false;

  if (*filename == '\0') {
    return changed;
  }

  for (fn = filename; *fn && (fn = strpbrk(fn, invalid)); fn++) {
    *fn = '_';
    changed = true;
  }

  /* Forbid only dots. */
  for (fn = filename; *fn == '.'; fn++) {
    /* Pass. */
  }
  if (*fn == '\0') {
    *filename = '_';
    changed = true;
  }

#ifdef WIN32
  {
    const char *invalid_names[] = {
        "con",  "prn",  "aux",  "null", "com1", "com2", "com3", "com4",
        "com5", "com6", "com7", "com8", "com9", "lpt1", "lpt2", "lpt3",
        "lpt4", "lpt5", "lpt6", "lpt7", "lpt8", "lpt9", NULL,
    };
    const size_t len = strlen(filename);
    char *filename_lower = BLI_strdupn(filename, len);
    const char **iname;

    /* Forbid trailing dot (trailing space has already been replaced above). */
    if (filename[len - 1] == '.') {
      filename[len - 1] = '_';
      changed = true;
    }

    /* Check for forbidden names - not we have to check all combination
     * of upper and lower cases, hence the usage of filename_lower
     * (more efficient than using #BLI_strcasestr repeatedly). */
    BLI_str_tolower_ascii(filename_lower, len);
    for (iname = invalid_names; *iname; iname++) {
      if (strstr(filename_lower, *iname) == filename_lower) {
        const size_t iname_len = strlen(*iname);
        /* Only invalid if the whole name is made of the invalid chunk, or it has an
         * (assumed extension) dot just after. This means it will also catch *valid*
         * names like `aux.foo.bar`, but should be good enough for us! */
        if ((iname_len == len) || (filename_lower[iname_len] == '.')) {
          *filename = '_';
          changed = true;
          break;
        }
      }
    }

    MEM_freeN(filename_lower);
  }
#endif

  return changed;
}

bool BLI_path_make_safe_filename(char *filename)
{
  return BLI_path_make_safe_filename_ex(filename, false);
}

bool BLI_path_make_safe(char *path)
{
  /* Simply apply #BLI_path_make_safe_filename() over each component of the path.
   * Luckily enough, same *safe* rules applies to file & directory names. */
  char *curr_slash, *curr_path = path;
  bool changed = false;
  bool skip_first = false;

#ifdef WIN32
  if (BLI_path_is_abs_win32(path)) {
    /* Do not make safe `C:` in `C:\foo\bar`. */
    skip_first = true;
  }
#endif

  for (curr_slash = (char *)BLI_path_slash_find(curr_path); curr_slash;
       curr_slash = (char *)BLI_path_slash_find(curr_path))
  {
    const char backup = *curr_slash;
    *curr_slash = '\0';
    if (!skip_first && (*curr_path != '\0') && BLI_path_make_safe_filename(curr_path)) {
      changed = true;
    }
    skip_first = false;
    curr_path = curr_slash + 1;
    *curr_slash = backup;
  }
  if (BLI_path_make_safe_filename(curr_path)) {
    changed = true;
  }

  return changed;
}

bool BLI_path_is_rel(const char *path)
{
  return path[0] == '/' && path[1] == '/';
}

bool BLI_path_is_unc(const char *path)
{
  return path[0] == '\\' && path[1] == '\\';
}

/**
 * Returns the length of the identifying prefix
 * of a UNC path which can start with '\\' (short version)
 * or '\\?\' (long version)
 * If the path is not a UNC path, return 0
 */
static int BLI_path_unc_prefix_len(const char *path)
{
  if (BLI_path_is_unc(path)) {
    if ((path[2] == '?') && (path[3] == '\\')) {
      /* We assume long UNC path like `\\?\server\share\folder` etc. */
      return 4;
    }

    return 2;
  }

  return 0;
}

#ifdef WIN32
static int BLI_path_win32_prefix_len(const char *path)
{
  if (BLI_path_is_win32_drive(path)) {
    return 2;
  }
  return BLI_path_unc_prefix_len(path);
}
#endif

bool BLI_path_is_win32_drive(const char *path)
{
  return isalpha(path[0]) && (path[1] == ':');
}

bool BLI_path_is_win32_drive_only(const char *path)
{
  return isalpha(path[0]) && (path[1] == ':') && (path[2] == '\0');
}

bool BLI_path_is_win32_drive_with_slash(const char *path)
{
  return isalpha(path[0]) && (path[1] == ':') && ELEM(path[2], '\\', '/');
}

#if defined(WIN32)

/**
 * Return true if the path is an absolute path on a WIN32 file-system, it either:
 * - Starts with a drive specifier* (eg `A:\`).
 * - Is a UNC path.
 *
 * \note Not to be confused with the opposite of #BLI_path_is_rel which checks for the
 * Blender specific convention of using `//` prefix for blend-file relative paths.
 */
static bool BLI_path_is_abs_win32(const char *path)
{
  return BLI_path_is_win32_drive_with_slash(path) || BLI_path_is_unc(path);
}

static wchar_t *next_slash(wchar_t *path)
{
  wchar_t *slash = path;
  while (*slash && *slash != L'\\') {
    slash++;
  }
  return slash;
}

/* Adds a slash if the UNC path points to a share. */
static void BLI_path_add_slash_to_share(wchar_t *uncpath)
{
  wchar_t *slash_after_server = next_slash(uncpath + 2);
  if (*slash_after_server) {
    wchar_t *slash_after_share = next_slash(slash_after_server + 1);
    if (!(*slash_after_share)) {
      slash_after_share[0] = L'\\';
      slash_after_share[1] = L'\0';
    }
  }
}

static void BLI_path_unc_to_short(wchar_t *unc)
{
  wchar_t tmp[PATH_MAX];

  int len = wcslen(unc);
  /* Convert:
   * - `\\?\UNC\server\share\folder\...` to `\\server\share\folder\...`
   * - `\\?\C:\` to `C:\`
   * - `\\?\C:\folder\...` to `C:\folder\...`
   */
  if ((len > 3) && (unc[0] == L'\\') && (unc[1] == L'\\') && (unc[2] == L'?') &&
      ELEM(unc[3], L'\\', L'/'))
  {
    if ((len > 5) && (unc[5] == L':')) {
      wcsncpy(tmp, unc + 4, len - 4);
      tmp[len - 4] = L'\0';
      wcscpy(unc, tmp);
    }
    else if ((len > 7) && (wcsncmp(&unc[4], L"UNC", 3) == 0) && ELEM(unc[7], L'\\', L'/')) {
      tmp[0] = L'\\';
      tmp[1] = L'\\';
      wcsncpy(tmp + 2, unc + 8, len - 8);
      tmp[len - 6] = L'\0';
      wcscpy(unc, tmp);
    }
  }
}

void BLI_path_normalize_unc(char *path, int path_maxncpy)
{
  wchar_t *tmp_16 = alloc_utf16_from_8(path, 1);
  BLI_path_normalize_unc_16(tmp_16);
  conv_utf_16_to_8(tmp_16, path, path_maxncpy);
}

void BLI_path_normalize_unc_16(wchar_t *path_16)
{
  BLI_path_unc_to_short(path_16);
  BLI_path_add_slash_to_share(path_16);
}
#endif

void BLI_path_rel(char path[FILE_MAX], const char *basepath)
{
  BLI_string_debug_size_after_nil(path, FILE_MAX);
  /* A `basepath` starting with `//` will be made relative multiple times. */
  BLI_assert_msg(!BLI_path_is_rel(basepath), "The 'basepath' cannot start with '//'!");

  const char *lslash;
  char temp[FILE_MAX];

  /* If path is already relative, bail out. */
  if (BLI_path_is_rel(path)) {
    return;
  }

  /* Also bail out if relative path is not set. */
  if (basepath[0] == '\0') {
    return;
  }

#ifdef WIN32
  if (BLI_strnlen(basepath, 3) > 2 && !BLI_path_is_abs_win32(basepath)) {
    char *ptemp;
    /* Fix missing volume name in relative base,
     * can happen with old `recent-files.txt` files. */
    BLI_windows_get_default_root_dir(temp);
    ptemp = &temp[2];
    if (!ELEM(basepath[0], '\\', '/')) {
      ptemp++;
    }
    BLI_strncpy(ptemp, basepath, FILE_MAX - 3);
  }
  else {
    BLI_strncpy(temp, basepath, FILE_MAX);
  }

  if (BLI_strnlen(path, 3) > 2) {
    bool is_unc = BLI_path_is_unc(path);

    /* Ensure paths are both UNC paths or are both drives. */
    if (BLI_path_is_unc(temp) != is_unc) {
      return;
    }

    /* Ensure both UNC paths are on the same share. */
    if (is_unc) {
      int off;
      int slash = 0;
      for (off = 0; temp[off] && slash < 4; off++) {
        if (temp[off] != path[off]) {
          return;
        }

        if (temp[off] == '\\') {
          slash++;
        }
      }
    }
    else if ((temp[1] == ':' && path[1] == ':') && (tolower(temp[0]) != tolower(path[0]))) {
      return;
    }
  }
#else
  STRNCPY(temp, basepath);
#endif

  BLI_string_replace_char(temp + BLI_path_unc_prefix_len(temp), '\\', '/');
  BLI_string_replace_char(path + BLI_path_unc_prefix_len(path), '\\', '/');

  /* Remove `/./` which confuse the following slash counting. */
  BLI_path_normalize(path);
  BLI_path_normalize(temp);

  /* The last slash in the path indicates where the path part ends. */
  lslash = BLI_path_slash_rfind(temp);

  if (lslash) {
    /* Find the prefix of the filename that is equal for both filenames.
     * This is replaced by the two slashes at the beginning. */
    const char *p = temp;
    const char *q = path;

#ifdef WIN32
    while (tolower(*p) == tolower(*q))
#else
    while (*p == *q)
#endif
    {
      p++;
      q++;

      /* Don't search beyond the end of the string in the rare case they match. */
      if ((*p == '\0') || (*q == '\0')) {
        break;
      }
    }

    /* We might have passed the slash when the beginning of a dir matches
     * so we rewind. Only check on the actual filename. */
    if (*q != '/') {
      while ((q >= path) && (*q != '/')) {
        q--;
        p--;
      }
    }
    else if (*p != '/') {
      while ((p >= temp) && (*p != '/')) {
        p--;
        q--;
      }
    }

    char res[FILE_MAX] = "//";
    char *r = res + 2;

    /* `p` now points to the slash that is at the beginning of the part
     * where the path is different from the relative path.
     * We count the number of directories we need to go up in the
     * hierarchy to arrive at the common prefix of the path. */
    if (p < temp) {
      p = temp;
    }
    while (p && p < lslash) {
      if (*p == '/') {
        r += BLI_strncpy_rlen(r, "../", sizeof(res) - (r - res));
      }
      p++;
    }

    /* Don't copy the slash at the beginning. */
    r += BLI_strncpy_rlen(r, q + 1, sizeof(res) - (r - res));

#ifdef WIN32
    BLI_string_replace_char(res + 2, '/', '\\');
#endif
    BLI_strncpy(path, res, FILE_MAX);
  }
}

bool BLI_path_suffix(char *path, size_t path_maxncpy, const char *suffix, const char *sep)
{
  BLI_string_debug_size_after_nil(path, path_maxncpy);

  const size_t suffix_len = strlen(suffix);
  const size_t sep_len = strlen(sep);
  char *extension = (char *)BLI_path_extension_or_end(path);
  const size_t extension_len = strlen(extension);
  const size_t path_end = extension - path;
  const size_t path_len = path_end + extension_len;
  if (path_len + sep_len + suffix_len >= path_maxncpy) {
    return false;
  }

  if (extension_len) {
    memmove(extension + (sep_len + suffix_len), extension, extension_len);
  }
  char *c = path + path_end;
  if (sep_len) {
    memcpy(c, sep, sep_len);
    c += sep_len;
  }
  if (suffix_len) {
    memcpy(c, suffix, suffix_len);
    c += suffix_len;
  }
  c += extension_len;
  *c = '\0';
  return true;
}

const char *BLI_path_parent_dir_end(const char *path, size_t path_len)
{
  const char *path_end = path + path_len - 1;
  const char *p = path_end;
  while (p >= path) {
    if (BLI_path_slash_is_native_compat(*p)) {
      break;
    }
    p--;
  }
  while (p > path) {
    if (BLI_path_slash_is_native_compat(*(p - 1))) {
      p -= 1; /* Skip `/`. */
    }
    else if ((p + 1 > path) && (*(p - 1) == '.') && BLI_path_slash_is_native_compat(*p - 2)) {
      p -= 2; /* Skip `/.` (actually `/./` but the last slash was already skipped) */
    }
    else {
      break;
    }
  }
  if ((p > path) && (p != path_end)) {
    return p;
  }
  return NULL;
}

bool BLI_path_parent_dir(char *path)
{
  /* Use #BLI_path_name_at_index instead of checking if the strings ends with `parent_dir`
   * to ensure the logic isn't confused by:
   * - Directory names that happen to end with `..`.
   * - When `path` is empty, the contents will be `../`
   *   which would cause checking for a tailing `/../` fail.
   * Extracting the span of the final directory avoids both these issues. */
  int tail_ofs = 0, tail_len = 0;
  if (!BLI_path_name_at_index(path, -1, &tail_ofs, &tail_len)) {
    return false;
  }
  if (tail_len == 1) {
    /* Last path is `.`, as normalize should remove this, it's safe to assume failure.
     * This happens when the input a single period (possibly with slashes before or after). */
    if (path[tail_ofs] == '.') {
      return false;
    }
  }

  /* Input paths should already be normalized if `..` is part of the path. */
  BLI_assert(!((tail_len == 2) && (path[tail_ofs] == '.') && (path[tail_ofs + 1] == '.')));
  path[tail_ofs] = '\0';
  return true;
}

bool BLI_path_parent_dir_until_exists(char *dir)
{
  bool valid_path = true;

  /* Loop as long as cur path is not a dir, and we can get a parent path. */
  while ((BLI_access(dir, R_OK) != 0) && (valid_path = BLI_path_parent_dir(dir))) {
    /* Pass. */
  }
  return (valid_path && dir[0]);
}

/**
 * Looks for a sequence of "#" characters in the last slash-separated component of `path`,
 * returning the indexes of the first and one past the last character in the sequence in
 * `char_start` and `char_end` respectively.
 *
 * \param char_start: The first `#` character.
 * \param char_end: The last `#` character +1.
 *
 * \return true if a frame sequence range was found.
 */
static bool path_frame_chars_find_range(const char *path, int *char_start, int *char_end)
{
  uint ch_sta, ch_end, i;
  /* Insert current frame: `file###` -> `file001`. */
  ch_sta = ch_end = 0;
  for (i = 0; path[i] != '\0'; i++) {
    if (ELEM(path[i], '\\', '/')) {
      ch_end = 0; /* This is a directory name, don't use any hashes we found. */
    }
    else if (path[i] == '#') {
      ch_sta = i;
      ch_end = ch_sta + 1;
      while (path[ch_end] == '#') {
        ch_end++;
      }
      i = ch_end - 1; /* Keep searching. */

      /* Don't break, there may be a slash after this that invalidates the previous #'s. */
    }
  }

  if (ch_end) {
    *char_start = ch_sta;
    *char_end = ch_end;
    return true;
  }

  *char_start = -1;
  *char_end = -1;
  return false;
}

/**
 * Ensure `path` contains at least one "#" character in its last slash-separated
 * component, appending one digits long if not.
 */
static void ensure_digits(char *path, int digits)
{
  char *file = (char *)BLI_path_basename(path);
  if (strrchr(file, '#') == NULL) {
    int len = strlen(file);

    while (digits--) {
      file[len++] = '#';
    }
    file[len] = '\0';
  }
}

bool BLI_path_frame(char *path, size_t path_maxncpy, int frame, int digits)
{
  BLI_string_debug_size_after_nil(path, path_maxncpy);

  int ch_sta, ch_end;

  if (digits) {
    ensure_digits(path, digits);
  }

  if (path_frame_chars_find_range(path, &ch_sta, &ch_end)) {
    char frame_str[FILENAME_FRAME_CHARS_MAX + 1]; /* One for null. */
    const int ch_span = MIN2(ch_end - ch_sta, FILENAME_FRAME_CHARS_MAX);
    SNPRINTF(frame_str, "%.*d", ch_span, frame);
    BLI_string_replace_range(path, path_maxncpy, ch_sta, ch_end, frame_str);
    return true;
  }
  return false;
}

bool BLI_path_frame_range(char *path, size_t path_maxncpy, int sta, int end, int digits)
{
  BLI_string_debug_size_after_nil(path, path_maxncpy);

  int ch_sta, ch_end;

  if (digits) {
    ensure_digits(path, digits);
  }

  if (path_frame_chars_find_range(path, &ch_sta, &ch_end)) {
    char frame_str[(FILENAME_FRAME_CHARS_MAX * 2) + 1 + 1]; /* One for null, one for the '-' */
    const int ch_span = MIN2(ch_end - ch_sta, FILENAME_FRAME_CHARS_MAX);
    SNPRINTF(frame_str, "%.*d-%.*d", ch_span, sta, ch_span, end);
    BLI_string_replace_range(path, path_maxncpy, ch_sta, ch_end, frame_str);
    return true;
  }
  return false;
}

bool BLI_path_frame_get(const char *path, int *r_frame, int *r_digits_len)
{
  if (*path == '\0') {
    return false;
  }

  *r_digits_len = 0;

  const char *file = BLI_path_basename(path);
  const char *file_ext = BLI_path_extension_or_end(file);
  const char *c = file_ext;

  /* Find start of number (if there is one). */
  int digits_len = 0;
  while (c-- != file && isdigit(*c)) {
    digits_len++;
  }
  c++;

  if (digits_len == 0) {
    return false;
  }

  /* No need to trim the string, `atio` ignores non-digits. */
  *r_frame = atoi(c);
  *r_digits_len = digits_len;
  return true;
}

void BLI_path_frame_strip(char *path, char *r_ext, const size_t ext_maxncpy)
{
  BLI_string_debug_size(r_ext, ext_maxncpy);
  *r_ext = '\0';
  if (*path == '\0') {
    return;
  }

  char *file = (char *)BLI_path_basename(path);
  char *file_ext = (char *)BLI_path_extension_or_end(file);
  char *c = file_ext;

  /* Find start of number (if there is one). */
  int digits_len = 0;
  while (c-- != file && isdigit(*c)) {
    digits_len++;
  }
  c++;

  BLI_strncpy(r_ext, file_ext, ext_maxncpy);

  /* Replace the number with the suffix and terminate the string. */
  while (digits_len--) {
    *c++ = '#';
  }
  *c = '\0';
}

bool BLI_path_frame_check_chars(const char *path)
{
  int ch_sta_dummy, ch_end_dummy;
  return path_frame_chars_find_range(path, &ch_sta_dummy, &ch_end_dummy);
}

void BLI_path_to_display_name(char *display_name, int display_name_maxncpy, const char *name)
{
  BLI_string_debug_size(display_name, display_name_maxncpy);

  /* Strip leading underscores and spaces. */
  int strip_offset = 0;
  while (ELEM(name[strip_offset], '_', ' ')) {
    strip_offset++;
  }

  BLI_strncpy(display_name, name + strip_offset, display_name_maxncpy);

  /* Replace underscores with spaces. */
  BLI_string_replace_char(display_name, '_', ' ');

  BLI_path_extension_strip(display_name);

  /* Test if string has any upper case characters. */
  bool all_lower = true;
  for (int i = 0; display_name[i]; i++) {
    if (isupper(display_name[i])) {
      all_lower = false;
      break;
    }
  }

  if (all_lower) {
    /* For full lowercase string, use title case. */
    bool prevspace = true;
    for (int i = 0; display_name[i]; i++) {
      if (prevspace) {
        display_name[i] = toupper(display_name[i]);
      }

      prevspace = isspace(display_name[i]);
    }
  }
}

bool BLI_path_abs(char path[FILE_MAX], const char *basepath)
{
  BLI_string_debug_size_after_nil(path, FILE_MAX);
  /* A `basepath` starting with `//` will be made absolute multiple times. */
  BLI_assert_msg(!BLI_path_is_rel(basepath), "The 'basepath' cannot start with '//'!");

  const bool wasrelative = BLI_path_is_rel(path);
  char tmp[FILE_MAX];
  char base[FILE_MAX];
#ifdef WIN32

  /* Without this, an empty string converts to: `C:\` */
  if (*path == '\0') {
    return wasrelative;
  }

  /* We are checking here if we have an absolute path that is not in the current `.blend` file
   * as a lib main - we are basically checking for the case that a UNIX root `/` is passed. */
  if (!wasrelative && !BLI_path_is_abs_win32(path)) {
    const size_t root_dir_len = 3;
    char *p = path;
    BLI_windows_get_default_root_dir(tmp);
    BLI_assert(strlen(tmp) == root_dir_len);

    /* Step over the slashes at the beginning of the path. */
    p = (char *)BLI_path_slash_skip(p);
    BLI_strncpy(tmp + root_dir_len, p, sizeof(tmp) - root_dir_len);
  }
  else {
    STRNCPY(tmp, path);
  }
#else
  STRNCPY(tmp, path);

  /* Check for loading a MS-Windows path on a POSIX system
   * in this case, there is no use in trying `C:/` since it
   * will never exist on a Unix system.
   *
   * Add a `/` prefix and lowercase the drive-letter, remove the `:`.
   * `C:\foo.JPG` -> `/c/foo.JPG` */

  if (BLI_path_is_win32_drive_with_slash(tmp)) {
    tmp[1] = tolower(tmp[0]); /* Replace `:` with drive-letter. */
    tmp[0] = '/';
    /* `\` the slash will be converted later. */
  }

#endif

  /* NOTE(@jesterKing): push slashes into unix mode - strings entering this part are
   * potentially messed up: having both back- and forward slashes.
   * Here we push into one conform direction, and at the end we
   * push them into the system specific dir. This ensures uniformity
   * of paths and solving some problems (and prevent potential future ones).
   *
   * NOTE(@elubie): For UNC paths the first characters containing the UNC prefix
   * shouldn't be switched as we need to distinguish them from
   * paths relative to the `.blend` file. */
  BLI_string_replace_char(tmp + BLI_path_unc_prefix_len(tmp), '\\', '/');

  /* Paths starting with `//` will get the blend file as their base,
   * this isn't standard in any OS but is used in blender all over the place. */
  if (wasrelative) {
    const char *lslash;
    STRNCPY(base, basepath);

    /* File component is ignored, so don't bother with the trailing slash. */
    BLI_path_normalize(base);
    lslash = BLI_path_slash_rfind(base);
    BLI_string_replace_char(base + BLI_path_unc_prefix_len(base), '\\', '/');

    if (lslash) {
      /* Length up to and including last `/`. */
      const int baselen = (int)(lslash - base) + 1;
      /* Use path for temp storage here, we copy back over it right away. */
      BLI_strncpy(path, tmp + 2, FILE_MAX); /* Strip `//` prefix. */

      memcpy(tmp, base, baselen); /* Prefix with base up to last `/`. */
      BLI_strncpy(tmp + baselen, path, sizeof(tmp) - baselen); /* Append path after `//`. */
      BLI_strncpy(path, tmp, FILE_MAX);                        /* Return as result. */
    }
    else {
      /* Base doesn't seem to be a directory, ignore it and just strip `//` prefix on path. */
      BLI_strncpy(path, tmp + 2, FILE_MAX);
    }
  }
  else {
    /* Base ignored. */
    BLI_strncpy(path, tmp, FILE_MAX);
  }

#ifdef WIN32
  /* NOTE(@jesterking): Skip first two chars, which in case of absolute path will
   * be `drive:/blabla` and in case of `relpath` `//blabla/`.
   * So `relpath` `//` will be retained, rest will be nice and shiny WIN32 backward slashes. */
  BLI_string_replace_char(path + 2, '/', '\\');
#endif

  /* Ensure this is after correcting for path switch. */
  BLI_path_normalize(path);

  return wasrelative;
}

bool BLI_path_is_abs_from_cwd(const char *path)
{
  bool is_abs = false;
  const int path_len_clamp = BLI_strnlen(path, 3);

#ifdef WIN32
  if ((path_len_clamp >= 3 && BLI_path_is_abs_win32(path)) || BLI_path_is_unc(path)) {
    is_abs = true;
  }
#else
  if (path_len_clamp >= 2 && path[0] == '/') {
    is_abs = true;
  }
#endif
  return is_abs;
}

bool BLI_path_abs_from_cwd(char *path, const size_t path_maxncpy)
{
  BLI_string_debug_size_after_nil(path, path_maxncpy);

  if (!BLI_path_is_abs_from_cwd(path)) {
    char cwd[PATH_MAX];
    /* In case the full path to the blend isn't used. */
    if (BLI_current_working_dir(cwd, sizeof(cwd))) {
      char origpath[PATH_MAX];
      STRNCPY(origpath, path);
      BLI_path_join(path, path_maxncpy, cwd, origpath);
    }
    else {
      printf("Could not get the current working directory - $PWD for an unknown reason.\n");
    }
    return true;
  }

  return false;
}

#ifdef _WIN32
/**
 * Tries appending each of the semicolon-separated extensions in the `PATHEXT`
 * environment variable (Windows-only) onto `program_name` in turn until such a file is found.
 * Returns success/failure.
 */
bool BLI_path_program_extensions_add_win32(char *program_name, const size_t program_name_maxncpy)
{
  bool retval = false;
  int type;

  type = BLI_exists(program_name);
  if ((type == 0) || S_ISDIR(type)) {
    /* Typically 3-5, ".EXE", ".BAT"... etc. */
    const int ext_max = 12;
    const char *ext = BLI_getenv("PATHEXT");
    if (ext) {
      const int program_name_len = strlen(program_name);
      char *filename = alloca(program_name_len + ext_max);
      char *filename_ext;
      const char *ext_next;

      /* Null terminated in the loop. */
      memcpy(filename, program_name, program_name_len);
      filename_ext = filename + program_name_len;

      do {
        int ext_len;
        ext_next = strchr(ext, ';');
        ext_len = ext_next ? ((ext_next++) - ext) : strlen(ext);

        if (LIKELY(ext_len < ext_max)) {
          memcpy(filename_ext, ext, ext_len);
          filename_ext[ext_len] = '\0';

          type = BLI_exists(filename);
          if (type && (!S_ISDIR(type))) {
            retval = true;
            BLI_strncpy(program_name, filename, program_name_maxncpy);
            break;
          }
        }
      } while ((ext = ext_next));
    }
  }
  else {
    retval = true;
  }

  return retval;
}
#endif /* WIN32 */

bool BLI_path_program_search(char *program_filepath,
                             const size_t program_filepath_maxncpy,
                             const char *program_name)
{
  BLI_string_debug_size(program_filepath, program_filepath_maxncpy);

  const char *path;
  bool retval = false;

#ifdef _WIN32
  const char separator = ';';
#else
  const char separator = ':';
#endif

  path = BLI_getenv("PATH");
  if (path) {
    char filepath_test[PATH_MAX];
    const char *temp;

    do {
      temp = strchr(path, separator);
      if (temp) {
        memcpy(filepath_test, path, temp - path);
        filepath_test[temp - path] = 0;
        path = temp + 1;
      }
      else {
        STRNCPY(filepath_test, path);
      }

      BLI_path_append(filepath_test, program_filepath_maxncpy, program_name);
      if (
#ifdef _WIN32
          BLI_path_program_extensions_add_win32(filepath_test, sizeof(filepath_test))
#else
          BLI_exists(filepath_test)
#endif
      )
      {
        BLI_strncpy(program_filepath, filepath_test, program_filepath_maxncpy);
        retval = true;
        break;
      }
    } while (temp);
  }

  if (retval == false) {
    *program_filepath = '\0';
  }

  return retval;
}

void BLI_setenv(const char *env, const char *val)
{
#if (defined(_WIN32) || defined(_WIN64))
  /* MS-Windows. */
  uputenv(env, val);
#else
  /* Linux/macOS/BSD */
  if (val) {
    setenv(env, val, 1);
  }
  else {
    unsetenv(env);
  }
#endif
}

void BLI_setenv_if_new(const char *env, const char *val)
{
  if (BLI_getenv(env) == NULL) {
    BLI_setenv(env, val);
  }
}

const char *BLI_getenv(const char *env)
{
#ifdef _MSC_VER
  const char *result = NULL;
  /* 32767 is the maximum size of the environment variable on windows,
   * reserve one more character for the zero terminator. */
  static wchar_t buffer[32768];
  wchar_t *env_16 = alloc_utf16_from_8(env, 0);
  if (env_16) {
    if (GetEnvironmentVariableW(env_16, buffer, ARRAY_SIZE(buffer))) {
      char *res_utf8 = alloc_utf_8_from_16(buffer, 0);
      /* Make sure the result is valid, and will fit into our temporary storage buffer. */
      if (res_utf8) {
        if (strlen(res_utf8) + 1 < sizeof(buffer)) {
          /* We are re-using the utf16 buffer here, since allocating a second static buffer to
           * contain the UTF-8 version to return would be wasteful. */
          memcpy(buffer, res_utf8, strlen(res_utf8) + 1);
          result = (const char *)buffer;
        }
        free(res_utf8);
      }
    }
  }
  return result;
#else
  return getenv(env);
#endif
}

static bool path_extension_check_ex(const char *path,
                                    const size_t path_len,
                                    const char *ext,
                                    const size_t ext_len)
{
  BLI_assert(strlen(path) == path_len);
  BLI_assert(strlen(ext) == ext_len);

  return (((path_len == 0 || ext_len == 0 || ext_len >= path_len) == 0) &&
          (BLI_strcasecmp(ext, path + path_len - ext_len) == 0));
}

bool BLI_path_extension_check(const char *path, const char *ext)
{
  return path_extension_check_ex(path, strlen(path), ext, strlen(ext));
}

bool BLI_path_extension_check_n(const char *path, ...)
{
  const size_t path_len = strlen(path);

  va_list args;
  const char *ext;
  bool ret = false;

  va_start(args, path);

  while ((ext = (const char *)va_arg(args, void *))) {
    if (path_extension_check_ex(path, path_len, ext, strlen(ext))) {
      ret = true;
      break;
    }
  }

  va_end(args);

  return ret;
}

bool BLI_path_extension_check_array(const char *path, const char **ext_array)
{
  const size_t path_len = strlen(path);
  int i = 0;

  while (ext_array[i]) {
    if (path_extension_check_ex(path, path_len, ext_array[i], strlen(ext_array[i]))) {
      return true;
    }

    i++;
  }
  return false;
}

bool BLI_path_extension_check_glob(const char *path, const char *ext_fnmatch)
{
  const char *ext_step = ext_fnmatch;
  char pattern[16];

  while (ext_step[0]) {
    const char *ext_next;
    size_t len_ext;

    if ((ext_next = strchr(ext_step, ';'))) {
      len_ext = ext_next - ext_step + 1;
      BLI_strncpy(pattern, ext_step, (len_ext > sizeof(pattern)) ? sizeof(pattern) : len_ext);
    }
    else {
      len_ext = STRNCPY_RLEN(pattern, ext_step);
    }

    if (fnmatch(pattern, path, FNM_CASEFOLD) == 0) {
      return true;
    }
    ext_step += len_ext;
  }

  return false;
}

bool BLI_path_extension_glob_validate(char *ext_fnmatch)
{
  bool only_wildcards = false;

  for (size_t i = strlen(ext_fnmatch); i-- > 0;) {
    if (ext_fnmatch[i] == ';') {
      /* Group separator, we truncate here if we only had wildcards so far.
       * Otherwise, all is sound and fine. */
      if (only_wildcards) {
        ext_fnmatch[i] = '\0';
        return true;
      }
      return false;
    }
    if (!ELEM(ext_fnmatch[i], '?', '*')) {
      /* Non-wildcard char, we can break here and consider the pattern valid. */
      return false;
    }
    /* So far, only wildcards in last group of the pattern. */
    only_wildcards = true;
  }
  /* Only one group in the pattern, so even if its only made of wildcard(s),
   * it is assumed valid. */
  return false;
}

bool BLI_path_extension_replace(char *path, size_t path_maxncpy, const char *ext)
{
  BLI_string_debug_size_after_nil(path, path_maxncpy);

  char *path_ext = (char *)BLI_path_extension_or_end(path);
  const size_t ext_len = strlen(ext);
  if ((path_ext - path) + ext_len >= path_maxncpy) {
    return false;
  }

  memcpy(path_ext, ext, ext_len + 1);
  return true;
}

bool BLI_path_extension_strip(char *path)
{
  char *path_ext = (char *)BLI_path_extension(path);
  if (path_ext == NULL) {
    return false;
  }
  *path_ext = '\0';
  return true;
}

bool BLI_path_extension_ensure(char *path, size_t path_maxncpy, const char *ext)
{
  BLI_string_debug_size_after_nil(path, path_maxncpy);

  /* First check the extension is already there.
   * If `path_ext` is the end of the string this is simply checking if `ext` is also empty. */
  const char *path_ext = BLI_path_extension_or_end(path);
  if (STREQ(path_ext, ext)) {
    return true;
  }

  const size_t path_len = strlen(path);
  const size_t ext_len = strlen(ext);
  ssize_t a;

  for (a = path_len - 1; a >= 0; a--) {
    if (path[a] == '.') {
      path[a] = '\0';
    }
    else {
      break;
    }
  }
  a++;

  if (a + ext_len >= path_maxncpy) {
    return false;
  }

  memcpy(path + a, ext, ext_len + 1);
  return true;
}

bool BLI_path_filename_ensure(char *filepath, size_t filepath_maxncpy, const char *filename)
{
  BLI_string_debug_size_after_nil(filepath, filepath_maxncpy);
  char *c = (char *)BLI_path_basename(filepath);
  const size_t filename_size = strlen(filename) + 1;
  if (filename_size <= filepath_maxncpy - (c - filepath)) {
    memcpy(c, filename, filename_size);
    return true;
  }
  return false;
}

void BLI_path_split_dir_file(const char *filepath,
                             char *dir,
                             const size_t dir_maxncpy,
                             char *file,
                             const size_t file_maxncpy)
{
  BLI_string_debug_size(dir, dir_maxncpy);
  BLI_string_debug_size(file, file_maxncpy);

  const char *basename = BLI_path_basename(filepath);
  if (basename != filepath) {
    const size_t dir_size = (basename - filepath) + 1;
    BLI_strncpy(dir, filepath, MIN2(dir_maxncpy, dir_size));
  }
  else {
    dir[0] = '\0';
  }
  BLI_strncpy(file, basename, file_maxncpy);
}

void BLI_path_split_dir_part(const char *filepath, char *dir, const size_t dir_maxncpy)
{
  BLI_string_debug_size(dir, dir_maxncpy);
  const char *basename = BLI_path_basename(filepath);
  if (basename != filepath) {
    const size_t dir_size = (basename - filepath) + 1;
    BLI_strncpy(dir, filepath, MIN2(dir_maxncpy, dir_size));
  }
  else {
    dir[0] = '\0';
  }
}

void BLI_path_split_file_part(const char *filepath, char *file, const size_t file_maxncpy)
{
  BLI_string_debug_size(file, file_maxncpy);
  const char *basename = BLI_path_basename(filepath);
  BLI_strncpy(file, basename, file_maxncpy);
}

const char *BLI_path_extension_or_end(const char *filepath)
{
  /* NOTE(@ideasman42): Skip the extension when there are no preceding non-extension characters in
   * the file name. This ignores extensions at the beginning of a string or directly after a slash.
   * Only using trailing extension characters has the advantage that stripping the extension
   * never leads to a blank string (which can't be used as a file path).
   * Matches Python's `os.path.splitext`. */
  const char *ext = NULL;
  bool has_non_ext = false;
  const char *c = filepath;
  for (; *c; c++) {
    switch (*c) {
      case '.': {
        if (has_non_ext) {
          ext = c;
        }
        break;
      }
      case SEP:
      case ALTSEP: {
        ext = NULL;
        has_non_ext = false;
        break;
      }
      default: {
        has_non_ext = true;
        break;
      }
    }
  }
  if (ext) {
    return ext;
  }
  BLI_assert(*c == '\0');
  return c;
}

const char *BLI_path_extension(const char *filepath)
{
  const char *ext = BLI_path_extension_or_end(filepath);
  return *ext ? ext : NULL;
}

size_t BLI_path_append(char *__restrict dst, const size_t dst_maxncpy, const char *__restrict file)
{
  /* Slash ensure uses #BLI_string_debug_size */
  int dst_len = BLI_path_slash_ensure(dst, dst_maxncpy);
  if (dst_len + 1 < dst_maxncpy) {
    dst_len += BLI_strncpy_rlen(dst + dst_len, file, dst_maxncpy - dst_len);
  }
  return dst_len;
}

size_t BLI_path_append_dir(char *__restrict dst,
                           const size_t dst_maxncpy,
                           const char *__restrict dir)
{
  size_t dst_len = BLI_path_append(dst, dst_maxncpy, dir);
  return BLI_path_slash_ensure_ex(dst, dst_maxncpy, dst_len);
}

size_t BLI_path_join_array(char *__restrict dst,
                           const size_t dst_maxncpy,
                           const char *path_array[],
                           const int path_array_num)
{
  BLI_assert(path_array_num > 0);
  BLI_string_debug_size(dst, dst_maxncpy);

  if (UNLIKELY(dst_maxncpy == 0)) {
    return 0;
  }
  const char *path = path_array[0];

  const size_t dst_last = dst_maxncpy - 1;
  size_t ofs = BLI_strncpy_rlen(dst, path, dst_maxncpy);

  if (ofs == dst_last) {
    return ofs;
  }

#ifdef WIN32
  /* Special case `//` for relative paths, don't use separator #SEP
   * as this has a special meaning on both WIN32 & UNIX.
   * Without this check joining `"//", "path"`. results in `"//\path"`. */
  if (ofs != 0) {
    size_t i;
    for (i = 0; i < ofs; i++) {
      if (dst[i] != '/') {
        break;
      }
    }
    if (i == ofs) {
      /* All slashes, keep them as-is, and join the remaining path array. */
      return path_array_num > 1 ?
                 BLI_path_join_array(
                     dst + ofs, dst_maxncpy - ofs, &path_array[1], path_array_num - 1) :
                 ofs;
    }
  }
#endif

  /* Remove trailing slashes, unless there are *only* trailing slashes
   * (allow `//` or `//some_path` as the first argument). */
  bool has_trailing_slash = false;
  if (ofs != 0) {
    size_t len = ofs;
    while ((len != 0) && BLI_path_slash_is_native_compat(path[len - 1])) {
      len -= 1;
    }

    if (len != 0) {
      ofs = len;
    }
    has_trailing_slash = (path[len] != '\0');
  }

  for (int path_index = 1; path_index < path_array_num; path_index++) {
    path = path_array[path_index];
    has_trailing_slash = false;
    const char *path_init = path;
    while (BLI_path_slash_is_native_compat(path[0])) {
      path++;
    }
    size_t len = strlen(path);
    if (len != 0) {
      while ((len != 0) && BLI_path_slash_is_native_compat(path[len - 1])) {
        len -= 1;
      }

      if (len != 0) {
        /* The very first path may have a slash at the end. */
        if (ofs && !BLI_path_slash_is_native_compat(dst[ofs - 1])) {
          dst[ofs++] = SEP;
          if (ofs == dst_last) {
            break;
          }
        }
        has_trailing_slash = (path[len] != '\0');
        if (ofs + len >= dst_last) {
          len = dst_last - ofs;
        }
        memcpy(&dst[ofs], path, len);
        ofs += len;
        if (ofs == dst_last) {
          break;
        }
      }
    }
    else {
      has_trailing_slash = (path_init != path);
    }
  }

  if (has_trailing_slash) {
    if ((ofs != dst_last) && (ofs != 0) && !BLI_path_slash_is_native_compat(dst[ofs - 1])) {
      dst[ofs++] = SEP;
    }
  }

  BLI_assert(ofs <= dst_last);
  dst[ofs] = '\0';

  return ofs;
}

const char *BLI_path_basename(const char *path)
{
  const char *const filename = BLI_path_slash_rfind(path);
  return filename ? filename + 1 : path;
}

static bool path_name_at_index_forward(const char *__restrict path,
                                       const int index,
                                       int *__restrict r_offset,
                                       int *__restrict r_len)
{
  BLI_assert(index >= 0);
  int index_step = 0;
  int prev = -1;
  int i = 0;
  while (true) {
    const char c = path[i];
    if ((c == '\0') || BLI_path_slash_is_native_compat(c)) {
      if (prev + 1 != i) {
        prev += 1;
        /* Skip `/./` (behave as if they don't exist). */
        if (!((i - prev == 1) && (prev != 0) && (path[prev] == '.'))) {
          if (index_step == index) {
            *r_offset = prev;
            *r_len = i - prev;
            return true;
          }
          index_step += 1;
        }
      }
      if (c == '\0') {
        break;
      }
      prev = i;
    }
    i += 1;
  }
  return false;
}

static bool path_name_at_index_backward(const char *__restrict path,
                                        const int index,
                                        int *__restrict r_offset,
                                        int *__restrict r_len)
{
  /* Negative number, reverse where -1 is the last element. */
  BLI_assert(index < 0);
  int index_step = -1;
  int prev = strlen(path);
  int i = prev - 1;
  while (true) {
    const char c = i >= 0 ? path[i] : '\0';
    if ((c == '\0') || BLI_path_slash_is_native_compat(c)) {
      if (prev - 1 != i) {
        i += 1;
        /* Skip `/./` (behave as if they don't exist). */
        if (!((prev - i == 1) && (i != 0) && (path[i] == '.'))) {
          if (index_step == index) {
            *r_offset = i;
            *r_len = prev - i;
            return true;
          }
          index_step -= 1;
        }
      }
      if (c == '\0') {
        break;
      }
      prev = i;
    }
    i -= 1;
  }
  return false;
}

bool BLI_path_name_at_index(const char *__restrict path,
                            const int index,
                            int *__restrict r_offset,
                            int *__restrict r_len)
{
  return (index >= 0) ? path_name_at_index_forward(path, index, r_offset, r_len) :
                        path_name_at_index_backward(path, index, r_offset, r_len);
}

bool BLI_path_contains(const char *container_path, const char *containee_path)
{
  char container_native[PATH_MAX];
  char containee_native[PATH_MAX];

  /* Keep space for a trailing slash. If the path is truncated by this, the containee path is
   * longer than #PATH_MAX and the result is ill-defined. */
  BLI_strncpy(container_native, container_path, PATH_MAX - 1);
  STRNCPY(containee_native, containee_path);

  BLI_path_slash_native(container_native);
  BLI_path_slash_native(containee_native);

  BLI_path_normalize(container_native);
  BLI_path_normalize(containee_native);

#ifdef WIN32
  BLI_str_tolower_ascii(container_native, PATH_MAX);
  BLI_str_tolower_ascii(containee_native, PATH_MAX);
#endif

  if (STREQ(container_native, containee_native)) {
    /* The paths are equal, they contain each other. */
    return true;
  }

  /* Add a trailing slash to prevent same-prefix directories from matching.
   * e.g. "/some/path" doesn't contain "/some/path_lib". */
  BLI_path_slash_ensure(container_native, sizeof(container_native));

  return BLI_str_startswith(containee_native, container_native);
}

const char *BLI_path_slash_find(const char *path)
{
  const char *const ffslash = strchr(path, '/');
  const char *const fbslash = strchr(path, '\\');

  if (!ffslash) {
    return fbslash;
  }
  if (!fbslash) {
    return ffslash;
  }

  return (ffslash < fbslash) ? ffslash : fbslash;
}

const char *BLI_path_slash_rfind(const char *path)
{
  const char *const lfslash = strrchr(path, '/');
  const char *const lbslash = strrchr(path, '\\');

  if (!lfslash) {
    return lbslash;
  }
  if (!lbslash) {
    return lfslash;
  }

  return (lfslash > lbslash) ? lfslash : lbslash;
}

int BLI_path_slash_ensure_ex(char *path, size_t path_maxncpy, size_t path_len)
{
  BLI_string_debug_size_after_nil(path, path_maxncpy);
  BLI_assert(strlen(path) == path_len);
  BLI_assert(path_len < path_maxncpy);
  if (path_len == 0 || !BLI_path_slash_is_native_compat(path[path_len - 1])) {
    /* Avoid unlikely buffer overflow. */
    if (path_len + 1 < path_maxncpy) {
      path[path_len++] = SEP;
      path[path_len] = '\0';
    }
  }
  return path_len;
}

int BLI_path_slash_ensure(char *path, size_t path_maxncpy)
{
  return BLI_path_slash_ensure_ex(path, path_maxncpy, strlen(path));
}

void BLI_path_slash_rstrip(char *path)
{
  int len = strlen(path);
  while (len) {
    if (BLI_path_slash_is_native_compat(path[len - 1])) {
      path[len - 1] = '\0';
      len--;
    }
    else {
      break;
    }
  }
}

const char *BLI_path_slash_skip(const char *path)
{
  /* This accounts for a null byte too. */
  while (BLI_path_slash_is_native_compat(*path)) {
    path++;
  }
  return path;
}

void BLI_path_slash_native(char *path)
{
#ifdef WIN32
  if (path && BLI_strnlen(path, 3) > 2) {
    BLI_string_replace_char(path + 2, ALTSEP, SEP);
  }
#else
  BLI_string_replace_char(path + BLI_path_unc_prefix_len(path), ALTSEP, SEP);
#endif
}

int BLI_path_cmp_normalized(const char *p1, const char *p2)
{
  BLI_assert_msg(!BLI_path_is_rel(p1) && !BLI_path_is_rel(p2), "Paths arguments must be absolute");

  /* Normalize the paths so we can compare them. */
  char norm_p1_buf[256];
  char norm_p2_buf[256];

  const size_t p1_size = strlen(p1) + 1;
  const size_t p2_size = strlen(p2) + 1;

  char *norm_p1 = (p1_size <= sizeof(norm_p1_buf)) ? norm_p1_buf : MEM_mallocN(p1_size, __func__);
  char *norm_p2 = (p2_size <= sizeof(norm_p2_buf)) ? norm_p2_buf : MEM_mallocN(p2_size, __func__);

  memcpy(norm_p1, p1, p1_size);
  memcpy(norm_p2, p2, p2_size);

  BLI_path_slash_native(norm_p1);
  BLI_path_slash_native(norm_p2);

  /* One of the paths ending with a slash does not make them different, strip both. */
  BLI_path_slash_rstrip(norm_p1);
  BLI_path_slash_rstrip(norm_p2);

  BLI_path_normalize(norm_p1);
  BLI_path_normalize(norm_p2);

  const int result = BLI_path_cmp(norm_p1, norm_p2);

  if (norm_p1 != norm_p1_buf) {
    MEM_freeN(norm_p1);
  }
  if (norm_p2 != norm_p2_buf) {
    MEM_freeN(norm_p2);
  }
  return result;
}
