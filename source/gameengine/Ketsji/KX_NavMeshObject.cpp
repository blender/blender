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

#include "MEM_guardedalloc.h"

#include "BLI_math_vector.h"
#include "KX_NavMeshObject.h"
#include "RAS_MeshObject.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

extern "C" {
#include "BKE_scene.h"
#include "BKE_customdata.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_DerivedMesh.h"
#include "BKE_navmesh_conversion.h"
}

#include "KX_PythonInit.h"
#include "KX_PyMath.h"
#include "Value.h"
#include "Recast.h"
#include "DetourStatNavMeshBuilder.h"
#include "KX_ObstacleSimulation.h"

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
KX_NavMeshObject::KX_NavMeshObject(void* sgReplicationInfo, SG_Callbacks callbacks)
:	KX_GameObject(sgReplicationInfo, callbacks)
,	m_navMesh(NULL)
{
	
}

KX_NavMeshObject::~KX_NavMeshObject()
{
	if (m_navMesh)
		delete m_navMesh;
}

CValue* KX_NavMeshObject::GetReplica()
{
	KX_NavMeshObject* replica = new KX_NavMeshObject(*this);
	replica->ProcessReplica();
	return replica;
}

void KX_NavMeshObject::ProcessReplica()
{
	KX_GameObject::ProcessReplica();

	BuildNavMesh();
	KX_Scene* scene = KX_GetActiveScene();
	KX_ObstacleSimulation* obssimulation = scene->GetObstacleSimulation();
	if (obssimulation)
		obssimulation->AddObstaclesForNavMesh(this);

}

