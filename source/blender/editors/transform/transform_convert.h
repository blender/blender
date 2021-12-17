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
 * \brief conversion and adaptation of different datablocks to a common struct.
 */

#pragma once

struct BMEditMesh;
struct BMesh;
struct BezTriple;
struct FCurve;
struct ListBase;
struct Object;
struct TransData;
struct TransDataCurveHandleFlags;
struct TransInfo;
struct bContext;

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
bool clipUVTransform(TransInfo *t, float vec[2], const bool resize);
void clipUVData(TransInfo *t);
void transform_convert_flush_handle2D(TransData *td, TransData2D *td2d, const float y_fac);
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
 * Called during special_aftertrans_update to make sure selected keyframes replace
 * any other keyframes which may reside on that frame (that is not selected).
 *
 * \param sel_flag: The flag (bezt.f1/2/3) value to use to determine selection. Usually `SELECT`,
 *                  but may want to use a different one at times (if caller does not operate on
 *                  selection).
 */
void posttrans_fcurve_clean(struct FCurve *fcu, const int sel_flag, const bool use_handle);
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
void animrecord_check_state(TransInfo *t, struct Object *ob);

/* transform_convert_action.c */

void createTransActionData(bContext *C, TransInfo *t);
/* helper for recalcData() - for Action Editor transforms */
void recalcData_actedit(TransInfo *t);
void special_aftertrans_update__actedit(bContext *C, TransInfo *t);

/* transform_convert_armature.c */

/**
 * Sets transform flags in the bones.
 * Returns total number of bones with #BONE_TRANSFORM.
 */
int transform_convert_pose_transflags_update(Object *ob,
                                             const int mode,
                                             const short around,
                                             bool has_translate_rotate[2]);
/**
 * When objects array is NULL, use 't->data_container' as is.
 */
void createTransPose(TransInfo *t);
void createTransArmatureVerts(TransInfo *t);
void recalcData_edit_armature(TransInfo *t);
void recalcData_pose(TransInfo *t);
void special_aftertrans_update__pose(bContext *C, TransInfo *t);

/* transform_convert_cursor.c */

void createTransCursor_image(TransInfo *t);
void createTransCursor_sequencer(TransInfo *t);
void createTransCursor_view3d(TransInfo *t);
void recalcData_cursor_image(TransInfo *t);
void recalcData_cursor_sequencer(TransInfo *t);
void recalcData_cursor_view3d(TransInfo *t);

/* transform_convert_curve.c */

void createTransCurveVerts(TransInfo *t);
void recalcData_curve(TransInfo *t);

/* transform_convert_graph.c */
/**
 * It is important to note that this doesn't always act on the selection (like it's usually done),
 * it acts on a subset of it. E.g. the selection code may leave a hint that we just dragged on a
 * left or right handle (SIPO_RUNTIME_FLAG_TWEAK_HANDLES_LEFT/RIGHT) and then we only transform the
 * selected left or right handles accordingly.
 * The points to be transformed are tagged with BEZT_FLAG_TEMP_TAG; some lower level curve
 * functions may need to be made aware of this. It's ugly that these act based on selection state
 * anyway.
 */
void createTransGraphEditData(bContext *C, TransInfo *t);
/* helper for recalcData() - for Graph Editor transforms */
void recalcData_graphedit(TransInfo *t);
void special_aftertrans_update__graph(bContext *C, TransInfo *t);

/* transform_convert_gpencil.c */
void createTransGPencil(bContext *C, TransInfo *t);
/* force recalculation of triangles during transformation */
void recalcData_gpencil_strokes(TransInfo *t);

/* transform_convert_lattice.c */
void createTransLatticeVerts(TransInfo *t);
void recalcData_lattice(TransInfo *t);

/* transform_convert_mask.c */
void createTransMaskingData(bContext *C, TransInfo *t);
void recalcData_mask_common(TransInfo *t);
void special_aftertrans_update__mask(bContext *C, TransInfo *t);

/* transform_convert_mball.c */
void createTransMBallVerts(TransInfo *t);
void recalcData_mball(TransInfo *t);

/* transform_convert_mesh.c */
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
                                         const bool calc_single_islands,
                                         const bool calc_island_center,
                                         const bool calc_island_axismtx,
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
                                            const bool use_select,
                                            const bool use_topology,
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

void createTransEditVerts(TransInfo *t);
void recalcData_mesh(TransInfo *t);
void special_aftertrans_update__mesh(bContext *C, TransInfo *t);

/* transform_convert_mesh_edge.c */
void createTransEdge(TransInfo *t);
void recalcData_mesh_edge(TransInfo *t);

/* transform_convert_mesh_skin.c */
void createTransMeshSkin(TransInfo *t);
void recalcData_mesh_skin(TransInfo *t);

/* transform_convert_mesh_uv.c */
void createTransUVs(bContext *C, TransInfo *t);
/* helper for recalcData() - for Image Editor transforms */
void recalcData_uv(TransInfo *t);

/* transform_convert_nla.c */
void createTransNlaData(bContext *C, TransInfo *t);
/* helper for recalcData() - for NLA Editor transforms */
void recalcData_nla(TransInfo *t);
void special_aftertrans_update__nla(bContext *C, TransInfo *t);

/* transform_convert_node.c */
void createTransNodeData(TransInfo *t);
void flushTransNodes(TransInfo *t);
void special_aftertrans_update__node(bContext *C, TransInfo *t);

/* transform_convert_object.c */
void createTransObject(bContext *C, TransInfo *t);
/* helper for recalcData() - for object transforms, typically in the 3D view */
void recalcData_objects(TransInfo *t);
void special_aftertrans_update__object(bContext *C, TransInfo *t);

/* transform_convert_object_texspace.c */
void createTransTexspace(TransInfo *t);
/* helper for recalcData() - for object transforms, typically in the 3D view */
void recalcData_texspace(TransInfo *t);

/* transform_convert_paintcurve.c */
void createTransPaintCurveVerts(bContext *C, TransInfo *t);
void flushTransPaintCurve(TransInfo *t);

/* transform_convert_particle.c */
void createTransParticleVerts(TransInfo *t);
void recalcData_particles(TransInfo *t);

/* transform_convert_sculpt.c */
void createTransSculpt(bContext *C, TransInfo *t);
void recalcData_sculpt(TransInfo *t);
void special_aftertrans_update__sculpt(bContext *C, TransInfo *t);

/* transform_convert_sequencer.c */
void createTransSeqData(TransInfo *t);
/* helper for recalcData() - for sequencer transforms */
void recalcData_sequencer(TransInfo *t);
void special_aftertrans_update__sequencer(bContext *C, TransInfo *t);

/* transform_convert_sequencer_image.c */
void createTransSeqImageData(TransInfo *t);
void recalcData_sequencer_image(TransInfo *t);
void special_aftertrans_update__sequencer_image(bContext *C, TransInfo *t);

/* transform_convert_tracking.c */
void createTransTrackingData(bContext *C, TransInfo *t);
/* helper for recalcData() - for Movie Clip transforms */
void recalcData_tracking(TransInfo *t);
void special_aftertrans_update__movieclip(bContext *C, TransInfo *t);
