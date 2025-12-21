/* SPDX-FileCopyrightText: 2007 by Janne Karhu. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_ID.h"
#include "DNA_boid_types.h"
#include "DNA_defs.h"

#ifdef __cplusplus
namespace blender {
template<int DimsNum> struct KDTree;
}  // namespace blender
using KDTree3d = blender::KDTree<3>;
#else
struct KDTree3d;
#endif

struct AnimData;

/** #SPHFluidSettings::flag */
enum {
  SPH_VISCOELASTIC_SPRINGS = 1 << 0,
  SPH_CURRENT_REST_LENGTH = 1 << 1,
  SPH_FAC_REPULSION = 1 << 2,
  SPH_FAC_DENSITY = 1 << 3,
  SPH_FAC_RADIUS = 1 << 4,
  SPH_FAC_VISCOSITY = 1 << 5,
  SPH_FAC_REST_LENGTH = 1 << 6,
};

/** #SPHFluidSettings::solver (numerical ID field, not bit-field). */
enum {
  SPH_SOLVER_DDR = 0,
  SPH_SOLVER_CLASSICAL = 1,
};

enum eParticleDrawFlag {
  PART_DRAW_VEL = (1 << 0),
  PART_DRAW_GLOBAL_OB = (1 << 1),
  PART_DRAW_SIZE = (1 << 2),
#ifdef DNA_DEPRECATED_ALLOW
  /** Render emitter as well. */
  PART_DRAW_EMITTER = (1 << 3), /* DEPRECATED */
#endif
  PART_DRAW_HEALTH = (1 << 4),
  PART_ABS_PATH_TIME = (1 << 5),
  PART_DRAW_COUNT_GR = (1 << 6),
  /* PART_DRAW_BB_LOCK = (1 << 7), */ /* DEPRECATED */
  /* used with billboards */          /* DEPRECATED */
  PART_DRAW_ROTATE_OB = (1 << 7),     /* used with instance object/collection */
  PART_DRAW_PARENT = (1 << 8),
  PART_DRAW_NUM = (1 << 9),
  PART_DRAW_RAND_GR = (1 << 10),
  PART_DRAW_REN_ADAPT = (1 << 11),
  PART_DRAW_VEL_LENGTH = (1 << 12),
  PART_DRAW_MAT_COL = (1 << 13), /* deprecated, but used in do_versions */
  PART_DRAW_WHOLE_GR = (1 << 14),
  PART_DRAW_REN_STRAND = (1 << 15),
  PART_DRAW_NO_SCALE_OB = (1 << 16), /* used with instance object/collection */
  PART_DRAW_GUIDE_HAIRS = (1 << 17),
  PART_DRAW_HAIR_GRID = (1 << 18),
};

/**
 * #ParticleSettings.type
 * Hair is always baked static in object/geometry space.
 * Other types (normal particles) are in global space and not static baked.
 */
enum {
  PART_EMITTER = 0,
  /* REACTOR type currently unused */
  /* PART_REACTOR = 1, */
  PART_HAIR = 2,
  PART_FLUID = 3, /* Deprecated (belonged to ELBEEM). */
  PART_FLUID_FLIP = 4,
  PART_FLUID_SPRAY = 5,
  PART_FLUID_BUBBLE = 6,
  PART_FLUID_FOAM = 7,
  PART_FLUID_TRACER = 8,
  PART_FLUID_SPRAYFOAM = 9,
  PART_FLUID_SPRAYBUBBLE = 10,
  PART_FLUID_FOAMBUBBLE = 11,
  PART_FLUID_SPRAYFOAMBUBBLE = 12,
};

/** Mirroring Mantaflow particle types from particle.h (Mantaflow header). */
enum {
  /* PARTICLE_TYPE_NONE = (0 << 0), */ /* UNUSED */
  /* PARTICLE_TYPE_NEW = (1 << 0), */  /* UNUSED */
  PARTICLE_TYPE_SPRAY = (1 << 1),
  PARTICLE_TYPE_BUBBLE = (1 << 2),
  PARTICLE_TYPE_FOAM = (1 << 3),
  PARTICLE_TYPE_TRACER = (1 << 4),
  PARTICLE_TYPE_DELETE = (1 << 10),
  /* PARTICLE_TYPE_INVALID = (1 << 30), */ /* UNUSED */
};

