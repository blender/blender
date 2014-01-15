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


#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_group.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_scene.h" /* BKE_scene_base_find */

static void free_group_object(GroupObject *go)
{
	MEM_freeN(go);
}


void BKE_group_free(Group *group)
{
	/* don't free group itself */
	GroupObject *go;
	
	while ((go = BLI_pophead(&group->gobject))) {
		free_group_object(go);
	}
}

void BKE_group_unlink(Group *group)
{
	Main *bmain = G.main;
	Material *ma;
	Object *ob;
	Scene *sce;
	SceneRenderLayer *srl;
	ParticleSystem *psys;
	
	for (ma = bmain->mat.first; ma; ma = ma->id.next) {
		if (ma->group == group)
			ma->group = NULL;
	}
	for (ma = bmain->mat.first; ma; ma = ma->id.next) {
		if (ma->group == group)
			ma->group = NULL;
	}
	for (sce = bmain->scene.first; sce; sce = sce->id.next) {
		Base *base = sce->base.first;
		
		/* ensure objects are not in this group */
		for (; base; base = base->next) {
			if (BKE_group_object_unlink(group, base->object, sce, base) &&
			    BKE_group_object_find(NULL, base->object) == NULL)
			{
				base->object->flag &= ~OB_FROMGROUP;
				base->flag &= ~OB_FROMGROUP;
			}
		}
		
		for (srl = sce->r.layers.first; srl; srl = srl->next) {
			FreestyleLineSet *lineset;

			if (srl->light_override == group)
				srl->light_override = NULL;
			for (lineset = srl->freestyleConfig.linesets.first; lineset; lineset = lineset->next) {
				if (lineset->group == group)
					lineset->group = NULL;
			}
		}
	}
	
	for (ob = bmain->object.first; ob; ob = ob->id.next) {
		
		if (ob->dup_group == group) {
			ob->dup_group = NULL;
		}
		
		for (psys = ob->particlesystem.first; psys; psys = psys->next) {
			if (psys->part->dup_group == group)
				psys->part->dup_group = NULL;
#if 0       /* not used anymore, only keps for readfile.c, no need to account for this */
			if (psys->part->eff_group == group)
				psys->part->eff_group = NULL;
#endif
		}
	}
	
	/* group stays in library, but no members */
	BKE_group_free(group);
	group->id.us = 0;
}

Group *BKE_group_add(Main *bmain, const char *name)
{
	Group *group;
	
	group = BKE_libblock_alloc(bmain, ID_GR, name);
	group->layer = (1 << 20) - 1;
	return group;
}

Group *BKE_group_copy(Group *group)
{
	Group *groupn;

	groupn = BKE_libblock_copy(&group->id);
	BLI_duplicatelist(&groupn->gobject, &group->gobject);

	return groupn;
}

/* external */
static int group_object_add_internal(Group *group, Object *ob)
{
	GroupObject *go;
	
	if (group == NULL || ob == NULL) {
		return FALSE;
	}
	
	/* check if the object has been added already */
	if (BLI_findptr(&group->gobject, ob, offsetof(GroupObject, ob))) {
		return FALSE;
	}
	
	go = MEM_callocN(sizeof(GroupObject), "groupobject");
	BLI_addtail(&group->gobject, go);
	
	go->ob = ob;
	
	return TRUE;
}

bool BKE_group_object_add(Group *group, Object *object, Scene *scene, Base *base)
{
	if (group_object_add_internal(group, object)) {
		if ((object->flag & OB_FROMGROUP) == 0) {

			if (scene && base == NULL)
				base = BKE_scene_base_find(scene, object);

			object->flag |= OB_FROMGROUP;

			if (base)
				base->flag |= OB_FROMGROUP;
		}
		return true;
	}
	else {
		return false;
	}
}

/* also used for (ob == NULL) */
static int group_object_unlink_internal(Group *group, Object *ob)
{
	GroupObject *go, *gon;
	int removed = 0;
	if (group == NULL) return 0;
	
	go = group->gobject.first;
	while (go) {
		gon = go->next;
		if (go->ob == ob) {
			BLI_remlink(&group->gobject, go);
			free_group_object(go);
			removed = 1;
			/* should break here since an object being in a group twice cant happen? */
		}
		go = gon;
	}
	return removed;
}

