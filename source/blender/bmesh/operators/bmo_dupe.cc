/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bmesh
 *
 * Duplicate, Split, Split operators.
 */

#include "MEM_guardedalloc.h"

#include "BLI_alloca.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"

#include "bmesh.hh"

#include "intern/bmesh_operators_private.hh" /* own include */

/* local flag define */
#define DUPE_INPUT 1 /* input from operator */
#define DUPE_NEW 2
#define DUPE_DONE 4
// #define DUPE_MAPPED     8 // UNUSED

/**
 * COPY VERTEX
 *
 * Copy an existing vertex from one bmesh to another.
 */
static BMVert *bmo_vert_copy(BMOperator *op,
                             BMOpSlot *slot_vertmap_out,
                             BMesh *bm_dst,
                             const std::optional<BMCustomDataCopyMap> &cd_vert_map,
                             BMVert *v_src,
                             GHash *vhash)
{
  BMVert *v_dst;

  /* Create a new vertex */
  v_dst = BM_vert_create(bm_dst, v_src->co, nullptr, BM_CREATE_SKIP_CD);
  BMO_slot_map_elem_insert(op, slot_vertmap_out, v_src, v_dst);
  BMO_slot_map_elem_insert(op, slot_vertmap_out, v_dst, v_src);

  /* Insert new vertex into the vert hash */
  BLI_ghash_insert(vhash, v_src, v_dst);

  /* Copy attributes */
  if (cd_vert_map.has_value()) {
    BM_elem_attrs_copy(bm_dst, cd_vert_map.value(), v_src, v_dst);
  }
  else {
    BM_elem_attrs_copy(bm_dst, v_src, v_dst);
  }

  /* Mark the vert for output */
  BMO_vert_flag_enable(bm_dst, v_dst, DUPE_NEW);

  return v_dst;
}

/**
 * COPY EDGE
 *
 * Copy an existing edge from one bmesh to another.
 */
static BMEdge *bmo_edge_copy(BMOperator *op,
                             BMOpSlot *slot_edgemap_out,
                             BMOpSlot *slot_boundarymap_out,
                             BMesh *bm_dst,
                             BMesh *bm_src,
                             const std::optional<BMCustomDataCopyMap> &cd_edge_map,
                             BMEdge *e_src,
                             GHash *vhash,
                             GHash *ehash,
                             const bool use_edge_flip_from_face)
{
  BMEdge *e_dst;
  BMVert *e_dst_v1, *e_dst_v2;
  uint rlen;

  /* see if any of the neighboring faces are
   * not being duplicated.  in that case,
   * add it to the new/old map. */
  /* lookup edge */
  rlen = 0;
  if (e_src->l) {
    BMLoop *l_iter_src, *l_first_src;
    l_iter_src = l_first_src = e_src->l;
    do {
      if (BMO_face_flag_test(bm_src, l_iter_src->f, DUPE_INPUT)) {
        rlen++;
      }
    } while ((l_iter_src = l_iter_src->radial_next) != l_first_src);
  }

  /* Lookup v1 and v2 */
  e_dst_v1 = static_cast<BMVert *>(BLI_ghash_lookup(vhash, e_src->v1));
  e_dst_v2 = static_cast<BMVert *>(BLI_ghash_lookup(vhash, e_src->v2));

  /* Create a new edge */
  e_dst = BM_edge_create(bm_dst, e_dst_v1, e_dst_v2, nullptr, BM_CREATE_SKIP_CD);
  BMO_slot_map_elem_insert(op, slot_edgemap_out, e_src, e_dst);
  BMO_slot_map_elem_insert(op, slot_edgemap_out, e_dst, e_src);

  /* Add to new/old edge map if necessary. */
  if (rlen < 2) {
    /* not sure what non-manifold cases of greater than three
     * radial should do. */
    BMO_slot_map_elem_insert(op, slot_boundarymap_out, e_src, e_dst);
  }

  /* Insert new edge into the edge hash */
  BLI_ghash_insert(ehash, e_src, e_dst);

  /* Copy attributes */
  if (cd_edge_map.has_value()) {
    BM_elem_attrs_copy(bm_dst, cd_edge_map.value(), e_src, e_dst);
  }
  else {
    BM_elem_attrs_copy(bm_dst, e_src, e_dst);
  }

  /* Mark the edge for output */
  BMO_edge_flag_enable(bm_dst, e_dst, DUPE_NEW);

  if (use_edge_flip_from_face) {
    /* Take winding from previous face (if we had one),
     * otherwise extruding a duplicated edges gives bad normals, see: #62487. */
    if (BM_edge_is_boundary(e_src) && (e_src->l->v == e_src->v1)) {
      BM_edge_verts_swap(e_dst);
    }
  }

  return e_dst;
}

