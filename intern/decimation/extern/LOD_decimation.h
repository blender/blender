/**
 * $Id$
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/**

 * @author Laurence Bourn
 * @date 6/7/2001
 *
 * This is the external interface for the decimation module.
 */

#ifndef NAN_INCLUDED_LOD_decimation_h
#define NAN_INCLUDED_LOD_decimation_h

#ifdef __cplusplus
extern "C" {
#endif

/** 
 * External decimation structure
 */

typedef struct LOD_Decimation_Info {
	float * vertex_buffer;
	float * vertex_normal_buffer;
	int * triangle_index_buffer;
	int vertex_num;
	int face_num;
	void * intern;
} LOD_Decimation_Info; 

typedef LOD_Decimation_Info* LOD_Decimation_InfoPtr;

/** 
 * Create internal mesh representation from 
 * LOD_Decimation_Info structure.
 * @return 1 on successful loading
 * @return 0 on failure
 * @warning This should be changed to return an enumeration
 * detailing the error encountered
 */

extern int LOD_LoadMesh(LOD_Decimation_InfoPtr info);

/**
 * Allocate and Compute internal data strucures required for
 * decimation.
 * @return 1 on successful computation of data
 * @return 0 on failure
 * @warning This should be changed to return an enumeration
 * detailing the error encountered
 */

extern int LOD_PreprocessMesh(LOD_Decimation_InfoPtr info);

/** 
 * Once both the stages above have been completed
 * this function collapses a single edge in the mesh.
 * The LOD_Decimation_Info structure is updated
 * to represent the new mesh.
 * @return 1 if an edge was collapsed.
 * @return 0 if no suitable edge was found to be collapsable
 * You should stop calling this method in this case
 * @warning Do not expect that the order of polygons, vertices or
 * vertex normals will be preserved by this operation. This function
 * returns a packed array of polygons and vertices and so necessarily
 * the order will be different. This means you should not expect to
 * find the same polygon in the same place in the polygon array after 
 * this function has been called.
 */

extern int LOD_CollapseEdge(LOD_Decimation_InfoPtr info);

/** 
 * Free any memory the decimation process used 
 * during the decimation process
 * @return 1 if internal data successfully freed
 * @return 0 if no data was freed
 */

extern int LOD_FreeDecimationData(LOD_Decimation_InfoPtr);

#ifdef __cplusplus
}
#endif

#endif // NAN_INCLUDED_LOD_decimation_h

