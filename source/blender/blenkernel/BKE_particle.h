/* BKE_particle.h
 *
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
 * The Original Code is Copyright (C) 2007 by Janne Karhu.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef BKE_PARTICLE_H
#define BKE_PARTICLE_H

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
struct LinkNode;
struct KDTree;
struct RNG;
struct SurfaceModifierData;
struct BVHTreeRay;
struct BVHTreeRayHit; 

#define PARTICLE_P				ParticleData *pa; int p
#define LOOP_PARTICLES	for(p=0, pa=psys->particles; p<psys->totpart; p++, pa++)

typedef struct ParticleEffectorCache {
	struct ParticleEffectorCache *next, *prev;
	struct Object *ob;
	
	/* precalculated variables for guides */
	float firstloc[4], firstdir[3];
	float *distances;
	float *locations;
	/* precalculated variables for deflection */
	float ob_minmax[6];
	float *face_minmax;
	float *vert_cos;
	/* precalculated variables for boids */
	struct KDTree *tree;

	short type, psys_nbr;
	
	struct Object obcopy;	/* for restoring transformation data */
	struct RNG *rng; /* random noise generator for e.g. wind */
} ParticleEffectorCache;

typedef struct ParticleReactEvent {
	struct ParticleReactEvent *next, *prev;
	int event, pa_num;
	Object *ob;
	struct ParticleSystem *psys;
	struct ParticleKey state;

	float time, size;
}ParticleReactEvent;

typedef struct ParticleTexture{
	float ivel;							/* used in reset */
	float time, life, exist, size;		/* used in init */
	float pvel[3];						/* used in physics */
	float length, clump, kink, effector;/* used in path caching */
	float rough1, rough2, roughe;		/* used in path caching */
} ParticleTexture;

typedef struct ParticleSeam{
	float v0[3], v1[3];
	float nor[3], dir[3], tan[3];
	float length2;
} ParticleSeam;

typedef struct ParticleCacheKey{
	float co[3];
	float vel[3];
	float rot[4];
	float col[3];
	float time;
	int steps;
} ParticleCacheKey;

