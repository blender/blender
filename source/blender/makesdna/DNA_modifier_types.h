/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "BLI_enum_flags.hh"
#include "BLI_implicit_sharing.h"
#include "BLI_math_constants.h"
#include "BLI_span.hh"

#include "DNA_armature_types.h"
#include "DNA_defs.h"
#include "DNA_lineart_types.h"
#include "DNA_listBase.h"
#include "DNA_modifier_enums.h"
#include "DNA_packedFile_types.h"
#include "DNA_vec_defaults.h"

namespace blender {

struct NodesModifierRuntime;
namespace bke {
struct BVHTreeFromMesh;
}

struct LineartModifierRuntime;
struct Mesh;

/* WARNING ALERT! TYPEDEF VALUES ARE WRITTEN IN FILES! SO DO NOT CHANGE!
 * (ONLY ADD NEW ITEMS AT THE END)
 */

enum ModifierType {
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
};

enum ModifierMode {
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
};
ENUM_OPERATORS(ModifierMode);

enum ModifierFlag {
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
};

struct ModifierData {
  struct ModifierData *next = nullptr, *prev = nullptr;

  /** #ModifierType. */
  int type = 0;
  /** #ModifierMode. */
  int mode = 0;
  /** Time in seconds that the modifier took to evaluate. This is only set on evaluated objects. */
  float execution_time = 0;
  /** #ModifierFlag. */
  short flag = 0;
  /** An "expand" bit for each of the modifier's (sub)panels (#uiPanelDataExpansion). */
  short ui_expand_flag = 0;
  /**
   * Bits that can be used for open-states of layout panels in the modifier. This can replace
   * `ui_expand_flag` once all modifiers use layout panels. Currently, trying to reuse the same
   * flags is problematic, because the bits in `ui_expand_flag` are mapped to panels automatically
   * and easily conflict with the explicit mapping of bits to panels here.
   */
  uint16_t layout_panel_open_flag = 0;
  char _pad[2] = {};
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
  int persistent_uid = 0;
  char name[/*MAX_NAME*/ 64] = "";

  char *error = nullptr;

  /** Runtime field which contains runtime data which is specific to a modifier type. */
  void *runtime = nullptr;
};

/**
 * \note Not a real modifier.
 */
struct MappingInfoModifierData {
  ModifierData modifier;

  struct Tex *texture = nullptr;
  struct Object *map_object = nullptr;
  char map_bone[/*MAXBONENAME*/ 64] = "";
  char uvlayer_name[/*MAX_CUSTOMDATA_LAYER_NAME*/ 68] = "";
  char _pad1[4] = {};
  int uvlayer_tmp = 0;
  int texmapping = 0;
};

enum SubsurfModifierFlag {
  eSubsurfModifierFlag_Incremental = (1 << 0),
  eSubsurfModifierFlag_DebugIncr = (1 << 1),
  eSubsurfModifierFlag_ControlEdges = (1 << 2),
  /* DEPRECATED, ONLY USED FOR DO-VERSIONS */
  eSubsurfModifierFlag_SubsurfUv_DEPRECATED = (1 << 3),
  eSubsurfModifierFlag_UseCrease = (1 << 4),
  eSubsurfModifierFlag_UseCustomNormals = (1 << 5),
  eSubsurfModifierFlag_UseRecursiveSubdivision = (1 << 6),
  eSubsurfModifierFlag_UseAdaptiveSubdivision = (1 << 7),
};

enum eSubsurfAdaptiveSpace {
  SUBSURF_ADAPTIVE_SPACE_PIXEL = 0,
  SUBSURF_ADAPTIVE_SPACE_OBJECT = 1,
};

enum eSubsurfModifierType {
  SUBSURF_TYPE_CATMULL_CLARK = 0,
  SUBSURF_TYPE_SIMPLE = 1,
};

enum eSubsurfUVSmooth {
  SUBSURF_UV_SMOOTH_NONE = 0,
  SUBSURF_UV_SMOOTH_PRESERVE_CORNERS = 1,
  SUBSURF_UV_SMOOTH_PRESERVE_CORNERS_AND_JUNCTIONS = 2,
  SUBSURF_UV_SMOOTH_PRESERVE_CORNERS_JUNCTIONS_AND_CONCAVE = 3,
  SUBSURF_UV_SMOOTH_PRESERVE_BOUNDARIES = 4,
  SUBSURF_UV_SMOOTH_ALL = 5,
};

enum eSubsurfBoundarySmooth {
  SUBSURF_BOUNDARY_SMOOTH_ALL = 0,
  SUBSURF_BOUNDARY_SMOOTH_PRESERVE_CORNERS = 1,
};

struct SubsurfModifierData {
  ModifierData modifier;

  /** #MeshSubdivType. */
  short subdivType = 0;
  short levels = 1, renderLevels = 2;
  /** #SubsurfModifierFlag. */
  short flags = eSubsurfModifierFlag_UseCrease | eSubsurfModifierFlag_ControlEdges;
  /** #eSubsurfUVSmooth. */
  short uv_smooth = SUBSURF_UV_SMOOTH_PRESERVE_BOUNDARIES;
  short quality = 3;
  /** #eSubsurfBoundarySmooth. */
  short boundary_smooth = SUBSURF_BOUNDARY_SMOOTH_ALL;
  /* Adaptive subdivision. */
  /** #eSubsurfAdaptiveSpace */
  short adaptive_space = SUBSURF_ADAPTIVE_SPACE_PIXEL;
  float adaptive_pixel_size = 1.0f;
  float adaptive_object_edge_length = 0.01f;
};

/** #LatticeModifierData.flag */
enum LatticeModifierFlag {
  MOD_LATTICE_INVERT_VGROUP = (1 << 0),
};

struct LatticeModifierData {
  ModifierData modifier;

  struct Object *object = nullptr;
  /** Optional vertex-group name. */
  char name[/*MAX_VGROUP_NAME*/ 64] = "";
  float strength = 1.0f;
  /** #LatticeModifierFlag. */
  short flag = 0;
  char _pad[2] = {};
  void *_pad1 = nullptr;
};

/** #CurveModifierData.flag */
enum CurveModifierFlag {
  MOD_CURVE_INVERT_VGROUP = (1 << 0),
};

/** #CurveModifierData.defaxis */
enum CurveModifierDefaultAxis {
  MOD_CURVE_POSX = 1,
  MOD_CURVE_POSY = 2,
  MOD_CURVE_POSZ = 3,
  MOD_CURVE_NEGX = 4,
  MOD_CURVE_NEGY = 5,
  MOD_CURVE_NEGZ = 6,
};

struct CurveModifierData {
  ModifierData modifier;

  struct Object *object = nullptr;
  /** Optional vertex-group name. */
  char name[/*MAX_VGROUP_NAME*/ 64] = "";
  /** #CurveModifierDefaultAxis. Axis along which curve deforms. */
  short defaxis = MOD_CURVE_POSX;
  /** #CurveModifierFlag. */
  short flag = 0;
  char _pad[4] = {};
  void *_pad1 = nullptr;
};

/** #BuildModifierData.flag */
enum BuildModifierFlag {
  /** order of vertices is randomized */
  MOD_BUILD_FLAG_RANDOMIZE = (1 << 0),
  /** frame range is reversed, resulting in a deconstruction effect */
  MOD_BUILD_FLAG_REVERSE = (1 << 1),
};

struct BuildModifierData {
  ModifierData modifier;

  float start = 1.0f, length = 100.0f;
  /** #BuildModifierFlag. */
  short flag = 0;

  /** (bool) whether order of vertices is randomized - legacy files (for readfile conversion). */
  short randomize = 0;
  /** (int) random seed. */
  int seed = 0;
};

/** #MaskModifierData.mode */
enum MaskModifierMode {
  MOD_MASK_MODE_VGROUP = 0,
  MOD_MASK_MODE_ARM = 1,
};

/** #MaskModifierData.flag */
enum MaskModifierFlag {
  MOD_MASK_INV = (1 << 0),
  MOD_MASK_SMOOTH = (1 << 1),
};

/** Mask Modifier. */
struct MaskModifierData {
  ModifierData modifier;

  /** Armature to use to in place of hardcoded vgroup. */
  struct Object *ob_arm = nullptr;
  /** Name of vertex group to use to mask. */
  char vgroup[/*MAX_VGROUP_NAME*/ 64] = "";

  /** #MaskModifierMode. Using armature or hardcoded vgroup. */
  short mode = 0;
  /** #MaskModifierFlag. Flags for various things. */
  short flag = 0;
  float threshold = 0.0f;
  void *_pad1 = nullptr;
};

/** #ArrayModifierData.fit_type */
enum ArrayModifierFitType {
  MOD_ARR_FIXEDCOUNT = 0,
  MOD_ARR_FITLENGTH = 1,
  MOD_ARR_FITCURVE = 2,
};

/** #ArrayModifierData.offset_type */
enum ArrayModifierOffsetType {
  MOD_ARR_OFF_CONST = (1 << 0),
  MOD_ARR_OFF_RELATIVE = (1 << 1),
  MOD_ARR_OFF_OBJ = (1 << 2),
};

/** #ArrayModifierData.flags */
enum ArrayModifierFlag {
  MOD_ARR_MERGE = (1 << 0),
  MOD_ARR_MERGEFINAL = (1 << 1),
};

struct ArrayModifierData {
  ModifierData modifier;

  /** The object with which to cap the start of the array. */
  struct Object *start_cap = nullptr;
  /** The object with which to cap the end of the array. */
  struct Object *end_cap = nullptr;
  /** The curve object to use for #MOD_ARR_FITCURVE. */
  struct Object *curve_ob = nullptr;
  /** The object to use for object offset. */
  struct Object *offset_ob = nullptr;
  /**
   * A constant duplicate offset;
   * 1 means the duplicates are 1 unit apart.
   */
  float offset[3] = {1.0f, 0.0f, 0.0f};
  /**
   * A scaled factor for duplicate offsets;
   * 1 means the duplicates are 1 object-width apart.
   */
  float scale[3] = {1.0f, 0.0f, 0.0f};
  /** The length over which to distribute the duplicates. */
  float length = 0.0f;
  /** The limit below which to merge vertices in adjacent duplicates. */
  float merge_dist = 0.01f;
  /**
   * #ArrayModifierFitType.
   * Determines how duplicate count is calculated; one of:
   * - #MOD_ARR_FIXEDCOUNT -> fixed.
   * - #MOD_ARR_FITLENGTH  -> calculated to fit a set length.
   * - #MOD_ARR_FITCURVE   -> calculated to fit the length of a Curve object.
   */
  int fit_type = MOD_ARR_FIXEDCOUNT;
  /**
   * #ArrayModifierOffsetType.
   * Flags specifying how total offset is calculated; binary OR of:
   * - #MOD_ARR_OFF_CONST    -> total offset += offset.
   * - #MOD_ARR_OFF_RELATIVE -> total offset += relative * object width.
   * - #MOD_ARR_OFF_OBJ      -> total offset += offset_ob's matrix.
   * Total offset is the sum of the individual enabled offsets.
   */
  int offset_type = MOD_ARR_OFF_RELATIVE;
  /**
   * #ArrayModifierFlag.
   * General flags:
   * #MOD_ARR_MERGE -> merge vertices in adjacent duplicates.
   */
  int flags = 0;
  /** The number of duplicates to generate for #MOD_ARR_FIXEDCOUNT. */
  int count = 2;
  float uv_offset[2] = {0.0f, 0.0f};
};

/** #MirrorModifierData.flag */
enum MirrorModifierFlag {
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

struct MirrorModifierData {
  DNA_DEFINE_CXX_METHODS(MirrorModifierData)

  ModifierData modifier;

  /** Deprecated, use flag instead. */
  DNA_DEPRECATED short axis = 0;
  /** #MirrorModifierFlag. */
  short flag = MOD_MIR_AXIS_X | MOD_MIR_VGROUP;
  float tolerance = 0.001f;
  float bisect_threshold = 0.001f;

  /** Mirror modifier used to merge the old vertex into its new copy, which would break code
   * relying on access to the original geometry vertices. However, modifying this behavior to the
   * correct one (i.e. merging the copy vertices into their original sources) has several potential
   * effects on other modifiers and tools, so we need to keep that incorrect behavior for existing
   * modifiers, and only use the new correct one for new modifiers. */
  uint8_t use_correct_order_on_merge = true;

  char _pad[3] = {};
  float uv_offset[2] = {0.0f, 0.0f};
  float uv_offset_copy[2] = {0.0f, 0.0f};
  struct Object *mirror_ob = nullptr;
  void *_pad1 = nullptr;
};

/** #EdgeSplitModifierData.flags */
enum EdgeSplitModifierFlag {
  MOD_EDGESPLIT_FROMANGLE = (1 << 1),
  MOD_EDGESPLIT_FROMFLAG = (1 << 2),
};

struct EdgeSplitModifierData {
  ModifierData modifier;

  /** Angle above which edges should be split. */
  float split_angle = DEG2RADF(30.0f);
  /** #EdgeSplitModifierFlag. */
  int flags = MOD_EDGESPLIT_FROMANGLE | MOD_EDGESPLIT_FROMFLAG;
};

/** #BevelModifierData.flags and BevelModifierData.lim_flags */
enum BevelModifierFlag {
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
};

/** #BevelModifierData.val_flags (not used as flags any more) */
enum BevelModifierValFlag {
  MOD_BEVEL_AMT_OFFSET = 0,
  MOD_BEVEL_AMT_WIDTH = 1,
  MOD_BEVEL_AMT_DEPTH = 2,
  MOD_BEVEL_AMT_PERCENT = 3,
  MOD_BEVEL_AMT_ABSOLUTE = 4,
};

/** #BevelModifierData.profile_type */
enum BevelModifierProfileType {
  MOD_BEVEL_PROFILE_SUPERELLIPSE = 0,
  MOD_BEVEL_PROFILE_CUSTOM = 1,
};

/** #BevelModifierData.edge_flags */
enum BevelModifierEdgeFlag {
  MOD_BEVEL_MARK_SEAM = (1 << 0),
  MOD_BEVEL_MARK_SHARP = (1 << 1),
};

/** #BevelModifierData.face_str_mode */
enum BevelModifierFaceStrengthMode {
  MOD_BEVEL_FACE_STRENGTH_NONE = 0,
  MOD_BEVEL_FACE_STRENGTH_NEW = 1,
  MOD_BEVEL_FACE_STRENGTH_AFFECTED = 2,
  MOD_BEVEL_FACE_STRENGTH_ALL = 3,
};

/** #BevelModifier.miter_inner & #BevelModifier.miter_outer */
enum BevelModifierMiter {
  MOD_BEVEL_MITER_SHARP = 0,
  MOD_BEVEL_MITER_PATCH = 1,
  MOD_BEVEL_MITER_ARC = 2,
};

/** #BevelModifier.vmesh_method */
enum BevelModifierVMeshMethod {
  MOD_BEVEL_VMESH_ADJ = 0,
  MOD_BEVEL_VMESH_CUTOFF = 1,
};

/** #BevelModifier.affect_type */
enum BevelModifierAffectType {
  MOD_BEVEL_AFFECT_VERTICES = 0,
  MOD_BEVEL_AFFECT_EDGES = 1,
};

struct BevelModifierData {
  ModifierData modifier;

