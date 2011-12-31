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
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file DNA_gpencil_types.h
 *  \ingroup DNA
 */

#ifndef DNA_GPENCIL_TYPES_H
#define DNA_GPENCIL_TYPES_H

#include "DNA_listBase.h"
#include "DNA_ID.h"

/* Grease-Pencil Annotations - 'Stroke Point'
 *	-> Coordinates may either be 2d or 3d depending on settings at the time
 * 	-> Coordinates of point on stroke, in proportions of window size
 *	   This assumes that the bottom-left corner is (0,0)
 */
typedef struct bGPDspoint {
	float x, y, z;			/* co-ordinates of point (usually 2d, but can be 3d as well) */				
	float pressure;			/* pressure of input device (from 0 to 1) at this point */
} bGPDspoint;

/* Grease-Pencil Annotations - 'Stroke'
 * 	-> A stroke represents a (simplified version) of the curve
 *	   drawn by the user in one 'mousedown'->'mouseup' operation
 */
typedef struct bGPDstroke {
	struct bGPDstroke *next, *prev;
	
	bGPDspoint *points;		/* array of data-points for stroke */
	int totpoints;			/* number of data-points in array */
	
	short thickness;		/* thickness of stroke (currently not used) */	
	short flag;				/* various settings about this stroke */
} bGPDstroke;

/* bGPDstroke->flag */
	/* stroke is in 3d-space */
#define GP_STROKE_3DSPACE		(1<<0)
	/* stroke is in 2d-space */
#define GP_STROKE_2DSPACE		(1<<1)
	/* stroke is in 2d-space (but with special 'image' scaling) */
#define GP_STROKE_2DIMAGE		(1<<2)
	/* only for use with stroke-buffer (while drawing eraser) */
#define GP_STROKE_ERASER		(1<<15)


/* Grease-Pencil Annotations - 'Frame'
 *	-> Acts as storage for the 'image' formed by strokes
 */
typedef struct bGPDframe {
	struct bGPDframe *next, *prev;
	
	ListBase strokes;	/* list of the simplified 'strokes' that make up the frame's data */
	
	int framenum;		/* frame number of this frame */
	int flag;			/* temp settings */
} bGPDframe;

/* bGPDframe->flag */	
	/* frame is being painted on */
#define GP_FRAME_PAINT		(1<<0)
	/* for editing in Action Editor */
#define GP_FRAME_SELECT		(1<<1)


/* Grease-Pencil Annotations - 'Layer' */
typedef struct bGPDlayer {
	struct bGPDlayer *next, *prev;
	
	ListBase frames;		/* list of annotations to display for frames (bGPDframe list) */
	bGPDframe *actframe;	/* active frame (should be the frame that is currently being displayed) */
	
	int flag;				/* settings for layer */		
	short thickness;		/* current thickness to apply to strokes */
	short gstep;			/* max number of frames between active and ghost to show (0=only those on either side) */
	
	float color[4];			/* color that should be used to draw all the strokes in this layer */
	
	char info[128];			/* optional reference info about this layer (i.e. "director's comments, 12/3")
							 * this is used for the name of the layer  too and kept unique. */
} bGPDlayer;

/* bGPDlayer->flag */
	/* don't display layer */
#define GP_LAYER_HIDE		(1<<0)
	/* protected from further editing */
#define GP_LAYER_LOCKED		(1<<1)	
	/* layer is 'active' layer being edited */
#define GP_LAYER_ACTIVE		(1<<2)
	/* draw points of stroke for debugging purposes */
#define GP_LAYER_DRAWDEBUG 	(1<<3)
	/* do onionskinning */
#define GP_LAYER_ONIONSKIN	(1<<4)
	/* for editing in Action Editor */
#define GP_LAYER_SELECT		(1<<5)
	/* current frame for layer can't be changed */
#define GP_LAYER_FRAMELOCK	(1<<6)
	/* don't render xray (which is default) */
#define GP_LAYER_NO_XRAY	(1<<7)


/* Grease-Pencil Annotations - 'DataBlock' */
typedef struct bGPdata {
	ID id;					/* Grease Pencil data is */
	
	/* saved Grease-Pencil data */
	ListBase layers;		/* bGPDlayers */
	int flag;				/* settings for this datablock */
	
	/* not-saved stroke buffer data (only used during paint-session) 
	 * 	- buffer must be initialised before use, but freed after 
	 *	  whole paint operation is over
	 */
	short sbuffer_size;			/* number of elements currently in cache */
	short sbuffer_sflag;		/* flags for stroke that cache represents */
	void *sbuffer;				/* stroke buffer (can hold GP_STROKE_BUFFER_MAX) */
} bGPdata;

/* bGPdata->flag */
// XXX many of these flags should be depreceated for more general ideas in 2.5
	/* don't allow painting to occur at all */
	// XXX is depreceated - not well understood
#define GP_DATA_LMBPLOCK	(1<<0)
	/* show debugging info in viewport (i.e. status print) */
#define GP_DATA_DISPINFO	(1<<1)
	/* in Action Editor, show as expanded channel */
#define GP_DATA_EXPAND		(1<<2)
	/* is the block overriding all clicks? */
	// XXX is depreceated - nasty old concept
#define GP_DATA_EDITPAINT	(1<<3)
	/* new strokes are added in viewport space */
#define GP_DATA_VIEWALIGN	(1<<4)
	/* Project into the screens Z values */
#define GP_DATA_DEPTH_VIEW	(1<<5)
#define GP_DATA_DEPTH_STROKE (1<<6)

#define GP_DATA_DEPTH_STROKE_ENDPOINTS (1<<7)

#endif /*  DNA_GPENCIL_TYPES_H */
