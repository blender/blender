/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cfloat>
#include <climits>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <fmt/format.h>

#include "BLI_listbase.h"
#include "BLI_math_base.h"
#include "BLI_set.hh"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_idprop.hh"
#include "BKE_lib_id.hh"

#include "CLG_log.h"

#include "MEM_guardedalloc.h"

#include "BLO_read_write.hh"

#include "BLI_strict_flags.h" /* IWYU pragma: keep. Keep last. */

/* IDPropertyTemplate is a union in DNA_ID.h */

/**
 * if the new is 'IDP_ARRAY_REALLOC_LIMIT' items less,
 * than #IDProperty.totallen, reallocate anyway.
 */
#define IDP_ARRAY_REALLOC_LIMIT 200

static CLG_LogRef LOG = {"lib.idprop"};

/** Local size table, aligned with #eIDPropertyType. */
static size_t idp_size_table[] = {
    1,                 /* #IDP_STRING */
    sizeof(int),       /* #IDP_INT */
    sizeof(float),     /* #IDP_FLOAT */
    sizeof(float[3]),  /* DEPRECATED (was vector). */
    sizeof(float[16]), /* DEPRECATED (was matrix). */
    0,                 /* #IDP_ARRAY (no fixed size). */
    sizeof(ListBase),  /* #IDP_GROUP */
    sizeof(void *),    /* #IDP_ID */
    sizeof(double),    /* #IDP_DOUBLE */
    0,                 /* #IDP_IDPARRAY (no fixed size). */
    sizeof(int8_t),    /* #IDP_BOOLEAN */
    sizeof(int),       /* #IDP_ENUM */
};

/* -------------------------------------------------------------------- */
/** \name Array Functions (IDP Array API)
 * \{ */

#define GETPROP(prop, i) &(IDP_property_array_get(prop)[i])

IDProperty *IDP_NewIDPArray(const blender::StringRef name)
{
  IDProperty *prop = MEM_callocN<IDProperty>("IDProperty prop array");
  prop->type = IDP_IDPARRAY;
  prop->len = 0;
  name.copy_utf8_truncated(prop->name);

  return prop;
}

IDProperty *IDP_CopyIDPArray(const IDProperty *array, const int flag)
{
  /* don't use MEM_dupallocN because this may be part of an array */
  BLI_assert(array->type == IDP_IDPARRAY);

  IDProperty *narray = MEM_mallocN<IDProperty>(__func__);
  *narray = *array;

  narray->data.pointer = MEM_dupallocN(array->data.pointer);
  for (int i = 0; i < narray->len; i++) {
    /* OK, the copy functions always allocate a new structure,
     * which doesn't work here.  instead, simply copy the
     * contents of the new structure into the array cell,
     * then free it.  this makes for more maintainable
     * code than simply re-implementing the copy functions
     * in this loop. */
    IDProperty *tmp = IDP_CopyProperty_ex(GETPROP(narray, i), flag);
    memcpy(GETPROP(narray, i), tmp, sizeof(IDProperty));
    MEM_freeN(tmp);
  }

  return narray;
}

static void IDP_FreeIDPArray(IDProperty *prop, const bool do_id_user)
{
  BLI_assert(prop->type == IDP_IDPARRAY);

  for (int i = 0; i < prop->len; i++) {
    IDP_FreePropertyContent_ex(GETPROP(prop, i), do_id_user);
  }

  if (prop->data.pointer) {
    MEM_freeN(prop->data.pointer);
  }
}

void IDP_SetIndexArray(IDProperty *prop, int index, IDProperty *item)
{
  BLI_assert(prop->type == IDP_IDPARRAY);

  if (index >= prop->len || index < 0) {
    return;
  }

  IDProperty *old = GETPROP(prop, index);
  if (item != old) {
    IDP_FreePropertyContent(old);

    memcpy(old, item, sizeof(IDProperty));
  }
}

IDProperty *IDP_GetIndexArray(IDProperty *prop, int index)
{
  BLI_assert(prop->type == IDP_IDPARRAY);

  return GETPROP(prop, index);
}

void IDP_AppendArray(IDProperty *prop, IDProperty *item)
{
  BLI_assert(prop->type == IDP_IDPARRAY);

  IDP_ResizeIDPArray(prop, prop->len + 1);
  IDP_SetIndexArray(prop, prop->len - 1, item);
}

static void idp_group_children_map_ensure(IDProperty &prop)
{
  BLI_assert(prop.type == IDP_GROUP);
  if (!prop.data.children_map) {
    prop.data.children_map = MEM_new<IDPropertyGroupChildrenSet>(__func__);
  }
}

void IDP_ResizeIDPArray(IDProperty *prop, int newlen)
{
  BLI_assert(prop->type == IDP_IDPARRAY);

  /* first check if the array buffer size has room */
  if (newlen <= prop->totallen) {
    if (newlen < prop->len && prop->totallen - newlen < IDP_ARRAY_REALLOC_LIMIT) {
      for (int i = newlen; i < prop->len; i++) {
        IDP_ClearProperty(GETPROP(prop, i));
      }

      prop->len = newlen;
      return;
    }
    if (newlen >= prop->len) {
      prop->len = newlen;
      return;
    }
  }

  /* free trailing items */
  if (newlen < prop->len) {
    /* newlen is smaller */
    for (int i = newlen; i < prop->len; i++) {
      IDP_ClearProperty(GETPROP(prop, i));
    }
  }

  /* NOTE: This code comes from python, here's the corresponding comment. */
  /* This over-allocates proportional to the list size, making room
   * for additional growth. The over-allocation is mild, but is
   * enough to give linear-time amortized behavior over a long
   * sequence of appends() in the presence of a poorly-performing
   * system realloc().
   * The growth pattern is:  0, 4, 8, 16, 25, 35, 46, 58, 72, 88, ...
   */
  int newsize = newlen;
  newsize = (newsize >> 3) + (newsize < 9 ? 3 : 6) + newsize;
  prop->data.pointer = MEM_recallocN(prop->data.pointer, sizeof(IDProperty) * size_t(newsize));
  prop->len = newlen;
  prop->totallen = newsize;
}

/* ----------- Numerical Array Type ----------- */
static void idp_resize_group_array(IDProperty *prop, int newlen, void *newarr)
{
  if (prop->subtype != IDP_GROUP) {
    return;
  }

  if (newlen >= prop->len) {
    /* bigger */
    IDProperty **array = static_cast<IDProperty **>(newarr);
    for (int a = prop->len; a < newlen; a++) {
      array[a] = blender::bke::idprop::create_group("IDP_ResizeArray group").release();
    }
  }
  else {
    /* smaller */
    IDProperty **array = static_cast<IDProperty **>(prop->data.pointer);

    for (int a = newlen; a < prop->len; a++) {
      IDP_FreeProperty(array[a]);
    }
  }
}

void IDP_ResizeArray(IDProperty *prop, int newlen)
{
  const bool is_grow = newlen >= prop->len;

  /* first check if the array buffer size has room */
  if (newlen <= prop->totallen && prop->totallen - newlen < IDP_ARRAY_REALLOC_LIMIT) {
    idp_resize_group_array(prop, newlen, prop->data.pointer);
    prop->len = newlen;
    return;
  }

  /* NOTE: This code comes from python, here's the corresponding comment. */
  /* This over-allocates proportional to the list size, making room
   * for additional growth.  The over-allocation is mild, but is
   * enough to give linear-time amortized behavior over a long
   * sequence of appends() in the presence of a poorly-performing
   * system realloc().
   * The growth pattern is:  0, 4, 8, 16, 25, 35, 46, 58, 72, 88, ...
   */
  int newsize = newlen;
  newsize = (newsize >> 3) + (newsize < 9 ? 3 : 6) + newsize;

  if (is_grow == false) {
    idp_resize_group_array(prop, newlen, prop->data.pointer);
  }

  prop->data.pointer = MEM_recallocN(prop->data.pointer,
                                     idp_size_table[int(prop->subtype)] * size_t(newsize));

  if (is_grow == true) {
    idp_resize_group_array(prop, newlen, prop->data.pointer);
  }

  prop->len = newlen;
  prop->totallen = newsize;
}

void IDP_FreeArray(IDProperty *prop)
{
  if (prop->data.pointer) {
    idp_resize_group_array(prop, 0, nullptr);
    MEM_freeN(prop->data.pointer);
  }
}