  /** The "raw" bevel value (distance/amount to bevel). */
  float value = 0.1f;
  /** The resolution (as originally coded, it is the number of recursive bevels). */
  int res = 1;
  /** #BevelModifierFlag. General option flags. */
  short flags = 0;
  /** #BevelModifierValFlag. Used to interpret the bevel value. */
  short val_flags = MOD_BEVEL_AMT_OFFSET;
  /** #BevelModifierProfileType. For the type and how we build the bevel's profile. */
  short profile_type = MOD_BEVEL_PROFILE_SUPERELLIPSE;
  /** #BevelModifierFlag. Flags to tell the tool how to limit the bevel. */
  short lim_flags = MOD_BEVEL_ANGLE;
  /** Flags to direct how edge weights are applied to verts. */
  short e_flags = 0;
  /** Material index if >= 0, else material inherited from surrounding faces. */
  short mat = -1;
  /** #BevelModifierEdgeFlag. */
  short edge_flags = 0;
  /** #BevelModifierFaceStrengthMode. */
  short face_str_mode = MOD_BEVEL_FACE_STRENGTH_NONE;
  /** #BevelModifierMiter. Patterns to use for mitering non-reflex and reflex miter edges */
  short miter_inner = MOD_BEVEL_MITER_SHARP;
  short miter_outer = MOD_BEVEL_MITER_SHARP;
  /** #BevelModifierVMeshMethod. The method to use for creating >2-way intersections */
  short vmesh_method = 0;
  /** #BevelModifierAffectType. Whether to affect vertices or edges. */
  char affect_type = MOD_BEVEL_AFFECT_EDGES;
  char _pad = {};
  /** Controls profile shape (0->1, .5 is round). */
  float profile = 0.5f;
  /** if the MOD_BEVEL_ANGLE is set,
   * this will be how "sharp" an edge must be before it gets beveled */
  float bevel_angle = DEG2RADF(30.0f);
  float spread = 0.1f;
  /** If the #MOD_BEVEL_VWEIGHT option is set, this will be the name of the vert group. */
  char defgrp_name[/*MAX_VGROUP_NAME*/ 64] = "";

  char _pad1[4] = {};
  /** Curve info for the custom profile */
  struct CurveProfile *custom_profile = nullptr;

  /** Custom bevel edge weight name. */
  char edge_weight_name[64] = "bevel_weight_edge";

  /** Custom bevel vertex weight name. */
  char vertex_weight_name[64] = "bevel_weight_vert";
};

/** #FluidModifierData.type */
enum FluidModifierType {
  MOD_FLUID_TYPE_DOMAIN = (1 << 0),
  MOD_FLUID_TYPE_FLOW = (1 << 1),
  MOD_FLUID_TYPE_EFFEC = (1 << 2),
};

struct FluidModifierData {
  ModifierData modifier;

  struct FluidDomainSettings *domain = nullptr;
  /** Inflow, outflow, smoke objects. */
  struct FluidFlowSettings *flow = nullptr;
  /** Effector objects (collision, guiding). */
  struct FluidEffectorSettings *effector = nullptr;
  float time = 0;
  /** #FluidModifierType. Domain, inflow, outflow, .... */
  int type = 0;
  void *_pad1 = nullptr;
};

/** #DisplaceModifierData.flag */
enum DisplaceModifierFlag {
  MOD_DISP_INVERT_VGROUP = (1 << 0),
};

/** #DisplaceModifierData.direction */
enum DisplaceModifierDirection {
  MOD_DISP_DIR_X = 0,
  MOD_DISP_DIR_Y = 1,
  MOD_DISP_DIR_Z = 2,
  MOD_DISP_DIR_NOR = 3,
  MOD_DISP_DIR_RGB_XYZ = 4,
  MOD_DISP_DIR_CLNOR = 5,
};

/** #DisplaceModifierData.texmapping */
enum DisplaceModifierTexMapping {
  MOD_DISP_MAP_LOCAL = 0,
  MOD_DISP_MAP_GLOBAL = 1,
  MOD_DISP_MAP_OBJECT = 2,
  MOD_DISP_MAP_UV = 3,
};

/** #DisplaceModifierData.space */
enum DisplaceModifierSpace {
  MOD_DISP_SPACE_LOCAL = 0,
  MOD_DISP_SPACE_GLOBAL = 1,
};

struct DisplaceModifierData {
  ModifierData modifier;

  /* Keep in sync with #MappingInfoModifierData. */

  struct Tex *texture = nullptr;
  struct Object *map_object = nullptr;
  char map_bone[/*MAXBONENAME*/ 64] = "";
  char uvlayer_name[/*MAX_CUSTOMDATA_LAYER_NAME*/ 68] = "";
  char _pad1[4] = {};
  int uvlayer_tmp = 0;
  /** #DisplaceModifierTexMapping. */
  int texmapping = 0;
  /* end MappingInfoModifierData */

  float strength = 1.0f;
  /** #DisplaceModifierDirection. */
  int direction = MOD_DISP_DIR_NOR;
  char defgrp_name[/*MAX_VGROUP_NAME*/ 64] = "";
  float midlevel = 0.5f;
  /** #DisplaceModifierSpace. */
  int space = MOD_DISP_SPACE_LOCAL;
  /** #DisplaceModifierFlag. */
  short flag = 0;
  char _pad2[6] = {};
};

#define MOD_UVPROJECT_MAXPROJECTORS 10

struct UVProjectModifierData {
  ModifierData modifier;
  /**
   * The objects which do the projecting.
   */
  struct Object *projectors[/*MOD_UVPROJECT_MAXPROJECTORS*/ 10] = {};
  char _pad2[4] = {};
  int projectors_num = 1;
  float aspectx = 1.0f, aspecty = 1.0f;
  float scalex = 1.0f, scaley = 1.0f;
  char uvlayer_name[/*MAX_CUSTOMDATA_LAYER_NAME*/ 68] = "";
  int uvlayer_tmp = 0;
};

enum DecimateModifierFlag {
  MOD_DECIM_FLAG_INVERT_VGROUP = (1 << 0),
  /** For collapse only. don't convert triangle pairs back to quads. */
  MOD_DECIM_FLAG_TRIANGULATE = (1 << 1),
  /** for dissolve only. collapse all verts between 2 faces */
  MOD_DECIM_FLAG_ALL_BOUNDARY_VERTS = (1 << 2),
  MOD_DECIM_FLAG_SYMMETRY = (1 << 3),
};

enum DecimateModifierMode {
  MOD_DECIM_MODE_COLLAPSE = 0,
  MOD_DECIM_MODE_UNSUBDIV = 1,
  /** called planar in the UI */
  MOD_DECIM_MODE_DISSOLVE = 2,
};

struct DecimateModifierData {
  ModifierData modifier;

  /** (mode == MOD_DECIM_MODE_COLLAPSE). */
  float percent = 1.0f;
  /** (mode == MOD_DECIM_MODE_UNSUBDIV). */
  short iter = 0;
  /** (mode == MOD_DECIM_MODE_DISSOLVE). */
  char delimit = 0;
  /** (mode == MOD_DECIM_MODE_COLLAPSE). */
  char symmetry_axis = 0;
  /** (mode == MOD_DECIM_MODE_DISSOLVE). */
  float angle = DEG2RADF(5.0f);

  char defgrp_name[/*MAX_VGROUP_NAME*/ 64] = "";
  float defgrp_factor = 1.0f;
  /** #DecimateModifierFlag. */
  short flag = 0;
  /** #DecimateModifierMode. */
  short mode = 0;

  /** runtime only. */
  int face_count = 0;
};

/** #SmoothModifierData.flag */
enum SmoothModifierFlag {
  MOD_SMOOTH_INVERT_VGROUP = (1 << 0),
  MOD_SMOOTH_X = (1 << 1),
  MOD_SMOOTH_Y = (1 << 2),
  MOD_SMOOTH_Z = (1 << 3),
};

struct SmoothModifierData {
  ModifierData modifier;
  float fac = 0.5f;
  char defgrp_name[/*MAX_VGROUP_NAME*/ 64] = "";
  /** #SmoothModifierFlag. */
  short flag = MOD_SMOOTH_X | MOD_SMOOTH_Y | MOD_SMOOTH_Z;
  short repeat = 1;
};

/** #CastModifierData.flag */
enum CastModifierFlag {
  /* And what bout (1 << 0) flag? ;) */
  MOD_CAST_INVERT_VGROUP = (1 << 0),
  MOD_CAST_X = (1 << 1),
  MOD_CAST_Y = (1 << 2),
  MOD_CAST_Z = (1 << 3),
  MOD_CAST_USE_OB_TRANSFORM = (1 << 4),
  MOD_CAST_SIZE_FROM_RADIUS = (1 << 5),
};

/** #CastModifierData.type */
enum CastModifierType {
  MOD_CAST_TYPE_SPHERE = 0,
  MOD_CAST_TYPE_CYLINDER = 1,
  MOD_CAST_TYPE_CUBOID = 2,
};

struct CastModifierData {
  ModifierData modifier;

  struct Object *object = nullptr;
  float fac = 0.5f;
  float radius = 0.0f;
  float size = 0.0f;
  char defgrp_name[/*MAX_VGROUP_NAME*/ 64] = "";
  /** #CastModifierFlag. */
  short flag = MOD_CAST_X | MOD_CAST_Y | MOD_CAST_Z | MOD_CAST_SIZE_FROM_RADIUS;
  /** #CastModifierType. Cast modifier projection type. */
  short type = MOD_CAST_TYPE_SPHERE;
  void *_pad1 = nullptr;
};

/** #WaveModifierData.flag */
enum WaveModifierFlag {
  MOD_WAVE_INVERT_VGROUP = (1 << 0),
  MOD_WAVE_X = (1 << 1),
  MOD_WAVE_Y = (1 << 2),
  MOD_WAVE_CYCL = (1 << 3),
  MOD_WAVE_NORM = (1 << 4),
  MOD_WAVE_NORM_X = (1 << 5),
  MOD_WAVE_NORM_Y = (1 << 6),
  MOD_WAVE_NORM_Z = (1 << 7),
};

struct WaveModifierData {
  ModifierData modifier;

  /* Keep in sync with #MappingInfoModifierData. */

  struct Tex *texture = nullptr;
  struct Object *map_object = nullptr;
  char map_bone[/*MAXBONENAME*/ 64] = "";
  char uvlayer_name[/*MAX_CUSTOMDATA_LAYER_NAME*/ 68] = "";
  char _pad1[4] = {};
  int uvlayer_tmp = 0;
  int texmapping = MOD_DISP_MAP_LOCAL;
  /* End MappingInfoModifierData. */

  struct Object *objectcenter = nullptr;
  char defgrp_name[/*MAX_VGROUP_NAME*/ 64] = "";

  /** #WaveModifierFlag. */
  short flag = MOD_WAVE_X | MOD_WAVE_Y | MOD_WAVE_CYCL | MOD_WAVE_NORM_X | MOD_WAVE_NORM_Y |
               MOD_WAVE_NORM_Z;
  char _pad2[2] = {};

  float startx = 0.0f, starty = 0.0f, height = 0.5f, width = 1.5f;
  float narrow = 1.5f, speed = 0.25f, damp = 10.0f, falloff = 0.0f;

  float timeoffs = 0.0f, lifetime = 0.0f;
  char _pad3[4] = {};
  void *_pad4 = nullptr;
};

struct ArmatureModifierData {
  ModifierData modifier;

  /** #eArmature_DeformFlag use instead of #bArmature.deformflag. */
  short deformflag = ARM_DEF_VGROUP, multi = 0.0f;
  char _pad2[4] = {};
  struct Object *object = nullptr;
  /** Stored input of previous modifier, for vertex-group blending. */
  float (*vert_coords_prev)[3] = {};
  char defgrp_name[/*MAX_VGROUP_NAME*/ 64] = "";
};

enum HookModifierFlag {
  MOD_HOOK_UNIFORM_SPACE = (1 << 0),
  MOD_HOOK_INVERT_VGROUP = (1 << 1),
};

/** \note same as #WarpModifierFalloff */
enum HookModifierFalloff {
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
};

struct HookModifierData {
  ModifierData modifier;

  struct Object *object = nullptr;
  /** Optional name of bone target. */
  char subtarget[/*MAX_NAME*/ 64] = "";

  /** #HookModifierFlag. */
  char flag = 0;
  /** Use enums from WarpModifier (exact same functionality). */
  char falloff_type = eHook_Falloff_Smooth;
  char _pad[6] = {};
  /** Matrix making current transform unmodified. */
  float parentinv[4][4] = _DNA_DEFAULT_UNIT_M4;
  /** Visualization of hook. */
  float cent[3] = {0.0f, 0.0f, 0.0f};
  /** If not zero, falloff is distance where influence zero. */
  float falloff = 0.0f;

  struct CurveMapping *curfalloff = nullptr;

  /** If NULL, it's using vertex-group. */
  int *indexar = nullptr;
  int indexar_num = 0;
  float force = 1.0f;
  /** Optional vertex-group name. */
  char name[/*MAX_VGROUP_NAME*/ 64] = "";
  void *_pad1 = nullptr;
};

struct SoftbodyModifierData {
  ModifierData modifier;
};

struct ClothModifierData {
  ModifierData modifier;

  /** The internal data structure for cloth. */
  struct Cloth *clothObject = nullptr;
  /** Definition is in DNA_cloth_types.h. */
  struct ClothSimSettings *sim_parms = nullptr;
  /** Definition is in DNA_cloth_types.h. */
  struct ClothCollSettings *coll_parms = nullptr;

  /**
   * PointCache can be shared with other instances of #ClothModifierData.
   * Inspect `modifier.flag & eModifierFlag_SharedCaches` to find out.
   */
  /** Definition is in DNA_object_force_types.h. */
  struct PointCache *point_cache = nullptr;
  struct ListBaseT<PointCache> ptcaches = {
    nullptr, nullptr
  };

  /** XXX: nasty hack, remove once hair can be separated from cloth modifier data. */
  struct ClothHairData *hairdata = nullptr;
  /** Grid geometry values of hair continuum. */
  float hair_grid_min[3] = {0.0f, 0.0f, 0.0f};
  float hair_grid_max[3] = {0.0f, 0.0f, 0.0f};
  int hair_grid_res[3] = {0, 0, 0};
  float hair_grid_cellsize = 0.0f;

  struct ClothSolverResult *solver_result = nullptr;
};

struct CollisionModifierData {
  ModifierData modifier;

  /** Position at the beginning of the frame. */
  float (*x)[3] = nullptr;
  /** Position at the end of the frame. */
  float (*xnew)[3] = nullptr;
  /** Unused at the moment, but was discussed during sprint. */
  float (*xold)[3] = nullptr;
  /** New position at the actual inter-frame step. */
  float (*current_xnew)[3] = nullptr;
  /** Position at the actual inter-frame step. */
  float (*current_x)[3] = nullptr;
  /** (xnew - x) at the actual inter-frame step. */
  float (*current_v)[3] = nullptr;

  int (*vert_tris)[3] = nullptr;

  unsigned int mvert_num = 0;
  unsigned int tri_num = 0;
  /** Cfra time of modifier. */
  float time_x = -1000.0f, time_xnew = -1000.0f;
  /** Collider doesn't move this frame, i.e. x[].co==xnew[].co. */
  char is_static = false;
  char _pad[7] = {};

  /** Bounding volume hierarchy for this cloth object. */
  struct BVHTree *bvhtree = nullptr;
};

struct SurfaceModifierData_Runtime {

  float (*vert_positions_prev)[3] = {};
  float (*vert_velocities)[3] = {};

  struct Mesh *mesh = nullptr;

  /** Bounding volume hierarchy of the mesh faces. */
  bke::BVHTreeFromMesh *bvhtree = nullptr;

  int cfra_prev = 0, verts_num = 0;
};

struct SurfaceModifierData {
  ModifierData modifier;

  SurfaceModifierData_Runtime runtime = {
      nullptr}; /* Include to avoid empty an struct (for MSVC). */
};

enum BooleanModifierMaterialMode {
  eBooleanModifierMaterialMode_Index = 0,
  eBooleanModifierMaterialMode_Transfer = 1,
};

/** #BooleanModifierData.operation */
enum BooleanModifierOp {
  eBooleanModifierOp_Intersect = 0,
  eBooleanModifierOp_Union = 1,
  eBooleanModifierOp_Difference = 2,
};

/** #BooleanModifierData.solver */
enum BooleanModifierSolver {
  eBooleanModifierSolver_Float = 0,
  eBooleanModifierSolver_Mesh_Arr = 1,
  eBooleanModifierSolver_Manifold = 2,
};

/** #BooleanModifierData.flag */
enum BooleanModifierFlag {
  eBooleanModifierFlag_Self = (1 << 0),
  eBooleanModifierFlag_Object = (1 << 1),
  eBooleanModifierFlag_Collection = (1 << 2),
  eBooleanModifierFlag_HoleTolerant = (1 << 3),
};

/** #BooleanModifierData.bm_flag (only used when #G_DEBUG is set). */
enum BooleanModifierBMeshFlag {
  eBooleanModifierBMeshFlag_BMesh_Separate = (1 << 0),
  eBooleanModifierBMeshFlag_BMesh_NoDissolve = (1 << 1),
  eBooleanModifierBMeshFlag_BMesh_NoConnectRegions = (1 << 2),
};

struct BooleanModifierData {
  ModifierData modifier;

