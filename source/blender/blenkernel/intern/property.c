
/*  property.c   june 2000
 * 
 *  ton roosendaal
 * $Id$
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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "MEM_guardedalloc.h"

#include "DNA_property_types.h"
#include "DNA_object_types.h"
#include "DNA_listBase.h"


#include "BLI_blenlib.h"
#include "BKE_bad_level_calls.h"
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
	free_properties( lbn ); /* incase we are copying to an object with props */
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
	prop->poin= 0;
	
	prop->otype= prop->type;
	prop->data= 0;
	
	switch(prop->type) {
	case PROP_BOOL:
		prop->poin= &prop->data;
		break;
	case PROP_INT:
		prop->poin= &prop->data;
		break;
	case PROP_FLOAT:
		prop->poin= &prop->data;
		break;
	case PROP_STRING:
		prop->poin= MEM_callocN(MAX_PROPSTRING, "property string");
		break;
	case PROP_TIME:
		prop->poin= &prop->data;
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

bProperty *get_ob_property(Object *ob, char *name)
{
	bProperty *prop;
	
	prop= ob->prop.first;
	while(prop) {
		if( strcmp(prop->name, name)==0 ) return prop;
		prop= prop->next;
	}
	return NULL;
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
int compare_property(bProperty *prop, char *str)
{
//	extern int Gdfra;		/* sector.c */
	float fvalue, ftest;
	
	switch(prop->type) {
	case PROP_BOOL:
		if(BLI_strcasecmp(str, "true")==0) {
			if(prop->data==1) return 0;
			else return 1;
		}
		else if(BLI_strcasecmp(str, "false")==0) {
			if(prop->data==0) return 0;
			else return 1;
		}
		/* no break, do prop_int too! */
		
	case PROP_INT:
		return prop->data - atoi(str);

	case PROP_FLOAT:
	case PROP_TIME:
		// WARNING: untested for PROP_TIME
		// function isn't used currently
		fvalue= *((float *)&prop->data);
		ftest= (float)atof(str);
		if( fvalue > ftest) return 1;
		else if( fvalue < ftest) return -1;
		return 0;

	case PROP_STRING:
		return strcmp(prop->poin, str);
	}
	
	return 0;
}

void set_property(bProperty *prop, char *str)
{
//	extern int Gdfra;		/* sector.c */

	switch(prop->type) {
	case PROP_BOOL:
		if(BLI_strcasecmp(str, "true")==0) prop->data= 1;
		else if(BLI_strcasecmp(str, "false")==0) prop->data= 0;
		else prop->data= (atoi(str)!=0);
		break;
	case PROP_INT:
		prop->data= atoi(str);
		break;
	case PROP_FLOAT:
	case PROP_TIME:
		*((float *)&prop->data)= (float)atof(str);
		break;
	case PROP_STRING:
		strcpy(prop->poin, str);
		break;
	}
	
}

void add_property(bProperty *prop, char *str)
{
//	extern int Gdfra;		/* sector.c */

	switch(prop->type) {
	case PROP_BOOL:
	case PROP_INT:
		prop->data+= atoi(str);
		break;
	case PROP_FLOAT:
	case PROP_TIME:
		*((float *)&prop->data)+= (float)atof(str);
		break;
	case PROP_STRING:
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
	case PROP_BOOL:
	case PROP_INT:
		sprintf(str, "%d", prop->data);
		break;
	case PROP_FLOAT:
	case PROP_TIME:
		sprintf(str, "%f", *((float *)&prop->data));
		break;
	case PROP_STRING:
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
