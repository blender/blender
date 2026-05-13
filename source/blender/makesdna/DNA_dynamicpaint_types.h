/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_listBase.h"

#include "BLI_enum_flags.hh"

namespace blender {

/* surface type */
enum eDynamicPaint_SurfaceType : short {
  MOD_DPAINT_SURFACE_T_PAINT = 0,
  MOD_DPAINT_SURFACE_T_DISPLACE = 1,
  MOD_DPAINT_SURFACE_T_WEIGHT = 2,
  MOD_DPAINT_SURFACE_T_WAVE = 3,
};

/* surface flags */
enum eDynamicPaint_SurfaceFlags : int {
  MOD_DPAINT_ACTIVE = 1 << 0, /* Is surface enabled */

  MOD_DPAINT_ANTIALIAS = 1 << 1,    /* do anti-aliasing. */
  MOD_DPAINT_DISSOLVE = 1 << 2,     /* do dissolve */
  MOD_DPAINT_MULALPHA = 1 << 3,     /* Multiply color by alpha when saving image */
  MOD_DPAINT_DISSOLVE_LOG = 1 << 4, /* Use 1/x for surface dissolve */
  MOD_DPAINT_DRY_LOG = 1 << 5,      /* Use 1/x for drying paint */

  MOD_DPAINT_WAVE_OPEN_BORDERS = 1 << 7, /* passes waves through mesh edges */
  MOD_DPAINT_DISP_INCREMENTAL = 1 << 8,  /* builds displace on top of earlier values */
  MOD_DPAINT_USE_DRYING = 1 << 9,        /* use drying */

  MOD_DPAINT_OUT1 = 1 << 10, /* output primary surface */
  MOD_DPAINT_OUT2 = 1 << 11, /* output secondary surface */
};
ENUM_OPERATORS(eDynamicPaint_SurfaceFlags)

/* image_fileformat */
enum eDynamicPaint_ImageFormat : short {
  MOD_DPAINT_IMGFORMAT_PNG = 0,
  MOD_DPAINT_IMGFORMAT_OPENEXR = 1,
};

/* disp_type */
enum eDynamicPaint_DispType : short {
  MOD_DPAINT_DISP_DISPLACE = 0, /* displacement output displace map */
  MOD_DPAINT_DISP_DEPTH = 1,    /* displacement output depth data */
};

/* effect */
enum eDynamicPaint_EffectFlags : int {
  MOD_DPAINT_EFFECT_DO_SPREAD = 1 << 0, /* do spread effect */
  MOD_DPAINT_EFFECT_DO_DRIP = 1 << 1,   /* do drip effect */
  MOD_DPAINT_EFFECT_DO_SHRINK = 1 << 2, /* do shrink effect */
};
ENUM_OPERATORS(eDynamicPaint_EffectFlags)

/* init_color_type */
enum eDynamicPaint_InitColorType : short {
  MOD_DPAINT_INITIAL_NONE = 0,
  MOD_DPAINT_INITIAL_COLOR = 1,
  MOD_DPAINT_INITIAL_TEXTURE = 2,
  MOD_DPAINT_INITIAL_VERTEXCOLOR = 3,
};

/* canvas flags */
enum eDynamicPaint_CanvasFlags : short {
  /** surface is already baking, so it won't get updated (loop) */
  MOD_DPAINT_BAKING = 1 << 1,
};
ENUM_OPERATORS(eDynamicPaint_CanvasFlags)

/* flags */
enum eDynamicPaint_BrushFlags : int {
  /** use particle radius */
  MOD_DPAINT_PART_RAD = 1 << 0,
  // MOD_DPAINT_USE_MATERIAL       = 1 << 1,  /* DNA_DEPRECATED */
  /** don't increase alpha unless paint alpha is higher than existing */
  MOD_DPAINT_ABS_ALPHA = 1 << 2,
  /** removes paint */
  MOD_DPAINT_ERASE = 1 << 3,

