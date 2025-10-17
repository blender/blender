/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "BLI_enum_flags.hh"
#include "BLI_implicit_sharing.h"

#include "DNA_defs.h"
#include "DNA_listBase.h"
#include "DNA_modifier_enums.h"
#include "DNA_packedFile_types.h"
#include "DNA_session_uid_types.h"

#ifdef __cplusplus
#  include "BLI_span.hh"

namespace blender {
struct NodesModifierRuntime;
namespace bke {
struct BVHTreeFromMesh;
}
}  // namespace blender
using NodesModifierRuntimeHandle = blender::NodesModifierRuntime;
using BVHTreeFromMeshHandle = blender::bke::BVHTreeFromMesh;
#else
typedef struct NodesModifierRuntimeHandle NodesModifierRuntimeHandle;
typedef struct BVHTreeFromMeshHandle BVHTreeFromMeshHandle;
#endif
struct LineartModifierRuntime;

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
  eModifierType_Nodes = 57,
  eModifierType_MeshToVolume = 58,
  eModifierType_VolumeDisplace = 59,
  eModifierType_VolumeToMesh = 60,
  eModifierType_GreasePencilOpacity = 61,
  eModifierType_GreasePencilSubdiv = 62,
  eModifierType_GreasePencilColor = 63,
  eModifierType_GreasePencilTint = 64,
  eModifierType_GreasePencilSmooth = 65,
  eModifierType_GreasePencilOffset = 66,
  eModifierType_GreasePencilNoise = 67,
  eModifierType_GreasePencilMirror = 68,
  eModifierType_GreasePencilThickness = 69,
  eModifierType_GreasePencilLattice = 70,
  eModifierType_GreasePencilDash = 71,
  eModifierType_GreasePencilMultiply = 72,
  eModifierType_GreasePencilLength = 73,
  eModifierType_GreasePencilWeightAngle = 74,
  eModifierType_GreasePencilArray = 75,
  eModifierType_GreasePencilWeightProximity = 76,
  eModifierType_GreasePencilHook = 77,
  eModifierType_GreasePencilLineart = 78,
  eModifierType_GreasePencilArmature = 79,
  eModifierType_GreasePencilTime = 80,
  eModifierType_GreasePencilEnvelope = 81,
  eModifierType_GreasePencilOutline = 82,
  eModifierType_GreasePencilShrinkwrap = 83,
  eModifierType_GreasePencilBuild = 84,
  eModifierType_GreasePencilSimplify = 85,
  eModifierType_GreasePencilTexture = 86,
  NUM_MODIFIER_TYPES,
} ModifierType;

typedef enum ModifierMode {
  eModifierMode_Realtime = (1 << 0),
  eModifierMode_Render = (1 << 1),
  eModifierMode_Editmode = (1 << 2),
  eModifierMode_OnCage = (1 << 3),
#ifdef DNA_DEPRECATED_ALLOW
  /** Old modifier box expansion, just for versioning. */
  eModifierMode_Expanded_DEPRECATED = (1 << 4),
#endif
  eModifierMode_Virtual = (1 << 5),
  eModifierMode_ApplyOnSpline = (1 << 6),
  eModifierMode_DisableTemporary = (1u << 31),
} ModifierMode;
ENUM_OPERATORS(ModifierMode);

typedef struct ModifierData {
  struct ModifierData *next, *prev;

  /** #ModifierType. */
  int type;
  /** #ModifierMode. */
  int mode;
  /** Time in seconds that the modifier took to evaluate. This is only set on evaluated objects. */
  float execution_time;
  /** #ModifierFlag. */
  short flag;
  /** An "expand" bit for each of the modifier's (sub)panels (#uiPanelDataExpansion). */
  short ui_expand_flag;
  /**
   * Bits that can be used for open-states of layout panels in the modifier. This can replace
   * `ui_expand_flag` once all modifiers use layout panels. Currently, trying to reuse the same
   * flags is problematic, because the bits in `ui_expand_flag` are mapped to panels automatically
   * and easily conflict with the explicit mapping of bits to panels here.
   */
  uint16_t layout_panel_open_flag;
  char _pad[2];
  /**
   * Uniquely identifies the modifier within the object. This identifier is stable across Blender
   * sessions. Modifiers on the original and corresponding evaluated object have matching
   * identifiers. The identifier stays the same if the modifier is renamed or moved in the modifier
   * stack.
   *
   * A valid identifier is non-negative (>= 1). Modifiers that are currently not on an object may
   * have invalid identifiers. It has to be initialized with #BKE_modifiers_persistent_uid_init
   * when it is added to an object.
   */
  int persistent_uid;
  char name[/*MAX_NAME*/ 64];

  char *error;

  /** Runtime field which contains runtime data which is specific to a modifier type. */
  void *runtime;
} ModifierData;

typedef enum {
  /** This modifier has been inserted in local override, and hence can be fully edited. */
  eModifierFlag_OverrideLibrary_Local = (1 << 0),
  /** This modifier does not own its caches, but instead shares them with another modifier. */
  eModifierFlag_SharedCaches = (1 << 1),
  /**
   * This modifier is the object's active modifier. Used for context in the node editor.
   * Only one modifier on an object should have this flag set.
   */
  eModifierFlag_Active = (1 << 2),
  /**
   * Only set on modifiers in evaluated objects. The flag indicates that the user modified inputs
   * to the modifier which might invalidate simulation caches.
   */
  eModifierFlag_UserModified = (1 << 3),
  /**
   * New modifiers are added before this modifier, and dragging non-pinned modifiers after is
   * disabled.
   */
  eModifierFlag_PinLast = (1 << 4),
} ModifierFlag;

/**
 * \note Not a real modifier.
 */
typedef struct MappingInfoModifierData {
  ModifierData modifier;

  struct Tex *texture;
  struct Object *map_object;
  char map_bone[/*MAXBONENAME*/ 64];
  char uvlayer_name[/*MAX_CUSTOMDATA_LAYER_NAME*/ 68];
  char _pad1[4];
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
  eSubsurfModifierFlag_UseCustomNormals = (1 << 5),
  eSubsurfModifierFlag_UseRecursiveSubdivision = (1 << 6),
  eSubsurfModifierFlag_UseAdaptiveSubdivision = (1 << 7),
} SubsurfModifierFlag;

typedef enum {
  SUBSURF_ADAPTIVE_SPACE_PIXEL = 0,
  SUBSURF_ADAPTIVE_SPACE_OBJECT = 1,
} eSubsurfAdaptiveSpace;

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

typedef enum {
  SUBSURF_BOUNDARY_SMOOTH_ALL = 0,
  SUBSURF_BOUNDARY_SMOOTH_PRESERVE_CORNERS = 1,
} eSubsurfBoundarySmooth;

typedef struct SubsurfModifierData {
  ModifierData modifier;

  /** #MeshSubdivType. */
  short subdivType;
  short levels, renderLevels;
  /** #SubsurfModifierFlag. */
  short flags;
  /** #eSubsurfUVSmooth. */
  short uv_smooth;
  short quality;
  /** #eSubsurfBoundarySmooth. */
  short boundary_smooth;
  /* Adaptive subdivision. */
  /** #eSubsurfAdaptiveSpace */
  short adaptive_space;
  float adaptive_pixel_size;
  float adaptive_object_edge_length;
} SubsurfModifierData;

typedef struct LatticeModifierData {
  ModifierData modifier;

  struct Object *object;
  /** Optional vertex-group name. */
  char name[/*MAX_VGROUP_NAME*/ 64];
  float strength;
  /** #LatticeModifierFlag. */
  short flag;
  char _pad[2];
  void *_pad1;
} LatticeModifierData;

/** #LatticeModifierData.flag */
typedef enum {
  MOD_LATTICE_INVERT_VGROUP = (1 << 0),
} LatticeModifierFlag;

typedef struct CurveModifierData {
  ModifierData modifier;

  struct Object *object;
  /** Optional vertex-group name. */
  char name[/*MAX_VGROUP_NAME*/ 64];
  /** #CurveModifierDefaultAxis. Axis along which curve deforms. */
  short defaxis;
  /** #CurveModifierFlag. */
  short flag;
  char _pad[4];
  void *_pad1;
} CurveModifierData;

/** #CurveModifierData.flag */
typedef enum {
  MOD_CURVE_INVERT_VGROUP = (1 << 0),
} CurveModifierFlag;

/** #CurveModifierData.defaxis */
typedef enum {
  MOD_CURVE_POSX = 1,
  MOD_CURVE_POSY = 2,
  MOD_CURVE_POSZ = 3,
  MOD_CURVE_NEGX = 4,
  MOD_CURVE_NEGY = 5,
  MOD_CURVE_NEGZ = 6,
} CurveModifierDefaultAxis;

typedef struct BuildModifierData {
  ModifierData modifier;

  float start, length;
  /** #BuildModifierFlag. */
  short flag;

  /** (bool) whether order of vertices is randomized - legacy files (for readfile conversion). */
  short randomize;
  /** (int) random seed. */
  int seed;
} BuildModifierData;

/** #BuildModifierData.flag */
typedef enum {
  /** order of vertices is randomized */
  MOD_BUILD_FLAG_RANDOMIZE = (1 << 0),
  /** frame range is reversed, resulting in a deconstruction effect */
  MOD_BUILD_FLAG_REVERSE = (1 << 1),
} BuildModifierFlag;

/** Mask Modifier. */
typedef struct MaskModifierData {
  ModifierData modifier;

  /** Armature to use to in place of hardcoded vgroup. */
  struct Object *ob_arm;
  /** Name of vertex group to use to mask. */
  char vgroup[/*MAX_VGROUP_NAME*/ 64];

  /** #MaskModifierMode. Using armature or hardcoded vgroup. */
  short mode;
  /** #MaskModifierFlag. Flags for various things. */
  short flag;
  float threshold;
  void *_pad1;
} MaskModifierData;

/** #MaskModifierData.mode */
typedef enum {
  MOD_MASK_MODE_VGROUP = 0,
  MOD_MASK_MODE_ARM = 1,
} MaskModifierMode;

/** #MaskModifierData.flag */
typedef enum {
  MOD_MASK_INV = (1 << 0),
  MOD_MASK_SMOOTH = (1 << 1),
} MaskModifierFlag;

typedef struct ArrayModifierData {
  ModifierData modifier;

  /** The object with which to cap the start of the array. */
  struct Object *start_cap;
  /** The object with which to cap the end of the array. */
  struct Object *end_cap;
  /** The curve object to use for #MOD_ARR_FITCURVE. */
  struct Object *curve_ob;
  /** The object to use for object offset. */
  struct Object *offset_ob;
  /**
   * A constant duplicate offset;
   * 1 means the duplicates are 1 unit apart.
   */
  float offset[3];
  /**
   * A scaled factor for duplicate offsets;
   * 1 means the duplicates are 1 object-width apart.
   */
  float scale[3];
  /** The length over which to distribute the duplicates. */
  float length;
  /** The limit below which to merge vertices in adjacent duplicates. */
  float merge_dist;
  /**
   * #ArrayModifierFitType.
   * Determines how duplicate count is calculated; one of:
   * - #MOD_ARR_FIXEDCOUNT -> fixed.
   * - #MOD_ARR_FITLENGTH  -> calculated to fit a set length.
   * - #MOD_ARR_FITCURVE   -> calculated to fit the length of a Curve object.
   */
  int fit_type;
  /**
   * #ArrayModifierOffsetType.
   * Flags specifying how total offset is calculated; binary OR of:
   * - #MOD_ARR_OFF_CONST    -> total offset += offset.
   * - #MOD_ARR_OFF_RELATIVE -> total offset += relative * object width.
   * - #MOD_ARR_OFF_OBJ      -> total offset += offset_ob's matrix.
   * Total offset is the sum of the individual enabled offsets.
   */
  int offset_type;
  /**
   * #ArrayModifierFlag.
   * General flags:
   * #MOD_ARR_MERGE -> merge vertices in adjacent duplicates.
   */
  int flags;
  /** The number of duplicates to generate for #MOD_ARR_FIXEDCOUNT. */
  int count;
  float uv_offset[2];
} ArrayModifierData;

/** #ArrayModifierData.fit_type */
typedef enum {
  MOD_ARR_FIXEDCOUNT = 0,
  MOD_ARR_FITLENGTH = 1,
  MOD_ARR_FITCURVE = 2,
} ArrayModifierFitType;

/** #ArrayModifierData.offset_type */
typedef enum {
  MOD_ARR_OFF_CONST = (1 << 0),
  MOD_ARR_OFF_RELATIVE = (1 << 1),
  MOD_ARR_OFF_OBJ = (1 << 2),
} ArrayModifierOffsetType;

/** #ArrayModifierData.flags */
typedef enum {
  MOD_ARR_MERGE = (1 << 0),
  MOD_ARR_MERGEFINAL = (1 << 1),
} ArrayModifierFlag;

typedef struct MirrorModifierData {
  ModifierData modifier;

  /** Deprecated, use flag instead. */
  short axis DNA_DEPRECATED;
  /** #MirrorModifierFlag. */
  short flag;
  float tolerance;
  float bisect_threshold;

  /** Mirror modifier used to merge the old vertex into its new copy, which would break code
   * relying on access to the original geometry vertices. However, modifying this behavior to the
   * correct one (i.e. merging the copy vertices into their original sources) has several potential
   * effects on other modifiers and tools, so we need to keep that incorrect behavior for existing
   * modifiers, and only use the new correct one for new modifiers. */
  uint8_t use_correct_order_on_merge;

  char _pad[3];
  float uv_offset[2];
  float uv_offset_copy[2];
  struct Object *mirror_ob;
  void *_pad1;
} MirrorModifierData;

/** #MirrorModifierData.flag */
typedef enum {
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
} MirrorModifierFlag;

typedef struct EdgeSplitModifierData {
  ModifierData modifier;

  /** Angle above which edges should be split. */
  float split_angle;
  /** #EdgeSplitModifierFlag. */
  int flags;
} EdgeSplitModifierData;

/** #EdgeSplitModifierData.flags */
typedef enum {
  MOD_EDGESPLIT_FROMANGLE = (1 << 1),
  MOD_EDGESPLIT_FROMFLAG = (1 << 2),
} EdgeSplitModifierFlag;

