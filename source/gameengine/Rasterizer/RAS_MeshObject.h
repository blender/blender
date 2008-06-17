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
#ifndef __RAS_MESHOBJECT
#define __RAS_MESHOBJECT

#ifdef WIN32
// disable the STL warnings ("debug information length > 255")
#pragma warning (disable:4786)
#endif

#include <vector>
#include <set>

#include "RAS_Polygon.h"
#include "RAS_MaterialBucket.h"
#include "MT_Transform.h"

#include "GEN_HashedPtr.h"

/**
 * This class holds an array of vertices and indicies.
 */
class KX_ArrayOptimizer
{
public:
	KX_ArrayOptimizer(int index) 
		: m_index1(index)
	{};
	virtual ~KX_ArrayOptimizer();
	
	vector<KX_VertexArray*>		m_VertexArrayCache1;
	vector<int>					m_TriangleArrayCount;
	vector<KX_IndexArray*>		m_IndexArrayCache1;

	/**
		order in which they are stored into the mesh
	*/
	int							m_index1;
};

/**
 * This struct holds a triangle.
 */
struct	RAS_TriangleIndex
{
public:
	int	m_index[3];
	int	m_array;
	RAS_IPolyMaterial*	m_matid;
	bool	m_collider;
};

/**
 * This class looks horribly broken.  Only m_matid is used, and
 * m_matid is a (int) RAS_IPolyMaterial*.  
 * --> m_matid == lhs.m_matid should be *m_matid == *lhs.m_matid
 */
class	RAS_MatArrayIndex
{
public:

	int	m_arrayindex1;
	RAS_IPolyMaterial*	m_matid;
	int	m_array;
	int	m_index;

/*
	inline bool	Less(const RAS_MatArrayIndex& lhs) const {
		bool result = 
			(	(m_matid < lhs.m_matid)		||
				((m_matid == lhs.m_matid)&&(m_array < lhs.m_array)) ||
				((m_matid == lhs.m_matid) && (m_array == lhs.m_array) &&
				(m_index < lhs.m_index))
					
			);
		return result;
			
	}
*/
	
};
/*
inline  bool operator <( const RAS_MatArrayIndex& rhs,const RAS_MatArrayIndex& lhs)
{
	return ( rhs.Less(lhs));
}*/

/**
 * RAS_MeshObject stores mesh data for the renderer.
 */
class RAS_MeshObject
{
	
	//	GEN_Map<class RAS_IPolyMaterial,KX_ArrayOptimizer*> m_matVertexArrayS;
	//vector<class RAS_IPolyMaterial*,KX_ArrayOptimizer> m_vertexArrays;
	virtual KX_ArrayOptimizer* GetArrayOptimizer(RAS_IPolyMaterial* polymat);
	//vector<RAS_Polygon*>		m_polygons;
	
	unsigned int				m_debugcolor;
	bool						m_bModified;
	int							m_lightlayer;
	
	vector<class RAS_Polygon*> 	m_Polygons;
	STR_String					m_name;
	static STR_String			s_emptyname;
	bool						m_zsort;
	bool						m_MeshMod;

	struct polygonSlot;
	struct backtofront;
	struct fronttoback;

	
protected:
	enum { BUCKET_MAX_INDICES = 65535 };//2048};//8192};
	enum { BUCKET_MAX_TRIANGLES = 65535 };
	
	GEN_Map<class RAS_IPolyMaterial,KX_ArrayOptimizer*> m_matVertexArrayS;
	
	RAS_MaterialBucket::Set			m_materials;
public:
	// for now, meshes need to be in a certain layer (to avoid sorting on lights in realtime)
	RAS_MeshObject(int lightlayer);
	virtual ~RAS_MeshObject();

	vector<RAS_IPolyMaterial*>				m_sortedMaterials;
	vector<vector<RAS_MatArrayIndex> >		m_xyz_index_to_vertex_index_mapping;
	vector<RAS_TriangleIndex >				m_triangle_indices;
	
	int							m_class;

	unsigned int				GetLightLayer();
	int					NumMaterials();
	const STR_String&	GetMaterialName(unsigned int matid);
	RAS_MaterialBucket*	GetMaterialBucket(unsigned int matid);
	const STR_String&	GetTextureName(unsigned int matid);
	virtual void		AddPolygon(RAS_Polygon* poly);
	void				UpdateMaterialList();
	
	int					NumPolygons();
	RAS_Polygon*		GetPolygon(int num);
	
	virtual void		Bucketize(
							double* oglmatrix,
							void* clientobj,
							bool useObjectColor,
							const MT_Vector4& rgbavec
						);

	void				RemoveFromBuckets(
							double* oglmatrix,
							void* clientobj
						);

	void				MarkVisible(
							double* oglmatrix,
							void* clientobj,
							bool visible,
							bool useObjectColor,
							const MT_Vector4& rgbavec
						);

	void				DebugColor(unsigned int abgr);
	
	/**
	 *  Sorts the polygons by their transformed z values.
	 */
	void				SortPolygons(const MT_Transform &transform);

	void				SchedulePolygons(
							const MT_Transform &transform,
							int drawingmode
						);

	void				ClearArrayData();
	
	RAS_MaterialBucket::Set::iterator GetFirstMaterial();
	RAS_MaterialBucket::Set::iterator GetLastMaterial();
	
	virtual RAS_TexVert*	GetVertex(
								short array,
								unsigned int index,
								RAS_IPolyMaterial* polymat
							);
	
	virtual int			FindVertexArray(
							int numverts,
							RAS_IPolyMaterial* polymat
						);
	
	void				SchedulePoly(
							const KX_VertexIndex& idx,
							int numverts,
							RAS_IPolyMaterial* mat
						);

	void				ScheduleWireframePoly(
							const KX_VertexIndex& idx,
							int numverts,
							int edgecode,
							RAS_IPolyMaterial* mat
						);
	
	// find (and share) or add vertices
	// for some speedup, only the last 20 added vertices are searched for equality
	
	virtual int			FindOrAddVertex(
							int vtxarray,
							const MT_Point3& xyz,
							const MT_Point2& uv,
							const MT_Point2& uv2,
							const MT_Vector4& tangent,
							const unsigned int rgbacolor,
							const MT_Vector3& normal,
							bool flat,
							RAS_IPolyMaterial* mat,
							int orgindex
						);
	
	const vecVertexArray&	GetVertexCache (RAS_IPolyMaterial* mat);
	
	int					GetVertexArrayLength(RAS_IPolyMaterial* mat);

	RAS_TexVert*		GetVertex(
							unsigned int matid,
							unsigned int index
						);

	const vecIndexArrays& GetIndexCache (RAS_IPolyMaterial* mat);
	void				SetName(STR_String name);
	const STR_String&	GetName();

	bool				MeshModified();
	void				SetMeshModified(bool v){m_MeshMod = v;}

};

#endif //__RAS_MESHOBJECT