typedef struct ParticleThreadContext {
	/* shared */
	struct Scene *scene;
	struct Object *ob;
	struct DerivedMesh *dm;
	struct ParticleSystemModifierData *psmd;
	struct ParticleSystem *psys;
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

typedef struct ParticleBillboardData
{
	struct Object *ob;
	float vec[3], vel[3];
	float offset[2];
	float size, tilt, random, time;
	int uv[3];
	int lock, num;
	int totnum;
	short align, uv_split, anim, split_offset;
}
ParticleBillboardData;

/* container for moving data between deflet_particle and particle_intersect_face */
typedef struct ParticleCollision
{
	struct Object *ob, *ob_t; // collided and current objects
	struct CollisionModifierData *md; // collision modifier for ob_t;
	float nor[3]; // normal at collision point
	float vel[3]; // velocity of collision point
	float co1[3], co2[3]; // ray start and end points
	float ray_len; // original length of co2-co1, needed for collision time evaluation
	float t;	// time of previous collision, needed for substracting face velocity
}
ParticleCollision;

/* ----------- functions needed outside particlesystem ---------------- */
/* particle.c */
int count_particles(struct ParticleSystem *psys);
int count_particles_mod(struct ParticleSystem *psys, int totgr, int cur);
int psys_count_keys(struct ParticleSystem *psys);
char *psys_menu_string(struct Object *ob, int for_sb);

struct ParticleSystem *psys_get_current(struct Object *ob);
short psys_get_current_num(struct Object *ob);
void psys_set_current_num(Object *ob, int index);
struct Object *psys_find_object(struct Scene *scene, struct ParticleSystem *psys);
//struct ParticleSystem *psys_get(struct Object *ob, int index);
struct ParticleData *psys_get_selected_particle(struct ParticleSystem *psys, int *index);
struct ParticleKey *psys_get_selected_key(struct ParticleSystem *psys, int pa_index, int *key_index);
void psys_change_act(void *ob_v, void *act_v);
struct Object *psys_get_lattice(struct Scene *scene, struct Object *ob, struct ParticleSystem *psys);
void psys_disable_all(struct Object *ob);
void psys_enable_all(struct Object *ob);
int psys_ob_has_hair(struct Object *ob);
int psys_in_edit_mode(struct Scene *scene, struct ParticleSystem *psys);
int psys_check_enabled(struct Object *ob, struct ParticleSystem *psys);

void psys_free_boid_rules(struct ListBase *list);
void psys_free_settings(struct ParticleSettings *part);
void free_child_path_cache(struct ParticleSystem *psys);
void psys_free_path_cache(struct ParticleSystem *psys, struct PTCacheEdit *edit);
void free_hair(struct Object *ob, struct ParticleSystem *psys, int dynamics);
void free_keyed_keys(struct ParticleSystem *psys);
void psys_free_particles(struct ParticleSystem *psys);
void psys_free(struct Object * ob, struct ParticleSystem * psys);
void psys_free_children(struct ParticleSystem *psys);

void psys_render_set(struct Object *ob, struct ParticleSystem *psys, float viewmat[][4], float winmat[][4], int winx, int winy, int timeoffset);
void psys_render_restore(struct Object *ob, struct ParticleSystem *psys);
int psys_render_simplify_distribution(struct ParticleThreadContext *ctx, int tot);
int psys_render_simplify_params(struct ParticleSystem *psys, struct ChildParticle *cpa, float *params);

void psys_interpolate_uvs(struct MTFace *tface, int quad, float *uv, float *uvco);
void psys_interpolate_mcol(struct MCol *mcol, int quad, float *uv, struct MCol *mc);

void copy_particle_key(struct ParticleKey *to, struct ParticleKey *from, int time);

void psys_particle_on_emitter(struct ParticleSystemModifierData *psmd, int distr, int index, int index_dmcache, float *fuv, float foffset, float *vec, float *nor, float *utan, float *vtan, float *orco, float *ornor);
struct ParticleSystemModifierData *psys_get_modifier(struct Object *ob, struct ParticleSystem *psys);

void object_add_particle_system(struct Scene *scene, struct Object *ob);
void object_remove_particle_system(struct Scene *scene, struct Object *ob);
struct ParticleSettings *psys_new_settings(char *name, struct Main *main);
struct ParticleSettings *psys_copy_settings(struct ParticleSettings *part);
void psys_flush_particle_settings(struct Scene *scene, struct ParticleSettings *part, int recalc);
void make_local_particlesettings(struct ParticleSettings *part);

struct LinkNode *psys_using_settings(struct Scene *scene, struct ParticleSettings *part, int flush_update);
void psys_reset(struct ParticleSystem *psys, int mode);

void psys_find_parents(struct Object *ob, struct ParticleSystemModifierData *psmd, struct ParticleSystem *psys);

void psys_cache_paths(struct Scene *scene, struct Object *ob, struct ParticleSystem *psys, float cfra);
void psys_cache_edit_paths(struct Scene *scene, struct Object *ob, struct PTCacheEdit *edit, float cfra);
void psys_cache_child_paths(struct Scene *scene, struct Object *ob, struct ParticleSystem *psys, float cfra, int editupdate);
int do_guide(struct Scene *scene, struct ParticleKey *state, int pa_num, float time, struct ListBase *lb);
float psys_get_size(struct Object *ob, struct Material *ma, struct ParticleSystemModifierData *psmd, struct IpoCurve *icu_size, struct ParticleSystem *psys, struct ParticleSettings *part, struct ParticleData *pa, float *vg_size);
float psys_get_timestep(struct ParticleSettings *part);
float psys_get_child_time(struct ParticleSystem *psys, struct ChildParticle *cpa, float cfra, float *birthtime, float *dietime);
float psys_get_child_size(struct ParticleSystem *psys, struct ChildParticle *cpa, float cfra, float *pa_time);
void psys_get_particle_on_path(struct Scene *scene, struct Object *ob, struct ParticleSystem *psys, int pa_num, struct ParticleKey *state, int vel);
int psys_get_particle_state(struct Scene *scene, struct Object *ob, struct ParticleSystem *psys, int p, struct ParticleKey *state, int always);
void psys_get_dupli_texture(struct Object *ob, struct ParticleSettings *part, struct ParticleSystemModifierData *psmd, struct ParticleData *pa, struct ChildParticle *cpa, float *uv, float *orco);
void psys_get_dupli_path_transform(struct Object *ob, struct ParticleSystem *psys, struct ParticleSystemModifierData *psmd, struct ParticleData *pa, struct ChildParticle *cpa, struct ParticleCacheKey *cache, float mat[][4], float *scale);

ParticleThread *psys_threads_create(struct Scene *scene, struct Object *ob, struct ParticleSystem *psys);
int psys_threads_init_distribution(ParticleThread *threads, struct Scene *scene, struct DerivedMesh *dm, int from);
int psys_threads_init_path(ParticleThread *threads, struct Scene *scene, float cfra, int editupdate);
void psys_threads_free(ParticleThread *threads);

void psys_thread_distribute_particle(ParticleThread *thread, struct ParticleData *pa, struct ChildParticle *cpa, int p);
void psys_thread_create_path(ParticleThread *thread, struct ChildParticle *cpa, ParticleCacheKey *keys, int i);

void psys_make_billboard(ParticleBillboardData *bb, float xvec[3], float yvec[3], float zvec[3], float center[3]);

/* particle_system.c */
struct ParticleSystem *psys_get_target_system(struct Object *ob, struct ParticleTarget *pt);
void psys_count_keyed_targets(struct Object *ob, struct ParticleSystem *psys);
void psys_get_reactor_target(struct Object *ob, struct ParticleSystem *psys, struct Object **target_ob, struct ParticleSystem **target_psys);

void psys_init_effectors(struct Scene *scene, struct Object *obsrc, struct Group *group, struct ParticleSystem *psys);
void psys_end_effectors(struct ParticleSystem *psys);

void psys_make_temp_pointcache(struct Object *ob, struct ParticleSystem *psys);
void psys_end_temp_pointcache(struct ParticleSystem *psys);
void psys_get_pointcache_start_end(struct Scene *scene, struct ParticleSystem *psys, int *sfra, int *efra);

void psys_check_boid_data(struct ParticleSystem *psys);

void particle_system_update(struct Scene *scene, struct Object *ob, struct ParticleSystem *psys);

/* ----------- functions needed only inside particlesystem ------------ */
/* particle.c */
void psys_interpolate_particle(short type, struct ParticleKey keys[4], float dt, struct ParticleKey *result, int velocity);
void psys_key_to_object(struct Object *ob, struct ParticleKey *key, float imat[][4]);
//void psys_key_to_geometry(struct DerivedMesh *dm, struct ParticleData *pa, struct ParticleKey *key);
//void psys_key_from_geometry(struct DerivedMesh *dm, struct ParticleData *pa, struct ParticleKey *key);
//void psys_face_mat(struct DerivedMesh *dm, struct ParticleData *pa, float mat[][4]);
void psys_vec_rot_to_face(struct DerivedMesh *dm, struct ParticleData *pa, float *vec);
//void psys_vec_rot_from_face(struct DerivedMesh *dm, struct ParticleData *pa, float *vec);
void psys_mat_hair_to_object(struct Object *ob, struct DerivedMesh *dm, short from, struct ParticleData *pa, float hairmat[][4]);
void psys_mat_hair_to_global(struct Object *ob, struct DerivedMesh *dm, short from, struct ParticleData *pa, float hairmat[][4]);
void psys_mat_hair_to_orco(struct Object *ob, struct DerivedMesh *dm, short from, struct ParticleData *pa, float hairmat[][4]);

float *psys_cache_vgroup(struct DerivedMesh *dm, struct ParticleSystem *psys, int vgroup);
void psys_get_texture(struct Object *ob, struct Material *ma, struct ParticleSystemModifierData *psmd, struct ParticleSystem *psys, struct ParticleData *pa, struct ParticleTexture *ptex, int event);
void psys_interpolate_face(struct MVert *mvert, struct MFace *mface, struct MTFace *tface, float (*orcodata)[3], float *uv, float *vec, float *nor, float *utan, float *vtan, float *orco, float *ornor);
float psys_particle_value_from_verts(struct DerivedMesh *dm, short from, struct ParticleData *pa, float *values);
float psys_interpolate_value_from_verts(struct DerivedMesh *dm, short from, int index, float *fw, float *values);
void psys_get_from_key(struct ParticleKey *key, float *loc, float *vel, float *rot, float *time);

int psys_intersect_dm(struct Scene *scene, struct Object *ob, struct DerivedMesh *dm, float *vert_cos, float *co1, float* co2, float *min_d, int *min_face, float *min_uv, float *face_minmax, float *pa_minmax, float radius, float *ipoint);
void particle_intersect_face(void *userdata, int index, const struct BVHTreeRay *ray, struct BVHTreeRayHit *hit);
void psys_particle_on_dm(struct DerivedMesh *dm, int from, int index, int index_dmcache, float *fw, float foffset, float *vec, float *nor, float *utan, float *vtan, float *orco, float *ornor);

/* particle_system.c */
void initialize_particle(struct ParticleData *pa, int p, struct Object *ob, struct ParticleSystem *psys, struct ParticleSystemModifierData *psmd);

int effector_find_co(struct Scene *scene, float *pco, struct SurfaceModifierData *sur, struct Object *ob, struct PartDeflect *pd, float *co, float *nor, float *vel, int *index);
void do_effectors(int pa_no, struct ParticleData *pa, struct ParticleKey *state, struct Scene *scene, struct Object *ob, struct ParticleSystem *psys, float *texco, float *force_field, float *vel,float framestep, float cfra);

void psys_calc_dmcache(struct Object *ob, struct DerivedMesh *dm, struct ParticleSystem *psys);
int psys_particle_dm_face_lookup(struct Object *ob, struct DerivedMesh *dm, int index, float *fw, struct LinkNode *node);

void reset_particle(struct Scene *scene, struct ParticleData *pa, struct ParticleSystem *psys, struct ParticleSystemModifierData *psmd, struct Object *ob,
					float dtime, float cfra, float *vg_vel, float *vg_tan, float *vg_rot);


/* psys_reset */
#define PSYS_RESET_ALL			1
#define PSYS_RESET_DEPSGRAPH 	2
#define PSYS_RESET_CHILDREN 	3
#define PSYS_RESET_CACHE_MISS	4

/* ParticleEffectorCache->type */
#define PSYS_EC_EFFECTOR	1
#define PSYS_EC_DEFLECT		2
#define PSYS_EC_PARTICLE	4
#define PSYS_EC_REACTOR		8

/* index_dmcache */
#define DMCACHE_NOTFOUND	-1
#define DMCACHE_ISCHILD		-2

#endif
