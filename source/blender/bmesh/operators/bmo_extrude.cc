/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bmesh
 *
 * Extrude faces and solidify.
 */

#include "MEM_guardedalloc.h"

#include "DNA_meshdata_types.h"

#include "BLI_math_geom.h"
#include "BLI_math_vector.h"
#include "BLI_vector.hh"

#include "BKE_customdata.hh"

#include "bmesh.hh"

#include "intern/bmesh_operators_private.hh" /* own include */

#define USE_EDGE_REGION_FLAGS

enum {
  EXT_INPUT = 1,
  EXT_KEEP = 2,
  EXT_DEL = 4,
  EXT_TAG = 8,
};

#define VERT_MARK 1
#define EDGE_MARK 1
#define FACE_MARK 1
#define VERT_NONMAN 2
#define EDGE_NONMAN 2

void bmo_extrude_discrete_faces_exec(BMesh *bm, BMOperator *op)
{
  const bool use_select_history = BMO_slot_bool_get(op->slots_in, "use_select_history");
  GHash *select_history_map = nullptr;

  BMOIter siter;
  BMFace *f_org;

  if (use_select_history) {
    select_history_map = BM_select_history_map_create(bm);
  }

  BMO_ITER (f_org, &siter, op->slots_in, "faces", BM_FACE) {
    BMFace *f_new;
    BMLoop *l_org, *l_org_first;
    BMLoop *l_new;

    BMO_face_flag_enable(bm, f_org, EXT_DEL);

    f_new = BM_face_copy(bm, f_org, true, true);
    BMO_face_flag_enable(bm, f_new, EXT_KEEP);

    if (select_history_map) {
      BMEditSelection *ese;
      ese = static_cast<BMEditSelection *>(BLI_ghash_lookup(select_history_map, f_org));
      if (ese) {
        ese->ele = (BMElem *)f_new;
      }
    }

    l_org = l_org_first = BM_FACE_FIRST_LOOP(f_org);
    l_new = BM_FACE_FIRST_LOOP(f_new);

    do {
      BMFace *f_side;
      BMLoop *l_side_iter;

      BM_elem_attrs_copy(bm, l_org, l_new);

      f_side = BM_face_create_quad_tri(
          bm, l_org->next->v, l_new->next->v, l_new->v, l_org->v, f_org, BM_CREATE_NOP);

      l_side_iter = BM_FACE_FIRST_LOOP(f_side);

      BM_elem_attrs_copy(bm, l_org->next, l_side_iter);
      l_side_iter = l_side_iter->next;
      BM_elem_attrs_copy(bm, l_org->next, l_side_iter);
      l_side_iter = l_side_iter->next;
      BM_elem_attrs_copy(bm, l_org, l_side_iter);
      l_side_iter = l_side_iter->next;
      BM_elem_attrs_copy(bm, l_org, l_side_iter);

      if (select_history_map) {
        BMEditSelection *ese;

        ese = static_cast<BMEditSelection *>(BLI_ghash_lookup(select_history_map, l_org->v));
        if (ese) {
          ese->ele = (BMElem *)l_new->v;
        }
        ese = static_cast<BMEditSelection *>(BLI_ghash_lookup(select_history_map, l_org->e));
        if (ese) {
          ese->ele = (BMElem *)l_new->e;
        }
      }

    } while (((void)(l_new = l_new->next), (l_org = l_org->next)) != l_org_first);
  }

  if (select_history_map) {
    BLI_ghash_free(select_history_map, nullptr, nullptr);
  }

  BMO_op_callf(bm, op->flag, "delete geom=%ff context=%i", EXT_DEL, DEL_ONLYFACES);
  BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "faces.out", BM_FACE, EXT_KEEP);
}

/**
 * \brief Copy the loop pair from an adjacent face to both sides of this quad.
 *
 * The face is assumed to be a quad, created by extruding.
 * This function won't crash if its not but won't work right either.
 * \a e_b is the new edge.
 *
 * \note The edge this face comes from needs to be from the first and second verts to the face.
 * The caller must ensure this else we will copy from the wrong source.
 */
