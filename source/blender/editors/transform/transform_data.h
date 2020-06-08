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
 * \ingroup edtransform
 */

#ifndef __TRANSFORM_DATA_H__
#define __TRANSFORM_DATA_H__

struct bConstraint;
struct Object;

#define TRANSDATABASIC \
  /** Extra data (mirrored element pointer, in editmode mesh to BMVert) \
   * (editbone for roll fixing) (...). */ \
  void *extra; \
  /** Location of the data to transform. */ \
  float *loc; \
  /** Initial location. */ \
  float iloc[3]; \
  /** Individual data center. */ \
  float center[3]; \
  /** Various flags. */ \
  int flag

typedef struct TransDataBasic {
  TRANSDATABASIC;
} TransDataBasic;

typedef struct TransDataMirror {
  TRANSDATABASIC;
  // int pad;
  /** Location of the data to transform. */
  float *loc_src;
} TransDataMirror;

typedef struct TransDataExtension {
  /** Initial object drot. */
  float drot[3];
  // /* Initial object drotAngle,    TODO: not yet implemented */
  // float drotAngle;
  // /* Initial object drotAxis, TODO: not yet implemented */
  // float drotAxis[3];
  /** Initial object delta quat. */
  float dquat[4];
  /** Initial object delta scale. */
  float dscale[3];
  /** Rotation of the data to transform. */
  float *rot;
  /** Initial rotation. */
  float irot[3];
  /** Rotation quaternion of the data to transform. */
  float *quat;
  /** Initial rotation quaternion. */
  float iquat[4];
  /** Rotation angle of the data to transform. */
  float *rotAngle;
  /** Initial rotation angle. */
  float irotAngle;
  /** Rotation axis of the data to transform. */
  float *rotAxis;
  /** Initial rotation axis. */
  float irotAxis[4];
  /** Size of the data to transform. */
  float *size;
  /** Initial size. */
  float isize[3];
  /** Object matrix. */
  float obmat[4][4];
  /** Use instead of #TransData.smtx,
   * It is the same but without the #Bone.bone_mat, see #TD_PBONE_LOCAL_MTX_C. */
  float l_smtx[3][3];
  /** The rotscale matrix of pose bone, to allow using snap-align in translation mode,
   * when td->mtx is the loc pose bone matrix (and hence can't be used to apply
   * rotation in some cases, namely when a bone is in "NoLocal" or "Hinge" mode)... */
  float r_mtx[3][3];
  /** Inverse of previous one. */
  float r_smtx[3][3];
  /** Rotation mode, as defined in #eRotationModes (DNA_action_types.h). */
  int rotOrder;
  /** Original object transformation used for rigid bodies. */
  float oloc[3], orot[3], oquat[4], orotAxis[3], orotAngle;
} TransDataExtension;

typedef struct TransData2D {
  /** Location of data used to transform (x,y,0). */
  float loc[3];
  /** Pointer to real 2d location of data. */
  float *loc2d;

  /** Pointer to handle locations, if handles aren't being moved independently. */
  float *h1, *h2;
  float ih1[2], ih2[2];
} TransData2D;

/**
 * Used to store 2 handles for each #TransData in case the other handle wasn't selected.
 * Also to unset temporary flags.
 */
typedef struct TransDataCurveHandleFlags {
  char ih1, ih2;
  char *h1, *h2;
} TransDataCurveHandleFlags;

typedef struct TransData {
  TRANSDATABASIC;
  /** Distance needed to affect element (for Proportionnal Editing). */
  float dist;
  /** Distance to the nearest element (for Proportionnal Editing). */
  float rdist;
  /** Factor of the transformation (for Proportionnal Editing). */
  float factor;
  /** Value pointer for special transforms. */
  float *val;
  /** Old value. */
  float ival;
  /** Transformation matrix from data space to global space. */
  float mtx[3][3];
  /** Transformation matrix from global space to data space. */
  float smtx[3][3];
  /** Axis orientation matrix of the data. */
  float axismtx[3][3];
  struct Object *ob;
  /** For objects/bones, the first constraint in its constraint stack. */
  struct bConstraint *con;
  /** For objects, poses. 1 single malloc per TransInfo! */
  TransDataExtension *ext;
  /** for curves, stores handle flags for modification/cancel. */
  TransDataCurveHandleFlags *hdata;
  /** If set, copy of Object or PoseChannel protection. */
  short protectflag;
} TransData;

/** #TransData.flag */
enum {
  TD_SELECTED = 1 << 0,
  TD_USEQUAT = 1 << 1,
  TD_NOTCONNECTED = 1 << 2,
  /** Used for scaling of #MetaElem.rad */
  TD_SINGLESIZE = 1 << 3,
  /** Scale relative to individual element center */
  TD_INDIVIDUAL_SCALE = 1 << 4,
  TD_NOCENTER = 1 << 5,
  /** #TransData.ext abused for particle key timing. */
  TD_NO_EXT = 1 << 6,
  /** don't transform this data */
  TD_SKIP = 1 << 7,
  /** if this is a bez triple, we need to restore the handles,
   * if this is set #TransData.hdata needs freeing */
  TD_BEZTRIPLE = 1 << 8,
  /** when this is set, don't apply translation changes to this element */
  TD_NO_LOC = 1 << 9,
  /** For Graph Editor autosnap, indicates that point should not undergo autosnapping */
  TD_NOTIMESNAP = 1 << 10,
  /** For Graph Editor - curves that can only have int-values
   * need their keyframes tagged with this. */
  TD_INTVALUES = 1 << 11,
  /** For editmode mirror. */
  TD_MIRROR_X = 1 << 12,
  TD_MIRROR_Y = 1 << 13,
  TD_MIRROR_Z = 1 << 14,
  /** For editmode mirror, clamp axis to 0 */
  TD_MIRROR_EDGE_X = 1 << 12,
  TD_MIRROR_EDGE_Y = 1 << 13,
  TD_MIRROR_EDGE_Z = 1 << 14,
  /** For fcurve handles, move them along with their keyframes */
  TD_MOVEHANDLE1 = 1 << 15,
  TD_MOVEHANDLE2 = 1 << 16,
  /** Exceptional case with pose bone rotating when a parent bone has 'Local Location'
   * option enabled and rotating also transforms it. */
  TD_PBONE_LOCAL_MTX_P = 1 << 17,
  /** Same as above but for a child bone. */
  TD_PBONE_LOCAL_MTX_C = 1 << 18,
};

/* Hard min/max for proportional size. */
#define T_PROP_SIZE_MIN 1e-6f
#define T_PROP_SIZE_MAX 1e12f

#endif
