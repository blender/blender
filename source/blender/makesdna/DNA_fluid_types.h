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

#ifndef __DNA_FLUID_TYPES_H__
#define __DNA_FLUID_TYPES_H__

#include "DNA_listBase.h"

/* Domain flags. */
enum {
  FLUID_DOMAIN_USE_NOISE = (1 << 1),        /* Use noise. */
  FLUID_DOMAIN_USE_DISSOLVE = (1 << 2),     /* Let smoke dissolve. */
  FLUID_DOMAIN_USE_DISSOLVE_LOG = (1 << 3), /* Using 1/x for dissolve. */

#ifdef DNA_DEPRECATED_ALLOW
  FLUID_DOMAIN_USE_HIGH_SMOOTH = (1 << 5), /* -- Deprecated -- */
#endif
  FLUID_DOMAIN_FILE_LOAD = (1 << 6), /* Flag for file load. */
  FLUID_DOMAIN_USE_ADAPTIVE_DOMAIN = (1 << 7),
  FLUID_DOMAIN_USE_ADAPTIVE_TIME = (1 << 8),    /* Adaptive time stepping in domain. */
  FLUID_DOMAIN_USE_MESH = (1 << 9),             /* Use mesh. */
  FLUID_DOMAIN_USE_GUIDE = (1 << 10),           /* Use guiding. */
  FLUID_DOMAIN_USE_SPEED_VECTORS = (1 << 11),   /* Generate mesh speed vectors. */
  FLUID_DOMAIN_EXPORT_MANTA_SCRIPT = (1 << 12), /* Export mantaflow script during bake. */
  FLUID_DOMAIN_USE_FRACTIONS = (1 << 13),       /* Use second order obstacles. */
  FLUID_DOMAIN_DELETE_IN_OBSTACLE = (1 << 14),  /* Delete fluid inside obstacles. */
  FLUID_DOMAIN_USE_DIFFUSION = (1 << 15), /* Use diffusion (e.g. viscosity, surface tension). */
  FLUID_DOMAIN_USE_RESUMABLE_CACHE = (1 << 16), /* Determine if cache should be resumable. */
};

/* Border collisions. */
enum {
  FLUID_DOMAIN_BORDER_FRONT = (1 << 1),
  FLUID_DOMAIN_BORDER_BACK = (1 << 2),
  FLUID_DOMAIN_BORDER_RIGHT = (1 << 3),
  FLUID_DOMAIN_BORDER_LEFT = (1 << 4),
  FLUID_DOMAIN_BORDER_TOP = (1 << 5),
  FLUID_DOMAIN_BORDER_BOTTOM = (1 << 6),
};

/* Cache file formats. */
enum {
  FLUID_DOMAIN_FILE_UNI = (1 << 0),
  FLUID_DOMAIN_FILE_OPENVDB = (1 << 1),
  FLUID_DOMAIN_FILE_RAW = (1 << 2),
  FLUID_DOMAIN_FILE_OBJECT = (1 << 3),
  FLUID_DOMAIN_FILE_BIN_OBJECT = (1 << 4),
};

/* Slice method. */
enum {
  FLUID_DOMAIN_SLICE_VIEW_ALIGNED = 0,
  FLUID_DOMAIN_SLICE_AXIS_ALIGNED = 1,
};

/* Axis aligned method. */
enum {
  AXIS_SLICE_FULL = 0,
  AXIS_SLICE_SINGLE = 1,
};

/* Single slice direction. */
enum {
  SLICE_AXIS_AUTO = 0,
  SLICE_AXIS_X = 1,
  SLICE_AXIS_Y = 2,
  SLICE_AXIS_Z = 3,
};

/* Axis aligned method. */
enum {
  VOLUME_INTERP_LINEAR = 0,
  VOLUME_INTERP_CUBIC = 1,
};

enum {
  VECTOR_DRAW_NEEDLE = 0,
  VECTOR_DRAW_STREAMLINE = 1,
};

enum {
  SNDPARTICLE_BOUNDARY_DELETE = 0,
  SNDPARTICLE_BOUNDARY_PUSHOUT = 1,
};

enum {
  SNDPARTICLE_COMBINED_EXPORT_OFF = 0,
  SNDPARTICLE_COMBINED_EXPORT_SPRAY_FOAM = 1,
  SNDPARTICLE_COMBINED_EXPORT_SPRAY_BUBBLE = 2,
  SNDPARTICLE_COMBINED_EXPORT_FOAM_BUBBLE = 3,
  SNDPARTICLE_COMBINED_EXPORT_SPRAY_FOAM_BUBBLE = 4,
};