static void bm_extrude_copy_face_loop_attributes(BMesh *bm, BMFace *f)
{
  /* edge we are extruded from */
  BMLoop *l_first_0 = BM_FACE_FIRST_LOOP(f);
  BMLoop *l_first_1 = l_first_0->next;
  BMLoop *l_first_2 = l_first_1->next;
  BMLoop *l_first_3 = l_first_2->next;

  BMLoop *l_other_0;
  BMLoop *l_other_1;

  if (UNLIKELY(l_first_0 == l_first_0->radial_next)) {
    return;
  }

  l_other_0 = BM_edge_other_loop(l_first_0->e, l_first_0);
  l_other_1 = BM_edge_other_loop(l_first_0->e, l_first_1);

  /* copy data */
  BM_elem_attrs_copy(bm, l_other_0->f, f);
  BM_elem_flag_disable(f, BM_ELEM_HIDDEN); /* possibly we copy from a hidden face */

  BM_elem_attrs_copy(bm, l_other_0, l_first_0);
  BM_elem_attrs_copy(bm, l_other_0, l_first_3);

  BM_elem_attrs_copy(bm, l_other_1, l_first_1);
  BM_elem_attrs_copy(bm, l_other_1, l_first_2);
}

/* Disable the skin root flag on the input vert, assumes that the vert
 * data includes an CD_MVERT_SKIN layer */
static void bm_extrude_disable_skin_root(BMesh *bm, BMVert *v)
{
  MVertSkin *vs;

  vs = static_cast<MVertSkin *>(CustomData_bmesh_get(&bm->vdata, v->head.data, CD_MVERT_SKIN));
  vs->flag &= ~MVERT_SKIN_ROOT;
}

void bmo_extrude_edge_only_exec(BMesh *bm, BMOperator *op)
{
  BMOIter siter;
  BMOperator dupeop;
  BMFace *f;
  BMEdge *e, *e_new;
  const bool use_normal_flip = BMO_slot_bool_get(op->slots_in, "use_normal_flip");

  BMO_ITER (e, &siter, op->slots_in, "edges", BM_EDGE) {
    BMO_edge_flag_enable(bm, e, EXT_INPUT);
    BMO_vert_flag_enable(bm, e->v1, EXT_INPUT);
    BMO_vert_flag_enable(bm, e->v2, EXT_INPUT);
  }

  BMO_op_initf(bm,
               &dupeop,
               op->flag,
               "duplicate geom=%fve use_select_history=%b",
               EXT_INPUT,
               BMO_slot_bool_get(op->slots_in, "use_select_history"));

  BMO_op_exec(bm, &dupeop);

  /* disable root flag on all new skin nodes */
  if (CustomData_has_layer(&bm->vdata, CD_MVERT_SKIN)) {
    BMVert *v;
    BMO_ITER (v, &siter, dupeop.slots_out, "geom.out", BM_VERT) {
      bm_extrude_disable_skin_root(bm, v);
    }
  }

  for (e = static_cast<BMEdge *>(BMO_iter_new(&siter, dupeop.slots_out, "boundary_map.out", 0)); e;
       e = static_cast<BMEdge *>(BMO_iter_step(&siter)))
  {
    BMVert *f_verts[4];
    e_new = static_cast<BMEdge *>(BMO_iter_map_value_ptr(&siter));

    const bool edge_normal_flip = !(e->l && e->v1 != e->l->v);
    if (edge_normal_flip == use_normal_flip) {
      f_verts[0] = e->v1;
      f_verts[1] = e->v2;
      f_verts[2] = e_new->v2;
      f_verts[3] = e_new->v1;
    }
    else {
      f_verts[0] = e->v2;
      f_verts[1] = e->v1;
      f_verts[2] = e_new->v1;
      f_verts[3] = e_new->v2;
    }
    /* not sure what to do about example face, pass nullptr for now */
    f = BM_face_create_verts(bm, f_verts, 4, nullptr, BM_CREATE_NOP, true);
    bm_extrude_copy_face_loop_attributes(bm, f);

    if (BMO_edge_flag_test(bm, e, EXT_INPUT)) {
      e = e_new;
    }

    BMO_face_flag_enable(bm, f, EXT_KEEP);
    BMO_edge_flag_enable(bm, e, EXT_KEEP);
    BMO_vert_flag_enable(bm, e->v1, EXT_KEEP);
    BMO_vert_flag_enable(bm, e->v2, EXT_KEEP);
  }

  BMO_op_finish(bm, &dupeop);

  BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "geom.out", BM_ALL_NOLOOP, EXT_KEEP);
}

