/*
* $Id:
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
* along with this program; if not, write to the Free Software  Foundation,
* Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
* The Original Code is Copyright (C) 2005 by the Blender Foundation.
* All rights reserved.
*
* Contributor(s): Daniel Dunbar
*                 Ton Roosendaal,
*                 Ben Batt,
*                 Brecht Van Lommel,
*                 Campbell Barton
*
* ***** END GPL LICENSE BLOCK *****
*
*/

#include "stddef.h"
#include "string.h"
#include "stdarg.h"
#include "math.h"
#include "float.h"

#include "BLI_kdtree.h"
#include "BLI_rand.h"
#include "BLI_uvproject.h"

#include "MEM_guardedalloc.h"

#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_curve_types.h"
#include "DNA_key_types.h"
#include "DNA_material_types.h"
#include "DNA_object_fluidsim.h"


#include "BKE_action.h"
#include "BKE_bmesh.h"
#include "BKE_cloth.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_displist.h"
#include "BKE_fluidsim.h"
#include "BKE_global.h"
#include "BKE_multires.h"
#include "BKE_key.h"
#include "BKE_lattice.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_scene.h"
#include "BKE_smoke.h"
#include "BKE_softbody.h"
#include "BKE_subsurf.h"
#include "BKE_texture.h"

#include "depsgraph_private.h"
#include "BKE_deform.h"
#include "BKE_shrinkwrap.h"

#include "LOD_decimation.h"

#include "CCGSubSurf.h"

#include "RE_shader_ext.h"

#include "MOD_modifiertypes.h"

/* EdgeSplit modifier: Splits edges in the mesh according to sharpness flag
 * or edge angle (can be used to achieve autosmoothing)
*/
#if 0
#define EDGESPLIT_DEBUG_3
#define EDGESPLIT_DEBUG_2
#define EDGESPLIT_DEBUG_1
#define EDGESPLIT_DEBUG_0
#endif

static void initData(ModifierData *md)
{
	EdgeSplitModifierData *emd = (EdgeSplitModifierData*) md;

	/* default to 30-degree split angle, sharpness from both angle & flag
	*/
	emd->split_angle = 30;
	emd->flags = MOD_EDGESPLIT_FROMANGLE | MOD_EDGESPLIT_FROMFLAG;
}

static void copyData(ModifierData *md, ModifierData *target)
{
	EdgeSplitModifierData *emd = (EdgeSplitModifierData*) md;
	EdgeSplitModifierData *temd = (EdgeSplitModifierData*) target;

	temd->split_angle = emd->split_angle;
	temd->flags = emd->flags;
}

/* Mesh data for edgesplit operation */
typedef struct SmoothVert {
	LinkNode *faces;     /* all faces which use this vert */
	int oldIndex; /* the index of the original DerivedMesh vert */
	int newIndex; /* the index of the new DerivedMesh vert */
} SmoothVert;

#define SMOOTHEDGE_NUM_VERTS 2

typedef struct SmoothEdge {
	SmoothVert *verts[SMOOTHEDGE_NUM_VERTS]; /* the verts used by this edge */
	LinkNode *faces;     /* all faces which use this edge */
	int oldIndex; /* the index of the original DerivedMesh edge */
	int newIndex; /* the index of the new DerivedMesh edge */
	short flag; /* the flags from the original DerivedMesh edge */
} SmoothEdge;

#define SMOOTHFACE_MAX_EDGES 4

typedef struct SmoothFace {
	SmoothEdge *edges[SMOOTHFACE_MAX_EDGES]; /* nonexistent edges == NULL */
	int flip[SMOOTHFACE_MAX_EDGES]; /* 1 = flip edge dir, 0 = don't flip */
	float normal[3]; /* the normal of this face */
	int oldIndex; /* the index of the original DerivedMesh face */
	int newIndex; /* the index of the new DerivedMesh face */
} SmoothFace;

typedef struct SmoothMesh {
	SmoothVert *verts;
	SmoothEdge *edges;
	SmoothFace *faces;
	int num_verts, num_edges, num_faces;
	int max_verts, max_edges, max_faces;
	DerivedMesh *dm;
	float threshold; /* the cosine of the smoothing angle */
	int flags;
	MemArena *arena;
	ListBase propagatestack, reusestack;
} SmoothMesh;

static SmoothVert *smoothvert_copy(SmoothVert *vert, SmoothMesh *mesh)
{
	SmoothVert *copy = &mesh->verts[mesh->num_verts];

	if(mesh->num_verts >= mesh->max_verts) {
		printf("Attempted to add a SmoothMesh vert beyond end of array\n");
		return NULL;
	}

	*copy = *vert;
	copy->faces = NULL;
	copy->newIndex = mesh->num_verts;
	++mesh->num_verts;

#ifdef EDGESPLIT_DEBUG_2
	printf("copied vert %4d to vert %4d\n", vert->newIndex, copy->newIndex);
#endif
	return copy;
}

static SmoothEdge *smoothedge_copy(SmoothEdge *edge, SmoothMesh *mesh)
{
	SmoothEdge *copy = &mesh->edges[mesh->num_edges];

	if(mesh->num_edges >= mesh->max_edges) {
		printf("Attempted to add a SmoothMesh edge beyond end of array\n");
		return NULL;
	}

	*copy = *edge;
	copy->faces = NULL;
	copy->newIndex = mesh->num_edges;
	++mesh->num_edges;

#ifdef EDGESPLIT_DEBUG_2
	printf("copied edge %4d to edge %4d\n", edge->newIndex, copy->newIndex);
#endif
	return copy;
}