/**
 * COPY FACE
 *
 * Copy an existing face from one bmesh to another.
 */
static BMFace *bmo_face_copy(BMOperator *op,
                             BMOpSlot *slot_facemap_out,
                             BMesh *bm_dst,
                             const std::optional<BMCustomDataCopyMap> &cd_face_map,
                             const std::optional<BMCustomDataCopyMap> &cd_loop_map,
                             BMFace *f_src,
                             GHash *vhash,
                             GHash *ehash)
{
  BMFace *f_dst;
  BMVert **vtar = BLI_array_alloca(vtar, f_src->len);
  BMEdge **edar = BLI_array_alloca(edar, f_src->len);
  BMLoop *l_iter_src, *l_iter_dst, *l_first_src;
  int i;

  l_first_src = BM_FACE_FIRST_LOOP(f_src);

  /* lookup edge */
  l_iter_src = l_first_src;
  i = 0;
  do {
    vtar[i] = static_cast<BMVert *>(BLI_ghash_lookup(vhash, l_iter_src->v));
    edar[i] = static_cast<BMEdge *>(BLI_ghash_lookup(ehash, l_iter_src->e));
    i++;
  } while ((l_iter_src = l_iter_src->next) != l_first_src);

  /* create new face */
  f_dst = BM_face_create(bm_dst, vtar, edar, f_src->len, nullptr, BM_CREATE_SKIP_CD);
  BMO_slot_map_elem_insert(op, slot_facemap_out, f_src, f_dst);
  BMO_slot_map_elem_insert(op, slot_facemap_out, f_dst, f_src);

  /* Copy attributes */
  if (cd_face_map.has_value()) {
    BM_elem_attrs_copy(bm_dst, cd_face_map.value(), f_src, f_dst);
  }
  else {
    BM_elem_attrs_copy(bm_dst, f_src, f_dst);
  }

  /* copy per-loop custom data */
  l_iter_src = l_first_src;
  l_iter_dst = BM_FACE_FIRST_LOOP(f_dst);
  do {
    if (cd_loop_map.has_value()) {
      BM_elem_attrs_copy(bm_dst, cd_loop_map.value(), l_iter_src, l_iter_dst);
    }
    else {
      BM_elem_attrs_copy(bm_dst, l_iter_src, l_iter_dst);
    }
  } while ((void)(l_iter_dst = l_iter_dst->next), (l_iter_src = l_iter_src->next) != l_first_src);

  /* Mark the face for output */
  BMO_face_flag_enable(bm_dst, f_dst, DUPE_NEW);

  return f_dst;
}

/**
 * COPY MESH
 *
 * Internal Copy function.
 */
