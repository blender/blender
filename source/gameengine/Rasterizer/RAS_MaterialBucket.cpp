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

#include "RAS_MaterialBucket.h"

#ifdef WIN32
#pragma warning (disable:4786)
#include <windows.h>
#endif // WIN32

#include "RAS_Polygon.h"
#include "RAS_TexVert.h"
#include "RAS_IRasterizer.h"
#include "RAS_IRenderTools.h"
#include "RAS_MeshObject.h"
#include "RAS_Deformer.h"	// __NLA

/* mesh slot */

RAS_MeshSlot::RAS_MeshSlot() : SG_QList()
{
	m_clientObj = NULL;
	m_pDeformer = NULL;
	m_OpenGLMatrix = NULL;
	m_mesh = NULL;
	m_bucket = NULL;
	m_bVisible = false;
	m_bCulled = true;
	m_bObjectColor = false;
	m_RGBAcolor = MT_Vector4(0.0, 0.0, 0.0, 0.0);
	m_DisplayList = NULL;
	m_bDisplayList = true;
	m_joinSlot = NULL;
	m_pDerivedMesh = NULL;
}

RAS_MeshSlot::~RAS_MeshSlot()
{
	RAS_DisplayArrayList::iterator it;

#ifdef USE_SPLIT
	Split(true);

	while(m_joinedSlots.size())
		m_joinedSlots.front()->Split(true);
#endif

	for(it=m_displayArrays.begin(); it!=m_displayArrays.end(); it++) {
		(*it)->m_users--;
		if((*it)->m_users == 0)
			delete *it;
	}

	if (m_DisplayList) {
		m_DisplayList->Release();
		m_DisplayList = NULL;
	}
}

RAS_MeshSlot::RAS_MeshSlot(const RAS_MeshSlot& slot) : SG_QList()
{
	RAS_DisplayArrayList::iterator it;

	m_clientObj = NULL;
	m_pDeformer = NULL;
	m_pDerivedMesh = NULL;
	m_OpenGLMatrix = NULL;
	m_mesh = slot.m_mesh;
	m_bucket = slot.m_bucket;
	m_bVisible = slot.m_bVisible;
	m_bCulled = slot.m_bCulled;
	m_bObjectColor = slot.m_bObjectColor;
	m_RGBAcolor = slot.m_RGBAcolor;
	m_DisplayList = NULL;
	m_bDisplayList = slot.m_bDisplayList;
	m_joinSlot = NULL;
	m_currentArray = slot.m_currentArray;
	m_displayArrays = slot.m_displayArrays;
	m_joinedSlots = slot.m_joinedSlots;

	m_startarray = slot.m_startarray;
	m_startvertex = slot.m_startvertex;
	m_startindex = slot.m_startindex;
	m_endarray = slot.m_endarray;
	m_endvertex = slot.m_endvertex;
	m_endindex = slot.m_endindex;

	for(it=m_displayArrays.begin(); it!=m_displayArrays.end(); it++) {
		// don't copy display arrays for now because it breaks python 
		// access to vertices, but we'll need a solution if we want to
		// join display arrays for reducing draw calls.
		//*it = new RAS_DisplayArray(**it);
		//(*it)->m_users = 1;

		(*it)->m_users++;
	}
}

void RAS_MeshSlot::init(RAS_MaterialBucket *bucket, int numverts)
{
	m_bucket = bucket;

	SetDisplayArray(numverts);

	m_startarray = 0;
	m_startvertex = 0;
	m_startindex = 0;
	m_endarray = 0;
	m_endvertex = 0;
	m_endindex = 0;
}

void RAS_MeshSlot::begin(RAS_MeshSlot::iterator& it)
{
	int startvertex, endvertex;
	int startindex, endindex;

	it.array = (m_displayArrays.size() > 0)? m_displayArrays[m_startarray]: NULL;

	if(it.array == NULL || it.array->m_index.size() == 0 || it.array->m_vertex.size() == 0) {
		it.array = NULL;
		it.vertex = NULL;
		it.index = NULL;
		it.startvertex = 0;
		it.endvertex = 0;
		it.totindex = 0;
	}
	else {
		startvertex = m_startvertex;
		endvertex = (m_startarray == m_endarray)? m_endvertex: it.array->m_vertex.size();
		startindex = m_startindex;
		endindex = (m_startarray == m_endarray)? m_endindex: it.array->m_index.size();

		it.vertex = &it.array->m_vertex[0];
		it.index = &it.array->m_index[startindex];
		it.startvertex = startvertex;
		it.endvertex = endvertex;
		it.totindex = endindex-startindex;
		it.arraynum = m_startarray;
	}
}

