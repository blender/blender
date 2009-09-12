/**
 * $Id: idprop.c
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Joseph Eagar
 *
 * ***** END GPL LICENSE BLOCK *****
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "DNA_listBase.h"
#include "DNA_ID.h"

#include "BKE_idprop.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_utildefines.h"

#include "BLI_blenlib.h"

#include "MEM_guardedalloc.h"

#define BSTR_EQ(a, b)	(*(a) == *(b) && !strcmp(a, b))

/* IDPropertyTemplate is a union in DNA_ID.h */

/*local size table.*/
static char idp_size_table[] = {
	1, /*strings*/
	sizeof(int),
	sizeof(float),
	sizeof(float)*3, /*Vector type, deprecated*/
	sizeof(float)*16, /*Matrix type, deprecated*/
	0, /*arrays don't have a fixed size*/
	sizeof(ListBase), /*Group type*/
	sizeof(void*),
	sizeof(double)
};

/* ------------Property Array Type ----------- */
#define GETPROP(prop, i) (((IDProperty*)(prop)->data.pointer)+(i))

/* --------- property array type -------------*/

/*note: as a start to move away from the stupid IDP_New function, this type
  has it's own allocation function.*/
IDProperty *IDP_NewIDPArray(const char *name)
{
	IDProperty *prop = MEM_callocN(sizeof(IDProperty), "IDProperty prop array");
	prop->type = IDP_IDPARRAY;
	prop->len = 0;
	BLI_strncpy(prop->name, name, MAX_IDPROP_NAME);
	
	return prop;
}

IDProperty *IDP_CopyIDPArray(IDProperty *array)
{
	IDProperty *narray = MEM_dupallocN(array), *tmp;
	int i;
	
	narray->data.pointer = MEM_dupallocN(array->data.pointer);
	for (i=0; i<narray->len; i++) {
		/*ok, the copy functions always allocate a new structure,
		  which doesn't work here.  instead, simply copy the
		  contents of the new structure into the array cell,
		  then free it.  this makes for more maintainable
		  code than simply reimplementing the copy functions
		  in this loop.*/
		tmp = IDP_CopyProperty(GETPROP(narray, i));
		memcpy(GETPROP(narray, i), tmp, sizeof(IDProperty));
		MEM_freeN(tmp);
	}
	
	return narray;
}

void IDP_FreeIDPArray(IDProperty *prop)
{
	int i;
	
	for (i=0; i<prop->len; i++)
		IDP_FreeProperty(GETPROP(prop, i));

	if(prop->data.pointer)
		MEM_freeN(prop->data.pointer);
}

/*shallow copies item*/
void IDP_SetIndexArray(IDProperty *prop, int index, IDProperty *item)
{
	IDProperty *old = GETPROP(prop, index);
	if (index >= prop->len || index < 0) return;
	if (item != old) IDP_FreeProperty(old);
	
	memcpy(GETPROP(prop, index), item, sizeof(IDProperty));
}

IDProperty *IDP_GetIndexArray(IDProperty *prop, int index)
{
	return GETPROP(prop, index);
}

IDProperty *IDP_AppendArray(IDProperty *prop, IDProperty *item)
{
	IDP_ResizeIDPArray(prop, prop->len+1);
	IDP_SetIndexArray(prop, prop->len-1, item);
	return item;
}