static void bmo_mesh_copy(BMOperator *op, BMesh *bm_dst, BMesh *bm_src)
{
  const bool use_select_history = BMO_slot_bool_get(op->slots_in, "use_select_history");
  const bool use_edge_flip_from_face = BMO_slot_bool_get(op->slots_in, "use_edge_flip_from_face");

  BMVert *v = nullptr, *v2;
  BMEdge *e = nullptr;
  BMFace *f = nullptr;

  BMIter viter, eiter, fiter;
  GHash *vhash, *ehash;

  BMOpSlot *slot_boundary_map_out = BMO_slot_get(op->slots_out, "boundary_map.out");
  BMOpSlot *slot_isovert_map_out = BMO_slot_get(op->slots_out, "isovert_map.out");

  BMOpSlot *slot_vert_map_out = BMO_slot_get(op->slots_out, "vert_map.out");
  BMOpSlot *slot_edge_map_out = BMO_slot_get(op->slots_out, "edge_map.out");
  BMOpSlot *slot_face_map_out = BMO_slot_get(op->slots_out, "face_map.out");

  /* initialize pointer hashes */
  vhash = BLI_ghash_ptr_new("bmesh dupeops v");
  ehash = BLI_ghash_ptr_new("bmesh dupeops e");

  const std::optional<BMCustomDataCopyMap> cd_vert_map =
      (bm_src == bm_dst) ? std::nullopt :
                           std::optional<BMCustomDataCopyMap>{
                               CustomData_bmesh_copy_map_calc(bm_src->vdata, bm_dst->vdata)};
  const std::optional<BMCustomDataCopyMap> cd_edge_map =
      (bm_src == bm_dst) ? std::nullopt :
                           std::optional<BMCustomDataCopyMap>{
                               CustomData_bmesh_copy_map_calc(bm_src->edata, bm_dst->edata)};
  const std::optional<BMCustomDataCopyMap> cd_face_map =
      (bm_src == bm_dst) ? std::nullopt :
                           std::optional<BMCustomDataCopyMap>{
                               CustomData_bmesh_copy_map_calc(bm_src->pdata, bm_dst->pdata)};
  const std::optional<BMCustomDataCopyMap> cd_loop_map =
      (bm_src == bm_dst) ? std::nullopt :
                           std::optional<BMCustomDataCopyMap>{
                               CustomData_bmesh_copy_map_calc(bm_src->ldata, bm_dst->ldata)};

  /* duplicate flagged vertices */
  BM_ITER_MESH (v, &viter, bm_src, BM_VERTS_OF_MESH) {
    if (BMO_vert_flag_test(bm_src, v, DUPE_INPUT) &&
        BMO_vert_flag_test(bm_src, v, DUPE_DONE) == false)
    {
      BMIter iter;
      bool isolated = true;

      v2 = bmo_vert_copy(op, slot_vert_map_out, bm_dst, cd_vert_map, v, vhash);

      BM_ITER_ELEM (f, &iter, v, BM_FACES_OF_VERT) {
        if (BMO_face_flag_test(bm_src, f, DUPE_INPUT)) {
          isolated = false;
          break;
        }
      }

      if (isolated) {
        BM_ITER_ELEM (e, &iter, v, BM_EDGES_OF_VERT) {
          if (BMO_edge_flag_test(bm_src, e, DUPE_INPUT)) {
            isolated = false;
            break;
          }
        }
      }

      if (isolated) {
        BMO_slot_map_elem_insert(op, slot_isovert_map_out, v, v2);
      }

      BMO_vert_flag_enable(bm_src, v, DUPE_DONE);
    }
  }

  /* now we dupe all the edges */
  BM_ITER_MESH (e, &eiter, bm_src, BM_EDGES_OF_MESH) {
    if (BMO_edge_flag_test(bm_src, e, DUPE_INPUT) &&
        BMO_edge_flag_test(bm_src, e, DUPE_DONE) == false)
    {
      /* make sure that verts are copied */
      if (!BMO_vert_flag_test(bm_src, e->v1, DUPE_DONE)) {
        bmo_vert_copy(op, slot_vert_map_out, bm_dst, cd_vert_map, e->v1, vhash);
        BMO_vert_flag_enable(bm_src, e->v1, DUPE_DONE);
      }
      if (!BMO_vert_flag_test(bm_src, e->v2, DUPE_DONE)) {
        bmo_vert_copy(op, slot_vert_map_out, bm_dst, cd_vert_map, e->v2, vhash);
        BMO_vert_flag_enable(bm_src, e->v2, DUPE_DONE);
      }
      /* now copy the actual edge */
      bmo_edge_copy(op,
                    slot_edge_map_out,
                    slot_boundary_map_out,
                    bm_dst,
                    bm_src,
                    cd_edge_map,
                    e,
                    vhash,
                    ehash,
                    use_edge_flip_from_face);
      BMO_edge_flag_enable(bm_src, e, DUPE_DONE);
    }
  }

  /* first we dupe all flagged faces and their elements from source */
  BM_ITER_MESH (f, &fiter, bm_src, BM_FACES_OF_MESH) {
    if (BMO_face_flag_test(bm_src, f, DUPE_INPUT)) {
      /* vertex pass */
      BM_ITER_ELEM (v, &viter, f, BM_VERTS_OF_FACE) {
        if (!BMO_vert_flag_test(bm_src, v, DUPE_DONE)) {
          bmo_vert_copy(op, slot_vert_map_out, bm_dst, cd_vert_map, v, vhash);
          BMO_vert_flag_enable(bm_src, v, DUPE_DONE);
        }
      }

      /* edge pass */
      BM_ITER_ELEM (e, &eiter, f, BM_EDGES_OF_FACE) {
        if (!BMO_edge_flag_test(bm_src, e, DUPE_DONE)) {
          bmo_edge_copy(op,
                        slot_edge_map_out,
                        slot_boundary_map_out,
                        bm_dst,
                        bm_src,
                        cd_edge_map,
                        e,
                        vhash,
                        ehash,
                        use_edge_flip_from_face);
          BMO_edge_flag_enable(bm_src, e, DUPE_DONE);
        }
      }
      bmo_face_copy(op, slot_face_map_out, bm_dst, cd_face_map, cd_loop_map, f, vhash, ehash);
      BMO_face_flag_enable(bm_src, f, DUPE_DONE);
    }
  }

  /* free pointer hashes */
  BLI_ghash_free(vhash, nullptr, nullptr);
  BLI_ghash_free(ehash, nullptr, nullptr);

  if (use_select_history) {
    BLI_assert(bm_src == bm_dst);
    BMO_mesh_selected_remap(
        bm_dst, slot_vert_map_out, slot_edge_map_out, slot_face_map_out, false);
  }
}

