/* SPDX-FileCopyrightText: 2004-2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_defs.h"
#include "DNA_listBase.h"

namespace blender {

struct BodySpring;

/** #EffectorWeights::flag */
enum {
  EFF_WEIGHT_DO_HAIR = 1,
};

/** #PartDeflect::flag: various settings. */
enum {
  PFIELD_USEMAX = 1 << 0,
  // PDEFLE_DEFORM = 1 << 1, /* UNUSED */
  /** TODO: do_versions for below */
  PFIELD_GUIDE_PATH_ADD = 1 << 2,
  /** used for do_versions */
  PFIELD_PLANAR = 1 << 3,
  PDEFLE_KILL_PART = 1 << 4,
  /** used for do_versions */
  PFIELD_POSZ = 1 << 5,
  PFIELD_TEX_OBJECT = 1 << 6,
  /** used for turbulence */
  PFIELD_GLOBAL_CO = 1 << 6,
  PFIELD_TEX_2D = 1 << 7,
  /** used for harmonic force */
  PFIELD_MULTIPLE_SPRINGS = 1 << 7,
  PFIELD_USEMIN = 1 << 8,
  PFIELD_USEMAXR = 1 << 9,
  PFIELD_USEMINR = 1 << 10,
  PFIELD_TEX_ROOTCO = 1 << 11,
  /** used for do_versions */
  PFIELD_SURFACE = 1 << 12,
  PFIELD_VISIBILITY = 1 << 13,
  PFIELD_DO_LOCATION = 1 << 14,
  PFIELD_DO_ROTATION = 1 << 15,
  /** apply curve weights */
  PFIELD_GUIDE_PATH_WEIGHT = 1 << 16,
  /** multiply smoke force by density */
  PFIELD_SMOKE_DENSITY = 1 << 17,
  /** used for (simple) force */
  PFIELD_GRAVITATION = 1 << 18,
  /** Enable cloth collision side detection based on normal. */
  PFIELD_CLOTH_USE_CULLING = 1 << 19,
  /** Replace collision direction with collider normal. */
  PFIELD_CLOTH_USE_NORMAL = 1 << 20,
};

/** #PartDeflect::falloff */
enum {
  PFIELD_FALL_SPHERE = 0,
  PFIELD_FALL_TUBE = 1,
  PFIELD_FALL_CONE = 2,
};

/** #PartDeflect::shape */
enum {
  PFIELD_SHAPE_POINT = 0,
  PFIELD_SHAPE_PLANE = 1,
  PFIELD_SHAPE_SURFACE = 2,
  PFIELD_SHAPE_POINTS = 3,
  PFIELD_SHAPE_LINE = 4,
};

/** #PartDeflect::tex_mode */
enum {
  PFIELD_TEX_RGB = 0,
  PFIELD_TEX_GRAD = 1,
  PFIELD_TEX_CURL = 2,
};

/** #PartDeflect::zdir */
enum {
  PFIELD_Z_BOTH = 0,
  PFIELD_Z_POS = 1,
  PFIELD_Z_NEG = 2,
};

/** #Object::softflag */
enum {
  OB_SB_ENABLE = 1 << 0, /* Deprecated (use modifier). */
  OB_SB_GOAL = 1 << 1,
  OB_SB_EDGES = 1 << 2,
  OB_SB_QUADS = 1 << 3,
  OB_SB_POSTDEF = 1 << 4,
  // OB_SB_REDO = 1 << 5,
  // OB_SB_BAKESET = 1 << 6,
  // OB_SB_BAKEDO = 1 << 7,
  // OB_SB_RESET = 1 << 8,
  OB_SB_SELF = 1 << 9,
  OB_SB_FACECOLL = 1 << 10,
  OB_SB_EDGECOLL = 1 << 11,
  // OB_SB_COLLFINAL = 1 << 12,  /* Deprecated. */
  // OB_SB_BIG_UI = 1 << 13,     /* Deprecated. */
  OB_SB_AERO_ANGLE = 1 << 14,
};

/** #SoftBody::solverflags */
enum {
  SBSO_MONITOR = 1 << 0,
  SBSO_OLDERR = 1 << 1,
  SBSO_ESTIMATEIPO = 1 << 2,
};

/** #SoftBody::sbc_mode */
enum {
  SBC_MODE_MANUAL = 0,
  SBC_MODE_AVG = 1,
  SBC_MODE_MIN = 2,
  SBC_MODE_MAX = 3,
  SBC_MODE_AVGMINMAX = 4,
};

