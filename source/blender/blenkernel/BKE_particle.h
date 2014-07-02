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
 * Adaptive time step
 * Classical SPH
 * Copyright 2011-2012 AutoCRC
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BKE_PARTICLE_H__
#define __BKE_PARTICLE_H__

/** \file BKE_particle.h
 *  \ingroup bke
 */

#include "BLI_utildefines.h"

#include "DNA_particle_types.h"
#include "DNA_object_types.h"

struct ParticleSystemModifierData;
struct ParticleSystem;
struct ParticleKey;
struct ParticleSettings;
struct HairKey;

struct Main;
struct Group;
struct Object;
struct Scene;
struct DerivedMesh;
struct ModifierData;
struct MTFace;
struct MCol;
struct MFace;
struct MVert;
struct IpoCurve;
struct LatticeDeformData;
struct LinkNode;
struct KDTree;
struct RNG;
struct SurfaceModifierData;
struct BVHTreeRay;
struct BVHTreeRayHit; 
struct EdgeHash;

#define PARTICLE_P              ParticleData * pa; int p
#define LOOP_PARTICLES  for (p = 0, pa = psys->particles; p < psys->totpart; p++, pa++)
#define LOOP_EXISTING_PARTICLES for (p = 0, pa = psys->particles; p < psys->totpart; p++, pa++) if (!(pa->flag & PARS_UNEXIST))
#define LOOP_SHOWN_PARTICLES for (p = 0, pa = psys->particles; p < psys->totpart; p++, pa++) if (!(pa->flag & (PARS_UNEXIST | PARS_NO_DISP)))
/* OpenMP: Can only advance one variable within loop definition. */
#define LOOP_DYNAMIC_PARTICLES for (p = 0; p < psys->totpart; p++) if ((pa = psys->particles + p)->state.time > 0.0f)

/* fast but sure way to get the modifier*/
#define PARTICLE_PSMD ParticleSystemModifierData * psmd = sim->psmd ? sim->psmd : psys_get_modifier(sim->ob, sim->psys)

/* common stuff that many particle functions need */
typedef struct ParticleSimulationData {
	struct Scene *scene;
	struct Object *ob;
	struct ParticleSystem *psys;
	struct ParticleSystemModifierData *psmd;
	struct ListBase *colliders;
	/* Courant number. This is used to implement an adaptive time step. Only the
	 * maximum value per time step is important. Only sph_integrate makes use of
	 * this at the moment. Other solvers could, too. */
	float courant_num;
} ParticleSimulationData;

typedef struct SPHData {
	ParticleSystem *psys[10];
	ParticleData *pa;
	float mass;
	struct EdgeHash *eh;
	float *gravity;
	float hfac;
	/* Average distance to neighbours (other particles in the support domain),
	 * for calculating the Courant number (adaptive time step). */
	int pass;
	float element_size;
	float flow[3];

	/* Integrator callbacks. This allows different SPH implementations. */
	void (*force_cb) (void *sphdata_v, ParticleKey *state, float *force, float *impulse);
	void (*density_cb) (void *rangedata_v, int index, float squared_dist);
} SPHData;

typedef struct ParticleTexture {
	float ivel;                           /* used in reset */
	float time, life, exist, size;        /* used in init */
	float damp, gravity, field;           /* used in physics */
	float length, clump, kink, effector;  /* used in path caching */
	float rough1, rough2, roughe;         /* used in path caching */
} ParticleTexture;

typedef struct ParticleSeam {
	float v0[3], v1[3];
	float nor[3], dir[3], tan[3];
	float length2;
} ParticleSeam;

typedef struct ParticleCacheKey {
	float co[3];
	float vel[3];
	float rot[4];
	float col[3];
	float time;
	int steps;
} ParticleCacheKey;

typedef struct ParticleThreadContext {
	/* shared */
	struct ParticleSimulationData sim;
	struct DerivedMesh *dm;
	struct Material *ma;

	/* distribution */
	struct KDTree *tree;

	struct ParticleSeam *seams;
	int totseam;

	float *jit, *jitoff, *weight;
	float maxweight;
	int *index, *skip, jitlevel;

	int from, cfrom, distr;

	struct ParticleData *tpars;

	/* path caching */
	int editupdate, between, steps;
	int totchild, totparent, parent_pass;

	float cfra;

	float *vg_length, *vg_clump, *vg_kink;
	float *vg_rough1, *vg_rough2, *vg_roughe;
	float *vg_effector;
} ParticleThreadContext;