void RAS_MeshSlot::next(RAS_MeshSlot::iterator& it)
{
	int startvertex, endvertex;
	int startindex, endindex;

	if(it.arraynum == (size_t)m_endarray) {
		it.array = NULL;
		it.vertex = NULL;
		it.index = NULL;
		it.startvertex = 0;
		it.endvertex = 0;
		it.totindex = 0;
	}
	else {
		it.arraynum++;
		it.array = m_displayArrays[it.arraynum];

		startindex = 0;
		endindex = (it.arraynum == (size_t)m_endarray)? m_endindex: it.array->m_index.size();
		startvertex = 0;
		endvertex = (it.arraynum == (size_t)m_endarray)? m_endvertex: it.array->m_vertex.size();

		it.vertex = &it.array->m_vertex[0];
		it.index = &it.array->m_index[startindex];
		it.startvertex = startvertex;
		it.endvertex = endvertex;
		it.totindex = endindex-startindex;
	}
}

bool RAS_MeshSlot::end(RAS_MeshSlot::iterator& it)
{
	return (it.array == NULL);
}

RAS_DisplayArray *RAS_MeshSlot::CurrentDisplayArray()
{
	return m_currentArray;
}

void RAS_MeshSlot::SetDisplayArray(int numverts)
{
	RAS_DisplayArrayList::iterator it;
	RAS_DisplayArray *darray = NULL;
	
	for(it=m_displayArrays.begin(); it!=m_displayArrays.end(); it++) {
		darray = *it;

		if(darray->m_type == numverts) {
			if(darray->m_index.size()+numverts >= RAS_DisplayArray::BUCKET_MAX_INDEX)
				darray = NULL;
			else if(darray->m_vertex.size()+numverts >= RAS_DisplayArray::BUCKET_MAX_VERTEX)
				darray = NULL;
			else
				break;
		}
		else
			darray = NULL;
	}

	if(!darray) {
		darray = new RAS_DisplayArray();
		darray->m_users = 1;

		if(numverts == 2) darray->m_type = RAS_DisplayArray::LINE;
		else if(numverts == 3) darray->m_type = RAS_DisplayArray::TRIANGLE;
		else darray->m_type = RAS_DisplayArray::QUAD;

		m_displayArrays.push_back(darray);

		if(numverts == 2)
			darray->m_type = RAS_DisplayArray::LINE;
		else if(numverts == 3)
			darray->m_type = RAS_DisplayArray::TRIANGLE;
		else if(numverts == 4)
			darray->m_type = RAS_DisplayArray::QUAD;
		
		m_endarray = m_displayArrays.size()-1;
		m_endvertex = 0;
		m_endindex = 0;
	}

	m_currentArray = darray;
}

void RAS_MeshSlot::AddPolygon(int numverts)
{
	SetDisplayArray(numverts);
}

int RAS_MeshSlot::AddVertex(const RAS_TexVert& tv)
{
	RAS_DisplayArray *darray;
	int offset;
	
	darray = m_currentArray;
	darray->m_vertex.push_back(tv);
	offset = darray->m_vertex.size()-1;

	if(darray == m_displayArrays[m_endarray])
		m_endvertex++;

	return offset;
}

void RAS_MeshSlot::AddPolygonVertex(int offset)
{
	RAS_DisplayArray *darray;

	darray = m_currentArray;
	darray->m_index.push_back(offset);

	if(darray == m_displayArrays[m_endarray])
		m_endindex++;
}

