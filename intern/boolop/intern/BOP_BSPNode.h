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

/** \file boolop/intern/BOP_BSPNode.h
 *  \ingroup boolopintern
 */

 
#ifndef BOP_BSPNODE_H
#define BOP_BSPNODE_H

#include "MT_Plane3.h"
#include "BOP_Tag.h"
#include "BOP_Face.h"

typedef std::vector<MT_Point3> BOP_BSPPoints;
typedef std::vector<MT_Point3>::const_iterator BOP_IT_BSPPoints;

class BOP_BSPNode
{
protected:
	BOP_BSPNode* m_inChild;
	BOP_BSPNode* m_outChild;
	MT_Plane3    m_plane;
	unsigned int m_deep;

public:
	// Construction methods
	BOP_BSPNode(const MT_Plane3& plane);
	~BOP_BSPNode();
	unsigned int addFace(const BOP_BSPPoints& pts, 
						 const MT_Plane3& plane);
	BOP_TAG classifyFace(const MT_Point3& p1, 
						 const MT_Point3& p2, 
						 const MT_Point3& p3, 
						 const MT_Plane3& plane) const;
	BOP_TAG simplifiedClassifyFace(const MT_Point3& p1, 
								   const MT_Point3& p2, 
								   const MT_Point3& p3, 
								   const MT_Plane3& plane) const;
	
protected:
	BOP_TAG testPoint(const MT_Point3& p) const;
	BOP_TAG classifyFaceIN(const MT_Point3& p1, 
						   const MT_Point3& p2, 
						   const MT_Point3& p3, 
						   const MT_Plane3& plane) const;
	BOP_TAG classifyFaceOUT(const MT_Point3& p1, 
							const MT_Point3& p2, 
							const MT_Point3& p3, 
							const MT_Plane3& plane) const;
	BOP_TAG simplifiedClassifyFaceIN(const MT_Point3& p1, 
									 const MT_Point3& p2, 
									 const MT_Point3& p3, 
									 const MT_Plane3& plane) const;
	BOP_TAG simplifiedClassifyFaceOUT(const MT_Point3& p1, 
									  const MT_Point3& p2, 
									  const MT_Point3& p3, 
									  const MT_Plane3& plane) const;
	bool hasSameOrientation(const MT_Plane3& plane) const;
	int compChildren() const;
	int splitTriangle(MT_Point3* res, 
					  const MT_Plane3& plane, 
					  const MT_Point3& p1, 
					  const MT_Point3& p2, 
					  const MT_Point3& p3, 
					  const BOP_TAG tag) const;

public:
	// Inline acces methods
	inline void setInChild(BOP_BSPNode* inChild) { m_inChild=inChild; };
	inline void setOutChild(BOP_BSPNode* outChild) { m_outChild=outChild; };
	inline BOP_BSPNode* getInChild() { return m_inChild; };
	inline BOP_BSPNode* getOutChild() { return m_outChild; };
	inline bool isLeaf() const { return !m_inChild && !m_outChild; };
	inline void setPlane(const MT_Plane3& plane) {m_plane=plane;};
	inline MT_Plane3& getPlane() { return m_plane; };

	inline unsigned int getDeep() const {return m_deep;};
	void print(unsigned int deep);
};

#endif
