/**
 * $Id: BIF_editaction.h 10519 2007-04-13 11:15:08Z aligorith $
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): 2007, Joshua Leung (major Action Editor recode)
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */
 
#ifndef BIF_EDITACTION_TYPES_H
#define BIF_EDITACTION_TYPES_H 

/******************************************************* */
/* FILTERED ACTION DATA - TYPES */

/* types of keyframe data in ActListElem */
#define ALE_NONE	0
#define ALE_IPO		1
#define ALE_ICU		2

/* This struct defines a structure used for quick access */
typedef struct bActListElem {
	struct bActListElem *next, *prev;
	
	void 	*data;		/* source data this elem represents */
	int 	type;		/* one of the ACTTYPE_* values */
	int		flag;		/* copy of elem's flags for quick access */
	
	void	*key_data;	/* motion data - ipo or ipo-curve */
	short	datatype;	/* type of motion data to expect */
	
	void 	*owner;		/* will either be an action channel or fake ipo-channel (for keys) */
	short	ownertype;	/* type of owner */
} bActListElem;

/******************************************************* */
/* FILTER ACTION DATA - METHODS/TYPES */

/* filtering flags  - under what circumstances should a channel be added */
#define ACTFILTER_VISIBLE		0x001	/* should channels be visible */
#define ACTFILTER_SEL			0x002	/* should channels be selected */
#define ACTFILTER_FOREDIT		0x004	/* does editable status matter */
#define ACTFILTER_CHANNELS		0x008	/* do we only care that it is a channel */
#define ACTFILTER_IPOKEYS		0x010	/* only channels referencing ipo's */
#define ACTFILTER_ONLYICU		0x020	/* only reference ipo-curves */

/* Action Editor - Main Data types */
#define ACTCONT_NONE		0
#define ACTCONT_ACTION		1
#define ACTCONT_SHAPEKEY	2

#endif
