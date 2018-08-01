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
 * The Original Code is Copyright (C) 2008, Blender Foundation.
 * This is a new part of Blender
 *
 * Contributor(s): Joshua Leung, Antonio Vazquez
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file DNA_gpencil_types.h
 *  \ingroup DNA
 */

#ifndef __DNA_GPENCIL_TYPES_H__
#define __DNA_GPENCIL_TYPES_H__

#include "DNA_listBase.h"
#include "DNA_ID.h"
#include "DNA_brush_types.h"

struct AnimData;
struct CurveMapping;
struct GHash;
struct MDeformVert;

/* TODO: add size as userprefs parameter */
#define GP_OBGPENCIL_DEFAULT_SIZE  0.2f
#define GP_DEFAULT_PIX_FACTOR 1.0f
#define GP_DEFAULT_GRID_LINES 4
#define GP_MAX_INPUT_SAMPLES 10

/* ***************************************** */
/* GP Stroke Points */

/* Grease-Pencil Annotations - 'Stroke Point'
 *	-> Coordinates may either be 2d or 3d depending on settings at the time
 * 	-> Coordinates of point on stroke, in proportions of window size
 *	   This assumes that the bottom-left corner is (0,0)
 */
typedef struct bGPDspoint {
	float x, y, z;			/* co-ordinates of point (usually 2d, but can be 3d as well) */
	float pressure;			/* pressure of input device (from 0 to 1) at this point */
	float strength;			/* color strength (used for alpha factor) */
	float time;				/* seconds since start of stroke */
	int flag;				/* additional options */

	float uv_fac;           /* factor of uv along the stroke */
	float uv_rot;           /* uv rotation for dot mode */
} bGPDspoint;

/* bGPDspoint->flag */
typedef enum eGPDspoint_Flag {
	/* stroke point is selected (for editing) */
	GP_SPOINT_SELECT	= (1 << 0),

	/* stroke point is tagged (for some editing operation) */
	GP_SPOINT_TAG       = (1 << 1),
} eGPSPoint_Flag;

/* ***************************************** */
/* GP Fill - Triangle Tesselation Data */

/* Grease-Pencil Annotations - 'Triangle'
 * 	-> A triangle contains the index of three vertices for filling the stroke
 *	   This is only used if high quality fill is enabled
 */
typedef struct bGPDtriangle {
	/* indices for tesselated triangle used for GP Fill */
	unsigned int verts[3];
	/* texture coordinates for verts */
	float uv[3][2];
} bGPDtriangle;

/* ***************************************** */

/* ***************************************** */
/* GP Palettes (Deprecated - 2.78 - 2.79 only) */

/* color of palettes */
typedef struct bGPDpalettecolor {
	struct bGPDpalettecolor *next, *prev;
	char info[64];           /* Color name. Must be unique. */
	float color[4];
	float fill[4];           /* color that should be used for drawing "fills" for strokes */
	short flag;              /* settings for palette color */
	char  pad[6];            /* padding for compiler alignment error */
} bGPDpalettecolor;

/* bGPDpalettecolor->flag */
typedef enum eGPDpalettecolor_Flag {
	/* color is active */
	PC_COLOR_ACTIVE = (1 << 0),
	/* don't display color */
	PC_COLOR_HIDE = (1 << 1),
	/* protected from further editing */
	PC_COLOR_LOCKED = (1 << 2),
	/* do onion skinning */
	PC_COLOR_ONIONSKIN = (1 << 3),
	/* "volumetric" strokes */
	PC_COLOR_VOLUMETRIC = (1 << 4)
} eGPDpalettecolor_Flag;

/* palette of colors */
typedef struct bGPDpalette {
	struct bGPDpalette *next, *prev;

	/* pointer to individual colours */
	ListBase colors;
	char info[64];          /* Palette name. Must be unique. */

	short flag;
	char pad[6];            /* padding for compiler alignment error */
} bGPDpalette;