/** #PartDeflect.forcefield: Effector Fields types. */
enum ePFieldType {
  /** (this is used for general effector weight). */
  PFIELD_NULL = 0,
  /** Force away/towards a point depending on force strength. */
  PFIELD_FORCE = 1,
  /** Force around the effector normal. */
  PFIELD_VORTEX = 2,
  /** Force from the cross product of effector normal and point velocity. */
  PFIELD_MAGNET = 3,
  /** Force away and towards a point depending which side of the effector normal the point is. */
  PFIELD_WIND = 4,
  /** Force along curve for dynamics, a shaping curve for hair paths. */
  PFIELD_GUIDE = 5,
  /** Force based on texture values calculated at point coordinates. */
  PFIELD_TEXTURE = 6,
  /** Force of a harmonic (damped) oscillator. */
  PFIELD_HARMONIC = 7,
  /** Force away/towards a point depending on point charge. */
  PFIELD_CHARGE = 8,
  /** Force due to a Lennard-Jones potential. */
  PFIELD_LENNARDJ = 9,
  /** Defines predator / goal for boids. */
  PFIELD_BOID = 10,
  /** Force defined by BLI_noise_generic_turbulence. */
  PFIELD_TURBULENCE = 11,
  /** Linear & quadratic drag. */
  PFIELD_DRAG = 12,
  /** Force based on fluid simulation velocities. */
  PFIELD_FLUIDFLOW = 13,

  /* Keep last. */
  NUM_PFIELD_TYPES,
};

struct PartDeflect {
  /** General settings flag. */
  int flag = 0;
  /** Deflection flag - does mesh deflect particles. */
  short deflect = 0;
  /** Force field type, do the vertices attract / repel particles? */
  short forcefield = 0;
  /** Fall-off type. */
  short falloff = 0;
  /** Point, plane or surface. */
  short shape = 0;
  /** Texture effector. */
  short tex_mode = 0;
  /** For curve guide. */
  short kink = 0, kink_axis = 0;
  short zdir = 0;

  /* Main effector values */
  /** The strength of the force (+ or - ). */
  float f_strength = 0;
  /** Damping ratio of the harmonic effector. */
  float f_damp = 0;
  /**
   * How much force is converted into "air flow", i.e.
   * force used as the velocity of surrounding medium. */
  float f_flow = 0;
  /** How much force is reduced when acting parallel to a surface, e.g. cloth. */
  float f_wind_factor = 0;

  char _pad0[4] = {};

  /** Noise size for noise effector, restlength for harmonic effector. */
  float f_size = 0;

  /* fall-off */
  /** The power law - real gravitation is 2 (square). */
  float f_power = 0;
  /** If indicated, use this maximum. */
  float maxdist = 0;
  /** If indicated, use this minimum. */
  float mindist = 0;
  /** Radial fall-off power. */
  float f_power_r = 0;
  /** Radial versions of above. */
  float maxrad = 0;
  float minrad = 0;

  /* particle collisions */
  /** Damping factor for particle deflection. */
  float pdef_damp = 0;
  /** Random element of damping for deflection. */
  float pdef_rdamp = 0;
  /** Chance of particle passing through mesh. */
  float pdef_perm = 0;
  /** Friction factor for particle deflection. */
  float pdef_frict = 0;
  /** Random element of friction for deflection. */
  float pdef_rfrict = 0;
  /** Surface particle stickiness. */
  float pdef_stickness = 0;

  /** Used for forces. */
  float absorption = 0;

  /* softbody collisions */
  /** Damping factor for softbody deflection. */
  float pdef_sbdamp = 0;
  /** Inner face thickness for softbody deflection. */
  float pdef_sbift = 0;
  /** Outer face thickness for softbody deflection. */
  float pdef_sboft = 0;

  /* guide curve, same as for particle child effects */
  float clump_fac = 0, clump_pow = 0;
  float kink_freq = 0, kink_shape = 0, kink_amp = 0, free_end = 0;

  /* texture effector */
  /** Used for calculating partial derivatives. */
  float tex_nabla = 0;
  /** Texture of the texture effector. */
  struct Tex *tex = nullptr;

  /* effector noise */
  /** Noise of force. */
  float f_noise = 0;
  /** Noise random seed. */
  int seed = 0;

  /* Display Size */
  /** Runtime only : start of the curve or draw scale. */
  float drawvec1[4] = {};
  /** Runtime only : end of the curve. */
  float drawvec2[4] = {};
  /** Runtime only. */
  float drawvec_falloff_min[3] = {};
  char _pad1[4] = {};
  /** Runtime only. */
  float drawvec_falloff_max[3] = {};
  char _pad2[4] = {};

  /** Force source object. */
  struct Object *f_source = nullptr;

