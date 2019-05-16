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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "BLI_utildefines.h"
#include "BLI_string.h"
#include "BLI_listbase.h"
#include "BLI_math.h"

#include "BKE_idprop.h"
#include "BKE_library.h"

#include "CLG_log.h"

#include "MEM_guardedalloc.h"

#include "BLI_strict_flags.h"

/* IDPropertyTemplate is a union in DNA_ID.h */

/**
 * if the new is 'IDP_ARRAY_REALLOC_LIMIT' items less,
 * than #IDProperty.totallen, reallocate anyway.
 */
#define IDP_ARRAY_REALLOC_LIMIT 200

static CLG_LogRef LOG = {"bke.idprop"};

/*local size table.*/
static size_t idp_size_table[] = {
    1, /*strings*/
    sizeof(int),
    sizeof(float),
    sizeof(float) * 3,  /*Vector type, deprecated*/
    sizeof(float) * 16, /*Matrix type, deprecated*/
    0,                  /*arrays don't have a fixed size*/
    sizeof(ListBase),   /*Group type*/
    sizeof(void *),
    sizeof(double),
};

/* -------------------------------------------------------------------- */
/* Array Functions */

/** \name IDP Array API
 * \{ */

#define GETPROP(prop, i) &(IDP_IDPArray(prop)[i])

/* --------- property array type -------------*/

/**
 * \note as a start to move away from the stupid IDP_New function, this type
 * has it's own allocation function.
 */
IDProperty *IDP_NewIDPArray(const char *name)
{
  IDProperty *prop = MEM_callocN(sizeof(IDProperty), "IDProperty prop array");
  prop->type = IDP_IDPARRAY;
  prop->len = 0;
  BLI_strncpy(prop->name, name, MAX_IDPROP_NAME);

  return prop;
}

IDProperty *IDP_CopyIDPArray(const IDProperty *array, const int flag)
{
  /* don't use MEM_dupallocN because this may be part of an array */
  IDProperty *narray, *tmp;
  int i;

  BLI_assert(array->type == IDP_IDPARRAY);

  narray = MEM_mallocN(sizeof(IDProperty), __func__);
  *narray = *array;

  narray->data.pointer = MEM_dupallocN(array->data.pointer);
  for (i = 0; i < narray->len; i++) {
    /* ok, the copy functions always allocate a new structure,
     * which doesn't work here.  instead, simply copy the
     * contents of the new structure into the array cell,
     * then free it.  this makes for more maintainable
     * code than simply reimplementing the copy functions
     * in this loop.*/
    tmp = IDP_CopyProperty_ex(GETPROP(narray, i), flag);
    memcpy(GETPROP(narray, i), tmp, sizeof(IDProperty));
    MEM_freeN(tmp);
  }

  return narray;
}

static void IDP_FreeIDPArray(IDProperty *prop, const bool do_id_user)
{
  int i;

  BLI_assert(prop->type == IDP_IDPARRAY);

  for (i = 0; i < prop->len; i++) {
    IDP_FreeProperty_ex(GETPROP(prop, i), do_id_user);
  }

  if (prop->data.pointer) {
    MEM_freeN(prop->data.pointer);
  }
}