bool KX_NavMeshObject::BuildVertIndArrays(float *&vertices, int& nverts,
									   unsigned short* &polys, int& npolys, unsigned short *&dmeshes,
									   float *&dvertices, int &ndvertsuniq, unsigned short *&dtris, 
									   int& ndtris, int &vertsPerPoly)
{
	DerivedMesh* dm = mesh_create_derived_no_virtual(KX_GetActiveScene()->GetBlenderScene(), GetBlenderObject(), 
													NULL, CD_MASK_MESH);
	int* recastData = (int*) dm->getTessFaceDataArray(dm, CD_RECAST);
	if (recastData)
	{
		int *dtrisToPolysMap=NULL, *dtrisToTrisMap=NULL, *trisToFacesMap=NULL;
		int nAllVerts = 0;
		float *allVerts = NULL;
		buildNavMeshDataByDerivedMesh(dm, &vertsPerPoly, &nAllVerts, &allVerts, &ndtris, &dtris,
			&npolys, &dmeshes, &polys, &dtrisToPolysMap, &dtrisToTrisMap, &trisToFacesMap);

		MEM_freeN(dtrisToPolysMap);
		MEM_freeN(dtrisToTrisMap);
		MEM_freeN(trisToFacesMap);

		unsigned short *verticesMap = new unsigned short[nAllVerts];
		memset(verticesMap, 0xffff, sizeof(unsigned short)*nAllVerts);
		int curIdx = 0;
		//vertices - mesh verts
		//iterate over all polys and create map for their vertices first...
		for (int polyidx=0; polyidx<npolys; polyidx++)
		{
			unsigned short* poly = &polys[polyidx*vertsPerPoly*2];
			for (int i=0; i<vertsPerPoly; i++)
			{
				unsigned short idx = poly[i];
				if (idx==0xffff)
					break;
				if (verticesMap[idx]==0xffff)
				{
					verticesMap[idx] = curIdx++;
				}
				poly[i] = verticesMap[idx];
			}
		}
		nverts = curIdx;
		//...then iterate over detailed meshes
		//transform indices to local ones (for each navigation polygon)
		for (int polyidx=0; polyidx<npolys; polyidx++)
		{
			unsigned short *poly = &polys[polyidx*vertsPerPoly*2];
			int nv = polyNumVerts(poly, vertsPerPoly);
			unsigned short *dmesh = &dmeshes[4*polyidx];
			unsigned short tribase = dmesh[2];
			unsigned short trinum = dmesh[3];
			unsigned short vbase = curIdx;
			for (int j=0; j<trinum; j++)
			{
				unsigned short* dtri = &dtris[(tribase+j)*3*2];
				for (int k=0; k<3; k++)
				{
					int newVertexIdx = verticesMap[dtri[k]];
					if (newVertexIdx==0xffff)
					{
						newVertexIdx = curIdx++;
						verticesMap[dtri[k]] = newVertexIdx;
					}

					if (newVertexIdx<nverts)
					{
						//it's polygon vertex ("shared")
						int idxInPoly = polyFindVertex(poly, vertsPerPoly, newVertexIdx);
						if (idxInPoly==-1)
						{
							printf("Building NavMeshObject: Error! Can't find vertex in polygon\n");
							return false;
						}
						dtri[k] = idxInPoly;
					}
					else
					{
						dtri[k] = newVertexIdx - vbase + nv;
					}
				}
			}
			dmesh[0] = vbase-nverts; //verts base
			dmesh[1] = curIdx-vbase; //verts num
		}

		vertices = new float[nverts*3];
		ndvertsuniq = curIdx - nverts;
		if (ndvertsuniq>0)
		{
			dvertices = new float[ndvertsuniq*3];
		}
		for (int vi=0; vi<nAllVerts; vi++)
		{
			int newIdx = verticesMap[vi];
			if (newIdx!=0xffff)
			{
				if (newIdx<nverts)
				{
					//navigation mesh vertex
					memcpy(vertices+3*newIdx, allVerts+3*vi, 3*sizeof(float));
				}
				else
				{
					//detailed mesh vertex
					memcpy(dvertices+3*(newIdx-nverts), allVerts+3*vi, 3*sizeof(float));
				}				
			}
		}

		MEM_freeN(allVerts);
	}
	else
	{
		//create from RAS_MeshObject (detailed mesh is fake)
		RAS_MeshObject* meshobj = GetMesh(0);
		vertsPerPoly = 3;
		nverts = meshobj->m_sharedvertex_map.size();
		if (nverts >= 0xffff)
			return false;
		//calculate count of tris
		int nmeshpolys = meshobj->NumPolygons();
		npolys = nmeshpolys;
		for (int p=0; p<nmeshpolys; p++)
		{
			int vertcount = meshobj->GetPolygon(p)->VertexCount();
			npolys+=vertcount-3;
		}

		//create verts
		vertices = new float[nverts*3];
		float* vert = vertices;
		for (int vi=0; vi<nverts; vi++)
		{
			const float* pos = !meshobj->m_sharedvertex_map[vi].empty() ? meshobj->GetVertexLocation(vi) : NULL;
			if (pos)
				copy_v3_v3(vert, pos);
			else
			{
				memset(vert, 0, 3*sizeof(float)); //vertex isn't in any poly, set dummy zero coordinates
			}
			vert+=3;		
		}

		//create tris
		polys = (unsigned short*)MEM_callocN(sizeof(unsigned short)*3*2*npolys, "BuildVertIndArrays polys");
		memset(polys, 0xff, sizeof(unsigned short)*3*2*npolys);
		unsigned short *poly = polys;
		RAS_Polygon* raspoly;
		for (int p=0; p<nmeshpolys; p++)
		{
			raspoly = meshobj->GetPolygon(p);
			for (int v=0; v<raspoly->VertexCount()-2; v++)
			{
				poly[0]= raspoly->GetVertex(0)->getOrigIndex();
				for (size_t i=1; i<3; i++)
				{
					poly[i]= raspoly->GetVertex(v+i)->getOrigIndex();
				}
				poly += 6;
			}
		}
		dmeshes = NULL;
		dvertices = NULL;
		ndvertsuniq = 0;
		dtris = NULL;
		ndtris = npolys;
	}
	dm->release(dm);
	
	return true;
}