enum {
  FLUID_DOMAIN_FIELD_DENSITY = 0,
  FLUID_DOMAIN_FIELD_HEAT = 1,
  FLUID_DOMAIN_FIELD_FUEL = 2,
  FLUID_DOMAIN_FIELD_REACT = 3,
  FLUID_DOMAIN_FIELD_FLAME = 4,
  FLUID_DOMAIN_FIELD_VELOCITY_X = 5,
  FLUID_DOMAIN_FIELD_VELOCITY_Y = 6,
  FLUID_DOMAIN_FIELD_VELOCITY_Z = 7,
  FLUID_DOMAIN_FIELD_COLOR_R = 8,
  FLUID_DOMAIN_FIELD_COLOR_G = 9,
  FLUID_DOMAIN_FIELD_COLOR_B = 10,
  FLUID_DOMAIN_FIELD_FORCE_X = 11,
  FLUID_DOMAIN_FIELD_FORCE_Y = 12,
  FLUID_DOMAIN_FIELD_FORCE_Z = 13,
};

/* Fluid domain types. */
enum {
  FLUID_DOMAIN_TYPE_GAS = 0,
  FLUID_DOMAIN_TYPE_LIQUID = 1,
};

/* Smoke noise types. */
enum {
  FLUID_NOISE_TYPE_WAVELET = (1 << 0),
};

/* Mesh levelset generator types. */
enum {
  FLUID_DOMAIN_MESH_IMPROVED = 0,
  FLUID_DOMAIN_MESH_UNION = 1,
};

/* Guiding velocity source. */
enum {
  FLUID_DOMAIN_GUIDE_SRC_DOMAIN = 0,
  FLUID_DOMAIN_GUIDE_SRC_EFFECTOR = 1,
};

/* Fluid data fields (active_fields). */
enum {
  FLUID_DOMAIN_ACTIVE_HEAT = (1 << 0),
  FLUID_DOMAIN_ACTIVE_FIRE = (1 << 1),
  FLUID_DOMAIN_ACTIVE_COLORS = (1 << 2),
  FLUID_DOMAIN_ACTIVE_COLOR_SET = (1 << 3),
  FLUID_DOMAIN_ACTIVE_OBSTACLE = (1 << 4),
  FLUID_DOMAIN_ACTIVE_GUIDE = (1 << 5),
  FLUID_DOMAIN_ACTIVE_INVEL = (1 << 6),
  FLUID_DOMAIN_ACTIVE_OUTFLOW = (1 << 7),
};

/* Particle types. */
enum {
  FLUID_DOMAIN_PARTICLE_FLIP = (1 << 0),
  FLUID_DOMAIN_PARTICLE_SPRAY = (1 << 1),
  FLUID_DOMAIN_PARTICLE_BUBBLE = (1 << 2),
  FLUID_DOMAIN_PARTICLE_FOAM = (1 << 3),
  FLUID_DOMAIN_PARTICLE_TRACER = (1 << 4),
};

/* Liquid simulation methods. */
enum {
  FLUID_DOMAIN_METHOD_FLIP = (1 << 0),
  FLUID_DOMAIN_METHOD_APIC = (1 << 1),
};

/* Cache options. */
enum {
  FLUID_DOMAIN_BAKING_DATA = (1 << 0),
  FLUID_DOMAIN_BAKED_DATA = (1 << 1),
  FLUID_DOMAIN_BAKING_NOISE = (1 << 2),
  FLUID_DOMAIN_BAKED_NOISE = (1 << 3),
  FLUID_DOMAIN_BAKING_MESH = (1 << 4),
  FLUID_DOMAIN_BAKED_MESH = (1 << 5),
  FLUID_DOMAIN_BAKING_PARTICLES = (1 << 6),
  FLUID_DOMAIN_BAKED_PARTICLES = (1 << 7),
  FLUID_DOMAIN_BAKING_GUIDE = (1 << 8),
  FLUID_DOMAIN_BAKED_GUIDE = (1 << 9),
  FLUID_DOMAIN_OUTDATED_DATA = (1 << 10),
  FLUID_DOMAIN_OUTDATED_NOISE = (1 << 11),
  FLUID_DOMAIN_OUTDATED_MESH = (1 << 12),
  FLUID_DOMAIN_OUTDATED_PARTICLES = (1 << 13),
  FLUID_DOMAIN_OUTDATED_GUIDE = (1 << 14),
};