void RAS_MeshSlot::SetDeformer(RAS_Deformer* deformer)
{
	if (deformer && m_pDeformer != deformer) {
		RAS_DisplayArrayList::iterator it;
		if (deformer->ShareVertexArray()) {
			// this deformer uses the base vertex array, first release the current ones
			for(it=m_displayArrays.begin(); it!=m_displayArrays.end(); it++) {
				(*it)->m_users--;
				if((*it)->m_users == 0)
					delete *it;
			}
			m_displayArrays.clear();
			// then hook to the base ones
			RAS_MeshMaterial *mmat = m_mesh->GetMeshMaterial(m_bucket->GetPolyMaterial());
			if (mmat && mmat->m_baseslot) {
				m_displayArrays = mmat->m_baseslot->m_displayArrays;
				for(it=m_displayArrays.begin(); it!=m_displayArrays.end(); it++) {
					(*it)->m_users++;
				}
			}
		}
		else {
			// no sharing
			// we create local copy of RAS_DisplayArray when we have a deformer:
			// this way we can avoid conflict between the vertex cache of duplicates
			for(it=m_displayArrays.begin(); it!=m_displayArrays.end(); it++) {
				if (deformer->UseVertexArray()) {
					// the deformer makes use of vertex array, make sure we have our local copy
					if ((*it)->m_users > 1) {
						// only need to copy if there are other users
						// note that this is the usual case as vertex arrays are held by the material base slot
						RAS_DisplayArray *newarray = new RAS_DisplayArray(*(*it));
						newarray->m_users = 1;
						(*it)->m_users--;
						*it = newarray;
					}
				} else {
					// the deformer is not using vertex array (Modifier), release them
					(*it)->m_users--;
					if((*it)->m_users == 0)
						delete *it;
				}
			}
			if (!deformer->UseVertexArray()) {
				m_displayArrays.clear();
				m_startarray = 0;
				m_startvertex = 0;
				m_startindex = 0;
				m_endarray = 0;
				m_endvertex = 0;
				m_endindex = 0;
			}
		}
	}
	m_pDeformer = deformer;
}

bool RAS_MeshSlot::Equals(RAS_MeshSlot *target)
{
	if(!m_OpenGLMatrix || !target->m_OpenGLMatrix)
		return false;
	if(m_pDeformer || target->m_pDeformer)
		return false;
	if(m_bVisible != target->m_bVisible)
		return false;
	if(m_bObjectColor != target->m_bObjectColor)
		return false;
	if(m_bObjectColor && !(m_RGBAcolor == target->m_RGBAcolor))
		return false;
	
	return true;
}

bool RAS_MeshSlot::Join(RAS_MeshSlot *target, MT_Scalar distance)
{
	RAS_DisplayArrayList::iterator it;
	iterator mit;
	size_t i;

	// verify if we can join
	if(m_joinSlot || m_joinedSlots.size() || target->m_joinSlot)
		return false;

	if(!Equals(target))
		return false;
	
	MT_Vector3 co(&m_OpenGLMatrix[12]);
	MT_Vector3 targetco(&target->m_OpenGLMatrix[12]);

	if((co - targetco).length() > distance)
		return false;

	MT_Matrix4x4 mat(m_OpenGLMatrix);
	MT_Matrix4x4 targetmat(target->m_OpenGLMatrix);
	targetmat.invert();

	MT_Matrix4x4 transform = targetmat*mat;
	
	// m_mesh, clientobj
	m_joinSlot = target;
	m_joinInvTransform = transform;
	m_joinInvTransform.invert();
	target->m_joinedSlots.push_back(this);

	MT_Matrix4x4 ntransform = m_joinInvTransform.transposed();
	ntransform[0][3]= ntransform[1][3]= ntransform[2][3]= 0.0f;

	for(begin(mit); !end(mit); next(mit))
		for(i=mit.startvertex; i<mit.endvertex; i++)
			mit.vertex[i].Transform(transform, ntransform);
	
	/* We know we'll need a list at least this big, reserve in advance */
	target->m_displayArrays.reserve(target->m_displayArrays.size() + m_displayArrays.size());

	for(it=m_displayArrays.begin(); it!=m_displayArrays.end(); it++) {
		target->m_displayArrays.push_back(*it);
		target->m_endarray++;
		target->m_endvertex = target->m_displayArrays.back()->m_vertex.size();
		target->m_endindex = target->m_displayArrays.back()->m_index.size();
	}

	if (m_DisplayList) {
		m_DisplayList->Release();
		m_DisplayList = NULL;
	}
	if (target->m_DisplayList) {
		target->m_DisplayList->Release();
		target->m_DisplayList = NULL;
	}
	
	return true;
#if 0
	return false;
#endif
}

