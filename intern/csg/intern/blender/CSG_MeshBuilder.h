#ifndef CSG_MeshBuilder_H
#define CSG_MeshBuilder_H
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
#include "CSG_Vertex.h"
#include "CSG_Polygon.h"
#include "CSG_Interface.h"
#include "CSG_BlenderMesh.h"

// build a mesh from external c interface to this module
////////////////////////////////////////////////////////

class MeshBuilder
{

public :

	// You must have set the size of the SimpleProps data ptr size
	// before calling this function.

	static AMesh* NewMesh(
		CSG_FaceIteratorDescriptor obBFaces,
		CSG_VertexIteratorDescriptor obBVertices
	) {
	
		AMesh *output = new AMesh();
	
		// first get the vertices.
		AMesh::VLIST& verts = output->Verts();	
		
		verts.reserve(obBVertices.num_elements);

		obBVertices.Reset(obBFaces.it);

		CSG_IVertex iVert;

		while (!obBVertices.Done(obBVertices.it))
		{
			obBVertices.Fill(obBVertices.it,&iVert);

			AMesh::Vertex aVertex;
			aVertex.Pos().setValue(iVert.position);
			verts.push_back(aVertex);
		
			obBVertices.Step(obBVertices.it);
		}

		// now for the faces
		////////////////////
	
		AMesh::PLIST &faces = output->Polys();
		faces.reserve(obBFaces.num_elements);

		CSG_IFace iFace;
		
		while (!obBFaces.Done(obBFaces.it))
		{
			obBFaces.Fill(obBFaces.it,&iFace);
						
			AMesh::Polygon aPolygon;
			aPolygon.FProp() = iFace.m_faceData;
		
			int i;
			for (i=0;i < 3; i++)
			{
				AMesh::Polygon::TVProp vProp;
				vProp.Data() = iFace.m_vertexData[i];
				aPolygon.Verts().push_back(vProp);
			}
			faces.push_back(aPolygon);

			if (iFace.m_vertexNumber == 4)
			{
				AMesh::Polygon::TVProp vProp[3];
				vProp[0].Data() = iFace.m_vertexData[2];
				vProp[1].Data() = iFace.m_vertexData[3];
				vProp[2].Data() = iFace.m_vertexData[0];

				aPolygon.VertexProps(0) = vProp[0];
				aPolygon.VertexProps(1) = vProp[1];
				aPolygon.VertexProps(2) = vProp[2];			

				faces.push_back(aPolygon);
			}

			obBFaces.Step(obBFaces.it);
		}
		
		return output;
	}
};


#endif

			




	

	