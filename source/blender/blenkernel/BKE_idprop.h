/**
 * $Id: BKE_idprop.h
 *
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
 
#ifndef _BKE_IDPROP_H
#define _BKE_IDPROP_H

#include "DNA_ID.h"

struct IDProperty;
struct ID;

typedef union {
	int i;
	float f;
	double d;
	char *str;
	struct ID *id;
	struct {
		short type;
		short len;
	} array;
	struct {
		int matvec_size;
		float *example;
	} matrix_or_vector;
} IDPropertyTemplate;

/* ----------- Property Array Type ---------- */

/*note: as a start to move away from the stupid IDP_New function, this type
  has it's own allocation function.*/
IDProperty *IDP_NewIDPArray(const char *name);
IDProperty *IDP_CopyIDPArray(IDProperty *array);

void IDP_FreeIDPArray(IDProperty *prop);

/* shallow copies item */
void IDP_SetIndexArray(struct IDProperty *prop, int index, struct IDProperty *item);
struct IDProperty *IDP_GetIndexArray(struct IDProperty *prop, int index);
struct IDProperty *IDP_AppendArray(struct IDProperty *prop, struct IDProperty *item);
void IDP_ResizeIDPArray(struct IDProperty *prop, int len);

/* ----------- Numeric Array Type ----------- */
/*this function works for strings too!*/
void IDP_ResizeArray(struct IDProperty *prop, int newlen);
void IDP_FreeArray(struct IDProperty *prop);
void IDP_UnlinkArray(struct IDProperty *prop);

/* ---------- String Type ------------ */
void IDP_AssignString(struct IDProperty *prop, char *st);
void IDP_ConcatStringC(struct IDProperty *prop, char *st);
void IDP_ConcatString(struct IDProperty *str1, struct IDProperty *append);
void IDP_FreeString(struct IDProperty *prop);

/*-------- ID Type -------*/
void IDP_LinkID(struct IDProperty *prop, ID *id);
void IDP_UnlinkID(struct IDProperty *prop);

/*-------- Group Functions -------*/

/* Sync values from one group to another, only where they match */
void IDP_SyncGroupValues(struct IDProperty *dest, struct IDProperty *src);

/*
 replaces all properties with the same name in a destination group from a source group.
*/
void IDP_ReplaceGroupInGroup(struct IDProperty *dest, struct IDProperty *src);

/*checks if a property with the same name as prop exists, and if so replaces it.
  Use this to preserve order!*/
void IDP_ReplaceInGroup(struct IDProperty *group, struct IDProperty *prop);

/*
This function has a sanity check to make sure ID properties with the same name don't
get added to the group.

The sanity check just means the property is not added to the group if another property
exists with the same name; the client code using ID properties then needs to detect this 
(the function that adds new properties to groups, IDP_AddToGroup, returns 0 if a property can't
be added to the group, and 1 if it can) and free the property.

Currently the code to free ID properties is designesd to leave the actual struct
you pass it un-freed, this is needed for how the system works.  This means
to free an ID property, you first call IDP_FreeProperty then MEM_freeN the
struct.  In the future this will just be IDP_FreeProperty and the code will
be reorganized to work properly.
*/
int IDP_AddToGroup(struct IDProperty *group, struct IDProperty *prop);

/*this is the same as IDP_AddToGroup, only you pass an item
  in the group list to be inserted after.*/
int IDP_InsertToGroup(struct IDProperty *group, struct IDProperty *previous, 
                      struct IDProperty *pnew);

/*NOTE: this does not free the property!!

 To free the property, you have to do:
 IDP_FreeProperty(prop); //free all subdata
 MEM_freeN(prop); //free property struct itself

*/
void IDP_RemFromGroup(struct IDProperty *group, struct IDProperty *prop);

IDProperty *IDP_GetPropertyFromGroup(struct IDProperty *prop, const char *name);

/*Get an iterator to iterate over the members of an id property group.
 Note that this will automatically free the iterator once iteration is complete;
 if you stop the iteration before hitting the end, make sure to call
 IDP_FreeIterBeforeEnd().*/
void *IDP_GetGroupIterator(struct IDProperty *prop);

/*Returns the next item in the iteration.  To use, simple for a loop like the following:
 while (IDP_GroupIterNext(iter) != NULL) {
	. . .
 }*/
IDProperty *IDP_GroupIterNext(void *vself);

/*Frees the iterator pointed to at vself, only use this if iteration is stopped early; 
  when the iterator hits the end of the list it'll automatially free itself.*/
void IDP_FreeIterBeforeEnd(void *vself);

/*-------- Main Functions --------*/
/*Get the Group property that contains the id properties for ID id.  Set create_if_needed
  to create the Group property and attach it to id if it doesn't exist; otherwise
  the function will return NULL if there's no Group property attached to the ID.*/
struct IDProperty *IDP_GetProperties(struct ID *id, int create_if_needed);
struct IDProperty *IDP_CopyProperty(struct IDProperty *prop);

int IDP_EqualsProperties(struct IDProperty *prop1, struct IDProperty *prop2);

/*
Allocate a new ID.

This function takes three arguments: the ID property type, a union which defines
it's initial value, and a name.

The union is simple to use; see the top of this header file for its definition. 
An example of using this function:

 IDPropertyTemplate val;
 IDProperty *group, *idgroup, *color;
 group = IDP_New(IDP_GROUP, val, "group1"); //groups don't need a template.

 val.array.len = 4
 val.array.type = IDP_FLOAT;
 color = IDP_New(IDP_ARRAY, val, "color1");

 idgroup = IDP_GetProperties(some_id, 1);
 IDP_AddToGroup(idgroup, color);
 IDP_AddToGroup(idgroup, group);

Note that you MUST either attach the id property to an id property group with 
IDP_AddToGroup or MEM_freeN the property, doing anything else might result in
a memory leak.
*/
struct IDProperty *IDP_New(int type, IDPropertyTemplate val, const char *name);
\
/*NOTE: this will free all child properties of list arrays and groups!
  Also, note that this does NOT unlink anything!  Plus it doesn't free
  the actual struct IDProperty struct either.*/
void IDP_FreeProperty(struct IDProperty *prop);

/*Unlinks any struct IDProperty<->ID linkage that might be going on.*/
void IDP_UnlinkProperty(struct IDProperty *prop);

#define IDP_Int(prop) ((prop)->data.val)
#define IDP_Float(prop) (*(float*)&(prop)->data.val)
#define IDP_String(prop) ((char*)(prop)->data.pointer)
#define IDP_Array(prop) ((prop)->data.pointer)
#define IDP_IDPArray(prop) ((IDProperty*)(prop)->data.pointer)
#define IDP_Double(prop) (*(double*)&(prop)->data.val)

#endif /* _BKE_IDPROP_H */
