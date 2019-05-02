/*
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
 * The Original Code is Copyright (C) 2006 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup DNA
 */

#ifndef __DNA_SMOKE_TYPES_H__
#define __DNA_SMOKE_TYPES_H__

/* flags */
enum {
  MOD_SMOKE_HIGHRES = (1 << 1),      /* enable high resolution */
  MOD_SMOKE_DISSOLVE = (1 << 2),     /* let smoke dissolve */
  MOD_SMOKE_DISSOLVE_LOG = (1 << 3), /* using 1/x for dissolve */

#ifdef DNA_DEPRECATED
  MOD_SMOKE_HIGH_SMOOTH = (1 << 5), /* -- Deprecated -- */
#endif
  MOD_SMOKE_FILE_LOAD = (1 << 6), /* flag for file load */
  MOD_SMOKE_ADAPTIVE_DOMAIN = (1 << 7),
};

/* noise */
#define MOD_SMOKE_NOISEWAVE (1 << 0)
#define MOD_SMOKE_NOISEFFT (1 << 1)
#define MOD_SMOKE_NOISECURL (1 << 2)
/* viewsettings */
#define MOD_SMOKE_VIEW_SHOW_HIGHRES (1 << 0)

/* slice method */
enum {
  MOD_SMOKE_SLICE_VIEW_ALIGNED = 0,
  MOD_SMOKE_SLICE_AXIS_ALIGNED = 1,
};

/* axis aligned method */
enum {
  AXIS_SLICE_FULL = 0,
  AXIS_SLICE_SINGLE = 1,
};

/* single slice direction */
enum {
  SLICE_AXIS_AUTO = 0,
  SLICE_AXIS_X = 1,
  SLICE_AXIS_Y = 2,
  SLICE_AXIS_Z = 3,
};

/* axis aligned method */
enum {
  VOLUME_INTERP_LINEAR = 0,
  VOLUME_INTERP_CUBIC = 1,
};

enum {
  VECTOR_DRAW_NEEDLE = 0,
  VECTOR_DRAW_STREAMLINE = 1,
};

enum {
  FLUID_FIELD_DENSITY = 0,
  FLUID_FIELD_HEAT = 1,
  FLUID_FIELD_FUEL = 2,
  FLUID_FIELD_REACT = 3,
  FLUID_FIELD_FLAME = 4,
  FLUID_FIELD_VELOCITY_X = 5,
  FLUID_FIELD_VELOCITY_Y = 6,
  FLUID_FIELD_VELOCITY_Z = 7,
  FLUID_FIELD_COLOR_R = 8,
  FLUID_FIELD_COLOR_G = 9,
  FLUID_FIELD_COLOR_B = 10,
  FLUID_FIELD_FORCE_X = 11,
  FLUID_FIELD_FORCE_Y = 12,
  FLUID_FIELD_FORCE_Z = 13,
};

/* cache compression */
#define SM_CACHE_LIGHT 0
#define SM_CACHE_HEAVY 1

/* domain border collision */
#define SM_BORDER_OPEN 0
#define SM_BORDER_VERTICAL 1
#define SM_BORDER_CLOSED 2

/* collision types */
#define SM_COLL_STATIC 0
#define SM_COLL_RIGID 1
#define SM_COLL_ANIMATED 2

/* high resolution sampling types */
#define SM_HRES_NEAREST 0
#define SM_HRES_LINEAR 1
#define SM_HRES_FULLSAMPLE 2

/* smoke data fields (active_fields) */
#define SM_ACTIVE_HEAT (1 << 0)
#define SM_ACTIVE_FIRE (1 << 1)
#define SM_ACTIVE_COLORS (1 << 2)
#define SM_ACTIVE_COLOR_SET (1 << 3)

enum {
  VDB_COMPRESSION_BLOSC = 0,
  VDB_COMPRESSION_ZIP = 1,
  VDB_COMPRESSION_NONE = 2,
};

