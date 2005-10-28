/**
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
 
#ifndef BOP_MATERIALCONTAINER_H
#define BOP_MATERIALCONTAINER_H

#include "BOP_Mesh.h"
#include "BOP_Material.h"
#include "BOP_Interface.h"
#include <vector>
using namespace std;

typedef vector<BOP_Material> BOP_Materials;
typedef vector<BOP_Material>::iterator BOP_IT_Materials;

class BOP_MaterialContainer
{
private:
	BOP_Materials m_materialList;
	CSG_InterpolateUserFaceVertexDataFunc m_interpFunc;

public:
	BOP_MaterialContainer();
	~BOP_MaterialContainer();
	BOP_Index addMaterial(BOP_Material m);
	void setInterpFunc(CSG_InterpolateUserFaceVertexDataFunc interpFunc);
	BOP_Materials& getMaterialList();
	BOP_Material* getMaterial(BOP_Index index);
	char* getFaceMaterial(BOP_Index index);
	char* getFaceVertexMaterial(BOP_Mesh *mesh, 
								BOP_Index originalFaceIndex, 
								MT_Point3 point, 
								char* faceVertexMaterial);

	friend ostream &operator<<(ostream &stream, BOP_MaterialContainer *mc);
	  
private:
	char* interpolateMaterial(BOP_Mesh* mesh,
							  BOP_Face* face, 
							  BOP_Material& material,
							  MT_Point3 point, 
							  char* faceVertexMaterial);
};

#endif
