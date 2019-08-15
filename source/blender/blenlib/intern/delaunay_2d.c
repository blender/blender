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
 * \ingroup bli
 *
 * Dynamic Constrained Delaunay Triangulation.
 * See paper by Marcelo Kallmann, Hanspeter Bieri, and Daniel Thalmann
 */

#include "MEM_guardedalloc.h"

#include "BLI_array.h"
#include "BLI_bitmap.h"
#include "BLI_linklist.h"
#include "BLI_math.h"
#include "BLI_memarena.h"
#include "BLI_mempool.h"
#include "BLI_rand.h"

#include "BLI_delaunay_2d.h"

/* Uncomment this define to get helpful debugging functions etc. defined. */
// #define DEBUG_CDT

struct CDTEdge;
struct CDTFace;
struct CDTVert;

typedef struct SymEdge {
  struct SymEdge *next; /* In face, doing CCW traversal of face. */
  struct SymEdge *rot;  /* CCW around vert. */
  struct CDTVert *vert; /* Vert at origin. */
  struct CDTEdge *edge; /* Undirected edge this is for. */
  struct CDTFace *face; /* Face on left side. */
} SymEdge;

typedef struct CDTVert {
  double co[2];        /* Coordinate. */
  SymEdge *symedge;    /* Some edge attached to it. */
  LinkNode *input_ids; /* List of corresponding vertex input ids. */
  int index;           /* Index into array that cdt keeps. */
} CDTVert;

typedef struct CDTEdge {
  LinkNode *input_ids; /* List of input edge ids that this is part of. */
  SymEdge symedges[2]; /* The directed edges for this edge. */
} CDTEdge;

typedef struct CDTFace {
  double centroid[2];  /* Average of vertex coords. */
  SymEdge *symedge;    /* A symedge in face; only used during output. */
  LinkNode *input_ids; /* List of input face ids that this is part of. */
  int visit_index;     /* Which visit epoch has this been seen. */
  bool deleted;        /* Marks this face no longer used. */
} CDTFace;

typedef struct CDT_state {
  LinkNode *edges;
  LinkNode *faces;
  CDTFace *outer_face;
  CDTVert **vert_array;
  int vert_array_len;
  int vert_array_len_alloc;
  double minx;
  double miny;
  double maxx;
  double maxy;
  double margin;
  int visit_count;
  int face_edge_offset;
  RNG *rng;
  MemArena *arena;
  BLI_mempool *listpool;
  double epsilon;
  bool output_prepared;
} CDT_state;

typedef struct LocateResult {
  enum { OnVert, OnEdge, InFace } loc_kind;
  SymEdge *se;
  double edge_lambda;
} LocateResult;

#define DLNY_ARENASIZE 1 << 14

/**
 * This margin means that will only be a 1 degree possible
 * concavity on outside if remove all border touching triangles.
 */
#define DLNY_MARGIN_PCT 2000.0

#ifdef DEBUG_CDT
#  define F2(p) p[0], p[1]
static void dump_se(const SymEdge *se, const char *lab);
static void dump_v(const CDTVert *v, const char *lab);
static void dump_se_cycle(const SymEdge *se, const char *lab, const int limit);
static void dump_id_list(const LinkNode *id_list, const char *lab);
static void dump_cdt(const CDT_state *cdt, const char *lab);
static void cdt_draw(CDT_state *cdt, const char *lab);
static void validate_face_centroid(SymEdge *se);
static void validate_cdt(CDT_state *cdt, bool check_all_tris);
#endif

/* TODO: move these to BLI_vector... and BLI_math... */
static double max_dd(const double a, const double b)
{
  return (a > b) ? a : b;
}

static double len_v2v2_db(const double a[2], const double b[2])
{
  return sqrt((b[0] - a[0]) * (b[0] - a[0]) + (b[1] - a[1]) * (b[1] - a[1]));
}

static double len_squared_v2v2_db(const double a[2], const double b[2])
{
  return (b[0] - a[0]) * (b[0] - a[0]) + (b[1] - a[1]) * (b[1] - a[1]);
}

static void add_v2_v2_db(double a[2], const double b[2])
{
  a[0] += b[0];
  a[1] += b[1];
}

static void sub_v2_v2v2_db(double *a, const double *b, const double *c)
{
  a[0] = b[0] - c[0];
  a[1] = b[1] - c[1];
}

static double dot_v2v2_db(const double *a, const double *b)
{
  return a[0] * b[0] + a[1] * b[1];
}

static double closest_to_line_v2_db(double r_close[2],
                                    const double p[2],
                                    const double l1[2],
                                    const double l2[2])
{
  double h[2], u[2], lambda, denom;
  sub_v2_v2v2_db(u, l2, l1);
  sub_v2_v2v2_db(h, p, l1);
  denom = dot_v2v2_db(u, u);
  if (denom < DBL_EPSILON) {
    r_close[0] = l1[0];
    r_close[1] = l1[1];
    return 0.0;
  }
  lambda = dot_v2v2_db(u, h) / dot_v2v2_db(u, u);
  r_close[0] = l1[0] + u[0] * lambda;
  r_close[1] = l1[1] + u[1] * lambda;
  return lambda;
}

/**
 * If intersection == ISECT_LINE_LINE_CROSS or ISECT_LINE_LINE_NONE:
 * <pre>
 * pt = v1 + lamba * (v2 - v1) = v3 + mu * (v4 - v3)
 * </pre>
 */
static int isect_seg_seg_v2_lambda_mu_db(const double v1[2],
                                         const double v2[2],
                                         const double v3[2],
                                         const double v4[2],
                                         double *r_lambda,
                                         double *r_mu)
{
  double div, lambda, mu;

  div = (v2[0] - v1[0]) * (v4[1] - v3[1]) - (v2[1] - v1[1]) * (v4[0] - v3[0]);
  if (fabs(div) < DBL_EPSILON) {
    return ISECT_LINE_LINE_COLINEAR;
  }

  lambda = ((v1[1] - v3[1]) * (v4[0] - v3[0]) - (v1[0] - v3[0]) * (v4[1] - v3[1])) / div;

  mu = ((v1[1] - v3[1]) * (v2[0] - v1[0]) - (v1[0] - v3[0]) * (v2[1] - v1[1])) / div;

  if (r_lambda) {
    *r_lambda = lambda;
  }
  if (r_mu) {
    *r_mu = mu;
  }

  if (lambda >= 0.0 && lambda <= 1.0 && mu >= 0.0 && mu <= 1.0) {
    if (lambda == 0.0 || lambda == 1.0 || mu == 0.0 || mu == 1.0) {
      return ISECT_LINE_LINE_EXACT;
    }
    return ISECT_LINE_LINE_CROSS;
  }
  return ISECT_LINE_LINE_NONE;
}

/** return 1 if a,b,c forms CCW angle, -1 if a CW angle, 0 if straight  */
static int CCW_test(const double a[2], const double b[2], const double c[2])
{
  double det;
  double ab;

  /* This is twice the signed area of triangle abc. */
  det = (b[0] - a[0]) * (c[1] - a[1]) - (c[0] - a[0]) * (b[1] - a[1]);
  ab = len_v2v2_db(a, b);
  if (ab < DBL_EPSILON) {
    return 0;
  }
  det /= ab;
  if (det > DBL_EPSILON) {
    return 1;
  }
  else if (det < -DBL_EPSILON) {
    return -1;
  }
  return 0;
}

/** return true if a -- b -- c are in that order, assuming they are on a straight line. */
static bool in_line(const double a[2], const double b[2], const double c[2])
{
  double dir_ab[2], dir_ac[2];

  sub_v2_v2v2_db(dir_ab, a, b);
  sub_v2_v2v2_db(dir_ac, a, c);
  return dot_v2v2_db(dir_ab, dir_ac) >= 0.0;
}

#ifndef NDEBUG
/** Is s2 reachable from s1 by next pointers with < limit hops? */
static bool reachable(SymEdge *s1, SymEdge *s2, int limit)
{
  int count = 0;
  for (SymEdge *s = s1; s && count < limit; s = s->next) {
    if (s == s2) {
      return true;
    }
    count++;
  }
  return false;
}
#endif

static void calc_face_centroid(SymEdge *se)
{
  SymEdge *senext;
  double *centroidp = se->face->centroid;
  int count;
  copy_v2_v2_db(centroidp, se->vert->co);
  count = 1;
  for (senext = se->next; senext != se; senext = senext->next) {
    add_v2_v2_db(centroidp, senext->vert->co);
    count++;
  }
  centroidp[0] /= count;
  centroidp[1] /= count;
}

/** Using array to store these instead of linked list so can make a random selection from them. */
static CDTVert *add_cdtvert(CDT_state *cdt, double x, double y)
{
  CDTVert *v = BLI_memarena_alloc(cdt->arena, sizeof(*v));
  v->co[0] = x;
  v->co[1] = y;
  v->input_ids = NULL;
  v->symedge = NULL;
  if (cdt->vert_array_len == cdt->vert_array_len_alloc) {
    CDTVert **old_array = cdt->vert_array;
    cdt->vert_array_len_alloc *= 4;
    cdt->vert_array = BLI_memarena_alloc(cdt->arena,
                                         cdt->vert_array_len_alloc * sizeof(cdt->vert_array[0]));
    memmove(cdt->vert_array, old_array, cdt->vert_array_len * sizeof(cdt->vert_array[0]));
  }
  BLI_assert(cdt->vert_array_len < cdt->vert_array_len_alloc);
  v->index = cdt->vert_array_len;
  cdt->vert_array[cdt->vert_array_len++] = v;
  return v;
}

static CDTEdge *add_cdtedge(
    CDT_state *cdt, CDTVert *v1, CDTVert *v2, CDTFace *fleft, CDTFace *fright)
{
  CDTEdge *e = BLI_memarena_alloc(cdt->arena, sizeof(*e));
  SymEdge *se = &e->symedges[0];
  SymEdge *sesym = &e->symedges[1];
  e->input_ids = NULL;
  BLI_linklist_prepend_arena(&cdt->edges, (void *)e, cdt->arena);
  se->edge = sesym->edge = e;
  se->face = fleft;
  sesym->face = fright;
  se->vert = v1;
  if (v1->symedge == NULL) {
    v1->symedge = se;
  }
  sesym->vert = v2;
  if (v2->symedge == NULL) {
    v2->symedge = sesym;
  }
  se->next = sesym->next = se->rot = sesym->rot = NULL;
  return e;
}

static CDTFace *add_cdtface(CDT_state *cdt)
{
  CDTFace *f = BLI_memarena_alloc(cdt->arena, sizeof(*f));
  f->visit_index = 0;
  f->deleted = false;
  f->symedge = NULL;
  f->input_ids = NULL;
  BLI_linklist_prepend_arena(&cdt->faces, (void *)f, cdt->arena);
  return f;
}

static bool id_in_list(const LinkNode *id_list, int id)
{
  const LinkNode *ln;

  for (ln = id_list; ln; ln = ln->next) {
    if (POINTER_AS_INT(ln->link) == id) {
      return true;
    }
  }
  return false;
}

/** is any id in (range_start, range_start+1, ... , range_end) in id_list? */
static bool id_range_in_list(const LinkNode *id_list, int range_start, int range_end)
{
  const LinkNode *ln;
  int id;

  for (ln = id_list; ln; ln = ln->next) {
    id = POINTER_AS_INT(ln->link);
    if (id >= range_start && id <= range_end) {
      return true;
    }
  }
  return false;
}

static void add_to_input_ids(LinkNode **dst, int input_id, CDT_state *cdt)
{
  if (!id_in_list(*dst, input_id)) {
    BLI_linklist_prepend_arena(dst, POINTER_FROM_INT(input_id), cdt->arena);
  }
}

static void add_list_to_input_ids(LinkNode **dst, const LinkNode *src, CDT_state *cdt)
{
  const LinkNode *ln;

  for (ln = src; ln; ln = ln->next) {
    add_to_input_ids(dst, POINTER_AS_INT(ln->link), cdt);
  }
}

/** Return other #SymEdge for same #CDTEdge as se. */
static inline SymEdge *sym(const SymEdge *se)
{
  return se->next->rot;
}

/** Return SymEdge whose next is se. */
static inline SymEdge *prev(const SymEdge *se)
{
  return se->rot->next->rot;
}

static inline bool is_border_edge(const CDTEdge *e, const CDT_state *cdt)
{
  return e->symedges[0].face == cdt->outer_face || e->symedges[1].face == cdt->outer_face;
}

