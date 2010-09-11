/**
 * $Id$
 *
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
#ifndef ED_GPENCIL_INTERN_H
#define ED_GPENCIL_INTERN_H

/* internal exports only */


/* ***************************************************** */
/* Operator Defines */

struct wmOperatorType;

/* drawing ---------- */

void GPENCIL_OT_draw(struct wmOperatorType *ot);

/* Paint Modes for operator*/
typedef enum eGPencil_PaintModes {
	GP_PAINTMODE_DRAW = 0,
	GP_PAINTMODE_ERASER,
	GP_PAINTMODE_DRAW_STRAIGHT,
} eGPencil_PaintModes;

/* buttons editing --- */

void GPENCIL_OT_data_add(struct wmOperatorType *ot);
void GPENCIL_OT_data_unlink(struct wmOperatorType *ot);

void GPENCIL_OT_layer_add(struct wmOperatorType *ot);

void GPENCIL_OT_active_frame_delete(struct wmOperatorType *ot);

void GPENCIL_OT_convert(struct wmOperatorType *ot);


/******************************************************* */
/* FILTERED ACTION DATA - TYPES  ---> XXX DEPRECEATED OLD ANIM SYSTEM CODE! */

/* XXX - TODO: replace this with the modern bAnimListElem... */
/* This struct defines a structure used for quick access */
typedef struct bActListElem {
	struct bActListElem *next, *prev;
	
	void 	*data;		/* source data this elem represents */
	int 	type;		/* one of the ACTTYPE_* values */
	int		flag;		/* copy of elem's flags for quick access */
	int 	index;		/* copy of adrcode where applicable */
	
	void	*key_data;	/* motion data - ipo or ipo-curve */
	short	datatype;	/* type of motion data to expect */
	
	struct bActionGroup *grp;	/* action group that owns the channel */
	
	void 	*owner;		/* will either be an action channel or fake ipo-channel (for keys) */
	short	ownertype;	/* type of owner */
} bActListElem;

/******************************************************* */
/* FILTER ACTION DATA - METHODS/TYPES */

/* filtering flags  - under what circumstances should a channel be added */
typedef enum ACTFILTER_FLAGS {
	ACTFILTER_VISIBLE		= (1<<0),	/* should channels be visible */
	ACTFILTER_SEL			= (1<<1),	/* should channels be selected */
	ACTFILTER_FOREDIT		= (1<<2),	/* does editable status matter */
	ACTFILTER_CHANNELS		= (1<<3),	/* do we only care that it is a channel */
	ACTFILTER_IPOKEYS		= (1<<4),	/* only channels referencing ipo's */
	ACTFILTER_ONLYICU		= (1<<5),	/* only reference ipo-curves */
	ACTFILTER_FORDRAWING	= (1<<6),	/* make list for interface drawing */
	ACTFILTER_ACTGROUPED	= (1<<7)	/* belongs to the active group */
} ACTFILTER_FLAGS;

/* Action Editor - Main Data types */
typedef enum ACTCONT_TYPES {
	ACTCONT_NONE = 0,
	ACTCONT_ACTION,
	ACTCONT_SHAPEKEY,
	ACTCONT_GPENCIL
} ACTCONT_TYPES;




#endif /* ED_GPENCIL_INTERN_H */