typedef struct ParticleThread {
	ParticleThreadContext *ctx;
	struct RNG *rng, *rng_path;
	int num, tot;
} ParticleThread;

typedef struct ParticleBillboardData {
	struct Object *ob;
	float vec[3], vel[3];
	float offset[2];
	float size[2];
	float tilt, random, time;
	int uv[3];
	int lock, num;
	int totnum;
	int lifetime;
	short align, uv_split, anim, split_offset;
} ParticleBillboardData;

typedef struct ParticleCollisionElement {
	/* pointers to original data */
	float *x[4], *v[4];

	/* values interpolated from original data*/
	float x0[3], x1[3], x2[3], p[3];
	
	/* results for found intersection point */
	float nor[3], vel[3], uv[2];

	/* count of original data (1-4) */
	int tot;

	/* index of the collision face */
	int index;

	/* flags for inversed normal / particle already inside element at start */
	short inv_nor, inside;
} ParticleCollisionElement;

/* container for moving data between deflet_particle and particle_intersect_face */
typedef struct ParticleCollision {
	struct Object *current;
	struct Object *hit;
	struct Object *prev;
	struct Object *skip;
	struct Object *emitter;

	struct CollisionModifierData *md; // collision modifier for current object;

	float f;    // time factor of previous collision, needed for substracting face velocity
	float fac1, fac2;

	float cfra, old_cfra;

	float original_ray_length; //original length of co2-co1, needed for collision time evaluation

	int prev_index;

	ParticleCollisionElement pce;

	/* total_time is the amount of time in this subframe
	 * inv_total_time is the opposite
	 * inv_timestep is the inverse of the amount of time in this frame */
	float total_time, inv_total_time, inv_timestep;

	float radius;
	float co1[3], co2[3];
	float ve1[3], ve2[3];

	float acc[3], boid_z;

	int boid;
} ParticleCollision;

typedef struct ParticleDrawData {
	float *vdata, *vd;      /* vertice data */
	float *ndata, *nd;      /* normal data */
	float *cdata, *cd;      /* color data */
	float *vedata, *ved;    /* velocity data */
	float *ma_col;
	int tot_vec_size, flag;
	int totpoint, totve;
} ParticleDrawData;

#define PARTICLE_DRAW_DATA_UPDATED  1

#define PSYS_FRAND_COUNT    1024
extern unsigned int PSYS_FRAND_SEED_OFFSET[PSYS_FRAND_COUNT];
extern unsigned int PSYS_FRAND_SEED_MULTIPLIER[PSYS_FRAND_COUNT];
extern float PSYS_FRAND_BASE[PSYS_FRAND_COUNT];

void psys_init_rng(void);

BLI_INLINE float psys_frand(ParticleSystem *psys, unsigned int seed)
{
	/* XXX far from ideal, this simply scrambles particle random numbers a bit
	 * to avoid obvious correlations.
	 * Can't use previous psys->frand arrays because these require initialization
	 * inside psys_check_enabled, which wreaks havok in multithreaded depgraph updates.
	 */
	unsigned int offset = PSYS_FRAND_SEED_OFFSET[psys->seed % PSYS_FRAND_COUNT];
	unsigned int multiplier = PSYS_FRAND_SEED_MULTIPLIER[psys->seed % PSYS_FRAND_COUNT];
	return PSYS_FRAND_BASE[(offset + seed * multiplier) % PSYS_FRAND_COUNT];
}

BLI_INLINE void psys_frand_vec(ParticleSystem *psys, unsigned int seed, float vec[3])
{
	unsigned int offset = PSYS_FRAND_SEED_OFFSET[psys->seed % PSYS_FRAND_COUNT];
	unsigned int multiplier = PSYS_FRAND_SEED_MULTIPLIER[psys->seed % PSYS_FRAND_COUNT];
	vec[0] = PSYS_FRAND_BASE[(offset + (seed + 0) * multiplier) % PSYS_FRAND_COUNT];
	vec[1] = PSYS_FRAND_BASE[(offset + (seed + 1) * multiplier) % PSYS_FRAND_COUNT];
	vec[2] = PSYS_FRAND_BASE[(offset + (seed + 2) * multiplier) % PSYS_FRAND_COUNT];
}

/* ----------- functions needed outside particlesystem ---------------- */
/* particle.c */
int count_particles(struct ParticleSystem *psys);
int count_particles_mod(struct ParticleSystem *psys, int totgr, int cur);

