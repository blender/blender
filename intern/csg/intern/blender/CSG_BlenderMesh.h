#ifndef CSG_BlenderMesh_H
#define CSG_BlenderMesh_H
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

//typedefs for types used to represent blender meshes in the CSG library

#include "CSG_IndexDefs.h"
#include "CSG_ConnectedMesh.h"

#include "CSG_Vertex.h"
#include "CSG_CVertex.h"
#include "CSG_Polygon.h"
#include "CSG_Mesh.h"
#include "CSG_MeshWrapper.h"
#include "CSG_Interface.h"
#include "CSG_BlenderVProp.h"

typedef PolygonBase<BlenderVProp,CSG_IFaceData> TestPolygon;

typedef Mesh<TestPolygon,VertexBase> AMesh;
typedef Mesh<TestPolygon,CVertex > AConnectedMesh;

typedef MeshWrapper<AMesh> AMeshWrapper;
typedef ConnectedMeshWrapper<AConnectedMesh> AConnectedMeshWrapper;

#endif