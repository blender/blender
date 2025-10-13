/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bmesh
 *
 * Cut meshes along intersections.
 *
 * Boolean-like modeling operation (without calculating inside/outside).
 *
 * Supported:
 * - Concave faces.
 * - Non-planar faces.
 * - Custom-data (UVs etc).
 *
 * Unsupported:
 * - Intersecting between different meshes.
 * - No support for holes (cutting a hole into a single face).
 */

#include "MEM_guardedalloc.h"

#include "BLI_alloca.h"
#include "BLI_linklist.h"
#include "BLI_math_geom.h"
#include "BLI_math_vector.h"
#include "BLI_memarena.h"
#include "BLI_sort_utils.h"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include "BLI_utildefines_stack.h"

#include "BLI_kdopbvh.hh"

#include "bmesh.hh"
#include "intern/bmesh_private.hh"

#include "bmesh_intersect.hh" /* own include */

#include "tools/bmesh_edgesplit.hh"

#include "BLI_strict_flags.h" /* IWYU pragma: keep. Keep last. */

/*
 * Some of these depend on each other:
 */

/* splice verts into existing edges */
#define USE_SPLICE
/* split faces by intersecting edges */
#define USE_NET
/* split resulting edges */
#define USE_SEPARATE
/* remove verts created by intersecting triangles */
#define USE_DISSOLVE
/* detect isolated holes and fill them */
#define USE_NET_ISLAND_CONNECT

/* strict asserts that may fail in practice (handy for debugging cases which should succeed) */
// #define USE_PARANOID
/* use accelerated overlap check */
#define USE_BVH

// #define USE_DUMP

static void tri_v3_scale(float v1[3], float v2[3], float v3[3], const float t)
{
  float p[3];

  mid_v3_v3v3v3(p, v1, v2, v3);

  interp_v3_v3v3(v1, p, v1, t);
  interp_v3_v3v3(v2, p, v2, t);
  interp_v3_v3v3(v3, p, v3, t);
}

#ifdef USE_DISSOLVE
/* other edge when a vert only has 2 edges */
static BMEdge *bm_vert_other_edge(BMVert *v, BMEdge *e)
{
  BLI_assert(BM_vert_is_edge_pair(v));
  BLI_assert(BM_vert_in_edge(e, v));

  if (v->e != e) {
    return v->e;
  }
  return BM_DISK_EDGE_NEXT(v->e, v);
}
#endif

enum ISectType {
  IX_NONE = -1,
  IX_EDGE_TRI_EDGE0,
  IX_EDGE_TRI_EDGE1,
  IX_EDGE_TRI_EDGE2,
  IX_EDGE_TRI,
  IX_TOT,
};

struct ISectEpsilon {
  float eps, eps_sq;
  float eps2x, eps2x_sq;
  float eps_margin, eps_margin_sq;
};

struct ISectState {
  BMesh *bm;
  GHash *edgetri_cache;    /* int[4]: BMVert */
  GHash *edge_verts;       /* BMEdge: LinkList(of verts), new and original edges */
  GHash *face_edges;       /* BMFace-index: LinkList(of edges), only original faces */
  GSet *wire_edges;        /* BMEdge  (could use tags instead) */
  LinkNode *vert_dissolve; /* BMVert's */

  MemArena *mem_arena;

  ISectEpsilon epsilon;
};

/**
 * Store as value in GHash so we can get list-length without counting every time.
 * Also means we don't need to update the GHash value each time.
 */
struct LinkBase {
  LinkNode *list;
  uint list_len;
};

static bool ghash_insert_link(GHash *gh, void *key, void *val, bool use_test, MemArena *mem_arena)
{
  void **ls_base_p;
  LinkBase *ls_base;
  LinkNode *ls;

  if (!BLI_ghash_ensure_p(gh, key, &ls_base_p)) {
    ls_base = static_cast<LinkBase *>(*ls_base_p = BLI_memarena_alloc(mem_arena,
                                                                      sizeof(*ls_base)));
    ls_base->list = nullptr;
    ls_base->list_len = 0;
  }
  else {
    ls_base = static_cast<LinkBase *>(*ls_base_p);
    if (use_test && (BLI_linklist_index(ls_base->list, val) != -1)) {
      return false;
    }
  }

  ls = static_cast<LinkNode *>(BLI_memarena_alloc(mem_arena, sizeof(*ls)));
  ls->next = ls_base->list;
  ls->link = val;
  ls_base->list = ls;
  ls_base->list_len += 1;

  return true;
}

struct VertSort {
  float val;
  BMVert *v;
};

#ifdef USE_SPLICE
static void edge_verts_sort(const float co[3], LinkBase *v_ls_base)
{
  /* not optimal but list will be typically < 5 */
  uint i;
  VertSort *vert_sort = BLI_array_alloca(vert_sort, v_ls_base->list_len);
  LinkNode *node;

  BLI_assert(v_ls_base->list_len > 1);

  for (i = 0, node = v_ls_base->list; i < v_ls_base->list_len; i++, node = node->next) {
    BMVert *v = static_cast<BMVert *>(node->link);
    BLI_assert(v->head.htype == BM_VERT);
    vert_sort[i].val = len_squared_v3v3(co, v->co);
    vert_sort[i].v = v;
  }

  qsort(vert_sort, v_ls_base->list_len, sizeof(*vert_sort), BLI_sortutil_cmp_float);

  for (i = 0, node = v_ls_base->list; i < v_ls_base->list_len; i++, node = node->next) {
    node->link = vert_sort[i].v;
  }
}
#endif

static void edge_verts_add(ISectState *s, BMEdge *e, BMVert *v, const bool use_test)
{
  BLI_assert(e->head.htype == BM_EDGE);
  BLI_assert(v->head.htype == BM_VERT);
  ghash_insert_link(s->edge_verts, (void *)e, v, use_test, s->mem_arena);
}

static void face_edges_add(ISectState *s, const int f_index, BMEdge *e, const bool use_test)
{
  void *f_index_key = POINTER_FROM_INT(f_index);
  BLI_assert(e->head.htype == BM_EDGE);
  BLI_assert(BM_edge_in_face(e, s->bm->ftable[f_index]) == false);
  BLI_assert(BM_elem_index_get(s->bm->ftable[f_index]) == f_index);

  ghash_insert_link(s->face_edges, f_index_key, e, use_test, s->mem_arena);
}

#ifdef USE_NET
static void face_edges_split(BMesh *bm,
                             BMFace *f,
                             LinkBase *e_ls_base,
                             bool use_island_connect,
                             bool use_partial_connect,
                             MemArena *mem_arena_edgenet)
{
  uint i;
  uint edge_arr_len = e_ls_base->list_len;
  BMEdge **edge_arr = BLI_array_alloca(edge_arr, edge_arr_len);
  LinkNode *node;
  BLI_assert(f->head.htype == BM_FACE);

  for (i = 0, node = e_ls_base->list; i < e_ls_base->list_len; i++, node = node->next) {
    edge_arr[i] = static_cast<BMEdge *>(node->link);
  }
  BLI_assert(node == nullptr);

#  ifdef USE_DUMP
  printf("# %s: %d %u\n", __func__, BM_elem_index_get(f), e_ls_base->list_len);
#  endif

#  ifdef USE_NET_ISLAND_CONNECT
  if (use_island_connect) {
    uint edge_arr_holes_len;
    BMEdge **edge_arr_holes;
    if (BM_face_split_edgenet_connect_islands(bm,
                                              f,
                                              edge_arr,
                                              edge_arr_len,
                                              use_partial_connect,
                                              mem_arena_edgenet,
                                              &edge_arr_holes,
                                              &edge_arr_holes_len))
    {
      edge_arr_len = edge_arr_holes_len;
      edge_arr = edge_arr_holes; /* owned by the arena */
    }
  }
#  else
  UNUSED_VARS(use_island_connect, mem_arena_edgenet);
#  endif

  BM_face_split_edgenet(bm, f, edge_arr, int(edge_arr_len), nullptr);
}
#endif