static int smoothedge_has_vert(SmoothEdge *edge, SmoothVert *vert)
{
	int i;
	for(i = 0; i < SMOOTHEDGE_NUM_VERTS; i++)
		if(edge->verts[i] == vert) return 1;

	return 0;
}

static SmoothMesh *smoothmesh_new(int num_verts, int num_edges, int num_faces,
				  int max_verts, int max_edges, int max_faces)
{
	SmoothMesh *mesh = MEM_callocN(sizeof(*mesh), "smoothmesh");
	mesh->verts = MEM_callocN(sizeof(*mesh->verts) * max_verts,
				  "SmoothMesh.verts");
	mesh->edges = MEM_callocN(sizeof(*mesh->edges) * max_edges,
				  "SmoothMesh.edges");
	mesh->faces = MEM_callocN(sizeof(*mesh->faces) * max_faces,
				  "SmoothMesh.faces");

	mesh->num_verts = num_verts;
	mesh->num_edges = num_edges;
	mesh->num_faces = num_faces;

	mesh->max_verts = max_verts;
	mesh->max_edges = max_edges;
	mesh->max_faces = max_faces;

	return mesh;
}

static void smoothmesh_free(SmoothMesh *mesh)
{
	int i;

	for(i = 0; i < mesh->num_verts; ++i)
		BLI_linklist_free(mesh->verts[i].faces, NULL);

	for(i = 0; i < mesh->num_edges; ++i)
		BLI_linklist_free(mesh->edges[i].faces, NULL);
	
	if(mesh->arena)
		BLI_memarena_free(mesh->arena);

	MEM_freeN(mesh->verts);
	MEM_freeN(mesh->edges);
	MEM_freeN(mesh->faces);
	MEM_freeN(mesh);
}

static void smoothmesh_resize_verts(SmoothMesh *mesh, int max_verts)
{
	int i;
	SmoothVert *tmp;

	if(max_verts <= mesh->max_verts) return;

	tmp = MEM_callocN(sizeof(*tmp) * max_verts, "SmoothMesh.verts");

	memcpy(tmp, mesh->verts, sizeof(*tmp) * mesh->num_verts);

	/* remap vert pointers in edges */
	for(i = 0; i < mesh->num_edges; ++i) {
		int j;
		SmoothEdge *edge = &mesh->edges[i];

		for(j = 0; j < SMOOTHEDGE_NUM_VERTS; ++j)
			/* pointer arithmetic to get vert array index */
			edge->verts[j] = &tmp[edge->verts[j] - mesh->verts];
	}

	MEM_freeN(mesh->verts);
	mesh->verts = tmp;
	mesh->max_verts = max_verts;
}

static void smoothmesh_resize_edges(SmoothMesh *mesh, int max_edges)
{
	int i;
	SmoothEdge *tmp;

	if(max_edges <= mesh->max_edges) return;

	tmp = MEM_callocN(sizeof(*tmp) * max_edges, "SmoothMesh.edges");

	memcpy(tmp, mesh->edges, sizeof(*tmp) * mesh->num_edges);

	/* remap edge pointers in faces */
	for(i = 0; i < mesh->num_faces; ++i) {
		int j;
		SmoothFace *face = &mesh->faces[i];

		for(j = 0; j < SMOOTHFACE_MAX_EDGES; ++j)
			if(face->edges[j])
				/* pointer arithmetic to get edge array index */
				face->edges[j] = &tmp[face->edges[j] - mesh->edges];
	}

	MEM_freeN(mesh->edges);
	mesh->edges = tmp;
	mesh->max_edges = max_edges;
}

#ifdef EDGESPLIT_DEBUG_0
static void smoothmesh_print(SmoothMesh *mesh)
{
	int i, j;
	DerivedMesh *dm = mesh->dm;

	printf("--- SmoothMesh ---\n");
	printf("--- Vertices ---\n");
	for(i = 0; i < mesh->num_verts; i++) {
		SmoothVert *vert = &mesh->verts[i];
		LinkNode *node;
		MVert mv;

		dm->getVert(dm, vert->oldIndex, &mv);

		printf("%3d: ind={%3d, %3d}, pos={% 5.1f, % 5.1f, % 5.1f}",
			   i, vert->oldIndex, vert->newIndex,
	 mv.co[0], mv.co[1], mv.co[2]);
		printf(", faces={");
		for(node = vert->faces; node != NULL; node = node->next) {
			printf(" %d", ((SmoothFace *)node->link)->newIndex);
		}
		printf("}\n");
	}

	printf("\n--- Edges ---\n");
	for(i = 0; i < mesh->num_edges; i++) {
		SmoothEdge *edge = &mesh->edges[i];
		LinkNode *node;

		printf("%4d: indices={%4d, %4d}, verts={%4d, %4d}",
			   i,
	 edge->oldIndex, edge->newIndex,
  edge->verts[0]->newIndex, edge->verts[1]->newIndex);
		if(edge->verts[0] == edge->verts[1]) printf(" <- DUPLICATE VERTEX");
		printf(", faces={");
		for(node = edge->faces; node != NULL; node = node->next) {
			printf(" %d", ((SmoothFace *)node->link)->newIndex);
		}
		printf("}\n");
	}

	printf("\n--- Faces ---\n");
	for(i = 0; i < mesh->num_faces; i++) {
		SmoothFace *face = &mesh->faces[i];

		printf("%4d: indices={%4d, %4d}, edges={", i,
			   face->oldIndex, face->newIndex);
		for(j = 0; j < SMOOTHFACE_MAX_EDGES && face->edges[j]; j++) {
			if(face->flip[j])
				printf(" -%-2d", face->edges[j]->newIndex);
			else
				printf("  %-2d", face->edges[j]->newIndex);
		}
		printf("}, verts={");
		for(j = 0; j < SMOOTHFACE_MAX_EDGES && face->edges[j]; j++) {
			printf(" %d", face->edges[j]->verts[face->flip[j]]->newIndex);
		}
		printf("}\n");
	}
}
#endif

