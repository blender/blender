/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <cstdlib>
#include <stdlib.h>
#include <string.h>

#include "BLI_alloca.h"
#include "BLI_dynstr.h"
#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_idprop.h"
#include "BKE_idtype.h"
#include "BKE_lib_id.h"

#include "DNA_ID.h" /* For ID properties. */

#include "MEM_guardedalloc.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_path.h"
#include "RNA_prototypes.h"

#include "rna_access_internal.h"
#include "rna_internal.h"

/**
 * Extract the first token from `path`.
 *
 * \param path: Extract the token from path, step the pointer to the beginning of the next token
 * \return The nil terminated token.
 */
static char *rna_path_token(const char **path, char *fixedbuf, int fixedlen)
{
  int len = 0;

  /* Get data until `.` or `[`. */
  const char *p = *path;
  while (*p && !ELEM(*p, '.', '[')) {
    len++;
    p++;
  }

  /* Empty, return. */
  if (UNLIKELY(len == 0)) {
    return nullptr;
  }

  /* Try to use fixed buffer if possible. */
  char *buf = (len + 1 < fixedlen) ? fixedbuf :
                                     (char *)MEM_mallocN(sizeof(char) * (len + 1), __func__);
  memcpy(buf, *path, sizeof(char) * len);
  buf[len] = '\0';

  if (*p == '.') {
    p++;
  }
  *path = p;

  return buf;
}

/**
 * Extract the first token in brackets from `path` (with quoted text support).
 *
 * - `[0]` -> `0`
 * - `["Some\"Quote"]` -> `Some"Quote`
 *
 * \param path: Extract the token from path, step the pointer to the beginning of the next token
 * (past quoted text and brackets).
 * \return The nil terminated token.
 */
static char *rna_path_token_in_brackets(const char **path,
                                        char *fixedbuf,
                                        int fixedlen,
                                        bool *r_quoted)
{
  int len = 0;
  bool quoted = false;

  BLI_assert(r_quoted != nullptr);

  /* Get data between `[]`, check escaping quotes and back-slashes with #BLI_str_unescape. */
  if (UNLIKELY(**path != '[')) {
    return nullptr;
  }

  (*path)++;
  const char *p = *path;

  /* 2 kinds of look-ups now, quoted or unquoted. */
  if (*p == '"') {
    /* Find the matching quote. */
    (*path)++;
    p = *path;
    const char *p_end = BLI_str_escape_find_quote(p);
    if (p_end == nullptr) {
      /* No Matching quote. */
      return nullptr;
    }
    /* Exclude the last quote from the length. */
    len += (p_end - p);

    /* Skip the last quoted char to get the `]`. */
    p_end += 1;
    p = p_end;
    quoted = true;
  }
  else {
    /* Find the matching bracket. */
    while (*p && (*p != ']')) {
      len++;
      p++;
    }
  }

  if (UNLIKELY(*p != ']')) {
    return nullptr;
  }

  /* Empty, return. */
  if (UNLIKELY(len == 0)) {
    return nullptr;
  }

  /* Try to use fixed buffer if possible. */
  char *buf = (len + 1 < fixedlen) ? fixedbuf :
                                     (char *)MEM_mallocN(sizeof(char) * (len + 1), __func__);

  /* Copy string, taking into account escaped ']' */
  if (quoted) {
    BLI_str_unescape(buf, *path, len);
    /* +1 to step over the last quote. */
    BLI_assert((*path)[len] == '"');
    p = (*path) + len + 1;
  }
  else {
    memcpy(buf, *path, sizeof(char) * len);
    buf[len] = '\0';
  }
  /* Set path to start of next token. */
  if (*p == ']') {
    p++;
  }
  if (*p == '.') {
    p++;
  }
  *path = p;

  *r_quoted = quoted;

  return buf;
}

/**
 * \return true when the key in the path is correctly parsed and found in the collection
 * or when the path is empty.
 */
static bool rna_path_parse_collection_key(const char **path,
                                          PointerRNA *ptr,
                                          PropertyRNA *prop,
                                          PointerRNA *r_nextptr)
{
  char fixedbuf[256];
  int intkey;

  *r_nextptr = *ptr;

  /* end of path, ok */
  if (!(**path)) {
    return true;
  }

  bool found = false;
  if (**path == '[') {
    bool quoted;
    char *token;

    /* resolve the lookup with [] brackets */
    token = rna_path_token_in_brackets(path, fixedbuf, sizeof(fixedbuf), &quoted);

    if (!token) {
      return false;
    }

    /* check for "" to see if it is a string */
    if (quoted) {
      if (RNA_property_collection_lookup_string(ptr, prop, token, r_nextptr)) {
        found = true;
      }
      else {
        r_nextptr->data = nullptr;
      }
    }
    else {
      /* otherwise do int lookup */
      intkey = atoi(token);
      if (intkey == 0 && (token[0] != '0' || token[1] != '\0')) {
        return false; /* we can be sure the fixedbuf was used in this case */
      }
      if (RNA_property_collection_lookup_int(ptr, prop, intkey, r_nextptr)) {
        found = true;
      }
      else {
        r_nextptr->data = nullptr;
      }
    }

    if (token != fixedbuf) {
      MEM_freeN(token);
    }
  }
  else {
    if (RNA_property_collection_type_get(ptr, prop, r_nextptr)) {
      found = true;
    }
    else {
      /* ensure we quit on invalid values */
      r_nextptr->data = nullptr;
    }
  }

  return found;
}

