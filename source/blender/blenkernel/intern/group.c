/* 
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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/group.c
 *  \ingroup bke
 */


#include <stdio.h>
#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "DNA_group_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_nla_types.h"
#include "DNA_scene_types.h"
#include "DNA_particle_types.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"


#include "BKE_global.h"
#include "BKE_group.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_scene.h" /* object_in_scene */

static void free_group_object(GroupObject *go)
{
	MEM_freeN(go);
}


void free_group_objects(Group *group)
{
	/* don't free group itself */
	GroupObject *go;
	
	while (group->gobject.first) {
		go= group->gobject.first;
		BLI_remlink(&group->gobject, go);
		free_group_object(go);
	}
}

void unlink_group(Group *group)
{
	Main *bmain= G.main;
	Material *ma;
	Object *ob;
	Scene *sce;
	SceneRenderLayer *srl;
	ParticleSystem *psys;
	
	for (ma= bmain->mat.first; ma; ma= ma->id.next) {
		if (ma->group==group)
			ma->group= NULL;
	}
	for (ma= bmain->mat.first; ma; ma= ma->id.next) {
		if (ma->group==group)
			ma->group= NULL;
	}
	for (sce= bmain->scene.first; sce; sce= sce->id.next) {
		Base *base= sce->base.first;
		
		/* ensure objects are not in this group */
		for (; base; base= base->next) {
			if (rem_from_group(group, base->object, sce, base) && find_group(base->object, NULL)==NULL) {
				base->object->flag &= ~OB_FROMGROUP;
				base->flag &= ~OB_FROMGROUP;
			}
		}			
		
		for (srl= sce->r.layers.first; srl; srl= srl->next) {
			if (srl->light_override==group)
				srl->light_override= NULL;
		}
	}
	
	for (ob= bmain->object.first; ob; ob= ob->id.next) {
		
		if (ob->dup_group==group) {
			ob->dup_group= NULL;
#if 0		/* XXX OLD ANIMSYS, NLASTRIPS ARE NO LONGER USED */
			{
				bActionStrip *strip;
				/* duplicator strips use a group object, we remove it */
				for (strip= ob->nlastrips.first; strip; strip= strip->next) {
					if (strip->object)
						strip->object= NULL;
				}
			}
#endif
		}
		
		for (psys=ob->particlesystem.first; psys; psys=psys->next) {
			if (psys->part->dup_group==group)
				psys->part->dup_group= NULL;
#if 0		/* not used anymore, only keps for readfile.c, no need to account for this */
			if (psys->part->eff_group==group)
				psys->part->eff_group= NULL;
#endif
		}
	}
	
	/* group stays in library, but no members */
	free_group_objects(group);
	group->id.us= 0;
}

Group *add_group(const char *name)
{
	Group *group;
	
	group = alloc_libblock(&G.main->group, ID_GR, name);
	group->layer= (1<<20)-1;
	return group;
}

Group *copy_group(Group *group)
{
	Group *groupn;

	groupn= MEM_dupallocN(group);
	BLI_duplicatelist(&groupn->gobject, &group->gobject);

	return groupn;
}

/* external */
static int add_to_group_internal(Group *group, Object *ob)
{
	GroupObject *go;
	
	if (group==NULL || ob==NULL) return 0;
	
	/* check if the object has been added already */
	for (go= group->gobject.first; go; go= go->next) {
		if (go->ob==ob) return 0;
	}
	
	go= MEM_callocN(sizeof(GroupObject), "groupobject");
	BLI_addtail(&group->gobject, go);
	
	go->ob= ob;
	
	return 1;
}

int add_to_group(Group *group, Object *object, Scene *scene, Base *base)
{
	if (add_to_group_internal(group, object)) {
		if ((object->flag & OB_FROMGROUP)==0) {

			if (scene && base==NULL)
				base= object_in_scene(object, scene);

			object->flag |= OB_FROMGROUP;

			if (base)
				base->flag |= OB_FROMGROUP;
		}
		return 1;
	}
	else {
		return 0;
	}
}

/* also used for ob==NULL */
static int rem_from_group_internal(Group *group, Object *ob)
{
	GroupObject *go, *gon;
	int removed = 0;
	if (group==NULL) return 0;
	
	go= group->gobject.first;
	while (go) {
		gon= go->next;
		if (go->ob==ob) {
			BLI_remlink(&group->gobject, go);
			free_group_object(go);
			removed = 1;
			/* should break here since an object being in a group twice cant happen? */
		}
		go= gon;
	}
	return removed;
}

int rem_from_group(Group *group, Object *object, Scene *scene, Base *base)
{
	if (rem_from_group_internal(group, object)) {
		/* object can be NULL */
		if (object && find_group(object, NULL) == NULL) {
			if (scene && base==NULL)
				base= object_in_scene(object, scene);

			object->flag &= ~OB_FROMGROUP;

			if (base)
				base->flag &= ~OB_FROMGROUP;
		}
		return 1;
	}
	else {
		return 0;
	}
}

int object_in_group(Object *ob, Group *group)
{
	GroupObject *go;
	
	if (group==NULL || ob==NULL) return 0;
	
	for (go= group->gobject.first; go; go= go->next) {
		if (go->ob==ob) 
			return 1;
	}
	return 0;
}

