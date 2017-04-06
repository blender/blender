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
 * The Original Code is Copyright (C) 2016 Blender Foundation.
 * All rights reserved.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/windowmanager/manipulators/intern/manipulator_library/manipulator_geometry.h
 *  \ingroup wm
 *
 * \name Manipulator Geometry
 *
 * \brief Prototypes for arrays defining the manipulator geometry. The actual definitions can be found in files usually
 *        called geom_xxx_manipulator.c
 */


#ifndef __MANIPULATOR_GEOMETRY_H__
#define __MANIPULATOR_GEOMETRY_H__

typedef struct ManipulatorGeomInfo {
	int nverts;
	int ntris;
	const float (*verts)[3];
	const float (*normals)[3];
	const unsigned short *indices;
} ManipulatorGeomInfo;

/* arrow manipulator */
extern ManipulatorGeomInfo wm_manipulator_geom_data_arrow;

/* cube manipulator */
extern ManipulatorGeomInfo wm_manipulator_geom_data_cube;

#endif  /* __MANIPULATOR_GEOMETRY_H__ */
