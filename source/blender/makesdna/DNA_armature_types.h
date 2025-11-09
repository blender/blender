/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_ID.h"
#include "DNA_defs.h"
#include "DNA_listBase.h"
#include "DNA_userdef_types.h"

#include "BLI_enum_flags.hh"

#ifdef __cplusplus
#  include "BLI_span.hh"
namespace blender::animrig {
class BoneColor;
}
#endif

struct AnimData;
struct BoneCollection;

/* The Armature system works on different transformation space levels:
 *
 * 1) Bone Space:     In the orientation of the parent bone, position relative
 *                    to the parent's tail. Same as Armature Space for bones
 *                    without parent.
 * 2) Armature Space: The bone's rest transform in Object space. This is the
 *                    multiplication of the bone space matrices of the bone and
 *                    all its ancestors.
 * 3) Pose Space:     The bone's posed transform in Object space. This is the
 *                    same space as Armature Space, except that it represents
 *                    the current bone transform instead of the rest pose.
 *                    See bPoseChannel::pose_mat
 * 4) Channel Space:  The bone's local transform relative to its rest transform.
 *                    See bPoseChannel::chan_mat
 */

typedef struct BoneColor {
  /**
   * Index of color palette to use when drawing bones.
   * 0=default, >0 = predefined in theme, -1=custom color in #custom.
   *
   * For the predefined ones, see #rna_enum_color_sets_items in rna_armature.c.
   */
  int8_t palette_index;
  uint8_t _pad0[7];
  ThemeWireColor custom;
#ifdef __cplusplus
  blender::animrig::BoneColor &wrap();
  const blender::animrig::BoneColor &wrap() const;
#endif
} BoneColor;

typedef struct Bone_Runtime {
  /* #BoneCollectionReference */
  ListBase collections;
} Bone_Runtime;

typedef struct Bone {
  /** Next/previous elements within this list. */
  struct Bone *next, *prev;
  /** User-Defined Properties on this Bone. */
  IDProperty *prop;
  /** System-Defined Properties storage. */
  IDProperty *system_properties;
  void *_pad0;
  /** Parent (IK parent if appropriate flag is set). */
  struct Bone *parent;
  /** Children. */
  ListBase childbase;
  /** Name of the bone - must be unique within the armature. */
  char name[/*MAXBONENAME*/ 64];

  /** Roll is input for edit-mode, length calculated. */
  float roll;
  /** Head position in Bone Space (see top of this file). */
  float head[3];
  /** Tail position in Bone Space (see top of this file). */
  float tail[3];
  /**
   * Bone matrix in Bone Space (see top of this file).
   *
   * bone.matrix in RNA. Computed in BKE_armature_where_is_bone(). */
  float bone_mat[3][3];

  int flag;
  int8_t drawtype; /* eArmature_Drawtype */
  char _pad1[3];
  BoneColor color; /* MUST be named the same as in bPoseChannel and EditBone structs. */

  char inherit_scale_mode;
  char _pad[3];

  /** Head position in armature space. So should be the same as head in edit mode. */
  float arm_head[3];
  /** Tail position in armature space. So should be the same as tail in edit mode. */
  float arm_tail[3];
  /** Matrix: `(bone_mat(b)+head(b))*arm_mat(b-1)`, rest pose in armature space. */
  float arm_mat[4][4];
  /** Roll in Armature Space (rest pose). */
  float arm_roll;

  /** Envelope distance, added to rad_head / rad_tail. */
  float dist;
  /** Weight: for non-deformgroup deforms. */
  float weight;
  /**
   * The width for block bones. The final X/Z bone widths are double these values.
   *
   * \note keep in this order for transform code which stores a pointer to `xwidth`,
   * accessing length and `zwidth` as offsets.
   */
  float xwidth, length, zwidth;
  /**
   * Radius for head/tail sphere, defining deform as well,
   * `parent->rad_tip` overrides `rad_head`.
   */
  float rad_head, rad_tail;

  /** Curved bones settings - these define the "rest-pose" for a curved bone. */
  float roll1, roll2;
  float curve_in_x, curve_in_z;
  float curve_out_x, curve_out_z;
  /** Length of bezier handles. */
  float ease1, ease2;
  float scale_in_x DNA_DEPRECATED, scale_in_z DNA_DEPRECATED;
  float scale_out_x DNA_DEPRECATED, scale_out_z DNA_DEPRECATED;
  float scale_in[3], scale_out[3];

  /** Patch for upward compatibility, UNUSED! */
  float size[3];
  /** Layers that bone appears on. */
  int layer;
  /** For B-bones. */
  short segments;
  /** Vertex to segment mapping mode. */
  char bbone_mapping_mode;
  char _pad2[7];

  /** Type of next/prev bone handles. */
  char bbone_prev_type;
  char bbone_next_type;
  /** B-Bone flags. */
  int bbone_flag;
  short bbone_prev_flag;
  short bbone_next_flag;
  /** Next/prev bones to use as handle references when calculating bbones (optional). */
  struct Bone *bbone_prev;
  struct Bone *bbone_next;

  /* Keep last. */
  Bone_Runtime runtime;
} Bone;