static bool rna_path_parse_array_index(const char **path,
                                       PointerRNA *ptr,
                                       PropertyRNA *prop,
                                       int *r_index)
{
  char fixedbuf[256];
  int index_arr[RNA_MAX_ARRAY_DIMENSION] = {0};
  int len[RNA_MAX_ARRAY_DIMENSION];
  const int dim = RNA_property_array_dimension(ptr, prop, len);
  int i;

  *r_index = -1;

  /* end of path, ok */
  if (!(**path)) {
    return true;
  }

  for (i = 0; i < dim; i++) {
    int temp_index = -1;
    char *token;

    /* multi index resolve */
    if (**path == '[') {
      bool quoted;
      token = rna_path_token_in_brackets(path, fixedbuf, sizeof(fixedbuf), &quoted);

      if (token == nullptr) {
        /* invalid syntax blah[] */
        return false;
      }
      /* check for "" to see if it is a string */
      if (quoted) {
        temp_index = RNA_property_array_item_index(prop, *token);
      }
      else {
        /* otherwise do int lookup */
        temp_index = atoi(token);

        if (temp_index == 0 && (token[0] != '0' || token[1] != '\0')) {
          if (token != fixedbuf) {
            MEM_freeN(token);
          }

          return false;
        }
      }
    }
    else if (dim == 1) {
      /* location.x || scale.X, single dimension arrays only */
      token = rna_path_token(path, fixedbuf, sizeof(fixedbuf));
      if (token == nullptr) {
        /* invalid syntax blah. */
        return false;
      }
      temp_index = RNA_property_array_item_index(prop, *token);
    }
    else {
      /* just to avoid uninitialized pointer use */
      token = fixedbuf;
    }

    if (token != fixedbuf) {
      MEM_freeN(token);
    }

    /* out of range */
    if (temp_index < 0 || temp_index >= len[i]) {
      return false;
    }

    index_arr[i] = temp_index;
    /* end multi index resolve */
  }

  /* arrays always contain numbers so further values are not valid */
  if (**path) {
    return false;
  }

  /* flatten index over all dimensions */
  {
    int totdim = 1;
    int flat_index = 0;

    for (i = dim - 1; i >= 0; i--) {
      flat_index += index_arr[i] * totdim;
      totdim *= len[i];
    }

    *r_index = flat_index;
  }
  return true;
}

/**
 * Generic rna path parser.
 *
 * \note All parameters besides \a ptr and \a path are optional.
 *
 * \param ptr: The root of given RNA path.
 * \param path: The RNA path.
 * \param r_ptr: The final RNA data holding the last property in \a path.
 * \param r_prop: The final property of \a r_ptr, from \a path.
 * \param r_index: The final index in the \a r_prop, if defined by \a path.
 * \param r_item_ptr: Only valid for Pointer and Collection, return the actual value of the
 *                    pointer, or of the collection item.
 *                    Mutually exclusive with \a eval_pointer option.
 * \param r_elements: A list of \a PropertyElemRNA items(pairs of \a PointerRNA, \a PropertyRNA
 *                    that represent the whole given \a path).
 * \param eval_pointer: If \a true, and \a path leads to a Pointer property, or an item in a
 *                      Collection property, \a r_ptr will be set to the value of that property,
 *                      and \a r_prop will be null.
 *                      Mutually exclusive with \a r_item_ptr.
 *
 * \return \a true on success, \a false if the path is somehow invalid.
 */