  struct Object *object = nullptr;
  struct Collection *collection = nullptr;
  float double_threshold = 1e-6f;
  /** #BooleanModifierOp. */
  char operation = eBooleanModifierOp_Difference;
  /** #BooleanModifierSolver. */
  char solver = eBooleanModifierSolver_Mesh_Arr;
  /** #BooleanModifierMaterialMode. */
  char material_mode = 0;
  /** #BooleanModifierFlag. */
  char flag = eBooleanModifierFlag_Object;
  /** #BooleanModifierBMeshFlag. */
  char bm_flag = 0;
  char _pad[7] = {};
};

struct MDefInfluence {
  int vertex = 0;
  float weight = 0;
};

struct MDefCell {
  int offset = 0;
  int influences_num = 0;
};

enum MeshDeformModifierFlag {
  MOD_MDEF_INVERT_VGROUP = (1 << 0),
  MOD_MDEF_DYNAMIC_BIND = (1 << 1),
};

struct MeshDeformModifierData {
  ModifierData modifier;

  /** Mesh object. */
  struct Object *object = nullptr;
  /** Optional vertex-group name. */
  char defgrp_name[/*MAX_VGROUP_NAME*/ 64] = "";

  short gridsize = 5;
  /** #MeshDeformModifierFlag. */
  short flag = 0;
  char _pad[4] = {};

  /* result of static binding */
  /** Influences. */
  MDefInfluence *bindinfluences = nullptr;
  const ImplicitSharingInfoHandle *bindinfluences_sharing_info = nullptr;
  /** Offsets into influences array. */
  int *bindoffsets = nullptr;
  const ImplicitSharingInfoHandle *bindoffsets_sharing_info = nullptr;
  /** Coordinates that cage was bound with. */
  float *bindcagecos = nullptr;
  const ImplicitSharingInfoHandle *bindcagecos_sharing_info = nullptr;
  /** Total vertices in mesh and cage. */
  int verts_num = 0, cage_verts_num = 0;

  /* result of dynamic binding */
  /** Grid with dynamic binding cell points. */
  MDefCell *dyngrid = nullptr;
  const ImplicitSharingInfoHandle *dyngrid_sharing_info = nullptr;
  /** Dynamic binding vertex influences. */
  MDefInfluence *dyninfluences = nullptr;
  const ImplicitSharingInfoHandle *dyninfluences_sharing_info = nullptr;
  /** Is this vertex bound or not? */
  int *dynverts = nullptr;
  const ImplicitSharingInfoHandle *dynverts_sharing_info = nullptr;
  /** Size of the dynamic bind grid. */
  int dyngridsize = 0;
  /** Total number of vertex influences. */
  int influences_num = 0;
  /** Offset of the dynamic bind grid. */
  float dyncellmin[3] = {0.0f, 0.0f, 0.0f};
  /** Width of dynamic bind cell. */
  float dyncellwidth = 0.0f;
  /** Matrix of cage at binding time. */
  float bindmat[4][4] = _DNA_DEFAULT_UNIT_M4;

  /* deprecated storage */
  /** Deprecated inefficient storage. */
  float *bindweights = nullptr;
  /** Deprecated storage of cage coords. */
  float *bindcos = nullptr;

  /* runtime */
  void (*bindfunc)(struct Object *object,
                   struct MeshDeformModifierData *mmd,
                   struct Mesh *cagemesh,
                   float *vertexcos,
                   int verts_num,
                   float cagemat[4][4]) = nullptr;
};

enum ParticleSystemModifierFlag {
  eParticleSystemFlag_Pars = (1 << 0),
  eParticleSystemFlag_psys_updated = (1 << 1),
  eParticleSystemFlag_file_loaded = (1 << 2),
};

enum ParticleInstanceModifierFlag {
  eParticleInstanceFlag_Parents = (1 << 0),
  eParticleInstanceFlag_Children = (1 << 1),
  eParticleInstanceFlag_Path = (1 << 2),
  eParticleInstanceFlag_Unborn = (1 << 3),
  eParticleInstanceFlag_Alive = (1 << 4),
  eParticleInstanceFlag_Dead = (1 << 5),
  eParticleInstanceFlag_KeepShape = (1 << 6),
  eParticleInstanceFlag_UseSize = (1 << 7),
};

enum ParticleInstanceModifierSpace {
  eParticleInstanceSpace_World = 0,
  eParticleInstanceSpace_Local = 1,
};

struct ParticleSystemModifierData {
  ModifierData modifier;

  /**
   * \note Storing the particle system pointer here is very weak, as it prevents modifiers' data
   * copying to be self-sufficient (extra external code needs to ensure the pointer remains valid
   * when the modifier data is copied from one object to another). See e.g.
   * `BKE_object_copy_particlesystems` or `BKE_object_copy_modifier`.
   */
  struct ParticleSystem *psys = nullptr;
  /** Final Mesh - its topology may differ from orig mesh. */
  struct Mesh *mesh_final = nullptr;
  /** Original mesh that particles are attached to. */
  struct Mesh *mesh_original = nullptr;
  int totdmvert = 0, totdmedge = 0, totdmface = 0;
  /** #ParticleSystemModifierFlag. */
  short flag = 0;
  char _pad[2] = {};
  void *_pad1 = nullptr;
};

struct ParticleInstanceModifierData {
  ModifierData modifier;

  struct Object *ob = nullptr;
  short psys = 1;
  /** #ParticleInstanceModifierFlag. */
  short flag = eParticleInstanceFlag_Parents | eParticleInstanceFlag_Unborn |
               eParticleInstanceFlag_Alive | eParticleInstanceFlag_Dead;
  short axis = 2;
  /** #ParticleInstanceModifierSpace. */
  short space = eParticleInstanceSpace_World;
  float position = 1.0f, random_position = 0.0f;
  float rotation = 0.0f, random_rotation = 0.0f;
  float particle_amount = 1.0f, particle_offset = 0.0f;
  char index_layer_name[/*MAX_CUSTOMDATA_LAYER_NAME*/ 68] = "";
  char value_layer_name[/*MAX_CUSTOMDATA_LAYER_NAME*/ 68] = "";
  void *_pad1 = nullptr;
};

enum ExplodeModifierFlag {
  eExplodeFlag_CalcFaces = (1 << 0),
  eExplodeFlag_PaSize = (1 << 1),
  eExplodeFlag_EdgeCut = (1 << 2),
  eExplodeFlag_Unborn = (1 << 3),
  eExplodeFlag_Alive = (1 << 4),
  eExplodeFlag_Dead = (1 << 5),
  eExplodeFlag_INVERT_VGROUP = (1 << 6),
};

struct ExplodeModifierData {
  ModifierData modifier;

  int *facepa = nullptr;
  /** #ExplodeModifierFlag. */
  short flag = eExplodeFlag_Unborn | eExplodeFlag_Alive | eExplodeFlag_Dead;
  short vgroup = 0;
  float protect = 0.0f;
  char uvname[/*MAX_CUSTOMDATA_LAYER_NAME*/ 68] = "";
  char _pad1[4] = {};
  void *_pad2 = nullptr;
};

enum MultiresModifierFlag {
  eMultiresModifierFlag_ControlEdges = (1 << 0),
  /* DEPRECATED, only used for versioning. */
  eMultiresModifierFlag_PlainUv_DEPRECATED = (1 << 1),
  eMultiresModifierFlag_UseCrease = (1 << 2),
  eMultiresModifierFlag_UseCustomNormals = (1 << 3),
  eMultiresModifierFlag_UseSculptBaseMesh = (1 << 4),
};

struct MultiresModifierData {
  DNA_DEFINE_CXX_METHODS(MultiresModifierData)

  ModifierData modifier;

  char lvl = 0, sculptlvl = 0, renderlvl = 0, totlvl = 0;
  DNA_DEPRECATED char simple = 0;
  /** #MultiresModifierFlag. */
  char flags = eMultiresModifierFlag_UseCrease | eMultiresModifierFlag_ControlEdges;
  char _pad[2] = {};
  short quality = 4;
  short uv_smooth = SUBSURF_UV_SMOOTH_PRESERVE_BOUNDARIES;
  short boundary_smooth = SUBSURF_BOUNDARY_SMOOTH_ALL;
  char _pad2[2] = {};
};

/** DEPRECATED: only used for versioning. */
struct FluidsimModifierData {
  ModifierData modifier;

  /** Definition is in DNA_object_fluidsim_types.h. */
  struct FluidsimSettings *fss = nullptr;
  void *_pad1 = nullptr;
};

/** DEPRECATED: only used for versioning. */
struct SmokeModifierData {
  ModifierData modifier;

  /** Domain, inflow, outflow, .... */
  int type = 0;
  int _pad = {};
};

struct ShrinkwrapModifierData {
  ModifierData modifier;

  /** Shrink target. */
  struct Object *target = nullptr;
  /** Additional shrink target. */
  struct Object *auxTarget = nullptr;
  /** Optional vertex-group name. */
  char vgroup_name[/*MAX_VGROUP_NAME*/ 64] = "";
  /** Distance offset to keep from mesh/projection point. */
  float keepDist = 0.0f;
  /** Shrink type projection. */
  short shrinkType = MOD_SHRINKWRAP_NEAREST_SURFACE;
  /** Shrink options. */
  char shrinkOpts = MOD_SHRINKWRAP_PROJECT_ALLOW_POS_DIR;
  /** Shrink to surface mode. */
  char shrinkMode = 0;
  /** Limit the projection ray cast. */
  float projLimit = 0.0f;
  /** Axis to project over. */
  char projAxis = 0;

  /**
   * If using projection over vertex normal this controls the level of subsurface that must be
   * done before getting the vertex coordinates and normal.
   */
  char subsurfLevels = 0;

  char _pad[2] = {};
};

/** #SimpleDeformModifierData.flag */
enum SimpleDeformModifierFlag {
  MOD_SIMPLEDEFORM_FLAG_INVERT_VGROUP = (1 << 0),
};

enum SimpleDeformModifierMode {
  MOD_SIMPLEDEFORM_MODE_TWIST = 1,
  MOD_SIMPLEDEFORM_MODE_BEND = 2,
  MOD_SIMPLEDEFORM_MODE_TAPER = 3,
  MOD_SIMPLEDEFORM_MODE_STRETCH = 4,
};

enum SimpleDeformModifierLockAxis {
  MOD_SIMPLEDEFORM_LOCK_AXIS_X = (1 << 0),
  MOD_SIMPLEDEFORM_LOCK_AXIS_Y = (1 << 1),
  MOD_SIMPLEDEFORM_LOCK_AXIS_Z = (1 << 2),
};

struct SimpleDeformModifierData {
  ModifierData modifier;

  /** Object to control the origin of modifier space coordinates. */
  struct Object *origin = nullptr;
  /** Optional vertex-group name. */
  char vgroup_name[/*MAX_VGROUP_NAME*/ 64] = "";
  /** Factors to control simple deforms. */
  float factor = DEG2RADF(45.0f);
  /** Lower and upper limit. */
  float limit[2] = {0.0f, 1.0f};

  /** #SimpleDeformModifierMode. Deform function. */
  char mode = MOD_SIMPLEDEFORM_MODE_TWIST;
  /** Lock axis (for taper and stretch). */
  char axis = 0;
  /**
   * #SimpleDeformModifierLockAxis.
   * Axis to perform the deform on (default is X, but can be overridden by origin.
   */
  char deform_axis = 0;
  /** #SimpleDeformModifierFlag. */
  char flag = 0;

  void *_pad1 = nullptr;
};

struct ShapeKeyModifierData {
  ModifierData modifier;
};

/** #SolidifyModifierData.flag */
enum SolidifyModifierFlag {
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
enum SolidifyModifierMode {
  MOD_SOLIDIFY_MODE_EXTRUDE = 0,
  MOD_SOLIDIFY_MODE_NONMANIFOLD = 1,
};

/** #SolidifyModifierData.nonmanifold_offset_mode */
enum SolifyModifierNonManifoldOffsetMode {
  MOD_SOLIDIFY_NONMANIFOLD_OFFSET_MODE_FIXED = 0,
  MOD_SOLIDIFY_NONMANIFOLD_OFFSET_MODE_EVEN = 1,
  MOD_SOLIDIFY_NONMANIFOLD_OFFSET_MODE_CONSTRAINTS = 2,
};

/** #SolidifyModifierData.nonmanifold_boundary_mode */
enum SolidifyModifierNonManifoldBoundaryMode {
  MOD_SOLIDIFY_NONMANIFOLD_BOUNDARY_MODE_NONE = 0,
  MOD_SOLIDIFY_NONMANIFOLD_BOUNDARY_MODE_ROUND = 1,
  MOD_SOLIDIFY_NONMANIFOLD_BOUNDARY_MODE_FLAT = 2,
};

struct SolidifyModifierData {
  ModifierData modifier;

  /** Name of vertex group to use. */
  char defgrp_name[/*MAX_VGROUP_NAME*/ 64] = "";
  char shell_defgrp_name[64] = "";
  char rim_defgrp_name[64] = "";
  /** New surface offset level. */
  float offset = 0.01f;
  /** Midpoint of the offset. */
  float offset_fac = -1.0f;
  /**
   * Factor for the minimum weight to use when vertex-groups are used,
   * avoids 0.0 weights giving duplicate geometry.
   */
  float offset_fac_vg = 0.0f;
  /** Clamp offset based on surrounding geometry. */
  float offset_clamp = 0.0f;
  /** #SolidifyModifierMode. */
  char mode = MOD_SOLIDIFY_MODE_EXTRUDE;

  /** Variables for #MOD_SOLIDIFY_MODE_NONMANIFOLD. */
  /** #SolifyModifierNonManifoldOffsetMode. */
  char nonmanifold_offset_mode = MOD_SOLIDIFY_NONMANIFOLD_OFFSET_MODE_CONSTRAINTS;
  /** #SolidifyModifierNonManifoldBoundaryMode. */
  char nonmanifold_boundary_mode = MOD_SOLIDIFY_NONMANIFOLD_BOUNDARY_MODE_NONE;

  char _pad = {};
  float crease_inner = 0.0f;
  float crease_outer = 0.0f;
  float crease_rim = 0.0f;
  /** #SolidifyModifierFlag. */
  int flag = MOD_SOLIDIFY_RIM;
  short mat_ofs = 0;
  short mat_ofs_rim = 0;

  float merge_tolerance = 0.0001f;
  float bevel_convex = 0.0f;
};

enum ScrewModifierFlag {
  MOD_SCREW_NORMAL_FLIP = (1 << 0),
  MOD_SCREW_NORMAL_CALC = (1 << 1),
  MOD_SCREW_OBJECT_OFFSET = (1 << 2),
  // MOD_SCREW_OBJECT_ANGLE = (1 << 4),
  MOD_SCREW_SMOOTH_SHADING = (1 << 5),
  MOD_SCREW_UV_STRETCH_U = (1 << 6),
  MOD_SCREW_UV_STRETCH_V = (1 << 7),
  MOD_SCREW_MERGE = (1 << 8),
};

struct ScrewModifierData {
  ModifierData modifier;