void bmo_extrude_vert_indiv_exec(BMesh *bm, BMOperator *op)
{
  const bool use_select_history = BMO_slot_bool_get(op->slots_in, "use_select_history");
  BMOIter siter;
  BMVert *v, *dupev;
  BMEdge *e;
  const bool has_vskin = CustomData_has_layer(&bm->vdata, CD_MVERT_SKIN);
  GHash *select_history_map = nullptr;

  if (use_select_history) {
    select_history_map = BM_select_history_map_create(bm);
  }

  for (v = static_cast<BMVert *>(BMO_iter_new(&siter, op->slots_in, "verts", BM_VERT)); v;
       v = static_cast<BMVert *>(BMO_iter_step(&siter)))
  {
    dupev = BM_vert_create(bm, v->co, v, BM_CREATE_NOP);
    BMO_vert_flag_enable(bm, dupev, EXT_KEEP);

    if (has_vskin) {
      bm_extrude_disable_skin_root(bm, v);
    }

    if (select_history_map) {
      BMEditSelection *ese;
      ese = static_cast<BMEditSelection *>(BLI_ghash_lookup(select_history_map, v));
      if (ese) {
        ese->ele = (BMElem *)dupev;
      }
    }

    /* not essential, but ensures face normals from extruded edges are contiguous */
    if (BM_vert_is_wire_endpoint(v)) {
      if (v->e->v1 == v) {
        std::swap(v, dupev);
      }
    }

    e = BM_edge_create(bm, v, dupev, nullptr, BM_CREATE_NOP);
    BMO_edge_flag_enable(bm, e, EXT_KEEP);
  }

  if (select_history_map) {
    BLI_ghash_free(select_history_map, nullptr, nullptr);
  }

  BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "verts.out", BM_VERT, EXT_KEEP);
  BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "edges.out", BM_EDGE, EXT_KEEP);
}

#ifdef USE_EDGE_REGION_FLAGS
/**
 * When create an edge for an extruded face region
 * check surrounding edge flags before creating a new edge.
 */
static bool bm_extrude_region_edge_flag(const BMVert *v, char r_e_hflag[2])
{
  BMEdge *e_iter;
  const char hflag_enable = BM_ELEM_SEAM;
  const char hflag_disable = BM_ELEM_SMOOTH;
  bool ok = false;

  r_e_hflag[0] = 0x0;
  r_e_hflag[1] = 0xff;

  /* clear flags on both disks */
  e_iter = v->e;
  do {
    if (e_iter->l && !BM_edge_is_boundary(e_iter)) {
      r_e_hflag[0] |= e_iter->head.hflag;
      r_e_hflag[1] &= e_iter->head.hflag;
      ok = true;
    }
  } while ((e_iter = BM_DISK_EDGE_NEXT(e_iter, v)) != v->e);

  if (ok) {
    r_e_hflag[0] &= hflag_enable;
    r_e_hflag[1] = hflag_disable & ~r_e_hflag[1];
  }
  return ok;
}
#endif /* USE_EDGE_REGION_FLAGS */