/** #ParticleSettings.flag */
enum {
  PART_REACT_STA_END = 1 << 0,
  PART_REACT_MULTIPLE = 1 << 1,

  // PART_LOOP = 1 << 2, /* not used anymore */

  /* For dope-sheet. */
  PART_DS_EXPAND = 1 << 3,

  /** Regrow hair for each frame. */
  PART_HAIR_REGROW = 1 << 4,

  /** Show unborn particles. */
  PART_UNBORN = 1 << 5,
  /** Show died particles. */
  PART_DIED = 1 << 6,

  PART_TRAND = 1 << 7,
  /** Particle/face from face areas. */
  PART_EDISTR = 1 << 8,

  /** Calculate particle rotations (and store them in point-cache). */
  PART_ROTATIONS = 1 << 9,
  PART_HAIR_BSPLINE = 1 << 10,
  PART_DIE_ON_COL = 1 << 12,
  /** Swept sphere deflections. */
  PART_SIZE_DEFL = 1 << 13,
  /** Dynamic rotation. */
  PART_ROT_DYN = 1 << 14,

  PART_HIDE_ADVANCED_HAIR = 1 << 15,

  PART_SIZEMASS = 1 << 16,

  // PART_ABS_TIME = 1 << 17,
  // PART_GLOB_TIME = 1 << 18,

  PART_BOIDS_2D = 1 << 19,

  // PART_BRANCHING = 1 << 20,
  // PART_ANIM_BRANCHING = 1 << 21,
  PART_SELF_EFFECT = 1 << 22,

  PART_GRID_HEXAGONAL = 1 << 24,
  PART_GRID_INVERT = 1 << 26,

  PART_CHILD_EFFECT = 1 << 27,
  PART_CHILD_LONG_HAIR = 1 << 28,
  // PART_CHILD_RENDER = 1 << 29, /* UNUSED */
  PART_CHILD_GUIDE = 1 << 30,

};

/** #ParticleSettings::from */
enum {
  PART_FROM_VERT = 0,
  PART_FROM_FACE = 1,
  PART_FROM_VOLUME = 2,
  // PART_FROM_PARTICLE = 3, /* Deprecated. */
  PART_FROM_CHILD = 4,
};

/** #ParticleSettings::distr */
enum {
  PART_DISTR_JIT = 0,
  PART_DISTR_RAND = 1,
  PART_DISTR_GRID = 2,
};

/** #ParticleSettings::phystype */
enum {
  PART_PHYS_NO = 0,
  PART_PHYS_NEWTON = 1,
  PART_PHYS_KEYED = 2,
  PART_PHYS_BOIDS = 3,
  PART_PHYS_FLUID = 4,
};

/** #ParticleSettings::kink */
enum eParticleKink {
  PART_KINK_NO = 0,
  PART_KINK_CURL = 1,
  PART_KINK_RADIAL = 2,
  PART_KINK_WAVE = 3,
  PART_KINK_BRAID = 4,
  PART_KINK_SPIRAL = 5,
};

/** #ParticleSettings::child_flag */
enum eParticleChildFlag {
  PART_CHILD_USE_CLUMP_NOISE = (1 << 0),
  PART_CHILD_USE_CLUMP_CURVE = (1 << 1),
  PART_CHILD_USE_ROUGH_CURVE = (1 << 2),
  PART_CHILD_USE_TWIST_CURVE = (1 << 3),
};

/** #ParticleSettings::shape_flag */
enum eParticleShapeFlag {
  PART_SHAPE_CLOSE_TIP = (1 << 0),
};

/** #ParticleSettings::draw_col */
enum {
  PART_DRAW_COL_NONE = 0,
  PART_DRAW_COL_MAT = 1,
  PART_DRAW_COL_VEL = 2,
  PART_DRAW_COL_ACC = 3,
};

