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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

// implementation of LOD_FaceNormalEditor.h

///////////////////////////////////////
#include "LOD_FaceNormalEditor.h"

using namespace std;

LOD_FaceNormalEditor::
LOD_FaceNormalEditor(
	LOD_ManMesh2 & mesh
) : m_mesh(mesh) {
};

	LOD_FaceNormalEditor *
LOD_FaceNormalEditor::
New(
	LOD_ManMesh2 &mesh
){
	// build a set of normals of the same size
	// as the number of polys in the mesh

	MEM_SmartPtr<LOD_FaceNormalEditor> output(new LOD_FaceNormalEditor(mesh));

	int face_num = mesh.FaceSet().size();

	MEM_SmartPtr<vector<MT_Vector3> > normals(new vector<MT_Vector3>);
	MEM_SmartPtr<vector<MT_Vector3> > vertex_normals(new vector<MT_Vector3>);

	if (output == NULL ||
		normals == NULL
	) {
		return NULL;
	}	

	normals->reserve(face_num);
	vertex_normals->reserve(mesh.VertexSet().size());
	output->m_normals = normals.Release();
	output->m_vertex_normals = vertex_normals.Release();

	return output.Release();
};


// Property editor interface
////////////////////////////

	void
LOD_FaceNormalEditor::
Remove(
	std::vector<LOD_FaceInd> &sorted_faces
){
	
	// assumes a collection of faces sorted in descending order .
	
	vector<MT_Vector3> & normals = m_normals.Ref();

	vector<LOD_FaceInd>::const_iterator it_start = sorted_faces.begin();
	vector<LOD_FaceInd>::const_iterator it_end = sorted_faces.end();

	for (; it_start != it_end; ++it_start) {

		if (normals.size() > 0) {
			MT_Vector3 temp = normals[*it_start];
		
			normals[*it_start] = normals.back();
			normals.back() = temp;

			normals.pop_back();
		}

		// FIXME - through exception
	}
}


	void
LOD_FaceNormalEditor::
Add(
){
	MT_Vector3 zero(0.0f,0.0f,0.0f);
	m_normals->push_back(zero);
}

	void
LOD_FaceNormalEditor::
Update(
	std::vector<LOD_FaceInd> &sorted_faces
){

	vector<MT_Vector3> & normals = m_normals.Ref();

	vector<LOD_FaceInd>::const_iterator it_start = sorted_faces.begin();
	vector<LOD_FaceInd>::const_iterator it_end = sorted_faces.end();

	const vector<LOD_TriFace> &faces = m_mesh.FaceSet();

	for (; it_start != it_end; ++it_start) {		
		normals[*it_start] = ComputeNormal(faces[*it_start]);		
	}	
};

// vertex normals
/////////////////


	void
LOD_FaceNormalEditor::
RemoveVertexNormals(
	vector<LOD_VertexInd> &sorted_verts
){
	vector<MT_Vector3> & vertex_normals = m_vertex_normals.Ref();

	vector<LOD_VertexInd>::const_iterator it_start = sorted_verts.begin();
	vector<LOD_VertexInd>::const_iterator it_end = sorted_verts.end();

	for (; it_start != it_end; ++it_start) {

		if (vertex_normals.size() > 0) {
			MT_Vector3 temp = vertex_normals[*it_start];
		
			vertex_normals[*it_start] = vertex_normals.back();
			vertex_normals.back() = temp;

			vertex_normals.pop_back();
		}

		// FIXME - through exception
	}
};

	void
LOD_FaceNormalEditor::
UpdateVertexNormals(
	vector<LOD_VertexInd> &sorted_verts
){
	vector<MT_Vector3> & vertex_normals = m_vertex_normals.Ref();

	vector<LOD_VertexInd>::const_iterator it_start = sorted_verts.begin();
	vector<LOD_VertexInd>::const_iterator it_end = sorted_verts.end();

	for (; it_start != it_end; ++it_start) {		
		vertex_normals[*it_start] = ComputeVertexNormal(*it_start);		
	}	
}



// Editor specific methods
//////////////////////////

	void
LOD_FaceNormalEditor::
BuildNormals(
){

	const vector<LOD_TriFace> &faces = m_mesh.FaceSet();
	vector<MT_Vector3> & normals = m_normals.Ref();

	int face_num = faces.size();
	int cur_face = 0;

	for (; cur_face < face_num; ++cur_face) {

		MT_Vector3 new_normal = ComputeNormal(faces[cur_face]);	
		normals.push_back(new_normal); 
	}
	// now build the vertex normals

	vector<MT_Vector3> & vertex_normals = m_vertex_normals.Ref();
	const vector<LOD_Vertex> &verts = m_mesh.VertexSet();

	int vertex_num = verts.size();
	int cur_vertex = 0;

	for (; cur_vertex < vertex_num; ++cur_vertex) {
		MT_Vector3 new_normal = ComputeVertexNormal(cur_vertex);
		vertex_normals.push_back(new_normal);
	}
}

const 
	MT_Vector3 
LOD_FaceNormalEditor::
ComputeNormal(
	const LOD_TriFace &face
) const {

	const vector<LOD_Vertex> &verts = m_mesh.VertexSet();

	MT_Vector3 vec1 = 
		verts[face.m_verts[1]].pos - 
		verts[face.m_verts[0]].pos;

	MT_Vector3 vec2 = 
		verts[face.m_verts[2]].pos - 
		verts[face.m_verts[1]].pos;

	vec1 = vec1.cross(vec2);

	if (!vec1.fuzzyZero()) {
		vec1.normalize();
		return (vec1);
	} else {		
		return (MT_Vector3(1.0,0,0));
	}		
}

const 
	MT_Vector3 
LOD_FaceNormalEditor::
ComputeVertexNormal(
	const LOD_VertexInd v
) const {

	// average the face normals surrounding this
	// vertex and normalize
	const vector<MT_Vector3> & face_normals = m_normals.Ref();

	vector<LOD_FaceInd> vertex_faces;
	vertex_faces.reserve(32);
	
	m_mesh.VertexFaces(v,vertex_faces);
	
	MT_Vector3 normal(0,0,0);

	vector<LOD_FaceInd>::const_iterator face_it = vertex_faces.begin();
	vector<LOD_FaceInd>::const_iterator face_end = vertex_faces.end();

	for (; face_it != face_end; ++face_it) {
		normal += face_normals[*face_it];
	}
	
	if (!normal.fuzzyZero()) {
		normal.normalize();
		return (normal);
	} else {		
		return (MT_Vector3(1.0,0,0));
	}		
}










	









