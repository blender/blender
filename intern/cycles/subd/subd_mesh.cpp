/*
 * Original code in the public domain -- castanyo@yahoo.es
 * 
 * Modifications copyright (c) 2011, Blender Foundation.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>

#include "subd_build.h"
#include "subd_edge.h"
#include "subd_face.h"
#include "subd_mesh.h"
#include "subd_patch.h"
#include "subd_split.h"
#include "subd_vert.h"

#include "util_debug.h"
#include "util_foreach.h"

CCL_NAMESPACE_BEGIN

SubdMesh::SubdMesh()
{
}

SubdMesh::~SubdMesh()
{
	pair<Key, SubdEdge*> em;

	foreach(SubdVert *vertex, verts)
		delete vertex;
	foreach(em, edge_map)
		delete em.second;
	foreach(SubdFace *face, faces)
		delete face;

	verts.clear();
	edges.clear();
	edge_map.clear();
	faces.clear();
}

SubdVert *SubdMesh::add_vert(const float3& co)
{
	SubdVert *v = new SubdVert(verts.size());
	v->co = co;
	verts.push_back(v);

	return v;
}

SubdFace *SubdMesh::add_face(int v0, int v1, int v2)
{
	int index[3] = {v0, v1, v2};
	return add_face(index, 3);
}

SubdFace *SubdMesh::add_face(int v0, int v1, int v2, int v3)
{
	int index[4] = {v0, v1, v2, v3};
	return add_face(index, 4);
}

SubdFace *SubdMesh::add_face(int *index, int num)
{
	/* test non-manifold cases */
	if(!can_add_face(index, num)) {
		/* we could try to add face in opposite winding instead .. */
		fprintf(stderr, "Warning: non manifold mesh, invalid face '%lu'.\n", (unsigned long)faces.size());
		return NULL;
	}
	
	SubdFace *f = new SubdFace(faces.size());
	
	SubdEdge *first_edge = NULL;
	SubdEdge *last = NULL;
	SubdEdge *current = NULL;

	/* add edges */
	for(int i = 0; i < num-1; i++) {
		current = add_edge(index[i], index[i+1]);
		assert(current != NULL);
		
		current->face = f;
		
		if(last != NULL) {
			last->next = current;
			current->prev = last;
		}
		else
			first_edge = current;
		
		last = current;
	}

	current = add_edge(index[num-1], index[0]);
	assert(current != NULL);
	
	current->face = f;

	last->next = current;
	current->prev = last;

	current->next = first_edge;
	first_edge->prev = current;

	f->edge = first_edge;
	faces.push_back(f);

	return f;
}

bool SubdMesh::can_add_face(int *index, int num)
{
	/* manifold check */
	for(int i = 0; i < num-1; i++)
		if(!can_add_edge(index[i], index[i+1]))
			return false;

	return can_add_edge(index[num-1], index[0]);
}

bool SubdMesh::can_add_edge(int i, int j)
{
	/* check for degenerate edge */
	if(i == j)
		return false;

	/* make sure edge has not been added yet. */
	return find_edge(i, j) == NULL;
}

SubdEdge *SubdMesh::add_edge(int i, int j)
{
	SubdEdge *edge;

	/* find pair */
	SubdEdge *pair = find_edge(j, i);

	if(pair != NULL) {
		/* create edge with same id */
		edge = new SubdEdge(pair->id + 1);
		
		/* link edge pairs */
		edge->pair = pair;
		pair->pair = edge;
		
		/* not sure this is necessary? */
		pair->vert->edge = pair;
	}
	else {
		/* create edge */
		edge = new SubdEdge(2*edges.size());
		
		/* add only unpaired edges */
		edges.push_back(edge);
	}
	
	/* assign vertex and put into map */
	edge->vert = verts[i];
	edge_map[Key(i, j)] = edge;
	
	/* face and next are set by add_face */
	
	return edge;
}

SubdEdge *SubdMesh::find_edge(int i, int j)
{
	map<Key, SubdEdge*>::const_iterator it = edge_map.find(Key(i, j));

	return (it == edge_map.end())? NULL: it->second;
}

bool SubdMesh::link_boundary()
{
	/* link boundary edges once the mesh has been created */
	int num = 0;
	
	/* create boundary edges */
	int num_edges = edges.size();

	for(int e = 0; e < num_edges; e++) {
		SubdEdge *edge = edges[e];

		if(edge->pair == NULL) {
			SubdEdge *pair = new SubdEdge(edge->id + 1);

			int i = edge->from()->id;
			int j = edge->to()->id;

			assert(edge_map.find(Key(j, i)) == edge_map.end());

			pair->vert = verts[j];
			edge_map[Key(j, i)] = pair;
			
			edge->pair = pair;
			pair->pair = edge;
			
			num++;
		}
	}

	/* link boundary edges */
	for(int e = 0; e < num_edges; e++) {
		SubdEdge *edge = edges[e];

		if(edge->pair->face == NULL)
			link_boundary_edge(edge->pair);
	}
	
	/* detect boundary intersections */
	int boundaryIntersections = 0;
	int num_verts = verts.size();

	for(int v = 0; v < num_verts; v++) {
		SubdVert *vertex = verts[v];

		int boundarySubdEdges = 0;
		for(SubdVert::EdgeIterator it(vertex->edges()); !it.isDone(); it.advance())
			if(it.current()->is_boundary())
				boundarySubdEdges++;

		if(boundarySubdEdges > 2) {
			assert((boundarySubdEdges & 1) == 0);
			boundaryIntersections++;
		}
	}

	if(boundaryIntersections != 0) {
		fprintf(stderr, "Invalid mesh, boundary intersections found!\n");
		return false;
	}

	return true;
}

void SubdMesh::link_boundary_edge(SubdEdge *edge)
{
	/* link this boundary edge. */

	/* make sure next pointer has not been set. */
	assert(edge->face == NULL);
	assert(edge->next == NULL);
		
	SubdEdge *next = edge;

	while(next->pair->face != NULL) {
		/* get pair prev */
		SubdEdge *e = next->pair->next;

		while(e->next != next->pair)
			e = e->next;

		next = e;
	}

	edge->next = next->pair;
	next->pair->prev = edge;
	
	/* adjust vertex edge, so that it's the boundary edge. (required for is_boundary()) */
	if(edge->vert->edge != edge)
		edge->vert->edge = edge;
}

void SubdMesh::tesselate(DiagSplit *split, bool linear, Mesh *mesh, int shader, bool smooth)
{
	SubdBuilder *builder = SubdBuilder::create(linear);
	int num_faces = faces.size();
		        
	for(int f = 0; f < num_faces; f++) {
		SubdFace *face = faces[f];
		Patch *patch = builder->run(face);

		if(patch->is_triangle())
			split->split_triangle(mesh, patch, shader, smooth);
		else
			split->split_quad(mesh, patch, shader, smooth);

		delete patch;
	}

	delete builder;
}

CCL_NAMESPACE_END

