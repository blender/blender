/*
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
 */

/** \file
 * \ingroup bmesh
 *
 * Connect vertex pair across multiple faces (splits faces).
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_heap_simple.h"

#include "bmesh.h"

#include "intern/bmesh_operators_private.h" /* own include */

#include "BLI_mempool.h"

/**
 * Method for connecting across many faces.
 *
 * - use the line between both verts and their normal average to construct a matrix.
 * - using the matrix, we can find all intersecting verts/edges.
 * - walk the connected data and find the shortest path.
 *   - store a heap of paths which are being scanned (#PathContext.states).
 *   - continuously search the shortest path in the heap.
 *   - never step over the same element twice (tag elements as #ELE_TOUCHED).
 *     this avoids going into an eternal loop if there are many possible branches (see T45582).
 *   - when running into a branch, create a new #PathLinkState state and add to the heap.
 *   - when the target is reached,
 *     finish - since none of the other paths can be shorter then the one just found.
 * - if the connection can't be found - fail.
 * - with the connection found, split all edges tagging verts
 *   (or tag verts that sit on the intersection).
 * - run the standard connect operator.
 */

#define CONNECT_EPS 0.0001f
#define VERT_OUT 1
#define VERT_EXCLUDE 2

/* typically hidden faces */
#define FACE_EXCLUDE 2

/* any element we've walked over (only do it once!) */
#define ELE_TOUCHED 4

#define FACE_WALK_TEST(f) \
  (CHECK_TYPE_INLINE(f, BMFace *), BMO_face_flag_test(pc->bm_bmoflag, f, FACE_EXCLUDE) == 0)
#define VERT_WALK_TEST(v) \
  (CHECK_TYPE_INLINE(v, BMVert *), BMO_vert_flag_test(pc->bm_bmoflag, v, VERT_EXCLUDE) == 0)

#if 0
#  define ELE_TOUCH_TEST(e) \
    (CHECK_TYPE_ANY(e, BMVert *, BMEdge *, BMElem *, BMElemF *), \
     BMO_elem_flag_test(pc->bm_bmoflag, (BMElemF *)e, ELE_TOUCHED))
#endif
#define ELE_TOUCH_MARK(e) \
  { \
    CHECK_TYPE_ANY(e, BMVert *, BMEdge *, BMElem *, BMElemF *); \
    BMO_elem_flag_enable(pc->bm_bmoflag, (BMElemF *)e, ELE_TOUCHED); \
  } \
  ((void)0)

#define ELE_TOUCH_TEST_VERT(v) BMO_vert_flag_test(pc->bm_bmoflag, v, ELE_TOUCHED)
// #define ELE_TOUCH_MARK_VERT(v) BMO_vert_flag_enable(pc->bm_bmoflag, (BMElemF *)v, ELE_TOUCHED)

#define ELE_TOUCH_TEST_EDGE(e) BMO_edge_flag_test(pc->bm_bmoflag, e, ELE_TOUCHED)
// #define ELE_TOUCH_MARK_EDGE(e) BMO_edge_flag_enable(pc->bm_bmoflag, (BMElemF *)e, ELE_TOUCHED)

// #define ELE_TOUCH_TEST_FACE(f) BMO_face_flag_test(pc->bm_bmoflag, f, ELE_TOUCHED)
// #define ELE_TOUCH_MARK_FACE(f) BMO_face_flag_enable(pc->bm_bmoflag, (BMElemF *)f, ELE_TOUCHED)

// #define DEBUG_PRINT

typedef struct PathContext {
  HeapSimple *states;
  float matrix[3][3];
  float axis_sep;

  /* only to access BMO flags */
  BMesh *bm_bmoflag;

  BMVert *v_a, *v_b;

  BLI_mempool *link_pool;
} PathContext;

/**
 * Single linked list where each item contains state and points to previous path item.
 */
typedef struct PathLink {
  struct PathLink *next;
  BMElem *ele;      /* edge or vert */
  BMElem *ele_from; /* edge or face we came from (not 'next->ele') */
} PathLink;

typedef struct PathLinkState {
  /* chain of links */
  struct PathLink *link_last;

  /* length along links */
  float dist;
  float co_prev[3];
} PathLinkState;

