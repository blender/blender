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
 * The Original Code is Copyright (C) Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/particle_child.c
 *  \ingroup bke
 */

#include "BLI_math.h"

#include "DNA_material_types.h"

#include "BKE_particle.h"

struct Material;

extern void do_child_modifiers(ParticleSimulationData *sim, ParticleTexture *ptex, ParticleKey *par, float *par_rot, ChildParticle *cpa, const float orco[3], float mat[4][4], ParticleKey *state, float t);

static void get_strand_normal(Material *ma, const float surfnor[3], float surfdist, float nor[3])
{
	float cross[3], nstrand[3], vnor[3], blend;

	if (!((ma->mode & MA_STR_SURFDIFF) || (ma->strand_surfnor > 0.0f)))
		return;

	if (ma->mode & MA_STR_SURFDIFF) {
		cross_v3_v3v3(cross, surfnor, nor);
		cross_v3_v3v3(nstrand, nor, cross);

		blend = dot_v3v3(nstrand, surfnor);
		CLAMP(blend, 0.0f, 1.0f);

		interp_v3_v3v3(vnor, nstrand, surfnor, blend);
		normalize_v3(vnor);
	}
	else {
		copy_v3_v3(vnor, nor);
	}
	
	if (ma->strand_surfnor > 0.0f) {
		if (ma->strand_surfnor > surfdist) {
			blend = (ma->strand_surfnor - surfdist) / ma->strand_surfnor;
			interp_v3_v3v3(vnor, vnor, surfnor, blend);
			normalize_v3(vnor);
		}
	}

	copy_v3_v3(nor, vnor);
}

/* ------------------------------------------------------------------------- */

typedef struct ParticlePathIterator {
	ParticleCacheKey *key;
	int index;
	float time;
	
	ParticleCacheKey *parent_key;
	float parent_rotation[4];
} ParticlePathIterator;

static void psys_path_iter_get(ParticlePathIterator *iter, ParticleCacheKey *keys, int totkeys, ParticleCacheKey *parent, int index)
{
	BLI_assert(index >= 0 && index < totkeys);
	
	iter->key = keys + index;
	iter->index = index;
	iter->time = (float)index / (float)(totkeys - 1);
	
	if (parent) {
		iter->parent_key = parent + index;
		if (index > 0)
			mul_qt_qtqt(iter->parent_rotation, iter->parent_key->rot, parent->rot);
		else
			copy_qt_qt(iter->parent_rotation, parent->rot);
	}
	else {
		iter->parent_key = NULL;
		unit_qt(iter->parent_rotation);
	}
}

typedef struct ParticlePathModifier {
	struct ParticlePathModifier *next, *prev;
	
	void (*apply)(ParticleCacheKey *keys, int totkeys, ParticleCacheKey *parent_keys);
} ParticlePathModifier;

/* ------------------------------------------------------------------------- */

static bool check_path_length(int k, ParticleCacheKey *keys, ParticleCacheKey *key, float max_length, float step_length, float *cur_length, float dvec[3])
{
	if (*cur_length + step_length > max_length) {
		sub_v3_v3v3(dvec, key->co, (key-1)->co);
		mul_v3_fl(dvec, (max_length - *cur_length) / step_length);
		add_v3_v3v3(key->co, (key-1)->co, dvec);
		keys->steps = k;
		/* something over the maximum step value */
		return false;
	}
	else {
		*cur_length += step_length;
		return true;
	}
}

void psys_apply_child_modifiers(ParticleThreadContext *ctx, struct ListBase *modifiers,
                                ChildParticle *cpa, ParticleTexture *ptex, const float orco[3], const float ornor[3], float hairmat[4][4],
                                ParticleCacheKey *keys, ParticleCacheKey *parent_keys)
{
	struct ParticleSettings *part = ctx->sim.psys->part;
	struct Material *ma = ctx->ma;
	const bool draw_col_ma = (part->draw_col == PART_DRAW_COL_MAT);
	
	const int totkeys = ctx->steps + 1;
	const float step_length = 1.0f / (float)ctx->steps;
	const float max_length = ptex->length;
	
	ParticlePathModifier *mod;
	ParticleCacheKey *key;
	int k;
	float cur_length;
	
#if 0 /* TODO for the future: use true particle modifiers that work on the whole curve */
	for (mod = modifiers->first; mod; mod = mod->next) {
		mod->apply(keys, totkeys, parent_keys);
	}
#else
	(void)modifiers;
	(void)mod;
	
	{
		ParticlePathIterator iter;
		for (k = 0, key = keys; k < totkeys; k++, key++) {
			psys_path_iter_get(&iter, keys, totkeys, parent_keys, k);
			
			/* apply different deformations to the child path */
			do_child_modifiers(&ctx->sim, ptex, (ParticleKey *)iter.parent_key, iter.parent_rotation, cpa, orco, hairmat, (ParticleKey *)key, iter.time);
		}
	}
#endif
	
	cur_length = 0.0f;
	/* we have to correct velocity because of kink & clump */
	for (k = 0, key = keys; k < totkeys; ++k, ++key) {
		if (k >= 2) {
			sub_v3_v3v3((key-1)->vel, key->co, (key-2)->co);
			mul_v3_fl((key-1)->vel, 0.5);
			
			if (ma && draw_col_ma)
				get_strand_normal(ma, ornor, cur_length, (key-1)->vel);
		}
		else if (k == totkeys-1) {
			/* last key */
			sub_v3_v3v3(key->vel, key->co, (key-1)->co);
		}
		
		if (k > 1) {
			float dvec[3];
			/* check if path needs to be cut before actual end of data points */
			if (!check_path_length(k, keys, key, max_length, step_length, &cur_length, dvec))
				break;
		}
		
		if (ma && draw_col_ma) {
			copy_v3_v3(key->col, &ma->r);
			get_strand_normal(ma, ornor, cur_length, key->vel);
		}
	}
}
