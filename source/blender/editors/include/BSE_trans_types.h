/**
 * $Id: BSE_trans_types.h 12441 2007-10-31 13:56:07Z ton $
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

#ifndef BSE_TRANS_TYPES_H
#define BSE_TRANS_TYPES_H

struct Object;
struct MDeformVert;
struct ColorBand;

typedef struct TransOb {
	float *loc;
	float oldloc[9];
	float *eff;
	float oldeff[3];
	float *rot;
	float oldrot[12];
	float olddrot[3];
	float *quat;
	float oldquat[16];
	float olddquat[4];
	float *size;
	float oldsize[12];
	float olddsize[3];
	float obmat[3][3];
	float obinv[3][3];
	float parmat[3][3];
	float parinv[3][3];
	float obvec[3];
	int flag; /* keys */
	float *locx, *locy, *locz;
	float *rotx, *roty, *rotz;
	float *quatx, *quaty, *quatz, *quatw;
	float *sizex, *sizey, *sizez;
	/* __NLA */
	float axismat[3][3];	/* Restmat of object (for localspace transforms) */
	void *data;	/* Arbitrary data */
	/* end __NLA */
	struct Object *ob;
} TransOb;

typedef struct TransVert {
	float *loc;
	float oldloc[3], fac;
	float *val, oldval;
	int flag;
	float *nor;
} TransVert;

typedef struct VPaint {
	float r, g, b, a;
	float size;			/* of brush */
	float gamma, mul;
	short mode, flag;
	int tot, pad;						/* allocation size of prev buffers */
	unsigned int *vpaint_prev;			/* previous mesh colors */
	struct MDeformVert *wpaint_prev;	/* previous vertex weights */
} VPaint;

#endif /* BSE_TRANS_TYPES_H */