  struct Object *ob_axis = nullptr;
  unsigned int steps = 16;
  unsigned int render_steps = 16;
  unsigned int iter = 1;
  float screw_ofs = 0.0f;
  float angle = 2.0f * M_PI;
  float merge_dist = 0.01f;
  /** #ScrewModifierFlag. */
  short flag = MOD_SCREW_SMOOTH_SHADING;
  char axis = 2;
  char _pad[5] = {};
  void *_pad1 = nullptr;
};

enum OceanModifierGeometryMode {
  MOD_OCEAN_GEOM_GENERATE = 0,
  MOD_OCEAN_GEOM_DISPLACE = 1,
  MOD_OCEAN_GEOM_SIM_ONLY = 2,
};

enum OceanModifierSpectrum {
  MOD_OCEAN_SPECTRUM_PHILLIPS = 0,
  MOD_OCEAN_SPECTRUM_PIERSON_MOSKOWITZ = 1,
  MOD_OCEAN_SPECTRUM_JONSWAP = 2,
  MOD_OCEAN_SPECTRUM_TEXEL_MARSEN_ARSLOE = 3,
};

enum OceanModifierFlag {
  MOD_OCEAN_GENERATE_FOAM = (1 << 0),
  MOD_OCEAN_GENERATE_NORMALS = (1 << 1),
  MOD_OCEAN_GENERATE_SPRAY = (1 << 2),
  MOD_OCEAN_INVERT_SPRAY = (1 << 3),
};

struct OceanModifierData {
  ModifierData modifier;

  struct Ocean *ocean = nullptr;
  struct OceanCache *oceancache = nullptr;

  /** Render resolution. */
  int resolution = 7;
  /** Viewport resolution for the non-render case. */
  int viewport_resolution = 7;

  int spatial_size = 50;

  float wind_velocity = 30.0f;

  float damp = 0.5f;
  float smallest_wave = 0.01f;
  float depth = 200.0f;

  float wave_alignment = 0.0f;
  float wave_direction = 0.0f;
  float wave_scale = 1.0f;

  float chop_amount = 1.0f;
  float foam_coverage = 0.0f;
  float time = 1.0f;

  /** #OceanModifierSpectrum. Spectrum being used. */
  int spectrum = MOD_OCEAN_SPECTRUM_PHILLIPS;

  /* Common JONSWAP parameters. */
  /**
   * This is the distance from a lee shore, called the fetch, or the distance
   * over which the wind blows with constant velocity.
   */
  float fetch_jonswap = 120.0f;
  float sharpen_peak_jonswap = 0.0f;

  int bakestart = 1;
  int bakeend = 250;

  char cachepath[/*FILE_MAX*/ 1024] = "";

  char foamlayername[/*MAX_CUSTOMDATA_LAYER_NAME*/ 68] = "";
  char spraylayername[/*MAX_CUSTOMDATA_LAYER_NAME*/ 68] = "";
  char cached = 0;
  /** #OceanModifierGeometryMode. */
  char geometry_mode = 0;

  /** #OceanModifierFlag. */
  char flag = 0;
  char _pad2 = {};

  short repeat_x = 1;
  short repeat_y = 1;

  int seed = 0;

  float size = 1.0f;

  float foam_fade = 0.98f;

  char _pad[4] = {};
};

/** #WarpModifierData.flag */
enum WarpModifierFlag {
  MOD_WARP_VOLUME_PRESERVE = (1 << 0),
  MOD_WARP_INVERT_VGROUP = (1 << 1),
};

/** \note same as #HookModifierFalloff. */
enum WarpModifierFalloff {
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
};

struct WarpModifierData {
  ModifierData modifier;

  /* Keep in sync with #MappingInfoModifierData. */

  struct Tex *texture = nullptr;
  struct Object *map_object = nullptr;
  char map_bone[/*MAXBONENAME*/ 64] = "";
  char uvlayer_name[/*MAX_CUSTOMDATA_LAYER_NAME*/ 68] = "";
  char _pad1[4] = {};
  int uvlayer_tmp = 0;
  int texmapping = 0;
  /* End #MappingInfoModifierData. */

  struct Object *object_from = nullptr;
  struct Object *object_to = nullptr;
  /** Optional name of bone target. */
  char bone_from[/*MAX_NAME*/ 64] = "";
  /** Optional name of bone target. */
  char bone_to[/*MAX_NAME*/ 64] = "";

  struct CurveMapping *curfalloff = nullptr;
  /** Optional vertex-group name. */
  char defgrp_name[/*MAX_VGROUP_NAME*/ 64] = "";
  float strength = 1.0f;
  float falloff_radius = 1.0f;
  /** #WarpModifierFlag. */
  char flag = 0;
  char falloff_type = eWarp_Falloff_Smooth;
  char _pad2[6] = {};
  void *_pad3 = nullptr;
};

/* Defines common to all WeightVG modifiers. */

/** #WeightVGEdit.edit_flags */
enum WeigthVGEditModifierEditFlags {
  MOD_WVG_EDIT_WEIGHTS_NORMALIZE = (1 << 0),
  MOD_WVG_INVERT_FALLOFF = (1 << 1),
  MOD_WVG_EDIT_INVERT_VGROUP_MASK = (1 << 2),
  /** Add vertices with higher weight than threshold to vgroup. */
  MOD_WVG_EDIT_ADD2VG = (1 << 3),
  /** Remove vertices with lower weight than threshold from vgroup. */
  MOD_WVG_EDIT_REMFVG = (1 << 4),
};

/** #WeightVGMixModifierData.mix_mode (how second vgroup's weights affect first ones). */
enum WeightVGMixModifierMixMode {
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
};

/** #WeightVGMixModifierData.mix_set (what vertices to affect). */
enum WeightVGMixModifierMixSet {
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

/** #WeightVGMixModifierData.flag */
enum WeightVGMixModifierFlag {
  MOD_WVG_MIX_INVERT_VGROUP_MASK = (1 << 0),
  MOD_WVG_MIX_WEIGHTS_NORMALIZE = (1 << 1),
  MOD_WVG_MIX_INVERT_VGROUP_A = (1 << 2),
  MOD_WVG_MIX_INVERT_VGROUP_B = (1 << 3),
};

enum GreasePencilWeightProximityFlag {
  MOD_GREASE_PENCIL_WEIGHT_PROXIMITY_INVERT_OUTPUT = (1 << 0),
  MOD_GREASE_PENCIL_WEIGHT_PROXIMITY_MULTIPLY_DATA = (1 << 1),
};

/** #WeightVGProximityModifierData.proximity_mode */
enum WeightVGProximityModifierProximityMode {
  MOD_WVG_PROXIMITY_OBJECT = 1,   /* source vertex to other location */
  MOD_WVG_PROXIMITY_GEOMETRY = 2, /* source vertex to other geometry */
};

/** #WeightVGProximityModifierData.proximity_flags */
enum WeightVGProximityModifierFlag {
  /* Use nearest vertices of target obj, in MOD_WVG_PROXIMITY_GEOMETRY mode. */
  MOD_WVG_PROXIMITY_GEOM_VERTS = (1 << 0),
  /* Use nearest edges of target obj, in MOD_WVG_PROXIMITY_GEOMETRY mode. */
  MOD_WVG_PROXIMITY_GEOM_EDGES = (1 << 1),
  /* Use nearest faces of target obj, in MOD_WVG_PROXIMITY_GEOMETRY mode. */
  MOD_WVG_PROXIMITY_GEOM_FACES = (1 << 2),
  MOD_WVG_PROXIMITY_INVERT_VGROUP_MASK = (1 << 3),
  MOD_WVG_PROXIMITY_INVERT_FALLOFF = (1 << 4),
  MOD_WVG_PROXIMITY_WEIGHTS_NORMALIZE = (1 << 5),
};

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
enum WeightVGProximityModifierMaskTexChannel {
  MOD_WVG_MASK_TEX_USE_INT = 1,
  MOD_WVG_MASK_TEX_USE_RED = 2,
  MOD_WVG_MASK_TEX_USE_GREEN = 3,
  MOD_WVG_MASK_TEX_USE_BLUE = 4,
  MOD_WVG_MASK_TEX_USE_HUE = 5,
  MOD_WVG_MASK_TEX_USE_SAT = 6,
  MOD_WVG_MASK_TEX_USE_VAL = 7,
  MOD_WVG_MASK_TEX_USE_ALPHA = 8,
};

struct WeightVGEditModifierData {
  ModifierData modifier;

  /** Name of vertex group to edit. */
  char defgrp_name[/*MAX_VGROUP_NAME*/ 64] = "";

  /** #WeigthVGEditModifierEditFlags. */
  short edit_flags = 0;
  /** Using MOD_WVG_MAPPING_* defines. */
  short falloff_type = MOD_WVG_MAPPING_NONE;
  /** Weight for vertices not in vgroup. */
  float default_weight = 0.0f;

  /* Mapping stuff. */
  /** The custom mapping curve. */
  struct CurveMapping *cmap_curve = nullptr;

  /* The add/remove vertices weight thresholds. */
  float add_threshold = 0.01f, rem_threshold = 0.01f;

  /* Masking options. */
  /** The global "influence", if no vgroup nor tex is used as mask. */
  float mask_constant = 1.0f;
  /** Name of mask vertex group from which to get weight factors. */
  char mask_defgrp_name[/*MAX_VGROUP_NAME*/ 64] = "";

  /* Texture masking. */
  /** Which channel to use as weight/mask. */
  int mask_tex_use_channel = MOD_WVG_MASK_TEX_USE_INT;
  /** The texture. */
  struct Tex *mask_texture = nullptr;
  /** Name of the map object. */
  struct Object *mask_tex_map_obj = nullptr;
  /** Name of the map bone. */
  char mask_tex_map_bone[/*MAXBONENAME*/ 64] = "";
  /** How to map the texture (using MOD_DISP_MAP_* enums). */
  int mask_tex_mapping = MOD_DISP_MAP_LOCAL;
  /** Name of the UV map. */
  char mask_tex_uvlayer_name[/*MAX_CUSTOMDATA_LAYER_NAME*/ 68] = "";

  /* Padding... */
  void *_pad1 = nullptr;
};

struct WeightVGMixModifierData {
  ModifierData modifier;

  /** Name of vertex group to modify/weight. */
  char defgrp_name_a[/*MAX_VGROUP_NAME*/ 64] = "";
  /** Name of other vertex group to mix in. */
  char defgrp_name_b[/*MAX_VGROUP_NAME*/ 64] = "";
  /** Default weight value for first vgroup. */
  float default_weight_a = 0.0f;
  /** Default weight value to mix in. */
  float default_weight_b = 0.0f;
  /** #WeightVGMixModifierMixMode. How second vgroups weights affect first ones. */
  char mix_mode = MOD_WVG_MIX_SET;
  /** #WeightVGMixModifierMixSet. What vertices to affect. */
  char mix_set = MOD_WVG_SET_AND;

  char _pad0[6] = {};

  /* Masking options. */
  /** The global "influence", if no vgroup nor tex is used as mask. */
  float mask_constant = 1.0f;
  /** Name of mask vertex group from which to get weight factors. */
  char mask_defgrp_name[/*MAX_VGROUP_NAME*/ 64] = "";

  /* Texture masking. */
  /** Which channel to use as weightf. */
  int mask_tex_use_channel = MOD_WVG_MASK_TEX_USE_INT;
  /** The texture. */
  struct Tex *mask_texture = nullptr;
  /** Name of the map object. */
  struct Object *mask_tex_map_obj = nullptr;
  /** Name of the map bone. */
  char mask_tex_map_bone[/*MAXBONENAME*/ 64] = "";
  /** How to map the texture. */
  int mask_tex_mapping = MOD_DISP_MAP_LOCAL;
  /** Name of the UV map. */
  char mask_tex_uvlayer_name[/*MAX_CUSTOMDATA_LAYER_NAME*/ 68] = "";
  char _pad1[4] = {};

  /** #WeightVGMixModifierFlag. */
  char flag = 0;

  /* Padding... */
  char _pad2[3] = {};
};

struct WeightVGProximityModifierData {
  ModifierData modifier;

  /** Name of vertex group to modify/weight. */
  char defgrp_name[/*MAX_VGROUP_NAME*/ 64] = "";

  /* Mapping stuff. */
  /** The custom mapping curve. */
  struct CurveMapping *cmap_curve = nullptr;

  /** #WeightVGProximityModifierProximityMode. Modes of proximity weighting. */
  int proximity_mode = MOD_WVG_PROXIMITY_OBJECT;
  /** #WeightVGProximityModifierFlag. Options for proximity weighting. */
  int proximity_flags = MOD_WVG_PROXIMITY_GEOM_VERTS;

  /* Target object from which to calculate vertices distances. */
  struct Object *proximity_ob_target = nullptr;

  /* Masking options. */
  /** The global "influence", if no vgroup nor tex is used as mask. */
  float mask_constant = 1.0f;
  /** Name of mask vertex group from which to get weight factors. */
  char mask_defgrp_name[/*MAX_VGROUP_NAME*/ 64] = "";

  /* Texture masking. */
  /** #WeightVGProximityModifierMaskTexChannel. Which channel to use as weightf. */
  int mask_tex_use_channel = MOD_WVG_MASK_TEX_USE_INT;
  /** The texture. */
  struct Tex *mask_texture = nullptr;
  /** Name of the map object. */
  struct Object *mask_tex_map_obj = nullptr;
  /** Name of the map bone. */
  char mask_tex_map_bone[/*MAXBONENAME*/ 64] = "";
  /** How to map the texture. */
  int mask_tex_mapping = MOD_DISP_MAP_LOCAL;
  /** Name of the UV Map. */
  char mask_tex_uvlayer_name[/*MAX_CUSTOMDATA_LAYER_NAME*/ 68] = "";
  char _pad1[4] = {};

  /** Distances mapping to 0.0/1.0 weights. */
  float min_dist = 0.0f, max_dist = 1.0f;

  /* Put here to avoid breaking existing struct... */
  /**
   * Mapping modes (using MOD_WVG_MAPPING_* enums). */
  short falloff_type = MOD_WVG_MAPPING_NONE;

  /* Padding... */
  char _pad0[2] = {};
};

/** #DynamicPaintModifierData.type */
enum DynamicPaintModifierType {
  MOD_DYNAMICPAINT_TYPE_CANVAS = (1 << 0),
  MOD_DYNAMICPAINT_TYPE_BRUSH = (1 << 1),
};

struct DynamicPaintModifierData {
  ModifierData modifier;

  struct DynamicPaintCanvasSettings *canvas = nullptr;
  struct DynamicPaintBrushSettings *brush = nullptr;
  /** #DynamicPaintModifierType. UI display: canvas / brush. */
  int type = MOD_DYNAMICPAINT_TYPE_CANVAS;
  char _pad[4] = {};
};

/** Remesh modifier. */
enum eRemeshModifierFlags {
  MOD_REMESH_FLOOD_FILL = (1 << 0),
  MOD_REMESH_SMOOTH_SHADING = (1 << 1),
};

enum eRemeshModifierMode {
  /* blocky */
  MOD_REMESH_CENTROID = 0,
  /* smooth */
  MOD_REMESH_MASS_POINT = 1,
  /* keeps sharp edges */
  MOD_REMESH_SHARP_FEATURES = 2,
  /* Voxel remesh */
  MOD_REMESH_VOXEL = 3,
};

struct RemeshModifierData {
  ModifierData modifier;

  /** Flood-fill option, controls how small components can be before they are removed. */
  float threshold = 1.0f;

  /* ratio between size of model and grid */
  float scale = 0.9f;

  float hermite_num = 1.0f;

  /* octree depth */
  char depth = 4;
  /** #RemeshModifierFlags. */
  char flag = MOD_REMESH_FLOOD_FILL;
  /** #eRemeshModifierMode. */
  char mode = MOD_REMESH_VOXEL;
  char _pad = {};

  /* OpenVDB Voxel remesh properties. */
  float voxel_size = 0.1f;
  float adaptivity = 0.0f;
};

/** #SkinModifierData.symmetry_axes */
enum SkinModifierSymmetryAxis {
  MOD_SKIN_SYMM_X = (1 << 0),
  MOD_SKIN_SYMM_Y = (1 << 1),
  MOD_SKIN_SYMM_Z = (1 << 2),
};

/** #SkinModifierData.flag */
enum SkinModifierFlag {
  MOD_SKIN_SMOOTH_SHADING = 1,
};

/** Skin modifier. */
struct SkinModifierData {
  ModifierData modifier;

  float branch_smoothing = 0.0f;

  /** #SkinModifierFlag. */
  char flag = 0;

  /** #SkinModifierSymmetryAxis. */
  char symmetry_axes = MOD_SKIN_SYMM_X;