#define FLUID_DOMAIN_BAKING_ALL \
  (FLUID_DOMAIN_BAKING_DATA | FLUID_DOMAIN_BAKING_NOISE | FLUID_DOMAIN_BAKING_MESH | \
   FLUID_DOMAIN_BAKING_PARTICLES | FLUID_DOMAIN_BAKING_GUIDE)

#define FLUID_DOMAIN_BAKED_ALL \
  (FLUID_DOMAIN_BAKED_DATA | FLUID_DOMAIN_BAKED_NOISE | FLUID_DOMAIN_BAKED_MESH | \
   FLUID_DOMAIN_BAKED_PARTICLES | FLUID_DOMAIN_BAKED_GUIDE)

#define FLUID_DOMAIN_DIR_DEFAULT "cache_fluid"
#define FLUID_DOMAIN_DIR_CONFIG "config"
#define FLUID_DOMAIN_DIR_DATA "data"
#define FLUID_DOMAIN_DIR_NOISE "noise"
#define FLUID_DOMAIN_DIR_MESH "mesh"
#define FLUID_DOMAIN_DIR_PARTICLES "particles"
#define FLUID_DOMAIN_DIR_GUIDE "guiding"
#define FLUID_DOMAIN_DIR_SCRIPT "script"
#define FLUID_DOMAIN_SMOKE_SCRIPT "smoke_script.py"
#define FLUID_DOMAIN_LIQUID_SCRIPT "liquid_script.py"
#define FLUID_CACHE_VERSION "C01"

/* Cache file names. */
#define FLUID_NAME_CONFIG "config"
#define FLUID_NAME_DATA "fluid_data"
#define FLUID_NAME_NOISE "fluid_noise"
#define FLUID_NAME_MESH "fluid_mesh"
#define FLUID_NAME_PARTICLES "fluid_particles"
#define FLUID_NAME_GUIDING "fluid_guiding"

/* Fluid object names.*/
#define FLUID_NAME_FLAGS "flags"                   /* == OpenVDB grid attribute name. */
#define FLUID_NAME_VELOCITY "velocity"             /* == OpenVDB grid attribute name. */
#define FLUID_NAME_VELOCITYTMP "velocity_previous" /* == OpenVDB grid attribute name. */
#define FLUID_NAME_VELOCITYX "x_vel"
#define FLUID_NAME_VELOCITYY "y_vel"
#define FLUID_NAME_VELOCITYZ "z_vel"
#define FLUID_NAME_PRESSURE "pressure"
#define FLUID_NAME_PHIOBS "phi_obstacle" /* == OpenVDB grid attribute name. */
#define FLUID_NAME_PHISIN "phiSIn"
#define FLUID_NAME_PHIIN "phi_inflow" /* == OpenVDB grid attribute name. */
#define FLUID_NAME_PHIOUT "phi_out"   /* == OpenVDB grid attribute name. */
#define FLUID_NAME_FORCES "forces"
#define FLUID_NAME_FORCE_X "x_force"
#define FLUID_NAME_FORCE_Y "y_force"
#define FLUID_NAME_FORCE_Z "z_force"
#define FLUID_NAME_NUMOBS "numObs"
#define FLUID_NAME_PHIOBSSIN "phiObsSIn"
#define FLUID_NAME_PHIOBSIN "phi_obstacle_inflow"
#define FLUID_NAME_OBVEL "obvel"
#define FLUID_NAME_OBVELC "obvelC"
#define FLUID_NAME_OBVEL_X "x_obvel"
#define FLUID_NAME_OBVEL_Y "y_obvel"
#define FLUID_NAME_OBVEL_Z "z_obvel"
#define FLUID_NAME_FRACTIONS "fractions"
#define FLUID_NAME_INVELC "invelC"
#define FLUID_NAME_INVEL "invel"
#define FLUID_NAME_INVEL_X "x_invel"
#define FLUID_NAME_INVEL_Y "y_invel"
#define FLUID_NAME_INVEL_Z "z_invel"
#define FLUID_NAME_PHIOUTSIN "phiOutSIn"
#define FLUID_NAME_PHIOUTIN "phi_out_inflow"