/**
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
 */
void bmo_duplicate_exec(BMesh *bm, BMOperator *op)
{
  BMOperator *dupeop = op;
  BMesh *bm_dst = static_cast<BMesh *>(BMO_slot_ptr_get(op->slots_in, "dest"));

  if (!bm_dst) {
    bm_dst = bm;
  }

  /* flag input */
  BMO_slot_buffer_flag_enable(bm, dupeop->slots_in, "geom", BM_ALL_NOLOOP, DUPE_INPUT);

  /* use the internal copy function */
  bmo_mesh_copy(dupeop, bm_dst, bm);

  /* Output */
  /* First copy the input buffers to output buffers - original data */
  BMO_slot_copy(dupeop, slots_in, "geom", dupeop, slots_out, "geom_orig.out");

  /* Now alloc the new output buffers */
  BMO_slot_buffer_from_enabled_flag(
      bm, dupeop, dupeop->slots_out, "geom.out", BM_ALL_NOLOOP, DUPE_NEW);
}

#if 0 /* UNUSED */
/**
 * executes the duplicate operation, feeding elements of
 * type flag etypeflag and header flag to it.
 * \note to get more useful information (such as the mapping from
 * original to new elements) you should run the dupe op manually.
 */
void BMO_dupe_from_flag(BMesh *bm, int htype, const char hflag)
{
  BMOperator dupeop;

  BMO_op_init(bm, &dupeop, "duplicate");
  BMO_slot_buffer_from_enabled_hflag(bm, &dupeop, "geom", htype, hflag);

  BMO_op_exec(bm, &dupeop);
  BMO_op_finish(bm, &dupeop);
}
#endif

/**
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
 *
 * \note Lower level uses of this operator may want to use #BM_mesh_separate_faces
 * Since it's faster for the 'use_only_faces' case.
 */
