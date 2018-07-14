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

/** \file gizmo_geometry.h
 *  \ingroup wm
 *
 * \name Gizmo Geometry
 *
 * \brief Prototypes for arrays defining the gizmo geometry. The actual definitions can be found in files usually
 *        called geom_xxx_gizmo.c
 */


#ifndef __GIZMO_GEOMETRY_H__
#define __GIZMO_GEOMETRY_H__

typedef struct GizmoGeomInfo {
	int nverts;
	int ntris;
	const float (*verts)[3];
	const float (*normals)[3];
	const unsigned short *indices;
} GizmoGeomInfo;

/* arrow gizmo */
extern GizmoGeomInfo wm_gizmo_geom_data_arrow;

/* cube gizmo */
extern GizmoGeomInfo wm_gizmo_geom_data_cube;

/* dial gizmo */
extern GizmoGeomInfo wm_gizmo_geom_data_dial;

#endif  /* __GIZMO_GEOMETRY_H__ */
