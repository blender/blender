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

#ifndef BSP_CSGMeshBuilder_h

#define BSP_CSGMeshBuilder_h

#include "../extern/CSG_BooleanOps.h"
#include "BSP_CSGMesh.h"
#include "MEM_NonCopyable.h"
#include "MEM_SmartPtr.h"

/**
 * This class helps you to build a mesh from 2 seperate vertex/face
 * iterators defined in the external interface of the bsp module.
 * This code should really become party of a generic C++ mesh interface
 * but later...
 */

class BSP_CSGMeshBuilder : public MEM_NonCopyable{

public :

	/**
	 * Return a new BSP_CSGMesh with the desired props
	 * built from the given face and vertex iterators.
	 * The iterators are exhausted by this action.
	 */

	static 
		MEM_SmartPtr<BSP_CSGMesh> 
	NewMesh(
		CSG_MeshPropertyDescriptor &props,
		CSG_FaceIteratorDescriptor &face_it,
		CSG_VertexIteratorDescriptor &vertex_it
	); 

private :

	BSP_CSGMeshBuilder(
	);

};


#endif