static bool rna_path_parse(const PointerRNA *ptr,
                           const char *path,
                           PointerRNA *r_ptr,
                           PropertyRNA **r_prop,
                           int *r_index,
                           PointerRNA *r_item_ptr,
                           ListBase *r_elements,
                           const bool eval_pointer)
{
  BLI_assert(r_item_ptr == nullptr || !eval_pointer);
  PropertyRNA *prop;
  PointerRNA curptr, nextptr;
  PropertyElemRNA *prop_elem = nullptr;
  int index = -1;
  char fixedbuf[256];
  int type;
  const bool do_item_ptr = r_item_ptr != nullptr && !eval_pointer;

  if (do_item_ptr) {
    RNA_POINTER_INVALIDATE(&nextptr);
  }

  prop = nullptr;
  curptr = *ptr;

  if (path == nullptr || *path == '\0') {
    return false;
  }

  while (*path) {
    if (do_item_ptr) {
      RNA_POINTER_INVALIDATE(&nextptr);
    }

    const bool use_id_prop = (*path == '[');
    /* Custom property lookup: e.g. `C.object["someprop"]`. */

    if (!curptr.data) {
      return false;
    }

    /* look up property name in current struct */
    bool quoted = false;
    char *token = use_id_prop ?
                      rna_path_token_in_brackets(&path, fixedbuf, sizeof(fixedbuf), &quoted) :
                      rna_path_token(&path, fixedbuf, sizeof(fixedbuf));
    if (!token) {
      return false;
    }

    prop = nullptr;
    if (use_id_prop) { /* look up property name in current struct */
      IDProperty *group = RNA_struct_idprops(&curptr, 0);
      if (group && quoted) {
        prop = (PropertyRNA *)IDP_GetPropertyFromGroup(group, token);
      }
    }
    else {
      prop = RNA_struct_find_property(&curptr, token);
    }

    if (token != fixedbuf) {
      MEM_freeN(token);
    }

    if (!prop) {
      return false;
    }

    if (r_elements) {
      prop_elem = MEM_cnew<PropertyElemRNA>(__func__);
      prop_elem->ptr = curptr;
      prop_elem->prop = prop;
      prop_elem->index = -1; /* index will be added later, if needed. */
      BLI_addtail(r_elements, prop_elem);
    }

    type = RNA_property_type(prop);

    /* now look up the value of this property if it is a pointer or
     * collection, otherwise return the property rna so that the
     * caller can read the value of the property itself */
    switch (type) {
      case PROP_POINTER: {
        /* resolve pointer if further path elements follow
         * or explicitly requested
         */
        if (do_item_ptr || eval_pointer || *path != '\0') {
          nextptr = RNA_property_pointer_get(&curptr, prop);
        }

        if (eval_pointer || *path != '\0') {
          curptr = nextptr;
          prop = nullptr; /* now we have a PointerRNA, the prop is our parent so forget it */
          index = -1;
        }
        break;
      }
      case PROP_COLLECTION: {
        /* Resolve pointer if further path elements follow.
         * Note that if path is empty, rna_path_parse_collection_key will do nothing anyway,
         * so do_item_ptr is of no use in that case.
         */
        if (*path) {
          if (!rna_path_parse_collection_key(&path, &curptr, prop, &nextptr)) {
            return false;
          }

          if (eval_pointer || *path != '\0') {
            curptr = nextptr;
            prop = nullptr; /* now we have a PointerRNA, the prop is our parent so forget it */
            index = -1;
          }
        }
        break;
      }
      default:
        if (r_index || prop_elem) {
          if (!rna_path_parse_array_index(&path, &curptr, prop, &index)) {
            return false;
          }

          if (prop_elem) {
            prop_elem->index = index;
          }
        }
        break;
    }
  }

  if (r_ptr) {
    *r_ptr = curptr;
  }
  if (r_prop) {
    *r_prop = prop;
  }
  if (r_index) {
    *r_index = index;
  }
  if (r_item_ptr && do_item_ptr) {
    *r_item_ptr = nextptr;
  }

  if (prop_elem &&
      (prop_elem->ptr.data != curptr.data || prop_elem->prop != prop || prop_elem->index != index))
  {
    prop_elem = MEM_cnew<PropertyElemRNA>(__func__);
    prop_elem->ptr = curptr;
    prop_elem->prop = prop;
    prop_elem->index = index;
    BLI_addtail(r_elements, prop_elem);
  }

  return true;
}

bool RNA_path_resolve(const PointerRNA *ptr,
                      const char *path,
                      PointerRNA *r_ptr,
                      PropertyRNA **r_prop)
{
  if (!rna_path_parse(ptr, path, r_ptr, r_prop, nullptr, nullptr, nullptr, true)) {
    return false;
  }

  return r_ptr->data != nullptr;
}

bool RNA_path_resolve_full(
    const PointerRNA *ptr, const char *path, PointerRNA *r_ptr, PropertyRNA **r_prop, int *r_index)
{
  if (!rna_path_parse(ptr, path, r_ptr, r_prop, r_index, nullptr, nullptr, true)) {
    return false;
  }

  return r_ptr->data != nullptr;
}

bool RNA_path_resolve_full_maybe_null(
    const PointerRNA *ptr, const char *path, PointerRNA *r_ptr, PropertyRNA **r_prop, int *r_index)
{
  return rna_path_parse(ptr, path, r_ptr, r_prop, r_index, nullptr, nullptr, true);
}

bool RNA_path_resolve_property(const PointerRNA *ptr,
                               const char *path,
                               PointerRNA *r_ptr,
                               PropertyRNA **r_prop)
{
  if (!rna_path_parse(ptr, path, r_ptr, r_prop, nullptr, nullptr, nullptr, false)) {
    return false;
  }

  return r_ptr->data != nullptr && *r_prop != nullptr;
}

bool RNA_path_resolve_property_full(
    const PointerRNA *ptr, const char *path, PointerRNA *r_ptr, PropertyRNA **r_prop, int *r_index)
{
  if (!rna_path_parse(ptr, path, r_ptr, r_prop, r_index, nullptr, nullptr, false)) {
    return false;
  }

  return r_ptr->data != nullptr && *r_prop != nullptr;
}

