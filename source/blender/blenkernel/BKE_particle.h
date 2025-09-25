/* SPDX-FileCopyrightText: 2007 Janne Karhu. All rights reserved.
 * SPDX-FileCopyrightText: 2011-2012 AutoCRC (adaptive time step, Classical SPH).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#include <optional>

#include "BLI_compiler_attrs.h"
#include "BLI_map.hh"
#include "BLI_ordered_edge.hh"
#include "BLI_vector.hh"

#include "BKE_lib_query.hh" /* For LibraryForeachIDCallbackFlag. */

#include "DNA_particle_types.h"

struct ParticleKey;
struct ParticleSettings;
struct ParticleSystem;
struct ParticleSystemModifierData;

struct BVHTreeRay;
struct BVHTreeRayHit;
struct BlendDataReader;
struct BlendLibReader;
struct BlendWriter;
struct CustomData_MeshMasks;
struct Depsgraph;
struct KDTree_3d;
struct LinkNode;
struct MCol;
struct MFace;
struct MTFace;
struct Main;
struct ModifierData;
struct Object;
struct RNG;
struct Scene;

#define PARTICLE_COLLISION_MAX_COLLISIONS 10

#define PARTICLE_P \
  ParticleData *pa; \
  int p
#define LOOP_PARTICLES for (p = 0, pa = psys->particles; p < psys->totpart; p++, pa++)
#define LOOP_EXISTING_PARTICLES \
  for (p = 0, pa = psys->particles; p < psys->totpart; p++, pa++) \
    if (!(pa->flag & PARS_UNEXIST))
#define LOOP_SHOWN_PARTICLES \
  for (p = 0, pa = psys->particles; p < psys->totpart; p++, pa++) \
    if (!(pa->flag & (PARS_UNEXIST | PARS_NO_DISP)))
#define LOOP_DYNAMIC_PARTICLES \
  for (p = 0; p < psys->totpart; p++) \
    if ((pa = psys->particles + p)->state.time > 0.0f)

/* Fast but sure way to get the modifier. */
#define PARTICLE_PSMD \
  ParticleSystemModifierData *psmd = sim->psmd ? sim->psmd : psys_get_modifier(sim->ob, sim->psys)

/* common stuff that many particle functions need */
typedef struct ParticleSimulationData {
  struct Depsgraph *depsgraph;
  struct Scene *scene;
  struct Object *ob;
  struct ParticleSystem *psys;
  struct ParticleSystemModifierData *psmd;
  struct ListBase *colliders;
  /* Courant number. This is used to implement an adaptive time step. Only the
   * maximum value per time step is important. Only sph_integrate makes use of
   * this at the moment. Other solvers could, too. */
  float courant_num;
  /* Only valid during dynamics_step(). */
  struct RNG *rng;
} ParticleSimulationData;

typedef struct SPHData {
  ParticleSystem *psys[10];
  ParticleData *pa;
  float mass;
  const blender::Map<blender::OrderedEdge, int> *eh;

  /** The gravity as a `float[3]`, may also be null when the simulation doesn't use gravity. */
  const float *gravity;

  float hfac;
  /* Average distance to neighbors (other particles in the support domain),
   * for calculating the Courant number (adaptive time step). */
  int pass;
  float element_size;
  float flow[3];

  /* Temporary thread-local buffer for springs created during this step. */
  blender::Vector<ParticleSpring> new_springs;

  /* Integrator callbacks. This allows different SPH implementations. */
  void (*force_cb)(void *sphdata_v, ParticleKey *state, float *force, float *impulse);
  void (*density_cb)(void *rangedata_v, int index, const float co[3], float squared_dist);
} SPHData;

typedef struct ParticleTexture {
  float ivel;                                         /* used in reset */
  float time, life, exist, size;                      /* used in init */
  float damp, gravity, field;                         /* used in physics */
  float length, clump, kink_freq, kink_amp, effector; /* used in path caching */
  float rough1, rough2, roughe;                       /* used in path caching */
  float twist;                                        /* used in path caching */
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
  int segments;
} ParticleCacheKey;