bool KX_NavMeshObject::BuildNavMesh()
{
	if (m_navMesh)
	{
		delete m_navMesh;
		m_navMesh = NULL;
	}

	if (GetMeshCount()==0)
	{
		printf("Can't find mesh for navmesh object: %s \n", m_name.ReadPtr());
		return false;
	}

	float *vertices = NULL, *dvertices = NULL;
	unsigned short *polys = NULL, *dtris = NULL, *dmeshes = NULL;
	int nverts = 0, npolys = 0, ndvertsuniq = 0, ndtris = 0;	
	int vertsPerPoly = 0;
	if (!BuildVertIndArrays(vertices, nverts, polys, npolys, 
							dmeshes, dvertices, ndvertsuniq, dtris, ndtris, vertsPerPoly ) 
			|| vertsPerPoly<3)
	{
		printf("Can't build navigation mesh data for object:%s \n", m_name.ReadPtr());
		return false;
	}
	
	MT_Point3 pos;
	if (dmeshes==NULL)
	{
		for (int i=0; i<nverts; i++)
		{
			flipAxes(&vertices[i*3]);
		}
		for (int i=0; i<ndvertsuniq; i++)
		{
			flipAxes(&dvertices[i*3]);
		}
	}

	buildMeshAdjacency(polys, npolys, nverts, vertsPerPoly);
	
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
	const int detailVertsSize = sizeof(float)*3*ndvertsuniq;
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
	header->ndverts = ndvertsuniq;
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
	const unsigned short* src = polys;
	for (int i = 0; i < npolys; ++i)
	{
		dtStatPoly* p = &navPolys[i];
		p->nv = 0;
		for (int j = 0; j < vertsPerPoly; ++j)
		{
			if (src[j] == 0xffff) break;
			p->v[j] = src[j];
			p->n[j] = src[vertsPerPoly+j]+1;
			p->nv++;
		}
		src += vertsPerPoly*2;
	}

	header->nnodes = createBVTree(vertsi, nverts, polys, npolys, vertsPerPoly,
								cs, cs, npolys*2, navNodes);
	
	
	if (dmeshes==NULL)
	{
		//create fake detail meshes
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
		for(size_t i=0; i<ndtris; i++)
		{
			for (size_t j=0; j<3; j++)
				tri[4*i+j] = j;
		}
	}
	else
	{
		//verts
		memcpy(navDVerts, dvertices, ndvertsuniq*3*sizeof(float));
		//tris
		unsigned char* tri = navDTris;
		for(size_t i=0; i<ndtris; i++)
		{
			for (size_t j=0; j<3; j++)
				tri[4*i+j] = dtris[6*i+j];
		}
		//detailed meshes
		for (int i = 0; i < npolys; ++i)
		{
			dtStatPolyDetail& dtl = navDMeshes[i];
			dtl.vbase = dmeshes[i*4+0];
			dtl.nverts = dmeshes[i*4+1];
			dtl.tbase = dmeshes[i*4+2];
			dtl.ntris = dmeshes[i*4+3];
		}		
	}

	m_navMesh = new dtStatNavMesh;
	m_navMesh->init(data, dataSize, true);	

	delete [] vertices;

	/* navmesh conversion is using C guarded alloc for memory allocaitons */
	MEM_freeN(polys);
	if (dmeshes) MEM_freeN(dmeshes);
	if (dtris) MEM_freeN(dtris);

	if (dvertices)
	{
		delete [] dvertices;
	}

	return true;
}

dtStatNavMesh* KX_NavMeshObject::GetNavMesh()
{
	return m_navMesh;
}