/* Smoke object names. */
#define FLUID_NAME_SHADOW "shadow"     /* == OpenVDB grid attribute name. */
#define FLUID_NAME_EMISSION "emission" /* == OpenVDB grid attribute name. */
#define FLUID_NAME_EMISSIONIN "emissionIn"
#define FLUID_NAME_DENSITY "density"          /* == OpenVDB grid attribute name. */
#define FLUID_NAME_DENSITYIN "density_inflow" /* == OpenVDB grid attribute name. */
#define FLUID_NAME_HEAT "heat"
#define FLUID_NAME_HEATIN "heatIn"
#define FLUID_NAME_TEMPERATURE "temperature"          /* == OpenVDB grid attribute name. */
#define FLUID_NAME_TEMPERATUREIN "temperature_inflow" /* == OpenVDB grid attribute name. */
#define FLUID_NAME_COLORR "color_r"                   /* == OpenVDB grid attribute name. */
#define FLUID_NAME_COLORG "color_g"                   /* == OpenVDB grid attribute name. */
#define FLUID_NAME_COLORB "color_b"                   /* == OpenVDB grid attribute name. */
#define FLUID_NAME_COLORRIN "color_r_inflow"          /* == OpenVDB grid attribute name. */
#define FLUID_NAME_COLORGIN "color_g_inflow"          /* == OpenVDB grid attribute name. */
#define FLUID_NAME_COLORBIN "color_b_inflow"          /* == OpenVDB grid attribute name. */
#define FLUID_NAME_FLAME "flame"                      /* == OpenVDB grid attribute name. */
#define FLUID_NAME_FUEL "fuel"                        /* == OpenVDB grid attribute name. */
#define FLUID_NAME_REACT "react"                      /* == OpenVDB grid attribute name. */
#define FLUID_NAME_FUELIN "fuel_inflow"               /* == OpenVDB grid attribute name. */
#define FLUID_NAME_REACTIN "react_inflow"             /* == OpenVDB grid attribute name. */

/* Liquid object names. */
#define FLUID_NAME_PHIPARTS "phi_particles" /* == OpenVDB grid attribute name. */
#define FLUID_NAME_PHI "phi"                /* == OpenVDB grid attribute name. */
#define FLUID_NAME_PHITMP "phi_previous"    /* == OpenVDB grid attribute name. */
#define FLUID_NAME_VELOCITYOLD "velOld"
#define FLUID_NAME_VELOCITYPARTS "velParts"
#define FLUID_NAME_MAPWEIGHTS "mapWeights"
#define FLUID_NAME_PP "pp"
#define FLUID_NAME_PVEL "pVel"
#define FLUID_NAME_PARTS "particles"                  /* == OpenVDB grid attribute name. */
#define FLUID_NAME_PARTSVELOCITY "particles_velocity" /* == OpenVDB grid attribute name. */
#define FLUID_NAME_PINDEX "pindex"
#define FLUID_NAME_GPI "gpi"
#define FLUID_NAME_CURVATURE "gpi"

/* Noise object names. */
#define FLUID_NAME_VELOCITY_NOISE "velocity_noise"
#define FLUID_NAME_DENSITY_NOISE "density_noise" /* == OpenVDB grid attribute name. */
#define FLUID_NAME_PHIIN_NOISE "phiIn_noise"
#define FLUID_NAME_PHIOUT_NOISE "phiOut_noise"
#define FLUID_NAME_PHIOBS_NOISE "phiObs_noise"
#define FLUID_NAME_FLAGS_NOISE "flags_noise"
#define FLUID_NAME_TMPIN_NOISE "tmpIn_noise"
#define FLUID_NAME_EMISSIONIN_NOISE "emissionIn_noise"
#define FLUID_NAME_ENERGY "energy"
#define FLUID_NAME_TMPFLAGS "tmpFlags"
#define FLUID_NAME_TEXTURE_U "textureU"
#define FLUID_NAME_TEXTURE_V "textureV"
#define FLUID_NAME_TEXTURE_W "textureW"
#define FLUID_NAME_TEXTURE_U2 "textureU2"
#define FLUID_NAME_TEXTURE_V2 "textureV2"
#define FLUID_NAME_TEXTURE_W2 "textureW2"
#define FLUID_NAME_UV0 "uv_grid_0"              /* == OpenVDB grid attribute name. */
#define FLUID_NAME_UV1 "uv_grid_1"              /* == OpenVDB grid attribute name. */
#define FLUID_NAME_COLORR_NOISE "color_r_noise" /* == OpenVDB grid attribute name. */
#define FLUID_NAME_COLORG_NOISE "color_g_noise" /* == OpenVDB grid attribute name. */
#define FLUID_NAME_COLORB_NOISE "color_b_noise" /* == OpenVDB grid attribute name. */
#define FLUID_NAME_FLAME_NOISE "flame_noise"
#define FLUID_NAME_FUEL_NOISE "fuel_noise"
#define FLUID_NAME_REACT_NOISE "react_noise"

