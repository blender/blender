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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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

#ifndef DNA_NLA_TYPES_H
#define DNA_NLA_TYPES_H

#include "DNA_listBase.h"

struct bAction;
struct Ipo;
struct Object;

/* simple uniform modifier structure, assumed it can hold all type info */
typedef struct bActionModifier {
	struct bActionModifier *next, *prev;
	short type, flag;
	char channel[32];
	
	/* noise modifier */
	float noisesize, turbul;
	short channels;
	
	/* path deform modifier */
	short no_rot_axis;
	struct Object *ob;	
} bActionModifier;

/* NLA-Modifier Types */
#define ACTSTRIP_MOD_DEFORM		0
#define ACTSTRIP_MOD_NOISE		1
#define ACTSTRIP_MOD_OOMPH		2

typedef struct bActionStrip {
	struct bActionStrip *next, *prev;
	short	flag, mode;
	short	stride_axis;		/* axis 0=x, 1=y, 2=z */
	short   curmod;				/* current modifier for buttons */

	struct	Ipo *ipo;			/* Blending ipo - was used for some old NAN era experiments. Non-functional currently. */
	struct	bAction *act;		/* The action referenced by this strip */
	struct  Object *object;		/* For groups, the actual object being nla'ed */
	float	start, end;			/* The range of frames covered by this strip */
	float	actstart, actend;	/* The range of frames taken from the action */
	float	actoffs;			/* Offset within action, for cycles and striding */
	float	stridelen;			/* The stridelength (considered when flag & ACT_USESTRIDE) */
	float	repeat;				/* The number of times to repeat the action range */
	float	scale;				/* The amount the action range is scaled by */

	float	blendin, blendout;	/* The number of frames on either end of the strip's length to fade in/out */
	
	char	stridechannel[32];	/* Instead of stridelen, it uses an action channel */
	char	offs_bone[32];		/* if repeat, use this bone/channel for defining offset */
	
	ListBase modifiers;			/* modifier stack */
} bActionStrip;

/* strip->mode (these defines aren't really used, but are here for reference) */
#define ACTSTRIPMODE_BLEND		0
#define ACTSTRIPMODE_ADD		1

/* strip->flag */
typedef enum eActStrip_Flag {
	ACTSTRIP_SELECT			= (1<<0),
	ACTSTRIP_USESTRIDE		= (1<<1),
	ACTSTRIP_BLENDTONEXT	= (1<<2),	/* Not implemented. Is not used anywhere */
	ACTSTRIP_HOLDLASTFRAME	= (1<<3),
	ACTSTRIP_ACTIVE			= (1<<4),
	ACTSTRIP_LOCK_ACTION	= (1<<5),
	ACTSTRIP_MUTE			= (1<<6),
	ACTSTRIP_REVERSE		= (1<<7),	/* This has yet to be implemented. To indicate that a strip should be played backwards */
	ACTSTRIP_CYCLIC_USEX	= (1<<8),
	ACTSTRIP_CYCLIC_USEY	= (1<<9),
	ACTSTRIP_CYCLIC_USEZ	= (1<<10),
	ACTSTRIP_AUTO_BLENDS	= (1<<11)
} eActStrip_Flag;

#endif

