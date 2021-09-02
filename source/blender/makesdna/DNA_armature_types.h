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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_ID.h"
#include "DNA_defs.h"
#include "DNA_listBase.h"

#ifdef __cplusplus
extern "C" {
#endif

struct AnimData;

/* this system works on different transformation space levels;
 *
 * 1) Bone Space;      with each Bone having own (0,0,0) origin
 * 2) Armature Space;  the rest position, in Object space, Bones Spaces are applied hierarchical
 * 3) Pose Space;      the animation position, in Object space
 * 4) World Space;     Object matrix applied to Pose or Armature space
 */

typedef struct Bone {
  /** Next/previous elements within this list. */
  struct Bone *next, *prev;
  /** User-Defined Properties on this Bone. */
  IDProperty *prop;
  /** Parent (IK parent if appropriate flag is set). */
  struct Bone *parent;
  /** Children. */
  ListBase childbase;
  /** Name of the bone - must be unique within the armature, MAXBONENAME. */
  char name[64];

  /** Roll is input for edit-mode, length calculated. */
  float roll;
  float head[3];
  /** Head/tail and roll in Bone Space. */
  float tail[3];
  /** Rotation derived from head/tail/roll. */
  float bone_mat[3][3];

  int flag;

  char inherit_scale_mode;
  char _pad[7];

  float arm_head[3];
  /** Head/tail in Armature Space (rest pose). */
  float arm_tail[3];
  /** Matrix: `(bonemat(b)+head(b))*arm_mat(b-1)`, rest pose. */
  float arm_mat[4][4];
  /** Roll in Armature Space (rest pose). */
  float arm_roll;

  /** dist, weight: for non-deformgroup deforms. */
  float dist, weight;
  /**
   * The width for block bones.
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
} Bone;

typedef struct bArmature {
  ID id;
  struct AnimData *adt;

  ListBase bonebase;

  /** Ghash for quicker lookups of bones by name. */
  struct GHash *bonehash;
  void *_pad1;

  /** Editbone listbase, we use pointer so we can check state. */
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
  int drawtype;

  short deformflag;
  short pathflag;

  /** For UI, to show which layers are there. */
  unsigned int layer_used;
  /** For buttons to work, both variables in this order together. */
  unsigned int layer, layer_protected;

  /** Relative position of the axes on the bone, from head (0.0f) to tail (1.0f). */
  float axes_position;
} bArmature;

/* armature->flag */
/* don't use bit 7, was saved in files to disable stuff */
typedef enum eArmature_Flag {
  ARM_RESTPOS = (1 << 0),
  /** XRAY is here only for backwards converting */
  ARM_FLAG_UNUSED_1 = (1 << 1), /* cleared */
  ARM_DRAWAXES = (1 << 2),
  ARM_DRAWNAMES = (1 << 3),
  ARM_POSEMODE = (1 << 4),
  ARM_FLAG_UNUSED_5 = (1 << 5), /* cleared */
  ARM_FLAG_UNUSED_6 = (1 << 6), /* cleared */
  ARM_FLAG_UNUSED_7 = (1 << 7),
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
  ARM_OCTA = 0,
  ARM_LINE = 1,
  ARM_B_BONE = 2,
  ARM_ENVELOPE = 3,
  ARM_WIRE = 4,
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
  BONE_SELECTED = (1 << 0),
  BONE_ROOTSEL = (1 << 1),
  BONE_TIPSEL = (1 << 2),
  /** Used instead of BONE_SELECTED during transform (clear before use) */
  BONE_TRANSFORM = (1 << 3),
  /** When bone has a parent, connect head of bone to parent's tail. */
  BONE_CONNECTED = (1 << 4),
  /* 32 used to be quatrot, was always set in files, do not reuse unless you clear it always */
  /** hidden Bones when drawing PoseChannels */
  BONE_HIDDEN_P = (1 << 6),
  /** For detecting cyclic dependencies */
  BONE_DONE = (1 << 7),
  /** active is on mouse clicks only - deprecated, ONLY USE FOR DRAWING */
  BONE_DRAW_ACTIVE = (1 << 8),
  /** No parent rotation or scale */
  BONE_HINGE = (1 << 9),
  /** hidden Bones when drawing Armature Editmode */
  BONE_HIDDEN_A = (1 << 10),
  /** multiplies vgroup with envelope */
  BONE_MULT_VG_ENV = (1 << 11),
  /** bone doesn't deform geometry */
  BONE_NO_DEFORM = (1 << 12),
#ifdef DNA_DEPRECATED_ALLOW
  /** set to prevent destruction of its unkeyframed pose (after transform) */
  BONE_UNKEYED = (1 << 13),
#endif
  /** set to prevent hinge child bones from influencing the transform center */
  BONE_HINGE_CHILD_TRANSFORM = (1 << 14),
#ifdef DNA_DEPRECATED_ALLOW
  /** No parent scale */
  BONE_NO_SCALE = (1 << 15),
#endif
  /** hidden bone when drawing PoseChannels (for ghost drawing) */
  BONE_HIDDEN_PG = (1 << 16),
  /** bone should be drawn as OB_WIRE, regardless of draw-types of view+armature */
  BONE_DRAWWIRE = (1 << 17),
  /** when no parent, bone will not get cyclic offset */
  BONE_NO_CYCLICOFFSET = (1 << 18),
  /** bone transforms are locked in EditMode */
  BONE_EDITMODE_LOCKED = (1 << 19),
  /** Indicates that a parent is also being transformed */
  BONE_TRANSFORM_CHILD = (1 << 20),
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

#ifdef __cplusplus
}
#endif