static SmoothMesh *smoothmesh_from_derivedmesh(DerivedMesh *dm)
{
	SmoothMesh *mesh;
	EdgeHash *edges = BLI_edgehash_new();
	int i;
	int totvert, totedge, totface;

	totvert = dm->getNumVerts(dm);
	totedge = dm->getNumEdges(dm);
	totface = dm->getNumFaces(dm);

	mesh = smoothmesh_new(totvert, totedge, totface,
				  totvert, totedge, totface);

	mesh->dm = dm;

	for(i = 0; i < totvert; i++) {
		SmoothVert *vert = &mesh->verts[i];

		vert->oldIndex = vert->newIndex = i;
	}

	for(i = 0; i < totedge; i++) {
		SmoothEdge *edge = &mesh->edges[i];
		MEdge med;

		dm->getEdge(dm, i, &med);
		edge->verts[0] = &mesh->verts[med.v1];
		edge->verts[1] = &mesh->verts[med.v2];
		edge->oldIndex = edge->newIndex = i;
		edge->flag = med.flag;

		BLI_edgehash_insert(edges, med.v1, med.v2, edge);
	}

	for(i = 0; i < totface; i++) {
		SmoothFace *face = &mesh->faces[i];
		MFace mf;
		MVert v1, v2, v3;
		int j;

		dm->getFace(dm, i, &mf);

		dm->getVert(dm, mf.v1, &v1);
		dm->getVert(dm, mf.v2, &v2);
		dm->getVert(dm, mf.v3, &v3);
		face->edges[0] = BLI_edgehash_lookup(edges, mf.v1, mf.v2);
		if(face->edges[0]->verts[1]->oldIndex == mf.v1) face->flip[0] = 1;
		face->edges[1] = BLI_edgehash_lookup(edges, mf.v2, mf.v3);
		if(face->edges[1]->verts[1]->oldIndex == mf.v2) face->flip[1] = 1;
		if(mf.v4) {
			MVert v4;
			dm->getVert(dm, mf.v4, &v4);
			face->edges[2] = BLI_edgehash_lookup(edges, mf.v3, mf.v4);
			if(face->edges[2]->verts[1]->oldIndex == mf.v3) face->flip[2] = 1;
			face->edges[3] = BLI_edgehash_lookup(edges, mf.v4, mf.v1);
			if(face->edges[3]->verts[1]->oldIndex == mf.v4) face->flip[3] = 1;
			normal_quad_v3( face->normal,v1.co, v2.co, v3.co, v4.co);
		} else {
			face->edges[2] = BLI_edgehash_lookup(edges, mf.v3, mf.v1);
			if(face->edges[2]->verts[1]->oldIndex == mf.v3) face->flip[2] = 1;
			face->edges[3] = NULL;
			normal_tri_v3( face->normal,v1.co, v2.co, v3.co);
		}

		for(j = 0; j < SMOOTHFACE_MAX_EDGES && face->edges[j]; j++) {
			SmoothEdge *edge = face->edges[j];
			BLI_linklist_prepend(&edge->faces, face);
			BLI_linklist_prepend(&edge->verts[face->flip[j]]->faces, face);
		}

		face->oldIndex = face->newIndex = i;
	}

	BLI_edgehash_free(edges, NULL);

	return mesh;
}

static DerivedMesh *CDDM_from_smoothmesh(SmoothMesh *mesh)
{
	DerivedMesh *result = CDDM_from_template(mesh->dm,
			mesh->num_verts,
   mesh->num_edges,
   mesh->num_faces);
	MVert *new_verts = CDDM_get_verts(result);
	MEdge *new_edges = CDDM_get_edges(result);
	MFace *new_faces = CDDM_get_faces(result);
	int i;

	for(i = 0; i < mesh->num_verts; ++i) {
		SmoothVert *vert = &mesh->verts[i];
		MVert *newMV = &new_verts[vert->newIndex];

		DM_copy_vert_data(mesh->dm, result,
				  vert->oldIndex, vert->newIndex, 1);
		mesh->dm->getVert(mesh->dm, vert->oldIndex, newMV);
	}

	for(i = 0; i < mesh->num_edges; ++i) {
		SmoothEdge *edge = &mesh->edges[i];
		MEdge *newME = &new_edges[edge->newIndex];

		DM_copy_edge_data(mesh->dm, result,
				  edge->oldIndex, edge->newIndex, 1);
		mesh->dm->getEdge(mesh->dm, edge->oldIndex, newME);
		newME->v1 = edge->verts[0]->newIndex;
		newME->v2 = edge->verts[1]->newIndex;
	}

	for(i = 0; i < mesh->num_faces; ++i) {
		SmoothFace *face = &mesh->faces[i];
		MFace *newMF = &new_faces[face->newIndex];

		DM_copy_face_data(mesh->dm, result,
				  face->oldIndex, face->newIndex, 1);
		mesh->dm->getFace(mesh->dm, face->oldIndex, newMF);

		newMF->v1 = face->edges[0]->verts[face->flip[0]]->newIndex;
		newMF->v2 = face->edges[1]->verts[face->flip[1]]->newIndex;
		newMF->v3 = face->edges[2]->verts[face->flip[2]]->newIndex;

		if(face->edges[3]) {
			newMF->v4 = face->edges[3]->verts[face->flip[3]]->newIndex;
		} else {
			newMF->v4 = 0;
		}
	}

	return result;
}

