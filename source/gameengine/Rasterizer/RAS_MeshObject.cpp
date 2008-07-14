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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "RAS_MeshObject.h"
#include "RAS_IRasterizer.h"
#include "MT_MinMax.h"
#include "MT_Point3.h"


STR_String RAS_MeshObject::s_emptyname = "";



KX_ArrayOptimizer::~KX_ArrayOptimizer()
{
	for (vector<KX_VertexArray*>::iterator itv = m_VertexArrayCache1.begin();
	!(itv == m_VertexArrayCache1.end());++itv)
	{
		delete (*itv);
	}

	for (vector<KX_IndexArray*>::iterator iti = m_IndexArrayCache1.begin();
	!(iti == m_IndexArrayCache1.end());++iti)
	{
		delete (*iti);
	}
	
	m_TriangleArrayCount.clear();
	m_VertexArrayCache1.clear();
	m_IndexArrayCache1.clear();


}



RAS_MeshObject::RAS_MeshObject(Mesh* mesh, int lightlayer)
	: m_bModified(true),
	m_lightlayer(lightlayer),
	m_zsort(false),
	m_MeshMod(true),
	m_mesh(mesh),
	m_class(0)
{
}


bool RAS_MeshObject::MeshModified()
{
	return m_MeshMod;
}

	
RAS_MeshObject::~RAS_MeshObject()
{
	for (vector<RAS_Polygon*>::iterator it=m_Polygons.begin();!(it==m_Polygons.end());it++)
	{
		delete (*it);
	}

	ClearArrayData();
}



unsigned int RAS_MeshObject::GetLightLayer()
{
	return m_lightlayer;
}



int RAS_MeshObject::NumMaterials()
{
	return m_materials.size();
}

const STR_String& RAS_MeshObject::GetMaterialName(unsigned int matid)
{ 
	RAS_MaterialBucket* bucket = GetMaterialBucket(matid);
	
	return bucket?bucket->GetPolyMaterial()->GetMaterialName():s_emptyname;
}

RAS_MaterialBucket* RAS_MeshObject::GetMaterialBucket(unsigned int matid)
{
	if (m_materials.size() > 0 && (matid < m_materials.size()))
	{
		RAS_MaterialBucket::Set::const_iterator it = m_materials.begin();
		while (matid--) ++it;
		return *it;
	}

	return NULL;
}



int RAS_MeshObject::NumPolygons()
{
	return m_Polygons.size();
}



RAS_Polygon* RAS_MeshObject::GetPolygon(int num)
{
	return m_Polygons[num];
}

	
	
RAS_MaterialBucket::Set::iterator RAS_MeshObject::GetFirstMaterial()
{
	return m_materials.begin();
}



RAS_MaterialBucket::Set::iterator RAS_MeshObject::GetLastMaterial()
{
	return m_materials.end();
}



void RAS_MeshObject::SetName(STR_String name)
{
	m_name = name;
}



const STR_String& RAS_MeshObject::GetName()
{
	return m_name;
}



const STR_String& RAS_MeshObject::GetTextureName(unsigned int matid)
{ 
	RAS_MaterialBucket* bucket = GetMaterialBucket(matid);
	
	return bucket?bucket->GetPolyMaterial()->GetTextureName():s_emptyname;
}



void RAS_MeshObject::AddPolygon(RAS_Polygon* poly)
{
	m_Polygons.push_back(poly);
}



void RAS_MeshObject::DebugColor(unsigned int abgr)
{
/*
	int numpolys = NumPolygons();
	for (int i=0;i<numpolys;i++)
	{
		RAS_Polygon* poly = m_polygons[i];
		for (int v=0;v<poly->VertexCount();v++)
		{
		RAS_TexVert* vtx = poly->GetVertex(v);
		vtx->setDebugRGBA(abgr);
		}
	}
	*/

	m_debugcolor = abgr;	
}