void KX_NavMeshObject::DrawNavMesh(NavMeshRenderMode renderMode)
{
	if (!m_navMesh)
		return;
	MT_Vector3 color(0.f, 0.f, 0.f);
	
	switch (renderMode)
	{
	case RM_POLYS :
	case RM_WALLS : 
		for (int pi=0; pi<m_navMesh->getPolyCount(); pi++)
		{
			const dtStatPoly* poly = m_navMesh->getPoly(pi);

			for (int i = 0, j = (int)poly->nv-1; i < (int)poly->nv; j = i++)
			{	
				if (poly->n[j] && renderMode==RM_WALLS) 
					continue;
				const float* vif = m_navMesh->getVertex(poly->v[i]);
				const float* vjf = m_navMesh->getVertex(poly->v[j]);
				MT_Point3 vi(vif[0], vif[2], vif[1]);
				MT_Point3 vj(vjf[0], vjf[2], vjf[1]);
				vi = TransformToWorldCoords(vi);
				vj = TransformToWorldCoords(vj);
				KX_RasterizerDrawDebugLine(vi, vj, color);
			}
		}
		break;
	case RM_TRIS : 
		for (int i = 0; i < m_navMesh->getPolyDetailCount(); ++i)
		{
			const dtStatPoly* p = m_navMesh->getPoly(i);
			const dtStatPolyDetail* pd = m_navMesh->getPolyDetail(i);

			for (int j = 0; j < pd->ntris; ++j)
			{
				const unsigned char* t = m_navMesh->getDetailTri(pd->tbase+j);
				MT_Point3 tri[3];
				for (int k = 0; k < 3; ++k)
				{
					const float* v;
					if (t[k] < p->nv)
						v = m_navMesh->getVertex(p->v[t[k]]);
					else
						v =  m_navMesh->getDetailVertex(pd->vbase+(t[k]-p->nv));
					float pos[3];
					rcVcopy(pos, v);
					flipAxes(pos);
					tri[k].setValue(pos);
				}

				for (int k=0; k<3; k++)
					tri[k] = TransformToWorldCoords(tri[k]);

				for (int k=0; k<3; k++)
					KX_RasterizerDrawDebugLine(tri[k], tri[(k+1)%3], color);
			}
		}
		break;
	default:
		/* pass */
		break;
	}
}

MT_Point3 KX_NavMeshObject::TransformToLocalCoords(const MT_Point3& wpos)
{
	MT_Matrix3x3 orientation = NodeGetWorldOrientation();
	const MT_Vector3& scaling = NodeGetWorldScaling();
	orientation.scale(scaling[0], scaling[1], scaling[2]);
	MT_Transform worldtr(NodeGetWorldPosition(), orientation); 
	MT_Transform invworldtr;
	invworldtr.invert(worldtr);
	MT_Point3 lpos = invworldtr(wpos);
	return lpos;
}

MT_Point3 KX_NavMeshObject::TransformToWorldCoords(const MT_Point3& lpos)
{
	MT_Matrix3x3 orientation = NodeGetWorldOrientation();
	const MT_Vector3& scaling = NodeGetWorldScaling();
	orientation.scale(scaling[0], scaling[1], scaling[2]);
	MT_Transform worldtr(NodeGetWorldPosition(), orientation); 
	MT_Point3 wpos = worldtr(lpos);
	return wpos;
}

int KX_NavMeshObject::FindPath(const MT_Point3& from, const MT_Point3& to, float* path, int maxPathLen)
{
	if (!m_navMesh)
		return 0;
	MT_Point3 localfrom = TransformToLocalCoords(from);
	MT_Point3 localto = TransformToLocalCoords(to);	
	float spos[3], epos[3];
	localfrom.getValue(spos); flipAxes(spos);
	localto.getValue(epos); flipAxes(epos);
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
			{
				flipAxes(&path[i*3]);
				MT_Point3 waypoint(&path[i*3]);
				waypoint = TransformToWorldCoords(waypoint);
				waypoint.getValue(&path[i*3]);
			}
		}
	}

	return pathLen;
}

