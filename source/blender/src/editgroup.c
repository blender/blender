/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_editVert.h"

#include "DNA_group_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"

#include "BKE_group.h"
#include "BKE_global.h"
#include "BKE_main.h"

#include "BIF_space.h"
#include "BIF_interface.h"
#include "BIF_toolbox.h"
#include "BIF_editgroup.h"

#include "blendef.h"
#include "mydevice.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

void set_active_group(void)
{
	/* with active object, find active group */
	Group *group;
	GroupObject *go;
	
	G.scene->group= NULL;
	
	if(BASACT) {
		group= G.main->group.first;
		while(group) {
			go= group->gobject.first;
			while(go) {
				if(go->ob == OBACT) {
					G.scene->group= group;
					return;
				}
				go= go->next;
			}
			group= group->id.next;
		}
	}
}


void add_selected_to_group(void)
{
	Base *base= FIRSTBASE;
	Group *group;
	
	if(BASACT==NULL) {
		error("No active object");
		return;
	}
	
	if(okee("Add selected to group")==0) return;
	
	if(G.scene->group==NULL) G.scene->group= add_group();
	
	while(base) {
		if TESTBASE(base) {
			
			/* each object only in one group */
			group= find_group(base->object);
			if(group==G.scene->group);
			else {
				if(group) {
					rem_from_group(group, base->object);
				}
				add_to_group(G.scene->group, base->object);
				base->object->flag |= OB_FROMGROUP;
				base->flag |= OB_FROMGROUP;
			}
		}
		base= base->next;
	}
	
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSOBJECT, 0);
}

void rem_selected_from_group(void)
{
	Base *base=FIRSTBASE;
	Group *group;
	
	if(okee("Remove selected from group")==0) return;

	while(base) {
		if TESTBASE(base) {

			group= find_group(base->object);
			if(group) {
				rem_from_group(group, base->object);
			
				base->object->flag &= ~OB_FROMGROUP;
				base->flag &= ~OB_FROMGROUP;
			}
		}
		base= base->next;
	}
	
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSOBJECT, 0);
}

void prev_group_key(Group *group)
{
	GroupKey *gk= group->active;
	
	if(gk) gk= gk->prev;
	
	if(gk==NULL) group->active= group->gkey.last;
	else group->active= gk;
	
	set_group_key(group);
}

void next_group_key(Group *group)
{
	GroupKey *gk= group->active;
	
	if(gk) gk= gk->next;
	
	if(gk==NULL) group->active= group->gkey.first;
	else group->active= gk;
	
	set_group_key(group);
	
}


