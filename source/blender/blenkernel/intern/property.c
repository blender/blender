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

void free_property(bProperty *prop)
{
	
	if(prop->poin && prop->poin != &prop->data) MEM_freeN(prop->poin);
	MEM_freeN(prop);
	
}

void free_properties(ListBase *lb)
{
	bProperty *prop;
	
	while( (prop= lb->first) ) {
		BLI_remlink(lb, prop);
		free_property(prop);
	}
}

bProperty *copy_property(bProperty *prop)
{
	bProperty *propn;
	
	propn= MEM_dupallocN(prop);
	if(prop->poin && prop->poin != &prop->data) {
		propn->poin= MEM_dupallocN(prop->poin);
	}
	else propn->poin= &propn->data;
	
	return propn;
}

void copy_properties(ListBase *lbn, ListBase *lbo)
{
	bProperty *prop, *propn;
	free_properties( lbn ); /* in case we are copying to an object with props */
	prop= lbo->first;
	while(prop) {
		propn= copy_property(prop);
		BLI_addtail(lbn, propn);
		prop= prop->next;
	}
	
	
}

void init_property(bProperty *prop)
{
	/* also use when property changes type */
	
	if(prop->poin && prop->poin != &prop->data) MEM_freeN(prop->poin);
	prop->poin= NULL;
	
	prop->data= 0;
	
	switch(prop->type) {
	case GPROP_BOOL:
	case GPROP_INT:
	case GPROP_FLOAT:
	case GPROP_TIME:
		prop->poin= &prop->data;
		break;
	case GPROP_STRING:
		prop->poin= MEM_callocN(MAX_PROPSTRING, "property string");
		break;
	}
}


bProperty *new_property(int type)
{
	bProperty *prop;

	prop= MEM_callocN(sizeof(bProperty), "property");
	prop->type= type;

	init_property(prop);
	
	strcpy(prop->name, "prop");

	return prop;
}

/* used by unique_property() only */
static bProperty *get_property__internal(bProperty *first, bProperty *self, const char *name)
{
	bProperty *p;
	for(p= first; p; p= p->next) {
		if (p!=self && (strcmp(p->name, name)==0))
			return p;
	}
	return NULL;
}
void unique_property(bProperty *first, bProperty *prop, int force)
{
	bProperty *p;

	/* set the first if its not set */
	if(first==NULL) {
		first= prop;
		while(first->prev) {
			first= first->prev;
		}
	}

	if(force) {
		/* change other names to make them unique */
		while((p = get_property__internal(first, prop, prop->name))) {
			unique_property(first, p, 0);
		}
	}else {
		/* change our own name until its unique */
		if(get_property__internal(first, prop, prop->name)) {
			/* there is a collision */
			char new_name[sizeof(prop->name)];
			char base_name[sizeof(prop->name)];
			char num[sizeof(prop->name)];
			int i= 0;

			/* strip numbers */
			BLI_strncpy(base_name, prop->name, sizeof(base_name));
			for(i= strlen(base_name)-1; (i>=0 && isdigit(base_name[i])); i--) {
				base_name[i]= '\0';
			}
			i= 0;

			do { /* ensure we have enough chars for the new number in the name */
				BLI_snprintf(num, sizeof(num), "%d", i++);
				BLI_strncpy(new_name, base_name, sizeof(prop->name) - strlen(num));
				strcat(new_name, num);
			} while(get_property__internal(first, prop, new_name));

			BLI_strncpy(prop->name, new_name, sizeof(prop->name));
		}
	}
}

bProperty *get_ob_property(Object *ob, const char *name)
{
	return BLI_findstring(&ob->prop, name, offsetof(bProperty, name));
}

void set_ob_property(Object *ob, bProperty *propc)
{
	bProperty *prop;
	prop= get_ob_property(ob, propc->name);
	if(prop) {
		free_property(prop);
		BLI_remlink(&ob->prop, prop);
	}
	BLI_addtail(&ob->prop, copy_property(propc));
}

/* negative: prop is smaller
 * positive: prop is larger
 */
int compare_property(bProperty *prop, const char *str)
{
//	extern int Gdfra;		/* sector.c */
	float fvalue, ftest;
	
	switch(prop->type) {
	case GPROP_BOOL:
		if(BLI_strcasecmp(str, "true")==0) {
			if(prop->data==1) return 0;
			else return 1;
		}
		else if(BLI_strcasecmp(str, "false")==0) {
			if(prop->data==0) return 0;
			else return 1;
		}
		/* no break, do GPROP_int too! */
		
	case GPROP_INT:
		return prop->data - atoi(str);

	case GPROP_FLOAT:
	case GPROP_TIME:
		// WARNING: untested for GPROP_TIME
		// function isn't used currently
		fvalue= *((float *)&prop->data);
		ftest= (float)atof(str);
		if( fvalue > ftest) return 1;
		else if( fvalue < ftest) return -1;
		return 0;

	case GPROP_STRING:
		return strcmp(prop->poin, str);
	}
	
	return 0;
}

void set_property(bProperty *prop, const char *str)
{
//	extern int Gdfra;		/* sector.c */

	switch(prop->type) {
	case GPROP_BOOL:
		if(BLI_strcasecmp(str, "true")==0) prop->data= 1;
		else if(BLI_strcasecmp(str, "false")==0) prop->data= 0;
		else prop->data= (atoi(str)!=0);
		break;
	case GPROP_INT:
		prop->data= atoi(str);
		break;
	case GPROP_FLOAT:
	case GPROP_TIME:
		*((float *)&prop->data)= (float)atof(str);
		break;
	case GPROP_STRING:
		strcpy(prop->poin, str); /* TODO - check size? */
		break;
	}
	
}

void add_property(bProperty *prop, const char *str)
{
//	extern int Gdfra;		/* sector.c */

	switch(prop->type) {
	case GPROP_BOOL:
	case GPROP_INT:
		prop->data+= atoi(str);
		break;
	case GPROP_FLOAT:
	case GPROP_TIME:
		*((float *)&prop->data)+= (float)atof(str);
		break;
	case GPROP_STRING:
		/* strcpy(prop->poin, str); */
		break;
	}
}

/* reads value of property, sets it in chars in str */
void set_property_valstr(bProperty *prop, char *str)
{
//	extern int Gdfra;		/* sector.c */

	if(str == NULL) return;

	switch(prop->type) {
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

void cp_property(bProperty *prop1, bProperty *prop2)
{
	char str[128];

	set_property_valstr(prop2, str);

	set_property(prop1, str);
}