void RAS_MeshObject::SchedulePoly(const KX_VertexIndex& idx,
								  int numverts,
								  RAS_IPolyMaterial* mat)
{
	//int indexpos = m_IndexArrayCount[idx.m_vtxarray];
	//m_IndexArrayCount[idx.m_vtxarray] = indexpos + 3;
	
	KX_ArrayOptimizer* ao = GetArrayOptimizer(mat);
	ao->m_IndexArrayCache1[idx.m_vtxarray]->push_back(idx.m_indexarray[0]);
	ao->m_IndexArrayCache1[idx.m_vtxarray]->push_back(idx.m_indexarray[1]);
	ao->m_IndexArrayCache1[idx.m_vtxarray]->push_back(idx.m_indexarray[2]);
	if (!mat->UsesTriangles()) //if (!m_bUseTriangles)
	{	
		//m_IndexArrayCount[idx.m_vtxarray] = indexpos+4;
		ao->m_IndexArrayCache1[idx.m_vtxarray]->push_back(idx.m_indexarray[3]);
	}
}



void RAS_MeshObject::ScheduleWireframePoly(const KX_VertexIndex& idx,
										   int numverts,
										   int edgecode,
										   RAS_IPolyMaterial* mat)
{
	//int indexpos = m_IndexArrayCount[idx.m_vtxarray];
	int edgetrace = 1<<(numverts-1);
	bool drawedge = (edgecode & edgetrace)!=0;
	edgetrace = 1;
	int prevvert = idx.m_indexarray[numverts-1];
	KX_ArrayOptimizer* ao = GetArrayOptimizer(mat);
	
	for (int v = 0; v < numverts; v++)
	{
		unsigned int curvert = idx.m_indexarray[v];
		if (drawedge)
		{
			ao->m_IndexArrayCache1[idx.m_vtxarray]->push_back(prevvert);
			ao->m_IndexArrayCache1[idx.m_vtxarray]->push_back(curvert);
		}
		prevvert = curvert;
		drawedge = (edgecode & edgetrace)!=0;
		edgetrace*=2;
	}
	//m_IndexArrayCount[idx.m_vtxarray] = indexpos;
}

int RAS_MeshObject::FindOrAddVertex(int vtxarray,
									const MT_Point3& xyz,
									const MT_Point2& uv,
									const MT_Point2& uv2,
									const MT_Vector4& tangent,
									const unsigned int rgbacolor,
									const MT_Vector3& normal,
									bool flat,
									RAS_IPolyMaterial* mat,
									int origindex)
{
	KX_ArrayOptimizer* ao = GetArrayOptimizer(mat);
	
	int numverts = ao->m_VertexArrayCache1[vtxarray]->size();//m_VertexArrayCount[vtxarray];
	RAS_TexVert newvert(xyz,uv,uv2,tangent,rgbacolor,normal, flat? TV_CALCFACENORMAL: 0,origindex);

#define KX_FIND_SHARED_VERTICES
#ifdef KX_FIND_SHARED_VERTICES
	if(!flat) {
		for (std::vector<RAS_MatArrayIndex>::iterator it = m_xyz_index_to_vertex_index_mapping[origindex].begin();
			 it != m_xyz_index_to_vertex_index_mapping[origindex].end();
			 it++)
		{
			if ((*it).m_arrayindex1 == ao->m_index1 &&
				(*it).m_array == vtxarray && 
				*(*it).m_matid == *mat &&
				(*ao->m_VertexArrayCache1[vtxarray])[(*it).m_index].closeTo(&newvert)
				)
			{
				return (*it).m_index;
			}
		}
	}
#endif // KX_FIND_SHARED_VERTICES
	
	// no vertex found, add one
	ao->m_VertexArrayCache1[vtxarray]->push_back(newvert);
	//	printf("(%f,%f,%f) ",xyz[0],xyz[1],xyz[2]);
	RAS_MatArrayIndex idx;
	idx.m_arrayindex1 = ao->m_index1;
	idx.m_array = vtxarray;
	idx.m_index = numverts;
	idx.m_matid = mat;
	m_xyz_index_to_vertex_index_mapping[origindex].push_back(idx); 
	
	return numverts;
}

