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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2011 Blender Foundation.
 * All rights reserved.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_utildefines.h"
#include "BLI_edgehash.h"

#include "BKE_DerivedMesh.h"

#include "MEM_guardedalloc.h"

#include "ED_mesh.h"

typedef struct SearchFace {
	unsigned int v[4];
	unsigned int index;
} SearchFace;

static int uint_cmp(const void *v1, const void *v2)
{
	const unsigned int x1= GET_INT_FROM_POINTER(v1), x2= GET_INT_FROM_POINTER(v2);

	if( x1 > x2 ) return 1;
	else if( x1 < x2 ) return -1;
	return 0;
}

static int search_face_cmp(const void *v1, const void *v2)
{
	const SearchFace *sfa= v1, *sfb= v2;

	if		(sfa->v[0] > sfb->v[0]) return 1;
	else if	(sfa->v[0] < sfb->v[0]) return -1;

	if		(sfa->v[1] > sfb->v[1]) return 1;
	else if	(sfa->v[1] < sfb->v[1]) return -1;

	if		(sfa->v[2] > sfb->v[2]) return 1;
	else if	(sfa->v[2] < sfb->v[2]) return -1;

	if		(sfa->v[3] > sfb->v[3]) return 1;
	else if	(sfa->v[3] < sfb->v[3]) return -1;

	return 0;
}