/* Mesh object names. */
#define FLUID_NAME_PHIPARTS_MESH "phiParts_mesh"
#define FLUID_NAME_PHI_MESH "phi_mesh"
#define FLUID_NAME_PP_MESH "pp_mesh"
#define FLUID_NAME_FLAGS_MESH "flags_mesh"
#define FLUID_NAME_LMESH "lMesh"
#define FLUID_NAME_VELOCITYVEC_MESH "vertex_velocities_mesh" /* == OpenVDB grid attribute name. \
                                                              */
#define FLUID_NAME_VELOCITY_MESH "velocity_mesh"
#define FLUID_NAME_PINDEX_MESH "pindex_mesh"
#define FLUID_NAME_GPI_MESH "gpi_mesh"

/* Particles object names. */
#define FLUID_NAME_PP_PARTICLES "ppSnd"
#define FLUID_NAME_PVEL_PARTICLES "pVelSnd"
#define FLUID_NAME_PLIFE_PARTICLES "pLifeSnd"
#define FLUID_NAME_PFORCE_PARTICLES "pForceSnd"
#define FLUID_NAME_PARTS_PARTICLES "particles_secondary" /* == OpenVDB grid attribute name. */
#define FLUID_NAME_PARTSVEL_PARTICLES \
  "particles_velocity_secondary" /* == OpenVDB grid attribute name. */
#define FLUID_NAME_PARTSLIFE_PARTICLES \
  "particles_life_secondary" /* == OpenVDB grid attribute name. */
#define FLUID_NAME_PARTSFORCE_PARTICLES "particles_force_secondary"
#define FLUID_NAME_VELOCITY_PARTICLES "velocity_secondary"
#define FLUID_NAME_FLAGS_PARTICLES "flags_secondary"
#define FLUID_NAME_PHI_PARTICLES "phi_secondary"
#define FLUID_NAME_PHIOBS_PARTICLES "phiObs_secondary"
#define FLUID_NAME_PHIOUT_PARTICLES "phiOut_secondary"
#define FLUID_NAME_NORMAL_PARTICLES "normal_secondary"
#define FLUID_NAME_NEIGHBORRATIO_PARTICLES "neighbor_ratio_secondary"
#define FLUID_NAME_TRAPPEDAIR_PARTICLES \
  "trapped_air_secondary"                                     /* == OpenVDB grid attribute name. */
#define FLUID_NAME_WAVECREST_PARTICLES "wave_crest_secondary" /* == OpenVDB grid attribute name. \
                                                               */
#define FLUID_NAME_KINETICENERGY_PARTICLES \
  "kinetic_energy_secondary" /* == OpenVDB grid attribute name. */

/* Guiding object names. */
#define FLUID_NAME_VELT "velT"
#define FLUID_NAME_WEIGHTGUIDE "weightGuide"
#define FLUID_NAME_NUMGUIDES "numGuides"
#define FLUID_NAME_PHIGUIDEIN "phiGuideIn"
#define FLUID_NAME_GUIDEVELC "guidevelC"
#define FLUID_NAME_GUIDEVEL_X "x_guidevel"
#define FLUID_NAME_GUIDEVEL_Y "y_guidevel"
#define FLUID_NAME_GUIDEVEL_Z "z_guidevel"
#define FLUID_NAME_GUIDEVEL "velocity_guide"

/* Cache file extensions. */
#define FLUID_DOMAIN_EXTENSION_UNI ".uni"
#define FLUID_DOMAIN_EXTENSION_OPENVDB ".vdb"
#define FLUID_DOMAIN_EXTENSION_RAW ".raw"
#define FLUID_DOMAIN_EXTENSION_OBJ ".obj"
#define FLUID_DOMAIN_EXTENSION_BINOBJ ".bobj.gz"

enum {
  FLUID_DOMAIN_GRID_FLOAT = 0,
  FLUID_DOMAIN_GRID_INT = 1,
  FLUID_DOMAIN_GRID_VEC3F = 2,
};

enum {
  FLUID_DOMAIN_CACHE_FILES_SINGLE = 0,
  FLUID_DOMAIN_CACHE_FILES_COMBINED = 1,
};

