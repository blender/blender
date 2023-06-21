/* SPDX-FileCopyrightText: 2017 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.h"
#include "BLI_utildefines.h"

#include "DNA_listBase.h"

#ifdef __GNUC__
#  pragma GCC diagnostic error "-Wsign-conversion"
#endif

size_t BLI_string_split_name_number(const char *name,
                                    const char delim,
                                    char *r_name_left,
                                    int *r_number)
{
  const size_t name_len = strlen(name);

  *r_number = 0;
  memcpy(r_name_left, name, (name_len + 1) * sizeof(char));

  /* name doesn't end with a delimiter "foo." */
  if ((name_len > 1 && name[name_len - 1] == delim) == 0) {
    size_t a = name_len;
    while (a--) {
      if (name[a] == delim) {
        r_name_left[a] = '\0'; /* truncate left part here */
        *r_number = atol(name + a + 1);
        /* casting down to an int, can overflow for large numbers */
        if (*r_number < 0) {
          *r_number = 0;
        }
        return a;
      }
      if (isdigit(name[a]) == 0) {
        /* non-numeric suffix - give up */
        break;
      }
    }
  }

  return name_len;
}

bool BLI_string_is_decimal(const char *string)
{
  if (*string == '\0') {
    return false;
  }

  /* Keep iterating over the string until a non-digit is found. */
  while (isdigit(*string)) {
    string++;
  }

  /* If the non-digit we found is the terminating \0, everything was digits. */
  return *string == '\0';
}

static bool is_char_sep(const char c)
{
  return ELEM(c, '.', ' ', '-', '_');
}

