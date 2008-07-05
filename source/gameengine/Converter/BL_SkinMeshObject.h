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
 */

#ifndef __BL_SKINMESHOBJECT
#define __BL_SKINMESHOBJECT

#ifdef WIN32
#pragma warning (disable:4786) // get rid of stupid stl-visual compiler debug warning
#endif //WIN32
#include "MEM_guardedalloc.h"
#include "RAS_MeshObject.h"
#include "RAS_Deformer.h"
#include "RAS_IPolygonMaterial.h"

#include "BL_MeshDeformer.h"

#include "DNA_mesh_types.h"
#include "DNA_key_types.h"
#include "DNA_meshdata_types.h"

typedef vector<struct MVert*> BL_MVertArray;
typedef vector<struct MDeformVert*> BL_DeformVertArray;
typedef vector<class BL_TexVert> BL_VertexArray;


typedef vector<vector<struct MDeformVert*>*> vecMDVertArray;
typedef vector<vector<class BL_TexVert>*> vecBVertexArray;

class BL_SkinArrayOptimizer : public KX_ArrayOptimizer  
{
public:
	BL_SkinArrayOptimizer(int index)
		:KX_ArrayOptimizer (index) {};
	virtual ~BL_SkinArrayOptimizer(){

		for (vector<KX_IndexArray*>::iterator itv = m_MvertArrayCache1.begin();
		!(itv == m_MvertArrayCache1.end());itv++)
		{
			delete (*itv);
		}
		for (vector<BL_DeformVertArray*>::iterator itd = m_DvertArrayCache1.begin();
		!(itd == m_DvertArrayCache1.end());itd++)
		{
			delete (*itd);
		}
		for (vector<KX_IndexArray*>::iterator iti = m_DIndexArrayCache1.begin();
		!(iti == m_DIndexArrayCache1.end());iti++)
		{
			delete (*iti);
		}
		
		m_MvertArrayCache1.clear();
		m_DvertArrayCache1.clear();
		m_DIndexArrayCache1.clear();
	};

	vector<KX_IndexArray*>		m_MvertArrayCache1;
	vector<BL_DeformVertArray*>	m_DvertArrayCache1;
	vector<KX_IndexArray*>		m_DIndexArrayCache1;

};

class BL_SkinMeshObject : public RAS_MeshObject
{

//	enum	{	BUCKET_MAX_INDICES = 16384};//2048};//8192};
//	enum	{	BUCKET_MAX_TRIANGLES = 4096};

	KX_ArrayOptimizer*		GetArrayOptimizer(RAS_IPolyMaterial* polymat)
	{
		KX_ArrayOptimizer** aop = (m_matVertexArrayS[*polymat]);
		if (aop)
			return *aop;
		int numelements = m_matVertexArrayS.size();
		m_sortedMaterials.push_back(polymat);
		
		BL_SkinArrayOptimizer* ao = new BL_SkinArrayOptimizer(numelements);
		m_matVertexArrayS.insert(*polymat,ao);
		return ao;
	}

protected:
	vector<int>				 m_cacheWeightIndex;

public:
	struct BL_MDVertMap { RAS_IPolyMaterial *mat; int index; };
	vector<vector<BL_MDVertMap> >	m_mvert_to_dvert_mapping;

	void Bucketize(double* oglmatrix,void* clientobj,bool useObjectColor,const MT_Vector4& rgbavec);
//	void Bucketize(double* oglmatrix,void* clientobj,bool useObjectColor,const MT_Vector4& rgbavec,class RAS_BucketManager* bucketmgr);

	int FindVertexArray(int numverts,RAS_IPolyMaterial* polymat);
	BL_SkinMeshObject(Mesh* mesh, int lightlayer) : RAS_MeshObject (mesh, lightlayer)
	{ 
		m_class = 1;
		if (m_mesh && m_mesh->key)
		{
			KeyBlock *kb;
			int count=0;
			// initialize weight cache for shape objects
			// count how many keys in this mesh
			for(kb= (KeyBlock*)m_mesh->key->block.first; kb; kb= (KeyBlock*)kb->next)
				count++;
			m_cacheWeightIndex.resize(count,-1);
		}
	};

	virtual ~BL_SkinMeshObject()
	{
		if (m_mesh && m_mesh->key) 
		{
			KeyBlock *kb;
			// remove the weight cache to avoid memory leak 
			for(kb= (KeyBlock*)m_mesh->key->block.first; kb; kb= (KeyBlock*)kb->next) {
				if(kb->weights) 
					MEM_freeN(kb->weights);
				kb->weights= NULL;
			}
		}
	};

	const vecIndexArrays& GetDIndexCache (RAS_IPolyMaterial* mat)
	{
		BL_SkinArrayOptimizer* ao = (BL_SkinArrayOptimizer*)GetArrayOptimizer(mat);//*(m_matVertexArrays[*mat]);
		return ao->m_DIndexArrayCache1;
	}
	const vecMDVertArray&	GetDVertCache (RAS_IPolyMaterial* mat)
	{
		BL_SkinArrayOptimizer* ao = (BL_SkinArrayOptimizer*)GetArrayOptimizer(mat);//*(m_matVertexArrays[*mat]);
		return ao->m_DvertArrayCache1;
	}
	const vecIndexArrays&	GetMVertCache (RAS_IPolyMaterial* mat)
	{
		BL_SkinArrayOptimizer* ao = (BL_SkinArrayOptimizer*)GetArrayOptimizer(mat);//*(m_matVertexArrays[*mat]);
		return ao->m_MvertArrayCache1;
	}
	
	void AddPolygon(RAS_Polygon* poly);
	int FindOrAddDeform(unsigned int vtxarray, unsigned int mv, struct MDeformVert *dv, RAS_IPolyMaterial* mat);
	int FindOrAddVertex(int vtxarray,const MT_Point3& xyz,
		const MT_Point2& uv,
		const MT_Point2& uv2,
		const MT_Vector4& tangent,
		const unsigned int rgbacolor,
		const MT_Vector3& normal, int defnr, bool flat, RAS_IPolyMaterial* mat, int origindex)
	{
		BL_SkinArrayOptimizer* ao = (BL_SkinArrayOptimizer*)GetArrayOptimizer(mat);
		int numverts = ao->m_VertexArrayCache1[vtxarray]->size();
		int index = RAS_MeshObject::FindOrAddVertex(vtxarray, xyz, uv, uv2, tangent, rgbacolor, normal, flat, mat, origindex);

		/* this means a new vertex was added, so we add the defnr too */
		if(index == numverts)
			ao->m_DIndexArrayCache1[vtxarray]->push_back(defnr);

		return index;
	}
	// for shape keys, 
	void CheckWeightCache(struct Object* obj);

};

#endif

