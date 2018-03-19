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
 * The Original Code is Copyright (C) 2007 by Janne Karhu.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/physics/particle_edit_undo.c
 *  \ingroup edphys
 */

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <assert.h>

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"

#include "ED_particle.h"

#include "particle_edit_utildefines.h"

#include "physics_intern.h"

static void free_PTCacheUndo(PTCacheUndo *undo)
{
	PTCacheEditPoint *point;
	int i;

	for (i=0, point=undo->points; i<undo->totpoint; i++, point++) {
		if (undo->particles && (undo->particles + i)->hair)
			MEM_freeN((undo->particles + i)->hair);
		if (point->keys)
			MEM_freeN(point->keys);
	}
	if (undo->points)
		MEM_freeN(undo->points);

	if (undo->particles)
		MEM_freeN(undo->particles);

	BKE_ptcache_free_mem(&undo->mem_cache);
}

static void make_PTCacheUndo(PTCacheEdit *edit, PTCacheUndo *undo)
{
	PTCacheEditPoint *point;
	int i;

	undo->totpoint= edit->totpoint;

	if (edit->psys) {
		ParticleData *pa;

		pa= undo->particles= MEM_dupallocN(edit->psys->particles);

		for (i=0; i<edit->totpoint; i++, pa++)
			pa->hair= MEM_dupallocN(pa->hair);

		undo->psys_flag = edit->psys->flag;
	}
	else {
		PTCacheMem *pm;

		BLI_duplicatelist(&undo->mem_cache, &edit->pid.cache->mem_cache);
		pm = undo->mem_cache.first;

		for (; pm; pm=pm->next) {
			for (i=0; i<BPHYS_TOT_DATA; i++)
				pm->data[i] = MEM_dupallocN(pm->data[i]);
		}
	}

	point= undo->points = MEM_dupallocN(edit->points);
	undo->totpoint = edit->totpoint;

	for (i=0; i<edit->totpoint; i++, point++) {
		point->keys= MEM_dupallocN(point->keys);
		/* no need to update edit key->co & key->time pointers here */
	}
}

static void get_PTCacheUndo(PTCacheEdit *edit, PTCacheUndo *undo)
{
	ParticleSystem *psys = edit->psys;
	ParticleData *pa;
	HairKey *hkey;
	POINT_P; KEY_K;

	LOOP_POINTS {
		if (psys && psys->particles[p].hair)
			MEM_freeN(psys->particles[p].hair);

		if (point->keys)
			MEM_freeN(point->keys);
	}
	if (psys && psys->particles)
		MEM_freeN(psys->particles);
	if (edit->points)
		MEM_freeN(edit->points);
	if (edit->mirror_cache) {
		MEM_freeN(edit->mirror_cache);
		edit->mirror_cache= NULL;
	}

	edit->points= MEM_dupallocN(undo->points);
	edit->totpoint = undo->totpoint;

	LOOP_POINTS {
		point->keys= MEM_dupallocN(point->keys);
	}

	if (psys) {
		psys->particles= MEM_dupallocN(undo->particles);

		psys->totpart= undo->totpoint;

		LOOP_POINTS {
			pa = psys->particles + p;
			hkey= pa->hair = MEM_dupallocN(pa->hair);

			LOOP_KEYS {
				key->co= hkey->co;
				key->time= &hkey->time;
				hkey++;
			}
		}

		psys->flag = undo->psys_flag;
	}
	else {
		PTCacheMem *pm;
		int i;

		BKE_ptcache_free_mem(&edit->pid.cache->mem_cache);

		BLI_duplicatelist(&edit->pid.cache->mem_cache, &undo->mem_cache);

		pm = edit->pid.cache->mem_cache.first;

		for (; pm; pm=pm->next) {
			for (i=0; i<BPHYS_TOT_DATA; i++)
				pm->data[i] = MEM_dupallocN(pm->data[i]);

			BKE_ptcache_mem_pointers_init(pm);

			LOOP_POINTS {
				LOOP_KEYS {
					if ((int)key->ftime == (int)pm->frame) {
						key->co = pm->cur[BPHYS_DATA_LOCATION];
						key->vel = pm->cur[BPHYS_DATA_VELOCITY];
						key->rot = pm->cur[BPHYS_DATA_ROTATION];
						key->time = &key->ftime;
					}
				}
				BKE_ptcache_mem_pointers_incr(pm);
			}
		}
	}
}