vecVertexArray& RAS_MeshObject::GetVertexCache (RAS_IPolyMaterial* mat)
{
	KX_ArrayOptimizer* ao = GetArrayOptimizer(mat);

	return ao->m_VertexArrayCache1;
}

int RAS_MeshObject::GetVertexArrayLength(RAS_IPolyMaterial* mat)
{
	int len = 0;

	const vecVertexArray & vertexvec = GetVertexCache(mat);
	vector<KX_VertexArray*>::const_iterator it = vertexvec.begin();
	
	for (; it != vertexvec.end(); ++it)
	{
		len += (*it)->size();
	}

	return len;
}

	

RAS_TexVert* RAS_MeshObject::GetVertex(unsigned int matid,
									   unsigned int index)
{
	RAS_TexVert* vertex = NULL;
	
	RAS_MaterialBucket* bucket = GetMaterialBucket(matid);
	if (bucket)
	{
		RAS_IPolyMaterial* mat = bucket->GetPolyMaterial();
		if (mat)
		{
			const vecVertexArray & vertexvec = GetVertexCache(mat);
			vector<KX_VertexArray*>::const_iterator it = vertexvec.begin();
			
			for (unsigned int len = 0; it != vertexvec.end(); ++it)
			{
				if (index < len + (*it)->size())
				{
					vertex = &(*(*it))[index-len];
					break;
				}
				else
				{
					len += (*it)->size();
				}
			}
		}
	}
	
	return vertex;
}



const vecIndexArrays& RAS_MeshObject::GetIndexCache (RAS_IPolyMaterial* mat)
{
	KX_ArrayOptimizer* ao = GetArrayOptimizer(mat);

	return ao->m_IndexArrayCache1;
}



KX_ArrayOptimizer* RAS_MeshObject::GetArrayOptimizer(RAS_IPolyMaterial* polymat)
{
	KX_ArrayOptimizer** aop = m_matVertexArrayS[polymat];

	if(aop)
		return *aop;
	
	// didn't find array, but an array might already exist
	// for a material equal to this one
	for(int i=0;i<m_matVertexArrayS.size();i++) {
		RAS_IPolyMaterial *mat = (RAS_IPolyMaterial*)(m_matVertexArrayS.getKey(i)->getValue());
		if(*mat == *polymat) {
			m_matVertexArrayS.insert(polymat, *m_matVertexArrayS.at(i));
			return *m_matVertexArrayS.at(i);
		}
	}

	// create new array
	int numelements = m_matVertexArrayS.size();
	m_sortedMaterials.push_back(polymat);

	KX_ArrayOptimizer* ao = new KX_ArrayOptimizer(numelements);
	m_matVertexArrayS.insert(polymat, ao);
	
	return ao;
}



void RAS_MeshObject::Bucketize(double* oglmatrix,
							   void* clientobj,
							   bool useObjectColor,
							   const MT_Vector4& rgbavec)
{
	KX_MeshSlot ms;
	ms.m_clientObj = clientobj;
	ms.m_mesh = this;
	ms.m_OpenGLMatrix = oglmatrix;
	ms.m_bObjectColor = useObjectColor;
	ms.m_RGBAcolor = rgbavec;
	
	for (RAS_MaterialBucket::Set::iterator it = m_materials.begin();it!=m_materials.end();++it)
	{
		RAS_MaterialBucket* bucket = *it;
		bucket->SchedulePolygons(0);
//		KX_ArrayOptimizer* oa = GetArrayOptimizer(bucket->GetPolyMaterial());
		bucket->SetMeshSlot(ms);
	}

}