typedef struct bArmature_Runtime {
  /**
   * Index of the active collection, -1 if there is no collection active.
   *
   * For UIList support in the user interface. Assigning here does nothing, use
   * `ANIM_armature_bonecoll_active_set` to set the active bone collection.
   */
  int active_collection_index;
  uint8_t _pad0[4];
  struct BoneCollection *active_collection;
} bArmature_Runtime;

typedef struct bArmature {
#ifdef __cplusplus
  /** See #ID_Type comment for why this is here. */
  static constexpr ID_Type id_type = ID_AR;
#endif

  ID id;
  struct AnimData *adt;

  ListBase bonebase;

  /** Use a hash-table for quicker lookups of bones by name. */
  struct GHash *bonehash;
  void *_pad1;

  /** #EditBone list (use an allocated pointer so the state can be checked). */
  ListBase *edbo;

  /* active bones should work like active object where possible
   * - active and selection are unrelated
   * - active & hidden is not allowed
   * - from the user perspective active == last selected
   * - active should be ignored when not visible (hidden layer) */

  /** Active bone. */
  Bone *act_bone;
  /** Active edit-bone (in edit-mode). */
  struct EditBone *act_edbone;

  /** ID data is older than edit-mode data (TODO: move to edit-mode struct). */
  char needs_flush_to_id;
  char _pad0[3];

  int flag;
  int drawtype; /* eArmature_Drawtype */

  short deformflag;
  short pathflag;

  /** This is used only for reading/writing BoneCollections in blend
   * files, for forwards/backwards compatibility with Blender 4.0. It
   * should always be empty at runtime. Use collection_array for
   * everything other than file reading/writing.
   * TODO: remove this in Blender 5.0, and instead write the contents of
   * collection_array to blend files directly. */
  ListBase collections_legacy; /* BoneCollection. */

  struct BoneCollection **collection_array; /* Array of `collection_array_num` BoneCollections. */
  int collection_array_num;
  /**
   * Number of root bone collections.
   *
   * `collection_array[0:collection_root_count]` are the collections without a parent collection.
   */
  int collection_root_count;

  /** Do not directly assign, use `ANIM_armature_bonecoll_active_set` instead.
   * This is stored as a string to make it possible for the library overrides system to understand
   * when it actually changed (compared to a BoneCollection*, which would change on every load).
   */
  char active_collection_name[/*MAX_NAME*/ 64];

  /** For UI, to show which layers are there. */
  unsigned int layer_used DNA_DEPRECATED;
  /** For buttons to work, both variables in this order together. */
  unsigned int layer DNA_DEPRECATED, layer_protected DNA_DEPRECATED;

  /** Relative position of the axes on the bone, from head (0.0f) to tail (1.0f). */
  float axes_position;

  /** Keep last, for consistency with the position of other DNA runtime structures. */
  struct bArmature_Runtime runtime;

#ifdef __cplusplus
  /* Collection array access for convenient for-loop iteration. */
  blender::Span<const BoneCollection *> collections_span() const;
  blender::Span<BoneCollection *> collections_span();

  /* Span of all root collections. */
  blender::Span<const BoneCollection *> collections_roots() const;
  blender::Span<BoneCollection *> collections_roots();

  /* Return the span of children of the given bone collection. */
  blender::Span<const BoneCollection *> collection_children(const BoneCollection *parent) const;
  blender::Span<BoneCollection *> collection_children(BoneCollection *parent);
#endif
} bArmature;

