/**
 * $Id$
 *
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
#ifndef __RAS_MATERIALBUCKET
#define __RAS_MATERIALBUCKET

#include "RAS_TexVert.h"
#include "GEN_Map.h"
#include "STR_HashedString.h"

#include "MT_Transform.h"
#include "RAS_IPolygonMaterial.h"
#include "RAS_Deformer.h"	// __NLA
#include <vector>
#include <map>
#include <set>
using namespace std;

typedef vector< vector<class RAS_TexVert>* >  vecVertexArray;
typedef vector<unsigned int> KX_IndexArray;
typedef vector< KX_IndexArray* > vecIndexArrays;

typedef vector<RAS_TexVert> KX_VertexArray;


/**
 * KX_VertexIndex
 */
struct KX_VertexIndex {
public:
	KX_VertexIndex(int size);
	void	SetIndex(short loc,short index);
	short	m_vtxarray;
	short	m_indexarray[4];
	short	m_size;
};


/**
 * KX_MeshSlot.
 */
class KX_MeshSlot
{
public:
	void*				m_clientObj;
	RAS_Deformer*			m_pDeformer;	//	__NLA
	double*				m_OpenGLMatrix;
	class RAS_MeshObject*		m_mesh;
	
	mutable bool			m_bVisible; // for visibility
	mutable bool  		    m_bObjectColor;
	mutable MT_Vector4		m_RGBAcolor;
	
	KX_MeshSlot() :m_pDeformer(NULL), m_bVisible(true) {}
	//	KX_MeshSlot() :m_bVisible(true) {}
	
	bool					Less(const KX_MeshSlot& lhs) const;
};


inline bool operator <( const KX_MeshSlot& rhs,const KX_MeshSlot& lhs)
{
	return ( rhs.Less(lhs));
}

/**
 * Contains a list of meshs with the same material properties.
 */
class RAS_MaterialBucket
{
public:
	typedef std::set<KX_MeshSlot> T_MeshSlotList;
	
	RAS_MaterialBucket(RAS_IPolyMaterial* mat);
	virtual ~RAS_MaterialBucket() {}
	
	void	Render(const MT_Transform& cameratrans,
					   class RAS_IRasterizer* rasty,
					   class RAS_IRenderTools* rendertools);
	
	void	SchedulePolygons(int drawingmode);
	void	ClearScheduledPolygons();
	
	RAS_IPolyMaterial*		GetPolyMaterial() const;
	bool	IsTransparant() const;
		
	static void	StartFrame();
	static void EndFrame();

	void	SetMeshSlot(KX_MeshSlot& ms);
	void	RemoveMeshSlot(KX_MeshSlot& ms);
	void	MarkVisibleMeshSlot(KX_MeshSlot& ms,
								bool visible,
								bool color,
								const MT_Vector4& rgbavec);
	
	void RenderMeshSlot(const MT_Transform& cameratrans, RAS_IRasterizer* rasty,
		RAS_IRenderTools* rendertools, const KX_MeshSlot &ms, int drawmode);
	int ActivateMaterial(const MT_Transform& cameratrans, RAS_IRasterizer* rasty,
		RAS_IRenderTools *rendertools);
	
	unsigned int NumMeshSlots();
	T_MeshSlotList::iterator msBegin();
	T_MeshSlotList::iterator msEnd();

	struct less
	{
		bool operator()(const RAS_MaterialBucket* x, const RAS_MaterialBucket* y) const 
		{ 
			return *x->GetPolyMaterial() < *y->GetPolyMaterial(); 
		}
	};
	
	typedef set<RAS_MaterialBucket*, less> Set;
private:
	
	T_MeshSlotList				m_meshSlots;
	bool						m_bScheduled;
	bool						m_bModified;
	RAS_IPolyMaterial*			m_material;
	double*						m_pOGLMatrix;
	
};

#endif //__KX_BUCKET

