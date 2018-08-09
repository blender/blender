/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ED_transform.h
 *  \ingroup editors
 */

#ifndef __ED_TRANSFORM_H__
#define __ED_TRANSFORM_H__

/* ******************* Registration Function ********************** */

struct ARegion;
struct ListBase;
struct Object;
struct View3D;
struct bContext;
struct wmEvent;
struct wmKeyConfig;
struct wmKeyMap;
struct wmOperatorType;
struct WorkSpace;
struct Main;
struct SnapObjectContext;
struct SnapObjectParams;

void transform_keymap_for_space(struct wmKeyConfig *keyconf, struct wmKeyMap *keymap, int spaceid);
void transform_operatortypes(void);

/* ******************** Macros & Prototypes *********************** */

/* MODE AND NUMINPUT FLAGS */
enum TfmMode {
	TFM_INIT = -1,
	TFM_DUMMY,
	TFM_TRANSLATION,
	TFM_ROTATION,
	TFM_RESIZE,
	TFM_SKIN_RESIZE,
	TFM_TOSPHERE,
	TFM_SHEAR,
	TFM_BEND,
	TFM_SHRINKFATTEN,
	TFM_TILT,
	TFM_TRACKBALL,
	TFM_PUSHPULL,
	TFM_CREASE,
	TFM_MIRROR,
	TFM_BONESIZE,
	TFM_BONE_ENVELOPE,
	TFM_CURVE_SHRINKFATTEN,
	TFM_MASK_SHRINKFATTEN,
	TFM_GPENCIL_SHRINKFATTEN,
	TFM_BONE_ROLL,
	TFM_TIME_TRANSLATE,
	TFM_TIME_SLIDE,
	TFM_TIME_SCALE,
	TFM_TIME_EXTEND,
	TFM_TIME_DUPLICATE,
	TFM_BAKE_TIME,
	TFM_DEPRECATED,  /* was BEVEL */
	TFM_BWEIGHT,
	TFM_ALIGN,
	TFM_EDGE_SLIDE,
	TFM_VERT_SLIDE,
	TFM_SEQ_SLIDE,
	TFM_BONE_ENVELOPE_DIST,
	TFM_NORMAL_ROTATION,
};

/* TRANSFORM CONTEXTS */
#define CTX_NONE            0
#define CTX_TEXTURE         (1 << 0)
#define CTX_EDGE            (1 << 1)
#define CTX_NO_PET          (1 << 2)
#define CTX_NO_MIRROR       (1 << 3)
#define CTX_AUTOCONFIRM     (1 << 4)
#define CTX_MOVIECLIP       (1 << 6)
#define CTX_MASK            (1 << 7)
#define CTX_PAINT_CURVE     (1 << 8)
#define CTX_GPENCIL_STROKES (1 << 9)
#define CTX_CURSOR          (1 << 10)

/* Standalone call to get the transformation center corresponding to the current situation
 * returns 1 if successful, 0 otherwise (usually means there's no selection)
 * (if 0 is returns, *vec is unmodified)
 * */
bool calculateTransformCenter(struct bContext *C, int centerMode, float cent3d[3], float cent2d[2]);

struct TransInfo;
struct Scene;
struct Object;
struct wmGizmoGroup;
struct wmGizmoGroupType;
struct wmOperator;

/* UNUSED */
// int BIF_snappingSupported(struct Object *obedit);

struct TransformOrientation;
struct bContext;
struct ReportList;

void BIF_clearTransformOrientation(struct bContext *C);
void BIF_removeTransformOrientation(struct bContext *C, struct TransformOrientation *ts);
void BIF_removeTransformOrientationIndex(struct bContext *C, int index);
void BIF_createTransformOrientation(struct bContext *C, struct ReportList *reports,
                                    const char *name, const bool use_view,
                                    const bool activate, const bool overwrite);
void BIF_selectTransformOrientation(struct bContext *C, struct TransformOrientation *ts);
void BIF_selectTransformOrientationValue(struct Scene *scene, int orientation);

void ED_getTransformOrientationMatrix(const struct bContext *C, float orientation_mat[3][3], const short around);

int BIF_countTransformOrientation(const struct bContext *C);

/* to be able to add operator properties to other operators */

#define P_MIRROR        (1 << 0)
#define P_MIRROR_DUMMY  (P_MIRROR | (1 << 9))
#define P_PROPORTIONAL  (1 << 1)
#define P_AXIS          (1 << 2)
#define P_SNAP          (1 << 3)
#define P_GEO_SNAP      (P_SNAP | (1 << 4))
#define P_ALIGN_SNAP    (P_GEO_SNAP | (1 << 5))
#define P_CONSTRAINT    (1 << 6)
#define P_OPTIONS       (1 << 7)
#define P_CORRECT_UV    (1 << 8)
#define P_NO_DEFAULTS   (1 << 10)
#define P_NO_TEXSPACE   (1 << 11)
#define P_CENTER        (1 << 12)
#define P_GPENCIL_EDIT  (1 << 13)
#define P_CURSOR_EDIT   (1 << 14)
#define P_CLNOR_INVALIDATE (1 << 15)

void Transform_Properties(struct wmOperatorType *ot, int flags);

/* transform gizmos */

void TRANSFORM_GGT_gizmo(struct wmGizmoGroupType *gzgt);
void VIEW3D_GGT_xform_cage(struct wmGizmoGroupType *gzgt);

bool ED_widgetgroup_gizmo2d_poll(const struct bContext *C, struct wmGizmoGroupType *gzgt);
void ED_widgetgroup_gizmo2d_setup(const struct bContext *C, struct wmGizmoGroup *gzgroup);
void ED_widgetgroup_gizmo2d_refresh(const struct bContext *C, struct wmGizmoGroup *gzgroup);
void ED_widgetgroup_gizmo2d_draw_prepare(const struct bContext *C, struct wmGizmoGroup *gzgroup);


/* Snapping */

#define SNAP_MIN_DISTANCE 30

bool peelObjectsTransform(
        struct TransInfo *t,
        const float mval[2],
        const bool use_peel_object,
        /* return args */
        float r_loc[3], float r_no[3], float *r_thickness);
bool peelObjectsSnapContext(
        struct SnapObjectContext *sctx,
        const float mval[2],
        const struct SnapObjectParams *params,
        const bool use_peel_object,
        /* return args */
        float r_loc[3], float r_no[3], float *r_thickness);

bool snapObjectsTransform(
        struct TransInfo *t, const float mval[2],
        float *dist_px,
        /* return args */
        float r_loc[3], float r_no[3]);
bool snapNodesTransform(
        struct TransInfo *t, const int mval[2],
        /* return args */
        float r_loc[2], float *r_dist_px, char *r_node_border);

struct TransformBounds {
	float center[3];		/* Center for transform widget. */
	float min[3], max[3];	/* Boundbox of selection for transform widget. */

	/* Normalized axis */
	float axis[3][3];
	float axis_min[3], axis_max[3];
};

struct TransformCalcParams {
	uint use_only_center : 1;
	uint use_local_axis : 1;
	/* Use 'Scene.orientation_type' when zero, otherwise subtract one and use. */
	ushort orientation_type;
};
int ED_transform_calc_gizmo_stats(
        const struct bContext *C,
        const struct TransformCalcParams *params,
        struct TransformBounds *tbounds);

#endif  /* __ED_TRANSFORM_H__ */