void bmo_extrude_face_region_exec(BMesh *bm, BMOperator *op)
{
  BMOperator dupeop, delop;
  BMOIter siter;
  BMIter iter, fiter, viter;
  BMEdge *e, *e_new;
  BMVert *v;
  BMFace *f;
  bool found, delorig = false;
  BMOpSlot *slot_facemap_out;
  BMOpSlot *slot_edges_exclude;
  const bool use_normal_flip = BMO_slot_bool_get(op->slots_in, "use_normal_flip");
  const bool use_normal_from_adjacent = BMO_slot_bool_get(op->slots_in,
                                                          "use_normal_from_adjacent");
  const bool use_dissolve_ortho_edges = BMO_slot_bool_get(op->slots_in,
                                                          "use_dissolve_ortho_edges");

  /* initialize our sub-operators */
  BMO_op_initf(bm,
               &dupeop,
               op->flag,
               "duplicate use_select_history=%b",
               BMO_slot_bool_get(op->slots_in, "use_select_history"));

  BMO_slot_buffer_flag_enable(bm, op->slots_in, "geom", BM_EDGE | BM_FACE, EXT_INPUT);

  /* if one flagged face is bordered by an un-flagged face, then we delete
   * original geometry unless caller explicitly asked to keep it. */
  if (!BMO_slot_bool_get(op->slots_in, "use_keep_orig")) {
    BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {

      int edge_face_tot;

      if (!BMO_edge_flag_test(bm, e, EXT_INPUT)) {
        continue;
      }

      found = false;     /* found a face that isn't input? */
      edge_face_tot = 0; /* edge/face count */

      BM_ITER_ELEM (f, &fiter, e, BM_FACES_OF_EDGE) {
        if (!BMO_face_flag_test(bm, f, EXT_INPUT)) {
          found = true;
          delorig = true;
          break;
        }

        edge_face_tot++;
      }

      if ((edge_face_tot > 1) && (found == false)) {
        /* edge has a face user, that face isn't extrude input */
        BMO_edge_flag_enable(bm, e, EXT_DEL);
      }
    }
  }

  /* calculate verts to delete */
  BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
    if (v->e) { /* only deal with verts attached to geometry #33651. */
      found = false;

      BM_ITER_ELEM (e, &viter, v, BM_EDGES_OF_VERT) {
        if (!BMO_edge_flag_test(bm, e, EXT_INPUT) || !BMO_edge_flag_test(bm, e, EXT_DEL)) {
          found = true;
          break;
        }
      }

      /* avoid an extra loop */
      if (found == false) {
        BM_ITER_ELEM (f, &viter, v, BM_FACES_OF_VERT) {
          if (!BMO_face_flag_test(bm, f, EXT_INPUT)) {
            found = true;
            break;
          }
        }
      }

      if (found == false) {
        BMO_vert_flag_enable(bm, v, EXT_DEL);
      }
    }
  }

  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    if (BMO_face_flag_test(bm, f, EXT_INPUT)) {
      BMO_face_flag_enable(bm, f, EXT_DEL);
    }
  }

  if (delorig == true) {
    BMO_op_initf(bm, &delop, op->flag, "delete geom=%fvef context=%i", EXT_DEL, DEL_ONLYTAGGED);
  }

  BMO_slot_copy(op, slots_in, "geom", &dupeop, slots_in, "geom");
  BMO_op_exec(bm, &dupeop);

  /* disable root flag on all new skin nodes */
  if (CustomData_has_layer(&bm->vdata, CD_MVERT_SKIN)) {
    BMO_ITER (v, &siter, dupeop.slots_out, "geom.out", BM_VERT) {
      bm_extrude_disable_skin_root(bm, v);
    }
  }

  slot_facemap_out = BMO_slot_get(dupeop.slots_out, "face_map.out");
  if (bm->act_face && BMO_face_flag_test(bm, bm->act_face, EXT_INPUT)) {
    bm->act_face = static_cast<BMFace *>(BMO_slot_map_elem_get(slot_facemap_out, bm->act_face));
  }

  if (delorig) {
    BMO_op_exec(bm, &delop);
  }

  const bool skip_input_flip = BMO_slot_bool_get(op->slots_in, "skip_input_flip");

  /* Flip input faces only when originals are kept (!delorig)
   * and the caller didn't request to skip flipping (!skip_input_flip).*/
  if (!delorig && !skip_input_flip) {
    BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
      if (BMO_face_flag_test(bm, f, EXT_INPUT)) {
        BM_face_normal_flip(bm, f);
      }
    }
  }

  BMVert **dissolve_verts = nullptr;
  int dissolve_verts_len = 0;
  float average_normal[3];
  if (use_dissolve_ortho_edges) {
    /* Calc average normal. */
    zero_v3(average_normal);
    BMO_ITER (f, &siter, dupeop.slots_out, "geom.out", BM_FACE) {
      add_v3_v3(average_normal, f->no);
    }
    if (normalize_v3(average_normal) == 0.0f) {
      average_normal[2] = 1.0f;
    }

    /* Allocate array to store possible vertices that will be dissolved. */
    int boundary_edges_len = BMO_slot_map_len(dupeop.slots_out, "boundary_map.out");
    /* We do not know the real number of boundary vertices. */
    int boundary_verts_len_maybe = 2 * boundary_edges_len;
    dissolve_verts = static_cast<BMVert **>(
        MEM_mallocN(boundary_verts_len_maybe * sizeof(*dissolve_verts), __func__));
  }

  BMO_slot_copy(&dupeop, slots_out, "geom.out", op, slots_out, "geom.out");

  slot_edges_exclude = BMO_slot_get(op->slots_in, "edges_exclude");
  for (e = static_cast<BMEdge *>(BMO_iter_new(&siter, dupeop.slots_out, "boundary_map.out", 0)); e;
       e = static_cast<BMEdge *>(BMO_iter_step(&siter)))
  {
    BMVert *f_verts[4];
#ifdef USE_EDGE_REGION_FLAGS
    BMEdge *f_edges[4];
#endif

    /* this should always be wire, so this is mainly a speedup to avoid map lookup */
    if (BM_edge_is_wire(e) && BMO_slot_map_contains(slot_edges_exclude, e)) {
      BMVert *v1 = e->v1, *v2 = e->v2;

      /* The original edge was excluded,
       * this would result in a standalone wire edge - see #30399. */
      BM_edge_kill(bm, e);

      /* kill standalone vertices from this edge - see #32341. */
      if (!v1->e) {
        BM_vert_kill(bm, v1);
      }
      if (!v2->e) {
        BM_vert_kill(bm, v2);
      }

      continue;
    }

    /* skip creating face for excluded edges see #35503. */
    if (BMO_slot_map_contains(slot_edges_exclude, e)) {
      /* simply skip creating the face */
      continue;
    }

    e_new = static_cast<BMEdge *>(BMO_iter_map_value_ptr(&siter));

    if (!e_new) {
      continue;
    }

    BMFace *join_face = nullptr;
    if (use_dissolve_ortho_edges) {
      if (BM_edge_is_boundary(e)) {
        join_face = e->l->f;
        if (fabs(dot_v3v3(average_normal, join_face->no)) > 0.0001f) {
          join_face = nullptr;
        }
      }
    }

    bool edge_normal_flip;
    if (use_normal_from_adjacent == false) {
      /* Orient loop to give same normal as a loop of 'e_new'
       * if it exists (will be one of the faces from the region),
       * else same normal as a loop of e, if it exists. */
      edge_normal_flip = !(e_new->l ? (e_new->l->v == e_new->v1) : (!e->l || !(e->l->v == e->v1)));
    }
    else {
      /* Special case, needed for repetitive extrusions
       * that use the normals from the previously created faces. */
      edge_normal_flip = !(e->l && e->v1 != e->l->v);
    }

    if (edge_normal_flip == use_normal_flip) {
      f_verts[0] = e->v1;
      f_verts[1] = e->v2;
      f_verts[2] = e_new->v2;
      f_verts[3] = e_new->v1;
    }
    else {
      f_verts[0] = e->v2;
      f_verts[1] = e->v1;
      f_verts[2] = e_new->v1;
      f_verts[3] = e_new->v2;
    }

#ifdef USE_EDGE_REGION_FLAGS
    /* handle new edges */
    f_edges[0] = e;
    f_edges[2] = e_new;

    f_edges[1] = BM_edge_exists(f_verts[1], f_verts[2]);
    if (f_edges[1] == nullptr) {
      char e_hflag[2];
      bool e_hflag_ok = bm_extrude_region_edge_flag(f_verts[2], e_hflag);
      f_edges[1] = BM_edge_create(bm, f_verts[1], f_verts[2], nullptr, BM_CREATE_NOP);
      if (e_hflag_ok) {
        BM_elem_flag_enable(f_edges[1], e_hflag[0]);
        BM_elem_flag_disable(f_edges[1], e_hflag[1]);
      }
    }

    f_edges[3] = BM_edge_exists(f_verts[3], f_verts[0]);
    if (f_edges[3] == nullptr) {
      char e_hflag[2];
      bool e_hflag_ok = bm_extrude_region_edge_flag(f_verts[3], e_hflag);
      f_edges[3] = BM_edge_create(bm, f_verts[3], f_verts[0], nullptr, BM_CREATE_NOP);
      if (e_hflag_ok) {
        BM_elem_flag_enable(f_edges[3], e_hflag[0]);
        BM_elem_flag_disable(f_edges[3], e_hflag[1]);
      }
    }

    f = BM_face_create(bm, f_verts, f_edges, 4, nullptr, BM_CREATE_NOP);
#else
    f = BM_face_create_verts(bm, f_verts, 4, nullptr, BM_CREATE_NOP, true);
#endif

    bm_extrude_copy_face_loop_attributes(bm, f);
    if (join_face) {
      BMVert *v1 = e->v1;
      BMVert *v2 = e->v2;
      if (!BMO_elem_flag_test(bm, v1, EXT_TAG)) {
        BMO_elem_flag_enable(bm, v1, EXT_TAG);
        dissolve_verts[dissolve_verts_len++] = v1;
      }
      if (!BMO_elem_flag_test(bm, v2, EXT_TAG)) {
        BMO_elem_flag_enable(bm, v2, EXT_TAG);
        dissolve_verts[dissolve_verts_len++] = v2;
      }
      /* Tag the edges that can collapse. */
      BMO_elem_flag_enable(bm, f_edges[0], EXT_TAG);
      BMO_elem_flag_enable(bm, f_edges[1], EXT_TAG);
      bmesh_kernel_join_face_kill_edge(bm, join_face, f, e);
    }
  }

  /* link isolated vert */
  for (v = static_cast<BMVert *>(BMO_iter_new(&siter, dupeop.slots_out, "isovert_map.out", 0)); v;
       v = static_cast<BMVert *>(BMO_iter_step(&siter)))
  {
    BMVert *v2 = static_cast<BMVert *>(BMO_iter_map_value_ptr(&siter));

    /* not essential, but ensures face normals from extruded edges are contiguous */
    if (BM_vert_is_wire_endpoint(v)) {
      if (v->e->v1 == v) {
        std::swap(v, v2);
      }
    }

    BM_edge_create(bm, v, v2, nullptr, BM_CREATE_NO_DOUBLE);
  }

  if (dissolve_verts) {
    BMVert **v_iter = &dissolve_verts[0];
    for (int i = dissolve_verts_len; i--; v_iter++) {
      v = *v_iter;
      e = v->e;
      BMEdge *e_other = BM_DISK_EDGE_NEXT(e, v);
      if ((e_other == e) || (BM_DISK_EDGE_NEXT(e_other, v) == e)) {
        /* Loose edge or BMVert is edge pair. */
        BM_edge_collapse(bm, BMO_elem_flag_test(bm, e, EXT_TAG) ? e : e_other, v, true, true);
      }
      else {
        BLI_assert(!BM_vert_is_edge_pair(v));
      }
    }
    MEM_freeN(dissolve_verts);
  }

  /* cleanup */
  if (delorig) {
    BMO_op_finish(bm, &delop);
  }
  BMO_op_finish(bm, &dupeop);
}