void IDP_ResizeIDPArray(IDProperty *prop, int newlen)
{
	void *newarr;
	int newsize=newlen;

	/*first check if the array buffer size has room*/
	/*if newlen is 200 chars less then totallen, reallocate anyway*/
	if (newlen <= prop->totallen && prop->totallen - newlen < 200) {
		int i;

		for(i=newlen; i<prop->len; i++)
			IDP_FreeProperty(GETPROP(prop, i));

		prop->len = newlen;
		return;
	}

	/* - Note: This code comes from python, here's the corrusponding comment. - */
	/* This over-allocates proportional to the list size, making room
	 * for additional growth.  The over-allocation is mild, but is
	 * enough to give linear-time amortized behavior over a long
	 * sequence of appends() in the presence of a poorly-performing
	 * system realloc().
	 * The growth pattern is:  0, 4, 8, 16, 25, 35, 46, 58, 72, 88, ...
	 */
	newsize = (newsize >> 3) + (newsize < 9 ? 3 : 6) + newsize;

	newarr = MEM_callocN(sizeof(IDProperty)*newsize, "idproperty array resized");
	if (newlen >= prop->len) {
		/* newlen is bigger*/
		memcpy(newarr, prop->data.pointer, prop->len*sizeof(IDProperty));
	}
	else {
		int i;
		/* newlen is smaller*/
		for (i=newlen; i<prop->len; i++) {
			IDP_FreeProperty(GETPROP(prop, i));
		}
		memcpy(newarr, prop->data.pointer, newlen*sizeof(IDProperty));
	}

	if(prop->data.pointer)
		MEM_freeN(prop->data.pointer);
	prop->data.pointer = newarr;
	prop->len = newlen;
	prop->totallen = newsize;
}

/* ----------- Numerical Array Type ----------- */
static void idp_resize_group_array(IDProperty *prop, int newlen, void *newarr)
{
	if(prop->subtype != IDP_GROUP)
		return;

	if(newlen >= prop->len) {
		/* bigger */
		IDProperty **array= newarr;
		IDPropertyTemplate val;
		int a;

		for(a=prop->len; a<newlen; a++) {
			val.i = 0; /* silence MSVC warning about uninitialized var when debugging */
			array[a]= IDP_New(IDP_GROUP, val, "IDP_ResizeArray group");
		}
	}
	else {
		/* smaller */
		IDProperty **array= prop->data.pointer;
		int a;

		for(a=newlen; a<prop->len; a++) {
			IDP_FreeProperty(array[a]);
			MEM_freeN(array[a]);
		}
	}
}

/*this function works for strings too!*/
void IDP_ResizeArray(IDProperty *prop, int newlen)
{
	void *newarr;
	int newsize=newlen;

	/*first check if the array buffer size has room*/
	/*if newlen is 200 chars less then totallen, reallocate anyway*/
	if (newlen <= prop->totallen && prop->totallen - newlen < 200) {
		idp_resize_group_array(prop, newlen, prop->data.pointer);
		prop->len = newlen;
		return;
	}

	/* - Note: This code comes from python, here's the corrusponding comment. - */
	/* This over-allocates proportional to the list size, making room
	 * for additional growth.  The over-allocation is mild, but is
	 * enough to give linear-time amortized behavior over a long
	 * sequence of appends() in the presence of a poorly-performing
	 * system realloc().
	 * The growth pattern is:  0, 4, 8, 16, 25, 35, 46, 58, 72, 88, ...
	 */
	newsize = (newsize >> 3) + (newsize < 9 ? 3 : 6) + newsize;

	newarr = MEM_callocN(idp_size_table[(int)prop->subtype]*newsize, "idproperty array resized");
	if (newlen >= prop->len) {
		/* newlen is bigger*/
		memcpy(newarr, prop->data.pointer, prop->len*idp_size_table[(int)prop->subtype]);
		idp_resize_group_array(prop, newlen, newarr);
	}
	else {
		/* newlen is smaller*/
		idp_resize_group_array(prop, newlen, newarr);
		memcpy(newarr, prop->data.pointer, newlen*prop->len*idp_size_table[(int)prop->subtype]);
	}

	MEM_freeN(prop->data.pointer);
	prop->data.pointer = newarr;
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


 static IDProperty *idp_generic_copy(IDProperty *prop)
 {
	IDProperty *newp = MEM_callocN(sizeof(IDProperty), "IDProperty array dup");

	BLI_strncpy(newp->name, prop->name, MAX_IDPROP_NAME);
	newp->type = prop->type;
	newp->flag = prop->flag;
	newp->data.val = prop->data.val;
	newp->data.val2 = prop->data.val2;

	return newp;
 }

IDProperty *IDP_CopyArray(IDProperty *prop)
{
	IDProperty *newp = idp_generic_copy(prop);

	if (prop->data.pointer) {
		newp->data.pointer = MEM_dupallocN(prop->data.pointer);

		if(prop->type == IDP_GROUP) {
			IDProperty **array= newp->data.pointer;
			int a;

			for(a=0; a<prop->len; a++)
				array[a]= IDP_CopyProperty(array[a]);
		}
	}
	newp->len = prop->len;
	newp->subtype = prop->subtype;
	newp->totallen = prop->totallen;

	return newp;
}

/*taken from readfile.c*/
#define SWITCH_LONGINT(a) { \
    char s_i, *p_i; \
    p_i= (char *)&(a);  \
    s_i=p_i[0]; p_i[0]=p_i[7]; p_i[7]=s_i; \
    s_i=p_i[1]; p_i[1]=p_i[6]; p_i[6]=s_i; \
    s_i=p_i[2]; p_i[2]=p_i[5]; p_i[5]=s_i; \
    s_i=p_i[3]; p_i[3]=p_i[4]; p_i[4]=s_i; }