bool RNA_path_resolve_property_and_item_pointer(const PointerRNA *ptr,
                                                const char *path,
                                                PointerRNA *r_ptr,
                                                PropertyRNA **r_prop,
                                                PointerRNA *r_item_ptr)
{
  if (!rna_path_parse(ptr, path, r_ptr, r_prop, nullptr, r_item_ptr, nullptr, false)) {
    return false;
  }

  return r_ptr->data != nullptr && *r_prop != nullptr;
}

bool RNA_path_resolve_property_and_item_pointer_full(const PointerRNA *ptr,
                                                     const char *path,
                                                     PointerRNA *r_ptr,
                                                     PropertyRNA **r_prop,
                                                     int *r_index,
                                                     PointerRNA *r_item_ptr)
{
  if (!rna_path_parse(ptr, path, r_ptr, r_prop, r_index, r_item_ptr, nullptr, false)) {
    return false;
  }

  return r_ptr->data != nullptr && *r_prop != nullptr;
}
bool RNA_path_resolve_elements(PointerRNA *ptr, const char *path, ListBase *r_elements)
{
  return rna_path_parse(ptr, path, nullptr, nullptr, nullptr, nullptr, r_elements, false);
}

char *RNA_path_append(const char *path,
                      const PointerRNA * /*ptr*/,
                      PropertyRNA *prop,
                      int intkey,
                      const char *strkey)
{
  DynStr *dynstr;
  char *result;

  dynstr = BLI_dynstr_new();

  /* add .identifier */
  if (path) {
    BLI_dynstr_append(dynstr, path);
    if (*path) {
      BLI_dynstr_append(dynstr, ".");
    }
  }

  BLI_dynstr_append(dynstr, RNA_property_identifier(prop));

  const bool has_key = (intkey > -1) || (strkey != nullptr);
  if (has_key && (RNA_property_type(prop) == PROP_COLLECTION)) {
    /* add ["strkey"] or [intkey] */
    BLI_dynstr_append(dynstr, "[");

    if (strkey) {
      const int strkey_esc_max_size = (strlen(strkey) * 2) + 1;
      char *strkey_esc = static_cast<char *>(BLI_array_alloca(strkey_esc, strkey_esc_max_size));
      BLI_str_escape(strkey_esc, strkey, strkey_esc_max_size);
      BLI_dynstr_append(dynstr, "\"");
      BLI_dynstr_append(dynstr, strkey_esc);
      BLI_dynstr_append(dynstr, "\"");
    }
    else {
      char appendstr[128];
      SNPRINTF(appendstr, "%d", intkey);
      BLI_dynstr_append(dynstr, appendstr);
    }

    BLI_dynstr_append(dynstr, "]");
  }

  result = BLI_dynstr_get_cstring(dynstr);
  BLI_dynstr_free(dynstr);

  return result;
}

/* Having both path append & back seems like it could be useful,
 * this function isn't used at the moment. */
static UNUSED_FUNCTION_WITH_RETURN_TYPE(char *, RNA_path_back)(const char *path)
{
  char fixedbuf[256];
  const char *previous, *current;
  char *result;
  int i;

  if (!path) {
    return nullptr;
  }

  previous = nullptr;
  current = path;

  /* parse token by token until the end, then we back up to the previous
   * position and strip of the next token to get the path one step back */
  while (*current) {
    char *token;

    token = rna_path_token(&current, fixedbuf, sizeof(fixedbuf));

    if (!token) {
      return nullptr;
    }
    if (token != fixedbuf) {
      MEM_freeN(token);
    }

    /* in case of collection we also need to strip off [] */
    bool quoted;
    token = rna_path_token_in_brackets(&current, fixedbuf, sizeof(fixedbuf), &quoted);
    if (token && token != fixedbuf) {
      MEM_freeN(token);
    }

    if (!*current) {
      break;
    }

    previous = current;
  }

  if (!previous) {
    return nullptr;
  }

  /* copy and strip off last token */
  i = previous - path;
  result = BLI_strdup(path);

  if (i > 0 && result[i - 1] == '.') {
    i--;
  }
  result[i] = 0;

  return result;
}

const char *RNA_path_array_index_token_find(const char *rna_path, const PropertyRNA *array_prop)
{
  if (array_prop != nullptr) {
    if (!ELEM(array_prop->type, PROP_BOOLEAN, PROP_INT, PROP_FLOAT)) {
      BLI_assert(array_prop->arraydimension == 0);
      return nullptr;
    }
    if (array_prop->arraydimension == 0) {
      return nullptr;
    }
  }

  /* Valid 'array part' of a rna path can only have '[', ']' and digit characters.
   * It may have more than one of those (e.g. `[12][1]`) in case of multi-dimensional arrays. */
  if (UNLIKELY(rna_path[0] == '\0')) {
    return nullptr;
  }
  size_t rna_path_len = size_t(strlen(rna_path)) - 1;
  if (rna_path[rna_path_len] != ']') {
    return nullptr;
  }

  const char *last_valid_index_token_start = nullptr;
  while (rna_path_len--) {
    switch (rna_path[rna_path_len]) {
      case '[':
        if (rna_path_len <= 0 || rna_path[rna_path_len - 1] != ']') {
          return &rna_path[rna_path_len];
        }
        last_valid_index_token_start = &rna_path[rna_path_len];
        rna_path_len--;
        break;
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
        break;
      default:
        return last_valid_index_token_start;
    }
  }
  return last_valid_index_token_start;
}

