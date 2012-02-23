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
 * Contributor(s): Joseph Eagar.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "MEM_guardedalloc.h"


#include "BLI_array.h"
#include "BLI_math.h"

#include "bmesh.h"

/* local flag define */
#define DUPE_INPUT		1 /* input from operato */
#define DUPE_NEW		2
#define DUPE_DONE		4
#define DUPE_MAPPED		8

/*
 *  COPY VERTEX
 *
 *   Copy an existing vertex from one bmesh to another.
 *
 */
static BMVert *copy_vertex(BMesh *source_mesh, BMVert *source_vertex, BMesh *target_mesh, GHash *vhash)
{
	BMVert *target_vertex = NULL;

	/* Create a new verte */
	target_vertex = BM_vert_create(target_mesh, source_vertex->co,  NULL);
	
	/* Insert new vertex into the vert has */
	BLI_ghash_insert(vhash, source_vertex, target_vertex);
	
	/* Copy attribute */
	BM_elem_attrs_copy(source_mesh, target_mesh, source_vertex, target_vertex);
	
	/* Set internal op flag */
	BMO_elem_flag_enable(target_mesh, target_vertex, DUPE_NEW);
	
	return target_vertex;
}

/*
 * COPY EDGE
 *
 * Copy an existing edge from one bmesh to another.
 *
 */
static BMEdge *copy_edge(BMOperator *op, BMesh *source_mesh,
                         BMEdge *source_edge, BMesh *target_mesh,
                         GHash *vhash, GHash *ehash)
{
	BMEdge *target_edge = NULL;
	BMVert *target_vert1, *target_vert2;
	BMFace *face;
	BMIter fiter;
	int rlen;

	/* see if any of the neighboring faces are
	 * not being duplicated.  in that case,
	 * add it to the new/old map. */
	rlen = 0;
	for (face = BM_iter_new(&fiter, source_mesh, BM_FACES_OF_EDGE, source_edge);
	     face;
	     face = BM_iter_step(&fiter))
	{
		if (BMO_elem_flag_test(source_mesh, face, DUPE_INPUT)) {
			rlen++;
		}
	}

	/* Lookup v1 and v2 */
	target_vert1 = BLI_ghash_lookup(vhash, source_edge->v1);
	target_vert2 = BLI_ghash_lookup(vhash, source_edge->v2);
	
	/* Create a new edg */
	target_edge = BM_edge_create(target_mesh, target_vert1, target_vert2, NULL, FALSE);
	
	/* add to new/old edge map if necassar */
	if (rlen < 2) {
		/* not sure what non-manifold cases of greater then three
		 * radial should do. */
		BMO_slot_map_ptr_insert(source_mesh, op, "boundarymap",
		                        source_edge, target_edge);
	}

	/* Insert new edge into the edge hash */
	BLI_ghash_insert(ehash, source_edge, target_edge);
	
	/* Copy attributes */
	BM_elem_attrs_copy(source_mesh, target_mesh, source_edge, target_edge);
	
	/* Set internal op flags */
	BMO_elem_flag_enable(target_mesh, target_edge, DUPE_NEW);
	
	return target_edge;
}

/*
 * COPY FACE
 *
 *  Copy an existing face from one bmesh to another.
 */

static BMFace *copy_face(BMOperator *op, BMesh *source_mesh,
                         BMFace *source_face, BMesh *target_mesh,
                         BMVert **vtar, BMEdge **edar, GHash *vhash, GHash *ehash)
{
	/* BMVert *target_vert1, *target_vert2; */ /* UNUSED */
	BMLoop *source_loop, *target_loop;
	BMFace *target_face = NULL;
	BMIter iter, iter2;
	int i;
	
	/* lookup the first and second vert */
#if 0 /* UNUSED */
	target_vert1 = BLI_ghash_lookup(vhash, BM_iter_new(&iter, source_mesh, BM_VERTS_OF_FACE, source_face));
	target_vert2 = BLI_ghash_lookup(vhash, BM_iter_step(&iter));
#else
	BM_iter_new(&iter, source_mesh, BM_VERTS_OF_FACE, source_face);
	BM_iter_step(&iter);
#endif

	/* lookup edge */
	for (i = 0, source_loop = BM_iter_new(&iter, source_mesh, BM_LOOPS_OF_FACE, source_face);
	     source_loop;
	     source_loop = BM_iter_step(&iter), i++)
	{
		vtar[i] = BLI_ghash_lookup(vhash, source_loop->v);
		edar[i] = BLI_ghash_lookup(ehash, source_loop->e);
	}
	
	/* create new fac */
	target_face = BM_face_create(target_mesh, vtar, edar, source_face->len, FALSE);
	BMO_slot_map_ptr_insert(source_mesh, op,
	                        "facemap", source_face, target_face);
	BMO_slot_map_ptr_insert(source_mesh, op,
	                        "facemap", target_face, source_face);

	BM_elem_attrs_copy(source_mesh, target_mesh, source_face, target_face);

	/* mark the face for outpu */
	BMO_elem_flag_enable(target_mesh, target_face, DUPE_NEW);
	
	/* copy per-loop custom dat */
	BM_ITER(source_loop, &iter, source_mesh, BM_LOOPS_OF_FACE, source_face) {
		BM_ITER(target_loop, &iter2, target_mesh, BM_LOOPS_OF_FACE, target_face) {
			if (BLI_ghash_lookup(vhash, source_loop->v) == target_loop->v) {
				BM_elem_attrs_copy(source_mesh, target_mesh, source_loop, target_loop);
				break;
			}
		}
	}

	return target_face;
}

