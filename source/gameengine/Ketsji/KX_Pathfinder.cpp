/**
* $Id$ 
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

#include "KX_Pathfinder.h"
#include "RAS_MeshObject.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
extern "C" {
#include "BKE_scene.h"
#include "BKE_customdata.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_DerivedMesh.h"
}
#include "KX_PythonInit.h"
#include "KX_PyMath.h"
#include "Value.h"
#include "Recast.h"
#include "DetourStatNavMeshBuilder.h"

static const int MAX_PATH_LEN = 256;
static const float polyPickExt[3] = {2, 4, 2};

static void calcMeshBounds(const float* vert, int nverts, float* bmin, float* bmax)
{
	bmin[0] = bmax[0] = vert[0];
	bmin[1] = bmax[1] = vert[1];
	bmin[2] = bmax[2] = vert[2];
	for (int i=1; i<nverts; i++)
	{
		if (bmin[0]>vert[3*i+0]) bmin[0] = vert[3*i+0];
		if (bmin[1]>vert[3*i+1]) bmin[1] = vert[3*i+1];
		if (bmin[2]>vert[3*i+2]) bmin[2] = vert[3*i+2];

		if (bmax[0]<vert[3*i+0]) bmax[0] = vert[3*i+0];
		if (bmax[1]<vert[3*i+1]) bmax[1] = vert[3*i+1];
		if (bmax[2]<vert[3*i+2]) bmax[2] = vert[3*i+2];
	}
}

inline void flipAxes(float* vec)
{
	std::swap(vec[1],vec[2]);
}

KX_Pathfinder::KX_Pathfinder(void* sgReplicationInfo, SG_Callbacks callbacks)
:	KX_GameObject(sgReplicationInfo, callbacks)
,	m_navMesh(NULL)
{
	
}

KX_Pathfinder::~KX_Pathfinder()
{
	if (m_navMesh)
		delete m_navMesh;
}

bool KX_Pathfinder::BuildVertIndArrays(RAS_MeshObject* meshobj, float *&vertices, int& nverts,
									   unsigned short* &faces, int& npolys)
{
	if (!meshobj || meshobj->HasColliderPolygon()==false) 
	{
		return false;
	}

	DerivedMesh* dm = CDDM_from_mesh(meshobj->GetMesh(), NULL);

	MVert *mvert = dm->getVertArray(dm);
	MFace *mface = dm->getFaceArray(dm);
	int numpolys = dm->getNumFaces(dm);
	int numverts = dm->getNumVerts(dm);
	int* index = (int*)dm->getFaceDataArray(dm, CD_ORIGINDEX);
	MTFace *tface = (MTFace *)dm->getFaceDataArray(dm, CD_MTFACE);

	nverts = numverts;
	if (nverts >= 0xffff)
		return false;
	//calculate count of tris
	npolys = numpolys;
	for (int p2=0; p2<numpolys; p2++)
	{
		MFace* mf = &mface[p2];
		if (mf->v4)
			npolys+=1;
	}

	//create verts
	vertices = new float[nverts*3];
	for (int vi=0; vi<nverts; vi++)
	{
		MVert *v = &mvert[vi];
		for (int j=0; j<3; j++)
			vertices[3*vi+j] = v->co[j];
	}
	//create tris
	faces = new unsigned short[npolys*3*2];
	memset(faces,0xff,sizeof(unsigned short)*3*2*npolys);
	unsigned short *face = faces;
	for (int p2=0; p2<numpolys; p2++)
	{
		MFace* mf = &mface[p2];
		face[0]= mf->v1;
		face[1]= mf->v2;
		face[2]= mf->v3;
		face += 6;
		if (mf->v4)
		{
			face[0]= mf->v1;
			face[1]= mf->v3;
			face[2]= mf->v4;
			face += 6;
		}
	}

	dm->release(dm);
	dm = NULL;
	
	return true;
}

bool KX_Pathfinder::BuildNavMesh()
{
	if (GetMeshCount()==0)
		return false;

	RAS_MeshObject* meshobj = GetMesh(0);

	float* vertices = NULL;
	unsigned short* faces = NULL;
	int nverts = 0, npolys = 0;	
	if (!BuildVertIndArrays(meshobj, vertices, nverts, faces, npolys))
		return false;
	
	//prepare vertices and indices
	MT_Transform worldTransform = GetSGNode()->GetWorldTransform();
	MT_Point3 pos;
	for (int i=0; i<nverts; i++)
	{
		flipAxes(&vertices[i*3]);
		pos.setValue(&vertices[i*3]);
		//add world transform
		pos = worldTransform(pos);
		pos.getValue(&vertices[i*3]);

	}
	//reorder tris 
	for (int i=0; i<npolys; i++)
	{
		std::swap(faces[6*i+1], faces[6*i+2]);
	}
	const int vertsPerPoly = 3;
	buildMeshAdjacency(faces, npolys, nverts, vertsPerPoly);

	

	
	int ndtris = npolys;
	int uniqueDetailVerts = 0;
	float cs = 0.2f;

	if (!nverts || !npolys)
		return false;

	float bmin[3], bmax[3];
	calcMeshBounds(vertices, nverts, bmin, bmax);
	//quantize vertex pos
	unsigned short* vertsi = new unsigned short[3*nverts];
	float ics = 1.f/cs;
	for (int i=0; i<nverts; i++)
	{
		vertsi[3*i+0] = static_cast<unsigned short>((vertices[3*i+0]-bmin[0])*ics);
		vertsi[3*i+1] = static_cast<unsigned short>((vertices[3*i+1]-bmin[1])*ics);
		vertsi[3*i+2] = static_cast<unsigned short>((vertices[3*i+2]-bmin[2])*ics);
	}

	// Calculate data size
	const int headerSize = sizeof(dtStatNavMeshHeader);
	const int vertsSize = sizeof(float)*3*nverts;
	const int polysSize = sizeof(dtStatPoly)*npolys;
	const int nodesSize = sizeof(dtStatBVNode)*npolys*2;
	const int detailMeshesSize = sizeof(dtStatPolyDetail)*npolys;
	const int detailVertsSize = sizeof(float)*3*uniqueDetailVerts;
	const int detailTrisSize = sizeof(unsigned char)*4*ndtris;

	const int dataSize = headerSize + vertsSize + polysSize + nodesSize +
		detailMeshesSize + detailVertsSize + detailTrisSize;
	unsigned char* data = new unsigned char[dataSize];
	if (!data)
		return false;
	memset(data, 0, dataSize);

	unsigned char* d = data;
	dtStatNavMeshHeader* header = (dtStatNavMeshHeader*)d; d += headerSize;
	float* navVerts = (float*)d; d += vertsSize;
	dtStatPoly* navPolys = (dtStatPoly*)d; d += polysSize;
	dtStatBVNode* navNodes = (dtStatBVNode*)d; d += nodesSize;
	dtStatPolyDetail* navDMeshes = (dtStatPolyDetail*)d; d += detailMeshesSize;
	float* navDVerts = (float*)d; d += detailVertsSize;
	unsigned char* navDTris = (unsigned char*)d; d += detailTrisSize;

	// Store header
	header->magic = DT_STAT_NAVMESH_MAGIC;
	header->version = DT_STAT_NAVMESH_VERSION;
	header->npolys = npolys;
	header->nverts = nverts;
	header->cs = cs;
	header->bmin[0] = bmin[0];
	header->bmin[1] = bmin[1];
	header->bmin[2] = bmin[2];
	header->bmax[0] = bmax[0];
	header->bmax[1] = bmax[1];
	header->bmax[2] = bmax[2];
	header->ndmeshes = npolys;
	header->ndverts = uniqueDetailVerts;
	header->ndtris = ndtris;

	// Store vertices
	for (int i = 0; i < nverts; ++i)
	{
		const unsigned short* iv = &vertsi[i*3];
		float* v = &navVerts[i*3];
		v[0] = bmin[0] + iv[0] * cs;
		v[1] = bmin[1] + iv[1] * cs;
		v[2] = bmin[2] + iv[2] * cs;
	}
	//memcpy(navVerts, vertices, nverts*3*sizeof(float));

	// Store polygons
	const int nvp = 3;
	const unsigned short* src = faces;
	for (int i = 0; i < npolys; ++i)
	{
		dtStatPoly* p = &navPolys[i];
		p->nv = 0;
		for (int j = 0; j < nvp; ++j)
		{
			p->v[j] = src[j];
			p->n[j] = src[nvp+j]+1;
			p->nv++;
		}
		src += nvp*2;
	}

	header->nnodes = createBVTree(vertsi, nverts, faces, npolys, nvp,
								cs, cs, npolys*2, navNodes);
	
	//create fake detail meshes
	unsigned short vbase = 0;
	for (int i = 0; i < npolys; ++i)
	{
		dtStatPolyDetail& dtl = navDMeshes[i];
		dtl.vbase = 0;
		dtl.nverts = 0;
		dtl.tbase = i;
		dtl.ntris = 1;
	}
	// setup triangles.
	unsigned char* tri = navDTris;
	const unsigned short* face = faces;
	for(size_t i=0; i<ndtris; i++)
	{
		for (size_t j=0; j<3; j++)
			tri[4*i+j] = j;
	}

	m_navMesh = new dtStatNavMesh;
	m_navMesh->init(data, dataSize, true);

	delete [] vertices;
	delete [] faces;
	
	return true;
}

void KX_Pathfinder::DebugDraw()
{
	if (!m_navMesh)
		return;
	MT_Vector3 color(0.f, 0.f, 0.f);

	for (int i = 0; i < m_navMesh->getPolyDetailCount(); ++i)
	{
		const dtStatPoly* p = m_navMesh->getPoly(i);
		const dtStatPolyDetail* pd = m_navMesh->getPolyDetail(i);

		for (int j = 0; j < pd->ntris; ++j)
		{
			const unsigned char* t = m_navMesh->getDetailTri(pd->tbase+j);
			MT_Vector3 tri[3];
			for (int k = 0; k < 3; ++k)
			{
				const float* v;
				if (t[k] < p->nv)
					v = m_navMesh->getVertex(p->v[t[k]]);
				else
					v =  m_navMesh->getDetailVertex(pd->vbase+(t[k]-p->nv));
				float pos[3];
				vcopy(pos, v);
				flipAxes(pos);
				tri[k].setValue(pos);
			}

			for (int k=0; k<3; k++)
				KX_RasterizerDrawDebugLine(tri[k], tri[(k+1)%3], color);
		}
	}
}

int KX_Pathfinder::FindPath(MT_Vector3& from, MT_Vector3& to, float* path, int maxPathLen)
{
	if (!m_navMesh)
		return 0;
	float spos[3], epos[3];
	from.getValue(spos); flipAxes(spos);
	to.getValue(epos); flipAxes(epos);
	dtStatPolyRef sPolyRef = m_navMesh->findNearestPoly(spos, polyPickExt);
	dtStatPolyRef ePolyRef = m_navMesh->findNearestPoly(epos, polyPickExt);

	int pathLen = 0;
	if (sPolyRef && ePolyRef)
	{
		dtStatPolyRef* polys = new dtStatPolyRef[maxPathLen];
		int npolys;
		npolys = m_navMesh->findPath(sPolyRef, ePolyRef, spos, epos, polys, maxPathLen);
		if (npolys)
		{
			pathLen = m_navMesh->findStraightPath(spos, epos, polys, npolys, path, maxPathLen);
			for (int i=0; i<pathLen; i++)
				flipAxes(&path[i*3]);
		}
	}

	return pathLen;
}

float KX_Pathfinder::Raycast(MT_Vector3& from, MT_Vector3& to)
{
	if (!m_navMesh)
		return 0.f;
	float spos[3], epos[3];
	from.getValue(spos); flipAxes(spos);
	to.getValue(epos); flipAxes(epos);
	dtStatPolyRef sPolyRef = m_navMesh->findNearestPoly(spos, polyPickExt);
	float t=0;
	static dtStatPolyRef polys[MAX_PATH_LEN];
	m_navMesh->raycast(sPolyRef, spos, epos, t, polys, MAX_PATH_LEN);
	return t;
}

#ifndef DISABLE_PYTHON
//----------------------------------------------------------------------------
//Python

PyTypeObject KX_Pathfinder::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"KX_Pathfinder",
	sizeof(PyObjectPlus_Proxy),
	0,
	py_base_dealloc,
	0,
	0,
	0,
	0,
	py_base_repr,
	0,
	0,
	0,
	0,0,0,0,0,0,
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	0,0,0,0,0,0,0,
	Methods,
	0,
	0,
	&CValue::Type,
	0,0,0,0,0,0,
	py_base_new
};

PyAttributeDef KX_Pathfinder::Attributes[] = {
	{ NULL }	//Sentinel
};

//KX_PYMETHODTABLE_NOARGS(KX_GameObject, getD),
PyMethodDef KX_Pathfinder::Methods[] = {
	KX_PYMETHODTABLE(KX_Pathfinder, findPath),
	KX_PYMETHODTABLE(KX_Pathfinder, raycast),
	KX_PYMETHODTABLE(KX_Pathfinder, draw),
	{NULL,NULL} //Sentinel
};

KX_PYMETHODDEF_DOC(KX_Pathfinder, findPath,
				   "findPath(start, goal): find path from start to goal points\n"
				   "Returns a path as list of points)\n")
{
	PyObject *ob_from, *ob_to;
	if (!PyArg_ParseTuple(args,"OO:getPath",&ob_from,&ob_to))
		return NULL;
	MT_Vector3 from, to;
	if (!PyVecTo(ob_from, from) || !PyVecTo(ob_to, to))
		return NULL;
	
	float path[MAX_PATH_LEN*3];
	int pathLen = FindPath(from, to, path, MAX_PATH_LEN);
	PyObject *pathList = PyList_New( pathLen );
	for (int i=0; i<pathLen; i++)
	{
		MT_Vector3 point(&path[3*i]);
		PyList_SET_ITEM(pathList, i, PyObjectFrom(point));
	}

	return pathList;
}

KX_PYMETHODDEF_DOC(KX_Pathfinder, raycast,
				   "raycast(start, goal): raycast from start to goal points\n"
				   "Returns hit factor)\n")
{
	PyObject *ob_from, *ob_to;
	if (!PyArg_ParseTuple(args,"OO:getPath",&ob_from,&ob_to))
		return NULL;
	MT_Vector3 from, to;
	if (!PyVecTo(ob_from, from) || !PyVecTo(ob_to, to))
		return NULL;
	float hit = Raycast(from, to);
	return PyFloat_FromDouble(hit);
}

KX_PYMETHODDEF_DOC_NOARGS(KX_Pathfinder, draw,
				   "draw(): navigation mesh debug drawing\n")
{
	DebugDraw();
	Py_RETURN_NONE;
}

#endif // DISABLE_PYTHON