  /** Friction of cloth collisions. */
  float pdef_cfrict = 0;
  char _pad[4] = {};
};

struct EffectorWeights {
  /** Only use effectors from this group of objects. */
  struct Collection *group = nullptr;

  /** Effector type specific weights. */
  float weight[14] = {};
  float global_gravity = 0;
  short flag = 0;
  char _pad[2] = {};
};

struct SBVertex {
  float vec[4] = {};
};

/* Container for data that is shared among evaluated copies.
 *
 * This is placed in a separate struct so that values can be changed
 * without having to update all evaluated copies. */
struct SoftBody_Shared {
  struct PointCache *pointcache = nullptr;
  ListBaseT<PointCache> ptcaches = {nullptr, nullptr};
};

struct SoftBody {
  DNA_DEFINE_CXX_METHODS(SoftBody)

  /* dynamic data */
  int totpoint = 0, totspring = 0;
  /** Not saved in file. */
  struct BodyPoint *bpoint = nullptr;
  /** Not saved in file. */
  struct BodySpring *bspring = nullptr;
  char _pad = {};
  char msg_lock = 0;
  short msg_value = 0;

  /* part of UI: */

  /* general options */
  /** Softbody mass of *vertex*. */
  float nodemass = 0;
  /**
   * Along with it introduce mass painting starting to fix old bug .. nastiness that VG are indexes
   * rather find them by name tag to find it -> jow20090613.
   */
  char namedVG_Mass[/*MAX_VGROUP_NAME*/ 64] = "";
  /** Softbody amount of gravitation to apply. */
  float grav = 0;
  /** Friction to env. */
  float mediafrict = 0;
  /** Error limit for ODE solver. */
  float rklimit = 0;
  /** User control over simulation speed. */
  float physics_speed = 0;

  /* goal */
  /** Softbody goal springs. */
  float goalspring = 0;
  /** Softbody goal springs friction. */
  float goalfrict = 0;
  /** Quick limits for goal. */
  float mingoal = 0;
  float maxgoal = 0;
  /** Default goal for vertices without vgroup. */
  float defgoal = 0;
  /** Index starting at 1. */
  short vertgroup = 0;
  /**
   * Starting to fix old bug .. nastiness that VG are indexes
   * rather find them by name tag to find it -> jow20090613.
   */
  char namedVG_Softgoal[/*MAX_VGROUP_NAME*/ 64] = "";

  short fuzzyness = 0;

  /* springs */
  /** Softbody inner springs. */
  float inspring = 0;
  /** Softbody inner springs friction. */
  float infrict = 0;
  /**
   * Along with it introduce Spring_K painting
   * starting to fix old bug .. nastiness that VG are indexes
   * rather find them by name tag to find it -> jow20090613.
   */
  char namedVG_Spring_K[/*MAX_VGROUP_NAME*/ 64] = "";

  /* baking */
  char _pad1[6] = {};
  /** Local==1: use local coords for baking. */
  char local = 0, solverflags = 0;

  /* -- these must be kept for backwards compatibility -- */
  /** Array of size totpointkey. */
  SBVertex **keys = nullptr;
  /** If totpointkey != totpoint or totkey!- (efra-sfra)/interval -> free keys. */
  int totpointkey = 0, totkey = 0;
  /* ---------------------------------------------------- */
  float secondspring = 0;

  /* Self collision. */
  /** Fixed collision ball size if > 0. */
  float colball = 0;
  /** Cooling down collision response. */
  float balldamp = 0;
  /** Pressure the ball is loaded with. */
  float ballstiff = 0;
  short sbc_mode = 0;
  short aeroedge = 0;
  short minloops = 0;
  short maxloops = 0;
  short choke = 0;
  short solver_ID = 0;
  short plastic = 0;
  short springpreload = 0;

  /** Scratchpad/cache on live time not saved in file. */
  struct SBScratch *scratch = nullptr;
  float shearstiff = 0;
  float inpush = 0;

  struct SoftBody_Shared *shared = nullptr;
  /** Moved to SoftBody_Shared. */
  DNA_DEPRECATED struct PointCache *pointcache = nullptr;
  /** Moved to SoftBody_Shared. */
  ListBaseT<PointCache> ptcaches = {nullptr, nullptr};

  struct Collection *collision_group = nullptr;

  struct EffectorWeights *effector_weights = nullptr;
  /* Reverse estimated object-matrix (run-time data, no need to store in the file). */
  float lcom[3] = {};
  float lrot[3][3] = {};
  float lscale[3][3] = {};

  int last_frame = 0;
};

}  // namespace blender
