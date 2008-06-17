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
 * Deformer that supports armature skinning
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#pragma warning (disable:4786) // get rid of stupid stl-visual compiler debug warning
#endif //WIN32
#include "RAS_IPolygonMaterial.h"
#include "BL_SkinMeshObject.h"
#include "BL_DeformableGameObject.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "KX_GameObject.h"
#include "RAS_BucketManager.h"

void BL_SkinMeshObject::AddPolygon(RAS_Polygon* poly)
{
	/* We're overriding this so that we can eventually associate faces with verts somehow */

	//	For vertIndex in poly:
	//		find the appropriate normal

	RAS_MeshObject::AddPolygon(poly);
}

int BL_SkinMeshObject::FindOrAddDeform(unsigned int vtxarray, unsigned int mv, struct MDeformVert *dv, RAS_IPolyMaterial* mat)
{
	BL_SkinArrayOptimizer* ao = (BL_SkinArrayOptimizer*)GetArrayOptimizer(mat);//*(m_matVertexArrays[*mat]);
	int numvert = ao->m_MvertArrayCache1[vtxarray]->size();

	/* Check to see if this has already been pushed */
	for (vector<BL_MDVertMap>::iterator it = m_mvert_to_dvert_mapping[mv].begin();
	     it != m_mvert_to_dvert_mapping[mv].end();
	     it++)
	{
		if(it->mat == mat)
			return it->index;
	}

	ao->m_MvertArrayCache1[vtxarray]->push_back(mv);
	ao->m_DvertArrayCache1[vtxarray]->push_back(dv);

	BL_MDVertMap mdmap;
	mdmap.mat = mat;
	mdmap.index = numvert;
	m_mvert_to_dvert_mapping[mv].push_back(mdmap);
	
	return numvert;
};

int	BL_SkinMeshObject::FindVertexArray(int numverts,RAS_IPolyMaterial* polymat)
{
	int array=-1;
	
	BL_SkinArrayOptimizer* ao = (BL_SkinArrayOptimizer*)GetArrayOptimizer(polymat);


	for (size_t i=0;i<ao->m_VertexArrayCache1.size();i++)
	{
		if ( (ao->m_TriangleArrayCount[i] + (numverts-2)) < BUCKET_MAX_TRIANGLES) 
		{
			 if((ao->m_VertexArrayCache1[i]->size()+numverts < BUCKET_MAX_INDICES))
				{
					array = i;
					ao->m_TriangleArrayCount[array]+=numverts-2;
					break;
				}
		}
	}
	

	if (array == -1)
	{
		array = ao->m_VertexArrayCache1.size();
		
		vector<RAS_TexVert>* va = new vector<RAS_TexVert>;
		ao->m_VertexArrayCache1.push_back(va);
		
		KX_IndexArray *ia = new KX_IndexArray();
		ao->m_IndexArrayCache1.push_back(ia);

		KX_IndexArray *bva = new KX_IndexArray();
		ao->m_MvertArrayCache1.push_back(bva);

		BL_DeformVertArray *dva = new BL_DeformVertArray();
		ao->m_DvertArrayCache1.push_back(dva);

		KX_IndexArray *da = new KX_IndexArray();
		ao->m_DIndexArrayCache1.push_back(da);

		ao->m_TriangleArrayCount.push_back(numverts-2);

	}

		
	return array;
}


//void BL_SkinMeshObject::Bucketize(double* oglmatrix,void* clientobj,bool useObjectColor,const MT_Vector4& rgbavec,RAS_BucketManager* bucketmgr)
void BL_SkinMeshObject::Bucketize(double* oglmatrix,void* clientobj,bool useObjectColor,const MT_Vector4& rgbavec)
{

	KX_MeshSlot ms;
	ms.m_clientObj = clientobj;
	ms.m_mesh = this;
	ms.m_OpenGLMatrix = oglmatrix;
	ms.m_bObjectColor = useObjectColor;
	ms.m_RGBAcolor = rgbavec;
	ms.m_pDeformer = ((BL_DeformableGameObject*)clientobj)->m_pDeformer;
	
	for (RAS_MaterialBucket::Set::iterator it = m_materials.begin();it!=m_materials.end();it++)
	{

		RAS_MaterialBucket* materialbucket = (*it);

//		KX_ArrayOptimizer* oa = GetArrayOptimizer(materialbucket->GetPolyMaterial());
		materialbucket->SetMeshSlot(ms);
	}

}