/**
 * \name Min Dist Dir Util
 *
 * Simply getting the closest intersecting vert/edge is _not_ good enough. see T43792
 * we need to get the closest in both directions since the absolute closest may be a dead-end.
 *
 * Logic is simple:
 *
 * - first intersection, store the direction.
 * - successive intersections will update the first distance if its aligned with the first hit.
 *   otherwise update the opposite distance.
 * - caller stores best outcome in both directions.
 *
 * \{ */

typedef struct MinDistDir {
  /* distance in both directions (FLT_MAX == uninitialized) */
  float dist_min[2];
  /* direction of the first intersection found */
  float dir[3];
} MinDistDir;

#define MIN_DIST_DIR_INIT \
  { \
    { \
      FLT_MAX, FLT_MAX \
    } \
  }

static int min_dist_dir_test(MinDistDir *mddir, const float dist_dir[3], const float dist_sq)
{

  if (mddir->dist_min[0] == FLT_MAX) {
    return 0;
  }
  else {
    if (dot_v3v3(dist_dir, mddir->dir) > 0.0f) {
      if (dist_sq < mddir->dist_min[0]) {
        return 0;
      }
    }
    else {
      if (dist_sq < mddir->dist_min[1]) {
        return 1;
      }
    }
  }

  return -1;
}

static void min_dist_dir_update(MinDistDir *dist, const float dist_dir[3])
{
  if (dist->dist_min[0] == FLT_MAX) {
    copy_v3_v3(dist->dir, dist_dir);
  }
}

/** \} */

static int state_isect_co_pair(const PathContext *pc, const float co_a[3], const float co_b[3])
{
  const float diff_a = dot_m3_v3_row_x((float(*)[3])pc->matrix, co_a) - pc->axis_sep;
  const float diff_b = dot_m3_v3_row_x((float(*)[3])pc->matrix, co_b) - pc->axis_sep;

  const int test_a = (fabsf(diff_a) < CONNECT_EPS) ? 0 : (diff_a < 0.0f) ? -1 : 1;
  const int test_b = (fabsf(diff_b) < CONNECT_EPS) ? 0 : (diff_b < 0.0f) ? -1 : 1;

  if ((test_a && test_b) && (test_a != test_b)) {
    return 1; /* on either side */
  }
  else {
    return 0;
  }
}

static int state_isect_co_exact(const PathContext *pc, const float co[3])
{
  const float diff = dot_m3_v3_row_x((float(*)[3])pc->matrix, co) - pc->axis_sep;
  return (fabsf(diff) <= CONNECT_EPS);
}

static float state_calc_co_pair_fac(const PathContext *pc,
                                    const float co_a[3],
                                    const float co_b[3])
{
  float diff_a, diff_b, diff_tot;

  diff_a = fabsf(dot_m3_v3_row_x((float(*)[3])pc->matrix, co_a) - pc->axis_sep);
  diff_b = fabsf(dot_m3_v3_row_x((float(*)[3])pc->matrix, co_b) - pc->axis_sep);
  diff_tot = (diff_a + diff_b);
  return (diff_tot > FLT_EPSILON) ? (diff_a / diff_tot) : 0.5f;
}

static void state_calc_co_pair(const PathContext *pc,
                               const float co_a[3],
                               const float co_b[3],
                               float r_co[3])
{
  const float fac = state_calc_co_pair_fac(pc, co_a, co_b);
  interp_v3_v3v3(r_co, co_a, co_b, fac);
}

#ifndef NDEBUG
/**
 * Ideally we wouldn't need this and for most cases we don't.
 * But when a face has vertices that are on the boundary more than once this becomes tricky.
 */
static bool state_link_find(const PathLinkState *state, BMElem *ele)
{
  PathLink *link = state->link_last;
  BLI_assert(ELEM(ele->head.htype, BM_VERT, BM_EDGE, BM_FACE));
  if (link) {
    do {
      if (link->ele == ele) {
        return true;
      }
    } while ((link = link->next));
  }
  return false;
}
#endif