/*
 * Compute higher-quality vertex normals used by solidify.
 * Only considers geometry in the marked solidify region.
 * Note that this does not work so well for non-manifold
 * regions.
 */
static void calc_solidify_normals(BMesh *bm)
{
  BMIter viter, eiter, fiter;
  BMVert *v;
  BMEdge *e;
  BMFace *f, *f1, *f2;
  float edge_normal[3];
  int i;

  /* can't use BM_edge_face_count because we need to count only marked faces */
  int *edge_face_count = MEM_calloc_arrayN<int>(bm->totedge, __func__);

  BM_ITER_MESH (v, &viter, bm, BM_VERTS_OF_MESH) {
    BM_elem_flag_enable(v, BM_ELEM_TAG);
  }

  BM_mesh_elem_index_ensure(bm, BM_EDGE);

  BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
    if (!BMO_face_flag_test(bm, f, FACE_MARK)) {
      continue;
    }

    BM_ITER_ELEM (e, &eiter, f, BM_EDGES_OF_FACE) {

      /* And mark all edges and vertices on the
       * marked faces */
      BMO_edge_flag_enable(bm, e, EDGE_MARK);
      BMO_vert_flag_enable(bm, e->v1, VERT_MARK);
      BMO_vert_flag_enable(bm, e->v2, VERT_MARK);
      edge_face_count[BM_elem_index_get(e)]++;
    }
  }

  BM_ITER_MESH (e, &eiter, bm, BM_EDGES_OF_MESH) {
    if (!BMO_edge_flag_test(bm, e, EDGE_MARK)) {
      continue;
    }

    i = edge_face_count[BM_elem_index_get(e)];

    if (i == 0 || i > 2) {
      /* Edge & vertices are non-manifold even when considering
       * only marked faces */
      BMO_edge_flag_enable(bm, e, EDGE_NONMAN);
      BMO_vert_flag_enable(bm, e->v1, VERT_NONMAN);
      BMO_vert_flag_enable(bm, e->v2, VERT_NONMAN);
    }
  }
  MEM_freeN(edge_face_count);
  edge_face_count = nullptr; /* don't re-use */

  BM_ITER_MESH (v, &viter, bm, BM_VERTS_OF_MESH) {
    if (!BM_vert_is_manifold(v)) {
      BMO_vert_flag_enable(bm, v, VERT_NONMAN);
      continue;
    }

    if (BMO_vert_flag_test(bm, v, VERT_MARK)) {
      zero_v3(v->no);
    }
  }

  BM_ITER_MESH (e, &eiter, bm, BM_EDGES_OF_MESH) {

    /* If the edge is not part of the solidify region
     * its normal should not be considered */
    if (!BMO_edge_flag_test(bm, e, EDGE_MARK)) {
      continue;
    }

    /* If the edge joins more than two marked faces high
     * quality normal computation won't work */
    if (BMO_edge_flag_test(bm, e, EDGE_NONMAN)) {
      continue;
    }

    f1 = f2 = nullptr;

    BM_ITER_ELEM (f, &fiter, e, BM_FACES_OF_EDGE) {
      if (BMO_face_flag_test(bm, f, FACE_MARK)) {
        if (f1 == nullptr) {
          f1 = f;
        }
        else {
          BLI_assert(f2 == nullptr);
          f2 = f;
        }
      }
    }

    BLI_assert(f1 != nullptr);

    if (f2 != nullptr) {
      const float angle = angle_normalized_v3v3(f1->no, f2->no);

      if (angle > 0.0f) {
        /* two faces using this edge, calculate the edge normal
         * using the angle between the faces as a weighting */
        add_v3_v3v3(edge_normal, f1->no, f2->no);
        normalize_v3_length(edge_normal, angle);
      }
      else {
        /* can't do anything useful here!
         * Set the face index for a vert in case it gets a zero normal */
        BM_elem_flag_disable(e->v1, BM_ELEM_TAG);
        BM_elem_flag_disable(e->v2, BM_ELEM_TAG);
        continue;
      }
    }
    else {
      /* only one face attached to that edge */
      /* an edge without another attached- the weight on this is undefined,
       * M_PI_2 is 90d in radians and that seems good enough */
      copy_v3_v3(edge_normal, f1->no);
      mul_v3_fl(edge_normal, M_PI_2);
    }

    add_v3_v3(e->v1->no, edge_normal);
    add_v3_v3(e->v2->no, edge_normal);
  }

  /* normalize accumulated vertex normal */
  BM_ITER_MESH (v, &viter, bm, BM_VERTS_OF_MESH) {
    if (!BMO_vert_flag_test(bm, v, VERT_MARK)) {
      continue;
    }

    if (BMO_vert_flag_test(bm, v, VERT_NONMAN)) {
      /* use standard normals for vertices connected to non-manifold edges */
      BM_vert_normal_update(v);
    }
    else if (normalize_v3(v->no) == 0.0f && !BM_elem_flag_test(v, BM_ELEM_TAG)) {
      /* exceptional case, totally flat. use the normal
       * of any marked face around the vertex */
      BM_ITER_ELEM (f, &fiter, v, BM_FACES_OF_VERT) {
        if (BMO_face_flag_test(bm, f, FACE_MARK)) {
          break;
        }
      }
      copy_v3_v3(v->no, f->no);
    }
  }
}

