/**
 * $Id$
 *
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
 * Simple deformation controller that restores a mesh to its rest position
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
// This warning tells us about truncation of __long__ stl-generated names.
// It can occasionally cause DevStudio to have internal compiler warnings.
#pragma warning( disable : 4786 )     
#endif

#include "RAS_IPolygonMaterial.h"
#include "BL_DeformableGameObject.h"
#include "BL_MeshDeformer.h"
#include "BL_SkinMeshObject.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "GEN_Map.h"
#include "STR_HashedString.h"

bool BL_MeshDeformer::Apply(RAS_IPolyMaterial*)
{
	size_t i, j;
	float *co;

	// only apply once per frame if the mesh is actually modified
	if(m_pMeshObject->MeshModified() &&
	   m_lastDeformUpdate != m_gameobj->GetLastFrame()) {
		// For each material
		for(RAS_MaterialBucket::Set::iterator mit = m_pMeshObject->GetFirstMaterial();
			mit != m_pMeshObject->GetLastMaterial(); ++ mit) {
			RAS_IPolyMaterial *mat = (*mit)->GetPolyMaterial();

			vecVertexArray& vertexarrays = m_pMeshObject->GetVertexCache(mat);

			// For each array
			for (i=0; i<vertexarrays.size(); i++){
				KX_VertexArray& vertexarray = (*vertexarrays[i]);

				//	For each vertex
				for (j=0; j<vertexarray.size(); j++){
					RAS_TexVert& v = vertexarray[j];
					co = m_bmesh->mvert[v.getOrigIndex()].co;
					v.SetXYZ(MT_Point3(co));
				}
			}
		}

		m_lastDeformUpdate = m_gameobj->GetLastFrame();

		return true;
	}

	return false;
}

BL_MeshDeformer::~BL_MeshDeformer()
{	
	if (m_transverts)
		delete [] m_transverts;
	if (m_transnors)
		delete [] m_transnors;
};

/**
 * @warning This function is expensive!
 */
void BL_MeshDeformer::RecalcNormals()
{
	/* We don't normalize for performance, not doing it for faces normals
	 * gives area-weight normals which often look better anyway, and use
	 * GL_NORMALIZE so we don't have to do per vertex normalization either
	 * since the GPU can do it faster */
	size_t i, j;

	/* set vertex normals to zero */
	memset(m_transnors, 0, sizeof(float)*3*m_bmesh->totvert);

	/* add face normals to vertices. */
	for(RAS_MaterialBucket::Set::iterator mit = m_pMeshObject->GetFirstMaterial();
		mit != m_pMeshObject->GetLastMaterial(); ++ mit) {
		RAS_IPolyMaterial *mat = (*mit)->GetPolyMaterial();

		const vecIndexArrays& indexarrays = m_pMeshObject->GetIndexCache(mat);
		vecVertexArray& vertexarrays = m_pMeshObject->GetVertexCache(mat);

		for (i=0; i<indexarrays.size(); i++) {
			KX_VertexArray& vertexarray = (*vertexarrays[i]);
			const KX_IndexArray& indexarray = (*indexarrays[i]);
			int nvert = mat->UsesTriangles()? 3: 4;

			for(j=0; j<indexarray.size(); j+=nvert) {
				RAS_TexVert& v1 = vertexarray[indexarray[j]];
				RAS_TexVert& v2 = vertexarray[indexarray[j+1]];
				RAS_TexVert& v3 = vertexarray[indexarray[j+2]];
				RAS_TexVert *v4 = NULL;

				const float *co1 = v1.getLocalXYZ();
				const float *co2 = v2.getLocalXYZ();
				const float *co3 = v3.getLocalXYZ();
				const float *co4 = NULL;
				
				/* compute face normal */
				float fnor[3], n1[3], n2[3];

				if(nvert == 4) {
					v4 = &vertexarray[indexarray[j+3]];
					co4 = v4->getLocalXYZ();

					n1[0]= co1[0]-co3[0];
					n1[1]= co1[1]-co3[1];
					n1[2]= co1[2]-co3[2];

					n2[0]= co2[0]-co4[0];
					n2[1]= co2[1]-co4[1];
					n2[2]= co2[2]-co4[2];
				}
				else {
					n1[0]= co1[0]-co2[0];
					n2[0]= co2[0]-co3[0];
					n1[1]= co1[1]-co2[1];

					n2[1]= co2[1]-co3[1];
					n1[2]= co1[2]-co2[2];
					n2[2]= co2[2]-co3[2];
				}

				fnor[0]= n1[1]*n2[2] - n1[2]*n2[1];
				fnor[1]= n1[2]*n2[0] - n1[0]*n2[2];
				fnor[2]= n1[0]*n2[1] - n1[1]*n2[0];

				/* add to vertices for smooth normals */
				float *vn1 = m_transnors[v1.getOrigIndex()];
				float *vn2 = m_transnors[v2.getOrigIndex()];
				float *vn3 = m_transnors[v3.getOrigIndex()];

				vn1[0] += fnor[0]; vn1[1] += fnor[1]; vn1[2] += fnor[2];
				vn2[0] += fnor[0]; vn2[1] += fnor[1]; vn2[2] += fnor[2];
				vn3[0] += fnor[0]; vn3[1] += fnor[1]; vn3[2] += fnor[2];

				if(v4) {
					float *vn4 = m_transnors[v4->getOrigIndex()];
					vn4[0] += fnor[0]; vn4[1] += fnor[1]; vn4[2] += fnor[2];
				}

				/* in case of flat - just assign, the vertices are split */
				if(v1.getFlag() & TV_CALCFACENORMAL) {
					v1.SetNormal(fnor);
					v2.SetNormal(fnor);
					v3.SetNormal(fnor);
					if(v4)
						v4->SetNormal(fnor);
				}
			}
		}
	}

	/* assign smooth vertex normals */
	for(RAS_MaterialBucket::Set::iterator mit = m_pMeshObject->GetFirstMaterial();
		mit != m_pMeshObject->GetLastMaterial(); ++ mit) {
		RAS_IPolyMaterial *mat = (*mit)->GetPolyMaterial();

		vecVertexArray& vertexarrays = m_pMeshObject->GetVertexCache(mat);

		for (i=0; i<vertexarrays.size(); i++) {
			KX_VertexArray& vertexarray = (*vertexarrays[i]);
			
			for(j=0; j<vertexarray.size(); j++) {
				RAS_TexVert& v = vertexarray[j];

				if(!(v.getFlag() & TV_CALCFACENORMAL))
					v.SetNormal(m_transnors[v.getOrigIndex()]); //.safe_normalized()
			}
		}
	}
}

void BL_MeshDeformer::VerifyStorage()
{
	/* Ensure that we have the right number of verts assigned */
	if (m_tvtot!=m_bmesh->totvert){
		if (m_transverts)
			delete [] m_transverts;
		if (m_transnors)
			delete [] m_transnors;
		
		m_transverts=new float[m_bmesh->totvert][3];
		m_transnors=new float[m_bmesh->totvert][3];
		m_tvtot = m_bmesh->totvert;
	}
}
 
