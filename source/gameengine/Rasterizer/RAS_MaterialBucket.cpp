/**
 * $Id$
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
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



void KX_VertexIndex::SetIndex(short loc,short index)
{
	m_indexarray[loc]=index;
}




bool KX_MeshSlot::Less(const KX_MeshSlot& lhs) const
{
	bool result = ((m_mesh < lhs.m_mesh ) ||
		((m_mesh == lhs.m_mesh)&&(m_OpenGLMatrix < lhs.m_OpenGLMatrix)));
	
	return result;
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

	

RAS_IPolyMaterial* RAS_MaterialBucket::GetPolyMaterial()
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



bool RAS_MaterialBucket::IsTransparant()
{	
	return (m_material->IsTransparant());
}



void RAS_MaterialBucket::StartFrame()
{
}



void RAS_MaterialBucket::EndFrame()
{
}



void RAS_MaterialBucket::Render(const MT_Transform& cameratrans,
								RAS_IRasterizer* rasty,
								RAS_IRenderTools* rendertools)
{

	if (m_meshSlots.begin()== m_meshSlots.end())
		return;
		
	rendertools->SetViewMat(cameratrans);

	rasty->SetMaterial(*m_material);
	
	if (m_meshSlots.size() >0)
	{
		
		rendertools->SetClientObject((*m_meshSlots.begin()).m_clientObj);
	}
	
	//printf("RAS_MatBucket::Render: %d m_meshSlots\n", m_meshSlots.size());

	bool dolights = m_material->GetDrawingMode()&16;

	if ((rasty->GetDrawingMode() <= RAS_IRasterizer::KX_SOLID) || !dolights)
	{
		bool bUseLights = rendertools->ProcessLighting(-1);
	}
	else
	{
		bool bUseLights = rendertools->ProcessLighting(m_material->GetLightLayer());
	}

	int drawmode = (rasty->GetDrawingMode()  < RAS_IRasterizer::KX_SOLID ? 	
		1:	(m_material->UsesTriangles() ? 0 : 2));

	for (T_MeshSlotList::const_iterator it = m_meshSlots.begin();
	! (it == m_meshSlots.end());it++)
	{
		//printf("RAS_MatBucket::Render: (%p) %s MeshSlot %s\n", this, it->m_mesh->m_class?"Skin":"Mesh", (const char *)(*it).m_mesh->GetName());
		if ((*it).m_bVisible)
		{
			rendertools->SetClientObject((*it).m_clientObj);
			
			/* __NLA Do the deformation */
			if ((*it).m_pDeformer)
				(*it).m_pDeformer->Apply(m_material);
			/* End __NLA */

			rendertools->PushMatrix();
			rendertools->applyTransform(rasty,(*it).m_OpenGLMatrix,m_material->GetDrawingMode());

			// Use the text-specific IndexPrimitives for text faces
			if (m_material->GetDrawingMode() & RAS_IRasterizer::RAS_RENDER_3DPOLYGON_TEXT)
			{
				rasty->IndexPrimitives_3DText(
						(*it).m_mesh->GetVertexCache(m_material), 
						(*it).m_mesh->GetIndexCache(m_material), 
						drawmode,
						m_material,
						rendertools, // needed for textprinting on polys
						(*it).m_bObjectColor,
						(*it).m_RGBAcolor);

			}
			// Use the (slower) IndexPrimitives_Ex which can recalc face normals & such
			//	for deformed objects - eventually should be extended to recalc ALL normals
			else if ((*it).m_pDeformer){
				rasty->IndexPrimitives_Ex(
						(*it).m_mesh->GetVertexCache(m_material), 
						(*it).m_mesh->GetIndexCache(m_material), 
						drawmode,
						m_material,
						rendertools, // needed for textprinting on polys
						(*it).m_bObjectColor,
						(*it).m_RGBAcolor
						);
			}
			// Use the normal IndexPrimitives
			else
			{
				rasty->IndexPrimitives(
						(*it).m_mesh->GetVertexCache(m_material), 
						(*it).m_mesh->GetIndexCache(m_material), 
						drawmode,
						m_material,
						rendertools, // needed for textprinting on polys
						(*it).m_bObjectColor,
						(*it).m_RGBAcolor
						);
			}
			
			rendertools->PopMatrix();
		}
		//printf("gets here\n");
	}
}


