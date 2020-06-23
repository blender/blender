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
 */

/** \file
 * \ingroup DNA
 */

#ifndef __DNA_MODIFIER_TYPES_H__
#define __DNA_MODIFIER_TYPES_H__

#include "DNA_defs.h"
#include "DNA_listBase.h"

#ifdef __cplusplus
extern "C" {
#endif

/* WARNING ALERT! TYPEDEF VALUES ARE WRITTEN IN FILES! SO DO NOT CHANGE!
 * (ONLY ADD NEW ITEMS AT THE END)
 */

struct Mesh;

typedef enum ModifierType {
  eModifierType_None = 0,
  eModifierType_Subsurf = 1,
  eModifierType_Lattice = 2,
  eModifierType_Curve = 3,
  eModifierType_Build = 4,
  eModifierType_Mirror = 5,
  eModifierType_Decimate = 6,
  eModifierType_Wave = 7,
  eModifierType_Armature = 8,
  eModifierType_Hook = 9,
  eModifierType_Softbody = 10,
  eModifierType_Boolean = 11,
  eModifierType_Array = 12,
  eModifierType_EdgeSplit = 13,
  eModifierType_Displace = 14,
  eModifierType_UVProject = 15,
  eModifierType_Smooth = 16,
  eModifierType_Cast = 17,
  eModifierType_MeshDeform = 18,
  eModifierType_ParticleSystem = 19,
  eModifierType_ParticleInstance = 20,
  eModifierType_Explode = 21,
  eModifierType_Cloth = 22,
  eModifierType_Collision = 23,
  eModifierType_Bevel = 24,
  eModifierType_Shrinkwrap = 25,
  eModifierType_Fluidsim = 26,
  eModifierType_Mask = 27,
  eModifierType_SimpleDeform = 28,
  eModifierType_Multires = 29,
  eModifierType_Surface = 30,
#ifdef DNA_DEPRECATED_ALLOW
  eModifierType_Smoke = 31,
#endif
  eModifierType_ShapeKey = 32,
  eModifierType_Solidify = 33,
  eModifierType_Screw = 34,
  eModifierType_Warp = 35,
  eModifierType_WeightVGEdit = 36,
  eModifierType_WeightVGMix = 37,
  eModifierType_WeightVGProximity = 38,
  eModifierType_Ocean = 39,
  eModifierType_DynamicPaint = 40,
  eModifierType_Remesh = 41,
  eModifierType_Skin = 42,
  eModifierType_LaplacianSmooth = 43,
  eModifierType_Triangulate = 44,
  eModifierType_UVWarp = 45,
  eModifierType_MeshCache = 46,
  eModifierType_LaplacianDeform = 47,
  eModifierType_Wireframe = 48,
  eModifierType_DataTransfer = 49,
  eModifierType_NormalEdit = 50,
  eModifierType_CorrectiveSmooth = 51,
  eModifierType_MeshSequenceCache = 52,
  eModifierType_SurfaceDeform = 53,
  eModifierType_WeightedNormal = 54,
  eModifierType_Weld = 55,
  eModifierType_Fluid = 56,
  eModifierType_Simulation = 57,
  NUM_MODIFIER_TYPES,
} ModifierType;

typedef enum ModifierMode {
  eModifierMode_Realtime = (1 << 0),
  eModifierMode_Render = (1 << 1),
  eModifierMode_Editmode = (1 << 2),
  eModifierMode_OnCage = (1 << 3),
  /* Old modifier box expansion, just for versioning. */
  eModifierMode_Expanded_DEPRECATED = (1 << 4),
  eModifierMode_Virtual = (1 << 5),
  eModifierMode_ApplyOnSpline = (1 << 6),
  eModifierMode_DisableTemporary = (1u << 31),
} ModifierMode;

typedef struct ModifierData {
  struct ModifierData *next, *prev;

  int type, mode;
  int stackindex;
  short flag;
  /* An "expand" bit for each of the modifier's (sub)panels. */
  short ui_expand_flag;
  /** MAX_NAME. */
  char name[64];

  char *error;

  /* Pointer to a ModifierData in the original domain. */
  struct ModifierData *orig_modifier_data;
  void *runtime;
} ModifierData;

typedef enum {
  /* This modifier has been inserted in local override, and hence can be fully edited. */
  eModifierFlag_OverrideLibrary_Local = (1 << 0),
  /* This modifier does not own its caches, but instead shares them with another modifier. */
  eModifierFlag_SharedCaches = (1 << 1),
} ModifierFlag;

/* not a real modifier */
typedef struct MappingInfoModifierData {
  ModifierData modifier;

  struct Tex *texture;
  struct Object *map_object;
  char map_bone[64];
  /** MAX_CUSTOMDATA_LAYER_NAME. */
  char uvlayer_name[64];
  int uvlayer_tmp;
  int texmapping;
} MappingInfoModifierData;

typedef enum {
  eSubsurfModifierFlag_Incremental = (1 << 0),
  eSubsurfModifierFlag_DebugIncr = (1 << 1),
  eSubsurfModifierFlag_ControlEdges = (1 << 2),
  /* DEPRECATED, ONLY USED FOR DO-VERSIONS */
  eSubsurfModifierFlag_SubsurfUv_DEPRECATED = (1 << 3),
  eSubsurfModifierFlag_UseCrease = (1 << 4),
} SubsurfModifierFlag;

typedef enum {
  SUBSURF_TYPE_CATMULL_CLARK = 0,
  SUBSURF_TYPE_SIMPLE = 1,
} eSubsurfModifierType;

typedef enum {
  SUBSURF_UV_SMOOTH_NONE = 0,
  SUBSURF_UV_SMOOTH_PRESERVE_CORNERS = 1,
  SUBSURF_UV_SMOOTH_PRESERVE_CORNERS_AND_JUNCTIONS = 2,
  SUBSURF_UV_SMOOTH_PRESERVE_CORNERS_JUNCTIONS_AND_CONCAVE = 3,
  SUBSURF_UV_SMOOTH_PRESERVE_BOUNDARIES = 4,
  SUBSURF_UV_SMOOTH_ALL = 5,
} eSubsurfUVSmooth;

typedef struct SubsurfModifierData {
  ModifierData modifier;

  short subdivType, levels, renderLevels, flags;
  short uv_smooth;
  short quality;
  char _pad[4];

  /* TODO(sergey): Get rid of those with the old CCG subdivision code. */
  void *emCache, *mCache;
} SubsurfModifierData;

typedef struct LatticeModifierData {
  ModifierData modifier;

  struct Object *object;
  /** Optional vertexgroup name, MAX_VGROUP_NAME. */
  char name[64];
  float strength;
  short flag;
  char _pad[2];
} LatticeModifierData;

/*Lattice modifier flags */
enum {
  MOD_LATTICE_INVERT_VGROUP = (1 << 0),
};

typedef struct CurveModifierData {
  ModifierData modifier;

  struct Object *object;
  /** Optional vertexgroup name, MAX_VGROUP_NAME. */
  char name[64];
  /** Axis along which curve deforms. */
  short defaxis;
  short flag;
  char _pad[4];
} CurveModifierData;

/* Curve modifier flags */
enum {
  MOD_CURVE_INVERT_VGROUP = (1 << 0),
};

/* CurveModifierData->defaxis */
enum {
  MOD_CURVE_POSX = 1,
  MOD_CURVE_POSY = 2,
  MOD_CURVE_POSZ = 3,
  MOD_CURVE_NEGX = 4,
  MOD_CURVE_NEGY = 5,
  MOD_CURVE_NEGZ = 6,
};

typedef struct BuildModifierData {
  ModifierData modifier;

  float start, length;
  short flag;

  /** (bool) whether order of vertices is randomized - legacy files (for readfile conversion). */
  short randomize;
  /** (int) random seed. */
  int seed;
} BuildModifierData;

/* Build Modifier -> flag */
enum {
  /** order of vertices is randomized */
  MOD_BUILD_FLAG_RANDOMIZE = (1 << 0),
  /** frame range is reversed, resulting in a deconstruction effect */
  MOD_BUILD_FLAG_REVERSE = (1 << 1),
};

/* Mask Modifier */
typedef struct MaskModifierData {
  ModifierData modifier;

  /** Armature to use to in place of hardcoded vgroup. */
  struct Object *ob_arm;
  /** Name of vertex group to use to mask, MAX_VGROUP_NAME. */
  char vgroup[64];

  /** Using armature or hardcoded vgroup. */
  short mode;
  /** Flags for various things. */
  short flag;
  float threshold;
} MaskModifierData;

/* Mask Modifier -> mode */
enum {
  MOD_MASK_MODE_VGROUP = 0,
  MOD_MASK_MODE_ARM = 1,
};

/* Mask Modifier -> flag */
enum {
  MOD_MASK_INV = (1 << 0),
};

typedef struct ArrayModifierData {
  ModifierData modifier;

  /* the object with which to cap the start of the array  */
  struct Object *start_cap;
  /* the object with which to cap the end of the array  */
  struct Object *end_cap;
  /* the curve object to use for MOD_ARR_FITCURVE */
  struct Object *curve_ob;
  /* the object to use for object offset */
  struct Object *offset_ob;
  /* a constant duplicate offset;
   * 1 means the duplicates are 1 unit apart
   */
  float offset[3];
  /* a scaled factor for duplicate offsets;
   * 1 means the duplicates are 1 object-width apart
   */
  float scale[3];
  /* the length over which to distribute the duplicates */
  float length;
  /* the limit below which to merge vertices in adjacent duplicates */
  float merge_dist;
  /* determines how duplicate count is calculated; one of:
   * - MOD_ARR_FIXEDCOUNT -> fixed
   * - MOD_ARR_FITLENGTH  -> calculated to fit a set length
   * - MOD_ARR_FITCURVE   -> calculated to fit the length of a Curve object
   */
  int fit_type;
  /* flags specifying how total offset is calculated; binary OR of:
   * - MOD_ARR_OFF_CONST    -> total offset += offset
   * - MOD_ARR_OFF_RELATIVE -> total offset += relative * object width
   * - MOD_ARR_OFF_OBJ      -> total offset += offset_ob's matrix
   * total offset is the sum of the individual enabled offsets
   */
  int offset_type;
  /* general flags:
   * MOD_ARR_MERGE -> merge vertices in adjacent duplicates
   */
  int flags;
  /* the number of duplicates to generate for MOD_ARR_FIXEDCOUNT */
  int count;
  float uv_offset[2];
} ArrayModifierData;

/* ArrayModifierData->fit_type */
enum {
  MOD_ARR_FIXEDCOUNT = 0,
  MOD_ARR_FITLENGTH = 1,
  MOD_ARR_FITCURVE = 2,
};

/* ArrayModifierData->offset_type */
enum {
  MOD_ARR_OFF_CONST = (1 << 0),
  MOD_ARR_OFF_RELATIVE = (1 << 1),
  MOD_ARR_OFF_OBJ = (1 << 2),
};

/* ArrayModifierData->flags */
enum {
  MOD_ARR_MERGE = (1 << 0),
  MOD_ARR_MERGEFINAL = (1 << 1),
};

typedef struct MirrorModifierData {
  ModifierData modifier;

  /** Deprecated, use flag instead. */
  short axis DNA_DEPRECATED;
  short flag;
  float tolerance;
  float uv_offset[2];
  float uv_offset_copy[2];
  struct Object *mirror_ob;
} MirrorModifierData;

/* MirrorModifierData->flag */
enum {
  MOD_MIR_CLIPPING = (1 << 0),
  MOD_MIR_MIRROR_U = (1 << 1),
  MOD_MIR_MIRROR_V = (1 << 2),
  MOD_MIR_AXIS_X = (1 << 3),
  MOD_MIR_AXIS_Y = (1 << 4),
  MOD_MIR_AXIS_Z = (1 << 5),
  MOD_MIR_VGROUP = (1 << 6),
  MOD_MIR_NO_MERGE = (1 << 7),
  MOD_MIR_BISECT_AXIS_X = (1 << 8),
  MOD_MIR_BISECT_AXIS_Y = (1 << 9),
  MOD_MIR_BISECT_AXIS_Z = (1 << 10),
  MOD_MIR_BISECT_FLIP_AXIS_X = (1 << 11),
  MOD_MIR_BISECT_FLIP_AXIS_Y = (1 << 12),
  MOD_MIR_BISECT_FLIP_AXIS_Z = (1 << 13),
  MOD_MIR_MIRROR_UDIM = (1 << 14),
};

typedef struct EdgeSplitModifierData {
  ModifierData modifier;

  /** Angle above which edges should be split. */
  float split_angle;
  int flags;
} EdgeSplitModifierData;

/* EdgeSplitModifierData->flags */
enum {
  MOD_EDGESPLIT_FROMANGLE = (1 << 1),
  MOD_EDGESPLIT_FROMFLAG = (1 << 2),
};

typedef struct BevelModifierData {
  ModifierData modifier;

  /** The "raw" bevel value (distance/amount to bevel). */
  float value;
  /** The resolution (as originally coded, it is the number of recursive bevels). */
  int res;
  /** General option flags. */
  short flags;
  /** Used to interpret the bevel value. */
  short val_flags;
  /** For the type and how we build the bevel's profile. */
  short profile_type;
  /** Flags to tell the tool how to limit the bevel. */
  short lim_flags;
  /** Flags to direct how edge weights are applied to verts. */
  short e_flags;
  /** Material index if >= 0, else material inherited from surrounding faces. */
  short mat;
  short edge_flags;
  short face_str_mode;
  /** Patterns to use for mitering non-reflex and reflex miter edges */
  short miter_inner;
  short miter_outer;
  /** The method to use for creating >2-way intersections */
  short vmesh_method;
  char _pad0[2];
  /** Controls profile shape (0->1, .5 is round). */
  float profile;
  /** if the MOD_BEVEL_ANGLE is set,
   * this will be how "sharp" an edge must be before it gets beveled */
  float bevel_angle;
  float spread;
  /** if the MOD_BEVEL_VWEIGHT option is set,
   * this will be the name of the vert group, MAX_VGROUP_NAME */
  char defgrp_name[64];

  char _pad1[4];
  /** Curve info for the custom profile */
  struct CurveProfile *custom_profile;

} BevelModifierData;

/* BevelModifierData->flags and BevelModifierData->lim_flags */
enum {
  MOD_BEVEL_VERT = (1 << 1),
  MOD_BEVEL_INVERT_VGROUP = (1 << 2),
  MOD_BEVEL_ANGLE = (1 << 3),
  MOD_BEVEL_WEIGHT = (1 << 4),
  MOD_BEVEL_VGROUP = (1 << 5),
/* unused                  = (1 << 6), */
#ifdef DNA_DEPRECATED_ALLOW
  MOD_BEVEL_CUSTOM_PROFILE_DEPRECATED = (1 << 7),
#endif
  /* unused                  = (1 << 8), */
  /* unused                  = (1 << 9), */
  /* unused                  = (1 << 10), */
  /* unused                  = (1 << 11), */
  /* unused                  = (1 << 12), */
  MOD_BEVEL_OVERLAP_OK = (1 << 13),
  MOD_BEVEL_EVEN_WIDTHS = (1 << 14),
  MOD_BEVEL_HARDEN_NORMALS = (1 << 15),
};

/* BevelModifierData->val_flags (not used as flags any more) */
enum {
  MOD_BEVEL_AMT_OFFSET = 0,
  MOD_BEVEL_AMT_WIDTH = 1,
  MOD_BEVEL_AMT_DEPTH = 2,
  MOD_BEVEL_AMT_PERCENT = 3,
  MOD_BEVEL_AMT_ABSOLUTE = 4,
};

/* BevelModifierData->profile_type */
enum {
  MOD_BEVEL_PROFILE_SUPERELLIPSE = 0,
  MOD_BEVEL_PROFILE_CUSTOM = 1,
};

/* BevelModifierData->edge_flags */
enum {
  MOD_BEVEL_MARK_SEAM = (1 << 0),
  MOD_BEVEL_MARK_SHARP = (1 << 1),
};

/* BevelModifierData->face_str_mode */
enum {
  MOD_BEVEL_FACE_STRENGTH_NONE = 0,
  MOD_BEVEL_FACE_STRENGTH_NEW = 1,
  MOD_BEVEL_FACE_STRENGTH_AFFECTED = 2,
  MOD_BEVEL_FACE_STRENGTH_ALL = 3,
};

/* BevelModifier->miter_inner and ->miter_outer */
enum {
  MOD_BEVEL_MITER_SHARP = 0,
  MOD_BEVEL_MITER_PATCH = 1,
  MOD_BEVEL_MITER_ARC = 2,
};

/* BevelModifier->vmesh_method */
enum {
  MOD_BEVEL_VMESH_ADJ = 0,
  MOD_BEVEL_VMESH_CUTOFF = 1,
};

typedef struct FluidModifierData {
  ModifierData modifier;

  struct FluidDomainSettings *domain;
  /** Inflow, outflow, smoke objects. */
  struct FluidFlowSettings *flow;
  /** Effector objects (collision, guiding). */
  struct FluidEffectorSettings *effector;
  float time;
  /** Domain, inflow, outflow, .... */
  int type;
} FluidModifierData;

/* Fluid modifier flags */
enum {
  MOD_FLUID_TYPE_DOMAIN = (1 << 0),
  MOD_FLUID_TYPE_FLOW = (1 << 1),
  MOD_FLUID_TYPE_EFFEC = (1 << 2),
};

typedef struct DisplaceModifierData {
  ModifierData modifier;

  /* keep in sync with MappingInfoModifierData */
  struct Tex *texture;
  struct Object *map_object;
  char map_bone[64];
  /** MAX_CUSTOMDATA_LAYER_NAME. */
  char uvlayer_name[64];
  int uvlayer_tmp;
  int texmapping;
  /* end MappingInfoModifierData */

  float strength;
  int direction;
  /** MAX_VGROUP_NAME. */
  char defgrp_name[64];
  float midlevel;
  int space;
  short flag;
  char _pad[6];
} DisplaceModifierData;

/* DisplaceModifierData->flag */
enum {
  MOD_DISP_INVERT_VGROUP = (1 << 0),
};

/* DisplaceModifierData->direction */
enum {
  MOD_DISP_DIR_X = 0,
  MOD_DISP_DIR_Y = 1,
  MOD_DISP_DIR_Z = 2,
  MOD_DISP_DIR_NOR = 3,
  MOD_DISP_DIR_RGB_XYZ = 4,
  MOD_DISP_DIR_CLNOR = 5,
};

/* DisplaceModifierData->texmapping */
enum {
  MOD_DISP_MAP_LOCAL = 0,
  MOD_DISP_MAP_GLOBAL = 1,
  MOD_DISP_MAP_OBJECT = 2,
  MOD_DISP_MAP_UV = 3,
};

/* DisplaceModifierData->space */
enum {
  MOD_DISP_SPACE_LOCAL = 0,
  MOD_DISP_SPACE_GLOBAL = 1,
};

typedef struct UVProjectModifierData {
  ModifierData modifier;

  /* the objects which do the projecting */
  /** MOD_UVPROJECT_MAXPROJECTORS. */
  struct Object *projectors[10];
  char _pad2[4];
  int num_projectors;
  float aspectx, aspecty;
  float scalex, scaley;
  /** MAX_CUSTOMDATA_LAYER_NAME. */
  char uvlayer_name[64];
  int uvlayer_tmp;
  char _pad[4];
} UVProjectModifierData;

#define MOD_UVPROJECT_MAXPROJECTORS 10

typedef struct DecimateModifierData {
  ModifierData modifier;

  /** (mode == MOD_DECIM_MODE_COLLAPSE). */
  float percent;
  /** (mode == MOD_DECIM_MODE_UNSUBDIV). */
  short iter;
  /** (mode == MOD_DECIM_MODE_DISSOLVE). */
  char delimit;
  /** (mode == MOD_DECIM_MODE_COLLAPSE). */
  char symmetry_axis;
  /** (mode == MOD_DECIM_MODE_DISSOLVE). */
  float angle;

  /** MAX_VGROUP_NAME. */
  char defgrp_name[64];
  float defgrp_factor;
  short flag, mode;

  /* runtime only */
  int face_count;
} DecimateModifierData;

enum {
  MOD_DECIM_FLAG_INVERT_VGROUP = (1 << 0),
  /** for collapse only. dont convert tri pairs back to quads */
  MOD_DECIM_FLAG_TRIANGULATE = (1 << 1),
  /** for dissolve only. collapse all verts between 2 faces */
  MOD_DECIM_FLAG_ALL_BOUNDARY_VERTS = (1 << 2),
  MOD_DECIM_FLAG_SYMMETRY = (1 << 3),
};

enum {
  MOD_DECIM_MODE_COLLAPSE = 0,
  MOD_DECIM_MODE_UNSUBDIV = 1,
  /** called planar in the UI */
  MOD_DECIM_MODE_DISSOLVE = 2,
};

typedef struct SmoothModifierData {
  ModifierData modifier;
  float fac;
  /** MAX_VGROUP_NAME. */
  char defgrp_name[64];
  short flag, repeat;

} SmoothModifierData;

/* Smooth modifier flags */
enum {
  MOD_SMOOTH_INVERT_VGROUP = (1 << 0),
  MOD_SMOOTH_X = (1 << 1),
  MOD_SMOOTH_Y = (1 << 2),
  MOD_SMOOTH_Z = (1 << 3),
};

typedef struct CastModifierData {
  ModifierData modifier;

  struct Object *object;
  float fac;
  float radius;
  float size;
  /** MAX_VGROUP_NAME. */
  char defgrp_name[64];
  short flag, type;
} CastModifierData;

/* Cast modifier flags */
enum {
  /* And what bout (1 << 0) flag? ;) */
  MOD_CAST_INVERT_VGROUP = (1 << 0),
  MOD_CAST_X = (1 << 1),
  MOD_CAST_Y = (1 << 2),
  MOD_CAST_Z = (1 << 3),
  MOD_CAST_USE_OB_TRANSFORM = (1 << 4),
  MOD_CAST_SIZE_FROM_RADIUS = (1 << 5),
};

/* Cast modifier projection types */
enum {
  MOD_CAST_TYPE_SPHERE = 0,
  MOD_CAST_TYPE_CYLINDER = 1,
  MOD_CAST_TYPE_CUBOID = 2,
};

typedef struct WaveModifierData {
  ModifierData modifier;

  /* keep in sync with MappingInfoModifierData */
  struct Tex *texture;
  struct Object *map_object;
  char map_bone[64];
  /** MAX_CUSTOMDATA_LAYER_NAME. */
  char uvlayer_name[64];
  int uvlayer_tmp;
  int texmapping;
  /* end MappingInfoModifierData */

  struct Object *objectcenter;
  /** MAX_VGROUP_NAME. */
  char defgrp_name[64];

  short flag;
  char _pad[2];

  float startx, starty, height, width;
  float narrow, speed, damp, falloff;

  float timeoffs, lifetime;
  char _pad1[4];
} WaveModifierData;

/* WaveModifierData.flag */
enum {
  MOD_WAVE_INVERT_VGROUP = (1 << 0),
  MOD_WAVE_X = (1 << 1),
  MOD_WAVE_Y = (1 << 2),
  MOD_WAVE_CYCL = (1 << 3),
  MOD_WAVE_NORM = (1 << 4),
  MOD_WAVE_NORM_X = (1 << 5),
  MOD_WAVE_NORM_Y = (1 << 6),
  MOD_WAVE_NORM_Z = (1 << 7),
};

typedef struct ArmatureModifierData {
  ModifierData modifier;

  /** #eArmature_DeformFlag use instead of #bArmature.deformflag. */
  short deformflag, multi;
  char _pad2[4];
  struct Object *object;
  /** Stored input of previous modifier, for vertex-group blending. */
  float (*vert_coords_prev)[3];
  /** MAX_VGROUP_NAME. */
  char defgrp_name[64];
} ArmatureModifierData;

enum {
  MOD_HOOK_UNIFORM_SPACE = (1 << 0),
  MOD_HOOK_INVERT_VGROUP = (1 << 1),
};

/* same as WarpModifierFalloff */
typedef enum {
  eHook_Falloff_None = 0,
  eHook_Falloff_Curve = 1,
  eHook_Falloff_Sharp = 2,     /* PROP_SHARP */
  eHook_Falloff_Smooth = 3,    /* PROP_SMOOTH */
  eHook_Falloff_Root = 4,      /* PROP_ROOT */
  eHook_Falloff_Linear = 5,    /* PROP_LIN */
  eHook_Falloff_Const = 6,     /* PROP_CONST */
  eHook_Falloff_Sphere = 7,    /* PROP_SPHERE */
  eHook_Falloff_InvSquare = 8, /* PROP_INVSQUARE */
  /* PROP_RANDOM not used */
} HookModifierFalloff;

typedef struct HookModifierData {
  ModifierData modifier;

  struct Object *object;
  /** Optional name of bone target, MAX_ID_NAME-2. */
  char subtarget[64];

  char flag;
  /** Use enums from WarpModifier (exact same functionality). */
  char falloff_type;
  char _pad[6];
  /** Matrix making current transform unmodified. */
  float parentinv[4][4];
  /** Visualization of hook. */
  float cent[3];
  /** If not zero, falloff is distance where influence zero. */
  float falloff;

  struct CurveMapping *curfalloff;

  /** If NULL, it's using vertexgroup. */
  int *indexar;
  int totindex;
  float force;
  /** Optional vertexgroup name, MAX_VGROUP_NAME. */
  char name[64];
} HookModifierData;

typedef struct SoftbodyModifierData {
  ModifierData modifier;
} SoftbodyModifierData;

typedef struct ClothModifierData {
  ModifierData modifier;

  /** The internal data structure for cloth. */
  struct Cloth *clothObject;
  /** Definition is in DNA_cloth_types.h. */
  struct ClothSimSettings *sim_parms;
  /** Definition is in DNA_cloth_types.h. */
  struct ClothCollSettings *coll_parms;

  /* PointCache can be shared with other instances of ClothModifierData.
   * Inspect (modifier.flag & eModifierFlag_SharedCaches) to find out. */
  /** Definition is in DNA_object_force_types.h. */
  struct PointCache *point_cache;
  struct ListBase ptcaches;

  /* XXX nasty hack, remove once hair can be separated from cloth modifier data */
  struct ClothHairData *hairdata;
  /* grid geometry values of hair continuum */
  float hair_grid_min[3];
  float hair_grid_max[3];
  int hair_grid_res[3];
  float hair_grid_cellsize;

  struct ClothSolverResult *solver_result;
} ClothModifierData;

typedef struct CollisionModifierData {
  ModifierData modifier;

  /** Position at the beginning of the frame. */
  struct MVert *x;
  /** Position at the end of the frame. */
  struct MVert *xnew;
  /** Unused atm, but was discussed during sprint. */
  struct MVert *xold;
  /** New position at the actual inter-frame step. */
  struct MVert *current_xnew;
  /** Position at the actual inter-frame step. */
  struct MVert *current_x;
  /** (xnew - x) at the actual inter-frame step. */
  struct MVert *current_v;

  struct MVertTri *tri;

  unsigned int mvert_num;
  unsigned int tri_num;
  /** Cfra time of modifier. */
  float time_x, time_xnew;
  /** Collider doesn't move this frame, i.e. x[].co==xnew[].co. */
  char is_static;
  char _pad[7];

  /** Bounding volume hierarchy for this cloth object. */
  struct BVHTree *bvhtree;
} CollisionModifierData;

typedef struct SurfaceModifierData {
  ModifierData modifier;

  /** Old position. */
  struct MVert *x;
  /** Velocity. */
  struct MVert *v;

  struct Mesh *mesh;

  /** Bounding volume hierarchy of the mesh faces. */
  struct BVHTreeFromMesh *bvhtree;

  int cfra, numverts;
} SurfaceModifierData;

typedef struct BooleanModifierData {
  ModifierData modifier;

  struct Object *object;
  char operation;
  char _pad[2];
  char bm_flag;
  float double_threshold;
} BooleanModifierData;

typedef enum {
  eBooleanModifierOp_Intersect = 0,
  eBooleanModifierOp_Union = 1,
  eBooleanModifierOp_Difference = 2,
} BooleanModifierOp;

/* bm_flag (only used when G_DEBUG) */
enum {
  eBooleanModifierBMeshFlag_BMesh_Separate = (1 << 0),
  eBooleanModifierBMeshFlag_BMesh_NoDissolve = (1 << 1),
  eBooleanModifierBMeshFlag_BMesh_NoConnectRegions = (1 << 2),
};

typedef struct MDefInfluence {
  int vertex;
  float weight;
} MDefInfluence;

typedef struct MDefCell {
  int offset;
  int totinfluence;
} MDefCell;

typedef struct MeshDeformModifierData {
  ModifierData modifier;

  /** Mesh object. */
  struct Object *object;
  /** Optional vertexgroup name, MAX_VGROUP_NAME. */
  char defgrp_name[64];

  short gridsize, flag;
  char _pad[4];

  /* result of static binding */
  /** Influences. */
  MDefInfluence *bindinfluences;
  /** Offsets into influences array. */
  int *bindoffsets;
  /** Coordinates that cage was bound with. */
  float *bindcagecos;
  /** Total vertices in mesh and cage. */
  int totvert, totcagevert;

  /* result of dynamic binding */
  /** Grid with dynamic binding cell points. */
  MDefCell *dyngrid;
  /** Dynamic binding vertex influences. */
  MDefInfluence *dyninfluences;
  /** Is this vertex bound or not?. */
  int *dynverts;
  /** Size of the dynamic bind grid. */
  int dyngridsize;
  /** Total number of vertex influences. */
  int totinfluence;
  /** Offset of the dynamic bind grid. */
  float dyncellmin[3];
  /** Width of dynamic bind cell. */
  float dyncellwidth;
  /** Matrix of cage at binding time. */
  float bindmat[4][4];

  /* deprecated storage */
  /** Deprecated inefficient storage. */
  float *bindweights;
  /** Deprecated storage of cage coords. */
  float *bindcos;

  /* runtime */
  void (*bindfunc)(struct MeshDeformModifierData *mmd,
                   struct Mesh *cagemesh,
                   float *vertexcos,
                   int totvert,
                   float cagemat[4][4]);
} MeshDeformModifierData;

enum {
  MOD_MDEF_INVERT_VGROUP = (1 << 0),
  MOD_MDEF_DYNAMIC_BIND = (1 << 1),
};

typedef struct ParticleSystemModifierData {
  ModifierData modifier;

  struct ParticleSystem *psys;
  /** Final Mesh - its topology may differ from orig mesh. */
  struct Mesh *mesh_final;
  /** Original mesh that particles are attached to. */
  struct Mesh *mesh_original;
  int totdmvert, totdmedge, totdmface;
  short flag;
  char _pad[2];
} ParticleSystemModifierData;

typedef enum {
  eParticleSystemFlag_Pars = (1 << 0),
  eParticleSystemFlag_psys_updated = (1 << 1),
  eParticleSystemFlag_file_loaded = (1 << 2),
} ParticleSystemModifierFlag;

typedef enum {
  eParticleInstanceFlag_Parents = (1 << 0),
  eParticleInstanceFlag_Children = (1 << 1),
  eParticleInstanceFlag_Path = (1 << 2),
  eParticleInstanceFlag_Unborn = (1 << 3),
  eParticleInstanceFlag_Alive = (1 << 4),
  eParticleInstanceFlag_Dead = (1 << 5),
  eParticleInstanceFlag_KeepShape = (1 << 6),
  eParticleInstanceFlag_UseSize = (1 << 7),
} ParticleInstanceModifierFlag;

typedef enum {
  eParticleInstanceSpace_World = 0,
  eParticleInstanceSpace_Local = 1,
} ParticleInstanceModifierSpace;

typedef struct ParticleInstanceModifierData {
  ModifierData modifier;

  struct Object *ob;
  short psys, flag, axis, space;
  float position, random_position;
  float rotation, random_rotation;
  float particle_amount, particle_offset;
  /** MAX_CUSTOMDATA_LAYER_NAME. */
  char index_layer_name[64];
  /** MAX_CUSTOMDATA_LAYER_NAME. */
  char value_layer_name[64];
} ParticleInstanceModifierData;

typedef enum {
  eExplodeFlag_CalcFaces = (1 << 0),
  eExplodeFlag_PaSize = (1 << 1),
  eExplodeFlag_EdgeCut = (1 << 2),
  eExplodeFlag_Unborn = (1 << 3),
  eExplodeFlag_Alive = (1 << 4),
  eExplodeFlag_Dead = (1 << 5),
  eExplodeFlag_INVERT_VGROUP = (1 << 6),
} ExplodeModifierFlag;

typedef struct ExplodeModifierData {
  ModifierData modifier;

  int *facepa;
  short flag, vgroup;
  float protect;
  /** MAX_CUSTOMDATA_LAYER_NAME. */
  char uvname[64];
} ExplodeModifierData;

typedef struct MultiresModifierData {
  ModifierData modifier;

  char lvl, sculptlvl, renderlvl, totlvl;
  char simple, flags, _pad[2];
  short quality;
  short uv_smooth;
  char _pad2[4];
} MultiresModifierData;

typedef enum {
  eMultiresModifierFlag_ControlEdges = (1 << 0),
  /* DEPRECATED, only used for versioning. */
  eMultiresModifierFlag_PlainUv_DEPRECATED = (1 << 1),
  eMultiresModifierFlag_UseCrease = (1 << 2),
} MultiresModifierFlag;

/* DEPRECATED, only used for versioning. */
typedef struct FluidsimModifierData {
  ModifierData modifier;

  /** Definition is in DNA_object_fluidsim_types.h. */
  struct FluidsimSettings *fss;
} FluidsimModifierData;

/* DEPRECATED, only used for versioning. */
typedef struct SmokeModifierData {
  ModifierData modifier;

  /** Domain, inflow, outflow, .... */
  int type;
  int _pad;
} SmokeModifierData;

typedef struct ShrinkwrapModifierData {
  ModifierData modifier;

  /** Shrink target. */
  struct Object *target;
  /** Additional shrink target. */
  struct Object *auxTarget;
  /** Optional vertexgroup name, MAX_VGROUP_NAME. */
  char vgroup_name[64];
  /** Distance offset to keep from mesh/projection point. */
  float keepDist;
  /** Shrink type projection. */
  short shrinkType;
  /** Shrink options. */
  char shrinkOpts;
  /** Shrink to surface mode. */
  char shrinkMode;
  /** Limit the projection ray cast. */
  float projLimit;
  /** Axis to project over. */
  char projAxis;

  /** If using projection over vertex normal this controls the level of subsurface that must be
   * done before getting the vertex coordinates and normal
   */
  char subsurfLevels;

  char _pad[2];
} ShrinkwrapModifierData;

/* Shrinkwrap->shrinkType */
enum {
  MOD_SHRINKWRAP_NEAREST_SURFACE = 0,
  MOD_SHRINKWRAP_PROJECT = 1,
  MOD_SHRINKWRAP_NEAREST_VERTEX = 2,
  MOD_SHRINKWRAP_TARGET_PROJECT = 3,
};

/* Shrinkwrap->shrinkMode */
enum {
  /** Move vertex to the surface of the target object (keepDist towards original position) */
  MOD_SHRINKWRAP_ON_SURFACE = 0,
  /** Move the vertex inside the target object; don't change if already inside */
  MOD_SHRINKWRAP_INSIDE = 1,
  /** Move the vertex outside the target object; don't change if already outside */
  MOD_SHRINKWRAP_OUTSIDE = 2,
  /** Move vertex to the surface of the target object, with keepDist towards the outside */
  MOD_SHRINKWRAP_OUTSIDE_SURFACE = 3,
  /** Move vertex to the surface of the target object, with keepDist along the normal */
  MOD_SHRINKWRAP_ABOVE_SURFACE = 4,
};

/* Shrinkwrap->shrinkOpts */
enum {
  /** allow shrinkwrap to move the vertex in the positive direction of axis */
  MOD_SHRINKWRAP_PROJECT_ALLOW_POS_DIR = (1 << 0),
  /** allow shrinkwrap to move the vertex in the negative direction of axis */
  MOD_SHRINKWRAP_PROJECT_ALLOW_NEG_DIR = (1 << 1),

  /** ignore vertex moves if a vertex ends projected on a front face of the target */
  MOD_SHRINKWRAP_CULL_TARGET_FRONTFACE = (1 << 3),
  /** ignore vertex moves if a vertex ends projected on a back face of the target */
  MOD_SHRINKWRAP_CULL_TARGET_BACKFACE = (1 << 4),

#ifdef DNA_DEPRECATED_ALLOW
  /** distance is measure to the front face of the target */
  MOD_SHRINKWRAP_KEEP_ABOVE_SURFACE = (1 << 5),
#endif

  MOD_SHRINKWRAP_INVERT_VGROUP = (1 << 6),
  MOD_SHRINKWRAP_INVERT_CULL_TARGET = (1 << 7),
};

#define MOD_SHRINKWRAP_CULL_TARGET_MASK \
  (MOD_SHRINKWRAP_CULL_TARGET_FRONTFACE | MOD_SHRINKWRAP_CULL_TARGET_BACKFACE)

/* Shrinkwrap->projAxis */
enum {
  /** projection over normal is used if no axis is selected */
  MOD_SHRINKWRAP_PROJECT_OVER_NORMAL = 0,
  MOD_SHRINKWRAP_PROJECT_OVER_X_AXIS = (1 << 0),
  MOD_SHRINKWRAP_PROJECT_OVER_Y_AXIS = (1 << 1),
  MOD_SHRINKWRAP_PROJECT_OVER_Z_AXIS = (1 << 2),
};

typedef struct SimpleDeformModifierData {
  ModifierData modifier;

  /** Object to control the origin of modifier space coordinates. */
  struct Object *origin;
  /** Optional vertexgroup name, MAX_VGROUP_NAME. */
  char vgroup_name[64];
  /** Factors to control simple deforms. */
  float factor;
  /** Lower and upper limit. */
  float limit[2];

  /** Deform function. */
  char mode;
  /** Lock axis (for taper and stretch). */
  char axis;
  /** Axis to perform the deform on (default is X, but can be overridden by origin. */
  char deform_axis;
  char flag;

} SimpleDeformModifierData;

/* SimpleDeform->flag */
enum {
  MOD_SIMPLEDEFORM_FLAG_INVERT_VGROUP = (1 << 0),
};

enum {
  MOD_SIMPLEDEFORM_MODE_TWIST = 1,
  MOD_SIMPLEDEFORM_MODE_BEND = 2,
  MOD_SIMPLEDEFORM_MODE_TAPER = 3,
  MOD_SIMPLEDEFORM_MODE_STRETCH = 4,
};

enum {
  MOD_SIMPLEDEFORM_LOCK_AXIS_X = (1 << 0),
  MOD_SIMPLEDEFORM_LOCK_AXIS_Y = (1 << 1),
  MOD_SIMPLEDEFORM_LOCK_AXIS_Z = (1 << 2),
};

typedef struct ShapeKeyModifierData {
  ModifierData modifier;
} ShapeKeyModifierData;

typedef struct SolidifyModifierData {
  ModifierData modifier;

  /** Name of vertex group to use, MAX_VGROUP_NAME. */
  char defgrp_name[64];
  char shell_defgrp_name[64];
  char rim_defgrp_name[64];
  /** New surface offset leve.l*/
  float offset;
  /** Midpoint of the offset . */
  float offset_fac;
  /** factor for the minimum weight to use when vgroups are used,
   * avoids 0.0 weights giving duplicate geometry */
  float offset_fac_vg;
  /** Clamp offset based on surrounding geometry. */
  float offset_clamp;
  char mode;

  /** Variables for #MOD_SOLIDIFY_MODE_NONMANIFOLD. */
  char nonmanifold_offset_mode;
  char nonmanifold_boundary_mode;

  char _pad;
  float crease_inner;
  float crease_outer;
  float crease_rim;
  int flag;
  short mat_ofs;
  short mat_ofs_rim;

  float merge_tolerance;
  float bevel_convex;
} SolidifyModifierData;

/** #SolidifyModifierData.flag */
enum {
  MOD_SOLIDIFY_RIM = (1 << 0),
  MOD_SOLIDIFY_EVEN = (1 << 1),
  MOD_SOLIDIFY_NORMAL_CALC = (1 << 2),
  MOD_SOLIDIFY_VGROUP_INV = (1 << 3),
#ifdef DNA_DEPRECATED_ALLOW
  MOD_SOLIDIFY_RIM_MATERIAL = (1 << 4), /* deprecated, used in do_versions */
#endif
  MOD_SOLIDIFY_FLIP = (1 << 5),
  MOD_SOLIDIFY_NOSHELL = (1 << 6),
  MOD_SOLIDIFY_OFFSET_ANGLE_CLAMP = (1 << 7),
  MOD_SOLIDIFY_NONMANIFOLD_FLAT_FACES = (1 << 8),
};

/** #SolidifyModifierData.mode */
enum {
  MOD_SOLIDIFY_MODE_EXTRUDE = 0,
  MOD_SOLIDIFY_MODE_NONMANIFOLD = 1,
};

/** #SolidifyModifierData.nonmanifold_offset_mode */
enum {
  MOD_SOLIDIFY_NONMANIFOLD_OFFSET_MODE_FIXED = 0,
  MOD_SOLIDIFY_NONMANIFOLD_OFFSET_MODE_EVEN = 1,
  MOD_SOLIDIFY_NONMANIFOLD_OFFSET_MODE_CONSTRAINTS = 2,
};

/** #SolidifyModifierData.nonmanifold_boundary_mode */
enum {
  MOD_SOLIDIFY_NONMANIFOLD_BOUNDARY_MODE_NONE = 0,
  MOD_SOLIDIFY_NONMANIFOLD_BOUNDARY_MODE_ROUND = 1,
  MOD_SOLIDIFY_NONMANIFOLD_BOUNDARY_MODE_FLAT = 2,
};

typedef struct ScrewModifierData {
  ModifierData modifier;

  struct Object *ob_axis;
  unsigned int steps;
  unsigned int render_steps;
  unsigned int iter;
  float screw_ofs;
  float angle;
  float merge_dist;
  short flag;
  char axis;
  char _pad[5];
} ScrewModifierData;

enum {
  MOD_SCREW_NORMAL_FLIP = (1 << 0),
  MOD_SCREW_NORMAL_CALC = (1 << 1),
  MOD_SCREW_OBJECT_OFFSET = (1 << 2),
  /*  MOD_SCREW_OBJECT_ANGLE   = (1 << 4), */
  MOD_SCREW_SMOOTH_SHADING = (1 << 5),
  MOD_SCREW_UV_STRETCH_U = (1 << 6),
  MOD_SCREW_UV_STRETCH_V = (1 << 7),
  MOD_SCREW_MERGE = (1 << 8),
};

typedef struct OceanModifierData {
  ModifierData modifier;

  struct Ocean *ocean;
  struct OceanCache *oceancache;

  int resolution;
  int spatial_size;

  float wind_velocity;

  float damp;
  float smallest_wave;
  float depth;

  float wave_alignment;
  float wave_direction;
  float wave_scale;

  float chop_amount;
  float foam_coverage;
  float time;

  char _pad1[4];

  /* Spectrum being used. */
  int spectrum;

  /* Common JONSWAP parameters. */
  /**
   * This is the distance from a lee shore, called the fetch, or the distance
   * over which the wind blows with constant velocity.
   */
  float fetch_jonswap;
  float sharpen_peak_jonswap;

  int bakestart;
  int bakeend;

  /** FILE_MAX. */
  char cachepath[1024];
  /** MAX_CUSTOMDATA_LAYER_NAME. */
  char foamlayername[64];
  char cached;
  char geometry_mode;

  char flag;
  char _pad2;

  short repeat_x;
  short repeat_y;

  int seed;

  float size;

  float foam_fade;

  char _pad[4];
} OceanModifierData;

enum {
  MOD_OCEAN_GEOM_GENERATE = 0,
  MOD_OCEAN_GEOM_DISPLACE = 1,
  MOD_OCEAN_GEOM_SIM_ONLY = 2,
};

enum {
  MOD_OCEAN_SPECTRUM_PHILLIPS = 0,
  MOD_OCEAN_SPECTRUM_PIERSON_MOSKOWITZ = 1,
  MOD_OCEAN_SPECTRUM_JONSWAP = 2,
  MOD_OCEAN_SPECTRUM_TEXEL_MARSEN_ARSLOE = 3,
};

enum {
  MOD_OCEAN_GENERATE_FOAM = (1 << 0),
  MOD_OCEAN_GENERATE_NORMALS = (1 << 1),
};

typedef struct WarpModifierData {
  ModifierData modifier;
  /* keep in sync with MappingInfoModifierData */
  struct Tex *texture;
  struct Object *map_object;
  char map_bone[64];
  /** MAX_CUSTOMDATA_LAYER_NAME. */
  char uvlayer_name[64];
  int uvlayer_tmp;
  int texmapping;
  /* end MappingInfoModifierData */

  struct Object *object_from;
  struct Object *object_to;
  /** Optional name of bone target, MAX_ID_NAME-2. */
  char bone_from[64];
  /** Optional name of bone target, MAX_ID_NAME-2. */
  char bone_to[64];

  struct CurveMapping *curfalloff;
  /** Optional vertexgroup name, MAX_VGROUP_NAME. */
  char defgrp_name[64];
  float strength;
  float falloff_radius;
  char flag;
  char falloff_type;
  char _pad[6];
} WarpModifierData;

/* WarpModifierData->flag */
enum {
  MOD_WARP_VOLUME_PRESERVE = (1 << 0),
  MOD_WARP_INVERT_VGROUP = (1 << 1),
};

typedef enum {
  eWarp_Falloff_None = 0,
  eWarp_Falloff_Curve = 1,
  eWarp_Falloff_Sharp = 2,     /* PROP_SHARP */
  eWarp_Falloff_Smooth = 3,    /* PROP_SMOOTH */
  eWarp_Falloff_Root = 4,      /* PROP_ROOT */
  eWarp_Falloff_Linear = 5,    /* PROP_LIN */
  eWarp_Falloff_Const = 6,     /* PROP_CONST */
  eWarp_Falloff_Sphere = 7,    /* PROP_SPHERE */
  eWarp_Falloff_InvSquare = 8, /* PROP_INVSQUARE */
  /* PROP_RANDOM not used */
} WarpModifierFalloff;

typedef struct WeightVGEditModifierData {
  ModifierData modifier;

  /** Name of vertex group to edit. MAX_VGROUP_NAME. */
  char defgrp_name[64];

  /** Using MOD_WVG_EDIT_* flags. */
  short edit_flags;
  /** Using MOD_WVG_MAPPING_* defines. */
  short falloff_type;
  /** Weight for vertices not in vgroup. */
  float default_weight;

  /* Mapping stuff. */
  /** The custom mapping curve!. */
  struct CurveMapping *cmap_curve;

  /* The add/remove vertices weight thresholds. */
  float add_threshold, rem_threshold;

  /* Masking options. */
  /** The global "influence", if no vgroup nor tex is used as mask. */
  float mask_constant;
  /** Name of mask vertex group from which to get weight factors. MAX_VGROUP_NAME. */
  char mask_defgrp_name[64];

  /* Texture masking. */
  /** Which channel to use as weightf. */
  int mask_tex_use_channel;
  /** The texture. */
  struct Tex *mask_texture;
  /** Name of the map object. */
  struct Object *mask_tex_map_obj;
  /** Name of the map bone. */
  char mask_tex_map_bone[64];
  /** How to map the texture (using MOD_DISP_MAP_* enums). */
  int mask_tex_mapping;
  /** Name of the UV map. MAX_CUSTOMDATA_LAYER_NAME. */
  char mask_tex_uvlayer_name[64];

  /* Padding... */
  char _pad0[4];
} WeightVGEditModifierData;

/* WeightVGEdit flags. */
enum {
  MOD_WVG_EDIT_WEIGHTS_NORMALIZE = (1 << 0),
  MOD_WVG_INVERT_FALLOFF = (1 << 1),
  MOD_WVG_EDIT_INVERT_VGROUP_MASK = (1 << 2),
  /** Add vertices with higher weight than threshold to vgroup. */
  MOD_WVG_EDIT_ADD2VG = (1 << 3),
  /** Remove vertices with lower weight than threshold from vgroup. */
  MOD_WVG_EDIT_REMFVG = (1 << 4),
};

typedef struct WeightVGMixModifierData {
  ModifierData modifier;

  /** Name of vertex group to modify/weight. MAX_VGROUP_NAME. */
  char defgrp_name_a[64];
  /** Name of other vertex group to mix in. MAX_VGROUP_NAME. */
  char defgrp_name_b[64];
  /** Default weight value for first vgroup. */
  float default_weight_a;
  /** Default weight value to mix in. */
  float default_weight_b;
  /** How second vgroups weights affect first ones. */
  char mix_mode;
  /** What vertices to affect. */
  char mix_set;

  char _pad0[6];

  /* Masking options. */
  /** The global "influence", if no vgroup nor tex is used as mask. */
  float mask_constant;
  /** Name of mask vertex group from which to get weight factors. MAX_VGROUP_NAME. */
  char mask_defgrp_name[64];

  /* Texture masking. */
  /** Which channel to use as weightf. */
  int mask_tex_use_channel;
  /** The texture. */
  struct Tex *mask_texture;
  /** Name of the map object. */
  struct Object *mask_tex_map_obj;
  /** Name of the map bone. */
  char mask_tex_map_bone[64];
  /** How to map the texture!. */
  int mask_tex_mapping;
  /** Name of the UV map. MAX_CUSTOMDATA_LAYER_NAME. */
  char mask_tex_uvlayer_name[64];

  char flag;

  /* Padding... */
  char _pad1[3];
} WeightVGMixModifierData;

/* How second vgroup's weights affect first ones. */
enum {
  /** Second weights replace weights. */
  MOD_WVG_MIX_SET = 1,
  /** Second weights are added to weights. */
  MOD_WVG_MIX_ADD = 2,
  /** Second weights are subtracted from weights. */
  MOD_WVG_MIX_SUB = 3,
  /** Second weights are multiplied with weights. */
  MOD_WVG_MIX_MUL = 4,
  /** Second weights divide weights. */
  MOD_WVG_MIX_DIV = 5,
  /** Difference between second weights and weights. */
  MOD_WVG_MIX_DIF = 6,
  /** Average of both weights. */
  MOD_WVG_MIX_AVG = 7,
};

/* What vertices to affect. */
enum {
  /** Affect all vertices. */
  MOD_WVG_SET_ALL = 1,
  /** Affect only vertices in first vgroup. */
  MOD_WVG_SET_A = 2,
  /** Affect only vertices in second vgroup. */
  MOD_WVG_SET_B = 3,
  /** Affect only vertices in one vgroup or the other. */
  MOD_WVG_SET_OR = 4,
  /** Affect only vertices in both vgroups. */
  MOD_WVG_SET_AND = 5,
};

/* WeightVGMix->flag */
enum {
  MOD_WVG_MIX_INVERT_VGROUP_MASK = (1 << 0),
  MOD_WVG_MIX_WEIGHTS_NORMALIZE = (1 << 1),
  MOD_WVG_MIX_INVERT_VGROUP_A = (1 << 2),
  MOD_WVG_MIX_INVERT_VGROUP_B = (1 << 3),
};

typedef struct WeightVGProximityModifierData {
  ModifierData modifier;

  /** Name of vertex group to modify/weight. MAX_VGROUP_NAME. */
  char defgrp_name[64];

  /* Proximity modes. */
  int proximity_mode;
  int proximity_flags;

  /* Target object from which to calculate vertices distances. */
  struct Object *proximity_ob_target;

  /* Masking options. */
  /** The global "influence", if no vgroup nor tex is used as mask. */
  float mask_constant;
  /** Name of mask vertex group from which to get weight factors. MAX_VGROUP_NAME. */
  char mask_defgrp_name[64];

  /* Texture masking. */
  /** Which channel to use as weightf. */
  int mask_tex_use_channel;
  /** The texture. */
  struct Tex *mask_texture;
  /** Name of the map object. */
  struct Object *mask_tex_map_obj;
  /** Name of the map bone. */
  char mask_tex_map_bone[64];
  /** How to map the texture!. */
  int mask_tex_mapping;
  /** Name of the UV Map. MAX_CUSTOMDATA_LAYER_NAME. */
  char mask_tex_uvlayer_name[64];

  /** Distances mapping to 0.0/1.0 weights. */
  float min_dist, max_dist;

  /* Put here to avoid breaking existing struct... */
  /** Using MOD_WVG_MAPPING_* enums. */
  short falloff_type;

  /* Padding... */
  char _pad0[2];
} WeightVGProximityModifierData;

/* Modes of proximity weighting. */
enum {
  MOD_WVG_PROXIMITY_OBJECT = 1,   /* source vertex to other location */
  MOD_WVG_PROXIMITY_GEOMETRY = 2, /* source vertex to other geometry */
};

/* Flags options for proximity weighting. */
enum {
  /* Use nearest vertices of target obj, in MOD_WVG_PROXIMITY_GEOMETRY mode. */
  MOD_WVG_PROXIMITY_GEOM_VERTS = (1 << 0),
  /* Use nearest edges of target obj, in MOD_WVG_PROXIMITY_GEOMETRY mode. */
  MOD_WVG_PROXIMITY_GEOM_EDGES = (1 << 1),
  /* Use nearest faces of target obj, in MOD_WVG_PROXIMITY_GEOMETRY mode. */
  MOD_WVG_PROXIMITY_GEOM_FACES = (1 << 2),
  MOD_WVG_PROXIMITY_INVERT_VGROUP_MASK = (1 << 3),
  MOD_WVG_PROXIMITY_INVERT_FALLOFF = (1 << 4),
  MOD_WVG_PROXIMITY_WEIGHTS_NORMALIZE = (1 << 3),
};

/* Defines common to all WeightVG modifiers. */
/* Mapping modes. */
enum {
  MOD_WVG_MAPPING_NONE = 0,
  MOD_WVG_MAPPING_CURVE = 1,
  MOD_WVG_MAPPING_SHARP = 2,  /* PROP_SHARP */
  MOD_WVG_MAPPING_SMOOTH = 3, /* PROP_SMOOTH */
  MOD_WVG_MAPPING_ROOT = 4,   /* PROP_ROOT */
  /* PROP_LIN not used (same as NONE, here...). */
  /* PROP_CONST not used. */
  MOD_WVG_MAPPING_SPHERE = 7, /* PROP_SPHERE */
  MOD_WVG_MAPPING_RANDOM = 8, /* PROP_RANDOM */
  MOD_WVG_MAPPING_STEP = 9,   /* Median Step. */
};

/* Tex channel to be used as mask. */
enum {
  MOD_WVG_MASK_TEX_USE_INT = 1,
  MOD_WVG_MASK_TEX_USE_RED = 2,
  MOD_WVG_MASK_TEX_USE_GREEN = 3,
  MOD_WVG_MASK_TEX_USE_BLUE = 4,
  MOD_WVG_MASK_TEX_USE_HUE = 5,
  MOD_WVG_MASK_TEX_USE_SAT = 6,
  MOD_WVG_MASK_TEX_USE_VAL = 7,
  MOD_WVG_MASK_TEX_USE_ALPHA = 8,
};

typedef struct DynamicPaintModifierData {
  ModifierData modifier;

  struct DynamicPaintCanvasSettings *canvas;
  struct DynamicPaintBrushSettings *brush;
  /** UI display: canvas / brush. */
  int type;
  char _pad[4];
} DynamicPaintModifierData;

/* Dynamic paint modifier flags */
enum {
  MOD_DYNAMICPAINT_TYPE_CANVAS = (1 << 0),
  MOD_DYNAMICPAINT_TYPE_BRUSH = (1 << 1),
};

/* Remesh modifier */
typedef enum eRemeshModifierFlags {
  MOD_REMESH_FLOOD_FILL = (1 << 0),
  MOD_REMESH_SMOOTH_SHADING = (1 << 1),
} RemeshModifierFlags;

typedef enum eRemeshModifierMode {
  /* blocky */
  MOD_REMESH_CENTROID = 0,
  /* smooth */
  MOD_REMESH_MASS_POINT = 1,
  /* keeps sharp edges */
  MOD_REMESH_SHARP_FEATURES = 2,
  /* Voxel remesh */
  MOD_REMESH_VOXEL = 3,
} eRemeshModifierMode;

typedef struct RemeshModifierData {
  ModifierData modifier;

  /* floodfill option, controls how small components can be before they are removed */
  float threshold;

  /* ratio between size of model and grid */
  float scale;

  float hermite_num;

  /* octree depth */
  char depth;
  char flag;
  char mode;
  char _pad;

  /* OpenVDB Voxel remesh properties. */
  float voxel_size;
  float adaptivity;
} RemeshModifierData;

/* Skin modifier */
typedef struct SkinModifierData {
  ModifierData modifier;

  float branch_smoothing;

  char flag;

  char symmetry_axes;

  char _pad[2];
} SkinModifierData;

/* SkinModifierData.symmetry_axes */
enum {
  MOD_SKIN_SYMM_X = (1 << 0),
  MOD_SKIN_SYMM_Y = (1 << 1),
  MOD_SKIN_SYMM_Z = (1 << 2),
};

/* SkinModifierData.flag */
enum {
  MOD_SKIN_SMOOTH_SHADING = 1,
};

/* Triangulate modifier */
typedef struct TriangulateModifierData {
  ModifierData modifier;

  int flag;
  int quad_method;
  int ngon_method;
  int min_vertices;
} TriangulateModifierData;

/* TriangulateModifierData.flag */
enum {
#ifdef DNA_DEPRECATED_ALLOW
  MOD_TRIANGULATE_BEAUTY = (1 << 0), /* deprecated */
#endif
  MOD_TRIANGULATE_KEEP_CUSTOMLOOP_NORMALS = 1 << 1,
};

/* Triangulate methods - NGons */
enum {
  MOD_TRIANGULATE_NGON_BEAUTY = 0,
  MOD_TRIANGULATE_NGON_EARCLIP = 1,
};

/* Triangulate methods - Quads */
enum {
  MOD_TRIANGULATE_QUAD_BEAUTY = 0,
  MOD_TRIANGULATE_QUAD_FIXED = 1,
  MOD_TRIANGULATE_QUAD_ALTERNATE = 2,
  MOD_TRIANGULATE_QUAD_SHORTEDGE = 3,
};

typedef struct LaplacianSmoothModifierData {
  ModifierData modifier;

  float lambda, lambda_border;
  char _pad1[4];
  /** MAX_VGROUP_NAME. */
  char defgrp_name[64];
  short flag, repeat;
} LaplacianSmoothModifierData;

/* Smooth modifier flags */
enum {
  MOD_LAPLACIANSMOOTH_X = (1 << 1),
  MOD_LAPLACIANSMOOTH_Y = (1 << 2),
  MOD_LAPLACIANSMOOTH_Z = (1 << 3),
  MOD_LAPLACIANSMOOTH_PRESERVE_VOLUME = (1 << 4),
  MOD_LAPLACIANSMOOTH_NORMALIZED = (1 << 5),
  MOD_LAPLACIANSMOOTH_INVERT_VGROUP = (1 << 6),
};

typedef struct CorrectiveSmoothDeltaCache {
  /* delta's between the original positions and the smoothed positions */
  float (*deltas)[3];
  unsigned int totverts;

  /* Value of settings when creating the cache.
   * These are used to check if the cache should be recomputed. */
  float lambda, scale;
  short repeat, flag;
  char smooth_type, rest_source;
  char _pad[6];
} CorrectiveSmoothDeltaCache;

typedef struct CorrectiveSmoothModifierData {
  ModifierData modifier;

  /* positions set during 'bind' operator
   * use for MOD_CORRECTIVESMOOTH_RESTSOURCE_BIND */
  float (*bind_coords)[3];

  /* note: -1 is used to bind */
  unsigned int bind_coords_num;

  float lambda, scale;
  short repeat, flag;
  char smooth_type, rest_source;
  char _pad[6];

  /** MAX_VGROUP_NAME. */
  char defgrp_name[64];

  /* runtime-only cache */
  CorrectiveSmoothDeltaCache delta_cache;
} CorrectiveSmoothModifierData;

enum {
  MOD_CORRECTIVESMOOTH_SMOOTH_SIMPLE = 0,
  MOD_CORRECTIVESMOOTH_SMOOTH_LENGTH_WEIGHT = 1,
};

enum {
  MOD_CORRECTIVESMOOTH_RESTSOURCE_ORCO = 0,
  MOD_CORRECTIVESMOOTH_RESTSOURCE_BIND = 1,
};

/* Corrective Smooth modifier flags */
enum {
  MOD_CORRECTIVESMOOTH_INVERT_VGROUP = (1 << 0),
  MOD_CORRECTIVESMOOTH_ONLY_SMOOTH = (1 << 1),
  MOD_CORRECTIVESMOOTH_PIN_BOUNDARY = (1 << 2),
};

typedef struct UVWarpModifierData {
  ModifierData modifier;

  char axis_u, axis_v;
  short flag;
  /** Used for rotate/scale. */
  float center[2];

  float offset[2];
  float scale[2];
  float rotation;

  /** Source. */
  struct Object *object_src;
  /** Optional name of bone target, MAX_ID_NAME-2. */
  char bone_src[64];
  /** Target. */
  struct Object *object_dst;
  /** Optional name of bone target, MAX_ID_NAME-2. */
  char bone_dst[64];

  /** Optional vertexgroup name, MAX_VGROUP_NAME. */
  char vgroup_name[64];
  /** MAX_CUSTOMDATA_LAYER_NAME. */
  char uvlayer_name[64];
} UVWarpModifierData;

/* UVWarp modifier flags */
enum {
  MOD_UVWARP_INVERT_VGROUP = 1 << 0,
};

/* cache modifier */
typedef struct MeshCacheModifierData {
  ModifierData modifier;

  char flag;
  /** File format. */
  char type;
  char time_mode;
  char play_mode;

  /* axis conversion */
  char forward_axis;
  char up_axis;
  char flip_axis;

  char interp;

  float factor;
  char deform_mode;
  char _pad[7];

  /* play_mode == MOD_MESHCACHE_PLAY_CFEA */
  float frame_start;
  float frame_scale;

  /* play_mode == MOD_MESHCACHE_PLAY_EVAL */
  /* we could use one float for all these but their purpose is very different */
  float eval_frame;
  float eval_time;
  float eval_factor;

  /** FILE_MAX. */
  char filepath[1024];
} MeshCacheModifierData;

enum {
  MOD_MESHCACHE_TYPE_MDD = 1,
  MOD_MESHCACHE_TYPE_PC2 = 2,
};

enum {
  MOD_MESHCACHE_DEFORM_OVERWRITE = 0,
  MOD_MESHCACHE_DEFORM_INTEGRATE = 1,
};

enum {
  MOD_MESHCACHE_INTERP_NONE = 0,
  MOD_MESHCACHE_INTERP_LINEAR = 1,
  /*  MOD_MESHCACHE_INTERP_CARDINAL  = 2, */
};

enum {
  MOD_MESHCACHE_TIME_FRAME = 0,
  MOD_MESHCACHE_TIME_SECONDS = 1,
  MOD_MESHCACHE_TIME_FACTOR = 2,
};

enum {
  MOD_MESHCACHE_PLAY_CFEA = 0,
  MOD_MESHCACHE_PLAY_EVAL = 1,
};

typedef struct LaplacianDeformModifierData {
  ModifierData modifier;
  /** MAX_VGROUP_NAME. */
  char anchor_grp_name[64];
  int total_verts, repeat;
  float *vertexco;
  /** Runtime only. */
  void *cache_system;
  short flag;
  char _pad[6];

} LaplacianDeformModifierData;

/* Laplacian Deform modifier flags */
enum {
  MOD_LAPLACIANDEFORM_BIND = 1 << 0,
  MOD_LAPLACIANDEFORM_INVERT_VGROUP = 1 << 1,
};

/* many of these options match 'solidify' */
typedef struct WireframeModifierData {
  ModifierData modifier;
  /** MAX_VGROUP_NAME. */
  char defgrp_name[64];
  float offset;
  float offset_fac;
  float offset_fac_vg;
  float crease_weight;
  short flag, mat_ofs;
  char _pad[4];
} WireframeModifierData;

enum {
  MOD_WIREFRAME_INVERT_VGROUP = (1 << 0),
  MOD_WIREFRAME_REPLACE = (1 << 1),
  MOD_WIREFRAME_BOUNDARY = (1 << 2),
  MOD_WIREFRAME_OFS_EVEN = (1 << 3),
  MOD_WIREFRAME_OFS_RELATIVE = (1 << 4),
  MOD_WIREFRAME_CREASE = (1 << 5),
};

typedef struct WeldModifierData {
  ModifierData modifier;

  /* The limit below which to merge vertices. */
  float merge_dist;
  unsigned int max_interactions;
  /* Name of vertex group to use to mask, MAX_VGROUP_NAME. */
  char defgrp_name[64];

  short flag;
  char _pad[6];
} WeldModifierData;

/* WeldModifierData->flag */
enum {
  MOD_WELD_INVERT_VGROUP = (1 << 0),
};

typedef struct DataTransferModifierData {
  ModifierData modifier;

  struct Object *ob_source;

  /** See DT_TYPE_ enum in ED_object.h. */
  int data_types;

  /* See MREMAP_MODE_ enum in BKE_mesh_mapping.h */
  int vmap_mode;
  int emap_mode;
  int lmap_mode;
  int pmap_mode;

  float map_max_distance;
  float map_ray_radius;
  float islands_precision;

  char _pad1[4];

  /** DT_MULTILAYER_INDEX_MAX; See DT_FROMLAYERS_ enum in ED_object.h. */
  int layers_select_src[4];
  /** DT_MULTILAYER_INDEX_MAX; See DT_TOLAYERS_ enum in ED_object.h. */
  int layers_select_dst[4];

  /** See CDT_MIX_ enum in BKE_customdata.h. */
  int mix_mode;
  float mix_factor;
  /** MAX_VGROUP_NAME. */
  char defgrp_name[64];

  int flags;
} DataTransferModifierData;

/* DataTransferModifierData.flags */
enum {
  MOD_DATATRANSFER_OBSRC_TRANSFORM = 1 << 0,
  MOD_DATATRANSFER_MAP_MAXDIST = 1 << 1,
  MOD_DATATRANSFER_INVERT_VGROUP = 1 << 2,

  /* Only for UI really. */
  MOD_DATATRANSFER_USE_VERT = 1 << 28,
  MOD_DATATRANSFER_USE_EDGE = 1 << 29,
  MOD_DATATRANSFER_USE_LOOP = 1 << 30,
  MOD_DATATRANSFER_USE_POLY = 1u << 31,
};

/* Set Split Normals modifier */
typedef struct NormalEditModifierData {
  ModifierData modifier;
  /** MAX_VGROUP_NAME. */
  char defgrp_name[64];
  /** Source of normals, or center of ellipsoid. */
  struct Object *target;
  short mode;
  short flag;
  short mix_mode;
  char _pad[2];
  float mix_factor;
  float mix_limit;
  float offset[3];
  char _pad0[4];
} NormalEditModifierData;

/* NormalEditModifierData.mode */
enum {
  MOD_NORMALEDIT_MODE_RADIAL = 0,
  MOD_NORMALEDIT_MODE_DIRECTIONAL = 1,
};

/* NormalEditModifierData.flags */
enum {
  MOD_NORMALEDIT_INVERT_VGROUP = (1 << 0),
  MOD_NORMALEDIT_USE_DIRECTION_PARALLEL = (1 << 1),
  MOD_NORMALEDIT_NO_POLYNORS_FIX = (1 << 2),
};

/* NormalEditModifierData.mix_mode */
enum {
  MOD_NORMALEDIT_MIX_COPY = 0,
  MOD_NORMALEDIT_MIX_ADD = 1,
  MOD_NORMALEDIT_MIX_SUB = 2,
  MOD_NORMALEDIT_MIX_MUL = 3,
};

typedef struct MeshSeqCacheModifierData {
  ModifierData modifier;

  struct CacheFile *cache_file;
  /** 1024 = FILE_MAX. */
  char object_path[1024];

  char read_flag;
  char _pad[7];

  /* Runtime. */
  struct CacheReader *reader;
  char reader_object_path[1024];
} MeshSeqCacheModifierData;

/* MeshSeqCacheModifierData.read_flag */
enum {
  MOD_MESHSEQ_READ_VERT = (1 << 0),
  MOD_MESHSEQ_READ_POLY = (1 << 1),
  MOD_MESHSEQ_READ_UV = (1 << 2),
  MOD_MESHSEQ_READ_COLOR = (1 << 3),
};

typedef struct SDefBind {
  unsigned int *vert_inds;
  unsigned int numverts;
  int mode;
  float *vert_weights;
  float normal_dist;
  float influence;
} SDefBind;

typedef struct SDefVert {
  SDefBind *binds;
  unsigned int numbinds;
  char _pad[4];
} SDefVert;

typedef struct SurfaceDeformModifierData {
  ModifierData modifier;

  struct Depsgraph *depsgraph;
  /** Bind target object. */
  struct Object *target;
  /** Vertex bind data. */
  SDefVert *verts;
  float falloff;
  unsigned int numverts, numpoly;
  int flags;
  float mat[4][4];
  float strength;
  char _pad[4];
  char defgrp_name[64];
} SurfaceDeformModifierData;

/* Surface Deform modifier flags */
enum {
  /* This indicates "do bind on next modifier evaluation" as well as "is bound". */
  MOD_SDEF_BIND = (1 << 0),
  MOD_SDEF_INVERT_VGROUP = (1 << 1)

  /* MOD_SDEF_USES_LOOPTRI = (1 << 1), */ /* UNUSED */
  /* MOD_SDEF_HAS_CONCAVE = (1 << 2), */  /* UNUSED */
};

/* Surface Deform vertex bind modes */
enum {
  MOD_SDEF_MODE_LOOPTRI = 0,
  MOD_SDEF_MODE_NGON = 1,
  MOD_SDEF_MODE_CENTROID = 2,
};

typedef struct WeightedNormalModifierData {
  ModifierData modifier;

  /** MAX_VGROUP_NAME. */
  char defgrp_name[64];
  char mode, flag;
  short weight;
  float thresh;
} WeightedNormalModifierData;

/* Name/id of the generic PROP_INT cdlayer storing face weights. */
#define MOD_WEIGHTEDNORMALS_FACEWEIGHT_CDLAYER_ID "__mod_weightednormals_faceweight"

/* WeightedNormalModifierData.mode */
enum {
  MOD_WEIGHTEDNORMAL_MODE_FACE = 0,
  MOD_WEIGHTEDNORMAL_MODE_ANGLE = 1,
  MOD_WEIGHTEDNORMAL_MODE_FACE_ANGLE = 2,
};

/* WeightedNormalModifierData.flag */
enum {
  MOD_WEIGHTEDNORMAL_KEEP_SHARP = (1 << 0),
  MOD_WEIGHTEDNORMAL_INVERT_VGROUP = (1 << 1),
  MOD_WEIGHTEDNORMAL_FACE_INFLUENCE = (1 << 2),
};

#define MOD_MESHSEQ_READ_ALL \
  (MOD_MESHSEQ_READ_VERT | MOD_MESHSEQ_READ_POLY | MOD_MESHSEQ_READ_UV | MOD_MESHSEQ_READ_COLOR)

typedef struct SimulationModifierData {
  ModifierData modifier;

  struct Simulation *simulation;
  char data_path[64];
} SimulationModifierData;

#ifdef __cplusplus
}
#endif

#endif /* __DNA_MODIFIER_TYPES_H__ */
