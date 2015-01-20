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
#ifndef __BKE_EFFECT_H__
#define __BKE_EFFECT_H__

/** \file BKE_effect.h
 *  \ingroup bke
 *  \since March 2001
 *  \author nzc
 */

#include "DNA_modifier_types.h"

#include "BLI_utildefines.h"

struct Object;
struct Scene;
struct Effect;
struct ListBase;
struct Particle;
struct Group;
struct RNG;
struct ParticleSimulationData;
struct ParticleData;
struct ParticleKey;

struct EffectorWeights *BKE_add_effector_weights(struct Group *group);
struct PartDeflect *object_add_collision_fields(int type);

/* Input to effector code */
typedef struct EffectedPoint {
	float *loc;
	float *vel;
	float *ave;  /* angular velocity for particles with dynamic rotation */
	float *rot; /* rotation quaternion for particles with dynamic rotation */
	float vel_to_frame;
	float vel_to_sec;

	/* only for particles */
	float size, charge;

	unsigned int flag;
	int index;

	struct ParticleSystem *psys;  /* particle system the point belongs to */
} EffectedPoint;

typedef struct GuideEffectorData {
	float vec_to_point[3];
	float strength;
} GuideEffectorData;

typedef struct EffectorData {
	/* Effector point */
	float loc[3];
	float nor[3];
	float vel[3];

	float vec_to_point[3];
	float distance, falloff;

	/* only for effector particles */
	float size, charge;

	/* only for vortex effector with surface falloff */
	float nor2[3], vec_to_point2[3];

	int *index; /* point index */
} EffectorData;

/* used for calculating the effector force */
typedef struct EffectorCache {
	struct EffectorCache *next, *prev;

	struct Scene *scene;
	struct Object *ob;
	struct ParticleSystem *psys;
	struct SurfaceModifierData *surmd;
	
	struct PartDeflect *pd;

	/* precalculated for guides */
	struct GuideEffectorData *guide_data;
	float guide_loc[4], guide_dir[3], guide_radius;
	float velocity[3];

	float frame;
	int flag;
} EffectorCache;

void            free_partdeflect(struct PartDeflect *pd);
struct ListBase *pdInitEffectors(struct Scene *scene, struct Object *ob_src, struct ParticleSystem *psys_src, struct EffectorWeights *weights, bool precalc);
void            pdEndEffectors(struct ListBase **effectors);
void            pdPrecalculateEffectors(struct ListBase *effectors);
void            pdDoEffectors(struct ListBase *effectors, struct ListBase *colliders, struct EffectorWeights *weights, struct EffectedPoint *point, float *force, float *impulse);

void pd_point_from_particle(struct ParticleSimulationData *sim, struct ParticleData *pa, struct ParticleKey *state, struct EffectedPoint *point);
void pd_point_from_loc(struct Scene *scene, float *loc, float *vel, int index, struct EffectedPoint *point);
void pd_point_from_soft(struct Scene *scene, float *loc, float *vel, int index, struct EffectedPoint *point);

/* needed for boids */
float effector_falloff(struct EffectorCache *eff, struct EffectorData *efd, struct EffectedPoint *point, struct EffectorWeights *weights);
int closest_point_on_surface(SurfaceModifierData *surmd, const float co[3], float surface_co[3], float surface_nor[3], float surface_vel[3]);
int get_effector_data(struct EffectorCache *eff, struct EffectorData *efd, struct EffectedPoint *point, int real_velocity);

/* required for particle_system.c */
//void do_physical_effector(struct EffectorData *eff, struct EffectorPoint *point, float *total_force);
//float effector_falloff(struct EffectorData *eff, struct EffectorPoint *point, struct EffectorWeights *weights);

/* EffectedPoint->flag */
#define PE_WIND_AS_SPEED        1
#define PE_DYNAMIC_ROTATION     2
#define PE_USE_NORMAL_DATA      4

/* EffectorData->flag */
#define PE_VELOCITY_TO_IMPULSE  1

/* ======== Simulation Debugging ======== */

#define SIM_DEBUG_HASH_BASE 5381

unsigned int BKE_sim_debug_data_hash(int i);
unsigned int BKE_sim_debug_data_hash_combine(unsigned int kx, unsigned int ky);

/* _VA_SIM_DEBUG_HASH#(i, ...): combined hash value of multiple integers */
/* internal helpers*/
#define _VA_SIM_DEBUG_HASH1(a) \
       (BKE_sim_debug_data_hash(a))