  char _pad[2] = {};
};

/** #TriangulateModifierData.flag */
enum TriangulateModifierFlag {
#ifdef DNA_DEPRECATED_ALLOW
  MOD_TRIANGULATE_BEAUTY = (1 << 0), /* deprecated */
#endif
  MOD_TRIANGULATE_KEEP_CUSTOMLOOP_NORMALS = 1 << 1,
};

/** #TriangulateModifierData.ngon_method triangulate method (N-gons). */
enum TriangulateModifierNgonMethod {
  MOD_TRIANGULATE_NGON_BEAUTY = 0,
  MOD_TRIANGULATE_NGON_EARCLIP = 1,
};

/** #TriangulateModifierData.quad_method triangulate method (quads). */
enum TriangulateModifierQuadMethod {
  MOD_TRIANGULATE_QUAD_BEAUTY = 0,
  MOD_TRIANGULATE_QUAD_FIXED = 1,
  MOD_TRIANGULATE_QUAD_ALTERNATE = 2,
  MOD_TRIANGULATE_QUAD_SHORTEDGE = 3,
  MOD_TRIANGULATE_QUAD_LONGEDGE = 4,
};

/** Triangulate modifier. */
struct TriangulateModifierData {
  ModifierData modifier;

  /** #TriangulateModifierFlag. */
  int flag = 0;
  /** #TriangulateModifierQuadMethod. */
  int quad_method = MOD_TRIANGULATE_QUAD_SHORTEDGE;
  /** #TriangulateModifierNgonMethod. */
  int ngon_method = MOD_TRIANGULATE_NGON_BEAUTY;
  int min_vertices = 4;
};

/** #LaplacianSmoothModifierData.flag */
enum LaplacianSmoothModifierFlag {
  MOD_LAPLACIANSMOOTH_X = (1 << 1),
  MOD_LAPLACIANSMOOTH_Y = (1 << 2),
  MOD_LAPLACIANSMOOTH_Z = (1 << 3),
  MOD_LAPLACIANSMOOTH_PRESERVE_VOLUME = (1 << 4),
  MOD_LAPLACIANSMOOTH_NORMALIZED = (1 << 5),
  MOD_LAPLACIANSMOOTH_INVERT_VGROUP = (1 << 6),
};

struct LaplacianSmoothModifierData {
  ModifierData modifier;

  float lambda = 0.01f, lambda_border = 0.01f;
  char _pad1[4] = {};
  char defgrp_name[/*MAX_VGROUP_NAME*/ 64] = "";
  /** #LaplacianSmoothModifierFlag. */
  short flag = MOD_LAPLACIANSMOOTH_X | MOD_LAPLACIANSMOOTH_Y | MOD_LAPLACIANSMOOTH_Z |
               MOD_LAPLACIANSMOOTH_PRESERVE_VOLUME | MOD_LAPLACIANSMOOTH_NORMALIZED;
  short repeat = 1;
};

enum CorrectiveSmoothModifierType {
  MOD_CORRECTIVESMOOTH_SMOOTH_SIMPLE = 0,
  MOD_CORRECTIVESMOOTH_SMOOTH_LENGTH_WEIGHT = 1,
};

enum CorrectiveSmoothRestSource {
  MOD_CORRECTIVESMOOTH_RESTSOURCE_ORCO = 0,
  MOD_CORRECTIVESMOOTH_RESTSOURCE_BIND = 1,
};

/** #CorrectiveSmoothModifierData.flag */
enum CorrectiveSmoothModifierFlag {
  MOD_CORRECTIVESMOOTH_INVERT_VGROUP = (1 << 0),
  MOD_CORRECTIVESMOOTH_ONLY_SMOOTH = (1 << 1),
  MOD_CORRECTIVESMOOTH_PIN_BOUNDARY = (1 << 2),
};

struct CorrectiveSmoothDeltaCache {
  /**
   * Delta's between the original positions and the smoothed positions,
   * calculated loop-tangent and which is accumulated into the vertex it uses.
   * (run-time only).
   */
  float (*deltas)[3] = {};
  unsigned int deltas_num = 0;

  /* Value of settings when creating the cache.
   * These are used to check if the cache should be recomputed. */
  float lambda = 0, scale = 0;
  short repeat = 0, flag = 0;
  char smooth_type = 0, rest_source = 0;
  char _pad[6] = {};
};

struct CorrectiveSmoothModifierData {
  ModifierData modifier;

  /* positions set during 'bind' operator
   * use for MOD_CORRECTIVESMOOTH_RESTSOURCE_BIND */
  float (*bind_coords)[3] = nullptr;
  const ImplicitSharingInfoHandle *bind_coords_sharing_info = nullptr;

  /* NOTE: -1 is used to bind. */
  unsigned int bind_coords_num = 0;

  float lambda = 0.5f, scale = 1.0f;
  short repeat = 5;
  /** #CorrectiveSmoothModifierFlag. */
  short flag = 0;
  /** #CorrectiveSmoothModifierType. */
  char smooth_type = MOD_CORRECTIVESMOOTH_SMOOTH_SIMPLE;
  /** #CorrectiveSmoothRestSource. */
  char rest_source = 0;
  char _pad[6] = {};

  char defgrp_name[/*MAX_VGROUP_NAME*/ 64] = "";

  /* runtime-only cache */
  CorrectiveSmoothDeltaCache delta_cache;
};

/** #UVWarpModifierData.flag */
enum UVWarpModifierFlag {
  MOD_UVWARP_INVERT_VGROUP = 1 << 0,
};

struct UVWarpModifierData {
  ModifierData modifier;

  char axis_u = 0, axis_v = 1;
  /** #UVWarpModifierFlag. */
  short flag = 0;
  /** Used for rotate/scale. */
  float center[2] = {0.5f, 0.5f};

  float offset[2] = {0.0f, 0.0f};
  float scale[2] = {1.0f, 1.0f};
  float rotation = 0.0f;

  /** Source. */
  struct Object *object_src = nullptr;
  /** Optional name of bone target. */
  char bone_src[/*MAX_NAME*/ 64] = "";
  /** Target. */
  struct Object *object_dst = nullptr;
  /** Optional name of bone target. */
  char bone_dst[/*MAX_NAME*/ 64] = "";

  /** Optional vertex-group name. */
  char vgroup_name[64] = "";
  char uvlayer_name[/*MAX_CUSTOMDATA_LAYER_NAME*/ 68] = "";
  char _pad[4] = {};
};

/** #MeshCacheModifierData.flag */
enum MeshCacheModifierFlag {
  MOD_MESHCACHE_INVERT_VERTEX_GROUP = 1 << 0,
};

enum MeshCacheModifierType {
  MOD_MESHCACHE_TYPE_MDD = 1,
  MOD_MESHCACHE_TYPE_PC2 = 2,
};

enum MeshCacheModifierDeformMode {
  MOD_MESHCACHE_DEFORM_OVERWRITE = 0,
  MOD_MESHCACHE_DEFORM_INTEGRATE = 1,
};

enum MeshCacheModifierInterpolation {
  MOD_MESHCACHE_INTERP_NONE = 0,
  MOD_MESHCACHE_INTERP_LINEAR = 1,
  // MOD_MESHCACHE_INTERP_CARDINAL = 2,
};

enum MeshCacheModifierTimeMode {
  MOD_MESHCACHE_TIME_FRAME = 0,
  MOD_MESHCACHE_TIME_SECONDS = 1,
  MOD_MESHCACHE_TIME_FACTOR = 2,
};

enum MeshCacheModifierPlayMode {
  MOD_MESHCACHE_PLAY_CFEA = 0,
  MOD_MESHCACHE_PLAY_EVAL = 1,
};

enum MeshCacheModifierFlipAxis {
  MOD_MESHCACHE_FLIP_AXIS_X = 1 << 0,
  MOD_MESHCACHE_FLIP_AXIS_Y = 1 << 1,
  MOD_MESHCACHE_FLIP_AXIS_Z = 1 << 2,
};

/** Mesh cache modifier. */
struct MeshCacheModifierData {
  ModifierData modifier;

  /** #MeshCacheModifierFlag. */
  char flag = 0;
  /** #MeshCacheModifierType. File format. */
  char type = MOD_MESHCACHE_TYPE_MDD;
  /** #MeshCacheModifierTimeMode. */
  char time_mode = 0;
  /** #MeshCacheModifierPlayMode. */
  char play_mode = 0;

  /* axis conversion */
  char forward_axis = 1;
  char up_axis = 2;
  /** #MeshCacheModifierFlipAxis. */
  char flip_axis = 0;

  /** #MeshCacheModifierInterpolation. */
  char interp = MOD_MESHCACHE_INTERP_LINEAR;

  float factor = 1.0f;
  /** #MeshCacheModifierDeformMode. */
  char deform_mode = 0.0f;
  char defgrp_name[64] = "";
  char _pad[7] = {};

  /* play_mode == MOD_MESHCACHE_PLAY_CFEA */
  float frame_start = 0.0f;
  float frame_scale = 1.0f;

  /* play_mode == MOD_MESHCACHE_PLAY_EVAL */
  /* we could use one float for all these but their purpose is very different */
  float eval_frame = 0.0f;
  float eval_time = 0.0f;
  float eval_factor = 0.0f;

  char filepath[/*FILE_MAX*/ 1024] = "";
};

/** #LaplacianDeformModifierData.flag */
enum LaplacianDeformModifierFlag {
  MOD_LAPLACIANDEFORM_BIND = 1 << 0,
  MOD_LAPLACIANDEFORM_INVERT_VGROUP = 1 << 1,
};

struct LaplacianDeformModifierData {
  ModifierData modifier;
  char anchor_grp_name[/*MAX_VGROUP_NAME*/ 64] = "";
  int verts_num = 0, repeat = 1;
  float *vertexco = nullptr;
  const ImplicitSharingInfoHandle *vertexco_sharing_info = nullptr;
  /** Runtime only. */
  void *cache_system = nullptr;
  /** #LaplacianDeformModifierFlag. */
  short flag = 0;
  char _pad[6] = {};
};

enum WireframeModifierFlag {
  MOD_WIREFRAME_INVERT_VGROUP = (1 << 0),
  MOD_WIREFRAME_REPLACE = (1 << 1),
  MOD_WIREFRAME_BOUNDARY = (1 << 2),
  MOD_WIREFRAME_OFS_EVEN = (1 << 3),
  MOD_WIREFRAME_OFS_RELATIVE = (1 << 4),
  MOD_WIREFRAME_CREASE = (1 << 5),
};

/**
 * \note many of these options match 'solidify'.
 */
struct WireframeModifierData {
  ModifierData modifier;
  char defgrp_name[/*MAX_VGROUP_NAME*/ 64] = "";
  float offset = 0.02f;
  float offset_fac = 0.0f;
  float offset_fac_vg = 0.0f;
  float crease_weight = 1.0f;
  /** #WireframeModifierFlag. */
  short flag = MOD_WIREFRAME_REPLACE | MOD_WIREFRAME_OFS_EVEN;
  short mat_ofs = 0;
  char _pad[4] = {};
};

/** #WeldModifierData.flag */
enum WeldModifierFlag {
  MOD_WELD_INVERT_VGROUP = (1 << 0),
  MOD_WELD_LOOSE_EDGES = (1 << 1),
};

/** #WeldModifierData.mode */
enum WeldModifierMode {
  MOD_WELD_MODE_ALL = 0,
  MOD_WELD_MODE_CONNECTED = 1,
};

struct WeldModifierData {
  ModifierData modifier;

  /* The limit below which to merge vertices. */
  float merge_dist = 0.001f;
  /** Name of vertex group to use to mask. */
  char defgrp_name[/*MAX_VGROUP_NAME*/ 64] = "";

  /** #WeldModifierMode. */
  char mode = MOD_WELD_MODE_ALL;
  /** #WeldModifierFlag. */
  char flag = 0;
  char _pad[2] = {};
};

/** #DataTransferModifierData.flags */
enum DataTransferModifierFlag {
  MOD_DATATRANSFER_OBSRC_TRANSFORM = 1 << 0,
  MOD_DATATRANSFER_MAP_MAXDIST = 1 << 1,
  MOD_DATATRANSFER_INVERT_VGROUP = 1 << 2,

  /* Only for UI really. */
  MOD_DATATRANSFER_USE_VERT = 1 << 28,
  MOD_DATATRANSFER_USE_EDGE = 1 << 29,
  MOD_DATATRANSFER_USE_LOOP = 1 << 30,
  MOD_DATATRANSFER_USE_POLY = 1u << 31,
};

struct DataTransferModifierData {
  ModifierData modifier;

  struct Object *ob_source = nullptr;

  /** See DT_TYPE_ enum in ED_object.hh. */
  int data_types = 0;

  /* See MREMAP_MODE_ enum in BKE_mesh_mapping.hh */
  int vmap_mode = MREMAP_MODE_VERT_NEAREST;
  int emap_mode = MREMAP_MODE_EDGE_NEAREST;
  int lmap_mode = MREMAP_MODE_LOOP_NEAREST_POLYNOR;
  int pmap_mode = MREMAP_MODE_POLY_NEAREST;

  float map_max_distance = 1.0f;
  float map_ray_radius = 0.0f;
  float islands_precision = 0.0f;

  char _pad1[4] = {};

  /** See DT_FROMLAYERS_ enum in ED_object.hh. */
  int layers_select_src[/*DT_MULTILAYER_INDEX_MAX*/ 5] = {
      DT_LAYERS_ALL_SRC, DT_LAYERS_ALL_SRC, DT_LAYERS_ALL_SRC, DT_LAYERS_ALL_SRC};
  /** See DT_TOLAYERS_ enum in ED_object.hh. */
  int layers_select_dst[/*DT_MULTILAYER_INDEX_MAX*/ 5] = {
      DT_LAYERS_NAME_DST, DT_LAYERS_NAME_DST, DT_LAYERS_NAME_DST, DT_LAYERS_NAME_DST};

  /** See CDT_MIX_ enum in BKE_customdata.hh. */
  int mix_mode = CDT_MIX_TRANSFER;
  float mix_factor = 0.0f;
  char defgrp_name[/*MAX_VGROUP_NAME*/ 64] = "";

  /** #DataTransferModifierFlag. */
  int flags = MOD_DATATRANSFER_OBSRC_TRANSFORM;
  void *_pad2 = nullptr;
};

/** #NormalEditModifierData.mode */
enum NormalEditModifierMode {
  MOD_NORMALEDIT_MODE_RADIAL = 0,
  MOD_NORMALEDIT_MODE_DIRECTIONAL = 1,
};

/** #NormalEditModifierData.flag */
enum NormalEditModifierFlag {
  MOD_NORMALEDIT_INVERT_VGROUP = (1 << 0),
  MOD_NORMALEDIT_USE_DIRECTION_PARALLEL = (1 << 1),
  MOD_NORMALEDIT_NO_POLYNORS_FIX = (1 << 2),
};

/** #NormalEditModifierData.mix_mode */
enum NormalEditModifierMixMode {
  MOD_NORMALEDIT_MIX_COPY = 0,
  MOD_NORMALEDIT_MIX_ADD = 1,
  MOD_NORMALEDIT_MIX_SUB = 2,
  MOD_NORMALEDIT_MIX_MUL = 3,
};

/** Normal Edit modifier. */
struct NormalEditModifierData {
  ModifierData modifier;
  char defgrp_name[/*MAX_VGROUP_NAME*/ 64] = "";
  /** Source of normals, or center of ellipsoid. */
  struct Object *target = nullptr;
  /** #NormalEditModifierMode. */
  short mode = MOD_NORMALEDIT_MODE_RADIAL;
  /** #NormalEditModifierFlag. */
  short flag = 0;
  /** #NormalEditModifierMixMode. */
  short mix_mode = MOD_NORMALEDIT_MIX_COPY;
  char _pad[2] = {};
  float mix_factor = 1.0f;
  float mix_limit = M_PI;
  float offset[3] = {0.0f, 0.0f, 0.0f};
  char _pad0[4] = {};
  void *_pad1 = nullptr;
};

/** #MeshSeqCacheModifierData.read_flag */
enum MeshSeqCacheModifierReadFlag {
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
};

#define MOD_MESHSEQ_READ_ALL \
  (MOD_MESHSEQ_READ_VERT | MOD_MESHSEQ_READ_POLY | MOD_MESHSEQ_READ_UV | MOD_MESHSEQ_READ_COLOR | \
   MOD_MESHSEQ_READ_ATTRIBUTES)

struct MeshSeqCacheModifierData {
  ModifierData modifier;

