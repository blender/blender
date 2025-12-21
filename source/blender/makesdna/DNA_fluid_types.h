/* SPDX-FileCopyrightText: 2006 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_listBase.h"
#include "DNA_vec_defaults.h"

#ifdef __cplusplus
namespace blender::gpu {
class Texture;
}  // namespace blender::gpu
using GPUTexture = blender::gpu::Texture;
#else
struct GPUTexture;
#endif

/**
 * #FluidDomainSettings.flags
 * Domain flags.
 */
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
  FLUID_DOMAIN_USE_VISCOSITY = (1 << 17),       /* Use viscosity. */
};

/**
 * #FluidDomainSettings.border_collisions
 * Border collisions.
 */
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

/**
 * #FluidDomainSettings.axis_slice_method
 * Axis aligned method.
 */
enum {
  AXIS_SLICE_FULL = 0,
  AXIS_SLICE_SINGLE = 1,
};

/**
 * #FluidDomainSettings.slice_axis
 * Single slice direction.
 */
enum {
  SLICE_AXIS_AUTO = 0,
  SLICE_AXIS_X = 1,
  SLICE_AXIS_Y = 2,
  SLICE_AXIS_Z = 3,
};

/**
 * #FluidDomainSettings.interp_method
 * Display interpolation method.
 */
enum FLUID_DisplayInterpolationMethod {
  FLUID_DISPLAY_INTERP_LINEAR = 0,
  FLUID_DISPLAY_INTERP_CUBIC = 1,
  FLUID_DISPLAY_INTERP_CLOSEST = 2,
};

/** #FluidDomainSettings.vector_draw_type */
enum {
  VECTOR_DRAW_NEEDLE = 0,
  VECTOR_DRAW_STREAMLINE = 1,
  VECTOR_DRAW_MAC = 2,
};

/** #FluidDomainSettings.vector_draw_mac_components */
enum {
  VECTOR_DRAW_MAC_X = (1 << 0),
  VECTOR_DRAW_MAC_Y = (1 << 1),
  VECTOR_DRAW_MAC_Z = (1 << 2),
};

/**
 * #FluidDomainSettings.vector_field
 * Fluid domain vector fields.
 */
enum FLUID_DisplayVectorField {
  FLUID_DOMAIN_VECTOR_FIELD_VELOCITY = 0,
  FLUID_DOMAIN_VECTOR_FIELD_GUIDE_VELOCITY = 1,
  FLUID_DOMAIN_VECTOR_FIELD_FORCE = 2,
};

/** #FluidDomainSettings.sndparticle_boundary */
enum {
  SNDPARTICLE_BOUNDARY_DELETE = 0,
  SNDPARTICLE_BOUNDARY_PUSHOUT = 1,
};

/** #FluidDomainSettings.sndparticle_combined_export */
enum {
  SNDPARTICLE_COMBINED_EXPORT_OFF = 0,
  SNDPARTICLE_COMBINED_EXPORT_SPRAY_FOAM = 1,
  SNDPARTICLE_COMBINED_EXPORT_SPRAY_BUBBLE = 2,
  SNDPARTICLE_COMBINED_EXPORT_FOAM_BUBBLE = 3,
  SNDPARTICLE_COMBINED_EXPORT_SPRAY_FOAM_BUBBLE = 4,
};

/** #FluidDomainSettings.coba_field */
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
  FLUID_DOMAIN_FIELD_PHI = 14,
  FLUID_DOMAIN_FIELD_PHI_IN = 15,
  FLUID_DOMAIN_FIELD_PHI_OUT = 16,
  FLUID_DOMAIN_FIELD_PHI_OBSTACLE = 17,
  FLUID_DOMAIN_FIELD_FLAGS = 18,
  FLUID_DOMAIN_FIELD_PRESSURE = 19,
};

/**
 * #FluidDomainSettings.gridlines_color_field
 * Fluid grid-line display color field types.
 */
