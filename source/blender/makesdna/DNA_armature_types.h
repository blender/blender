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

#ifndef __DNA_ARMATURE_TYPES_H__
#define __DNA_ARMATURE_TYPES_H__

#include "DNA_defs.h"
#include "DNA_listBase.h"
#include "DNA_ID.h"

struct AnimData;

/* this system works on different transformation space levels;
 *
 * 1) Bone Space;      with each Bone having own (0,0,0) origin
 * 2) Armature Space;  the rest position, in Object space, Bones Spaces are applied hierarchical
 * 3) Pose Space;      the animation position, in Object space
 * 4) World Space;     Object matrix applied to Pose or Armature space
 */

typedef struct Bone {
  /**  Next/prev elements within this list. */
  struct Bone *next, *prev;
  /** User-Defined Properties on this Bone. */
  IDProperty *prop;
  /**  Parent (ik parent if appropriate flag is set. */
  struct Bone *parent;
  /**  Children   . */
  ListBase childbase;
  /**  Name of the bone - must be unique within the armature, MAXBONENAME. */
  char name[64];

  /**  roll is input for editmode, length calculated. */
  float roll;
  float head[3];
  /**  head/tail and roll in Bone Space   . */
  float tail[3];
  /**  rotation derived from head/tail/roll. */
  float bone_mat[3][3];

  int flag;

  float arm_head[3];
  /**  head/tail in Armature Space (rest pos). */
  float arm_tail[3];
  /**  matrix: (bonemat(b)+head(b))*arm_mat(b-1), rest po.s*/
  float arm_mat[4][4];
  /** Roll in Armature Space (rest pos). */
  float arm_roll;

  /**  dist, weight: for non-deformgroup deforms. */
  float dist, weight;
  /**  width: for block bones. keep in this order, transform!. */
  float xwidth, length, zwidth;
  /** Radius for head/tail sphere, defining deform as well, parent->rad_tip overrides rad_head. */
  float rad_head, rad_tail;

  /** Curved bones settings - these define the "restpose" for a curved bone. */
  float roll1, roll2;
  float curve_in_x, curve_in_y;
  float curve_out_x, curve_out_y;
  /** Length of bezier handles. */
  float ease1, ease2;
  float scale_in_x, scale_in_y;
  float scale_out_x, scale_out_y;

  /**  patch for upward compat, UNUSED!. */
  float size[3];
  /** Layers that bone appears on. */
  int layer;
  /**  for B-bones. */
  short segments;

  /** Type of next/prev bone handles. */
  char bbone_prev_type;
  char bbone_next_type;
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
  /** Active editbone (in editmode). */
  struct EditBone *act_edbone;

  int flag;
  int drawtype;
  /** How vertex deformation is handled in the ge. */
  int gevertdeformer;
  char _pad[4];
  short deformflag;
  short pathflag;

  /** For UI, to show which layers are there. */
  unsigned int layer_used;
  /** For buttons to work, both variables in this order together. */
  unsigned int layer, layer_protected;
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
  ARM_DELAYDEFORM = (1 << 6),
  ARM_FLAG_UNUSED_7 = (1 << 7),
  ARM_MIRROR_EDIT = (1 << 8),
  ARM_FLAG_UNUSED_9 = (1 << 9),
  /** made option negative, for backwards compat */
  ARM_NO_CUSTOM = (1 << 10),
  /** draw custom colors  */
  ARM_COL_CUSTOM = (1 << 11),
  /** when ghosting, only show selected bones (this should belong to ghostflag instead) */
  ARM_FLAG_UNUSED_12 = (1 << 12), /* cleared */
  /** dopesheet channel is expanded */
  ARM_DS_EXPAND = (1 << 13),
  /** other objects are used for visualizing various states (hack for efficient updates) */
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

/* armature->gevertdeformer */
typedef enum eArmature_VertDeformer {
  ARM_VDEF_BLENDER = 0,
  ARM_VDEF_BGE_CPU = 1,
} eArmature_VertDeformer;

/* armature->deformflag */
typedef enum eArmature_DeformFlag {
  ARM_DEF_VGROUP = (1 << 0),
  ARM_DEF_ENVELOPE = (1 << 1),
  ARM_DEF_QUATERNION = (1 << 2),
#ifdef DNA_DEPRECATED
  ARM_DEF_B_BONE_REST = (1 << 3), /* deprecated */
#endif
  ARM_DEF_INVERT_VGROUP = (1 << 4),
} eArmature_DeformFlag;

/* armature->pathflag */
// XXX deprecated... old animation system (armature only viz)
#ifdef DNA_DEPRECATED
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
  /** when bone has a parent, connect head of bone to parent's tail*/
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
  /** set to prevent destruction of its unkeyframed pose (after transform) */
  BONE_UNKEYED = (1 << 13),
  /** set to prevent hinge child bones from influencing the transform center */
  BONE_HINGE_CHILD_TRANSFORM = (1 << 14),
  /** No parent scale */
  BONE_NO_SCALE = (1 << 15),
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
  /** it will add the parent end roll to the inroll */
  BONE_ADD_PARENT_END_ROLL = (1 << 24),
  /** this bone was transformed by the mirror function */
  BONE_TRANSFORM_MIRROR = (1 << 25),

} eBone_Flag;

/* bone->bbone_prev_type, bbone_next_type */
typedef enum eBone_BBoneHandleType {
  BBONE_HANDLE_AUTO = 0, /* Default mode based on parents & children. */
  BBONE_HANDLE_ABSOLUTE, /* Custom handle in absolute position mode. */
  BBONE_HANDLE_RELATIVE, /* Custom handle in relative position mode. */
  BBONE_HANDLE_TANGENT,  /* Custom handle in tangent mode (use direction, not location). */
} eBone_BBoneHandleType;

#define MAXBONENAME 64

#endif