  struct CacheFile *cache_file = nullptr;
  char object_path[/*FILE_MAX*/ 1024] = "";

  /** #MeshSeqCacheModifierReadFlag. */
  char read_flag = MOD_MESHSEQ_READ_VERT | MOD_MESHSEQ_READ_POLY | MOD_MESHSEQ_READ_UV |
                   MOD_MESHSEQ_READ_COLOR | MOD_MESHSEQ_INTERPOLATE_VERTICES |
                   MOD_MESHSEQ_READ_ATTRIBUTES;
  char _pad[3] = {};

  float velocity_scale = 1.0f;

  /* Runtime. */
  struct CacheReader *reader = nullptr;
  char reader_object_path[/*FILE_MAX*/ 1024] = "";
};

/** Surface Deform modifier flags. */
enum SurfaceDeformModifierFlag {
  /* This indicates "do bind on next modifier evaluation" as well as "is bound". */
  MOD_SDEF_BIND = (1 << 0),
  MOD_SDEF_INVERT_VGROUP = (1 << 1),
  /* Only store bind data for nonzero vgroup weights at the time of bind. */
  MOD_SDEF_SPARSE_BIND = (1 << 2),
};

/** Surface Deform vertex bind modes. */
enum SurfaceDeformModifierBindMode {
  MOD_SDEF_MODE_CORNER_TRIS = 0,
  MOD_SDEF_MODE_NGONS = 1,
  MOD_SDEF_MODE_CENTROID = 2,
};

struct SDefBind {
  unsigned int *vert_inds = nullptr;
  unsigned int verts_num = 0;
  /** #SurfaceDeformModifierBindMode. */
  int mode = 0;
  float *vert_weights = nullptr;
  float normal_dist = 0;
  float influence = 0;
};

struct SDefVert {
  SDefBind *binds = nullptr;
  unsigned int binds_num = 0;
  unsigned int vertex_idx = 0;
};

struct SurfaceDeformModifierData {
  ModifierData modifier;

  struct Depsgraph *depsgraph = nullptr;
  /** Bind target object. */
  struct Object *target = nullptr;
  /** Vertex bind data. */
  SDefVert *verts = nullptr;
  const ImplicitSharingInfoHandle *verts_sharing_info = nullptr;
  float falloff = 4.0f;
  /* Number of vertices on the deformed mesh upon the bind process. */
  unsigned int mesh_verts_num = 0;
  /* Number of vertices in the `verts` array of this modifier. */
  unsigned int bind_verts_num = 0;
  /* Number of vertices and polygons on the target mesh upon bind process. */
  unsigned int target_verts_num = 0, target_polys_num = 0;
  /** #SurfaceDeformModifierFlag. */
  int flags = 0;
  float mat[4][4] = {};
  float strength = 1.0f;
  char defgrp_name[64] = "";
  int _pad2 = {};
};

/* Name/id of the generic PROP_INT cdlayer storing face weights. */
#define MOD_WEIGHTEDNORMALS_FACEWEIGHT_CDLAYER_ID "__mod_weightednormals_faceweight"

/** #WeightedNormalModifierData.mode */
enum WeightedNormalModifierMode {
  MOD_WEIGHTEDNORMAL_MODE_FACE = 0,
  MOD_WEIGHTEDNORMAL_MODE_ANGLE = 1,
  MOD_WEIGHTEDNORMAL_MODE_FACE_ANGLE = 2,
};

/** #WeightedNormalModifierData.flag */
enum WeightedNormalModifierFlag {
  MOD_WEIGHTEDNORMAL_KEEP_SHARP = (1 << 0),
  MOD_WEIGHTEDNORMAL_INVERT_VGROUP = (1 << 1),
  MOD_WEIGHTEDNORMAL_FACE_INFLUENCE = (1 << 2),
};

struct WeightedNormalModifierData {
  ModifierData modifier;

  char defgrp_name[/*MAX_VGROUP_NAME*/ 64] = "";
  /** #WeightedNormalModifierMode. */
  char mode = MOD_WEIGHTEDNORMAL_MODE_FACE;
  /** #WeightedNormalModifierFlag. */
  char flag = 0;
  short weight = 50;
  float thresh = 0.01f;
};

enum NodesModifierPanelFlag {
  NODES_MODIFIER_PANEL_OPEN = 1 << 0,
};

enum NodesModifierBakeFlag {
  NODES_MODIFIER_BAKE_CUSTOM_SIMULATION_FRAME_RANGE = 1 << 0,
  NODES_MODIFIER_BAKE_CUSTOM_PATH = 1 << 1,
};

enum NodesModifierBakeTarget {
  NODES_MODIFIER_BAKE_TARGET_INHERIT = 0,
  NODES_MODIFIER_BAKE_TARGET_PACKED = 1,
  NODES_MODIFIER_BAKE_TARGET_DISK = 2,
};

enum NodesModifierBakeMode {
  NODES_MODIFIER_BAKE_MODE_ANIMATION = 0,
  NODES_MODIFIER_BAKE_MODE_STILL = 1,
};

enum GeometryNodesModifierPanel {
  NODES_MODIFIER_PANEL_OUTPUT_ATTRIBUTES = 0,
  NODES_MODIFIER_PANEL_MANAGE = 1,
  NODES_MODIFIER_PANEL_BAKE = 2,
  NODES_MODIFIER_PANEL_NAMED_ATTRIBUTES = 3,
  NODES_MODIFIER_PANEL_BAKE_DATA_BLOCKS = 4,
  NODES_MODIFIER_PANEL_WARNINGS = 5,
};

enum NodesModifierFlag {
  NODES_MODIFIER_HIDE_DATABLOCK_SELECTOR = (1 << 0),
  NODES_MODIFIER_HIDE_MANAGE_PANEL = (1 << 1),
};

struct NodesModifierSettings {
  /* This stores data that is passed into the node group. */
  struct IDProperty *properties = nullptr;
};

/**
 * Maps a name (+ optional library name) to a data-block. The name can be stored on disk and is
 * remapped to the data-block when the data is loaded.
 *
 * At run-time, #BakeDataBlockID is used to pair up the data-block and library name.
 */
struct NodesModifierDataBlock {
  /**
   * Name of the data-block. Can be empty in which case the name of the `id` below is used.
   * This only needs to be set manually when the name stored on disk does not exist in the .blend
   * file anymore, because e.g. the ID has been renamed.
   */
  char *id_name = nullptr;
  /**
   * Name of the library the ID is in. Can be empty when the ID is not linked or when `id_name` is
   * empty as well and thus the names from the `id` below are used.
   */
  char *lib_name = nullptr;
  /** ID that this is mapped to. */
  struct ID *id = nullptr;
  /** Type of ID that is referenced by this mapping. */
  int id_type = 0;
  char _pad[4] = {};
};

struct NodesModifierBakeFile {
  const char *name = nullptr;
  /* May be null if the file is empty. */
  PackedFile *packed_file = nullptr;

#ifdef __cplusplus
  Span<std::byte> data() const
  {
    if (this->packed_file) {
      return Span{static_cast<const std::byte *>(this->packed_file->data),
                  this->packed_file->size};
    }
    return {};
  }
#endif
};

/**
 * A packed bake. The format is the same as if the bake was stored on disk.
 */
struct NodesModifierPackedBake {
  int meta_files_num = 0;
  int blob_files_num = 0;
  NodesModifierBakeFile *meta_files = nullptr;
  NodesModifierBakeFile *blob_files = nullptr;
};

struct NodesModifierBake {
  /** An id that references a nested node in the node tree. Also see #bNestedNodeRef. */
  int id = 0;
  /** #NodesModifierBakeFlag. */
  uint32_t flag = 0;
  /** #NodesModifierBakeMode. */
  uint8_t bake_mode = 0;
  /** #NodesModifierBakeTarget. */
  int8_t bake_target = 0;
  char _pad[6] = {};
  /**
   * Directory where the baked data should be stored. This is only used when
   * `NODES_MODIFIER_BAKE_CUSTOM_PATH` is set.
   */
  char *directory = nullptr;
  /**
   * Frame range for the simulation and baking that is used if
   * `NODES_MODIFIER_BAKE_CUSTOM_SIMULATION_FRAME_RANGE` is set.
   */
  int frame_start = 0;
  int frame_end = 0;

  /**
   * Maps data-block names to actual data-blocks, so that names stored in caches or on disk can be
   * remapped to actual IDs on load. The mapping also makes sure that IDs referenced by baked data
   * are not automatically removed because they are not referenced anymore. Furthermore, it allows
   * the modifier to add all required IDs to the dependency graph before actually loading the baked
   * data.
   */
  int data_blocks_num = 0;
  int active_data_block = 0;
  NodesModifierDataBlock *data_blocks = nullptr;
  NodesModifierPackedBake *packed = nullptr;

  void *_pad2 = nullptr;
  int64_t bake_size = 0;
};

struct NodesModifierPanel {
  /** ID of the corresponding panel from #bNodeTreeInterfacePanel::identifier. */
  int id = 0;
  /** #NodesModifierPanelFlag. */
  uint32_t flag = 0;
};

struct NodesModifierData {
  ModifierData modifier;
  struct bNodeTree *node_group = nullptr;
  struct NodesModifierSettings settings;
  /**
   * Directory where baked simulation states are stored. This may be relative to the .blend file.
   */
  char *bake_directory = nullptr;
  /** NodesModifierFlag. */
  int8_t flag = 0;
  /** #NodesModifierBakeTarget. */
  int8_t bake_target = NODES_MODIFIER_BAKE_TARGET_PACKED;

  char _pad[2] = {};
  int bakes_num = 0;
  NodesModifierBake *bakes = nullptr;

  char _pad2[4] = {};
  int panels_num = 0;
  NodesModifierPanel *panels = nullptr;

  NodesModifierRuntime *runtime = nullptr;

#ifdef __cplusplus
  NodesModifierBake *find_bake(int id);
  const NodesModifierBake *find_bake(int id) const;
#endif
};

/** #MeshToVolumeModifierData.resolution_mode */
enum MeshToVolumeModifierResolutionMode {
  MESH_TO_VOLUME_RESOLUTION_MODE_VOXEL_AMOUNT = 0,
  MESH_TO_VOLUME_RESOLUTION_MODE_VOXEL_SIZE = 1,
};

struct MeshToVolumeModifierData {
  ModifierData modifier;

  /** This is the object that is supposed to be converted to a volume. */
  struct Object *object = nullptr;

  /** MeshToVolumeModifierResolutionMode */
  int resolution_mode = 0;
  /** Size of a voxel in object space. */
  float voxel_size = 0;
  /** The desired amount of voxels along one axis. The actual amount of voxels might be slightly
   * different. */
  int voxel_amount = 0;

  float interior_band_width = 0;

  float density = 0;
  char _pad2[4] = {};
  void *_pad3 = nullptr;
};

/** #VolumeDisplaceModifierData.texture_map_mode */
enum VolumeDisplaceModifierTextureMapMode {
  MOD_VOLUME_DISPLACE_MAP_LOCAL = 0,
  MOD_VOLUME_DISPLACE_MAP_GLOBAL = 1,
  MOD_VOLUME_DISPLACE_MAP_OBJECT = 2,
};

struct VolumeDisplaceModifierData {
  ModifierData modifier;

  struct Tex *texture = nullptr;
  struct Object *texture_map_object = nullptr;
  /** #VolumeDisplaceModifierTextureMapMode. */
  int texture_map_mode = 0;

  float strength = 0;
  float texture_mid_level[3] = {};
  float texture_sample_radius = 0;
};

/** VolumeToMeshModifierData->resolution_mode */
enum VolumeToMeshResolutionMode {
  VOLUME_TO_MESH_RESOLUTION_MODE_GRID = 0,
  VOLUME_TO_MESH_RESOLUTION_MODE_VOXEL_AMOUNT = 1,
  VOLUME_TO_MESH_RESOLUTION_MODE_VOXEL_SIZE = 2,
};

/** VolumeToMeshModifierData->flag */
enum VolumeToMeshFlag {
  VOLUME_TO_MESH_USE_SMOOTH_SHADE = 1 << 0,
};

struct VolumeToMeshModifierData {
  ModifierData modifier;

  /** This is the volume object that is supposed to be converted to a mesh. */
  struct Object *object = nullptr;

  float threshold = 0;
  float adaptivity = 0;

  /** VolumeToMeshFlag */
  uint32_t flag = 0;

  /** VolumeToMeshResolutionMode */
  int resolution_mode = 0;
  float voxel_size = 0;
  int voxel_amount = 0;

  char grid_name[/*MAX_NAME*/ 64] = "";
  void *_pad1 = nullptr;
};

enum GreasePencilModifierInfluenceFlag {
  GREASE_PENCIL_INFLUENCE_INVERT_LAYER_FILTER = (1 << 0),
  GREASE_PENCIL_INFLUENCE_USE_LAYER_PASS_FILTER = (1 << 1),
  GREASE_PENCIL_INFLUENCE_INVERT_LAYER_PASS_FILTER = (1 << 2),
  GREASE_PENCIL_INFLUENCE_INVERT_MATERIAL_FILTER = (1 << 3),
  GREASE_PENCIL_INFLUENCE_USE_MATERIAL_PASS_FILTER = (1 << 4),
  GREASE_PENCIL_INFLUENCE_INVERT_MATERIAL_PASS_FILTER = (1 << 5),
  GREASE_PENCIL_INFLUENCE_INVERT_VERTEX_GROUP = (1 << 6),
  GREASE_PENCIL_INFLUENCE_USE_CUSTOM_CURVE = (1 << 7),
  GREASE_PENCIL_INFLUENCE_USE_LAYER_GROUP_FILTER = (1 << 8),
};

/**
 * Common influence data for grease pencil modifiers.
 * Not all parts may be used by all modifier types.
 */
struct GreasePencilModifierInfluenceData {
  /** GreasePencilModifierInfluenceFlag */
  int flag = 0;
  char _pad1[4] = {};
  /** Filter by layer name. */
  char layer_name[64] = "";
  /** Filter by stroke material. */
  struct Material *material = nullptr;
  /** Filter by layer pass. */
  int layer_pass = 0;
  /** Filter by material pass. */
  int material_pass = 0;
  char vertex_group_name[/*MAX_VGROUP_NAME*/ 64] = "";
  struct CurveMapping *custom_curve = nullptr;
  void *_pad2 = nullptr;
};

/** Which attributes are affected by color modifiers. */
enum GreasePencilModifierColorMode {
  MOD_GREASE_PENCIL_COLOR_STROKE = 0,
  MOD_GREASE_PENCIL_COLOR_FILL = 1,
  MOD_GREASE_PENCIL_COLOR_BOTH = 2,
  MOD_GREASE_PENCIL_COLOR_HARDNESS = 3,
};

enum GreasePencilOpacityModifierFlag {
  /* Use vertex group as opacity factors instead of influence. */
  MOD_GREASE_PENCIL_OPACITY_USE_WEIGHT_AS_FACTOR = (1 << 0),
  /* Set the opacity for every point in a stroke, otherwise multiply existing opacity. */
  MOD_GREASE_PENCIL_OPACITY_USE_UNIFORM_OPACITY = (1 << 1),
};

struct GreasePencilOpacityModifierData {
  ModifierData modifier;
  GreasePencilModifierInfluenceData influence;
  /** GreasePencilOpacityModifierFlag */
  int flag = 0;
  /** GreasePencilModifierColorMode */
  char color_mode = MOD_GREASE_PENCIL_COLOR_BOTH;
  char _pad1[3] = {};
  float color_factor = 1.0f;
  float hardness_factor = 1.0f;
  void *_pad2 = nullptr;
};

enum GreasePencilSubdivideType {
  MOD_GREASE_PENCIL_SUBDIV_CATMULL = 0,
  MOD_GREASE_PENCIL_SUBDIV_SIMPLE = 1,
};

struct GreasePencilSubdivModifierData {
  ModifierData modifier;
  GreasePencilModifierInfluenceData influence;
  /** #GreasePencilSubdivideType. */
  int type = 0;
  /** Level of subdivisions, will generate 2^level segments. */
  int level = 1;