enum {
  FLUID_GRIDLINE_COLOR_TYPE_NONE = 0,
  FLUID_GRIDLINE_COLOR_TYPE_FLAGS = 1,
  FLUID_GRIDLINE_COLOR_TYPE_RANGE = 2,
};

/**
 * #FluidDomainSettings.gridlines_cell_filter
 * Fluid cell types.
 */
enum {
  FLUID_CELL_TYPE_NONE = 0,
  FLUID_CELL_TYPE_FLUID = (1 << 0),
  FLUID_CELL_TYPE_OBSTACLE = (1 << 1),
  FLUID_CELL_TYPE_EMPTY = (1 << 2),
  FLUID_CELL_TYPE_INFLOW = (1 << 3),
  FLUID_CELL_TYPE_OUTFLOW = (1 << 4),
};

/* Fluid domain types. */
enum {
  FLUID_DOMAIN_TYPE_GAS = 0,
  FLUID_DOMAIN_TYPE_LIQUID = 1,
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

/* Fluid object names. */
#define FLUID_NAME_FLAGS "flags"       /* == OpenVDB grid attribute name. */
#define FLUID_NAME_VELOCITY "velocity" /* == OpenVDB grid attribute name. */
#define FLUID_NAME_VEL "vel"
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
/* == OpenVDB grid attribute name. */
#define FLUID_NAME_VELOCITYVEC_MESH "vertex_velocities_mesh"
#define FLUID_NAME_VELOCITY_MESH "velocity_mesh"
#define FLUID_NAME_PINDEX_MESH "pindex_mesh"
#define FLUID_NAME_GPI_MESH "gpi_mesh"

/* Particles object names. */
#define FLUID_NAME_PP_PARTICLES "ppSnd"
#define FLUID_NAME_PVEL_PARTICLES "pVelSnd"
#define FLUID_NAME_PLIFE_PARTICLES "pLifeSnd"
#define FLUID_NAME_PFORCE_PARTICLES "pForceSnd"
/* == OpenVDB grid attribute name. */
#define FLUID_NAME_PARTS_PARTICLES "particles_secondary"
/* == OpenVDB grid attribute name. */
#define FLUID_NAME_PARTSVEL_PARTICLES "particles_velocity_secondary"
/* == OpenVDB grid attribute name. */
#define FLUID_NAME_PARTSLIFE_PARTICLES "particles_life_secondary"
#define FLUID_NAME_PARTSFORCE_PARTICLES "particles_force_secondary"
#define FLUID_NAME_VELOCITY_PARTICLES "velocity_secondary"
#define FLUID_NAME_FLAGS_PARTICLES "flags_secondary"
#define FLUID_NAME_PHI_PARTICLES "phi_secondary"
#define FLUID_NAME_PHIOBS_PARTICLES "phiObs_secondary"
#define FLUID_NAME_PHIOUT_PARTICLES "phiOut_secondary"
#define FLUID_NAME_NORMAL_PARTICLES "normal_secondary"
#define FLUID_NAME_NEIGHBORRATIO_PARTICLES "neighbor_ratio_secondary"
/* == OpenVDB grid attribute name. */
#define FLUID_NAME_TRAPPEDAIR_PARTICLES "trapped_air_secondary"
/* == OpenVDB grid attribute name. */
#define FLUID_NAME_WAVECREST_PARTICLES "wave_crest_secondary"
/* == OpenVDB grid attribute name. */
#define FLUID_NAME_KINETICENERGY_PARTICLES "kinetic_energy_secondary"

/* Guiding object names. */
#define FLUID_NAME_VELT "velT"
#define FLUID_NAME_WEIGHTGUIDE "weightGuide"
#define FLUID_NAME_NUMGUIDES "numGuides"
#define FLUID_NAME_PHIGUIDEIN "phiGuideIn"
#define FLUID_NAME_GUIDEVELC "guidevelC"
#define FLUID_NAME_GUIDEVEL_X "x_guidevel"
#define FLUID_NAME_GUIDEVEL_Y "y_guidevel"
#define FLUID_NAME_GUIDEVEL_Z "z_guidevel"
#define FLUID_NAME_GUIDEVEL "guidevel"
#define FLUID_NAME_VELOCITY_GUIDE "velocity_guide"

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
  VDB_PRECISION_MINI_FLOAT = 2,
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

struct FluidDomainSettings {
  /* -- Runtime-only fields (from here on). -- */