typedef struct BevelModifierData {
  ModifierData modifier;

  /** The "raw" bevel value (distance/amount to bevel). */
  float value;
  /** The resolution (as originally coded, it is the number of recursive bevels). */
  int res;
  /** #BevelModifierFlag. General option flags. */
  short flags;
  /** #BevelModifierValFlag. Used to interpret the bevel value. */
  short val_flags;
  /** #BevelModifierProfileType. For the type and how we build the bevel's profile. */
  short profile_type;
  /** #BevelModifierFlag. Flags to tell the tool how to limit the bevel. */
  short lim_flags;
  /** Flags to direct how edge weights are applied to verts. */
  short e_flags;
  /** Material index if >= 0, else material inherited from surrounding faces. */
  short mat;
  /** #BevelModifierEdgeFlag. */
  short edge_flags;
  /** #BevelModifierFaceStrengthMode. */
  short face_str_mode;
  /** #BevelModifierMiter. Patterns to use for mitering non-reflex and reflex miter edges */
  short miter_inner;
  short miter_outer;
  /** #BevelModifierVMeshMethod. The method to use for creating >2-way intersections */
  short vmesh_method;
  /** #BevelModifierAffectType. Whether to affect vertices or edges. */
  char affect_type;
  char _pad;
  /** Controls profile shape (0->1, .5 is round). */
  float profile;
  /** if the MOD_BEVEL_ANGLE is set,
   * this will be how "sharp" an edge must be before it gets beveled */
  float bevel_angle;
  float spread;
  /** If the #MOD_BEVEL_VWEIGHT option is set, this will be the name of the vert group. */
  char defgrp_name[/*MAX_VGROUP_NAME*/ 64];

  char _pad1[4];
  /** Curve info for the custom profile */
  struct CurveProfile *custom_profile;

  /** Custom bevel edge weight name. */
  char edge_weight_name[64];

  /** Custom bevel vertex weight name. */
  char vertex_weight_name[64];
} BevelModifierData;

/** #BevelModifierData.flags and BevelModifierData.lim_flags */
typedef enum {
#ifdef DNA_DEPRECATED_ALLOW
  MOD_BEVEL_VERT_DEPRECATED = (1 << 1),
#endif
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
} BevelModifierFlag;

/** #BevelModifierData.val_flags (not used as flags any more) */
typedef enum {
  MOD_BEVEL_AMT_OFFSET = 0,
  MOD_BEVEL_AMT_WIDTH = 1,
  MOD_BEVEL_AMT_DEPTH = 2,
  MOD_BEVEL_AMT_PERCENT = 3,
  MOD_BEVEL_AMT_ABSOLUTE = 4,
} BevelModifierValFlag;

/** #BevelModifierData.profile_type */
typedef enum {
  MOD_BEVEL_PROFILE_SUPERELLIPSE = 0,
  MOD_BEVEL_PROFILE_CUSTOM = 1,
} BevelModifierProfileType;

/** #BevelModifierData.edge_flags */
typedef enum {
  MOD_BEVEL_MARK_SEAM = (1 << 0),
  MOD_BEVEL_MARK_SHARP = (1 << 1),
} BevelModifierEdgeFlag;

/** #BevelModifierData.face_str_mode */
typedef enum {
  MOD_BEVEL_FACE_STRENGTH_NONE = 0,
  MOD_BEVEL_FACE_STRENGTH_NEW = 1,
  MOD_BEVEL_FACE_STRENGTH_AFFECTED = 2,
  MOD_BEVEL_FACE_STRENGTH_ALL = 3,
} BevelModifierFaceStrengthMode;

/** #BevelModifier.miter_inner & #BevelModifier.miter_outer */
typedef enum {
  MOD_BEVEL_MITER_SHARP = 0,
  MOD_BEVEL_MITER_PATCH = 1,
  MOD_BEVEL_MITER_ARC = 2,
} BevelModifierMiter;

/** #BevelModifier.vmesh_method */
typedef enum {
  MOD_BEVEL_VMESH_ADJ = 0,
  MOD_BEVEL_VMESH_CUTOFF = 1,
} BevelModifierVMeshMethod;

/** #BevelModifier.affect_type */
typedef enum {
  MOD_BEVEL_AFFECT_VERTICES = 0,
  MOD_BEVEL_AFFECT_EDGES = 1,
} BevelModifierAffectType;

typedef struct FluidModifierData {
  ModifierData modifier;

  struct FluidDomainSettings *domain;
  /** Inflow, outflow, smoke objects. */
  struct FluidFlowSettings *flow;
  /** Effector objects (collision, guiding). */
  struct FluidEffectorSettings *effector;
  float time;
  /** #FluidModifierType. Domain, inflow, outflow, .... */
  int type;
  void *_pad1;
} FluidModifierData;

/** #FluidModifierData.type */
typedef enum {
  MOD_FLUID_TYPE_DOMAIN = (1 << 0),
  MOD_FLUID_TYPE_FLOW = (1 << 1),
  MOD_FLUID_TYPE_EFFEC = (1 << 2),
} FluidModifierType;

typedef struct DisplaceModifierData {
  ModifierData modifier;

  /* Keep in sync with #MappingInfoModifierData. */

  struct Tex *texture;
  struct Object *map_object;
  char map_bone[/*MAXBONENAME*/ 64];
  char uvlayer_name[/*MAX_CUSTOMDATA_LAYER_NAME*/ 68];
  char _pad1[4];
  int uvlayer_tmp;
  /** #DisplaceModifierTexMapping. */
  int texmapping;
  /* end MappingInfoModifierData */

  float strength;
  /** #DisplaceModifierDirection. */
  int direction;
  char defgrp_name[/*MAX_VGROUP_NAME*/ 64];
  float midlevel;
  /** #DisplaceModifierSpace. */
  int space;
  /** #DisplaceModifierFlag. */
  short flag;
  char _pad2[6];
} DisplaceModifierData;

/** #DisplaceModifierData.flag */
typedef enum {
  MOD_DISP_INVERT_VGROUP = (1 << 0),
} DisplaceModifierFlag;

/** #DisplaceModifierData.direction */
typedef enum {
  MOD_DISP_DIR_X = 0,
  MOD_DISP_DIR_Y = 1,
  MOD_DISP_DIR_Z = 2,
  MOD_DISP_DIR_NOR = 3,
  MOD_DISP_DIR_RGB_XYZ = 4,
  MOD_DISP_DIR_CLNOR = 5,
} DisplaceModifierDirection;

/** #DisplaceModifierData.texmapping */
typedef enum {
  MOD_DISP_MAP_LOCAL = 0,
  MOD_DISP_MAP_GLOBAL = 1,
  MOD_DISP_MAP_OBJECT = 2,
  MOD_DISP_MAP_UV = 3,
} DisplaceModifierTexMapping;

/** #DisplaceModifierData.space */
typedef enum {
  MOD_DISP_SPACE_LOCAL = 0,
  MOD_DISP_SPACE_GLOBAL = 1,
} DisplaceModifierSpace;

typedef struct UVProjectModifierData {
  ModifierData modifier;
  /**
   * The objects which do the projecting.
   */
  struct Object *projectors[/*MOD_UVPROJECT_MAXPROJECTORS*/ 10];
  char _pad2[4];
  int projectors_num;
  float aspectx, aspecty;
  float scalex, scaley;
  char uvlayer_name[/*MAX_CUSTOMDATA_LAYER_NAME*/ 68];
  int uvlayer_tmp;
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

  char defgrp_name[/*MAX_VGROUP_NAME*/ 64];
  float defgrp_factor;
  /** #DecimateModifierFlag. */
  short flag;
  /** #DecimateModifierMode. */
  short mode;

  /** runtime only. */
  int face_count;
} DecimateModifierData;

typedef enum {
  MOD_DECIM_FLAG_INVERT_VGROUP = (1 << 0),
  /** For collapse only. don't convert triangle pairs back to quads. */
  MOD_DECIM_FLAG_TRIANGULATE = (1 << 1),
  /** for dissolve only. collapse all verts between 2 faces */
  MOD_DECIM_FLAG_ALL_BOUNDARY_VERTS = (1 << 2),
  MOD_DECIM_FLAG_SYMMETRY = (1 << 3),
} DecimateModifierFlag;

typedef enum {
  MOD_DECIM_MODE_COLLAPSE = 0,
  MOD_DECIM_MODE_UNSUBDIV = 1,
  /** called planar in the UI */
  MOD_DECIM_MODE_DISSOLVE = 2,
} DecimateModifierMode;

typedef struct SmoothModifierData {
  ModifierData modifier;
  float fac;
  char defgrp_name[/*MAX_VGROUP_NAME*/ 64];
  /** #SmoothModifierFlag. */
  short flag;
  short repeat;

} SmoothModifierData;

/** #SmoothModifierData.flag */
typedef enum {
  MOD_SMOOTH_INVERT_VGROUP = (1 << 0),
  MOD_SMOOTH_X = (1 << 1),
  MOD_SMOOTH_Y = (1 << 2),
  MOD_SMOOTH_Z = (1 << 3),
} SmoothModifierFlag;

typedef struct CastModifierData {
  ModifierData modifier;

  struct Object *object;
  float fac;
  float radius;
  float size;
  char defgrp_name[/*MAX_VGROUP_NAME*/ 64];
  /** #CastModifierFlag. */
  short flag;
  /** #CastModifierType. Cast modifier projection type. */
  short type;
  void *_pad1;
} CastModifierData;

/** #CastModifierData.flag */
typedef enum {
  /* And what bout (1 << 0) flag? ;) */
  MOD_CAST_INVERT_VGROUP = (1 << 0),
  MOD_CAST_X = (1 << 1),
  MOD_CAST_Y = (1 << 2),
  MOD_CAST_Z = (1 << 3),
  MOD_CAST_USE_OB_TRANSFORM = (1 << 4),
  MOD_CAST_SIZE_FROM_RADIUS = (1 << 5),
} CastModifierFlag;

/** #CastModifierData.type */
typedef enum {
  MOD_CAST_TYPE_SPHERE = 0,
  MOD_CAST_TYPE_CYLINDER = 1,
  MOD_CAST_TYPE_CUBOID = 2,
} CastModifierType;

typedef struct WaveModifierData {
  ModifierData modifier;

  /* Keep in sync with #MappingInfoModifierData. */

  struct Tex *texture;
  struct Object *map_object;
  char map_bone[/*MAXBONENAME*/ 64];
  char uvlayer_name[/*MAX_CUSTOMDATA_LAYER_NAME*/ 68];
  char _pad1[4];
  int uvlayer_tmp;
  int texmapping;
  /* End MappingInfoModifierData. */

  struct Object *objectcenter;
  char defgrp_name[/*MAX_VGROUP_NAME*/ 64];

  /** #WaveModifierFlag. */
  short flag;
  char _pad2[2];

  float startx, starty, height, width;
  float narrow, speed, damp, falloff;

  float timeoffs, lifetime;
  char _pad3[4];
  void *_pad4;
} WaveModifierData;

/** #WaveModifierData.flag */
typedef enum {
  MOD_WAVE_INVERT_VGROUP = (1 << 0),
  MOD_WAVE_X = (1 << 1),
  MOD_WAVE_Y = (1 << 2),
  MOD_WAVE_CYCL = (1 << 3),
  MOD_WAVE_NORM = (1 << 4),
  MOD_WAVE_NORM_X = (1 << 5),
  MOD_WAVE_NORM_Y = (1 << 6),
  MOD_WAVE_NORM_Z = (1 << 7),
} WaveModifierFlag;

typedef struct ArmatureModifierData {
  ModifierData modifier;

  /** #eArmature_DeformFlag use instead of #bArmature.deformflag. */
  short deformflag, multi;
  char _pad2[4];
  struct Object *object;
  /** Stored input of previous modifier, for vertex-group blending. */
  float (*vert_coords_prev)[3];
  char defgrp_name[/*MAX_VGROUP_NAME*/ 64];
} ArmatureModifierData;

typedef enum {
  MOD_HOOK_UNIFORM_SPACE = (1 << 0),
  MOD_HOOK_INVERT_VGROUP = (1 << 1),
} HookModifierFlag;

/** \note same as #WarpModifierFalloff */
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
  /** Optional name of bone target. */
  char subtarget[/*MAX_NAME*/ 64];

  /** #HookModifierFlag. */
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

  /** If NULL, it's using vertex-group. */
  int *indexar;
  int indexar_num;
  float force;
  /** Optional vertex-group name. */
  char name[/*MAX_VGROUP_NAME*/ 64];
  void *_pad1;
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

  /**
   * PointCache can be shared with other instances of #ClothModifierData.
   * Inspect `modifier.flag & eModifierFlag_SharedCaches` to find out.
   */
  /** Definition is in DNA_object_force_types.h. */
  struct PointCache *point_cache;
  struct ListBase ptcaches;

  /** XXX: nasty hack, remove once hair can be separated from cloth modifier data. */
  struct ClothHairData *hairdata;
  /** Grid geometry values of hair continuum. */
  float hair_grid_min[3];
  float hair_grid_max[3];
  int hair_grid_res[3];
  float hair_grid_cellsize;

  struct ClothSolverResult *solver_result;
} ClothModifierData;

