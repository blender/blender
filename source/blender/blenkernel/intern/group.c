/* 
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
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_ID.h"
#include "DNA_group_types.h"
#include "DNA_object_types.h"
#include "DNA_ipo_types.h"

#include "BLI_blenlib.h"
#include "BKE_global.h"
#include "BKE_main.h"

#include "BKE_library.h"
#include "BKE_group.h"
#include "BKE_object.h"
#include "BKE_ipo.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

void free_group_object(GroupObject *go)
{
	MEM_freeN(go);
}


void free_group(Group *group)
{
	/* don't free group itself */
	GroupObject *go;
	
	while(group->gobject.first) {
		go= group->gobject.first;
		BLI_remlink(&group->gobject, go);
		free_group_object(go);
	}
}

Group *add_group()
{
	Group *group;
	
	group = alloc_libblock(&G.main->group, ID_GR, "Group");
	return group;
}

/* external */
void add_to_group(Group *group, Object *ob)
{
	GroupObject *go;
	
	if(group==NULL || ob==NULL) return;
	
	/* check if the object has been added already */
	for(go= group->gobject.first; go; go= go->next) {
		if(go->ob==ob) return;
	}
	
	go= MEM_callocN(sizeof(GroupObject), "groupobject");
	BLI_addtail( &group->gobject, go);
	
	go->ob= ob;
	
}

void rem_from_group(Group *group, Object *ob)
{
	GroupObject *go, *gon;
	
	if(group==NULL || ob==NULL) return;
	
	go= group->gobject.first;
	while(go) {
		gon= go->next;
		if(go->ob==ob) {
			BLI_remlink(&group->gobject, go);
			free_group_object(go);
		}
		go= gon;
	}
}

int object_in_group(Object *ob, Group *group)
{
	GroupObject *go;
	
	if(group==NULL || ob==NULL) return 0;
	
	for(go= group->gobject.first; go; go= go->next) {
		if(go->ob==ob) 
			return 1;
	}
	return 0;
}

Group *find_group(Object *ob)
{
	Group *group= G.main->group.first;
	
	while(group) {
		if(object_in_group(ob, group))
			return group;
		group= group->id.next;
	}
	return NULL;
}