void RAS_MeshObject::MarkVisible(double* oglmatrix,
								 void* clientobj,
								 bool visible,
								 bool useObjectColor,
								 const MT_Vector4& rgbavec)
{
	KX_MeshSlot ms;
	ms.m_clientObj = clientobj;
	ms.m_mesh = this;
	ms.m_OpenGLMatrix = oglmatrix;
	ms.m_RGBAcolor = rgbavec;
	ms.m_bObjectColor= useObjectColor;

	for (RAS_MaterialBucket::Set::iterator it = m_materials.begin();it!=m_materials.end();++it)
	{
		RAS_MaterialBucket* bucket = *it;
		bucket->SchedulePolygons(0);
//		KX_ArrayOptimizer* oa = GetArrayOptimizer(bucket->GetPolyMaterial());
		bucket->MarkVisibleMeshSlot(ms,visible,useObjectColor,rgbavec);
	}
}


void RAS_MeshObject::RemoveFromBuckets(double* oglmatrix,
									   void* clientobj)
{
	KX_MeshSlot ms;
	ms.m_clientObj = clientobj;
	ms.m_mesh = this;
	ms.m_OpenGLMatrix = oglmatrix;

	for (RAS_MaterialBucket::Set::iterator it = m_materials.begin();it!=m_materials.end();++it)
	{
		RAS_MaterialBucket* bucket = *it;
//		RAS_IPolyMaterial* polymat = bucket->GetPolyMaterial();
		bucket->SchedulePolygons(0);
		//KX_ArrayOptimizer* oa = GetArrayOptimizer(polymat);
		bucket->RemoveMeshSlot(ms);
	}

}



/*
 * RAS_MeshObject::GetVertex returns the vertex located somewhere in the vertexpool
 * it is the clients responsibility to make sure the array and index are valid
 */
RAS_TexVert* RAS_MeshObject::GetVertex(short array,
									   unsigned int index,
									   RAS_IPolyMaterial* polymat)
{
	 KX_ArrayOptimizer* ao = GetArrayOptimizer(polymat);
	return &((*(ao->m_VertexArrayCache1)[array])[index]);
}



void RAS_MeshObject::ClearArrayData()
{
	for (int i=0;i<m_matVertexArrayS.size();i++) {
		KX_ArrayOptimizer** ao = m_matVertexArrayS.at(i);

		// we have duplicate entries, only free once
		for(int j=i+1;j<m_matVertexArrayS.size();j++) {
			if(ao == m_matVertexArrayS.at(j)) {
				ao = NULL;
				break;
			}
		}

		if (ao)
			delete *ao;
	}
}



/**
 * RAS_MeshObject::CreateNewVertices creates vertices within sorted pools of vertices that share same material
*/
int	RAS_MeshObject::FindVertexArray(int numverts,
									RAS_IPolyMaterial* polymat)
{
//	bool found=false;
	int array=-1;
	
	KX_ArrayOptimizer* ao = GetArrayOptimizer(polymat);

	for (unsigned int i=0;i<ao->m_VertexArrayCache1.size();i++)
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
		ao->m_TriangleArrayCount.push_back(numverts-2);
	}

	return array;
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



void RAS_MeshObject::UpdateMaterialList()
{
	m_materials.clear();
	unsigned int numpolys = m_Polygons.size();
	// for all polygons, find out which material they use, and add it to the set of materials
	for (unsigned int i=0;i<numpolys;i++)
	{
		m_materials.insert(m_Polygons[i]->GetMaterial());
	}
}

struct RAS_MeshObject::polygonSlot
{
	float        m_z;
	RAS_Polygon *m_poly;
		
