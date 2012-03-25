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

struct wmWindowManager;
struct wmOperatorType;
struct ListBase;
struct wmEvent;
struct bContext;
struct Object;
struct uiLayout;
struct EnumPropertyItem;
struct wmOperatorType;
struct wmKeyMap;
struct wmKeyConfig;

void transform_keymap_for_space(struct wmKeyConfig *keyconf, struct wmKeyMap *keymap, int spaceid);
void transform_operatortypes(void);

/* ******************** Macros & Prototypes *********************** */

/* MODE AND NUMINPUT FLAGS */
enum {
	TFM_INIT = -1,
	TFM_DUMMY,
	TFM_TRANSLATION,
	TFM_ROTATION,
	TFM_RESIZE,
	TFM_TOSPHERE,
	TFM_SHEAR,
	TFM_WARP,
	TFM_SHRINKFATTEN,
	TFM_TILT,
	TFM_TRACKBALL,
	TFM_PUSHPULL,
	TFM_CREASE,
	TFM_MIRROR,
	TFM_BONESIZE,
	TFM_BONE_ENVELOPE,
	TFM_CURVE_SHRINKFATTEN,
	TFM_BONE_ROLL,
	TFM_TIME_TRANSLATE,
	TFM_TIME_SLIDE,
	TFM_TIME_SCALE,
	TFM_TIME_EXTEND,
	TFM_TIME_DUPLICATE,
	TFM_BAKE_TIME,
	TFM_BEVEL,
	TFM_BWEIGHT,
	TFM_ALIGN,
	TFM_EDGE_SLIDE,
	TFM_SEQ_SLIDE
} TfmMode;

/* TRANSFORM CONTEXTS */
#define CTX_NONE			0
#define CTX_TEXTURE			1
#define CTX_EDGE			2
#define CTX_NO_PET			4
#define CTX_TWEAK			8
#define CTX_NO_MIRROR		16
#define CTX_AUTOCONFIRM		32
#define CTX_BMESH			64
#define CTX_NDOF			128
#define CTX_MOVIECLIP		256

/* Standalone call to get the transformation center corresponding to the current situation
 * returns 1 if successful, 0 otherwise (usually means there's no selection)
 * (if 0 is returns, *vec is unmodified)
 * */
int calculateTransformCenter(struct bContext *C, int centerMode, float *vec);

struct TransInfo;
struct ScrArea;
struct Base;
struct Scene;
struct Object;

int BIF_snappingSupported(struct Object *obedit);

struct TransformOrientation;
struct bContext;
struct ReportList;

void BIF_clearTransformOrientation(struct bContext *C);
void BIF_removeTransformOrientation(struct bContext *C, struct TransformOrientation *ts);
void BIF_removeTransformOrientationIndex(struct bContext *C, int index);
void BIF_createTransformOrientation(struct bContext *C, struct ReportList *reports, char *name, int use, int overwrite);
void BIF_selectTransformOrientation(struct bContext *C, struct TransformOrientation *ts);
void BIF_selectTransformOrientationValue(struct bContext *C, int orientation);

void ED_getTransformOrientationMatrix(const struct bContext *C, float orientation_mat[][3], int activeOnly);

struct EnumPropertyItem *BIF_enumTransformOrientation(struct bContext *C);
const char * BIF_menustringTransformOrientation(const struct bContext *C, const char *title); /* the returned value was allocated and needs to be freed after use */
int BIF_countTransformOrientation(const struct bContext *C);

void BIF_TransformSetUndo(const char *str);

void BIF_selectOrientation(void);

/* to be able to add operator properties to other operators */

#define P_MIRROR		(1 << 0)
#define P_PROPORTIONAL	(1 << 1)
#define P_AXIS			(1 << 2)
#define P_SNAP			(1 << 3)
#define P_GEO_SNAP		(P_SNAP|(1 << 4))
#define P_ALIGN_SNAP	(P_GEO_SNAP|(1 << 5))
#define P_CONSTRAINT	(1 << 6)
#define P_OPTIONS		(1 << 7)
#define P_CORRECT_UV 	(1 << 8)

void Transform_Properties(struct wmOperatorType *ot, int flags);

/* view3d manipulators */

int BIF_do_manipulator(struct bContext *C, struct wmEvent *event, struct wmOperator *op);
void BIF_draw_manipulator(const struct bContext *C);

/* Snapping */


typedef struct DepthPeel
{
	struct DepthPeel *next, *prev;

	float depth;
	float p[3];
	float no[3];
	struct Object *ob;
	int flag;
} DepthPeel;

struct ListBase;

typedef enum SnapMode
{
	SNAP_ALL = 0,
	SNAP_NOT_SELECTED = 1,
	SNAP_NOT_OBEDIT = 2
} SnapMode;

#define SNAP_MIN_DISTANCE 30

int peelObjectsTransForm(struct TransInfo *t, struct ListBase *depth_peels, const float mval[2], SnapMode mode);
int peelObjectsContext(struct bContext *C, struct ListBase *depth_peels, const float mval[2], SnapMode mode);
int snapObjectsTransform(struct TransInfo *t, const float mval[2], int *r_dist, float r_loc[3], float r_no[3], SnapMode mode);
int snapObjectsContext(struct bContext *C, const float mval[2], int *r_dist, float r_loc[3], float r_no[3], SnapMode mode);

#endif

