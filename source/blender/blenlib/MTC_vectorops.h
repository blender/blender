/*
 * vectorops.h
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

#ifndef VECTOROPS_H
#define VECTOROPS_H 

/* ------------------------------------------------------------------------- */

void  MTC_diff3Int(int v1[3], int v2[3], int v3[3]);
void  MTC_cross3Int(int v1[3], int v2[3], int v3[3]);
int   MTC_dot3Int(int v1[3], int v2[3]); 

void  MTC_diff3Float(float v1[3], float v2[3], float v3[3]);
void  MTC_cross3Float(float v1[3], float v2[3], float v3[3]);
float MTC_dot3Float(float v1[3], float v2[3]); 
void  MTC_cp3Float(float v1[3], float v2[3]);
/**
 * Copy vector with a minus sign (so a = -b)
 */
void  MTC_cp3FloatInv(float v1[3], float v2[3]);

void  MTC_swapInt(int *i1, int *i2);

void  MTC_diff3DFF(double v1[3], float v2[3], float v3[3]);
void  MTC_cross3Double(double v1[3], double v2[3], double v3[3]);
float MTC_normalize3DF(float n[3]);

/* ------------------------------------------------------------------------- */
#endif /* VECTOROPS_H */

