/**
* $Id: 
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
#include "Recast.h"
#include "DetourStatNavMeshBuilder.h"

static void calcMeshBounds(const float* vert, int nverts, float* bmin, float* bmax)
{
	bmin[0] = bmax[0] = vert[0];
	bmin[1] = bmax[1] = vert[1];
	bmin[2] = bmax[2] = vert[2];
	for (int i=1; i<nverts; i++)
	{
		if (bmin[0]>vert[i+0]) bmin[0] = vert[i+0];
		if (bmin[1]>vert[i+1]) bmin[1] = vert[i+1];
		if (bmin[2]>vert[i+2]) bmin[2] = vert[i+2];

		if (bmax[0]<vert[i+0]) bmax[0] = vert[i+0];
		if (bmax[1]<vert[i+1]) bmax[2] = vert[i+1];
		if (bmax[2]<vert[i+2]) bmax[1] = vert[i+2];
	}
}

KX_Pathfinder::KX_Pathfinder()
:	m_navMesh(NULL)
{
	
}

KX_Pathfinder::~KX_Pathfinder()
{
	if (m_navMesh)
		delete m_navMesh;
}

bool KX_Pathfinder::buildVertIndArrays(RAS_MeshObject* meshobj, float *&vertices, int& nverts,
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

	vector<bool> vert_tag_array(numverts, false);
	vector<size_t> vert_remap_array(numverts, 0);

	// Tag verts we're using
	for (int p2=0; p2<numpolys; p2++)
	{
		MFace* mf = &mface[p2];
		RAS_Polygon* poly= meshobj->GetPolygon((index)? index[p2]: p2);
		// only add polygons that have the collision flag set
		if (poly->IsCollider())
		{
			if (vert_tag_array[mf->v1]==false)
			{vert_tag_array[mf->v1]= true;vert_remap_array[mf->v1]= (size_t)nverts;nverts++;}
			if (vert_tag_array[mf->v2]==false)
			{vert_tag_array[mf->v2]= true;vert_remap_array[mf->v2]= (size_t)nverts;nverts++;}
			if (vert_tag_array[mf->v3]==false)
			{vert_tag_array[mf->v3]= true;vert_remap_array[mf->v3]= (size_t)nverts;nverts++;}
			if (mf->v4 && vert_tag_array[mf->v4]==false)
			{vert_tag_array[mf->v4]= true;vert_remap_array[mf->v4]= (size_t)nverts;nverts++;}
			npolys += (mf->v4 ? 2:1); /* a quad or a tri */
		}
	}

	if (nverts >= 0xffff)
		return false;

	vertices = new float[nverts*3];
	faces = new unsigned short[npolys*3*2];
	memset(faces,0xff,sizeof(unsigned short)*3*2*npolys);
	float *bt= vertices;
	unsigned short *tri_pt= faces;

	for (int p2=0; p2<numpolys; p2++)
	{
		MFace* mf = &mface[p2];
		MTFace* tf = (tface) ? &tface[p2] : NULL;
		RAS_Polygon* poly= meshobj->GetPolygon((index)? index[p2]: p2);
		// only add polygons that have the collisionflag set
		if (poly->IsCollider())
		{
			MVert *v1= &mvert[mf->v1];
			MVert *v2= &mvert[mf->v2];
			MVert *v3= &mvert[mf->v3];

			// the face indicies
			tri_pt[0]= vert_remap_array[mf->v1];
			tri_pt[1]= vert_remap_array[mf->v2];
			tri_pt[2]= vert_remap_array[mf->v3];
			tri_pt= tri_pt+6;

			// the vertex location
			if (vert_tag_array[mf->v1]==true) { /* *** v1 *** */
				vert_tag_array[mf->v1]= false;
				*bt++ = v1->co[0];
				*bt++ = v1->co[1];
				*bt++ = v1->co[2];
			}
			if (vert_tag_array[mf->v2]==true) { /* *** v2 *** */
				vert_tag_array[mf->v2]= false;
				*bt++ = v2->co[0];
				*bt++ = v2->co[1];
				*bt++ = v2->co[2];
			}
			if (vert_tag_array[mf->v3]==true) { /* *** v3 *** */
				vert_tag_array[mf->v3]= false;
				*bt++ = v3->co[0];	
				*bt++ = v3->co[1];
				*bt++ = v3->co[2];
			}

			if (mf->v4)
			{
				MVert *v4= &mvert[mf->v4];

				tri_pt[0]= vert_remap_array[mf->v1];
				tri_pt[1]= vert_remap_array[mf->v3];
				tri_pt[2]= vert_remap_array[mf->v4];
				tri_pt= tri_pt+3;

				// the vertex location
				if (vert_tag_array[mf->v4]==true) { /* *** v4 *** */
					vert_tag_array[mf->v4]= false;
					*bt++ = v4->co[0];
					*bt++ = v4->co[1];	
					*bt++ = v4->co[2];
				}
			}
		}
	}

	dm->release(dm);
	dm = NULL;
	const int vertsPerPoly = 3;
	buildMeshAdjacency(faces, npolys, nverts, vertsPerPoly);
	return true;
}

bool KX_Pathfinder::createFromMesh(RAS_MeshObject* meshobj)
{
	float* vertices = NULL;
	unsigned short* faces = NULL;
	int nverts = 0, npolys = 0;	
	if (!buildVertIndArrays(meshobj, vertices, nverts, faces, npolys))
		return false;
	
	
	int ndtris = npolys;
	int uniqueDetailVerts = 0;
	float cs = 0.2f;

	if (!nverts || !npolys)
		return false;

	float bmin[3], bmax[3];
	calcMeshBounds(vertices, nverts, bmin, bmax);

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

	memcpy(navVerts, vertices, nverts*3*sizeof(float));

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

	//quantize vertex pos to creating BVTree 
	unsigned short* vertsi = new unsigned short[3*nverts];
	float* vf = vertices;
	unsigned short* vi = vertsi;
	float ics = 1.f/cs;
	for (int i=0; i<nverts*3; i++)
	{
		vi[i] = static_cast<unsigned short>(vf[i]*ics);
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

void KX_Pathfinder::debugDraw()
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
				if (t[k] < p->nv)
					tri[k].setValue(m_navMesh->getVertex(p->v[t[k]]));
				else
					tri[k].setValue(m_navMesh->getDetailVertex(pd->vbase+(t[k]-p->nv)));
			}

			for (int k=0; k<3; k++)
				KX_RasterizerDrawDebugLine(tri[k], tri[(k+1)%3], color);
		}
	}
}