/**
 * Collection of Bones within an Armature.
 *
 * BoneCollections are owned by their Armature, and cannot be shared between
 * different armatures.
 *
 * Bones can be in more than one collection at a time.
 *
 * Selectability and visibility of bones are determined by OR-ing the collection
 * flags.
 */
typedef struct BoneCollection {
  struct BoneCollection *next, *prev;

  char name[/*MAX_NAME*/ 64];

  /** BoneCollectionMember. */
  ListBase bones;

  /** eBoneCollection_Flag. */
  uint8_t flags;
  uint8_t _pad0[7];

  /*
   * Hierarchy information. The Armature has an array of BoneCollection pointers. These are ordered
   * such that siblings are always stored in consecutive array elements.
   */
  /** Array index of the first child of this BoneCollection. */
  int child_index;
  /** Number of children of this BoneCollection. */
  int child_count;

  /** Custom properties. */
  struct IDProperty *prop;
  /** Custom system IDProperties. */
  struct IDProperty *system_properties;

#ifdef __cplusplus
  /**
   * Return whether this collection is marked as 'visible'.
   *
   * Note that its effective visibility depends on the visibility of its ancestors as well.
   *
   * \see is_visible_with_ancestors
   * \see ANIM_bonecoll_show
   * \see ANIM_bonecoll_hide
   */
  bool is_visible() const;

  /**
   * Return whether this collection's ancestors are visible or not.
   *
   * \see is_visible_with_ancestors
   */
  bool is_visible_ancestors() const;

  /**
   * Return whether this collection is visible, taking into account the
   * visibility of its ancestors.
   *
   * \return true when this collection and all its ancestors are visible.
   *
   * \see is_visible
   */
  bool is_visible_with_ancestors() const;

  /**
   * Return whether this collection is marked as 'solo'.
   */
  bool is_solo() const;
  /**
   * Whether or not this bone collection is expanded in the tree view.
   *
   * This corresponds to the #BONE_COLLECTION_EXPANDED flag.
   */
  bool is_expanded() const;
#endif
} BoneCollection;

/** Membership relation of a bone with a bone collection. */
typedef struct BoneCollectionMember {
  struct BoneCollectionMember *next, *prev;
  struct Bone *bone;
} BoneCollectionMember;

/**
 * Membership relation of a bone with its collections.
 *
 * This is only bone-runtime data for easy lookups, the actual membership is
 * stored on the #bArmature in #BoneCollectionMember structs.
 */
typedef struct BoneCollectionReference {
  struct BoneCollectionReference *next, *prev;
  struct BoneCollection *bcoll;
} BoneCollectionReference;

