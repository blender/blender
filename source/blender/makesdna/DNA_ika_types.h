/**
 * blenlib/DNA_ika_types.h (mar-2001 nzc)
 *
 * Old ika types. These will be superceded by Reevan's stuff, soon (I
 * hope).
 *
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
#ifndef DNA_IKA_TYPES_H
#define DNA_IKA_TYPES_H

#include "DNA_listBase.h"
#include "DNA_ID.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


struct Object;
struct Ipo;

typedef struct Deform {
	struct Object *ob;
	short flag, partype;
	int par1, par2, par3;	/* kunnen vertexnrs zijn */
	float imat[4][4], premat[4][4], postmat[4][4];
	float vec[3];	/* als partype==LIMB, voor distfunc */
	float fac, dist, pad;
	
} Deform;

typedef struct Limb {
	struct Limb *next, *prev;
	
	float len, leno, fac, alpha, alphao, pad;
	float eff[2];
	
} Limb;

typedef struct Ika {
	ID id;
	
	short partype, flag, iter, lastfra;
	
	ListBase limbbase;
	float eff[3], effg[3], effn[3];	/* current, global en (local)wanted */
	float mem, slow, toty, totx, xyconstraint;
	
	struct Ipo *ipo;
	struct Object *parent;
	int par1, par2, par3;	/* kunnen vertexnrs zijn */

	int totdef;
	Deform *def;
	
	int def_scroll;
	int limb_scroll;
} Ika;

/* these defines are used for working with ikas*/

/* ika.flag: */
#define IK_GRABEFF		1
#define IK_XYCONSTRAINT	2

#endif

