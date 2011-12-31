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
 * Contributor(s): Tao Ju
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef GEOCOMMON_H
#define GEOCOMMON_H

#define UCHAR unsigned char
#define USHORT unsigned short

#define USE_MINIMIZER

/**
 * Structure definitions for points and triangles.
 *
 * @author Tao Ju
 */


// 3d point with integer coordinates
typedef struct
{
	int x, y, z;
} Point3i;

typedef struct
{
	Point3i begin;
	Point3i end;
} BoundingBox;

// triangle that points to three vertices
typedef struct 
{
	float vt[3][3] ;
} Triangle;

// 3d point with float coordinates
typedef struct
{
	float x, y, z;
} Point3f;

typedef struct
{
	Point3f begin;
	Point3f end;
} BoundingBoxf;


#endif