/** Does one edge of this edge touch the frame? */
static bool edge_touches_frame(const CDTEdge *e)
{
  return e->symedges[0].vert->index < 4 || e->symedges[1].vert->index < 4;
}

static inline bool is_constrained_edge(const CDTEdge *e)
{
  return e->input_ids != NULL;
}

static inline bool is_deleted_edge(const CDTEdge *e)
{
  return e->symedges[0].next == NULL;
}

/** Is there already an edge between a and b? */
static bool exists_edge(const CDTVert *a, const CDTVert *b)
{
  SymEdge *se, *ss;
  se = a->symedge;
  if (se->next->vert == b) {
    return true;
  }
  for (ss = se->rot; ss != se; ss = ss->rot) {
    if (ss->next->vert == b) {
      return true;
    }
  }
  return false;
}

/**
 * Assume s1 and s2 are both SymEdges in a face with > 3 sides,
 * and one is not the next of the other.
 * Add an edge from s1->v to s2->v, splitting the face in two.
 * The original face will continue to be associated with the subface
 * that has s1, and a new face will be made for s2's new face.
 * The centroids of both faces are recalculated.
 * Return the new diagonal's CDTEdge *.
 */
static CDTEdge *add_diagonal(CDT_state *cdt, SymEdge *s1, SymEdge *s2)
{
  CDTEdge *ediag;
  CDTFace *fold, *fnew;
  SymEdge *sdiag, *sdiagsym;
  SymEdge *s1prev, *s1prevsym, *s2prev, *s2prevsym, *se;
  BLI_assert(reachable(s1, s2, 20));
  BLI_assert(reachable(s2, s1, 20));
  fold = s1->face;
  fnew = add_cdtface(cdt);
  s1prev = prev(s1);
  s1prevsym = sym(s1prev);
  s2prev = prev(s2);
  s2prevsym = sym(s2prev);
  ediag = add_cdtedge(cdt, s1->vert, s2->vert, fnew, fold);
  sdiag = &ediag->symedges[0];
  sdiagsym = &ediag->symedges[1];
  sdiag->next = s2;
  sdiagsym->next = s1;
  s2prev->next = sdiagsym;
  s1prev->next = sdiag;
  s1->rot = sdiag;
  sdiag->rot = s1prevsym;
  s2->rot = sdiagsym;
  sdiagsym->rot = s2prevsym;
#ifdef DEBUG_CDT
  BLI_assert(reachable(s2, sdiag, 20));
#endif
  for (se = s2; se != sdiag; se = se->next) {
    se->face = fnew;
  }
  add_list_to_input_ids(&fnew->input_ids, fold->input_ids, cdt);
  calc_face_centroid(sdiag);
  calc_face_centroid(sdiagsym);
  return ediag;
}

/**
 * Split \a se at fraction \a lambda,
 * and return the new #CDTEdge that is the new second half.
 * Copy the edge input_ids into the new one.
 */
static CDTEdge *split_edge(CDT_state *cdt, SymEdge *se, double lambda)
{
  const double *a, *b;
  double p[2];
  CDTVert *v;
  CDTEdge *e;
  SymEdge *sesym, *newse, *newsesym, *senext, *sesymprev, *sesymprevsym;
  /* Split e at lambda. */
  a = se->vert->co;
  b = se->next->vert->co;
  sesym = sym(se);
  sesymprev = prev(sesym);
  sesymprevsym = sym(sesymprev);
  senext = se->next;
  p[0] = (1.0 - lambda) * a[0] + lambda * b[0];
  p[1] = (1.0 - lambda) * a[1] + lambda * b[1];
  v = add_cdtvert(cdt, p[0], p[1]);
  e = add_cdtedge(cdt, v, se->next->vert, se->face, sesym->face);
  sesym->vert = v;
  newse = &e->symedges[0];
  newsesym = &e->symedges[1];
  se->next = newse;
  newsesym->next = sesym;
  newse->next = senext;
  newse->rot = sesym;
  sesym->rot = newse;
  senext->rot = newsesym;
  newsesym->rot = sesymprevsym;
  sesymprev->next = newsesym;
  if (newsesym->vert->symedge == sesym) {
    newsesym->vert->symedge = newsesym;
  }
  add_list_to_input_ids(&e->input_ids, se->edge->input_ids, cdt);
  calc_face_centroid(se);
  calc_face_centroid(sesym);
  return e;
}

/**
 * Delete an edge from the structure. The new combined face on either side of
 * the deleted edge will be the one that was e's face; the centroid is updated.
 * There will be now an unused face, marked by setting its deleted flag,
 * and an unused #CDTEdge, marked by setting the next and rot pointers of
 * its SymEdges to NULL.
 * <pre>
 *        .  v2               .
 *       / \                 / \
 *      /f|j\               /   \
 *     /  |  \             /     \
 *        |
 *      A |  B                A
 *    \  e|   /           \       /
 *     \  | /              \     /
 *      \h|i/               \   /
 *        .  v1               .
 * </pre>
 * Also handle variant cases where one or both ends
 * are attached only to e.
 */
static void delete_edge(CDT_state *cdt, SymEdge *e)
{
  SymEdge *esym, *f, *h, *i, *j, *k, *jsym, *hsym;
  CDTFace *aface, *bface;
  CDTVert *v1, *v2;
  bool v1_isolated, v2_isolated;

  esym = sym(e);
  v1 = e->vert;
  v2 = esym->vert;
  aface = e->face;
  bface = esym->face;
  f = e->next;
  h = prev(e);
  i = esym->next;
  j = prev(esym);
  jsym = sym(j);
  hsym = sym(h);
  v1_isolated = (i == e);
  v2_isolated = (f == esym);

  if (!v1_isolated) {
    h->next = i;
    i->rot = hsym;
  }
  if (!v2_isolated) {
    j->next = f;
    f->rot = jsym;
  }
  if (!v1_isolated && !v2_isolated && aface != bface) {
    for (k = i; k != f; k = k->next) {
      k->face = aface;
    }
  }

  /* If e was representative symedge for v1 or v2, fix that. */
  if (v1_isolated) {
    v1->symedge = NULL;
  }
  else if (v1->symedge == e) {
    v1->symedge = i;
  }
  if (v2_isolated) {
    v2->symedge = NULL;
  }
  else if (v2->symedge == esym) {
    v2->symedge = f;
  }

  /* Mark SymEdge as deleted by setting all its pointers to NULL. */
  e->next = e->rot = NULL;
  esym->next = esym->rot = NULL;
  if (!v1_isolated && !v2_isolated && aface != bface) {
    bface->deleted = true;
    if (cdt->outer_face == bface) {
      cdt->outer_face = aface;
    }
  }
  if (aface != cdt->outer_face) {
    calc_face_centroid(f);
  }
}

/**
 * The initial structure will be the rectangle with opposite corners (minx,miny)
 * and (maxx,maxy), and a diagonal going between those two corners.
 * We keep track of the outer face (surrounding the entire structure; its boundary
 * is the clockwise traversal of the bounding box rectangle initially) in cdt->outer_face.
 *
 * The vertices are kept as pointers in an array (which may need to be reallocated from
 * time to time); the edges and faces are kept in lists. Sometimes edges and faces are deleted,
 * marked by setting all pointers to NULL (for edges), or setting the deleted flag to true (for
 * faces).
 *
 * A #MemArena is allocated to do all allocations from except for link list nodes; a listpool
 * is created for link list node allocations.
 *
 * The epsilon argument is stored and used in "near enough" distance calculations.
 *
 * When done, caller must call BLI_constrained_delaunay_free to free
 * the memory used by the returned #CDT_state.
 */
static CDT_state *cdt_init(double minx, double maxx, double miny, double maxy, double epsilon)
{
  double x0, x1, y0, y1;
  double margin;
  CDTVert *v[4];
  CDTEdge *e[4];
  CDTFace *f0, *fouter;
  int i, inext, iprev;
  MemArena *arena = BLI_memarena_new(DLNY_ARENASIZE, __func__);
  CDT_state *cdt = BLI_memarena_alloc(arena, sizeof(CDT_state));
  cdt->edges = NULL;
  cdt->faces = NULL;
  cdt->vert_array_len = 0;
  cdt->vert_array_len_alloc = 32;
  cdt->vert_array = BLI_memarena_alloc(arena,
                                       cdt->vert_array_len_alloc * sizeof(*cdt->vert_array));
  cdt->minx = minx;
  cdt->miny = miny;
  cdt->maxx = maxx;
  cdt->maxy = maxy;
  cdt->arena = arena;
  cdt->listpool = BLI_mempool_create(sizeof(LinkNode), 128, 128, 0);
  cdt->rng = BLI_rng_new(0);
  cdt->epsilon = epsilon;

  /* Expand bounding box a bit and make initial CDT from it. */
  margin = DLNY_MARGIN_PCT * max_dd(maxx - minx, maxy - miny) / 100.0;
  if (margin <= 0.0) {
    margin = 1.0;
  }
  if (margin < epsilon) {
    margin = 4 * epsilon; /* Make sure constraint verts don't merge with border verts. */
  }
  cdt->margin = margin;
  x0 = minx - margin;
  y0 = miny - margin;
  x1 = maxx + margin;
  y1 = maxy + margin;

  /* Make a quad, then split it with a diagonal. */
  v[0] = add_cdtvert(cdt, x0, y0);
  v[1] = add_cdtvert(cdt, x1, y0);
  v[2] = add_cdtvert(cdt, x1, y1);
  v[3] = add_cdtvert(cdt, x0, y1);
  cdt->outer_face = fouter = add_cdtface(cdt);
  f0 = add_cdtface(cdt);
  for (i = 0; i < 4; i++) {
    e[i] = add_cdtedge(cdt, v[i], v[(i + 1) % 4], f0, fouter);
  }
  for (i = 0; i < 4; i++) {
    inext = (i + 1) % 4;
    iprev = (i + 3) % 4;
    e[i]->symedges[0].next = &e[inext]->symedges[0];
    e[inext]->symedges[1].next = &e[i]->symedges[1];
    e[i]->symedges[0].rot = &e[iprev]->symedges[1];
    e[iprev]->symedges[1].rot = &e[i]->symedges[0];
  }
  calc_face_centroid(&e[0]->symedges[0]);
  add_diagonal(cdt, &e[0]->symedges[0], &e[2]->symedges[0]);
  fouter->centroid[0] = fouter->centroid[1] = 0.0;

  cdt->visit_count = 0;
  cdt->output_prepared = false;
  cdt->face_edge_offset = 0;
  return cdt;
}

static void cdt_free(CDT_state *cdt)
{
  BLI_rng_free(cdt->rng);
  BLI_mempool_destroy(cdt->listpool);
  BLI_memarena_free(cdt->arena);
}

