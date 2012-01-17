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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file boolop/intern/BOP_BSPTree.cpp
 *  \ingroup boolopintern
 */

 
#include "BOP_BSPTree.h"
#include <vector>
#include <iostream>

/**
 * Constructs a new BSP tree.
 */
BOP_BSPTree::BOP_BSPTree()
{
	m_root = NULL;
	m_bspBB = NULL;
}

/**
 * Destroys a BSP tree.
 */
BOP_BSPTree::~BOP_BSPTree()
{
	if (m_root!=NULL) delete m_root;
	if (m_bspBB!=NULL) delete m_bspBB;
}

/**
 * Adds all mesh faces to BSP tree.
 * @param mesh mesh to add.
 * @param facesList face list to add.
 */
void BOP_BSPTree::addMesh(BOP_Mesh* mesh, BOP_Faces& facesList)
{
	for (BOP_IT_Faces it = facesList.begin(); it != facesList.end(); ++it) {
		addFace( mesh, *it );
	}
	
}

/**
 * Adds a new face into bsp tree.
 * @param mesh Input data for BSP tree.
 * @param face index to mesh face.
 */

void BOP_BSPTree::addFace(BOP_Mesh* mesh, BOP_Face* face)
{
	addFace(mesh->getVertex(face->getVertex(0))->getPoint(),
			mesh->getVertex(face->getVertex(1))->getPoint(),
			mesh->getVertex(face->getVertex(2))->getPoint(),
			face->getPlane());
}

/**
 * Adds new facee to the bsp-tree.
 * @param p1 first face point.
 * @param p2 second face point.
 * @param p3 third face point.
 * @param plane face plane.
 */
void BOP_BSPTree::addFace(const MT_Point3& p1, 
						  const MT_Point3& p2, 
						  const MT_Point3& p3, 
						  const MT_Plane3& plane)
{
	if (m_root == NULL)
		m_root = new BOP_BSPNode(plane);
	else {
		BOP_BSPPoints pts;

		pts.push_back(p1);
		pts.push_back(p2);
		pts.push_back(p3);

		m_root->addFace(pts,plane);
	}

	// update bounding box
	m_bbox.add(p1);
	m_bbox.add(p2);
	m_bbox.add(p3);
}

/**
 * Tests face vs bsp-tree (returns where is the face respect bsp planes).
 * @param p1 first face triangle point.
 * @param p2 secons face triangle point.
 * @param p3 third face triangle point.
 * @param plane face plane.
 * @return BSP_IN, BSP_OUT or BSP_IN_OUT
 */
BOP_TAG BOP_BSPTree::classifyFace(const MT_Point3& p1, 
								  const MT_Point3& p2, 
								  const MT_Point3& p3, 
								  const MT_Plane3& plane) const
{
	if ( m_root != NULL )
	  return m_root->classifyFace(p1, p2, p3, plane);
	else
	  return OUT;
}

/**
 * Filters a face using the BSP bounding infomation.
 * @param p1 first face triangle point.
 * @param p2 secons face triangle point.
 * @param p3 third face triangle point.
 * @param face face to test.
 * @return UNCLASSIFIED, BSP_IN, BSP_OUT or BSP_IN_OUT
 */
BOP_TAG BOP_BSPTree::filterFace(const MT_Point3& p1, 
								const MT_Point3& p2, 
								const MT_Point3& p3, 
								BOP_Face* face)
{
	if ( m_bspBB != NULL ) {
		return m_bspBB->classifyFace(p1,p2,p3,face->getPlane());
	}
	else
		return UNCLASSIFIED;
}

/**
 * Tests face vs bsp-tree (returns where is the face respect bsp planes).
 * @param p1 first face triangle point.
 * @param p2 secons face triangle point.
 * @param p3 third face triangle point.
 * @param plane face plane.
 * @return BSP_IN, BSP_OUT or BSP_IN_OUT
 */
BOP_TAG BOP_BSPTree::simplifiedClassifyFace(const MT_Point3& p1, 
											const MT_Point3& p2, 
											const MT_Point3& p3, 
											const MT_Plane3& plane) const
{
	if ( m_root != NULL )
	  return m_root->simplifiedClassifyFace(p1, p2, p3, plane);
	else
	  return OUT;
}

/**
 * Returns the deep of this BSP tree.
 * @return tree deep
 */
unsigned int BOP_BSPTree::getDeep() const
{
	if ( m_root != NULL )
	  return m_root->getDeep();
	else
	  return 0;
}

/**
 * Prints debug information.
 */
void BOP_BSPTree::print()
{
	if ( m_root != NULL )
		m_root->print( 0 );
}

