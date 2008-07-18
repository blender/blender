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

static int get_def_index(Object* ob, const char* vgroup)
{
	bDeformGroup *curdef;
	int index = 0;

	for (curdef = (bDeformGroup*)ob->defbase.first; curdef; curdef=(bDeformGroup*)curdef->next, index++)
		if (!strcmp(curdef->name, vgroup))
			return index;
	return -1;
}

void BL_SkinMeshObject::CheckWeightCache(Object* obj)
{
	KeyBlock *kb;
	int kbindex, defindex;
	MDeformVert *dvert= NULL;
	int totvert, i, j;
	float *weights;

	if (!m_mesh->key)
		return;

	for(kbindex=0, kb= (KeyBlock*)m_mesh->key->block.first; kb; kb= (KeyBlock*)kb->next, kbindex++)
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

			dvert= m_mesh->dvert;
			totvert= m_mesh->totvert;
		
			weights= (float*)MEM_callocN(totvert*sizeof(float), "weights");
		
			for (i=0; i < totvert; i++, dvert++) {
				for(j=0; j<dvert->totweight; j++) {
					if (dvert->dw[j].def_nr == defindex) {
						weights[i]= dvert->dw[j].weight;
						break;
					}
				}
			}
			kb->weights = weights;
			m_cacheWeightIndex[kbindex] = defindex;
		}
	}
}