/* generic path search func
 * if its needed this could also reference the IDProperty direct */
typedef struct IDP_Chain {
  IDP_Chain *up; /* parent member, reverse and set to child for path conversion. */

  const char *name;
  int index;

} IDP_Chain;

static char *rna_idp_path_create(IDP_Chain *child_link)
{
  DynStr *dynstr = BLI_dynstr_new();
  char *path;
  bool is_first = true;

  IDP_Chain *link = child_link;

  /* reverse the list */
  IDP_Chain *link_prev;
  link_prev = nullptr;
  while (link) {
    IDP_Chain *link_next = link->up;
    link->up = link_prev;
    link_prev = link;
    link = link_next;
  }

  for (link = link_prev; link; link = link->up) {
    /* pass */
    if (link->index >= 0) {
      BLI_dynstr_appendf(dynstr, is_first ? "%s[%d]" : ".%s[%d]", link->name, link->index);
    }
    else {
      BLI_dynstr_appendf(dynstr, is_first ? "%s" : ".%s", link->name);
    }

    is_first = false;
  }

  path = BLI_dynstr_get_cstring(dynstr);
  BLI_dynstr_free(dynstr);

  if (*path == '\0') {
    MEM_freeN(path);
    path = nullptr;
  }

  return path;
}

static char *rna_idp_path(PointerRNA *ptr,
                          IDProperty *haystack,
                          IDProperty *needle,
                          IDP_Chain *parent_link)
{
  char *path = nullptr;
  IDP_Chain link;

  IDProperty *iter;
  int i;

  BLI_assert(haystack->type == IDP_GROUP);

  link.up = parent_link;
  /* Always set both name and index, else a stale value might get used. */
  link.name = nullptr;
  link.index = -1;

  for (i = 0, iter = static_cast<IDProperty *>(haystack->data.group.first); iter;
       iter = iter->next, i++)
  {
    if (needle == iter) { /* found! */
      link.name = iter->name;
      link.index = -1;
      path = rna_idp_path_create(&link);
      break;
    }

    /* Early out in case the IDProperty type cannot contain RNA properties. */
    if (!ELEM(iter->type, IDP_GROUP, IDP_IDPARRAY)) {
      continue;
    }

    /* Ensure this is RNA. */
    /* NOTE: `iter` might be a fully user-defined IDProperty (a.k.a. custom data), which name
     * collides with an actual fully static RNA property of the same struct (which would then not
     * be flagged with `PROP_IDPROPERTY`).
     *
     * That case must be ignored here, we only want to deal with runtime RNA properties stored in
     * IDProps.
     *
     * See #84091. */
    PropertyRNA *prop = RNA_struct_find_property(ptr, iter->name);
    if (prop == nullptr || (prop->flag & PROP_IDPROPERTY) == 0) {
      continue;
    }

    if (iter->type == IDP_GROUP) {
      if (prop->type == PROP_POINTER) {
        PointerRNA child_ptr = RNA_property_pointer_get(ptr, prop);
        if (RNA_pointer_is_null(&child_ptr)) {
          /* Pointer ID prop might be a 'leaf' in the IDProp group hierarchy, in which case a null
           * value is perfectly valid. Just means it won't match the searched needle. */
          continue;
        }

        link.name = iter->name;
        link.index = -1;
        if ((path = rna_idp_path(&child_ptr, iter, needle, &link))) {
          break;
        }
      }
    }
    else if (iter->type == IDP_IDPARRAY) {
      if (prop->type == PROP_COLLECTION) {
        IDProperty *array = IDP_IDPArray(iter);
        if (needle >= array && needle < (iter->len + array)) { /* found! */
          link.name = iter->name;
          link.index = int(needle - array);
          path = rna_idp_path_create(&link);
          break;
        }
        int j;
        link.name = iter->name;
        for (j = 0; j < iter->len; j++, array++) {
          PointerRNA child_ptr;
          if (RNA_property_collection_lookup_int(ptr, prop, j, &child_ptr)) {
            if (RNA_pointer_is_null(&child_ptr)) {
              /* Array item ID prop might be a 'leaf' in the IDProp group hierarchy, in which case
               * a null value is perfectly valid. Just means it won't match the searched needle. */
              continue;
            }
            link.index = j;
            if ((path = rna_idp_path(&child_ptr, array, needle, &link))) {
              break;
            }
          }
        }
        if (path) {
          break;
        }
      }
    }
  }

  return path;
}