IDPropertyUIData *IDP_ui_data_copy(const IDProperty *prop)
{
  IDPropertyUIData *dst_ui_data = static_cast<IDPropertyUIData *>(MEM_dupallocN(prop->ui_data));

  /* Copy extra type specific data. */
  switch (IDP_ui_data_type(prop)) {
    case IDP_UI_DATA_TYPE_STRING: {
      const IDPropertyUIDataString *src = (const IDPropertyUIDataString *)prop->ui_data;
      IDPropertyUIDataString *dst = (IDPropertyUIDataString *)dst_ui_data;
      dst->default_value = static_cast<char *>(MEM_dupallocN(src->default_value));
      break;
    }
    case IDP_UI_DATA_TYPE_ID: {
      break;
    }
    case IDP_UI_DATA_TYPE_INT: {
      const IDPropertyUIDataInt *src = (const IDPropertyUIDataInt *)prop->ui_data;
      IDPropertyUIDataInt *dst = (IDPropertyUIDataInt *)dst_ui_data;
      dst->default_array = static_cast<int *>(MEM_dupallocN(src->default_array));
      dst->enum_items = static_cast<IDPropertyUIDataEnumItem *>(MEM_dupallocN(src->enum_items));
      for (const int64_t i : blender::IndexRange(src->enum_items_num)) {
        const IDPropertyUIDataEnumItem &src_item = src->enum_items[i];
        IDPropertyUIDataEnumItem &dst_item = dst->enum_items[i];
        dst_item.identifier = BLI_strdup(src_item.identifier);
        dst_item.name = BLI_strdup_null(src_item.name);
        dst_item.description = BLI_strdup_null(src_item.description);
      }
      break;
    }
    case IDP_UI_DATA_TYPE_BOOLEAN: {
      const IDPropertyUIDataBool *src = (const IDPropertyUIDataBool *)prop->ui_data;
      IDPropertyUIDataBool *dst = (IDPropertyUIDataBool *)dst_ui_data;
      dst->default_array = static_cast<int8_t *>(MEM_dupallocN(src->default_array));
      break;
    }
    case IDP_UI_DATA_TYPE_FLOAT: {
      const IDPropertyUIDataFloat *src = (const IDPropertyUIDataFloat *)prop->ui_data;
      IDPropertyUIDataFloat *dst = (IDPropertyUIDataFloat *)dst_ui_data;
      dst->default_array = static_cast<double *>(MEM_dupallocN(src->default_array));
      break;
    }
    case IDP_UI_DATA_TYPE_UNSUPPORTED: {
      break;
    }
  }

  dst_ui_data->description = static_cast<char *>(MEM_dupallocN(prop->ui_data->description));

  return dst_ui_data;
}

static IDProperty *idp_generic_copy(const IDProperty *prop, const int /*flag*/)
{
  IDProperty *newp = MEM_callocN<IDProperty>(__func__);

  STRNCPY(newp->name, prop->name);
  newp->type = prop->type;
  newp->flag = prop->flag;
  newp->data.val = prop->data.val;
  newp->data.val2 = prop->data.val2;

  if (prop->ui_data != nullptr) {
    newp->ui_data = IDP_ui_data_copy(prop);
  }

  return newp;
}