/* bGPDpalette->flag */
typedef enum eGPDpalette_Flag {
	/* palette is active */
	PL_PALETTE_ACTIVE = (1 << 0)
} eGPDpalette_Flag;

/* ***************************************** */
/* GP Strokes */

/* Runtime temp data for bGPDstroke */
typedef struct bGPDstroke_runtime {
	/* runtime final colors (result of original colors and modifiers) */
	float tmp_stroke_rgba[4];
	float tmp_fill_rgba[4];

	/* temporary layer name only used during copy/paste to put the stroke in the original layer */
	char tmp_layerinfo[128];

	float multi_frame_falloff; /* runtime falloff factor (only for transform) */
} bGPDstroke_runtime;

/* Grease-Pencil Annotations - 'Stroke'
 * 	-> A stroke represents a (simplified version) of the curve
 *	   drawn by the user in one 'mousedown'->'mouseup' operation
 */
typedef struct bGPDstroke {
	struct bGPDstroke *next, *prev;

	bGPDspoint *points;		/* array of data-points for stroke */
	bGPDtriangle *triangles;/* tesselated triangles for GP Fill */
	int totpoints;          /* number of data-points in array */
	int tot_triangles;      /* number of triangles in array */

	short thickness;        /* thickness of stroke */
	short flag, pad[2];     /* various settings about this stroke */

	double inittime;		/* Init time of stroke */

	char colorname[128] DNA_DEPRECATED;    /* color name */

	int mat_nr;             /* material index */
	char pad_[4];

	struct MDeformVert *dvert;    /* vertex weight data */

	bGPDstroke_runtime runtime;
	char pad_1[4];
} bGPDstroke;

/* bGPDstroke->flag */
typedef enum eGPDstroke_Flag {
	/* stroke is in 3d-space */
	GP_STROKE_3DSPACE		= (1 << 0),
	/* stroke is in 2d-space */
	GP_STROKE_2DSPACE		= (1 << 1),
	/* stroke is in 2d-space (but with special 'image' scaling) */
	GP_STROKE_2DIMAGE		= (1 << 2),
	/* stroke is selected */
	GP_STROKE_SELECT		= (1 << 3),
	/* Recalculate triangulation for high quality fill (when true, force a new recalc) */
	GP_STROKE_RECALC_CACHES = (1 << 4),
	/* Flag used to indicate that stroke is closed and draw edge between last and first point */
	GP_STROKE_CYCLIC = (1 << 7),
	/* Flag used to indicate that stroke is used for fill close and must use fill color for stroke and no fill area */
	GP_STROKE_NOFILL = (1 << 8),
	/* only for use with stroke-buffer (while drawing eraser) */
	GP_STROKE_ERASER		= (1 << 15)
} eGPDstroke_Flag;

/* ***************************************** */
/* GP Frame */

/* Runtime temp data for bGPDframe */
typedef struct bGPDframe_runtime {
	float viewmatrix[4][4];     /* parent matrix for drawing */
} bGPDframe_runtime;

/* Grease-Pencil Annotations - 'Frame'
 *	-> Acts as storage for the 'image' formed by strokes
 */
typedef struct bGPDframe {
	struct bGPDframe *next, *prev;

	ListBase strokes;	/* list of the simplified 'strokes' that make up the frame's data */

	int framenum;		/* frame number of this frame */

	short flag;			/* temp settings */
	short key_type;		/* keyframe type (eBezTriple_KeyframeType) */

	bGPDframe_runtime runtime;
} bGPDframe;

/* bGPDframe->flag */
typedef enum eGPDframe_Flag {
	/* frame is being painted on */
	GP_FRAME_PAINT		= (1 << 0),
	/* for editing in Action Editor */
	GP_FRAME_SELECT		= (1 << 1)
} eGPDframe_Flag;

/* ***************************************** */
/* GP Layer */