/* shallow copies item */
void IDP_SetIndexArray(IDProperty *prop, int index, IDProperty *item)
{
  IDProperty *old;

  BLI_assert(prop->type == IDP_IDPARRAY);

  if (index >= prop->len || index < 0) {
    return;
  }

  old = GETPROP(prop, index);
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
  int newsize;

  BLI_assert(prop->type == IDP_IDPARRAY);

  /* first check if the array buffer size has room */
  if (newlen <= prop->totallen) {
    if (newlen < prop->len && prop->totallen - newlen < IDP_ARRAY_REALLOC_LIMIT) {
      int i;

      for (i = newlen; i < prop->len; i++) {
        IDP_FreePropertyContent(GETPROP(prop, i));
      }

      prop->len = newlen;
      return;
    }
    else if (newlen >= prop->len) {
      prop->len = newlen;
      return;
    }
  }

  /* free trailing items */
  if (newlen < prop->len) {
    /* newlen is smaller */
    int i;
    for (i = newlen; i < prop->len; i++) {
      IDP_FreePropertyContent(GETPROP(prop, i));
    }
  }

  /* - Note: This code comes from python, here's the corresponding comment. - */
  /* This over-allocates proportional to the list size, making room
   * for additional growth.  The over-allocation is mild, but is
   * enough to give linear-time amortized behavior over a long
   * sequence of appends() in the presence of a poorly-performing
   * system realloc().
   * The growth pattern is:  0, 4, 8, 16, 25, 35, 46, 58, 72, 88, ...
   */
  newsize = newlen;
  newsize = (newsize >> 3) + (newsize < 9 ? 3 : 6) + newsize;
  prop->data.pointer = MEM_recallocN(prop->data.pointer, sizeof(IDProperty) * (size_t)newsize);
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
    IDProperty **array = newarr;
    IDPropertyTemplate val;
    int a;

    for (a = prop->len; a < newlen; a++) {
      val.i = 0; /* silence MSVC warning about uninitialized var when debugging */
      array[a] = IDP_New(IDP_GROUP, &val, "IDP_ResizeArray group");
    }
  }
  else {
    /* smaller */
    IDProperty **array = prop->data.pointer;
    int a;

    for (a = newlen; a < prop->len; a++) {
      IDP_FreeProperty(array[a]);
    }
  }
}

/*this function works for strings too!*/
void IDP_ResizeArray(IDProperty *prop, int newlen)
{
  int newsize;
  const bool is_grow = newlen >= prop->len;

  /* first check if the array buffer size has room */
  if (newlen <= prop->totallen && prop->totallen - newlen < IDP_ARRAY_REALLOC_LIMIT) {
    idp_resize_group_array(prop, newlen, prop->data.pointer);
    prop->len = newlen;
    return;
  }

  /* - Note: This code comes from python, here's the corresponding comment. - */
  /* This over-allocates proportional to the list size, making room
   * for additional growth.  The over-allocation is mild, but is
   * enough to give linear-time amortized behavior over a long
   * sequence of appends() in the presence of a poorly-performing
   * system realloc().
   * The growth pattern is:  0, 4, 8, 16, 25, 35, 46, 58, 72, 88, ...
   */
  newsize = newlen;
  newsize = (newsize >> 3) + (newsize < 9 ? 3 : 6) + newsize;

  if (is_grow == false) {
    idp_resize_group_array(prop, newlen, prop->data.pointer);
  }

  prop->data.pointer = MEM_recallocN(prop->data.pointer,
                                     idp_size_table[(int)prop->subtype] * (size_t)newsize);

  if (is_grow == true) {
    idp_resize_group_array(prop, newlen, prop->data.pointer);
  }

  prop->len = newlen;
  prop->totallen = newsize;
}

void IDP_FreeArray(IDProperty *prop)
{
  if (prop->data.pointer) {
    idp_resize_group_array(prop, 0, NULL);
    MEM_freeN(prop->data.pointer);
  }
}

static IDProperty *idp_generic_copy(const IDProperty *prop, const int UNUSED(flag))
{
  IDProperty *newp = MEM_callocN(sizeof(IDProperty), __func__);

  BLI_strncpy(newp->name, prop->name, MAX_IDPROP_NAME);
  newp->type = prop->type;
  newp->flag = prop->flag;
  newp->data.val = prop->data.val;
  newp->data.val2 = prop->data.val2;

  return newp;
}