  char _pad[8] = {};
  void *_pad1 = nullptr;
};

struct GreasePencilColorModifierData {
  ModifierData modifier;
  GreasePencilModifierInfluenceData influence;
  /** GreasePencilModifierColorMode */
  char color_mode = MOD_GREASE_PENCIL_COLOR_BOTH;
  char _pad1[3] = {};
  /** HSV factors. */
  float hsv[3] = {0.5f, 1.0f, 1.0f};
  void *_pad2 = nullptr;
};

enum GreasePencilTintModifierMode {
  MOD_GREASE_PENCIL_TINT_UNIFORM = 0,
  MOD_GREASE_PENCIL_TINT_GRADIENT = 1,
};

enum GreasePencilTintModifierFlag {
  /* Use vertex group as factors instead of influence. */
  MOD_GREASE_PENCIL_TINT_USE_WEIGHT_AS_FACTOR = (1 << 0),
};

struct GreasePencilTintModifierData {
  ModifierData modifier;
  GreasePencilModifierInfluenceData influence;
  /** GreasePencilTintModifierFlag */
  short flag = 0;
  /** GreasePencilModifierColorMode */
  char color_mode = MOD_GREASE_PENCIL_COLOR_BOTH;
  /** GreasePencilTintModifierMode */
  char tint_mode = MOD_GREASE_PENCIL_TINT_UNIFORM;
  float factor = 0.5f;
  /** Influence distance from the gradient object. */
  float radius = 1.0f;
  /** Simple tint color. */
  float color[3] = {1.0f, 1.0f, 1.0f};
  /** Object for gradient direction. */
  struct Object *object = nullptr;
  /** Color ramp for the gradient. */
  struct ColorBand *color_ramp = nullptr;
  void *_pad = nullptr;
};

enum eGreasePencilSmooth_Flag {
  MOD_GREASE_PENCIL_SMOOTH_MOD_LOCATION = (1 << 0),
  MOD_GREASE_PENCIL_SMOOTH_MOD_STRENGTH = (1 << 1),
  MOD_GREASE_PENCIL_SMOOTH_MOD_THICKNESS = (1 << 2),
  MOD_GREASE_PENCIL_SMOOTH_MOD_UV = (1 << 3),
  MOD_GREASE_PENCIL_SMOOTH_KEEP_SHAPE = (1 << 4),
  MOD_GREASE_PENCIL_SMOOTH_SMOOTH_ENDS = (1 << 5),
};

/* Enum definitions for length modifier stays in the old DNA for the moment. */
struct GreasePencilSmoothModifierData {
  ModifierData modifier;
  GreasePencilModifierInfluenceData influence;
  /** #eGreasePencilSmooth_Flag. */
  int flag = MOD_GREASE_PENCIL_SMOOTH_MOD_LOCATION;
  /** Factor of smooth. */
  float factor = 1.0f;
  /** How many times apply smooth. */
  int step = 1;
  char _pad[4] = {};
  void *_pad1 = nullptr;
};

enum GreasePencilOffsetModifierFlag {
  MOD_GREASE_PENCIL_OFFSET_UNIFORM_RANDOM_SCALE = (1 << 0),
};

enum GreasePencilOffsetModifierMode {
  MOD_GREASE_PENCIL_OFFSET_RANDOM = 0,
  MOD_GREASE_PENCIL_OFFSET_LAYER = 1,
  MOD_GREASE_PENCIL_OFFSET_MATERIAL = 2,
  MOD_GREASE_PENCIL_OFFSET_STROKE = 3,
};

struct GreasePencilOffsetModifierData {
  ModifierData modifier;
  GreasePencilModifierInfluenceData influence;
  /** GreasePencilOffsetModifierFlag */
  int flag = 0;
  /** GreasePencilOffsetModifierMode */
  int offset_mode = MOD_GREASE_PENCIL_OFFSET_RANDOM;
  /** Global offset. */
  float loc[3] = {0.0f, 0.0f, 0.0f};
  float rot[3] = {0.0f, 0.0f, 0.0f};
  float scale[3] = {0.0f, 0.0f, 0.0f};
  /** Offset per stroke. */
  float stroke_loc[3] = {};
  float stroke_rot[3] = {};
  float stroke_scale[3] = {};
  int seed = 0;
  int stroke_step = 1;
  int stroke_start_offset = 0;
  char _pad1[4] = {};
  void *_pad2 = nullptr;
};

struct GreasePencilNoiseModifierData {
  ModifierData modifier;
  GreasePencilModifierInfluenceData influence;

  /** For convenience of versioning, these flags are kept in `eNoiseGpencil_Flag`. */
  int flag = GP_NOISE_FULL_STROKE | GP_NOISE_USE_RANDOM;

  /** Factor of noise. */
  float factor = 0.5f;
  float factor_strength = 0.0f;
  float factor_thickness = 0.0f;
  float factor_uvs = 0.0f;
  /** Noise Frequency scaling */
  float noise_scale = 0.0f;
  float noise_offset = 0.0f;
  short noise_mode = 0;
  char _pad[2] = {};
  /** How many frames before recalculate randoms. */
  int step = 4;
  /** Random seed */
  int seed = 1;

  void *_pad1 = nullptr;
};

enum GreasePencilMirrorModifierFlag {
  MOD_GREASE_PENCIL_MIRROR_AXIS_X = (1 << 0),
  MOD_GREASE_PENCIL_MIRROR_AXIS_Y = (1 << 1),
  MOD_GREASE_PENCIL_MIRROR_AXIS_Z = (1 << 2),
};

struct GreasePencilMirrorModifierData {
  ModifierData modifier;
  GreasePencilModifierInfluenceData influence;
  struct Object *object = nullptr;
  /** #GreasePencilMirrorModifierFlag */
  int flag = MOD_GREASE_PENCIL_MIRROR_AXIS_X;
  char _pad[4] = {};
};

enum GreasePencilThicknessModifierFlag {
  MOD_GREASE_PENCIL_THICK_NORMALIZE = (1 << 0),
  MOD_GREASE_PENCIL_THICK_WEIGHT_FACTOR = (1 << 1),
};

struct GreasePencilThickModifierData {
  ModifierData modifier;
  GreasePencilModifierInfluenceData influence;
  /** #GreasePencilThicknessModifierFlag */
  int flag = 0;
  /** Relative thickness factor. */
  float thickness_fac = 1.0f;
  /** Absolute thickness override. */
  float thickness = 0.02;
  char _pad[4] = {};
  void *_pad1 = nullptr;
};

struct GreasePencilLatticeModifierData {
  ModifierData modifier;
  GreasePencilModifierInfluenceData influence;
  struct Object *object = nullptr;
  float strength = 1.0f;
  char _pad[4] = {};
};

enum GreasePencilDashModifierFlag {
  MOD_GREASE_PENCIL_DASH_USE_CYCLIC = (1 << 0),
};

struct GreasePencilDashModifierSegment {
  char name[64] = "Segment";
  int dash = 2;
  int gap = 1;
  float radius = 1.0f;
  float opacity = 1.0f;
  int mat_nr = -1;
  /** #GreasePencilDashModifierFlag */
  int flag = 0;
};

struct GreasePencilDashModifierData {
  ModifierData modifier;
  GreasePencilModifierInfluenceData influence;

  GreasePencilDashModifierSegment *segments_array = nullptr;
  int segments_num = 0;
  int segment_active_index = 0;

  int dash_offset = 0;
  char _pad[4] = {};

#ifdef __cplusplus
  Span<GreasePencilDashModifierSegment> segments() const;
  MutableSpan<GreasePencilDashModifierSegment> segments();
#endif
};

enum GreasePencilMultiplyModifierFlag {
  /* GP_MULTIPLY_ENABLE_ANGLE_SPLITTING = (1 << 1),  Deprecated. */
  MOD_GREASE_PENCIL_MULTIPLY_ENABLE_FADING = (1 << 2),
};

struct GreasePencilMultiModifierData {
  ModifierData modifier;
  GreasePencilModifierInfluenceData influence;

  /* #GreasePencilMultiplyModifierFlag */
  int flag = 0;

  int duplications = 3;
  float distance = 0.1f;
  /* -1:inner 0:middle 1:outer */
  float offset = 0.0f;

  float fading_center = 0.5f;
  float fading_thickness = 0.5f;
  float fading_opacity = 0.5f;

  int _pad0 = {};

  void *_pad = nullptr;
};

struct GreasePencilLengthModifierData {
  ModifierData modifier;
  GreasePencilModifierInfluenceData influence;
  int flag = GP_LENGTH_USE_CURVATURE;
  float start_fac = 0.1f, end_fac = 0.1f;
  float rand_start_fac = 0.0f, rand_end_fac = 0.0f, rand_offset = 0.0f;
  float overshoot_fac = 0.1f;
  /** (first element is the index) random values. */
  int seed = 0;
  /** How many frames before recalculate randoms. */
  int step = 4;
  /** #eLengthGpencil_Type. */
  int mode = 0;
  char _pad[4] = {};
  /* Curvature parameters. */
  float point_density = 30.0f;
  float segment_influence = 0.0f;
  float max_angle = DEG2RAD(170.0f);

  void *_pad1 = nullptr;
};

enum GreasePencilWeightAngleModifierFlag {
  MOD_GREASE_PENCIL_WEIGHT_ANGLE_MULTIPLY_DATA = (1 << 5),
  MOD_GREASE_PENCIL_WEIGHT_ANGLE_INVERT_OUTPUT = (1 << 6),
};
enum GreasePencilWeightAngleModifierSpace {
  MOD_GREASE_PENCIL_WEIGHT_ANGLE_SPACE_LOCAL = 0,
  MOD_GREASE_PENCIL_WEIGHT_ANGLE_SPACE_WORLD = 1,
};

struct GreasePencilWeightAngleModifierData {
  ModifierData modifier;
  GreasePencilModifierInfluenceData influence;
  /** #GreasePencilWeightAngleModifierFlag */
  int flag = 0;
  float min_weight = 0;
  /** Axis. */
  int16_t axis = 1;
  /** #GreasePencilWeightAngleModifierSpace */
  int16_t space = 0;
  /** Angle */
  float angle = 0;
  /** Weights output to this vertex group, can be the same as source group. */
  char target_vgname[64] = "";

  void *_pad = nullptr;
};

enum GreasePencilArrayModifierFlag {
  MOD_GREASE_PENCIL_ARRAY_USE_OFFSET = (1 << 7),
  MOD_GREASE_PENCIL_ARRAY_USE_RELATIVE = (1 << 8),
  MOD_GREASE_PENCIL_ARRAY_USE_OB_OFFSET = (1 << 9),
  MOD_GREASE_PENCIL_ARRAY_UNIFORM_RANDOM_SCALE = (1 << 10),
};

struct GreasePencilArrayModifierData {
  ModifierData modifier;
  GreasePencilModifierInfluenceData influence;
  struct Object *object = nullptr;
  int count = 2;
  /** #GreasePencilArrayModifierFlag */
  int flag = GP_ARRAY_USE_RELATIVE;
  float offset[3] = {0.0f, 0.0f, 0.0f};
  float shift[3] = {1.0f, 0.0f, 0.0f};

  float rnd_offset[3] = {0.0f, 0.0f, 0.0f};
  float rnd_rot[3] = {0.0f, 0.0f, 0.0f};
  float rnd_scale[3] = {0.0f, 0.0f, 0.0f};

  char _pad[4] = {};
  /** (first element is the index) random values. (?) */
  int seed = 1;

  /* Replacement material index. */
  int mat_rpl = 0;
};

struct GreasePencilWeightProximityModifierData {
  ModifierData modifier;
  GreasePencilModifierInfluenceData influence;

  /* #GreasePencilWeightProximityFlag. */
  int flag = 0;
  char target_vgname[64] = "";
  float min_weight = 0;

  float dist_start = 0.0f;
  float dist_end = 20.0f;

  struct Object *object = nullptr;
};

enum GreasePencilHookFlag {
  MOD_GREASE_PENCIL_HOOK_UNIFORM_SPACE = (1 << 0),
};

enum GreasePencilHookFalloff {
  MOD_GREASE_PENCIL_HOOK_Falloff_None = 0,
  MOD_GREASE_PENCIL_HOOK_Falloff_Curve = 1,
  MOD_GREASE_PENCIL_HOOK_Falloff_Sharp = 2,
  MOD_GREASE_PENCIL_HOOK_Falloff_Smooth = 3,
  MOD_GREASE_PENCIL_HOOK_Falloff_Root = 4,
  MOD_GREASE_PENCIL_HOOK_Falloff_Linear = 5,
  MOD_GREASE_PENCIL_HOOK_Falloff_Const = 6,
  MOD_GREASE_PENCIL_HOOK_Falloff_Sphere = 7,
  MOD_GREASE_PENCIL_HOOK_Falloff_InvSquare = 8,
};

struct GreasePencilHookModifierData {
  ModifierData modifier;
  GreasePencilModifierInfluenceData influence;

  struct Object *object = nullptr;
  /** Optional name of bone target. */
  char subtarget[/*MAX_NAME*/ 64] = "";
  char _pad[4] = {};

  /** #GreasePencilHookFlag. */
  int flag = 0;
  /** #GreasePencilHookFalloff. */
  char falloff_type = MOD_GREASE_PENCIL_HOOK_Falloff_Smooth;
  char _pad1[3] = {};
  /** Matrix making current transform unmodified. */
  float parentinv[4][4] = _DNA_DEFAULT_UNIT_M4;
  /** Visualization of hook. */
  float cent[3] = {0.0f, 0.0f, 0.0f};
  /** If not zero, falloff is distance where influence zero. */
  float falloff = 0.0f;
  float force = 0.5f;
};

/* This enum is for modifier internal state only. */
enum eGreasePencilLineartFlags {
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
};

enum GreasePencilLineartModifierSource {
  LINEART_SOURCE_COLLECTION = 0,
  LINEART_SOURCE_OBJECT = 1,
  LINEART_SOURCE_SCENE = 2,
};

enum GreasePencilLineartModifierShadowFilter {
  /* These options need to be ordered in this way because those latter options requires line art to
   * run a few extra stages. Having those values set up this way will allow
   * #BKE_gpencil_get_lineart_modifier_limits() to find out maximum stages needed in multiple
   * cached line art modifiers. */
  LINEART_SHADOW_FILTER_NONE = 0,
  LINEART_SHADOW_FILTER_ILLUMINATED = 1,
  LINEART_SHADOW_FILTER_SHADED = 2,
  LINEART_SHADOW_FILTER_ILLUMINATED_ENCLOSED_SHAPES = 3,
};

/* This enum is for modifier internal state only. */
enum eLineArtGPencilModifierFlags {
  /* These two moved to #eLineartMainFlags to keep consistent with flag variable purpose. */
  /* MOD_LINEART_INVERT_SOURCE_VGROUP = (1 << 0), */
  /* MOD_LINEART_MATCH_OUTPUT_VGROUP = (1 << 1), */
  MOD_LINEART_BINARY_WEIGHTS = (1 << 2) /* Deprecated, this is removed for lack of use case. */,
  MOD_LINEART_IS_BAKED = (1 << 3),
  MOD_LINEART_USE_CACHE = (1 << 4),
  MOD_LINEART_OFFSET_TOWARDS_CUSTOM_CAMERA = (1 << 5),
  MOD_LINEART_INVERT_COLLECTION = (1 << 6),
  MOD_LINEART_INVERT_SILHOUETTE_FILTER = (1 << 7),
};

enum GreasePencilLineartMaskSwitches {
  MOD_LINEART_MATERIAL_MASK_ENABLE = (1 << 0),
  /** When set, material mask bit comparisons are done with bit wise "AND" instead of "OR". */
  MOD_LINEART_MATERIAL_MASK_MATCH = (1 << 1),
  MOD_LINEART_INTERSECTION_MATCH = (1 << 2),
};

enum eGreasePencilLineartMaskSwitches {
  LINEART_GPENCIL_MATERIAL_MASK_ENABLE = (1 << 0),
  /** When set, material mask bit comparisons are done with bit wise "AND" instead of "OR". */
  LINEART_GPENCIL_MATERIAL_MASK_MATCH = (1 << 1),
  LINEART_GPENCIL_INTERSECTION_MATCH = (1 << 2),
};

enum eGreasePencilLineartSilhouetteFilter {
  LINEART_SILHOUETTE_FILTER_NONE = 0,
  LINEART_SILHOUETTE_FILTER_GROUP = (1 << 0),
  LINEART_SILHOUETTE_FILTER_INDIVIDUAL = (1 << 1),
};

struct LineartCache;

struct GreasePencilLineartModifierData {
  ModifierData modifier;