static void solidify_add_thickness(BMesh *bm, const float dist)
{
  BMFace *f;
  BMVert *v;
  BMLoop *l;
  BMIter iter, loopIter;
  float *vert_angles = MEM_calloc_arrayN<float>(size_t(bm->totvert) * 2, "solidify"); /* 2 in 1 */
  float *vert_accum = vert_angles + bm->totvert;
  int i, index;

  blender::Vector<float, BM_DEFAULT_NGON_STACK_SIZE> face_angles;
  blender::Vector<float *, BM_DEFAULT_NGON_STACK_SIZE> verts;

  BM_mesh_elem_index_ensure(bm, BM_VERT);

  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    if (BMO_face_flag_test(bm, f, FACE_MARK)) {

      /* array for passing verts to angle_poly_v3 */
      face_angles.resize(f->len);
      /* array for receiving angles from angle_poly_v3 */
      verts.resize(f->len);

      BM_ITER_ELEM_INDEX (l, &loopIter, f, BM_LOOPS_OF_FACE, i) {
        verts[i] = l->v->co;
      }

      angle_poly_v3(face_angles.data(), (const float **)verts.data(), f->len);

      i = 0;
      BM_ITER_ELEM (l, &loopIter, f, BM_LOOPS_OF_FACE) {
        v = l->v;
        index = BM_elem_index_get(v);
        vert_accum[index] += face_angles[i];
        vert_angles[index] += shell_v3v3_normalized_to_dist(v->no, f->no) * face_angles[i];
        i++;
      }
    }
  }

  BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
    index = BM_elem_index_get(v);
    if (vert_accum[index]) { /* zero if unselected */
      madd_v3_v3fl(v->co, v->no, dist * (vert_angles[index] / vert_accum[index]));
    }
  }

  MEM_freeN(vert_angles);
}

