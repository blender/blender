/*
 *
 * Some vector operations.
 *
 * Always use
 * - vector with x components :   float x[3], int x[3], etc
 *
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

/* ------------------------------------------------------------------------- */
/* General format: op(a, b, c): a = b op c                                   */
/* Copying is done cp <from, to>                                             */
/* ------------------------------------------------------------------------- */

#include "MTC_vectorops.h"
#include <math.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

void MTC_diff3Int(int v1[3], int v2[3], int v3[3])
{
	v1[0] = v2[0] - v3[0];
	v1[1] = v2[1] - v3[1];
	v1[2] = v2[2] - v3[2];
}

/* ------------------------------------------------------------------------- */
void MTC_diff3Float(float v1[3], float v2[3], float v3[3])
{
	v1[0] = v2[0] - v3[0];
	v1[1] = v2[1] - v3[1];
	v1[2] = v2[2] - v3[2];
}

/* ------------------------------------------------------------------------- */

void MTC_cross3Int(int v1[3], int v2[3], int v3[3])
{
	v1[0] = v2[1]*v3[2] - v2[2]*v3[1];
	v1[1] = v2[2]*v3[0] - v2[0]*v3[2];
	v1[2] = v2[0]*v3[1] - v2[1]*v3[0];
}

/* ------------------------------------------------------------------------- */

void MTC_cross3Float(float v1[3], float v2[3], float v3[3])
{
	v1[0] = v2[1]*v3[2] - v2[2]*v3[1];
	v1[1] = v2[2]*v3[0] - v2[0]*v3[2];
	v1[2] = v2[0]*v3[1] - v2[1]*v3[0];
}
/* ------------------------------------------------------------------------- */

void MTC_cross3Double(double v1[3], double v2[3], double v3[3])
{
	v1[0] = v2[1]*v3[2] - v2[2]*v3[1];
	v1[1] = v2[2]*v3[0] - v2[0]*v3[2];
	v1[2] = v2[0]*v3[1] - v2[1]*v3[0];
}

/* ------------------------------------------------------------------------- */

int MTC_dot3Int(int v1[3], int v2[3])
{
	return (v1[0]*v2[0] + v1[1]*v2[1] + v1[2]*v2[2]);
}

/* ------------------------------------------------------------------------- */

float MTC_dot3Float(float v1[3], float v2[3])
{
	return (v1[0]*v2[0] + v1[1]*v2[1] + v1[2]*v2[2]);
}

/* ------------------------------------------------------------------------- */

void MTC_cp3Float(float v1[3], float v2[3])
{
	v2[0] = v1[0];
	v2[1] = v1[1];
	v2[2] = v1[2];
}

/* ------------------------------------------------------------------------- */

void MTC_cp3FloatInv(float v1[3], float v2[3])
{
	v2[0] = -v1[0];
	v2[1] = -v1[1];
	v2[2] = -v1[2];
}

/* ------------------------------------------------------------------------- */

void MTC_swapInt(int *i1, int *i2)
{
	int swap;
	swap = *i1;
	*i1 = *i2;
	*i2 = swap;
}

/* ------------------------------------------------------------------------- */

void  MTC_diff3DFF(double v1[3], float v2[3], float v3[3])
{
	v1[0] = v2[0] - v3[0];
	v1[1] = v2[1] - v3[1];
	v1[2] = v2[2] - v3[2];
}

/* ------------------------------------------------------------------------- */
float MTC_normalize3DF(float n[3])
{
	float d;
	
	d= n[0]*n[0]+n[1]*n[1]+n[2]*n[2];
	/* FLT_EPSILON is too large! A larger value causes normalize errors in   */
	/* a scaled down utah teapot                                             */
	if(d>0.0000000000001) {

		/* d= sqrt(d);  This _should_ be sqrt, but internally it's a double*/
		/* anyway. This is safe.                                             */
		d = sqrt(d);
		
		n[0]/=d; 
		n[1]/=d; 
		n[2]/=d;
	} else {
		n[0]=n[1]=n[2]= 0.0;
		d= 0.0;
	}
	return d;
}

/* ------------------------------------------------------------------------- */

/* eof */
