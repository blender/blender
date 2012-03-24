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

/** \file gameengine/Rasterizer/RAS_MeshObject.cpp
 *  \ingroup bgerast
 */

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"
#include "DNA_key_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "RAS_MeshObject.h"
#include "RAS_IRasterizer.h"
#include "MT_MinMax.h"
#include "MT_Point3.h"

#include <algorithm>

extern "C" {
#	include "BKE_deform.h"
}

/* polygon sorting */

struct RAS_MeshObject::polygonSlot
{
	float m_z;
	int m_index[4];
	
	polygonSlot() {}

	/* pnorm is the normal from the plane equation that the distance from is
	 * used to sort again. */
	void get(const RAS_TexVert *vertexarray, const unsigned short *indexarray,
		int offset, int nvert, const MT_Vector3& pnorm)
	{
		MT_Vector3 center(0, 0, 0);
		int i;

		for (i=0; i<nvert; i++) {
			m_index[i] = indexarray[offset+i];
			center += vertexarray[m_index[i]].getXYZ();
		}

		/* note we don't divide center by the number of vertices, since all
		 * polygons have the same number of vertices, and that we leave out
		 * the 4-th component of the plane equation since it is constant. */
		m_z = MT_dot(pnorm, center);
	}

	void set(unsigned short *indexarray, int offset, int nvert)
	{
		int i;

		for (i=0; i<nvert; i++)
			indexarray[offset+i] = m_index[i];
	}
};
	
struct RAS_MeshObject::backtofront
{
	bool operator()(const polygonSlot &a, const polygonSlot &b) const
	{
		return a.m_z < b.m_z;
	}
};

struct RAS_MeshObject::fronttoback
{
	bool operator()(const polygonSlot &a, const polygonSlot &b) const
	{
		return a.m_z > b.m_z;
	}
};

/* mesh object */

STR_String RAS_MeshObject::s_emptyname = "";

RAS_MeshObject::RAS_MeshObject(Mesh* mesh)
	: m_bModified(true),
	m_bMeshModified(true),
	m_mesh(mesh)
{
	if (m_mesh && m_mesh->key)
	{
		KeyBlock *kb;
		int count=0;
		// initialize weight cache for shape objects
		// count how many keys in this mesh
		for (kb= (KeyBlock*)m_mesh->key->block.first; kb; kb= (KeyBlock*)kb->next)
			count++;
		m_cacheWeightIndex.resize(count,-1);
	}
}

RAS_MeshObject::~RAS_MeshObject()
{
	vector<RAS_Polygon*>::iterator it;

	if (m_mesh && m_mesh->key) 
	{
		KeyBlock *kb;
		// remove the weight cache to avoid memory leak 
		for (kb= (KeyBlock*)m_mesh->key->block.first; kb; kb= (KeyBlock*)kb->next) {
			if (kb->weights) 
				MEM_freeN(kb->weights);
			kb->weights= NULL;
		}
	}

	for (it=m_Polygons.begin(); it!=m_Polygons.end(); it++)
		delete (*it);

	m_sharedvertex_map.clear();
	m_Polygons.clear();
	m_materials.clear();
}

bool RAS_MeshObject::MeshModified()
{
	return m_bMeshModified;
}

//unsigned int RAS_MeshObject::GetLightLayer()
//{
//	return m_lightlayer;
//}



int RAS_MeshObject::NumMaterials()
{
	return m_materials.size();
}

const STR_String& RAS_MeshObject::GetMaterialName(unsigned int matid)
{ 
	RAS_MeshMaterial* mmat = GetMeshMaterial(matid);

	if (mmat)
		return mmat->m_bucket->GetPolyMaterial()->GetMaterialName();
	
	return s_emptyname;
}

RAS_MeshMaterial* RAS_MeshObject::GetMeshMaterial(unsigned int matid)
{
	if (m_materials.size() > 0 && (matid < m_materials.size()))
	{
		list<RAS_MeshMaterial>::iterator it = m_materials.begin();
		while (matid--) ++it;
		return &*it;
	}

	return NULL;
}



int RAS_MeshObject::NumPolygons()
{
	return m_Polygons.size();
}



RAS_Polygon* RAS_MeshObject::GetPolygon(int num) const
{
	return m_Polygons[num];
}

	
	

	list<RAS_MeshMaterial>::iterator GetFirstMaterial();
	list<RAS_MeshMaterial>::iterator GetLastMaterial();
