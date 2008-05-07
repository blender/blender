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

#ifndef NAN_INCLUDED_LOD_QuadricEditor_h
#define NAN_INCLUDED_LOD_QuadricEditor_h

#include "MEM_NonCopyable.h"
#include "LOD_ManMesh2.h"
#include "MT_Vector3.h"
#include "LOD_Quadric.h"

class LOD_ExternNormalEditor;


class LOD_QuadricEditor : public MEM_NonCopyable
{

public : 

	// Creation
	///////////

	static
		LOD_QuadricEditor *
	New(
		LOD_ManMesh2 &mesh
	); 

	// Property editor interface
	////////////////////////////

		void
	Remove(
		std::vector<LOD_VertexInd> &sorted_vertices
	);

		void
	Update(
		std::vector<LOD_FaceInd> &sorted_vertices
	);


		std::vector<LOD_Quadric> &
	Quadrics(
	) const {
		return *m_quadrics;
	};


	// Editor specific methods
	//////////////////////////

		bool
	BuildQuadrics(
		LOD_ExternNormalEditor& normal_editor,
		bool preserve_boundaries
	);	


		void
	ComputeEdgeCosts(
		std::vector<LOD_EdgeInd> &edges
	); 	

		MT_Vector3 
	TargetVertex(
		LOD_Edge &e
	);

	~LOD_QuadricEditor(
	 ){
		delete(m_quadrics);
	};

		
private :

	std::vector<LOD_Quadric> * m_quadrics;

	LOD_ManMesh2 &m_mesh;

private :
	
	LOD_QuadricEditor(LOD_ManMesh2 &mesh);



};

#endif

