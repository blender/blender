#ifndef CCG_SplitFunction_H
#define CCG_SplitFunction_H
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

// This class contains the all important split function.
// It's a bit of weird definition but doing it like this 
// means we don't have to copy any code between implementations
// that preserve mesh connectivity and those that introduce T junctions.

template <typename TMesh, typename TSplitFunctionBindor> class SplitFunction
{
private :

	TMesh& m_mesh;
	TSplitFunctionBindor& m_functionBindor;

public :

	SplitFunction( TMesh& mesh, TSplitFunctionBindor& functionBindor)
	: m_mesh(mesh), m_functionBindor(functionBindor)
	{};

		void 
	SplitPolygon(
		const int p1Index,
		const MT_Plane3& plane,
		int& inPiece,
		int& outPiece,
		const MT_Scalar onEpsilon
	){

		const TMesh::Polygon& p = m_mesh.Polys()[p1Index];
		TMesh::Polygon inP(p),outP(p);
		
		inP.Verts().clear();
		outP.Verts().clear();

		// Disconnect the polygon from the mesh.
		m_functionBindor.DisconnectPolygon(p1Index);

		int lastIndex = p.Verts().back();	
		MT_Point3 lastVertex = m_mesh.Verts()[lastIndex].Pos();
				
		int lastClassification = CSG_Geometry::ComputeClassification(plane.signedDistance(lastVertex),onEpsilon);
		int totalClassification(lastClassification);			

		// iterate through the vertex indices of the to-be-split polygon

		int i;
		int j=p.Size()-1;
		for (i = 0; i < p.Size(); j = i, ++i)
		{
			int newIndex = p[i];
			MT_Point3 aVertex = m_mesh.Verts()[newIndex].Pos();
			int newClassification = CSG_Geometry::ComputeClassification(plane.signedDistance(aVertex),onEpsilon);

			// if neither the new vertex nor the old vertex were on and they differ
			if ((newClassification != lastClassification) && newClassification && lastClassification)
			{
				// create new vertex		
				int newVertexIndex = m_mesh.Verts().size();
				m_mesh.Verts().push_back(TMesh::Vertex());

				// work out new vertex position.
				MT_Vector3 v = aVertex - lastVertex;
				MT_Scalar sideA = plane.signedDistance(lastVertex);

				MT_Scalar epsilon = -sideA/plane.Normal().dot(v);
				m_mesh.Verts().back().Pos() = lastVertex + (v * epsilon);

				// Make a new VertexProp 
				TMesh::Polygon::TVProp splitProp(newVertexIndex,p.VertexProps(j),p.VertexProps(i),epsilon);
				

				// add new index to both polygons.
				inP.Verts().push_back(  splitProp );
				outP.Verts().push_back(	splitProp );
			
				// insert vertex into any neighbouring polygons of this edge
				m_functionBindor.InsertVertexAlongEdge(lastIndex,newIndex,splitProp);

			} 
			
			Classify(inP.Verts(),outP.Verts(),newClassification,p.VertexProps(i));
			lastClassification = newClassification;
			totalClassification |= newClassification;
			lastVertex = aVertex;	
			lastIndex = newIndex;
		}

		if (totalClassification == 3)
		{
			// replace polygon p with the inpiece and add the outPiece to the back 
			// of the mesh. we just replace the vertices as the normal will be the same.
			
			inPiece = p1Index;
			outPiece = m_mesh.Polys().size();
			
			m_mesh.Polys()[p1Index] = inP;
			m_mesh.Polys().push_back( outP );

			m_functionBindor.ConnectPolygon(inPiece);
			m_functionBindor.ConnectPolygon(outPiece);

		} else {

			// remember to connect back the original polygon!
			m_functionBindor.ConnectPolygon(p1Index);
		
			// dont touch the mesh but just return the index of the original polygon
			if (totalClassification == 1) 
			{
				inPiece = p1Index;
				outPiece = -1;
			} else {
				outPiece = p1Index;
				inPiece = -1;
			}
		}		
	}	


	void Classify(
		TMesh::Polygon::TVPropList &inGroup,
		TMesh::Polygon::TVPropList &outGroup,
		int classification,
		TMesh::Polygon::TVProp prop
	) {
		switch (classification) 
		{
			case 0 :
				inGroup.push_back(prop);
				outGroup.push_back(prop);
				break;
			case 1 :
				inGroup.push_back(prop);
				break;
			case 2 :
				outGroup.push_back(prop);
				break;
			default :
				break;
		}
	}

	~SplitFunction() {};
};


template <typename PROP> class DefaultSplitFunctionBindor
{
public :

	DefaultSplitFunctionBindor() {};

	void DisconnectPolygon(int){
	}

	void ConnectPolygon(int) {
	}
	
	void InsertVertexAlongEdge(int,int,const PROP& ) {
	}

	~DefaultSplitFunctionBindor(){};
};


#endif