  struct FluidModifierData *fmd = nullptr; /* For fast RNA access. */
  struct MANTA *fluid = nullptr;
  struct MANTA *fluid_old = nullptr; /* Adaptive domain needs access to old fluid state. */
  void *fluid_mutex = nullptr;
  struct Collection *fluid_group = nullptr;
  struct Collection *force_group = nullptr;    /* UNUSED */
  struct Collection *effector_group = nullptr; /* Effector objects group. */
  GPUTexture *tex_density = nullptr;
  GPUTexture *tex_color = nullptr;
  GPUTexture *tex_wt = nullptr;
  GPUTexture *tex_shadow = nullptr;
  GPUTexture *tex_flame = nullptr;
  GPUTexture *tex_flame_coba = nullptr;
  GPUTexture *tex_coba = nullptr;
  GPUTexture *tex_field = nullptr;
  GPUTexture *tex_velocity_x = nullptr;
  GPUTexture *tex_velocity_y = nullptr;
  GPUTexture *tex_velocity_z = nullptr;
  GPUTexture *tex_flags = nullptr;
  GPUTexture *tex_range_field = nullptr;
  struct Object *guide_parent = nullptr;
  struct EffectorWeights *effector_weights = nullptr; /* #BKE_effector_add_weights. */

  /* Domain object data. */
  float p0[3] = {0.0f, 0.0f, 0.0f};        /* Start point of BB in local space
                                            * (includes sub-cell shift for adaptive domain). */
  float p1[3] = {0.0f, 0.0f, 0.0f};        /* End point of BB in local space. */
  float dp0[3] = {0.0f, 0.0f, 0.0f};       /* Difference from object center to grid start point. */
  float cell_size[3] = {0.0f, 0.0f, 0.0f}; /* Size of simulation cell in local space. */
  float global_size[3] = {0.0f, 0.0f, 0.0f}; /* Global size of domain axes. */
  float prev_loc[3] = {0.0f, 0.0f, 0.0f};
  int shift[3] = {0, 0, 0};                    /* Current domain shift in simulation cells. */
  float shift_f[3] = {0.0f, 0.0f, 0.0f};       /* Exact domain shift. */
  float obj_shift_f[3] = {0.0f, 0.0f, 0.0f};   /* How much object has shifted since previous smoke
                                                * frame (used to "lock" domain while drawing). */
  float imat[4][4] = _DNA_DEFAULT_UNIT_M4;     /* Domain object imat. */
  float obmat[4][4] = _DNA_DEFAULT_UNIT_M4;    /* Domain obmat. */
  float fluidmat[4][4] = _DNA_DEFAULT_UNIT_M4; /* Low res fluid matrix. */
  float fluidmat_wt[4][4] = _DNA_DEFAULT_UNIT_M4; /* High res fluid matrix. */
  int base_res[3] = {0, 0, 0};                    /* Initial "non-adapted" resolution. */
  int res_min[3] = {0, 0, 0};                     /* Cell min. */
  int res_max[3] = {0, 0, 0};                     /* Cell max. */
  int res[3] = {0, 0, 0};                         /* Data resolution (res_max-res_min). */
  int total_cells = 0;
  float dx = 0;           /* 1.0f / res. */
  float scale = 0.0f;     /* Largest domain size. */
  int boundary_width = 1; /* Usually this is just 1. */
  float gravity_final[3] = {
      0.0f, 0.0f, 0.0f}; /* Scene or domain gravity multiplied with gravity weight. */