  uint16_t edge_types =
      MOD_LINEART_EDGE_FLAG_INIT_TYPE; /* line type enable flags, bits in eLineartEdgeFlag */

  /** Object or Collection, from #eGreasePencilLineartSource. */
  char source_type = 0;

  char use_multiple_levels = 0;
  short level_start = 0;
  short level_end = 0;

  struct Object *source_camera = nullptr;
  struct Object *light_contour_object = nullptr;

  struct Object *source_object = nullptr;
  struct Collection *source_collection = nullptr;

  struct Material *target_material = nullptr;
  char target_layer[64] = "";

  /**
   * These two variables are to pass on vertex group information from mesh to strokes.
   * `vgname` specifies which vertex groups our strokes from source_vertex_group will go to.
   */
  char source_vertex_group[64] = "";
  char vgname[64] = "";

  /* Camera focal length is divided by (1 + over-scan), before calculation, which give a wider FOV,
   * this doesn't change coordinates range internally (-1, 1), but makes the calculated frame
   * bigger than actual output. This is for the easier shifting calculation. A value of 0.5 means
   * the "internal" focal length become 2/3 of the actual camera. */
  float overscan = 0.1f;

  /* Values for point light and directional (sun) light. */
  /* For point light, fov always gonna be 120 deg horizontal, with 3 "cameras" covering 360 deg. */
  float shadow_camera_fov = 0;
  float shadow_camera_size = 200.0f;
  float shadow_camera_near = 0.1f;
  float shadow_camera_far = 200.0f;

  float opacity = 1.0f;
  float radius = 0.0025;

  short thickness_legacy = 25; /* Deprecated, use `radius`. */

  unsigned char mask_switches = 0; /* #eGreasePencilLineartMaskSwitches */
  unsigned char material_mask_bits = 0;
  unsigned char intersection_mask = 0;

  unsigned char shadow_selection = 0;
  unsigned char silhouette_selection = 0;
  char _pad[5] = {};

  /** `0..1` range for cosine angle */
  float crease_threshold = DEG2RAD(140.0f);

  /** `0..PI` angle, for splitting strokes at sharp points. */
  float angle_splitting_threshold = 0.0f;

  /** Strength for smoothing jagged chains. */
  float chain_smooth_tolerance = 0.0f;

  /* CPU mode */
  float chaining_image_threshold = 0.001f;

  /* eLineartMainFlags, for one time calculation. */
  int calculation_flags = MOD_LINEART_ALLOW_DUPLI_OBJECTS | MOD_LINEART_ALLOW_CLIPPING_BOUNDARIES |
                          MOD_LINEART_USE_CREASE_ON_SHARP_EDGES |
                          MOD_LINEART_FILTER_FACE_MARK_KEEP_CONTOUR |
                          MOD_LINEART_MATCH_OUTPUT_VGROUP;

  /* #eGreasePencilLineartFlags, modifier internal state. */
  int flags = 0;

  /* Move strokes towards camera to avoid clipping while preserve depth for the viewport. */
  float stroke_depth_offset = 0.05;

  /* Runtime data. */

  /* Because we can potentially only compute features lines once per modifier stack (Use Cache), we
   * need to have these override values to ensure that we have the data we need is computed and
   * stored in the cache. */
  char level_start_override = 0;
  char level_end_override = 0;
  short edge_types_override = 0;
  char shadow_selection_override = 0;
  char shadow_use_silhouette_override = 0;

  char _pad2[6] = {};

  /* Shared cache will only be on the first line art modifier in the stack, and will exist until
   * the end of modifier stack evaluation. If the object has line art modifiers, this variable is
   * then initialized in #grease_pencil_evaluate_modifiers(). */
  struct LineartCache *shared_cache = nullptr;

  /* Cache for single execution of line art, when LINEART_GPENCIL_USE_CACHE is enabled, this is a
   * reference to first_lineart->shared_cache, otherwise it holds its own cache. */
  struct LineartCache *cache = nullptr;

  /* Keep a pointer to the render buffer so we can call destroy from #ModifierData. */
  struct LineartData *la_data_ptr = nullptr;

  /* Points to a `LineartModifierRuntime`, which includes the object dependency list. */
  struct LineartModifierRuntime *runtime = nullptr;
};

struct GreasePencilArmatureModifierData {
  ModifierData modifier;
  GreasePencilModifierInfluenceData influence;

  struct Object *object = nullptr;
  /** #eArmature_DeformFlag. */
  short deformflag = ARM_DEF_VGROUP;
  char _pad[6] = {};
};

enum GreasePencilTimeModifierFlag {
  MOD_GREASE_PENCIL_TIME_KEEP_LOOP = (1 << 0),
  MOD_GREASE_PENCIL_TIME_CUSTOM_RANGE = (1 << 1),
};

enum GreasePencilTimeModifierMode {
  MOD_GREASE_PENCIL_TIME_MODE_NORMAL = 0,
  MOD_GREASE_PENCIL_TIME_MODE_REVERSE = 1,
  MOD_GREASE_PENCIL_TIME_MODE_FIX = 2,
  MOD_GREASE_PENCIL_TIME_MODE_PINGPONG = 3,
  MOD_GREASE_PENCIL_TIME_MODE_CHAIN = 4,
};

enum GreasePencilTimeModifierSegmentMode {
  MOD_GREASE_PENCIL_TIME_SEG_MODE_NORMAL = 0,
  MOD_GREASE_PENCIL_TIME_SEG_MODE_REVERSE = 1,
  MOD_GREASE_PENCIL_TIME_SEG_MODE_PINGPONG = 2,
};

struct GreasePencilTimeModifierSegment {
  char name[64] = "Segment";
  int segment_start = 1;
  int segment_end = 2;
  /** #GreasePencilTimeModifierSegmentMode. */
  int segment_mode = 0;
  int segment_repeat = 1;
};

struct GreasePencilTimeModifierData {
  ModifierData modifier;
  GreasePencilModifierInfluenceData influence;
  /** #GreasePencilTimeModifierFlag */
  int flag = MOD_GREASE_PENCIL_TIME_KEEP_LOOP;
  int offset = 1;
  /** Animation scale. */
  float frame_scale = 1.0f;
  /** #GreasePencilTimeModifierMode. */
  int mode = 0;
  /** Start and end frame for custom range. */
  int sfra = 1, efra = 250;

  GreasePencilTimeModifierSegment *segments_array = nullptr;
  int segments_num = 1;
  int segment_active_index = 0;

#ifdef __cplusplus
  Span<GreasePencilTimeModifierSegment> segments() const;
  MutableSpan<GreasePencilTimeModifierSegment> segments();
#endif
};

/* Texture->mode */
enum GreasePencilEnvelopeModifierMode {
  MOD_GREASE_PENCIL_ENVELOPE_DEFORM = 0,
  MOD_GREASE_PENCIL_ENVELOPE_SEGMENTS = 1,
  MOD_GREASE_PENCIL_ENVELOPE_FILLS = 2,
};

struct GreasePencilEnvelopeModifierData {
  ModifierData modifier;
  GreasePencilModifierInfluenceData influence;
  /* #GreasePencilEnvelopeModifierMode. */
  int mode = MOD_GREASE_PENCIL_ENVELOPE_SEGMENTS;
  /** Material for the new strokes. */
  int mat_nr = -1;
  /** Thickness multiplier for the new strokes. */
  float thickness = 1.0f;
  /** Strength multiplier for the new strokes. */
  float strength = 1.0f;
  /** Number of points to skip over. */
  int skip = 0;
  /* Length of the envelope effect. */
  int spread = 10;
};

enum GreasePencilOutlineModifierFlag {
  MOD_GREASE_PENCIL_OUTLINE_KEEP_SHAPE = (1 << 0),
};

struct GreasePencilOutlineModifierData {
  ModifierData modifier;
  GreasePencilModifierInfluenceData influence;

  /** Target stroke origin. */
  struct Object *object = nullptr;
  /** #GreasePencilOutlineModifierFlag. */
  int flag = MOD_GREASE_PENCIL_OUTLINE_KEEP_SHAPE;
  /** Thickness. */
  int thickness = 1;
  /** Sample Length. */
  float sample_length = 0.0f;
  /** Subdivisions. */
  int subdiv = 3;
  /** Material for outline. */
  struct Material *outline_material = nullptr;
};

struct GreasePencilShrinkwrapModifierData {
  ModifierData modifier;
  GreasePencilModifierInfluenceData influence;

  /** Shrink target. */
  struct Object *target = nullptr;
  /** Additional shrink target. */
  struct Object *aux_target = nullptr;
  /** Distance offset to keep from mesh/projection point. */
  float keep_dist = 0.05f;
  /** Shrink type projection. */
  short shrink_type = MOD_SHRINKWRAP_NEAREST_SURFACE;
  /** Shrink options. */
  char shrink_opts = MOD_SHRINKWRAP_PROJECT_ALLOW_POS_DIR;
  /** Shrink to surface mode. */
  char shrink_mode = MOD_SHRINKWRAP_ON_SURFACE;
  /** Limit the projection ray cast. */
  float proj_limit = 0.0f;
  /** Axis to project over. */
  char proj_axis = MOD_SHRINKWRAP_PROJECT_OVER_NORMAL;

  /**
   * If using projection over vertex normal this controls the level of subsurface that must be
   * done before getting the vertex coordinates and normal.
   */
  char subsurf_levels = 0;
  char _pad[2] = {};
  /** Factor of smooth. */
  float smooth_factor = 0.05f;
  /** How many times apply smooth. */
  int smooth_step = 1;

  /** Runtime only. */
  struct ShrinkwrapTreeData *cache_data = nullptr;
};

enum GreasePencilBuildMode {
  /* Strokes are shown one by one until all have appeared */
  MOD_GREASE_PENCIL_BUILD_MODE_SEQUENTIAL = 0,
  /* All strokes start at the same time */
  MOD_GREASE_PENCIL_BUILD_MODE_CONCURRENT = 1,
  /* Only the new strokes are built */
  MOD_GREASE_PENCIL_BUILD_MODE_ADDITIVE = 2,
};

enum GreasePencilBuildTransition {
  /* Show in forward order */
  MOD_GREASE_PENCIL_BUILD_TRANSITION_GROW = 0,
  /* Hide in reverse order */
  MOD_GREASE_PENCIL_BUILD_TRANSITION_SHRINK = 1,
  /* Hide in forward order */
  MOD_GREASE_PENCIL_BUILD_TRANSITION_VANISH = 2,
};

enum GreasePencilBuildTimeAlignment {
  /* All strokes start at same time */
  MOD_GREASE_PENCIL_BUILD_TIMEALIGN_START = 0,
  /* All strokes end at same time */
  MOD_GREASE_PENCIL_BUILD_TIMEALIGN_END = 1,

  /* TODO: Random Offsets, Stretch-to-Fill */
};

enum GreasePencilBuildTimeMode {
  /** Use a number of frames build. */
  MOD_GREASE_PENCIL_BUILD_TIMEMODE_FRAMES = 0,
  /** Use manual percentage to build. */
  MOD_GREASE_PENCIL_BUILD_TIMEMODE_PERCENTAGE = 1,
  /** Use factor of recorded speed to build. */
  MOD_GREASE_PENCIL_BUILD_TIMEMODE_DRAWSPEED = 2,
};

enum GreasePencilBuildFlag {
  /* Restrict modifier to only operating between the nominated frames */
  MOD_GREASE_PENCIL_BUILD_RESTRICT_TIME = (1 << 0),
  MOD_GREASE_PENCIL_BUILD_USE_FADING = (1 << 14),
};

struct GreasePencilBuildModifierData {
  ModifierData modifier;
  GreasePencilModifierInfluenceData influence;
  /**
   * If GP_BUILD_RESTRICT_TIME is set,
   * the defines the frame range where GP frames are considered.
   */
  float start_frame = 1;
  float end_frame = 125;

  /** Start time added on top of the drawing frame number */
  float start_delay = 0.0f;
  float length = 100.0f;

  /** #GreasePencilBuildFlag. */
  short flag = 0;

  /** #GreasePencilBuildMode. */
  short mode = 0;
  /** #GreasePencilBuildTransition. */
  short transition = 0;

  /**
   * #GreasePencilBuildTimeAlignment.
   * For the "Concurrent" mode, when should "shorter" strips start/end.
   */
  short time_alignment = 0;

  /** Speed factor for #GP_BUILD_TIMEMODE_DRAWSPEED. */
  float speed_fac = 1.2f;
  /** Maximum time gap between strokes for #GP_BUILD_TIMEMODE_DRAWSPEED. */
  float speed_maxgap = 0.5f;
  /** GreasePencilBuildTimeMode. */
  short time_mode = 0;
  char _pad[6] = {};

  /** Build origin control object. */
  struct Object *object = nullptr;

  /** Factor of the stroke (used instead of frame evaluation). */
  float percentage_fac = 0.0f;

  /** Weight fading at the end of the stroke. */
  float fade_fac = 0;
  /** Target vertex-group name. */
  char target_vgname[/*MAX_VGROUP_NAME*/ 64] = "";
  /** Fading strength of opacity and thickness */
  float fade_opacity_strength = 0;
  float fade_thickness_strength = 0;
};

enum GreasePencilSimplifyModifierMode {
  MOD_GREASE_PENCIL_SIMPLIFY_FIXED = 0,
  MOD_GREASE_PENCIL_SIMPLIFY_ADAPTIVE = 1,
  MOD_GREASE_PENCIL_SIMPLIFY_SAMPLE = 2,
  MOD_GREASE_PENCIL_SIMPLIFY_MERGE = 3,
};

struct GreasePencilSimplifyModifierData {
  ModifierData modifier;
  GreasePencilModifierInfluenceData influence;

  /** #GreasePencilSimplifyModifierMode. */
  short mode = MOD_GREASE_PENCIL_SIMPLIFY_FIXED;
  char _pad[4] = {};
  /** Every n vertex to keep. */
  short step = 1;
  float factor = 0.0f;
  /** For sampling. */
  float length = 0.1f;
  float sharp_threshold = 0;

  /** Merge distance */
  float distance = 0.1f;
};

/* Texture->fit_method */
enum GreasePencilTextureModifierFit {
  MOD_GREASE_PENCIL_TEXTURE_FIT_STROKE = 0,
  MOD_GREASE_PENCIL_TEXTURE_CONSTANT_LENGTH = 1,
};

/* Texture->mode */
enum GreasePencilTextureModifierMode {
  MOD_GREASE_PENCIL_TEXTURE_STROKE = 0,
  MOD_GREASE_PENCIL_TEXTURE_FILL = 1,
  MOD_GREASE_PENCIL_TEXTURE_STROKE_AND_FILL = 2,
};

struct GreasePencilTextureModifierData {
  ModifierData modifier;
  GreasePencilModifierInfluenceData influence;
  /* Offset value to add to uv_fac. */
  float uv_offset = 0.0f;
  float uv_scale = 1.0f;
  float fill_rotation = 0.0f;
  float fill_offset[2] = {0.0f, 0.0f};
  float fill_scale = 1.0f;
  /* Custom index for passes. */
  int layer_pass = 0;
  /**  #GreasePencilTextureModifierFit.Texture fit options. */
  short fit_method = GP_TEX_CONSTANT_LENGTH;
  /** #GreasePencilTextureModifierMode. */
  short mode = 0;
  /* Dot texture rotation. */
  float alignment_rotation = 0;
  char _pad[4] = {};
};

}  // namespace blender