/* ---------- String Type ------------ */
IDProperty *IDP_CopyString(IDProperty *prop)
{
	IDProperty *newp = idp_generic_copy(prop);

	if (prop->data.pointer) newp->data.pointer = MEM_dupallocN(prop->data.pointer);
	newp->len = prop->len;
	newp->subtype = prop->subtype;
	newp->totallen = prop->totallen;

	return newp;
}


void IDP_AssignString(IDProperty *prop, char *st)
{
	int stlen;

	stlen = strlen(st);

	IDP_ResizeArray(prop, stlen+1); /*make room for null byte :) */
	strcpy(prop->data.pointer, st);
}

void IDP_ConcatStringC(IDProperty *prop, char *st)
{
	int newlen;

	newlen = prop->len + strlen(st);
	/*we have to remember that prop->len includes the null byte for strings.
	 so there's no need to add +1 to the resize function.*/
	IDP_ResizeArray(prop, newlen);
	strcat(prop->data.pointer, st);
}

void IDP_ConcatString(IDProperty *str1, IDProperty *append)
{
	int newlen;

	/*since ->len for strings includes the NULL byte, we have to subtract one or
	 we'll get an extra null byte after each concatination operation.*/
	newlen = str1->len + append->len - 1;
	IDP_ResizeArray(str1, newlen);
	strcat(str1->data.pointer, append->data.pointer);
}

void IDP_FreeString(IDProperty *prop)
{
	if(prop->data.pointer)
		MEM_freeN(prop->data.pointer);
}


/*-------- ID Type, not in use yet -------*/

void IDP_LinkID(IDProperty *prop, ID *id)
{
	if (prop->data.pointer) ((ID*)prop->data.pointer)->us--;
	prop->data.pointer = id;
	id_us_plus(id);
}

void IDP_UnlinkID(IDProperty *prop)
{
	((ID*)prop->data.pointer)->us--;
}

/*-------- Group Functions -------*/

/*checks if a property with the same name as prop exists, and if so replaces it.*/
IDProperty *IDP_CopyGroup(IDProperty *prop)
{
	IDProperty *newp = idp_generic_copy(prop), *link;
	newp->len = prop->len;
	
	for (link=prop->data.group.first; link; link=link->next) {
		BLI_addtail(&newp->data.group, IDP_CopyProperty(link));
	}

	return newp;
}

/*
 replaces a property with the same name in a group, or adds 
 it if the propery doesn't exist.
*/
void IDP_ReplaceInGroup(IDProperty *group, IDProperty *prop)
{
	IDProperty *loop;
	for (loop=group->data.group.first; loop; loop=loop->next) {
		if (BSTR_EQ(loop->name, prop->name)) {
			if (loop->next) BLI_insertlinkbefore(&group->data.group, loop->next, prop);
			else BLI_addtail(&group->data.group, prop);
			
			BLI_remlink(&group->data.group, loop);
			IDP_FreeProperty(loop);
			MEM_freeN(loop);			
			return;
		}
	}

	group->len++;
	BLI_addtail(&group->data.group, prop);
}