#ifdef USE_DISSOLVE
static void vert_dissolve_add(ISectState *s, BMVert *v)
{
  BLI_assert(v->head.htype == BM_VERT);
  BLI_assert(!BM_elem_flag_test(v, BM_ELEM_TAG));
  BLI_assert(BLI_linklist_index(s->vert_dissolve, v) == -1);

  BM_elem_flag_enable(v, BM_ELEM_TAG);
  BLI_linklist_prepend_arena(&s->vert_dissolve, v, s->mem_arena);
}
#endif

static enum ISectType intersect_line_tri(const float p0[3],
                                         const float p1[3],
                                         const float *t_cos[3],
                                         const float t_nor[3],
                                         float r_ix[3],
                                         const ISectEpsilon *e)
{
  float p_dir[3];
  uint i_t0;
  float fac;

  sub_v3_v3v3(p_dir, p0, p1);
  normalize_v3(p_dir);

  for (i_t0 = 0; i_t0 < 3; i_t0++) {
    const uint i_t1 = (i_t0 + 1) % 3;
    float te_dir[3];

    sub_v3_v3v3(te_dir, t_cos[i_t0], t_cos[i_t1]);
    normalize_v3(te_dir);
    if (fabsf(dot_v3v3(p_dir, te_dir)) >= 1.0f - e->eps) {
      /* co-linear */
    }
    else {
      float ix_pair[2][3];
      int ix_pair_type;

      ix_pair_type = isect_line_line_epsilon_v3(
          p0, p1, t_cos[i_t0], t_cos[i_t1], ix_pair[0], ix_pair[1], 0.0f);

      if (ix_pair_type != 0) {
        if (ix_pair_type == 1) {
          copy_v3_v3(ix_pair[1], ix_pair[0]);
        }

        if ((ix_pair_type == 1) || (len_squared_v3v3(ix_pair[0], ix_pair[1]) <= e->eps_margin_sq))
        {
          fac = line_point_factor_v3(ix_pair[1], t_cos[i_t0], t_cos[i_t1]);
          if ((fac >= e->eps_margin) && (fac <= 1.0f - e->eps_margin)) {
            fac = line_point_factor_v3(ix_pair[0], p0, p1);
            if ((fac >= e->eps_margin) && (fac <= 1.0f - e->eps_margin)) {
              copy_v3_v3(r_ix, ix_pair[0]);
              return ISectType(IX_EDGE_TRI_EDGE0 + (enum ISectType)i_t0);
            }
          }
        }
      }
    }
  }

  /* check ray isn't planar with tri */
  if (fabsf(dot_v3v3(p_dir, t_nor)) >= e->eps) {
    if (isect_line_segment_tri_epsilon_v3(
            p0, p1, t_cos[0], t_cos[1], t_cos[2], &fac, nullptr, 0.0f))
    {
      if ((fac >= e->eps_margin) && (fac <= 1.0f - e->eps_margin)) {
        interp_v3_v3v3(r_ix, p0, p1, fac);
        if (min_fff(len_squared_v3v3(t_cos[0], r_ix),
                    len_squared_v3v3(t_cos[1], r_ix),
                    len_squared_v3v3(t_cos[2], r_ix)) >= e->eps_margin_sq)
        {
          return IX_EDGE_TRI;
        }
      }
    }
  }

  /* r_ix may be unset */
  return IX_NONE;
}

static BMVert *bm_isect_edge_tri(ISectState *s,
                                 BMVert *e_v0,
                                 BMVert *e_v1,
                                 BMVert *t[3],
                                 const int t_index,
                                 const float *t_cos[3],
                                 const float t_nor[3],
                                 enum ISectType *r_side)
{
  BMesh *bm = s->bm;
  int k_arr[IX_TOT][4];
  uint i;
  const int ti[3] = {UNPACK3_EX(BM_elem_index_get, t, )};
  float ix[3];

  if (BM_elem_index_get(e_v0) > BM_elem_index_get(e_v1)) {
    std::swap(e_v0, e_v1);
  }

#ifdef USE_PARANOID
  BLI_assert(len_squared_v3v3(e_v0->co, t[0]->co) >= s->epsilon.eps_sq);
  BLI_assert(len_squared_v3v3(e_v0->co, t[1]->co) >= s->epsilon.eps_sq);
  BLI_assert(len_squared_v3v3(e_v0->co, t[2]->co) >= s->epsilon.eps_sq);
  BLI_assert(len_squared_v3v3(e_v1->co, t[0]->co) >= s->epsilon.eps_sq);
  BLI_assert(len_squared_v3v3(e_v1->co, t[1]->co) >= s->epsilon.eps_sq);
  BLI_assert(len_squared_v3v3(e_v1->co, t[2]->co) >= s->epsilon.eps_sq);
#endif

#define KEY_SET(k, i0, i1, i2, i3) \
  { \
    (k)[0] = i0; \
    (k)[1] = i1; \
    (k)[2] = i2; \
    (k)[3] = i3; \
  } \
  (void)0

/* Order tri, then order (1-2, 2-3). */
#define KEY_EDGE_TRI_ORDER(k) \
  { \
    if (k[2] > k[3]) { \
      std::swap(k[2], k[3]); \
    } \
    if (k[0] > k[2]) { \
      std::swap(k[0], k[2]); \
      std::swap(k[1], k[3]); \
    } \
  } \
  (void)0

  KEY_SET(k_arr[IX_EDGE_TRI], BM_elem_index_get(e_v0), BM_elem_index_get(e_v1), t_index, -1);
  /* need to order here */
  KEY_SET(
      k_arr[IX_EDGE_TRI_EDGE0], BM_elem_index_get(e_v0), BM_elem_index_get(e_v1), ti[0], ti[1]);
  KEY_SET(
      k_arr[IX_EDGE_TRI_EDGE1], BM_elem_index_get(e_v0), BM_elem_index_get(e_v1), ti[1], ti[2]);
  KEY_SET(
      k_arr[IX_EDGE_TRI_EDGE2], BM_elem_index_get(e_v0), BM_elem_index_get(e_v1), ti[2], ti[0]);

  KEY_EDGE_TRI_ORDER(k_arr[IX_EDGE_TRI_EDGE0]);
  KEY_EDGE_TRI_ORDER(k_arr[IX_EDGE_TRI_EDGE1]);
  KEY_EDGE_TRI_ORDER(k_arr[IX_EDGE_TRI_EDGE2]);

#undef KEY_SET
#undef KEY_EDGE_TRI_ORDER

  for (i = 0; i < ARRAY_SIZE(k_arr); i++) {
    BMVert *iv;

    iv = static_cast<BMVert *>(BLI_ghash_lookup(s->edgetri_cache, k_arr[i]));

    if (iv) {
#ifdef USE_DUMP
      printf("# cache hit (%d, %d, %d, %d)\n", UNPACK4(k_arr[i]));
#endif
      *r_side = (enum ISectType)i;
      return iv;
    }
  }

