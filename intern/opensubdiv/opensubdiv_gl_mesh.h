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
 * The Original Code is Copyright (C) 2018 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Sergey Sharybin.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __OPENSUBDIV_GL_MESH_H__
#define __OPENSUBDIV_GL_MESH_H__

struct OpenSubdiv_GLMeshDescr;
struct OpenSubdiv_TopologyRefinerDescr;
struct OpenSubdiv_GLMeshFVarData;

typedef struct OpenSubdiv_GLMesh {
	int evaluator_type;
	OpenSubdiv_GLMeshDescr *descriptor;
	OpenSubdiv_TopologyRefinerDescr *topology_refiner;
	OpenSubdiv_GLMeshFVarData *fvar_data;
} OpenSubdiv_GLMesh;

#endif  /* __OPENSUBDIV_GL_MESH_H__ */
