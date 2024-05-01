/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 * \brief conversion and adaptation of different datablocks to a common struct.
 */

#pragma once

#include "RE_engine.h"

#include "BKE_curves.hh"

#include "BLI_index_mask.hh"

struct BMEditMesh;
struct BMesh;
struct BezTriple;
struct ListBase;
struct Object;
struct TransData;
struct TransDataCurveHandleFlags;
struct TransInfo;
struct bContext;

struct TransConvertTypeInfo {
  int flags; /* #eTFlag. */

  /**
   * Allocate and initialize `t->data`.
   */
  void (*create_trans_data)(bContext *C, TransInfo *t);

  /**
   * Force recalculation of data during transformation.
   */
  void (*recalc_data)(TransInfo *t);

  /**
   * Called when the operation is finished.
   */
  void (*special_aftertrans_update)(bContext *C, TransInfo *t);
};

/**
 * Structure used for Edge Slide operation.
 * The data is filled based on the 'transform_convert_' type.
 */
struct TransDataEdgeSlideVert {
  TransData *td;
  blender::float3 dir_side[2]; /* Directional vectors on the sides. */
  float edge_len;              /* Distance between vectors. */
  int loop_nr;                 /* Number that identifies the group of connected edges. */

  const float *v_co_orig() const
  {
    return this->td->iloc;
  }
};

/**
 * Structure used for Vert Slide operation.
 * The data is filled based on the 'transform_convert_' type.
 */
struct TransDataVertSlideVert {
  TransData *td;
  blender::Span<blender::float3> co_link_orig_3d; /* Target locations. */
  int co_link_curr;

  const float *co_orig_3d() const
  {
    return this->td->iloc;
  }

  const blender::float3 &co_dest_3d() const
  {
    return this->co_link_orig_3d[this->co_link_curr];
  }
};

/**
 * Structure used for curves transform operation.
 * Used for both curves and grease pencil objects.
 */
struct CurvesTransformData {
  blender::IndexMaskMemory memory;
  blender::Vector<blender::IndexMask> selection_by_layer;

  /**
   * The offsets of every grease pencil layer into `positions` array.
   * For curves layers are used to store: positions, handle_positions_left and
   * handle_positions_right.
   */
  blender::Vector<int> layer_offsets;

  /**
   * Copy of all positions being transformed.
   */
  blender::Array<blender::float3> positions;
};

/* `transform_convert.cc` */

/**
 * Change the chain-length of auto-IK.
 */
void transform_autoik_update(TransInfo *t, short mode);
int special_transform_moving(TransInfo *t);
/**
 * Inserting keys, point-cache, redraw events.
 */
void special_aftertrans_update(bContext *C, TransInfo *t);
void sort_trans_data_dist(TransInfo *t);
void create_trans_data(bContext *C, TransInfo *t);
void clipUVData(TransInfo *t);
void transform_convert_flush_handle2D(TransData *td, TransData2D *td2d, float y_fac);
/**
 * Called for updating while transform acts, once per redraw.
 */
void recalc_data(TransInfo *t);

/* `transform_convert_mesh.cc` */

void transform_convert_mesh_customdatacorrect_init(TransInfo *t);

/* `transform_convert_sequencer.cc` */

void transform_convert_sequencer_channel_clamp(TransInfo *t, float r_val[2]);

/********************* intern **********************/

/* `transform_convert.cc` */

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
TransDataCurveHandleFlags *initTransDataCurveHandles(TransData *td, BezTriple *bezt);
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
void animrecord_check_state(TransInfo *t, ID *id);

/* `transform_convert_curves.cc` */

/**
 * Used for both curves and grease pencil objects.
 */
void curve_populate_trans_data_structs(
    TransDataContainer &tc,
    blender::bke::CurvesGeometry &curves,
    const blender::float4x4 &transform,
    std::optional<blender::MutableSpan<float>> value_attribute,
    const blender::Span<blender::IndexMask> points_to_transform_indices,
    const blender::IndexMask &affected_curves,
    bool use_connected_only,
    const blender::IndexMask &bezier_curves);

CurvesTransformData *create_curves_transform_custom_data(TransCustomData &custom_data);

void copy_positions_from_curves_transform_custom_data(
    const TransCustomData &custom_data,
    const int layer,
    blender::MutableSpan<blender::float3> positions_dst);

/* `transform_convert_action.cc` */

extern TransConvertTypeInfo TransConvertType_Action;

/* `transform_convert_armature.cc` */

extern TransConvertTypeInfo TransConvertType_EditArmature;
extern TransConvertTypeInfo TransConvertType_Pose;

/**
 * Sets transform flags in the bones.
 * Returns total number of bones with #BONE_TRANSFORM.
 */
void transform_convert_pose_transflags_update(Object *ob, int mode, short around);

/* `transform_convert_cursor.cc` */

extern TransConvertTypeInfo TransConvertType_CursorImage;
extern TransConvertTypeInfo TransConvertType_CursorSequencer;
extern TransConvertTypeInfo TransConvertType_Cursor3D;

/* `transform_convert_curve.cc` */

extern TransConvertTypeInfo TransConvertType_Curve;

/* `transform_convert_curves.cc` */

extern TransConvertTypeInfo TransConvertType_Curves;

