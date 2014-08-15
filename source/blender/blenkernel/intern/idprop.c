/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * Contributor(s): Joseph Eagar
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/idprop.c
 *  \ingroup bke
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

#include "MEM_guardedalloc.h"

/* IDPropertyTemplate is a union in DNA_ID.h */

/*local size table.*/
static char idp_size_table[] = {
	1, /*strings*/
	sizeof(int),
	sizeof(float),
	sizeof(float) * 3, /*Vector type, deprecated*/
	sizeof(float) * 16, /*Matrix type, deprecated*/
	0, /*arrays don't have a fixed size*/
	sizeof(ListBase), /*Group type*/
	sizeof(void *),
	sizeof(double)
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

IDProperty *IDP_CopyIDPArray(const IDProperty *array)
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
		tmp = IDP_CopyProperty(GETPROP(narray, i));
		memcpy(GETPROP(narray, i), tmp, sizeof(IDProperty));
		MEM_freeN(tmp);
	}
	
	return narray;
}

void IDP_FreeIDPArray(IDProperty *prop)
{
	int i;
	
	BLI_assert(prop->type == IDP_IDPARRAY);

	for (i = 0; i < prop->len; i++)
		IDP_FreeProperty(GETPROP(prop, i));

	if (prop->data.pointer)
		MEM_freeN(prop->data.pointer);
}

/*shallow copies item*/
void IDP_SetIndexArray(IDProperty *prop, int index, IDProperty *item)
{
	IDProperty *old;

	BLI_assert(prop->type == IDP_IDPARRAY);

	old = GETPROP(prop, index);
	if (index >= prop->len || index < 0) return;
	if (item != old) IDP_FreeProperty(old);
	
	memcpy(GETPROP(prop, index), item, sizeof(IDProperty));
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
	/* if newlen is 200 items less then totallen, reallocate anyway */
	if (newlen <= prop->totallen) {
		if (newlen < prop->len && prop->totallen - newlen < 200) {
			int i;

			for (i = newlen; i < prop->len; i++)
				IDP_FreeProperty(GETPROP(prop, i));

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
			IDP_FreeProperty(GETPROP(prop, i));
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
	prop->data.pointer = MEM_recallocN(prop->data.pointer, sizeof(IDProperty) * newsize);
	prop->len = newlen;
	prop->totallen = newsize;
}

/* ----------- Numerical Array Type ----------- */
static void idp_resize_group_array(IDProperty *prop, int newlen, void *newarr)
{
	if (prop->subtype != IDP_GROUP)
		return;

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
			MEM_freeN(array[a]);
		}
	}
}