typedef struct CollisionModifierData {
  ModifierData modifier;

  /** Position at the beginning of the frame. */
  float (*x)[3];
  /** Position at the end of the frame. */
  float (*xnew)[3];
  /** Unused at the moment, but was discussed during sprint. */
  float (*xold)[3];
  /** New position at the actual inter-frame step. */
  float (*current_xnew)[3];
  /** Position at the actual inter-frame step. */
  float (*current_x)[3];
  /** (xnew - x) at the actual inter-frame step. */
  float (*current_v)[3];

  int (*vert_tris)[3];

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

typedef struct SurfaceModifierData_Runtime {

  float (*vert_positions_prev)[3];
  float (*vert_velocities)[3];

  struct Mesh *mesh;

  /** Bounding volume hierarchy of the mesh faces. */
  BVHTreeFromMeshHandle *bvhtree;

  int cfra_prev, verts_num;

} SurfaceModifierData_Runtime;

typedef struct SurfaceModifierData {
  ModifierData modifier;

  SurfaceModifierData_Runtime runtime;
} SurfaceModifierData;

typedef struct BooleanModifierData {
  ModifierData modifier;

  struct Object *object;
  struct Collection *collection;
  float double_threshold;
  /** #BooleanModifierOp. */
  char operation;
  /** #BooleanModifierSolver. */
  char solver;
  /** #BooleanModifierMaterialMode. */
  char material_mode;
  /** #BooleanModifierFlag. */
  char flag;
  /** #BooleanModifierBMeshFlag. */
  char bm_flag;
  char _pad[7];
} BooleanModifierData;

typedef enum BooleanModifierMaterialMode {
  eBooleanModifierMaterialMode_Index = 0,
  eBooleanModifierMaterialMode_Transfer = 1,
} BooleanModifierMaterialMode;

/** #BooleanModifierData.operation */
typedef enum {
  eBooleanModifierOp_Intersect = 0,
  eBooleanModifierOp_Union = 1,
  eBooleanModifierOp_Difference = 2,
} BooleanModifierOp;

/** #BooleanModifierData.solver */
typedef enum {
  eBooleanModifierSolver_Float = 0,
  eBooleanModifierSolver_Mesh_Arr = 1,
  eBooleanModifierSolver_Manifold = 2,
} BooleanModifierSolver;

/** #BooleanModifierData.flag */
typedef enum {
  eBooleanModifierFlag_Self = (1 << 0),
  eBooleanModifierFlag_Object = (1 << 1),
  eBooleanModifierFlag_Collection = (1 << 2),
  eBooleanModifierFlag_HoleTolerant = (1 << 3),
} BooleanModifierFlag;

/** #BooleanModifierData.bm_flag (only used when #G_DEBUG is set). */
typedef enum {
  eBooleanModifierBMeshFlag_BMesh_Separate = (1 << 0),
  eBooleanModifierBMeshFlag_BMesh_NoDissolve = (1 << 1),
  eBooleanModifierBMeshFlag_BMesh_NoConnectRegions = (1 << 2),
} BooleanModifierBMeshFlag;

typedef struct MDefInfluence {
  int vertex;
  float weight;
} MDefInfluence;

typedef struct MDefCell {
  int offset;
  int influences_num;
} MDefCell;

typedef struct MeshDeformModifierData {
  ModifierData modifier;

  /** Mesh object. */
  struct Object *object;
  /** Optional vertex-group name. */
  char defgrp_name[/*MAX_VGROUP_NAME*/ 64];

  short gridsize;
  /** #MeshDeformModifierFlag. */
  short flag;
  char _pad[4];

  /* result of static binding */
  /** Influences. */
  MDefInfluence *bindinfluences;
  const ImplicitSharingInfoHandle *bindinfluences_sharing_info;
  /** Offsets into influences array. */
  int *bindoffsets;
  const ImplicitSharingInfoHandle *bindoffsets_sharing_info;
  /** Coordinates that cage was bound with. */
  float *bindcagecos;
  const ImplicitSharingInfoHandle *bindcagecos_sharing_info;
  /** Total vertices in mesh and cage. */
  int verts_num, cage_verts_num;

  /* result of dynamic binding */
  /** Grid with dynamic binding cell points. */
  MDefCell *dyngrid;
  const ImplicitSharingInfoHandle *dyngrid_sharing_info;
  /** Dynamic binding vertex influences. */
  MDefInfluence *dyninfluences;
  const ImplicitSharingInfoHandle *dyninfluences_sharing_info;
  /** Is this vertex bound or not? */
  int *dynverts;
  const ImplicitSharingInfoHandle *dynverts_sharing_info;
  /** Size of the dynamic bind grid. */
  int dyngridsize;
  /** Total number of vertex influences. */
  int influences_num;
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
  void (*bindfunc)(struct Object *object,
                   struct MeshDeformModifierData *mmd,
                   struct Mesh *cagemesh,
                   float *vertexcos,
                   int verts_num,
                   float cagemat[4][4]);
} MeshDeformModifierData;

typedef enum {
  MOD_MDEF_INVERT_VGROUP = (1 << 0),
  MOD_MDEF_DYNAMIC_BIND = (1 << 1),
} MeshDeformModifierFlag;

typedef struct ParticleSystemModifierData {
  ModifierData modifier;

  /**
   * \note Storing the particle system pointer here is very weak, as it prevents modifiers' data
   * copying to be self-sufficient (extra external code needs to ensure the pointer remains valid
   * when the modifier data is copied from one object to another). See e.g.
   * `BKE_object_copy_particlesystems` or `BKE_object_copy_modifier`.
   */
  struct ParticleSystem *psys;
  /** Final Mesh - its topology may differ from orig mesh. */
  struct Mesh *mesh_final;
  /** Original mesh that particles are attached to. */
  struct Mesh *mesh_original;
  int totdmvert, totdmedge, totdmface;
  /** #ParticleSystemModifierFlag. */
  short flag;
  char _pad[2];
  void *_pad1;
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
  short psys;
  /** #ParticleInstanceModifierFlag. */
  short flag;
  short axis;
  /** #ParticleInstanceModifierSpace. */
  short space;
  float position, random_position;
  float rotation, random_rotation;
  float particle_amount, particle_offset;
  char index_layer_name[/*MAX_CUSTOMDATA_LAYER_NAME*/ 68];
  char value_layer_name[/*MAX_CUSTOMDATA_LAYER_NAME*/ 68];
  void *_pad1;
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
  /** #ExplodeModifierFlag. */
  short flag;
  short vgroup;
  float protect;
  char uvname[/*MAX_CUSTOMDATA_LAYER_NAME*/ 68];
  char _pad1[4];
  void *_pad2;
} ExplodeModifierData;

typedef struct MultiresModifierData {
  DNA_DEFINE_CXX_METHODS(MultiresModifierData)

  ModifierData modifier;

  char lvl, sculptlvl, renderlvl, totlvl;
  char simple DNA_DEPRECATED;
  /** #MultiresModifierFlag. */
  char flags;
  char _pad[2];
  short quality;
  short uv_smooth;
  short boundary_smooth;
  char _pad2[2];
} MultiresModifierData;

typedef enum {
  eMultiresModifierFlag_ControlEdges = (1 << 0),
  /* DEPRECATED, only used for versioning. */
  eMultiresModifierFlag_PlainUv_DEPRECATED = (1 << 1),
  eMultiresModifierFlag_UseCrease = (1 << 2),
  eMultiresModifierFlag_UseCustomNormals = (1 << 3),
  eMultiresModifierFlag_UseSculptBaseMesh = (1 << 4),
} MultiresModifierFlag;

/** DEPRECATED: only used for versioning. */
typedef struct FluidsimModifierData {
  ModifierData modifier;

  /** Definition is in DNA_object_fluidsim_types.h. */
  struct FluidsimSettings *fss;
  void *_pad1;
} FluidsimModifierData;

/** DEPRECATED: only used for versioning. */
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
  /** Optional vertex-group name. */
  char vgroup_name[/*MAX_VGROUP_NAME*/ 64];
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

  /**
   * If using projection over vertex normal this controls the level of subsurface that must be
   * done before getting the vertex coordinates and normal.
   */
  char subsurfLevels;

  char _pad[2];
} ShrinkwrapModifierData;

typedef struct SimpleDeformModifierData {
  ModifierData modifier;

  /** Object to control the origin of modifier space coordinates. */
  struct Object *origin;
  /** Optional vertex-group name. */
  char vgroup_name[/*MAX_VGROUP_NAME*/ 64];
  /** Factors to control simple deforms. */
  float factor;
  /** Lower and upper limit. */
  float limit[2];

  /** #SimpleDeformModifierMode. Deform function. */
  char mode;
  /** Lock axis (for taper and stretch). */
  char axis;
  /**
   * #SimpleDeformModifierLockAxis.
   * Axis to perform the deform on (default is X, but can be overridden by origin.
   */
  char deform_axis;
  /** #SimpleDeformModifierFlag. */
  char flag;

  void *_pad1;
} SimpleDeformModifierData;

/** #SimpleDeformModifierData.flag */
typedef enum {
  MOD_SIMPLEDEFORM_FLAG_INVERT_VGROUP = (1 << 0),
} SimpleDeformModifierFlag;

typedef enum {
  MOD_SIMPLEDEFORM_MODE_TWIST = 1,
  MOD_SIMPLEDEFORM_MODE_BEND = 2,
  MOD_SIMPLEDEFORM_MODE_TAPER = 3,
  MOD_SIMPLEDEFORM_MODE_STRETCH = 4,
} SimpleDeformModifierMode;

typedef enum {
  MOD_SIMPLEDEFORM_LOCK_AXIS_X = (1 << 0),
  MOD_SIMPLEDEFORM_LOCK_AXIS_Y = (1 << 1),
  MOD_SIMPLEDEFORM_LOCK_AXIS_Z = (1 << 2),
} SimpleDeformModifierLockAxis;

typedef struct ShapeKeyModifierData {
  ModifierData modifier;
} ShapeKeyModifierData;

typedef struct SolidifyModifierData {
  ModifierData modifier;

  /** Name of vertex group to use. */
  char defgrp_name[/*MAX_VGROUP_NAME*/ 64];
  char shell_defgrp_name[64];
  char rim_defgrp_name[64];
  /** New surface offset level. */
  float offset;
  /** Midpoint of the offset. */
  float offset_fac;
  /**
   * Factor for the minimum weight to use when vertex-groups are used,
   * avoids 0.0 weights giving duplicate geometry.
   */
  float offset_fac_vg;
  /** Clamp offset based on surrounding geometry. */
  float offset_clamp;
  /** #SolidifyModifierMode. */
  char mode;

  /** Variables for #MOD_SOLIDIFY_MODE_NONMANIFOLD. */
  /** #SolifyModifierNonManifoldOffsetMode. */
  char nonmanifold_offset_mode;
  /** #SolidifyModifierNonManifoldBoundaryMode. */
  char nonmanifold_boundary_mode;

  char _pad;
  float crease_inner;
  float crease_outer;
  float crease_rim;
  /** #SolidifyModifierFlag. */
  int flag;
  short mat_ofs;
  short mat_ofs_rim;

  float merge_tolerance;
  float bevel_convex;
} SolidifyModifierData;

/** #SolidifyModifierData.flag */
typedef enum {
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
} SolidifyModifierFlag;

/** #SolidifyModifierData.mode */
typedef enum {
  MOD_SOLIDIFY_MODE_EXTRUDE = 0,
  MOD_SOLIDIFY_MODE_NONMANIFOLD = 1,
} SolidifyModifierMode;

/** #SolidifyModifierData.nonmanifold_offset_mode */
typedef enum {
  MOD_SOLIDIFY_NONMANIFOLD_OFFSET_MODE_FIXED = 0,
  MOD_SOLIDIFY_NONMANIFOLD_OFFSET_MODE_EVEN = 1,
  MOD_SOLIDIFY_NONMANIFOLD_OFFSET_MODE_CONSTRAINTS = 2,
} SolifyModifierNonManifoldOffsetMode;

/** #SolidifyModifierData.nonmanifold_boundary_mode */
typedef enum {
  MOD_SOLIDIFY_NONMANIFOLD_BOUNDARY_MODE_NONE = 0,
  MOD_SOLIDIFY_NONMANIFOLD_BOUNDARY_MODE_ROUND = 1,
  MOD_SOLIDIFY_NONMANIFOLD_BOUNDARY_MODE_FLAT = 2,
} SolidifyModifierNonManifoldBoundaryMode;

typedef struct ScrewModifierData {
  ModifierData modifier;

  struct Object *ob_axis;
  unsigned int steps;
  unsigned int render_steps;
  unsigned int iter;
  float screw_ofs;
  float angle;
  float merge_dist;
  /** #ScrewModifierFlag. */
  short flag;
  char axis;
  char _pad[5];
  void *_pad1;
} ScrewModifierData;

typedef enum {
  MOD_SCREW_NORMAL_FLIP = (1 << 0),
  MOD_SCREW_NORMAL_CALC = (1 << 1),
  MOD_SCREW_OBJECT_OFFSET = (1 << 2),
  // MOD_SCREW_OBJECT_ANGLE = (1 << 4),
  MOD_SCREW_SMOOTH_SHADING = (1 << 5),
  MOD_SCREW_UV_STRETCH_U = (1 << 6),
  MOD_SCREW_UV_STRETCH_V = (1 << 7),
  MOD_SCREW_MERGE = (1 << 8),
} ScrewModifierFlag;

typedef struct OceanModifierData {
  ModifierData modifier;

  struct Ocean *ocean;
  struct OceanCache *oceancache;

  /** Render resolution. */
  int resolution;
  /** Viewport resolution for the non-render case. */
  int viewport_resolution;

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

  /** #OceanModifierSpectrum. Spectrum being used. */
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

  char cachepath[/*FILE_MAX*/ 1024];

  char foamlayername[/*MAX_CUSTOMDATA_LAYER_NAME*/ 68];
  char spraylayername[/*MAX_CUSTOMDATA_LAYER_NAME*/ 68];
  char cached;
  /** #OceanModifierGeometryMode. */
  char geometry_mode;

  /** #OceanModifierFlag. */
  char flag;
  char _pad2;

  short repeat_x;
  short repeat_y;

  int seed;

  float size;

  float foam_fade;

  char _pad[4];
} OceanModifierData;

typedef enum {
  MOD_OCEAN_GEOM_GENERATE = 0,
  MOD_OCEAN_GEOM_DISPLACE = 1,
  MOD_OCEAN_GEOM_SIM_ONLY = 2,
} OceanModifierGeometryMode;

typedef enum {
  MOD_OCEAN_SPECTRUM_PHILLIPS = 0,
  MOD_OCEAN_SPECTRUM_PIERSON_MOSKOWITZ = 1,
  MOD_OCEAN_SPECTRUM_JONSWAP = 2,
  MOD_OCEAN_SPECTRUM_TEXEL_MARSEN_ARSLOE = 3,
} OceanModifierSpectrum;

typedef enum {
  MOD_OCEAN_GENERATE_FOAM = (1 << 0),
  MOD_OCEAN_GENERATE_NORMALS = (1 << 1),
  MOD_OCEAN_GENERATE_SPRAY = (1 << 2),
  MOD_OCEAN_INVERT_SPRAY = (1 << 3),
} OceanModifierFlag;

typedef struct WarpModifierData {
  ModifierData modifier;

  /* Keep in sync with #MappingInfoModifierData. */

  struct Tex *texture;
  struct Object *map_object;
  char map_bone[/*MAXBONENAME*/ 64];
  char uvlayer_name[/*MAX_CUSTOMDATA_LAYER_NAME*/ 68];
  char _pad1[4];
  int uvlayer_tmp;
  int texmapping;
  /* End #MappingInfoModifierData. */

  struct Object *object_from;
  struct Object *object_to;
  /** Optional name of bone target. */
  char bone_from[/*MAX_NAME*/ 64];
  /** Optional name of bone target. */
  char bone_to[/*MAX_NAME*/ 64];

  struct CurveMapping *curfalloff;
  /** Optional vertex-group name. */
  char defgrp_name[/*MAX_VGROUP_NAME*/ 64];
  float strength;
  float falloff_radius;
  /** #WarpModifierFlag. */
  char flag;
  char falloff_type;
  char _pad2[6];
  void *_pad3;
} WarpModifierData;

