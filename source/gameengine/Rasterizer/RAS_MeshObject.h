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
#include <list>

#include "RAS_Polygon.h"
#include "RAS_MaterialBucket.h"
#include "MT_Transform.h"

#include "GEN_HashedPtr.h"

struct Mesh;
class RAS_Deformer;

/* RAS_MeshObject is a mesh used for rendering. It stores polygons,
 * but the actual vertices and index arrays are stored in material
 * buckets, referenced by the list of RAS_MeshMaterials. */

class RAS_MeshObject
{
private:
	unsigned int				m_debugcolor;

	bool						m_bModified;
	bool						m_bMeshModified;

	STR_String					m_name;
	static STR_String			s_emptyname;

	vector<class RAS_Polygon*> 	m_Polygons;

	/* polygon sorting */
	struct polygonSlot;
	struct backtofront;
	struct fronttoback;

protected:
	list<RAS_MeshMaterial>			m_materials;
	Mesh*							m_mesh;
	bool							m_bDeformed;

public:
	// for now, meshes need to be in a certain layer (to avoid sorting on lights in realtime)
	RAS_MeshObject(Mesh* mesh);
	virtual ~RAS_MeshObject();


	bool				IsDeformed() { return (m_bDeformed && m_mesh); }
	
	/* materials */
	int					NumMaterials();
	const STR_String&	GetMaterialName(unsigned int matid);
	const STR_String&	GetTextureName(unsigned int matid);

	RAS_MeshMaterial* 	GetMeshMaterial(unsigned int matid);
	RAS_MeshMaterial*	GetMeshMaterial(RAS_IPolyMaterial *mat);
	int					GetMaterialId(RAS_IPolyMaterial *mat);

	list<RAS_MeshMaterial>::iterator GetFirstMaterial();
	list<RAS_MeshMaterial>::iterator GetLastMaterial();

	//unsigned int		GetLightLayer();

	/* name */
	void				SetName(const char *name);
	STR_String&			GetName();

	/* modification state */
	bool				MeshModified();
	void				SetMeshModified(bool v){m_bMeshModified = v;}

	/* original blender mesh */
	Mesh*				GetMesh() { return m_mesh; }

	/* mesh construction */
	
	virtual RAS_Polygon*	AddPolygon(RAS_MaterialBucket *bucket, int numverts);
	virtual void			AddVertex(RAS_Polygon *poly, int i,
							const MT_Point3& xyz,
							const MT_Point2& uv,
							const MT_Point2& uv2,
							const MT_Vector4& tangent,
							const unsigned int rgbacolor,
							const MT_Vector3& normal,
							bool flat,
							int origindex);

	void					SchedulePolygons(int drawingmode);

	/* vertex and polygon acces */
	int					NumVertices(RAS_IPolyMaterial* mat);
	RAS_TexVert*		GetVertex(unsigned int matid, unsigned int index);
	const float*		GetVertexLocation(unsigned int orig_index);

	int					NumPolygons();
	RAS_Polygon*		GetPolygon(int num) const;
	
	/* buckets */
	virtual void		AddMeshUser(void *clientobj, SG_QList *head, RAS_Deformer* deformer);
	virtual void		UpdateBuckets(
							void* clientobj,
							double* oglmatrix,
							bool useObjectColor,
							const MT_Vector4& rgbavec,
							bool visible,
							bool culled);

	void				RemoveFromBuckets(void *clientobj);
	void				EndConversion() {
#if 0
		m_sharedvertex_map.clear(); // SharedVertex
		vector<vector<SharedVertex> >	shared_null(0);
		shared_null.swap( m_sharedvertex_map ); /* really free the memory */
#endif
	}

	/* colors */
	void				DebugColor(unsigned int abgr);
	void 				SetVertexColor(RAS_IPolyMaterial* mat,MT_Vector4 rgba);
	
	/* polygon sorting by Z for alpha */
	void				SortPolygons(RAS_MeshSlot& ms, const MT_Transform &transform);


	bool				HasColliderPolygon() {
		int numpolys= NumPolygons();
		for (int p=0; p<numpolys; p++)
			if (m_Polygons[p]->IsCollider())
				return true;
		
		return false;
	}

	/* for construction to find shared vertices */
	struct SharedVertex {
		RAS_DisplayArray *m_darray;
		int m_offset;
	};

	vector<vector<SharedVertex> >	m_sharedvertex_map;
	
	
#ifdef WITH_CXX_GUARDEDALLOC
public:
	void *operator new( unsigned int num_bytes) { return MEM_mallocN(num_bytes, "GE:RAS_MeshObject"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};

#endif //__RAS_MESHOBJECT