/* returns the other vert in the given edge
 */
static SmoothVert *other_vert(SmoothEdge *edge, SmoothVert *vert)
{
	if(edge->verts[0] == vert) return edge->verts[1];
	else return edge->verts[0];
}

/* returns the other edge in the given face that uses the given vert
 * returns NULL if no other edge in the given face uses the given vert
 * (this should never happen)
 */
static SmoothEdge *other_edge(SmoothFace *face, SmoothVert *vert,
				  SmoothEdge *edge)
{
	int i,j;
	for(i = 0; i < SMOOTHFACE_MAX_EDGES && face->edges[i]; i++) {
		SmoothEdge *tmp_edge = face->edges[i];
		if(tmp_edge == edge) continue;

		for(j = 0; j < SMOOTHEDGE_NUM_VERTS; j++)
			if(tmp_edge->verts[j] == vert) return tmp_edge;
	}

	/* if we get to here, something's wrong (there should always be 2 edges
	* which use the same vert in a face)
	*/
	return NULL;
}

/* returns a face attached to the given edge which is not the given face.
 * returns NULL if no other faces use this edge.
 */
static SmoothFace *other_face(SmoothEdge *edge, SmoothFace *face)
{
	LinkNode *node;

	for(node = edge->faces; node != NULL; node = node->next)
		if(node->link != face) return node->link;

	return NULL;
}

#if 0
/* copies source list to target, overwriting target (target is not freed)
 * nodes in the copy will be in the same order as in source
 */
static void linklist_copy(LinkNode **target, LinkNode *source)
{
	LinkNode *node = NULL;
	*target = NULL;

	for(; source; source = source->next) {
		if(node) {
			node->next = MEM_mallocN(sizeof(*node->next), "nlink_copy");
										node = node->next;
} else {
										node = *target = MEM_mallocN(sizeof(**target), "nlink_copy");
}
										node->link = source->link;
										node->next = NULL;
}
}
#endif

										/* appends source to target if it's not already in target */
										static void linklist_append_unique(LinkNode **target, void *source) 
{
	LinkNode *node;
	LinkNode *prev = NULL;

	/* check if source value is already in the list */
	for(node = *target; node; prev = node, node = node->next)
		if(node->link == source) return;

	node = MEM_mallocN(sizeof(*node), "nlink");
	node->next = NULL;
	node->link = source;

	if(prev) prev->next = node;
	else *target = node;
}

/* appends elements of source which aren't already in target to target */
static void linklist_append_list_unique(LinkNode **target, LinkNode *source)
{
	for(; source; source = source->next)
		linklist_append_unique(target, source->link);
}

#if 0 /* this is no longer used, it should possibly be removed */
/* prepends prepend to list - doesn't copy nodes, just joins the lists */
static void linklist_prepend_linklist(LinkNode **list, LinkNode *prepend)
{
	if(prepend) {
		LinkNode *node = prepend;
		while(node->next) node = node->next;

		node->next = *list;
		*list = prepend;
}
}
#endif

/* returns 1 if the linked list contains the given pointer, 0 otherwise
 */
static int linklist_contains(LinkNode *list, void *ptr)
{
	LinkNode *node;

	for(node = list; node; node = node->next)
		if(node->link == ptr) return 1;

	return 0;
}

/* returns 1 if the first linked list is a subset of the second (comparing
 * pointer values), 0 if not
 */
static int linklist_subset(LinkNode *list1, LinkNode *list2)
{
	for(; list1; list1 = list1->next)
		if(!linklist_contains(list2, list1->link))
			return 0;

	return 1;
}

#if 0
/* empties the linked list
 * frees pointers with freefunc if freefunc is not NULL
 */
static void linklist_empty(LinkNode **list, LinkNodeFreeFP freefunc)
{
	BLI_linklist_free(*list, freefunc);
	*list = NULL;
}
#endif

/* removes the first instance of value from the linked list
 * frees the pointer with freefunc if freefunc is not NULL
 */
static void linklist_remove_first(LinkNode **list, void *value,
				  LinkNodeFreeFP freefunc)
{
	LinkNode *node = *list;
	LinkNode *prev = NULL;

	while(node && node->link != value) {
		prev = node;
		node = node->next;
	}

	if(node) {
		if(prev)
			prev->next = node->next;
		else
			*list = node->next;

		if(freefunc)
			freefunc(node->link);

		MEM_freeN(node);
	}
}

/* removes all elements in source from target */
static void linklist_remove_list(LinkNode **target, LinkNode *source,
				 LinkNodeFreeFP freefunc)
{
	for(; source; source = source->next)
		linklist_remove_first(target, source->link, freefunc);
}

#ifdef EDGESPLIT_DEBUG_0
static void print_ptr(void *ptr)
{
	printf("%p\n", ptr);
}

static void print_edge(void *ptr)
{
	SmoothEdge *edge = ptr;
	printf(" %4d", edge->newIndex);
}

static void print_face(void *ptr)
{
	SmoothFace *face = ptr;
	printf(" %4d", face->newIndex);
}
#endif

typedef struct ReplaceData {
	void *find;
	void *replace;
} ReplaceData;

