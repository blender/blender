/**
 * $Id$
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

#ifndef BSP_CSGHELPER_H

#define BSP_CSGHELPER_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


class BSP_CSGMesh;
class BSP_MeshFragment;

#include "../extern/CSG_BooleanOps.h"
#include "BSP_MeshPrimitives.h"

enum BSP_OperationType{
	e_intern_csg_union,
	e_intern_csg_intersection,
	e_intern_csg_difference
};

class BSP_CSGHelper {
public :

	BSP_CSGHelper(
	);

		bool
	ComputeOp(
		BSP_CSGMesh * obA,
		BSP_CSGMesh * obB,
		BSP_OperationType op_type, 
		BSP_CSGMesh & output,
		CSG_InterpolateUserFaceVertexDataFunc fv_func
	);

	
	~BSP_CSGHelper(
	);

private:

	// Iterate through the fragment,
	// add new vertices to output,
	// map polygons to new vertices.

		void
	DuplicateMesh(
		const BSP_MeshFragment & frag,
		BSP_CSGMesh & output
	);

		void
	TranslateSplitFragments(
		const BSP_MeshFragment & in_frag,
		const BSP_MeshFragment & out_frag,
		const BSP_MeshFragment & on_frag,
		BSP_Classification keep,
		BSP_MeshFragment & spanning_frag,
		BSP_MeshFragment & output
	);

		void
	MergeFrags(
		const BSP_MeshFragment & in,
		BSP_MeshFragment & out
	);

	

};

#endif