  *r_side = intersect_line_tri(e_v0->co, e_v1->co, t_cos, t_nor, ix, &s->epsilon);
  if (*r_side != IX_NONE) {
    BMVert *iv;
    BMEdge *e;
#ifdef USE_DUMP
    printf("# new vertex (%.6f, %.6f, %.6f) %d\n", UNPACK3(ix), *r_side);
#endif

#ifdef USE_PARANOID
    BLI_assert(len_squared_v3v3(ix, e_v0->co) > s->epsilon.eps_sq);
    BLI_assert(len_squared_v3v3(ix, e_v1->co) > s->epsilon.eps_sq);
    BLI_assert(len_squared_v3v3(ix, t[0]->co) > s->epsilon.eps_sq);
    BLI_assert(len_squared_v3v3(ix, t[1]->co) > s->epsilon.eps_sq);
    BLI_assert(len_squared_v3v3(ix, t[2]->co) > s->epsilon.eps_sq);
#endif
    iv = BM_vert_create(bm, ix, nullptr, eBMCreateFlag(0));

    e = BM_edge_exists(e_v0, e_v1);
    if (e) {
#ifdef USE_DUMP
      printf("# adding to edge %d\n", BM_elem_index_get(e));
#endif
      edge_verts_add(s, e, iv, false);
    }
    else {
#ifdef USE_DISSOLVE
      vert_dissolve_add(s, iv);
#endif
    }

    if ((*r_side >= IX_EDGE_TRI_EDGE0) && (*r_side <= IX_EDGE_TRI_EDGE2)) {
      i = uint(*r_side - IX_EDGE_TRI_EDGE0);
      e = BM_edge_exists(t[i], t[(i + 1) % 3]);
      if (e) {
        edge_verts_add(s, e, iv, false);
      }
    }

    {
      int *k = static_cast<int *>(BLI_memarena_alloc(s->mem_arena, sizeof(int[4])));
      memcpy(k, k_arr[*r_side], sizeof(int[4]));
      BLI_ghash_insert(s->edgetri_cache, k, iv);
    }

    return iv;
  }

  *r_side = IX_NONE;

  return nullptr;
}

struct LoopFilterWrap {
  int (*test_fn)(BMFace *f, void *user_data);
  void *user_data;
};

static bool bm_loop_filter_fn(const BMLoop *l, void *user_data)
{
  if (BM_elem_flag_test(l->e, BM_ELEM_TAG)) {
    return false;
  }

  if (l->radial_next != l) {
    LoopFilterWrap *data = static_cast<LoopFilterWrap *>(user_data);
    BMLoop *l_iter = l->radial_next;
    const int face_side = data->test_fn(l->f, data->user_data);
    do {
      const int face_side_other = data->test_fn(l_iter->f, data->user_data);
      if (UNLIKELY(face_side_other == -1)) {
        /* pass */
      }
      else if (face_side_other != face_side) {
        return false;
      }
    } while ((l_iter = l_iter->radial_next) != l);
    return true;
  }
  return false;
}

/**
 * Return true if we have any intersections.
 */