static void edge_replace_vert(void *ptr, void *userdata)
{
	SmoothEdge *edge = ptr;
	SmoothVert *find = ((ReplaceData *)userdata)->find;
	SmoothVert *replace = ((ReplaceData *)userdata)->replace;
	int i;

#ifdef EDGESPLIT_DEBUG_3
	printf("replacing vert %4d with %4d in edge %4d",
		   find->newIndex, replace->newIndex, edge->newIndex);
	printf(": {%4d, %4d}", edge->verts[0]->newIndex, edge->verts[1]->newIndex);
#endif

	for(i = 0; i < SMOOTHEDGE_NUM_VERTS; i++) {
		if(edge->verts[i] == find) {
			linklist_append_list_unique(&replace->faces, edge->faces);
			linklist_remove_list(&find->faces, edge->faces, NULL);

			edge->verts[i] = replace;
		}
	}

#ifdef EDGESPLIT_DEBUG_3
	printf(" -> {%4d, %4d}\n", edge->verts[0]->newIndex, edge->verts[1]->newIndex);
#endif
}

static void face_replace_vert(void *ptr, void *userdata)
{
	SmoothFace *face = ptr;
	int i;

	for(i = 0; i < SMOOTHFACE_MAX_EDGES && face->edges[i]; i++)
		edge_replace_vert(face->edges[i], userdata);
}

static void face_replace_edge(void *ptr, void *userdata)
{
	SmoothFace *face = ptr;
	SmoothEdge *find = ((ReplaceData *)userdata)->find;
	SmoothEdge *replace = ((ReplaceData *)userdata)->replace;
	int i;

#ifdef EDGESPLIT_DEBUG_3
	printf("replacing edge %4d with %4d in face %4d",
		   find->newIndex, replace->newIndex, face->newIndex);
	if(face->edges[3])
		printf(": {%2d %2d %2d %2d}",
			   face->edges[0]->newIndex, face->edges[1]->newIndex,
	 face->edges[2]->newIndex, face->edges[3]->newIndex);
	else
		printf(": {%2d %2d %2d}",
			   face->edges[0]->newIndex, face->edges[1]->newIndex,
	 face->edges[2]->newIndex);
#endif

	for(i = 0; i < SMOOTHFACE_MAX_EDGES && face->edges[i]; i++) {
		if(face->edges[i] == find) {
			linklist_remove_first(&face->edges[i]->faces, face, NULL);
			BLI_linklist_prepend(&replace->faces, face);
			face->edges[i] = replace;
		}
	}

#ifdef EDGESPLIT_DEBUG_3
	if(face->edges[3])
		printf(" -> {%2d %2d %2d %2d}\n",
			   face->edges[0]->newIndex, face->edges[1]->newIndex,
	 face->edges[2]->newIndex, face->edges[3]->newIndex);
	else
		printf(" -> {%2d %2d %2d}\n",
			   face->edges[0]->newIndex, face->edges[1]->newIndex,
	 face->edges[2]->newIndex);
#endif
}

static int edge_is_loose(SmoothEdge *edge)
{
	return !(edge->faces && edge->faces->next);
}

static int edge_is_sharp(SmoothEdge *edge, int flags,
			 float threshold)
{
#ifdef EDGESPLIT_DEBUG_1
	printf("edge %d: ", edge->newIndex);
#endif
	if(edge->flag & ME_SHARP) {
		/* edge can only be sharp if it has at least 2 faces */
		if(!edge_is_loose(edge)) {
#ifdef EDGESPLIT_DEBUG_1
			printf("sharp\n");
#endif
			return 1;
		} else {
			/* edge is loose, so it can't be sharp */
			edge->flag &= ~ME_SHARP;
		}
	}

#ifdef EDGESPLIT_DEBUG_1
	printf("not sharp\n");
#endif
	return 0;
}

/* finds another sharp edge which uses vert, by traversing faces around the
 * vert until it does one of the following:
 * - hits a loose edge (the edge is returned)
 * - hits a sharp edge (the edge is returned)
 * - returns to the start edge (NULL is returned)
 */
static SmoothEdge *find_other_sharp_edge(SmoothVert *vert, SmoothEdge *edge,
					 LinkNode **visited_faces, float threshold, int flags)
{
	SmoothFace *face = NULL;
	SmoothEdge *edge2 = NULL;
	/* holds the edges we've seen so we can avoid looping indefinitely */
	LinkNode *visited_edges = NULL;
#ifdef EDGESPLIT_DEBUG_1
	printf("=== START === find_other_sharp_edge(edge = %4d, vert = %4d)\n",
		   edge->newIndex, vert->newIndex);
#endif

	/* get a face on which to start */
	if(edge->faces) face = edge->faces->link;
	else return NULL;

	/* record this edge as visited */
	BLI_linklist_prepend(&visited_edges, edge);

	/* get the next edge */
	edge2 = other_edge(face, vert, edge);

	/* record this face as visited */
	if(visited_faces)
		BLI_linklist_prepend(visited_faces, face);

	/* search until we hit a loose edge or a sharp edge or an edge we've
	* seen before
	*/
	while(face && !edge_is_sharp(edge2, flags, threshold)
			 && !linklist_contains(visited_edges, edge2)) {
#ifdef EDGESPLIT_DEBUG_3
		printf("current face %4d; current edge %4d\n", face->newIndex,
			   edge2->newIndex);
#endif
		/* get the next face */
		face = other_face(edge2, face);

		/* if face == NULL, edge2 is a loose edge */
		if(face) {
			/* record this face as visited */
			if(visited_faces)
				BLI_linklist_prepend(visited_faces, face);

			/* record this edge as visited */
			BLI_linklist_prepend(&visited_edges, edge2);

			/* get the next edge */
			edge2 = other_edge(face, vert, edge2);
#ifdef EDGESPLIT_DEBUG_3
			printf("next face %4d; next edge %4d\n",
				   face->newIndex, edge2->newIndex);
		} else {
			printf("loose edge: %4d\n", edge2->newIndex);
#endif
		}
			 }

			 /* either we came back to the start edge or we found a sharp/loose edge */
			 if(linklist_contains(visited_edges, edge2))
				 /* we came back to the start edge */
				 edge2 = NULL;

			 BLI_linklist_free(visited_edges, NULL);

#ifdef EDGESPLIT_DEBUG_1
			 printf("=== END === find_other_sharp_edge(edge = %4d, vert = %4d), "
					 "returning edge %d\n",
	 edge->newIndex, vert->newIndex, edge2 ? edge2->newIndex : -1);
#endif
			 return edge2;
}