/* armature->flag */
/* don't use bit 7, was saved in files to disable stuff */
typedef enum eArmature_Flag {
  ARM_RESTPOS = (1 << 0),
  /** XRAY is here only for backwards converting */
  ARM_FLAG_UNUSED_1 = (1 << 1), /* cleared */
  ARM_DRAWAXES = (1 << 2),
  ARM_DRAWNAMES = (1 << 3),
  /* ARM_POSEMODE = (1 << 4), Deprecated. */
  /** Position of the parent-child relation lines on the bone (cleared = drawn
   * from the tail, set = drawn from the head). Only controls the parent side of
   * the line; the child side is always drawn to the head of the bone. */
  ARM_DRAW_RELATION_FROM_HEAD = (1 << 5), /* Cleared in versioning of pre-2.80 files. */
  /**
   * Whether any bone collection is marked with the 'solo' flag.
   * When this is the case, bone collection visibility flags don't matter any more, and only ones
   * that have their 'solo' flag set will be visible.
   *
   * \see eBoneCollection_Flag::BONE_COLLECTION_SOLO */
  ARM_BCOLL_SOLO_ACTIVE = (1 << 6), /* Cleared in versioning of pre-2.80 files. */
  ARM_FLAG_UNUSED_7 = (1 << 7),     /* cleared */
  ARM_MIRROR_EDIT = (1 << 8),
  ARM_FLAG_UNUSED_9 = (1 << 9),
  /** Made option negative, for backwards compatibility. */
  ARM_NO_CUSTOM = (1 << 10),
  /** Draw custom colors. */
  ARM_COL_CUSTOM = (1 << 11),
  /** When ghosting, only show selected bones (this should belong to ghostflag instead). */
  ARM_FLAG_UNUSED_12 = (1 << 12), /* cleared */
  /** Dope-sheet channel is expanded */
  ARM_DS_EXPAND = (1 << 13),
  /** Other objects are used for visualizing various states (hack for efficient updates). */
  ARM_HAS_VIZ_DEPS = (1 << 14),
} eArmature_Flag;

/* armature->drawtype */
typedef enum eArmature_Drawtype {
  ARM_DRAW_TYPE_ARMATURE_DEFINED = -1, /* Use draw type from Armature (only used on Bones). */
  ARM_DRAW_TYPE_OCTA = 0,
  ARM_DRAW_TYPE_STICK = 1,
  ARM_DRAW_TYPE_B_BONE = 2,
  ARM_DRAW_TYPE_ENVELOPE = 3,
  ARM_DRAW_TYPE_WIRE = 4,
} eArmature_Drawtype;

/* armature->deformflag */
typedef enum eArmature_DeformFlag {
  ARM_DEF_VGROUP = (1 << 0),
  ARM_DEF_ENVELOPE = (1 << 1),
  ARM_DEF_QUATERNION = (1 << 2),
#ifdef DNA_DEPRECATED_ALLOW
  ARM_DEF_B_BONE_REST = (1 << 3), /* deprecated */
#endif
  ARM_DEF_INVERT_VGROUP = (1 << 4),
} eArmature_DeformFlag;

#ifdef DNA_DEPRECATED_ALLOW /* Old animation system (armature only viz). */
/** #bArmature.pathflag */
typedef enum eArmature_PathFlag {
  ARM_PATH_FNUMS = (1 << 0),
  ARM_PATH_KFRAS = (1 << 1),
  ARM_PATH_HEADS = (1 << 2),
  ARM_PATH_ACFRA = (1 << 3),
  ARM_PATH_KFNOS = (1 << 4),
} eArmature_PathFlag;
#endif