char *RNA_path_from_struct_to_idproperty(PointerRNA *ptr, IDProperty *needle)
{
  IDProperty *haystack = RNA_struct_idprops(ptr, false);

  if (haystack) { /* can fail when called on bones */
    return rna_idp_path(ptr, haystack, needle, nullptr);
  }
  return nullptr;
}

static char *rna_path_from_ID_to_idpgroup(const PointerRNA *ptr)
{
  PointerRNA id_ptr;

  BLI_assert(ptr->owner_id != nullptr);

  /* TODO: Support Bones/PoseBones. no pointers stored to the bones from here, only the ID.
   *       See example in #25746.
   *       Unless this is added only way to find this is to also search
   *       all bones and pose bones of an armature or object.
   */
  RNA_id_pointer_create(ptr->owner_id, &id_ptr);

  return RNA_path_from_struct_to_idproperty(&id_ptr, static_cast<IDProperty *>(ptr->data));
}

ID *RNA_find_real_ID_and_path(ID *id, const char **r_path)
{
  if (r_path) {
    *r_path = "";
  }

  if ((id == nullptr) || (id->flag & LIB_EMBEDDED_DATA) == 0) {
    return id;
  }

  if (r_path) {
    switch (GS(id->name)) {
      case ID_NT:
        *r_path = "node_tree";
        break;
      case ID_GR:
        *r_path = "collection";
        break;
      default:
        BLI_assert_msg(0, "Missing handling of embedded id type.");
    }
  }

  ID *owner_id = BKE_id_owner_get(id);
  BLI_assert_msg(owner_id != nullptr, "Missing handling of embedded id type.");
  return (owner_id != nullptr) ? owner_id : id;
}

static char *rna_prepend_real_ID_path(Main * /*bmain*/, ID *id, char *path, ID **r_real_id)
{
  if (r_real_id != nullptr) {
    *r_real_id = nullptr;
  }

  const char *prefix;
  ID *real_id = RNA_find_real_ID_and_path(id, &prefix);

  if (r_real_id != nullptr) {
    *r_real_id = real_id;
  }

  if (path != nullptr) {
    char *new_path = nullptr;

    if (real_id) {
      if (prefix[0]) {
        new_path = BLI_sprintfN("%s%s%s", prefix, path[0] == '[' ? "" : ".", path);
      }
      else {
        return path;
      }
    }

    MEM_freeN(path);
    return new_path;
  }
  return prefix[0] != '\0' ? BLI_strdup(prefix) : nullptr;
}

char *RNA_path_from_ID_to_struct(const PointerRNA *ptr)
{
  char *ptrpath = nullptr;

  if (!ptr->owner_id || !ptr->data) {
    return nullptr;
  }

  if (!RNA_struct_is_ID(ptr->type)) {
    if (ptr->type->path) {
      /* if type has a path to some ID, use it */
      ptrpath = ptr->type->path((PointerRNA *)ptr);
    }
    else if (ptr->type->nested && RNA_struct_is_ID(ptr->type->nested)) {
      PointerRNA parentptr;
      PropertyRNA *userprop;

      /* find the property in the struct we're nested in that references this struct, and
       * use its identifier as the first part of the path used...
       */
      RNA_id_pointer_create(ptr->owner_id, &parentptr);
      userprop = rna_struct_find_nested(&parentptr, ptr->type);

      if (userprop) {
        ptrpath = BLI_strdup(RNA_property_identifier(userprop));
      }
      else {
        return nullptr; /* can't do anything about this case yet... */
      }
    }
    else if (RNA_struct_is_a(ptr->type, &RNA_PropertyGroup)) {
      /* special case, easier to deal with here than in ptr->type->path() */
      return rna_path_from_ID_to_idpgroup(ptr);
    }
    else {
      return nullptr;
    }
  }

  return ptrpath;
}

char *RNA_path_from_real_ID_to_struct(Main *bmain, const PointerRNA *ptr, ID **r_real)
{
  char *path = RNA_path_from_ID_to_struct(ptr);

  /* Null path is valid in that case, when given struct is an ID one. */
  return rna_prepend_real_ID_path(bmain, ptr->owner_id, path, r_real);
}

static void rna_path_array_multi_from_flat_index(const int dimsize[RNA_MAX_ARRAY_LENGTH],
                                                 const int totdims,
                                                 const int index_dim,
                                                 int index,
                                                 int r_index_multi[RNA_MAX_ARRAY_LENGTH])
{
  int dimsize_step[RNA_MAX_ARRAY_LENGTH + 1];
  int i = totdims - 1;
  dimsize_step[i + 1] = 1;
  dimsize_step[i] = dimsize[i];
  while (--i != -1) {
    dimsize_step[i] = dimsize[i] * dimsize_step[i + 1];
  }
  while (++i != index_dim) {
    int index_round = index / dimsize_step[i + 1];
    r_index_multi[i] = index_round;
    index -= (index_round * dimsize_step[i + 1]);
  }
  BLI_assert(index == 0);
}