static void state_link_add(PathContext *pc, PathLinkState *state, BMElem *ele, BMElem *ele_from)
{
  PathLink *step_new = BLI_mempool_alloc(pc->link_pool);
  BLI_assert(ele != ele_from);
  BLI_assert(state_link_find(state, ele) == false);

  /* never walk onto this again */
  ELE_TOUCH_MARK(ele);

#ifdef DEBUG_PRINT
  printf("%s: adding to state %p, %.4f - ", __func__, state, state->dist);
  if (ele->head.htype == BM_VERT) {
    printf("vert %d, ", BM_elem_index_get(ele));
  }
  else if (ele->head.htype == BM_EDGE) {
    printf("edge %d, ", BM_elem_index_get(ele));
  }
  else {
    BLI_assert(0);
  }

  if (ele_from == NULL) {
    printf("from NULL\n");
  }
  else if (ele_from->head.htype == BM_EDGE) {
    printf("from edge %d\n", BM_elem_index_get(ele_from));
  }
  else if (ele_from->head.htype == BM_FACE) {
    printf("from face %d\n", BM_elem_index_get(ele_from));
  }
  else {
    BLI_assert(0);
  }
#endif

  /* track distance */
  {
    float co[3];
    if (ele->head.htype == BM_VERT) {
      copy_v3_v3(co, ((BMVert *)ele)->co);
    }
    else if (ele->head.htype == BM_EDGE) {
      state_calc_co_pair(pc, ((BMEdge *)ele)->v1->co, ((BMEdge *)ele)->v2->co, co);
    }
    else {
      BLI_assert(0);
    }

    /* tally distance */
    if (ele_from) {
      state->dist += len_v3v3(state->co_prev, co);
    }
    copy_v3_v3(state->co_prev, co);
  }

  step_new->ele = ele;
  step_new->ele_from = ele_from;
  step_new->next = state->link_last;
  state->link_last = step_new;
}

static PathLinkState *state_dupe_add(PathLinkState *state, const PathLinkState *state_orig)
{
  state = MEM_mallocN(sizeof(*state), __func__);
  *state = *state_orig;
  return state;
}

static PathLinkState *state_link_add_test(PathContext *pc,
                                          PathLinkState *state,
                                          const PathLinkState *state_orig,
                                          BMElem *ele,
                                          BMElem *ele_from)
{
  const bool is_new = (state_orig->link_last != state->link_last);
  if (is_new) {
    state = state_dupe_add(state, state_orig);
  }

  state_link_add(pc, state, ele, ele_from);

  /* after adding a link so we use the updated 'state->dist' */
  if (is_new) {
    BLI_heapsimple_insert(pc->states, state->dist, state);
  }

  return state;
}

/* walk around the face edges */
static PathLinkState *state_step__face_edges(PathContext *pc,
                                             PathLinkState *state,
                                             const PathLinkState *state_orig,
                                             BMLoop *l_iter,
                                             BMLoop *l_last,
                                             MinDistDir *mddir)
{

  BMLoop *l_iter_best[2] = {NULL, NULL};
  int i;

  do {
    if (state_isect_co_pair(pc, l_iter->v->co, l_iter->next->v->co)) {
      float dist_test;
      float co_isect[3];
      float dist_dir[3];
      int index;

      state_calc_co_pair(pc, l_iter->v->co, l_iter->next->v->co, co_isect);

      sub_v3_v3v3(dist_dir, co_isect, state_orig->co_prev);
      dist_test = len_squared_v3(dist_dir);
      if ((index = min_dist_dir_test(mddir, dist_dir, dist_test)) != -1) {
        BMElem *ele_next = (BMElem *)l_iter->e;
        BMElem *ele_next_from = (BMElem *)l_iter->f;

        if (FACE_WALK_TEST((BMFace *)ele_next_from) &&
            (ELE_TOUCH_TEST_EDGE((BMEdge *)ele_next) == false)) {
          min_dist_dir_update(mddir, dist_dir);
          mddir->dist_min[index] = dist_test;
          l_iter_best[index] = l_iter;
        }
      }
    }
  } while ((l_iter = l_iter->next) != l_last);

  for (i = 0; i < 2; i++) {
    if ((l_iter = l_iter_best[i])) {
      BMElem *ele_next = (BMElem *)l_iter->e;
      BMElem *ele_next_from = (BMElem *)l_iter->f;
      state = state_link_add_test(pc, state, state_orig, ele_next, ele_next_from);
    }
  }

  return state;
}