void bmo_split_exec(BMesh *bm, BMOperator *op)
{
#define SPLIT_INPUT 1

  BMOperator *splitop = op;
  BMOperator dupeop;
  const bool use_only_faces = BMO_slot_bool_get(op->slots_in, "use_only_faces");

  /* initialize our sub-operator */
  BMO_op_init(bm, &dupeop, op->flag, "duplicate");

  BMO_slot_copy(splitop, slots_in, "geom", &dupeop, slots_in, "geom");
  BMO_op_exec(bm, &dupeop);

  BMFace *new_act_face = static_cast<BMFace *>(
      BMO_slot_map_elem_get(BMO_slot_get(dupeop.slots_out, "face_map.out"), bm->act_face));
  if (new_act_face) {
    bm->act_face = new_act_face;
  }

  BMO_slot_buffer_flag_enable(bm, splitop->slots_in, "geom", BM_ALL_NOLOOP, SPLIT_INPUT);

  if (use_only_faces) {
    BMVert *v;
    BMEdge *e;
    BMFace *f;
    BMIter iter, iter2;

    /* make sure to remove edges and verts we don't need */
    BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
      bool found = false;
      BM_ITER_ELEM (f, &iter2, e, BM_FACES_OF_EDGE) {
        if (!BMO_face_flag_test(bm, f, SPLIT_INPUT)) {
          found = true;
          break;
        }
      }
      if (found == false) {
        BMO_edge_flag_enable(bm, e, SPLIT_INPUT);
      }
    }

    BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
      bool found = false;
      BM_ITER_ELEM (e, &iter2, v, BM_EDGES_OF_VERT) {
        if (!BMO_edge_flag_test(bm, e, SPLIT_INPUT)) {
          found = true;
          break;
        }
      }
      if (found == false) {
        BMO_vert_flag_enable(bm, v, SPLIT_INPUT);
      }
    }
  }

  BMO_slot_copy(&dupeop, slots_out, "geom.out", splitop, slots_out, "geom.out");
  BMO_slot_copy(&dupeop, slots_out, "isovert_map.out", splitop, slots_out, "isovert_map.out");

  /* connect outputs of dupe to delete, excluding keep geometry */
  BMO_mesh_delete_oflag_context(
      bm,
      SPLIT_INPUT,
      DEL_FACES,
      /* Call before deletion so deleted geometry isn't copied. */
      [&bm, &dupeop, &splitop]() {
        /* Now we make our outputs by copying the dupe output. */

        /* NOTE: `boundary_map.out` can't use #BMO_slot_copy because some of the "source"
         * geometry has been removed. In this case the (source -> destination) map doesn't work.
         * In this case there isn't an especially good option.
         * The geometry needs to be included so the boundary is accessible.
         * Use the "destination" as the key and the value since it avoids adding freed
         * geometry into the map and can be easily detected by other operators.
         * See: #142633. */
        const char *slot_name_boundary_map = "boundary_map.out";
        BMOpSlot *splitop_boundary_map = BMO_slot_get(splitop->slots_out, slot_name_boundary_map);
        BMOIter siter;
        BMElem *ele_key;
        BMO_ITER (ele_key, &siter, dupeop.slots_out, slot_name_boundary_map, 0) {
          BMElem *ele_val = static_cast<BMElem *>(BMO_iter_map_value_ptr(&siter));
          if (BMO_elem_flag_test(bm, ele_key, SPLIT_INPUT)) {
            ele_key = ele_val;
          }
          BMO_slot_map_elem_insert(splitop, splitop_boundary_map, ele_key, ele_val);
        }
      });

  /* cleanup */
  BMO_op_finish(bm, &dupeop);

#undef SPLIT_INPUT
}

void bmo_delete_exec(BMesh *bm, BMOperator *op)
{
#define DEL_INPUT 1

  BMOperator *delop = op;

  /* Mark Buffer */
  BMO_slot_buffer_flag_enable(bm, delop->slots_in, "geom", BM_ALL_NOLOOP, DEL_INPUT);

  BMO_mesh_delete_oflag_context(bm, DEL_INPUT, BMO_slot_int_get(op->slots_in, "context"), nullptr);

#undef DEL_INPUT
}

/**
 * Spin Operator
 *
 * Extrude or duplicate geometry a number of times,
 * rotating and possibly translating after each step
 */