list<RAS_MeshMaterial>::iterator RAS_MeshObject::GetFirstMaterial()
{
	return m_materials.begin();
}



list<RAS_MeshMaterial>::iterator RAS_MeshObject::GetLastMaterial()
{
	return m_materials.end();
}



void RAS_MeshObject::SetName(const char *name)
{
	m_name = name;
}



STR_String& RAS_MeshObject::GetName()
{
	return m_name;
}



const STR_String& RAS_MeshObject::GetTextureName(unsigned int matid)
{ 
	RAS_MeshMaterial* mmat = GetMeshMaterial(matid);
	
	if (mmat)
		return mmat->m_bucket->GetPolyMaterial()->GetTextureName();

	return s_emptyname;
}

RAS_MeshMaterial *RAS_MeshObject::GetMeshMaterial(RAS_IPolyMaterial *mat)
{
	list<RAS_MeshMaterial>::iterator mit;

	/* find a mesh material */
	for (mit = m_materials.begin(); mit != m_materials.end(); mit++)
		if (mit->m_bucket->GetPolyMaterial() == mat)
			return &*mit;

	return NULL;
}

int RAS_MeshObject::GetMaterialId(RAS_IPolyMaterial *mat)
{
	list<RAS_MeshMaterial>::iterator mit;
	int imat;

	/* find a mesh material */
	for (imat=0, mit = m_materials.begin(); mit != m_materials.end(); mit++, imat++)
		if (mit->m_bucket->GetPolyMaterial() == mat)
			return imat;

	return -1;
}

RAS_Polygon* RAS_MeshObject::AddPolygon(RAS_MaterialBucket *bucket, int numverts)
{
	RAS_MeshMaterial *mmat;
	RAS_Polygon *poly;
	RAS_MeshSlot *slot;

	/* find a mesh material */
	mmat = GetMeshMaterial(bucket->GetPolyMaterial());

	/* none found, create a new one */
	if (!mmat) {
		RAS_MeshMaterial meshmat;
		meshmat.m_bucket = bucket;
		meshmat.m_baseslot = meshmat.m_bucket->AddMesh(numverts);
		meshmat.m_baseslot->m_mesh = this;
		m_materials.push_back(meshmat);
		mmat = &m_materials.back();
	}

	/* add it to the bucket, this also adds new display arrays */
	slot = mmat->m_baseslot;
	slot->AddPolygon(numverts);

	/* create a new polygon */
	RAS_DisplayArray *darray = slot->CurrentDisplayArray();
	poly = new RAS_Polygon(bucket, darray, numverts);
	m_Polygons.push_back(poly);

	return poly;
}

void RAS_MeshObject::DebugColor(unsigned int abgr)
{
	/*int numpolys = NumPolygons();

	for (int i=0;i<numpolys;i++) {
		RAS_Polygon* poly = m_polygons[i];
		for (int v=0;v<poly->VertexCount();v++)
			RAS_TexVert* vtx = poly->GetVertex(v)->setDebugRGBA(abgr);
	}
	*/

	/* m_debugcolor = abgr;	*/
}

void RAS_MeshObject::SetVertexColor(RAS_IPolyMaterial* mat,MT_Vector4 rgba)
{
	RAS_MeshMaterial *mmat = GetMeshMaterial(mat);
	RAS_MeshSlot *slot = mmat->m_baseslot;
	RAS_MeshSlot::iterator it;
	size_t i;

	for (slot->begin(it); !slot->end(it); slot->next(it))
		for (i=it.startvertex; i<it.endvertex; i++)
			it.vertex[i].SetRGBA(rgba);
}