  /** only read falloff ramp alpha */
  MOD_DPAINT_RAMP_ALPHA = 1 << 4,
  /** do proximity check only in defined dir */
  MOD_DPAINT_PROX_PROJECT = 1 << 5,
  /** inverse proximity painting */
  MOD_DPAINT_INVERSE_PROX = 1 << 6,
  /** negates volume influence on "volume + prox" mode */
  MOD_DPAINT_NEGATE_VOLUME = 1 << 7,

  /** brush smudges existing paint */
  MOD_DPAINT_DO_SMUDGE = 1 << 8,
  /** multiply brush influence by velocity */
  MOD_DPAINT_VELOCITY_ALPHA = 1 << 9,
  /** replace brush color by velocity color ramp */
  MOD_DPAINT_VELOCITY_COLOR = 1 << 10,
  /** multiply brush intersection depth by velocity */
  MOD_DPAINT_VELOCITY_DEPTH = 1 << 11,

  MOD_DPAINT_USES_VELOCITY = (MOD_DPAINT_DO_SMUDGE | MOD_DPAINT_VELOCITY_ALPHA |
                              MOD_DPAINT_VELOCITY_COLOR | MOD_DPAINT_VELOCITY_DEPTH),
};
ENUM_OPERATORS(eDynamicPaint_BrushFlags)

/* collision type */
enum eDynamicPaint_CollisionType : int {
  MOD_DPAINT_COL_VOLUME = 0,  /* paint with mesh volume */
  MOD_DPAINT_COL_DIST = 1,    /* paint using distance to mesh surface */
  MOD_DPAINT_COL_VOLDIST = 2, /* use both volume and distance */
  MOD_DPAINT_COL_PSYS = 3,    /* use particle system */
  MOD_DPAINT_COL_POINT = 4,   /* use distance to object center point */
};

/* proximity_falloff */
enum eDynamicPaint_ProximityFalloff : short {
  MOD_DPAINT_PRFALL_CONSTANT = 0, /* no-falloff */
  MOD_DPAINT_PRFALL_SMOOTH = 1,   /* smooth, linear falloff */
  MOD_DPAINT_PRFALL_RAMP = 2,     /* use color ramp */
};

/* wave_brush_type */
enum eDynamicPaint_WaveBrushType : short {
  MOD_DPAINT_WAVEB_DEPTH = 0,   /* use intersection depth */
  MOD_DPAINT_WAVEB_FORCE = 1,   /* act as a force on intersection area */
  MOD_DPAINT_WAVEB_REFLECT = 2, /* obstacle that reflects waves */
  MOD_DPAINT_WAVEB_CHANGE = 3,  /* use change of intersection depth from previous frame */
};

/* brush ray_dir */
enum eDynamicPaint_RayDir : short {
  MOD_DPAINT_RAY_CANVAS = 0,
  MOD_DPAINT_RAY_BRUSH_AVG = 1,
  MOD_DPAINT_RAY_ZPLUS = 2,
};

struct PaintSurfaceData;

/* surface format */
enum eDynamicPaint_SurfaceFormat : short {
  MOD_DPAINT_SURFACE_F_PTEX = 0,
  MOD_DPAINT_SURFACE_F_VERTEX = 1,
  MOD_DPAINT_SURFACE_F_IMAGESEQ = 2,
};

struct DynamicPaintSurface {

  struct DynamicPaintSurface *next = nullptr, *prev = nullptr;
  /** For fast RNA access. */
  struct DynamicPaintCanvasSettings *canvas = nullptr;
  struct PaintSurfaceData *data = nullptr;

  struct Collection *brush_group = nullptr;
  struct EffectorWeights *effector_weights = nullptr;

  /* cache */
  struct PointCache *pointcache = nullptr;
  ListBaseT<PointCache> ptcaches = {nullptr, nullptr};
  int current_frame = 0;