Group *find_group(Object *ob, Group *group)
{
	if (group)
		group= group->id.next;
	else
		group= G.main->group.first;
	
	while (group) {
		if (object_in_group(ob, group))
			return group;
		group= group->id.next;
	}
	return NULL;
}

void group_tag_recalc(Group *group)
{
	GroupObject *go;
	
	if (group==NULL) return;
	
	for (go= group->gobject.first; go; go= go->next) {
		if (go->ob) 
			go->ob->recalc= go->recalc;
	}
}

int group_is_animated(Object *UNUSED(parent), Group *group)
{
	GroupObject *go;

#if 0 /* XXX OLD ANIMSYS, NLASTRIPS ARE NO LONGER USED */
	if (parent->nlastrips.first)
		return 1;
#endif

	for (go= group->gobject.first; go; go= go->next)
		if (go->ob && go->ob->proxy)
			return 1;

	return 0;
}

#if 0 // add back when timeoffset & animsys work again
/* only replaces object strips or action when parent nla instructs it */
/* keep checking nla.c though, in case internal structure of strip changes */
static void group_replaces_nla(Object *parent, Object *target, char mode)
{
	static ListBase nlastrips={NULL, NULL};
	static bAction *action= NULL;
	static int done= 0;
	bActionStrip *strip, *nstrip;
	
	if (mode=='s') {
		
		for (strip= parent->nlastrips.first; strip; strip= strip->next) {
			if (strip->object==target) {
				if (done==0) {
					/* clear nla & action from object */
					nlastrips= target->nlastrips;
					target->nlastrips.first= target->nlastrips.last= NULL;
					action= target->action;
					target->action= NULL;
					target->nlaflag |= OB_NLA_OVERRIDE;
					done= 1;
				}
				nstrip= MEM_dupallocN(strip);
				BLI_addtail(&target->nlastrips, nstrip);
			}
		}
	}
	else if (mode=='e') {
		if (done) {
			BLI_freelistN(&target->nlastrips);
			target->nlastrips= nlastrips;
			target->action= action;
			
			nlastrips.first= nlastrips.last= NULL;	/* not needed, but yah... :) */
			action= NULL;
			done= 0;
		}
	}
}
#endif

/* puts all group members in local timing system, after this call
 * you can draw everything, leaves tags in objects to signal it needs further updating */

/* note: does not work for derivedmesh and render... it recreates all again in convertblender.c */
void group_handle_recalc_and_update(Scene *scene, Object *UNUSED(parent), Group *group)
{
	GroupObject *go;
	
#if 0 /* warning, isn't clearing the recalc flag on the object which causes it to run all the time,
	   * not just on frame change.
	   * This isn't working because the animation data is only re-evalyated on frame change so commenting for now
	   * but when its enabled at some point it will need to be changed so as not to update so much - campbell */

	/* if animated group... */
	if (parent->nlastrips.first) {
		int cfrao;
		
		/* switch to local time */
		cfrao= scene->r.cfra;
		
		/* we need a DAG per group... */
		for (go= group->gobject.first; go; go= go->next) {
			if (go->ob && go->recalc) {
				go->ob->recalc= go->recalc;
				
				group_replaces_nla(parent, go->ob, 's');
				object_handle_update(scene, go->ob);
				group_replaces_nla(parent, go->ob, 'e');
				
				/* leave recalc tags in case group members are in normal scene */
				go->ob->recalc= go->recalc;
			}
		}
		
		/* restore */
		scene->r.cfra= cfrao;
	}
	else
#endif
	{
		/* only do existing tags, as set by regular depsgraph */
		for (go= group->gobject.first; go; go= go->next) {
			if (go->ob) {
				if (go->ob->recalc) {
					object_handle_update(scene, go->ob);
				}
			}
		}
	}
}

#if 0
Object *group_get_member_with_action(Group *group, bAction *act)
{
	GroupObject *go;
	
	if (group==NULL || act==NULL) return NULL;
	
	for (go= group->gobject.first; go; go= go->next) {
		if (go->ob) {
			if (go->ob->action==act)
				return go->ob;
			if (go->ob->nlastrips.first) {
				bActionStrip *strip;
				
				for (strip= go->ob->nlastrips.first; strip; strip= strip->next) {
					if (strip->act==act)
						return go->ob;
				}
			}
		}
	}
	return NULL;
}

/* if group has NLA, we try to map the used objects in NLA to group members */
/* this assuming that object has received a new group link */
void group_relink_nla_objects(Object *ob)
{
	Group *group;
	GroupObject *go;
	bActionStrip *strip;
	
	if (ob==NULL || ob->dup_group==NULL) return;
	group= ob->dup_group;
	
	for (strip= ob->nlastrips.first; strip; strip= strip->next) {
		if (strip->object) {
			for (go= group->gobject.first; go; go= go->next) {
				if (go->ob) {
					if (strcmp(go->ob->id.name, strip->object->id.name)==0)
						break;
				}
			}
			if (go)
				strip->object= go->ob;
			else
				strip->object= NULL;
		}
			
	}
}

#endif
