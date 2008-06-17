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
#include "BL_MeshDeformer.h"
#include "BL_SkinMeshObject.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "GEN_Map.h"
#include "STR_HashedString.h"

bool BL_MeshDeformer::Apply(RAS_IPolyMaterial *mat)
{
	size_t			i, j, index;
	vecVertexArray	array;
	vecIndexArrays	mvarray;
	vecIndexArrays	diarray;

	RAS_TexVert *tv;
	MVert	*mvert;

	// For each material
	array = m_pMeshObject->GetVertexCache(mat);
	mvarray = m_pMeshObject->GetMVertCache(mat);
	diarray = m_pMeshObject->GetDIndexCache(mat);

	// For each array
	for (i=0; i<array.size(); i++){
		//	For each vertex
		for (j=0; j<array[i]->size(); j++){
			tv = &((*array[i])[j]);
			index = ((*diarray[i])[j]);

			mvert = &(m_bmesh->mvert[((*mvarray[i])[index])]);
			tv->SetXYZ(MT_Point3(mvert->co));
		}
	}
	return true;
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
	 * since the GPU can do it faster
	 *
	 * There's a lot of indirection here to get to the data, can this work
	 * with less arrays/indirection? */

	vecIndexArrays indexarrays;
	vecIndexArrays mvarrays;
	vecIndexArrays diarrays;
	vecVertexArray vertexarrays;
	size_t i, j;

	/* set vertex normals to zero */
	for (i=0; i<(size_t)m_bmesh->totvert; i++)
		m_transnors[i] = MT_Vector3(0.0f, 0.0f, 0.0f);

	/* add face normals to vertices. */
	for(RAS_MaterialBucket::Set::iterator mit = m_pMeshObject->GetFirstMaterial();
		mit != m_pMeshObject->GetLastMaterial(); ++ mit) {
		RAS_IPolyMaterial *mat = (*mit)->GetPolyMaterial();

		indexarrays = m_pMeshObject->GetIndexCache(mat);
		vertexarrays = m_pMeshObject->GetVertexCache(mat);
		diarrays = m_pMeshObject->GetDIndexCache(mat);
		mvarrays = m_pMeshObject->GetMVertCache(mat);

		for (i=0; i<indexarrays.size(); i++) {
			KX_VertexArray& vertexarray = (*vertexarrays[i]);
			const KX_IndexArray& mvarray = (*mvarrays[i]);
			const KX_IndexArray& diarray = (*diarrays[i]);
			const KX_IndexArray& indexarray = (*indexarrays[i]);
			int nvert = mat->UsesTriangles()? 3: 4;

			for(j=0; j<indexarray.size(); j+=nvert) {
				MT_Point3 mv1, mv2, mv3, mv4, fnor;
				int i1 = indexarray[j];
				int i2 = indexarray[j+1];
				int i3 = indexarray[j+2];
				RAS_TexVert& v1 = vertexarray[i1];
				RAS_TexVert& v2 = vertexarray[i2];
				RAS_TexVert& v3 = vertexarray[i3];

				/* compute face normal */
				mv1 = MT_Point3(v1.getLocalXYZ());
				mv2 = MT_Point3(v2.getLocalXYZ());
				mv3 = MT_Point3(v3.getLocalXYZ());

				if(nvert == 4) {
					int i4 = indexarray[j+3];
					RAS_TexVert& v4 = vertexarray[i4];
					mv4 = MT_Point3(v4.getLocalXYZ());

					fnor = (((mv2-mv1).cross(mv3-mv2))+((mv4-mv3).cross(mv1-mv4))); //.safe_normalized();
				}
				else
					fnor = ((mv2-mv1).cross(mv3-mv2)); //.safe_normalized();

				/* add to vertices for smooth normals */
				m_transnors[mvarray[diarray[i1]]] += fnor;
				m_transnors[mvarray[diarray[i2]]] += fnor;
				m_transnors[mvarray[diarray[i3]]] += fnor;

				/* in case of flat - just assign, the vertices are split */
				if(v1.getFlag() & TV_CALCFACENORMAL) {
					v1.SetNormal(fnor);
					v2.SetNormal(fnor);
					v3.SetNormal(fnor);
				}

				if(nvert == 4) {
					int i4 = indexarray[j+3];
					RAS_TexVert& v4 = vertexarray[i4];

					/* same as above */
					m_transnors[mvarray[diarray[i4]]] += fnor;

					if(v4.getFlag() & TV_CALCFACENORMAL)
						v4.SetNormal(fnor);
				}
			}
		}
	}

	/* assign smooth vertex normals */
	for(RAS_MaterialBucket::Set::iterator mit = m_pMeshObject->GetFirstMaterial();
		mit != m_pMeshObject->GetLastMaterial(); ++ mit) {
		RAS_IPolyMaterial *mat = (*mit)->GetPolyMaterial();

		vertexarrays = m_pMeshObject->GetVertexCache(mat);
		diarrays = m_pMeshObject->GetDIndexCache(mat);
		mvarrays = m_pMeshObject->GetMVertCache(mat);

		for (i=0; i<vertexarrays.size(); i++) {
			KX_VertexArray& vertexarray = (*vertexarrays[i]);
			const KX_IndexArray& mvarray = (*mvarrays[i]);
			const KX_IndexArray& diarray = (*diarrays[i]);
			
			for(j=0; j<vertexarray.size(); j++)
				if(!(vertexarray[j].getFlag() & TV_CALCFACENORMAL))
					vertexarray[j].SetNormal(m_transnors[mvarray[diarray[j]]]); //.safe_normalized()
		}
	}
}

void BL_MeshDeformer::VerifyStorage()
{
	/* Ensure that we have the right number of verts assigned */
	if (m_tvtot!=m_bmesh->totvert+m_bmesh->totface) {
		if (m_transverts)
			delete [] m_transverts;
		if (m_transnors)
			delete [] m_transnors;
		
		m_transverts=new float[(sizeof(*m_transverts)*m_bmesh->totvert)][3];
		m_transnors=new MT_Vector3[m_bmesh->totvert];
		m_tvtot = m_bmesh->totvert;
	}
}
 
