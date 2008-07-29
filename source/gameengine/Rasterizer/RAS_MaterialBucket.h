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
#ifndef __RAS_MATERIALBUCKET
#define __RAS_MATERIALBUCKET

#include "RAS_TexVert.h"
#include "GEN_Map.h"
#include "STR_HashedString.h"

#include "MT_Transform.h"
#include "RAS_IPolygonMaterial.h"
#include "RAS_IRasterizer.h"
#include "RAS_Deformer.h"	// __NLA
#include <vector>
#include <map>
#include <set>
using namespace std;

/**
 * KX_VertexIndex
 */
struct KX_VertexIndex {
public:
	KX_VertexIndex(int size);
	void	SetIndex(short loc,unsigned int index);
	
	// The vertex array
	short	m_vtxarray;
	// An index into the vertex array for up to 4 verticies
	unsigned short	m_indexarray[4];
	short	m_size;
};

/**
 * KX_ListSlot.
 */
class KX_ListSlot
{
protected:
	int m_refcount;
public:
	KX_ListSlot(){ m_refcount=1; }
	virtual ~KX_ListSlot() {}
	virtual int Release() { 
		if (--m_refcount > 0)
			return m_refcount;
		delete this;
		return 0;
	}
	virtual KX_ListSlot* AddRef() {
		m_refcount++;
		return this;
	}
	virtual void SetModified(bool mod)=0;
};

/**
 * KX_MeshSlot.
 */
class KX_MeshSlot
{
public:
	void*						m_clientObj;
	RAS_Deformer*				m_pDeformer;	//	__NLA
	double*						m_OpenGLMatrix;
	class RAS_MeshObject*		m_mesh;
	mutable bool				m_bVisible; // for visibility
	mutable bool				m_bObjectColor;
	mutable MT_Vector4			m_RGBAcolor;
	mutable KX_ListSlot*		m_DisplayList; // for lists
	KX_MeshSlot() :
		m_pDeformer(NULL), 
		m_bVisible(true),
		m_DisplayList(0)
	{
	}
	~KX_MeshSlot();
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
	
	RAS_IPolyMaterial*		GetPolyMaterial() const;
	bool	IsAlpha() const;
	bool	IsZSort() const;
		
	static void	StartFrame();
	static void EndFrame();

	void	SetMeshSlot(KX_MeshSlot& ms);
	void	RemoveMeshSlot(KX_MeshSlot& ms);
	void	MarkVisibleMeshSlot(KX_MeshSlot& ms,
								bool visible,
								bool color,
								const MT_Vector4& rgbavec);

	void RenderMeshSlot(const MT_Transform& cameratrans, RAS_IRasterizer* rasty,
		RAS_IRenderTools* rendertools, const KX_MeshSlot &ms, RAS_IRasterizer::DrawMode drawmode);
	bool ActivateMaterial(const MT_Transform& cameratrans, RAS_IRasterizer* rasty,
		RAS_IRenderTools *rendertools, RAS_IRasterizer::DrawMode& drawmode);
	
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
	bool						m_bModified;
	RAS_IPolyMaterial*			m_material;
	double*						m_pOGLMatrix;
	
};

#endif //__KX_BUCKET

