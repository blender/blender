/*
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
 * Contributor(s): Nicholas Bishop
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/bmesh/operators/bmo_hull.c
 *  \ingroup bmesh
 *
 * Create a convex hull using bullet physics library.
 */

#ifdef WITH_BULLET

#include "MEM_guardedalloc.h"

#include "BLI_array.h"
#include "BLI_listbase.h"
#include "BLI_math.h"

#include "Bullet-C-Api.h"

/* XXX: using 128 for totelem and pchunk of mempool, no idea what good
 * values would be though */

#include "bmesh.h"

#include "intern/bmesh_operators_private.h"  /* own include */

/* Internal operator flags */
typedef enum {
	HULL_FLAG_INPUT =           (1 << 0),
	
	HULL_FLAG_INTERIOR_ELE =    (1 << 1),
	HULL_FLAG_OUTPUT_GEOM =     (1 << 2),
	
	HULL_FLAG_DEL =             (1 << 3),
	HULL_FLAG_HOLE =            (1 << 4)
} HullFlags;

/* Store hull triangles separate from BMesh faces until the end; this
 * way we don't have to worry about cleaning up extraneous edges or
 * incorrectly deleting existing geometry. */
typedef struct HullTriangle {
	BMVert *v[3];
	float no[3];
	int skip;
} HullTriangle;



/*************************** Hull Triangles ***************************/

static void hull_add_triangle(BMesh *bm, GSet *hull_triangles, BLI_mempool *pool,
                              BMVert *v1, BMVert *v2, BMVert *v3)
{
	HullTriangle *t;
	int i;

	t = BLI_mempool_calloc(pool);
	t->v[0] = v1;
	t->v[1] = v2;
	t->v[2] = v3;

	/* Mark triangles vertices as not interior */
	for (i = 0; i < 3; i++)
		BMO_elem_flag_disable(bm, t->v[i], HULL_FLAG_INTERIOR_ELE);

	BLI_gset_insert(hull_triangles, t);
	normal_tri_v3(t->no, v1->co, v2->co, v3->co);
}

static BMFace *hull_find_example_face(BMesh *bm, BMEdge *e)
{
	BMIter iter;
	BMFace *f;

	BM_ITER_ELEM (f, &iter, e, BM_FACES_OF_EDGE) {
		if (BMO_elem_flag_test(bm, f, HULL_FLAG_INPUT) ||
		    !BMO_elem_flag_test(bm, f, HULL_FLAG_OUTPUT_GEOM))
		{
			return f;
		}
	}

	return NULL;
}

static void hull_output_triangles(BMesh *bm, GSet *hull_triangles)
{
	GSetIterator iter;
	
	GSET_ITER (iter, hull_triangles) {
		HullTriangle *t = BLI_gsetIterator_getKey(&iter);
		int i;

		if (!t->skip) {
			BMEdge *edges[3] = {
				BM_edge_create(bm, t->v[0], t->v[1], NULL, BM_CREATE_NO_DOUBLE),
				BM_edge_create(bm, t->v[1], t->v[2], NULL, BM_CREATE_NO_DOUBLE),
				BM_edge_create(bm, t->v[2], t->v[0], NULL, BM_CREATE_NO_DOUBLE)
			};
			BMFace *f, *example = NULL;

			if (BM_face_exists(t->v, 3, &f)) {
				/* If the operator is run with "use_existing_faces"
				 * disabled, but an output face in the hull is the
				 * same as a face in the existing mesh, it should not
				 * be marked as unused or interior. */
				BMO_elem_flag_enable(bm, f, HULL_FLAG_OUTPUT_GEOM);
				BMO_elem_flag_disable(bm, f, HULL_FLAG_HOLE);
				BMO_elem_flag_disable(bm, f, HULL_FLAG_INTERIOR_ELE);
			}
			else {
				/* Look for an adjacent face that existed before the hull */
				for (i = 0; i < 3; i++) {
					if (!example)
						example = hull_find_example_face(bm, edges[i]);
				}

				/* Create new hull face */
				f = BM_face_create_verts(bm, t->v, 3, example, BM_CREATE_NO_DOUBLE, true);
				BM_face_copy_shared(bm, f, NULL, NULL);
			}
			/* Mark face for 'geom.out' slot and select */
			BMO_elem_flag_enable(bm, f, HULL_FLAG_OUTPUT_GEOM);
			BM_face_select_set(bm, f, true);

			/* Mark edges for 'geom.out' slot */
			for (i = 0; i < 3; i++) {
				BMO_elem_flag_enable(bm, edges[i], HULL_FLAG_OUTPUT_GEOM);
			}
		}
		else {
			/* Mark input edges for 'geom.out' slot */
			for (i = 0; i < 3; i++) {
				const int next = (i == 2 ? 0 : i + 1);
				BMEdge *e = BM_edge_exists(t->v[i], t->v[next]);
				if (e &&
				    BMO_elem_flag_test(bm, e, HULL_FLAG_INPUT) &&
				    !BMO_elem_flag_test(bm, e, HULL_FLAG_HOLE))
				{
					BMO_elem_flag_enable(bm, e, HULL_FLAG_OUTPUT_GEOM);
				}
			}
		}

		/* Mark verts for 'geom.out' slot */
		for (i = 0; i < 3; i++) {
			BMO_elem_flag_enable(bm, t->v[i], HULL_FLAG_OUTPUT_GEOM);
		}
	}
}