/* Runtime temp data for bGPDlayer */
typedef struct bGPDlayer_runtime {
	struct GHash *derived_data;     /* runtime data created by modifiers */
	int icon_id;                    /* id for dynamic icon used to show annotation color preview for layer */
	char pad[4];
} bGPDlayer_runtime;

/* Grease-Pencil Annotations - 'Layer' */
typedef struct bGPDlayer {
	struct bGPDlayer *next, *prev;

	ListBase frames;		/* list of annotations to display for frames (bGPDframe list) */
	bGPDframe *actframe;	/* active frame (should be the frame that is currently being displayed) */

	short flag;				/* settings for layer */
	short onion_flag;		/* Per-layer onion-skinning flags (eGPDlayer_OnionFlag) */

	float color[4];			/* Color for strokes in layers. Used for annotations, and for ruler (which uses GPencil internally) */
	float fill[4];			/* Fill color for strokes in layers. Not used anymore (was only for) */

	char info[128];			/* name/reference info for this layer (i.e. "director's comments, 12/3")
							 * needs to be kept unique, as it's used as the layer identifier */

	short thickness;		/* thickness to apply to strokes (Annotations) */
	char pad_1[2];

	struct Object *parent;  /* parent object */
	float inverse[4][4];    /* inverse matrix (only used if parented) */
	char parsubstr[64];     /* String describing subobject info, MAX_ID_NAME-2 */
	short partype;

	short line_change;      /* Thickness adjustment */
	float tintcolor[4];     /* Color used to tint layer, alpha value is used as factor */
	float opacity;          /* Opacity of the layer */

	bGPDlayer_runtime runtime;
} bGPDlayer;

/* bGPDlayer->flag */
typedef enum eGPDlayer_Flag {
	/* don't display layer */
	GP_LAYER_HIDE			= (1 << 0),
	/* protected from further editing */
	GP_LAYER_LOCKED			= (1 << 1),
	/* layer is 'active' layer being edited */
	GP_LAYER_ACTIVE			= (1 << 2),
	/* draw points of stroke for debugging purposes */
	GP_LAYER_DRAWDEBUG 		= (1 << 3),
	/* for editing in Action Editor */
	GP_LAYER_SELECT			= (1 << 5),
	/* current frame for layer can't be changed */
	GP_LAYER_FRAMELOCK		= (1 << 6),
	/* don't render xray (which is default) */
	GP_LAYER_NO_XRAY		= (1 << 7),
	/* "volumetric" strokes */
	GP_LAYER_VOLUMETRIC		= (1 << 10),
	/* Unlock color */
	GP_LAYER_UNLOCK_COLOR 	= (1 << 12),
} eGPDlayer_Flag;

/* bGPDlayer->onion_flag */
typedef enum eGPDlayer_OnionFlag {
	/* do onion skinning */
	GP_LAYER_ONIONSKIN = (1 << 0),
} eGPDlayer_OnionFlag;

/* ***************************************** */
/* GP Datablock */

/* Runtime temp data for bGPdata */
typedef struct bGPdata_runtime {
	/* Drawing Manager cache */
	struct GHash *batch_cache_data;
	void *sbuffer;				/* stroke buffer (can hold GP_STROKE_BUFFER_MAX) */

	/* GP Object drawing */
	float scolor[4];            /* buffer stroke color */
	float sfill[4];             /* buffer fill color */
	short mode;                 /* settings for color */
	short bstroke_style;        /* buffer style for drawing strokes (used to select shader type) */
	short bfill_style;          /* buffer style for filling areas (used to select shader type) */

	/* Stroke Buffer data (only used during paint-session)
	* 	- buffer must be initialized before use, but freed after
	*	  whole paint operation is over
	*/
	short sbuffer_size;			/* number of elements currently in cache */
	short sbuffer_sflag;		/* flags for stroke that cache represents */
	char pad_[6];
} bGPdata_runtime;

