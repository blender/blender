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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

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



KX_VertexIndex::KX_VertexIndex(int size)
{
	m_size = size;
}



void KX_VertexIndex::SetIndex(short loc,unsigned int index)
{
	m_indexarray[loc]=index;
}

bool KX_MeshSlot::Less(const KX_MeshSlot& lhs) const
{
	bool result = ((m_mesh < lhs.m_mesh ) ||
		((m_mesh == lhs.m_mesh)&&(m_OpenGLMatrix < lhs.m_OpenGLMatrix)));
	
	return result;
}

KX_MeshSlot::~KX_MeshSlot()
{
	if (m_DisplayList)
		m_DisplayList->Release();
}


RAS_MaterialBucket::RAS_MaterialBucket(RAS_IPolyMaterial* mat)
	:m_bModified(true)
{
	m_bScheduled=true;
	m_material = mat;
}



void RAS_MaterialBucket::SchedulePolygons(int drawingmode)
{ 
	m_bScheduled = true;
}



void RAS_MaterialBucket::ClearScheduledPolygons()
{ 
	m_bScheduled = false;
}

	

RAS_IPolyMaterial* RAS_MaterialBucket::GetPolyMaterial() const
{ 
	return m_material;
}


	
void RAS_MaterialBucket::SetMeshSlot(KX_MeshSlot& ms)
{
	m_meshSlots.insert(ms);
}



void RAS_MaterialBucket::RemoveMeshSlot(KX_MeshSlot& ms)
{
	T_MeshSlotList::iterator it = m_meshSlots.find(ms);

	if (!(it == m_meshSlots.end()))
		m_meshSlots.erase(it);
			
}



void RAS_MaterialBucket::MarkVisibleMeshSlot(KX_MeshSlot& ms,
											 bool visible,
											 bool color,
											 const MT_Vector4& rgbavec)
{
	T_MeshSlotList::iterator it = m_meshSlots.find(ms);
			
	assert (!(it == m_meshSlots.end())); 
	(*it).m_bVisible = visible;
	(*it).m_bObjectColor = color;
	(*it).m_RGBAcolor= rgbavec;
}

bool RAS_MaterialBucket::IsTransparant() const
{	
	return (m_material->IsTransparant());
}



void RAS_MaterialBucket::StartFrame()
{
}



void RAS_MaterialBucket::EndFrame()
{
}

unsigned int RAS_MaterialBucket::NumMeshSlots()
{
	return m_meshSlots.size();
}

RAS_MaterialBucket::T_MeshSlotList::iterator RAS_MaterialBucket::msBegin()
{
	return m_meshSlots.begin();
}

RAS_MaterialBucket::T_MeshSlotList::iterator RAS_MaterialBucket::msEnd()
{
	return m_meshSlots.end();
}

bool RAS_MaterialBucket::ActivateMaterial(const MT_Transform& cameratrans, RAS_IRasterizer* rasty,
	RAS_IRenderTools *rendertools, RAS_IRasterizer::DrawMode &drawmode)
{
	rendertools->SetViewMat(cameratrans);

	if (!rasty->SetMaterial(*m_material))
		return false;
	
	if (m_material->UsesLighting(rasty))
		rendertools->ProcessLighting(RAS_IRenderTools::RAS_LIGHT_OBJECT_LAYER/*m_material->GetLightLayer()*/);
	else
		rendertools->ProcessLighting(-1);

	if(rasty->GetDrawingMode() < RAS_IRasterizer::KX_SOLID)
		drawmode = RAS_IRasterizer::KX_MODE_LINES;
	else if(m_material->UsesTriangles())
		drawmode = RAS_IRasterizer::KX_MODE_TRIANGLES;
	else
		drawmode = RAS_IRasterizer::KX_MODE_QUADS;
	
	return true;
}

