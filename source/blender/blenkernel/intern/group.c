/*  group.c   sept 2000
 *  - cleaned up mar-01 nzc 
 * 
 * 
 *  ton roosendaal
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

void free_object_key(ObjectKey *ok)
{
	if(ok->ipo) ok->ipo->id.us--;
	
	MEM_freeN(ok);
}

void free_group_object(GroupObject *go)
{
	ObjectKey *ok;
	
	while(go->okey.first) {
		ok= go->okey.first;
		BLI_remlink(&go->okey, ok);
		free_object_key(ok);
	}
	MEM_freeN(go);
}


void free_group(Group *group)
{
	/* don't free group itself */
	GroupObject *go;
	
	BLI_freelistN(&group->gkey);
	
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

/* assumes 'ok' is unitialized */
void object_to_obkey(Object *ob, ObjectKey *ok)
{
	ok->partype= ob->partype;
	ok->par1= ob->par1;
	ok->par2= ob->par2;
	ok->par3= ob->par3;
	
	ok->parent= ob->parent;
	ok->track= ob->track;
	
	ok->ipo= copy_ipo(ob->ipo);
	
	memcpy(ok->loc, ob->loc, 7*3*sizeof(float));
	memcpy(ok->quat, ob->quat, 2*4*sizeof(float));
	memcpy(ok->obmat, ob->obmat, 3*4*4*sizeof(float));
	
	ok->lay= ob->lay;
	ok->transflag= ob->transflag;
	ok->trackflag= ob->transflag;
	ok->upflag= ob->upflag;
	ok->sf= ob->sf;
	ok->ctime= ob->ctime;

	
}

void obkey_to_object(ObjectKey *ok, Object *ob)
{
	ob->partype= ok->partype;
	ob->par1= ok->par1;
	ob->par2= ok->par2;
	ob->par3= ok->par3;
	
	ob->parent= ok->parent;
	ob->track= ok->track;
	
	/* pretty tricky, this makes ob->ipo blocks with users 'hanging around' */
	if(ob->ipo) {
		free_libblock_us(&G.main->ipo, ob->ipo);
	}
	ob->ipo= copy_ipo(ok->ipo);
	
	memcpy(ob->loc, ok->loc, 7*3*sizeof(float));
	memcpy(ob->quat, ok->quat, 2*4*sizeof(float));
	memcpy(ob->obmat, ok->obmat, 3*4*4*sizeof(float));
	
	ob->lay= ok->lay;
	ob->transflag= ok->transflag;
	ob->trackflag= ok->transflag;
	ob->upflag= ok->upflag;
	ob->sf= ok->sf;
	ob->ctime= ok->ctime;
}

/* current ob position */
void add_object_key(GroupObject *go, GroupKey *gk)
{
	ObjectKey *ok;
	
	/* check if there already is a key */
	ok= go->okey.first;
	while(ok) {
		if(ok->gkey == gk) break;
		ok= ok->next;
	}
	
	if(ok) {
		BLI_remlink(&go->okey, ok);
		free_object_key(ok);
	}
	ok= MEM_mallocN(sizeof(ObjectKey), "objectkey");
	ok->gkey= gk;
	
	object_to_obkey(go->ob, ok);
	
	BLI_addtail(&go->okey, ok);
	
}

/* external */
void add_to_group(Group *group, Object *ob)
{
	GroupObject *go;
	GroupKey *gk;
	
	/* check if the object has been added already */
	go= group->gobject.first;
	while(go) {
		if(go->ob==ob) return;
		go= go->next;
	}
	
	go= MEM_callocN(sizeof(GroupObject), "groupobject");
	BLI_addtail( &group->gobject, go);
	
	go->ob= ob;
	
	/* keys? */
	gk= group->gkey.first;
	while(gk) {
		add_object_key(go, gk);
		gk= gk->next;
	}
}

void rem_from_group(Group *group, Object *ob)
{
	GroupObject *go, *gon;
	ObjectKey *ok;
	
	go= group->gobject.first;
	while(go) {
		gon= go->next;
		if(go->ob==ob) {
			BLI_remlink(&group->gobject, go);
			free_group_object(go);
		}
		else {
			ok= go->okey.first;
			while(ok) {
				if(ok->parent==ob) ok->parent= NULL;
				if(ok->track==ob) ok->track= NULL;
				ok= ok->next;
			}
		}
		go= gon;
	}
}

void add_group_key(Group *group)
{
	GroupObject *go;
	GroupKey *gk;	
	int nr=10;
	extern char colname_array[][20]; /* material.c */

	gk= group->gkey.first;
	while(gk) {
		nr++;
		gk= gk->next;
	}

	gk= MEM_callocN(sizeof(GroupKey), "groupkey");
	BLI_addtail(&group->gkey, gk);
	strcpy(gk->name, colname_array[ nr % 120 ]);
	
	go= group->gobject.first;
	while(go) {
		add_object_key(go, gk);
		go= go->next;
	}
	
	group->active= gk;
}

void set_object_key(Object *ob, ObjectKey *ok)
{
	obkey_to_object(ok, ob);	
}

void set_group_key(Group *group)
{
	/* sets active */
	GroupObject *go;
	ObjectKey *ok;
	
	if(group->active==NULL) return;
	
	go= group->gobject.first;
	while(go) {
		ok= go->okey.first;
		while(ok) {
			if(ok->gkey==group->active) {
				set_object_key(go->ob, ok);
				break;
			}
			ok= ok->next;
		}
		go= go->next;
	}
	
}

Group *find_group(Object *ob)
{
	Group *group= G.main->group.first;
	GroupObject *go;
	
	while(group) {

		go= group->gobject.first;
		while(go) {
			if(go->ob==ob) return group;
			go= go->next;
		}
		group= group->id.next;
	}
	return NULL;
}

void set_group_key_name(Group *group, char *name)
{
	GroupKey *gk;
	
	if(group==NULL) return;
	
	gk= group->gkey.first;
	while(gk) {
		if(strcmp(name, gk->name)==0) break;
		gk= gk->next;
	}
	
	if(gk) {
		group->active= gk;
		set_group_key(group);
	}
}

void set_group_key_frame(Group *group, float frame)
{
	GroupObject *go;
	
	if(group==NULL) return;

	go= group->gobject.first;
	while(go) {
		where_is_object_time(go->ob, frame);
		go= go->next;
	}
}