/** #ParticleSettings::time_flag */
enum {
  /** Automatic sub-frames. */
  PART_TIME_AUTOSF = 1 << 0,
};

/** #ParticleSettings::draw_as, #ParticleSettings::ren_as */
enum {
  PART_DRAW_NOT = 0,
  PART_DRAW_DOT = 1,
  PART_DRAW_HALO = 1,
  PART_DRAW_CIRC = 2,
  PART_DRAW_CROSS = 3,
  PART_DRAW_AXIS = 4,
  PART_DRAW_LINE = 5,
  PART_DRAW_PATH = 6,
  PART_DRAW_OB = 7,
  PART_DRAW_GR = 8,
  PART_DRAW_BB = 9, /* Deprecated. */
  PART_DRAW_REND = 10,
};

/** #ParticleSettings::integrator */
enum {
  PART_INT_EULER = 0,
  PART_INT_MIDPOINT = 1,
  PART_INT_RK4 = 2,
  PART_INT_VERLET = 3,
};

/** #ParticleSettings::rotmode */
enum {
  PART_ROT_NOR = 1,
  PART_ROT_VEL = 2,
  PART_ROT_GLOB_X = 3,
  PART_ROT_GLOB_Y = 4,
  PART_ROT_GLOB_Z = 5,
  PART_ROT_OB_X = 6,
  PART_ROT_OB_Y = 7,
  PART_ROT_OB_Z = 8,
  PART_ROT_NOR_TAN = 9,
};

/** #ParticleSettings::avemode */
enum {
  PART_AVE_VELOCITY = 1,
  PART_AVE_RAND = 2,
  PART_AVE_HORIZONTAL = 3,
  PART_AVE_VERTICAL = 4,
  PART_AVE_GLOBAL_X = 5,
  PART_AVE_GLOBAL_Y = 6,
  PART_AVE_GLOBAL_Z = 7,
};

/** #ParticleSettings::reactevent */
enum {
  PART_EVENT_DEATH = 0,
  PART_EVENT_COLLIDE = 1,
  PART_EVENT_NEAR = 2,
};

/** #ParticleSettings::childtype */
enum {
  PART_CHILD_PARTICLES = 1,
  PART_CHILD_FACES = 2,
};

/** #PartialSystem::flag */
enum {
  PSYS_CURRENT = 1 << 0,
  PSYS_GLOBAL_HAIR = 1 << 1,
  PSYS_HAIR_DYNAMICS = 1 << 2,
  PSYS_KEYED_TIMING = 1 << 3,
  // PSYS_ENABLED = 1 << 4, /* Deprecated. */
  /** Signal for updating hair particle mode. */
  PSYS_HAIR_UPDATED = 1 << 5,
  // PSYS_DRAWING = 1 << 6, /* Deprecated. */
  // PSYS_USE_IMAT = 1 << 7, /* Deprecated. */
  /** Remove particle-system as soon as possible. */
  PSYS_DELETE = 1 << 8,
  PSYS_HAIR_DONE = 1 << 9,
  PSYS_KEYED = 1 << 10,
  PSYS_EDITED = 1 << 11,
  // PSYS_PROTECT_CACHE = 1 << 12, /* Deprecated. */
  PSYS_DISABLED = 1 << 13,
  /** Runtime flag. */
  PSYS_OB_ANIM_RESTORE = 1 << 14,
  PSYS_SHARED_CACHES = 1 << 15,
};

/** #ParticleData::flag */
enum {
  PARS_UNEXIST = 1 << 0,
  PARS_NO_DISP = 1 << 1,
  // PARS_STICKY = 1 << 2, /* deprecated */
  PARS_REKEY = 1 << 3,
};

/** #ParticleData::alive */
enum {
  PARS_KILLED = 0, /* Deprecated. */
  PARS_DEAD = 1,
  PARS_UNBORN = 2,
  PARS_ALIVE = 3,
  PARS_DYING = 4,
};

/** #ParticleDupliWeight::flag */
enum {
  PART_DUPLIW_CURRENT = 1,
};