void RAS_MeshObject::AddVertex(RAS_Polygon *poly, int i,
								const MT_Point3& xyz,
								const MT_Point2& uv,
								const MT_Point2& uv2,
								const MT_Vector4& tangent,
								const unsigned int rgba,
								const MT_Vector3& normal,
								bool flat,
								int origindex)
{
	RAS_TexVert texvert(xyz, uv, uv2, tangent, rgba, normal, flat, origindex);
	RAS_MeshMaterial *mmat;
	RAS_DisplayArray *darray;
	RAS_MeshSlot *slot;
	int offset;
	
	mmat = GetMeshMaterial(poly->GetMaterial()->GetPolyMaterial());
	slot = mmat->m_baseslot;
	darray = slot->CurrentDisplayArray();

	{ /* Shared Vertex! */
		/* find vertices shared between faces, with the restriction
		 * that they exist in the same display array, and have the
		 * same uv coordinate etc */
		vector<SharedVertex>& sharedmap = m_sharedvertex_map[origindex];
		vector<SharedVertex>::iterator it;

		for (it = sharedmap.begin(); it != sharedmap.end(); it++)
		{
			if (it->m_darray != darray)
				continue;
			if (!it->m_darray->m_vertex[it->m_offset].closeTo(&texvert))
				continue;

			/* found one, add it and we're done */
			if (poly->IsVisible())
				slot->AddPolygonVertex(it->m_offset);
			poly->SetVertexOffset(i, it->m_offset);
			return;
		}
	}

	/* no shared vertex found, add a new one */
	offset = slot->AddVertex(texvert);
	if (poly->IsVisible())
		slot->AddPolygonVertex(offset);
	poly->SetVertexOffset(i, offset);

	{ /* Shared Vertex! */
		SharedVertex shared;
		shared.m_darray = darray;
		shared.m_offset = offset;
		m_sharedvertex_map[origindex].push_back(shared);
	}
}

int RAS_MeshObject::NumVertices(RAS_IPolyMaterial* mat)
{
	RAS_MeshMaterial *mmat;
	RAS_MeshSlot *slot;
	RAS_MeshSlot::iterator it;
	size_t len = 0;

	mmat = GetMeshMaterial(mat);
	slot = mmat->m_baseslot;
	for (slot->begin(it); !slot->end(it); slot->next(it))
		len += it.endvertex - it.startvertex;
	
	return len;
}


RAS_TexVert* RAS_MeshObject::GetVertex(unsigned int matid,
									   unsigned int index)
{
	RAS_MeshMaterial *mmat;
	RAS_MeshSlot *slot;
	RAS_MeshSlot::iterator it;
	size_t len;

	mmat = GetMeshMaterial(matid);

	if (!mmat)
		return NULL;
	
	slot = mmat->m_baseslot;
	len = 0;
	for (slot->begin(it); !slot->end(it); slot->next(it)) {
		if (index >= len + it.endvertex - it.startvertex)
			len += it.endvertex - it.startvertex;
		else
			return &it.vertex[index - len];
	}
	
	return NULL;
}

const float* RAS_MeshObject::GetVertexLocation(unsigned int orig_index)
{
	vector<SharedVertex>& sharedmap = m_sharedvertex_map[orig_index];
	vector<SharedVertex>::iterator it= sharedmap.begin();
	return it->m_darray->m_vertex[it->m_offset].getXYZ();
}

void RAS_MeshObject::AddMeshUser(void *clientobj, SG_QList *head, RAS_Deformer* deformer)
{
	list<RAS_MeshMaterial>::iterator it;
	list<RAS_MeshMaterial>::iterator mit;

	for (it = m_materials.begin();it!=m_materials.end();++it) {
		/* always copy from the base slot, which is never removed 
		 * since new objects can be created with the same mesh data */
		if (deformer && !deformer->UseVertexArray())
		{
			// HACK! 
			// this deformer doesn't use vertex array => derive mesh
			// we must keep only the mesh slots that have unique material id
			// this is to match the derived mesh drawing function
			// Need a better solution in the future: scan the derive mesh and create vertex array
			RAS_IPolyMaterial* curmat = it->m_bucket->GetPolyMaterial();
			if (curmat->GetFlag() & RAS_BLENDERGLSL) 
			{
				for (mit = m_materials.begin(); mit != it; ++mit)
				{
					RAS_IPolyMaterial* mat = mit->m_bucket->GetPolyMaterial();
					if ((mat->GetFlag() & RAS_BLENDERGLSL) && 
						mat->GetMaterialIndex() == curmat->GetMaterialIndex())
						// no need to convert current mesh slot
						break;
				}
				if (mit != it)
					continue;
			}
		}
		RAS_MeshSlot *ms = it->m_bucket->CopyMesh(it->m_baseslot);
		ms->m_clientObj = clientobj;
		ms->SetDeformer(deformer);
		it->m_slots.insert(clientobj, ms);
		head->QAddBack(ms);
	}
}

void RAS_MeshObject::RemoveFromBuckets(void *clientobj)
{
	list<RAS_MeshMaterial>::iterator it;
	
	for (it = m_materials.begin();it!=m_materials.end();++it) {
		RAS_MeshSlot **msp = it->m_slots[clientobj];

		if (!msp)
			continue;

		RAS_MeshSlot *ms = *msp;

		it->m_bucket->RemoveMesh(ms);
		it->m_slots.remove(clientobj);
	}
}

