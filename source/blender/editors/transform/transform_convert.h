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
struct FCurve;
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
void transform_autoik_update(TransInfo *t, short mode);
int special_transform_moving(TransInfo *t);
void special_aftertrans_update(struct bContext *C, TransInfo *t);
void sort_trans_data_dist(TransInfo *t);
void createTransData(struct bContext *C, TransInfo *t);
bool clipUVTransform(TransInfo *t, float vec[2], const bool resize);
void clipUVData(TransInfo *t);

/* transform_convert_mesh.c */
void trans_mesh_customdata_correction_init(TransInfo *t);
void trans_mesh_customdata_correction_apply(struct TransDataContainer *tc, bool is_final);

/* transform_convert_sequencer.c */
int transform_convert_sequencer_get_snap_bound(TransInfo *t);

/********************* intern **********************/

typedef enum eTransConvertType {
  TC_NONE = 0,
  TC_ACTION_DATA,
  TC_POSE,
  TC_ARMATURE_VERTS,
  TC_CURSOR_IMAGE,
  TC_CURSOR_VIEW3D,
  TC_CURVE_VERTS,
  TC_GRAPH_EDIT_DATA,
  TC_GPENCIL,
  TC_LATTICE_VERTS,
  TC_MASKING_DATA,
  TC_MBALL_VERTS,
  TC_MESH_VERTS,
  TC_MESH_EDGES,
  TC_MESH_UV,
  TC_NLA_DATA,
  TC_NODE_DATA,
  TC_OBJECT,
  TC_OBJECT_TEXSPACE,
  TC_PAINT_CURVE_VERTS,
  TC_PARTICLE_VERTS,
  TC_SCULPT,
  TC_SEQ_DATA,
  TC_TRACKING_DATA,
} eTransConvertType;

/* transform_convert.c */
bool transform_mode_use_local_origins(const TransInfo *t);
void transform_around_single_fallback(TransInfo *t);
void posttrans_fcurve_clean(struct FCurve *fcu, const int sel_flag, const bool use_handle);
bool constraints_list_needinv(TransInfo *t, ListBase *list);
void calc_distanceCurveVerts(TransData *head, TransData *tail);
struct TransDataCurveHandleFlags *initTransDataCurveHandles(TransData *td, struct BezTriple *bezt);
char transform_convert_frame_side_dir_get(TransInfo *t, float cframe);
bool FrameOnMouseSide(char side, float frame, float cframe);
void clipMirrorModifier(TransInfo *t);
void animrecord_check_state(TransInfo *t, struct Object *ob);

/* transform_convert_action.c */
void createTransActionData(bContext *C, TransInfo *t);
void recalcData_actedit(TransInfo *t);
void special_aftertrans_update__actedit(bContext *C, TransInfo *t);

/* transform_convert_armature.c */
int transform_convert_pose_transflags_update(Object *ob,
                                             const int mode,
                                             const short around,
                                             bool has_translate_rotate[2]);
void createTransPose(TransInfo *t);
void createTransArmatureVerts(TransInfo *t);
void recalcData_edit_armature(TransInfo *t);
void recalcData_pose(TransInfo *t);
void special_aftertrans_update__pose(bContext *C, TransInfo *t);

/* transform_convert_cursor.c */
void createTransCursor_image(TransInfo *t);
void createTransCursor_view3d(TransInfo *t);

/* transform_convert_curve.c */
void createTransCurveVerts(TransInfo *t);
void recalcData_curve(TransInfo *t);

/* transform_convert_graph.c */
void createTransGraphEditData(bContext *C, TransInfo *t);
void recalcData_graphedit(TransInfo *t);
void special_aftertrans_update__graph(bContext *C, TransInfo *t);

/* transform_convert_gpencil.c */
void createTransGPencil(bContext *C, TransInfo *t);
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

/* transform_convert_mesh.c */
void createTransEditVerts(TransInfo *t);
void recalcData_mesh(TransInfo *t);
void special_aftertrans_update__mesh(bContext *C, TransInfo *t);

/* transform_convert_mesh_edge.c */
void createTransEdge(TransInfo *t);

/* transform_convert_mesh_uv.c */
void createTransUVs(bContext *C, TransInfo *t);
void recalcData_uv(TransInfo *t);

/* transform_convert_nla.c */
void createTransNlaData(bContext *C, TransInfo *t);
void recalcData_nla(TransInfo *t);
void special_aftertrans_update__nla(bContext *C, TransInfo *t);

/* transform_convert_node.c */
void createTransNodeData(TransInfo *t);
void flushTransNodes(TransInfo *t);
void special_aftertrans_update__node(bContext *C, TransInfo *t);

/* transform_convert_object.c */
void createTransObject(bContext *C, TransInfo *t);
void createTransTexspace(TransInfo *t);
void recalcData_objects(TransInfo *t);
void special_aftertrans_update__object(bContext *C, TransInfo *t);

/* transform_convert_paintcurve.c */
void createTransPaintCurveVerts(bContext *C, TransInfo *t);
void flushTransPaintCurve(TransInfo *t);

/* transform_convert_particle.c */
void createTransParticleVerts(bContext *C, TransInfo *t);
void recalcData_particles(TransInfo *t);

/* transform_convert_sculpt.c */
void createTransSculpt(bContext *C, TransInfo *t);
void recalcData_sculpt(TransInfo *t);
void special_aftertrans_update__sculpt(bContext *C, TransInfo *t);

/* transform_convert_sequencer.c */
void createTransSeqData(TransInfo *t);
void recalcData_sequencer(TransInfo *t);
void special_aftertrans_update__sequencer(bContext *C, TransInfo *t);

/* transform_convert_tracking.c */
void createTransTrackingData(bContext *C, TransInfo *t);
void recalcData_tracking(TransInfo *t);
void special_aftertrans_update__movieclip(bContext *C, TransInfo *t);
#endif