/* walk around the face verts */
static PathLinkState *state_step__face_verts(PathContext *pc,
                                             PathLinkState *state,
                                             const PathLinkState *state_orig,
                                             BMLoop *l_iter,
                                             BMLoop *l_last,
                                             MinDistDir *mddir)
{
  BMLoop *l_iter_best[2] = {NULL, NULL};
  int i;

  do {
    if (state_isect_co_exact(pc, l_iter->v->co)) {
      float dist_test;
      const float *co_isect = l_iter->v->co;
      float dist_dir[3];
      int index;

      sub_v3_v3v3(dist_dir, co_isect, state_orig->co_prev);
      dist_test = len_squared_v3(dist_dir);
      if ((index = min_dist_dir_test(mddir, dist_dir, dist_test)) != -1) {
        BMElem *ele_next = (BMElem *)l_iter->v;
        BMElem *ele_next_from = (BMElem *)l_iter->f;

        if (FACE_WALK_TEST((BMFace *)ele_next_from) &&
            (ELE_TOUCH_TEST_VERT((BMVert *)ele_next) == false)) {
          min_dist_dir_update(mddir, dist_dir);
          mddir->dist_min[index] = dist_test;
          l_iter_best[index] = l_iter;
        }
      }
    }
  } while ((l_iter = l_iter->next) != l_last);

  for (i = 0; i < 2; i++) {
    if ((l_iter = l_iter_best[i])) {
      BMElem *ele_next = (BMElem *)l_iter->v;
      BMElem *ele_next_from = (BMElem *)l_iter->f;
      state = state_link_add_test(pc, state, state_orig, ele_next, ele_next_from);
    }
  }

  return state;
}

static bool state_step(PathContext *pc, PathLinkState *state)
{
  PathLinkState state_orig = *state;
  BMElem *ele = state->link_last->ele;
  const void *ele_from = state->link_last->ele_from;

  if (ele->head.htype == BM_EDGE) {
    BMEdge *e = (BMEdge *)ele;

    BMIter liter;
    BMLoop *l_start;

    BM_ITER_ELEM (l_start, &liter, e, BM_LOOPS_OF_EDGE) {
      if ((l_start->f != ele_from) && FACE_WALK_TEST(l_start->f)) {
        MinDistDir mddir = MIN_DIST_DIR_INIT;
        /* very similar to block below */
        state = state_step__face_edges(pc, state, &state_orig, l_start->next, l_start, &mddir);
        state = state_step__face_verts(
            pc, state, &state_orig, l_start->next->next, l_start, &mddir);
      }
    }
  }
  else if (ele->head.htype == BM_VERT) {
    BMVert *v = (BMVert *)ele;

    /* vert loops */
    {
      BMIter liter;
      BMLoop *l_start;

      BM_ITER_ELEM (l_start, &liter, v, BM_LOOPS_OF_VERT) {
        if ((l_start->f != ele_from) && FACE_WALK_TEST(l_start->f)) {
          MinDistDir mddir = MIN_DIST_DIR_INIT;
          /* very similar to block above */
          state = state_step__face_edges(
              pc, state, &state_orig, l_start->next, l_start->prev, &mddir);
          if (l_start->f->len > 3) {
            /* adjacent verts are handled in state_step__vert_edges */
            state = state_step__face_verts(
                pc, state, &state_orig, l_start->next->next, l_start->prev, &mddir);
          }
        }
      }
    }

    /* vert edges  */
    {
      BMIter eiter;
      BMEdge *e;
      BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
        BMVert *v_other = BM_edge_other_vert(e, v);
        if (((BMElem *)e != ele_from) && VERT_WALK_TEST(v_other)) {
          if (state_isect_co_exact(pc, v_other->co)) {
            BMElem *ele_next = (BMElem *)v_other;
            BMElem *ele_next_from = (BMElem *)e;
            if (ELE_TOUCH_TEST_VERT((BMVert *)ele_next) == false) {
              state = state_link_add_test(pc, state, &state_orig, ele_next, ele_next_from);
            }
          }
        }
      }
    }
  }
  else {
    BLI_assert(0);
  }
  return (state_orig.link_last != state->link_last);
}

/**
 * Get a orientation matrix from 2 vertices.
 */
