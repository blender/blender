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

#ifndef NAN_INCLUDED_MeshBounds_h
#define NAN_INCLUDED_MeshBounds_h

#include "MEM_SmartPtr.h"
#include "LOD_MeshPrimitives.h"
#include "LOD_ManMesh2.h"
#include "MT_assert.h"

// simple class to compute the mesh bounds of a manifold mesh,

class LOD_MeshBounds {

public :
	static
		LOD_MeshBounds *
	New(
	){

		MEM_SmartPtr<LOD_MeshBounds> output(new LOD_MeshBounds());		
		return output.Release();
	}

		void
	ComputeBounds(
		const LOD_ManMesh2 * mesh
	){
		MT_assert(mesh!=NULL);
		MT_assert(mesh->VertexSet().size() > 0);

		const std::vector<LOD_Vertex> &verts = mesh->VertexSet(); 
		
		m_min = verts[0].pos;
		m_max = verts[0].pos;

		// iterate through the verts

		int t;
		const int size = verts.size();

		for (t=1; t< size ; ++t) {

			UpdateBounds(verts[t].pos,m_min,m_max);
		}
	}
				
		MT_Vector3
	Min(
	) const {
		return m_min;
	}

		MT_Vector3
	Max(
	) const {
		return m_max;
	}

private :

		void
	UpdateBounds(
		MT_Vector3 vertex,
		MT_Vector3& min,
		MT_Vector3& max
	) {
		if (vertex.x() < min.x()) {
			min.x() = vertex.x();
		} else
		if (vertex.x() > max.x()) {
			max.x()= vertex.x();
		}

		if (vertex.y() < min.y()) {
			min.y() = vertex.y();
		} else
		if (vertex.y() > max.y()) {
			max.y()= vertex.y();
		}

		if (vertex.z() < min.z()) {
			min.z() = vertex.z();
		} else
		if (vertex.z() > max.z()) {
			max.z()= vertex.z();
		}
	}

	LOD_MeshBounds(
	) :
		m_min(0,0,0),
		m_max(0,0,0)
	{
	};
			
	MT_Vector3 m_min;
	MT_Vector3 m_max;

};

#endif