/***************************** Final Edges ****************************/

typedef struct {
	GHash *edges;
	BLI_mempool *base_pool, *link_pool;
} HullFinalEdges;

static LinkData *final_edges_find_link(ListBase *adj, BMVert *v)
{
	LinkData *link;

	for (link = adj->first; link; link = link->next) {
		if (link->data == v)
			return link;
	}

	return NULL;
}

static int hull_final_edges_lookup(HullFinalEdges *final_edges,
                                   BMVert *v1, BMVert *v2)
{
	ListBase *adj;

	/* Use lower vertex pointer for hash key */
	if (v1 > v2)
		SWAP(BMVert *, v1, v2);

	adj = BLI_ghash_lookup(final_edges->edges, v1);
	if (!adj)
		return false;

	return !!final_edges_find_link(adj, v2);
}

/* Used for checking whether a pre-existing edge lies on the hull */
static HullFinalEdges *hull_final_edges(GSet *hull_triangles)
{
	HullFinalEdges *final_edges;
	GSetIterator iter;
	
	final_edges = MEM_callocN(sizeof(HullFinalEdges), "HullFinalEdges");
	final_edges->edges = BLI_ghash_ptr_new("final edges ghash");
	final_edges->base_pool = BLI_mempool_create(sizeof(ListBase), 0, 128, BLI_MEMPOOL_NOP);
	final_edges->link_pool = BLI_mempool_create(sizeof(LinkData), 0, 128, BLI_MEMPOOL_NOP);

	GSET_ITER (iter, hull_triangles) {
		HullTriangle *t = BLI_gsetIterator_getKey(&iter);
		LinkData *link;
		int i;

		for (i = 0; i < 3; i++) {
			BMVert *v1 = t->v[i];
			BMVert *v2 = t->v[(i + 1) % 3];
			ListBase *adj;

			/* Use lower vertex pointer for hash key */
			if (v1 > v2)
				SWAP(BMVert *, v1, v2);

			adj = BLI_ghash_lookup(final_edges->edges, v1);
			if (!adj) {
				adj = BLI_mempool_calloc(final_edges->base_pool);
				BLI_ghash_insert(final_edges->edges, v1, adj);
			}

			if (!final_edges_find_link(adj, v2)) {
				link = BLI_mempool_calloc(final_edges->link_pool);
				link->data = v2;
				BLI_addtail(adj, link);
			}
		}
	}

	return final_edges;
}

static void hull_final_edges_free(HullFinalEdges *final_edges)
{
	BLI_ghash_free(final_edges->edges, NULL, NULL);
	BLI_mempool_destroy(final_edges->base_pool);
	BLI_mempool_destroy(final_edges->link_pool);
	MEM_freeN(final_edges);
}



/**************************** Final Output ****************************/