  /* surface */
  char name[64] = "";
  eDynamicPaint_SurfaceFormat format = MOD_DPAINT_SURFACE_F_PTEX;
  eDynamicPaint_SurfaceType type = MOD_DPAINT_SURFACE_T_PAINT;
  eDynamicPaint_DispType disp_type = MOD_DPAINT_DISP_DISPLACE;
  eDynamicPaint_ImageFormat image_fileformat = MOD_DPAINT_IMGFORMAT_PNG;
  /** Ui selection box. */
  short effect_ui = 0;
  eDynamicPaint_InitColorType init_color_type = MOD_DPAINT_INITIAL_NONE;
  eDynamicPaint_SurfaceFlags flags = {};
  eDynamicPaint_EffectFlags effect = {};

  int image_resolution = 0, substeps = 0;
  int start_frame = 0, end_frame = 0;

  /* initial color */
  float init_color[4] = {};
  struct Tex *init_texture = nullptr;
  char init_layername[/*MAX_CUSTOMDATA_LAYER_NAME*/ 68] = "";

  int dry_speed = 0, diss_speed = 0;
  float color_dry_threshold = 0;
  float depth_clamp = 0, disp_factor = 0;

  float spread_speed = 0, color_spread_speed = 0, shrink_speed = 0;
  float drip_vel = 0, drip_acc = 0;

  /* per surface brush settings */
  float influence_scale = 0, radius_scale = 0;

  /* wave settings */
  float wave_damping = 0, wave_speed = 0, wave_timescale = 0, wave_spring = 0, wave_smoothness = 0;
  char _pad2[4] = {};

  char uvlayer_name[/*MAX_CUSTOMDATA_LAYER_NAME*/ 68] = "";
  char image_output_path[/*FILE_MAX*/ 1024] = "";
  char output_name[/*MAX_CUSTOMDATA_LAYER_NAME*/ 68] = "";
  char output_name2[/*MAX_CUSTOMDATA_LAYER_NAME*/ 68] = "";
};

/* Canvas settings */
struct DynamicPaintCanvasSettings {
  /** For fast RNA access. */
  struct DynamicPaintModifierData *pmd = nullptr;

  ListBaseT<DynamicPaintSurface> surfaces = {nullptr, nullptr};
  short active_sur = 0;
  eDynamicPaint_CanvasFlags flags = {};
  char _pad[4] = {};

  /** Bake error description. */
  char error[64] = "";
};

/* Brush settings */
struct DynamicPaintBrushSettings {
  /** For fast RNA access. */
  struct DynamicPaintModifierData *pmd = nullptr;

  /**
   * \note Storing the particle system pointer here is very weak, as it prevents modifiers' data
   * copying to be self-sufficient (extra external code needs to ensure the pointer remains valid
   * when the modifier data is copied from one object to another). See e.g.
   * `BKE_object_copy_particlesystems` or `BKE_object_copy_modifier`.
   */
  struct ParticleSystem *psys = nullptr;

  eDynamicPaint_BrushFlags flags = {};
  eDynamicPaint_CollisionType collision = MOD_DPAINT_COL_VOLUME;

  float r = 0, g = 0, b = 0, alpha = 0;
  float wetness = 0;

  float particle_radius = 0, particle_smooth = 0;
  float paint_distance = 0;

  /* color ramps */
  /** Proximity paint falloff. */
  struct ColorBand *paint_ramp = nullptr;
  /** Velocity paint ramp. */
  struct ColorBand *vel_ramp = nullptr;

  eDynamicPaint_ProximityFalloff proximity_falloff = MOD_DPAINT_PRFALL_CONSTANT;
  eDynamicPaint_WaveBrushType wave_type = MOD_DPAINT_WAVEB_DEPTH;
  eDynamicPaint_RayDir ray_dir = MOD_DPAINT_RAY_CANVAS;
  char _pad[2] = {};

  float wave_factor = 0, wave_clamp = 0;
  float max_velocity = 0, smudge_strength = 0;
};

}  // namespace blender