/*returns 0 if an id property with the same name exists and it failed,
  or 1 if it succeeded in adding to the group.*/
int IDP_AddToGroup(IDProperty *group, IDProperty *prop)
{
	IDProperty *loop;
	for (loop=group->data.group.first; loop; loop=loop->next) {
		if (BSTR_EQ(loop->name, prop->name)) return 0;
	}

	group->len++;
	BLI_addtail(&group->data.group, prop);

	return 1;
}

int IDP_InsertToGroup(IDProperty *group, IDProperty *previous, IDProperty *pnew)
{
	IDProperty *loop;
	for (loop=group->data.group.first; loop; loop=loop->next) {
		if (BSTR_EQ(loop->name, pnew->name)) return 0;
	}
	
	group->len++;

	BLI_insertlink(&group->data.group, previous, pnew);
	return 1;
}

void IDP_RemFromGroup(IDProperty *group, IDProperty *prop)
{
	group->len--;
	BLI_remlink(&group->data.group, prop);
}

IDProperty *IDP_GetPropertyFromGroup(IDProperty *prop, char *name)
{
	IDProperty *loop;
	for (loop=prop->data.group.first; loop; loop=loop->next) {
		if (strcmp(loop->name, name)==0) return loop;
	}
	return NULL;
}

typedef struct IDPIter {
	void *next;
	IDProperty *parent;
} IDPIter;

void *IDP_GetGroupIterator(IDProperty *prop)
{
	IDPIter *iter = MEM_callocN(sizeof(IDPIter), "IDPIter");
	iter->next = prop->data.group.first;
	iter->parent = prop;
	return (void*) iter;
}

IDProperty *IDP_GroupIterNext(void *vself)
{
	IDPIter *self = (IDPIter*) vself;
	Link *next = (Link*) self->next;
	if (self->next == NULL) {
		MEM_freeN(self);
		return NULL;
	}

	self->next = next->next;
	return (void*) next;
}

void IDP_FreeIterBeforeEnd(void *vself)
{
	MEM_freeN(vself);
}

/*Ok, the way things work, Groups free the ID Property structs of their children.
  This is because all ID Property freeing functions free only direct data (not the ID Property
  struct itself), but for Groups the child properties *are* considered
  direct data.*/
static void IDP_FreeGroup(IDProperty *prop)
{
	IDProperty *loop, *next;
	for (loop=prop->data.group.first; loop; loop=next)
	{
		next = loop->next;
		BLI_remlink(&prop->data.group, loop);
		IDP_FreeProperty(loop);
		MEM_freeN(loop);
	}
}


/*-------- Main Functions --------*/
IDProperty *IDP_CopyProperty(IDProperty *prop)
{
	switch (prop->type) {
		case IDP_GROUP: return IDP_CopyGroup(prop);
		case IDP_STRING: return IDP_CopyString(prop);
		case IDP_ARRAY: return IDP_CopyArray(prop);
		case IDP_IDPARRAY: return IDP_CopyIDPArray(prop);
		default: return idp_generic_copy(prop);
	}
}

IDProperty *IDP_GetProperties(ID *id, int create_if_needed)
{
	if (id->properties) return id->properties;
	else {
		if (create_if_needed) {
			id->properties = MEM_callocN(sizeof(IDProperty), "IDProperty");
			id->properties->type = IDP_GROUP;
			/* dont overwite the data's name and type
			 * some functions might need this if they
			 * dont have a real ID, should be named elsewhere - Campbell */
			/* strcpy(id->name, "top_level_group");*/
		}
		return id->properties;
	}
}