static void hull_remove_overlapping(BMesh *bm, GSet *hull_triangles,
                                    HullFinalEdges *final_edges)
{
	GSetIterator hull_iter;

	GSET_ITER (hull_iter, hull_triangles) {
		HullTriangle *t = BLI_gsetIterator_getKey(&hull_iter);
		BMIter bm_iter1, bm_iter2;
		BMFace *f;
		bool f_on_hull;

		BM_ITER_ELEM (f, &bm_iter1, t->v[0], BM_FACES_OF_VERT) {
			BMEdge *e;

			/* Check that all the face's edges are on the hull,
			 * otherwise can't reuse it */
			f_on_hull = true;
			BM_ITER_ELEM (e, &bm_iter2, f, BM_EDGES_OF_FACE) {
				if (!hull_final_edges_lookup(final_edges, e->v1, e->v2)) {
					f_on_hull = false;
					break;
				}
			}
			
			/* Note: can't change ghash while iterating, so mark
			 * with 'skip' flag rather than deleting triangles */
			if (BM_vert_in_face(f, t->v[1]) &&
			    BM_vert_in_face(f, t->v[2]) && f_on_hull)
			{
				t->skip = true;
				BMO_elem_flag_disable(bm, f, HULL_FLAG_INTERIOR_ELE);
				BMO_elem_flag_enable(bm, f, HULL_FLAG_HOLE);
			}
		}
	}
}

static void hull_mark_interior_elements(BMesh *bm, BMOperator *op,
                                        HullFinalEdges *final_edges)
{
	BMEdge *e;
	BMFace *f;
	BMOIter oiter;

	/* Check for interior edges too */
	BMO_ITER (e, &oiter, op->slots_in, "input", BM_EDGE) {
		if (!hull_final_edges_lookup(final_edges, e->v1, e->v2))
			BMO_elem_flag_enable(bm, e, HULL_FLAG_INTERIOR_ELE);
	}

	/* Mark all input faces as interior, some may be unmarked in
	 * hull_remove_overlapping() */
	BMO_ITER (f, &oiter, op->slots_in, "input", BM_FACE) {
		BMO_elem_flag_enable(bm, f, HULL_FLAG_INTERIOR_ELE);
	}
}

static void hull_tag_unused(BMesh *bm, BMOperator *op)
{
	BMIter iter;
	BMOIter oiter;
	BMVert *v;
	BMEdge *e;
	BMFace *f;

	/* Mark vertices, edges, and faces that are already marked
	 * interior (i.e. were already part of the input, but not part of
	 * the hull), but that aren't also used by elements outside the
	 * input set */
	BMO_ITER (v, &oiter, op->slots_in, "input", BM_VERT) {
		if (BMO_elem_flag_test(bm, v, HULL_FLAG_INTERIOR_ELE)) {
			bool del = true;
		
			BM_ITER_ELEM (e, &iter, v, BM_EDGES_OF_VERT) {
				if (!BMO_elem_flag_test(bm, e, HULL_FLAG_INPUT)) {
					del = false;
					break;
				}
			}

			BM_ITER_ELEM (f, &iter, v, BM_FACES_OF_VERT) {
				if (!BMO_elem_flag_test(bm, f, HULL_FLAG_INPUT)) {
					del = false;
					break;
				}
			}

			if (del)
				BMO_elem_flag_enable(bm, v, HULL_FLAG_DEL);
		}
	}

	BMO_ITER (e, &oiter, op->slots_in, "input", BM_EDGE) {
		if (BMO_elem_flag_test(bm, e, HULL_FLAG_INTERIOR_ELE)) {
			bool del = true;

			BM_ITER_ELEM (f, &iter, e, BM_FACES_OF_EDGE) {
				if (!BMO_elem_flag_test(bm, f, HULL_FLAG_INPUT)) {
					del = false;
					break;
				}
			}

			if (del)
				BMO_elem_flag_enable(bm, e, HULL_FLAG_DEL);
		}
	}

	BMO_ITER (f, &oiter, op->slots_in, "input", BM_FACE) {
		if (BMO_elem_flag_test(bm, f, HULL_FLAG_INTERIOR_ELE))
			BMO_elem_flag_enable(bm, f, HULL_FLAG_DEL);
	}
}