static void split_single_vert(SmoothVert *vert, SmoothFace *face,
				  SmoothMesh *mesh)
{
	SmoothVert *copy_vert;
	ReplaceData repdata;

	copy_vert = smoothvert_copy(vert, mesh);

	repdata.find = vert;
	repdata.replace = copy_vert;
	face_replace_vert(face, &repdata);
}

typedef struct PropagateEdge {
	struct PropagateEdge *next, *prev;
	SmoothEdge *edge;
	SmoothVert *vert;
} PropagateEdge;

static void push_propagate_stack(SmoothEdge *edge, SmoothVert *vert, SmoothMesh *mesh)
{
	PropagateEdge *pedge = mesh->reusestack.first;

	if(pedge) {
		BLI_remlink(&mesh->reusestack, pedge);
	}
	else {
		if(!mesh->arena) {
			mesh->arena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE);
			BLI_memarena_use_calloc(mesh->arena);
		}

		pedge = BLI_memarena_alloc(mesh->arena, sizeof(PropagateEdge));
	}

	pedge->edge = edge;
	pedge->vert = vert;
	BLI_addhead(&mesh->propagatestack, pedge);
}

static void pop_propagate_stack(SmoothEdge **edge, SmoothVert **vert, SmoothMesh *mesh)
{
	PropagateEdge *pedge = mesh->propagatestack.first;

	if(pedge) {
		*edge = pedge->edge;
		*vert = pedge->vert;
		BLI_remlink(&mesh->propagatestack, pedge);
		BLI_addhead(&mesh->reusestack, pedge);
	}
	else {
		*edge = NULL;
		*vert = NULL;
	}
}

static void split_edge(SmoothEdge *edge, SmoothVert *vert, SmoothMesh *mesh);

static void propagate_split(SmoothEdge *edge, SmoothVert *vert,
				SmoothMesh *mesh)
{
	SmoothEdge *edge2;
	LinkNode *visited_faces = NULL;
#ifdef EDGESPLIT_DEBUG_1
	printf("=== START === propagate_split(edge = %4d, vert = %4d)\n",
		   edge->newIndex, vert->newIndex);
#endif

	edge2 = find_other_sharp_edge(vert, edge, &visited_faces,
					  mesh->threshold, mesh->flags);

	if(!edge2) {
		/* didn't find a sharp or loose edge, so we've hit a dead end */
	} else if(!edge_is_loose(edge2)) {
		/* edge2 is not loose, so it must be sharp */
		if(edge_is_loose(edge)) {
			/* edge is loose, so we can split edge2 at this vert */
			split_edge(edge2, vert, mesh);
		} else if(edge_is_sharp(edge, mesh->flags, mesh->threshold)) {
			/* both edges are sharp, so we can split the pair at vert */
			split_edge(edge, vert, mesh);
		} else {
			/* edge is not sharp, so try to split edge2 at its other vert */
			split_edge(edge2, other_vert(edge2, vert), mesh);
		}
	} else { /* edge2 is loose */
		if(edge_is_loose(edge)) {
			SmoothVert *vert2;
			ReplaceData repdata;

			/* can't split edge, what should we do with vert? */
			if(linklist_subset(vert->faces, visited_faces)) {
				/* vert has only one fan of faces attached; don't split it */
			} else {
				/* vert has more than one fan of faces attached; split it */
				vert2 = smoothvert_copy(vert, mesh);

				/* replace vert with its copy in visited_faces */
				repdata.find = vert;
				repdata.replace = vert2;
				BLI_linklist_apply(visited_faces, face_replace_vert, &repdata);
			}
		} else {
			/* edge is not loose, so it must be sharp; split it */
			split_edge(edge, vert, mesh);
		}
	}

	BLI_linklist_free(visited_faces, NULL);
#ifdef EDGESPLIT_DEBUG_1
	printf("=== END === propagate_split(edge = %4d, vert = %4d)\n",
		   edge->newIndex, vert->newIndex);
#endif
}

