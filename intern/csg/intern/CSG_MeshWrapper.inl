// Implementation of MeshWrapper template class.
////////////////////////////////////////////////
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
#include "CSG_Math.h"
#include "CSG_Triangulate.h"
#include "CSG_SplitFunction.h"

template <typename TMesh>
void MeshWrapper<TMesh>::ComputePlanes()
{
	PLIST& polyList = Polys();

	int i;
	for (i=0;i < polyList.size(); i++) 
	{
		TGBinder binder(m_mesh,i);
		polyList[i].SetPlane(CSG_Math<TGBinder>::ComputePlane(binder));
	}
}

template <typename TMesh>
void MeshWrapper<TMesh>::BurnTransform(const MT_Transform& t)
{
	VLIST& vertexList = Verts();

	int i;
	for (i=0;i<vertexList.size(); i++) 
	{
		vertexList[i].Pos() = t * vertexList[i].Pos();
	}
	ComputePlanes();

}

template <typename TMesh>
BBox MeshWrapper<TMesh>::ComputeBBox() const
{
	const VLIST& vertexList = Verts();
	
	BBox bbox;
	bbox.SetEmpty();

	int i;
	for (i=0;i<vertexList.size(); i++) 
	{
		bbox.Include(vertexList[i].Pos());
	}
	return bbox;

}


template <typename TMesh>
void MeshWrapper<TMesh>::Triangulate()
{
	vector<Polygon> newPolys;

	CSG_Triangulate<TGBinder> triangulator;
	int i;
	for (i=0; i< Polys().size(); ++i)
	{
		TGBinder pg(m_mesh,i);
		const Polygon& poly = Polys()[i];
		
		if (pg.Size() >= 4) 
		{
			VIndexList triangleList;

			if (triangulator.Process(pg,poly.Plane(),triangleList))
			{

				// translate the triangle list into the real vertex properties
				int tSize = triangleList.size();
				Polygon::TVPropList triangleProps(tSize);			
			
				int j;
				for (j=0; j < tSize; j++)
				{
					triangleProps[j] = poly.VertexProps(triangleList[j]);
				}

				// iterate through the new triangles
				for (j=0; j < tSize; j+=3)
				{
					// copy the polygon
					newPolys.push_back(poly);
					newPolys.back().Verts().clear();

					// copy the relevant triangle indices
					newPolys.back().Verts().assign(triangleProps.begin() + j,triangleProps.begin() + j +3);
				}

			}
		} else {
			if (pg.Size() >= 3) {
				newPolys.push_back(poly);
			}
		}			
	}

	// replace our polygons with the new ones.
	Polys() = newPolys;
};

template<typename TMesh>
void MeshWrapper<TMesh>::SplitPolygon(
	const int p1Index,
	const MT_Plane3& plane,
	int& inPiece,
	int& outPiece,
	const MT_Scalar onEpsilon
){	

	DefaultSplitFunctionBindor<TMesh::Polygon::TVProp> defaultSplitFunction;

	SplitFunction<MyType,DefaultSplitFunctionBindor<TMesh::Polygon::TVProp> > splitFunction(*this,defaultSplitFunction);
	splitFunction.SplitPolygon(p1Index,plane,inPiece,outPiece,onEpsilon);

}