static void hull_tag_holes(BMesh *bm, BMOperator *op)
{
	BMIter iter;
	BMOIter oiter;
	BMFace *f;
	BMEdge *e;

	/* Unmark any hole faces if they are isolated or part of a
	 * border */
	BMO_ITER (f, &oiter, op->slots_in, "input", BM_FACE) {
		if (BMO_elem_flag_test(bm, f, HULL_FLAG_HOLE)) {
			BM_ITER_ELEM (e, &iter, f, BM_EDGES_OF_FACE) {
				if (BM_edge_is_boundary(e)) {
					BMO_elem_flag_disable(bm, f, HULL_FLAG_HOLE);
					break;
				}
			}
		}
	}

	/* Mark edges too if all adjacent faces are holes and the edge is
	 * not already isolated */
	BMO_ITER (e, &oiter, op->slots_in, "input", BM_EDGE) {
		bool hole = true;
		bool any_faces = false;
		
		BM_ITER_ELEM (f, &iter, e, BM_FACES_OF_EDGE) {
			any_faces = true;
			if (!BMO_elem_flag_test(bm, f, HULL_FLAG_HOLE)) {
				hole = false;
				break;
			}
		}

		if (hole && any_faces)
			BMO_elem_flag_enable(bm, e, HULL_FLAG_HOLE);
	}
}

static int hull_input_vert_count(BMOperator *op)
{
	BMOIter oiter;
	BMVert *v;
	int count = 0;

	BMO_ITER (v, &oiter, op->slots_in, "input", BM_VERT) {
		count++;
	}

	return count;
}

static BMVert **hull_input_verts_copy(BMOperator *op,
                                      const int num_input_verts)
{
	BMOIter oiter;
	BMVert *v;
	BMVert **input_verts = MEM_callocN(sizeof(*input_verts) *
	                                   num_input_verts, AT);
	int i = 0;

	BMO_ITER (v, &oiter, op->slots_in, "input", BM_VERT) {
		input_verts[i++] = v;
	}

	return input_verts;
}

static float (*hull_verts_for_bullet(BMVert **input_verts,
                                     const int num_input_verts))[3]
{
	float (*coords)[3] = MEM_callocN(sizeof(*coords) * num_input_verts, __func__);
	int i;

	for (i = 0; i < num_input_verts; i++) {
		copy_v3_v3(coords[i], input_verts[i]->co);
	}

	return coords;
}

static BMVert **hull_verts_from_bullet(plConvexHull hull,
                                       BMVert **input_verts,
                                       const int num_input_verts)
{
	const int num_verts = plConvexHullNumVertices(hull);
	BMVert **hull_verts = MEM_mallocN(sizeof(*hull_verts) *
	                                  num_verts, AT);
	int i;

	for (i = 0; i < num_verts; i++) {
		float co[3];
		int original_index;
		plConvexHullGetVertex(hull, i, co, &original_index);

		if (original_index >= 0 && original_index < num_input_verts) {
			hull_verts[i] = input_verts[original_index];
		}
		else
			BLI_assert(!"Unexpected new vertex in hull output");
	}

	return hull_verts;
}