  /* -- User-accessible fields (from here on). -- */

  /* Adaptive domain options. */
  int adapt_margin = 4;
  int adapt_res = 0;
  float adapt_threshold = 0.002f;

  /* Fluid domain options */
  int maxres = 32;           /* Longest axis on the BB gets this resolution assigned. */
  int solver_res = 3;        /* Dimension of manta solver, 2d or 3d. */
  int border_collisions = 0; /* How domain border collisions are handled. */
  int flags = FLUID_DOMAIN_USE_DISSOLVE_LOG | FLUID_DOMAIN_USE_ADAPTIVE_TIME |
              FLUID_DOMAIN_USE_MESH; /* Use-mesh, use-noise, etc. */
  float gravity[3] = {0.0f, 0.0f, -9.81f};
  int active_fields = 0;
  short type = FLUID_DOMAIN_TYPE_GAS; /* Gas, liquid. */
  char _pad2[6] = {};                 /* Unused. */

  /* Smoke domain options. */
  float alpha = 1.0f;
  float beta = 1.0f;
  int diss_speed = 5; /* In frames. */
  float vorticity = 0.0f;
  float active_color[3] = {0.0f, 0.0f, 0.0f}; /* Monitor smoke color. */
  int highres_sampling = SM_HRES_FULLSAMPLE;

  /* Flame options. */
  float burning_rate = 0.75f, flame_smoke = 1.0f, flame_vorticity = 0.5f;
  float flame_ignition = 1.5f, flame_max_temp = 3.0f;
  float flame_smoke_color[3] = {0.7f, 0.7f, 0.7f};

  /* Noise options. */
  float noise_strength = 1.0f;
  float noise_pos_scale = 2.0f;
  float noise_time_anim = 0.1f;
  int res_noise[3] = {0, 0, 0};
  int noise_scale = 2;
  char _pad3[4] = {}; /* Unused. */

  /* Liquid domain options. */
  float particle_randomness = 0.1f;
  int particle_number = 2;
  int particle_minimum = 8;
  int particle_maximum = 16;
  float particle_radius = 1.0f;
  float particle_band_width = 3.0f;
  float fractions_threshold = 0.05f;
  float fractions_distance = 0.5f;
  float flip_ratio = 0.97f;
  int sys_particle_maximum = 0;
  short simulation_method = FLUID_DOMAIN_METHOD_FLIP;
  char _pad4[6] = {};

  /* Viscosity options. */
  float viscosity_value = 0.05f;
  char _pad5[4] = {};

  /* Diffusion options. */
  float surface_tension = 0.0f;
  float viscosity_base = 1.0f;
  int viscosity_exponent = 6.0f;

  /* Mesh options. */
  float mesh_concave_upper = 3.5f;
  float mesh_concave_lower = 0.4f;
  float mesh_particle_radius = 2.0f;
  int mesh_smoothen_pos = 1;
  int mesh_smoothen_neg = 1;
  int mesh_scale = 2;
  short mesh_generator = FLUID_DOMAIN_MESH_IMPROVED;
  char _pad6[2] = {}; /* Unused. */

  /* Secondary particle options. */
  int particle_type = 0;
  int particle_scale = 1;
  float sndparticle_tau_min_wc = 2.0f;
  float sndparticle_tau_max_wc = 8.0f;
  float sndparticle_tau_min_ta = 5.0f;
  float sndparticle_tau_max_ta = 20.0f;
  float sndparticle_tau_min_k = 1.0f;
  float sndparticle_tau_max_k = 5.0f;
  int sndparticle_k_wc = 200;
  int sndparticle_k_ta = 40;
  float sndparticle_k_b = 0.5f;
  float sndparticle_k_d = 0.6f;
  float sndparticle_l_min = 10.0f;
  float sndparticle_l_max = 25.0f;
  int sndparticle_potential_radius = 2;
  int sndparticle_update_radius = 2;
  char sndparticle_boundary = SNDPARTICLE_BOUNDARY_DELETE;
  char sndparticle_combined_export = SNDPARTICLE_COMBINED_EXPORT_OFF;
  char _pad7[6] = {}; /* Unused. */

