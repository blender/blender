/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 * \brief conversion and adaptation of different datablocks to a common struct.
 */

#pragma once

#include "RE_engine.h"

#ifdef __cplusplus
extern "C" {
#endif

struct BMEditMesh;
struct BMesh;
struct BezTriple;
struct ListBase;
struct Object;
struct TransData;
struct TransDataCurveHandleFlags;
struct TransInfo;
struct bContext;

typedef struct TransConvertTypeInfo {
  int flags; /* eTFlag */

  /**
   * Allocate and initialize `t->data`.
   */
  void (*createTransData)(bContext *C, TransInfo *t);

  /**
   * Force recalculation of data during transformation.
   */
  void (*recalcData)(TransInfo *t);

  /**
   * Called when the operation is finished.
   */
  void (*special_aftertrans_update)(bContext *C, TransInfo *t);
} TransConvertTypeInfo;

/* transform_convert.c */

/**
 * Change the chain-length of auto-IK.
 */
void transform_autoik_update(TransInfo *t, short mode);
int special_transform_moving(TransInfo *t);
/**
 * Inserting keys, point-cache, redraw events.
 */
void special_aftertrans_update(struct bContext *C, TransInfo *t);
void sort_trans_data_dist(TransInfo *t);
void createTransData(struct bContext *C, TransInfo *t);
void clipUVData(TransInfo *t);
void transform_convert_flush_handle2D(TransData *td, TransData2D *td2d, float y_fac);
/**
 * Called for updating while transform acts, once per redraw.
 */
void recalcData(TransInfo *t);

/* transform_convert_mesh.c */

void transform_convert_mesh_customdatacorrect_init(TransInfo *t);

/* transform_convert_sequencer.c */

void transform_convert_sequencer_channel_clamp(TransInfo *t, float r_val[2]);

/********************* intern **********************/

/* transform_convert.c */

bool transform_mode_use_local_origins(const TransInfo *t);
/**
 * Transforming around ourselves is no use, fallback to individual origins,
 * useful for curve/armatures.
 */
void transform_around_single_fallback_ex(TransInfo *t, int data_len_all);
void transform_around_single_fallback(TransInfo *t);
/**
 * Little helper function for ObjectToTransData used to give certain
 * constraints (ChildOf, FollowPath, and others that may be added)
 * inverse corrections for transform, so that they aren't in CrazySpace.
 * These particular constraints benefit from this, but others don't, hence
 * this semi-hack ;-)    - Aligorith
 */
bool constraints_list_needinv(TransInfo *t, ListBase *list);
void calc_distanceCurveVerts(TransData *head, TransData *tail, bool cyclic);
/**
 * Utility function for getting the handle data from bezier's.
 */
struct TransDataCurveHandleFlags *initTransDataCurveHandles(TransData *td, struct BezTriple *bezt);
/**
 * Used for `TFM_TIME_EXTEND`.
 */
char transform_convert_frame_side_dir_get(TransInfo *t, float cframe);
/**
 * This function tests if a point is on the "mouse" side of the cursor/frame-marking.
 */
bool FrameOnMouseSide(char side, float frame, float cframe);
void transform_convert_clip_mirror_modifier_apply(TransDataContainer *tc);
/**
 * For the realtime animation recording feature, handle overlapping data.
 */
void animrecord_check_state(TransInfo *t, struct ID *id);

/* transform_convert_action.c */

extern TransConvertTypeInfo TransConvertType_Action;

/* transform_convert_armature.c */

extern TransConvertTypeInfo TransConvertType_EditArmature;
extern TransConvertTypeInfo TransConvertType_Pose;

/**
 * Sets transform flags in the bones.
 * Returns total number of bones with #BONE_TRANSFORM.
 */
void transform_convert_pose_transflags_update(Object *ob, int mode, short around);

/* transform_convert_cursor.c */

extern TransConvertTypeInfo TransConvertType_CursorImage;
extern TransConvertTypeInfo TransConvertType_CursorSequencer;
extern TransConvertTypeInfo TransConvertType_Cursor3D;

/* transform_convert_curve.c */

extern TransConvertTypeInfo TransConvertType_Curve;

/* transform_convert_curves.cc */

extern TransConvertTypeInfo TransConvertType_Curves;

/* transform_convert_graph.c */

extern TransConvertTypeInfo TransConvertType_Graph;

/* transform_convert_gpencil_legacy.c */

extern TransConvertTypeInfo TransConvertType_GPencil;

/* transform_convert_lattice.c */

extern TransConvertTypeInfo TransConvertType_Lattice;

/* transform_convert_mask.c */

extern TransConvertTypeInfo TransConvertType_Mask;

/* transform_convert_mball.c */

extern TransConvertTypeInfo TransConvertType_MBall;

/* transform_convert_mesh.c */

extern TransConvertTypeInfo TransConvertType_Mesh;

struct TransIslandData {
  float (*center)[3];
  float (*axismtx)[3][3];
  int island_tot;
  int *island_vert_map;
};

struct MirrorDataVert {
  int index;
  int flag;
};

struct TransMirrorData {
  struct MirrorDataVert *vert_map;
  int mirror_elem_len;
};

struct TransMeshDataCrazySpace {
  float (*quats)[4];
  float (*defmats)[3][3];
};

void transform_convert_mesh_islands_calc(struct BMEditMesh *em,
                                         bool calc_single_islands,
                                         bool calc_island_center,
                                         bool calc_island_axismtx,
                                         struct TransIslandData *r_island_data);
void transform_convert_mesh_islanddata_free(struct TransIslandData *island_data);
/**
 * \param mtx: Measure distance in this space.
 * \param dists: Store the closest connected distance to selected vertices.
 * \param index: Optionally store the original index we're measuring the distance to (can be NULL).
 */
void transform_convert_mesh_connectivity_distance(struct BMesh *bm,
                                                  const float mtx[3][3],
                                                  float *dists,
                                                  int *index);
void transform_convert_mesh_mirrordata_calc(struct BMEditMesh *em,
                                            bool use_select,
                                            bool use_topology,
                                            const bool mirror_axis[3],
                                            struct TransMirrorData *r_mirror_data);
void transform_convert_mesh_mirrordata_free(struct TransMirrorData *mirror_data);
/**
 * Detect CrazySpace [tm].
 * Vertices with space affected by quats are marked with #BM_ELEM_TAG.
 */
void transform_convert_mesh_crazyspace_detect(TransInfo *t,
                                              struct TransDataContainer *tc,
                                              struct BMEditMesh *em,
                                              struct TransMeshDataCrazySpace *r_crazyspace_data);
void transform_convert_mesh_crazyspace_transdata_set(const float mtx[3][3],
                                                     const float smtx[3][3],
                                                     const float defmat[3][3],
                                                     const float quat[4],
                                                     struct TransData *r_td);
void transform_convert_mesh_crazyspace_free(struct TransMeshDataCrazySpace *r_crazyspace_data);

/* transform_convert_mesh_edge.c */

extern TransConvertTypeInfo TransConvertType_MeshEdge;

/* transform_convert_mesh_skin.c */

extern TransConvertTypeInfo TransConvertType_MeshSkin;

/* transform_convert_mesh_uv.c */

extern TransConvertTypeInfo TransConvertType_MeshUV;

/* transform_convert_mesh_vert_cdata.c */

extern TransConvertTypeInfo TransConvertType_MeshVertCData;

/* transform_convert_nla.c */

extern TransConvertTypeInfo TransConvertType_NLA;

/* transform_convert_node.cc */

extern TransConvertTypeInfo TransConvertType_Node;

/* transform_convert_object.c */

extern TransConvertTypeInfo TransConvertType_Object;

/* transform_convert_object_texspace.c */

extern TransConvertTypeInfo TransConvertType_ObjectTexSpace;

/* transform_convert_paintcurve.c */

extern TransConvertTypeInfo TransConvertType_PaintCurve;

/* transform_convert_particle.c */

extern TransConvertTypeInfo TransConvertType_Particle;

/* transform_convert_sculpt.cc */

extern TransConvertTypeInfo TransConvertType_Sculpt;

/* transform_convert_sequencer.c */

extern TransConvertTypeInfo TransConvertType_Sequencer;

/* transform_convert_sequencer_image.c */

extern TransConvertTypeInfo TransConvertType_SequencerImage;

/* transform_convert_tracking.c */

extern TransConvertTypeInfo TransConvertType_Tracking;

/* transform_convert_tracking_curves.c */

extern TransConvertTypeInfo TransConvertType_TrackingCurves;

#ifdef __cplusplus
}
#endif