void bmo_spin_exec(BMesh *bm, BMOperator *op)
{
  BMOperator dupop, extop;
  float cent[3], dvec[3];
  float axis[3];
  float rmat[3][3];
  float phi;
  int steps, do_dupli, a;
  bool use_dvec;

  BMO_slot_vec_get(op->slots_in, "cent", cent);
  BMO_slot_vec_get(op->slots_in, "axis", axis);
  normalize_v3(axis);
  BMO_slot_vec_get(op->slots_in, "dvec", dvec);
  use_dvec = !is_zero_v3(dvec);
  steps = BMO_slot_int_get(op->slots_in, "steps");
  phi = BMO_slot_float_get(op->slots_in, "angle") / steps;
  do_dupli = BMO_slot_bool_get(op->slots_in, "use_duplicate");
  const bool use_normal_flip = BMO_slot_bool_get(op->slots_in, "use_normal_flip");
  /* Caller needs to perform other sanity checks (such as the spin being 360d). */
  const bool use_merge = BMO_slot_bool_get(op->slots_in, "use_merge") && steps >= 3;

  axis_angle_normalized_to_mat3(rmat, axis, phi);

  BMVert **vtable = nullptr;
  if (use_merge) {
    vtable = MEM_malloc_arrayN<BMVert *>(bm->totvert, __func__);
    int i = 0;
    BMIter iter;
    BMVert *v;
    BM_ITER_MESH_INDEX (v, &iter, bm, BM_VERTS_OF_MESH, i) {
      vtable[i] = v;
      /* Evil! store original index in normal,
       * this is duplicated into every other vertex.
       * So we can read the original from the final.
       *
       * The normals must be recalculated anyway. */
      *((int *)&v->no[0]) = i;
    }
  }

  BMO_slot_copy(op, slots_in, "geom", op, slots_out, "geom_last.out");
  for (a = 0; a < steps; a++) {
    if (do_dupli) {
      BMO_op_initf(bm, &dupop, op->flag, "duplicate geom=%S", op, "geom_last.out");
      BMO_op_exec(bm, &dupop);
      BMO_op_callf(bm,
                   op->flag,
                   "rotate cent=%v matrix=%m3 space=%s verts=%S",
                   cent,
                   rmat,
                   op,
                   "space",
                   &dupop,
                   "geom.out");
      BMO_slot_copy(&dupop, slots_out, "geom.out", op, slots_out, "geom_last.out");
      BMO_op_finish(bm, &dupop);
    }
    else {
      BMO_op_initf(bm,
                   &extop,
                   op->flag,
                   "extrude_face_region "
                   "geom=%S "
                   "use_keep_orig=%b "
                   "use_normal_flip=%b "
                   "use_normal_from_adjacent=%b "
                   "skip_input_flip=%b",
                   op,
                   "geom_last.out",
                   use_merge,
                   use_normal_flip && (a == 0),
                   (a != 0),
                   true);
      BMO_op_exec(bm, &extop);
      if ((use_merge && (a == steps - 1)) == false) {
        BMO_op_callf(bm,
                     op->flag,
                     "rotate cent=%v matrix=%m3 space=%s verts=%S",
                     cent,
                     rmat,
                     op,
                     "space",
                     &extop,
                     "geom.out");
        BMO_slot_copy(&extop, slots_out, "geom.out", op, slots_out, "geom_last.out");
      }
      else {
        /* Merge first/last vertices and edges (maintaining 'geom.out' state). */
        BMOpSlot *slot_geom_out = BMO_slot_get(extop.slots_out, "geom.out");
        BMElem **elem_array = (BMElem **)slot_geom_out->data.buf;
        int elem_array_len = slot_geom_out->len;
        for (int i = 0; i < elem_array_len;) {
          if (elem_array[i]->head.htype == BM_VERT) {
            BMVert *v_src = (BMVert *)elem_array[i];
            BMVert *v_dst = vtable[*((const int *)&v_src->no[0])];
            BM_vert_splice(bm, v_dst, v_src);
            elem_array_len--;
            elem_array[i] = elem_array[elem_array_len];
          }
          else {
            i++;
          }
        }
        for (int i = 0; i < elem_array_len;) {
          if (elem_array[i]->head.htype == BM_EDGE) {
            BMEdge *e_src = (BMEdge *)elem_array[i];
            BMEdge *e_dst = BM_edge_find_double(e_src);
            if (e_dst != nullptr) {
              BM_edge_splice(bm, e_dst, e_src);
              elem_array_len--;
              elem_array[i] = elem_array[elem_array_len];
              continue;
            }
          }
          i++;
        }
        /* Full copies of faces may cause overlap. */
        for (int i = 0; i < elem_array_len;) {
          if (elem_array[i]->head.htype == BM_FACE) {
            BMFace *f_src = (BMFace *)elem_array[i];
            BMFace *f_dst = BM_face_find_double(f_src);
            if (f_dst != nullptr) {
              BM_face_kill(bm, f_src);
              elem_array_len--;
              elem_array[i] = elem_array[elem_array_len];
              continue;
            }
          }
          i++;
        }
        slot_geom_out->len = elem_array_len;
      }
      BMO_op_finish(bm, &extop);
    }

    if (use_dvec) {
      mul_m3_v3(rmat, dvec);
      BMO_op_callf(bm,
                   op->flag,
                   "translate vec=%v space=%s verts=%S",
                   dvec,
                   op,
                   "space",
                   op,
                   "geom_last.out");
    }
  }

  if (vtable) {
    MEM_freeN(vtable);
  }
}
