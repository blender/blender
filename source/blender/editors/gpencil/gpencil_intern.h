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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/gpencil/gpencil_intern.h
 *  \ingroup edgpencil
 */

#ifndef __GPENCIL_INTERN_H__
#define __GPENCIL_INTERN_H__


#include "DNA_vec_types.h"


/* internal exports only */
struct bGPdata;
struct bGPDstroke;
struct bGPDspoint;

struct ARegion;
struct View2D;
struct wmOperatorType;


/* ***************************************************** */
/* Internal API */

/* Stroke Coordinates API ------------------------------ */
/* gpencil_utils.c */

typedef struct GP_SpaceConversion {
	struct bGPdata *gpd;
	struct bGPDlayer *gpl;

	struct ScrArea *sa;
	struct ARegion *ar;
	struct View2D *v2d;

	rctf *subrect;       /* for using the camera rect within the 3d view */
	rctf subrect_data;

	float mat[4][4];     /* transform matrix on the strokes (introduced in [b770964]) */
} GP_SpaceConversion;


/**
 * Check whether a given stroke segment is inside a circular brush
 *
 * \param mval     The current screen-space coordinates (midpoint) of the brush
 * \param mvalo    The previous screen-space coordinates (midpoint) of the brush (NOT CURRENTLY USED)
 * \param rad      The radius of the brush
 *
 * \param x0, y0   The screen-space x and y coordinates of the start of the stroke segment
 * \param x1, y1   The screen-space x and y coordinates of the end of the stroke segment
 */
bool gp_stroke_inside_circle(const int mval[2], const int UNUSED(mvalo[2]),
                             int rad, int x0, int y0, int x1, int y1);


/**
 * Init settings for stroke point space conversions
 *
 * \param[out] r_gsc  The space conversion settings struct, populated with necessary params
 */
void gp_point_conversion_init(struct bContext *C, GP_SpaceConversion *r_gsc);

/**
 * Convert a Grease Pencil coordinate (i.e. can be 2D or 3D) to screenspace (2D)
 *
 * \param[out] r_x  The screen-space x-coordinate of the point
 * \param[out] r_y  The screen-space y-coordinate of the point
 */
void gp_point_to_xy(GP_SpaceConversion *settings, struct bGPDstroke *gps, struct bGPDspoint *pt,
                    int *r_x, int *r_y);

/* ***************************************************** */
/* Operator Defines */

/* drawing ---------- */

void GPENCIL_OT_draw(struct wmOperatorType *ot);

/* Paint Modes for operator*/
typedef enum eGPencil_PaintModes {
	GP_PAINTMODE_DRAW = 0,
	GP_PAINTMODE_ERASER,
	GP_PAINTMODE_DRAW_STRAIGHT,
	GP_PAINTMODE_DRAW_POLY
} eGPencil_PaintModes;

/* stroke editing ----- */

void GPENCIL_OT_select(struct wmOperatorType *ot);
void GPENCIL_OT_select_all(struct wmOperatorType *ot);
void GPENCIL_OT_select_circle(struct wmOperatorType *ot);
void GPENCIL_OT_select_border(struct wmOperatorType *ot);
void GPENCIL_OT_select_lasso(struct wmOperatorType *ot);

void GPENCIL_OT_select_linked(struct wmOperatorType *ot);
void GPENCIL_OT_select_more(struct wmOperatorType *ot);
void GPENCIL_OT_select_less(struct wmOperatorType *ot);

void GPENCIL_OT_duplicate(struct wmOperatorType *ot);
void GPENCIL_OT_delete(struct wmOperatorType *ot);

/* buttons editing --- */

void GPENCIL_OT_data_add(struct wmOperatorType *ot);
void GPENCIL_OT_data_unlink(struct wmOperatorType *ot);

void GPENCIL_OT_layer_add(struct wmOperatorType *ot);
void GPENCIL_OT_layer_remove(struct wmOperatorType *ot);
void GPENCIL_OT_layer_move(struct wmOperatorType *ot);
void GPENCIL_OT_layer_duplicate(struct wmOperatorType *ot);

void GPENCIL_OT_active_frame_delete(struct wmOperatorType *ot);

void GPENCIL_OT_convert(struct wmOperatorType *ot);

/* undo stack ---------- */

void gpencil_undo_init(struct bGPdata *gpd);
void gpencil_undo_push(struct bGPdata *gpd);
void gpencil_undo_finish(void);

/******************************************************* */
/* FILTERED ACTION DATA - TYPES  ---> XXX DEPRECEATED OLD ANIM SYSTEM CODE! */

/* XXX - TODO: replace this with the modern bAnimListElem... */
/* This struct defines a structure used for quick access */
typedef struct bActListElem {
	struct bActListElem *next, *prev;

	void *data;   /* source data this elem represents */
	int   type;   /* one of the ACTTYPE_* values */
	int   flag;   /* copy of elem's flags for quick access */
	int   index;  /* copy of adrcode where applicable */

	void  *key_data;  /* motion data - ipo or ipo-curve */
	short  datatype;  /* type of motion data to expect */

	struct bActionGroup *grp;   /* action group that owns the channel */

	void  *owner;      /* will either be an action channel or fake ipo-channel (for keys) */
	short  ownertype;  /* type of owner */
} bActListElem;

/******************************************************* */
/* FILTER ACTION DATA - METHODS/TYPES */

/* filtering flags  - under what circumstances should a channel be added */
typedef enum ACTFILTER_FLAGS {
	ACTFILTER_VISIBLE       = (1 << 0),   /* should channels be visible */
	ACTFILTER_SEL           = (1 << 1),   /* should channels be selected */
	ACTFILTER_FOREDIT       = (1 << 2),   /* does editable status matter */
	ACTFILTER_CHANNELS      = (1 << 3),   /* do we only care that it is a channel */
	ACTFILTER_IPOKEYS       = (1 << 4),   /* only channels referencing ipo's */
	ACTFILTER_ONLYICU       = (1 << 5),   /* only reference ipo-curves */
	ACTFILTER_FORDRAWING    = (1 << 6),   /* make list for interface drawing */
	ACTFILTER_ACTGROUPED    = (1 << 7)    /* belongs to the active group */
} ACTFILTER_FLAGS;

/* Action Editor - Main Data types */
typedef enum ACTCONT_TYPES {
	ACTCONT_NONE = 0,
	ACTCONT_ACTION,
	ACTCONT_SHAPEKEY,
	ACTCONT_GPENCIL
} ACTCONT_TYPES;




#endif /* __GPENCIL_INTERN_H__ */