void BKE_mesh_validate_arrays(MVert *UNUSED(mverts), int totvert, MEdge *medges, int totedge, MFace *mfaces, int totface)
{
//	MVert *mv;
	MEdge *med;
	MFace *mf;
	int i;

	EdgeHash *edge_hash = BLI_edgehash_new();

	SearchFace *search_faces= MEM_callocN(sizeof(SearchFace) * totface, "search faces");
	SearchFace *sf;
	SearchFace *sf_prev;

	printf("ED_mesh_validate: verts(%d), edges(%d), faces(%d)\n", totvert, totedge, totface);

	if(totedge==0 && totface != 0) {
		printf("    locical error, %d faces and 0 edges\n", totface);
	}

	for(i=0, med=medges; i<totedge; i++, med++) {
		if(med->v1 == med->v2) {
			printf("    edge %d: has matching verts, both %d\n", i, med->v1);
		}
		if(med->v1 < 0 || med->v1 >= totvert) {
			printf("    edge %d: v1 index out of range, %d\n", i, med->v1);
		}
		if(med->v2 < 0 || med->v2 >= totvert) {
			printf("    edge %d: v2 index out of range, %d\n", i, med->v2);
		}

		if(BLI_edgehash_haskey(edge_hash, med->v1, med->v2)) {
			printf("    edge %d: is a duplicate of, %d\n", i, GET_INT_FROM_POINTER(BLI_edgehash_lookup(edge_hash, med->v1, med->v2)));
		}

		BLI_edgehash_insert(edge_hash, med->v1, med->v2, SET_INT_IN_POINTER(i));
	}

	for(i=0, mf=mfaces; i<totface; i++, mf++) {
		unsigned int fverts[4];
		// unsigned int fedges[4];
		int fidx;

		fidx = mf->v4 ? 3:2;
		do {
			fverts[fidx]= *(&mf->v1 + fidx);
			if(fverts[fidx] < 0 || fverts[fidx] >= totvert) {
				printf("    face %d: 'v%d' index out of range, %d\n", i, fidx + 1, fverts[fidx]);
			}
		} while (fidx--);

		if(mf->v4) {
			if(mf->v1 == mf->v2) printf("    face %d: verts invalid, v1/v2 both %d\n", i, mf->v1);
			if(mf->v1 == mf->v3) printf("    face %d: verts invalid, v1/v3 both %d\n", i, mf->v1);
			if(mf->v1 == mf->v4) printf("    face %d: verts invalid, v1/v4 both %d\n", i, mf->v1);

			if(mf->v2 == mf->v3) printf("    face %d: verts invalid, v2/v3 both %d\n", i, mf->v2);
			if(mf->v2 == mf->v4) printf("    face %d: verts invalid, v2/v4 both %d\n", i, mf->v2);

			if(mf->v3 == mf->v4) printf("    face %d: verts invalid, v3/v4 both %d\n", i, mf->v3);

			if(totedge) {
				if(!BLI_edgehash_haskey(edge_hash, mf->v1, mf->v2)) printf("    face %d: edge v1/v2 (%d,%d) is missing egde data\n", i, mf->v1, mf->v2);
				if(!BLI_edgehash_haskey(edge_hash, mf->v2, mf->v3)) printf("    face %d: edge v2/v3 (%d,%d) is missing egde data\n", i, mf->v2, mf->v3);
				if(!BLI_edgehash_haskey(edge_hash, mf->v3, mf->v4)) printf("    face %d: edge v3/v4 (%d,%d) is missing egde data\n", i, mf->v3, mf->v4);
				if(!BLI_edgehash_haskey(edge_hash, mf->v4, mf->v1)) printf("    face %d: edge v4/v1 (%d,%d) is missing egde data\n", i, mf->v4, mf->v1);
			}
			/* TODO, avoid double lookop */
			/*
			fedges[0]= GET_INT_FROM_POINTER(BLI_edgehash_lookup(edge_hash, mf->v1, mf->v2));
			fedges[1]= GET_INT_FROM_POINTER(BLI_edgehash_lookup(edge_hash, mf->v2, mf->v3));
			fedges[2]= GET_INT_FROM_POINTER(BLI_edgehash_lookup(edge_hash, mf->v3, mf->v4));
			fedges[3]= GET_INT_FROM_POINTER(BLI_edgehash_lookup(edge_hash, mf->v4, mf->v1));
			*/
			qsort(fverts, 4, sizeof(int), uint_cmp);
		}
		else {
			if(mf->v1 == mf->v2) printf("    face %d: verts invalid, v1/v2 both %d\n", i, mf->v1);
			if(mf->v1 == mf->v3) printf("    face %d: verts invalid, v1/v3 both %d\n", i, mf->v1);

			if(mf->v2 == mf->v3) printf("    face %d: verts invalid, v2/v3 both %d\n", i, mf->v2);

			if(totedge) {
				if(!BLI_edgehash_haskey(edge_hash, mf->v1, mf->v2)) printf("    face %d: edge v1/v2 (%d,%d) is missing egde data\n", i, mf->v1, mf->v2);
				if(!BLI_edgehash_haskey(edge_hash, mf->v2, mf->v3)) printf("    face %d: edge v2/v3 (%d,%d) is missing egde data\n", i, mf->v2, mf->v3);
				if(!BLI_edgehash_haskey(edge_hash, mf->v3, mf->v1)) printf("    face %d: edge v3/v1 (%d,%d) is missing egde data\n", i, mf->v3, mf->v1);
			}
			/* TODO, avoid double lookop */
			/*
			fedges[0]= GET_INT_FROM_POINTER(BLI_edgehash_lookup(edge_hash, mf->v1, mf->v2));
			fedges[1]= GET_INT_FROM_POINTER(BLI_edgehash_lookup(edge_hash, mf->v2, mf->v3));
			fedges[2]= GET_INT_FROM_POINTER(BLI_edgehash_lookup(edge_hash, mf->v3, mf->v1));
			*/
			qsort(fverts, 3, sizeof(int), uint_cmp);
		}

		search_faces[i].index = i;

		if(mf->v4) {
			search_faces[i].v[0] = fverts[0];
			search_faces[i].v[1] = fverts[1];
			search_faces[i].v[2] = fverts[2];
			search_faces[i].v[3] = fverts[3];
		}
		else {
			search_faces[i].v[0] = fverts[0];
			search_faces[i].v[1] = fverts[1];
			search_faces[i].v[2] = fverts[2];
			search_faces[i].v[3] = UINT_MAX;
		}
	}

	qsort(search_faces, totface, sizeof(SearchFace), search_face_cmp);

	sf= search_faces;
	sf_prev= sf;
	sf++;

	for(i=1; i<totface; i++, sf++, sf_prev++) {
		/* on a valid mesh, code below will never run */
		if(memcmp(sf->v, sf_prev->v, sizeof(sf_prev->v)) == 0) {
			/* slow, could be smarter here */
			MFace *mf= mfaces + sf->index;
			MFace *mf_prev= mfaces + sf_prev->index;
			int size_expect, size_found;

			EdgeHash *eh_tmp= BLI_edgehash_new();
			if(mf->v4) {
				BLI_edgehash_insert(eh_tmp, mf->v1, mf->v2, NULL);
				BLI_edgehash_insert(eh_tmp, mf->v2, mf->v3, NULL);
				BLI_edgehash_insert(eh_tmp, mf->v3, mf->v4, NULL);
				BLI_edgehash_insert(eh_tmp, mf->v4, mf->v1, NULL);

				BLI_edgehash_insert(eh_tmp, mf_prev->v1, mf_prev->v2, NULL);
				BLI_edgehash_insert(eh_tmp, mf_prev->v2, mf_prev->v3, NULL);
				BLI_edgehash_insert(eh_tmp, mf_prev->v3, mf_prev->v4, NULL);
				BLI_edgehash_insert(eh_tmp, mf_prev->v4, mf_prev->v1, NULL);

				size_expect= 4;
			}
			else {
				BLI_edgehash_insert(eh_tmp, mf->v1, mf->v2, NULL);
				BLI_edgehash_insert(eh_tmp, mf->v2, mf->v3, NULL);
				BLI_edgehash_insert(eh_tmp, mf->v3, mf->v1, NULL);

				BLI_edgehash_insert(eh_tmp, mf_prev->v1, mf_prev->v2, NULL);
				BLI_edgehash_insert(eh_tmp, mf_prev->v2, mf_prev->v3, NULL);
				BLI_edgehash_insert(eh_tmp, mf_prev->v3, mf_prev->v1, NULL);

				size_expect= 3;
			}

			size_found= BLI_edgehash_size(eh_tmp);
			BLI_edgehash_free(eh_tmp, NULL);

			if(size_found != size_expect) {
				printf("    face %d & %d: are duplicates ", sf->index, sf_prev->index);
				if(mf->v4) {
					printf("(%d,%d,%d,%d) ", mf->v1, mf->v2, mf->v3, mf->v4);
					printf("(%d,%d,%d,%d)\n", mf_prev->v1, mf_prev->v2, mf_prev->v3, mf_prev->v4);
				}
				else {
					printf("(%d,%d,%d) ", mf->v1, mf->v2, mf->v3);
					printf("(%d,%d,%d)\n", mf_prev->v1, mf_prev->v2, mf_prev->v3);
				}
			}
		}
	}

	BLI_edgehash_free(edge_hash, NULL);
	MEM_freeN(search_faces);

	printf("BKE_mesh_validate: finished\n\n");
}

void BKE_mesh_validate(Mesh *me)
{
	printf("MESH: %s\n", me->id.name+2);
	BKE_mesh_validate_arrays(me->mvert, me->totvert, me->medge, me->totedge, me->mface, me->totface);
}

void BKE_mesh_validate_dm(DerivedMesh *dm)
{
	BKE_mesh_validate_arrays(dm->getVertArray(dm), dm->getNumVerts(dm), dm->getEdgeArray(dm), dm->getNumEdges(dm), dm->getFaceArray(dm), dm->getNumFaces(dm));
}
