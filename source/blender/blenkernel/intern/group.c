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
#include "DNA_scene_types.h"
#include "DNA_particle_types.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"


#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_group.h"
#include "BKE_icons.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_scene.h" /* BKE_scene_base_find */

static void free_group_object(GroupObject *go)
{
	MEM_freeN(go);
}

/** Free (or release) any data used by this group (does not free the group itself). */
void BKE_group_free(Group *group)
{
	/* don't free group itself */
	GroupObject *go;

	/* No animdata here. */

	while ((go = BLI_pophead(&group->gobject))) {
		free_group_object(go);
	}

	BKE_previewimg_free(&group->preview);
}

Group *BKE_group_add(Main *bmain, const char *name)
{
	Group *group;
	
	group = BKE_libblock_alloc(bmain, ID_GR, name, 0);
	id_us_min(&group->id);
	id_us_ensure_real(&group->id);
	group->layer = (1 << 20) - 1;

	group->preview = NULL;

	return group;
}

/**
 * Only copy internal data of Group ID from source to already allocated/initialized destination.
 * You probably nerver want to use that directly, use id_copy or BKE_id_copy_ex for typical needs.
 *
 * WARNING! This function will not handle ID user count!
 *
 * \param flag  Copying options (see BKE_library.h's LIB_ID_COPY_... flags for more).
 */
void BKE_group_copy_data(Main *UNUSED(bmain), Group *group_dst, const Group *group_src, const int flag)
{
	BLI_duplicatelist(&group_dst->gobject, &group_src->gobject);

	/* Do not copy group's preview (same behavior as for objects). */
	if ((flag & LIB_ID_COPY_NO_PREVIEW) == 0 && false) {  /* XXX TODO temp hack */
		BKE_previewimg_id_copy(&group_dst->id, &group_src->id);
	}
	else {
		group_dst->preview = NULL;
	}
}

Group *BKE_group_copy(Main *bmain, const Group *group)
{
	Group *group_copy;
	BKE_id_copy_ex(bmain, &group->id, (ID **)&group_copy, 0, false);
	return group_copy;
}

void BKE_group_make_local(Main *bmain, Group *group, const bool lib_local)
{
	BKE_id_make_local_generic(bmain, &group->id, true, lib_local);
}

/* external */
static bool group_object_add_internal(Group *group, Object *ob)
{
	GroupObject *go;
	
	if (group == NULL || ob == NULL) {
		return false;
	}
	
	/* check if the object has been added already */
	if (BLI_findptr(&group->gobject, ob, offsetof(GroupObject, ob))) {
		return false;
	}
	
	go = MEM_callocN(sizeof(GroupObject), "groupobject");
	BLI_addtail(&group->gobject, go);
	
	go->ob = ob;
	id_us_ensure_real(&go->ob->id);
	
	return true;
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

static bool group_object_cyclic_check_internal(Object *object, Group *group)
{
	if (object->dup_group) {
		Group *dup_group = object->dup_group;
		if ((dup_group->id.tag & LIB_TAG_DOIT) == 0) {
			/* Cycle already exists in groups, let's prevent further crappyness */
			return true;
		}
		/* flag the object to identify cyclic dependencies in further dupli groups */
		dup_group->id.tag &= ~LIB_TAG_DOIT;

		if (dup_group == group)
			return true;
		else {
			GroupObject *gob;
			for (gob = dup_group->gobject.first; gob; gob = gob->next) {
				if (group_object_cyclic_check_internal(gob->ob, group)) {
					return true;
				}
			}
		}

		/* un-flag the object, it's allowed to have the same group multiple times in parallel */
		dup_group->id.tag |= LIB_TAG_DOIT;
	}

	return false;
}

bool BKE_group_object_cyclic_check(Main *bmain, Object *object, Group *group)
{
	/* first flag all groups */
	BKE_main_id_tag_listbase(&bmain->group, LIB_TAG_DOIT, true);

	return group_object_cyclic_check_internal(object, group);
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
	static bool done = false;
	bActionStrip *strip, *nstrip;
	
	if (mode == 's') {
		
		for (strip = parent->nlastrips.first; strip; strip = strip->next) {
			if (strip->object == target) {
				if (done == 0) {
					/* clear nla & action from object */
					nlastrips = target->nlastrips;
					BLI_listbase_clear(&target->nlastrips);
					action = target->action;
					target->action = NULL;
					target->nlaflag |= OB_NLA_OVERRIDE;
					done = true;
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
			
			BLI_listbase_clear(&nlastrips);  /* not needed, but yah... :) */
			action = NULL;
			done = false;
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