static void bm_isect_tri_tri(ISectState *s,
                             int a_index,
                             int b_index,
                             const std::array<BMLoop *, 3> &a,
                             const std::array<BMLoop *, 3> &b,
                             bool no_shared)
{
  BMFace *f_a = a[0]->f;
  BMFace *f_b = b[0]->f;
  BMVert *fv_a[3] = {UNPACK3_EX(, a, ->v)};
  BMVert *fv_b[3] = {UNPACK3_EX(, b, ->v)};
  const float *f_a_cos[3] = {UNPACK3_EX(, fv_a, ->co)};
  const float *f_b_cos[3] = {UNPACK3_EX(, fv_b, ->co)};
  float f_a_nor[3];
  float f_b_nor[3];
  uint i;

  /* should be enough but may need to bump */
  BMVert *iv_ls_a[8];
  BMVert *iv_ls_b[8];
  STACK_DECLARE(iv_ls_a);
  STACK_DECLARE(iv_ls_b);

  if (no_shared) {
    if (UNLIKELY(ELEM(fv_a[0], UNPACK3(fv_b)) || ELEM(fv_a[1], UNPACK3(fv_b)) ||
                 ELEM(fv_a[2], UNPACK3(fv_b))))
    {
      return;
    }
  }
  else {
    if (UNLIKELY(BM_face_share_edge_check(f_a, f_b))) {
      return;
    }
  }

  STACK_INIT(iv_ls_a, ARRAY_SIZE(iv_ls_a));
  STACK_INIT(iv_ls_b, ARRAY_SIZE(iv_ls_b));

#define VERT_VISIT_A _FLAG_WALK
#define VERT_VISIT_B _FLAG_WALK_ALT

#define STACK_PUSH_TEST_A(ele) \
  if (BM_ELEM_API_FLAG_TEST(ele, VERT_VISIT_A) == 0) { \
    BM_ELEM_API_FLAG_ENABLE(ele, VERT_VISIT_A); \
    STACK_PUSH(iv_ls_a, ele); \
  } \
  ((void)0)

#define STACK_PUSH_TEST_B(ele) \
  if (BM_ELEM_API_FLAG_TEST(ele, VERT_VISIT_B) == 0) { \
    BM_ELEM_API_FLAG_ENABLE(ele, VERT_VISIT_B); \
    STACK_PUSH(iv_ls_b, ele); \
  } \
  ((void)0)

  /* vert-vert
   * --------- */
  {
    /* first check if any verts are touching
     * (any case where we won't create new verts)
     */
    uint i_a;
    for (i_a = 0; i_a < 3; i_a++) {
      uint i_b;
      for (i_b = 0; i_b < 3; i_b++) {
        if (len_squared_v3v3(fv_a[i_a]->co, fv_b[i_b]->co) <= s->epsilon.eps2x_sq) {
#ifdef USE_DUMP
          if (BM_ELEM_API_FLAG_TEST(fv_a[i_a], VERT_VISIT_A) == 0) {
            printf("  ('VERT-VERT-A') %u, %d),\n", i_a, BM_elem_index_get(fv_a[i_a]));
          }
          if (BM_ELEM_API_FLAG_TEST(fv_b[i_b], VERT_VISIT_B) == 0) {
            printf("  ('VERT-VERT-B') %u, %d),\n", i_b, BM_elem_index_get(fv_b[i_b]));
          }
#endif
          STACK_PUSH_TEST_A(fv_a[i_a]);
          STACK_PUSH_TEST_B(fv_b[i_b]);
        }
      }
    }
  }

  /* vert-edge
   * --------- */
  {
    uint i_a;
    for (i_a = 0; i_a < 3; i_a++) {
      if (BM_ELEM_API_FLAG_TEST(fv_a[i_a], VERT_VISIT_A) == 0) {
        uint i_b_e0;
        for (i_b_e0 = 0; i_b_e0 < 3; i_b_e0++) {
          uint i_b_e1 = (i_b_e0 + 1) % 3;

          if (BM_ELEM_API_FLAG_TEST(fv_b[i_b_e0], VERT_VISIT_B) ||
              BM_ELEM_API_FLAG_TEST(fv_b[i_b_e1], VERT_VISIT_B))
          {
            continue;
          }

          const float fac = line_point_factor_v3(
              fv_a[i_a]->co, fv_b[i_b_e0]->co, fv_b[i_b_e1]->co);
          if ((fac > 0.0f - s->epsilon.eps) && (fac < 1.0f + s->epsilon.eps)) {
            float ix[3];
            interp_v3_v3v3(ix, fv_b[i_b_e0]->co, fv_b[i_b_e1]->co, fac);
            if (len_squared_v3v3(ix, fv_a[i_a]->co) <= s->epsilon.eps2x_sq) {
              BMEdge *e;
              STACK_PUSH_TEST_B(fv_a[i_a]);
              // STACK_PUSH_TEST_A(fv_a[i_a]);
              e = BM_edge_exists(fv_b[i_b_e0], fv_b[i_b_e1]);
#ifdef USE_DUMP
              printf("  ('VERT-EDGE-A', %d, %d),\n",
                     BM_elem_index_get(fv_b[i_b_e0]),
                     BM_elem_index_get(fv_b[i_b_e1]));
#endif
              if (e) {
#ifdef USE_DUMP
                printf("# adding to edge %d\n", BM_elem_index_get(e));
#endif
                edge_verts_add(s, e, fv_a[i_a], true);
              }
              break;
            }
          }
        }
      }
    }
  }

  {
    uint i_b;
    for (i_b = 0; i_b < 3; i_b++) {
      if (BM_ELEM_API_FLAG_TEST(fv_b[i_b], VERT_VISIT_B) == 0) {
        uint i_a_e0;
        for (i_a_e0 = 0; i_a_e0 < 3; i_a_e0++) {
          uint i_a_e1 = (i_a_e0 + 1) % 3;

          if (BM_ELEM_API_FLAG_TEST(fv_a[i_a_e0], VERT_VISIT_A) ||
              BM_ELEM_API_FLAG_TEST(fv_a[i_a_e1], VERT_VISIT_A))
          {
            continue;
          }

          const float fac = line_point_factor_v3(
              fv_b[i_b]->co, fv_a[i_a_e0]->co, fv_a[i_a_e1]->co);
          if ((fac > 0.0f - s->epsilon.eps) && (fac < 1.0f + s->epsilon.eps)) {
            float ix[3];
            interp_v3_v3v3(ix, fv_a[i_a_e0]->co, fv_a[i_a_e1]->co, fac);
            if (len_squared_v3v3(ix, fv_b[i_b]->co) <= s->epsilon.eps2x_sq) {
              BMEdge *e;
              STACK_PUSH_TEST_A(fv_b[i_b]);
              // STACK_PUSH_NOTEST(iv_ls_b, fv_b[i_b]);
              e = BM_edge_exists(fv_a[i_a_e0], fv_a[i_a_e1]);
#ifdef USE_DUMP
              printf("  ('VERT-EDGE-B', %d, %d),\n",
                     BM_elem_index_get(fv_a[i_a_e0]),
                     BM_elem_index_get(fv_a[i_a_e1]));
#endif
              if (e) {
#ifdef USE_DUMP
                printf("# adding to edge %d\n", BM_elem_index_get(e));
#endif
                edge_verts_add(s, e, fv_b[i_b], true);
              }
              break;
            }
          }
        }
      }
    }
  }

  /* vert-tri
   * -------- */
  {

    float t_scale[3][3];
    uint i_a;

    copy_v3_v3(t_scale[0], fv_b[0]->co);
    copy_v3_v3(t_scale[1], fv_b[1]->co);
    copy_v3_v3(t_scale[2], fv_b[2]->co);
    tri_v3_scale(UNPACK3(t_scale), 1.0f - s->epsilon.eps2x);

    /* second check for verts intersecting the triangle */
    for (i_a = 0; i_a < 3; i_a++) {
      if (BM_ELEM_API_FLAG_TEST(fv_a[i_a], VERT_VISIT_A)) {
        continue;
      }

      float ix[3];
      if (isect_point_tri_v3(fv_a[i_a]->co, UNPACK3(t_scale), ix)) {
        if (len_squared_v3v3(ix, fv_a[i_a]->co) <= s->epsilon.eps2x_sq) {
          STACK_PUSH_TEST_A(fv_a[i_a]);
          STACK_PUSH_TEST_B(fv_a[i_a]);
#ifdef USE_DUMP
          printf("  'VERT TRI-A',\n");
#endif
        }
      }
    }
  }

  {
    float t_scale[3][3];
    uint i_b;

    copy_v3_v3(t_scale[0], fv_a[0]->co);
    copy_v3_v3(t_scale[1], fv_a[1]->co);
    copy_v3_v3(t_scale[2], fv_a[2]->co);
    tri_v3_scale(UNPACK3(t_scale), 1.0f - s->epsilon.eps2x);

    for (i_b = 0; i_b < 3; i_b++) {
      if (BM_ELEM_API_FLAG_TEST(fv_b[i_b], VERT_VISIT_B)) {
        continue;
      }

      float ix[3];
      if (isect_point_tri_v3(fv_b[i_b]->co, UNPACK3(t_scale), ix)) {
        if (len_squared_v3v3(ix, fv_b[i_b]->co) <= s->epsilon.eps2x_sq) {
          STACK_PUSH_TEST_A(fv_b[i_b]);
          STACK_PUSH_TEST_B(fv_b[i_b]);
#ifdef USE_DUMP
          printf("  'VERT TRI-B',\n");
#endif
        }
      }
    }
  }

  if ((STACK_SIZE(iv_ls_a) >= 3) && (STACK_SIZE(iv_ls_b) >= 3)) {
#ifdef USE_DUMP
    printf("# OVERLAP\n");
#endif
    goto finally;
  }

  normal_tri_v3(f_a_nor, UNPACK3(f_a_cos));
  normal_tri_v3(f_b_nor, UNPACK3(f_b_cos));

  /* edge-tri & edge-edge
   * -------------------- */
  {
    for (uint i_a_e0 = 0; i_a_e0 < 3; i_a_e0++) {
      uint i_a_e1 = (i_a_e0 + 1) % 3;
      enum ISectType side;
      BMVert *iv;

      if (BM_ELEM_API_FLAG_TEST(fv_a[i_a_e0], VERT_VISIT_A) ||
          BM_ELEM_API_FLAG_TEST(fv_a[i_a_e1], VERT_VISIT_A))
      {
        continue;
      }

      iv = bm_isect_edge_tri(
          s, fv_a[i_a_e0], fv_a[i_a_e1], fv_b, b_index, f_b_cos, f_b_nor, &side);
      if (iv) {
        STACK_PUSH_TEST_A(iv);
        STACK_PUSH_TEST_B(iv);
#ifdef USE_DUMP
        printf("  ('EDGE-TRI-A', %d),\n", side);
#endif
      }
    }

    for (uint i_b_e0 = 0; i_b_e0 < 3; i_b_e0++) {
      uint i_b_e1 = (i_b_e0 + 1) % 3;
      enum ISectType side;
      BMVert *iv;

      if (BM_ELEM_API_FLAG_TEST(fv_b[i_b_e0], VERT_VISIT_B) ||
          BM_ELEM_API_FLAG_TEST(fv_b[i_b_e1], VERT_VISIT_B))
      {
        continue;
      }

      iv = bm_isect_edge_tri(
          s, fv_b[i_b_e0], fv_b[i_b_e1], fv_a, a_index, f_a_cos, f_a_nor, &side);
      if (iv) {
        STACK_PUSH_TEST_A(iv);
        STACK_PUSH_TEST_B(iv);
#ifdef USE_DUMP
        printf("  ('EDGE-TRI-B', %d),\n", side);
#endif
      }
    }
  }

  {
    for (i = 0; i < 2; i++) {
      BMVert **ie_vs;
      BMFace *f;
      bool ie_exists;
      BMEdge *ie;

      if (i == 0) {
        if (STACK_SIZE(iv_ls_a) != 2) {
          continue;
        }
        ie_vs = iv_ls_a;
        f = f_a;
      }
      else {
        if (STACK_SIZE(iv_ls_b) != 2) {
          continue;
        }
        ie_vs = iv_ls_b;
        f = f_b;
      }

      /* possible but unlikely we get this - for edge-edge intersection */
      ie = BM_edge_exists(UNPACK2(ie_vs));
      if (ie == nullptr) {
        ie_exists = false;
        /* one of the verts must be new if we are making an edge
         * ...no, we need this in case 2x quads intersect at either ends.
         * if not (ie_vs[0].index == -1 or ie_vs[1].index == -1):
         *     continue */
        ie = BM_edge_create(s->bm, UNPACK2(ie_vs), nullptr, eBMCreateFlag(0));
        BLI_gset_insert(s->wire_edges, ie);
      }
      else {
        ie_exists = true;
        /* may already exist */
        BLI_gset_add(s->wire_edges, ie);

        if (BM_edge_in_face(ie, f)) {
          continue;
        }
      }

      face_edges_add(s, BM_elem_index_get(f), ie, ie_exists);
      // BLI_assert(len(ie_vs) <= 2)
    }
  }

finally:
  for (i = 0; i < STACK_SIZE(iv_ls_a); i++) {
    BM_ELEM_API_FLAG_DISABLE(iv_ls_a[i], VERT_VISIT_A);
  }
  for (i = 0; i < STACK_SIZE(iv_ls_b); i++) {
    BM_ELEM_API_FLAG_DISABLE(iv_ls_b[i], VERT_VISIT_B);
  }
}

