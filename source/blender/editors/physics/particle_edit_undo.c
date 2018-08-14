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
#include "DNA_windowmanager_types.h"

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_main.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_undo_system.h"

#include "ED_object.h"
#include "ED_particle.h"
#include "ED_physics.h"

#include "particle_edit_utildefines.h"

#include "physics_intern.h"

/* -------------------------------------------------------------------- */
/** \name Undo Conversion
 * \{ */

static void undoptcache_from_editcache(PTCacheUndo *undo, PTCacheEdit *edit)
{
	PTCacheEditPoint *point;
	int i;

	size_t mem_used_prev = MEM_get_memory_in_use();

	undo->totpoint = edit->totpoint;

	if (edit->psys) {
		ParticleData *pa;

		pa = undo->particles = MEM_dupallocN(edit->psys->particles);

		for (i = 0; i < edit->totpoint; i++, pa++) {
			pa->hair = MEM_dupallocN(pa->hair);
		}

		undo->psys_flag = edit->psys->flag;
	}
	else {
		PTCacheMem *pm;

		BLI_duplicatelist(&undo->mem_cache, &edit->pid.cache->mem_cache);
		pm = undo->mem_cache.first;

		for (; pm; pm = pm->next) {
			for (i = 0; i < BPHYS_TOT_DATA; i++) {
				pm->data[i] = MEM_dupallocN(pm->data[i]);
			}
		}
	}

	point = undo->points = MEM_dupallocN(edit->points);
	undo->totpoint = edit->totpoint;

	for (i = 0; i < edit->totpoint; i++, point++) {
		point->keys = MEM_dupallocN(point->keys);
		/* no need to update edit key->co & key->time pointers here */
	}

	size_t mem_used_curr = MEM_get_memory_in_use();

	undo->undo_size = mem_used_prev < mem_used_curr ? mem_used_curr - mem_used_prev : sizeof(PTCacheUndo);
}

static void undoptcache_to_editcache(PTCacheUndo *undo, PTCacheEdit *edit)
{
	ParticleSystem *psys = edit->psys;
	ParticleData *pa;
	HairKey *hkey;
	POINT_P; KEY_K;

	LOOP_POINTS {
		if (psys && psys->particles[p].hair) {
			MEM_freeN(psys->particles[p].hair);
		}

		if (point->keys) {
			MEM_freeN(point->keys);
		}
	}
	if (psys && psys->particles) {
		MEM_freeN(psys->particles);
	}
	if (edit->points) {
		MEM_freeN(edit->points);
	}
	if (edit->mirror_cache) {
		MEM_freeN(edit->mirror_cache);
		edit->mirror_cache = NULL;
	}

	edit->points = MEM_dupallocN(undo->points);
	edit->totpoint = undo->totpoint;

	LOOP_POINTS {
		point->keys = MEM_dupallocN(point->keys);
	}

	if (psys) {
		psys->particles = MEM_dupallocN(undo->particles);

		psys->totpart = undo->totpoint;

		LOOP_POINTS {
			pa = psys->particles + p;
			hkey = pa->hair = MEM_dupallocN(pa->hair);

			LOOP_KEYS {
				key->co = hkey->co;
				key->time = &hkey->time;
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

		for (; pm; pm = pm->next) {
			for (i = 0; i < BPHYS_TOT_DATA; i++) {
				pm->data[i] = MEM_dupallocN(pm->data[i]);
			}
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

static void undoptcache_free_data(PTCacheUndo *undo)
{
	PTCacheEditPoint *point;
	int i;

	for (i = 0, point = undo->points; i < undo->totpoint; i++, point++) {
		if (undo->particles && (undo->particles + i)->hair) {
			MEM_freeN((undo->particles + i)->hair);
		}
		if (point->keys) {
			MEM_freeN(point->keys);
		}
	}
	if (undo->points) {
		MEM_freeN(undo->points);
	}
	if (undo->particles) {
		MEM_freeN(undo->particles);
	}
	BKE_ptcache_free_mem(&undo->mem_cache);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Implements ED Undo System
 * \{ */

typedef struct ParticleUndoStep {
	UndoStep step;
	UndoRefID_Scene scene_ref;
	UndoRefID_Object object_ref;
	PTCacheUndo data;
} ParticleUndoStep;

static bool particle_undosys_poll(struct bContext *C)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	Object *ob = OBACT;
	PTCacheEdit *edit = PE_get_current(bmain, scene, ob);
	return (edit != NULL);
}

static bool particle_undosys_step_encode(struct bContext *C, UndoStep *us_p)
{
	Main *bmain = CTX_data_main(C);
	ParticleUndoStep *us = (ParticleUndoStep *)us_p;
	us->scene_ref.ptr = CTX_data_scene(C);
	us->object_ref.ptr = us->scene_ref.ptr->basact->object;
	PTCacheEdit *edit = PE_get_current(bmain, us->scene_ref.ptr, us->object_ref.ptr);
	undoptcache_from_editcache(&us->data, edit);
	return true;
}

static void particle_undosys_step_decode(struct bContext *C, UndoStep *us_p, int UNUSED(dir))
{
	Main *bmain = CTX_data_main(C);
	/* TODO(campbell): undo_system: use low-level API to set mode. */
	ED_object_mode_set(C, OB_MODE_PARTICLE_EDIT);
	BLI_assert(particle_undosys_poll(C));

	ParticleUndoStep *us = (ParticleUndoStep *)us_p;
	Scene *scene = us->scene_ref.ptr;
	Object *ob = us->object_ref.ptr;
	PTCacheEdit *edit = PE_get_current(bmain, scene, ob);
	if (edit) {
		undoptcache_to_editcache(&us->data, edit);
		DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	}
	else {
		BLI_assert(0);
	}
}

static void particle_undosys_step_free(UndoStep *us_p)
{
	ParticleUndoStep *us = (ParticleUndoStep *)us_p;
	undoptcache_free_data(&us->data);
}

static void particle_undosys_foreach_ID_ref(
        UndoStep *us_p, UndoTypeForEachIDRefFn foreach_ID_ref_fn, void *user_data)
{
	ParticleUndoStep *us = (ParticleUndoStep *)us_p;
	foreach_ID_ref_fn(user_data, ((UndoRefID *)&us->scene_ref));
	foreach_ID_ref_fn(user_data, ((UndoRefID *)&us->object_ref));
}

/* Export for ED_undo_sys. */
void ED_particle_undosys_type(UndoType *ut)
{
	ut->name = "Edit Particle";
	ut->poll = particle_undosys_poll;
	ut->step_encode = particle_undosys_step_encode;
	ut->step_decode = particle_undosys_step_decode;
	ut->step_free = particle_undosys_step_free;

	ut->step_foreach_ID_ref = particle_undosys_foreach_ID_ref;

	ut->mode = BKE_UNDOTYPE_MODE_STORE;
	ut->use_context = true;

	ut->step_size = sizeof(ParticleUndoStep);
}

/** \} */