int IDP_EqualsProperties(IDProperty *prop1, IDProperty *prop2)
{
	if(prop1 == NULL && prop2 == NULL)
		return 1;
	else if(prop1 == NULL || prop2 == NULL)
		return 0;
	else if(prop1->type != prop2->type)
		return 0;

	if(prop1->type == IDP_INT)
		return (IDP_Int(prop1) == IDP_Int(prop2));
	else if(prop1->type == IDP_FLOAT)
		return (IDP_Float(prop1) == IDP_Float(prop2));
	else if(prop1->type == IDP_DOUBLE)
		return (IDP_Double(prop1) == IDP_Double(prop2));
	else if(prop1->type == IDP_STRING)
		return BSTR_EQ(IDP_String(prop1), IDP_String(prop2));
	else if(prop1->type == IDP_ARRAY) {
		if(prop1->len == prop2->len && prop1->subtype == prop2->subtype)
			return memcmp(IDP_Array(prop1), IDP_Array(prop2), idp_size_table[(int)prop1->subtype]*prop1->len);
		else
			return 0;
	}
	else if(prop1->type == IDP_GROUP) {
		IDProperty *link1, *link2;

		if(BLI_countlist(&prop1->data.group) != BLI_countlist(&prop2->data.group))
			return 0;

		for(link1=prop1->data.group.first; link1; link1=link1->next) {
			link2= IDP_GetPropertyFromGroup(prop2, link1->name);

			if(!IDP_EqualsProperties(link1, link2))
				return 0;
		}

		return 1;
	}
	else if(prop1->type == IDP_IDPARRAY) {
		IDProperty *array1= IDP_IDPArray(prop1);
		IDProperty *array2= IDP_IDPArray(prop2);
		int i;

		if(prop1->len != prop2->len)
			return 0;
		
		for(i=0; i<prop1->len; i++)
			if(!IDP_EqualsProperties(&array1[i], &array2[i]))
				return 0;
	}
	
	return 1;
}

IDProperty *IDP_New(int type, IDPropertyTemplate val, const char *name)
{
	IDProperty *prop=NULL;

	switch (type) {
		case IDP_INT:
			prop = MEM_callocN(sizeof(IDProperty), "IDProperty int");
			prop->data.val = val.i;
			break;
		case IDP_FLOAT:
			prop = MEM_callocN(sizeof(IDProperty), "IDProperty float");
			*(float*)&prop->data.val = val.f;
			break;
		case IDP_DOUBLE:
			prop = MEM_callocN(sizeof(IDProperty), "IDProperty float");
			*(double*)&prop->data.val = val.d;
			break;		
		case IDP_ARRAY:
		{
			/*for now, we only support float and int and double arrays*/
			if (val.array.type == IDP_FLOAT || val.array.type == IDP_INT || val.array.type == IDP_DOUBLE || val.array.type == IDP_GROUP) {
				prop = MEM_callocN(sizeof(IDProperty), "IDProperty array");
				prop->subtype = val.array.type;
				if (val.array.len)
					prop->data.pointer = MEM_callocN(idp_size_table[val.array.type]*val.array.len, "id property array");
				prop->len = prop->totallen = val.array.len;
				break;
			} else {
				return NULL;
			}
		}
		case IDP_STRING:
		{
			char *st = val.str;
			int stlen;

			prop = MEM_callocN(sizeof(IDProperty), "IDProperty string");
			if (st == NULL) {
				prop->data.pointer = MEM_callocN(DEFAULT_ALLOC_FOR_NULL_STRINGS, "id property string 1");
				prop->totallen = DEFAULT_ALLOC_FOR_NULL_STRINGS;
				prop->len = 1; /*NULL string, has len of 1 to account for null byte.*/
			} else {
				stlen = strlen(st) + 1;
				prop->data.pointer = MEM_callocN(stlen, "id property string 2");
				prop->len = prop->totallen = stlen;
				strcpy(prop->data.pointer, st);
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
	
	/*security null byte*/
	prop->name[MAX_IDPROP_NAME-1] = 0;
	
	return prop;
}

/*NOTE: this will free all child properties including list arrays and groups!
  Also, note that this does NOT unlink anything!  Plus it doesn't free
  the actual IDProperty struct either.*/
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

/*Unlinks any IDProperty<->ID linkage that might be going on.
  note: currently unused.*/
void IDP_UnlinkProperty(IDProperty *prop)
{
	switch (prop->type) {
		case IDP_ID:
			IDP_UnlinkID(prop);
	}
}