/*
 * COPY MESH
 *
 * Internal Copy function.
 */
static void copy_mesh(BMOperator *op, BMesh *source, BMesh *target)
{

	BMVert *v = NULL, *v2;
	BMEdge *e = NULL;
	BMFace *f = NULL;

	BLI_array_declare(vtar);
	BLI_array_declare(edar);
	BMVert **vtar = NULL;
	BMEdge **edar = NULL;
	
	BMIter verts;
	BMIter edges;
	BMIter faces;
	
	GHash *vhash;
	GHash *ehash;

	/* initialize pointer hashe */
	vhash = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, "bmesh dupeops v");
	ehash = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, "bmesh dupeops e");
	
	for (v = BM_iter_new(&verts, source, BM_VERTS_OF_MESH, source); v; v = BM_iter_step(&verts)) {
		if ( BMO_elem_flag_test(source, v, DUPE_INPUT) &&
		    !BMO_elem_flag_test(source, v, DUPE_DONE))
		{
			BMIter iter;
			int iso = 1;

			v2 = copy_vertex(source, v, target, vhash);

			BM_ITER(f, &iter, source, BM_FACES_OF_VERT, v) {
				if (BMO_elem_flag_test(source, f, DUPE_INPUT)) {
					iso = 0;
					break;
				}
			}

			if (iso) {
				BM_ITER(e, &iter, source, BM_EDGES_OF_VERT, v) {
					if (BMO_elem_flag_test(source, e, DUPE_INPUT)) {
						iso = 0;
						break;
					}
				}
			}

			if (iso) {
				BMO_slot_map_ptr_insert(source, op, "isovertmap", v, v2);
			}

			BMO_elem_flag_enable(source, v, DUPE_DONE);
		}
	}

	/* now we dupe all the edge */
	for (e = BM_iter_new(&edges, source, BM_EDGES_OF_MESH, source); e; e = BM_iter_step(&edges)) {
		if ( BMO_elem_flag_test(source, e, DUPE_INPUT) &&
		    !BMO_elem_flag_test(source, e, DUPE_DONE))
		{
			/* make sure that verts are copie */
			if (!BMO_elem_flag_test(source, e->v1, DUPE_DONE)) {
				copy_vertex(source, e->v1, target, vhash);
				BMO_elem_flag_enable(source, e->v1, DUPE_DONE);
			}
			if (!BMO_elem_flag_test(source, e->v2, DUPE_DONE)) {
				copy_vertex(source, e->v2, target, vhash);
				BMO_elem_flag_enable(source, e->v2, DUPE_DONE);
			}
			/* now copy the actual edg */
			copy_edge(op, source, e, target,  vhash,  ehash);
			BMO_elem_flag_enable(source, e, DUPE_DONE);
		}
	}

	/* first we dupe all flagged faces and their elements from sourc */
	for (f = BM_iter_new(&faces, source, BM_FACES_OF_MESH, source); f; f = BM_iter_step(&faces)) {
		if (BMO_elem_flag_test(source, f, DUPE_INPUT)) {
			/* vertex pas */
			for (v = BM_iter_new(&verts, source, BM_VERTS_OF_FACE, f); v; v = BM_iter_step(&verts)) {
				if (!BMO_elem_flag_test(source, v, DUPE_DONE)) {
					copy_vertex(source, v, target, vhash);
					BMO_elem_flag_enable(source, v, DUPE_DONE);
				}
			}

			/* edge pas */
			for (e = BM_iter_new(&edges, source, BM_EDGES_OF_FACE, f); e; e = BM_iter_step(&edges)) {
				if (!BMO_elem_flag_test(source, e, DUPE_DONE)) {
					copy_edge(op, source, e, target,  vhash,  ehash);
					BMO_elem_flag_enable(source, e, DUPE_DONE);
				}
			}

			/* ensure arrays are the right size */
			BLI_array_empty(vtar);
			BLI_array_empty(edar);

			BLI_array_growitems(vtar, f->len);
			BLI_array_growitems(edar, f->len);

			copy_face(op, source, f, target, vtar, edar, vhash, ehash);
			BMO_elem_flag_enable(source, f, DUPE_DONE);
		}
	}
	
	/* free pointer hashe */
	BLI_ghash_free(vhash, NULL, NULL);
	BLI_ghash_free(ehash, NULL, NULL);

	BLI_array_free(vtar); /* free vert pointer array */
	BLI_array_free(edar); /* free edge pointer array */
}