static void split_edge(SmoothEdge *edge, SmoothVert *vert, SmoothMesh *mesh)
{
	SmoothEdge *edge2;
	SmoothVert *vert2;
	ReplaceData repdata;
	/* the list of faces traversed while looking for a sharp edge */
	LinkNode *visited_faces = NULL;
#ifdef EDGESPLIT_DEBUG_1
	printf("=== START === split_edge(edge = %4d, vert = %4d)\n",
		   edge->newIndex, vert->newIndex);
#endif

	edge2 = find_other_sharp_edge(vert, edge, &visited_faces,
					  mesh->threshold, mesh->flags);

	if(!edge2) {
		/* didn't find a sharp or loose edge, so try the other vert */
		vert2 = other_vert(edge, vert);
		push_propagate_stack(edge, vert2, mesh);
	} else if(!edge_is_loose(edge2)) {
		/* edge2 is not loose, so it must be sharp */
		SmoothEdge *copy_edge = smoothedge_copy(edge, mesh);
		SmoothEdge *copy_edge2 = smoothedge_copy(edge2, mesh);
		SmoothVert *vert2;

		/* replace edge with its copy in visited_faces */
		repdata.find = edge;
		repdata.replace = copy_edge;
		BLI_linklist_apply(visited_faces, face_replace_edge, &repdata);

		/* replace edge2 with its copy in visited_faces */
		repdata.find = edge2;
		repdata.replace = copy_edge2;
		BLI_linklist_apply(visited_faces, face_replace_edge, &repdata);

		vert2 = smoothvert_copy(vert, mesh);

		/* replace vert with its copy in visited_faces (must be done after
		* edge replacement so edges have correct vertices)
		*/
		repdata.find = vert;
		repdata.replace = vert2;
		BLI_linklist_apply(visited_faces, face_replace_vert, &repdata);

		/* all copying and replacing is done; the mesh should be consistent.
		* now propagate the split to the vertices at either end
		*/
		push_propagate_stack(copy_edge, other_vert(copy_edge, vert2), mesh);
		push_propagate_stack(copy_edge2, other_vert(copy_edge2, vert2), mesh);

		if(smoothedge_has_vert(edge, vert))
			push_propagate_stack(edge, vert, mesh);
	} else {
		/* edge2 is loose */
		SmoothEdge *copy_edge = smoothedge_copy(edge, mesh);
		SmoothVert *vert2;

		/* replace edge with its copy in visited_faces */
		repdata.find = edge;
		repdata.replace = copy_edge;
		BLI_linklist_apply(visited_faces, face_replace_edge, &repdata);

		vert2 = smoothvert_copy(vert, mesh);

		/* replace vert with its copy in visited_faces (must be done after
		* edge replacement so edges have correct vertices)
		*/
		repdata.find = vert;
		repdata.replace = vert2;
		BLI_linklist_apply(visited_faces, face_replace_vert, &repdata);

		/* copying and replacing is done; the mesh should be consistent.
		* now propagate the split to the vertex at the other end
		*/
		push_propagate_stack(copy_edge, other_vert(copy_edge, vert2), mesh);

		if(smoothedge_has_vert(edge, vert))
			push_propagate_stack(edge, vert, mesh);
	}

	BLI_linklist_free(visited_faces, NULL);
#ifdef EDGESPLIT_DEBUG_1
	printf("=== END === split_edge(edge = %4d, vert = %4d)\n",
		   edge->newIndex, vert->newIndex);
#endif
}

static void tag_and_count_extra_edges(SmoothMesh *mesh, float split_angle,
					  int flags, int *extra_edges)
{
	/* if normal1 dot normal2 < threshold, angle is greater, so split */
	/* FIXME not sure if this always works */
	/* 0.00001 added for floating-point rounding */
	float threshold = cos((split_angle + 0.00001) * M_PI / 180.0);
	int i;

	*extra_edges = 0;

	/* loop through edges, counting potential new ones */
	for(i = 0; i < mesh->num_edges; i++) {
		SmoothEdge *edge = &mesh->edges[i];
		int sharp = 0;

		/* treat all non-manifold edges (3 or more faces) as sharp */
		if(edge->faces && edge->faces->next && edge->faces->next->next) {
			LinkNode *node;

			/* this edge is sharp */
			sharp = 1;

			/* add an extra edge for every face beyond the first */
			*extra_edges += 2;
			for(node = edge->faces->next->next->next; node; node = node->next)
				(*extra_edges)++;
		} else if((flags & (MOD_EDGESPLIT_FROMANGLE | MOD_EDGESPLIT_FROMFLAG))
					 && !edge_is_loose(edge)) {
			/* (the edge can only be sharp if we're checking angle or flag,
			* and it has at least 2 faces) */

						 /* if we're checking the sharp flag and it's set, good */
						 if((flags & MOD_EDGESPLIT_FROMFLAG) && (edge->flag & ME_SHARP)) {
							 /* this edge is sharp */
							 sharp = 1;

							 (*extra_edges)++;
						 } else if(flags & MOD_EDGESPLIT_FROMANGLE) {
							 /* we know the edge has 2 faces, so check the angle */
							 SmoothFace *face1 = edge->faces->link;
							 SmoothFace *face2 = edge->faces->next->link;
							 float edge_angle_cos = dot_v3v3(face1->normal,
									 face2->normal);

							 if(edge_angle_cos < threshold) {
								 /* this edge is sharp */
								 sharp = 1;

								 (*extra_edges)++;
							 }
						 }
					 }

					 /* set/clear sharp flag appropriately */
					 if(sharp) edge->flag |= ME_SHARP;
					 else edge->flag &= ~ME_SHARP;
	}
}

static void split_sharp_edges(SmoothMesh *mesh, float split_angle, int flags)
{
	SmoothVert *vert;
	int i;
	/* if normal1 dot normal2 < threshold, angle is greater, so split */
	/* FIXME not sure if this always works */
	/* 0.00001 added for floating-point rounding */
	mesh->threshold = cos((split_angle + 0.00001) * M_PI / 180.0);
	mesh->flags = flags;

	/* loop through edges, splitting sharp ones */
	/* can't use an iterator here, because we'll be adding edges */
	for(i = 0; i < mesh->num_edges; i++) {
		SmoothEdge *edge = &mesh->edges[i];

		if(edge_is_sharp(edge, flags, mesh->threshold)) {
			split_edge(edge, edge->verts[0], mesh);

			do {
				pop_propagate_stack(&edge, &vert, mesh);
				if(edge && smoothedge_has_vert(edge, vert))
					propagate_split(edge, vert, mesh);
			} while(edge);
		}
	}
}