enum {
  FLUID_DOMAIN_CACHE_REPLAY = 0,
  FLUID_DOMAIN_CACHE_MODULAR = 1,
  FLUID_DOMAIN_CACHE_ALL = 2,
};

enum {
  VDB_COMPRESSION_BLOSC = 0,
  VDB_COMPRESSION_ZIP = 1,
  VDB_COMPRESSION_NONE = 2,
};

enum {
  VDB_PRECISION_HALF_FLOAT = 0,
  VDB_PRECISION_FULL_FLOAT = 1,
};

/* Deprecated values (i.e. all defines and enums below this line up until typedefs). */
/* Cache compression. */
enum {
  SM_CACHE_LIGHT = 0,
  SM_CACHE_HEAVY = 1,
};

/* High resolution sampling types. */
enum {
  SM_HRES_NEAREST = 0,
  SM_HRES_LINEAR = 1,
  SM_HRES_FULLSAMPLE = 2,
};

typedef struct FluidDomainVertexVelocity {
  float vel[3];
} FluidDomainVertexVelocity;

typedef struct FluidDomainSettings {

  /* -- Runtime-only fields (from here on). -- */

  struct FluidModifierData *mmd; /* For fast RNA access. */
  struct MANTA *fluid;
  struct MANTA *fluid_old; /* Adaptive domain needs access to old fluid state. */
  void *fluid_mutex;
  struct Collection *fluid_group;
  struct Collection *force_group;    /* UNUSED */
  struct Collection *effector_group; /* Effector objects group. */
  struct GPUTexture *tex_density;
  struct GPUTexture *tex_color;
  struct GPUTexture *tex_wt;
  struct GPUTexture *tex_shadow;
  struct GPUTexture *tex_flame;
  struct GPUTexture *tex_flame_coba;
  struct GPUTexture *tex_coba;
  struct GPUTexture *tex_field;
  struct GPUTexture *tex_velocity_x;
  struct GPUTexture *tex_velocity_y;
  struct GPUTexture *tex_velocity_z;
  struct Object *guide_parent;
  /** Vertex velocities of simulated fluid mesh. */
  struct FluidDomainVertexVelocity *mesh_velocities;
  struct EffectorWeights *effector_weights;

  /* Domain object data. */
  float p0[3];          /* Start point of BB in local space
                         * (includes sub-cell shift for adaptive domain). */
  float p1[3];          /* End point of BB in local space. */
  float dp0[3];         /* Difference from object center to grid start point. */
  float cell_size[3];   /* Size of simulation cell in local space. */
  float global_size[3]; /* Global size of domain axises. */
  float prev_loc[3];
  int shift[3];         /* Current domain shift in simulation cells. */
  float shift_f[3];     /* Exact domain shift. */
  float obj_shift_f[3]; /* How much object has shifted since previous smoke frame (used to "lock"
                         * domain while drawing). */
  float imat[4][4];     /* Domain object imat. */
  float obmat[4][4];    /* Domain obmat. */
  float fluidmat[4][4]; /* Low res fluid matrix. */
  float fluidmat_wt[4][4]; /* High res fluid matrix. */
  int base_res[3];         /* Initial "non-adapted" resolution. */
  int res_min[3];          /* Cell min. */
  int res_max[3];          /* Cell max. */
  int res[3];              /* Data resolution (res_max-res_min). */
  int total_cells;
  float dx;           /* 1.0f / res. */
  float scale;        /* Largest domain size. */
  int boundary_width; /* Usually this is just 1. */

  /* -- User-accesible fields (from here on). -- */

  /* Adaptive domain options. */
  int adapt_margin;
  int adapt_res;
  float adapt_threshold;
  char _pad1[4]; /* Unused. */

  /* Fluid domain options */
  int maxres;            /* Longest axis on the BB gets this resolution assigned. */
  int solver_res;        /* Dimension of manta solver, 2d or 3d. */
  int border_collisions; /* How domain border collisions are handled. */
  int flags;             /* Use-mesh, use-noise, etc. */
  float gravity[3];
  int active_fields;
  short type;    /* Gas, liquid. */
  char _pad2[6]; /* Unused. */

  /* Smoke domain options. */
  float alpha;
  float beta;
  int diss_speed; /* In frames. */
  float vorticity;
  float active_color[3]; /* Monitor smoke color. */
  int highres_sampling;

  /* Flame options. */
  float burning_rate, flame_smoke, flame_vorticity;
  float flame_ignition, flame_max_temp;
  float flame_smoke_color[3];

  /* Noise options. */
  float noise_strength;
  float noise_pos_scale;
  float noise_time_anim;
  int res_noise[3];
  int noise_scale;
  short noise_type; /* Noise type: wave, curl, anisotropic. */
  char _pad3[2];    /* Unused. */

  /* Liquid domain options. */
  float particle_randomness;
  int particle_number;
  int particle_minimum;
  int particle_maximum;
  float particle_radius;
  float particle_band_width;
  float fractions_threshold;
  float flip_ratio;
  short simulation_method;
  char _pad4[6];

  /* Diffusion options. */
  float surface_tension;
  float viscosity_base;
  int viscosity_exponent;

  /* Mesh options. */
  float mesh_concave_upper;
  float mesh_concave_lower;
  float mesh_particle_radius;
  int mesh_smoothen_pos;
  int mesh_smoothen_neg;
  int mesh_scale;
  int totvert;
  short mesh_generator;
  char _pad5[6]; /* Unused. */

  /* Secondary particle options. */
  int particle_type;
  int particle_scale;
  float sndparticle_tau_min_wc;
  float sndparticle_tau_max_wc;
  float sndparticle_tau_min_ta;
  float sndparticle_tau_max_ta;
  float sndparticle_tau_min_k;
  float sndparticle_tau_max_k;
  int sndparticle_k_wc;
  int sndparticle_k_ta;
  float sndparticle_k_b;
  float sndparticle_k_d;
  float sndparticle_l_min;
  float sndparticle_l_max;
  int sndparticle_potential_radius;
  int sndparticle_update_radius;
  char sndparticle_boundary;
  char sndparticle_combined_export;
  char _pad6[6]; /* Unused. */

  /* Fluid guiding options. */
  float guide_alpha;      /* Guiding weight scalar (determines strength). */
  int guide_beta;         /* Guiding blur radius (affects size of vortices). */
  float guide_vel_factor; /* Multiply guiding velocity by this factor. */
  int guide_res[3];       /* Res for velocity guide grids - independent from base res. */
  short guide_source;
  char _pad7[2]; /* Unused. */

  /* Cache options. */
  int cache_frame_start;
  int cache_frame_end;
  int cache_frame_pause_data;
  int cache_frame_pause_noise;
  int cache_frame_pause_mesh;
  int cache_frame_pause_particles;
  int cache_frame_pause_guide;
  int cache_flag;
  char cache_mesh_format;
  char cache_data_format;
  char cache_particle_format;
  char cache_noise_format;
  char cache_directory[1024];
  char error[64]; /* Bake error description. */
  short cache_type;
  char cache_id[4]; /* Run-time only */
  char _pad8[6];

  /* Time options. */
  float dt;
  float time_total;
  float time_per_frame;
  float frame_length;
  float time_scale;
  float cfl_condition;
  int timesteps_minimum;
  int timesteps_maximum;

  /* Display options. */
  char slice_method, axis_slice_method;
  char slice_axis, draw_velocity;
  float slice_per_voxel;
  float slice_depth;
  float display_thickness;
  struct ColorBand *coba;
  float vector_scale;
  char vector_draw_type;
  char use_coba;
  char coba_field; /* Simulation field used for the color mapping. */
  char interp_method;

  /* OpenVDB cache options. */
  int openvdb_compression;
  float clipping;
  char openvdb_data_depth;
  char _pad9[7]; /* Unused. */

  /* -- Deprecated / unsed options (below). -- */

  /* View options. */
  int viewsettings;
  char _pad10[4]; /* Unused. */

  /* Pointcache options. */
  /* Smoke uses only one cache from now on (index [0]), but keeping the array for now for reading
   * old files. */
  struct PointCache *point_cache[2]; /* Definition is in DNA_object_force_types.h. */
  struct ListBase ptcaches[2];
  int cache_comp;
  int cache_high_comp;
  char cache_file_format;
  char _pad11[7]; /* Unused. */

} FluidDomainSettings;

