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

#include "BLI_endian_switch.h"
#include "BLI_listbase.h"
#include "BLI_math_base.h"
#include "BLI_set.hh"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_global.hh"
#include "BKE_idprop.h"
#include "BKE_lib_id.hh"

#include "CLG_log.h"

#include "MEM_guardedalloc.h"

#include "BLO_read_write.hh"

#include "BLI_strict_flags.h" /* Keep last. */

/* IDPropertyTemplate is a union in DNA_ID.h */

/**
 * if the new is 'IDP_ARRAY_REALLOC_LIMIT' items less,
 * than #IDProperty.totallen, reallocate anyway.
 */
#define IDP_ARRAY_REALLOC_LIMIT 200

static CLG_LogRef LOG = {"bke.idprop"};

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

#define GETPROP(prop, i) &(IDP_IDPArray(prop)[i])

IDProperty *IDP_NewIDPArray(const char *name)
{
  IDProperty *prop = static_cast<IDProperty *>(
      MEM_callocN(sizeof(IDProperty), "IDProperty prop array"));
  prop->type = IDP_IDPARRAY;
  prop->len = 0;
  STRNCPY(prop->name, name);

  return prop;
}

IDProperty *IDP_CopyIDPArray(const IDProperty *array, const int flag)
{
  /* don't use MEM_dupallocN because this may be part of an array */
  BLI_assert(array->type == IDP_IDPARRAY);

  IDProperty *narray = static_cast<IDProperty *>(MEM_mallocN(sizeof(IDProperty), __func__));
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

void IDP_ResizeIDPArray(IDProperty *prop, int newlen)
{
  BLI_assert(prop->type == IDP_IDPARRAY);

  /* first check if the array buffer size has room */
  if (newlen <= prop->totallen) {
    if (newlen < prop->len && prop->totallen - newlen < IDP_ARRAY_REALLOC_LIMIT) {
      for (int i = newlen; i < prop->len; i++) {
        IDP_FreePropertyContent(GETPROP(prop, i));
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
      IDP_FreePropertyContent(GETPROP(prop, i));
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
    IDPropertyTemplate val;

    for (int a = prop->len; a < newlen; a++) {
      val.i = 0; /* silence MSVC warning about uninitialized var when debugging */
      array[a] = IDP_New(IDP_GROUP, &val, "IDP_ResizeArray group");
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
  IDProperty *newp = static_cast<IDProperty *>(MEM_callocN(sizeof(IDProperty), __func__));

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
  IDProperty *newp = idp_generic_copy(prop, flag);

  if (prop->data.pointer) {
    newp->data.pointer = MEM_dupallocN(prop->data.pointer);

    if (prop->type == IDP_GROUP) {
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

IDProperty *IDP_NewStringMaxSize(const char *st, const size_t st_maxncpy, const char *name)
{
  IDProperty *prop = static_cast<IDProperty *>(
      MEM_callocN(sizeof(IDProperty), "IDProperty string"));

  if (st == nullptr) {
    prop->data.pointer = MEM_mallocN(DEFAULT_ALLOC_FOR_NULL_STRINGS, "id property string 1");
    *IDP_String(prop) = '\0';
    prop->totallen = DEFAULT_ALLOC_FOR_NULL_STRINGS;
    prop->len = 1; /* nullptr string, has len of 1 to account for null byte. */
  }
  else {
    /* include null terminator '\0' */
    const int stlen = int((st_maxncpy > 0) ? BLI_strnlen(st, st_maxncpy - 1) : strlen(st)) + 1;

    prop->data.pointer = MEM_mallocN(size_t(stlen), "id property string 2");
    prop->len = prop->totallen = stlen;

    /* Ensured above, must always be true otherwise null terminator assignment will be invalid. */
    BLI_assert(stlen > 0);
    if (stlen > 1) {
      memcpy(prop->data.pointer, st, size_t(stlen));
    }
    IDP_String(prop)[stlen - 1] = '\0';
  }

  prop->type = IDP_STRING;
  STRNCPY(prop->name, name);

  return prop;
}

IDProperty *IDP_NewString(const char *st, const char *name)
{
  return IDP_NewStringMaxSize(st, 0, name);
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
  BLI_assert(prop->type == IDP_STRING);
  const bool is_byte = prop->subtype == IDP_STRING_SUB_BYTE;
  const int stlen = int((st_maxncpy > 0) ? BLI_strnlen(st, st_maxncpy - 1) : strlen(st)) +
                    (is_byte ? 0 : 1);
  IDP_ResizeArray(prop, stlen);
  if (stlen > 0) {
    memcpy(prop->data.pointer, st, size_t(stlen));
    if (is_byte == false) {
      IDP_String(prop)[stlen - 1] = '\0';
    }
  }
}

void IDP_AssignString(IDProperty *prop, const char *st)
{
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

  const int value = IDP_Int(prop);
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
    id_us_plus(IDP_Id(newp));
  }

  return newp;
}

void IDP_AssignID(IDProperty *prop, ID *id, const int flag)
{
  BLI_assert(prop->type == IDP_ID);

  if ((flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0 && IDP_Id(prop) != nullptr) {
    id_us_min(IDP_Id(prop));
  }

  prop->data.pointer = id;

  if ((flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0) {
    id_us_plus(IDP_Id(prop));
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
  newp->len = prop->len;
  newp->subtype = prop->subtype;

  LISTBASE_FOREACH (IDProperty *, link, &prop->data.group) {
    BLI_addtail(&newp->data.group, IDP_CopyProperty_ex(link, flag));
  }

  return newp;
}

void IDP_SyncGroupValues(IDProperty *dest, const IDProperty *src)
{
  BLI_assert(dest->type == IDP_GROUP);
  BLI_assert(src->type == IDP_GROUP);

  LISTBASE_FOREACH (IDProperty *, prop, &src->data.group) {
    IDProperty *other = static_cast<IDProperty *>(
        BLI_findstring(&dest->data.group, prop->name, offsetof(IDProperty, name)));
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
          BLI_insertlinkreplace(&dest->data.group, other, IDP_CopyProperty(prop));
          IDP_FreeProperty(other);
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
        BLI_insertlinkreplace(&dest->data.group, prop_dst, IDP_CopyProperty(prop_src));
        IDP_FreeProperty(prop_dst);
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
    IDProperty *loop;
    for (loop = static_cast<IDProperty *>(dest->data.group.first); loop; loop = loop->next) {
      if (STREQ(loop->name, prop->name)) {
        BLI_insertlinkreplace(&dest->data.group, loop, IDP_CopyProperty(prop));
        IDP_FreeProperty(loop);
        break;
      }
    }

    /* only add at end if not added yet */
    if (loop == nullptr) {
      IDProperty *copy = IDP_CopyProperty(prop);
      dest->len++;
      BLI_addtail(&dest->data.group, copy);
    }
  }
}

void IDP_ReplaceInGroup_ex(IDProperty *group, IDProperty *prop, IDProperty *prop_exist)
{
  BLI_assert(group->type == IDP_GROUP);
  BLI_assert(prop_exist == IDP_GetPropertyFromGroup(group, prop->name));

  if (prop_exist != nullptr) {
    BLI_insertlinkreplace(&group->data.group, prop_exist, prop);
    IDP_FreeProperty(prop_exist);
  }
  else {
    group->len++;
    BLI_addtail(&group->data.group, prop);
  }
}

void IDP_ReplaceInGroup(IDProperty *group, IDProperty *prop)
{
  IDProperty *prop_exist = IDP_GetPropertyFromGroup(group, prop->name);

  IDP_ReplaceInGroup_ex(group, prop, prop_exist);
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
      IDP_ReplaceInGroup(dest, copy);
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
        IDProperty *copy = IDP_CopyProperty_ex(prop, flag);
        dest->len++;
        BLI_addtail(&dest->data.group, copy);
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

  if (IDP_GetPropertyFromGroup(group, prop->name) == nullptr) {
    group->len++;
    BLI_addtail(&group->data.group, prop);
    return true;
  }

  return false;
}

bool IDP_InsertToGroup(IDProperty *group, IDProperty *previous, IDProperty *pnew)
{
  BLI_assert(group->type == IDP_GROUP);

  if (IDP_GetPropertyFromGroup(group, pnew->name) == nullptr) {
    group->len++;
    BLI_insertlinkafter(&group->data.group, previous, pnew);
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
}

void IDP_FreeFromGroup(IDProperty *group, IDProperty *prop)
{
  IDP_RemoveFromGroup(group, prop);
  IDP_FreeProperty(prop);
}

IDProperty *IDP_GetPropertyFromGroup(const IDProperty *prop, const char *name)
{
  BLI_assert(prop->type == IDP_GROUP);

  return (IDProperty *)BLI_findstring(&prop->data.group, name, offsetof(IDProperty, name));
}
IDProperty *IDP_GetPropertyTypeFromGroup(const IDProperty *prop, const char *name, const char type)
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
      return IDP_Int(prop);
    case IDP_DOUBLE:
      return int(IDP_Double(prop));
    case IDP_FLOAT:
      return int(IDP_Float(prop));
    case IDP_BOOLEAN:
      return int(IDP_Bool(prop));
    default:
      return 0;
  }
}

double IDP_coerce_to_double_or_zero(const IDProperty *prop)
{
  switch (prop->type) {
    case IDP_DOUBLE:
      return IDP_Double(prop);
    case IDP_FLOAT:
      return double(IDP_Float(prop));
    case IDP_INT:
      return double(IDP_Int(prop));
    case IDP_BOOLEAN:
      return double(IDP_Bool(prop));
    default:
      return 0.0;
  }
}

float IDP_coerce_to_float_or_zero(const IDProperty *prop)
{
  switch (prop->type) {
    case IDP_FLOAT:
      return IDP_Float(prop);
    case IDP_DOUBLE:
      return float(IDP_Double(prop));
    case IDP_INT:
      return float(IDP_Int(prop));
    case IDP_BOOLEAN:
      return float(IDP_Bool(prop));
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
    id->properties = static_cast<IDProperty *>(MEM_callocN(sizeof(IDProperty), "IDProperty"));
    id->properties->type = IDP_GROUP;
    /* NOTE(@ideasman42): Don't overwrite the data's name and type
     * some functions might need this if they
     * don't have a real ID, should be named elsewhere. */
    // STRNCPY(id->name, "top_level_group");
  }
  return id->properties;
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
      return (IDP_Int(prop1) == IDP_Int(prop2));
    case IDP_FLOAT:
#if !defined(NDEBUG) && defined(WITH_PYTHON)
    {
      float p1 = IDP_Float(prop1);
      float p2 = IDP_Float(prop2);
      if ((p1 != p2) && ((fabsf(p1 - p2) / max_ff(p1, p2)) < 0.001f)) {
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
      return (IDP_Float(prop1) == IDP_Float(prop2));
    case IDP_DOUBLE:
      return (IDP_Double(prop1) == IDP_Double(prop2));
    case IDP_BOOLEAN:
      return (IDP_Bool(prop1) == IDP_Bool(prop2));
    case IDP_STRING: {
      return ((prop1->len == prop2->len) &&
              STREQLEN(IDP_String(prop1), IDP_String(prop2), size_t(prop1->len)));
    }
    case IDP_ARRAY:
      if (prop1->len == prop2->len && prop1->subtype == prop2->subtype) {
        return (memcmp(IDP_Array(prop1),
                       IDP_Array(prop2),
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
      const IDProperty *array1 = IDP_IDPArray(prop1);
      const IDProperty *array2 = IDP_IDPArray(prop2);

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
      return (IDP_Id(prop1) == IDP_Id(prop2));
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

IDProperty *IDP_New(const char type, const IDPropertyTemplate *val, const char *name)
{
  IDProperty *prop = nullptr;

  switch (type) {
    case IDP_INT:
      prop = static_cast<IDProperty *>(MEM_callocN(sizeof(IDProperty), "IDProperty int"));
      prop->data.val = val->i;
      break;
    case IDP_FLOAT:
      prop = static_cast<IDProperty *>(MEM_callocN(sizeof(IDProperty), "IDProperty float"));
      *(float *)&prop->data.val = val->f;
      break;
    case IDP_DOUBLE:
      prop = static_cast<IDProperty *>(MEM_callocN(sizeof(IDProperty), "IDProperty double"));
      *(double *)&prop->data.val = val->d;
      break;
    case IDP_BOOLEAN:
      prop = static_cast<IDProperty *>(MEM_callocN(sizeof(IDProperty), "IDProperty boolean"));
      prop->data.val = bool(val->i);
      break;
    case IDP_ARRAY: {
      if (ELEM(val->array.type, IDP_FLOAT, IDP_INT, IDP_DOUBLE, IDP_GROUP, IDP_BOOLEAN)) {
        prop = static_cast<IDProperty *>(MEM_callocN(sizeof(IDProperty), "IDProperty array"));
        prop->subtype = val->array.type;
        if (val->array.len) {
          prop->data.pointer = MEM_callocN(
              idp_size_table[val->array.type] * size_t(val->array.len), "id property array");
        }
        prop->len = prop->totallen = val->array.len;
        break;
      }
      CLOG_ERROR(&LOG, "bad array type.");
      return nullptr;
    }
    case IDP_STRING: {
      const char *st = val->string.str;

      prop = static_cast<IDProperty *>(MEM_callocN(sizeof(IDProperty), "IDProperty string"));
      if (val->string.subtype == IDP_STRING_SUB_BYTE) {
        /* NOTE: Intentionally not null terminated. */
        if (st == nullptr) {
          prop->data.pointer = MEM_mallocN(DEFAULT_ALLOC_FOR_NULL_STRINGS, "id property string 1");
          *IDP_String(prop) = '\0';
          prop->totallen = DEFAULT_ALLOC_FOR_NULL_STRINGS;
          prop->len = 0;
        }
        else {
          prop->data.pointer = MEM_mallocN(size_t(val->string.len), "id property string 2");
          prop->len = prop->totallen = val->string.len;
          memcpy(prop->data.pointer, st, size_t(val->string.len));
        }
        prop->subtype = IDP_STRING_SUB_BYTE;
      }
      else {
        if (st == nullptr || val->string.len <= 1) {
          prop->data.pointer = MEM_mallocN(DEFAULT_ALLOC_FOR_NULL_STRINGS, "id property string 1");
          *IDP_String(prop) = '\0';
          prop->totallen = DEFAULT_ALLOC_FOR_NULL_STRINGS;
          /* nullptr string, has len of 1 to account for null byte. */
          prop->len = 1;
        }
        else {
          BLI_assert(int(val->string.len) <= int(strlen(st)) + 1);
          prop->data.pointer = MEM_mallocN(size_t(val->string.len), "id property string 3");
          memcpy(prop->data.pointer, st, size_t(val->string.len) - 1);
          IDP_String(prop)[val->string.len - 1] = '\0';
          prop->len = prop->totallen = val->string.len;
        }
        prop->subtype = IDP_STRING_SUB_UTF8;
      }
      break;
    }
    case IDP_GROUP: {
      /* Values are set properly by calloc. */
      prop = static_cast<IDProperty *>(MEM_callocN(sizeof(IDProperty), "IDProperty group"));
      break;
    }
    case IDP_ID: {
      prop = static_cast<IDProperty *>(MEM_callocN(sizeof(IDProperty), "IDProperty datablock"));
      prop->data.pointer = (void *)val->id;
      prop->type = IDP_ID;
      id_us_plus(IDP_Id(prop));
      break;
    }
    default: {
      prop = static_cast<IDProperty *>(MEM_callocN(sizeof(IDProperty), "IDProperty array"));
      break;
    }
  }

  prop->type = type;
  STRNCPY(prop->name, name);

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

void IDP_ui_data_free(IDProperty *prop)
{
  switch (IDP_ui_data_type(prop)) {
    case IDP_UI_DATA_TYPE_STRING: {
      IDPropertyUIDataString *ui_data_string = (IDPropertyUIDataString *)prop->ui_data;
      MEM_SAFE_FREE(ui_data_string->default_value);
      break;
    }
    case IDP_UI_DATA_TYPE_ID: {
      break;
    }
    case IDP_UI_DATA_TYPE_INT: {
      IDPropertyUIDataInt *ui_data_int = (IDPropertyUIDataInt *)prop->ui_data;
      MEM_SAFE_FREE(ui_data_int->default_array);
      IDP_int_ui_data_free_enum_items(ui_data_int);
      break;
    }
    case IDP_UI_DATA_TYPE_BOOLEAN: {
      IDPropertyUIDataBool *ui_data_bool = (IDPropertyUIDataBool *)prop->ui_data;
      MEM_SAFE_FREE(ui_data_bool->default_array);
      break;
    }
    case IDP_UI_DATA_TYPE_FLOAT: {
      IDPropertyUIDataFloat *ui_data_float = (IDPropertyUIDataFloat *)prop->ui_data;
      MEM_SAFE_FREE(ui_data_float->default_array);
      break;
    }
    case IDP_UI_DATA_TYPE_UNSUPPORTED: {
      break;
    }
  }

  MEM_SAFE_FREE(prop->ui_data->description);

  MEM_freeN(prop->ui_data);
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
        id_us_min(IDP_Id(prop));
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
                          IDPForeachPropertyCallback callback,
                          void *user_data)
{
  if (!id_property_root) {
    return;
  }

  if (type_filter == 0 || (1 << id_property_root->type) & type_filter) {
    callback(id_property_root, user_data);
  }

  /* Recursive call into container types of ID properties. */
  switch (id_property_root->type) {
    case IDP_GROUP: {
      LISTBASE_FOREACH (IDProperty *, loop, &id_property_root->data.group) {
        IDP_foreach_property(loop, type_filter, callback, user_data);
      }
      break;
    }
    case IDP_IDPARRAY: {
      IDProperty *loop = static_cast<IDProperty *>(IDP_Array(id_property_root));
      for (int i = 0; i < id_property_root->len; i++) {
        IDP_foreach_property(&loop[i], type_filter, callback, user_data);
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
    BLO_write_raw(writer, MEM_allocN_len(prop->data.pointer), prop->data.pointer);

    if (prop->subtype == IDP_GROUP) {
      IDProperty **array = static_cast<IDProperty **>(prop->data.pointer);
      int a;

      for (a = 0; a < prop->len; a++) {
        IDP_BlendWrite(writer, array[a]);
      }
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
  BLO_write_raw(writer, size_t(prop->len), prop->data.pointer);
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
  BLO_read_data_address(reader, &prop->ui_data);
  if (!prop->ui_data) {
    /* Can happen when opening more recent files with unknown types of IDProperties. */
    return;
  }
  BLO_read_data_address(reader, &prop->ui_data->description);

  switch (IDP_ui_data_type(prop)) {
    case IDP_UI_DATA_TYPE_STRING: {
      IDPropertyUIDataString *ui_data_string = (IDPropertyUIDataString *)prop->ui_data;
      BLO_read_data_address(reader, &ui_data_string->default_value);
      break;
    }
    case IDP_UI_DATA_TYPE_ID: {
      break;
    }
    case IDP_UI_DATA_TYPE_INT: {
      IDPropertyUIDataInt *ui_data_int = (IDPropertyUIDataInt *)prop->ui_data;
      if (prop->type == IDP_ARRAY) {
        BLO_read_int32_array(
            reader, ui_data_int->default_array_len, (int **)&ui_data_int->default_array);
      }
      BLO_read_data_address(reader, &ui_data_int->enum_items);
      for (const int64_t i : blender::IndexRange(ui_data_int->enum_items_num)) {
        IDPropertyUIDataEnumItem &item = ui_data_int->enum_items[i];
        BLO_read_data_address(reader, &item.identifier);
        BLO_read_data_address(reader, &item.name);
        BLO_read_data_address(reader, &item.description);
      }
      break;
    }
    case IDP_UI_DATA_TYPE_BOOLEAN: {
      IDPropertyUIDataBool *ui_data_bool = (IDPropertyUIDataBool *)prop->ui_data;
      if (prop->type == IDP_ARRAY) {
        BLO_read_int8_array(
            reader, ui_data_bool->default_array_len, (int8_t **)&ui_data_bool->default_array);
      }
      break;
    }
    case IDP_UI_DATA_TYPE_FLOAT: {
      IDPropertyUIDataFloat *ui_data_float = (IDPropertyUIDataFloat *)prop->ui_data;
      if (prop->type == IDP_ARRAY) {
        BLO_read_double_array(
            reader, ui_data_float->default_array_len, (double **)&ui_data_float->default_array);
      }
      break;
    }
    case IDP_UI_DATA_TYPE_UNSUPPORTED: {
      BLI_assert_unreachable();
      break;
    }
  }
}

static void IDP_DirectLinkIDPArray(IDProperty *prop, BlendDataReader *reader)
{
  /* since we didn't save the extra buffer, set totallen to len */
  prop->totallen = prop->len;
  BLO_read_data_address(reader, &prop->data.pointer);

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

  if (prop->subtype == IDP_GROUP) {
    BLO_read_pointer_array(reader, &prop->data.pointer);
    IDProperty **array = static_cast<IDProperty **>(prop->data.pointer);

    for (int i = 0; i < prop->len; i++) {
      IDP_DirectLinkProperty(array[i], reader);
    }
  }
  else if (prop->subtype == IDP_DOUBLE) {
    BLO_read_double_array(reader, prop->len, (double **)&prop->data.pointer);
  }
  else if (ELEM(prop->subtype, IDP_INT, IDP_FLOAT)) {
    /* also used for floats */
    BLO_read_int32_array(reader, prop->len, (int **)&prop->data.pointer);
  }
  else if (prop->subtype == IDP_BOOLEAN) {
    BLO_read_int8_array(reader, prop->len, (int8_t **)&prop->data.pointer);
  }
}

static void IDP_DirectLinkString(IDProperty *prop, BlendDataReader *reader)
{
  /* Since we didn't save the extra string buffer, set totallen to len. */
  prop->totallen = prop->len;
  BLO_read_data_address(reader, &prop->data.pointer);
}

static void IDP_DirectLinkGroup(IDProperty *prop, BlendDataReader *reader)
{
  ListBase *lb = &prop->data.group;

  BLO_read_list(reader, lb);

  /* Link child id properties now. */
  LISTBASE_FOREACH (IDProperty *, loop, &prop->data.group) {
    IDP_DirectLinkProperty(loop, reader);
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
      /* Workaround for doubles.
       * They are stored in the same field as `int val, val2` in the #IDPropertyData struct,
       * they have to deal with endianness specifically.
       *
       * In theory, val and val2 would've already been swapped
       * if switch_endian is true, so we have to first un-swap
       * them then re-swap them as a single 64-bit entity. */
      if (BLO_read_requires_endian_switch(reader)) {
        BLI_endian_switch_int32(&prop->data.val);
        BLI_endian_switch_int32(&prop->data.val2);
        BLI_endian_switch_int64((int64_t *)&prop->data.val);
      }
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
      IDP_Int(prop) = 0;
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

IDPropertyUIData *IDP_ui_data_ensure(IDProperty *prop)
{
  if (prop->ui_data != nullptr) {
    return prop->ui_data;
  }

  switch (IDP_ui_data_type(prop)) {
    case IDP_UI_DATA_TYPE_STRING: {
      prop->ui_data = static_cast<IDPropertyUIData *>(
          MEM_callocN(sizeof(IDPropertyUIDataString), __func__));
      break;
    }
    case IDP_UI_DATA_TYPE_ID: {
      IDPropertyUIDataID *ui_data = static_cast<IDPropertyUIDataID *>(
          MEM_callocN(sizeof(IDPropertyUIDataID), __func__));
      prop->ui_data = (IDPropertyUIData *)ui_data;
      break;
    }
    case IDP_UI_DATA_TYPE_INT: {
      IDPropertyUIDataInt *ui_data = static_cast<IDPropertyUIDataInt *>(
          MEM_callocN(sizeof(IDPropertyUIDataInt), __func__));
      ui_data->min = INT_MIN;
      ui_data->max = INT_MAX;
      ui_data->soft_min = INT_MIN;
      ui_data->soft_max = INT_MAX;
      ui_data->step = 1;
      prop->ui_data = (IDPropertyUIData *)ui_data;
      break;
    }
    case IDP_UI_DATA_TYPE_BOOLEAN: {
      IDPropertyUIDataBool *ui_data = static_cast<IDPropertyUIDataBool *>(
          MEM_callocN(sizeof(IDPropertyUIDataBool), __func__));
      prop->ui_data = (IDPropertyUIData *)ui_data;
      break;
    }
    case IDP_UI_DATA_TYPE_FLOAT: {
      IDPropertyUIDataFloat *ui_data = static_cast<IDPropertyUIDataFloat *>(
          MEM_callocN(sizeof(IDPropertyUIDataFloat), __func__));
      ui_data->min = -FLT_MAX;
      ui_data->max = FLT_MAX;
      ui_data->soft_min = -FLT_MAX;
      ui_data->soft_max = FLT_MAX;
      ui_data->step = 1.0f;
      ui_data->precision = 3;
      prop->ui_data = (IDPropertyUIData *)ui_data;
      break;
    }
    case IDP_UI_DATA_TYPE_UNSUPPORTED: {
      /* UI data not supported for remaining types, this shouldn't be called in those cases. */
      BLI_assert_unreachable();
      break;
    }
  }

  return prop->ui_data;
}

/** \} */
