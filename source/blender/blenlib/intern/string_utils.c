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
 * The Original Code is Copyright (C) 2017 by the Blender FOundation.
 * All rights reserved.
 */

/** \file
 * \ingroup bli
 */

#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.h"
#include "BLI_utildefines.h"

#include "DNA_listBase.h"

#ifdef __GNUC__
#  pragma GCC diagnostic error "-Wsign-conversion"
#endif

/**
 * Looks for a numeric suffix preceded by delim character on the end of
 * name, puts preceding part into *left and value of suffix into *nr.
 * Returns the length of *left.
 *
 * Foo.001 -> "Foo", 1
 * Returning the length of "Foo"
 *
 * \param left: Where to return copy of part preceding delim
 * \param nr: Where to return value of numeric suffix
 * \param name: String to split
 * \param delim: Delimiter character
 * \return  Length of \a left
 */
size_t BLI_split_name_num(char *left, int *nr, const char *name, const char delim)
{
  const size_t name_len = strlen(name);

  *nr = 0;
  memcpy(left, name, (name_len + 1) * sizeof(char));

  /* name doesn't end with a delimiter "foo." */
  if ((name_len > 1 && name[name_len - 1] == delim) == 0) {
    size_t a = name_len;
    while (a--) {
      if (name[a] == delim) {
        left[a] = '\0'; /* truncate left part here */
        *nr = atol(name + a + 1);
        /* casting down to an int, can overflow for large numbers */
        if (*nr < 0) {
          *nr = 0;
        }
        return a;
      }
      else if (isdigit(name[a]) == 0) {
        /* non-numeric suffix - give up */
        break;
      }
    }
  }

  return name_len;
}

static bool is_char_sep(const char c)
{
  return ELEM(c, '.', ' ', '-', '_');
}

/**
 * based on `BLI_split_dirfile()` / `os.path.splitext()`,
 * `"a.b.c"` -> (`"a.b"`, `".c"`).
 */
void BLI_string_split_suffix(const char *string, char *r_body, char *r_suf, const size_t str_len)
{
  size_t len = BLI_strnlen(string, str_len);
  size_t i;

  r_body[0] = r_suf[0] = '\0';

  for (i = len; i > 0; i--) {
    if (is_char_sep(string[i])) {
      BLI_strncpy(r_body, string, i + 1);
      BLI_strncpy(r_suf, string + i, (len + 1) - i);
      return;
    }
  }

  memcpy(r_body, string, len + 1);
}

/**
 * `"a.b.c"` -> (`"a."`, `"b.c"`)
 */
void BLI_string_split_prefix(const char *string, char *r_pre, char *r_body, const size_t str_len)
{
  size_t len = BLI_strnlen(string, str_len);
  size_t i;

  r_body[0] = r_pre[0] = '\0';

  for (i = 1; i < len; i++) {
    if (is_char_sep(string[i])) {
      i++;
      BLI_strncpy(r_pre, string, i + 1);
      BLI_strncpy(r_body, string + i, (len + 1) - i);
      return;
    }
  }

  BLI_strncpy(r_body, string, len);
}

/**
 * Finds the best possible flipped (left/right) name.
 * For renaming; check for unique names afterwards.
 *
 * \param r_name: flipped name,
 * assumed to be a pointer to a string of at least \a name_len size.
 * \param from_name: original name,
 * assumed to be a pointer to a string of at least \a name_len size.
 * \param strip_number: If set, remove number extensions.
 */