/* Flow types. */
enum {
  FLUID_FLOW_TYPE_SMOKE = 1,
  FLUID_FLOW_TYPE_FIRE = 2,
  FLUID_FLOW_TYPE_SMOKEFIRE = 3,
  FLUID_FLOW_TYPE_LIQUID = 4,
};

/* Flow behavior types. */
enum {
  FLUID_FLOW_BEHAVIOR_INFLOW = 0,
  FLUID_FLOW_BEHAVIOR_OUTFLOW = 1,
  FLUID_FLOW_BEHAVIOR_GEOMETRY = 2,
};

/* Flow source types. */
enum {
  FLUID_FLOW_SOURCE_PARTICLES = 0,
  FLUID_FLOW_SOURCE_MESH = 1,
};

/* Flow texture types. */
enum {
  FLUID_FLOW_TEXTURE_MAP_AUTO = 0,
  FLUID_FLOW_TEXTURE_MAP_UV = 1,
};

/* Flow flags. */
enum {
  /* Old style emission. */
  FLUID_FLOW_ABSOLUTE = (1 << 1),
  /* Passes particles speed to the smoke. */
  FLUID_FLOW_INITVELOCITY = (1 << 2),
  /* Use texture to control emission speed. */
  FLUID_FLOW_TEXTUREEMIT = (1 << 3),
  /* Use specific size for particles instead of closest cell. */
  FLUID_FLOW_USE_PART_SIZE = (1 << 4),
  /* Control when to apply inflow. */
  FLUID_FLOW_USE_INFLOW = (1 << 5),
  /* Control how to initialize flow objects. */
  FLUID_FLOW_USE_PLANE_INIT = (1 << 6),
  /* Notify domain objects about state change (invalidate cache). */
  FLUID_FLOW_NEEDS_UPDATE = (1 << 7),
};