	polygonSlot(float z, RAS_Polygon* poly) :
		m_z(z),
		m_poly(poly)
	{}
	/**
	 * pnorm and pval form the plane equation that the distance from is used to
	 * sort against.
	 */
        polygonSlot(const MT_Vector3 &pnorm, const MT_Scalar &pval, RAS_MeshObject *mesh, RAS_Polygon* poly) :
			m_poly(poly)
	{
		const KX_VertexIndex &base = m_poly->GetIndexBase();
		RAS_TexVert *vert = mesh->GetVertex(base.m_vtxarray, base.m_indexarray[0], poly->GetMaterial()->GetPolyMaterial());
		m_z = MT_dot(pnorm, vert->getLocalXYZ()) + pval;
		
		for(int i = 1; i < m_poly->VertexCount(); i++)
		{
			vert = mesh->GetVertex(base.m_vtxarray, base.m_indexarray[i], poly->GetMaterial()->GetPolyMaterial());
			float z = MT_dot(pnorm, vert->getLocalXYZ()) + pval;
			m_z += z;
		}
		m_z /= m_poly->VertexCount();
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


void RAS_MeshObject::SortPolygons(const MT_Transform &transform)
{
	if (!m_zsort)
		return;
		
	// Extract camera Z plane...
	const MT_Vector3 pnorm(transform.getBasis()[2]);
	const MT_Scalar pval = transform.getOrigin()[2];
	
	unsigned int numpolys = m_Polygons.size();
	std::multiset<polygonSlot, backtofront> alphapolyset;
	std::multiset<polygonSlot, fronttoback> solidpolyset;
	
	for (unsigned int p = 0; p < numpolys; p++)
	{
		RAS_Polygon* poly = m_Polygons[p];
		if (poly->IsVisible())
		{
			if (poly->GetMaterial()->GetPolyMaterial()->IsTransparant())
			{
				alphapolyset.insert(polygonSlot(pnorm, pval, this, poly));
			} else {
				solidpolyset.insert(polygonSlot(pnorm, pval, this, poly));
			}
		}
	}
	
	// Clear current array data.
	for (RAS_MaterialBucket::Set::iterator it = m_materials.begin();it!=m_materials.end();++it)
	{
		vector<KX_IndexArray*> *indexcache = &GetArrayOptimizer((*it)->GetPolyMaterial())->m_IndexArrayCache1;
		for (vector<KX_IndexArray*>::iterator iit = indexcache->begin(); iit != indexcache->end(); ++iit)
			(*iit)->clear();
	}

	std::multiset<polygonSlot, fronttoback>::iterator sit = solidpolyset.begin();
	for (; sit != solidpolyset.end(); ++sit)
		SchedulePoly((*sit).m_poly->GetVertexIndexBase(), (*sit).m_poly->VertexCount(), (*sit).m_poly->GetMaterial()->GetPolyMaterial());
	
	std::multiset<polygonSlot, backtofront>::iterator ait = alphapolyset.begin();
	for (; ait != alphapolyset.end(); ++ait)
		SchedulePoly((*ait).m_poly->GetVertexIndexBase(), (*ait).m_poly->VertexCount(), (*ait).m_poly->GetMaterial()->GetPolyMaterial());
}


void RAS_MeshObject::SchedulePolygons(const MT_Transform &transform, int drawingmode)
{
//	int nummaterials = m_materials.size();
	int i;

	if (m_bModified)
	{
		for (RAS_MaterialBucket::Set::iterator it = m_materials.begin();it!=m_materials.end();++it)
		{
			RAS_MaterialBucket* bucket = *it;

			bucket->SchedulePolygons(drawingmode);
			if (bucket->GetPolyMaterial()->IsZSort())
				m_zsort = true;
		}

		int numpolys = m_Polygons.size();
		
		if ((drawingmode > RAS_IRasterizer::KX_BOUNDINGBOX) && 
			(drawingmode < RAS_IRasterizer::KX_SOLID))
		{
			for (i=0;i<numpolys;i++)
			{
				RAS_Polygon* poly = m_Polygons[i];
				if (poly->IsVisible())
					ScheduleWireframePoly(poly->GetVertexIndexBase(),poly->VertexCount(),poly->GetEdgeCode()
					,poly->GetMaterial()->GetPolyMaterial());
				
			}
			m_zsort = false;
		}
		else
		{
			if (!m_zsort)
			{
				for (i=0;i<numpolys;i++)
				{
					RAS_Polygon* poly = m_Polygons[i];
					if (poly->IsVisible())
					{
						SchedulePoly(poly->GetVertexIndexBase(),poly->VertexCount(),poly->GetMaterial()->GetPolyMaterial());
					}
				}
			}
		}

		m_bModified = false;

		m_MeshMod = true;
	} 
}