void RAS_MaterialBucket::RenderMeshSlot(const MT_Transform& cameratrans, RAS_IRasterizer* rasty,
	RAS_IRenderTools* rendertools, const KX_MeshSlot &ms, RAS_IRasterizer::DrawMode drawmode)
{
	if (!ms.m_bVisible)
		return;
	
	m_material->ActivateMeshSlot(ms, rasty);

	/* __NLA Do the deformation */
	if (ms.m_pDeformer)
	{
		ms.m_pDeformer->Apply(m_material);
	//	KX_ReInstanceShapeFromMesh(ms.m_mesh); // Recompute the physics mesh. (Can't call KX_* from RAS_)
	}
	/* End __NLA */
	
	if (rasty->GetDrawingMode() >= RAS_IRasterizer::KX_SOLID)
		ms.m_mesh->SortPolygons(cameratrans*MT_Transform(ms.m_OpenGLMatrix));

	rendertools->PushMatrix();
	rendertools->applyTransform(rasty,ms.m_OpenGLMatrix,m_material->GetDrawingMode());

	if(rasty->QueryLists())
	{
		if(ms.m_DisplayList)
			ms.m_DisplayList->SetModified(ms.m_mesh->MeshModified());
	}

	// verify if we can use display list, not for deformed object, and
	// also don't create a new display list when drawing shadow buffers,
	// then it won't have texture coordinates for actual drawing
	KX_ListSlot **displaylist;
	if(ms.m_pDeformer)
		displaylist = 0;
	else if(!ms.m_DisplayList && rasty->GetDrawingMode() == RAS_IRasterizer::KX_SHADOW)
		displaylist = 0;
	else
		displaylist = &ms.m_DisplayList;

	// Use the text-specific IndexPrimitives for text faces
	if (m_material->GetDrawingMode() & RAS_IRasterizer::RAS_RENDER_3DPOLYGON_TEXT)
	{
		rasty->IndexPrimitives_3DText(
				ms.m_mesh->GetVertexCache(m_material), 
				ms.m_mesh->GetIndexCache(m_material), 
				drawmode,
				m_material,
				rendertools, // needed for textprinting on polys
				ms.m_bObjectColor,
				ms.m_RGBAcolor);
	}

	// for using glMultiTexCoord
	else if((m_material->GetFlag() & RAS_MULTITEX))
	{
		rasty->IndexPrimitivesMulti(
				ms.m_mesh->GetVertexCache(m_material), 
				ms.m_mesh->GetIndexCache(m_material), 
				drawmode,
				ms.m_bObjectColor,
				ms.m_RGBAcolor,
				displaylist);
	}

	// Use the normal IndexPrimitives
	else
	{
		rasty->IndexPrimitives(
				ms.m_mesh->GetVertexCache(m_material), 
				ms.m_mesh->GetIndexCache(m_material), 
				drawmode,
				ms.m_bObjectColor,
				ms.m_RGBAcolor,
				displaylist);
	}

	if(rasty->QueryLists()) {
		if(ms.m_DisplayList)
			ms.m_mesh->SetMeshModified(false);
	}

	rendertools->PopMatrix();
}

void RAS_MaterialBucket::Render(const MT_Transform& cameratrans,
								RAS_IRasterizer* rasty,
								RAS_IRenderTools* rendertools)
{
	if (m_meshSlots.begin()== m_meshSlots.end())
		return;
		
	//rendertools->SetViewMat(cameratrans);

	//rasty->SetMaterial(*m_material);
	
	RAS_IRasterizer::DrawMode drawmode;
	for (T_MeshSlotList::const_iterator it = m_meshSlots.begin();
	! (it == m_meshSlots.end()); ++it)
	{
		rendertools->SetClientObject((*it).m_clientObj);
		while (ActivateMaterial(cameratrans, rasty, rendertools, drawmode)) {
			RenderMeshSlot(cameratrans, rasty, rendertools, *it, drawmode);
		}
	}
	// to reset the eventual GL_CW mode
	rendertools->SetClientObject(NULL);
}