  /* Fluid guiding options. */
  float guide_alpha = 2.0f;      /* Guiding weight scalar (determines strength). */
  int guide_beta = 5;            /* Guiding blur radius (affects size of vortices). */
  float guide_vel_factor = 2.0f; /* Multiply guiding velocity by this factor. */
  int guide_res[3] = {0, 0, 0};  /* Res for velocity guide grids - independent from base res. */
  short guide_source = FLUID_DOMAIN_GUIDE_SRC_DOMAIN;
  char _pad8[2] = {}; /* Unused. */

  /* Cache options. */
  int cache_frame_start = 1;
  int cache_frame_end = 250;
  int cache_frame_pause_data = 0;
  int cache_frame_pause_noise = 0;
  int cache_frame_pause_mesh = 0;
  int cache_frame_pause_particles = 0;
  int cache_frame_pause_guide = 0;
  int cache_frame_offset = 0;
  int cache_flag = 0;
  char cache_mesh_format = FLUID_DOMAIN_FILE_BIN_OBJECT;
  char cache_data_format = FLUID_DOMAIN_FILE_OPENVDB;
  char cache_particle_format = FLUID_DOMAIN_FILE_OPENVDB;
  char cache_noise_format = FLUID_DOMAIN_FILE_OPENVDB;
  char cache_directory[/*FILE_MAX*/ 1024] = "";
  char error[64] = ""; /* Bake error description. */
  short cache_type = FLUID_DOMAIN_CACHE_REPLAY;
  char cache_id[4] = ""; /* Run-time only */
  char _pad9[2] = {};    /* Unused. */

  /* Time options. */
  float dt = 0.0f;
  float time_total = 0.0f;
  float time_per_frame = 0.0f;
  float frame_length = 0.0f;
  float time_scale = 1.0f;
  float cfl_condition = 2.0f;
  int timesteps_minimum = 1;
  int timesteps_maximum = 4;

  /* Display options. */
  float slice_per_voxel = 5.0f;
  float slice_depth = 0.5f;
  float display_thickness = 1.0f;
  float grid_scale = 1.0f;
  struct ColorBand *coba = nullptr;
  float vector_scale = 1.0f;
  float gridlines_lower_bound = 0.0f;
  float gridlines_upper_bound = 1.0f;
  float gridlines_range_color[4] = {1.0f, 0.0f, 0.0f, 1.0f};
  char axis_slice_method = AXIS_SLICE_FULL;
  char slice_axis = 0;
  char show_gridlines = false;
  char draw_velocity = false;
  char vector_draw_type = VECTOR_DRAW_NEEDLE;
  char vector_field =
      FLUID_DOMAIN_VECTOR_FIELD_VELOCITY; /* Simulation field used for vector display. */
  char vector_scale_with_magnitude = true;
  char vector_draw_mac_components = VECTOR_DRAW_MAC_X | VECTOR_DRAW_MAC_Y | VECTOR_DRAW_MAC_Z;
  char use_coba = false;
  char coba_field = FLUID_DOMAIN_FIELD_DENSITY; /* Simulation field used for the color mapping. */
  char interp_method = FLUID_DISPLAY_INTERP_LINEAR;
  char gridlines_color_field = 0; /* Simulation field used to color map onto gridlines. */
  char gridlines_cell_filter = FLUID_CELL_TYPE_NONE;
  char _pad10[3] = {}; /* Unused. */

  /* Velocity factor for motion blur rendering. */
  float velocity_scale = 1.0f;