/** #PartialSystem::vg */
enum {
  PSYS_TOT_VG = 13,
};

/** #PartialSystem::vgroup (indices into this array). */
enum {
  PSYS_VG_DENSITY = 0,
  PSYS_VG_VEL = 1,
  PSYS_VG_LENGTH = 2,
  PSYS_VG_CLUMP = 3,
  PSYS_VG_KINK = 4,
  PSYS_VG_ROUGH1 = 5,
  PSYS_VG_ROUGH2 = 6,
  PSYS_VG_ROUGHE = 7,
  PSYS_VG_SIZE = 8,
  PSYS_VG_TAN = 9,
  PSYS_VG_ROT = 10,
  PSYS_VG_EFFECTOR = 11,
  PSYS_VG_TWIST = 12,
};

/** #ParticleTarget::flag */
enum {
  PTARGET_CURRENT = 1,
  PTARGET_VALID = 2,
};

/** #ParticleTarget::mode */
enum {
  PTARGET_MODE_NEUTRAL = 0,
  PTARGET_MODE_FRIEND = 1,
  PTARGET_MODE_ENEMY = 2,
};

/** #MTex::mapto */
enum eParticleTextureInfluence {
  /* init */
  PAMAP_TIME = (1 << 0), /* emission time */
  PAMAP_LIFE = (1 << 1), /* life time */
  PAMAP_DENS = (1 << 2), /* density */
  PAMAP_SIZE = (1 << 3), /* physical size */
  PAMAP_INIT = (PAMAP_TIME | PAMAP_LIFE | PAMAP_DENS | PAMAP_SIZE),
  /* reset */
  PAMAP_IVEL = (1 << 5), /* initial velocity */
  /* physics */
  PAMAP_FIELD = (1 << 6), /* force fields */
  PAMAP_GRAVITY = (1 << 10),
  PAMAP_DAMP = (1 << 11),
  PAMAP_PHYSICS = (PAMAP_FIELD | PAMAP_GRAVITY | PAMAP_DAMP),
  /* children */
  PAMAP_CLUMP = (1 << 7),
  PAMAP_KINK_FREQ = (1 << 8),
  PAMAP_KINK_AMP = (1 << 12),
  PAMAP_ROUGH = (1 << 9),
  PAMAP_LENGTH = (1 << 4),
  PAMAP_TWIST = (1 << 13),
  PAMAP_CHILD = (PAMAP_CLUMP | PAMAP_KINK_FREQ | PAMAP_KINK_AMP | PAMAP_ROUGH | PAMAP_LENGTH |
                 PAMAP_TWIST),
};

struct HairKey {
  /** Location of hair vertex. */
  float co[3];
  /** Time along hair, default 0-100. */
  float time;
  /** Softbody weight. */
  float weight;
  /** Saved particled edit mode flags. */
  short editflag;
  char _pad[2];
  float world_co[3];
};

struct ParticleKey { /* when changed update size of struct to copy_particleKey()!! */
  /** Location. */
  float co[3];
  /** Velocity. */
  float vel[3];
  /** Rotation quaternion. */
  float rot[4];
  /** Angular velocity. */
  float ave[3];
  /** When this key happens. */
  float time;
};

struct BoidParticle {
  struct Object *ground = nullptr;
  struct BoidData data;
  float gravity[3] = {};
  float wander[3] = {};
  char _pad0[4] = {};
};

struct ParticleSpring {
  float rest_length;
  unsigned int particle_index[2], delete_flag;
};

/** Child particles are created around or between parent particles. */
struct ChildParticle {
  /** Face index on the final derived mesh. */
  int num;
  int parent;
  /** Nearest particles to the child, used for the interpolation. */
  int pa[4];
  /** Interpolation weights for the above particles. */
  float w[4];
  /** Face vertex weights and offset. */
  float fuv[4], foffset;
  char _pad0[4];
};

struct ParticleTarget {
  struct ParticleTarget *next = nullptr, *prev = nullptr;
  struct Object *ob = nullptr;
  int psys = 0;
  short flag = 0, mode = 0;
  float time = 0, duration = 0;
};