/** #WarpModifierData.flag */
typedef enum {
  MOD_WARP_VOLUME_PRESERVE = (1 << 0),
  MOD_WARP_INVERT_VGROUP = (1 << 1),
} WarpModifierFlag;

/** \note same as #HookModifierFalloff. */
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

  /** Name of vertex group to edit. */
  char defgrp_name[/*MAX_VGROUP_NAME*/ 64];

  /** #WeigthVGEditModifierEditFlags. */
  short edit_flags;
  /** Using MOD_WVG_MAPPING_* defines. */
  short falloff_type;
  /** Weight for vertices not in vgroup. */
  float default_weight;

  /* Mapping stuff. */
  /** The custom mapping curve. */
  struct CurveMapping *cmap_curve;

  /* The add/remove vertices weight thresholds. */
  float add_threshold, rem_threshold;

  /* Masking options. */
  /** The global "influence", if no vgroup nor tex is used as mask. */
  float mask_constant;
  /** Name of mask vertex group from which to get weight factors. */
  char mask_defgrp_name[/*MAX_VGROUP_NAME*/ 64];

  /* Texture masking. */
  /** Which channel to use as weight/mask. */
  int mask_tex_use_channel;
  /** The texture. */
  struct Tex *mask_texture;
  /** Name of the map object. */
  struct Object *mask_tex_map_obj;
  /** Name of the map bone. */
  char mask_tex_map_bone[/*MAXBONENAME*/ 64];
  /** How to map the texture (using MOD_DISP_MAP_* enums). */
  int mask_tex_mapping;
  /** Name of the UV map. */
  char mask_tex_uvlayer_name[/*MAX_CUSTOMDATA_LAYER_NAME*/ 68];

  /* Padding... */
  void *_pad1;
} WeightVGEditModifierData;

/** #WeightVGEdit.edit_flags */
typedef enum {
  MOD_WVG_EDIT_WEIGHTS_NORMALIZE = (1 << 0),
  MOD_WVG_INVERT_FALLOFF = (1 << 1),
  MOD_WVG_EDIT_INVERT_VGROUP_MASK = (1 << 2),
  /** Add vertices with higher weight than threshold to vgroup. */
  MOD_WVG_EDIT_ADD2VG = (1 << 3),
  /** Remove vertices with lower weight than threshold from vgroup. */
  MOD_WVG_EDIT_REMFVG = (1 << 4),
} WeigthVGEditModifierEditFlags;

typedef struct WeightVGMixModifierData {
  ModifierData modifier;

  /** Name of vertex group to modify/weight. */
  char defgrp_name_a[/*MAX_VGROUP_NAME*/ 64];
  /** Name of other vertex group to mix in. */
  char defgrp_name_b[/*MAX_VGROUP_NAME*/ 64];
  /** Default weight value for first vgroup. */
  float default_weight_a;
  /** Default weight value to mix in. */
  float default_weight_b;
  /** #WeightVGMixModifierMixMode. How second vgroups weights affect first ones. */
  char mix_mode;
  /** #WeightVGMixModifierMixSet. What vertices to affect. */
  char mix_set;

  char _pad0[6];

  /* Masking options. */
  /** The global "influence", if no vgroup nor tex is used as mask. */
  float mask_constant;
  /** Name of mask vertex group from which to get weight factors. */
  char mask_defgrp_name[/*MAX_VGROUP_NAME*/ 64];

  /* Texture masking. */
  /** Which channel to use as weightf. */
  int mask_tex_use_channel;
  /** The texture. */
  struct Tex *mask_texture;
  /** Name of the map object. */
  struct Object *mask_tex_map_obj;
  /** Name of the map bone. */
  char mask_tex_map_bone[/*MAXBONENAME*/ 64];
  /** How to map the texture. */
  int mask_tex_mapping;
  /** Name of the UV map. */
  char mask_tex_uvlayer_name[/*MAX_CUSTOMDATA_LAYER_NAME*/ 68];
  char _pad1[4];

  /** #WeightVGMixModifierFlag. */
  char flag;

  /* Padding... */
  char _pad2[3];
} WeightVGMixModifierData;

/** #WeightVGMixModifierData.mix_mode (how second vgroup's weights affect first ones). */
typedef enum {
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
  /** Minimum of both weights. */
  MOD_WVG_MIX_MIN = 8,
  /** Maximum of both weights. */
  MOD_WVG_MIX_MAX = 9,
} WeightVGMixModifierMixMode;

/** #WeightVGMixModifierData.mix_set (what vertices to affect). */
typedef enum {
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
} WeightVGMixModifierMixSet;

/** #WeightVGMixModifierData.flag */
typedef enum {
  MOD_WVG_MIX_INVERT_VGROUP_MASK = (1 << 0),
  MOD_WVG_MIX_WEIGHTS_NORMALIZE = (1 << 1),
  MOD_WVG_MIX_INVERT_VGROUP_A = (1 << 2),
  MOD_WVG_MIX_INVERT_VGROUP_B = (1 << 3),
} WeightVGMixModifierFlag;

typedef struct WeightVGProximityModifierData {
  ModifierData modifier;

  /** Name of vertex group to modify/weight. */
  char defgrp_name[/*MAX_VGROUP_NAME*/ 64];

  /* Mapping stuff. */
  /** The custom mapping curve. */
  struct CurveMapping *cmap_curve;

  /** #WeightVGProximityModifierProximityMode. Modes of proximity weighting. */
  int proximity_mode;
  /** #WeightVGProximityModifierFlag. Options for proximity weighting. */
  int proximity_flags;

  /* Target object from which to calculate vertices distances. */
  struct Object *proximity_ob_target;

  /* Masking options. */
  /** The global "influence", if no vgroup nor tex is used as mask. */
  float mask_constant;
  /** Name of mask vertex group from which to get weight factors. */
  char mask_defgrp_name[/*MAX_VGROUP_NAME*/ 64];

  /* Texture masking. */
  /** #WeightVGProximityModifierMaskTexChannel. Which channel to use as weightf. */
  int mask_tex_use_channel;
  /** The texture. */
  struct Tex *mask_texture;
  /** Name of the map object. */
  struct Object *mask_tex_map_obj;
  /** Name of the map bone. */
  char mask_tex_map_bone[/*MAXBONENAME*/ 64];
  /** How to map the texture. */
  int mask_tex_mapping;
  /** Name of the UV Map. */
  char mask_tex_uvlayer_name[/*MAX_CUSTOMDATA_LAYER_NAME*/ 68];
  char _pad1[4];

  /** Distances mapping to 0.0/1.0 weights. */
  float min_dist, max_dist;

  /* Put here to avoid breaking existing struct... */
  /**
   * Mapping modes (using MOD_WVG_MAPPING_* enums). */
  short falloff_type;

  /* Padding... */
  char _pad0[2];
} WeightVGProximityModifierData;

/** #WeightVGProximityModifierData.proximity_mode */
typedef enum {
  MOD_WVG_PROXIMITY_OBJECT = 1,   /* source vertex to other location */
  MOD_WVG_PROXIMITY_GEOMETRY = 2, /* source vertex to other geometry */
} WeightVGProximityModifierProximityMode;

/** #WeightVGProximityModifierData.proximity_flags */
typedef enum {
  /* Use nearest vertices of target obj, in MOD_WVG_PROXIMITY_GEOMETRY mode. */
  MOD_WVG_PROXIMITY_GEOM_VERTS = (1 << 0),
  /* Use nearest edges of target obj, in MOD_WVG_PROXIMITY_GEOMETRY mode. */
  MOD_WVG_PROXIMITY_GEOM_EDGES = (1 << 1),
  /* Use nearest faces of target obj, in MOD_WVG_PROXIMITY_GEOMETRY mode. */
  MOD_WVG_PROXIMITY_GEOM_FACES = (1 << 2),
  MOD_WVG_PROXIMITY_INVERT_VGROUP_MASK = (1 << 3),
  MOD_WVG_PROXIMITY_INVERT_FALLOFF = (1 << 4),
  MOD_WVG_PROXIMITY_WEIGHTS_NORMALIZE = (1 << 5),
} WeightVGProximityModifierFlag;

/* Defines common to all WeightVG modifiers. */

/** #WeightVGProximityModifierData.falloff_type */
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

/** #WeightVGProximityModifierData.mask_tex_use_channel */
typedef enum {
  MOD_WVG_MASK_TEX_USE_INT = 1,
  MOD_WVG_MASK_TEX_USE_RED = 2,
  MOD_WVG_MASK_TEX_USE_GREEN = 3,
  MOD_WVG_MASK_TEX_USE_BLUE = 4,
  MOD_WVG_MASK_TEX_USE_HUE = 5,
  MOD_WVG_MASK_TEX_USE_SAT = 6,
  MOD_WVG_MASK_TEX_USE_VAL = 7,
  MOD_WVG_MASK_TEX_USE_ALPHA = 8,
} WeightVGProximityModifierMaskTexChannel;

typedef struct DynamicPaintModifierData {
  ModifierData modifier;

  struct DynamicPaintCanvasSettings *canvas;
  struct DynamicPaintBrushSettings *brush;
  /** #DynamicPaintModifierType. UI display: canvas / brush. */
  int type;
  char _pad[4];
} DynamicPaintModifierData;

/** #DynamicPaintModifierData.type */
typedef enum {
  MOD_DYNAMICPAINT_TYPE_CANVAS = (1 << 0),
  MOD_DYNAMICPAINT_TYPE_BRUSH = (1 << 1),
} DynamicPaintModifierType;

/** Remesh modifier. */
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

  /** Flood-fill option, controls how small components can be before they are removed. */
  float threshold;

  /* ratio between size of model and grid */
  float scale;

  float hermite_num;

  /* octree depth */
  char depth;
  /** #RemeshModifierFlags. */
  char flag;
  /** #eRemeshModifierMode. */
  char mode;
  char _pad;

  /* OpenVDB Voxel remesh properties. */
  float voxel_size;
  float adaptivity;
} RemeshModifierData;

/** Skin modifier. */
typedef struct SkinModifierData {
  ModifierData modifier;

  float branch_smoothing;

  /** #SkinModifierFlag. */
  char flag;

  /** #SkinModifierSymmetryAxis. */
  char symmetry_axes;

  char _pad[2];
} SkinModifierData;

/** #SkinModifierData.symmetry_axes */
typedef enum {
  MOD_SKIN_SYMM_X = (1 << 0),
  MOD_SKIN_SYMM_Y = (1 << 1),
  MOD_SKIN_SYMM_Z = (1 << 2),
} SkinModifierSymmetryAxis;

/** #SkinModifierData.flag */
typedef enum {
  MOD_SKIN_SMOOTH_SHADING = 1,
} SkinModifierFlag;

/** Triangulate modifier. */
typedef struct TriangulateModifierData {
  ModifierData modifier;

  /** #TriangulateModifierFlag. */
  int flag;
  /** #TriangulateModifierQuadMethod. */
  int quad_method;
  /** #TriangulateModifierNgonMethod. */
  int ngon_method;
  int min_vertices;
} TriangulateModifierData;

/** #TriangulateModifierData.flag */
typedef enum {
#ifdef DNA_DEPRECATED_ALLOW
  MOD_TRIANGULATE_BEAUTY = (1 << 0), /* deprecated */
#endif
  MOD_TRIANGULATE_KEEP_CUSTOMLOOP_NORMALS = 1 << 1,
} TriangulateModifierFlag;

/** #TriangulateModifierData.ngon_method triangulate method (N-gons). */
typedef enum {
  MOD_TRIANGULATE_NGON_BEAUTY = 0,
  MOD_TRIANGULATE_NGON_EARCLIP = 1,
} TriangulateModifierNgonMethod;

/** #TriangulateModifierData.quad_method triangulate method (quads). */
typedef enum {
  MOD_TRIANGULATE_QUAD_BEAUTY = 0,
  MOD_TRIANGULATE_QUAD_FIXED = 1,
  MOD_TRIANGULATE_QUAD_ALTERNATE = 2,
  MOD_TRIANGULATE_QUAD_SHORTEDGE = 3,
  MOD_TRIANGULATE_QUAD_LONGEDGE = 4,
} TriangulateModifierQuadMethod;

typedef struct LaplacianSmoothModifierData {
  ModifierData modifier;

  float lambda, lambda_border;
  char _pad1[4];
  char defgrp_name[/*MAX_VGROUP_NAME*/ 64];
  /** #LaplacianSmoothModifierFlag. */
  short flag;
  short repeat;
} LaplacianSmoothModifierData;

/** #LaplacianSmoothModifierData.flag */
typedef enum {
  MOD_LAPLACIANSMOOTH_X = (1 << 1),
  MOD_LAPLACIANSMOOTH_Y = (1 << 2),
  MOD_LAPLACIANSMOOTH_Z = (1 << 3),
  MOD_LAPLACIANSMOOTH_PRESERVE_VOLUME = (1 << 4),
  MOD_LAPLACIANSMOOTH_NORMALIZED = (1 << 5),
  MOD_LAPLACIANSMOOTH_INVERT_VGROUP = (1 << 6),
} LaplacianSmoothModifierFlag;

typedef struct CorrectiveSmoothDeltaCache {
  /**
   * Delta's between the original positions and the smoothed positions,
   * calculated loop-tangent and which is accumulated into the vertex it uses.
   * (run-time only).
   */
  float (*deltas)[3];
  unsigned int deltas_num;

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
  const ImplicitSharingInfoHandle *bind_coords_sharing_info;

  /* NOTE: -1 is used to bind. */
  unsigned int bind_coords_num;

  float lambda, scale;
  short repeat;
  /** #CorrectiveSmoothModifierFlag. */
  short flag;
  /** #CorrectiveSmoothModifierType. */
  char smooth_type;
  /** #CorrectiveSmoothRestSource. */
  char rest_source;
  char _pad[6];

  char defgrp_name[/*MAX_VGROUP_NAME*/ 64];

  /* runtime-only cache */
  CorrectiveSmoothDeltaCache delta_cache;
} CorrectiveSmoothModifierData;

typedef enum {
  MOD_CORRECTIVESMOOTH_SMOOTH_SIMPLE = 0,
  MOD_CORRECTIVESMOOTH_SMOOTH_LENGTH_WEIGHT = 1,
} CorrectiveSmoothModifierType;

typedef enum {
  MOD_CORRECTIVESMOOTH_RESTSOURCE_ORCO = 0,
  MOD_CORRECTIVESMOOTH_RESTSOURCE_BIND = 1,
} CorrectiveSmoothRestSource;