typedef struct ParticleThreadContext {
  /* shared */
  struct ParticleSimulationData sim;
  struct Mesh *mesh;
  struct Material *ma;

  /* distribution */
  struct KDTree_3d *tree;

  struct ParticleSeam *seams;
  int totseam;

  float *jit, *jitoff, *weight;
  float maxweight;
  int *index, jitlevel;

  int cfrom, distr;

  struct ParticleData *tpars;

  /* path caching */
  bool editupdate;
  int between, segments, extra_segments;
  int totchild, totparent, parent_pass;

  float cfra;

  float *vg_length, *vg_clump, *vg_kink;
  float *vg_rough1, *vg_rough2, *vg_roughe;
  float *vg_effector;
  float *vg_twist;

  struct CurveMapping *clumpcurve;
  struct CurveMapping *roughcurve;
  struct CurveMapping *twistcurve;
} ParticleThreadContext;

typedef struct ParticleTask {
  ParticleThreadContext *ctx = nullptr;
  struct RNG *rng = nullptr, *rng_path = nullptr;
  int begin = 0, end = 0;
} ParticleTask;

typedef struct ParticleCollisionElement {
  /* pointers to original data */
  float *x[3], *v[3];

  /* Values interpolated from original data. */
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

/** Container for moving data between deflet_particle and particle_intersect_face. */
typedef struct ParticleCollision {
  struct Object *current;
  struct Object *hit;
  struct Object *skip[PARTICLE_COLLISION_MAX_COLLISIONS + 1];
  struct Object *emitter;

  /** Collision modifier for current object. */
  struct CollisionModifierData *md;

  /** Time factor of previous collision, needed for subtracting face velocity. */
  float f;
  float fac1, fac2;

  float cfra, old_cfra;

  /** Original length of co2-co1, needed for collision time evaluation. */
  float original_ray_length;

  int skip_count;

  ParticleCollisionElement pce;

  /** The amount of time in this sub-frame. */
  float total_time;
  /** The inverse of `total_time`. */
  float inv_total_time;
  /** The inverse of the amount of time in this frame. */
  float inv_timestep;

  float radius;
  float co1[3], co2[3];
  float ve1[3], ve2[3];

  float acc[3], boid_z;

  int boid;
} ParticleCollision;

typedef struct ParticleDrawData {
  float *vdata, *vd;   /* vertex data */
  float *ndata, *nd;   /* normal data */
  float *cdata, *cd;   /* color data */
  float *vedata, *ved; /* velocity data */
  float *ma_col;
  int totpart, partsize;
  int flag;
  int totpoint, totve;
} ParticleDrawData;

#define PARTICLE_DRAW_DATA_UPDATED 1

#define PSYS_FRAND_COUNT 1024
extern unsigned int PSYS_FRAND_SEED_OFFSET[PSYS_FRAND_COUNT];
extern unsigned int PSYS_FRAND_SEED_MULTIPLIER[PSYS_FRAND_COUNT];
extern float PSYS_FRAND_BASE[PSYS_FRAND_COUNT];

void BKE_particle_init_rng(void);

BLI_INLINE float psys_frand(ParticleSystem *psys, unsigned int seed)
{
  /* XXX far from ideal, this simply scrambles particle random numbers a bit
   * to avoid obvious correlations.
   * Can't use previous psys->frand arrays because these require initialization
   * inside psys_check_enabled, which wreaks havoc in multi-threaded depsgraph updates.
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
/* particle.cc */

/* Few helpers for count-all etc. */

int count_particles(struct ParticleSystem *psys);
int count_particles_mod(struct ParticleSystem *psys, int totgr, int cur);

int psys_get_child_number(struct Scene *scene,
                          struct ParticleSystem *psys,
                          bool use_render_params);
int psys_get_tot_child(struct Scene *scene, struct ParticleSystem *psys, bool use_render_params);

/**
 * Get object's active particle system safely.
 */
struct ParticleSystem *psys_get_current(struct Object *ob);

/* For RNA API. */

short psys_get_current_num(struct Object *ob);
void psys_set_current_num(struct Object *ob, int index);
/* UNUSED */
// struct Object *psys_find_object(struct Scene *scene, struct ParticleSystem *psys);

/**
 * Initialize/free data for particle simulation evaluation.
 */
void psys_sim_data_init(struct ParticleSimulationData *sim);
void psys_sim_data_free(struct ParticleSimulationData *sim);

/**
 * For a given evaluated particle system get its original.
 *
 * If this input is an original particle system already, the return value is the same as the input.
 */
struct ParticleSystem *psys_orig_get(struct ParticleSystem *psys);

/**
 * For a given original object and its particle system,
 * get evaluated particle system within a given dependency graph.
 */
struct ParticleSystem *psys_eval_get(struct Depsgraph *depsgraph,
                                     struct Object *object,
                                     struct ParticleSystem *psys);

bool psys_in_edit_mode(struct Depsgraph *depsgraph, const struct ParticleSystem *psys);
bool psys_check_enabled(struct Object *ob, struct ParticleSystem *psys, bool use_render_params);
bool psys_check_edited(struct ParticleSystem *psys);

void psys_find_group_weights(struct ParticleSettings *part);
void psys_check_group_weights(struct ParticleSettings *part);
int psys_uses_gravity(struct ParticleSimulationData *sim);
void BKE_particlesettings_fluid_default_settings(struct ParticleSettings *part);

/**
 * Free cache path.
 */
void psys_free_path_cache(struct ParticleSystem *psys, struct PTCacheEdit *edit);
/**
 * Free everything.
 */
void psys_free(struct Object *ob, struct ParticleSystem *psys);
/**
 * Copy.
 */
void psys_copy_particles(struct ParticleSystem *psys_dst, struct ParticleSystem *psys_src);

bool psys_render_simplify_params(struct ParticleSystem *psys,
                                 struct ChildParticle *cpa,
                                 float *params);

void psys_interpolate_uvs(const struct MTFace *tface, int quad, const float w[4], float r_uv[2]);
void psys_interpolate_mcol(const struct MCol *mcol, int quad, const float w[4], struct MCol *mc);

void copy_particle_key(struct ParticleKey *to, struct ParticleKey *from, int time);

void psys_emitter_customdata_mask(struct ParticleSystem *psys,
                                  struct CustomData_MeshMasks *r_cddata_masks);
void psys_particle_on_emitter(struct ParticleSystemModifierData *psmd,
                              int from,
                              int index,
                              int index_dmcache,
                              float fuv[4],
                              float foffset,
                              float vec[3],
                              float nor[3],
                              float utan[3],
                              float vtan[3],
                              float orco[3]);
struct ParticleSystemModifierData *psys_get_modifier(struct Object *ob,
                                                     struct ParticleSystem *psys);

struct ModifierData *object_add_particle_system(struct Main *bmain,
                                                const struct Scene *scene,
                                                struct Object *ob,
                                                const char *name);
struct ModifierData *object_copy_particle_system(struct Main *bmain,
                                                 const struct Scene *scene,
                                                 struct Object *ob,
                                                 const struct ParticleSystem *psys_orig);
void object_remove_particle_system(struct Main *bmain,
                                   struct Scene *scene,
                                   struct Object *ob,
                                   struct ParticleSystem *psys);
struct ParticleSettings *BKE_particlesettings_add(struct Main *bmain, const char *name);
void psys_reset(struct ParticleSystem *psys, int mode);

void psys_find_parents(struct ParticleSimulationData *sim, bool use_render_params);

void psys_unique_name(struct Object *object, struct ParticleSystem *psys, const char *defname)
    ATTR_NONNULL(1, 2, 3);

/**
 * Calculates paths ready for drawing/rendering
 * - Useful for making use of opengl vertex arrays for super fast strand drawing.
 * - Makes child strands possible and creates them too into the cache.
 * - Cached path data is also used to determine cut position for the edit-mode tool.
 */
void psys_cache_paths(struct ParticleSimulationData *sim, float cfra, bool use_render_params);
void psys_cache_edit_paths(struct Depsgraph *depsgraph,
                           struct Scene *scene,
                           struct Object *ob,
                           struct PTCacheEdit *edit,
                           float cfra,
                           bool use_render_params);
void psys_cache_child_paths(struct ParticleSimulationData *sim,
                            float cfra,
                            bool editupdate,
                            bool use_render_params);
bool do_guides(struct Depsgraph *depsgraph,
               struct ParticleSettings *part,
               struct ListBase *effectors,
               ParticleKey *state,
               int index,
               float time);
void precalc_guides(struct ParticleSimulationData *sim, struct ListBase *effectors);
float psys_get_timestep(struct ParticleSimulationData *sim);
float psys_get_child_time(struct ParticleSystem *psys,
                          struct ChildParticle *cpa,
                          float cfra,
                          float *birthtime,
                          float *dietime);
float psys_get_child_size(struct ParticleSystem *psys,
                          struct ChildParticle *cpa,
                          float cfra,
                          float *pa_time);
/**
 * Gets hair (or keyed) particles state at the "path time" specified in `state->time`.
 */
void psys_get_particle_on_path(struct ParticleSimulationData *sim,
                               int pa_num,
                               struct ParticleKey *state,
                               bool vel);
/**
 * Gets particle's state at a time. Must call psys_sim_data_init before this.
 * \return true if particle exists and can be seen and false if not.
 */
bool psys_get_particle_state(struct ParticleSimulationData *sim,
                             int p,
                             struct ParticleKey *state,
                             bool always);

/* Child paths. */

void BKE_particlesettings_clump_curve_init(struct ParticleSettings *part);
void BKE_particlesettings_rough_curve_init(struct ParticleSettings *part);
void BKE_particlesettings_twist_curve_init(struct ParticleSettings *part);
void psys_apply_child_modifiers(struct ParticleThreadContext *ctx,
                                struct ListBase *modifiers,
                                struct ChildParticle *cpa,
                                struct ParticleTexture *ptex,
                                const float orco[3],
                                float hairmat[4][4],
                                struct ParticleCacheKey *keys,
                                struct ParticleCacheKey *parent_keys,
                                const float parent_orco[3]);

void psys_sph_finalize(struct SPHData *sphdata);
/**
 * Sample the density field at a point in space.
 */
void psys_sph_density(struct BVHTree *tree, struct SPHData *data, float co[3], float vars[2]);

/* For anim.c */

void psys_get_dupli_texture(struct ParticleSystem *psys,
                            struct ParticleSettings *part,
                            struct ParticleSystemModifierData *psmd,
                            struct ParticleData *pa,
                            struct ChildParticle *cpa,
                            float uv[2],
                            float orco[3]);
void psys_get_dupli_path_transform(struct ParticleSimulationData *sim,
                                   struct ParticleData *pa,
                                   struct ChildParticle *cpa,
                                   struct ParticleCacheKey *cache,
                                   float mat[4][4],
                                   float *scale);

/**
 * Threaded child particle distribution and path caching.
 */
void psys_thread_context_init(struct ParticleThreadContext *ctx,
                              struct ParticleSimulationData *sim);
void psys_thread_context_free(struct ParticleThreadContext *ctx);
blender::Vector<ParticleTask> psys_tasks_create(struct ParticleThreadContext *ctx,
                                                int startpart,
                                                int endpart);
void psys_tasks_free(blender::Vector<ParticleTask> &tasks);

void psys_apply_hair_lattice(struct Depsgraph *depsgraph,
                             struct Scene *scene,
                             struct Object *ob,
                             struct ParticleSystem *psys);

/* `particle_system.cc` */

struct ParticleSystem *psys_get_target_system(struct Object *ob, struct ParticleTarget *pt);
/**
 * Counts valid keyed targets.
 */
void psys_count_keyed_targets(struct ParticleSimulationData *sim);
void psys_update_particle_tree(struct ParticleSystem *psys, float cfra);
/**
 * System type has changed so set sensible defaults and clear non applicable flags.
 */
void psys_changed_type(struct Object *ob, struct ParticleSystem *psys);

void psys_make_temp_pointcache(struct Object *ob, struct ParticleSystem *psys);
void psys_get_pointcache_start_end(struct Scene *scene,
                                   ParticleSystem *psys,
                                   int *sfra,
                                   int *efra);

void psys_check_boid_data(struct ParticleSystem *psys);

void psys_get_birth_coords(struct ParticleSimulationData *sim,
                           struct ParticleData *pa,
                           struct ParticleKey *state,
                           float dtime,
                           float cfra);

/**
 * Main particle update call, checks that things are ok on the large scale and
 * then advances in to actual particle calculations depending on particle type.
 */
void particle_system_update(struct Depsgraph *depsgraph,
                            struct Scene *scene,
                            struct Object *ob,
                            struct ParticleSystem *psys,
                            bool use_render_params);

/**
 * Callback format for performing operations on ID-pointers for particle systems.
 */
typedef void (*ParticleSystemIDFunc)(struct ParticleSystem *psys,
                                     struct ID **idpoin,
                                     void *userdata,
                                     LibraryForeachIDCallbackFlag cb_flag);

void BKE_particlesystem_id_loop(struct ParticleSystem *psys,
                                ParticleSystemIDFunc func,
                                void *userdata);

/**
 * Reset all particle systems in the given object.
 */
void BKE_particlesystem_reset_all(struct Object *object);

/* ----------- functions needed only inside particlesystem ------------ */

/* particle.cc */

void psys_disable_all(struct Object *ob);
void psys_enable_all(struct Object *ob);

void free_hair(struct Object *ob, struct ParticleSystem *psys, int dynamics);
void free_keyed_keys(struct ParticleSystem *psys);
void psys_free_particles(struct ParticleSystem *psys);
void psys_free_children(struct ParticleSystem *psys);

void psys_interpolate_particle(
    short type, struct ParticleKey keys[4], float dt, struct ParticleKey *result, bool velocity);
void psys_vec_rot_to_face(struct Mesh *mesh, struct ParticleData *pa, float vec[3]);
void psys_mat_hair_to_object(struct Object *ob,
                             struct Mesh *mesh,
                             short from,
                             struct ParticleData *pa,
                             float hairmat[4][4]);
void psys_mat_hair_to_global(struct Object *ob,
                             struct Mesh *mesh,
                             short from,
                             struct ParticleData *pa,
                             float hairmat[4][4]);
void psys_mat_hair_to_orco(struct Object *ob,
                           struct Mesh *mesh,
                           short from,
                           struct ParticleData *pa,
                           float hairmat[4][4]);

float psys_get_dietime_from_cache(struct PointCache *cache, int index);

void psys_free_pdd(struct ParticleSystem *psys);

float *psys_cache_vgroup(struct Mesh *mesh, struct ParticleSystem *psys, int vgroup);
void psys_get_texture(struct ParticleSimulationData *sim,
                      struct ParticleData *pa,
                      struct ParticleTexture *ptex,
                      int event,
                      float cfra);
/**
 * Interpolate a location on a face based on face coordinates.
 */
void psys_interpolate_face(struct Mesh *mesh,
                           const float (*vert_positions)[3],
                           const float (*vert_normals)[3],
                           const struct MFace *mface,
                           struct MTFace *tface,
                           const float (*orcodata)[3],
                           float w[4],
                           float vec[3],
                           float nor[3],
                           float utan[3],
                           float vtan[3],
                           float orco[3]);
float psys_particle_value_from_verts(struct Mesh *mesh,
                                     short from,
                                     struct ParticleData *pa,
                                     float *values);
void psys_get_from_key(
    struct ParticleKey *key, float loc[3], float vel[3], float rot[4], float *time);

/**
 * Callback for #BVHTree near test.
 */
void BKE_psys_collision_neartest_cb(void *userdata,
                                    int index,
                                    const struct BVHTreeRay *ray,
                                    struct BVHTreeRayHit *hit);
/**
 * Interprets particle data to get a point on a mesh in object space.
 */
void psys_particle_on_dm(struct Mesh *mesh_final,
                         int from,
                         int index,
                         int index_dmcache,
                         const float fw[4],
                         float foffset,
                         float vec[3],
                         float nor[3],
                         float utan[3],
                         float vtan[3],
                         float orco[3]);

/* `particle_system.cc` */

void distribute_particles(struct ParticleSimulationData *sim, int from);
/**
 * Set particle parameters that don't change during particle's life.
 */
void init_particle(struct ParticleSimulationData *sim, struct ParticleData *pa);
void psys_calc_dmcache(struct Object *ob,
                       struct Mesh *mesh_final,
                       struct Mesh *mesh_original,
                       struct ParticleSystem *psys);
/**
 * Find the final derived mesh tessface for a particle, from its original tessface index.
 * This is slow and can be optimized but only for many lookups.
 *
 * \param mesh_final: Final mesh, it may not have the same topology as original mesh.
 * \param mesh_original: Original mesh, use for accessing poly to #MFace mapping.
 * \param findex_orig: The input tessface index.
 * \param fw: Face weights (position of the particle inside the \a findex_orig tessface).
 * \param poly_nodes: May be NULL, otherwise an array of linked list,
 * one for each final \a mesh_final face, containing all its tessfaces indices.
 * \return The \a mesh_final tessface index.
 */
int psys_particle_dm_face_lookup(struct Mesh *mesh_final,
                                 struct Mesh *mesh_original,
                                 int findex_orig,
                                 const float fw[4],
                                 struct LinkNode **poly_nodes);

/**
 * Sets particle to the emitter surface with initial velocity & rotation.
 */
void reset_particle(struct ParticleSimulationData *sim,
                    struct ParticleData *pa,
                    float dtime,
                    float cfra);

float psys_get_current_display_percentage(struct ParticleSystem *psys, bool use_render_params);

/* psys_reset */
#define PSYS_RESET_ALL 1
#define PSYS_RESET_DEPSGRAPH 2
// #define PSYS_RESET_CHILDREN  3 /*UNUSED*/
#define PSYS_RESET_CACHE_MISS 4

/* index_dmcache */
#define DMCACHE_NOTFOUND -1
#define DMCACHE_ISCHILD -2

/* **** Depsgraph evaluation **** */

struct Depsgraph;

void BKE_particle_settings_eval_reset(struct Depsgraph *depsgraph,
                                      struct ParticleSettings *particle_settings);

void BKE_particle_system_eval_init(struct Depsgraph *depsgraph, struct Object *object);

/* Draw Cache */
enum {
  BKE_PARTICLE_BATCH_DIRTY_ALL = 0,
};
void BKE_particle_batch_cache_dirty_tag(struct ParticleSystem *psys, int mode);
void BKE_particle_batch_cache_free(struct ParticleSystem *psys);

extern void (*BKE_particle_batch_cache_dirty_tag_cb)(struct ParticleSystem *psys, int mode);
extern void (*BKE_particle_batch_cache_free_cb)(struct ParticleSystem *psys);

/* .blend file I/O */

void BKE_particle_partdeflect_blend_read_data(struct BlendDataReader *reader,
                                              struct PartDeflect *pd);
void BKE_particle_system_blend_write(struct BlendWriter *writer, struct ListBase *particles);
void BKE_particle_system_blend_read_data(struct BlendDataReader *reader,
                                         struct ListBase *particles);
void BKE_particle_system_blend_read_after_liblink(struct BlendLibReader *reader,
                                                  struct Object *ob,
                                                  struct ID *id,
                                                  struct ListBase *particles);
