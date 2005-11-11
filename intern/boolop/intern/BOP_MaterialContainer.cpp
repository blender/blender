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
 
#include <iostream>
#include "BOP_MaterialContainer.h"
#include "BOP_MathUtils.h"
#include "MEM_SmartPtr.h"

/**
 * Constructs a new material container.
 */
BOP_MaterialContainer::BOP_MaterialContainer()
{
	m_interpFunc = NULL;
}

/**
 * Destroys a material container.
 */
BOP_MaterialContainer::~BOP_MaterialContainer()
{
}

/**
 * Adds a new material to this container.
 * @param m material material object to add.
 * @return the new material index.
 */
BOP_Index BOP_MaterialContainer::addMaterial(BOP_Material m)
{
	m_materialList.push_back(m);
	return m_materialList.size()-1;
}

/**
 * Updates the interpolation function of this container.
 * @param interpFunc the interpolation function.
 */
void BOP_MaterialContainer::setInterpFunc(CSG_InterpolateUserFaceVertexDataFunc interpFunc)
{
	m_interpFunc = interpFunc;
}

/**
 * Returns the material list.
 * @return
 */
BOP_Materials& BOP_MaterialContainer::getMaterialList()
{
	return m_materialList;
}

/**
 * Returns the material with the specified index.
 * @param index material index.
 * @return material with the specified index.
 */
BOP_Material* BOP_MaterialContainer::getMaterial(BOP_Index index)
{
	return index < m_materialList.size() ? &(m_materialList[index]) : NULL;
}

/**
 * Returns the pointer to face material specified by index.
 * @param index material index.
 * @return pointer to face material.
 */
char* BOP_MaterialContainer::getFaceMaterial(BOP_Index index)
{
	if (index < m_materialList.size())
		return m_materialList[index].getFaceMaterial();
	else return NULL;
}

/**
 * Returns a pointer to face vertex material, if is the material not exist, then
 * returns an interpoled material.
 * @param mesh original mesh data.
 * @param originalFaceIndex index to the original mesh face.
 * @param point point who needs a material.
 * @param faceVertexMaterial pointer to mem region where the material will be 
 * saved.
 * @return pointer to the face vertex material.
 */
char* BOP_MaterialContainer::getFaceVertexMaterial(BOP_Mesh *mesh, 
												   BOP_Index originalFaceIndex, 
												   MT_Point3 point, 
												   char* faceVertexMaterial)
{
	unsigned int i;

	if (originalFaceIndex>=m_materialList.size()) return NULL;

	BOP_Material& material = m_materialList[originalFaceIndex];
	
	if (material.isQuad()) {
		
		BOP_Face *face1 = mesh->getFace(material.getOriginalFace());
		BOP_Face *face2 = mesh->getFace(material.getOriginalFace()+1);
		
		if (!face1 || !face2) return NULL;
		
		// Search original point
		for (i=0;i<face1->size();i++) {
			if (point == mesh->getVertex(face1->getVertex(i))->getPoint()) {
				return material.getOriginalFaceVertexMaterial(face1->getVertex(i));
			}
		}
		for (i=0;i<face2->size();i++) {
			if (point == mesh->getVertex(face2->getVertex(i))->getPoint()) {
				return material.getOriginalFaceVertexMaterial(face2->getVertex(i));
			}
		}
		// wich is the half quad where the point is?
		MT_Vector3 N = face1->getPlane().Normal();
		MT_Point3 p0 = mesh->getVertex(face1->getVertex(0))->getPoint();
		MT_Point3 q(p0.x()+N.x(),p0.y()+N.y(),p0.z()+N.z());
		MT_Point3 p2 = mesh->getVertex(face1->getVertex(1))->getPoint();
		MT_Plane3 plane(p0,p2,q);
		
		if (BOP_sign(plane.signedDistance(point))==-1) {
			// first half quad
			faceVertexMaterial = interpolateMaterial(mesh, face1, material, point, faceVertexMaterial);
		}
		else {
			// second half quad
			faceVertexMaterial = interpolateMaterial(mesh, face2, material, point, faceVertexMaterial);
		}
	}
	else {
		BOP_Face *face1 = mesh->getFace(material.getOriginalFace());
		
		if (!face1) return NULL;
		
		// Search original point
		for (i=0;i<face1->size();i++) {
			if (point == mesh->getVertex(face1->getVertex(i))->getPoint())
				return material.getOriginalFaceVertexMaterial(face1->getVertex(i));
		}
		
		faceVertexMaterial = interpolateMaterial(mesh, face1, material, point, faceVertexMaterial);
	}
	
	return faceVertexMaterial;
}

/**
 * Performs vertex data interpolation.
 * @param mesh original mesh data.
 * @param face face used to interpolate an interior face point material
 * @param material face material, input data for implementation.
 * @param point interpolated point.
 * @param faceVertexMaterial pointer to memory region.
 * @return pointer to face vertex material.
 */
char* BOP_MaterialContainer::interpolateMaterial(BOP_Mesh* mesh,
												 BOP_Face* face, 
												 BOP_Material& material,
												 MT_Point3 point, 
												 char* faceVertexMaterial)
{
	//  (p1)-----(I)------(p2)
	//    \       |        /
	//     \      |       /
	//      \     |      /
	//       \ (point)  /
	//        \   |    /
	//         \  |   /
	//          \ |  /
	//           (p3)

	MT_Point3 p1 = mesh->getVertex(face->getVertex(0))->getPoint();
	MT_Point3 p2 = mesh->getVertex(face->getVertex(1))->getPoint();
	MT_Point3 p3 = mesh->getVertex(face->getVertex(2))->getPoint();
	MT_Point3 I = BOP_4PointIntersect(p1, p2, p3, point);
	MT_Scalar epsilon0 = 1.0-BOP_EpsilonDistance(p1, p2, I);
	MT_Scalar epsilon1 = 1.0-BOP_EpsilonDistance(I, p3, point);
	
	// Interpolate data
	if (m_interpFunc) {
		// temporal data
		char* faceVertexMaterialTemp = new char[material.getFaceVertexWidth()];
		
		(*m_interpFunc)(material.getOriginalFaceVertexMaterial(face->getVertex(0)),
						material.getOriginalFaceVertexMaterial(face->getVertex(1)),
						faceVertexMaterialTemp,
						epsilon0);

		(*m_interpFunc)(faceVertexMaterialTemp,
						material.getOriginalFaceVertexMaterial(face->getVertex(2)),
						faceVertexMaterial,
						epsilon1);

		// free temporal data
		delete[] faceVertexMaterialTemp;
		
	}
	else faceVertexMaterial = NULL;
	
	// return the result
	return (char*) faceVertexMaterial;
}

/**
 * Implements operator <<
 */
ostream &operator<<(ostream &stream, BOP_MaterialContainer *mc)
{
	stream << "***[ Material List ]***********************************************" << endl;
	BOP_IT_Materials it;
	for (it=mc->getMaterialList().begin();it!=mc->getMaterialList().end();++it) {
		stream << "[" << it - mc->getMaterialList().begin() << "] ";
		stream << &(*it);
	}
	stream << "*******************************************************************" << endl;
	return stream;
}