struct ParticleDupliWeight {
  struct ParticleDupliWeight *next = nullptr, *prev = nullptr;
  struct Object *ob = nullptr;
  short count = 0;
  short flag = 0;
  /** Only updated on file save and used on file load. */
  short index = 0;
  char _pad0[2] = {};
};

struct ParticleData {
  /** Current global coordinates. */
  ParticleKey state;

  /** Previous state. */
  ParticleKey prev_state;

  /** Hair vertices. */
  HairKey *hair = nullptr;

  /** Keyed keys. */
  ParticleKey *keys = nullptr;

  /** Boids data. */
  BoidParticle *boid = nullptr;

  /** Amount of hair or keyed keys. */
  int totkey = 0;

  /** Die-time is not necessarily time+lifetime as. */
  float time = 0, lifetime = 0;
  /**
   * Particles can die unnaturally (collision).
   *
   * \note Particles die on this frame, be sure to add 1 when clamping the lifetime of particles
   * to inclusive ranges such as the scenes end frame. See: #68290.
   */
  float dietime = 0;

  /**
   * WARNING! Those two indices,
   * when not affected to vertices, are for !!! TESSELLATED FACES !!!, not POLYGONS!
   */
  /** Index to vert/edge/face. */
  int num = 0;
  /**
   * Index to derived mesh data (face) to avoid slow lookups. It can also have negative
   * values DMCACHE_NOTFOUND and DMCACHE_ISCHILD.
   */
  int num_dmcache = 0;

  /** Coordinates on face/edge number "num" and depth along. */
  float fuv[4] = {}, foffset = 0;
  /* face normal for volume emission. */

  /** Size and multiplier so that we can update size when ever. */
  float size = 0;

  /** Density of SPH particle. */
  float sphdensity = 0;
  char _pad[4] = {};

  int hair_index = 0;
  short flag = 0;
  /** The life state of a particle. */
  short alive = 0;
};

struct SPHFluidSettings {
  /* Particle Fluid. */
  float radius = 0, spring_k = 0, rest_length = 0;
  float plasticity_constant = 0, yield_ratio = 0;
  float plasticity_balance = 0, yield_balance = 0;
  float viscosity_omega = 0, viscosity_beta = 0;
  float stiffness_k = 0, stiffness_knear = 0, rest_density = 0;
  float buoyancy = 0;
  int flag = 0, spring_frames = 0;
  short solver = 0;
  char _pad[6] = {};
};

struct ParticleSettings {
#ifdef __cplusplus
  /** See #ID_Type comment for why this is here. */
  static constexpr ID_Type id_type = ID_PA;
#endif

  ID id;
  struct AnimData *adt = nullptr;

  struct BoidSettings *boids = nullptr;
  struct SPHFluidSettings *fluid = nullptr;

  struct EffectorWeights *effector_weights = nullptr;
  struct Collection *collision_group = nullptr;

  int flag = PART_EDISTR | PART_TRAND | PART_HIDE_ADVANCED_HAIR;
  char _pad1[4] = {};
  short type = PART_EMITTER, from = PART_FROM_FACE, distr = PART_DISTR_JIT, texact = 0;
  /* physics modes */
  short phystype = PART_PHYS_NEWTON, rotmode = PART_ROT_VEL, avemode = PART_AVE_VELOCITY,
        reactevent = PART_EVENT_DEATH;
  int draw = 0;
  float draw_size = 0.1f;
  short draw_as = PART_DRAW_REND, childtype = 0;
  char _pad2[4] = {};
  short ren_as = PART_DRAW_HALO, subframes = 0, draw_col = PART_DRAW_COL_MAT;
  /* number of path segments, power of 2 except */
  short draw_step = 2, ren_step = 3;
  short hair_step = 5, keys_step = 5;

  /* adaptive path rendering */
  short adapt_angle = 5, adapt_pix = 3;

  short disp = 100, omat = 1, interpolation = 0, integrator = PART_INT_MIDPOINT;
  DNA_DEPRECATED short rotfrom = 0;
  short kink = 0, kink_axis = 2;