typedef struct FluidFlowSettings {

  /* -- Runtime-only fields (from here on). -- */

  /* For fast RNA access. */
  struct FluidModifierData *mmd;
  struct Mesh *mesh;
  struct ParticleSystem *psys;
  struct Tex *noise_texture;

  /* Initial velocity. */
  /* Previous vertex positions in domain space. */
  float *verts_old;
  int numverts;
  float vel_multi; /* Multiplier for inherited velocity. */
  float vel_normal;
  float vel_random;
  float vel_coord[3];
  char _pad1[4];

  /* -- User-accesible fields (from here on). -- */

  /* Emission. */
  float density;
  float color[3];
  float fuel_amount;
  /* Delta temperature (temp - ambient temp). */
  float temperature;
  /* Density emitted within mesh volume. */
  float volume_density;
  /* Maximum emission distance from mesh surface. */
  float surface_distance;
  float particle_size;
  int subframes;

  /* Texture control. */
  float texture_size;
  float texture_offset;
  char _pad2[4];
  /* MAX_CUSTOMDATA_LAYER_NAME. */
  char uvlayer_name[64];
  short vgroup_density;

  short type;     /* Smoke, flames, both, outflow, liquid.  */
  short behavior; /* Inflow, outflow, static.  */
  short source;
  short texture_type;
  short _pad3[3];
  int flags; /* Absolute emission etc. */
} FluidFlowSettings;

/* Effector types. */
enum {
  FLUID_EFFECTOR_TYPE_COLLISION = 0,
  FLUID_EFFECTOR_TYPE_GUIDE = 1,
};

/* Guiding velocity modes. */
enum {
  FLUID_EFFECTOR_GUIDE_MAX = 0,
  FLUID_EFFECTOR_GUIDE_MIN = 1,
  FLUID_EFFECTOR_GUIDE_OVERRIDE = 2,
  FLUID_EFFECTOR_GUIDE_AVERAGED = 3,
};

/* Effector flags. */
enum {
  /* Control when to apply inflow. */
  FLUID_EFFECTOR_USE_EFFEC = (1 << 1),
  /* Control how to initialize flow objects. */
  FLUID_EFFECTOR_USE_PLANE_INIT = (1 << 2),
  /* Notify domain objects about state change (invalidate cache). */
  FLUID_EFFECTOR_NEEDS_UPDATE = (1 << 3),
};

/* Collision objects (filled with smoke). */
typedef struct FluidEffectorSettings {

  /* -- Runtime-only fields (from here on). -- */

  /* For fast RNA access. */
  struct FluidModifierData *mmd;
  struct Mesh *mesh;
  float *verts_old;
  int numverts;

  /* -- User-accesible fields (from here on). -- */

  float surface_distance; /* Thickness of mesh surface, used in obstacle sdf. */
  int flags;
  int subframes;
  short type;
  char _pad1[6];

  /* Guiding options. */
  float vel_multi; /* Multiplier for object velocity. */
  short guide_mode;
  char _pad2[2];
} FluidEffectorSettings;

#endif