bool RAS_MeshSlot::Split(bool force)
{
	list<RAS_MeshSlot*>::iterator jit;
	RAS_MeshSlot *target = m_joinSlot;
	RAS_DisplayArrayList::iterator it, jt;
	iterator mit;
	size_t i, found0 = 0, found1 = 0;

	if(target && (force || !Equals(target))) {
		m_joinSlot = NULL;

		for(jit=target->m_joinedSlots.begin(); jit!=target->m_joinedSlots.end(); jit++) {
			if(*jit == this) {
				target->m_joinedSlots.erase(jit);
				found0 = 1;
				break;
			}
		}

		if(!found0)
			abort();

		for(it=m_displayArrays.begin(); it!=m_displayArrays.end(); it++) {
			found1 = 0;
			for(jt=target->m_displayArrays.begin(); jt!=target->m_displayArrays.end(); jt++) {
				if(*jt == *it) {
					target->m_displayArrays.erase(jt);
					target->m_endarray--;
					found1 = 1;
					break;
				}
			}

			if(!found1)
				abort();
		}

		if(target->m_displayArrays.size()) {
			target->m_endvertex = target->m_displayArrays.back()->m_vertex.size();
			target->m_endindex = target->m_displayArrays.back()->m_index.size();
		}
		else {
			target->m_endvertex = 0;
			target->m_endindex = 0;
		}

		MT_Matrix4x4 ntransform = m_joinInvTransform.inverse().transposed();
		ntransform[0][3]= ntransform[1][3]= ntransform[2][3]= 0.0f;

		for(begin(mit); !end(mit); next(mit))
			for(i=mit.startvertex; i<mit.endvertex; i++)
				mit.vertex[i].Transform(m_joinInvTransform, ntransform);

		if (target->m_DisplayList) {
			target->m_DisplayList->Release();
			target->m_DisplayList = NULL;
		}

		return true;
	}

	return false;
}


#ifdef USE_SPLIT	
bool RAS_MeshSlot::IsCulled()
{
	if(m_joinSlot)
		return true;
	if(!m_bCulled)
		return false;
	list<RAS_MeshSlot*>::iterator it;
	for(it=m_joinedSlots.begin(); it!=m_joinedSlots.end(); it++)
		if(!(*it)->m_bCulled)
			return false;
	return true;
}
#endif	

/* material bucket sorting */

struct RAS_MaterialBucket::less
{
	bool operator()(const RAS_MaterialBucket* x, const RAS_MaterialBucket* y) const 
	{ 
		return *x->GetPolyMaterial() < *y->GetPolyMaterial(); 
	}
};

/* material bucket */

RAS_MaterialBucket::RAS_MaterialBucket(RAS_IPolyMaterial* mat)
{
	m_material = mat;
}

RAS_MaterialBucket::~RAS_MaterialBucket()
{
}

RAS_IPolyMaterial* RAS_MaterialBucket::GetPolyMaterial() const
{ 
	return m_material;
}

bool RAS_MaterialBucket::IsAlpha() const
{	
	return (m_material->IsAlpha());
}

bool RAS_MaterialBucket::IsZSort() const
{	
	return (m_material->IsZSort());
}

RAS_MeshSlot* RAS_MaterialBucket::AddMesh(int numverts)
{
	RAS_MeshSlot *ms;

	m_meshSlots.push_back(RAS_MeshSlot());
	
	ms = &m_meshSlots.back();
	ms->init(this, numverts);

	return ms;
}

RAS_MeshSlot* RAS_MaterialBucket::CopyMesh(RAS_MeshSlot *ms)
{
	m_meshSlots.push_back(RAS_MeshSlot(*ms));
	
	return &m_meshSlots.back();
}

