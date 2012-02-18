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

/** \file boolop/intern/BOP_BSPTree.h
 *  \ingroup boolopintern
 */

 
#ifndef __BOP_BSPTREE_H__
#define __BOP_BSPTREE_H__

#include "BOP_BSPNode.h"
#include "BOP_Mesh.h"
#include "BOP_Tag.h"
#include "BOP_BBox.h"

class BOP_BSPTree
{
protected:
	BOP_BSPNode* m_root;
	BOP_BSPNode* m_bspBB;
	BOP_BBox     m_bbox;
public:
	// Construction methods
	BOP_BSPTree();
	virtual ~BOP_BSPTree();
	void addMesh(BOP_Mesh* mesh, BOP_Faces& facesList);
	void addFace(BOP_Mesh* mesh, BOP_Face* face);
	virtual void addFace(const MT_Point3& p1, 
						 const MT_Point3& p2, 
						 const MT_Point3& p3, 
						 const MT_Plane3& plane);
	BOP_TAG classifyFace(const MT_Point3& p1, 
						 const MT_Point3& p2, 
						 const MT_Point3& p3, 
						 const MT_Plane3& plane) const;
	BOP_TAG filterFace(const MT_Point3& p1, 
					   const MT_Point3& p2, 
					   const MT_Point3& p3, 
					   BOP_Face* face);
	BOP_TAG simplifiedClassifyFace(const MT_Point3& p1, 
								   const MT_Point3& p2, 
								   const MT_Point3& p3, 
								   const MT_Plane3& plane) const;
	unsigned int getDeep() const;
	void print();
	inline void setRoot(BOP_BSPNode* root) {m_root=root;};
	inline BOP_BSPNode* getRoot() const {return m_root;};
};

#endif