void BLI_string_flip_side_name(char *r_name,
                               const char *from_name,
                               const bool strip_number,
                               const size_t name_len)
{
  size_t len;
  char *prefix = alloca(name_len);  /* The part before the facing */
  char *suffix = alloca(name_len);  /* The part after the facing */
  char *replace = alloca(name_len); /* The replacement string */
  char *number = alloca(name_len);  /* The number extension string */
  char *index = NULL;
  bool is_set = false;

  *prefix = *suffix = *replace = *number = '\0';

  /* always copy the name, since this can be called with an uninitialized string */
  BLI_strncpy(r_name, from_name, name_len);

  len = BLI_strnlen(from_name, name_len);
  if (len < 3) {
    /* we don't do names like .R or .L */
    return;
  }

  /* We first check the case with a .### extension, let's find the last period */
  if (isdigit(r_name[len - 1])) {
    index = strrchr(r_name, '.');     /* last occurrence. */
    if (index && isdigit(index[1])) { /* doesn't handle case bone.1abc2 correct..., whatever! */
      if (strip_number == false) {
        BLI_strncpy(number, index, name_len);
      }
      *index = 0;
      len = BLI_strnlen(r_name, name_len);
    }
  }

  BLI_strncpy(prefix, r_name, name_len);

  /* first case; separator . - _ with extensions r R l L  */
  if ((len > 1) && is_char_sep(r_name[len - 2])) {
    is_set = true;
    switch (r_name[len - 1]) {
      case 'l':
        prefix[len - 1] = 0;
        strcpy(replace, "r");
        break;
      case 'r':
        prefix[len - 1] = 0;
        strcpy(replace, "l");
        break;
      case 'L':
        prefix[len - 1] = 0;
        strcpy(replace, "R");
        break;
      case 'R':
        prefix[len - 1] = 0;
        strcpy(replace, "L");
        break;
      default:
        is_set = false;
    }
  }

  /* case; beginning with r R l L, with separator after it */
  if (!is_set && is_char_sep(r_name[1])) {
    is_set = true;
    switch (r_name[0]) {
      case 'l':
        strcpy(replace, "r");
        BLI_strncpy(suffix, r_name + 1, name_len);
        prefix[0] = 0;
        break;
      case 'r':
        strcpy(replace, "l");
        BLI_strncpy(suffix, r_name + 1, name_len);
        prefix[0] = 0;
        break;
      case 'L':
        strcpy(replace, "R");
        BLI_strncpy(suffix, r_name + 1, name_len);
        prefix[0] = 0;
        break;
      case 'R':
        strcpy(replace, "L");
        BLI_strncpy(suffix, r_name + 1, name_len);
        prefix[0] = 0;
        break;
      default:
        is_set = false;
    }
  }

  if (!is_set && len > 5) {
    /* hrms, why test for a separator? lets do the rule 'ultimate left or right' */
    if (((index = BLI_strcasestr(prefix, "right")) == prefix) || (index == prefix + len - 5)) {
      is_set = true;
      if (index[0] == 'r') {
        strcpy(replace, "left");
      }
      else {
        strcpy(replace, (index[1] == 'I') ? "LEFT" : "Left");
      }
      *index = 0;
      BLI_strncpy(suffix, index + 5, name_len);
    }
    else if (((index = BLI_strcasestr(prefix, "left")) == prefix) || (index == prefix + len - 4)) {
      is_set = true;
      if (index[0] == 'l') {
        strcpy(replace, "right");
      }
      else {
        strcpy(replace, (index[1] == 'E') ? "RIGHT" : "Right");
      }
      *index = 0;
      BLI_strncpy(suffix, index + 4, name_len);
    }
  }

  BLI_snprintf(r_name, name_len, "%s%s%s%s", prefix, replace, suffix, number);
}

/* Unique name utils. */

/**
 * Ensures name is unique (according to criteria specified by caller in unique_check callback),
 * incrementing its numeric suffix as necessary. Returns true if name had to be adjusted.
 *
 * \param unique_check: Return true if name is not unique
 * \param arg: Additional arg to unique_check--meaning is up to caller
 * \param defname: To initialize name if latter is empty
 * \param delim: Delimits numeric suffix in name
 * \param name: Name to be ensured unique
 * \param name_len: Maximum length of name area
 * \return true if there if the name was changed
 */
bool BLI_uniquename_cb(UniquenameCheckCallback unique_check,
                       void *arg,
                       const char *defname,
                       char delim,
                       char *name,
                       size_t name_len)
{
  if (name[0] == '\0') {
    BLI_strncpy(name, defname, name_len);
  }

  if (unique_check(arg, name)) {
    char numstr[16];
    char *tempname = alloca(name_len);
    char *left = alloca(name_len);
    int number;
    size_t len = BLI_split_name_num(left, &number, name, delim);
    do {
      /* add 1 to account for \0 */
      const size_t numlen = BLI_snprintf(numstr, sizeof(numstr), "%c%03d", delim, ++number) + 1;

      /* highly unlikely the string only has enough room for the number
       * but support anyway */
      if ((len == 0) || (numlen >= name_len)) {
        /* number is know not to be utf-8 */
        BLI_strncpy(tempname, numstr, name_len);
      }
      else {
        char *tempname_buf;
        tempname_buf = tempname + BLI_strncpy_utf8_rlen(tempname, left, name_len - numlen);
        memcpy(tempname_buf, numstr, numlen);
      }
    } while (unique_check(arg, tempname));

    BLI_strncpy(name, tempname, name_len);

    return true;
  }

  return false;
}