/* `transform_convert_graph.cc` */

extern TransConvertTypeInfo TransConvertType_Graph;

/* `transform_convert_gpencil_legacy.cc` */

extern TransConvertTypeInfo TransConvertType_GPencil;

/* `transform_convert_greasepencil.cc` */

extern TransConvertTypeInfo TransConvertType_GreasePencil;

/* `transform_convert_lattice.cc` */

extern TransConvertTypeInfo TransConvertType_Lattice;

/* `transform_convert_mask.cc` */

extern TransConvertTypeInfo TransConvertType_Mask;

/* `transform_convert_mball.cc` */

extern TransConvertTypeInfo TransConvertType_MBall;

/* `transform_convert_mesh.cc` */

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
  MirrorDataVert *vert_map;
  int mirror_elem_len;
};

struct TransMeshDataCrazySpace {
  float (*quats)[4];
  blender::Array<blender::float3x3, 0> defmats;
};

void transform_convert_mesh_islands_calc(BMEditMesh *em,
                                         bool calc_single_islands,
                                         bool calc_island_center,
                                         bool calc_island_axismtx,
                                         TransIslandData *r_island_data);
void transform_convert_mesh_islanddata_free(TransIslandData *island_data);
/**
 * \param mtx: Measure distance in this space.
 * \param dists: Store the closest connected distance to selected vertices.
 * \param index: Optionally store the original index we're measuring the distance to (can be NULL).
 */
void transform_convert_mesh_connectivity_distance(BMesh *bm,
                                                  const float mtx[3][3],
                                                  float *dists,
                                                  int *index);
void transform_convert_mesh_mirrordata_calc(BMEditMesh *em,
                                            bool use_select,
                                            bool use_topology,
                                            const bool mirror_axis[3],
                                            TransMirrorData *r_mirror_data);
void transform_convert_mesh_mirrordata_free(TransMirrorData *mirror_data);
/**
 * Detect CrazySpace (Blender term).
 * Vertices with space affected by quaternions are marked with #BM_ELEM_TAG.
 */
void transform_convert_mesh_crazyspace_detect(TransInfo *t,
                                              TransDataContainer *tc,
                                              BMEditMesh *em,
                                              TransMeshDataCrazySpace *r_crazyspace_data);
void transform_convert_mesh_crazyspace_transdata_set(const float mtx[3][3],
                                                     const float smtx[3][3],
                                                     const float defmat[3][3],
                                                     const float quat[4],
                                                     TransData *r_td);
void transform_convert_mesh_crazyspace_free(TransMeshDataCrazySpace *r_crazyspace_data);

blender::Array<TransDataVertSlideVert> transform_mesh_vert_slide_data_create(
    const TransDataContainer *tc, blender::Vector<blender::float3> &r_loc_dst_buffer);

blender::Array<TransDataEdgeSlideVert> transform_mesh_edge_slide_data_create(
    const TransDataContainer *tc, int *r_group_len);

/* `transform_convert_mesh_edge.cc` */

extern TransConvertTypeInfo TransConvertType_MeshEdge;

/* `transform_convert_mesh_skin.cc` */

extern TransConvertTypeInfo TransConvertType_MeshSkin;

/* `transform_convert_mesh_uv.cc` */

extern TransConvertTypeInfo TransConvertType_MeshUV;

blender::Array<TransDataVertSlideVert> transform_mesh_uv_vert_slide_data_create(
    const TransInfo *t,
    TransDataContainer *tc,
    blender::Vector<blender::float3> &r_loc_dst_buffer);

blender::Array<TransDataEdgeSlideVert> transform_mesh_uv_edge_slide_data_create(
    const TransInfo *t, TransDataContainer *tc, int *r_group_len);

/* `transform_convert_mesh_vert_cdata.cc` */

extern TransConvertTypeInfo TransConvertType_MeshVertCData;

/* `transform_convert_nla.cc` */

extern TransConvertTypeInfo TransConvertType_NLA;

/* `transform_convert_node.cc` */

extern TransConvertTypeInfo TransConvertType_Node;

/* `transform_convert_object.cc` */

extern TransConvertTypeInfo TransConvertType_Object;

/* `transform_convert_object_texspace.cc` */

extern TransConvertTypeInfo TransConvertType_ObjectTexSpace;

/* `transform_convert_paintcurve.cc` */

extern TransConvertTypeInfo TransConvertType_PaintCurve;

/* `transform_convert_particle.cc` */

extern TransConvertTypeInfo TransConvertType_Particle;

/* `transform_convert_sculpt.cc` */

extern TransConvertTypeInfo TransConvertType_Sculpt;

/* `transform_convert_sequencer.cc` */

extern TransConvertTypeInfo TransConvertType_Sequencer;

/* `transform_convert_sequencer_image.cc` */

extern TransConvertTypeInfo TransConvertType_SequencerImage;

/* `transform_convert_sequencer_retiming.cc` */

extern TransConvertTypeInfo TransConvertType_SequencerRetiming;

/* `transform_convert_tracking.cc` */

extern TransConvertTypeInfo TransConvertType_Tracking;

/* `transform_convert_tracking_curves.cc` */

extern TransConvertTypeInfo TransConvertType_TrackingCurves;
