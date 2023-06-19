/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdlib.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "Util.h"

void *memdbl(void *mem, int *size_pr, int item_size)
{
  int cur_size = *size_pr;
  int new_size = cur_size ? (cur_size * 2) : 1;
  void *nmem = MEM_mallocN(new_size * item_size, "memdbl");

  memcpy(nmem, mem, cur_size * item_size);
  MEM_freeN(mem);

  *size_pr = new_size;
  return nmem;
}

char *string_dup(char *str)
{
  int len = strlen(str);
  char *nstr = MEM_mallocN(len + 1, "string_dup");

  memcpy(nstr, str, len + 1);

  return nstr;
}

void fatal(char *fmt, ...)
{
  va_list ap;

  fprintf(stderr, "FATAL: ");
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fprintf(stderr, "\n");

  exit(1);
}