/* little helper macro for BLI_uniquename */
#ifndef GIVE_STRADDR
#  define GIVE_STRADDR(data, offset) (((char *)data) + offset)
#endif

/**
 * Generic function to set a unique name. It is only designed to be used in situations
 * where the name is part of the struct.
 *
 * For places where this is used, see constraint.c for example...
 *
 * \param name_offs: should be calculated using offsetof(structname, membername)
 * macro from stddef.h
 */
static bool uniquename_find_dupe(ListBase *list, void *vlink, const char *name, int name_offs)
{
  Link *link;

  for (link = list->first; link; link = link->next) {
    if (link != vlink) {
      if (STREQ(GIVE_STRADDR(link, name_offs), name)) {
        return true;
      }
    }
  }

  return false;
}

static bool uniquename_unique_check(void *arg, const char *name)
{
  struct {
    ListBase *lb;
    void *vlink;
    int name_offs;
  } *data = arg;
  return uniquename_find_dupe(data->lb, data->vlink, name, data->name_offs);
}

/**
 * Ensures that the specified block has a unique name within the containing list,
 * incrementing its numeric suffix as necessary. Returns true if name had to be adjusted.
 *
 * \param list: List containing the block
 * \param vlink: The block to check the name for
 * \param defname: To initialize block name if latter is empty
 * \param delim: Delimits numeric suffix in name
 * \param name_offs: Offset of name within block structure
 * \param name_len: Maximum length of name area
 */
bool BLI_uniquename(
    ListBase *list, void *vlink, const char *defname, char delim, int name_offs, size_t name_len)
{
  struct {
    ListBase *lb;
    void *vlink;
    int name_offs;
  } data;
  data.lb = list;
  data.vlink = vlink;
  data.name_offs = name_offs;

  BLI_assert(name_len > 1);

  /* See if we are given an empty string */
  if (ELEM(NULL, vlink, defname)) {
    return false;
  }

  return BLI_uniquename_cb(
      uniquename_unique_check, &data, defname, delim, GIVE_STRADDR(vlink, name_offs), name_len);
}

/* ------------------------------------------------------------------------- */
/** \name Join Strings
 *
 * For non array versions of these functions, use the macros:
 * - #BLI_string_joinN
 * - #BLI_string_join_by_sep_charN
 * - #BLI_string_join_by_sep_char_with_tableN
 *
 * \{ */

/**
 * Join an array of strings into a newly allocated, null terminated string.
 */
char *BLI_string_join_arrayN(const char *strings[], uint strings_len)
{
  uint total_len = 1;
  for (uint i = 0; i < strings_len; i++) {
    total_len += strlen(strings[i]);
  }
  char *result = MEM_mallocN(sizeof(char) * total_len, __func__);
  char *c = result;
  for (uint i = 0; i < strings_len; i++) {
    c += BLI_strcpy_rlen(c, strings[i]);
  }
  return result;
}

/**
 * A version of #BLI_string_joinN that takes a separator which can be any character including '\0'.
 */
char *BLI_string_join_array_by_sep_charN(char sep, const char *strings[], uint strings_len)
{
  uint total_len = 0;
  for (uint i = 0; i < strings_len; i++) {
    total_len += strlen(strings[i]) + 1;
  }
  if (total_len == 0) {
    total_len = 1;
  }

  char *result = MEM_mallocN(sizeof(char) * total_len, __func__);
  char *c = result;
  if (strings_len != 0) {
    for (uint i = 0; i < strings_len; i++) {
      c += BLI_strcpy_rlen(c, strings[i]);
      *c = sep;
      c++;
    }
    c--;
  }
  *c = '\0';
  return result;
}

/**
 * A version of #BLI_string_join_array_by_sep_charN that takes a table array.
 * The new location of each string is written into this array.
 */
char *BLI_string_join_array_by_sep_char_with_tableN(char sep,
                                                    char *table[],
                                                    const char *strings[],
                                                    uint strings_len)
{
  uint total_len = 0;
  for (uint i = 0; i < strings_len; i++) {
    total_len += strlen(strings[i]) + 1;
  }
  if (total_len == 0) {
    total_len = 1;
  }

  char *result = MEM_mallocN(sizeof(char) * total_len, __func__);
  char *c = result;
  if (strings_len != 0) {
    for (uint i = 0; i < strings_len; i++) {
      table[i] = c; /* <-- only difference to BLI_string_join_array_by_sep_charN. */
      c += BLI_strcpy_rlen(c, strings[i]);
      *c = sep;
      c++;
    }
    c--;
  }
  *c = '\0';
  return result;
}

/** \} */
