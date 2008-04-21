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

#ifndef NAN_INCLUDED_FaceNormalEditor_h
#define NAN_INCLUDED_FaceNormalEditor_h

#include "MEM_NonCopyable.h"
#include "LOD_ManMesh2.h"
#include "MT_Vector3.h"


class LOD_FaceNormalEditor : public MEM_NonCopyable
{

public : 

	// Creation
	///////////

	static
		LOD_FaceNormalEditor *
	New(
		LOD_ManMesh2 &mesh
	); 

	// Property editor interface
	////////////////////////////


	// Faces
	////////
		void
	Remove(
		std::vector<LOD_FaceInd> &sorted_faces
	);

		void
	Add(
	); 	

		void
	Update(
		std::vector<LOD_FaceInd> &sorted_faces
	);

	
	// vertex normals
	/////////////////

		void
	RemoveVertexNormals(
		std::vector<LOD_VertexInd> &sorted_verts
	);


		void
	UpdateVertexNormals(
		std::vector<LOD_VertexInd> &sorted_verts
	);



	const 
		std::vector<MT_Vector3> &
	Normals(
	) const {
		return m_normals.Ref();
	};


	const 
		std::vector<MT_Vector3> &
	VertexNormals(
	) const {
		return m_vertex_normals.Ref();
	};

	// Editor specific methods
	//////////////////////////

		void
	BuildNormals(
	);	


private :

	MEM_SmartPtr<std::vector<MT_Vector3> > m_normals;
	MEM_SmartPtr<std::vector<MT_Vector3> > m_vertex_normals;

	LOD_ManMesh2 &m_mesh;

private :
	
		
	LOD_FaceNormalEditor(LOD_ManMesh2 &mesh);

	const 
		MT_Vector3 
	ComputeNormal(
		const LOD_TriFace &face
	) const ;

	const 
		MT_Vector3
	ComputeVertexNormal (
		const LOD_VertexInd vi
	) const;



};

#endif

