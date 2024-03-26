/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "BLI_dynstr.h"
#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_ID.h"

#include "BKE_idprop.hh"
#include "BKE_idtype.hh"

#include "MEM_guardedalloc.h"

#include "BLI_strict_flags.h" /* Keep last. */

/* -------------------------------------------------------------------- */
/** \name IDProp Repr
 *
 * Convert an IDProperty to a string.
 *
 * Output should be a valid Python literal
 * (with minor exceptions - float nan for eg).
 * \{ */

struct ReprState {
  void (*str_append_fn)(void *user_data, const char *str, uint str_len);
  void *user_data;
  /* Big enough to format any primitive type. */
  char buf[128];
};

static void idp_str_append_escape(ReprState *state,
                                  const char *str,
                                  const uint str_len,
                                  bool quote)
{
  if (quote) {
    state->str_append_fn(state->user_data, "\"", 1);
  }
  uint i_prev = 0, i = 0;
  while (i < str_len) {
    const char c = str[i];
    if (c == '"') {
      if (i_prev != i) {
        state->str_append_fn(state->user_data, str + i_prev, i - i_prev);
      }
      state->str_append_fn(state->user_data, "\\\"", 2);
      i_prev = i + 1;
    }
    else if (c == '\\') {
      if (i_prev != i) {
        state->str_append_fn(state->user_data, str + i_prev, i - i_prev);
      }
      state->str_append_fn(state->user_data, "\\\\", 2);
      i_prev = i + 1;
    }
    else if (c < 32) {
      if (i_prev != i) {
        state->str_append_fn(state->user_data, str + i_prev, i - i_prev);
      }
      char buf[5];
      uint len = uint(SNPRINTF_RLEN(buf, "\\x%02x", c));
      BLI_assert(len == 4);
      state->str_append_fn(state->user_data, buf, len);
      i_prev = i + 1;
    }
    i++;
  }
  state->str_append_fn(state->user_data, str + i_prev, i - i_prev);
  if (quote) {
    state->str_append_fn(state->user_data, "\"", 1);
  }
}

