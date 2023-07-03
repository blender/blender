/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 * Dynamically sized string ADT.
 */

#include <stdio.h>
#include <stdlib.h> /* malloc */
#include <string.h>

#include "BLI_dynstr.h"
#include "BLI_memarena.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"
#include "MEM_guardedalloc.h"

/***/

typedef struct DynStrElem DynStrElem;
struct DynStrElem {
  DynStrElem *next;

  char *str;
};

struct DynStr {
  DynStrElem *elems, *last;
  int curlen;
  MemArena *memarena;
};

/***/

DynStr *BLI_dynstr_new(void)
{
  DynStr *ds = MEM_mallocN(sizeof(*ds), "DynStr");
  ds->elems = ds->last = NULL;
  ds->curlen = 0;
  ds->memarena = NULL;

  return ds;
}

DynStr *BLI_dynstr_new_memarena(void)
{
  DynStr *ds = MEM_mallocN(sizeof(*ds), "DynStr");
  ds->elems = ds->last = NULL;
  ds->curlen = 0;
  ds->memarena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);

  return ds;
}

BLI_INLINE void *dynstr_alloc(DynStr *__restrict ds, size_t size)
{
  return ds->memarena ? BLI_memarena_alloc(ds->memarena, size) : malloc(size);
}

void BLI_dynstr_append(DynStr *__restrict ds, const char *cstr)
{
  DynStrElem *dse = dynstr_alloc(ds, sizeof(*dse));
  int cstrlen = strlen(cstr);

  dse->str = dynstr_alloc(ds, cstrlen + 1);
  memcpy(dse->str, cstr, cstrlen + 1);
  dse->next = NULL;

  if (!ds->last) {
    ds->last = ds->elems = dse;
  }
  else {
    ds->last = ds->last->next = dse;
  }

  ds->curlen += cstrlen;
}

void BLI_dynstr_nappend(DynStr *__restrict ds, const char *cstr, int len)
{
  DynStrElem *dse = dynstr_alloc(ds, sizeof(*dse));
  int cstrlen = BLI_strnlen(cstr, len);

  dse->str = dynstr_alloc(ds, cstrlen + 1);
  memcpy(dse->str, cstr, cstrlen);
  dse->str[cstrlen] = '\0';
  dse->next = NULL;

  if (!ds->last) {
    ds->last = ds->elems = dse;
  }
  else {
    ds->last = ds->last->next = dse;
  }

  ds->curlen += cstrlen;
}

void BLI_dynstr_vappendf(DynStr *__restrict ds, const char *__restrict format, va_list args)
{
  char *str, fixed_buf[256];
  size_t str_len;
  str = BLI_vsprintfN_with_buffer(fixed_buf, sizeof(fixed_buf), &str_len, format, args);
  BLI_dynstr_append(ds, str);
  if (str != fixed_buf) {
    MEM_freeN(str);
  }
}

void BLI_dynstr_appendf(DynStr *__restrict ds, const char *__restrict format, ...)
{
  va_list args;
  char *str, fixed_buf[256];
  size_t str_len;
  va_start(args, format);
  str = BLI_vsprintfN_with_buffer(fixed_buf, sizeof(fixed_buf), &str_len, format, args);
  va_end(args);
  if (LIKELY(str)) {
    BLI_dynstr_append(ds, str);
    if (str != fixed_buf) {
      MEM_freeN(str);
    }
  }
}

int BLI_dynstr_get_len(const DynStr *ds)
{
  return ds->curlen;
}

void BLI_dynstr_get_cstring_ex(const DynStr *__restrict ds, char *__restrict rets)
{
  char *s;
  const DynStrElem *dse;

  for (s = rets, dse = ds->elems; dse; dse = dse->next) {
    int slen = strlen(dse->str);

    memcpy(s, dse->str, slen);

    s += slen;
  }
  BLI_assert((s - rets) == ds->curlen);
  rets[ds->curlen] = '\0';
}

char *BLI_dynstr_get_cstring(const DynStr *ds)
{
  char *rets = MEM_mallocN(ds->curlen + 1, "dynstr_cstring");
  BLI_dynstr_get_cstring_ex(ds, rets);
  return rets;
}

void BLI_dynstr_clear(DynStr *ds)
{
  if (ds->memarena) {
    BLI_memarena_clear(ds->memarena);
  }
  else {
    for (DynStrElem *dse_next, *dse = ds->elems; dse; dse = dse_next) {
      dse_next = dse->next;

      free(dse->str);
      free(dse);
    }
  }

  ds->elems = ds->last = NULL;
  ds->curlen = 0;
}

void BLI_dynstr_free(DynStr *ds)
{
  if (ds->memarena) {
    BLI_memarena_free(ds->memarena);
  }
  else {
    BLI_dynstr_clear(ds);
  }

  MEM_freeN(ds);
}