static void rna_path_array_multi_string_from_flat_index(const PointerRNA *ptr,
                                                        PropertyRNA *prop,
                                                        int index_dim,
                                                        int index,
                                                        char *index_str,
                                                        int index_str_len)
{
  int dimsize[RNA_MAX_ARRAY_LENGTH];
  int totdims = RNA_property_array_dimension(ptr, prop, dimsize);
  int index_multi[RNA_MAX_ARRAY_LENGTH];

  rna_path_array_multi_from_flat_index(dimsize, totdims, index_dim, index, index_multi);

  for (int i = 0, offset = 0; (i < index_dim) && (offset < index_str_len); i++) {
    offset += BLI_snprintf_rlen(
        &index_str[offset], index_str_len - offset, "[%d]", index_multi[i]);
  }
}

char *RNA_path_from_ID_to_property_index(const PointerRNA *ptr,
                                         PropertyRNA *prop,
                                         int index_dim,
                                         int index)
{
  const bool is_rna = (prop->magic == RNA_MAGIC);
  const char *propname;
  char *ptrpath, *path;

  if (!ptr->owner_id || !ptr->data) {
    return nullptr;
  }

  /* path from ID to the struct holding this property */
  ptrpath = RNA_path_from_ID_to_struct(ptr);

  propname = RNA_property_identifier(prop);

  /* support indexing w/ multi-dimensional arrays */
  char index_str[RNA_MAX_ARRAY_LENGTH * 12 + 1];
  if (index_dim == 0) {
    index_str[0] = '\0';
  }
  else {
    rna_path_array_multi_string_from_flat_index(
        ptr, prop, index_dim, index, index_str, sizeof(index_str));
  }

  if (ptrpath) {
    if (is_rna) {
      path = BLI_sprintfN("%s.%s%s", ptrpath, propname, index_str);
    }
    else {
      char propname_esc[MAX_IDPROP_NAME * 2];
      BLI_str_escape(propname_esc, propname, sizeof(propname_esc));
      path = BLI_sprintfN("%s[\"%s\"]%s", ptrpath, propname_esc, index_str);
    }
    MEM_freeN(ptrpath);
  }
  else if (RNA_struct_is_ID(ptr->type)) {
    if (is_rna) {
      path = BLI_sprintfN("%s%s", propname, index_str);
    }
    else {
      char propname_esc[MAX_IDPROP_NAME * 2];
      BLI_str_escape(propname_esc, propname, sizeof(propname_esc));
      path = BLI_sprintfN("[\"%s\"]%s", propname_esc, index_str);
    }
  }
  else {
    path = nullptr;
  }

  return path;
}

char *RNA_path_from_ID_to_property(const PointerRNA *ptr, PropertyRNA *prop)
{
  return RNA_path_from_ID_to_property_index(ptr, prop, 0, -1);
}

char *RNA_path_from_real_ID_to_property_index(Main *bmain,
                                              const PointerRNA *ptr,
                                              PropertyRNA *prop,
                                              int index_dim,
                                              int index,
                                              ID **r_real_id)
{
  char *path = RNA_path_from_ID_to_property_index(ptr, prop, index_dim, index);

  /* Null path is always an error here, in that case do not return the 'fake ID from real ID' part
   * of the path either. */
  return path != nullptr ? rna_prepend_real_ID_path(bmain, ptr->owner_id, path, r_real_id) :
                           nullptr;
}

char *RNA_path_resolve_from_type_to_property(const PointerRNA *ptr,
                                             PropertyRNA *prop,
                                             const StructRNA *type)
{
  /* Try to recursively find an "type"'d ancestor,
   * to handle situations where path from ID is not enough. */
  PointerRNA idptr;
  ListBase path_elems = {nullptr};
  char *path = nullptr;
  char *full_path = RNA_path_from_ID_to_property(ptr, prop);

  if (full_path == nullptr) {
    return nullptr;
  }

  RNA_id_pointer_create(ptr->owner_id, &idptr);

  if (RNA_path_resolve_elements(&idptr, full_path, &path_elems)) {
    LISTBASE_FOREACH_BACKWARD (PropertyElemRNA *, prop_elem, &path_elems) {
      if (RNA_struct_is_a(prop_elem->ptr.type, type)) {
        char *ref_path = RNA_path_from_ID_to_struct(&prop_elem->ptr);
        if (ref_path) {
          path = BLI_strdup(full_path + strlen(ref_path) + 1); /* +1 for the linking '.' */
          MEM_freeN(ref_path);
        }
        break;
      }
    }

    BLI_freelistN(&path_elems);
  }

  MEM_freeN(full_path);
  return path;
}

