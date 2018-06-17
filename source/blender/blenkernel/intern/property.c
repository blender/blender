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
 * The Original Code is: all of this file.
 *
 * Contributor(s): ton roosendaal
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/property.c
 *  \ingroup bke
 *
 * This module deals with bProperty only,
 * they are used on blender objects in the game engine
 * (where they get converted into C++ classes - CValue and subclasses)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>

#include "MEM_guardedalloc.h"

#include "DNA_property_types.h"
#include "DNA_object_types.h"

#include "BLI_blenlib.h"

#include "BKE_property.h"

void BKE_bproperty_free(bProperty *prop)
{

	if (prop->poin && prop->poin != &prop->data) MEM_freeN(prop->poin);
	MEM_freeN(prop);

}

void BKE_bproperty_free_list(ListBase *lb)
{
	bProperty *prop;

	while ((prop = BLI_pophead(lb))) {
		BKE_bproperty_free(prop);
	}
}

bProperty *BKE_bproperty_copy(const bProperty *prop)
{
	bProperty *propn;

	propn = MEM_dupallocN(prop);
	if (prop->poin && prop->poin != &prop->data) {
		propn->poin = MEM_dupallocN(prop->poin);
	}
	else {
		propn->poin = &propn->data;
	}

	return propn;
}

void BKE_bproperty_copy_list(ListBase *lbn, const ListBase *lbo)
{
	bProperty *prop, *propn;
	BKE_bproperty_free_list(lbn); /* in case we are copying to an object with props */
	prop = lbo->first;
	while (prop) {
		propn = BKE_bproperty_copy(prop);
		BLI_addtail(lbn, propn);
		prop = prop->next;
	}


}

void BKE_bproperty_init(bProperty *prop)
{
	/* also use when property changes type */

	if (prop->poin && prop->poin != &prop->data) MEM_freeN(prop->poin);
	prop->poin = NULL;

	prop->data = 0;

	switch (prop->type) {
		case GPROP_BOOL:
		case GPROP_INT:
		case GPROP_FLOAT:
		case GPROP_TIME:
			prop->poin = &prop->data;
			break;
		case GPROP_STRING:
			prop->poin = MEM_callocN(MAX_PROPSTRING, "property string");
			break;
	}
}


bProperty *BKE_bproperty_new(int type)
{
	bProperty *prop;

	prop = MEM_callocN(sizeof(bProperty), "property");
	prop->type = type;

	BKE_bproperty_init(prop);

	strcpy(prop->name, "prop");

	return prop;
}


bProperty *BKE_bproperty_object_get(Object *ob, const char *name)
{
	return BLI_findstring(&ob->prop, name, offsetof(bProperty, name));
}

void BKE_bproperty_object_set(Object *ob, bProperty *propc)
{
	bProperty *prop;
	prop = BKE_bproperty_object_get(ob, propc->name);
	if (prop) {
		BLI_remlink(&ob->prop, prop);
		BKE_bproperty_free(prop);
	}
	BLI_addtail(&ob->prop, BKE_bproperty_copy(propc));
}

/* negative: prop is smaller
 * positive: prop is larger
 */
#if 0  /* UNUSED */
int BKE_bproperty_cmp(bProperty *prop, const char *str)
{
//	extern int Gdfra;		/* sector.c */
	float fvalue, ftest;

	switch (prop->type) {
		case GPROP_BOOL:
			if (BLI_strcasecmp(str, "true") == 0) {
				if (prop->data == 1) return 0;
				else return 1;
			}
			else if (BLI_strcasecmp(str, "false") == 0) {
				if (prop->data == 0) return 0;
				else return 1;
			}
		/* no break, do GPROP_int too! */

		case GPROP_INT:
			return prop->data - atoi(str);

		case GPROP_FLOAT:
		case GPROP_TIME:
			/* WARNING: untested for GPROP_TIME
			 * function isn't used currently */
			fvalue = *((float *)&prop->data);
			ftest = (float)atof(str);
			if (fvalue > ftest) return 1;
			else if (fvalue < ftest) return -1;
			return 0;

		case GPROP_STRING:
			return strcmp(prop->poin, str);
	}

	return 0;
}
#endif

void BKE_bproperty_set(bProperty *prop, const char *str)
{
//	extern int Gdfra;		/* sector.c */

	switch (prop->type) {
		case GPROP_BOOL:
			if (BLI_strcasecmp(str, "true") == 0) prop->data = 1;
			else if (BLI_strcasecmp(str, "false") == 0) prop->data = 0;
			else prop->data = (atoi(str) != 0);
			break;
		case GPROP_INT:
			prop->data = atoi(str);
			break;
		case GPROP_FLOAT:
		case GPROP_TIME:
			*((float *)&prop->data) = (float)atof(str);
			break;
		case GPROP_STRING:
			strcpy(prop->poin, str); /* TODO - check size? */
			break;
	}

}

void BKE_bproperty_add(bProperty *prop, const char *str)
{
//	extern int Gdfra;		/* sector.c */

	switch (prop->type) {
		case GPROP_BOOL:
		case GPROP_INT:
			prop->data += atoi(str);
			break;
		case GPROP_FLOAT:
		case GPROP_TIME:
			*((float *)&prop->data) += (float)atof(str);
			break;
		case GPROP_STRING:
			/* strcpy(prop->poin, str); */
			break;
	}
}

/* reads value of property, sets it in chars in str */
void BKE_bproperty_set_valstr(bProperty *prop, char str[MAX_PROPSTRING])
{
//	extern int Gdfra;		/* sector.c */

	if (str == NULL) return;

	switch (prop->type) {
		case GPROP_BOOL:
		case GPROP_INT:
			sprintf(str, "%d", prop->data);
			break;
		case GPROP_FLOAT:
		case GPROP_TIME:
			sprintf(str, "%f", *((float *)&prop->data));
			break;
		case GPROP_STRING:
			BLI_strncpy(str, prop->poin, MAX_PROPSTRING);
			break;
	}
}

#if 0   /* UNUSED */
void cp_property(bProperty *prop1, bProperty *prop2)
{
	char str[128];

	BKE_bproperty_set_valstr(prop2, str);

	BKE_bproperty_set(prop1, str);
}
#endif