#ifdef USE_BVH

struct RaycastData {
  const float **looptris;
  blender::Vector<float, 64> *z_buffer;
};

#  ifdef USE_KDOPBVH_WATERTIGHT
static const IsectRayPrecalc isect_precalc_x = {1, 2, 0, 0, 0, 1};
#  endif

static void raycast_callback(void *userdata,
                             int index,
                             const BVHTreeRay *ray,
                             BVHTreeRayHit * /*hit*/)
{
  RaycastData *raycast_data = static_cast<RaycastData *>(userdata);
  const float **looptris = raycast_data->looptris;
  const float *v0 = looptris[index * 3 + 0];
  const float *v1 = looptris[index * 3 + 1];
  const float *v2 = looptris[index * 3 + 2];
  float dist;

  if (
#  ifdef USE_KDOPBVH_WATERTIGHT
      isect_ray_tri_watertight_v3(ray->origin, &isect_precalc_x, v0, v1, v2, &dist, nullptr)
#  else
      isect_ray_tri_epsilon_v3(
          ray->origin, ray->direction, v0, v1, v2, &dist, nullptr, FLT_EPSILON)
#  endif
  )
  {
    if (dist >= 0.0f) {
#  ifdef USE_DUMP
      printf("%s:\n", __func__);
      print_v3("  origin", ray->origin);
      print_v3("  direction", ray->direction);
      printf("  dist %f\n", dist);
      print_v3("  v0", v0);
      print_v3("  v1", v1);
      print_v3("  v2", v2);
#  endif

#  ifdef USE_DUMP
      printf("%s: Adding depth %f\n", __func__, dist);
#  endif
      raycast_data->z_buffer->append(dist);
    }
  }
}

static int isect_bvhtree_point_v3(BVHTree *tree, const float **looptris, const float co[3])
{
  blender::Vector<float, 64> z_buffer;

  RaycastData raycast_data = {
      looptris,
      &z_buffer,
  };
  BVHTreeRayHit hit = {0};
  const float dir[3] = {1.0f, 0.0f, 0.0f};

  /* Need to initialize hit even tho it's not used.
   * This is to make it so KD-tree believes we didn't intersect anything and
   * keeps calling the intersect callback.
   */
  hit.index = -1;
  hit.dist = BVH_RAYCAST_DIST_MAX;

  BLI_bvhtree_ray_cast(tree, co, dir, 0.0f, &hit, raycast_callback, &raycast_data);

#  ifdef USE_DUMP
  printf("%s: Total intersections: %zu\n", __func__, z_buffer.count);
#  endif

  int num_isect;

  if (z_buffer.is_empty()) {
    num_isect = 0;
  }
  else if (z_buffer.size() == 1) {
    num_isect = 1;
  }
  else {
    /* 2 or more */
    const float eps = FLT_EPSILON * 10;
    num_isect = 1; /* always count first */

    std::sort(z_buffer.begin(), z_buffer.end());

    const float *depth_arr = z_buffer.data();
    float depth_last = depth_arr[0];

    for (uint i = 1; i < z_buffer.size(); i++) {
      if (depth_arr[i] - depth_last > eps) {
        depth_last = depth_arr[i];
        num_isect++;
      }
    }
  }

  //  return (num_isect & 1) == 1;
  return num_isect;
}

#endif /* USE_BVH */

bool BM_mesh_intersect(BMesh *bm,
                       const blender::Span<std::array<BMLoop *, 3>> looptris,
                       int (*test_fn)(BMFace *f, void *user_data),
                       void *user_data,
                       const bool use_self,
                       const bool use_separate,
                       const bool use_dissolve,
                       const bool use_island_connect,
                       const bool use_partial_connect,
                       const bool use_edge_tag,
                       const int boolean_mode,
                       const float eps)
{
  ISectState s;
  const int totface_orig = bm->totface;

  /* use to check if we made any changes */
  bool has_edit_isect = false, has_edit_boolean = false;

  /* needed for boolean, since cutting up faces moves the loops within the face */
  const float **looptri_coords = nullptr;

#ifdef USE_BVH
  BVHTree *tree_a, *tree_b;
  uint tree_overlap_tot;
  BVHTreeOverlap *overlap;
#else
  int i_a, i_b;
#endif

  s.bm = bm;

  s.edgetri_cache = BLI_ghash_new(
      BLI_ghashutil_inthash_v4_p, BLI_ghashutil_inthash_v4_cmp, __func__);

  s.edge_verts = BLI_ghash_ptr_new(__func__);
  s.face_edges = BLI_ghash_int_new(__func__);
  s.wire_edges = BLI_gset_ptr_new(__func__);
  s.vert_dissolve = nullptr;

  s.mem_arena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);

  /* setup epsilon from base */
  s.epsilon.eps = eps;
  s.epsilon.eps2x = eps * 2.0f;
  s.epsilon.eps_margin = s.epsilon.eps2x * 10.0f;

  s.epsilon.eps_sq = s.epsilon.eps * s.epsilon.eps;
  s.epsilon.eps2x_sq = s.epsilon.eps2x * s.epsilon.eps2x;
  s.epsilon.eps_margin_sq = s.epsilon.eps_margin * s.epsilon.eps_margin;

  BM_mesh_elem_index_ensure(bm,
                            BM_VERT | BM_EDGE |
#ifdef USE_NET
                                BM_FACE |
#endif
                                0);

  BM_mesh_elem_table_ensure(bm,
#ifdef USE_SPLICE
                            BM_EDGE |
#endif
#ifdef USE_NET
                                BM_FACE |
#endif
                                0);