/** #CorrectiveSmoothModifierData.flag */
typedef enum {
  MOD_CORRECTIVESMOOTH_INVERT_VGROUP = (1 << 0),
  MOD_CORRECTIVESMOOTH_ONLY_SMOOTH = (1 << 1),
  MOD_CORRECTIVESMOOTH_PIN_BOUNDARY = (1 << 2),
} CorrectiveSmoothModifierFlag;

typedef struct UVWarpModifierData {
  ModifierData modifier;

  char axis_u, axis_v;
  /** #UVWarpModifierFlag. */
  short flag;
  /** Used for rotate/scale. */
  float center[2];

  float offset[2];
  float scale[2];
  float rotation;

  /** Source. */
  struct Object *object_src;
  /** Optional name of bone target. */
  char bone_src[/*MAX_NAME*/ 64];
  /** Target. */
  struct Object *object_dst;
  /** Optional name of bone target. */
  char bone_dst[/*MAX_NAME*/ 64];

  /** Optional vertex-group name. */
  char vgroup_name[64];
  char uvlayer_name[/*MAX_CUSTOMDATA_LAYER_NAME*/ 68];
  char _pad[4];
} UVWarpModifierData;

/** #UVWarpModifierData.flag */
typedef enum {
  MOD_UVWARP_INVERT_VGROUP = 1 << 0,
} UVWarpModifierFlag;

/** Mesh cache modifier. */
typedef struct MeshCacheModifierData {
  ModifierData modifier;

  /** #MeshCacheModifierFlag. */
  char flag;
  /** #MeshCacheModifierType. File format. */
  char type;
  /** #MeshCacheModifierTimeMode. */
  char time_mode;
  /** #MeshCacheModifierPlayMode. */
  char play_mode;

  /* axis conversion */
  char forward_axis;
  char up_axis;
  /** #MeshCacheModifierFlipAxis. */
  char flip_axis;

  /** #MeshCacheModifierInterpolation. */
  char interp;

  float factor;
  /** #MeshCacheModifierDeformMode. */
  char deform_mode;
  char defgrp_name[64];
  char _pad[7];

  /* play_mode == MOD_MESHCACHE_PLAY_CFEA */
  float frame_start;
  float frame_scale;

  /* play_mode == MOD_MESHCACHE_PLAY_EVAL */
  /* we could use one float for all these but their purpose is very different */
  float eval_frame;
  float eval_time;
  float eval_factor;

  char filepath[/*FILE_MAX*/ 1024];
} MeshCacheModifierData;

/** #MeshCacheModifierData.flag */
typedef enum {
  MOD_MESHCACHE_INVERT_VERTEX_GROUP = 1 << 0,
} MeshCacheModifierFlag;

typedef enum {
  MOD_MESHCACHE_TYPE_MDD = 1,
  MOD_MESHCACHE_TYPE_PC2 = 2,
} MeshCacheModifierType;

typedef enum {
  MOD_MESHCACHE_DEFORM_OVERWRITE = 0,
  MOD_MESHCACHE_DEFORM_INTEGRATE = 1,
} MeshCacheModifierDeformMode;

typedef enum {
  MOD_MESHCACHE_INTERP_NONE = 0,
  MOD_MESHCACHE_INTERP_LINEAR = 1,
  // MOD_MESHCACHE_INTERP_CARDINAL = 2,
} MeshCacheModifierInterpolation;

typedef enum {
  MOD_MESHCACHE_TIME_FRAME = 0,
  MOD_MESHCACHE_TIME_SECONDS = 1,
  MOD_MESHCACHE_TIME_FACTOR = 2,
} MeshCacheModifierTimeMode;

typedef enum {
  MOD_MESHCACHE_PLAY_CFEA = 0,
  MOD_MESHCACHE_PLAY_EVAL = 1,
} MeshCacheModifierPlayMode;

typedef enum {
  MOD_MESHCACHE_FLIP_AXIS_X = 1 << 0,
  MOD_MESHCACHE_FLIP_AXIS_Y = 1 << 1,
  MOD_MESHCACHE_FLIP_AXIS_Z = 1 << 2,
} MeshCacheModifierFlipAxis;

typedef struct LaplacianDeformModifierData {
  ModifierData modifier;
  char anchor_grp_name[/*MAX_VGROUP_NAME*/ 64];
  int verts_num, repeat;
  float *vertexco;
  const ImplicitSharingInfoHandle *vertexco_sharing_info;
  /** Runtime only. */
  void *cache_system;
  /** #LaplacianDeformModifierFlag. */
  short flag;
  char _pad[6];

} LaplacianDeformModifierData;

/** #LaplacianDeformModifierData.flag */
typedef enum {
  MOD_LAPLACIANDEFORM_BIND = 1 << 0,
  MOD_LAPLACIANDEFORM_INVERT_VGROUP = 1 << 1,
} LaplacianDeformModifierFlag;

/**
 * \note many of these options match 'solidify'.
 */
typedef struct WireframeModifierData {
  ModifierData modifier;
  char defgrp_name[/*MAX_VGROUP_NAME*/ 64];
  float offset;
  float offset_fac;
  float offset_fac_vg;
  float crease_weight;
  /** #WireframeModifierFlag. */
  short flag;
  short mat_ofs;
  char _pad[4];
} WireframeModifierData;

typedef enum {
  MOD_WIREFRAME_INVERT_VGROUP = (1 << 0),
  MOD_WIREFRAME_REPLACE = (1 << 1),
  MOD_WIREFRAME_BOUNDARY = (1 << 2),
  MOD_WIREFRAME_OFS_EVEN = (1 << 3),
  MOD_WIREFRAME_OFS_RELATIVE = (1 << 4),
  MOD_WIREFRAME_CREASE = (1 << 5),
} WireframeModifierFlag;

typedef struct WeldModifierData {
  ModifierData modifier;

  /* The limit below which to merge vertices. */
  float merge_dist;
  /** Name of vertex group to use to mask. */
  char defgrp_name[/*MAX_VGROUP_NAME*/ 64];

  /** #WeldModifierMode. */
  char mode;
  /** #WeldModifierFlag. */
  char flag;
  char _pad[2];
} WeldModifierData;

/** #WeldModifierData.flag */
typedef enum {
  MOD_WELD_INVERT_VGROUP = (1 << 0),
  MOD_WELD_LOOSE_EDGES = (1 << 1),
} WeldModifierFlag;

/** #WeldModifierData.mode */
typedef enum {
  MOD_WELD_MODE_ALL = 0,
  MOD_WELD_MODE_CONNECTED = 1,
} WeldModifierMode;

typedef struct DataTransferModifierData {
  ModifierData modifier;

  struct Object *ob_source;

  /** See DT_TYPE_ enum in ED_object.hh. */
  int data_types;

  /* See MREMAP_MODE_ enum in BKE_mesh_mapping.hh */
  int vmap_mode;
  int emap_mode;
  int lmap_mode;
  int pmap_mode;

  float map_max_distance;
  float map_ray_radius;
  float islands_precision;

  char _pad1[4];

  /** See DT_FROMLAYERS_ enum in ED_object.hh. */
  int layers_select_src[/*DT_MULTILAYER_INDEX_MAX*/ 5];
  /** See DT_TOLAYERS_ enum in ED_object.hh. */
  int layers_select_dst[/*DT_MULTILAYER_INDEX_MAX*/ 5];

  /** See CDT_MIX_ enum in BKE_customdata.hh. */
  int mix_mode;
  float mix_factor;
  char defgrp_name[/*MAX_VGROUP_NAME*/ 64];

  /** #DataTransferModifierFlag. */
  int flags;
  void *_pad2;
} DataTransferModifierData;

/** #DataTransferModifierData.flags */
typedef enum {
  MOD_DATATRANSFER_OBSRC_TRANSFORM = 1 << 0,
  MOD_DATATRANSFER_MAP_MAXDIST = 1 << 1,
  MOD_DATATRANSFER_INVERT_VGROUP = 1 << 2,

  /* Only for UI really. */
  MOD_DATATRANSFER_USE_VERT = 1 << 28,
  MOD_DATATRANSFER_USE_EDGE = 1 << 29,
  MOD_DATATRANSFER_USE_LOOP = 1 << 30,
  MOD_DATATRANSFER_USE_POLY = 1u << 31,
} DataTransferModifierFlag;

/** Normal Edit modifier. */
typedef struct NormalEditModifierData {
  ModifierData modifier;
  char defgrp_name[/*MAX_VGROUP_NAME*/ 64];
  /** Source of normals, or center of ellipsoid. */
  struct Object *target;
  /** #NormalEditModifierMode. */
  short mode;
  /** #NormalEditModifierFlag. */
  short flag;
  /** #NormalEditModifierMixMode. */
  short mix_mode;
  char _pad[2];
  float mix_factor;
  float mix_limit;
  float offset[3];
  char _pad0[4];
  void *_pad1;
} NormalEditModifierData;

/** #NormalEditModifierData.mode */
typedef enum {
  MOD_NORMALEDIT_MODE_RADIAL = 0,
  MOD_NORMALEDIT_MODE_DIRECTIONAL = 1,
} NormalEditModifierMode;

/** #NormalEditModifierData.flag */
typedef enum {
  MOD_NORMALEDIT_INVERT_VGROUP = (1 << 0),
  MOD_NORMALEDIT_USE_DIRECTION_PARALLEL = (1 << 1),
  MOD_NORMALEDIT_NO_POLYNORS_FIX = (1 << 2),
} NormalEditModifierFlag;

/** #NormalEditModifierData.mix_mode */
typedef enum {
  MOD_NORMALEDIT_MIX_COPY = 0,
  MOD_NORMALEDIT_MIX_ADD = 1,
  MOD_NORMALEDIT_MIX_SUB = 2,
  MOD_NORMALEDIT_MIX_MUL = 3,
} NormalEditModifierMixMode;

typedef struct MeshSeqCacheModifierData {
  ModifierData modifier;

  struct CacheFile *cache_file;
  char object_path[/*FILE_MAX*/ 1024];

  /** #MeshSeqCacheModifierReadFlag. */
  char read_flag;
  char _pad[3];

  float velocity_scale;

  /* Runtime. */
  struct CacheReader *reader;
  char reader_object_path[/*FILE_MAX*/ 1024];
} MeshSeqCacheModifierData;

/** #MeshSeqCacheModifierData.read_flag */
typedef enum {
  MOD_MESHSEQ_READ_VERT = (1 << 0),
  MOD_MESHSEQ_READ_POLY = (1 << 1),
  MOD_MESHSEQ_READ_UV = (1 << 2),
  MOD_MESHSEQ_READ_COLOR = (1 << 3),

  /* Allow interpolation of mesh vertex positions. There is a heuristic to avoid interpolation when
   * the mesh topology changes, but this heuristic sometimes fails. In these cases, users can
   * disable interpolation with this flag. */
  MOD_MESHSEQ_INTERPOLATE_VERTICES = (1 << 4),

  /* Read animated custom attributes from point cache files. */
  MOD_MESHSEQ_READ_ATTRIBUTES = (1 << 5),
} MeshSeqCacheModifierReadFlag;

typedef struct SDefBind {
  unsigned int *vert_inds;
  unsigned int verts_num;
  /** #SurfaceDeformModifierBindMode. */
  int mode;
  float *vert_weights;
  float normal_dist;
  float influence;
} SDefBind;

typedef struct SDefVert {
  SDefBind *binds;
  unsigned int binds_num;
  unsigned int vertex_idx;
} SDefVert;

typedef struct SurfaceDeformModifierData {
  ModifierData modifier;

  struct Depsgraph *depsgraph;
  /** Bind target object. */
  struct Object *target;
  /** Vertex bind data. */
  SDefVert *verts;
  const ImplicitSharingInfoHandle *verts_sharing_info;
  float falloff;
  /* Number of vertices on the deformed mesh upon the bind process. */
  unsigned int mesh_verts_num;
  /* Number of vertices in the `verts` array of this modifier. */
  unsigned int bind_verts_num;
  /* Number of vertices and polygons on the target mesh upon bind process. */
  unsigned int target_verts_num, target_polys_num;
  /** #SurfaceDeformModifierFlag. */
  int flags;
  float mat[4][4];
  float strength;
  char defgrp_name[64];
  int _pad2;
} SurfaceDeformModifierData;

/** Surface Deform modifier flags. */
typedef enum {
  /* This indicates "do bind on next modifier evaluation" as well as "is bound". */
  MOD_SDEF_BIND = (1 << 0),
  MOD_SDEF_INVERT_VGROUP = (1 << 1),
  /* Only store bind data for nonzero vgroup weights at the time of bind. */
  MOD_SDEF_SPARSE_BIND = (1 << 2),
} SurfaceDeformModifierFlag;

/** Surface Deform vertex bind modes. */
typedef enum {
  MOD_SDEF_MODE_CORNER_TRIS = 0,
  MOD_SDEF_MODE_NGONS = 1,
  MOD_SDEF_MODE_CENTROID = 2,
} SurfaceDeformModifierBindMode;

typedef struct WeightedNormalModifierData {
  ModifierData modifier;

  char defgrp_name[/*MAX_VGROUP_NAME*/ 64];
  /** #WeightedNormalModifierMode. */
  char mode;
  /** #WeightedNormalModifierFlag. */
  char flag;
  short weight;
  float thresh;
} WeightedNormalModifierData;

/* Name/id of the generic PROP_INT cdlayer storing face weights. */
#define MOD_WEIGHTEDNORMALS_FACEWEIGHT_CDLAYER_ID "__mod_weightednormals_faceweight"

/** #WeightedNormalModifierData.mode */
typedef enum {
  MOD_WEIGHTEDNORMAL_MODE_FACE = 0,
  MOD_WEIGHTEDNORMAL_MODE_ANGLE = 1,
  MOD_WEIGHTEDNORMAL_MODE_FACE_ANGLE = 2,
} WeightedNormalModifierMode;

/** #WeightedNormalModifierData.flag */
typedef enum {
  MOD_WEIGHTEDNORMAL_KEEP_SHARP = (1 << 0),
  MOD_WEIGHTEDNORMAL_INVERT_VGROUP = (1 << 1),
  MOD_WEIGHTEDNORMAL_FACE_INFLUENCE = (1 << 2),
} WeightedNormalModifierFlag;

#define MOD_MESHSEQ_READ_ALL \
  (MOD_MESHSEQ_READ_VERT | MOD_MESHSEQ_READ_POLY | MOD_MESHSEQ_READ_UV | MOD_MESHSEQ_READ_COLOR | \
   MOD_MESHSEQ_READ_ATTRIBUTES)