char *RNA_path_full_ID_py(ID *id)
{
  const char *path;
  ID *id_real = RNA_find_real_ID_and_path(id, &path);

  if (id_real) {
    id = id_real;
  }
  else {
    path = "";
  }

  char lib_filepath_esc[(sizeof(id->lib->filepath) * 2) + 4];
  if (ID_IS_LINKED(id)) {
    int ofs = 0;
    memcpy(lib_filepath_esc, ", \"", 3);
    ofs += 3;
    ofs += BLI_str_escape(lib_filepath_esc + ofs, id->lib->filepath, sizeof(lib_filepath_esc));
    memcpy(lib_filepath_esc + ofs, "\"", 2);
  }
  else {
    lib_filepath_esc[0] = '\0';
  }

  char id_esc[(sizeof(id->name) - 2) * 2];
  BLI_str_escape(id_esc, id->name + 2, sizeof(id_esc));

  return BLI_sprintfN("bpy.data.%s[\"%s\"%s]%s%s",
                      BKE_idtype_idcode_to_name_plural(GS(id->name)),
                      id_esc,
                      lib_filepath_esc,
                      path[0] ? "." : "",
                      path);
}

char *RNA_path_full_struct_py(const PointerRNA *ptr)
{
  char *id_path;
  char *data_path;

  char *ret;

  if (!ptr->owner_id) {
    return nullptr;
  }

  /* never fails */
  id_path = RNA_path_full_ID_py(ptr->owner_id);

  data_path = RNA_path_from_ID_to_struct(ptr);

  /* XXX data_path may be null (see #36788),
   * do we want to get the 'bpy.data.foo["bar"].(null)' stuff? */
  ret = BLI_sprintfN("%s.%s", id_path, data_path);

  if (data_path) {
    MEM_freeN(data_path);
  }
  MEM_freeN(id_path);

  return ret;
}

char *RNA_path_full_property_py_ex(const PointerRNA *ptr,
                                   PropertyRNA *prop,
                                   int index,
                                   bool use_fallback)
{
  char *id_path;
  const char *data_delim;
  const char *data_path;
  bool data_path_free;

  char *ret;

  if (!ptr->owner_id) {
    return nullptr;
  }

  /* never fails */
  id_path = RNA_path_full_ID_py(ptr->owner_id);

  data_path = RNA_path_from_ID_to_property(ptr, prop);
  if (data_path) {
    data_delim = (data_path[0] == '[') ? "" : ".";
    data_path_free = true;
  }
  else {
    if (use_fallback) {
      /* Fuzzy fallback. Be explicit in our ignorance. */
      data_path = RNA_property_identifier(prop);
      data_delim = " ... ";
    }
    else {
      data_delim = ".";
    }
    data_path_free = false;
  }

  if ((index == -1) || (RNA_property_array_check(prop) == false)) {
    ret = BLI_sprintfN("%s%s%s", id_path, data_delim, data_path);
  }
  else {
    ret = BLI_sprintfN("%s%s%s[%d]", id_path, data_delim, data_path, index);
  }
  MEM_freeN(id_path);
  if (data_path_free) {
    MEM_freeN((void *)data_path);
  }

  return ret;
}

char *RNA_path_full_property_py(const PointerRNA *ptr, PropertyRNA *prop, int index)
{
  return RNA_path_full_property_py_ex(ptr, prop, index, false);
}

char *RNA_path_struct_property_py(PointerRNA *ptr, PropertyRNA *prop, int index)
{
  char *data_path;

  char *ret;

  if (!ptr->owner_id) {
    return nullptr;
  }

  data_path = RNA_path_from_ID_to_property(ptr, prop);

  if (data_path == nullptr) {
    /* This may not be an ID at all, check for simple when pointer owns property.
     * TODO: more complex nested case. */
    if (!RNA_struct_is_ID(ptr->type)) {
      const char *prop_identifier = RNA_property_identifier(prop);
      if (RNA_struct_find_property(ptr, prop_identifier) == prop) {
        data_path = BLI_strdup(prop_identifier);
      }
    }
  }

  if ((index == -1) || (RNA_property_array_check(prop) == false)) {
    ret = BLI_strdup(data_path);
  }
  else {
    ret = BLI_sprintfN("%s[%d]", data_path, index);
  }

  if (data_path) {
    MEM_freeN(data_path);
  }

  return ret;
}

char *RNA_path_property_py(const PointerRNA * /*ptr*/, PropertyRNA *prop, int index)
{
  const bool is_rna = (prop->magic == RNA_MAGIC);
  const char *propname = RNA_property_identifier(prop);
  char *ret;

  if ((index == -1) || (RNA_property_array_check(prop) == false)) {
    if (is_rna) {
      ret = BLI_strdup(propname);
    }
    else {
      char propname_esc[MAX_IDPROP_NAME * 2];
      BLI_str_escape(propname_esc, propname, sizeof(propname_esc));
      ret = BLI_sprintfN("[\"%s\"]", propname_esc);
    }
  }
  else {
    if (is_rna) {
      ret = BLI_sprintfN("%s[%d]", propname, index);
    }
    else {
      char propname_esc[MAX_IDPROP_NAME * 2];
      BLI_str_escape(propname_esc, propname, sizeof(propname_esc));
      ret = BLI_sprintfN("[\"%s\"][%d]", propname_esc, index);
    }
  }

  return ret;
}