float KX_NavMeshObject::Raycast(const MT_Point3& from, const MT_Point3& to)
{
	if (!m_navMesh)
		return 0.f;
	MT_Point3 localfrom = TransformToLocalCoords(from);
	MT_Point3 localto = TransformToLocalCoords(to);	
	float spos[3], epos[3];
	localfrom.getValue(spos); flipAxes(spos);
	localto.getValue(epos); flipAxes(epos);
	dtStatPolyRef sPolyRef = m_navMesh->findNearestPoly(spos, polyPickExt);
	float t=0;
	static dtStatPolyRef polys[MAX_PATH_LEN];
	m_navMesh->raycast(sPolyRef, spos, epos, t, polys, MAX_PATH_LEN);
	return t;
}

void KX_NavMeshObject::DrawPath(const float *path, int pathLen, const MT_Vector3& color)
{
	MT_Vector3 a,b;
	for (int i=0; i<pathLen-1; i++)
	{
		a.setValue(&path[3*i]);
		b.setValue(&path[3*(i+1)]);
		KX_RasterizerDrawDebugLine(a, b, color);
	}
}


#ifdef WITH_PYTHON
//----------------------------------------------------------------------------
//Python

PyTypeObject KX_NavMeshObject::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"KX_NavMeshObject",
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
	&KX_GameObject::Type,
	0,0,0,0,0,0,
	py_base_new
};

PyAttributeDef KX_NavMeshObject::Attributes[] = {
	{ NULL }	//Sentinel
};

//KX_PYMETHODTABLE_NOARGS(KX_GameObject, getD),
PyMethodDef KX_NavMeshObject::Methods[] = {
	KX_PYMETHODTABLE(KX_NavMeshObject, findPath),
	KX_PYMETHODTABLE(KX_NavMeshObject, raycast),
	KX_PYMETHODTABLE(KX_NavMeshObject, draw),
	KX_PYMETHODTABLE(KX_NavMeshObject, rebuild),
	{NULL,NULL} //Sentinel
};

KX_PYMETHODDEF_DOC(KX_NavMeshObject, findPath,
				   "findPath(start, goal): find path from start to goal points\n"
				   "Returns a path as list of points)\n")
{
	PyObject *ob_from, *ob_to;
	if (!PyArg_ParseTuple(args,"OO:getPath",&ob_from,&ob_to))
		return NULL;
	MT_Point3 from, to;
	if (!PyVecTo(ob_from, from) || !PyVecTo(ob_to, to))
		return NULL;
	
	float path[MAX_PATH_LEN*3];
	int pathLen = FindPath(from, to, path, MAX_PATH_LEN);
	PyObject *pathList = PyList_New( pathLen );
	for (int i=0; i<pathLen; i++)
	{
		MT_Point3 point(&path[3*i]);
		PyList_SET_ITEM(pathList, i, PyObjectFrom(point));
	}

	return pathList;
}

KX_PYMETHODDEF_DOC(KX_NavMeshObject, raycast,
				   "raycast(start, goal): raycast from start to goal points\n"
				   "Returns hit factor)\n")
{
	PyObject *ob_from, *ob_to;
	if (!PyArg_ParseTuple(args,"OO:getPath",&ob_from,&ob_to))
		return NULL;
	MT_Point3 from, to;
	if (!PyVecTo(ob_from, from) || !PyVecTo(ob_to, to))
		return NULL;
	float hit = Raycast(from, to);
	return PyFloat_FromDouble(hit);
}

KX_PYMETHODDEF_DOC(KX_NavMeshObject, draw,
				   "draw(mode): navigation mesh debug drawing\n"
				   "mode: WALLS, POLYS, TRIS\n")
{
	int arg;
	NavMeshRenderMode renderMode = RM_TRIS;
	if (PyArg_ParseTuple(args,"i:rebuild",&arg) && arg>=0 && arg<RM_MAX)
		renderMode = (NavMeshRenderMode)arg;
	DrawNavMesh(renderMode);
	Py_RETURN_NONE;
}

KX_PYMETHODDEF_DOC_NOARGS(KX_NavMeshObject, rebuild,
						  "rebuild(): rebuild navigation mesh\n")
{
	BuildNavMesh();
	Py_RETURN_NONE;
}

#endif // WITH_PYTHON