static bool locate_point_final(const double p[2],
                               SymEdge *tri_se,
                               bool try_neighbors,
                               const double epsilon,
                               LocateResult *r_lr)
{
  /* 'p' should be in or on our just outside of 'cur_tri'. */
  double dist_inside[3];
  int i;
  SymEdge *se;
  const double *a, *b;
  double lambda, close[2];
  bool done = false;
#ifdef DEBUG_CDT
  int dbglevel = 0;
  if (dbglevel > 0) {
    fprintf(stderr, "locate_point_final %d\n", try_neighbors);
    dump_se(tri_se, "tri_se");
    fprintf(stderr, "\n");
  }
#endif
  se = tri_se;
  i = 0;
  do {
#ifdef DEBUG_CDT
    if (dbglevel > 1) {
      fprintf(stderr, "%d: ", i);
      dump_se(se, "search se");
    }
#endif
    a = se->vert->co;
    b = se->next->vert->co;
    lambda = closest_to_line_v2_db(close, p, a, b);
    double len_close_p = len_v2v2_db(close, p);
    if (len_close_p < epsilon) {
      if (len_v2v2_db(p, a) < epsilon) {
#ifdef DEBUG_CDT
        if (dbglevel > 0) {
          fprintf(stderr, "OnVert case a (%.2f,%.2f)\n", F2(a));
        }
#endif
        r_lr->loc_kind = OnVert;
        r_lr->se = se;
        r_lr->edge_lambda = 0.0;
        done = true;
      }
      else if (len_v2v2_db(p, b) < epsilon) {
#ifdef DEBUG_CDT
        if (dbglevel > 0) {
          fprintf(stderr, "OnVert case b (%.2f,%.2f)\n", F2(b));
        }
#endif
        r_lr->loc_kind = OnVert;
        r_lr->se = se->next;
        r_lr->edge_lambda = 0.0;
        done = true;
      }
      else if (lambda > 0.0 && lambda < 1.0) {
#ifdef DEBUG_CDT
        if (dbglevel > 0) {
          fprintf(stderr, "OnEdge case, lambda=%f\n", lambda);
          dump_se(se, "se");
        }
#endif
        r_lr->loc_kind = OnEdge;
        r_lr->se = se;
        r_lr->edge_lambda = lambda;
        done = true;
      }
    }
    else {
      dist_inside[i] = len_close_p;
      dist_inside[i] = CCW_test(a, b, p) >= 0 ? len_close_p : -len_close_p;
    }
    i++;
    se = se->next;
  } while (se != tri_se && !done);
  if (!done) {
#ifdef DEBUG_CDT
    if (dbglevel > 1) {
      fprintf(stderr,
              "not done, dist_inside=%f %f %f\n",
              dist_inside[0],
              dist_inside[1],
              dist_inside[2]);
    }
#endif
    if (dist_inside[0] >= 0.0 && dist_inside[1] >= 0.0 && dist_inside[2] >= 0.0) {
#ifdef DEBUG_CDT
      if (dbglevel > 0) {
        fprintf(stderr, "InFace case\n");
        dump_se_cycle(tri_se, "tri", 10);
      }
#endif
      r_lr->loc_kind = InFace;
      r_lr->se = tri_se;
      r_lr->edge_lambda = 0.0;
      done = true;
    }
    else if (try_neighbors) {
      for (se = tri_se->next; se != tri_se; se = se->next) {
        if (locate_point_final(p, se, false, epsilon, r_lr)) {
          done = true;
          break;
        }
      }
      if (!done) {
        /* Shouldn't happen desperation mode: pick something. */
        se = NULL;
        if (dist_inside[0] > 0) {
          se = tri_se;
        }
        if (dist_inside[1] > 0 && (se == NULL || dist_inside[1] < dist_inside[i])) {
          se = tri_se->next;
        }
        if (se == NULL) {
          se = tri_se->next->next;
        }
        a = se->vert->co;
        b = se->next->vert->co;
        lambda = closest_to_line_v2_db(close, p, a, b);
        if (lambda <= 0.0) {
          r_lr->loc_kind = OnVert;
          r_lr->se = se;
          r_lr->edge_lambda = 0.0;
        }
        else if (lambda >= 1.0) {
          r_lr->loc_kind = OnVert;
          r_lr->se = se->next;
          r_lr->edge_lambda = 0.0;
        }
        else {
          r_lr->loc_kind = OnEdge;
          r_lr->se = se->next;
          r_lr->edge_lambda = lambda;
        }
#ifdef DEBUG_CDT
        if (dbglevel > 0) {
          fprintf(
              stderr, "desperation case kind=%u lambda=%f\n", r_lr->loc_kind, r_lr->edge_lambda);
          dump_se(r_lr->se, "se");
          BLI_assert(0); /* While developing, catch these "should not happens" */
        }
#endif
        fprintf(stderr, "desperation!\n");  // TODO: remove
        return true;
      }
    }
  }
  return done;
}

static LocateResult locate_point(CDT_state *cdt, const double p[2])
{
  LocateResult lr;
  SymEdge *cur_se, *next_se, *next_se_sym;
  CDTFace *cur_tri;
  bool done;
  int sample_n, i, k;
  CDTVert *v, *best_start_vert;
  double dist_squared, best_dist_squared;
  double *a, *b, *c;
  const double epsilon = cdt->epsilon;
  int visit = ++cdt->visit_count;
  int loop_count = 0;
#ifdef DEBUG_CDT
  int dbglevel = 0;

  if (dbglevel > 0) {
    fprintf(stderr, "locate_point (%.2f,%.2f), visit_index=%d\n", F2(p), visit);
  }
#endif
  /* Starting point determined by closest to p in an n ** (1/3) sized sample of current points. */
  BLI_assert(cdt->vert_array_len > 0);
  sample_n = (int)round(pow((double)cdt->vert_array_len, 0.33333));
  if (sample_n < 1) {
    sample_n = 1;
  }
  best_start_vert = NULL;
  best_dist_squared = DBL_MAX;
  for (k = 0; k < sample_n; k++) {
    /* Yes, this may try some i's more than once,
     * but will still get about an n ** (1/3) size sample. */
    i = (int)(BLI_rng_get_uint(cdt->rng) % cdt->vert_array_len);
    v = cdt->vert_array[i];
    dist_squared = len_squared_v2v2_db(p, v->co);
#ifdef DEBUG_CDT
    if (dbglevel > 0) {
      fprintf(stderr, "try start vert %d, dist_squared=%f\n", i, dist_squared);
      dump_v(v, "v");
    }
#endif
    if (dist_squared < best_dist_squared) {
      best_dist_squared = dist_squared;
      best_start_vert = v;
    }
  }
  cur_se = &best_start_vert->symedge[0];
  if (cur_se->face == cdt->outer_face) {
    cur_se = cur_se->rot;
    BLI_assert(cur_se->face != cdt->outer_face);
  }
#ifdef DEBUG_CDT
  if (dbglevel > 0) {
    dump_se(cur_se, "start vert edge");
  }
#endif
  done = false;
  while (!done) {
    /* Find edge of cur_tri that separates p and t's centroid,
     * and where other tri over the edge is unvisited. */
#ifdef DEBUG_CDT
    if (dbglevel > 0) {
      dump_se_cycle(cur_se, "cur search face", 5);
    }
#endif
    cur_tri = cur_se->face;
    BLI_assert(cur_tri != cdt->outer_face);
    cur_tri->visit_index = visit;
    /* Is p in or on current triangle? */
    a = cur_se->vert->co;
    b = cur_se->next->vert->co;
    c = cur_se->next->next->vert->co;
    if (CCW_test(a, b, p) >= 0 && CCW_test(b, c, p) >= 0 && CCW_test(c, a, p) >= 0) {
#ifdef DEBUG_CDT
      if (dbglevel > 1) {
        fprintf(stderr, "p in current triangle\n");
      }
#endif
      done = locate_point_final(p, cur_se, false, epsilon, &lr);
      BLI_assert(done == true);
      break;
    }
    bool found_next = false;
    next_se = cur_se;
    do {
      a = next_se->vert->co;
      b = next_se->next->vert->co;
      c = next_se->next->next->vert->co;
#ifdef DEBUG_CDT
      if (dbglevel > 1) {
        dump_se(next_se, "search edge");
        fprintf(stderr, "tri centroid=(%.2f,%.2f)\n", F2(cur_tri->centroid));
        validate_face_centroid(next_se);
      }
#endif
      next_se_sym = sym(next_se);
      if (CCW_test(a, b, p) <= 0 && next_se->face != cdt->outer_face) {
#ifdef DEBUG_CDT
        if (dbglevel > 1) {
          fprintf(stderr, "CCW_test(a, b, p) <= 0\n");
        }
#endif
#ifdef DEBUG_CDT
        if (dbglevel > 0) {
          dump_se(next_se_sym, "next_se_sym");
          fprintf(stderr, "next_se_sym face visit=%d\n", next_se_sym->face->visit_index);
        }
#endif
        if (next_se_sym->face->visit_index != visit) {
#ifdef DEBUG_CDT
          if (dbglevel > 0) {
            fprintf(stderr, "found edge to cross\n");
          }
#endif
          found_next = true;
          cur_se = next_se_sym;
          break;
        }
      }
      next_se = next_se->next;
    } while (next_se != cur_se);
    if (!found_next) {
      done = locate_point_final(p, cur_se, true, epsilon, &lr);
      BLI_assert(done = true);
      done = true;
    }
    if (++loop_count > 1000000) {
      fprintf(stderr, "infinite search loop?\n");
      done = locate_point_final(p, cur_se, true, epsilon, &lr);
    }
  }

  return lr;
}

/** return true if circumcircle(v1, v2, v3) does not contain p. */
static bool delaunay_check(CDTVert *v1, CDTVert *v2, CDTVert *v3, CDTVert *p, const double epsilon)
{
  double a, b, c, d, z1, z2, z3;
  const double *p1, *p2, *p3;
  double cen[2], r, len_pc;
  /* To do epislon test, need center and radius of circumcircle. */
  p1 = v1->co;
  p2 = v2->co;
  p3 = v3->co;
  z1 = dot_v2v2_db(p1, p1);
  z2 = dot_v2v2_db(p2, p2);
  z3 = dot_v2v2_db(p3, p3);
  a = p1[0] * (p2[1] - p3[1]) - p1[1] * (p2[0] - p3[0]) + p2[0] * p3[1] - p3[0] * p2[1];
  b = z1 * (p3[1] - p2[1]) + z2 * (p1[1] - p3[1]) + z3 * (p2[1] - p1[1]);
  c = z1 * (p2[0] - p3[0]) + z2 * (p3[0] - p1[0]) + z3 * (p1[0] - p2[0]);
  d = z1 * (p3[0] * p2[1] - p2[0] * p3[1]) + z2 * (p1[0] * p3[1] - p3[0] * p1[1]) +
      z3 * (p2[0] * p1[1] - p1[0] * p2[1]);
  if (a == 0.0) {
    return true; /* Not really, but this shouldn't happen. */
  }
  cen[0] = -b / (2 * a);
  cen[1] = -c / (2 * a);
  r = sqrt((b * b + c * c - 4 * a * d) / (4 * a * a));
  len_pc = len_v2v2_db(p->co, cen);
  return (len_pc >= (r - epsilon));
}

/** Use LinkNode linked list as stack of SymEdges, allocating from cdt->listpool. */
typedef LinkNode *Stack;

static inline void push(Stack *stack, SymEdge *se, CDT_state *cdt)
{
  BLI_linklist_prepend_pool(stack, se, cdt->listpool);
}

static inline SymEdge *pop(Stack *stack, CDT_state *cdt)
{
  return (SymEdge *)BLI_linklist_pop_pool(stack, cdt->listpool);
}

static inline bool is_empty(Stack *stack)
{
  return *stack == NULL;
}

/**
 * <pre>
 *       /\                  /\
 *      /a|\                /  \
 *     /  | sesym          /    \
 *    /   |  \            /      \
 *   . b  | d .  ->      . se______
 *    \ se|  /            \       /
 *     \  |c/              \     /
 *      \ |/                \   /
 * </pre>
 */
static void flip(SymEdge *se, CDT_state *cdt)
{
  SymEdge *a, *b, *c, *d;
  SymEdge *sesym, *asym, *bsym, *csym, *dsym;
  CDTFace *t1, *t2;
  CDTVert *v1, *v2;
#ifdef DEBUG_CDT
  const int dbglevel = 0;
#endif

  sesym = sym(se);
#ifdef DEBUG_CDT
  if (dbglevel > 0) {
    fprintf(stderr, "flip\n");
    dump_se(se, "se");
    dump_se(sesym, "sesym");
  }
#endif
  a = se->next;
  b = a->next;
  c = sesym->next;
  d = c->next;
  asym = sym(a);
  bsym = sym(b);
  csym = sym(c);
  dsym = sym(d);
#ifdef DEBUG_CDT
  if (dbglevel > 1) {
    dump_se(a, "a");
    dump_se(b, "b");
    dump_se(c, "c");
    dump_se(d, "d");
  }
#endif
  v1 = se->vert;
  v2 = sesym->vert;
  t1 = a->face;
  t2 = c->face;

  se->vert = b->vert;
  sesym->vert = d->vert;

  a->next = se;
  se->next = d;
  d->next = a;

  sesym->next = b;
  b->next = c;
  c->next = sesym;

  a->rot = dsym;
  b->rot = se;
  se->rot = asym;

  c->rot = bsym;
  d->rot = sesym;
  sesym->rot = csym;

  a->face = se->face = d->face = t1;
  sesym->face = b->face = c->face = t2;

  if (v1->symedge == se) {
    v1->symedge = c;
  }
  if (v2->symedge == sesym) {
    v2->symedge = a;
  }

  calc_face_centroid(a);
  calc_face_centroid(sesym);

#ifdef DEBUG_CDT
  if (dbglevel > 0) {
    fprintf(stderr, "after flip\n");
    dump_se_cycle(a, "a cycle", 5);
    dump_se_cycle(sesym, "sesym cycle", 5);
  }
#endif
  if (cdt) {
    /* Pass. */
  }
}