static void bm_vert_pair_to_matrix(BMVert *v_pair[2], float r_unit_mat[3][3])
{
  const float eps = 1e-8f;

  float basis_dir[3];
  float basis_tmp[3];
  float basis_nor[3];

  sub_v3_v3v3(basis_dir, v_pair[0]->co, v_pair[1]->co);
  normalize_v3(basis_dir);

#if 0
  add_v3_v3v3(basis_nor, v_pair[0]->no, v_pair[1]->no);
  cross_v3_v3v3(basis_tmp, basis_nor, basis_dir);
  cross_v3_v3v3(basis_nor, basis_tmp, basis_dir);
#else
  /* align both normals to the directions before combining */
  {
    float basis_nor_a[3];
    float basis_nor_b[3];

    /* align normal to direction */
    project_plane_normalized_v3_v3v3(basis_nor_a, v_pair[0]->no, basis_dir);
    project_plane_normalized_v3_v3v3(basis_nor_b, v_pair[1]->no, basis_dir);

    /* Don't normalize before combining so as normals approach the direction,
     * they have less effect (T46784). */

    /* combine the normals */
    /* for flipped faces */
    if (dot_v3v3(basis_nor_a, basis_nor_b) < 0.0f) {
      negate_v3(basis_nor_b);
    }
    add_v3_v3v3(basis_nor, basis_nor_a, basis_nor_b);
  }
#endif

  /* get third axis */
  normalize_v3(basis_nor);
  cross_v3_v3v3(basis_tmp, basis_dir, basis_nor);

  /* Try get the axis from surrounding faces, fallback to 'ortho_v3_v3' */
  if (UNLIKELY(normalize_v3(basis_tmp) < eps)) {
    /* vertex normals are directly opposite */

    /* find the loop with the lowest angle */
    struct {
      float nor[3];
      float angle_cos;
    } axis_pair[2];
    int i;

    for (i = 0; i < 2; i++) {
      BMIter liter;
      BMLoop *l;

      zero_v2(axis_pair[i].nor);
      axis_pair[i].angle_cos = -FLT_MAX;

      BM_ITER_ELEM (l, &liter, v_pair[i], BM_LOOPS_OF_VERT) {
        float basis_dir_proj[3];
        float angle_cos_test;

        /* project basis dir onto the normal to find its closest angle */
        project_plane_normalized_v3_v3v3(basis_dir_proj, basis_dir, l->f->no);

        if (normalize_v3(basis_dir_proj) > eps) {
          angle_cos_test = dot_v3v3(basis_dir_proj, basis_dir);

          if (angle_cos_test > axis_pair[i].angle_cos) {
            axis_pair[i].angle_cos = angle_cos_test;
            copy_v3_v3(axis_pair[i].nor, basis_dir_proj);
          }
        }
      }
    }

    /* create a new 'basis_nor' from the best direction.
     * note: we could add the directions,
     * but this more often gives 45d rotated matrix, so just use the best one. */
    copy_v3_v3(basis_nor, axis_pair[axis_pair[0].angle_cos < axis_pair[1].angle_cos].nor);
    project_plane_normalized_v3_v3v3(basis_nor, basis_nor, basis_dir);

    cross_v3_v3v3(basis_tmp, basis_dir, basis_nor);

    /* last resort, pick _any_ ortho axis */
    if (UNLIKELY(normalize_v3(basis_tmp) < eps)) {
      ortho_v3_v3(basis_nor, basis_dir);
      normalize_v3(basis_nor);
      cross_v3_v3v3(basis_tmp, basis_dir, basis_nor);
      normalize_v3(basis_tmp);
    }
  }

  copy_v3_v3(r_unit_mat[0], basis_tmp);
  copy_v3_v3(r_unit_mat[1], basis_dir);
  copy_v3_v3(r_unit_mat[2], basis_nor);
  if (invert_m3(r_unit_mat) == false) {
    unit_m3(r_unit_mat);
  }
}