typedef struct SmokeDomainSettings {
  /** For fast RNA access. */
  struct SmokeModifierData *smd;
  struct FLUID_3D *fluid;
  void *fluid_mutex;
  struct Collection *fluid_group;
  struct Collection *eff_group;   // UNUSED
  struct Collection *coll_group;  // collision objects group
  struct WTURBULENCE *wt;         // WTURBULENCE object, if active
  struct GPUTexture *tex;
  struct GPUTexture *tex_wt;
  struct GPUTexture *tex_shadow;
  struct GPUTexture *tex_flame;
  struct GPUTexture *tex_flame_coba;
  struct GPUTexture *tex_coba;
  struct GPUTexture *tex_field;
  struct GPUTexture *tex_velocity_x;
  struct GPUTexture *tex_velocity_y;
  struct GPUTexture *tex_velocity_z;
  float *shadow;

  /* simulation data */
  /** Start point of BB in local space (includes sub-cell shift for adaptive domain.)*/
  float p0[3];
  /** End point of BB in local space. */
  float p1[3];
  /** Difference from object center to grid start point. */
  float dp0[3];
  /** Size of simulation cell in local space. */
  float cell_size[3];
  /** Global size of domain axises. */
  float global_size[3];
  float prev_loc[3];
  /** Current domain shift in simulation cells. */
  int shift[3];
  /** Exact domain shift. */
  float shift_f[3];
  /**
   * How much object has shifted since previous smoke frame
   * (used to "lock" domain while drawing).
   */
  float obj_shift_f[3];
  /** Domain object imat. */
  float imat[4][4];
  /** Domain obmat. */
  float obmat[4][4];
  /** Low res fluid matrix. */
  float fluidmat[4][4];
  /** High res fluid matrix. */
  float fluidmat_wt[4][4];

  /** Initial "non-adapted" resolution. */
  int base_res[3];
  /** Cell min. */
  int res_min[3];
  /** Cell max. */
  int res_max[3];
  /** Data resolution (res_max-res_min). */
  int res[3];
  int total_cells;
  /** 1.0f / res. */
  float dx;
  /** Largest domain size. */
  float scale;

  /* user settings */
  int adapt_margin;
  int adapt_res;
  float adapt_threshold;

  float alpha;
  float beta;
  /** Wavelet amplification. */
  int amplify;
  /** Longest axis on the BB gets this resolution assigned. */
  int maxres;
  /** Show up-res or low res, etc. */
  int flags;
  int viewsettings;
  /** Noise type: wave, curl, anisotropic. */
  short noise;
  short diss_percent;
  /** In frames. */
  int diss_speed;
  float strength;
  int res_wt[3];
  float dx_wt;
  /* point cache options */
  int cache_comp;
  int cache_high_comp;
  /* OpenVDB cache options */
  int openvdb_comp;
  char cache_file_format;
  char data_depth;
  char _pad[2];

  /* Smoke uses only one cache from now on (index [0]),
   * but keeping the array for now for reading old files. */
  /** Definition is in DNA_object_force_types.h. */
  struct PointCache *point_cache[2];
  struct ListBase ptcaches[2];
  struct EffectorWeights *effector_weights;
  /** How domain border collisions are handled. */
  int border_collisions;
  float time_scale;
  float vorticity;
  int active_fields;
  /** Monitor color situation of simulation. */
  float active_color[3];
  int highres_sampling;

  /* flame parameters */
  float burning_rate, flame_smoke, flame_vorticity;
  float flame_ignition, flame_max_temp;
  float flame_smoke_color[3];

  /* Display settings */
  char slice_method, axis_slice_method;
  char slice_axis, draw_velocity;
  float slice_per_voxel;
  float slice_depth;
  float display_thickness;

  struct ColorBand *coba;
  float vector_scale;
  char vector_draw_type;
  char use_coba;
  /** Simulation field used for the color mapping. */
  char coba_field;
  char interp_method;

  float clipping;
  char _pad3[4];
} SmokeDomainSettings;

/* inflow / outflow */

/* type */
#define MOD_SMOKE_FLOW_TYPE_SMOKE 0
#define MOD_SMOKE_FLOW_TYPE_FIRE 1
#define MOD_SMOKE_FLOW_TYPE_OUTFLOW 2
#define MOD_SMOKE_FLOW_TYPE_SMOKEFIRE 3

/* flow source */
#define MOD_SMOKE_FLOW_SOURCE_PARTICLES 0
#define MOD_SMOKE_FLOW_SOURCE_MESH 1

/* flow texture type */
#define MOD_SMOKE_FLOW_TEXTURE_MAP_AUTO 0
#define MOD_SMOKE_FLOW_TEXTURE_MAP_UV 1

/* flags */
enum {
  /** Old style emission. */
  MOD_SMOKE_FLOW_ABSOLUTE = (1 << 1),
  /** Passes particles speed to the smoke. */
  MOD_SMOKE_FLOW_INITVELOCITY = (1 << 2),
  /** Use texture to control emission speed. */
  MOD_SMOKE_FLOW_TEXTUREEMIT = (1 << 3),
  /** Use specific size for particles instead of closest cell. */
  MOD_SMOKE_FLOW_USE_PART_SIZE = (1 << 4),
};

typedef struct SmokeFlowSettings {
  /** For fast RNA access. */
  struct SmokeModifierData *smd;
  struct Mesh *mesh;
  struct ParticleSystem *psys;
  struct Tex *noise_texture;

  /* initial velocity */
  /** Previous vertex positions in domain space. */
  float *verts_old;
  int numverts;
  float vel_multi;  // Multiplier for inherited velocity
  float vel_normal;
  float vel_random;
  /* emission */
  float density;
  float color[3];
  float fuel_amount;
  /** Delta temperature (temp - ambient temp). */
  float temp;
  /** Density emitted within mesh volume. */
  float volume_density;
  /** Maximum emission distance from mesh surface. */
  float surface_distance;
  float particle_size;
  int subframes;
  /* texture control */
  float texture_size;
  float texture_offset;
  char _pad[4];
  /** MAX_CUSTOMDATA_LAYER_NAME. */
  char uvlayer_name[64];
  short vgroup_density;

  /** Smoke, flames, both, outflow. */
  short type;
  short source;
  short texture_type;
  /** Absolute emission et.c*/
  int flags;
} SmokeFlowSettings;

// struct BVHTreeFromMesh *bvh;
// float mat[4][4];
// float mat_old[4][4];

/* collision objects (filled with smoke) */
typedef struct SmokeCollSettings {
  /** For fast RNA access. */
  struct SmokeModifierData *smd;
  struct Mesh *mesh;
  float *verts_old;
  int numverts;
  short type;  // static = 0, rigid = 1, dynamic = 2
  char _pad[2];
} SmokeCollSettings;

#endif