/* bone->flag */
typedef enum eBone_Flag {
  /**
   * Bone selection, must only be set when the bone is not hidden
   * (#BONE_HIDDEN_A / #BONE_HIDDEN_P flags must not be enabled as well).
   *
   * However the bone may not be visible to the user since the bones collection
   * may be hidden.
   * In most cases `blender::animrig::bone_is_visible` or
   * `blender::animrig::bone_is_visible` should be used to check if the bone is visible to
   * the user before operating on them.
   */
  BONE_SELECTED = (1 << 0),
  BONE_ROOTSEL = (1 << 1),
  BONE_TIPSEL = (1 << 2),
  /** Used instead of BONE_SELECTED during transform (clear before use) */
  BONE_TRANSFORM = (1 << 3),
  /** When bone has a parent, connect head of bone to parent's tail. */
  BONE_CONNECTED = (1 << 4),
  /* 32 used to be quatrot, was always set in files, do not reuse unless you clear it always */
  /**
   * Hidden Bones when drawing PoseChannels.
   * When set #BONE_SELECTED must be cleared.
   */
  BONE_HIDDEN_P = (1 << 6),
  /** For detecting cyclic dependencies */
  BONE_DONE = (1 << 7),
  /** active is on mouse clicks only - deprecated, ONLY USE FOR DRAWING */
  BONE_DRAW_ACTIVE = (1 << 8),
  /** No parent rotation or scale */
  BONE_HINGE = (1 << 9),
  /**
   * Hidden Bones when drawing Armature edit-mode.
   * When set, selection flags (#BONE_SELECTED, #BONE_ROOTSEL & BONE_TIPSEL) must be cleared.
   */
  BONE_HIDDEN_A = (1 << 10),
  /** multiplies vgroup with envelope */
  BONE_MULT_VG_ENV = (1 << 11),
  /** bone doesn't deform geometry */
  BONE_NO_DEFORM = (1 << 12),
#ifdef DNA_DEPRECATED_ALLOW
  /** set to prevent destruction of its unkeyframed pose (after transform) */
  BONE_UNKEYED = (1 << 13),
  /** set to prevent hinge child bones from influencing the transform center */
  BONE_HINGE_CHILD_TRANSFORM = (1 << 14),
  /** No parent scale */
  BONE_NO_SCALE = (1 << 15),
#endif
  /** bone should be drawn as OB_WIRE, regardless of draw-types of view+armature */
  BONE_DRAWWIRE = (1 << 17),
  /** when no parent, bone will not get cyclic offset */
  BONE_NO_CYCLICOFFSET = (1 << 18),
  /** bone transforms are locked in EditMode */
  BONE_EDITMODE_LOCKED = (1 << 19),
#ifdef DNA_DEPRECATED_ALLOW
  /** Indicates that a parent is also being transformed */
  BONE_TRANSFORM_CHILD = (1 << 20),
#endif
  /** bone cannot be selected */
  BONE_UNSELECTABLE = (1 << 21),
  /** bone location is in armature space */
  BONE_NO_LOCAL_LOCATION = (1 << 22),
  /** object child will use relative transform (like deform) */
  BONE_RELATIVE_PARENTING = (1 << 23),
#ifdef DNA_DEPRECATED_ALLOW
  /** it will add the parent end roll to the inroll */
  BONE_ADD_PARENT_END_ROLL = (1 << 24),
#endif
  /** this bone was transformed by the mirror function */
  BONE_TRANSFORM_MIRROR = (1 << 25),
  /** this bone is associated with a locked vertex group, ONLY USE FOR DRAWING */
  BONE_DRAW_LOCKED_WEIGHT = (1 << 26),
} eBone_Flag;
ENUM_OPERATORS(eBone_Flag)

/* bone->inherit_scale_mode */
typedef enum eBone_InheritScaleMode {
  /* Inherit all scale and shear. */
  BONE_INHERIT_SCALE_FULL = 0,
  /* Inherit scale, but remove final shear. */
  BONE_INHERIT_SCALE_FIX_SHEAR = 1,
  /* Inherit average scale. */
  BONE_INHERIT_SCALE_AVERAGE = 2,
  /* Inherit no scale or shear. */
  BONE_INHERIT_SCALE_NONE = 3,
  /* Inherit effects of shear on parent (same as old disabled Inherit Scale). */
  BONE_INHERIT_SCALE_NONE_LEGACY = 4,
  /* Inherit parent X scale as child X scale etc. */
  BONE_INHERIT_SCALE_ALIGNED = 5,
} eBone_InheritScaleMode;