static int count_bridge_verts(SmoothMesh *mesh)
{
	int i, j, count = 0;

	for(i = 0; i < mesh->num_faces; i++) {
		SmoothFace *face = &mesh->faces[i];

		for(j = 0; j < SMOOTHFACE_MAX_EDGES && face->edges[j]; j++) {
			SmoothEdge *edge = face->edges[j];
			SmoothEdge *next_edge;
			SmoothVert *vert = edge->verts[1 - face->flip[j]];
			int next = (j + 1) % SMOOTHFACE_MAX_EDGES;

			/* wrap next around if at last edge */
			if(!face->edges[next]) next = 0;

			next_edge = face->edges[next];

			/* if there are other faces sharing this vertex but not
			* these edges, the vertex will be split, so count it
			*/
			/* vert has to have at least one face (this one), so faces != 0 */
			if(!edge->faces->next && !next_edge->faces->next
						 && vert->faces->next) {
				count++;
						 }
		}
	}

	/* each bridge vert will be counted once per face that uses it,
	* so count is too high, but it's ok for now
	*/
	return count;
}

static void split_bridge_verts(SmoothMesh *mesh)
{
	int i,j;

	for(i = 0; i < mesh->num_faces; i++) {
		SmoothFace *face = &mesh->faces[i];

		for(j = 0; j < SMOOTHFACE_MAX_EDGES && face->edges[j]; j++) {
			SmoothEdge *edge = face->edges[j];
			SmoothEdge *next_edge;
			SmoothVert *vert = edge->verts[1 - face->flip[j]];
			int next = (j + 1) % SMOOTHFACE_MAX_EDGES;

			/* wrap next around if at last edge */
			if(!face->edges[next]) next = 0;

			next_edge = face->edges[next];

			/* if there are other faces sharing this vertex but not
			* these edges, split the vertex
			*/
			/* vert has to have at least one face (this one), so faces != 0 */
			if(!edge->faces->next && !next_edge->faces->next
						 && vert->faces->next)
				/* FIXME this needs to find all faces that share edges with
				* this one and split off together
				*/
				split_single_vert(vert, face, mesh);
		}
	}
}

static DerivedMesh *edgesplitModifier_do(EdgeSplitModifierData *emd,
					 Object *ob, DerivedMesh *dm)
{
	SmoothMesh *mesh;
	DerivedMesh *result;
	int max_verts, max_edges;

	if(!(emd->flags & (MOD_EDGESPLIT_FROMANGLE | MOD_EDGESPLIT_FROMFLAG)))
		return dm;

	/* 1. make smoothmesh with initial number of elements */
	mesh = smoothmesh_from_derivedmesh(dm);

	/* 2. count max number of elements to add */
	tag_and_count_extra_edges(mesh, emd->split_angle, emd->flags, &max_edges);
	max_verts = max_edges * 2 + mesh->max_verts;
	max_verts += count_bridge_verts(mesh);
	max_edges += mesh->max_edges;

	/* 3. reallocate smoothmesh arrays & copy elements across */
	/* 4. remap copied elements' pointers to point into the new arrays */
	smoothmesh_resize_verts(mesh, max_verts);
	smoothmesh_resize_edges(mesh, max_edges);

#ifdef EDGESPLIT_DEBUG_1
	printf("********** Pre-split **********\n");
	smoothmesh_print(mesh);
#endif

	split_sharp_edges(mesh, emd->split_angle, emd->flags);
#ifdef EDGESPLIT_DEBUG_1
	printf("********** Post-edge-split **********\n");
	smoothmesh_print(mesh);
#endif

	split_bridge_verts(mesh);

#ifdef EDGESPLIT_DEBUG_1
	printf("********** Post-vert-split **********\n");
	smoothmesh_print(mesh);
#endif

#ifdef EDGESPLIT_DEBUG_0
	printf("Edgesplit: Estimated %d verts & %d edges, "
			"found %d verts & %d edges\n", max_verts, max_edges,
   mesh->num_verts, mesh->num_edges);
#endif

	result = CDDM_from_smoothmesh(mesh);
	smoothmesh_free(mesh);

	return result;
}

static DerivedMesh *applyModifier(
		ModifierData *md, Object *ob, DerivedMesh *derivedData,
  int useRenderParams, int isFinalCalc)
{
	DerivedMesh *result;
	EdgeSplitModifierData *emd = (EdgeSplitModifierData*) md;

	result = edgesplitModifier_do(emd, ob, derivedData);

	if(result != derivedData)
		CDDM_calc_normals(result);

	return result;
}

static DerivedMesh *applyModifierEM(
		ModifierData *md, Object *ob, EditMesh *editData,
  DerivedMesh *derivedData)
{
	return applyModifier(md, ob, derivedData, 0, 1);
}


ModifierTypeInfo modifierType_EdgeSplit = {
	/* name */              "EdgeSplit",
	/* structName */        "EdgeSplitModifierData",
	/* structSize */        sizeof(EdgeSplitModifierData),
	/* type */              eModifierTypeType_Constructive,
	/* flags */             eModifierTypeFlag_AcceptsMesh
							| eModifierTypeFlag_AcceptsCVs
							| eModifierTypeFlag_SupportsMapping
							| eModifierTypeFlag_SupportsEditmode
							| eModifierTypeFlag_EnableInEditmode,

	/* copyData */          copyData,
	/* deformVerts */       0,
	/* deformVertsEM */     0,
	/* deformMatricesEM */  0,
	/* applyModifier */     applyModifier,
	/* applyModifierEM */   applyModifierEM,
	/* initData */          initData,
	/* requiredDataMask */  0,
	/* freeData */          0,
	/* isDisabled */        0,
	/* updateDepgraph */    0,
	/* dependsOnTime */     0,
	/* foreachObjectLink */ 0,
	/* foreachIDLink */     0,
};