struct ParticleSystem *psys_get_current(struct Object *ob);
/* for rna */
short psys_get_current_num(struct Object *ob);
void psys_set_current_num(Object *ob, int index);
/* UNUSED */
// struct Object *psys_find_object(struct Scene *scene, struct ParticleSystem *psys);

struct LatticeDeformData *psys_create_lattice_deform_data(struct ParticleSimulationData *sim);

int psys_in_edit_mode(struct Scene *scene, struct ParticleSystem *psys);
int psys_check_enabled(struct Object *ob, struct ParticleSystem *psys);
int psys_check_edited(struct ParticleSystem *psys);

void psys_check_group_weights(struct ParticleSettings *part);
int psys_uses_gravity(struct ParticleSimulationData *sim);

/* free */
void BKE_particlesettings_free(struct ParticleSettings *part);
void psys_free_path_cache(struct ParticleSystem *psys, struct PTCacheEdit *edit);
void psys_free(struct Object *ob, struct ParticleSystem *psys);

void psys_render_set(struct Object *ob, struct ParticleSystem *psys, float viewmat[4][4], float winmat[4][4], int winx, int winy, int timeoffset);
void psys_render_restore(struct Object *ob, struct ParticleSystem *psys);
int psys_render_simplify_distribution(struct ParticleThreadContext *ctx, int tot);
bool psys_render_simplify_params(struct ParticleSystem *psys, struct ChildParticle *cpa, float *params);

void psys_interpolate_uvs(const struct MTFace *tface, int quad, const float w[4], float uvco[2]);
void psys_interpolate_mcol(const struct MCol *mcol, int quad, const float w[4], struct MCol *mc);

void copy_particle_key(struct ParticleKey *to, struct ParticleKey *from, int time);

void psys_particle_on_emitter(struct ParticleSystemModifierData *psmd, int distr, int index, int index_dmcache,
                              float fuv[4], float foffset, float vec[3], float nor[3],
                              float utan[3], float vtan[3], float orco[3], float ornor[3]);
struct ParticleSystemModifierData *psys_get_modifier(struct Object *ob, struct ParticleSystem *psys);

struct ModifierData *object_add_particle_system(struct Scene *scene, struct Object *ob, const char *name);
void object_remove_particle_system(struct Scene *scene, struct Object *ob);
struct ParticleSettings *psys_new_settings(const char *name, struct Main *main);
struct ParticleSettings *BKE_particlesettings_copy(struct ParticleSettings *part);
void BKE_particlesettings_make_local(struct ParticleSettings *part);

void psys_reset(struct ParticleSystem *psys, int mode);

void psys_find_parents(struct ParticleSimulationData *sim);

void psys_cache_paths(struct ParticleSimulationData *sim, float cfra);
void psys_cache_edit_paths(struct Scene *scene, struct Object *ob, struct PTCacheEdit *edit, float cfra);
void psys_cache_child_paths(struct ParticleSimulationData *sim, float cfra, int editupdate);
int do_guides(struct ListBase *effectors, ParticleKey *state, int pa_num, float time);
void precalc_guides(struct ParticleSimulationData *sim, struct ListBase *effectors);
float psys_get_timestep(struct ParticleSimulationData *sim);
float psys_get_child_time(struct ParticleSystem *psys, struct ChildParticle *cpa, float cfra, float *birthtime, float *dietime);
float psys_get_child_size(struct ParticleSystem *psys, struct ChildParticle *cpa, float cfra, float *pa_time);
void psys_get_particle_on_path(struct ParticleSimulationData *sim, int pa_num, struct ParticleKey *state, const bool vel);
int psys_get_particle_state(struct ParticleSimulationData *sim, int p, struct ParticleKey *state, int always);

void psys_sph_init(struct ParticleSimulationData *sim, struct SPHData *sphdata);
void psys_sph_finalise(struct SPHData *sphdata);
void psys_sph_density(struct BVHTree *tree, struct SPHData *data, float co[3], float vars[2]);

/* for anim.c */
void psys_get_dupli_texture(struct ParticleSystem *psys, struct ParticleSettings *part,
                            struct ParticleSystemModifierData *psmd, struct ParticleData *pa, struct ChildParticle *cpa,
                            float uv[2], float orco[3]);
void psys_get_dupli_path_transform(struct ParticleSimulationData *sim, struct ParticleData *pa, struct ChildParticle *cpa,
                                   struct ParticleCacheKey *cache, float mat[4][4], float *scale);

ParticleThread *psys_threads_create(struct ParticleSimulationData *sim);
void psys_threads_free(ParticleThread *threads);