typedef struct NodesModifierSettings {
  /* This stores data that is passed into the node group. */
  struct IDProperty *properties;
} NodesModifierSettings;

/**
 * Maps a name (+ optional library name) to a data-block. The name can be stored on disk and is
 * remapped to the data-block when the data is loaded.
 *
 * At run-time, #BakeDataBlockID is used to pair up the data-block and library name.
 */
typedef struct NodesModifierDataBlock {
  /**
   * Name of the data-block. Can be empty in which case the name of the `id` below is used.
   * This only needs to be set manually when the name stored on disk does not exist in the .blend
   * file anymore, because e.g. the ID has been renamed.
   */
  char *id_name;
  /**
   * Name of the library the ID is in. Can be empty when the ID is not linked or when `id_name` is
   * empty as well and thus the names from the `id` below are used.
   */
  char *lib_name;
  /** ID that this is mapped to. */
  struct ID *id;
  /** Type of ID that is referenced by this mapping. */
  int id_type;
  char _pad[4];
} NodesModifierDataBlock;

typedef struct NodesModifierBakeFile {
  const char *name;
  /* May be null if the file is empty. */
  PackedFile *packed_file;

#ifdef __cplusplus
  blender::Span<std::byte> data() const
  {
    if (this->packed_file) {
      return blender::Span{static_cast<const std::byte *>(this->packed_file->data),
                           this->packed_file->size};
    }
    return {};
  }
#endif
} NodesModifierBakeFile;

/**
 * A packed bake. The format is the same as if the bake was stored on disk.
 */
typedef struct NodesModifierPackedBake {
  int meta_files_num;
  int blob_files_num;
  NodesModifierBakeFile *meta_files;
  NodesModifierBakeFile *blob_files;
} NodesModifierPackedBake;

typedef struct NodesModifierBake {
  /** An id that references a nested node in the node tree. Also see #bNestedNodeRef. */
  int id;
  /** #NodesModifierBakeFlag. */
  uint32_t flag;
  /** #NodesModifierBakeMode. */
  uint8_t bake_mode;
  /** #NodesModifierBakeTarget. */
  int8_t bake_target;
  char _pad[6];
  /**
   * Directory where the baked data should be stored. This is only used when
   * `NODES_MODIFIER_BAKE_CUSTOM_PATH` is set.
   */
  char *directory;
  /**
   * Frame range for the simulation and baking that is used if
   * `NODES_MODIFIER_BAKE_CUSTOM_SIMULATION_FRAME_RANGE` is set.
   */
  int frame_start;
  int frame_end;

  /**
   * Maps data-block names to actual data-blocks, so that names stored in caches or on disk can be
   * remapped to actual IDs on load. The mapping also makes sure that IDs referenced by baked data
   * are not automatically removed because they are not referenced anymore. Furthermore, it allows
   * the modifier to add all required IDs to the dependency graph before actually loading the baked
   * data.
   */
  int data_blocks_num;
  int active_data_block;
  NodesModifierDataBlock *data_blocks;
  NodesModifierPackedBake *packed;

  void *_pad2;
  int64_t bake_size;
} NodesModifierBake;

typedef struct NodesModifierPanel {
  /** ID of the corresponding panel from #bNodeTreeInterfacePanel::identifier. */
  int id;
  /** #NodesModifierPanelFlag. */
  uint32_t flag;
} NodesModifierPanel;

typedef enum NodesModifierPanelFlag {
  NODES_MODIFIER_PANEL_OPEN = 1 << 0,
} NodesModifierPanelFlag;

typedef enum NodesModifierBakeFlag {
  NODES_MODIFIER_BAKE_CUSTOM_SIMULATION_FRAME_RANGE = 1 << 0,
  NODES_MODIFIER_BAKE_CUSTOM_PATH = 1 << 1,
} NodesModifierBakeFlag;

typedef enum NodesModifierBakeTarget {
  NODES_MODIFIER_BAKE_TARGET_INHERIT = 0,
  NODES_MODIFIER_BAKE_TARGET_PACKED = 1,
  NODES_MODIFIER_BAKE_TARGET_DISK = 2,
} NodesModifierBakeTarget;

typedef enum NodesModifierBakeMode {
  NODES_MODIFIER_BAKE_MODE_ANIMATION = 0,
  NODES_MODIFIER_BAKE_MODE_STILL = 1,
} NodesModifierBakeMode;

typedef enum GeometryNodesModifierPanel {
  NODES_MODIFIER_PANEL_OUTPUT_ATTRIBUTES = 0,
  NODES_MODIFIER_PANEL_MANAGE = 1,
  NODES_MODIFIER_PANEL_BAKE = 2,
  NODES_MODIFIER_PANEL_NAMED_ATTRIBUTES = 3,
  NODES_MODIFIER_PANEL_BAKE_DATA_BLOCKS = 4,
  NODES_MODIFIER_PANEL_WARNINGS = 5,
} GeometryNodesModifierPanel;

typedef struct NodesModifierData {
  ModifierData modifier;
  struct bNodeTree *node_group;
  struct NodesModifierSettings settings;
  /**
   * Directory where baked simulation states are stored. This may be relative to the .blend file.
   */
  char *bake_directory;
  /** NodesModifierFlag. */
  int8_t flag;
  /** #NodesModifierBakeTarget. */
  int8_t bake_target;

  char _pad[2];
  int bakes_num;
  NodesModifierBake *bakes;

  char _pad2[4];
  int panels_num;
  NodesModifierPanel *panels;

  NodesModifierRuntimeHandle *runtime;

#ifdef __cplusplus
  NodesModifierBake *find_bake(int id);
  const NodesModifierBake *find_bake(int id) const;
#endif
} NodesModifierData;

typedef enum NodesModifierFlag {
  NODES_MODIFIER_HIDE_DATABLOCK_SELECTOR = (1 << 0),
  NODES_MODIFIER_HIDE_MANAGE_PANEL = (1 << 1),
} NodesModifierFlag;

typedef struct MeshToVolumeModifierData {
  ModifierData modifier;

  /** This is the object that is supposed to be converted to a volume. */
  struct Object *object;

  /** MeshToVolumeModifierResolutionMode */
  int resolution_mode;
  /** Size of a voxel in object space. */
  float voxel_size;
  /** The desired amount of voxels along one axis. The actual amount of voxels might be slightly
   * different. */
  int voxel_amount;

  float interior_band_width;

  float density;
  char _pad2[4];
  void *_pad3;
} MeshToVolumeModifierData;

/** #MeshToVolumeModifierData.resolution_mode */
typedef enum MeshToVolumeModifierResolutionMode {
  MESH_TO_VOLUME_RESOLUTION_MODE_VOXEL_AMOUNT = 0,
  MESH_TO_VOLUME_RESOLUTION_MODE_VOXEL_SIZE = 1,
} MeshToVolumeModifierResolutionMode;

typedef struct VolumeDisplaceModifierData {
  ModifierData modifier;

  struct Tex *texture;
  struct Object *texture_map_object;
  /** #VolumeDisplaceModifierTextureMapMode. */
  int texture_map_mode;

  float strength;
  float texture_mid_level[3];
  float texture_sample_radius;
} VolumeDisplaceModifierData;

/** #VolumeDisplaceModifierData.texture_map_mode */
typedef enum {
  MOD_VOLUME_DISPLACE_MAP_LOCAL = 0,
  MOD_VOLUME_DISPLACE_MAP_GLOBAL = 1,
  MOD_VOLUME_DISPLACE_MAP_OBJECT = 2,
} VolumeDisplaceModifierTextureMapMode;

typedef struct VolumeToMeshModifierData {
  ModifierData modifier;

  /** This is the volume object that is supposed to be converted to a mesh. */
  struct Object *object;

  float threshold;
  float adaptivity;

  /** VolumeToMeshFlag */
  uint32_t flag;

  /** VolumeToMeshResolutionMode */
  int resolution_mode;
  float voxel_size;
  int voxel_amount;

  char grid_name[/*MAX_NAME*/ 64];
  void *_pad1;
} VolumeToMeshModifierData;

/** VolumeToMeshModifierData->resolution_mode */
typedef enum VolumeToMeshResolutionMode {
  VOLUME_TO_MESH_RESOLUTION_MODE_GRID = 0,
  VOLUME_TO_MESH_RESOLUTION_MODE_VOXEL_AMOUNT = 1,
  VOLUME_TO_MESH_RESOLUTION_MODE_VOXEL_SIZE = 2,
} VolumeToMeshResolutionMode;

/** VolumeToMeshModifierData->flag */
typedef enum VolumeToMeshFlag {
  VOLUME_TO_MESH_USE_SMOOTH_SHADE = 1 << 0,
} VolumeToMeshFlag;

/**
 * Common influence data for grease pencil modifiers.
 * Not all parts may be used by all modifier types.
 */
typedef struct GreasePencilModifierInfluenceData {
  /** GreasePencilModifierInfluenceFlag */
  int flag;
  char _pad1[4];
  /** Filter by layer name. */
  char layer_name[64];
  /** Filter by stroke material. */
  struct Material *material;
  /** Filter by layer pass. */
  int layer_pass;
  /** Filter by material pass. */
  int material_pass;
  char vertex_group_name[/*MAX_VGROUP_NAME*/ 64];
  struct CurveMapping *custom_curve;
  void *_pad2;
} GreasePencilModifierInfluenceData;

typedef enum GreasePencilModifierInfluenceFlag {
  GREASE_PENCIL_INFLUENCE_INVERT_LAYER_FILTER = (1 << 0),
  GREASE_PENCIL_INFLUENCE_USE_LAYER_PASS_FILTER = (1 << 1),
  GREASE_PENCIL_INFLUENCE_INVERT_LAYER_PASS_FILTER = (1 << 2),
  GREASE_PENCIL_INFLUENCE_INVERT_MATERIAL_FILTER = (1 << 3),
  GREASE_PENCIL_INFLUENCE_USE_MATERIAL_PASS_FILTER = (1 << 4),
  GREASE_PENCIL_INFLUENCE_INVERT_MATERIAL_PASS_FILTER = (1 << 5),
  GREASE_PENCIL_INFLUENCE_INVERT_VERTEX_GROUP = (1 << 6),
  GREASE_PENCIL_INFLUENCE_USE_CUSTOM_CURVE = (1 << 7),
  GREASE_PENCIL_INFLUENCE_USE_LAYER_GROUP_FILTER = (1 << 8),
} GreasePencilModifierInfluenceFlag;

typedef struct GreasePencilOpacityModifierData {
  ModifierData modifier;
  GreasePencilModifierInfluenceData influence;
  /** GreasePencilOpacityModifierFlag */
  int flag;
  /** GreasePencilModifierColorMode */
  char color_mode;
  char _pad1[3];
  float color_factor;
  float hardness_factor;
  void *_pad2;
} GreasePencilOpacityModifierData;

/** Which attributes are affected by color modifiers. */
typedef enum GreasePencilModifierColorMode {
  MOD_GREASE_PENCIL_COLOR_STROKE = 0,
  MOD_GREASE_PENCIL_COLOR_FILL = 1,
  MOD_GREASE_PENCIL_COLOR_BOTH = 2,
  MOD_GREASE_PENCIL_COLOR_HARDNESS = 3,
} GreasePencilModifierColorMode;

typedef enum GreasePencilOpacityModifierFlag {
  /* Use vertex group as opacity factors instead of influence. */
  MOD_GREASE_PENCIL_OPACITY_USE_WEIGHT_AS_FACTOR = (1 << 0),
  /* Set the opacity for every point in a stroke, otherwise multiply existing opacity. */
  MOD_GREASE_PENCIL_OPACITY_USE_UNIFORM_OPACITY = (1 << 1),
} GreasePencilOpacityModifierFlag;

typedef struct GreasePencilSubdivModifierData {
  ModifierData modifier;
  GreasePencilModifierInfluenceData influence;
  /** #GreasePencilSubdivideType. */
  int type;
  /** Level of subdivisions, will generate 2^level segments. */
  int level;

  char _pad[8];
  void *_pad1;
} GreasePencilSubdivModifierData;

typedef enum GreasePencilSubdivideType {
  MOD_GREASE_PENCIL_SUBDIV_CATMULL = 0,
  MOD_GREASE_PENCIL_SUBDIV_SIMPLE = 1,
} GreasePencilSubdivideType;

typedef struct GreasePencilColorModifierData {
  ModifierData modifier;
  GreasePencilModifierInfluenceData influence;
  /** GreasePencilModifierColorMode */
  char color_mode;
  char _pad1[3];
  /** HSV factors. */
  float hsv[3];
  void *_pad2;
} GreasePencilColorModifierData;

typedef struct GreasePencilTintModifierData {
  ModifierData modifier;
  GreasePencilModifierInfluenceData influence;
  /** GreasePencilTintModifierFlag */
  short flag;
  /** GreasePencilModifierColorMode */
  char color_mode;
  /** GreasePencilTintModifierMode */
  char tint_mode;
  float factor;
  /** Influence distance from the gradient object. */
  float radius;
  /** Simple tint color. */
  float color[3];
  /** Object for gradient direction. */
  struct Object *object;
  /** Color ramp for the gradient. */
  struct ColorBand *color_ramp;
  void *_pad;
} GreasePencilTintModifierData;

typedef enum GreasePencilTintModifierMode {
  MOD_GREASE_PENCIL_TINT_UNIFORM = 0,
  MOD_GREASE_PENCIL_TINT_GRADIENT = 1,
} GreasePencilTintModifierMode;

typedef enum GreasePencilTintModifierFlag {
  /* Use vertex group as factors instead of influence. */
  MOD_GREASE_PENCIL_TINT_USE_WEIGHT_AS_FACTOR = (1 << 0),
} GreasePencilTintModifierFlag;

/* Enum definitions for length modifier stays in the old DNA for the moment. */
typedef struct GreasePencilSmoothModifierData {
  ModifierData modifier;
  GreasePencilModifierInfluenceData influence;
  /** #eGreasePencilSmooth_Flag. */
  int flag;
  /** Factor of smooth. */
  float factor;
  /** How many times apply smooth. */
  int step;
  char _pad[4];
  void *_pad1;
} GreasePencilSmoothModifierData;

typedef enum eGreasePencilSmooth_Flag {
  MOD_GREASE_PENCIL_SMOOTH_MOD_LOCATION = (1 << 0),
  MOD_GREASE_PENCIL_SMOOTH_MOD_STRENGTH = (1 << 1),
  MOD_GREASE_PENCIL_SMOOTH_MOD_THICKNESS = (1 << 2),
  MOD_GREASE_PENCIL_SMOOTH_MOD_UV = (1 << 3),
  MOD_GREASE_PENCIL_SMOOTH_KEEP_SHAPE = (1 << 4),
  MOD_GREASE_PENCIL_SMOOTH_SMOOTH_ENDS = (1 << 5),
} eGreasePencilSmooth_Flag;