//void RAS_MeshObject::Transform(const MT_Transform& trans)
//{
	//m_trans.translate(MT_Vector3(0,0,1));//.operator *=(trans);
	
//	for (int i=0;i<m_Polygons.size();i++)
//	{
//		m_Polygons[i]->Transform(trans);
//	}
//}


/*
void RAS_MeshObject::RelativeTransform(const MT_Vector3& vec)
{
	for (int i=0;i<m_Polygons.size();i++)
	{
		m_Polygons[i]->RelativeTransform(vec);
	}
}
*/

void RAS_MeshObject::SortPolygons(RAS_MeshSlot& ms, const MT_Transform &transform)
{
	// Limitations: sorting is quite simple, and handles many
	// cases wrong, partially due to polygons being sorted per
	// bucket.
	// 
	// a) mixed triangles/quads are sorted wrong
	// b) mixed materials are sorted wrong
	// c) more than 65k faces are sorted wrong
	// d) intersecting objects are sorted wrong
	// e) intersecting polygons are sorted wrong
	//
	// a) can be solved by making all faces either triangles or quads
	// if they need to be z-sorted. c) could be solved by allowing
	// larger buckets, b) and d) cannot be solved easily if we want
	// to avoid excessive state changes while drawing. e) would
	// require splitting polygons.

	RAS_MeshSlot::iterator it;
	size_t j;

	for (ms.begin(it); !ms.end(it); ms.next(it)) {
		unsigned int nvert = (int)it.array->m_type;
		unsigned int totpoly = it.totindex/nvert;

		if (totpoly <= 1)
			continue;
		if (it.array->m_type == RAS_DisplayArray::LINE)
			continue;

		// Extract camera Z plane...
		const MT_Vector3 pnorm(transform.getBasis()[2]);
		// unneeded: const MT_Scalar pval = transform.getOrigin()[2];

		vector<polygonSlot> slots(totpoly);

		/* get indices and z into temporary array */
		for (j=0; j<totpoly; j++)
			slots[j].get(it.vertex, it.index, j*nvert, nvert, pnorm);

		/* sort (stable_sort might be better, if flickering happens?) */
		std::sort(slots.begin(), slots.end(), backtofront());

		/* get indices from temporary array again */
		for (j=0; j<totpoly; j++)
			slots[j].set(it.index, j*nvert, nvert);
	}
}


void RAS_MeshObject::SchedulePolygons(int drawingmode)
{
	if (m_bModified)
	{
		m_bModified = false;
		m_bMeshModified = true;
	} 
}

static int get_def_index(Object* ob, const char* vgroup)
{
	bDeformGroup *curdef;
	int index = 0;

	for (curdef = (bDeformGroup*)ob->defbase.first; curdef; curdef=(bDeformGroup*)curdef->next, index++)
		if (!strcmp(curdef->name, vgroup))
			return index;

	return -1;
}

void RAS_MeshObject::CheckWeightCache(Object* obj)
{
	KeyBlock *kb;
	int kbindex, defindex;
	MDeformVert *dv= NULL;
	int totvert, i;
	float *weights;

	if (!m_mesh->key)
		return;

	for (kbindex=0, kb= (KeyBlock*)m_mesh->key->block.first; kb; kb= (KeyBlock*)kb->next, kbindex++)
	{
		// first check the cases where the weight must be cleared
		if (kb->vgroup[0] == 0 ||
			m_mesh->dvert == NULL ||
			(defindex = get_def_index(obj, kb->vgroup)) == -1) {
			if (kb->weights) {
				MEM_freeN(kb->weights);
				kb->weights = NULL;
			}
			m_cacheWeightIndex[kbindex] = -1;
		} else if (m_cacheWeightIndex[kbindex] != defindex) {
			// a weight array is required but the cache is not matching
			if (kb->weights) {
				MEM_freeN(kb->weights);
				kb->weights = NULL;
			}

			dv= m_mesh->dvert;
			totvert= m_mesh->totvert;
		
			weights= (float*)MEM_mallocN(totvert*sizeof(float), "weights");
		
			for (i=0; i < totvert; i++, dv++) {
				weights[i]= defvert_find_weight(dv, defindex);
			}

			kb->weights = weights;
			m_cacheWeightIndex[kbindex] = defindex;
		}
	}
}