/* Grease-Pencil Annotations - 'DataBlock' */
typedef struct bGPdata {
	ID id;					/* Grease Pencil data is a datablock */
	struct AnimData *adt;   /* animation data - for animating draw settings */

	/* Grease-Pencil data */
	ListBase layers;		/* bGPDlayers */
	int flag;				/* settings for this datablock */

	short xray_mode;            /* xray mode for strokes (eGP_DepthOrdering) */
	char pad_1[2];

	/* Palettes */
	ListBase palettes DNA_DEPRECATED;    /* list of bGPDpalette's   - Deprecated (2.78 - 2.79 only) */

	/* 3D Viewport/Appearance Settings */
	float pixfactor;            /* factor to define pixel size conversion */
	float line_color[4];        /* color for edit line */

	/* Onion skinning */
	float onion_factor;         /* onion alpha factor change */
	int onion_mode;             /* onion skinning range (eGP_OnionModes) */
	int onion_flag;             /* onion skinning flags (eGPD_OnionFlag) */
	short gstep;			    /* Ghosts Before: max number of ghost frames to show between active frame and the one before it (0 = only the ghost itself) */
	short gstep_next;		    /* Ghosts After:  max number of ghost frames to show after active frame and the following it    (0 = only the ghost itself) */

	float gcolor_prev[3];	    /* optional color for ghosts before the active frame */
	float gcolor_next[3];	    /* optional color for ghosts after the active frame */

	char pad[4];
	struct Material **mat;      /* materials array */
	short totcol;               /* total materials */

	/* stats */
	short totlayer;
	short totframe;
	short totstroke;
	short totpoint;
	char pad_2[6];
	bGPdata_runtime runtime;
} bGPdata;

/* bGPdata->flag */
/* NOTE: A few flags have been deprecated since early 2.5,
 *       since they have been made redundant by interaction
 *       changes made during the porting process.
 */
typedef enum eGPdata_Flag {
	/* datablock is used for "annotations"
	 * NOTE: This flag used to be used in 2.4x, but should hardly ever have been set.
	 *       We can use this freely now, as all GP datablocks from pre-2.8 will get
	 *       set on file load (as many old use cases are for "annotations" only)
	 */
	GP_DATA_ANNOTATIONS = (1 << 0),

	/* show debugging info in viewport (i.e. status print) */
	GP_DATA_DISPINFO	= (1 << 1),
	/* in Action Editor, show as expanded channel */
	GP_DATA_EXPAND		= (1 << 2),

	/* is the block overriding all clicks? */
	/* GP_DATA_EDITPAINT = (1 << 3), */

/* ------------------------------------------------ DEPRECATED */
	/* new strokes are added in viewport space */
	GP_DATA_VIEWALIGN	= (1 << 4),

	/* Project into the screen's Z values */
	GP_DATA_DEPTH_VIEW	= (1 << 5),
	GP_DATA_DEPTH_STROKE = (1 << 6),

	GP_DATA_DEPTH_STROKE_ENDPOINTS = (1 << 7),
/* ------------------------------------------------ DEPRECATED */

	/* Stroke Editing Mode - Toggle to enable alternative keymap for easier editing of stroke points */
	GP_DATA_STROKE_EDITMODE	= (1 << 8),

	/* Main flag to switch onion skinning on/off */
	GP_DATA_SHOW_ONIONSKINS = (1 << 9),
	/* Draw a green and red point to indicate start and end of the stroke */
	GP_DATA_SHOW_DIRECTION = (1 << 10),

	/* Batch drawing cache need to be recalculated */
	GP_DATA_CACHE_IS_DIRTY = (1 << 11),

	/* Stroke Paint Mode - Toggle paint mode */
	GP_DATA_STROKE_PAINTMODE = (1 << 12),
	/* Stroke Editing Mode - Toggle sculpt mode */
	GP_DATA_STROKE_SCULPTMODE = (1 << 13),
	/* Stroke Editing Mode - Toggle weight paint mode */
	GP_DATA_STROKE_WEIGHTMODE = (1 << 14),

	/* keep stroke thickness unchanged when zoom change */
	GP_DATA_STROKE_KEEPTHICKNESS = (1 << 15),

	/* Allow edit several frames at the same time */
	GP_DATA_STROKE_MULTIEDIT = (1 << 16),
} eGPdata_Flag;