typedef struct GreasePencilOffsetModifierData {
  ModifierData modifier;
  GreasePencilModifierInfluenceData influence;
  /** GreasePencilOffsetModifierFlag */
  int flag;
  /** GreasePencilOffsetModifierMode */
  int offset_mode;
  /** Global offset. */
  float loc[3];
  float rot[3];
  float scale[3];
  /** Offset per stroke. */
  float stroke_loc[3];
  float stroke_rot[3];
  float stroke_scale[3];
  int seed;
  int stroke_step;
  int stroke_start_offset;
  char _pad1[4];
  void *_pad2;
} GreasePencilOffsetModifierData;

typedef enum GreasePencilOffsetModifierFlag {
  MOD_GREASE_PENCIL_OFFSET_UNIFORM_RANDOM_SCALE = (1 << 0),
} GreasePencilOffsetModifierFlag;

typedef enum GreasePencilOffsetModifierMode {
  MOD_GREASE_PENCIL_OFFSET_RANDOM = 0,
  MOD_GREASE_PENCIL_OFFSET_LAYER = 1,
  MOD_GREASE_PENCIL_OFFSET_MATERIAL = 2,
  MOD_GREASE_PENCIL_OFFSET_STROKE = 3,
} GreasePencilOffsetModifierMode;

typedef struct GreasePencilNoiseModifierData {
  ModifierData modifier;
  GreasePencilModifierInfluenceData influence;

  /** For convenience of versioning, these flags are kept in `eNoiseGpencil_Flag`. */
  int flag;

  /** Factor of noise. */
  float factor;
  float factor_strength;
  float factor_thickness;
  float factor_uvs;
  /** Noise Frequency scaling */
  float noise_scale;
  float noise_offset;
  short noise_mode;
  char _pad[2];
  /** How many frames before recalculate randoms. */
  int step;
  /** Random seed */
  int seed;

  void *_pad1;
} GreasePencilNoiseModifierData;

typedef struct GreasePencilMirrorModifierData {
  ModifierData modifier;
  GreasePencilModifierInfluenceData influence;
  struct Object *object;
  /** #GreasePencilMirrorModifierFlag */
  int flag;
  char _pad[4];
} GreasePencilMirrorModifierData;

typedef enum GreasePencilMirrorModifierFlag {
  MOD_GREASE_PENCIL_MIRROR_AXIS_X = (1 << 0),
  MOD_GREASE_PENCIL_MIRROR_AXIS_Y = (1 << 1),
  MOD_GREASE_PENCIL_MIRROR_AXIS_Z = (1 << 2),
} GreasePencilMirrorModifierFlag;

typedef struct GreasePencilThickModifierData {
  ModifierData modifier;
  GreasePencilModifierInfluenceData influence;
  /** #GreasePencilThicknessModifierFlag */
  int flag;
  /** Relative thickness factor. */
  float thickness_fac;
  /** Absolute thickness override. */
  float thickness;
  char _pad[4];
  void *_pad1;
} GreasePencilThickModifierData;

typedef enum GreasePencilThicknessModifierFlag {
  MOD_GREASE_PENCIL_THICK_NORMALIZE = (1 << 0),
  MOD_GREASE_PENCIL_THICK_WEIGHT_FACTOR = (1 << 1),
} GreasePencilThicknessModifierFlag;

typedef struct GreasePencilLatticeModifierData {
  ModifierData modifier;
  GreasePencilModifierInfluenceData influence;
  struct Object *object;
  float strength;
  char _pad[4];
} GreasePencilLatticeModifierData;

typedef struct GreasePencilDashModifierSegment {
  char name[64];
  int dash;
  int gap;
  float radius;
  float opacity;
  int mat_nr;
  /** #GreasePencilDashModifierFlag */
  int flag;
} GreasePencilDashModifierSegment;

typedef struct GreasePencilDashModifierData {
  ModifierData modifier;
  GreasePencilModifierInfluenceData influence;

  GreasePencilDashModifierSegment *segments_array;
  int segments_num;
  int segment_active_index;

  int dash_offset;
  char _pad[4];

#ifdef __cplusplus
  blender::Span<GreasePencilDashModifierSegment> segments() const;
  blender::MutableSpan<GreasePencilDashModifierSegment> segments();
#endif
} GreasePencilDashModifierData;

typedef enum GreasePencilDashModifierFlag {
  MOD_GREASE_PENCIL_DASH_USE_CYCLIC = (1 << 0),
} GreasePencilDashModifierFlag;

typedef struct GreasePencilMultiModifierData {
  ModifierData modifier;
  GreasePencilModifierInfluenceData influence;

  /* #GreasePencilMultiplyModifierFlag */
  int flag;

  int duplications;
  float distance;
  /* -1:inner 0:middle 1:outer */
  float offset;

  float fading_center;
  float fading_thickness;
  float fading_opacity;

  int _pad0;

  void *_pad;
} GreasePencilMultiModifierData;

typedef enum GreasePencilMultiplyModifierFlag {
  /* GP_MULTIPLY_ENABLE_ANGLE_SPLITTING = (1 << 1),  Deprecated. */
  MOD_GREASE_PENCIL_MULTIPLY_ENABLE_FADING = (1 << 2),
} GreasePencilMultiplyModifierFlag;

typedef struct GreasePencilLengthModifierData {
  ModifierData modifier;
  GreasePencilModifierInfluenceData influence;
  int flag;
  float start_fac, end_fac;
  float rand_start_fac, rand_end_fac, rand_offset;
  float overshoot_fac;
  /** (first element is the index) random values. */
  int seed;
  /** How many frames before recalculate randoms. */
  int step;
  /** #eLengthGpencil_Type. */
  int mode;
  char _pad[4];
  /* Curvature parameters. */
  float point_density;
  float segment_influence;
  float max_angle;

  void *_pad1;
} GreasePencilLengthModifierData;

typedef struct GreasePencilWeightAngleModifierData {
  ModifierData modifier;
  GreasePencilModifierInfluenceData influence;
  /** #GreasePencilWeightAngleModifierFlag */
  int flag;
  float min_weight;
  /** Axis. */
  int16_t axis;
  /** #GreasePencilWeightAngleModifierSpace */
  int16_t space;
  /** Angle */
  float angle;
  /** Weights output to this vertex group, can be the same as source group. */
  char target_vgname[64];

  void *_pad;
} GreasePencilWeightAngleModifierData;

typedef enum GreasePencilWeightAngleModifierFlag {
  MOD_GREASE_PENCIL_WEIGHT_ANGLE_MULTIPLY_DATA = (1 << 5),
  MOD_GREASE_PENCIL_WEIGHT_ANGLE_INVERT_OUTPUT = (1 << 6),
} GreasePencilWeightAngleModifierFlag;

typedef enum GreasePencilWeightAngleModifierSpace {
  MOD_GREASE_PENCIL_WEIGHT_ANGLE_SPACE_LOCAL = 0,
  MOD_GREASE_PENCIL_WEIGHT_ANGLE_SPACE_WORLD = 1,
} GreasePencilWeightAngleModifierSpace;

typedef struct GreasePencilArrayModifierData {
  ModifierData modifier;
  GreasePencilModifierInfluenceData influence;
  struct Object *object;
  int count;
  /** #GreasePencilArrayModifierFlag */
  int flag;
  float offset[3];
  float shift[3];

  float rnd_offset[3];
  float rnd_rot[3];
  float rnd_scale[3];

  char _pad[4];
  /** (first element is the index) random values. (?) */
  int seed;

  /* Replacement material index. */
  int mat_rpl;
} GreasePencilArrayModifierData;

typedef enum GreasePencilArrayModifierFlag {
  MOD_GREASE_PENCIL_ARRAY_USE_OFFSET = (1 << 7),
  MOD_GREASE_PENCIL_ARRAY_USE_RELATIVE = (1 << 8),
  MOD_GREASE_PENCIL_ARRAY_USE_OB_OFFSET = (1 << 9),
  MOD_GREASE_PENCIL_ARRAY_UNIFORM_RANDOM_SCALE = (1 << 10),
} GreasePencilArrayModifierFlag;

typedef struct GreasePencilWeightProximityModifierData {
  ModifierData modifier;
  GreasePencilModifierInfluenceData influence;

  /* #GreasePencilWeightProximityFlag. */
  int flag;
  char target_vgname[64];
  float min_weight;

  float dist_start;
  float dist_end;

  struct Object *object;
} GreasePencilWeightProximityModifierData;

typedef enum GreasePencilWeightProximityFlag {
  MOD_GREASE_PENCIL_WEIGHT_PROXIMITY_INVERT_OUTPUT = (1 << 0),
  MOD_GREASE_PENCIL_WEIGHT_PROXIMITY_MULTIPLY_DATA = (1 << 1),
} GreasePencilWeightProximityFlag;

typedef struct GreasePencilHookModifierData {
  ModifierData modifier;
  GreasePencilModifierInfluenceData influence;

  struct Object *object;
  /** Optional name of bone target. */
  char subtarget[/*MAX_NAME*/ 64];
  char _pad[4];

  /** #GreasePencilHookFlag. */
  int flag;
  /** #GreasePencilHookFalloff. */
  char falloff_type;
  char _pad1[3];
  /** Matrix making current transform unmodified. */
  float parentinv[4][4];
  /** Visualization of hook. */
  float cent[3];
  /** If not zero, falloff is distance where influence zero. */
  float falloff;
  float force;
} GreasePencilHookModifierData;

typedef enum GreasePencilHookFlag {
  MOD_GREASE_PENCIL_HOOK_UNIFORM_SPACE = (1 << 0),
} GreasePencilHookFlag;

typedef enum GreasePencilHookFalloff {
  MOD_GREASE_PENCIL_HOOK_Falloff_None = 0,
  MOD_GREASE_PENCIL_HOOK_Falloff_Curve = 1,
  MOD_GREASE_PENCIL_HOOK_Falloff_Sharp = 2,
  MOD_GREASE_PENCIL_HOOK_Falloff_Smooth = 3,
  MOD_GREASE_PENCIL_HOOK_Falloff_Root = 4,
  MOD_GREASE_PENCIL_HOOK_Falloff_Linear = 5,
  MOD_GREASE_PENCIL_HOOK_Falloff_Const = 6,
  MOD_GREASE_PENCIL_HOOK_Falloff_Sphere = 7,
  MOD_GREASE_PENCIL_HOOK_Falloff_InvSquare = 8,
} GreasePencilHookFalloff;

/* This enum is for modifier internal state only. */
typedef enum eGreasePencilLineartFlags {
  /* These two moved to #eLineartMainFlags to keep consistent with flag variable purpose. */
  /* LINEART_GPENCIL_INVERT_SOURCE_VGROUP = (1 << 0), */
  /* LINEART_GPENCIL_MATCH_OUTPUT_VGROUP = (1 << 1), */
  LINEART_GPENCIL_BINARY_WEIGHTS = (1
                                    << 2) /* Deprecated, this is removed for lack of use case. */,
  LINEART_GPENCIL_IS_BAKED = (1 << 3),
  LINEART_GPENCIL_USE_CACHE = (1 << 4),
  LINEART_GPENCIL_OFFSET_TOWARDS_CUSTOM_CAMERA = (1 << 5),
  LINEART_GPENCIL_INVERT_COLLECTION = (1 << 6),
  LINEART_GPENCIL_INVERT_SILHOUETTE_FILTER = (1 << 7),
} eGreasePencilLineartFlags;

typedef enum GreasePencilLineartModifierSource {
  LINEART_SOURCE_COLLECTION = 0,
  LINEART_SOURCE_OBJECT = 1,
  LINEART_SOURCE_SCENE = 2,
} GreasePencilLineartModifierSource;

typedef enum GreasePencilLineartModifierShadowFilter {
  /* These options need to be ordered in this way because those latter options requires line art to
   * run a few extra stages. Having those values set up this way will allow
   * #BKE_gpencil_get_lineart_modifier_limits() to find out maximum stages needed in multiple
   * cached line art modifiers. */
  LINEART_SHADOW_FILTER_NONE = 0,
  LINEART_SHADOW_FILTER_ILLUMINATED = 1,
  LINEART_SHADOW_FILTER_SHADED = 2,
  LINEART_SHADOW_FILTER_ILLUMINATED_ENCLOSED_SHAPES = 3,
} GreasePencilLineartModifierShadowFilter;

/* This enum is for modifier internal state only. */
typedef enum eLineArtGPencilModifierFlags {
  /* These two moved to #eLineartMainFlags to keep consistent with flag variable purpose. */
  /* MOD_LINEART_INVERT_SOURCE_VGROUP = (1 << 0), */
  /* MOD_LINEART_MATCH_OUTPUT_VGROUP = (1 << 1), */
  MOD_LINEART_BINARY_WEIGHTS = (1 << 2) /* Deprecated, this is removed for lack of use case. */,
  MOD_LINEART_IS_BAKED = (1 << 3),
  MOD_LINEART_USE_CACHE = (1 << 4),
  MOD_LINEART_OFFSET_TOWARDS_CUSTOM_CAMERA = (1 << 5),
  MOD_LINEART_INVERT_COLLECTION = (1 << 6),
  MOD_LINEART_INVERT_SILHOUETTE_FILTER = (1 << 7),
} eLineArtGPencilModifierFlags;

typedef enum GreasePencilLineartMaskSwitches {
  MOD_LINEART_MATERIAL_MASK_ENABLE = (1 << 0),
  /** When set, material mask bit comparisons are done with bit wise "AND" instead of "OR". */
  MOD_LINEART_MATERIAL_MASK_MATCH = (1 << 1),
  MOD_LINEART_INTERSECTION_MATCH = (1 << 2),
} GreasePencilLineartMaskSwitches;

typedef enum eGreasePencilLineartMaskSwitches {
  LINEART_GPENCIL_MATERIAL_MASK_ENABLE = (1 << 0),
  /** When set, material mask bit comparisons are done with bit wise "AND" instead of "OR". */
  LINEART_GPENCIL_MATERIAL_MASK_MATCH = (1 << 1),
  LINEART_GPENCIL_INTERSECTION_MATCH = (1 << 2),
} eGreasePencilLineartMaskSwitches;