void psys_make_billboard(ParticleBillboardData *bb, float xvec[3], float yvec[3], float zvec[3], float center[3]);
void psys_apply_hair_lattice(struct Scene *scene, struct Object *ob, struct ParticleSystem *psys);

/* particle_system.c */
struct ParticleSystem *psys_get_target_system(struct Object *ob, struct ParticleTarget *pt);
void psys_count_keyed_targets(struct ParticleSimulationData *sim);
void psys_update_particle_tree(struct ParticleSystem *psys, float cfra);
void psys_changed_type(struct Object *ob, struct ParticleSystem *psys);

void psys_make_temp_pointcache(struct Object *ob, struct ParticleSystem *psys);
void psys_get_pointcache_start_end(struct Scene *scene, ParticleSystem *psys, int *sfra, int *efra);

void psys_check_boid_data(struct ParticleSystem *psys);

void psys_get_birth_coordinates(struct ParticleSimulationData *sim, struct ParticleData *pa, struct ParticleKey *state, float dtime, float cfra);

void particle_system_update(struct Scene *scene, struct Object *ob, struct ParticleSystem *psys);

/* ----------- functions needed only inside particlesystem ------------ */
/* particle.c */
void psys_disable_all(struct Object *ob);
void psys_enable_all(struct Object *ob);

void free_hair(struct Object *ob, struct ParticleSystem *psys, int dynamics);
void free_keyed_keys(struct ParticleSystem *psys);
void psys_free_particles(struct ParticleSystem *psys);
void psys_free_children(struct ParticleSystem *psys);

void psys_interpolate_particle(short type, struct ParticleKey keys[4], float dt, struct ParticleKey *result, int velocity);
void psys_vec_rot_to_face(struct DerivedMesh *dm, struct ParticleData *pa, float vec[3]);
void psys_mat_hair_to_object(struct Object *ob, struct DerivedMesh *dm, short from, struct ParticleData *pa, float hairmat[4][4]);
void psys_mat_hair_to_global(struct Object *ob, struct DerivedMesh *dm, short from, struct ParticleData *pa, float hairmat[4][4]);
void psys_mat_hair_to_orco(struct Object *ob, struct DerivedMesh *dm, short from, struct ParticleData *pa, float hairmat[4][4]);

float psys_get_dietime_from_cache(struct PointCache *cache, int index);

void psys_free_pdd(struct ParticleSystem *psys);

float *psys_cache_vgroup(struct DerivedMesh *dm, struct ParticleSystem *psys, int vgroup);
void psys_get_texture(struct ParticleSimulationData *sim, struct ParticleData *pa, struct ParticleTexture *ptex, int event, float cfra);
void psys_interpolate_face(struct MVert *mvert, struct MFace *mface, struct MTFace *tface,
                           float (*orcodata)[3], float w[4], float vec[3], float nor[3], float utan[3], float vtan[3],
                           float orco[3], float ornor[3]);
float psys_particle_value_from_verts(struct DerivedMesh *dm, short from, struct ParticleData *pa, float *values);
void psys_get_from_key(struct ParticleKey *key, float loc[3], float vel[3], float rot[4], float *time);

/* BLI_bvhtree_ray_cast callback */
void BKE_psys_collision_neartest_cb(void *userdata, int index, const struct BVHTreeRay *ray, struct BVHTreeRayHit *hit);
void psys_particle_on_dm(struct DerivedMesh *dm, int from, int index, int index_dmcache,
                         const float fw[4], float foffset, float vec[3], float nor[3], float utan[3], float vtan[3],
                         float orco[3], float ornor[3]);

/* particle_system.c */
void initialize_particle(struct ParticleSimulationData *sim, struct ParticleData *pa);
void psys_calc_dmcache(struct Object *ob, struct DerivedMesh *dm, struct ParticleSystem *psys);
int psys_particle_dm_face_lookup(struct Object *ob, struct DerivedMesh *dm, int index, const float fw[4], struct LinkNode *node);

void reset_particle(struct ParticleSimulationData *sim, struct ParticleData *pa, float dtime, float cfra);

float psys_get_current_display_percentage(struct ParticleSystem *psys);

/* psys_reset */
#define PSYS_RESET_ALL          1
#define PSYS_RESET_DEPSGRAPH    2
/* #define PSYS_RESET_CHILDREN  3 */ /*UNUSED*/
#define PSYS_RESET_CACHE_MISS   4

/* index_dmcache */
#define DMCACHE_NOTFOUND    -1
#define DMCACHE_ISCHILD     -2

#endif