static IDProperty *IDP_CopyArray(const IDProperty *prop, const int flag)
{
  BLI_assert(prop->type == IDP_ARRAY);
  IDProperty *newp = idp_generic_copy(prop, flag);

  if (prop->data.pointer) {
    newp->data.pointer = MEM_dupallocN(prop->data.pointer);

    if (prop->subtype == IDP_GROUP) {
      IDProperty **array = static_cast<IDProperty **>(newp->data.pointer);
      int a;

      for (a = 0; a < prop->len; a++) {
        array[a] = IDP_CopyProperty_ex(array[a], flag);
      }
    }
  }
  newp->len = prop->len;
  newp->subtype = prop->subtype;
  newp->totallen = prop->totallen;

  return newp;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name String Functions (IDProperty String API)
 * \{ */

IDProperty *IDP_NewStringMaxSize(const char *st,
                                 const size_t st_maxncpy,
                                 const blender::StringRef name,
                                 const eIDPropertyFlag flags)
{
  IDProperty *prop = MEM_callocN<IDProperty>("IDProperty string");

  if (st == nullptr) {
    prop->data.pointer = MEM_malloc_arrayN<char>(DEFAULT_ALLOC_FOR_NULL_STRINGS,
                                                 "id property string 1");
    *IDP_string_get(prop) = '\0';
    prop->totallen = DEFAULT_ALLOC_FOR_NULL_STRINGS;
    prop->len = 1; /* nullptr string, has len of 1 to account for null byte. */
  }
  else {
    /* include null terminator '\0' */
    const int stlen = int((st_maxncpy > 0) ? BLI_strnlen(st, st_maxncpy - 1) : strlen(st)) + 1;

    prop->data.pointer = MEM_malloc_arrayN<char>(size_t(stlen), "id property string 2");
    prop->len = prop->totallen = stlen;

    /* Ensured above, must always be true otherwise null terminator assignment will be invalid. */
    BLI_assert(stlen > 0);
    if (stlen > 1) {
      memcpy(prop->data.pointer, st, size_t(stlen));
    }
    IDP_string_get(prop)[stlen - 1] = '\0';
  }

  prop->type = IDP_STRING;
  name.copy_utf8_truncated(prop->name);
  prop->flag = short(flags);

  return prop;
}

IDProperty *IDP_NewString(const char *st,
                          const blender::StringRef name,
                          const eIDPropertyFlag flags)
{
  return IDP_NewStringMaxSize(st, 0, name, flags);
}

static IDProperty *IDP_CopyString(const IDProperty *prop, const int flag)
{
  BLI_assert(prop->type == IDP_STRING);
  IDProperty *newp = idp_generic_copy(prop, flag);

  if (prop->data.pointer) {
    newp->data.pointer = MEM_dupallocN(prop->data.pointer);
  }
  newp->len = prop->len;
  newp->subtype = prop->subtype;
  newp->totallen = prop->totallen;

  return newp;
}

void IDP_AssignStringMaxSize(IDProperty *prop, const char *st, const size_t st_maxncpy)
{
  /* FIXME: This function is broken for bytes (in case there are null chars in it),
   * needs a dedicated function which takes directly the size of the byte buffer. */

  BLI_assert(prop->type == IDP_STRING);
  const bool is_byte = prop->subtype == IDP_STRING_SUB_BYTE;
  const int stlen = int((st_maxncpy > 0) ? BLI_strnlen(st, st_maxncpy - 1) : strlen(st)) +
                    (is_byte ? 0 : 1);
  IDP_ResizeArray(prop, stlen);
  if (stlen > 0) {
    memcpy(prop->data.pointer, st, size_t(stlen));
    if (is_byte == false) {
      IDP_string_get(prop)[stlen - 1] = '\0';
    }
  }
}

void IDP_AssignString(IDProperty *prop, const char *st)
{
  /* FIXME: Should never be called for `byte` subtype, needs an assert. */

  IDP_AssignStringMaxSize(prop, st, 0);
}

void IDP_FreeString(IDProperty *prop)
{
  BLI_assert(prop->type == IDP_STRING);

  if (prop->data.pointer) {
    MEM_freeN(prop->data.pointer);
  }
}
/** \} */

/* -------------------------------------------------------------------- */
/** \name Enum Type (IDProperty Enum API)
 * \{ */

static void IDP_int_ui_data_free_enum_items(IDPropertyUIDataInt *ui_data)
{
  for (const int64_t i : blender::IndexRange(ui_data->enum_items_num)) {
    IDPropertyUIDataEnumItem &item = ui_data->enum_items[i];
    MEM_SAFE_FREE(item.identifier);
    MEM_SAFE_FREE(item.name);
    MEM_SAFE_FREE(item.description);
  }
  MEM_SAFE_FREE(ui_data->enum_items);
}

const IDPropertyUIDataEnumItem *IDP_EnumItemFind(const IDProperty *prop)
{
  BLI_assert(prop->type == IDP_INT);
  const IDPropertyUIDataInt *ui_data = reinterpret_cast<const IDPropertyUIDataInt *>(
      prop->ui_data);

  const int value = IDP_int_get(prop);
  for (const IDPropertyUIDataEnumItem &item :
       blender::Span(ui_data->enum_items, ui_data->enum_items_num))
  {
    if (item.value == value) {
      return &item;
    }
  }
  return nullptr;
}

bool IDP_EnumItemsValidate(const IDPropertyUIDataEnumItem *items,
                           const int items_num,
                           void (*error_fn)(const char *))
{
  blender::Set<int> used_values;
  blender::Set<const char *> used_identifiers;
  used_values.reserve(items_num);
  used_identifiers.reserve(items_num);

  bool is_valid = true;
  for (const int64_t i : blender::IndexRange(items_num)) {
    const IDPropertyUIDataEnumItem &item = items[i];
    if (item.identifier == nullptr || item.identifier[0] == '\0') {
      if (error_fn) {
        const std::string msg = "Item identifier is empty";
        error_fn(msg.c_str());
      }
      is_valid = false;
    }
    if (!used_identifiers.add(item.identifier)) {
      if (error_fn) {
        const std::string msg = fmt::format("Item identifier '{}' is already used",
                                            item.identifier);
        error_fn(msg.c_str());
      }
      is_valid = false;
    }
    if (!used_values.add(item.value)) {
      if (error_fn) {
        const std::string msg = fmt::format(
            "Item value {} for item '{}' is already used", item.value, item.identifier);
        error_fn(msg.c_str());
      }
      is_valid = false;
    }
  }
  return is_valid;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name ID Type (IDProperty ID API)
 * \{ */

static IDProperty *IDP_CopyID(const IDProperty *prop, const int flag)
{
  BLI_assert(prop->type == IDP_ID);
  IDProperty *newp = idp_generic_copy(prop, flag);

  newp->data.pointer = prop->data.pointer;
  if ((flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0) {
    id_us_plus(IDP_ID_get(newp));
  }

  return newp;
}

void IDP_AssignID(IDProperty *prop, ID *id, const int flag)
{
  BLI_assert(prop->type == IDP_ID);
  /* Do not assign embedded IDs to IDProperties. */
  BLI_assert(!id || (id->flag & ID_FLAG_EMBEDDED_DATA) == 0);

  if ((flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0 && IDP_ID_get(prop) != nullptr) {
    id_us_min(IDP_ID_get(prop));
  }

  prop->data.pointer = id;

  if ((flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0) {
    id_us_plus(IDP_ID_get(prop));
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Group Functions (IDProperty Group API)
 * \{ */

/**
 * Checks if a property with the same name as prop exists, and if so replaces it.
 */
static IDProperty *IDP_CopyGroup(const IDProperty *prop, const int flag)
{
  BLI_assert(prop->type == IDP_GROUP);
  IDProperty *newp = idp_generic_copy(prop, flag);
  newp->subtype = prop->subtype;

  LISTBASE_FOREACH (IDProperty *, link, &prop->data.group) {
    IDP_AddToGroup(newp, IDP_CopyProperty_ex(link, flag));
  }

  return newp;
}

void IDP_SyncGroupValues(IDProperty *dest, const IDProperty *src)
{
  BLI_assert(dest->type == IDP_GROUP);
  BLI_assert(src->type == IDP_GROUP);

  LISTBASE_FOREACH (IDProperty *, prop, &src->data.group) {
    IDProperty *other = IDP_GetPropertyFromGroup(dest, prop->name);
    if (other && prop->type == other->type) {
      switch (prop->type) {
        case IDP_INT:
        case IDP_FLOAT:
        case IDP_DOUBLE:
        case IDP_BOOLEAN:
          other->data = prop->data;
          break;
        case IDP_GROUP:
          IDP_SyncGroupValues(other, prop);
          break;
        default: {
          IDP_ReplaceInGroup_ex(dest, IDP_CopyProperty(prop), other, 0);
          break;
        }
      }
    }
  }
}

void IDP_SyncGroupTypes(IDProperty *dest, const IDProperty *src, const bool do_arraylen)
{
  LISTBASE_FOREACH_MUTABLE (IDProperty *, prop_dst, &dest->data.group) {
    const IDProperty *prop_src = IDP_GetPropertyFromGroup((IDProperty *)src, prop_dst->name);
    if (prop_src != nullptr) {
      /* check of we should replace? */
      if ((prop_dst->type != prop_src->type || prop_dst->subtype != prop_src->subtype) ||
          (do_arraylen && ELEM(prop_dst->type, IDP_ARRAY, IDP_IDPARRAY) &&
           (prop_src->len != prop_dst->len)))
      {
        IDP_ReplaceInGroup_ex(dest, IDP_CopyProperty(prop_src), prop_dst, 0);
      }
      else if (prop_dst->type == IDP_GROUP) {
        IDP_SyncGroupTypes(prop_dst, prop_src, do_arraylen);
      }
    }
    else {
      IDP_FreeFromGroup(dest, prop_dst);
    }
  }
}

void IDP_ReplaceGroupInGroup(IDProperty *dest, const IDProperty *src)
{
  BLI_assert(dest->type == IDP_GROUP);
  BLI_assert(src->type == IDP_GROUP);

  LISTBASE_FOREACH (IDProperty *, prop, &src->data.group) {
    IDProperty *old_dest_prop = IDP_GetPropertyFromGroup(dest, prop->name);
    IDP_ReplaceInGroup_ex(dest, IDP_CopyProperty(prop), old_dest_prop, 0);
  }
}

void IDP_ReplaceInGroup_ex(IDProperty *group,
                           IDProperty *prop,
                           IDProperty *prop_exist,
                           const int flag)
{
  BLI_assert(group->type == IDP_GROUP);
  BLI_assert(prop_exist == IDP_GetPropertyFromGroup(group, prop->name));

  if (prop_exist != nullptr) {
    /* Insert the new property at the same position as the old one in the linked list. */
    BLI_insertlinkreplace(&group->data.group, prop_exist, prop);
    BLI_assert(group->data.children_map);
    group->data.children_map->children.remove_contained(prop_exist);
    group->data.children_map->children.add_new(prop);
    IDP_FreeProperty_ex(prop_exist, (flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0);
  }
  else {
    IDP_AddToGroup(group, prop);
  }
}

void IDP_ReplaceInGroup(IDProperty *group, IDProperty *prop)
{
  IDProperty *prop_exist = IDP_GetPropertyFromGroup(group, prop->name);

  IDP_ReplaceInGroup_ex(group, prop, prop_exist, 0);
}

void IDP_MergeGroup_ex(IDProperty *dest,
                       const IDProperty *src,
                       const bool do_overwrite,
                       const int flag)
{
  BLI_assert(dest->type == IDP_GROUP);
  BLI_assert(src->type == IDP_GROUP);

  if (do_overwrite) {
    LISTBASE_FOREACH (IDProperty *, prop, &src->data.group) {
      if (prop->type == IDP_GROUP) {
        IDProperty *prop_exist = IDP_GetPropertyFromGroup(dest, prop->name);

        if (prop_exist != nullptr) {
          IDP_MergeGroup_ex(prop_exist, prop, do_overwrite, flag);
          continue;
        }
      }

      IDProperty *copy = IDP_CopyProperty_ex(prop, flag);
      IDP_ReplaceInGroup_ex(dest, copy, IDP_GetPropertyFromGroup(dest, copy->name), flag);
    }
  }
  else {
    LISTBASE_FOREACH (IDProperty *, prop, &src->data.group) {
      IDProperty *prop_exist = IDP_GetPropertyFromGroup(dest, prop->name);
      if (prop_exist != nullptr) {
        if (prop->type == IDP_GROUP) {
          IDP_MergeGroup_ex(prop_exist, prop, do_overwrite, flag);
          continue;
        }
      }
      else {
        IDP_AddToGroup(dest, IDP_CopyProperty_ex(prop, flag));
      }
    }
  }
}

void IDP_MergeGroup(IDProperty *dest, const IDProperty *src, const bool do_overwrite)
{
  IDP_MergeGroup_ex(dest, src, do_overwrite, 0);
}

bool IDP_AddToGroup(IDProperty *group, IDProperty *prop)
{
  BLI_assert(group->type == IDP_GROUP);

  idp_group_children_map_ensure(*group);
  if (group->data.children_map->children.add(prop)) {
    group->len++;
    BLI_addtail(&group->data.group, prop);
    return true;
  }
  return false;
}

void IDP_RemoveFromGroup(IDProperty *group, IDProperty *prop)
{
  BLI_assert(group->type == IDP_GROUP);
  BLI_assert(BLI_findindex(&group->data.group, prop) != -1);

  group->len--;
  BLI_remlink(&group->data.group, prop);
  BLI_assert(group->data.children_map);
  group->data.children_map->children.remove_contained(prop);
}

void IDP_FreeFromGroup(IDProperty *group, IDProperty *prop)
{
  IDP_RemoveFromGroup(group, prop);
  IDP_FreeProperty(prop);
}

IDProperty *IDP_GetPropertyFromGroup(const IDProperty *prop, const blender::StringRef name)
{
  BLI_assert(prop->type == IDP_GROUP);
  if (prop->len == 0) {
    BLI_assert(prop->data.children_map == nullptr || prop->data.children_map->children.is_empty());
    return nullptr;
  }
  /* If there is at least one item, the map is expected to exist. */
  BLI_assert(prop->data.children_map);
  BLI_assert(prop->data.children_map->children.size() == prop->len);
  return prop->data.children_map->children.lookup_key_default_as(name, nullptr);
}

IDProperty *IDP_GetPropertyFromGroup_null(const IDProperty *prop, const blender::StringRef name)
{
  if (!prop) {
    return nullptr;
  }
  return IDP_GetPropertyFromGroup(prop, name);
}

IDProperty *IDP_GetPropertyTypeFromGroup(const IDProperty *prop,
                                         const blender::StringRef name,
                                         const char type)
{
  IDProperty *idprop = IDP_GetPropertyFromGroup(prop, name);
  return (idprop && idprop->type == type) ? idprop : nullptr;
}

/* Ok, the way things work, Groups free the ID Property structs of their children.
 * This is because all ID Property freeing functions free only direct data (not the ID Property
 * struct itself), but for Groups the child properties *are* considered
 * direct data. */
static void IDP_FreeGroup(IDProperty *prop, const bool do_id_user)
{
  BLI_assert(prop->type == IDP_GROUP);

  MEM_SAFE_DELETE(prop->data.children_map);
  LISTBASE_FOREACH (IDProperty *, loop, &prop->data.group) {
    IDP_FreePropertyContent_ex(loop, do_id_user);
  }
  BLI_freelistN(&prop->data.group);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Main Functions  (IDProperty Main API)
 * \{ */

int IDP_coerce_to_int_or_zero(const IDProperty *prop)
{
  switch (prop->type) {
    case IDP_INT:
      return IDP_int_get(prop);
    case IDP_DOUBLE:
      return int(IDP_double_get(prop));
    case IDP_FLOAT:
      return int(IDP_float_get(prop));
    case IDP_BOOLEAN:
      return int(IDP_bool_get(prop));
    default:
      return 0;
  }
}

double IDP_coerce_to_double_or_zero(const IDProperty *prop)
{
  switch (prop->type) {
    case IDP_DOUBLE:
      return IDP_double_get(prop);
    case IDP_FLOAT:
      return double(IDP_float_get(prop));
    case IDP_INT:
      return double(IDP_int_get(prop));
    case IDP_BOOLEAN:
      return double(IDP_bool_get(prop));
    default:
      return 0.0;
  }
}

float IDP_coerce_to_float_or_zero(const IDProperty *prop)
{
  switch (prop->type) {
    case IDP_FLOAT:
      return IDP_float_get(prop);
    case IDP_DOUBLE:
      return float(IDP_double_get(prop));
    case IDP_INT:
      return float(IDP_int_get(prop));
    case IDP_BOOLEAN:
      return float(IDP_bool_get(prop));
    default:
      return 0.0f;
  }
}

IDProperty *IDP_CopyProperty_ex(const IDProperty *prop, const int flag)
{
  switch (prop->type) {
    case IDP_GROUP:
      return IDP_CopyGroup(prop, flag);
    case IDP_STRING:
      return IDP_CopyString(prop, flag);
    case IDP_ID:
      return IDP_CopyID(prop, flag);
    case IDP_ARRAY:
      return IDP_CopyArray(prop, flag);
    case IDP_IDPARRAY:
      return IDP_CopyIDPArray(prop, flag);
    default:
      return idp_generic_copy(prop, flag);
  }
}

IDProperty *IDP_CopyProperty(const IDProperty *prop)
{
  return IDP_CopyProperty_ex(prop, 0);
}

void IDP_CopyPropertyContent(IDProperty *dst, const IDProperty *src)
{
  IDProperty *idprop_tmp = IDP_CopyProperty(src);
  idprop_tmp->prev = dst->prev;
  idprop_tmp->next = dst->next;
  std::swap(*dst, *idprop_tmp);
  IDP_FreeProperty(idprop_tmp);
}

IDProperty *IDP_GetProperties(ID *id)
{
  return id->properties;
}

IDProperty *IDP_EnsureProperties(ID *id)
{
  if (id->properties == nullptr) {
    id->properties = MEM_callocN<IDProperty>("IDProperty");
    id->properties->type = IDP_GROUP;
    /* NOTE(@ideasman42): Don't overwrite the data's name and type
     * some functions might need this if they
     * don't have a real ID, should be named elsewhere. */
    // STRNCPY(id->name, "top_level_group");
  }
  return id->properties;
}

IDProperty *IDP_ID_system_properties_get(ID *id)
{
  return id->system_properties;
}

IDProperty *IDP_ID_system_properties_ensure(ID *id)
{
  if (id->system_properties == nullptr) {
    id->system_properties = MEM_callocN<IDProperty>(__func__);
    id->system_properties->type = IDP_GROUP;
    /* NOTE(@ideasman42): Don't overwrite the data's name and type
     * some functions might need this if they
     * don't have a real ID, should be named elsewhere. */
    // STRNCPY(id->name, "top_level_group");
  }
  return id->system_properties;
}

bool IDP_EqualsProperties_ex(const IDProperty *prop1,
                             const IDProperty *prop2,
                             const bool is_strict)
{
  if (prop1 == nullptr && prop2 == nullptr) {
    return true;
  }
  if (prop1 == nullptr || prop2 == nullptr) {
    return is_strict ? false : true;
  }
  if (prop1->type != prop2->type) {
    return false;
  }

  switch (prop1->type) {
    case IDP_INT:
      return (IDP_int_get(prop1) == IDP_int_get(prop2));
    case IDP_FLOAT:
#if !defined(NDEBUG) && defined(WITH_PYTHON)
    {
      float p1 = IDP_float_get(prop1);
      float p2 = IDP_float_get(prop2);
      if ((p1 != p2) && ((fabsf(p1 - p2) / max_ff(fabsf(p1), fabsf(p2))) < 0.001f)) {
        printf(
            "WARNING: Comparing two float properties that have nearly the same value (%f vs. "
            "%f)\n",
            p1,
            p2);
        printf("    p1: ");
        IDP_print(prop1);
        printf("    p2: ");
        IDP_print(prop2);
      }
    }
#endif
      return (IDP_float_get(prop1) == IDP_float_get(prop2));
    case IDP_DOUBLE:
      return (IDP_double_get(prop1) == IDP_double_get(prop2));
    case IDP_BOOLEAN:
      return (IDP_bool_get(prop1) == IDP_bool_get(prop2));
    case IDP_STRING: {
      return ((prop1->len == prop2->len) &&
              STREQLEN(IDP_string_get(prop1), IDP_string_get(prop2), size_t(prop1->len)));
    }
    case IDP_ARRAY:
      if (prop1->len == prop2->len && prop1->subtype == prop2->subtype) {
        return (memcmp(IDP_array_voidp_get(prop1),
                       IDP_array_voidp_get(prop2),
                       idp_size_table[int(prop1->subtype)] * size_t(prop1->len)) == 0);
      }
      return false;
    case IDP_GROUP: {
      if (is_strict && prop1->len != prop2->len) {
        return false;
      }

      LISTBASE_FOREACH (const IDProperty *, link1, &prop1->data.group) {
        const IDProperty *link2 = IDP_GetPropertyFromGroup(prop2, link1->name);

        if (!IDP_EqualsProperties_ex(link1, link2, is_strict)) {
          return false;
        }
      }

      return true;
    }
    case IDP_IDPARRAY: {
      const IDProperty *array1 = IDP_property_array_get(prop1);
      const IDProperty *array2 = IDP_property_array_get(prop2);

      if (prop1->len != prop2->len) {
        return false;
      }

      for (int i = 0; i < prop1->len; i++) {
        if (!IDP_EqualsProperties_ex(&array1[i], &array2[i], is_strict)) {
          return false;
        }
      }
      return true;
    }
    case IDP_ID:
      return (IDP_ID_get(prop1) == IDP_ID_get(prop2));
    default:
      BLI_assert_unreachable();
      break;
  }

  return true;
}

bool IDP_EqualsProperties(const IDProperty *prop1, const IDProperty *prop2)
{
  return IDP_EqualsProperties_ex(prop1, prop2, true);
}

IDProperty *IDP_New(const char type,
                    const IDPropertyTemplate *val,
                    const blender::StringRef name,
                    const eIDPropertyFlag flags)
{
  IDProperty *prop = nullptr;

  switch (type) {
    case IDP_INT:
      prop = MEM_callocN<IDProperty>("IDProperty int");
      prop->data.val = val->i;
      break;
    case IDP_FLOAT:
      prop = MEM_callocN<IDProperty>("IDProperty float");
      *(float *)&prop->data.val = val->f;
      break;
    case IDP_DOUBLE:
      prop = MEM_callocN<IDProperty>("IDProperty double");
      *(double *)&prop->data.val = val->d;
      break;
    case IDP_BOOLEAN:
      prop = MEM_callocN<IDProperty>("IDProperty boolean");
      prop->data.val = bool(val->i);
      break;
    case IDP_ARRAY: {
      /* FIXME: This seems to be the only place in code allowing `IDP_GROUP` as subtype of an
       * `IDP_ARRAY`. This is most likely a mistake. `IDP_GROUP` array should be of type
       * `IDP_IDPARRAY`, as done e.g. in #idp_from_PySequence_Buffer in bpy API. */
      if (ELEM(val->array.type, IDP_FLOAT, IDP_INT, IDP_DOUBLE, IDP_GROUP, IDP_BOOLEAN)) {
        prop = MEM_callocN<IDProperty>("IDProperty array");
        prop->subtype = val->array.type;
        if (val->array.len) {
          prop->data.pointer = MEM_calloc_arrayN(
              size_t(val->array.len), idp_size_table[val->array.type], "id property array");
        }
        prop->len = prop->totallen = val->array.len;
        break;
      }
      CLOG_ERROR(&LOG, "bad array type.");
      return nullptr;
    }
    case IDP_STRING: {
      const char *st = val->string.str;

      prop = MEM_callocN<IDProperty>("IDProperty string");
      if (val->string.subtype == IDP_STRING_SUB_BYTE) {
        /* NOTE: Intentionally not null terminated. */
        if (st == nullptr) {
          prop->data.pointer = MEM_malloc_arrayN<char>(DEFAULT_ALLOC_FOR_NULL_STRINGS,
                                                       "id property string 1");
          *IDP_string_get(prop) = '\0';
          prop->totallen = DEFAULT_ALLOC_FOR_NULL_STRINGS;
          prop->len = 0;
        }
        else {
          prop->data.pointer = MEM_malloc_arrayN<char>(size_t(val->string.len),
                                                       "id property string 2");
          prop->len = prop->totallen = val->string.len;
          memcpy(prop->data.pointer, st, size_t(val->string.len));
        }
        prop->subtype = IDP_STRING_SUB_BYTE;
      }
      else {
        if (st == nullptr || val->string.len <= 1) {
          prop->data.pointer = MEM_malloc_arrayN<char>(DEFAULT_ALLOC_FOR_NULL_STRINGS,
                                                       "id property string 1");
          *IDP_string_get(prop) = '\0';
          prop->totallen = DEFAULT_ALLOC_FOR_NULL_STRINGS;
          /* nullptr string, has len of 1 to account for null byte. */
          prop->len = 1;
        }
        else {
          BLI_assert(int(val->string.len) <= int(strlen(st)) + 1);
          prop->data.pointer = MEM_malloc_arrayN<char>(size_t(val->string.len),
                                                       "id property string 3");
          memcpy(prop->data.pointer, st, size_t(val->string.len) - 1);
          IDP_string_get(prop)[val->string.len - 1] = '\0';
          prop->len = prop->totallen = val->string.len;
        }
        prop->subtype = IDP_STRING_SUB_UTF8;
      }
      break;
    }
    case IDP_GROUP: {
      /* Values are set properly by calloc. */
      prop = MEM_callocN<IDProperty>("IDProperty group");
      break;
    }
    case IDP_ID: {
      prop = MEM_callocN<IDProperty>("IDProperty datablock");
      prop->data.pointer = (void *)val->id;
      prop->type = IDP_ID;
      id_us_plus(IDP_ID_get(prop));
      break;
    }
    default: {
      prop = MEM_callocN<IDProperty>("IDProperty array");
      break;
    }
  }

  prop->type = type;
  name.copy_utf8_truncated(prop->name);
  prop->flag = short(flags);

  return prop;
}

void IDP_ui_data_free_unique_contents(IDPropertyUIData *ui_data,
                                      const eIDPropertyUIDataType type,
                                      const IDPropertyUIData *other)
{
  if (ui_data->description != other->description) {
    MEM_SAFE_FREE(ui_data->description);
  }

  switch (type) {
    case IDP_UI_DATA_TYPE_STRING: {
      const IDPropertyUIDataString *other_string = (const IDPropertyUIDataString *)other;
      IDPropertyUIDataString *ui_data_string = (IDPropertyUIDataString *)ui_data;
      if (ui_data_string->default_value != other_string->default_value) {
        MEM_SAFE_FREE(ui_data_string->default_value);
      }
      break;
    }
    case IDP_UI_DATA_TYPE_ID: {
      break;
    }
    case IDP_UI_DATA_TYPE_INT: {
      const IDPropertyUIDataInt *other_int = (const IDPropertyUIDataInt *)other;
      IDPropertyUIDataInt *ui_data_int = (IDPropertyUIDataInt *)ui_data;
      if (ui_data_int->default_array != other_int->default_array) {
        MEM_SAFE_FREE(ui_data_int->default_array);
      }
      if (ui_data_int->enum_items != other_int->enum_items) {
        IDP_int_ui_data_free_enum_items(ui_data_int);
      }
      break;
    }
    case IDP_UI_DATA_TYPE_BOOLEAN: {
      const IDPropertyUIDataBool *other_bool = (const IDPropertyUIDataBool *)other;
      IDPropertyUIDataBool *ui_data_bool = (IDPropertyUIDataBool *)ui_data;
      if (ui_data_bool->default_array != other_bool->default_array) {
        MEM_SAFE_FREE(ui_data_bool->default_array);
      }
      break;
    }
    case IDP_UI_DATA_TYPE_FLOAT: {
      const IDPropertyUIDataFloat *other_float = (const IDPropertyUIDataFloat *)other;
      IDPropertyUIDataFloat *ui_data_float = (IDPropertyUIDataFloat *)ui_data;
      if (ui_data_float->default_array != other_float->default_array) {
        MEM_SAFE_FREE(ui_data_float->default_array);
      }
      break;
    }
    case IDP_UI_DATA_TYPE_UNSUPPORTED: {
      break;
    }
  }
}

static void ui_data_free(IDPropertyUIData *ui_data, const eIDPropertyUIDataType type)
{
  switch (type) {
    case IDP_UI_DATA_TYPE_STRING: {
      IDPropertyUIDataString *ui_data_string = (IDPropertyUIDataString *)ui_data;
      MEM_SAFE_FREE(ui_data_string->default_value);
      break;
    }
    case IDP_UI_DATA_TYPE_ID: {
      break;
    }
    case IDP_UI_DATA_TYPE_INT: {
      IDPropertyUIDataInt *ui_data_int = (IDPropertyUIDataInt *)ui_data;
      MEM_SAFE_FREE(ui_data_int->default_array);
      IDP_int_ui_data_free_enum_items(ui_data_int);
      break;
    }
    case IDP_UI_DATA_TYPE_BOOLEAN: {
      IDPropertyUIDataBool *ui_data_bool = (IDPropertyUIDataBool *)ui_data;
      MEM_SAFE_FREE(ui_data_bool->default_array);
      break;
    }
    case IDP_UI_DATA_TYPE_FLOAT: {
      IDPropertyUIDataFloat *ui_data_float = (IDPropertyUIDataFloat *)ui_data;
      MEM_SAFE_FREE(ui_data_float->default_array);
      break;
    }
    case IDP_UI_DATA_TYPE_UNSUPPORTED: {
      break;
    }
  }

  MEM_SAFE_FREE(ui_data->description);

  MEM_freeN(ui_data);
}

void IDP_ui_data_free(IDProperty *prop)
{
  ui_data_free(prop->ui_data, IDP_ui_data_type(prop));
  prop->ui_data = nullptr;
}

void IDP_FreePropertyContent_ex(IDProperty *prop, const bool do_id_user)
{
  switch (prop->type) {
    case IDP_ARRAY:
      IDP_FreeArray(prop);
      break;
    case IDP_STRING:
      IDP_FreeString(prop);
      break;
    case IDP_GROUP:
      IDP_FreeGroup(prop, do_id_user);
      break;
    case IDP_IDPARRAY:
      IDP_FreeIDPArray(prop, do_id_user);
      break;
    case IDP_ID:
      if (do_id_user) {
        id_us_min(IDP_ID_get(prop));
      }
      break;
  }

  if (prop->ui_data != nullptr) {
    IDP_ui_data_free(prop);
  }
}

void IDP_FreePropertyContent(IDProperty *prop)
{
  IDP_FreePropertyContent_ex(prop, true);
}

void IDP_FreeProperty_ex(IDProperty *prop, const bool do_id_user)
{
  IDP_FreePropertyContent_ex(prop, do_id_user);
  MEM_freeN(prop);
}

void IDP_FreeProperty(IDProperty *prop)
{
  IDP_FreePropertyContent(prop);
  MEM_freeN(prop);
}

void IDP_ClearProperty(IDProperty *prop)
{
  IDP_FreePropertyContent(prop);
  prop->data.pointer = nullptr;
  prop->len = prop->totallen = 0;
}

void IDP_Reset(IDProperty *prop, const IDProperty *reference)
{
  if (prop == nullptr) {
    return;
  }
  IDP_ClearProperty(prop);
  if (reference != nullptr) {
    IDP_MergeGroup(prop, reference, true);
  }
}

void IDP_foreach_property(IDProperty *id_property_root,
                          const int type_filter,
                          const blender::FunctionRef<void(IDProperty *id_property)> callback)
{
  if (!id_property_root) {
    return;
  }

  if (type_filter == 0 || (1 << id_property_root->type) & type_filter) {
    callback(id_property_root);
  }

  /* Recursive call into container types of ID properties. */
  switch (id_property_root->type) {
    case IDP_GROUP: {
      LISTBASE_FOREACH (IDProperty *, loop, &id_property_root->data.group) {
        IDP_foreach_property(loop, type_filter, callback);
      }
      break;
    }
    case IDP_IDPARRAY: {
      IDProperty *loop = IDP_property_array_get(id_property_root);
      for (int i = 0; i < id_property_root->len; i++) {
        IDP_foreach_property(&loop[i], type_filter, callback);
      }
      break;
    }
    default:
      break; /* Nothing to do here with other types of IDProperties... */
  }
}

void IDP_WriteProperty_OnlyData(const IDProperty *prop, BlendWriter *writer);

static void write_ui_data(const IDProperty *prop, BlendWriter *writer)
{
  IDPropertyUIData *ui_data = prop->ui_data;

  BLO_write_string(writer, ui_data->description);

  switch (IDP_ui_data_type(prop)) {
    case IDP_UI_DATA_TYPE_STRING: {
      IDPropertyUIDataString *ui_data_string = (IDPropertyUIDataString *)ui_data;
      BLO_write_string(writer, ui_data_string->default_value);
      BLO_write_struct(writer, IDPropertyUIDataString, ui_data);
      break;
    }
    case IDP_UI_DATA_TYPE_ID: {
      BLO_write_struct(writer, IDPropertyUIDataID, ui_data);
      break;
    }
    case IDP_UI_DATA_TYPE_INT: {
      IDPropertyUIDataInt *ui_data_int = (IDPropertyUIDataInt *)ui_data;
      if (prop->type == IDP_ARRAY) {
        BLO_write_int32_array(
            writer, uint(ui_data_int->default_array_len), (int32_t *)ui_data_int->default_array);
      }
      BLO_write_struct_array(
          writer, IDPropertyUIDataEnumItem, ui_data_int->enum_items_num, ui_data_int->enum_items);
      for (const int64_t i : blender::IndexRange(ui_data_int->enum_items_num)) {
        IDPropertyUIDataEnumItem &item = ui_data_int->enum_items[i];
        BLO_write_string(writer, item.identifier);
        BLO_write_string(writer, item.name);
        BLO_write_string(writer, item.description);
      }
      BLO_write_struct(writer, IDPropertyUIDataInt, ui_data);
      break;
    }
    case IDP_UI_DATA_TYPE_BOOLEAN: {
      IDPropertyUIDataBool *ui_data_bool = (IDPropertyUIDataBool *)ui_data;
      if (prop->type == IDP_ARRAY) {
        BLO_write_int8_array(writer,
                             uint(ui_data_bool->default_array_len),
                             (const int8_t *)ui_data_bool->default_array);
      }
      BLO_write_struct(writer, IDPropertyUIDataBool, ui_data);
      break;
    }
    case IDP_UI_DATA_TYPE_FLOAT: {
      IDPropertyUIDataFloat *ui_data_float = (IDPropertyUIDataFloat *)ui_data;
      if (prop->type == IDP_ARRAY) {
        BLO_write_double_array(
            writer, uint(ui_data_float->default_array_len), ui_data_float->default_array);
      }
      BLO_write_struct(writer, IDPropertyUIDataFloat, ui_data);
      break;
    }
    case IDP_UI_DATA_TYPE_UNSUPPORTED: {
      BLI_assert_unreachable();
      break;
    }
  }
}

static void IDP_WriteArray(const IDProperty *prop, BlendWriter *writer)
{
  /* Remember to set #IDProperty.totallen to len in the linking code! */
  if (prop->data.pointer) {
    /* Only write the actual data (`prop->len`), not the whole allocated buffer (`prop->totallen`).
     */
    switch (eIDPropertyType(prop->subtype)) {
      case IDP_GROUP: {
        BLO_write_pointer_array(writer, uint32_t(prop->len), prop->data.pointer);

        IDProperty **array = static_cast<IDProperty **>(prop->data.pointer);
        for (int i = 0; i < prop->len; i++) {
          IDP_BlendWrite(writer, array[i]);
        }
        break;
      }
      case IDP_DOUBLE:
        BLO_write_double_array(
            writer, uint32_t(prop->len), static_cast<double *>(prop->data.pointer));
        break;
      case IDP_INT:
        BLO_write_int32_array(writer, uint32_t(prop->len), static_cast<int *>(prop->data.pointer));
        break;
      case IDP_FLOAT:
        BLO_write_float_array(
            writer, uint32_t(prop->len), static_cast<float *>(prop->data.pointer));
        break;
      case IDP_BOOLEAN:
        BLO_write_int8_array(
            writer, uint32_t(prop->len), static_cast<int8_t *>(prop->data.pointer));
        break;
      case IDP_STRING:
      case IDP_ARRAY:
      case IDP_ID:
      case IDP_IDPARRAY:
        BLI_assert_unreachable();
        break;
    }
  }
}

static void IDP_WriteIDPArray(const IDProperty *prop, BlendWriter *writer)
{
  /* Remember to set #IDProperty.totallen to len in the linking code! */
  if (prop->data.pointer) {
    const IDProperty *array = static_cast<const IDProperty *>(prop->data.pointer);

    BLO_write_struct_array(writer, IDProperty, prop->len, array);

    for (int a = 0; a < prop->len; a++) {
      IDP_WriteProperty_OnlyData(&array[a], writer);
    }
  }
}

static void IDP_WriteString(const IDProperty *prop, BlendWriter *writer)
{
  /* Remember to set #IDProperty.totallen to len in the linking code! */
  /* Do not use #BLO_write_string here, since 'bytes' sub-type of IDProperties may not be
   * null-terminated. */
  BLO_write_char_array(writer, uint(prop->len), static_cast<char *>(prop->data.pointer));
}

static void IDP_WriteGroup(const IDProperty *prop, BlendWriter *writer)
{
  LISTBASE_FOREACH (IDProperty *, loop, &prop->data.group) {
    IDP_BlendWrite(writer, loop);
  }
}

/* Functions to read/write ID Properties */
void IDP_WriteProperty_OnlyData(const IDProperty *prop, BlendWriter *writer)
{
  switch (prop->type) {
    case IDP_GROUP:
      IDP_WriteGroup(prop, writer);
      break;
    case IDP_STRING:
      IDP_WriteString(prop, writer);
      break;
    case IDP_ARRAY:
      IDP_WriteArray(prop, writer);
      break;
    case IDP_IDPARRAY:
      IDP_WriteIDPArray(prop, writer);
      break;
  }
  if (prop->ui_data != nullptr) {
    write_ui_data(prop, writer);
  }
}

void IDP_BlendWrite(BlendWriter *writer, const IDProperty *prop)
{
  BLO_write_struct(writer, IDProperty, prop);
  IDP_WriteProperty_OnlyData(prop, writer);
}

static void IDP_DirectLinkProperty(IDProperty *prop, BlendDataReader *reader);

static void read_ui_data(IDProperty *prop, BlendDataReader *reader)
{
  /* NOTE: null UI data can happen when opening more recent files with unknown types of
   * IDProperties. */

  switch (IDP_ui_data_type(prop)) {
    case IDP_UI_DATA_TYPE_STRING: {
      BLO_read_struct(reader, IDPropertyUIDataString, &prop->ui_data);
      if (prop->ui_data) {
        IDPropertyUIDataString *ui_data_string = (IDPropertyUIDataString *)prop->ui_data;
        BLO_read_string(reader, &ui_data_string->default_value);
      }
      break;
    }
    case IDP_UI_DATA_TYPE_ID: {
      BLO_read_struct(reader, IDPropertyUIDataID, &prop->ui_data);
      break;
    }
    case IDP_UI_DATA_TYPE_INT: {
      BLO_read_struct(reader, IDPropertyUIDataInt, &prop->ui_data);
      IDPropertyUIDataInt *ui_data_int = (IDPropertyUIDataInt *)prop->ui_data;
      if (prop->type == IDP_ARRAY) {
        BLO_read_int32_array(
            reader, ui_data_int->default_array_len, (&ui_data_int->default_array));
      }
      else {
        ui_data_int->default_array = nullptr;
        ui_data_int->default_array_len = 0;
      }
      BLO_read_struct_array(reader,
                            IDPropertyUIDataEnumItem,
                            size_t(ui_data_int->enum_items_num),
                            &ui_data_int->enum_items);
      for (const int64_t i : blender::IndexRange(ui_data_int->enum_items_num)) {
        IDPropertyUIDataEnumItem &item = ui_data_int->enum_items[i];
        BLO_read_string(reader, &item.identifier);
        BLO_read_string(reader, &item.name);
        BLO_read_string(reader, &item.description);
      }
      break;
    }
    case IDP_UI_DATA_TYPE_BOOLEAN: {
      BLO_read_struct(reader, IDPropertyUIDataBool, &prop->ui_data);
      IDPropertyUIDataBool *ui_data_bool = (IDPropertyUIDataBool *)prop->ui_data;
      if (prop->type == IDP_ARRAY) {
        BLO_read_int8_array(
            reader, ui_data_bool->default_array_len, (&ui_data_bool->default_array));
      }
      else {
        ui_data_bool->default_array = nullptr;
        ui_data_bool->default_array_len = 0;
      }
      break;
    }
    case IDP_UI_DATA_TYPE_FLOAT: {
      BLO_read_struct(reader, IDPropertyUIDataFloat, &prop->ui_data);
      IDPropertyUIDataFloat *ui_data_float = (IDPropertyUIDataFloat *)prop->ui_data;
      if (prop->type == IDP_ARRAY) {
        BLO_read_double_array(
            reader, ui_data_float->default_array_len, (&ui_data_float->default_array));
      }
      else {
        ui_data_float->default_array = nullptr;
        ui_data_float->default_array_len = 0;
      }
      break;
    }
    case IDP_UI_DATA_TYPE_UNSUPPORTED: {
      BLI_assert_unreachable();
      /* Do not attempt to read unknown data. */
      prop->ui_data = nullptr;
      break;
    }
  }

  if (prop->ui_data) {
    BLO_read_string(reader, &prop->ui_data->description);
  }
}

static void IDP_DirectLinkIDPArray(IDProperty *prop, BlendDataReader *reader)
{
  /* since we didn't save the extra buffer, set totallen to len */
  prop->totallen = prop->len;
  BLO_read_struct_array(reader, IDProperty, size_t(prop->len), &prop->data.pointer);

  IDProperty *array = (IDProperty *)prop->data.pointer;

  /* NOTE:, idp-arrays didn't exist in 2.4x, so the pointer will be cleared
   * there's not really anything we can do to correct this, at least don't crash */
  if (array == nullptr) {
    prop->len = 0;
    prop->totallen = 0;
  }

  for (int i = 0; i < prop->len; i++) {
    IDP_DirectLinkProperty(&array[i], reader);
  }
}

static void IDP_DirectLinkArray(IDProperty *prop, BlendDataReader *reader)
{
  /* since we didn't save the extra buffer, set totallen to len */
  prop->totallen = prop->len;

  switch (eIDPropertyType(prop->subtype)) {
    case IDP_GROUP: {
      BLO_read_pointer_array(reader, prop->len, &prop->data.pointer);
      IDProperty **array = static_cast<IDProperty **>(prop->data.pointer);
      for (int i = 0; i < prop->len; i++) {
        IDP_DirectLinkProperty(array[i], reader);
      }
      break;
    }
    case IDP_DOUBLE:
      BLO_read_double_array(reader, prop->len, (double **)&prop->data.pointer);
      break;
    case IDP_INT:
      BLO_read_int32_array(reader, prop->len, (int **)&prop->data.pointer);
      break;
    case IDP_FLOAT:
      BLO_read_float_array(reader, prop->len, (float **)&prop->data.pointer);
      break;
    case IDP_BOOLEAN:
      BLO_read_int8_array(reader, prop->len, (int8_t **)&prop->data.pointer);
      break;
    case IDP_STRING:
    case IDP_ARRAY:
    case IDP_ID:
    case IDP_IDPARRAY:
      BLI_assert_unreachable();
      break;
  }
}

static void IDP_DirectLinkString(IDProperty *prop, BlendDataReader *reader)
{
  /* Since we didn't save the extra string buffer, set totallen to len. */
  prop->totallen = prop->len;
  BLO_read_char_array(reader, prop->len, reinterpret_cast<char **>(&prop->data.pointer));
}

static void IDP_DirectLinkGroup(IDProperty *prop, BlendDataReader *reader)
{
  ListBase *lb = &prop->data.group;
  prop->data.children_map = nullptr;

  BLO_read_struct_list(reader, IDProperty, lb);

  if (!BLI_listbase_is_empty(&prop->data.group)) {
    idp_group_children_map_ensure(*prop);
  }

  /* Link child id properties now. */
  LISTBASE_FOREACH (IDProperty *, loop, &prop->data.group) {
    IDP_DirectLinkProperty(loop, reader);
    if (!prop->data.children_map->children.add(loop)) {
      CLOG_WARN(&LOG, "duplicate ID property '%s' in group", loop->name);
    }
  }
}

static void IDP_DirectLinkProperty(IDProperty *prop, BlendDataReader *reader)
{
  switch (prop->type) {
    case IDP_GROUP:
      IDP_DirectLinkGroup(prop, reader);
      break;
    case IDP_STRING:
      IDP_DirectLinkString(prop, reader);
      break;
    case IDP_ARRAY:
      IDP_DirectLinkArray(prop, reader);
      break;
    case IDP_IDPARRAY:
      IDP_DirectLinkIDPArray(prop, reader);
      break;
    case IDP_DOUBLE:
      /* NOTE: this is endianness-sensitive. */
      /* Doubles are stored in the same field as `int val, val2` in the #IDPropertyData struct.
       *
       * In case of endianness switching, `val` and `val2` would have already been switched by the
       * generic reading code, so they would need to be first un-switched individually, and then
       * re-switched as a single 64-bit entity. */
      break;
    case IDP_INT:
    case IDP_FLOAT:
    case IDP_BOOLEAN:
    case IDP_ID:
      break; /* Nothing special to do here. */
    default:
      /* Unknown IDP type, nuke it (we cannot handle unknown types everywhere in code,
       * IDP are way too polymorphic to do it safely. */
      printf(
          "%s: found unknown IDProperty type %d, reset to Integer one !\n", __func__, prop->type);
      /* NOTE: we do not attempt to free unknown prop, we have no way to know how to do that! */
      prop->type = IDP_INT;
      prop->subtype = 0;
      IDP_int_set(prop, 0);
  }

  if (prop->ui_data != nullptr) {
    read_ui_data(prop, reader);
  }
}

void IDP_BlendReadData_impl(BlendDataReader *reader, IDProperty **prop, const char *caller_func_id)
{
  if (*prop) {
    if ((*prop)->type == IDP_GROUP) {
      IDP_DirectLinkGroup(*prop, reader);
    }
    else {
      /* corrupt file! */
      printf("%s: found non group data, freeing type %d!\n", caller_func_id, (*prop)->type);
      /* don't risk id, data's likely corrupt. */
      // IDP_FreePropertyContent(*prop);
      *prop = nullptr;
    }
  }
}

eIDPropertyUIDataType IDP_ui_data_type(const IDProperty *prop)
{
  if (prop->type == IDP_STRING) {
    return IDP_UI_DATA_TYPE_STRING;
  }
  if (prop->type == IDP_ID) {
    return IDP_UI_DATA_TYPE_ID;
  }
  if (prop->type == IDP_INT || (prop->type == IDP_ARRAY && prop->subtype == IDP_INT)) {
    return IDP_UI_DATA_TYPE_INT;
  }
  if (ELEM(prop->type, IDP_FLOAT, IDP_DOUBLE) ||
      (prop->type == IDP_ARRAY && ELEM(prop->subtype, IDP_FLOAT, IDP_DOUBLE)))
  {
    return IDP_UI_DATA_TYPE_FLOAT;
  }
  if (prop->type == IDP_BOOLEAN || (prop->type == IDP_ARRAY && prop->subtype == IDP_BOOLEAN)) {
    return IDP_UI_DATA_TYPE_BOOLEAN;
  }
  return IDP_UI_DATA_TYPE_UNSUPPORTED;
}

bool IDP_ui_data_supported(const IDProperty *prop)
{
  return IDP_ui_data_type(prop) != IDP_UI_DATA_TYPE_UNSUPPORTED;
}

static IDPropertyUIData *ui_data_alloc(const eIDPropertyUIDataType type)
{
  switch (type) {
    case IDP_UI_DATA_TYPE_STRING: {
      IDPropertyUIDataString *ui_data = MEM_callocN<IDPropertyUIDataString>(__func__);
      return &ui_data->base;
    }
    case IDP_UI_DATA_TYPE_ID: {
      IDPropertyUIDataID *ui_data = MEM_callocN<IDPropertyUIDataID>(__func__);
      return &ui_data->base;
    }
    case IDP_UI_DATA_TYPE_INT: {
      IDPropertyUIDataInt *ui_data = MEM_callocN<IDPropertyUIDataInt>(__func__);
      ui_data->min = INT_MIN;
      ui_data->max = INT_MAX;
      ui_data->soft_min = INT_MIN;
      ui_data->soft_max = INT_MAX;
      ui_data->step = 1;
      return &ui_data->base;
    }
    case IDP_UI_DATA_TYPE_BOOLEAN: {
      IDPropertyUIDataBool *ui_data = MEM_callocN<IDPropertyUIDataBool>(__func__);
      return &ui_data->base;
    }
    case IDP_UI_DATA_TYPE_FLOAT: {
      IDPropertyUIDataFloat *ui_data = MEM_callocN<IDPropertyUIDataFloat>(__func__);
      ui_data->min = -FLT_MAX;
      ui_data->max = FLT_MAX;
      ui_data->soft_min = -FLT_MAX;
      ui_data->soft_max = FLT_MAX;
      ui_data->step = 1.0f;
      ui_data->precision = 3;
      return &ui_data->base;
    }
    case IDP_UI_DATA_TYPE_UNSUPPORTED: {
      /* UI data not supported for remaining types, this shouldn't be called in those cases. */
      BLI_assert_unreachable();
      break;
    }
  }
  return nullptr;
}

IDPropertyUIData *IDP_ui_data_ensure(IDProperty *prop)
{
  if (prop->ui_data != nullptr) {
    return prop->ui_data;
  }
  prop->ui_data = ui_data_alloc(IDP_ui_data_type(prop));
  return prop->ui_data;
}

static IDPropertyUIData *convert_base_ui_data(IDPropertyUIData *src,
                                              const eIDPropertyUIDataType dst_type)
{
  IDPropertyUIData *dst = ui_data_alloc(dst_type);
  *dst = *src;
  src->description = nullptr;
  return dst;
}

IDPropertyUIData *IDP_TryConvertUIData(IDPropertyUIData *src,
                                       const eIDPropertyUIDataType src_type,
                                       const eIDPropertyUIDataType dst_type)
{
  switch (src_type) {
    case IDP_UI_DATA_TYPE_STRING: {
      switch (dst_type) {
        case IDP_UI_DATA_TYPE_STRING:
          return src;
        case IDP_UI_DATA_TYPE_INT:
        case IDP_UI_DATA_TYPE_BOOLEAN:
        case IDP_UI_DATA_TYPE_FLOAT:
        case IDP_UI_DATA_TYPE_ID: {
          IDPropertyUIData *dst = convert_base_ui_data(src, dst_type);
          ui_data_free(src, src_type);
          return dst;
        }
        case IDP_UI_DATA_TYPE_UNSUPPORTED:
          break;
      }
      break;
    }
    case IDP_UI_DATA_TYPE_ID: {
      switch (dst_type) {
        case IDP_UI_DATA_TYPE_ID:
          return src;
        case IDP_UI_DATA_TYPE_STRING:
        case IDP_UI_DATA_TYPE_INT:
        case IDP_UI_DATA_TYPE_BOOLEAN:
        case IDP_UI_DATA_TYPE_FLOAT: {
          IDPropertyUIData *dst = convert_base_ui_data(src, dst_type);
          ui_data_free(src, src_type);
          return dst;
        }
        case IDP_UI_DATA_TYPE_UNSUPPORTED:
          break;
      }
      break;
    }
    case IDP_UI_DATA_TYPE_INT: {
      IDPropertyUIDataInt *src_int = reinterpret_cast<IDPropertyUIDataInt *>(src);
      switch (dst_type) {
        case IDP_UI_DATA_TYPE_INT:
          return src;
        case IDP_UI_DATA_TYPE_ID:
        case IDP_UI_DATA_TYPE_STRING: {
          IDPropertyUIData *dst = convert_base_ui_data(src, dst_type);
          ui_data_free(src, src_type);
          return dst;
        }
        case IDP_UI_DATA_TYPE_BOOLEAN: {
          IDPropertyUIDataBool *dst = reinterpret_cast<IDPropertyUIDataBool *>(
              convert_base_ui_data(src, dst_type));
          dst->default_value = src_int->default_value != 0;
          if (src_int->default_array) {
            dst->default_array = MEM_malloc_arrayN<int8_t>(size_t(src_int->default_array_len),
                                                           __func__);
            for (int i = 0; i < src_int->default_array_len; i++) {
              dst->default_array[i] = src_int->default_array[i] != 0;
            }
          }
          ui_data_free(src, src_type);
          return &dst->base;
        }
        case IDP_UI_DATA_TYPE_FLOAT: {
          IDPropertyUIDataFloat *dst = reinterpret_cast<IDPropertyUIDataFloat *>(
              convert_base_ui_data(src, dst_type));
          dst->min = double(src_int->min);
          dst->max = double(src_int->max);
          dst->soft_min = double(src_int->soft_min);
          dst->soft_max = double(src_int->soft_max);
          dst->step = float(src_int->step);
          dst->default_value = double(src_int->default_value);
          if (src_int->default_array) {
            dst->default_array = MEM_malloc_arrayN<double>(size_t(src_int->default_array_len),
                                                           __func__);
            for (int i = 0; i < src_int->default_array_len; i++) {
              dst->default_array[i] = double(src_int->default_array[i]);
            }
          }
          ui_data_free(src, src_type);
          return &dst->base;
        }
        case IDP_UI_DATA_TYPE_UNSUPPORTED:
          break;
      }
      break;
    }
    case IDP_UI_DATA_TYPE_BOOLEAN: {
      IDPropertyUIDataBool *src_bool = reinterpret_cast<IDPropertyUIDataBool *>(src);
      switch (dst_type) {
        case IDP_UI_DATA_TYPE_BOOLEAN:
          return src;
        case IDP_UI_DATA_TYPE_ID:
        case IDP_UI_DATA_TYPE_STRING: {
          IDPropertyUIData *dst = convert_base_ui_data(src, dst_type);
          ui_data_free(src, src_type);
          return dst;
        }
        case IDP_UI_DATA_TYPE_INT: {
          IDPropertyUIDataInt *dst = reinterpret_cast<IDPropertyUIDataInt *>(
              convert_base_ui_data(src, dst_type));
          dst->min = 0;
          dst->max = 1;
          dst->soft_min = 0;
          dst->soft_max = 1;
          dst->step = 1;
          dst->default_value = int(src_bool->default_value);
          if (src_bool->default_array) {
            dst->default_array = MEM_malloc_arrayN<int>(size_t(src_bool->default_array_len),
                                                        __func__);
            for (int i = 0; i < src_bool->default_array_len; i++) {
              dst->default_array[i] = int(src_bool->default_array[i]);
            }
          }
          ui_data_free(src, src_type);
          return &dst->base;
        }
        case IDP_UI_DATA_TYPE_FLOAT: {
          IDPropertyUIDataFloat *dst = reinterpret_cast<IDPropertyUIDataFloat *>(
              convert_base_ui_data(src, dst_type));
          dst->min = 0.0;
          dst->max = 1.0;
          dst->soft_min = 0.0;
          dst->soft_max = 1.0;
          dst->step = 1.0;
          if (src_bool->default_array) {
            dst->default_array = MEM_malloc_arrayN<double>(size_t(src_bool->default_array_len),
                                                           __func__);
            for (int i = 0; i < src_bool->default_array_len; i++) {
              dst->default_array[i] = src_bool->default_array[i] == 0 ? 0.0 : 1.0;
            }
          }
          ui_data_free(src, src_type);
          return &dst->base;
        }
        case IDP_UI_DATA_TYPE_UNSUPPORTED:
          break;
      }
      break;
    }
    case IDP_UI_DATA_TYPE_FLOAT: {
      IDPropertyUIDataFloat *src_float = reinterpret_cast<IDPropertyUIDataFloat *>(src);
      switch (dst_type) {
        case IDP_UI_DATA_TYPE_FLOAT:
          return src;
        case IDP_UI_DATA_TYPE_ID:
        case IDP_UI_DATA_TYPE_STRING:
          return convert_base_ui_data(src, dst_type);
        case IDP_UI_DATA_TYPE_INT: {
          auto clamp_double_to_int = [](const double value) {
            return int(std::clamp<double>(value, INT_MIN, INT_MAX));
          };
          IDPropertyUIDataInt *dst = reinterpret_cast<IDPropertyUIDataInt *>(
              convert_base_ui_data(src, dst_type));
          dst->min = clamp_double_to_int(src_float->min);
          dst->max = clamp_double_to_int(src_float->max);
          dst->soft_min = clamp_double_to_int(src_float->soft_min);
          dst->soft_max = clamp_double_to_int(src_float->soft_max);
          dst->step = clamp_double_to_int(src_float->step);
          dst->default_value = clamp_double_to_int(src_float->default_value);
          if (src_float->default_array) {
            dst->default_array = MEM_malloc_arrayN<int>(size_t(src_float->default_array_len),
                                                        __func__);
            for (int i = 0; i < src_float->default_array_len; i++) {
              dst->default_array[i] = clamp_double_to_int(src_float->default_array[i]);
            }
          }
          ui_data_free(src, src_type);
          return &dst->base;
        }
        case IDP_UI_DATA_TYPE_BOOLEAN: {
          IDPropertyUIDataBool *dst = reinterpret_cast<IDPropertyUIDataBool *>(
              convert_base_ui_data(src, dst_type));
          dst->default_value = src_float->default_value > 0.0f;
          if (src_float->default_array) {
            dst->default_array = MEM_malloc_arrayN<int8_t>(size_t(src_float->default_array_len),
                                                           __func__);
            for (int i = 0; i < src_float->default_array_len; i++) {
              dst->default_array[i] = src_float->default_array[i] > 0.0f;
            }
          }
          ui_data_free(src, src_type);
          return &dst->base;
        }
        case IDP_UI_DATA_TYPE_UNSUPPORTED:
          break;
      }
      break;
    }
    case IDP_UI_DATA_TYPE_UNSUPPORTED:
      break;
  }
  ui_data_free(src, src_type);
  return nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Debugging
 * \{ */

#ifndef NDEBUG
const IDProperty *_IDP_assert_type(const IDProperty *prop, const char ty)
{
  BLI_assert(prop->type == ty);
  return prop;
}
const IDProperty *_IDP_assert_type_and_subtype(const IDProperty *prop,
                                               const char ty,
                                               const char sub_ty)
{
  BLI_assert((prop->type == ty) && (prop->subtype == sub_ty));
  return prop;
}

const IDProperty *_IDP_assert_type_mask(const IDProperty *prop, const int ty_mask)
{
  BLI_assert(1 << int(prop->type) & ty_mask);
  return prop;
}
#endif /* !NDEBUG */

/** \} */