static void flip_edges(CDTVert *v, Stack *stack, CDT_state *cdt)
{
  SymEdge *se, *sesym;
  CDTVert *a, *b, *c, *d;
  SymEdge *tri_without_p;
  bool is_delaunay;
  const double epsilon = cdt->epsilon;
  int count = 0;
#ifdef DEBUG_CDT
  const int dbglevel = 0;
  if (dbglevel > 0) {
    fprintf(stderr, "flip_edges, v=(%.2f,%.2f)\n", F2(v->co));
  }
#endif
  while (!is_empty(stack)) {
    if (++count > 10000) {
      fprintf(stderr, "infinite flip loop?\n");
      return;
    }
    se = pop(stack, cdt);
#ifdef DEBUG_CDT
    if (dbglevel > 0) {
      dump_se(se, "flip_edges popped");
    }
#endif
    if (!is_constrained_edge(se->edge)) {
      /* Edge is not constrained; is it Delaunay? */
#ifdef DEBUG_CDT
      if (dbglevel > 1) {
        dump_se_cycle(se, "unconstrained edge", 5);
      }
      else if (dbglevel > 0) {
        fprintf(stderr, "unconstrained edge\n");
      }
#endif
      a = se->vert;
      b = se->next->vert;
      c = se->next->next->vert;
      sesym = sym(se);
      d = sesym->next->next->vert;
#ifdef DEBUG_CDT
      if (dbglevel > 1) {
        fprintf(stderr, "a=(%.2f,%.2f) b=(%.2f,%.2f)\n", F2(a->co), F2(b->co));
        fprintf(stderr, "c=(%.2f,%.2f) d=(%.2f,%.2f)\n", F2(c->co), F2(d->co));
      }
#endif
      if (v == c) {
        tri_without_p = sesym;
        is_delaunay = delaunay_check(a, b, c, d, epsilon);
#ifdef DEBUG_CDT
        if (dbglevel > 1) {
          fprintf(stderr, "v==c, delaunay(a,b,c,d)=%d\n", is_delaunay);
        }
#endif
      }
      else {
        tri_without_p = se;
        BLI_assert(d == v);
        is_delaunay = delaunay_check(b, a, d, c, epsilon);
#ifdef DEBUG_CDT
        if (dbglevel > 1) {
          fprintf(stderr, "v!=c, delaunay(b,a,d,c)=%d\n", is_delaunay);
        }
#endif
      }
      if (!is_delaunay) {
        /* Push two edges of tri without p that aren't se. */
#ifdef DEBUG_CDT
        if (dbglevel > 0) {
          fprintf(stderr, "maybe pushing more edges\n");
        }
#endif
        if (!is_border_edge(tri_without_p->next->edge, cdt)) {
#ifdef DEBUG_CDT
          if (dbglevel > 0) {
            dump_se(tri_without_p->next, "push1");
          }
#endif
          push(stack, tri_without_p->next, cdt);
        }
        if (!is_border_edge(tri_without_p->next->next->edge, cdt)) {
#ifdef DEBUG_CDT
          if (dbglevel > 0) {
            dump_se(tri_without_p->next->next, "\npush2");
          }
#endif
          push(stack, tri_without_p->next->next, cdt);
        }
        flip(se, cdt);
      }
    }
  }
}

/**
 * Splits e at lambda and returns a #SymEdge with new vert as its vert.
 * The two opposite triangle vertices to e are connect to new point.
 * <pre>
 *       /\                  /\
 *      /f|\                / |\
 *     /  |j\              /  | \
 *    /   | i\            /  k|  \
 *   .    |   .  ->      . l_ p m_.
 *    \g  |  /            \    |  /
 *     \  |h/              \   | /
 *      \e|/                \ e|/
 *
 * t1 = {e, f, g}; t2 = {h, i, j};
 * t1' = {e, l.sym, g}; t2' = {h, m.sym, e'.sym}
 * t3 = {k, f, l}; t4 = {m, i, j}
 * </pre>
 */
static CDTVert *insert_point_in_edge(CDT_state *cdt, SymEdge *e, double lambda)
{
  SymEdge *f, *g, *h, *i, *j, *k;
  CDTEdge *ke;
  CDTVert *p;
  Stack stack;
  /* Split e at lambda. */

  f = e->next;
  g = f->next;
  BLI_assert(g->next == e);
  j = sym(e);
  h = j->next;
  i = h->next;
  BLI_assert(i->next == j);

  ke = split_edge(cdt, e, lambda);
  k = &ke->symedges[0];
  p = k->vert;

  add_diagonal(cdt, g, k);
  add_diagonal(cdt, sym(e), i);

  stack = NULL;
  if (!is_border_edge(f->edge, cdt)) {
    push(&stack, f, cdt);
  }
  if (!is_border_edge(g->edge, cdt)) {
    push(&stack, g, cdt);
  }
  if (!is_border_edge(h->edge, cdt)) {
    push(&stack, h, cdt);
  }
  if (!is_border_edge(i->edge, cdt)) {
    push(&stack, i, cdt);
  }
  flip_edges(k->vert, &stack, cdt);
  return p;
}

/**
 * Inserts p inside e's triangle and connects the three cornders
 * of the triangle to the new point. Returns a SymEdge that has
 * new point as its point.
 * <pre>
 *               *                                *
 *             *g  *                            * .j*
 *           *       *                        *   .   *
 *         *     p     *       ->           *  1. p .  3*
 *       *               *                *  .         .  *
 *     *   e              f*            *  . h     2    i . *
 *   * * * * * * * * * * * * *        * * * * * * * * * * * * *
 * </pre>
 */
static CDTVert *insert_point_in_face(CDT_state *cdt, SymEdge *e, const double p[2])
{
  SymEdge *f, *g, *h, *i, *j;
  SymEdge *esym, *fsym, *gsym, *hsym, *isym, *jsym;
  CDTVert *v;
  CDTEdge *he, *ie, *je;
  CDTFace *t1, *t2, *t3;
  Stack stack;

  f = e->next;
  g = f->next;
  esym = sym(e);
  fsym = sym(f);
  gsym = sym(g);
  t1 = e->face;
  t2 = add_cdtface(cdt);
  t3 = add_cdtface(cdt);

  v = add_cdtvert(cdt, p[0], p[1]);
  he = add_cdtedge(cdt, e->vert, v, t1, t2);
  h = &he->symedges[0];
  hsym = &he->symedges[1];
  ie = add_cdtedge(cdt, f->vert, v, t2, t3);
  i = &ie->symedges[0];
  isym = &ie->symedges[1];
  je = add_cdtedge(cdt, g->vert, v, t3, t1);
  j = &je->symedges[0];
  jsym = &je->symedges[1];

  e->next = i;
  i->next = hsym;
  hsym->next = e;
  e->face = t2;

  f->next = j;
  j->next = isym;
  isym->next = f;
  f->face = t3;

  g->next = h;
  h->next = jsym;
  jsym->next = g;
  g->face = t1;

  e->rot = h;
  i->rot = esym;
  hsym->rot = isym;

  f->rot = i;
  j->rot = fsym;
  isym->rot = jsym;

  g->rot = j;
  h->rot = gsym;
  jsym->rot = hsym;

  calc_face_centroid(e);
  calc_face_centroid(f);
  calc_face_centroid(g);

  stack = NULL;
  if (!is_border_edge(e->edge, cdt)) {
    push(&stack, e, cdt);
  }
  if (!is_border_edge(f->edge, cdt)) {
    push(&stack, f, cdt);
  }
  if (!is_border_edge(g->edge, cdt)) {
    push(&stack, g, cdt);
  }
  flip_edges(v, &stack, cdt);

  return v;
}

/**
 * Re-triangulates, assuring constrained delaunay condition,
 * the pseudo-polygon that cycles from se.
 * "pseudo" because a vertex may be repeated.
 * See Anglada paper, "An Improved incremental algorithm
 * for constructing restricted Delaunay triangulations".
 */
static void re_delaunay_triangulate(CDT_state *cdt, SymEdge *se)
{
  SymEdge *ss, *first, *cse;
  CDTVert *a, *b, *c, *v;
  CDTEdge *ebc, *eca;
  const double epsilon = cdt->epsilon;
  int count;
#ifdef DEBUG_CDT
  SymEdge *last;
  const int dbg_level = 0;

  if (dbg_level > 0) {
    fprintf(stderr, "retriangulate");
    dump_se_cycle(se, "poly ", 1000);
  }
#endif
  /* 'se' is a diagonal just added, and it is base of area to retriangulate (face on its left) */
  count = 1;
  for (ss = se->next; ss != se; ss = ss->next) {
    count++;
  }
  if (count <= 3) {
#ifdef DEBUG_CDT
    if (dbg_level > 0) {
      fprintf(stderr, "nothing to do\n");
    }
#endif
    return;
  }
  /* First and last are the SymEdges whose verts are first and last off of base,
   * continuing from 'se'. */
  first = se->next->next;
  /* We want to make a triangle with 'se' as base and some other c as 3rd vertex. */
  a = se->vert;
  b = se->next->vert;
  c = first->vert;
  cse = first;
#ifdef DEBUG_CDT
  last = prev(se);
  if (dbg_level > 1) {
    dump_se(first, "first");
    dump_se(last, "last");
    dump_v(a, "a");
    dump_v(b, "b");
    dump_v(c, "c");
  }
#endif
  for (ss = first->next; ss != se; ss = ss->next) {
    v = ss->vert;
    if (!delaunay_check(a, b, c, v, epsilon)) {
      c = v;
      cse = ss;
#ifdef DEBUG_CDT
      if (dbg_level > 1) {
        dump_v(c, "new c ");
      }
#endif
    }
  }
  /* Add diagonals necessary to make abc a triangle. */
#ifdef DEBUG_CDT
  if (dbg_level > 0) {
    fprintf(stderr, "make triangle abc exist where\n");
    dump_v(a, "  a");
    dump_v(b, "  b");
    dump_v(c, "  c");
  }
#endif
  ebc = NULL;
  eca = NULL;
  if (!exists_edge(b, c)) {
    ebc = add_diagonal(cdt, se->next, cse);
#ifdef DEBUG_CDT
    if (dbg_level > 1) {
      fprintf(stderr, "added edge ebc\n");
      dump_se(&ebc->symedges[0], "  ebc");
    }
#endif
  }
  if (!exists_edge(c, a)) {
    eca = add_diagonal(cdt, cse, se);
#ifdef DEBUG_CDT
    if (dbg_level > 1) {
      fprintf(stderr, "added edge eca\n");
      dump_se(&eca->symedges[0], "  eca");
    }
#endif
  }
  /* Now recurse. */
  if (ebc) {
    re_delaunay_triangulate(cdt, &ebc->symedges[1]);
  }
  if (eca) {
    re_delaunay_triangulate(cdt, &eca->symedges[1]);
  }
}

/**
 * Add a constrained point to cdt structure, and return the corresponding CDTVert*.
 * May not be at exact coords given, because it can be merged with an existing vertex
 * or moved to an existing edge (which could be a triangulation edge, not just a constraint one)
 * if the point is within cdt->epsilon of those other elements.
 *
 * input_id will be added to the list of input_ids for the returned CDTVert (don't use -1 for id).
 *
 * Assumes cdt has been initialized, with min/max bounds that contain coords.
 * Assumes that #BLI_constrained_delaunay_get_output has not been called yet.
 */
static CDTVert *add_point_constraint(CDT_state *cdt, const double coords[2], int input_id)
{
  LocateResult lr;
  CDTVert *v;
#ifdef DEBUG_CDT
  const int dbg_level = 0;
#endif

  BLI_assert(!cdt->output_prepared);
#ifdef DEBUG_CDT
  if (dbg_level > 0) {
    fprintf(stderr, "add point constraint (%.3f,%.3f), id=%d\n", F2(coords), input_id);
  }
#endif
  lr = locate_point(cdt, coords);
#ifdef DEBUG_CDT
  if (dbg_level > 0) {
    fprintf(stderr, "  locate result has loc_kind %u\n", lr.loc_kind);
  }
#endif
  if (lr.loc_kind == OnVert) {
    v = lr.se->vert;
  }
  else if (lr.loc_kind == OnEdge) {
    v = insert_point_in_edge(cdt, lr.se, lr.edge_lambda);
  }
  else {
    v = insert_point_in_face(cdt, lr.se, coords);
  }
  add_to_input_ids(&v->input_ids, input_id, cdt);
  return v;
}