#ifdef USE_DISSOLVE
  if (use_dissolve) {
    BM_mesh_elem_hflag_disable_all(bm, BM_EDGE | BM_VERT, BM_ELEM_TAG, false);
  }
#else
  UNUSED_VARS(use_dissolve);
#endif

#ifdef USE_DUMP
  printf("data = [\n");
#endif

  if (boolean_mode != BMESH_ISECT_BOOLEAN_NONE) {
    /* Keep original geometry for ray-cast callbacks. */
    float **cos;
    int i, j;

    cos = static_cast<float **>(
        MEM_mallocN(size_t(looptris.size()) * sizeof(*looptri_coords) * 3, __func__));
    for (i = 0, j = 0; i < int(looptris.size()); i++) {
      cos[j++] = looptris[i][0]->v->co;
      cos[j++] = looptris[i][1]->v->co;
      cos[j++] = looptris[i][2]->v->co;
    }
    looptri_coords = (const float **)cos;
  }

#ifdef USE_BVH
  {
    int i;
    tree_a = BLI_bvhtree_new(int(looptris.size()), s.epsilon.eps_margin, 8, 8);
    for (i = 0; i < int(looptris.size()); i++) {
      if (test_fn(looptris[i][0]->f, user_data) == 0) {
        const float t_cos[3][3] = {
            {UNPACK3(looptris[i][0]->v->co)},
            {UNPACK3(looptris[i][1]->v->co)},
            {UNPACK3(looptris[i][2]->v->co)},
        };

        BLI_bvhtree_insert(tree_a, i, (const float *)t_cos, 3);
      }
    }
    BLI_bvhtree_balance(tree_a);
  }

  if (use_self == false) {
    int i;
    tree_b = BLI_bvhtree_new(int(looptris.size()), s.epsilon.eps_margin, 8, 8);
    for (i = 0; i < int(looptris.size()); i++) {
      if (test_fn(looptris[i][0]->f, user_data) == 1) {
        const float t_cos[3][3] = {
            {UNPACK3(looptris[i][0]->v->co)},
            {UNPACK3(looptris[i][1]->v->co)},
            {UNPACK3(looptris[i][2]->v->co)},
        };

        BLI_bvhtree_insert(tree_b, i, (const float *)t_cos, 3);
      }
    }
    BLI_bvhtree_balance(tree_b);
  }
  else {
    tree_b = tree_a;
  }

  /* For self intersection this can be useful, sometimes users generate geometry
   * where surfaces that seem disconnected happen to share an edge.
   * So when performing intersection calculation allow shared vertices,
   * just not shared edges. See #75946. */
  const bool isect_tri_tri_no_shared = (boolean_mode != BMESH_ISECT_BOOLEAN_NONE);

  int flag = BVH_OVERLAP_USE_THREADING | BVH_OVERLAP_RETURN_PAIRS;
#  ifndef NDEBUG
  /* The overlap result must match that obtained in Release to succeed
   * in the `bmesh_boolean` test. */
  if (looptris.size() < 1024) {
    flag &= ~BVH_OVERLAP_USE_THREADING;
  }
#  endif
  overlap = BLI_bvhtree_overlap_ex(tree_b, tree_a, &tree_overlap_tot, nullptr, nullptr, 0, flag);

  if (overlap) {
    uint i;

    for (i = 0; i < tree_overlap_tot; i++) {
#  ifdef USE_DUMP
      printf("  ((%d, %d), (\n", overlap[i].indexA, overlap[i].indexB);
#  endif
      bm_isect_tri_tri(&s,
                       overlap[i].indexA,
                       overlap[i].indexB,
                       looptris[overlap[i].indexA],
                       looptris[overlap[i].indexB],
                       isect_tri_tri_no_shared);
#  ifdef USE_DUMP
      printf(")),\n");
#  endif
    }
    MEM_freeN(overlap);
  }

  if (boolean_mode == BMESH_ISECT_BOOLEAN_NONE) {
    /* no booleans, just free immediate */
    BLI_bvhtree_free(tree_a);
    if (tree_a != tree_b) {
      BLI_bvhtree_free(tree_b);
    }
  }

#else
  {
    for (i_a = 0; i_a < looptris.size(); i_a++) {
      const int t_a = test_fn(looptris[i_a][0]->f, user_data);
      for (i_b = i_a + 1; i_b < looptris.size(); i_b++) {
        const int t_b = test_fn(looptris[i_b][0]->f, user_data);

        if (use_self) {
          if ((t_a != 0) || (t_b != 0)) {
            continue;
          }
        }
        else {
          if ((t_a != t_b) && !ELEM(-1, t_a, t_b)) {
            continue;
          }
        }

#  ifdef USE_DUMP
        printf("  ((%d, %d), (", i_a, i_b);
#  endif
        bm_isect_tri_tri(&s, i_a, i_b, looptris[i_a], looptris[i_b], isect_tri_tri_no_shared);
#  ifdef USE_DUMP
        printf(")),\n");
#  endif
      }
    }
  }
#endif /* USE_BVH */

#ifdef USE_DUMP
  printf("]\n");
#endif

  /* --------- */

#ifdef USE_SPLICE
  {
    GHashIterator gh_iter;

    GHASH_ITER (gh_iter, s.edge_verts) {
      BMEdge *e = static_cast<BMEdge *>(BLI_ghashIterator_getKey(&gh_iter));
      LinkBase *v_ls_base = static_cast<LinkBase *>(BLI_ghashIterator_getValue(&gh_iter));

      BMVert *v_start;
      BMVert *v_end;
      BMVert *v_prev;
      bool is_wire;

      LinkNode *node;

      /* direction is arbitrary, could be swapped */
      v_start = e->v1;
      v_end = e->v2;

      if (v_ls_base->list_len > 1) {
        edge_verts_sort(v_start->co, v_ls_base);
      }

#  ifdef USE_DUMP
      printf("# SPLITTING EDGE: %d, %u\n", BM_elem_index_get(e), v_ls_base->list_len);
#  endif
      /* intersect */
      is_wire = BLI_gset_haskey(s.wire_edges, e);

#  ifdef USE_PARANOID
      for (node = v_ls_base->list; node; node = node->next) {
        BMVert *_v = node->link;
        BLI_assert(len_squared_v3v3(_v->co, e->v1->co) > s.epsilon.eps_sq);
        BLI_assert(len_squared_v3v3(_v->co, e->v2->co) > s.epsilon.eps_sq);
      }
#  endif

      v_prev = v_start;

      for (node = v_ls_base->list; node; node = node->next) {
        BMVert *vi = static_cast<BMVert *>(node->link);
        const float fac = line_point_factor_v3(vi->co, e->v1->co, e->v2->co);

        if (BM_vert_in_edge(e, v_prev)) {
          BMEdge *e_split;
          v_prev = BM_edge_split(bm, e, v_prev, &e_split, clamp_f(fac, 0.0f, 1.0f));
          BLI_assert(BM_vert_in_edge(e, v_end));

          if (!BM_edge_exists(v_prev, vi) && !BM_vert_splice_check_double(v_prev, vi) &&
              !BM_vert_pair_share_face_check(v_prev, vi))
          {
            BM_vert_splice(bm, vi, v_prev);
          }
          else {
            copy_v3_v3(v_prev->co, vi->co);
          }
          v_prev = vi;
          if (is_wire) {
            BLI_gset_insert(s.wire_edges, e_split);
          }
        }
      }
      UNUSED_VARS_NDEBUG(v_end);
    }
  }
