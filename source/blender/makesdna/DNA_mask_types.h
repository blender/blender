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
 * The Original Code is Copyright (C) 2012 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file DNA_mask_types.h
 *  \ingroup DNA
 *  \since march-2012
 *  \author Sergey Sharybin
 *
 * Mask data-blocks are collections of 2D curves to be used
 * for image masking in the compositor and sequencer.
 */

#ifndef __DNA_MASK_TYPES_H__
#define __DNA_MASK_TYPES_H__

#include "DNA_defs.h"
#include "DNA_ID.h"
#include "DNA_listBase.h"
#include "DNA_curve_types.h"

typedef struct Mask {
	ID id;
	struct AnimData *adt;
	ListBase masklayers;  /* mask layers */
	int masklay_act;      /* index of active mask layer (-1 == None) */
	int masklay_tot;      /* total number of mask layers */

	int sfra, efra;       /* frames, used by the sequencer */

	int flag;  /* for anim info */
	int pad;
} Mask;

typedef struct MaskParent {
	// int flag;             /* parenting flags */ /* not used */
	int id_type;          /* type of parenting */
	int type;             /* type of parenting */
	ID *id;               /* ID block of entity to which mask/spline is parented to
	                       * in case of parenting to movie tracking data set to MovieClip datablock */
	char parent[64];      /* entity of parent to which parenting happened
	                       * in case of parenting to movie tracking data contains name of layer */
	char sub_parent[64];  /* sub-entity of parent to which parenting happened
	                       * in case of parenting to movie tracking data contains name of track */
	float parent_orig[2]; /* track location at the moment of parenting,
	                       * stored in mask space*/

	float parent_corners_orig[4][2]; /* Original corners of plane track at the moment of parenting */
} MaskParent;

typedef struct MaskSplinePointUW {
	float u, w;            /* u coordinate along spline segment and weight of this point */
	int flag;              /* different flags of this point */
} MaskSplinePointUW;

typedef struct MaskSplinePoint {
	BezTriple bezt;        /* actual point coordinates and it's handles  */
	int pad;
	int tot_uw;            /* number of uv feather values */
	MaskSplinePointUW *uw; /* feather UV values */
	MaskParent parent;     /* parenting information of particular spline point */
} MaskSplinePoint;

typedef struct MaskSpline {
	struct MaskSpline *next, *prev;

	short flag;              /* defferent spline flag (closed, ...) */
	char offset_mode;        /* feather offset method */
	char weight_interp;      /* weight interpolation */

	int tot_point;           /* total number of points */
	MaskSplinePoint *points; /* points which defines spline itself */
	MaskParent parent;       /* parenting information of the whole spline */

	MaskSplinePoint *points_deform; /* deformed copy of 'points' BezTriple data - not saved */
} MaskSpline;

/* one per frame */
typedef struct MaskLayerShape {
	struct MaskLayerShape *next, *prev;

	float *data;             /* u coordinate along spline segment and weight of this point */
	int    tot_vert;         /* to ensure no buffer overruns's: alloc size is (tot_vert * MASK_OBJECT_SHAPE_ELEM_SIZE) */
	int    frame;            /* different flags of this point */
	char   flag;             /* animation flag */
	char   pad[7];
} MaskLayerShape;

/* cast to this for convenience, not saved */
#define MASK_OBJECT_SHAPE_ELEM_SIZE 8 /* 3x 2D points + weight + radius == 8 */

#
#
typedef struct MaskLayerShapeElem {
	float value[MASK_OBJECT_SHAPE_ELEM_SIZE];
} MaskLayerShapeElem;

typedef struct MaskLayer {
	struct MaskLayer *next, *prev;

	char name[64];                     /* name of the mask layer (64 = MAD_ID_NAME - 2) */

	ListBase splines;                  /* list of splines which defines this mask layer */
	ListBase splines_shapes;

	struct MaskSpline *act_spline;     /* active spline */
	struct MaskSplinePoint *act_point; /* active point */

	/* blending options */
	float  alpha;
	char   blend;
	char   blend_flag;
	char   falloff;
	char   pad[7];

	char   flag;             /* for animation */
	char   restrictflag;     /* matching 'Object' flag of the same name - eventually use in the outliner  */
} MaskLayer;

/* MaskParent->flag */
/* #define MASK_PARENT_ACTIVE  (1 << 0) */ /* UNUSED */

/* MaskParent->type */
enum {
	MASK_PARENT_POINT_TRACK = 0, /* parenting happens to point track */
	MASK_PARENT_PLANE_TRACK = 1, /* parenting happens to plane track */
};

/* MaskSpline->flag */
/* reserve (1 << 0) for SELECT */
enum {
	MASK_SPLINE_CYCLIC  = (1 << 1),
	MASK_SPLINE_NOFILL  = (1 << 2),
	MASK_SPLINE_NOINTERSECT = (1 << 3)
};

/* MaskSpline->weight_interp */
enum {
	MASK_SPLINE_INTERP_LINEAR  = 1,
	MASK_SPLINE_INTERP_EASE    = 2
};

/* MaskSpline->offset_mode */
enum {
	MASK_SPLINE_OFFSET_EVEN   = 0,
	MASK_SPLINE_OFFSET_SMOOTH = 1
};


/* ob->restrictflag */
#define MASK_RESTRICT_VIEW      1
#define MASK_RESTRICT_SELECT    2
#define MASK_RESTRICT_RENDER    4

/* SpaceClip->mask_draw_flag */
#define MASK_DRAWFLAG_SMOOTH    1
#define MASK_DRAWFLAG_OVERLAY   2

/* copy of eSpaceImage_UVDT */
/* SpaceClip->mask_draw_type */
enum {
	MASK_DT_OUTLINE = 0,
	MASK_DT_DASH    = 1,
	MASK_DT_BLACK   = 2,
	MASK_DT_WHITE   = 3
};

/* MaskSpaceInfo->overlay_mode */
enum {
	MASK_OVERLAY_ALPHACHANNEL = 0,
	MASK_OVERLAY_COMBINED     = 1
};

/* masklay->blend */
enum {
	MASK_BLEND_ADD             = 0,
	MASK_BLEND_SUBTRACT        = 1,
	MASK_BLEND_LIGHTEN         = 2,
	MASK_BLEND_DARKEN          = 3,
	MASK_BLEND_MUL             = 4,
	MASK_BLEND_REPLACE         = 5,
	MASK_BLEND_DIFFERENCE      = 6,
	MASK_BLEND_MERGE_ADD       = 7,
	MASK_BLEND_MERGE_SUBTRACT  = 8
};

/* masklay->blend_flag */
enum {
	MASK_BLENDFLAG_INVERT = (1 << 0)
};

/* masklay->flag */
enum {
	MASK_LAYERFLAG_LOCKED = (1 << 4),
	MASK_LAYERFLAG_SELECT = (1 << 5),

	/* no holes */
	MASK_LAYERFLAG_FILL_DISCRETE = (1 << 6),
	MASK_LAYERFLAG_FILL_OVERLAP  = (1 << 7),
};

/* masklay_shape->flag */
enum {
	MASK_SHAPE_SELECT = (1 << 0)
};


/* mask->flag */
enum {
	MASK_ANIMF_EXPAND = (1 << 4)
};

#endif  /* __DNA_MASK_TYPES_H__ */