void PE_undo_push(Scene *scene, const char *str)
{
	PTCacheEdit *edit= PE_get_current(scene, OBACT);
	PTCacheUndo *undo;
	int nr;

	if (!edit) return;

	/* remove all undos after (also when curundo==NULL) */
	while (edit->undo.last != edit->curundo) {
		undo= edit->undo.last;
		BLI_remlink(&edit->undo, undo);
		free_PTCacheUndo(undo);
		MEM_freeN(undo);
	}

	/* make new */
	edit->curundo= undo= MEM_callocN(sizeof(PTCacheUndo), "particle undo file");
	BLI_strncpy(undo->name, str, sizeof(undo->name));
	BLI_addtail(&edit->undo, undo);
	
	/* and limit amount to the maximum */
	nr= 0;
	undo= edit->undo.last;
	while (undo) {
		nr++;
		if (nr==U.undosteps) break;
		undo= undo->prev;
	}
	if (undo) {
		while (edit->undo.first != undo) {
			PTCacheUndo *first= edit->undo.first;
			BLI_remlink(&edit->undo, first);
			free_PTCacheUndo(first);
			MEM_freeN(first);
		}
	}

	/* copy  */
	make_PTCacheUndo(edit, edit->curundo);
}

void PE_undo_step(Scene *scene, int step)
{	
	PTCacheEdit *edit= PE_get_current(scene, OBACT);

	if (!edit) return;

	if (step==0) {
		get_PTCacheUndo(edit, edit->curundo);
	}
	else if (step==1) {
		
		if (edit->curundo==NULL || edit->curundo->prev==NULL) {
			/* pass */
		}
		else {
			if (G.debug & G_DEBUG) printf("undo %s\n", edit->curundo->name);
			edit->curundo= edit->curundo->prev;
			get_PTCacheUndo(edit, edit->curundo);
		}
	}
	else {
		/* curundo has to remain current situation! */
		
		if (edit->curundo==NULL || edit->curundo->next==NULL) {
			/* pass */
		}
		else {
			get_PTCacheUndo(edit, edit->curundo->next);
			edit->curundo= edit->curundo->next;
			if (G.debug & G_DEBUG) printf("redo %s\n", edit->curundo->name);
		}
	}

	DAG_id_tag_update(&OBACT->id, OB_RECALC_DATA);
}

bool PE_undo_is_valid(Scene *scene)
{
	PTCacheEdit *edit= PE_get_current(scene, OBACT);
	
	if (edit) {
		return (edit->undo.last != edit->undo.first);
	}
	return 0;
}

void PTCacheUndo_clear(PTCacheEdit *edit)
{
	PTCacheUndo *undo;

	if (edit==NULL) return;
	
	undo= edit->undo.first;
	while (undo) {
		free_PTCacheUndo(undo);
		undo= undo->next;
	}
	BLI_freelistN(&edit->undo);
	edit->curundo= NULL;
}

void PE_undo(Scene *scene)
{
	PE_undo_step(scene, 1);
}

void PE_redo(Scene *scene)
{
	PE_undo_step(scene, -1);
}

void PE_undo_number(Scene *scene, int nr)
{
	PTCacheEdit *edit= PE_get_current(scene, OBACT);
	PTCacheUndo *undo;
	int a=0;
	
	for (undo= edit->undo.first; undo; undo= undo->next, a++) {
		if (a==nr) break;
	}
	edit->curundo= undo;
	PE_undo_step(scene, 0);
}


/* get name of undo item, return null if no item with this index */
/* if active pointer, set it to 1 if true */
const char *PE_undo_get_name(Scene *scene, int nr, bool *r_active)
{
	PTCacheEdit *edit= PE_get_current(scene, OBACT);
	PTCacheUndo *undo;
	
	if (r_active) *r_active = false;
	
	if (edit) {
		undo= BLI_findlink(&edit->undo, nr);
		if (undo) {
			if (r_active && (undo == edit->curundo)) {
				*r_active = true;
			}
			return undo->name;
		}
	}
	return NULL;
}