/**
 * Add a constrained edge between v1 and v2 to cdt structure.
 * This may result in a number of #CDTEdges created, due to intersections
 * and partial overlaps with existing cdt vertices and edges.
 * Each created #CDTEdge will have input_id added to its input_ids list.
 *
 * If \a r_edges is not NULL, the #CDTEdges generated or found that go from
 * v1 to v2 are put into that linked list, in order.
 *
 * Assumes that #BLI_constrained_delaunay_get_output has not been called yet.
 */
static void add_edge_constraint(
    CDT_state *cdt, CDTVert *v1, CDTVert *v2, int input_id, LinkNode **r_edges)
{
  CDTVert *va, *vb, *vc;
  SymEdge *vse1;
#ifdef DEBUG_CDT
  SymEdge *vse2;
#endif
  SymEdge *t, *tstart, *tout, *tnext;
  SymEdge *se;
  CDTEdge *edge;
  int ccw1, ccw2, isect;
  int i, search_count;
  double lambda;
  bool done, state_through_vert;
  LinkNodePair edge_list = {NULL, NULL};
  typedef struct CrossData {
    double lambda;
    CDTVert *vert;
    SymEdge *in;
    SymEdge *out;
  } CrossData;
  CrossData cdata;
  CrossData *crossings = NULL;
  CrossData *cd;
  BLI_array_staticdeclare(crossings, 128);
#ifdef DEBUG_CDT
  const int dbg_level = 0;
#endif

  /* Find path through structure from v1 to v2 and record how we got there in crossings.
   * In crossings array, each CrossData is populated as follows:
   *
   * If ray from previous node goes through a face, not along an edge:
   *
   *               _  B
   *            /    |\
   *         - -     | \
   *     prev........X  \
   *       \ d       |   \C
   *        --       |   /
   *          \     a| b/
   *           - -   | /
   *               \  A
   *
   *   lambda = fraction of way along AB where X is.
   *   vert = NULL initially, will later get new node that splits AB
   *   in = a (SymEdge from A->B, whose face the ray goes through)
   *   out = b (SymEdge from A->C, whose face the ray goes through next
   *
   * If the ray from the previous node goes directly to an existing vertex, say A
   * in the previous diagram, maybe along an existing edge like d in that diagram
   * but if prev had lambda !=0 then there may be no such edge d, then:
   *
   *    lambda = 0
   *    vert = A
   *    in = a
   *    out = b
   *
   * crossings[0] will have in = NULL, and crossings[last] will have out = NULL
   */
  if (r_edges) {
    *r_edges = NULL;
  }
  vse1 = v1->symedge;
#ifdef DEBUG_CDT
  if (dbg_level > 0) {
    vse2 = v2->symedge;
    fprintf(stderr, "\ninsert_segment %d\n", input_id);
    dump_v(v1, "  1");
    dump_v(v2, "  2");
    if (dbg_level > 1) {
      dump_se(vse1, "  se1");
      dump_se(vse2, "  se2");
    }
  }
#endif
  if (v1 == v2) {
#ifdef DEBUG_CDT
    if (dbg_level > 0) {
      fprintf(stderr, "segment between same vertices, ignored\n");
    }
#endif
    return;
  }
  state_through_vert = true;
  done = false;
  t = vse1;
  search_count = 0;
  while (!done) {
    /* Invariant: crossings[0 .. BLI_array_len(crossings)] has crossing info for path up to
     * but not including the crossing of edge t, which will either be through a vert
     * (if state_through_vert is true) or through edge t not at either end.
     * In the latter case, t->face is the face that ray v1--v2 goes through after path-so-far.
     */
#ifdef DEBUG_CDT
    if (dbg_level > 1) {
      fprintf(
          stderr, "top of insert_segment main loop, state_through_vert=%d\n", state_through_vert);
      dump_se_cycle(t, "current t ", 4);
    }
#endif
    if (state_through_vert) {
      /* Invariant: ray v1--v2 contains t->vert. */
      cdata.in = (BLI_array_len(crossings) == 0) ? NULL : t;
      cdata.out = NULL; /* To be filled in if this isn't final. */
      cdata.lambda = 0.0;
      cdata.vert = t->vert;
      BLI_array_append(crossings, cdata);
      if (t->vert == v2) {
#ifdef DEBUG_CDT
        if (dbg_level > 0) {
          fprintf(stderr, "found v2, so done\n");
        }
#endif
        done = true;
      }
      else {
        /* Do ccw scan of triangles around t->vert to find exit triangle for ray v1--v2. */
        tstart = t;
        tout = NULL;
        do {
          va = t->next->vert;
          vb = t->next->next->vert;
          ccw1 = CCW_test(t->vert->co, va->co, v2->co);
          ccw2 = CCW_test(t->vert->co, vb->co, v2->co);
#ifdef DEBUG_CDT
          if (dbg_level > 1) {
            fprintf(stderr, "non-final through vert case\n");
            dump_v(va, " va");
            dump_v(vb, " vb");
            fprintf(stderr, "ccw1=%d, ccw2=%d\n", ccw1, ccw2);
          }
#endif
          if (ccw1 == 0 && in_line(t->vert->co, va->co, v2->co)) {
#ifdef DEBUG_CDT
            if (dbg_level > 0) {
              fprintf(stderr, "ray goes through va\n");
            }
#endif
            state_through_vert = true;
            tout = t;
            t = t->next;
            break;
          }
          else if (ccw2 == 0 && in_line(t->vert->co, vb->co, v2->co)) {
#ifdef DEBUG_CDT
            if (dbg_level > 0) {
              fprintf(stderr, "ray goes through vb\n");
            }
#endif
            state_through_vert = true;
            t = t->next->next;
            tout = sym(t);
            break;
          }
          else if (ccw1 > 0 && ccw2 < 0) {
#ifdef DEBUG_CDT
            if (dbg_level > 0) {
              fprintf(stderr, "segment intersects\n");
            }
#endif
            state_through_vert = false;
            tout = t;
            t = t->next;
            break;
          }
          t = t->rot;
#ifdef DEBUG_CDT
          if (dbg_level > 1) {
            dump_se_cycle(t, "next rot tri", 4);
          }
#endif
        } while (t != tstart);
        BLI_assert(tout != NULL); /* TODO: something sensivle for "this can't happen" */
        crossings[BLI_array_len(crossings) - 1].out = tout;
      }
    }
    else { /* State is "through edge", not "through vert" */
      /* Invariant: ray v1--v2 intersects segment t->edge, not at either end.
       * and t->face is the face we have just passed through. */
      va = t->vert;
      vb = t->next->vert;
#ifdef DEBUG_CDT
      if (dbg_level > 1) {
        fprintf(stderr, "through edge case\n");
        dump_v(va, " va");
        dump_v(vb, " vb");
      }
#endif
      isect = isect_seg_seg_v2_lambda_mu_db(va->co, vb->co, v1->co, v2->co, &lambda, NULL);
      /* TODO: something sensible for "this can't happen" */
      BLI_assert(isect == ISECT_LINE_LINE_CROSS);
      UNUSED_VARS_NDEBUG(isect);
#ifdef DEBUG_CDT
      if (dbg_level > 0) {
        fprintf(stderr, "intersect point at %f along va--vb\n", lambda);
        if (dbg_level == 1) {
          dump_v(va, " va");
          dump_v(vb, " vb");
        }
      }
#endif
      tout = sym(t)->next;
      cdata.in = t;
      cdata.out = tout;
      cdata.lambda = lambda;
      cdata.vert = NULL; /* To be filled in with edge split vertex later. */
      BLI_array_append(crossings, cdata);
#ifdef DEBUG_CDT
      if (dbg_level > 0) {
        dump_se_cycle(tout, "next search tri", 4);
      }
#endif
      /* 'tout' is 'symedge' from 'vb' to third vertex, 'vc'. */
      BLI_assert(tout->vert == va);
      vc = tout->next->vert;
      ccw1 = CCW_test(v1->co, v2->co, vc->co);
#ifdef DEBUG_CDT
      if (dbg_level > 1) {
        fprintf(stderr, "now searching with third vertex ");
        dump_v(vc, "vc");
        fprintf(stderr, "ccw(v1, v2, vc) = %d\n", ccw1);
      }
#endif
      if (ccw1 == -1) {
        /* v1--v2 should intersect vb--vc. */
#ifdef DEBUG_CDT
        if (dbg_level > 1) {
          fprintf(stderr, "v1--v2 intersects vb--vc\n");
        }
#endif
        t = tout->next;
        state_through_vert = false;
      }
      else if (ccw1 == 1) {
        /* v1--v2 should intersect va--vc. */
#ifdef DEBUG_CDT
        if (dbg_level > 1) {
          fprintf(stderr, "v1--v2 intersects va--vc\n");
        }
#endif
        t = tout;
        state_through_vert = false;
      }
      else {
        /* ccw1 == 0. */
#ifdef DEBUG_CDT
        if (dbg_level > 1) {
          fprintf(stderr, "ccw==0 case, so going through or to vc\n");
        }
#endif
        t = tout->next;
        state_through_vert = true;
      }
    }
    if (++search_count > 10000) {
      fprintf(stderr, "infinite loop? bailing out\n");
      BLI_assert(0); /* Catch these while developing. */
      break;
    }
  }
#ifdef DEBUG_CDT
  if (dbg_level > 0) {
    fprintf(stderr, "Crossing info gathered:\n");
    for (i = 0; i < BLI_array_len(crossings); i++) {
      cd = &crossings[i];
      fprintf(stderr, "%d:\n", i);
      if (cd->vert != NULL) {
        dump_v(cd->vert, "  vert: ");
      }
      else {
        fprintf(stderr, "  lambda=%f along in\n", cd->lambda);
      }
      if (cd->in) {
        dump_se(cd->in, "  in: ");
      }
      if (cd->out) {
        dump_se(cd->out, "  out: ");
      }
    }
  }
#endif

  if (BLI_array_len(crossings) == 2) {
    /* For speed, handle special case of segment must have already been there. */
    se = crossings[1].in;
    if (se->next->vert != v1) {
      se = prev(se);
    }
    BLI_assert(se->vert == v1 || se->next->vert == v1);
#ifdef DEBUG_CDT
    if (dbg_level > 0) {
      fprintf(stderr, "segment already there: ");
      dump_se(se, "");
    }
#endif
    add_to_input_ids(&se->edge->input_ids, input_id, cdt);
    if (r_edges != NULL) {
      BLI_linklist_append_pool(&edge_list, se->edge, cdt->listpool);
    }
  }
  else {
    /* Insert all intersection points. */
    for (i = 0; i < BLI_array_len(crossings); i++) {
      cd = &crossings[i];
      if (cd->lambda != 0.0 && is_constrained_edge(cd->in->edge)) {
        edge = split_edge(cdt, cd->in, cd->lambda);
        cd->vert = edge->symedges[0].vert;
#ifdef DEBUG_CDT
        if (dbg_level > 1) {
          fprintf(stderr, "insert vert for crossing %d: ", i);
          dump_v(cd->vert, "inserted");
        }
#endif
      }
    }

    /* Remove any crossed, non-intersected edges. */
    for (i = 0; i < BLI_array_len(crossings); i++) {
      cd = &crossings[i];
      if (cd->lambda != 0.0 && !is_constrained_edge(cd->in->edge)) {
        delete_edge(cdt, cd->in);
#ifdef DEBUG_CDT
        if (dbg_level > 1) {
          fprintf(stderr, "delete edge for crossing %d\n", i);
        }
#endif
      }
    }

    /* Insert segments for v1->v2. */
    tstart = crossings[0].out;
    for (i = 1; i < BLI_array_len(crossings); i++) {
      cd = &crossings[i];
      t = tnext = NULL;
      if (cd->lambda != 0.0) {
        if (is_constrained_edge(cd->in->edge)) {
          t = cd->vert->symedge;
          tnext = sym(t)->next;
        }
      }
      else if (cd->lambda == 0.0) {
        t = cd->in;
        tnext = cd->out;
      }
      if (t) {
#ifdef DEBUG_CDT
        if (dbg_level > 1) {
          fprintf(stderr, "insert diagonal between\n");
          dump_se(tstart, "  ");
          dump_se(t, " ");
          dump_se_cycle(tstart, "tstart", 100);
          dump_se_cycle(t, "t", 100);
        }
#endif
        if (tstart->next->vert == t->vert) {
          edge = tstart->edge;
#ifdef DEBUG_CDT
          if (dbg_level > 1) {
            fprintf(stderr, "already there\n");
          }
#endif
        }
        else {
          edge = add_diagonal(cdt, tstart, t);
        }
        add_to_input_ids(&edge->input_ids, input_id, cdt);
#ifdef DEBUG_CDT
        if (dbg_level > 1) {
          fprintf(stderr, "added\n");
        }
#endif
        if (r_edges != NULL) {
          BLI_linklist_append_pool(&edge_list, edge, cdt->listpool);
        }
        /* Now retriangulate upper and lower gaps. */
        re_delaunay_triangulate(cdt, &edge->symedges[0]);
        re_delaunay_triangulate(cdt, &edge->symedges[1]);
      }
      if (i < BLI_array_len(crossings) - 1) {
        if (tnext != NULL) {
          tstart = tnext;
#ifdef DEBUG_CDT
          if (dbg_level > 1) {
            fprintf(stderr, "now tstart = ");
            dump_se(tstart, "");
          }
#endif
        }
      }
    }
  }
  if (r_edges) {
    *r_edges = edge_list.list;
  }
  BLI_array_free(crossings);
}