  /* OpenVDB cache options. */
  int openvdb_compression = VDB_COMPRESSION_BLOSC;
  float clipping = 1e-6f;
  char openvdb_data_depth = 0;
  char _pad11[7] = {}; /* Unused. */

  /* -- Deprecated / unused options (below). -- */

  /* View options. */
  int viewsettings = 0;
  char _pad12[4] = {}; /* Unused. */

  /**
   * Point-cache options.
   * Smoke uses only one cache from now on (index [0]),
   * but keeping the array for now for reading old files.
   */
  struct PointCache *point_cache[2] = {nullptr, nullptr}; /* Use #BKE_ptcache_add. */
  struct ListBaseT<PointCache> ptcaches[2] = {
    {nullptr, nullptr}, {nullptr, nullptr},
  };
  int cache_comp = SM_CACHE_LIGHT;
  int cache_high_comp = SM_CACHE_LIGHT;
  char cache_file_format = 0;
  char _pad13[7] = {}; /* Unused. */
};

struct FluidFlowSettings {

  /* -- Runtime-only fields (from here on). -- */

  /* For fast RNA access. */
  struct FluidModifierData *fmd = nullptr;
  struct Mesh *mesh = nullptr;
  struct ParticleSystem *psys = nullptr;
  struct Tex *noise_texture = nullptr;

  /* Initial velocity. */
  /* Previous vertex positions in domain space. */
  float *verts_old = nullptr;
  int numverts = 0;
  float vel_multi = 1.0f; /* Multiplier for inherited velocity. */
  float vel_normal = 0.0f;
  float vel_random = 0.0f;
  float vel_coord[3] = {0.0f, 0.0f, 0.0f};
  char _pad1[4] = {};

  /* -- User-accessible fields (from here on). -- */

  /* Emission. */
  float density = 1.0f;
  float color[3] = {0.7f, 0.7f, 0.7f};
  float fuel_amount = 1.0f;
  /* Delta temperature (temp - ambient temp). */
  float temperature = 1.0f;
  /* Density emitted within mesh volume. */
  float volume_density = 0.0f;
  /* Maximum emission distance from mesh surface. */
  float surface_distance = 1.0f;
  float particle_size = 1.0f;
  int subframes = 0;

  /* Texture control. */
  float texture_size = 1.0f;
  float texture_offset = 0.0f;
  char _pad2[4] = {};
  char uvlayer_name[/*MAX_CUSTOMDATA_LAYER_NAME*/ 68] = "";
  char _pad3[4] = {};
  short vgroup_density = 0;

  short type = FLUID_FLOW_TYPE_SMOKE;            /* Smoke, flames, both, outflow, liquid. */
  short behavior = FLUID_FLOW_BEHAVIOR_GEOMETRY; /* Inflow, outflow, static. */
  short source = FLUID_FLOW_SOURCE_MESH;
  short texture_type = 0;
  short _pad4[3] = {};
  int flags = FLUID_FLOW_ABSOLUTE | FLUID_FLOW_USE_PART_SIZE |
              FLUID_FLOW_USE_INFLOW; /* Absolute emission etc. */
};

/* Collision objects (filled with smoke). */
struct FluidEffectorSettings {

  /* -- Runtime-only fields (from here on). -- */

  /* For fast RNA access. */
  struct FluidModifierData *fmd = nullptr;
  struct Mesh *mesh = nullptr;
  float *verts_old = nullptr;
  int numverts = 0;

  /* -- User-accessible fields (from here on). -- */

  float surface_distance = 0.0f; /* Thickness of mesh surface, used in obstacle SDF. */
  int flags = FLUID_EFFECTOR_USE_EFFEC;
  int subframes = 0;
  short type = FLUID_EFFECTOR_TYPE_COLLISION;
  char _pad1[6] = {};

  /* Guiding options. */
  float vel_multi = 1.0f; /* Multiplier for object velocity. */
  short guide_mode = FLUID_EFFECTOR_GUIDE_OVERRIDE;
  char _pad2[2] = {};
};