/*
 * Duplicate Operator
 *
 * Duplicates verts, edges and faces of a mesh.
 *
 * INPUT SLOTS:
 *
 * BMOP_DUPE_VINPUT: Buffer containing pointers to mesh vertices to be duplicated
 * BMOP_DUPE_EINPUT: Buffer containing pointers to mesh edges to be duplicated
 * BMOP_DUPE_FINPUT: Buffer containing pointers to mesh faces to be duplicated
 *
 * OUTPUT SLOTS:
 *
 * BMOP_DUPE_VORIGINAL: Buffer containing pointers to the original mesh vertices
 * BMOP_DUPE_EORIGINAL: Buffer containing pointers to the original mesh edges
 * BMOP_DUPE_FORIGINAL: Buffer containing pointers to the original mesh faces
 * BMOP_DUPE_VNEW: Buffer containing pointers to the new mesh vertices
 * BMOP_DUPE_ENEW: Buffer containing pointers to the new mesh edges
 * BMOP_DUPE_FNEW: Buffer containing pointers to the new mesh faces
 *
 */

void dupeop_exec(BMesh *bm, BMOperator *op)
{
	BMOperator *dupeop = op;
	BMesh *bm2 = BMO_slot_ptr_get(op, "dest");
	
	if (!bm2)
		bm2 = bm;

	/* flag inpu */
	BMO_slot_buffer_flag_enable(bm, dupeop, "geom", DUPE_INPUT, BM_ALL);

	/* use the internal copy functio */
	copy_mesh(dupeop, bm, bm2);
	
	/* Outpu */
	/* First copy the input buffers to output buffers - original dat */
	BMO_slot_copy(dupeop, dupeop, "geom", "origout");

	/* Now alloc the new output buffer */
	BMO_slot_from_flag(bm, dupeop, "newout", DUPE_NEW, BM_ALL);
}

#if 0 /* UNUSED */
/* executes the duplicate operation, feeding elements of
 * type flag etypeflag and header flag flag to it.  note,
 * to get more useful information (such as the mapping from
 * original to new elements) you should run the dupe op manually */
void BMO_dupe_from_flag(BMesh *bm, int etypeflag, const char hflag)
{
	BMOperator dupeop;

	BMO_op_init(bm, &dupeop, "dupe");
	BMO_slot_from_hflag(bm, &dupeop, "geom", hflag, etypeflag);

	BMO_op_exec(bm, &dupeop);
	BMO_op_finish(bm, &dupeop);
}
#endif

/*
 * Split Operator
 *
 * Duplicates verts, edges and faces of a mesh but also deletes the originals.
 *
 * INPUT SLOTS:
 *
 * BMOP_DUPE_VINPUT: Buffer containing pointers to mesh vertices to be split
 * BMOP_DUPE_EINPUT: Buffer containing pointers to mesh edges to be split
 * BMOP_DUPE_FINPUT: Buffer containing pointers to mesh faces to be split
 *
 * OUTPUT SLOTS:
 *
 * BMOP_DUPE_VOUTPUT: Buffer containing pointers to the split mesh vertices
 * BMOP_DUPE_EOUTPUT: Buffer containing pointers to the split mesh edges
 * BMOP_DUPE_FOUTPUT: Buffer containing pointers to the split mesh faces
 */