#endif

/* important to handle before edgenet */
#ifdef USE_DISSOLVE
  if (use_dissolve && (boolean_mode == BMESH_ISECT_BOOLEAN_NONE)) {
    /* first pass */
    BMVert *(*splice_ls)[2];
    STACK_DECLARE(splice_ls);
    LinkNode *node;

    for (node = s.vert_dissolve; node; node = node->next) {
      BMVert *v = static_cast<BMVert *>(node->link);
      if (BM_elem_flag_test(v, BM_ELEM_TAG)) {
        if (!BM_vert_is_edge_pair(v)) {
          BM_elem_flag_disable(v, BM_ELEM_TAG);
        }
      }
    }

    splice_ls = static_cast<BMVert *(*)[2]>(
        MEM_mallocN(BLI_gset_len(s.wire_edges) * sizeof(*splice_ls), __func__));
    STACK_INIT(splice_ls, BLI_gset_len(s.wire_edges));

    for (node = s.vert_dissolve; node; node = node->next) {
      BMEdge *e_pair[2];
      BMVert *v = static_cast<BMVert *>(node->link);
      BMVert *v_a, *v_b;

      if (!BM_elem_flag_test(v, BM_ELEM_TAG)) {
        continue;
      }

      /* get chain */
      e_pair[0] = v->e;
      e_pair[1] = BM_DISK_EDGE_NEXT(v->e, v);

      if (BM_elem_flag_test(e_pair[0], BM_ELEM_TAG) || BM_elem_flag_test(e_pair[1], BM_ELEM_TAG)) {
        continue;
      }

      /* It's possible the vertex to dissolve is an edge on an existing face
       * that doesn't divide the face, therefor the edges are not wire
       * and shouldn't be handled here, see: #63787. */
      if (!BLI_gset_haskey(s.wire_edges, e_pair[0]) || !BLI_gset_haskey(s.wire_edges, e_pair[1])) {
        continue;
      }

      v_a = BM_edge_other_vert(e_pair[0], v);
      v_b = BM_edge_other_vert(e_pair[1], v);

      /* simple case */
      if (BM_elem_flag_test(v_a, BM_ELEM_TAG) && BM_elem_flag_test(v_b, BM_ELEM_TAG)) {
        /* only start on an edge-case */
        /* pass */
      }
      else if (!BM_elem_flag_test(v_a, BM_ELEM_TAG) && !BM_elem_flag_test(v_b, BM_ELEM_TAG)) {
        /* simple case, single edge spans face */
        BMVert **splice_pair;
        BM_elem_flag_enable(e_pair[1], BM_ELEM_TAG);
        splice_pair = STACK_PUSH_RET(splice_ls);
        splice_pair[0] = v;
        splice_pair[1] = v_b;
#  ifdef USE_DUMP
        printf("# Simple Case!\n");
#  endif
      }
      else {
#  ifdef USE_PARANOID
        BMEdge *e_keep;
#  endif
        BMEdge *e;
        BMEdge *e_step;
        BMVert *v_step;

        /* walk the chain! */
        if (BM_elem_flag_test(v_a, BM_ELEM_TAG)) {
          e = e_pair[0];
#  ifdef USE_PARANOID
          e_keep = e_pair[1];
#  endif
        }
        else {
          std::swap(v_a, v_b);
          e = e_pair[1];
#  ifdef USE_PARANOID
          e_keep = e_pair[0];
#  endif
        }

        /* WALK */
        v_step = v;
        e_step = e;

        while (true) {
          BMEdge *e_next;
          BMVert *v_next;

          v_next = BM_edge_other_vert(e_step, v_step);
          BM_elem_flag_enable(e_step, BM_ELEM_TAG);
          if (!BM_elem_flag_test(v_next, BM_ELEM_TAG)) {
            BMVert **splice_pair;
#  ifdef USE_PARANOID
            BLI_assert(e_step != e_keep);
#  endif
            splice_pair = STACK_PUSH_RET(splice_ls);
            splice_pair[0] = v;
            splice_pair[1] = v_next;
            break;
          }

          e_next = bm_vert_other_edge(v_next, e_step);
          e_step = e_next;
          v_step = v_next;
          BM_elem_flag_enable(e_step, BM_ELEM_TAG);
#  ifdef USE_PARANOID
          BLI_assert(e_step != e_keep);
#  endif
#  ifdef USE_DUMP
          printf("# walk step %p %p\n", e_next, v_next);
#  endif
        }
#  ifdef USE_PARANOID
        BLI_assert(BM_elem_flag_test(e_keep, BM_ELEM_TAG) == 0);
#  endif
      }
    }

    /* Remove edges! */
    {
      GHashIterator gh_iter;

      GHASH_ITER (gh_iter, s.face_edges) {
        LinkBase *e_ls_base = static_cast<LinkBase *>(BLI_ghashIterator_getValue(&gh_iter));
        LinkNode **node_prev_p;

        node_prev_p = &e_ls_base->list;
        for (node = e_ls_base->list; node; node = node->next) {
          BMEdge *e = static_cast<BMEdge *>(node->link);
          if (BM_elem_flag_test(e, BM_ELEM_TAG)) {
            /* allocated by arena, don't free */
            *node_prev_p = node->next;
            e_ls_base->list_len--;
          }
          else {
            node_prev_p = &node->next;
          }
        }
      }
    }

    {
      BMIter eiter;
      BMEdge *e, *e_next;

      BM_ITER_MESH_MUTABLE (e, e_next, &eiter, bm, BM_EDGES_OF_MESH) {
        if (BM_elem_flag_test(e, BM_ELEM_TAG)) {

          /* in rare and annoying cases,
           * there can be faces from 's.face_edges' removed by the edges.
           * These are degenerate cases, so just make sure we don't reference the faces again. */
          if (e->l) {
            BMLoop *l_iter = e->l;
            BMFace **faces;

            faces = bm->ftable;

            do {
              const int f_index = BM_elem_index_get(l_iter->f);
              if (f_index >= 0) {
                BLI_assert(f_index < totface_orig);
                /* we could check if these are in: 's.face_edges', but easier just to remove */
                faces[f_index] = nullptr;
              }
            } while ((l_iter = l_iter->radial_next) != e->l);
          }

          BLI_gset_remove(s.wire_edges, e, nullptr);
          BM_edge_kill(bm, e);
        }
      }
    }

    /* Remove verts! */
    {
      GSet *verts_invalid = BLI_gset_ptr_new(__func__);

      for (node = s.vert_dissolve; node; node = node->next) {
        /* arena allocated, don't free */
        BMVert *v = static_cast<BMVert *>(node->link);
        if (BM_elem_flag_test(v, BM_ELEM_TAG)) {
          if (!v->e) {
            BLI_gset_add(verts_invalid, v);
            BM_vert_kill(bm, v);
          }
        }
      }

      {
        uint i;
        for (i = 0; i < STACK_SIZE(splice_ls); i++) {
          if (!BLI_gset_haskey(verts_invalid, splice_ls[i][0]) &&
              !BLI_gset_haskey(verts_invalid, splice_ls[i][1]))
          {
            if (!BM_edge_exists(UNPACK2(splice_ls[i])) &&
                !BM_vert_splice_check_double(UNPACK2(splice_ls[i])))
            {
              BM_vert_splice(bm, splice_ls[i][1], splice_ls[i][0]);
            }
          }
        }
      }

      BLI_gset_free(verts_invalid, nullptr);
    }

    MEM_freeN(splice_ls);
  }
