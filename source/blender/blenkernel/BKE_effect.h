/**
 * blenlib/BKE_effect.h (mar-2001 nzc)
 *	
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
#ifndef BKE_EFFECT_H
#define BKE_EFFECT_H

#include "DNA_object_types.h"

struct Effect;
struct ListBase;
struct Particle;
struct Group;
struct RNG;

typedef struct pEffectorCache {
	struct pEffectorCache *next, *prev;
	Object *ob;
	
	/* precalculated variables */
	float oldloc[3], oldspeed[3];
	float scale, time_scale;
	float guide_dist;
	
	Object obcopy;	/* for restoring transformation data */
} pEffectorCache;

void free_effect(struct Effect *eff);
void free_effects(struct ListBase *lb);
struct Effect *copy_effect(struct Effect *eff);
void copy_effects(struct ListBase *lbn, struct ListBase *lb);
void deselectall_eff(struct Object *ob);

/* particle deflector */
#define PE_WIND_AS_SPEED 0x00000001

struct PartEff *give_parteff(struct Object *ob);
struct ListBase *pdInitEffectors(struct Object *obsrc, struct Group *group);
void			pdEndEffectors(struct ListBase *lb);
void			pdDoEffectors(struct ListBase *lb, float *opco, float *force, float *speed, float cur_time, float loc_time, unsigned int flags);

/* required for particle_system.c */
void do_physical_effector(short type, float force_val, float distance, float falloff, float size, float damp, float *eff_velocity, float *vec_to_part, float *velocity, float *field, int planar, struct RNG *rng, float noise_factor);
float effector_falloff(struct PartDeflect *pd, float *eff_velocity, float *vec_to_part);




#endif