bool BKE_group_object_unlink(Group *group, Object *object, Scene *scene, Base *base)
{
	if (group_object_unlink_internal(group, object)) {
		/* object can be NULL */
		if (object && BKE_group_object_find(NULL, object) == NULL) {
			if (scene && base == NULL)
				base = BKE_scene_base_find(scene, object);

			object->flag &= ~OB_FROMGROUP;

			if (base)
				base->flag &= ~OB_FROMGROUP;
		}
		return true;
	}
	else {
		return false;
	}
}

bool BKE_group_object_exists(Group *group, Object *ob)
{
	if (group == NULL || ob == NULL) {
		return false;
	}
	else {
		return (BLI_findptr(&group->gobject, ob, offsetof(GroupObject, ob)) != NULL);
	}
}

Group *BKE_group_object_find(Group *group, Object *ob)
{
	if (group)
		group = group->id.next;
	else
		group = G.main->group.first;
	
	while (group) {
		if (BKE_group_object_exists(group, ob))
			return group;
		group = group->id.next;
	}
	return NULL;
}

void BKE_group_tag_recalc(Group *group)
{
	GroupObject *go;
	
	if (group == NULL) return;
	
	for (go = group->gobject.first; go; go = go->next) {
		if (go->ob) 
			go->ob->recalc = go->recalc;
	}
}

bool BKE_group_is_animated(Group *group, Object *UNUSED(parent))
{
	GroupObject *go;

#if 0 /* XXX OLD ANIMSYS, NLASTRIPS ARE NO LONGER USED */
	if (parent->nlastrips.first)
		return 1;
#endif

	for (go = group->gobject.first; go; go = go->next)
		if (go->ob && go->ob->proxy)
			return true;

	return false;
}

#if 0 // add back when timeoffset & animsys work again
/* only replaces object strips or action when parent nla instructs it */
/* keep checking nla.c though, in case internal structure of strip changes */
static void group_replaces_nla(Object *parent, Object *target, char mode)
{
	static ListBase nlastrips = {NULL, NULL};
	static bAction *action = NULL;
	static int done = FALSE;
	bActionStrip *strip, *nstrip;
	
	if (mode == 's') {
		
		for (strip = parent->nlastrips.first; strip; strip = strip->next) {
			if (strip->object == target) {
				if (done == 0) {
					/* clear nla & action from object */
					nlastrips = target->nlastrips;
					target->nlastrips.first = target->nlastrips.last = NULL;
					action = target->action;
					target->action = NULL;
					target->nlaflag |= OB_NLA_OVERRIDE;
					done = TRUE;
				}
				nstrip = MEM_dupallocN(strip);
				BLI_addtail(&target->nlastrips, nstrip);
			}
		}
	}
	else if (mode == 'e') {
		if (done) {
			BLI_freelistN(&target->nlastrips);
			target->nlastrips = nlastrips;
			target->action = action;
			
			nlastrips.first = nlastrips.last = NULL;  /* not needed, but yah... :) */
			action = NULL;
			done = FALSE;
		}
	}
}
#endif

/* puts all group members in local timing system, after this call
 * you can draw everything, leaves tags in objects to signal it needs further updating */

/* note: does not work for derivedmesh and render... it recreates all again in convertblender.c */
void BKE_group_handle_recalc_and_update(EvaluationContext *eval_ctx, Scene *scene, Object *UNUSED(parent), Group *group)
{
	GroupObject *go;
	
#if 0 /* warning, isn't clearing the recalc flag on the object which causes it to run all the time,
	   * not just on frame change.
	   * This isn't working because the animation data is only re-evaluated on frame change so commenting for now
	   * but when its enabled at some point it will need to be changed so as not to update so much - campbell */

	/* if animated group... */
	if (parent->nlastrips.first) {
		int cfrao;
		
		/* switch to local time */
		cfrao = scene->r.cfra;
		
		/* we need a DAG per group... */
		for (go = group->gobject.first; go; go = go->next) {
			if (go->ob && go->recalc) {
				go->ob->recalc = go->recalc;
				
				group_replaces_nla(parent, go->ob, 's');
				BKE_object_handle_update(eval_ctx, scene, go->ob);
				group_replaces_nla(parent, go->ob, 'e');
				
				/* leave recalc tags in case group members are in normal scene */
				go->ob->recalc = go->recalc;
			}
		}
		
		/* restore */
		scene->r.cfra = cfrao;
	}
	else
#endif
	{
		/* only do existing tags, as set by regular depsgraph */
		for (go = group->gobject.first; go; go = go->next) {
			if (go->ob) {
				if (go->ob->recalc) {
					BKE_object_handle_update(eval_ctx, scene, go->ob);
				}
			}
		}
	}
}