/* gpd->onion_flag */
typedef enum eGPD_OnionFlag {
	/* use custom color for ghosts before current frame */
	GP_ONION_GHOST_PREVCOL = (1 << 0),
	/* use custom color for ghosts after current frame */
	GP_ONION_GHOST_NEXTCOL = (1 << 1),
	/* always show onion skins (i.e. even during renders/animation playback) */
	GP_ONION_GHOST_ALWAYS = (1 << 2),
	/* use fade color in onion skin */
	GP_ONION_FADE = (1 << 3),
	/* Loop showing first frame after last frame */
	GP_ONION_LOOP = (1 << 4),
} eGPD_OnionFlag;

/* gpd->onion_mode */
typedef enum eGP_OnionModes {
	GP_ONION_MODE_ABSOLUTE = 0,
	GP_ONION_MODE_RELATIVE = 1,
	GP_ONION_MODE_SELECTED = 2,
} eGP_OnionModes;

/* xray modes (Depth Ordering) */
typedef enum eGP_DepthOrdering {
	GP_XRAY_FRONT = 0,
	GP_XRAY_3DSPACE = 1,
	GP_XRAY_BACK  = 2
} eGP_DepthOrdering;

/* ***************************************** */
/* Mode Checking Macros */

/* Check if 'multiedit sessions' is enabled */
#define GPENCIL_MULTIEDIT_SESSIONS_ON(gpd) \
	((gpd) && (gpd->flag & \
	           (GP_DATA_STROKE_EDITMODE | GP_DATA_STROKE_SCULPTMODE | GP_DATA_STROKE_WEIGHTMODE)) && \
	 (gpd->flag & GP_DATA_STROKE_MULTIEDIT))

/* Macros to check grease pencil modes */
#define GPENCIL_ANY_MODE(gpd) \
	((gpd) && (gpd->flag & \
	           (GP_DATA_STROKE_PAINTMODE | GP_DATA_STROKE_EDITMODE | \
	            GP_DATA_STROKE_SCULPTMODE | GP_DATA_STROKE_WEIGHTMODE)))
#define GPENCIL_ANY_EDIT_MODE(gpd) \
	((gpd) && (gpd->flag & (GP_DATA_STROKE_EDITMODE | GP_DATA_STROKE_SCULPTMODE | GP_DATA_STROKE_WEIGHTMODE)))
#define GPENCIL_PAINT_MODE(gpd) \
	((gpd) && (gpd->flag & (GP_DATA_STROKE_PAINTMODE)))
#define GPENCIL_SCULPT_OR_WEIGHT_MODE(gpd) \
	((gpd) && (gpd->flag & (GP_DATA_STROKE_SCULPTMODE | GP_DATA_STROKE_WEIGHTMODE)))
#define GPENCIL_NONE_EDIT_MODE(gpd) \
	((gpd) && ((gpd->flag & (GP_DATA_STROKE_EDITMODE | GP_DATA_STROKE_SCULPTMODE | GP_DATA_STROKE_WEIGHTMODE)) == 0))
#define GPENCIL_LAZY_MODE(brush, shift) \
	(((brush) && ((brush->gpencil_settings->flag & GP_BRUSH_STABILIZE_MOUSE) && (shift == 0))) || \
	 (((brush->gpencil_settings->flag & GP_BRUSH_STABILIZE_MOUSE) == 0) && (shift == 1)))

#endif /*  __DNA_GPENCIL_TYPES_H__ */
