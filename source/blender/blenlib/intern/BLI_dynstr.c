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

#ifdef _WIN32
#  ifndef vsnprintf
#    define vsnprintf _vsnprintf
#  endif
#endif

#ifndef va_copy
#  ifdef __va_copy
#    define va_copy(a, b) __va_copy(a, b)
#  else /* !__va_copy */
#    define va_copy(a, b) ((a) = (b))
#  endif /* __va_copy */
#endif   /* va_copy */

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
  char *message, fixedmessage[256];
  int len = sizeof(fixedmessage);
  const int maxlen = 65536;
  int retval;

  while (1) {
    va_list args_cpy;
    if (len == sizeof(fixedmessage)) {
      message = fixedmessage;
    }
    else {
      message = MEM_callocN(sizeof(char) * len, "BLI_dynstr_appendf");
    }

    /* can't reuse the same args, so work on a copy */
    va_copy(args_cpy, args);
    retval = vsnprintf(message, len, format, args_cpy);
    va_end(args_cpy);

    if (retval == -1) {
      /* -1 means not enough space, but on windows it may also mean
       * there is a formatting error, so we impose a maximum length */
      if (message != fixedmessage) {
        MEM_freeN(message);
      }
      message = NULL;

      len *= 2;
      if (len > maxlen) {
        fprintf(stderr, "BLI_dynstr_append text too long or format error.\n");
        break;
      }
    }
    else if (retval >= len) {
      /* in C99 the actual length required is returned */
      if (message != fixedmessage) {
        MEM_freeN(message);
      }
      message = NULL;

      /* retval doesn't include \0 terminator */
      len = retval + 1;
    }
    else {
      break;
    }
  }

  if (message) {
    BLI_dynstr_append(ds, message);

    if (message != fixedmessage) {
      MEM_freeN(message);
    }
  }
}

void BLI_dynstr_appendf(DynStr *__restrict ds, const char *__restrict format, ...)
{
  va_list args;
  char *message, fixedmessage[256];
  int len = sizeof(fixedmessage);
  const int maxlen = 65536;
  int retval;

  /* note that it's tempting to just call BLI_dynstr_vappendf here
   * and avoid code duplication, that crashes on some system because
   * va_start/va_end have to be called for each vsnprintf call */

  while (1) {
    if (len == sizeof(fixedmessage)) {
      message = fixedmessage;
    }
    else {
      message = MEM_callocN(sizeof(char) * (len), "BLI_dynstr_appendf");
    }

    va_start(args, format);
    retval = vsnprintf(message, len, format, args);
    va_end(args);

    if (retval == -1) {
      /* -1 means not enough space, but on windows it may also mean
       * there is a formatting error, so we impose a maximum length */
      if (message != fixedmessage) {
        MEM_freeN(message);
      }
      message = NULL;

      len *= 2;
      if (len > maxlen) {
        fprintf(stderr, "BLI_dynstr_append text too long or format error.\n");
        break;
      }
    }
    else if (retval >= len) {
      /* in C99 the actual length required is returned */
      if (message != fixedmessage) {
        MEM_freeN(message);
      }
      message = NULL;

      /* retval doesn't include \0 terminator */
      len = retval + 1;
    }
    else {
      break;
    }
  }

  if (message) {
    BLI_dynstr_append(ds, message);

    if (message != fixedmessage) {
      MEM_freeN(message);
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