  /* billboards */
  DNA_DEPRECATED short bb_align = 0, bb_uv_split = 1, bb_anim = 0, bb_split_offset = 0;
  DNA_DEPRECATED float bb_tilt = 0, bb_rand_tilt = 0, bb_offset[2] = {}, bb_size[2] = {1.0f, 1.0f};
  DNA_DEPRECATED float bb_vel_head = 0;
  DNA_DEPRECATED float bb_vel_tail = 0;

  /* draw color */
  float color_vec_max = 1.0f;

  /* time and emission */
  float sta = 1.0f, end = 200.0f, lifetime = 50.0f, randlife = 0;
  float timetweak = 1.0f, courant_target = 0.2f;
  float jitfac = 1.0f, eff_hair = 0, grid_rand = 0, ps_offset[1] = {};
  int totpart = 1000, userjit = 0, grid_res = 10, effector_amount = 0;
  short time_flag = 0;
  char _pad0[6] = {};

  /* initial velocity factors */
  float normfac = 1.0f, obfac = 0, randfac = 0, partfac = 0, tanfac = 0, tanphase = 0,
        reactfac = 0;
  float ob_vel[3] = {};
  float avefac = 0, phasefac = 0, randrotfac = 0, randphasefac = 0;
  /* physical properties */
  float mass = 1.0f, size = 0.05f, randsize = 0;
  /* global physical properties */
  float acc[3] = {}, dragfac = 0, brownfac = 0, dampfac = 0;
  /* length */
  float randlength = 0;
  /* children */
  int child_flag = 0;
  char _pad3[4] = {};
  int child_percent = 10, child_render_percent = 100;
  float parents = 0, childsize = 1.0f, childrandsize = 0;
  float childrad = 0.2f, childflat = 0.0f;
  /* clumping */
  float clumpfac = 0, clumppow = 0.0f;
  /* kink */
  float kink_amp = 0.2f, kink_freq = 2.0f, kink_shape = 0, kink_flat = 0;
  float kink_amp_clump = 1.0f;
  int kink_extra_steps = 4;
  char _pad4[4] = {};
  float kink_axis_random = 0, kink_amp_random = 0;
  /* rough */
  float rough1 = 0, rough1_size = 1.0f;
  float rough2 = 0, rough2_size = 1.0f, rough2_thres = 0;
  float rough_end = 0, rough_end_shape = 1.0f;
  /* length */
  float clength = 1.0f, clength_thres = 0.0f;
  /* parting */
  float parting_fac = 0;
  float parting_min = 0, parting_max = 0;
  /* branching */
  float branch_thres = 0;
  /* drawing stuff */
  float draw_line[2] = {
      0.5f,
  };
  float path_start = 0.0f, path_end = 1.0f;
  int trail_count = 0;
  /* keyed particles */
  int keyed_loops = 1;
  struct CurveMapping *clumpcurve = nullptr;
  struct CurveMapping *roughcurve = nullptr;
  float clump_noise_size = 1.0f;

  /* hair dynamics */
  float bending_random = 0;

  struct MTex *mtex[/*MAX_MTEX*/ 18] = {};

  struct Collection *instance_collection = nullptr;
  ListBaseT<ParticleDupliWeight> instance_weights = {nullptr, nullptr};
  DNA_DEPRECATED struct Collection *force_group = nullptr; /* deprecated */
  struct Object *instance_object = nullptr;
  struct Object *bb_ob = nullptr;
  struct PartDeflect *pd = nullptr;
  struct PartDeflect *pd2 = nullptr;

  /* Evaluated mesh support. */
  short use_modifier_stack = false;
  char _pad5[2] = {};

  /* hair shape */
  short shape_flag = PART_SHAPE_CLOSE_TIP;
  char _pad6[2] = {};

  float twist = 0;
  char _pad8[4] = {};

  /* hair thickness shape */
  float shape = 0.0f;
  float rad_root = 1.0f, rad_tip = 0.0f, rad_scale = 0.01f;

