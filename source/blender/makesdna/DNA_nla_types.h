/**
 * $Id$
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
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */


#ifndef DNA_NLA_TYPES_H
#define DNA_NLA_TYPES_H

struct bAction;
struct Ipo;

typedef struct bActionStrip {
	struct bActionStrip *next, *prev;
	short	flag;
	short	mode;
	int		reserved1;

	struct	Ipo *ipo;			/* Blending ipo */
	struct	bAction *act;		/* The action referenced by this strip */

	float	start, end;			/* The range of frames covered by this strip */
	float	actstart, actend;	/* The range of frames taken from the action */
	float	stridelen;			/* The stridelength (considered when flag & ACT_USESTRIDE) */
	float	repeat;				/* The number of times to repeat the action range */

	float	blendin, blendout;
} bActionStrip;

#define ACTSTRIPMODE_BLEND		0
#define ACTSTRIPMODE_ADD		1

#define ACTSTRIP_SELECT			0x00000001

#define ACTSTRIP_USESTRIDE		0x00000002
#define ACTSTRIP_BLENDTONEXT	0x00000004
#define ACTSTRIP_HOLDLASTFRAME	0x00000008

#define ACTSTRIP_SELECTBIT			0
#define ACTSTRIP_USESTRIDEBIT		1
#define ACTSTRIP_BLENDTONEXTBIT		2
#define ACTSTRIP_HOLDLASTFRAMEBIT	3
#endif