static void hull_from_bullet(BMesh *bm, BMOperator *op,
                             GSet *hull_triangles,
                             BLI_mempool *pool)
{
	int *fvi = NULL;
	BLI_array_declare(fvi);

	BMVert **input_verts;
	float (*coords)[3];
	BMVert **hull_verts;

	plConvexHull hull;
	int i, count = 0;

	const int num_input_verts = hull_input_vert_count(op);

	input_verts = hull_input_verts_copy(op, num_input_verts);
	coords = hull_verts_for_bullet(input_verts, num_input_verts);

	hull = plConvexHullCompute(coords, num_input_verts);
	hull_verts = hull_verts_from_bullet(hull, input_verts, num_input_verts);
	
	count = plConvexHullNumFaces(hull);
	for (i = 0; i < count; i++) {
		const int len = plConvexHullGetFaceSize(hull, i);

		if (len > 2) {
			BMVert *fv[3];
			int j;

			/* Get face vertex indices */
			BLI_array_empty(fvi);
			BLI_array_grow_items(fvi, len);
			plConvexHullGetFaceVertices(hull, i, fvi);

			/* Note: here we throw away any NGons from Bullet and turn
			 * them into triangle fans. Would be nice to use these
			 * directly, but will have to wait until HullTriangle goes
			 * away (TODO) */
			fv[0] = hull_verts[fvi[0]];
			for (j = 2; j < len; j++) {
				fv[1] = hull_verts[fvi[j - 1]];
				fv[2] = hull_verts[fvi[j]];

				hull_add_triangle(bm, hull_triangles, pool,
				                  fv[0], fv[1], fv[2]);
			}
		}
	}

	BLI_array_free(fvi);
	MEM_freeN(hull_verts);
	MEM_freeN(coords);
	MEM_freeN(input_verts);
}

/* Check that there are at least three vertices in the input */
static bool hull_num_input_verts_is_ok(BMOperator *op)
{
	BMOIter oiter;
	BMVert *v;
	int partial_num_verts = 0;

	BMO_ITER (v, &oiter, op->slots_in, "input", BM_VERT) {
		partial_num_verts++;
		if (partial_num_verts >= 3)
			break;
	}

	return (partial_num_verts >= 3);
}

void bmo_convex_hull_exec(BMesh *bm, BMOperator *op)
{
	HullFinalEdges *final_edges;
	BLI_mempool *hull_pool;
	BMElemF *ele;
	BMOIter oiter;
	GSet *hull_triangles;

	/* Verify that at least three verts in the input */
	if (!hull_num_input_verts_is_ok(op)) {
		BMO_error_raise(bm, op, BMERR_CONVEX_HULL_FAILED,
		                "Requires at least three vertices");
		return;
	}

	/* Tag input elements */
	BMO_ITER (ele, &oiter, op->slots_in, "input", BM_ALL) {
		BMO_elem_flag_enable(bm, ele, HULL_FLAG_INPUT);
		
		/* Mark all vertices as interior to begin with */
		if (ele->head.htype == BM_VERT)
			BMO_elem_flag_enable(bm, ele, HULL_FLAG_INTERIOR_ELE);
	}

	hull_pool = BLI_mempool_create(sizeof(HullTriangle), 0, 128, BLI_MEMPOOL_NOP);
	hull_triangles = BLI_gset_ptr_new("hull_triangles");

	hull_from_bullet(bm, op, hull_triangles, hull_pool);

	final_edges = hull_final_edges(hull_triangles);
	
	hull_mark_interior_elements(bm, op, final_edges);

	/* Remove hull triangles covered by an existing face */
	if (BMO_slot_bool_get(op->slots_in, "use_existing_faces")) {
		hull_remove_overlapping(bm, hull_triangles, final_edges);

		hull_tag_holes(bm, op);
	}

	/* Done with edges */
	hull_final_edges_free(final_edges);

	/* Convert hull triangles to BMesh faces */
	hull_output_triangles(bm, hull_triangles);
	BLI_mempool_destroy(hull_pool);

	BLI_gset_free(hull_triangles, NULL);

	hull_tag_unused(bm, op);

	/* Output slot of input elements that ended up inside the hull
	 * rather than part of it */
	BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "geom_interior.out",
	                                  BM_ALL_NOLOOP, HULL_FLAG_INTERIOR_ELE);

	/* Output slot of input elements that ended up inside the hull and
	 * are are unused by other geometry. */
	BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "geom_unused.out",
	                                  BM_ALL_NOLOOP, HULL_FLAG_DEL);

	/* Output slot of faces and edges that were in the input and on
	 * the hull (useful for cases like bridging where you want to
	 * delete some input geometry) */
	BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "geom_holes.out",
	                                  BM_ALL_NOLOOP, HULL_FLAG_HOLE);

	/* Output slot of all hull vertices, faces, and edges */
	BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "geom.out",
	                                  BM_ALL_NOLOOP, HULL_FLAG_OUTPUT_GEOM);
}

#endif  /* WITH_BULLET */