/**
 * Add face_id to the input_ids lists of all #CDTFace's on the interior of the input face with that
 * id. face_symedge is on edge of the boundary of the input face, with assumption that interior is
 * on the left of that SymEdge.
 *
 * The algorithm is: starting from the #CDTFace for face_symedge, add the face_id and then
 * process all adjacent faces where the adjacency isn't across an edge that was a constraint added
 * for the boundary of the input face.
 * fedge_start..fedge_end is the inclusive range of edge input ids that are for the given face.
 *
 * Note: if the input face is not CCW oriented, we'll be labeling the outside, not the inside.
 * Note 2: if the boundary has self-crossings, this method will arbitrarily pick one of the
 * contiguous set of faces enclosed by parts of the boundary, leaving the other such untagged. This
 * may be a feature instead of a bug if the first contiguous section is most of the face and the
 * others are tiny self-crossing triangles at some parts of the boundary. On the other hand, if
 * decide we want to handle these in full generality, then will need a more complicated algorithm
 * (using "inside" tests and a parity rule) to decide on the interior.
 */
static void add_face_ids(
    CDT_state *cdt, SymEdge *face_symedge, int face_id, int fedge_start, int fedge_end)
{
  Stack stack;
  SymEdge *se, *se_start, *se_sym;
  CDTFace *face, *face_other;
  int visit;

  /* Can't loop forever since eventually would visit every face. */
  cdt->visit_count++;
  visit = cdt->visit_count;
  stack = NULL;
  push(&stack, face_symedge, cdt);
  while (!is_empty(&stack)) {
    se = pop(&stack, cdt);
    face = se->face;
    if (face->visit_index == visit) {
      continue;
    }
    face->visit_index = visit;
    add_to_input_ids(&face->input_ids, face_id, cdt);
    se_start = se;
    for (se = se->next; se != se_start; se = se->next) {
      if (!id_range_in_list(se->edge->input_ids, fedge_start, fedge_end)) {
        se_sym = sym(se);
        face_other = se_sym->face;
        if (face_other->visit_index != visit) {
          push(&stack, se_sym, cdt);
        }
      }
    }
  }
}

/* Delete_edge but try not to mess up outer face.
 * Also faces have symedges now, so make sure not
 * to mess those up either. */
static void dissolve_symedge(CDT_state *cdt, SymEdge *se)
{
  SymEdge *symse = sym(se);
  if (symse->face == cdt->outer_face) {
    se = sym(se);
    symse = sym(se);
  }
  if (cdt->outer_face->symedge == se || cdt->outer_face->symedge == symse) {
    /* Advancing by 2 to get past possible 'sym(se)'. */
    if (se->next->next == se) {
      cdt->outer_face->symedge = NULL;
    }
    else {
      cdt->outer_face->symedge = se->next->next;
    }
  }
  else {
    if (se->face->symedge == se) {
      se->face->symedge = se->next;
    }
    if (symse->face->symedge == se) {
      symse->face->symedge = symse->next;
    }
  }
  delete_edge(cdt, se);
}

static void remove_non_constraint_edges(CDT_state *cdt, const bool valid_bmesh)
{
  LinkNode *ln;
  CDTEdge *e;
  SymEdge *se, *se2;
  CDTFace *fleft, *fright;
  bool dissolve;

  for (ln = cdt->edges; ln; ln = ln->next) {
    e = (CDTEdge *)ln->link;
    dissolve = !is_deleted_edge(e) && !is_constrained_edge(e);
    if (dissolve) {
      se = &e->symedges[0];
      if (valid_bmesh) {
        fleft = se->face;
        fright = sym(se)->face;
        if (fleft != cdt->outer_face && fright != cdt->outer_face &&
            (fleft->input_ids != NULL || fright->input_ids != NULL)) {
          /* Is there another symedge with same left and right faces? */
          for (se2 = se->next; dissolve && se2 != se; se2 = se2->next) {
            if (sym(se2)->face == fright) {
              dissolve = false;
            }
          }
        }
      }
      if (dissolve) {
        dissolve_symedge(cdt, se);
      }
    }
  }
}

static void remove_outer_edges(CDT_state *cdt, const bool remove_until_constraints)
{
  LinkNode *fstack = NULL;
  SymEdge *se, *se_start;
  CDTFace *f, *fsym;
  int visit = ++cdt->visit_count;
#ifdef DEBUG_CDT
  int dbg_level = 0;

  if (dbg_level > 0) {
    fprintf(stderr, "remove_outer_edges, until_constraints=%d\n", remove_until_constraints);
  }
#endif

  cdt->outer_face->visit_index = visit;

  /* Find an f, not outer face, but touching outer face. */
  f = NULL;
  se_start = se = cdt->vert_array[0]->symedge;
  do {
    if (se->face != cdt->outer_face) {
      f = se->face;
      break;
    }
    se = se->rot;
  } while (se != se_start);
  BLI_assert(f != NULL && f->symedge != NULL);
  if (f == NULL) {
    return;
  }
  BLI_linklist_prepend_pool(&fstack, f, cdt->listpool);
  while (fstack != NULL) {
    LinkNode *to_dissolve = NULL;
    bool dissolvable;
    f = (CDTFace *)BLI_linklist_pop_pool(&fstack, cdt->listpool);
    if (f->visit_index == visit) {
#ifdef DEBUG_CDT
      if (dbg_level > 0) {
        fprintf(stderr, "skipping f=%p, already visited\n", f);
      }
#endif
      continue;
    }
    BLI_assert(f != cdt->outer_face);
#ifdef DEBUG_CDT
    if (dbg_level > 0) {
      fprintf(stderr, "top of loop, f=%p\n", f);
      dump_se_cycle(f->symedge, "visit", 10000);
      dump_cdt(cdt, "cdt at top of loop");
    }
#endif
    f->visit_index = visit;
    se_start = se = f->symedge;
    do {
      if (remove_until_constraints) {
        dissolvable = !is_constrained_edge(se->edge);
      }
      else {
        dissolvable = edge_touches_frame(se->edge);
      }
#ifdef DEBUG_CDT
      if (dbg_level > 1) {
        dump_se(se, "edge in f");
        fprintf(stderr, "  dissolvable=%d\n", dissolvable);
      }
#endif
      if (dissolvable) {
        fsym = sym(se)->face;
#ifdef DEBUG_CDT
        if (dbg_level > 1) {
          dump_se_cycle(fsym->symedge, "fsym", 10000);
          fprintf(stderr, "  visited=%d\n", fsym->visit_index == visit);
        }
#endif
        if (fsym->visit_index != visit) {
#ifdef DEBUG_CDT
          if (dbg_level > 0) {
            fprintf(stderr, "pushing face %p\n", fsym);
            dump_se_cycle(fsym->symedge, "pushed", 10000);
          }
#endif
          BLI_linklist_prepend_pool(&fstack, fsym, cdt->listpool);
        }
        else {
          BLI_linklist_prepend_pool(&to_dissolve, se, cdt->listpool);
        }
      }
      se = se->next;
    } while (se != se_start);
    while (to_dissolve != NULL) {
      se = (SymEdge *)BLI_linklist_pop_pool(&to_dissolve, cdt->listpool);
      if (se->next != NULL) {
        dissolve_symedge(cdt, se);
      }
    }
  }
}

/**
 * Remove edges and merge faces to get desired output, as per options.
 * \note the cdt cannot be further changed after this.
 */
static void prepare_cdt_for_output(CDT_state *cdt, const CDT_output_type output_type)
{
  CDTFace *f;
  CDTEdge *e;
  LinkNode *ln;

  cdt->output_prepared = true;

  /* Make sure all non-deleted faces have a symedge. */
  for (ln = cdt->edges; ln; ln = ln->next) {
    e = (CDTEdge *)ln->link;
    if (e->symedges[0].face->symedge == NULL) {
      e->symedges[0].face->symedge = &e->symedges[0];
    }
    if (e->symedges[1].face->symedge == NULL) {
      e->symedges[1].face->symedge = &e->symedges[1];
    }
  }
#ifdef DEBUG_CDT
  /* All non-deleted faces should have a symedge now. */
  for (ln = cdt->faces; ln; ln = ln->next) {
    f = (CDTFace *)ln->link;
    if (!f->deleted) {
      BLI_assert(f->symedge != NULL);
    }
  }
#else
  UNUSED_VARS(f);
#endif

  if (output_type == CDT_CONSTRAINTS || output_type == CDT_CONSTRAINTS_VALID_BMESH) {
    remove_non_constraint_edges(cdt, output_type == CDT_CONSTRAINTS_VALID_BMESH);
  }
  else if (output_type == CDT_FULL || output_type == CDT_INSIDE) {
    remove_outer_edges(cdt, output_type == CDT_INSIDE);
  }
}

#define NUM_BOUND_VERTS 4
#define VERT_OUT_INDEX(v) ((v)->index - NUM_BOUND_VERTS)

