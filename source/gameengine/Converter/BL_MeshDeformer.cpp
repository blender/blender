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
#include "BLI_arithb.h"

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
		delete []m_transverts;
	if (m_transnors)
		delete []m_transnors;
};

/**
 * @warning This function is expensive!
 */
void BL_MeshDeformer::RecalcNormals()
{
	int v, f;
	float fnor[3], co1[3], co2[3], co3[3], co4[3];

	/* Clear all vertex normal accumulators */
	for (v =0; v<m_bmesh->totvert; v++){
		m_transnors[v]=MT_Point3(0,0,0);
	}
	
	/* Find the face normals */
	for (f = 0; f<m_bmesh->totface; f++){
		// Make new face normal based on the transverts
		MFace *mf= &((MFace*)m_bmesh->mface)[f];
		
		if (mf->v3) {
			for (int vl=0; vl<3; vl++){
				co1[vl]=m_transverts[mf->v1][vl];
				co2[vl]=m_transverts[mf->v2][vl];
				co3[vl]=m_transverts[mf->v3][vl];
				if (mf->v4)
					co4[vl]=m_transverts[mf->v4][vl];
			}

			/* FIXME: Use moto */
			if (mf->v4)
				CalcNormFloat4(co1, co2, co3, co4, fnor);
			else
				CalcNormFloat(co1, co2, co3, fnor);
	
			/* Decide which normals are affected by this face's normal */
			m_transnors[mf->v1]+=MT_Point3(fnor);
			m_transnors[mf->v2]+=MT_Point3(fnor);
			m_transnors[mf->v3]+=MT_Point3(fnor);
			if (mf->v4)
				m_transnors[mf->v4]+=MT_Point3(fnor);
		}
	}
	
	for (v =0; v<m_bmesh->totvert; v++){
//		float nor[3];

		m_transnors[v]=m_transnors[v].safe_normalized();
//		nor[0]=m_transnors[v][0];
//		nor[1]=m_transnors[v][1];
//		nor[2]=m_transnors[v][2];
		
	};
}

void BL_MeshDeformer::VerifyStorage()
{
	/* Ensure that we have the right number of verts assigned */
	if (m_tvtot!=m_bmesh->totvert+m_bmesh->totface){
		if (m_transverts)
			delete []m_transverts;
		if (m_transnors)
			delete []m_transnors;
		
		m_transnors =new MT_Point3[m_bmesh->totvert+m_bmesh->totface];
		m_transverts=new float[(sizeof(*m_transverts)*m_bmesh->totvert)][3];
		m_tvtot = m_bmesh->totvert;
	}
}
 