void bmo_connect_vert_pair_exec(BMesh *bm, BMOperator *op)
{
  BMOpSlot *op_verts_slot = BMO_slot_get(op->slots_in, "verts");

  PathContext pc;
  PathLinkState state_best = {NULL};

  if (op_verts_slot->len != 2) {
    /* fail! */
    return;
  }

  pc.bm_bmoflag = bm;
  pc.v_a = ((BMVert **)op_verts_slot->data.p)[0];
  pc.v_b = ((BMVert **)op_verts_slot->data.p)[1];

  /* fail! */
  if (!(pc.v_a && pc.v_b)) {
    return;
  }

#ifdef DEBUG_PRINT
  printf("%s: v_a: %d\n", __func__, BM_elem_index_get(pc.v_a));
  printf("%s: v_b: %d\n", __func__, BM_elem_index_get(pc.v_b));
#endif

  /* tag so we won't touch ever (typically hidden faces) */
  BMO_slot_buffer_flag_enable(bm, op->slots_in, "faces_exclude", BM_FACE, FACE_EXCLUDE);
  BMO_slot_buffer_flag_enable(bm, op->slots_in, "verts_exclude", BM_VERT, VERT_EXCLUDE);

  /* setup context */
  {
    pc.states = BLI_heapsimple_new();
    pc.link_pool = BLI_mempool_create(sizeof(PathLink), 0, 512, BLI_MEMPOOL_NOP);
  }

  /* calculate matrix */
  {
    bm_vert_pair_to_matrix(&pc.v_a, pc.matrix);
    pc.axis_sep = dot_m3_v3_row_x(pc.matrix, pc.v_a->co);
  }

  /* add first vertex */
  {
    PathLinkState *state;
    state = MEM_callocN(sizeof(*state), __func__);
    state_link_add(&pc, state, (BMElem *)pc.v_a, NULL);
    BLI_heapsimple_insert(pc.states, state->dist, state);
  }

  while (!BLI_heapsimple_is_empty(pc.states)) {

#ifdef DEBUG_PRINT
    printf("\n%s: stepping %u\n", __func__, BLI_heapsimple_len(pc.states));
#endif

    while (!BLI_heapsimple_is_empty(pc.states)) {
      PathLinkState *state = BLI_heapsimple_pop_min(pc.states);

      /* either we insert this into 'pc.states' or its freed */
      bool continue_search;

      if (state->link_last->ele == (BMElem *)pc.v_b) {
        /* pass, wait until all are found */
#ifdef DEBUG_PRINT
        printf("%s: state %p loop found %.4f\n", __func__, state, state->dist);
#endif
        state_best = *state;

        /* we're done, exit all loops */
        BLI_heapsimple_clear(pc.states, MEM_freeN);
        continue_search = false;
      }
      else if (state_step(&pc, state)) {
        continue_search = true;
      }
      else {
        /* didn't reach the end, remove it,
         * links are shared between states so just free the link_pool at the end */

#ifdef DEBUG_PRINT
        printf("%s: state %p removed\n", __func__, state);
#endif
        continue_search = false;
      }

      if (continue_search) {
        BLI_heapsimple_insert(pc.states, state->dist, state);
      }
      else {
        MEM_freeN(state);
      }
    }
  }

  if (state_best.link_last) {
    PathLink *link;

    /* find the best state */
    link = state_best.link_last;
    do {
      if (link->ele->head.htype == BM_EDGE) {
        BMEdge *e = (BMEdge *)link->ele;
        BMVert *v_new;
        float e_fac = state_calc_co_pair_fac(&pc, e->v1->co, e->v2->co);
        v_new = BM_edge_split(bm, e, e->v1, NULL, e_fac);
        BMO_vert_flag_enable(bm, v_new, VERT_OUT);
      }
      else if (link->ele->head.htype == BM_VERT) {
        BMVert *v = (BMVert *)link->ele;
        BMO_vert_flag_enable(bm, v, VERT_OUT);
      }
      else {
        BLI_assert(0);
      }
    } while ((link = link->next));
  }

  BMO_vert_flag_enable(bm, pc.v_a, VERT_OUT);
  BMO_vert_flag_enable(bm, pc.v_b, VERT_OUT);

  BLI_mempool_destroy(pc.link_pool);

  BLI_heapsimple_free(pc.states, MEM_freeN);

#if 1
  if (state_best.link_last) {
    BMOperator op_sub;
    BMO_op_initf(bm,
                 &op_sub,
                 0,
                 "connect_verts verts=%fv faces_exclude=%s check_degenerate=%b",
                 VERT_OUT,
                 op,
                 "faces_exclude",
                 true);
    BMO_op_exec(bm, &op_sub);
    BMO_slot_copy(&op_sub, slots_out, "edges.out", op, slots_out, "edges.out");
    BMO_op_finish(bm, &op_sub);
  }
#endif
}