static CDT_result *cdt_get_output(CDT_state *cdt, const CDT_output_type output_type)
{
  int i, j, nv, ne, nf, faces_len_total;
  int orig_map_size, orig_map_index;
  CDT_result *result;
  LinkNode *lne, *lnf, *ln;
  SymEdge *se, *se_start;
  CDTEdge *e;
  CDTFace *f;

  prepare_cdt_for_output(cdt, output_type);

  result = (CDT_result *)MEM_callocN(sizeof(*result), __func__);

  /* All verts except first NUM_BOUND_VERTS will be output. */
  nv = cdt->vert_array_len - NUM_BOUND_VERTS;
  if (nv <= 0) {
    return result;
  }

  result->verts_len = nv;
  result->vert_coords = MEM_malloc_arrayN(nv, sizeof(result->vert_coords[0]), __func__);

  /* Make the vertex "orig" map arrays, mapping output verts to lists of input ones. */
  orig_map_size = 0;
  for (i = 0; i < nv; i++) {
    orig_map_size += BLI_linklist_count(cdt->vert_array[i + 4]->input_ids);
  }
  result->verts_orig_len_table = MEM_malloc_arrayN(nv, sizeof(int), __func__);
  result->verts_orig_start_table = MEM_malloc_arrayN(nv, sizeof(int), __func__);
  result->verts_orig = MEM_malloc_arrayN(orig_map_size, sizeof(int), __func__);

  orig_map_index = 0;
  for (i = 0; i < nv; i++) {
    j = i + NUM_BOUND_VERTS;
    result->vert_coords[i][0] = (float)cdt->vert_array[j]->co[0];
    result->vert_coords[i][1] = (float)cdt->vert_array[j]->co[1];
    result->verts_orig_start_table[i] = orig_map_index;
    for (ln = cdt->vert_array[j]->input_ids; ln; ln = ln->next) {
      result->verts_orig[orig_map_index++] = POINTER_AS_INT(ln->link);
    }
    result->verts_orig_len_table[i] = orig_map_index - result->verts_orig_start_table[i];
  }

  ne = 0;
  orig_map_size = 0;
  for (ln = cdt->edges; ln; ln = ln->next) {
    e = (CDTEdge *)ln->link;
    if (!is_deleted_edge(e)) {
      ne++;
      if (e->input_ids) {
        orig_map_size += BLI_linklist_count(e->input_ids);
      }
    }
  }
  if (ne != 0) {
    result->edges_len = ne;
    result->face_edge_offset = cdt->face_edge_offset;
    result->edges = MEM_malloc_arrayN(ne, sizeof(result->edges[0]), __func__);
    result->edges_orig_len_table = MEM_malloc_arrayN(ne, sizeof(int), __func__);
    result->edges_orig_start_table = MEM_malloc_arrayN(ne, sizeof(int), __func__);
    if (orig_map_size > 0) {
      result->edges_orig = MEM_malloc_arrayN(orig_map_size, sizeof(int), __func__);
    }
    orig_map_index = 0;
    i = 0;
    for (lne = cdt->edges; lne; lne = lne->next) {
      e = (CDTEdge *)lne->link;
      if (!is_deleted_edge(e)) {
        result->edges[i][0] = VERT_OUT_INDEX(e->symedges[0].vert);
        result->edges[i][1] = VERT_OUT_INDEX(e->symedges[1].vert);
        result->edges_orig_start_table[i] = orig_map_index;
        for (ln = e->input_ids; ln; ln = ln->next) {
          result->edges_orig[orig_map_index++] = POINTER_AS_INT(ln->link);
        }
        result->edges_orig_len_table[i] = orig_map_index - result->edges_orig_start_table[i];
        i++;
      }
    }
  }

  nf = 0;
  faces_len_total = 0;
  orig_map_size = 0;
  for (ln = cdt->faces; ln; ln = ln->next) {
    f = (CDTFace *)ln->link;
    if (!f->deleted && f != cdt->outer_face) {
      nf++;
      se = se_start = f->symedge;
      BLI_assert(se != NULL);
      do {
        faces_len_total++;
        se = se->next;
      } while (se != se_start);
      if (f->input_ids) {
        orig_map_size += BLI_linklist_count(f->input_ids);
      }
    }
  }

  if (nf != 0) {
    result->faces_len = nf;
    result->faces_len_table = MEM_malloc_arrayN(nf, sizeof(int), __func__);
    result->faces_start_table = MEM_malloc_arrayN(nf, sizeof(int), __func__);
    result->faces = MEM_malloc_arrayN(faces_len_total, sizeof(int), __func__);
    result->faces_orig_len_table = MEM_malloc_arrayN(nf, sizeof(int), __func__);
    result->faces_orig_start_table = MEM_malloc_arrayN(nf, sizeof(int), __func__);
    if (orig_map_size > 0) {
      result->faces_orig = MEM_malloc_arrayN(orig_map_size, sizeof(int), __func__);
    }
    orig_map_index = 0;
    i = 0;
    j = 0;
    for (lnf = cdt->faces; lnf; lnf = lnf->next) {
      f = (CDTFace *)lnf->link;
      if (!f->deleted && f != cdt->outer_face) {
        result->faces_start_table[i] = j;
        se = se_start = f->symedge;
        do {
          result->faces[j++] = VERT_OUT_INDEX(se->vert);
          se = se->next;
        } while (se != se_start);
        result->faces_len_table[i] = j - result->faces_start_table[i];
        result->faces_orig_start_table[i] = orig_map_index;
        for (ln = f->input_ids; ln; ln = ln->next) {
          result->faces_orig[orig_map_index++] = POINTER_AS_INT(ln->link);
        }
        result->faces_orig_len_table[i] = orig_map_index - result->faces_orig_start_table[i];
        i++;
      }
    }
  }
  return result;
}

CDT_result *BLI_delaunay_2d_cdt_calc(const CDT_input *input, const CDT_output_type output_type)
{
  int nv = input->verts_len;
  int ne = input->edges_len;
  int nf = input->faces_len;
  double epsilon = (double)input->epsilon;
  int i, f, v1, v2;
  int fedge_start, fedge_end;
  double minx, maxx, miny, maxy;
  float *xy;
  double vert_co[2];
  CDT_state *cdt;
  CDT_result *result;
  CDTVert **verts;
  LinkNode *edge_list;
  CDTEdge *face_edge;
  SymEdge *face_symedge;
#ifdef DEBUG_CDT
  int dbg_level = 1;
#endif

  if ((nv > 0 && input->vert_coords == NULL) || (ne > 0 && input->edges == NULL) ||
      (nf > 0 && (input->faces == NULL || input->faces_start_table == NULL ||
                  input->faces_len_table == NULL))) {
#ifdef DEBUG_CDT
    fprintf(stderr, "invalid input: unexpected NULL array(s)\n");
#endif
    return NULL;
  }

  if (nv > 0) {
    minx = miny = DBL_MAX;
    maxx = maxy = -DBL_MAX;
    for (i = 0; i < nv; i++) {
      xy = input->vert_coords[i];
      if (xy[0] < minx) {
        minx = xy[0];
      }
      if (xy[0] > maxx) {
        maxx = xy[0];
      }
      if (xy[1] < miny) {
        miny = xy[1];
      }
      if (xy[1] > maxy) {
        maxy = xy[1];
      }
    }
    verts = (CDTVert **)MEM_mallocN(nv * sizeof(CDTVert *), "constrained delaunay");
  }
  else {
    minx = miny = maxx = maxy = 0;
    verts = NULL;
  }

  if (epsilon == 0.0) {
    epsilon = 1e-8;
  }
  cdt = cdt_init(minx, maxx, miny, maxy, epsilon);
  /* TODO: use a random permutation for order of adding the vertices. */
  for (i = 0; i < nv; i++) {
    vert_co[0] = (double)input->vert_coords[i][0];
    vert_co[1] = (double)input->vert_coords[i][1];
    verts[i] = add_point_constraint(cdt, vert_co, i);
  }
  for (i = 0; i < ne; i++) {
    v1 = input->edges[i][0];
    v2 = input->edges[i][1];
    if (v1 < 0 || v1 >= nv || v2 < 0 || v2 >= nv) {
#ifdef DEBUG_CDT
      fprintf(stderr, "edge indices not valid: v1=%d, v2=%d, nv=%d\n", v1, v2, nv);
#endif
      continue;
    }
    add_edge_constraint(cdt, verts[v1], verts[v2], i, NULL);
  }
  cdt->face_edge_offset = ne;
  for (f = 0; f < nf; f++) {
    int flen = input->faces_len_table[f];
    int fstart = input->faces_start_table[f];
    if (flen <= 2) {
#ifdef DEBUG_CDT
      fprintf(stderr, "face %d has length %d; ignored\n", f, flen);
#endif
      continue;
    }
    for (i = 0; i < flen; i++) {
      int face_edge_id = cdt->face_edge_offset + fstart + i;
      v1 = input->faces[fstart + i];
      v2 = input->faces[fstart + ((i + 1) % flen)];
      if (v1 < 0 || v1 >= nv || v2 < 0 || v2 >= nv) {
#ifdef DEBUG_CDT
        fprintf(stderr, "face indices not valid: f=%d, v1=%d, v2=%d, nv=%d\n", f, v1, v2, nv);
#endif
        continue;
      }
      add_edge_constraint(cdt, verts[v1], verts[v2], face_edge_id, &edge_list);
#ifdef DEBUG_CDT
      if (dbg_level > 1) {
        fprintf(stderr, "edges for edge %d:\n", i);
        for (LinkNode *ln = edge_list; ln; ln = ln->next) {
          CDTEdge *cdt_e = (CDTEdge *)ln->link;
          fprintf(stderr,
                  "  (%.2f,%.2f)->(%.2f,%.2f)\n",
                  F2(cdt_e->symedges[0].vert->co),
                  F2(cdt_e->symedges[1].vert->co));
        }
      }
#endif
      if (i == 0) {
        face_edge = (CDTEdge *)edge_list->link;
        face_symedge = &face_edge->symedges[0];
        if (face_symedge->vert != verts[v1]) {
          face_symedge = &face_edge->symedges[1];
          BLI_assert(face_symedge->vert == verts[v1]);
        }
      }
      BLI_linklist_free_pool(edge_list, NULL, cdt->listpool);
    }
    fedge_start = cdt->face_edge_offset + fstart;
    fedge_end = fedge_start + flen - 1;
    add_face_ids(cdt, face_symedge, f, fedge_start, fedge_end);
  }
#ifdef DEBUG_CDT
  if (dbg_level > 1) {
    validate_cdt(cdt, true);
  }
  if (dbg_level > 1) {
    cdt_draw(cdt, "before cdt_get_output");
  }
#endif
  result = cdt_get_output(cdt, output_type);
#ifdef DEBUG_CDT
  if (dbg_level > 0) {
    cdt_draw(cdt, "final");
  }
#endif
  if (verts) {
    MEM_freeN(verts);
  }
  cdt_free(cdt);
  return result;
}

void BLI_delaunay_2d_cdt_free(CDT_result *result)
{
  if (result == NULL) {
    return;
  }
  if (result->vert_coords) {
    MEM_freeN(result->vert_coords);
  }
  if (result->edges) {
    MEM_freeN(result->edges);
  }
  if (result->faces) {
    MEM_freeN(result->faces);
  }
  if (result->faces_start_table) {
    MEM_freeN(result->faces_start_table);
  }
  if (result->faces_len_table) {
    MEM_freeN(result->faces_len_table);
  }
  if (result->verts_orig) {
    MEM_freeN(result->verts_orig);
  }
  if (result->verts_orig_start_table) {
    MEM_freeN(result->verts_orig_start_table);
  }
  if (result->verts_orig_len_table) {
    MEM_freeN(result->verts_orig_len_table);
  }
  if (result->edges_orig) {
    MEM_freeN(result->edges_orig);
  }
  if (result->edges_orig_start_table) {
    MEM_freeN(result->edges_orig_start_table);
  }
  if (result->edges_orig_len_table) {
    MEM_freeN(result->edges_orig_len_table);
  }
  if (result->faces_orig) {
    MEM_freeN(result->faces_orig);
  }
  if (result->faces_orig_start_table) {
    MEM_freeN(result->faces_orig_start_table);
  }
  if (result->faces_orig_len_table) {
    MEM_freeN(result->faces_orig_len_table);
  }
  MEM_freeN(result);
}

#ifdef DEBUG_CDT

static void dump_se(const SymEdge *se, const char *lab)
{
  if (se->next) {
    fprintf(
        stderr, "%s((%.2f,%.2f)->(%.2f,%.2f))\n", lab, F2(se->vert->co), F2(se->next->vert->co));
  }
  else {
    fprintf(stderr, "%s((%.2f,%.2f)->NULL)\n", lab, F2(se->vert->co));
  }
}

static void dump_v(const CDTVert *v, const char *lab)
{
  fprintf(stderr, "%s(%.2f,%.2f)\n", lab, F2(v->co));
}

static void dump_se_cycle(const SymEdge *se, const char *lab, const int limit)
{
  int count = 0;
  const SymEdge *s = se;
  fprintf(stderr, "%s:\n", lab);
  do {
    dump_se(s, "  ");
    s = s->next;
    count++;
  } while (s != se && count < limit);
  if (count == limit) {
    fprintf(stderr, "  limit hit without cycle!\n");
  }
}

static void dump_id_list(const LinkNode *id_list, const char *lab)
{
  const LinkNode *ln;
  if (!id_list) {
    return;
  }
  fprintf(stderr, "%s", lab);
  for (ln = id_list; ln; ln = ln->next) {
    fprintf(stderr, "%d%c", POINTER_AS_INT(ln->link), ln->next ? ' ' : '\n');
  }
}