/*this function works for strings too!*/
void IDP_ResizeArray(IDProperty *prop, int newlen)
{
	int newsize;
	const bool is_grow = newlen >= prop->len;

	/* first check if the array buffer size has room */
	/* if newlen is 200 chars less then totallen, reallocate anyway */
	if (newlen <= prop->totallen && prop->totallen - newlen < 200) {
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

	if (is_grow == false)
		idp_resize_group_array(prop, newlen, prop->data.pointer);

	prop->data.pointer = MEM_recallocN(prop->data.pointer, idp_size_table[(int)prop->subtype] * newsize);

	if (is_grow == true)
		idp_resize_group_array(prop, newlen, prop->data.pointer);

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


static IDProperty *idp_generic_copy(const IDProperty *prop)
{
	IDProperty *newp = MEM_callocN(sizeof(IDProperty), "IDProperty array dup");

	BLI_strncpy(newp->name, prop->name, MAX_IDPROP_NAME);
	newp->type = prop->type;
	newp->flag = prop->flag;
	newp->data.val = prop->data.val;
	newp->data.val2 = prop->data.val2;

	return newp;
}

static IDProperty *IDP_CopyArray(const IDProperty *prop)
{
	IDProperty *newp = idp_generic_copy(prop);

	if (prop->data.pointer) {
		newp->data.pointer = MEM_dupallocN(prop->data.pointer);

		if (prop->type == IDP_GROUP) {
			IDProperty **array = newp->data.pointer;
			int a;

			for (a = 0; a < prop->len; a++)
				array[a] = IDP_CopyProperty(array[a]);
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
 * \param st  The string to assign.
 * \param name  The property name.
 * \param maxlen  The size of the new string (including the \0 terminator)
 * \return
 */
IDProperty *IDP_NewString(const char *st, const char *name, int maxlen)
{
	IDProperty *prop = MEM_callocN(sizeof(IDProperty), "IDProperty string");

	if (st == NULL) {
		prop->data.pointer = MEM_mallocN(DEFAULT_ALLOC_FOR_NULL_STRINGS, "id property string 1");
		*IDP_String(prop) = '\0';
		prop->totallen = DEFAULT_ALLOC_FOR_NULL_STRINGS;
		prop->len = 1;  /* NULL string, has len of 1 to account for null byte. */
	}
	else {
		/* include null terminator '\0' */
		int stlen = strlen(st) + 1;

		if (maxlen > 0 && maxlen < stlen)
			stlen = maxlen;

		prop->data.pointer = MEM_mallocN(stlen, "id property string 2");
		prop->len = prop->totallen = stlen;
		BLI_strncpy(prop->data.pointer, st, stlen);
	}

	prop->type = IDP_STRING;
	BLI_strncpy(prop->name, name, MAX_IDPROP_NAME);

	return prop;
}

static IDProperty *IDP_CopyString(const IDProperty *prop)
{
	IDProperty *newp;

	BLI_assert(prop->type == IDP_STRING);
	newp = idp_generic_copy(prop);

	if (prop->data.pointer)
		newp->data.pointer = MEM_dupallocN(prop->data.pointer);
	newp->len = prop->len;
	newp->subtype = prop->subtype;
	newp->totallen = prop->totallen;

	return newp;
}


void IDP_AssignString(IDProperty *prop, const char *st, int maxlen)
{
	int stlen;

	BLI_assert(prop->type == IDP_STRING);
	stlen = strlen(st);
	if (maxlen > 0 && maxlen < stlen)
		stlen = maxlen;

	if (prop->subtype == IDP_STRING_SUB_BYTE) {
		IDP_ResizeArray(prop, stlen);
		memcpy(prop->data.pointer, st, stlen);
	}
	else {
		stlen++;
		IDP_ResizeArray(prop, stlen);
		BLI_strncpy(prop->data.pointer, st, stlen);
	}
}

void IDP_ConcatStringC(IDProperty *prop, const char *st)
{
	int newlen;

	BLI_assert(prop->type == IDP_STRING);

	newlen = prop->len + strlen(st);
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

	if (prop->data.pointer)
		MEM_freeN(prop->data.pointer);
}
/** \} */


/* -------------------------------------------------------------------- */
/* ID Type (not in use yet) */

/** \name IDProperty ID API (unused)
 * \{ */
void IDP_LinkID(IDProperty *prop, ID *id)
{
	if (prop->data.pointer) ((ID *)prop->data.pointer)->us--;
	prop->data.pointer = id;
	id_us_plus(id);
}

void IDP_UnlinkID(IDProperty *prop)
{
	((ID *)prop->data.pointer)->us--;
}
/** \} */


/* -------------------------------------------------------------------- */
/* Group Functions */

/** \name IDProperty Group API
 * \{ */

/**
 * Checks if a property with the same name as prop exists, and if so replaces it.
 */
static IDProperty *IDP_CopyGroup(const IDProperty *prop)
{
	IDProperty *newp, *link;
	
	BLI_assert(prop->type == IDP_GROUP);
	newp = idp_generic_copy(prop);
	newp->len = prop->len;

	for (link = prop->data.group.first; link; link = link->next) {
		BLI_addtail(&newp->data.group, IDP_CopyProperty(link));
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
				default:
				{
					IDProperty *tmp = other;
					IDProperty *copy = IDP_CopyProperty(prop);

					BLI_insertlinkafter(&dest->data.group, other, copy);
					BLI_remlink(&dest->data.group, tmp);

					IDP_FreeProperty(tmp);
					MEM_freeN(tmp);
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
			    (do_arraylen && ELEM(prop_dst->type, IDP_ARRAY, IDP_IDPARRAY) && (prop_src->len != prop_dst->len)))
			{
				IDP_FreeFromGroup(dst, prop_dst);
				prop_dst = IDP_CopyProperty(prop_src);

				dst->len++;
				BLI_insertlinkbefore(&dst->data.group, prop_dst_next, prop_dst);
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
				IDProperty *copy = IDP_CopyProperty(prop);

				BLI_insertlinkafter(&dest->data.group, loop, copy);

				BLI_remlink(&dest->data.group, loop);
				IDP_FreeProperty(loop);
				MEM_freeN(loop);
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
void IDP_ReplaceInGroup(IDProperty *group, IDProperty *prop)
{
	IDProperty *loop;

	BLI_assert(group->type == IDP_GROUP);

	if ((loop = IDP_GetPropertyFromGroup(group, prop->name))) {
		BLI_insertlinkafter(&group->data.group, loop, prop);
		
		BLI_remlink(&group->data.group, loop);
		IDP_FreeProperty(loop);
		MEM_freeN(loop);
	}
	else {
		group->len++;
		BLI_addtail(&group->data.group, prop);
	}
}

/**
 * If a property is missing in \a dest, add it.
 */
void IDP_MergeGroup(IDProperty *dest, const IDProperty *src, const bool do_overwrite)
{
	IDProperty *prop;

	BLI_assert(dest->type == IDP_GROUP);
	BLI_assert(src->type == IDP_GROUP);

	if (do_overwrite) {
		for (prop = src->data.group.first; prop; prop = prop->next) {
			IDProperty *copy = IDP_CopyProperty(prop);
			IDP_ReplaceInGroup(dest, copy);
		}
	}
	else {
		for (prop = src->data.group.first; prop; prop = prop->next) {
			if (IDP_GetPropertyFromGroup(dest, prop->name) == NULL) {
				IDProperty *copy = IDP_CopyProperty(prop);
				dest->len++;
				BLI_addtail(&dest->data.group, copy);
			}
		}
	}
}

/**
 * This function has a sanity check to make sure ID properties with the same name don't
 * get added to the group.
 *
 * The sanity check just means the property is not added to the group if another property
 * exists with the same name; the client code using ID properties then needs to detect this
 * (the function that adds new properties to groups, IDP_AddToGroup,returns 0 if a property can't
 * be added to the group, and 1 if it can) and free the property.
 *
 * Currently the code to free ID properties is designed to leave the actual struct
 * you pass it un-freed, this is needed for how the system works.  This means
 * to free an ID property, you first call IDP_FreeProperty then MEM_freeN the
 * struct.  In the future this will just be IDP_FreeProperty and the code will
 * be reorganized to work properly.
 */
bool IDP_AddToGroup(IDProperty *group, IDProperty *prop)
{
	BLI_assert(group->type == IDP_GROUP);

	if (IDP_GetPropertyFromGroup(group, prop->name) == NULL) {
		group->len++;
		BLI_addtail(&group->data.group, prop);
		return 1;
	}

	return 0;
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
		return 1;
	}

	return 0;
}

/**
 * \note this does not free the property!!
 *
 * To free the property, you have to do:
 * IDP_FreeProperty(prop); //free all subdata
 * MEM_freeN(prop); //free property struct itself
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
	MEM_freeN(prop);
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

typedef struct IDPIter {
	void *next;
	IDProperty *parent;
} IDPIter;

/**
 * Get an iterator to iterate over the members of an id property group.
 * Note that this will automatically free the iterator once iteration is complete;
 * if you stop the iteration before hitting the end, make sure to call
 * IDP_FreeIterBeforeEnd().
 */
void *IDP_GetGroupIterator(IDProperty *prop)
{
	IDPIter *iter;

	BLI_assert(prop->type == IDP_GROUP);
	iter = MEM_mallocN(sizeof(IDPIter), "IDPIter");
	iter->next = prop->data.group.first;
	iter->parent = prop;
	return (void *) iter;
}

/**
 * Returns the next item in the iteration.  To use, simple for a loop like the following:
 * while (IDP_GroupIterNext(iter) != NULL) {
 *     ...
 * }
 */
IDProperty *IDP_GroupIterNext(void *vself)
{
	IDPIter *self = (IDPIter *) vself;
	Link *next = (Link *) self->next;
	if (self->next == NULL) {
		MEM_freeN(self);
		return NULL;
	}

	self->next = next->next;
	return (void *) next;
}

/**
 * Frees the iterator pointed to at vself, only use this if iteration is stopped early;
 * when the iterator hits the end of the list it'll automatically free itself.\
 */
void IDP_FreeIterBeforeEnd(void *vself)
{
	MEM_freeN(vself);
}

/* Ok, the way things work, Groups free the ID Property structs of their children.
 * This is because all ID Property freeing functions free only direct data (not the ID Property
 * struct itself), but for Groups the child properties *are* considered
 * direct data. */
static void IDP_FreeGroup(IDProperty *prop)
{
	IDProperty *loop;

	BLI_assert(prop->type == IDP_GROUP);
	for (loop = prop->data.group.first; loop; loop = loop->next) {
		IDP_FreeProperty(loop);
	}
	BLI_freelistN(&prop->data.group);
}
/** \} */


/* -------------------------------------------------------------------- */
/* Main Functions */

/** \name IDProperty Main API
 * \{ */
IDProperty *IDP_CopyProperty(const IDProperty *prop)
{
	switch (prop->type) {
		case IDP_GROUP: return IDP_CopyGroup(prop);
		case IDP_STRING: return IDP_CopyString(prop);
		case IDP_ARRAY: return IDP_CopyArray(prop);
		case IDP_IDPARRAY: return IDP_CopyIDPArray(prop);
		default: return idp_generic_copy(prop);
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
 * \param is_strict When false treat missing items as a match */
bool IDP_EqualsProperties_ex(IDProperty *prop1, IDProperty *prop2, const bool is_strict)
{
	if (prop1 == NULL && prop2 == NULL)
		return 1;
	else if (prop1 == NULL || prop2 == NULL)
		return is_strict ? 0 : 1;
	else if (prop1->type != prop2->type)
		return 0;

	switch (prop1->type) {
		case IDP_INT:
			return (IDP_Int(prop1) == IDP_Int(prop2));
		case IDP_FLOAT:
#if !defined(NDEBUG) && defined(WITH_PYTHON)
			{
				float p1 = IDP_Float(prop1);
				float p2 = IDP_Float(prop2);
				if ((p1 != p2) && ((fabsf(p1 - p2) / max_ff(p1, p2)) < 0.001f)) {
					printf("WARNING: Comparing two float properties that have nearly the same value (%f vs. %f)\n", p1, p2);
					printf("    p1: ");
					IDP_spit(prop1);
					printf("    p2: ");
					IDP_spit(prop2);
				}
			}
#endif
			return (IDP_Float(prop1) == IDP_Float(prop2));
		case IDP_DOUBLE:
			return (IDP_Double(prop1) == IDP_Double(prop2));
		case IDP_STRING:
			return ((prop1->len == prop2->len) && strncmp(IDP_String(prop1), IDP_String(prop2), prop1->len) == 0);
		case IDP_ARRAY:
			if (prop1->len == prop2->len && prop1->subtype == prop2->subtype) {
				return memcmp(IDP_Array(prop1), IDP_Array(prop2), idp_size_table[(int)prop1->subtype] * prop1->len);
			}
			return 0;
		case IDP_GROUP:
		{
			IDProperty *link1, *link2;

			if (is_strict && prop1->len != prop2->len)
				return 0;

			for (link1 = prop1->data.group.first; link1; link1 = link1->next) {
				link2 = IDP_GetPropertyFromGroup(prop2, link1->name);

				if (!IDP_EqualsProperties_ex(link1, link2, is_strict))
					return 0;
			}

			return 1;
		}
		case IDP_IDPARRAY:
		{
			IDProperty *array1 = IDP_IDPArray(prop1);
			IDProperty *array2 = IDP_IDPArray(prop2);
			int i;

			if (prop1->len != prop2->len)
				return 0;

			for (i = 0; i < prop1->len; i++)
				if (!IDP_EqualsProperties(&array1[i], &array2[i]))
					return 0;
			return 1;
		}
		default:
			/* should never get here */
			BLI_assert(0);
			break;
	}

	return 1;
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
 *     IDPropertyTemplate val;
 *     IDProperty *group, *idgroup, *color;
 *     group = IDP_New(IDP_GROUP, val, "group1"); //groups don't need a template.
 *
 *     val.array.len = 4
 *     val.array.type = IDP_FLOAT;
 *     color = IDP_New(IDP_ARRAY, val, "color1");
 *
 *     idgroup = IDP_GetProperties(some_id, 1);
 *     IDP_AddToGroup(idgroup, color);
 *     IDP_AddToGroup(idgroup, group);
 *
 * Note that you MUST either attach the id property to an id property group with
 * IDP_AddToGroup or MEM_freeN the property, doing anything else might result in
 * a memory leak.
 */
IDProperty *IDP_New(const int type, const IDPropertyTemplate *val, const char *name)
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
			prop = MEM_callocN(sizeof(IDProperty), "IDProperty float");
			*(double *)&prop->data.val = val->d;
			break;
		case IDP_ARRAY:
		{
			/* for now, we only support float and int and double arrays */
			if ( (val->array.type == IDP_FLOAT) ||
			     (val->array.type == IDP_INT) ||
			     (val->array.type == IDP_DOUBLE) ||
			     (val->array.type == IDP_GROUP) )
			{
				prop = MEM_callocN(sizeof(IDProperty), "IDProperty array");
				prop->subtype = val->array.type;
				if (val->array.len)
					prop->data.pointer = MEM_callocN(idp_size_table[val->array.type] * val->array.len, "id property array");
				prop->len = prop->totallen = val->array.len;
				break;
			}
			return NULL;
		}
		case IDP_STRING:
		{
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
					prop->data.pointer = MEM_mallocN(val->string.len, "id property string 2");
					prop->len = prop->totallen = val->string.len;
					memcpy(prop->data.pointer, st, val->string.len);
				}
				prop->subtype = IDP_STRING_SUB_BYTE;
			}
			else {
				if (st == NULL) {
					prop->data.pointer = MEM_mallocN(DEFAULT_ALLOC_FOR_NULL_STRINGS, "id property string 1");
					*IDP_String(prop) = '\0';
					prop->totallen = DEFAULT_ALLOC_FOR_NULL_STRINGS;
					prop->len = 1; /*NULL string, has len of 1 to account for null byte.*/
				}
				else {
					int stlen = strlen(st) + 1;
					prop->data.pointer = MEM_mallocN(stlen, "id property string 3");
					prop->len = prop->totallen = stlen;
					memcpy(prop->data.pointer, st, stlen);
				}
				prop->subtype = IDP_STRING_SUB_UTF8;
			}
			break;
		}
		case IDP_GROUP:
		{
			prop = MEM_callocN(sizeof(IDProperty), "IDProperty group");
			/* heh I think all needed values are set properly by calloc anyway :) */
			break;
		}
		default:
		{
			prop = MEM_callocN(sizeof(IDProperty), "IDProperty array");
			break;
		}
	}

	prop->type = type;
	BLI_strncpy(prop->name, name, MAX_IDPROP_NAME);
	
	return prop;
}

/**
 * \note this will free all child properties of list arrays and groups!
 * Also, note that this does NOT unlink anything!  Plus it doesn't free
 * the actual struct IDProperty struct either.
 */
void IDP_FreeProperty(IDProperty *prop)
{
	switch (prop->type) {
		case IDP_ARRAY:
			IDP_FreeArray(prop);
			break;
		case IDP_STRING:
			IDP_FreeString(prop);
			break;
		case IDP_GROUP:
			IDP_FreeGroup(prop);
			break;
		case IDP_IDPARRAY:
			IDP_FreeIDPArray(prop);
			break;
	}
}

void IDP_ClearProperty(IDProperty *prop)
{
	IDP_FreeProperty(prop);
	prop->data.pointer = NULL;
	prop->len = prop->totallen = 0;
}

/**
 * Unlinks any struct IDProperty<->ID linkage that might be going on.
 *
 * \note currently unused
 */
void IDP_UnlinkProperty(IDProperty *prop)
{
	switch (prop->type) {
		case IDP_ID:
			IDP_UnlinkID(prop);
			break;
	}
}

/** \} */