static IDProperty *IDP_CopyArray(const IDProperty *prop, const int flag)
{
  IDProperty *newp = idp_generic_copy(prop, flag);

  if (prop->data.pointer) {
    newp->data.pointer = MEM_dupallocN(prop->data.pointer);

    if (prop->type == IDP_GROUP) {
      IDProperty **array = newp->data.pointer;
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
/* String Functions */

/** \name IDProperty String API
 * \{ */

/**
 *
 * \param st: The string to assign.
 * \param name: The property name.
 * \param maxlen: The size of the new string (including the \0 terminator).
 * \return The new string property.
 */
IDProperty *IDP_NewString(const char *st, const char *name, int maxlen)
{
  IDProperty *prop = MEM_callocN(sizeof(IDProperty), "IDProperty string");

  if (st == NULL) {
    prop->data.pointer = MEM_mallocN(DEFAULT_ALLOC_FOR_NULL_STRINGS, "id property string 1");
    *IDP_String(prop) = '\0';
    prop->totallen = DEFAULT_ALLOC_FOR_NULL_STRINGS;
    prop->len = 1; /* NULL string, has len of 1 to account for null byte. */
  }
  else {
    /* include null terminator '\0' */
    int stlen = (int)strlen(st) + 1;

    if (maxlen > 0 && maxlen < stlen) {
      stlen = maxlen;
    }

    prop->data.pointer = MEM_mallocN((size_t)stlen, "id property string 2");
    prop->len = prop->totallen = stlen;
    BLI_strncpy(prop->data.pointer, st, (size_t)stlen);
  }

  prop->type = IDP_STRING;
  BLI_strncpy(prop->name, name, MAX_IDPROP_NAME);

  return prop;
}

static IDProperty *IDP_CopyString(const IDProperty *prop, const int flag)
{
  IDProperty *newp;

  BLI_assert(prop->type == IDP_STRING);
  newp = idp_generic_copy(prop, flag);

  if (prop->data.pointer) {
    newp->data.pointer = MEM_dupallocN(prop->data.pointer);
  }
  newp->len = prop->len;
  newp->subtype = prop->subtype;
  newp->totallen = prop->totallen;

  return newp;
}

void IDP_AssignString(IDProperty *prop, const char *st, int maxlen)
{
  int stlen;

  BLI_assert(prop->type == IDP_STRING);
  stlen = (int)strlen(st);
  if (maxlen > 0 && maxlen < stlen) {
    stlen = maxlen;
  }

  if (prop->subtype == IDP_STRING_SUB_BYTE) {
    IDP_ResizeArray(prop, stlen);
    memcpy(prop->data.pointer, st, (size_t)stlen);
  }
  else {
    stlen++;
    IDP_ResizeArray(prop, stlen);
    BLI_strncpy(prop->data.pointer, st, (size_t)stlen);
  }
}

void IDP_ConcatStringC(IDProperty *prop, const char *st)
{
  int newlen;

  BLI_assert(prop->type == IDP_STRING);

  newlen = prop->len + (int)strlen(st);
  /* we have to remember that prop->len includes the null byte for strings.
   * so there's no need to add +1 to the resize function.*/
  IDP_ResizeArray(prop, newlen);
  strcat(prop->data.pointer, st);
}

void IDP_ConcatString(IDProperty *str1, IDProperty *append)
{
  int newlen;

  BLI_assert(append->type == IDP_STRING);

  /* since ->len for strings includes the NULL byte, we have to subtract one or
   * we'll get an extra null byte after each concatenation operation.*/
  newlen = str1->len + append->len - 1;
  IDP_ResizeArray(str1, newlen);
  strcat(str1->data.pointer, append->data.pointer);
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
/* ID Type */

/** \name IDProperty ID API
 * \{ */

static IDProperty *IDP_CopyID(const IDProperty *prop, const int flag)
{
  IDProperty *newp;

  BLI_assert(prop->type == IDP_ID);
  newp = idp_generic_copy(prop, flag);

  newp->data.pointer = prop->data.pointer;
  if ((flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0) {
    id_us_plus(IDP_Id(newp));
  }

  return newp;
}

/** \} */

/* -------------------------------------------------------------------- */
/* Group Functions */

/** \name IDProperty Group API
 * \{ */

/**
 * Checks if a property with the same name as prop exists, and if so replaces it.
 */
static IDProperty *IDP_CopyGroup(const IDProperty *prop, const int flag)
{
  IDProperty *newp, *link;

  BLI_assert(prop->type == IDP_GROUP);
  newp = idp_generic_copy(prop, flag);
  newp->len = prop->len;
  newp->subtype = prop->subtype;

  for (link = prop->data.group.first; link; link = link->next) {
    BLI_addtail(&newp->data.group, IDP_CopyProperty_ex(link, flag));
  }

  return newp;
}

/* use for syncing proxies.
 * When values name and types match, copy the values, else ignore */
void IDP_SyncGroupValues(IDProperty *dest, const IDProperty *src)
{
  IDProperty *other, *prop;

  BLI_assert(dest->type == IDP_GROUP);
  BLI_assert(src->type == IDP_GROUP);

  for (prop = src->data.group.first; prop; prop = prop->next) {
    other = BLI_findstring(&dest->data.group, prop->name, offsetof(IDProperty, name));
    if (other && prop->type == other->type) {
      switch (prop->type) {
        case IDP_INT:
        case IDP_FLOAT:
        case IDP_DOUBLE:
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

void IDP_SyncGroupTypes(IDProperty *dst, const IDProperty *src, const bool do_arraylen)
{
  IDProperty *prop_dst, *prop_dst_next;
  const IDProperty *prop_src;

  for (prop_dst = dst->data.group.first; prop_dst; prop_dst = prop_dst_next) {
    prop_dst_next = prop_dst->next;
    if ((prop_src = IDP_GetPropertyFromGroup((IDProperty *)src, prop_dst->name))) {
      /* check of we should replace? */
      if ((prop_dst->type != prop_src->type || prop_dst->subtype != prop_src->subtype) ||
          (do_arraylen && ELEM(prop_dst->type, IDP_ARRAY, IDP_IDPARRAY) &&
           (prop_src->len != prop_dst->len))) {
        BLI_insertlinkreplace(&dst->data.group, prop_dst, IDP_CopyProperty(prop_src));
        IDP_FreeProperty(prop_dst);
      }
      else if (prop_dst->type == IDP_GROUP) {
        IDP_SyncGroupTypes(prop_dst, prop_src, do_arraylen);
      }
    }
    else {
      IDP_FreeFromGroup(dst, prop_dst);
    }
  }
}

/**
 * Replaces all properties with the same name in a destination group from a source group.
 */
void IDP_ReplaceGroupInGroup(IDProperty *dest, const IDProperty *src)
{
  IDProperty *loop, *prop;

  BLI_assert(dest->type == IDP_GROUP);
  BLI_assert(src->type == IDP_GROUP);

  for (prop = src->data.group.first; prop; prop = prop->next) {
    for (loop = dest->data.group.first; loop; loop = loop->next) {
      if (STREQ(loop->name, prop->name)) {
        BLI_insertlinkreplace(&dest->data.group, loop, IDP_CopyProperty(prop));
        IDP_FreeProperty(loop);
        break;
      }
    }

    /* only add at end if not added yet */
    if (loop == NULL) {
      IDProperty *copy = IDP_CopyProperty(prop);
      dest->len++;
      BLI_addtail(&dest->data.group, copy);
    }
  }
}

/**
 * Checks if a property with the same name as prop exists, and if so replaces it.
 * Use this to preserve order!
 */
void IDP_ReplaceInGroup_ex(IDProperty *group, IDProperty *prop, IDProperty *prop_exist)
{
  BLI_assert(group->type == IDP_GROUP);
  BLI_assert(prop_exist == IDP_GetPropertyFromGroup(group, prop->name));

  if (prop_exist != NULL) {
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

/**
 * If a property is missing in \a dest, add it.
 * Do it recursively.
 */
void IDP_MergeGroup_ex(IDProperty *dest,
                       const IDProperty *src,
                       const bool do_overwrite,
                       const int flag)
{
  IDProperty *prop;

  BLI_assert(dest->type == IDP_GROUP);
  BLI_assert(src->type == IDP_GROUP);

  if (do_overwrite) {
    for (prop = src->data.group.first; prop; prop = prop->next) {
      if (prop->type == IDP_GROUP) {
        IDProperty *prop_exist = IDP_GetPropertyFromGroup(dest, prop->name);

        if (prop_exist != NULL) {
          IDP_MergeGroup_ex(prop_exist, prop, do_overwrite, flag);
          continue;
        }
      }

      IDProperty *copy = IDP_CopyProperty_ex(prop, flag);
      IDP_ReplaceInGroup(dest, copy);
    }
  }
  else {
    for (prop = src->data.group.first; prop; prop = prop->next) {
      IDProperty *prop_exist = IDP_GetPropertyFromGroup(dest, prop->name);
      if (prop_exist != NULL) {
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

/**
 * If a property is missing in \a dest, add it.
 * Do it recursively.
 */
void IDP_MergeGroup(IDProperty *dest, const IDProperty *src, const bool do_overwrite)
{
  IDP_MergeGroup_ex(dest, src, do_overwrite, 0);
}

/**
 * This function has a sanity check to make sure ID properties with the same name don't
 * get added to the group.
 *
 * The sanity check just means the property is not added to the group if another property
 * exists with the same name; the client code using ID properties then needs to detect this
 * (the function that adds new properties to groups, #IDP_AddToGroup,
 * returns false if a property can't be added to the group, and true if it can)
 * and free the property.
 */
bool IDP_AddToGroup(IDProperty *group, IDProperty *prop)
{
  BLI_assert(group->type == IDP_GROUP);

  if (IDP_GetPropertyFromGroup(group, prop->name) == NULL) {
    group->len++;
    BLI_addtail(&group->data.group, prop);
    return true;
  }

  return false;
}

/**
 * This is the same as IDP_AddToGroup, only you pass an item
 * in the group list to be inserted after.
 */
bool IDP_InsertToGroup(IDProperty *group, IDProperty *previous, IDProperty *pnew)
{
  BLI_assert(group->type == IDP_GROUP);

  if (IDP_GetPropertyFromGroup(group, pnew->name) == NULL) {
    group->len++;
    BLI_insertlinkafter(&group->data.group, previous, pnew);
    return true;
  }

  return false;
}

/**
 * \note this does not free the property!!
 *
 * To free the property, you have to do:
 * IDP_FreeProperty(prop);
 */
void IDP_RemoveFromGroup(IDProperty *group, IDProperty *prop)
{
  BLI_assert(group->type == IDP_GROUP);

  group->len--;
  BLI_remlink(&group->data.group, prop);
}

/**
 * Removes the property from the group and frees it.
 */
void IDP_FreeFromGroup(IDProperty *group, IDProperty *prop)
{
  IDP_RemoveFromGroup(group, prop);
  IDP_FreeProperty(prop);
}

IDProperty *IDP_GetPropertyFromGroup(IDProperty *prop, const char *name)
{
  BLI_assert(prop->type == IDP_GROUP);

  return (IDProperty *)BLI_findstring(&prop->data.group, name, offsetof(IDProperty, name));
}
/** same as above but ensure type match */
IDProperty *IDP_GetPropertyTypeFromGroup(IDProperty *prop, const char *name, const char type)
{
  IDProperty *idprop = IDP_GetPropertyFromGroup(prop, name);
  return (idprop && idprop->type == type) ? idprop : NULL;
}

/* Ok, the way things work, Groups free the ID Property structs of their children.
 * This is because all ID Property freeing functions free only direct data (not the ID Property
 * struct itself), but for Groups the child properties *are* considered
 * direct data. */
static void IDP_FreeGroup(IDProperty *prop, const bool do_id_user)
{
  IDProperty *loop;

  BLI_assert(prop->type == IDP_GROUP);
  for (loop = prop->data.group.first; loop; loop = loop->next) {
    IDP_FreeProperty_ex(loop, do_id_user);
  }
  BLI_freelistN(&prop->data.group);
}
/** \} */

/* -------------------------------------------------------------------- */
/* Main Functions */

/** \name IDProperty Main API
 * \{ */
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

/* Updates ID pointers after an object has been copied */
/* TODO Nuke this once its only user has been correctly converted
 * to use generic ID management from BKE_library! */
void IDP_RelinkProperty(struct IDProperty *prop)
{
  if (!prop) {
    return;
  }

  switch (prop->type) {
    case IDP_GROUP: {
      for (IDProperty *loop = prop->data.group.first; loop; loop = loop->next) {
        IDP_RelinkProperty(loop);
      }
      break;
    }
    case IDP_IDPARRAY: {
      IDProperty *idp_array = IDP_Array(prop);
      for (int i = 0; i < prop->len; i++) {
        IDP_RelinkProperty(&idp_array[i]);
      }
      break;
    }
    case IDP_ID: {
      ID *id = IDP_Id(prop);
      if (id && id->newid) {
        id_us_min(IDP_Id(prop));
        prop->data.pointer = id->newid;
        id_us_plus(IDP_Id(prop));
      }
      break;
    }
    default:
      break; /* Nothing to do for other IDProp types. */
  }
}

/**
 * Get the Group property that contains the id properties for ID id.  Set create_if_needed
 * to create the Group property and attach it to id if it doesn't exist; otherwise
 * the function will return NULL if there's no Group property attached to the ID.
 */
IDProperty *IDP_GetProperties(ID *id, const bool create_if_needed)
{
  if (id->properties) {
    return id->properties;
  }
  else {
    if (create_if_needed) {
      id->properties = MEM_callocN(sizeof(IDProperty), "IDProperty");
      id->properties->type = IDP_GROUP;
      /* don't overwrite the data's name and type
       * some functions might need this if they
       * don't have a real ID, should be named elsewhere - Campbell */
      /* strcpy(id->name, "top_level_group");*/
    }
    return id->properties;
  }
}

/**
 * \param is_strict: When false treat missing items as a match */
bool IDP_EqualsProperties_ex(IDProperty *prop1, IDProperty *prop2, const bool is_strict)
{
  if (prop1 == NULL && prop2 == NULL) {
    return true;
  }
  else if (prop1 == NULL || prop2 == NULL) {
    return is_strict ? false : true;
  }
  else if (prop1->type != prop2->type) {
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
    case IDP_STRING: {
      return (((prop1->len == prop2->len) &&
               STREQLEN(IDP_String(prop1), IDP_String(prop2), (size_t)prop1->len)));
    }
    case IDP_ARRAY:
      if (prop1->len == prop2->len && prop1->subtype == prop2->subtype) {
        return (memcmp(IDP_Array(prop1),
                       IDP_Array(prop2),
                       idp_size_table[(int)prop1->subtype] * (size_t)prop1->len) == 0);
      }
      return false;
    case IDP_GROUP: {
      IDProperty *link1, *link2;

      if (is_strict && prop1->len != prop2->len) {
        return false;
      }

      for (link1 = prop1->data.group.first; link1; link1 = link1->next) {
        link2 = IDP_GetPropertyFromGroup(prop2, link1->name);

        if (!IDP_EqualsProperties_ex(link1, link2, is_strict)) {
          return false;
        }
      }

      return true;
    }
    case IDP_IDPARRAY: {
      IDProperty *array1 = IDP_IDPArray(prop1);
      IDProperty *array2 = IDP_IDPArray(prop2);
      int i;

      if (prop1->len != prop2->len) {
        return false;
      }

      for (i = 0; i < prop1->len; i++) {
        if (!IDP_EqualsProperties_ex(&array1[i], &array2[i], is_strict)) {
          return false;
        }
      }
      return true;
    }
    case IDP_ID:
      return (IDP_Id(prop1) == IDP_Id(prop2));
    default:
      BLI_assert(0);
      break;
  }

  return true;
}

bool IDP_EqualsProperties(IDProperty *prop1, IDProperty *prop2)
{
  return IDP_EqualsProperties_ex(prop1, prop2, true);
}

/**
 * Allocate a new ID.
 *
 * This function takes three arguments: the ID property type, a union which defines
 * it's initial value, and a name.
 *
 * The union is simple to use; see the top of this header file for its definition.
 * An example of using this function:
 *
 * \code{.c}
 * IDPropertyTemplate val;
 * IDProperty *group, *idgroup, *color;
 * group = IDP_New(IDP_GROUP, val, "group1"); //groups don't need a template.
 *
 * val.array.len = 4
 * val.array.type = IDP_FLOAT;
 * color = IDP_New(IDP_ARRAY, val, "color1");
 *
 * idgroup = IDP_GetProperties(some_id, 1);
 * IDP_AddToGroup(idgroup, color);
 * IDP_AddToGroup(idgroup, group);
 * \endcode
 *
 * Note that you MUST either attach the id property to an id property group with
 * IDP_AddToGroup or MEM_freeN the property, doing anything else might result in
 * a memory leak.
 */
IDProperty *IDP_New(const char type, const IDPropertyTemplate *val, const char *name)
{
  IDProperty *prop = NULL;

  switch (type) {
    case IDP_INT:
      prop = MEM_callocN(sizeof(IDProperty), "IDProperty int");
      prop->data.val = val->i;
      break;
    case IDP_FLOAT:
      prop = MEM_callocN(sizeof(IDProperty), "IDProperty float");
      *(float *)&prop->data.val = val->f;
      break;
    case IDP_DOUBLE:
      prop = MEM_callocN(sizeof(IDProperty), "IDProperty double");
      *(double *)&prop->data.val = val->d;
      break;
    case IDP_ARRAY: {
      /* for now, we only support float and int and double arrays */
      if ((val->array.type == IDP_FLOAT) || (val->array.type == IDP_INT) ||
          (val->array.type == IDP_DOUBLE) || (val->array.type == IDP_GROUP)) {
        prop = MEM_callocN(sizeof(IDProperty), "IDProperty array");
        prop->subtype = val->array.type;
        if (val->array.len) {
          prop->data.pointer = MEM_callocN(
              idp_size_table[val->array.type] * (size_t)val->array.len, "id property array");
        }
        prop->len = prop->totallen = val->array.len;
        break;
      }
      CLOG_ERROR(&LOG, "bad array type.");
      return NULL;
    }
    case IDP_STRING: {
      const char *st = val->string.str;

      prop = MEM_callocN(sizeof(IDProperty), "IDProperty string");
      if (val->string.subtype == IDP_STRING_SUB_BYTE) {
        /* note, intentionally not null terminated */
        if (st == NULL) {
          prop->data.pointer = MEM_mallocN(DEFAULT_ALLOC_FOR_NULL_STRINGS, "id property string 1");
          *IDP_String(prop) = '\0';
          prop->totallen = DEFAULT_ALLOC_FOR_NULL_STRINGS;
          prop->len = 0;
        }
        else {
          prop->data.pointer = MEM_mallocN((size_t)val->string.len, "id property string 2");
          prop->len = prop->totallen = val->string.len;
          memcpy(prop->data.pointer, st, (size_t)val->string.len);
        }
        prop->subtype = IDP_STRING_SUB_BYTE;
      }
      else {
        if (st == NULL || val->string.len <= 1) {
          prop->data.pointer = MEM_mallocN(DEFAULT_ALLOC_FOR_NULL_STRINGS, "id property string 1");
          *IDP_String(prop) = '\0';
          prop->totallen = DEFAULT_ALLOC_FOR_NULL_STRINGS;
          /* NULL string, has len of 1 to account for null byte. */
          prop->len = 1;
        }
        else {
          BLI_assert((int)val->string.len <= (int)strlen(st) + 1);
          prop->data.pointer = MEM_mallocN((size_t)val->string.len, "id property string 3");
          memcpy(prop->data.pointer, st, (size_t)val->string.len - 1);
          IDP_String(prop)[val->string.len - 1] = '\0';
          prop->len = prop->totallen = val->string.len;
        }
        prop->subtype = IDP_STRING_SUB_UTF8;
      }
      break;
    }
    case IDP_GROUP: {
      /* Values are set properly by calloc. */
      prop = MEM_callocN(sizeof(IDProperty), "IDProperty group");
      break;
    }
    case IDP_ID: {
      prop = MEM_callocN(sizeof(IDProperty), "IDProperty datablock");
      prop->data.pointer = (void *)val->id;
      prop->type = IDP_ID;
      id_us_plus(IDP_Id(prop));
      break;
    }
    default: {
      prop = MEM_callocN(sizeof(IDProperty), "IDProperty array");
      break;
    }
  }

  prop->type = type;
  BLI_strncpy(prop->name, name, MAX_IDPROP_NAME);

  return prop;
}

/**
 * \note This will free allocated data, all child properties of arrays and groups, and unlink IDs!
 * But it does not free the actual IDProperty struct itself.
 */
void IDP_FreeProperty_ex(IDProperty *prop, const bool do_id_user)
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
}

void IDP_FreePropertyContent(IDProperty *prop)
{
  IDP_FreeProperty_ex(prop, true);
}

void IDP_FreeProperty(IDProperty *prop)
{
  IDP_FreePropertyContent(prop);
  MEM_freeN(prop);
}

void IDP_ClearProperty(IDProperty *prop)
{
  IDP_FreePropertyContent(prop);
  prop->data.pointer = NULL;
  prop->len = prop->totallen = 0;
}

void IDP_Reset(IDProperty *prop, const IDProperty *reference)
{
  if (prop == NULL) {
    return;
  }
  IDP_ClearProperty(prop);
  if (reference != NULL) {
    IDP_MergeGroup(prop, reference, true);
  }
}

/** \} */