#define _VA_SIM_DEBUG_HASH2(a, b) \
       (BKE_sim_debug_data_hash_combine(BKE_sim_debug_data_hash(a), _VA_SIM_DEBUG_HASH1(b)))
#define _VA_SIM_DEBUG_HASH3(a, b, c) \
       (BKE_sim_debug_data_hash_combine(BKE_sim_debug_data_hash(a), _VA_SIM_DEBUG_HASH2(b, c)))
#define _VA_SIM_DEBUG_HASH4(a, b, c, d) \
       (BKE_sim_debug_data_hash_combine(BKE_sim_debug_data_hash(a), _VA_SIM_DEBUG_HASH3(b, c, d)))
#define _VA_SIM_DEBUG_HASH5(a, b, c, d, e) \
       (BKE_sim_debug_data_hash_combine(BKE_sim_debug_data_hash(a), _VA_SIM_DEBUG_HASH4(b, c, d, e)))
#define _VA_SIM_DEBUG_HASH6(a, b, c, d, e, f) \
       (BKE_sim_debug_data_hash_combine(BKE_sim_debug_data_hash(a), _VA_SIM_DEBUG_HASH5(b, c, d, e, f)))
#define _VA_SIM_DEBUG_HASH7(a, b, c, d, e, f, g) \
       (BKE_sim_debug_data_hash_combine(BKE_sim_debug_data_hash(a), _VA_SIM_DEBUG_HASH6(b, c, d, e, f, g)))
#define _VA_SIM_DEBUG_HASH8(a, b, c, d, e, f, g, h) \
       (BKE_sim_debug_data_hash_combine(BKE_sim_debug_data_hash(a), _VA_SIM_DEBUG_HASH7(b, c, d, e, f, g, h)))

#define SIM_DEBUG_HASH(...) VA_NARGS_CALL_OVERLOAD(_VA_SIM_DEBUG_HASH, __VA_ARGS__)

typedef struct SimDebugElement {
	unsigned int category_hash;
	unsigned int hash;
	
	int type;
	float color[3];
	
	float v1[3], v2[3];
} SimDebugElement;

typedef enum eSimDebugElement_Type {
	SIM_DEBUG_ELEM_DOT,
	SIM_DEBUG_ELEM_CIRCLE,
	SIM_DEBUG_ELEM_LINE,
	SIM_DEBUG_ELEM_VECTOR,
} eSimDebugElement_Type;

typedef struct SimDebugData {
	struct GHash *gh;
} SimDebugData;

extern SimDebugData *_sim_debug_data;

void BKE_sim_debug_data_set_enabled(bool enable);
bool BKE_sim_debug_data_get_enabled(void);
void BKE_sim_debug_data_free(void);

void BKE_sim_debug_data_add_element(int type, const float v1[3], const float v2[3],
                                    float r, float g, float b, const char *category, unsigned int hash);
void BKE_sim_debug_data_remove_element(unsigned int hash);

#define BKE_sim_debug_data_add_dot(p, r, g, b, category, ...) { \
	const float v2[3] = { 0.0f, 0.0f, 0.0f }; \
	BKE_sim_debug_data_add_element(SIM_DEBUG_ELEM_DOT, p, v2, r, g, b, category, SIM_DEBUG_HASH(__VA_ARGS__)); \
}

#define BKE_sim_debug_data_add_circle(p, radius, r, g, b, category, ...) { \
	const float v2[3] = { radius, 0.0f, 0.0f }; \
	BKE_sim_debug_data_add_element(SIM_DEBUG_ELEM_CIRCLE, p, v2, r, g, b, category, SIM_DEBUG_HASH(__VA_ARGS__)); \
}

#define BKE_sim_debug_data_add_line(p1, p2, r, g, b, category, ...) { \
	BKE_sim_debug_data_add_element(SIM_DEBUG_ELEM_LINE, p1, p2, r, g, b, category, SIM_DEBUG_HASH(__VA_ARGS__)); \
}

#define BKE_sim_debug_data_add_vector(p, d, r, g, b, category, ...) { \
	BKE_sim_debug_data_add_element(SIM_DEBUG_ELEM_VECTOR, p, d, r, g, b, category, SIM_DEBUG_HASH(__VA_ARGS__)); \
}

#define BKE_sim_debug_data_remove(...) \
	BKE_sim_debug_data_remove_element(SIM_DEBUG_HASH(__VA_ARGS__))

void BKE_sim_debug_data_clear(void);
void BKE_sim_debug_data_clear_category(const char *category);

#endif