#define SPLIT_INPUT	1
void splitop_exec(BMesh *bm, BMOperator *op)
{
	BMOperator *splitop = op;
	BMOperator dupeop;
	BMOperator delop;
	const short use_only_faces = BMO_slot_bool_get(op, "use_only_faces");

	/* initialize our sub-operator */
	BMO_op_init(bm, &dupeop, "dupe");
	BMO_op_init(bm, &delop, "del");
	
	BMO_slot_copy(splitop, &dupeop, "geom", "geom");
	BMO_op_exec(bm, &dupeop);
	
	BMO_slot_buffer_flag_enable(bm, splitop, "geom", SPLIT_INPUT, BM_ALL);

	if (use_only_faces) {
		BMVert *v;
		BMEdge *e;
		BMFace *f;
		BMIter iter, iter2;
		int found;

		/* make sure to remove edges and verts we don't need */
		for (e = BM_iter_new(&iter, bm, BM_EDGES_OF_MESH, NULL); e; e = BM_iter_step(&iter)) {
			found = 0;
			f = BM_iter_new(&iter2, bm, BM_FACES_OF_EDGE, e);
			for ( ; f; f = BM_iter_step(&iter2)) {
				if (!BMO_elem_flag_test(bm, f, SPLIT_INPUT)) {
					found = 1;
					break;
				}
			}
			if (!found) BMO_elem_flag_enable(bm, e, SPLIT_INPUT);
		}

		for (v = BM_iter_new(&iter, bm, BM_VERTS_OF_MESH, NULL); v; v = BM_iter_step(&iter)) {
			found = 0;
			e = BM_iter_new(&iter2, bm, BM_EDGES_OF_VERT, v);
			for ( ; e; e = BM_iter_step(&iter2)) {
				if (!BMO_elem_flag_test(bm, e, SPLIT_INPUT)) {
					found = 1;
					break;
				}
			}
			if (!found) BMO_elem_flag_enable(bm, v, SPLIT_INPUT);

		}
	}

	/* connect outputs of dupe to delete, exluding keep geometr */
	BMO_slot_int_set(&delop, "context", DEL_FACES);
	BMO_slot_from_flag(bm, &delop, "geom", SPLIT_INPUT, BM_ALL);
	
	BMO_op_exec(bm, &delop);

	/* now we make our outputs by copying the dupe output */
	BMO_slot_copy(&dupeop, splitop, "newout", "geomout");
	BMO_slot_copy(&dupeop, splitop, "boundarymap",
	              "boundarymap");
	BMO_slot_copy(&dupeop, splitop, "isovertmap",
	              "isovertmap");
	
	/* cleanu */
	BMO_op_finish(bm, &delop);
	BMO_op_finish(bm, &dupeop);
}


void delop_exec(BMesh *bm, BMOperator *op)
{
#define DEL_INPUT 1

	BMOperator *delop = op;

	/* Mark Buffer */
	BMO_slot_buffer_flag_enable(bm, delop, "geom", DEL_INPUT, BM_ALL);

	BMO_remove_tagged_context(bm, DEL_INPUT, BMO_slot_int_get(op, "context"));

#undef DEL_INPUT
}

/*
 * Spin Operator
 *
 * Extrude or duplicate geometry a number of times,
 * rotating and possibly translating after each step
 */

void spinop_exec(BMesh *bm, BMOperator *op)
{
	BMOperator dupop, extop;
	float cent[3], dvec[3];
	float axis[3] = {0.0f, 0.0f, 1.0f};
	float q[4];
	float rmat[3][3];
	float phi, si;
	int steps, do_dupli, a, usedvec;

	BMO_slot_vec_get(op, "cent", cent);
	BMO_slot_vec_get(op, "axis", axis);
	normalize_v3(axis);
	BMO_slot_vec_get(op, "dvec", dvec);
	usedvec = !is_zero_v3(dvec);
	steps = BMO_slot_int_get(op, "steps");
	phi = BMO_slot_float_get(op, "ang") * (float)M_PI / (360.0f * steps);
	do_dupli = BMO_slot_bool_get(op, "do_dupli");

	si = (float)sin(phi);
	q[0] = (float)cos(phi);
	q[1] = axis[0]*si;
	q[2] = axis[1]*si;
	q[3] = axis[2]*si;
	quat_to_mat3(rmat, q);

	BMO_slot_copy(op, op, "geom", "lastout");
	for (a = 0; a < steps; a++) {
		if (do_dupli) {
			BMO_op_initf(bm, &dupop, "dupe geom=%s", op, "lastout");
			BMO_op_exec(bm, &dupop);
			BMO_op_callf(bm, "rotate cent=%v mat=%m3 verts=%s",
			             cent, rmat, &dupop, "newout");
			BMO_slot_copy(&dupop, op, "newout", "lastout");
			BMO_op_finish(bm, &dupop);
		}
		else {
			BMO_op_initf(bm, &extop, "extrudefaceregion edgefacein=%s",
			             op, "lastout");
			BMO_op_exec(bm, &extop);
			BMO_op_callf(bm, "rotate cent=%v mat=%m3 verts=%s",
			             cent, rmat, &extop, "geomout");
			BMO_slot_copy(&extop, op, "geomout", "lastout");
			BMO_op_finish(bm, &extop);
		}

		if (usedvec)
			BMO_op_callf(bm, "translate vec=%v verts=%s", dvec, op, "lastout");
	}
}
