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

#ifndef __TRANSFORM_CONVERT_H__
#define __TRANSFORM_CONVERT_H__

struct BezTriple;
struct ListBase;
struct Object;
struct TransData;
struct TransDataContainer;
struct TransDataCurveHandleFlags;
struct TransInfo;
struct bContext;
struct bKinematicConstraint;
struct bPoseChannel;

/* transform_convert.c */
int transform_convert_pose_transflags_update(Object *ob,
                                             const int mode,
                                             const short around,
                                             bool has_translate_rotate[2]);
void transform_autoik_update(TransInfo *t, short mode);
void autokeyframe_object(struct bContext *C,
                         struct Scene *scene,
                         struct ViewLayer *view_layer,
                         struct Object *ob,
                         int tmode);
void autokeyframe_pose(
    struct bContext *C, struct Scene *scene, struct Object *ob, int tmode, short targetless_ik);
bool motionpath_need_update_object(struct Scene *scene, struct Object *ob);
bool motionpath_need_update_pose(struct Scene *scene, struct Object *ob);
int special_transform_moving(TransInfo *t);
void remake_graph_transdata(TransInfo *t, struct ListBase *anim_data);
void special_aftertrans_update(struct bContext *C, TransInfo *t);
void sort_trans_data_dist(TransInfo *t);
void createTransData(struct bContext *C, TransInfo *t);
bool clipUVTransform(TransInfo *t, float vec[2], const bool resize);
void clipUVData(TransInfo *t);

/* transform_convert_action.c */
void flushTransIntFrameActionData(TransInfo *t);

/* transform_convert_armature.c */
void pose_transform_mirror_update(TransInfo *t, TransDataContainer *tc, Object *ob);
void restoreMirrorPoseBones(TransDataContainer *tc);
void restoreBones(TransDataContainer *tc);

/* transform_convert_graph.c */
void flushTransGraphData(TransInfo *t);

/* transform_convert_mask.c */
void flushTransMasking(TransInfo *t);

/* transform_convert_mesh.c */
void flushTransUVs(TransInfo *t);
void trans_mesh_customdata_correction_init(TransInfo *t);
void trans_mesh_customdata_correction_apply(struct TransDataContainer *tc, bool is_final);

/* transform_convert_node.c */
void flushTransNodes(TransInfo *t);

/* transform_convert_object.c */
void trans_obdata_in_obmode_update_all(struct TransInfo *t);
void trans_obchild_in_obmode_update_all(struct TransInfo *t);

/* transform_convert_paintcurve.c */
void flushTransPaintCurve(TransInfo *t);

/* transform_convert_particle.c */
void flushTransParticles(TransInfo *t);

/* transform_convert_sequencer.c */
void flushTransSeq(TransInfo *t);
int transform_convert_sequencer_get_snap_bound(TransInfo *t);

/* transform_convert_tracking.c */
void flushTransTracking(TransInfo *t);

/********************* intern **********************/

/* transform_convert.c */
bool transform_mode_use_local_origins(const TransInfo *t);
void transform_around_single_fallback(TransInfo *t);
bool constraints_list_needinv(TransInfo *t, ListBase *list);
void calc_distanceCurveVerts(TransData *head, TransData *tail);
struct TransDataCurveHandleFlags *initTransDataCurveHandles(TransData *td, struct BezTriple *bezt);
char transform_convert_frame_side_dir_get(TransInfo *t, float cframe);
bool FrameOnMouseSide(char side, float frame, float cframe);

/* transform_convert_action.c */
void createTransActionData(bContext *C, TransInfo *t);

/* transform_convert_armature.c */
struct bKinematicConstraint *has_targetless_ik(struct bPoseChannel *pchan);
void createTransPose(TransInfo *t);
void createTransArmatureVerts(TransInfo *t);

/* transform_convert_cursor.c */
void createTransCursor_image(TransInfo *t);
void createTransCursor_view3d(TransInfo *t);

/* transform_convert_curve.c */
void createTransCurveVerts(TransInfo *t);

/* transform_convert_graph.c */
void createTransGraphEditData(bContext *C, TransInfo *t);

/* transform_convert_gpencil.c */
void createTransGPencil(bContext *C, TransInfo *t);

/* transform_convert_lattice.c */
void createTransLatticeVerts(TransInfo *t);

/* transform_convert_mask.c */
void createTransMaskingData(bContext *C, TransInfo *t);

/* transform_convert_mball.c */
void createTransMBallVerts(TransInfo *t);

/* transform_convert_mesh.c */
void createTransEditVerts(TransInfo *t);
void createTransEdge(TransInfo *t);
void createTransUVs(bContext *C, TransInfo *t);

/* transform_convert_nla.c */
void createTransNlaData(bContext *C, TransInfo *t);

/* transform_convert_node.c */
void createTransNodeData(bContext *UNUSED(C), TransInfo *t);

/* transform_convert_object.c */
void clear_trans_object_base_flags(TransInfo *t);
void createTransObject(bContext *C, TransInfo *t);
void createTransTexspace(TransInfo *t);

/* transform_convert_paintcurve.c */
void createTransPaintCurveVerts(bContext *C, TransInfo *t);

/* transform_convert_particle.c */
void createTransParticleVerts(bContext *C, TransInfo *t);

/* transform_convert_sculpt.c */
void createTransSculpt(TransInfo *t);

/* transform_convert_sequence.c */
void createTransSeqData(TransInfo *t);

/* transform_convert_tracking.c */
void createTransTrackingData(bContext *C, TransInfo *t);
void cancelTransTracking(TransInfo *t);
#endif