#endif /* USE_DISSOLVE */

/* now split faces */
#ifdef USE_NET
  {
    GHashIterator gh_iter;
    BMFace **faces;

    MemArena *mem_arena_edgenet = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);

    faces = bm->ftable;

    GHASH_ITER (gh_iter, s.face_edges) {
      const int f_index = POINTER_AS_INT(BLI_ghashIterator_getKey(&gh_iter));
      BMFace *f;
      LinkBase *e_ls_base = static_cast<LinkBase *>(BLI_ghashIterator_getValue(&gh_iter));

      BLI_assert(f_index >= 0 && f_index < totface_orig);

      f = faces[f_index];
      if (UNLIKELY(f == nullptr)) {
        continue;
      }

      BLI_assert(BM_elem_index_get(f) == f_index);

      face_edges_split(
          bm, f, e_ls_base, use_island_connect, use_partial_connect, mem_arena_edgenet);

      BLI_memarena_clear(mem_arena_edgenet);
    }

    BLI_memarena_free(mem_arena_edgenet);
  }
#else
  UNUSED_VARS(use_island_connect);
#endif /* USE_NET */
  (void)totface_orig;

#ifdef USE_SEPARATE
  if (use_separate) {
    GSetIterator gs_iter;

    BM_mesh_elem_hflag_disable_all(bm, BM_EDGE, BM_ELEM_TAG, false);

    GSET_ITER (gs_iter, s.wire_edges) {
      BMEdge *e = static_cast<BMEdge *>(BLI_gsetIterator_getKey(&gs_iter));
      BM_elem_flag_enable(e, BM_ELEM_TAG);
    }

    BM_mesh_edgesplit(bm, false, true, false);
  }
  else if (boolean_mode != BMESH_ISECT_BOOLEAN_NONE || use_edge_tag) {
    GSetIterator gs_iter;

    /* no need to clear for boolean */

    GSET_ITER (gs_iter, s.wire_edges) {
      BMEdge *e = static_cast<BMEdge *>(BLI_gsetIterator_getKey(&gs_iter));
      BM_elem_flag_enable(e, BM_ELEM_TAG);
    }
  }
#else
  (void)use_separate;
#endif /* USE_SEPARATE */

  if (boolean_mode != BMESH_ISECT_BOOLEAN_NONE) {
    BVHTree *tree_pair[2] = {tree_a, tree_b};

    /* group vars */
    int *groups_array;
    int (*group_index)[2];
    int group_tot;
    int i;
    BMFace **ftable;

    BM_mesh_elem_table_ensure(bm, BM_FACE);
    ftable = bm->ftable;

    /* wrap the face-test callback to make it into an edge-loop delimiter */
    LoopFilterWrap user_data_wrap{};
    user_data_wrap.test_fn = test_fn;
    user_data_wrap.user_data = user_data;

    groups_array = MEM_malloc_arrayN<int>(size_t(bm->totface), __func__);
    group_tot = BM_mesh_calc_face_groups(
        bm, groups_array, &group_index, bm_loop_filter_fn, nullptr, &user_data_wrap, 0, BM_EDGE);

#ifdef USE_DUMP
    printf("%s: Total face-groups: %d\n", __func__, group_tot);
#endif

    /* Check if island is inside/outside */
    for (i = 0; i < group_tot; i++) {
      int fg = group_index[i][0];
      int fg_end = group_index[i][1] + fg;
      bool do_remove, do_flip;

      {
        /* For now assume this is an OK face to test with (not degenerate!) */
        BMFace *f = ftable[groups_array[fg]];
        float co[3];
        int hits;
        int side = test_fn(f, user_data);

        if (side == -1) {
          continue;
        }
        BLI_assert(ELEM(side, 0, 1));
        side = !side;

        // BM_face_calc_center_median(f, co);
        BM_face_calc_point_in_face(f, co);

        hits = isect_bvhtree_point_v3(tree_pair[side], looptri_coords, co);

        switch (boolean_mode) {
          case BMESH_ISECT_BOOLEAN_ISECT:
            do_remove = ((hits & 1) != 1);
            do_flip = false;
            break;
          case BMESH_ISECT_BOOLEAN_UNION:
            do_remove = ((hits & 1) == 1);
            do_flip = false;
            break;
          case BMESH_ISECT_BOOLEAN_DIFFERENCE:
            do_remove = ((hits & 1) == 1) == side;
            do_flip = (side == 0);
            break;
        }
      }

      if (do_remove) {
        for (; fg != fg_end; fg++) {
          /* postpone killing the face since we access below, mark instead */
          // BM_face_kill_loose(bm, ftable[groups_array[fg]]);
          ftable[groups_array[fg]]->mat_nr = -1;
        }
      }
      else if (do_flip) {
        for (; fg != fg_end; fg++) {
          BM_face_normal_flip(bm, ftable[groups_array[fg]]);
        }
      }

      has_edit_boolean |= (do_flip || do_remove);
    }

    MEM_freeN(groups_array);
    MEM_freeN(group_index);

#ifdef USE_DISSOLVE
    /* We have dissolve code above, this is alternative logic,
     * we need to do it after the boolean is executed. */
    if (use_dissolve) {
      LinkNode *node;
      for (node = s.vert_dissolve; node; node = node->next) {
        BMVert *v = static_cast<BMVert *>(node->link);
        if (BM_vert_is_edge_pair(v)) {
          /* we won't create degenerate faces from this */
          bool ok = true;

          /* would we create a 2-sided-face?
           * if so, don't dissolve this since we may */
          if (v->e->l) {
            BMLoop *l_iter = v->e->l;
            do {
              if (l_iter->f->len == 3) {
                ok = false;
                break;
              }
            } while ((l_iter = l_iter->radial_next) != v->e->l);
          }

          if (ok) {
            BM_vert_collapse_edge(bm, v->e, v, true, false, false);
          }
        }
      }
    }
#endif

    {
      int tot = bm->totface;
      for (i = 0; i < tot; i++) {
        if (ftable[i]->mat_nr == -1) {
          BM_face_kill_loose(bm, ftable[i]);
        }
      }
    }
  }

  if (boolean_mode != BMESH_ISECT_BOOLEAN_NONE) {
    MEM_freeN(looptri_coords);

    /* no booleans, just free immediate */
    BLI_bvhtree_free(tree_a);
    if (tree_a != tree_b) {
      BLI_bvhtree_free(tree_b);
    }
  }

  has_edit_isect = (BLI_ghash_len(s.face_edges) != 0);

  /* cleanup */
  BLI_ghash_free(s.edgetri_cache, nullptr, nullptr);

  BLI_ghash_free(s.edge_verts, nullptr, nullptr);
  BLI_ghash_free(s.face_edges, nullptr, nullptr);
  BLI_gset_free(s.wire_edges, nullptr);

  BLI_memarena_free(s.mem_arena);

  /* It's unlikely the selection history is useful at this point,
   * if this is not called this array would need to be validated, see: #86799. */
  BM_select_history_clear(bm);

  return (has_edit_isect || has_edit_boolean);
}
