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
struct EnumPropertyItem;
struct ListBase;
struct Object;
struct View3D;
struct bContext;
struct uiLayout;
struct wmEvent;
struct wmKeyConfig;
struct wmKeyMap;
struct wmOperatorType;
struct wmWindowManager;

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
	TFM_SEQ_SLIDE
};

/* TRANSFORM CONTEXTS */
#define CTX_NONE            0
#define CTX_TEXTURE         (1 << 0)
#define CTX_EDGE            (1 << 1)
#define CTX_NO_PET          (1 << 2)
#define CTX_NO_MIRROR       (1 << 3)
#define CTX_AUTOCONFIRM     (1 << 4)
#define CTX_NDOF            (1 << 5)
#define CTX_MOVIECLIP       (1 << 6)
#define CTX_MASK            (1 << 7)
#define CTX_PAINT_CURVE     (1 << 8)
#define CTX_GPENCIL_STROKES (1 << 9)

/* Standalone call to get the transformation center corresponding to the current situation
 * returns 1 if successful, 0 otherwise (usually means there's no selection)
 * (if 0 is returns, *vec is unmodified)
 * */
bool calculateTransformCenter(struct bContext *C, int centerMode, float cent3d[3], float cent2d[2]);

struct TransInfo;
struct ScrArea;
struct Base;
struct Scene;
struct Object;

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
void BIF_selectTransformOrientationValue(struct bContext *C, int orientation);

void ED_getTransformOrientationMatrix(const struct bContext *C, float orientation_mat[3][3], const bool activeOnly);

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
#define P_GPENCIL_EDIT  (1 << 12)

void Transform_Properties(struct wmOperatorType *ot, int flags);

/* view3d manipulators */

int BIF_do_manipulator(struct bContext *C, const struct wmEvent *event, struct wmOperator *op);
void BIF_draw_manipulator(const struct bContext *C);

/* Snapping */


typedef struct DepthPeel {
	struct DepthPeel *next, *prev;

	float depth;
	float p[3];
	float no[3];
	struct Object *ob;
	int flag;
} DepthPeel;

struct ListBase;

typedef enum SnapMode {
	SNAP_ALL = 0,
	SNAP_NOT_SELECTED = 1,
	SNAP_NOT_OBEDIT = 2
} SnapMode;

#define SNAP_MIN_DISTANCE 30
#define TRANSFORM_DIST_MAX_RAY (FLT_MAX / 2.0f)

bool peelObjectsTransForm(struct TransInfo *t, struct ListBase *depth_peels, const float mval[2], SnapMode mode);
bool peelObjectsContext(struct bContext *C, struct ListBase *depth_peels, const float mval[2], SnapMode mode);
bool snapObjectsTransform(struct TransInfo *t, const float mval[2], float *r_dist_px, float r_loc[3], float r_no[3], SnapMode mode);
bool snapObjectsContext(struct bContext *C, const float mval[2], float *r_dist_px, float r_loc[3], float r_no[3], SnapMode mode);
/* taks args for all settings */
bool snapObjectsEx(struct Scene *scene, struct Base *base_act, struct View3D *v3d, struct ARegion *ar, struct Object *obedit, short snap_mode,
                   const float mval[2], float *r_dist_px,
                   float r_loc[3], float r_no[3], float *r_ray_dist, SnapMode mode);
bool snapObjectsRayEx(struct Scene *scene, struct Base *base_act, struct View3D *v3d, struct ARegion *ar, struct Object *obedit, short snap_mode,
                      struct Object **r_ob, float r_obmat[4][4],
                      const float ray_start[3], const float ray_normal[3], float *r_ray_dist,
                      const float mval[2], float *r_dist_px, float r_loc[3], float r_no[3], SnapMode mode);

bool snapNodesTransform(struct TransInfo *t, const int mval[2], float *r_dist_px, float r_loc[2], char *r_node_border, SnapMode mode);
bool snapNodesContext(struct bContext *C, const int mval[2], float *r_dist_px, float r_loc[2], char *r_node_border, SnapMode mode);

#endif