static void idp_repr_fn_recursive(ReprState *state, const IDProperty *prop)
{
/* NOTE: 'strlen' will be calculated at compile time for literals. */
#define STR_APPEND_STR(str) state->str_append_fn(state->user_data, str, uint(strlen(str)))

#define STR_APPEND_STR_QUOTE(str) idp_str_append_escape(state, str, uint(strlen(str)), true)
#define STR_APPEND_STR_LEN_QUOTE(str, str_len) idp_str_append_escape(state, str, str_len, true)

#define STR_APPEND_FMT(format, ...) \
  state->str_append_fn( \
      state->user_data, state->buf, uint(SNPRINTF_RLEN(state->buf, format, __VA_ARGS__)))

  switch (prop->type) {
    case IDP_STRING: {
      STR_APPEND_STR_LEN_QUOTE(IDP_String(prop), uint(std::max(0, prop->len - 1)));
      break;
    }
    case IDP_INT: {
      if (const IDPropertyUIDataEnumItem *item = IDP_EnumItemFind(prop)) {
        STR_APPEND_FMT("%s", item->name);
      }
      STR_APPEND_FMT("%d", IDP_Int(prop));
      break;
    }
    case IDP_FLOAT: {
      STR_APPEND_FMT("%g", double(IDP_Float(prop)));
      break;
    }
    case IDP_DOUBLE: {
      STR_APPEND_FMT("%g", IDP_Double(prop));
      break;
    }
    case IDP_BOOLEAN: {
      STR_APPEND_FMT("%s", IDP_Bool(prop) ? "True" : "False");
      break;
    }
    case IDP_ARRAY: {
      STR_APPEND_STR("[");
      switch (prop->subtype) {
        case IDP_INT:
          for (const int *v = static_cast<const int *>(prop->data.pointer), *v_end = v + prop->len;
               v != v_end;
               v++)
          {
            if (v != prop->data.pointer) {
              STR_APPEND_STR(", ");
            }
            STR_APPEND_FMT("%d", *v);
          }
          break;
        case IDP_FLOAT:
          for (const float *v = static_cast<const float *>(prop->data.pointer),
                           *v_end = v + prop->len;
               v != v_end;
               v++)
          {
            if (v != prop->data.pointer) {
              STR_APPEND_STR(", ");
            }
            STR_APPEND_FMT("%g", double(*v));
          }
          break;
        case IDP_DOUBLE:
          for (const double *v = static_cast<const double *>(prop->data.pointer),
                            *v_end = v + prop->len;
               v != v_end;
               v++)
          {
            if (v != prop->data.pointer) {
              STR_APPEND_STR(", ");
            }
            STR_APPEND_FMT("%g", *v);
          }
          break;
        case IDP_BOOLEAN:
          for (const double *v = static_cast<const double *>(prop->data.pointer),
                            *v_end = v + prop->len;
               v != v_end;
               v++)
          {
            if (v != prop->data.pointer) {
              STR_APPEND_STR(", ");
            }
            STR_APPEND_FMT("%s", IDP_Bool(prop) ? "True" : "False");
          }
          break;
      }
      STR_APPEND_STR("]");
      break;
    }
    case IDP_IDPARRAY: {
      STR_APPEND_STR("[");
      for (const IDProperty *v = static_cast<const IDProperty *>(prop->data.pointer),
                            *v_end = v + prop->len;
           v != v_end;
           v++)
      {
        if (v != prop->data.pointer) {
          STR_APPEND_STR(", ");
        }
        idp_repr_fn_recursive(state, v);
      }
      STR_APPEND_STR("]");
      break;
    }
    case IDP_GROUP: {
      STR_APPEND_STR("{");
      LISTBASE_FOREACH (const IDProperty *, subprop, &prop->data.group) {
        if (subprop != prop->data.group.first) {
          STR_APPEND_STR(", ");
        }
        STR_APPEND_STR_QUOTE(subprop->name);
        STR_APPEND_STR(": ");
        idp_repr_fn_recursive(state, subprop);
      }
      STR_APPEND_STR("}");
      break;
    }
    case IDP_ID: {
      const ID *id = static_cast<const ID *>(prop->data.pointer);
      if (id != nullptr) {
        STR_APPEND_STR("bpy.data.");
        STR_APPEND_STR(BKE_idtype_idcode_to_name_plural(GS(id->name)));
        STR_APPEND_STR("[");
        STR_APPEND_STR_QUOTE(id->name + 2);
        STR_APPEND_STR("]");
      }
      else {
        STR_APPEND_STR("None");
      }
      break;
    }
    default: {
      BLI_assert_unreachable();
      break;
    }
  }

#undef STR_APPEND_STR
#undef STR_APPEND_STR_QUOTE
#undef STR_APPEND_STR_LEN_QUOTE
#undef STR_APPEND_FMT
}

void IDP_repr_fn(const IDProperty *prop,
                 void (*str_append_fn)(void *user_data, const char *str, uint str_len),
                 void *user_data)
{
  ReprState state{};
  state.str_append_fn = str_append_fn;
  state.user_data = user_data;
  idp_repr_fn_recursive(&state, prop);
}

static void repr_str(void *user_data, const char *str, uint len)
{
  BLI_dynstr_nappend(static_cast<DynStr *>(user_data), str, int(len));
}

char *IDP_reprN(const IDProperty *prop, uint *r_len)
{
  DynStr *ds = BLI_dynstr_new();
  IDP_repr_fn(prop, repr_str, ds);
  char *cstring = BLI_dynstr_get_cstring(ds);
  if (r_len != nullptr) {
    *r_len = uint(BLI_dynstr_get_len(ds));
  }
  BLI_dynstr_free(ds);
  return cstring;
}

void IDP_print(const IDProperty *prop)
{
  char *repr = IDP_reprN(prop, nullptr);
  printf("IDProperty(%p): ", prop);
  puts(repr);
  MEM_freeN(repr);
}

/** \} */