void BLI_string_split_suffix(const char *string,
                             const size_t string_maxlen,
                             char *r_body,
                             char *r_suf)
{
  BLI_string_debug_size(r_body, string_maxlen);
  BLI_string_debug_size(r_suf, string_maxlen);

  size_t len = BLI_strnlen(string, string_maxlen);
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

void BLI_string_split_prefix(const char *string,
                             const size_t string_maxlen,
                             char *r_pre,
                             char *r_body)
{
  BLI_string_debug_size(r_pre, string_maxlen);
  BLI_string_debug_size(r_body, string_maxlen);

  size_t len = BLI_strnlen(string, string_maxlen);
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

size_t BLI_string_flip_side_name(char *name_dst,
                                 const char *name_src,
                                 const bool strip_number,
                                 const size_t name_dst_maxncpy)
{
  BLI_string_debug_size(name_dst, name_dst_maxncpy);

  size_t len;
  char *prefix = alloca(name_dst_maxncpy); /* The part before the facing */
  char *suffix = alloca(name_dst_maxncpy); /* The part after the facing */
  char *number = alloca(name_dst_maxncpy); /* The number extension string */
  const char *replace = NULL;
  char *index = NULL;
  bool is_set = false;

  *prefix = *suffix = *number = '\0';

  /* always copy the name, since this can be called with an uninitialized string */
  len = BLI_strncpy_rlen(name_dst, name_src, name_dst_maxncpy);
  if (len < 3) {
    /* we don't do names like .R or .L */
    return len;
  }

  /* We first check the case with a .### extension, let's find the last period */
  if (isdigit(name_dst[len - 1])) {
    index = strrchr(name_dst, '.');   /* Last occurrence. */
    if (index && isdigit(index[1])) { /* Doesn't handle case `bone.1abc2` correct..., whatever! */
      if (strip_number == false) {
        BLI_strncpy(number, index, name_dst_maxncpy);
      }
      *index = 0;
      len = BLI_strnlen(name_dst, name_dst_maxncpy);
    }
  }

  BLI_strncpy(prefix, name_dst, name_dst_maxncpy);

  /* First case; separator (`.` or `_`) with extensions in `r R l L`. */
  if ((len > 1) && is_char_sep(name_dst[len - 2])) {
    is_set = true;
    switch (name_dst[len - 1]) {
      case 'l':
        prefix[len - 1] = 0;
        replace = "r";
        break;
      case 'r':
        prefix[len - 1] = 0;
        replace = "l";
        break;
      case 'L':
        prefix[len - 1] = 0;
        replace = "R";
        break;
      case 'R':
        prefix[len - 1] = 0;
        replace = "L";
        break;
      default:
        is_set = false;
    }
  }

  /* case; beginning with r R l L, with separator after it */
  if (!is_set && is_char_sep(name_dst[1])) {
    is_set = true;
    switch (name_dst[0]) {
      case 'l':
        replace = "r";
        BLI_strncpy(suffix, name_dst + 1, name_dst_maxncpy);
        prefix[0] = 0;
        break;
      case 'r':
        replace = "l";
        BLI_strncpy(suffix, name_dst + 1, name_dst_maxncpy);
        prefix[0] = 0;
        break;
      case 'L':
        replace = "R";
        BLI_strncpy(suffix, name_dst + 1, name_dst_maxncpy);
        prefix[0] = 0;
        break;
      case 'R':
        replace = "L";
        BLI_strncpy(suffix, name_dst + 1, name_dst_maxncpy);
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
        replace = "left";
      }
      else {
        replace = (index[1] == 'I' ? "LEFT" : "Left");
      }
      *index = 0;
      BLI_strncpy(suffix, index + 5, name_dst_maxncpy);
    }
    else if (((index = BLI_strcasestr(prefix, "left")) == prefix) || (index == prefix + len - 4)) {
      is_set = true;
      if (index[0] == 'l') {
        replace = "right";
      }
      else {
        replace = (index[1] == 'E' ? "RIGHT" : "Right");
      }
      *index = 0;
      BLI_strncpy(suffix, index + 4, name_dst_maxncpy);
    }
  }

  return BLI_snprintf_rlen(
      name_dst, name_dst_maxncpy, "%s%s%s%s", prefix, replace ? replace : "", suffix, number);
}

/* Unique name utils. */

bool BLI_uniquename_cb(UniquenameCheckCallback unique_check,
                       void *arg,
                       const char *defname,
                       char delim,
                       char *name,
                       size_t name_maxncpy)
{
  BLI_string_debug_size_after_nil(name, name_maxncpy);

  if (name[0] == '\0') {
    BLI_strncpy(name, defname, name_maxncpy);
  }

  if (unique_check(arg, name)) {
    char numstr[16];
    char *tempname = alloca(name_maxncpy);
    char *left = alloca(name_maxncpy);
    int number;
    size_t len = BLI_string_split_name_number(name, delim, left, &number);
    do {
      const size_t numlen = SNPRINTF(numstr, "%c%03d", delim, ++number);

      /* highly unlikely the string only has enough room for the number
       * but support anyway */
      if (UNLIKELY((len == 0) || (numlen + 1 >= name_maxncpy))) {
        /* number is know not to be utf-8 */
        BLI_strncpy(tempname, numstr, name_maxncpy);
      }
      else {
        char *tempname_buf;
        tempname_buf = tempname + BLI_strncpy_utf8_rlen(tempname, left, name_maxncpy - numlen);
        memcpy(tempname_buf, numstr, numlen + 1);
      }
    } while (unique_check(arg, tempname));

    BLI_strncpy(name, tempname, name_maxncpy);

    return true;
  }

  return false;
}

/**
 * Generic function to set a unique name. It is only designed to be used in situations
 * where the name is part of the struct.
 *
 * For places where this is used, see constraint.c for example...
 *
 * \param name_offset: should be calculated using `offsetof(structname, membername)`
 * macro from `stddef.h`
 */
static bool uniquename_find_dupe(ListBase *list, void *vlink, const char *name, int name_offset)
{
  Link *link;

  for (link = list->first; link; link = link->next) {
    if (link != vlink) {
      if (STREQ(POINTER_OFFSET((const char *)link, name_offset), name)) {
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
    int name_offset;
  } *data = arg;
  return uniquename_find_dupe(data->lb, data->vlink, name, data->name_offset);
}

bool BLI_uniquename(ListBase *list,
                    void *vlink,
                    const char *defname,
                    char delim,
                    int name_offset,
                    size_t name_maxncpy)
{
  struct {
    ListBase *lb;
    void *vlink;
    int name_offset;
  } data;
  data.lb = list;
  data.vlink = vlink;
  data.name_offset = name_offset;

  BLI_assert(name_maxncpy > 1);

  /* See if we are given an empty string */
  if (ELEM(NULL, vlink)) {
    return false;
  }

  return BLI_uniquename_cb(uniquename_unique_check,
                           &data,
                           defname,
                           delim,
                           POINTER_OFFSET(vlink, name_offset),
                           name_maxncpy);
}

/* ------------------------------------------------------------------------- */
/** \name Join Strings
 *
 * For non array versions of these functions, use the macros:
 * - #BLI_string_join
 * - #BLI_string_joinN
 * - #BLI_string_join_by_sep_charN
 * - #BLI_string_join_by_sep_char_with_tableN
 *
 * \{ */

size_t BLI_string_join_array(char *result,
                             size_t result_maxncpy,
                             const char *strings[],
                             uint strings_num)
{
  BLI_string_debug_size(result, result_maxncpy);

  char *c = result;
  char *c_end = &result[result_maxncpy - 1];
  for (uint i = 0; i < strings_num; i++) {
    const char *p = strings[i];
    while (*p && (c < c_end)) {
      *c++ = *p++;
    }
  }
  *c = '\0';
  return (size_t)(c - result);
}

size_t BLI_string_join_array_by_sep_char(
    char *result, size_t result_maxncpy, char sep, const char *strings[], uint strings_num)
{
  BLI_string_debug_size(result, result_maxncpy);

  char *c = result;
  char *c_end = &result[result_maxncpy - 1];
  for (uint i = 0; i < strings_num; i++) {
    if (i != 0) {
      if (c < c_end) {
        *c++ = sep;
      }
    }
    const char *p = strings[i];
    while (*p && (c < c_end)) {
      *c++ = *p++;
    }
  }
  *c = '\0';
  return (size_t)(c - result);
}

char *BLI_string_join_arrayN(const char *strings[], uint strings_num)
{
  uint total_len = 1;
  for (uint i = 0; i < strings_num; i++) {
    total_len += strlen(strings[i]);
  }
  char *result = MEM_mallocN(sizeof(char) * total_len, __func__);
  char *c = result;
  for (uint i = 0; i < strings_num; i++) {
    c += BLI_strcpy_rlen(c, strings[i]);
  }
  /* Only needed when `strings_num == 0`. */
  *c = '\0';
  return result;
}

char *BLI_string_join_array_by_sep_charN(char sep, const char *strings[], uint strings_num)
{
  uint total_len = 0;
  for (uint i = 0; i < strings_num; i++) {
    total_len += strlen(strings[i]) + 1;
  }
  if (total_len == 0) {
    total_len = 1;
  }

  char *result = MEM_mallocN(sizeof(char) * total_len, __func__);
  char *c = result;
  if (strings_num != 0) {
    for (uint i = 0; i < strings_num; i++) {
      c += BLI_strcpy_rlen(c, strings[i]);
      *c = sep;
      c++;
    }
    c--;
  }
  *c = '\0';
  return result;
}

char *BLI_string_join_array_by_sep_char_with_tableN(char sep,
                                                    char *table[],
                                                    const char *strings[],
                                                    uint strings_num)
{
  uint total_len = 0;
  for (uint i = 0; i < strings_num; i++) {
    total_len += strlen(strings[i]) + 1;
  }
  if (total_len == 0) {
    total_len = 1;
  }

  char *result = MEM_mallocN(sizeof(char) * total_len, __func__);
  char *c = result;
  if (strings_num != 0) {
    for (uint i = 0; i < strings_num; i++) {
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