typedef enum eGreasePencilLineartSilhouetteFilter {
  LINEART_SILHOUETTE_FILTER_NONE = 0,
  LINEART_SILHOUETTE_FILTER_GROUP = (1 << 0),
  LINEART_SILHOUETTE_FILTER_INDIVIDUAL = (1 << 1),
} eGreasePencilLineartSilhouetteFilter;

struct LineartCache;

typedef struct GreasePencilLineartModifierData {
  ModifierData modifier;

  uint16_t edge_types; /* line type enable flags, bits in eLineartEdgeFlag */

  /** Object or Collection, from #eGreasePencilLineartSource. */
  char source_type;

  char use_multiple_levels;
  short level_start;
  short level_end;

  struct Object *source_camera;
  struct Object *light_contour_object;

  struct Object *source_object;
  struct Collection *source_collection;

  struct Material *target_material;
  char target_layer[64];

  /**
   * These two variables are to pass on vertex group information from mesh to strokes.
   * `vgname` specifies which vertex groups our strokes from source_vertex_group will go to.
   */
  char source_vertex_group[64];
  char vgname[64];

  /* Camera focal length is divided by (1 + over-scan), before calculation, which give a wider FOV,
   * this doesn't change coordinates range internally (-1, 1), but makes the calculated frame
   * bigger than actual output. This is for the easier shifting calculation. A value of 0.5 means
   * the "internal" focal length become 2/3 of the actual camera. */
  float overscan;

  /* Values for point light and directional (sun) light. */
  /* For point light, fov always gonna be 120 deg horizontal, with 3 "cameras" covering 360 deg. */
  float shadow_camera_fov;
  float shadow_camera_size;
  float shadow_camera_near;
  float shadow_camera_far;

  float opacity;
  float radius;

  short thickness_legacy; /* Deprecated, use `radius`. */

  unsigned char mask_switches; /* #eGreasePencilLineartMaskSwitches */
  unsigned char material_mask_bits;
  unsigned char intersection_mask;

  unsigned char shadow_selection;
  unsigned char silhouette_selection;
  char _pad[5];

  /** `0..1` range for cosine angle */
  float crease_threshold;

  /** `0..PI` angle, for splitting strokes at sharp points. */
  float angle_splitting_threshold;

  /** Strength for smoothing jagged chains. */
  float chain_smooth_tolerance;

  /* CPU mode */
  float chaining_image_threshold;

  /* eLineartMainFlags, for one time calculation. */
  int calculation_flags;

  /* #eGreasePencilLineartFlags, modifier internal state. */
  int flags;

  /* Move strokes towards camera to avoid clipping while preserve depth for the viewport. */
  float stroke_depth_offset;

  /* Runtime data. */

  /* Because we can potentially only compute features lines once per modifier stack (Use Cache), we
   * need to have these override values to ensure that we have the data we need is computed and
   * stored in the cache. */
  char level_start_override;
  char level_end_override;
  short edge_types_override;
  char shadow_selection_override;
  char shadow_use_silhouette_override;

  char _pad2[6];

  /* Shared cache will only be on the first line art modifier in the stack, and will exist until
   * the end of modifier stack evaluation. If the object has line art modifiers, this variable is
   * then initialized in #grease_pencil_evaluate_modifiers(). */
  struct LineartCache *shared_cache;

  /* Cache for single execution of line art, when LINEART_GPENCIL_USE_CACHE is enabled, this is a
   * reference to first_lineart->shared_cache, otherwise it holds its own cache. */
  struct LineartCache *cache;

  /* Keep a pointer to the render buffer so we can call destroy from #ModifierData. */
  struct LineartData *la_data_ptr;

  /* Points to a `LineartModifierRuntime`, which includes the object dependency list. */
  struct LineartModifierRuntime *runtime;
} GreasePencilLineartModifierData;

typedef struct GreasePencilArmatureModifierData {
  ModifierData modifier;
  GreasePencilModifierInfluenceData influence;

  struct Object *object;
  /** #eArmature_DeformFlag. */
  short deformflag;
  char _pad[6];
} GreasePencilArmatureModifierData;

typedef struct GreasePencilTimeModifierSegment {
  char name[64];
  int segment_start;
  int segment_end;
  /** #GreasePencilTimeModifierSegmentMode. */
  int segment_mode;
  int segment_repeat;
} GreasePencilTimeModifierSegment;

typedef struct GreasePencilTimeModifierData {
  ModifierData modifier;
  GreasePencilModifierInfluenceData influence;
  /** #GreasePencilTimeModifierFlag */
  int flag;
  int offset;
  /** Animation scale. */
  float frame_scale;
  /** #GreasePencilTimeModifierMode. */
  int mode;
  /** Start and end frame for custom range. */
  int sfra, efra;

  GreasePencilTimeModifierSegment *segments_array;
  int segments_num;
  int segment_active_index;

#ifdef __cplusplus
  blender::Span<GreasePencilTimeModifierSegment> segments() const;
  blender::MutableSpan<GreasePencilTimeModifierSegment> segments();
#endif
} GreasePencilTimeModifierData;

typedef enum GreasePencilTimeModifierFlag {
  MOD_GREASE_PENCIL_TIME_KEEP_LOOP = (1 << 0),
  MOD_GREASE_PENCIL_TIME_CUSTOM_RANGE = (1 << 1),
} GreasePencilTimeModifierFlag;

typedef enum GreasePencilTimeModifierMode {
  MOD_GREASE_PENCIL_TIME_MODE_NORMAL = 0,
  MOD_GREASE_PENCIL_TIME_MODE_REVERSE = 1,
  MOD_GREASE_PENCIL_TIME_MODE_FIX = 2,
  MOD_GREASE_PENCIL_TIME_MODE_PINGPONG = 3,
  MOD_GREASE_PENCIL_TIME_MODE_CHAIN = 4,
} GreasePencilTimeModifierMode;

typedef enum GreasePencilTimeModifierSegmentMode {
  MOD_GREASE_PENCIL_TIME_SEG_MODE_NORMAL = 0,
  MOD_GREASE_PENCIL_TIME_SEG_MODE_REVERSE = 1,
  MOD_GREASE_PENCIL_TIME_SEG_MODE_PINGPONG = 2,
} GreasePencilTimeModifierSegmentMode;

typedef struct GreasePencilEnvelopeModifierData {
  ModifierData modifier;
  GreasePencilModifierInfluenceData influence;
  /* #GreasePencilEnvelopeModifierMode. */
  int mode;
  /** Material for the new strokes. */
  int mat_nr;
  /** Thickness multiplier for the new strokes. */
  float thickness;
  /** Strength multiplier for the new strokes. */
  float strength;
  /** Number of points to skip over. */
  int skip;
  /* Length of the envelope effect. */
  int spread;
} GreasePencilEnvelopeModifierData;

/* Texture->mode */
typedef enum GreasePencilEnvelopeModifierMode {
  MOD_GREASE_PENCIL_ENVELOPE_DEFORM = 0,
  MOD_GREASE_PENCIL_ENVELOPE_SEGMENTS = 1,
  MOD_GREASE_PENCIL_ENVELOPE_FILLS = 2,
} GreasePencilEnvelopeModifierMode;

typedef struct GreasePencilOutlineModifierData {
  ModifierData modifier;
  GreasePencilModifierInfluenceData influence;

  /** Target stroke origin. */
  struct Object *object;
  /** #GreasePencilOutlineModifierFlag. */
  int flag;
  /** Thickness. */
  int thickness;
  /** Sample Length. */
  float sample_length;
  /** Subdivisions. */
  int subdiv;
  /** Material for outline. */
  struct Material *outline_material;
} GreasePencilOutlineModifierData;

typedef enum GreasePencilOutlineModifierFlag {
  MOD_GREASE_PENCIL_OUTLINE_KEEP_SHAPE = (1 << 0),
} GreasePencilOutlineModifierFlag;

typedef struct GreasePencilShrinkwrapModifierData {
  ModifierData modifier;
  GreasePencilModifierInfluenceData influence;

  /** Shrink target. */
  struct Object *target;
  /** Additional shrink target. */
  struct Object *aux_target;
  /** Distance offset to keep from mesh/projection point. */
  float keep_dist;
  /** Shrink type projection. */
  short shrink_type;
  /** Shrink options. */
  char shrink_opts;
  /** Shrink to surface mode. */
  char shrink_mode;
  /** Limit the projection ray cast. */
  float proj_limit;
  /** Axis to project over. */
  char proj_axis;

  /**
   * If using projection over vertex normal this controls the level of subsurface that must be
   * done before getting the vertex coordinates and normal.
   */
  char subsurf_levels;
  char _pad[2];
  /** Factor of smooth. */
  float smooth_factor;
  /** How many times apply smooth. */
  int smooth_step;

  /** Runtime only. */
  struct ShrinkwrapTreeData *cache_data;
} GreasePencilShrinkwrapModifierData;

typedef struct GreasePencilBuildModifierData {
  ModifierData modifier;
  GreasePencilModifierInfluenceData influence;
  /**
   * If GP_BUILD_RESTRICT_TIME is set,
   * the defines the frame range where GP frames are considered.
   */
  float start_frame;
  float end_frame;

  /** Start time added on top of the drawing frame number */
  float start_delay;
  float length;

  /** #GreasePencilBuildFlag. */
  short flag;

  /** #GreasePencilBuildMode. */
  short mode;
  /** #GreasePencilBuildTransition. */
  short transition;

  /**
   * #GreasePencilBuildTimeAlignment.
   * For the "Concurrent" mode, when should "shorter" strips start/end.
   */
  short time_alignment;

  /** Speed factor for #GP_BUILD_TIMEMODE_DRAWSPEED. */
  float speed_fac;
  /** Maximum time gap between strokes for #GP_BUILD_TIMEMODE_DRAWSPEED. */
  float speed_maxgap;
  /** GreasePencilBuildTimeMode. */
  short time_mode;
  char _pad[6];

  /** Build origin control object. */
  struct Object *object;

  /** Factor of the stroke (used instead of frame evaluation). */
  float percentage_fac;

  /** Weight fading at the end of the stroke. */
  float fade_fac;
  /** Target vertex-group name. */
  char target_vgname[/*MAX_VGROUP_NAME*/ 64];
  /** Fading strength of opacity and thickness */
  float fade_opacity_strength;
  float fade_thickness_strength;
} GreasePencilBuildModifierData;

typedef enum GreasePencilBuildMode {
  /* Strokes are shown one by one until all have appeared */
  MOD_GREASE_PENCIL_BUILD_MODE_SEQUENTIAL = 0,
  /* All strokes start at the same time */
  MOD_GREASE_PENCIL_BUILD_MODE_CONCURRENT = 1,
  /* Only the new strokes are built */
  MOD_GREASE_PENCIL_BUILD_MODE_ADDITIVE = 2,
} GreasePencilBuildMode;

typedef enum GreasePencilBuildTransition {
  /* Show in forward order */
  MOD_GREASE_PENCIL_BUILD_TRANSITION_GROW = 0,
  /* Hide in reverse order */
  MOD_GREASE_PENCIL_BUILD_TRANSITION_SHRINK = 1,
  /* Hide in forward order */
  MOD_GREASE_PENCIL_BUILD_TRANSITION_VANISH = 2,
} GreasePencilBuildTransition;

typedef enum GreasePencilBuildTimeAlignment {
  /* All strokes start at same time */
  MOD_GREASE_PENCIL_BUILD_TIMEALIGN_START = 0,
  /* All strokes end at same time */
  MOD_GREASE_PENCIL_BUILD_TIMEALIGN_END = 1,

  /* TODO: Random Offsets, Stretch-to-Fill */
} GreasePencilBuildTimeAlignment;

typedef enum GreasePencilBuildTimeMode {
  /** Use a number of frames build. */
  MOD_GREASE_PENCIL_BUILD_TIMEMODE_FRAMES = 0,
  /** Use manual percentage to build. */
  MOD_GREASE_PENCIL_BUILD_TIMEMODE_PERCENTAGE = 1,
  /** Use factor of recorded speed to build. */
  MOD_GREASE_PENCIL_BUILD_TIMEMODE_DRAWSPEED = 2,
} GreasePencilBuildTimeMode;

typedef enum GreasePencilBuildFlag {
  /* Restrict modifier to only operating between the nominated frames */
  MOD_GREASE_PENCIL_BUILD_RESTRICT_TIME = (1 << 0),
  MOD_GREASE_PENCIL_BUILD_USE_FADING = (1 << 14),
} GreasePencilBuildFlag;

typedef struct GreasePencilSimplifyModifierData {
  ModifierData modifier;
  GreasePencilModifierInfluenceData influence;

  /** #GreasePencilSimplifyModifierMode. */
  short mode;
  char _pad[4];
  /** Every n vertex to keep. */
  short step;
  float factor;
  /** For sampling. */
  float length;
  float sharp_threshold;

  /** Merge distance */
  float distance;
} GreasePencilSimplifyModifierData;

typedef enum GreasePencilSimplifyModifierMode {
  MOD_GREASE_PENCIL_SIMPLIFY_FIXED = 0,
  MOD_GREASE_PENCIL_SIMPLIFY_ADAPTIVE = 1,
  MOD_GREASE_PENCIL_SIMPLIFY_SAMPLE = 2,
  MOD_GREASE_PENCIL_SIMPLIFY_MERGE = 3,
} GreasePencilSimplifyModifierMode;

typedef struct GreasePencilTextureModifierData {
  ModifierData modifier;
  GreasePencilModifierInfluenceData influence;
  /* Offset value to add to uv_fac. */
  float uv_offset;
  float uv_scale;
  float fill_rotation;
  float fill_offset[2];
  float fill_scale;
  /* Custom index for passes. */
  int layer_pass;
  /**  #GreasePencilTextureModifierFit.Texture fit options. */
  short fit_method;
  /** #GreasePencilTextureModifierMode. */
  short mode;
  /* Dot texture rotation. */
  float alignment_rotation;
  char _pad[4];
} GreasePencilTextureModifierData;

/* Texture->fit_method */
typedef enum GreasePencilTextureModifierFit {
  MOD_GREASE_PENCIL_TEXTURE_FIT_STROKE = 0,
  MOD_GREASE_PENCIL_TEXTURE_CONSTANT_LENGTH = 1,
} GreasePencilTextureModifierFit;

/* Texture->mode */
typedef enum GreasePencilTextureModifierMode {
  MOD_GREASE_PENCIL_TEXTURE_STROKE = 0,
  MOD_GREASE_PENCIL_TEXTURE_FILL = 1,
  MOD_GREASE_PENCIL_TEXTURE_STROKE_AND_FILL = 2,
} GreasePencilTextureModifierMode;
