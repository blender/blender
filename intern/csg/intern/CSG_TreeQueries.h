#ifndef CSG_TREEQUERIES_H
#define CSG_TREEQUERIES_H
/*
  CSGLib - Software Library for Constructive Solid Geometry
  Copyright (C) 2003-2004  Laurence Bourn

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Library General Public License for more details.

  You should have received a copy of the GNU Library General Public
  License along with this library; if not, write to the Free
  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

  Please send remarks, questions and bug reports to laurencebourn@hotmail.com
*/

#include "CSG_IndexDefs.h"
#include "CSG_BBoxTree.h"
#include "CSG_Math.h"

template <typename TMesh> class TreeIntersector
{
public :

	TreeIntersector(
		const BBoxTree& a,
		const BBoxTree& b,
		OverlapTable *aOverlapsB,
		OverlapTable *bOverlapsA,
		const TMesh* meshA,
		const TMesh* meshB
	) {
		m_aOverlapsB = aOverlapsB;
		m_bOverlapsA = bOverlapsA;
		m_meshA = meshA;
		m_meshB = meshB;
			
		MarkIntersectingPolygons(a.RootNode(),b.RootNode());		
	}

private :

	void MarkIntersectingPolygons(const BBoxNode*a,const BBoxNode *b)
	{
		if (!intersect(a->m_bbox, b->m_bbox)) return;
		if (a->m_tag == BBoxNode::LEAF && b->m_tag == BBoxNode::LEAF) 
		{
			const BBoxLeaf *la = (const BBoxLeaf *)a;
			const BBoxLeaf *lb = (const BBoxLeaf *)b;

			PolygonGeometry<TMesh> pg1(*m_meshA,la->m_polyIndex);
			PolygonGeometry<TMesh> pg2(*m_meshB,lb->m_polyIndex);

			if (CSG_PolygonIntersector<PolygonGeometry<TMesh>,PolygonGeometry<TMesh> >::
				IntersectPolygons(pg1,pg2,
					m_meshA->Polys()[la->m_polyIndex].Plane(),
					m_meshB->Polys()[lb->m_polyIndex].Plane()
				)
			) {
				(*m_aOverlapsB)[lb->m_polyIndex].push_back(la->m_polyIndex);
				(*m_bOverlapsA)[la->m_polyIndex].push_back(lb->m_polyIndex);			
			}
		} else 
		if ( a->m_tag == BBoxNode::LEAF || 
			(b->m_tag != BBoxNode::LEAF && a->m_bbox.Size() < b->m_bbox.Size())
		) {
			MarkIntersectingPolygons(a,((const BBoxInternal *)b)->lson);
			MarkIntersectingPolygons(a,((const BBoxInternal *)b)->rson);
		} else {
			MarkIntersectingPolygons(((const BBoxInternal *)a)->lson,b);
			MarkIntersectingPolygons(((const BBoxInternal *)a)->rson,b);
		}
	}

	// Temporaries held through recursive intersection
	OverlapTable* m_aOverlapsB;
	OverlapTable* m_bOverlapsA;
	const TMesh* m_meshA;
	const TMesh* m_meshB;

};

template<typename TMesh> class RayTreeIntersector
{
public:

	RayTreeIntersector(
		const BBoxTree& a,
		const TMesh* meshA,
		const MT_Line3& xRay,
		int& polyIndex
	):
		m_meshA(meshA),
		m_polyIndex(-1),
		m_lastIntersectValue(MT_INFINITY)
	{
		FindIntersectingPolygons(a.RootNode(),xRay);
		polyIndex = m_polyIndex;
	}

		
private :

	void FindIntersectingPolygons(const BBoxNode*a,const MT_Line3& xRay)
	{
		if ((xRay.Origin().x() + m_lastIntersectValue < a->m_bbox.Lower(0)) ||
			!a->m_bbox.IntersectXRay(xRay.Origin())
		) {
			// ray does not intersect this box.
			return;
		}

		if (a->m_tag == BBoxNode::LEAF)
		{
			const BBoxLeaf *la = (const BBoxLeaf *)a;
			MT_Scalar testParameter(0);
	
			PolygonGeometry<TMesh> pg(*m_meshA,la->m_polyIndex);

			if (
				CSG_Math<PolygonGeometry<TMesh> >::InstersectPolyWithLine3D(
					xRay,pg,m_meshA->Polys()[la->m_polyIndex].Plane(),testParameter
				)
			) {
				if (testParameter < m_lastIntersectValue) 
				{
					m_lastIntersectValue = testParameter;
					m_polyIndex = la->m_polyIndex;
				}
			}
		} else {
			FindIntersectingPolygons(((const BBoxInternal*)a)->lson,xRay);
			FindIntersectingPolygons(((const BBoxInternal*)a)->rson,xRay);
		}		
	}

	const TMesh* m_meshA;

	MT_Scalar m_lastIntersectValue;
	int m_polyIndex;
};









#endif
