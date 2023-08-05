/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#pragma once

struct Object;
struct bConstraint;

#define TRANSDATABASIC \
  /** Extra data (mirrored element pointer, in edit-mode mesh to #BMVert) \
   * (edit-bone for roll fixing) (...). */ \
  void *extra; \
  /** Location of the data to transform. */ \
  float *loc; \
  /** Initial location. */ \
  float iloc[3]; \
  /** Individual data center. */ \
  float center[3]; \
  /** Value pointer for special transforms. */ \
  float *val; \
  /** Old value. */ \
  float ival; \
  /** Various flags. */ \
  int flag

struct TransDataBasic {
  TRANSDATABASIC;
};

struct TransDataMirror {
  TRANSDATABASIC;
  // int pad;
  /** Location of the data to transform. */
  float *loc_src;
};

struct TransDataExtension {
  /** Initial object drot. */
  float drot[3];
#if 0 /* TODO: not yet implemented */
  /* Initial object drotAngle */
  float drotAngle;
  /* Initial object drotAxis */
  float drotAxis[3];
#endif
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
  /** Use for #V3D_ORIENT_GIMBAL orientation. */
  float axismtx_gimbal[3][3];
  /** Use instead of #TransData.smtx,
   * It is the same but without the #Bone.bone_mat, see #TD_PBONE_LOCAL_MTX_C. */
  float l_smtx[3][3];
  /** The rotscale matrix of pose bone, to allow using snap-align in translation mode,
   * when #TransData.mtx is the loc pose bone matrix (and hence can't be used to apply
   * rotation in some cases, namely when a bone is in "No-Local" or "Hinge" mode)... */
  float r_mtx[3][3];
  /** Inverse of previous one. */
  float r_smtx[3][3];
  /** Rotation mode, as defined in #eRotationModes (DNA_action_types.h). */
  int rotOrder;
  /** Original object transformation used for rigid bodies. */
  float oloc[3], orot[3], oquat[4], orotAxis[3], orotAngle;
};

struct TransData2D {
  /** Location of data used to transform (x,y,0). */
  float loc[3];
  /** Pointer to real 2d location of data. */
  float *loc2d;

  /** Pointer to handle locations, if handles aren't being moved independently. */
  float *h1, *h2;
  float ih1[2], ih2[2];
};

/**
 * Used to store 2 handles for each #TransData in case the other handle wasn't selected.
 * Also to unset temporary flags.
 */
struct TransDataCurveHandleFlags {
  uint8_t ih1, ih2;
  uint8_t *h1, *h2;
};

struct TransData {
  TRANSDATABASIC;
  /** Distance needed to affect element (for Proportional Editing). */
  float dist;
  /** Distance to the nearest element (for Proportional Editing). */
  float rdist;
  /** Factor of the transformation (for Proportional Editing). */
  float factor;
  /** Transformation matrix from data space to global space. */
  float mtx[3][3];
  /** Transformation matrix from global space to data space. */
  float smtx[3][3];
  /** Axis orientation matrix of the data. */
  float axismtx[3][3];
  Object *ob;
  /** For objects/bones, the first constraint in its constraint stack. */
  bConstraint *con;
  /** For objects, poses. 1 single allocation per #TransInfo! */
  TransDataExtension *ext;
  /** for curves, stores handle flags for modification/cancel. */
  TransDataCurveHandleFlags *hdata;
  /** If set, copy of Object or #bPoseChannel protection. */
  short protectflag;
};

#define TRANSDATA_THREAD_LIMIT 1024

/** #TransData.flag */
enum {
  TD_SELECTED = 1 << 0,
  TD_USEQUAT = 1 << 1,
  /* TD_NOTCONNECTED = 1 << 2, */
  /** Used for scaling of #MetaElem.rad */
  TD_SINGLESIZE = 1 << 3,
  /** Scale relative to individual element center. */
  TD_INDIVIDUAL_SCALE = 1 << 4,
  TD_NOCENTER = 1 << 5,
  /** #TransData.ext abused for particle key timing. */
  TD_NO_EXT = 1 << 6,
  /** Don't transform this data. */
  TD_SKIP = 1 << 7,
  /**
   * If this is a bezier triple, we need to restore the handles,
   * if this is set #TransData.hdata needs freeing.
   */
  TD_BEZTRIPLE = 1 << 8,
  /** when this is set, don't apply translation changes to this element */
  TD_NO_LOC = 1 << 9,
  /** For Graph Editor auto-snap, indicates that point should not undergo auto-snapping. */
  TD_NOTIMESNAP = 1 << 10,
  /**
   * For Graph Editor - curves that can only have int-values
   * need their keyframes tagged with this.
   */
  TD_INTVALUES = 1 << 11,
#define TD_MIRROR_AXIS_SHIFT 12
  /** For edit-mode mirror. */
  TD_MIRROR_X = 1 << 12,
  TD_MIRROR_Y = 1 << 13,
  TD_MIRROR_Z = 1 << 14,
#define TD_MIRROR_EDGE_AXIS_SHIFT 12
  /** For edit-mode mirror, clamp axis to 0. */
  TD_MIRROR_EDGE_X = 1 << 12,
  TD_MIRROR_EDGE_Y = 1 << 13,
  TD_MIRROR_EDGE_Z = 1 << 14,
  /** For F-curve handles, move them along with their keyframes. */
  TD_MOVEHANDLE1 = 1 << 15,
  TD_MOVEHANDLE2 = 1 << 16,
  /**
   * Exceptional case with pose bone rotating when a parent bone has 'Local Location'
   * option enabled and rotating also transforms it.
   */
  TD_PBONE_LOCAL_MTX_P = 1 << 17,
  /** Same as above but for a child bone. */
  TD_PBONE_LOCAL_MTX_C = 1 << 18,
};

/* Hard min/max for proportional size. */
#define T_PROP_SIZE_MIN 1e-6f
#define T_PROP_SIZE_MAX 1e12f