  struct CurveMapping *twistcurve = nullptr;
};

struct ParticleSystem {
  DNA_DEFINE_CXX_METHODS(ParticleSystem)

  /* note1: make sure all (run-time) are NULL's in `copy_particlesystem` XXX,
   * this function is no more! - need to investigate. */

  /* note2: make sure any uses of this struct in DNA are
   * accounted for in #BKE_object_copy_particlesystems. */

  struct ParticleSystem *next = nullptr, *prev = nullptr;

  /** Particle settings. */
  ParticleSettings *part = nullptr;

  /** (parent) particles. */
  ParticleData *particles = nullptr;
  /** Child particles. */
  ChildParticle *child = nullptr;

  /** Particle editmode (runtime). */
  struct PTCacheEdit *edit = nullptr;
  /** Free callback. */
  void (*free_edit)(struct PTCacheEdit *edit) = nullptr;

  /** Path cache (runtime). */
  struct ParticleCacheKey **pathcache = nullptr;
  /** Child cache (runtime). */
  struct ParticleCacheKey **childcache = nullptr;
  /** Buffers for the above. */
  ListBaseT<LinkData> pathcachebufs, childcachebufs;

  /** Cloth simulation for hair. */
  struct ClothModifierData *clmd = nullptr;
  /** Input/output for cloth simulation. */
  struct Mesh *hair_in_mesh = nullptr, *hair_out_mesh = nullptr;

  struct Object *target_ob = nullptr;

  /** Run-time only lattice deformation data. */
  struct LatticeDeformData *lattice_deform_data = nullptr;

  /** Particles from global space -> parent space. */
  struct Object *parent = nullptr;

  /** Used for keyed and boid physics. */
  ListBaseT<ParticleTarget> targets = {nullptr, nullptr};

  /** Particle system name. */
  char name[/*MAX_NAME*/ 64] = "";

  /** Used for instancing. */
  float imat[4][4] = {};
  float cfra = 0, tree_frame = 0, bvhtree_frame = 0;
  int seed = 0, child_seed = 0;
  int flag = 0, totpart = 0, totunexist = 0, totchild = 0, totcached = 0, totchildcache = 0;
  /* NOTE: Recalc is one of ID_RECALC_PSYS_ALL flags.
   *
   * TODO(sergey): Use #ParticleSettings.id.recalc instead of this duplicated flag somehow. */
  int recalc = 0;
  short target_psys = 0, totkeyed = 0, bakespace = 0;
  char _pad1[6] = {};

  /** Billboard UV name. */
  DNA_DEPRECATED char bb_uvname[3][/*MAX_CUSTOMDATA_LAYER_NAME*/ 68];

  char _pad2[4] = {};
  /* if you change these remember to update array lengths to PSYS_TOT_VG! */
  /** Vertex groups, 0==disable, 1==starting index. */
  short vgroup[13] = {}, vg_neg = 0, rt3 = 0;
  char _pad3[6] = {};

  /* point cache */
  struct PointCache *pointcache = nullptr;
  ListBaseT<PointCache> ptcaches = {nullptr, nullptr};

  struct ListBaseT<struct EffectorCache> *effectors = nullptr;

  ParticleSpring *fluid_springs = nullptr;
  int tot_fluidsprings = 0, alloc_fluidsprings = 0;

  /** Used for interactions with self and other systems. */
  KDTree3d *tree = nullptr;
  /** Used for interactions with self and other systems. */
  struct BVHTree *bvhtree = nullptr;

  struct ParticleDrawData *pdd = nullptr;

  /** Current time step, as a fraction of a frame. */
  float dt_frac = 0;
  /** Influence of the lattice modifier. */
  float lattice_strength = 0;

  void *batch_cache = nullptr;

  /**
   * Set by dependency graph's copy-on-evaluation, allows to quickly go
   * from evaluated particle system to original one.
   *
   * Original system will have this set to NULL.
   *
   * Use #psys_orig_get() function to access.
   */
  struct ParticleSystem *orig_psys = nullptr;
};