#  define PL(p) (POINTER_AS_UINT(p) & 0xFFFF)
static void dump_cdt(const CDT_state *cdt, const char *lab)
{
  LinkNode *ln;
  CDTVert *v;
  CDTEdge *e;
  CDTFace *f;
  SymEdge *se;
  int i;

  fprintf(stderr, "\nCDT %s\n", lab);
  fprintf(stderr, "\nVERTS\n");
  for (i = 0; i < cdt->vert_array_len; i++) {
    v = cdt->vert_array[i];
    fprintf(stderr, "%x: (%f,%f) symedge=%x\n", PL(v), F2(v->co), PL(v->symedge));
    dump_id_list(v->input_ids, "  ");
  }
  fprintf(stderr, "\nEDGES\n");
  for (ln = cdt->edges; ln; ln = ln->next) {
    e = (CDTEdge *)ln->link;
    if (e->symedges[0].next == NULL) {
      continue;
    }
    fprintf(stderr, "%x:\n", PL(e));
    for (i = 0; i < 2; i++) {
      se = &e->symedges[i];
      fprintf(stderr,
              "  se[%d] @%x: next=%x, rot=%x, vert=%x (%.2f,%.2f), edge=%x, face=%x\n",
              i,
              PL(se),
              PL(se->next),
              PL(se->rot),
              PL(se->vert),
              F2(se->vert->co),
              PL(se->edge),
              PL(se->face));
    }
    dump_id_list(e->input_ids, "  ");
  }
  fprintf(stderr, "\nFACES\n");
  for (ln = cdt->faces; ln; ln = ln->next) {
    f = (CDTFace *)ln->link;
    if (f->deleted) {
      continue;
    }
    if (f == cdt->outer_face) {
      fprintf(stderr, "outer");
    }
    else {
      fprintf(stderr, "%x: centroid (%f,%f)", PL(f), F2(f->centroid));
    }
    fprintf(stderr, " symedge=%x\n", PL(f->symedge));
    dump_id_list(f->input_ids, "  ");
  }
  fprintf(stderr, "\nOTHER\n");
  fprintf(
      stderr, "minx=%f, maxx=%f, miny=%f, maxy=%f\n", cdt->minx, cdt->maxx, cdt->miny, cdt->maxy);
  fprintf(stderr, "margin=%f\n", cdt->margin);
}
#  undef PL

/**
 * Make an html file with svg in it to display the argument cdt.
 * Mouse-overs will reveal the coordinates of vertices and edges.
 * Constraint edges are drawn thicker than non-constraint edges.
 * The first call creates DRAWFILE; subsequent calls append to it.
 */
#  define DRAWFILE "/tmp/debug_draw.html"
#  define MAX_DRAW_WIDTH 1000
#  define MAX_DRAW_HEIGHT 700
static void cdt_draw(CDT_state *cdt, const char *lab)
{
  static bool append = false;
  FILE *f = fopen(DRAWFILE, append ? "a" : "w");
  double draw_margin = (cdt->maxx - cdt->minx + cdt->maxy - cdt->miny + 1) * 0.05;
  double minx = cdt->minx - draw_margin;
  double maxx = cdt->maxx + draw_margin;
  double miny = cdt->miny - draw_margin;
  double maxy = cdt->maxy + draw_margin;
  double width = maxx - minx;
  double height = maxy - miny;
  double aspect = height / width;
  int view_width, view_height;
  double scale;
  LinkNode *ln;
  CDTVert *v, *u;
  CDTEdge *e;
  int i, strokew;

  view_width = MAX_DRAW_WIDTH;
  view_height = (int)(view_width * aspect);
  if (view_height > MAX_DRAW_HEIGHT) {
    view_height = MAX_DRAW_HEIGHT;
    view_width = (int)(view_height / aspect);
  }
  scale = view_width / width;

#  define SX(x) ((x - minx) * scale)
#  define SY(y) ((maxy - y) * scale)

  if (!f) {
    printf("couldn't open file %s\n", DRAWFILE);
    return;
  }
  fprintf(f, "<div>%s</div>\n<div>\n", lab);
  fprintf(f,
          "<svg version=\"1.1\" "
          "xmlns=\"http://www.w3.org/2000/svg\" "
          "xmlns:xlink=\"http://www.w3.org/1999/xlink\" "
          "xml:space=\"preserve\"\n");
  fprintf(f, "width=\"%d\" height=\"%d\">/n", view_width, view_height);

  for (ln = cdt->edges; ln; ln = ln->next) {
    e = (CDTEdge *)ln->link;
    if (is_deleted_edge(e)) {
      continue;
    }
    u = e->symedges[0].vert;
    v = e->symedges[1].vert;
    strokew = is_constrained_edge(e) ? 5 : 2;
    fprintf(f,
            "<line fill=\"none\" stroke=\"black\" stroke-width=\"%d\" "
            "x1=\"%.1f\" y1=\"%.1f\" x2=\"%.1f\" y2=\"%.1f\">\n",
            strokew,
            SX(u->co[0]),
            SY(u->co[1]),
            SX(v->co[0]),
            SY(v->co[1]));
    fprintf(
        f, "  <title>(%.3f,%.3f)(%.3f,%.3f)</title>\n", u->co[0], u->co[1], v->co[0], v->co[1]);
    fprintf(f, "</line>\n");
  }
  i = cdt->output_prepared ? NUM_BOUND_VERTS : 0;
  for (; i < cdt->vert_array_len; i++) {
    v = cdt->vert_array[i];
    fprintf(f,
            "<circle fill=\"black\" cx=\"%.1f\" cy=\"%.1f\" r=\"5\">\n",
            SX(v->co[0]),
            SY(v->co[1]));
    fprintf(f, "  <title>(%.3f,%.3f)</title>\n", v->co[0], v->co[1]);
    fprintf(f, "</circle>\n");
  }

  fprintf(f, "</svg>\n</div>\n");
  fclose(f);
  append = true;
#  undef SX
#  undef SY
}

#  ifndef NDEBUG /* Only used in assert. */
/**
 * Is a visible from b: i.e., ab crosses no edge of cdt?
 * If constrained is true, consider only constrained edges as possible crossers.
 * In any case, don't count an edge ab itself.
 */
static bool is_visible(const CDTVert *a, const CDTVert *b, bool constrained, const CDT_state *cdt)
{
  const LinkNode *ln;
  const CDTEdge *e;
  const SymEdge *se, *senext;
  int ikind;

  for (ln = cdt->edges; ln; ln = ln->next) {
    e = (const CDTEdge *)ln->link;
    if (is_deleted_edge(e) || is_border_edge(e, cdt)) {
      continue;
    }
    if (constrained && !is_constrained_edge(e)) {
      continue;
    }
    se = (const SymEdge *)&e->symedges[0];
    senext = se->next;
    if ((a == se->vert || a == senext->vert) || b == se->vert || b == se->next->vert) {
      continue;
    }
    ikind = isect_seg_seg_v2_lambda_mu_db(
        a->co, b->co, se->vert->co, senext->vert->co, NULL, NULL);
    if (ikind != ISECT_LINE_LINE_NONE) {
      if (ikind == ISECT_LINE_LINE_COLINEAR) {
        /* TODO: special test here for overlap. */
        continue;
      }
      return false;
    }
  }
  return true;
}
#  endif

#  ifndef NDEBUG /* Only used in assert. */
/**
 * Check that edge ab satisfies constrained delaunay condition:
 * That is, for all non-constraint, non-border edges ab,
 * (1) ab is visible in the constraint graph; and
 * (2) there is a circle through a and b such that any vertex v connected by an edge to a or b
 *     is not inside that circle.
 * The argument 'se' specifies ab by: a is se's vert and b is se->next's vert.
 * Return true if check is OK.
 */
static bool is_delaunay_edge(const SymEdge *se, const double epsilon)
{
  int i;
  CDTVert *a, *b, *c;
  const SymEdge *sesym, *curse, *ss;
  bool ok[2];

  if (!is_constrained_edge(se->edge)) {
    return true;
  }
  sesym = sym(se);
  a = se->vert;
  b = se->next->vert;
  /* Try both the triangles adjacent to se's edge for circle. */
  for (i = 0; i < 2; i++) {
    ok[i] = true;
    curse = (i == 0) ? se : sesym;
    a = curse->vert;
    b = curse->next->vert;
    c = curse->next->next->vert;
    for (ss = curse->rot; ss != curse; ss = ss->rot) {
      ok[i] |= delaunay_check(a, b, c, ss->next->vert, epsilon);
    }
  }
  return ok[0] || ok[1];
}
#  endif

#  ifndef NDEBUG
static bool plausible_non_null_ptr(void *p)
{
  return p > (void *)0x1000;
}
#  endif

static void validate_face_centroid(SymEdge *se)
{
  SymEdge *senext;
#  ifndef NDEBUG
  double *centroidp = se->face->centroid;
#  endif
  double c[2];
  int count;
  copy_v2_v2_db(c, se->vert->co);
  BLI_assert(reachable(se->next, se, 100));
  count = 1;
  for (senext = se->next; senext != se; senext = senext->next) {
    add_v2_v2_db(c, senext->vert->co);
    count++;
  }
  c[0] /= count;
  c[1] /= count;
  BLI_assert(fabs(c[0] - centroidp[0]) < 1e-8 && fabs(c[1] - centroidp[1]) < 1e-8);
}

static void validate_cdt(CDT_state *cdt, bool check_all_tris)
{
  LinkNode *ln, *lne;
  int totedges, totfaces, totverts, totborderedges;
  CDTEdge *e;
  SymEdge *se, *sesym, *s;
  CDTVert *v;
  CDTFace *f;
  double *p;
  double margin;
  int i, limit;
  bool isborder;

  if (cdt->output_prepared) {
    return;
  }

  BLI_assert(cdt != NULL);
  BLI_assert(cdt->maxx >= cdt->minx);
  BLI_assert(cdt->maxy >= cdt->miny);
  totedges = 0;
  totborderedges = 0;
  for (ln = cdt->edges; ln; ln = ln->next) {
    e = (CDTEdge *)ln->link;
    se = &e->symedges[0];
    sesym = &e->symedges[1];
    if (is_deleted_edge(e)) {
      BLI_assert(se->rot == NULL && sesym->next == NULL && sesym->rot == NULL);
      continue;
    }
    totedges++;
    isborder = is_border_edge(e, cdt);
    if (isborder) {
      totborderedges++;
      BLI_assert((se->face == cdt->outer_face && sesym->face != cdt->outer_face) ||
                 (se->face != cdt->outer_face && sesym->face == cdt->outer_face));
    }
    /* BLI_assert(se->face != sesym->face);
     * Not required because faces can have intruding wire edges. */
    BLI_assert(se->vert != sesym->vert);
    BLI_assert(se->edge == sesym->edge && se->edge == e);
    BLI_assert(sym(se) == sesym && sym(sesym) == se);
    for (i = 0; i < 2; i++) {
      se = &e->symedges[i];
      v = se->vert;
      f = se->face;
      p = v->co;
      UNUSED_VARS_NDEBUG(p);
      BLI_assert(plausible_non_null_ptr(v));
      if (f != NULL) {
        BLI_assert(plausible_non_null_ptr(f));
      }
      BLI_assert(plausible_non_null_ptr(se->next));
      BLI_assert(plausible_non_null_ptr(se->rot));
      if (check_all_tris && se->face != cdt->outer_face) {
        limit = 3;
      }
      else {
        limit = 10000;
      }
      BLI_assert(reachable(se->next, se, limit));
      UNUSED_VARS_NDEBUG(limit);
      BLI_assert(se->next->next != se);
      s = se;
      do {
        BLI_assert(prev(s)->next == s);
        BLI_assert(s->rot == sym(prev(s)));
        s = s->next;
      } while (s != se);
    }
    BLI_assert(isborder || is_visible(se->vert, se->next->vert, false, cdt));
    BLI_assert(isborder || is_delaunay_edge(se, cdt->epsilon));
  }
  totverts = 0;
  margin = cdt->margin;
  for (i = 0; i < cdt->vert_array_len; i++) {
    totverts++;
    v = cdt->vert_array[i];
    BLI_assert(plausible_non_null_ptr(v));
    p = v->co;
    BLI_assert(p[0] >= cdt->minx - margin && p[0] <= cdt->maxx + margin);
    UNUSED_VARS_NDEBUG(margin);
    BLI_assert(v->symedge->vert == v);
  }
  totfaces = 0;
  for (ln = cdt->faces; ln; ln = ln->next) {
    f = (CDTFace *)ln->link;
    BLI_assert(plausible_non_null_ptr(f));
    if (f->deleted) {
      continue;
    }
    totfaces++;
    if (f == cdt->outer_face) {
      continue;
    }
    for (lne = cdt->edges; lne; lne = lne->next) {
      e = (CDTEdge *)lne->link;
      if (!is_deleted_edge(e)) {
        for (i = 0; i < 2; i++) {
          if (e->symedges[i].face == f) {
            validate_face_centroid(&e->symedges[i]);
          }
        }
      }
    }
    p = f->centroid;
    BLI_assert(p[0] >= cdt->minx - margin && p[0] <= cdt->maxx + margin);
    BLI_assert(p[1] >= cdt->miny - margin && p[1] <= cdt->maxy + margin);
  }
  /* Euler's formula for planar graphs. */
  if (check_all_tris) {
    BLI_assert(totverts - totedges + totfaces == 2);
  }
}
#endif