void bmo_solidify_face_region_exec(BMesh *bm, BMOperator *op)
{
  BMOperator extrudeop;
  BMOperator reverseop;
  float thickness;

  thickness = BMO_slot_float_get(op->slots_in, "thickness");

  /* Flip original faces (so the shell is extruded inward) */
  BMO_op_init(bm, &reverseop, op->flag, "reverse_faces");
  BMO_slot_bool_set(reverseop.slots_in, "flip_multires", true);
  BMO_slot_copy(op, slots_in, "geom", &reverseop, slots_in, "faces");
  BMO_op_exec(bm, &reverseop);
  BMO_op_finish(bm, &reverseop);

  /* Extrude the region */
  BMO_op_initf(bm, &extrudeop, op->flag, "extrude_face_region use_keep_orig=%b", true);
  BMO_slot_copy(op, slots_in, "geom", &extrudeop, slots_in, "geom");
  BMO_op_exec(bm, &extrudeop);

  /* Push the verts of the extruded faces inward to create thickness */
  BMO_slot_buffer_flag_enable(bm, extrudeop.slots_out, "geom.out", BM_FACE, FACE_MARK);
  calc_solidify_normals(bm);
  solidify_add_thickness(bm, thickness);

  BMO_slot_copy(&extrudeop, slots_out, "geom.out", op, slots_out, "geom.out");

  BMO_op_finish(bm, &extrudeop);
}