void RAS_MaterialBucket::RemoveMesh(RAS_MeshSlot* ms)
{
	list<RAS_MeshSlot>::iterator it;

	for(it=m_meshSlots.begin(); it!=m_meshSlots.end(); it++) {
		if(&*it == ms) {
			m_meshSlots.erase(it);
			return;
		}
	}
}

list<RAS_MeshSlot>::iterator RAS_MaterialBucket::msBegin()
{
	return m_meshSlots.begin();
}

list<RAS_MeshSlot>::iterator RAS_MaterialBucket::msEnd()
{
	return m_meshSlots.end();
}

bool RAS_MaterialBucket::ActivateMaterial(const MT_Transform& cameratrans, RAS_IRasterizer* rasty,
	RAS_IRenderTools *rendertools)
{
	bool uselights;

	if(!rasty->SetMaterial(*m_material))
		return false;
	
	uselights= m_material->UsesLighting(rasty);
	rendertools->ProcessLighting(rasty, uselights, cameratrans);
	
	return true;
}

void RAS_MaterialBucket::RenderMeshSlot(const MT_Transform& cameratrans, RAS_IRasterizer* rasty,
	RAS_IRenderTools* rendertools, RAS_MeshSlot &ms)
{
	m_material->ActivateMeshSlot(ms, rasty);

	if (ms.m_pDeformer)
	{
		ms.m_pDeformer->Apply(m_material);
	//	KX_ReInstanceShapeFromMesh(ms.m_mesh); // Recompute the physics mesh. (Can't call KX_* from RAS_)
	}
	
	if(IsZSort() && rasty->GetDrawingMode() >= RAS_IRasterizer::KX_SOLID)
		ms.m_mesh->SortPolygons(ms, cameratrans*MT_Transform(ms.m_OpenGLMatrix));

	rendertools->PushMatrix();
	if (!ms.m_pDeformer || !ms.m_pDeformer->SkipVertexTransform())
	{
		rendertools->applyTransform(rasty,ms.m_OpenGLMatrix,m_material->GetDrawingMode());
	}

	if(rasty->QueryLists())
		if(ms.m_DisplayList)
			ms.m_DisplayList->SetModified(ms.m_mesh->MeshModified());

	// verify if we can use display list, not for deformed object, and
	// also don't create a new display list when drawing shadow buffers,
	// then it won't have texture coordinates for actual drawing. also
	// for zsort we can't make a display list, since the polygon order
	// changes all the time.
	if(ms.m_pDeformer && ms.m_pDeformer->IsDynamic())
		ms.m_bDisplayList = false;
	else if(!ms.m_DisplayList && rasty->GetDrawingMode() == RAS_IRasterizer::KX_SHADOW)
		ms.m_bDisplayList = false;
	else if (IsZSort())
		ms.m_bDisplayList = false;
	else if(m_material->UsesObjectColor() && ms.m_bObjectColor)
		ms.m_bDisplayList = false;
	else
		ms.m_bDisplayList = true;

	// for text drawing using faces
	if (m_material->GetDrawingMode() & RAS_IRasterizer::RAS_RENDER_3DPOLYGON_TEXT)
		rasty->IndexPrimitives_3DText(ms, m_material, rendertools);
	// for multitexturing
	else if((m_material->GetFlag() & (RAS_MULTITEX|RAS_BLENDERGLSL)))
		rasty->IndexPrimitivesMulti(ms);
	// use normal IndexPrimitives
	else
		rasty->IndexPrimitives(ms);

	if(rasty->QueryLists())
		if(ms.m_DisplayList)
			ms.m_mesh->SetMeshModified(false);

	rendertools->PopMatrix();
}

void RAS_MaterialBucket::Optimize(MT_Scalar distance)
{
	/* TODO: still have to check before this works correct:
	 * - lightlayer, frontface, text, billboard
	 * - make it work with physics */
	
#if 0
	list<RAS_MeshSlot>::iterator it;
	list<RAS_MeshSlot>::iterator jt;

	// greed joining on all following buckets
	for(it=m_meshSlots.begin(); it!=m_meshSlots.end(); it++)
		for(jt=it, jt++; jt!=m_meshSlots.end(); jt++)
			jt->Join(&*it, distance);
#endif
}