/* bone->bbone_prev_type, bbone_next_type */
typedef enum eBone_BBoneHandleType {
  BBONE_HANDLE_AUTO = 0,     /* Default mode based on parents & children. */
  BBONE_HANDLE_ABSOLUTE = 1, /* Custom handle in absolute position mode. */
  BBONE_HANDLE_RELATIVE = 2, /* Custom handle in relative position mode. */
  BBONE_HANDLE_TANGENT = 3,  /* Custom handle in tangent mode (use direction, not location). */
} eBone_BBoneHandleType;

/* bone->bbone_mapping_mode */
typedef enum eBone_BBoneMappingMode {
  BBONE_MAPPING_STRAIGHT = 0, /* Default mode that ignores the rest pose curvature. */
  BBONE_MAPPING_CURVED = 1,   /* Mode that takes the rest pose curvature into account. */
} eBone_BBoneMappingMode;

/* bone->bbone_flag */
typedef enum eBone_BBoneFlag {
  /** Add the parent Out roll to the In roll. */
  BBONE_ADD_PARENT_END_ROLL = (1 << 0),
  /** Multiply B-Bone easing values with Scale Length. */
  BBONE_SCALE_EASING = (1 << 1),
} eBone_BBoneFlag;

/* bone->bbone_prev/next_flag */
typedef enum eBone_BBoneHandleFlag {
  /** Use handle bone scaling for scale X. */
  BBONE_HANDLE_SCALE_X = (1 << 0),
  /** Use handle bone scaling for scale Y (length). */
  BBONE_HANDLE_SCALE_Y = (1 << 1),
  /** Use handle bone scaling for scale Z. */
  BBONE_HANDLE_SCALE_Z = (1 << 2),
  /** Use handle bone scaling for easing. */
  BBONE_HANDLE_SCALE_EASE = (1 << 3),
  /** Is handle scale required? */
  BBONE_HANDLE_SCALE_ANY = BBONE_HANDLE_SCALE_X | BBONE_HANDLE_SCALE_Y | BBONE_HANDLE_SCALE_Z |
                           BBONE_HANDLE_SCALE_EASE,
} eBone_BBoneHandleFlag;

#define MAXBONENAME 64

/** #BoneCollection.flag */
typedef enum eBoneCollection_Flag {
  BONE_COLLECTION_VISIBLE = (1 << 0),    /* Visibility flag of this particular collection. */
  BONE_COLLECTION_SELECTABLE = (1 << 1), /* Intended to be implemented in the not-so-far future. */
  BONE_COLLECTION_OVERRIDE_LIBRARY_LOCAL = (1 << 2), /* Added by a local library override. */

  /**
   * Set when all ancestors are visible.
   *
   * This would actually be a runtime flag, but bone collections don't have a
   * runtime struct yet, and the addition of one more flag doesn't seem worth
   * the effort. */
  BONE_COLLECTION_ANCESTORS_VISIBLE = (1 << 3),

  /**
   * Whether this bone collection is marked as 'solo'.
   *
   * If no bone collections have this flag set, visibility is determined by
   * BONE_COLLECTION_VISIBLE.
   *
   * If there is any bone collection with the BONE_COLLECTION_SOLO flag enabled, all bone
   * collections are effectively hidden, except other collections with this flag enabled.
   *
   * \see eArmature_Flag::ARM_BCOLL_SOLO_ACTIVE
   */
  BONE_COLLECTION_SOLO = (1 << 4),

  BONE_COLLECTION_EXPANDED = (1 << 5), /* Expanded in the tree view. */
} eBoneCollection_Flag;
ENUM_OPERATORS(eBoneCollection_Flag)

#ifdef __cplusplus

inline blender::animrig::BoneColor &BoneColor::wrap()
{
  return *reinterpret_cast<blender::animrig::BoneColor *>(this);
}
inline const blender::animrig::BoneColor &BoneColor::wrap() const
{
  return *reinterpret_cast<const blender::animrig::BoneColor *>(this);
}
#endif
