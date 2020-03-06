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
 * Constrained 2d Delaunay Triangulation.
 */

#include "MEM_guardedalloc.h"

#include "BLI_array.h"
#include "BLI_bitmap.h"
#include "BLI_linklist.h"
#include "BLI_math.h"
#include "BLI_memarena.h"
#include "BLI_mempool.h"

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
  int merge_to_index;  /* Index of a CDTVert that this has merged to. -1 if no merge. */
  int visit_index;     /* Which visit epoch has this been seen. */
} CDTVert;

typedef struct CDTEdge {
  LinkNode *input_ids; /* List of input edge ids that this is part of. */
  SymEdge symedges[2]; /* The directed edges for this edge. */
  bool in_queue;       /* Used in flipping algorithm. */
} CDTEdge;

typedef struct CDTFace {
  SymEdge *symedge;    /* A symedge in face; only used during output, so only valid then. */
  LinkNode *input_ids; /* List of input face ids that this is part of. */
  int visit_index;     /* Which visit epoch has this been seen. */
  bool deleted;        /* Marks this face no longer used. */
  bool in_queue;       /* Used in remove_small_features algorithm. */
} CDTFace;

typedef struct CDT_state {
  LinkNode *edges;          /* List of CDTEdge pointer. */
  LinkNode *faces;          /* List of CDTFace pointer. */
  CDTFace *outer_face;      /* Which CDTFace is the outer face. */
  CDTVert **vert_array;     /* Array of CDTVert pointer, grows. */
  int vert_array_len;       /* Current length of vert_array. */
  int vert_array_len_alloc; /* Allocated length of vert_array. */
  int input_vert_tot;       /* How many verts were in input (will be first in vert_array). */
  double minx;              /* Used for debug drawing. */
  double miny;              /* Used for debug drawing. */
  double maxx;              /* Used for debug drawing. */
  double maxy;              /* Used for debug drawing. */
  double margin;            /* Used for debug drawing. */
  int visit_count; /* Used for visiting things without having to initialized their visit fields. */
  int face_edge_offset;  /* Input edge id where we start numbering the face edges. */
  MemArena *arena;       /* Most allocations are done from here, so can free all at once at end. */
  BLI_mempool *listpool; /* Allocations of ListNodes done from this pool. */
  double epsilon;        /* The user-specified nearness limit. */
  double epsilon_squared; /* Square of epsilon. */
  bool output_prepared;   /* Set after the mesh has been modified for output (may not be all
                             triangles now). */
} CDT_state;

#define DLNY_ARENASIZE 1 << 14

#ifdef DEBUG_CDT
#  ifdef __GNUC__
#    define ATTU __attribute__((unused))
#  else
#    define ATTU
#  endif
#  define F2(p) p[0], p[1]
#  define F3(p) p[0], p[1], p[2]
struct CrossData;
ATTU static void dump_se(const SymEdge *se, const char *lab);
ATTU static void dump_se_short(const SymEdge *se, const char *lab);
ATTU static void dump_v(const CDTVert *v, const char *lab);
ATTU static void dump_se_cycle(const SymEdge *se, const char *lab, const int limit);
ATTU static void dump_id_list(const LinkNode *id_list, const char *lab);
ATTU static void dump_cross_data(struct CrossData *cd, const char *lab);
ATTU static void dump_cdt(const CDT_state *cdt, const char *lab);
ATTU static void dump_cdt_vert_neighborhood(CDT_state *cdt, int v, int maxdist, const char *lab);
ATTU static void cdt_draw(CDT_state *cdt, const char *lab);
ATTU static void cdt_draw_region(
    CDT_state *cdt, const char *lab, double minx, double miny, double maxx, double maxy);

ATTU static void cdt_draw_vertex_region(CDT_state *cdt, int v, double dist, const char *lab);
ATTU static void cdt_draw_edge_region(
    CDT_state *cdt, int v1, int v2, double dist, const char *lab);
ATTU static void write_cdt_input_to_file(const CDT_input *inp);
ATTU static void validate_cdt(CDT_state *cdt,
                              bool check_all_tris,
                              bool check_delaunay,
                              bool check_visibility);
#endif

static void exactinit(void);
static double orient2d(const double *pa, const double *pb, const double *pc);
static double incircle(const double *pa, const double *pb, const double *pc, const double *pd);

/** Return other #SymEdge for same #CDTEdge as se. */
BLI_INLINE SymEdge *sym(const SymEdge *se)
{
  return se->next->rot;
}

/** Return SymEdge whose next is se. */
BLI_INLINE SymEdge *prev(const SymEdge *se)
{
  return se->rot->next->rot;
}

/**
 * Return true if a -- b -- c are in that order, assuming they are on a straight line according to
 * orient2d and we know the order is either `abc` or `bac`.
 * This means `ab . ac` and `bc . ac` must both be non-negative.  */
static bool in_line(const double a[2], const double b[2], const double c[2])
{
  double ab[2], bc[2], ac[2];
  sub_v2_v2v2_db(ab, b, a);
  sub_v2_v2v2_db(bc, c, b);
  sub_v2_v2v2_db(ac, c, a);
  if (dot_v2v2_db(ab, ac) < 0.0) {
    return false;
  }
  return dot_v2v2_db(bc, ac) >= 0.0;
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
  v->merge_to_index = -1;
  v->visit_index = 0;
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
  e->in_queue = false;
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
  f->in_queue = false;
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

BLI_INLINE bool is_border_edge(const CDTEdge *e, const CDT_state *cdt)
{
  return e->symedges[0].face == cdt->outer_face || e->symedges[1].face == cdt->outer_face;
}

BLI_INLINE bool is_constrained_edge(const CDTEdge *e)
{
  return e->input_ids != NULL;
}

BLI_INLINE bool is_deleted_edge(const CDTEdge *e)
{
  return e->symedges[0].next == NULL;
}

BLI_INLINE bool is_original_vert(const CDTVert *v, CDT_state *cdt)
{
  return (v->index < cdt->input_vert_tot);
}

/** Return the Symedge that goes from v1 to v2, if it exists, else return NULL. */
static SymEdge *find_symedge_between_verts(const CDTVert *v1, const CDTVert *v2)
{
  SymEdge *tstart, *t;

  t = tstart = v1->symedge;
  do {
    if (t->next->vert == v2) {
      return t;
    }
  } while ((t = t->rot) != tstart);
  return NULL;
}

/** Return the SymEdge attached to v that has face f, if it exists, else return NULL. */
static SymEdge *find_symedge_with_face(const CDTVert *v, const CDTFace *f)
{
  SymEdge *tstart, *t;

  t = tstart = v->symedge;
  do {
    if (t->face == f) {
      return t;
    }
  } while ((t = t->rot) != tstart);
  return NULL;
}

/** Is there already an edge between a and b? */
static inline bool exists_edge(const CDTVert *v1, const CDTVert *v2)
{
  return find_symedge_between_verts(v1, v2) != NULL;
}

/** Is the vertex v incident on face f? */
static bool vert_touches_face(const CDTVert *v, const CDTFace *f)
{
  SymEdge *se = v->symedge;
  do {
    if (se->face == f) {
      return true;
    }
  } while ((se = se->rot) != v->symedge);
  return false;
}

/**
 * Assume s1 and s2 are both SymEdges in a face with > 3 sides,
 * and one is not the next of the other.
 * Add an edge from s1->v to s2->v, splitting the face in two.
 * The original face will continue to be associated with the subface
 * that has s1, and a new face will be made for s2's new face.
 * Return the new diagonal's CDTEdge *.
 */
static CDTEdge *add_diagonal(CDT_state *cdt, SymEdge *s1, SymEdge *s2)
{
  CDTEdge *ediag;
  CDTFace *fold, *fnew;
  SymEdge *sdiag, *sdiagsym;
  SymEdge *s1prev, *s1prevsym, *s2prev, *s2prevsym, *se;
  BLI_assert(reachable(s1, s2, 20000));
  BLI_assert(reachable(s2, s1, 20000));
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
  BLI_assert(reachable(s2, sdiag, 2000));
#endif
  for (se = s2; se != sdiag; se = se->next) {
    se->face = fnew;
  }
  add_list_to_input_ids(&fnew->input_ids, fold->input_ids, cdt);
  return ediag;
}

/**
 * Add a dangling edge from an isolated v to the vert at se in the same face as se->face.
 */
static CDTEdge *add_vert_to_symedge_edge(CDT_state *cdt, CDTVert *v, SymEdge *se)
{
  CDTEdge *e;
  SymEdge *se_rot, *se_rotsym, *new_se, *new_se_sym;

  se_rot = se->rot;
  se_rotsym = sym(se_rot);
  e = add_cdtedge(cdt, v, se->vert, se->face, se->face);
  new_se = &e->symedges[0];
  new_se_sym = &e->symedges[1];
  new_se->next = se;
  new_se_sym->next = new_se;
  new_se->rot = new_se;
  new_se_sym->rot = se_rot;
  se->rot = new_se_sym;
  se_rotsym->next = new_se_sym;
  return e;
}

/* Connect the verts of se1 and se2, assuming that currently those two SymEdges are on
 * the outer boundary (have face == outer_face) of two components that are isolated from
 * each other.
 */
static CDTEdge *connect_separate_parts(CDT_state *cdt, SymEdge *se1, SymEdge *se2)
{
  CDTEdge *e;
  SymEdge *se1_rot, *se1_rotsym, *se2_rot, *se2_rotsym, *new_se, *new_se_sym;

  BLI_assert(se1->face == cdt->outer_face && se2->face == cdt->outer_face);
  se1_rot = se1->rot;
  se1_rotsym = sym(se1_rot);
  se2_rot = se2->rot;
  se2_rotsym = sym(se2_rot);
  e = add_cdtedge(cdt, se1->vert, se2->vert, cdt->outer_face, cdt->outer_face);
  new_se = &e->symedges[0];
  new_se_sym = &e->symedges[1];
  new_se->next = se2;
  new_se_sym->next = se1;
  new_se->rot = se1_rot;
  new_se_sym->rot = se2_rot;
  se1->rot = new_se;
  se2->rot = new_se_sym;
  se1_rotsym->next = new_se;
  se2_rotsym->next = new_se_sym;
  return e;
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
  return e;
}

/**
 * Delete an edge from the structure. The new combined face on either side of
 * the deleted edge will be the one that was e's face.
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
}

static CDT_state *cdt_init(const CDT_input *in)
{
  int i;
  MemArena *arena = BLI_memarena_new(DLNY_ARENASIZE, __func__);
  CDT_state *cdt = BLI_memarena_calloc(arena, sizeof(CDT_state));

  cdt->epsilon = (double)in->epsilon;
  cdt->epsilon_squared = cdt->epsilon * cdt->epsilon;
  cdt->arena = arena;
  cdt->input_vert_tot = in->verts_len;
  cdt->vert_array_len_alloc = 2 * in->verts_len;
  cdt->vert_array = BLI_memarena_alloc(arena,
                                       cdt->vert_array_len_alloc * sizeof(*cdt->vert_array));
  cdt->listpool = BLI_mempool_create(
      sizeof(LinkNode), 128 + 4 * in->verts_len, 128 + in->verts_len, 0);

  for (i = 0; i < in->verts_len; i++) {
    add_cdtvert(cdt, (double)(in->vert_coords[i][0]), (double)(in->vert_coords[i][1]));
  }
  cdt->outer_face = add_cdtface(cdt);
  return cdt;
}

static void new_cdt_free(CDT_state *cdt)
{
  BLI_mempool_destroy(cdt->listpool);
  BLI_memarena_free(cdt->arena);
}

typedef struct SiteInfo {
  CDTVert *v;
  int orig_index;
} SiteInfo;

static int site_lexicographic_cmp(const void *a, const void *b)
{
  const SiteInfo *s1 = a;
  const SiteInfo *s2 = b;
  const double *co1 = s1->v->co;
  const double *co2 = s2->v->co;

  if (co1[0] < co2[0]) {
    return -1;
  }
  else if (co1[0] > co2[0]) {
    return 1;
  }
  else if (co1[1] < co2[1]) {
    return -1;
  }
  else if (co1[1] > co2[1]) {
    return 1;
  }
  else if (s1->orig_index < s2->orig_index) {
    return -1;
  }
  else if (s1->orig_index > s2->orig_index) {
    return 1;
  }
  return 0;
}

BLI_INLINE bool vert_left_of_symedge(CDTVert *v, SymEdge *se)
{
  return orient2d(v->co, se->vert->co, se->next->vert->co) > 0.0;
}

BLI_INLINE bool vert_right_of_symedge(CDTVert *v, SymEdge *se)
{
  return orient2d(v->co, se->next->vert->co, se->vert->co) > 0.0;
}

/* Is se above basel? */
BLI_INLINE bool dc_tri_valid(SymEdge *se, SymEdge *basel, SymEdge *basel_sym)
{
  return orient2d(se->next->vert->co, basel_sym->vert->co, basel->vert->co) > 0.0;
}

/* Delaunay triangulate sites[start} to sites[end-1].
 * Assume sites are lexicographically sorted by coordinate.
 * Return SymEdge of ccw convex hull at left-most point in *r_le
 * and that of right-most point of cw convex null in *r_re.
 */
static void dc_tri(
    CDT_state *cdt, SiteInfo *sites, int start, int end, SymEdge **r_le, SymEdge **r_re)
{
  int n = end - start;
  int n2;
  CDTVert *v1, *v2, *v3;
  CDTEdge *ea, *eb, *ebasel;
  SymEdge *ldo, *ldi, *rdi, *rdo, *basel, *basel_sym, *lcand, *rcand, *t;
  double orient;
  bool valid_lcand, valid_rcand;
#ifdef DEBUG_CDT
  char label_buf[100];
  int dbg_level = 0;

  if (dbg_level > 0) {
    fprintf(stderr, "DC_TRI start=%d end=%d\n", start, end);
  }
#endif

  BLI_assert(r_le != NULL && r_re != NULL);
  if (n <= 1) {
    *r_le = NULL;
    *r_re = NULL;
    return;
  }
  if (n <= 3) {
    v1 = sites[start].v;
    v2 = sites[start + 1].v;
    ea = add_cdtedge(cdt, v1, v2, cdt->outer_face, cdt->outer_face);
    ea->symedges[0].next = &ea->symedges[1];
    ea->symedges[1].next = &ea->symedges[0];
    ea->symedges[0].rot = &ea->symedges[0];
    ea->symedges[1].rot = &ea->symedges[1];
    if (n == 2) {
      *r_le = &ea->symedges[0];
      *r_re = &ea->symedges[1];
      return;
    }
    v3 = sites[start + 2].v;
    eb = add_vert_to_symedge_edge(cdt, v3, &ea->symedges[1]);
    orient = orient2d(v1->co, v2->co, v3->co);
    if (orient > 0.0) {
      add_diagonal(cdt, &eb->symedges[0], &ea->symedges[0]);
      *r_le = &ea->symedges[0];
      *r_re = &eb->symedges[0];
    }
    else if (orient < 0.0) {
      add_diagonal(cdt, &ea->symedges[0], &eb->symedges[0]);
      *r_le = ea->symedges[0].rot;
      *r_re = eb->symedges[0].rot;
    }
    else {
      /* Collinear points. Just return a line. */
      *r_le = &ea->symedges[0];
      *r_re = &eb->symedges[0];
    }
    return;
  }
  /* Here: n >= 4.  Divide and conquer. */
  n2 = n / 2;
  BLI_assert(n2 >= 2 && end - (start + n2) >= 2);

  /* Delaunay triangulate two halves, L and R. */
  dc_tri(cdt, sites, start, start + n2, &ldo, &ldi);
  dc_tri(cdt, sites, start + n2, end, &rdi, &rdo);
#ifdef DEBUG_CDT
  if (dbg_level > 0) {
    fprintf(stderr, "\nDC_TRI merge step for start=%d, end=%d\n", start, end);
    dump_se(ldo, "ldo");
    dump_se(ldi, "ldi");
    dump_se(rdi, "rdi");
    dump_se(rdo, "rdo");
    if (dbg_level > 1) {
      sprintf(label_buf, "dc_tri(%d,%d)(%d,%d)", start, start + n2, start + n2, end);
      /* dump_cdt(cdt, label_buf); */
      cdt_draw(cdt, label_buf);
    }
  }
#endif

  /* Find lower common tangent of L and R. */
  for (;;) {
    if (vert_left_of_symedge(rdi->vert, ldi)) {
      ldi = ldi->next;
    }
    else if (vert_right_of_symedge(ldi->vert, rdi)) {
      rdi = sym(rdi)->rot; /* Previous edge to rdi with same right face. */
    }
    else {
      break;
    }
  }
#ifdef DEBUG_CDT
  if (dbg_level > 0) {
    fprintf(stderr, "common lower tangent is between\n");
    dump_se(rdi, "rdi");
    dump_se(ldi, "ldi");
  }
#endif
  ebasel = connect_separate_parts(cdt, sym(rdi)->next, ldi);
  basel = &ebasel->symedges[0];
  basel_sym = &ebasel->symedges[1];
#ifdef DEBUG_CDT
  if (dbg_level > 1) {
    dump_se(basel, "basel");
    cdt_draw(cdt, "after basel made");
  }
#endif
  if (ldi->vert == ldo->vert) {
    ldo = basel_sym;
  }
  if (rdi->vert == rdo->vert) {
    rdo = basel;
  }

  /* Merge loop. */
  for (;;) {
    /* Locate the first point lcand->next->vert encountered by rising bubble,
     * and delete L edges out of basel->next->vert that fail the circle test. */
    lcand = basel_sym->rot;
    rcand = basel_sym->next;
#ifdef DEBUG_CDT
    if (dbg_level > 1) {
      fprintf(stderr, "\ntop of merge loop\n");
      dump_se(lcand, "lcand");
      dump_se(rcand, "rcand");
      dump_se(basel, "basel");
    }
#endif
    if (dc_tri_valid(lcand, basel, basel_sym)) {
#ifdef DEBUG_CDT
      if (dbg_level > 1) {
        fprintf(stderr, "found valid lcand\n");
        dump_se(lcand, "  lcand");
      }
#endif
      while (incircle(basel_sym->vert->co,
                      basel->vert->co,
                      lcand->next->vert->co,
                      lcand->rot->next->vert->co) > 0.0) {
#ifdef DEBUG_CDT
        if (dbg_level > 1) {
          fprintf(stderr, "incircle says to remove lcand\n");
          dump_se(lcand, "  lcand");
        }
#endif
        t = lcand->rot;
        delete_edge(cdt, sym(lcand));
        lcand = t;
      }
    }
    /* Symmetrically, locate first R point to be hit and delete R edges. */
    if (dc_tri_valid(rcand, basel, basel_sym)) {
#ifdef DEBUG_CDT
      if (dbg_level > 1) {
        fprintf(stderr, "found valid rcand\n");
        dump_se(rcand, "  rcand");
      }
#endif
      while (incircle(basel_sym->vert->co,
                      basel->vert->co,
                      rcand->next->vert->co,
                      sym(rcand)->next->next->vert->co) > 0.0) {
#ifdef DEBUG_CDT
        if (dbg_level > 0) {
          fprintf(stderr, "incircle says to remove rcand\n");
          dump_se(lcand, "  rcand");
        }
#endif
        t = sym(rcand)->next;
        delete_edge(cdt, rcand);
        rcand = t;
      }
    }
    /* If both lcand and rcand are invalid, then basel is the common upper tangent. */
    valid_lcand = dc_tri_valid(lcand, basel, basel_sym);
    valid_rcand = dc_tri_valid(rcand, basel, basel_sym);
#ifdef DEBUG_CDT
    if (dbg_level > 0) {
      fprintf(
          stderr, "after bubbling up, valid_lcand=%d, valid_rcand=%d\n", valid_lcand, valid_rcand);
      dump_se(lcand, "lcand");
      dump_se(rcand, "rcand");
    }
#endif
    if (!valid_lcand && !valid_rcand) {
      break;
    }
    /* The next cross edge to be connected is to either lcand->next->vert or rcand->next->vert;
     * if both are valid, choose the appropriate one using the incircle test.
     */
    if (!valid_lcand ||
        (valid_rcand &&
         incircle(lcand->next->vert->co, lcand->vert->co, rcand->vert->co, rcand->next->vert->co) >
             0.0)) {
#ifdef DEBUG_CDT
      if (dbg_level > 0) {
        fprintf(stderr, "connecting rcand\n");
        dump_se(basel_sym, "  se1=basel_sym");
        dump_se(rcand->next, "  se2=rcand->next");
      }
#endif
      ebasel = add_diagonal(cdt, rcand->next, basel_sym);
    }
    else {
#ifdef DEBUG_CDT
      if (dbg_level > 0) {
        fprintf(stderr, "connecting lcand\n");
        dump_se(sym(lcand), "  se1=sym(lcand)");
        dump_se(basel_sym->next, "  se2=basel_sym->next");
      }
#endif
      ebasel = add_diagonal(cdt, basel_sym->next, sym(lcand));
    }
    basel = &ebasel->symedges[0];
    basel_sym = &ebasel->symedges[1];
    BLI_assert(basel_sym->face == cdt->outer_face);
#ifdef DEBUG_CDT
    if (dbg_level > 2) {
      cdt_draw(cdt, "after adding new crossedge");
      // dump_cdt(cdt, "after adding new crossedge");
    }
#endif
  }
  *r_le = ldo;
  *r_re = rdo;
  BLI_assert(sym(ldo)->face == cdt->outer_face && rdo->face == cdt->outer_face);
}

/* Guibas-Stolfi Divide-and_Conquer algorithm. */
static void dc_triangulate(CDT_state *cdt, SiteInfo *sites, int nsites)
{
  int i, j, n;
  SymEdge *le, *re;

  /* Compress sites in place to eliminated verts that merge to others. */
  i = 0;
  j = 0;
  while (j < nsites) {
    /* Invariante: sites[0..i-1] have non-merged verts from 0..(j-1) in them. */
    sites[i] = sites[j++];
    if (sites[i].v->merge_to_index < 0) {
      i++;
    }
  }
  n = i;
  if (n == 0) {
    return;
  }
  dc_tri(cdt, sites, 0, n, &le, &re);
}

/**
 * Do a Delaunay Triangulation of the points in cdt->vert_array.
 * This  is only a first step in the Constrained Delaunay triangulation,
 * because it doesn't yet deal with the segment constraints.
 * The algorithm used is the Divide & Conquer algorithm from the
 * Guibas-Stolfi "Primitives for the Manipulation of General Subdivision
 * and the Computation of Voronoi Diagrams" paper.
 * The data structure here is similar to but not exactly the same as
 * the quad-edge structure described in that paper.
 * The incircle and ccw tests are done using Shewchuk's exact
 * primitives (see below), so that this routine is robust.
 *
 * As a preprocessing step, we want to merge all vertices that are
 * within cdt->epsilon of each other. This is accomplished by lexicographically
 * sorting the coordinates first (which is needed anyway for the D&C algorithm).
 * The CDTVerts with merge_to_index not equal to -1 are after this regarded
 * as having been merged into the vertex with the corresponding index.
 */
static void initial_triangulation(CDT_state *cdt)
{
  int i, j, n;
  SiteInfo *sites;
  double *ico, *jco;
  double xend, yend, xcur;
  double epsilon = cdt->epsilon;
  double epsilon_squared = cdt->epsilon_squared;
#ifdef SJF_WAY
  CDTEdge *e;
  CDTVert *va, *vb;
#endif
#ifdef DEBUG_CDT
  int dbg_level = 0;

  if (dbg_level > 0) {
    fprintf(stderr, "\nINITIAL TRIANGULATION\n\n");
  }
#endif

  /* First sort the vertices by lexicographic order of their
   * coordinates, breaking ties by putting earlier original-index
   * vertices first.
   */
  n = cdt->vert_array_len;
  if (n <= 1) {
    return;
  }
  sites = MEM_malloc_arrayN(n, sizeof(SiteInfo), __func__);
  for (i = 0; i < n; i++) {
    sites[i].v = cdt->vert_array[i];
    sites[i].orig_index = i;
  }
  qsort(sites, n, sizeof(SiteInfo), site_lexicographic_cmp);
#ifdef DEBUG_CDT
  if (dbg_level > 0) {
    fprintf(stderr, "after sorting\n");
    for (i = 0; i < n; i++) {
      fprintf(stderr, "%d: orig index: %d, (%f,%f)\n", i, sites[i].orig_index, F2(sites[i].v->co));
    }
  }
#endif

  /* Now dedup according to user-defined epsilon.
   * We will merge a vertex into an earlier-indexed vertex
   * that is within epsilon (Euclidean distance).
   * Merges may cascade. So we may end up merging two things
   * that are farther than epsilon by transitive merging. Oh well.
   * Assume that merges are rare, so use simple searches in the
   * lexicographic ordering - likely we will soon hit y's with
   * the same x that are farther away than epsilon, and then
   * skipping ahead to the next biggest x, are likely to soon
   * find one of those farther away than epsilon.
   */
  for (i = 0; i < n - 1; i++) {
    ico = sites[i].v->co;
    /* Start j at next place that has both x and y coords within epsilon. */
    xend = ico[0] + epsilon;
    yend = ico[1] + epsilon;
    j = i + 1;
    while (j < n) {
      jco = sites[j].v->co;
      if (jco[0] > xend) {
        break; /* No more j's to process. */
      }
      else if (jco[1] > yend) {
        /* Get past any string of v's with the same x and too-big y. */
        xcur = jco[0];
        while (++j < n) {
          if (sites[j].v->co[0] > xcur) {
            break;
          }
        }
        BLI_assert(j == n || sites[j].v->co[0] > xcur);
        if (j == n) {
          break;
        }
        jco = sites[j].v->co;
        if (jco[0] > xend || jco[1] > yend) {
          break;
        }
      }
      /* When here, vertex i and j are within epsilon by box test.
       * The Euclidean distance test is stricter, so need to do it too, now.
       */
      BLI_assert(j < n && jco[0] <= xend && jco[1] <= yend);
      if (len_squared_v2v2_db(ico, jco) <= epsilon_squared) {
        sites[j].v->merge_to_index = (sites[i].v->merge_to_index == -1) ?
                                         sites[i].orig_index :
                                         sites[i].v->merge_to_index;
#ifdef DEBUG_CDT
        if (dbg_level > 0) {
          fprintf(stderr,
                  "merged orig vert %d to %d\n",
                  sites[j].orig_index,
                  sites[j].v->merge_to_index);
        }
#endif
      }
      j++;
    }
  }

  /* Now add non-dup vertices into triangulation in lexicographic order. */

  dc_triangulate(cdt, sites, n);
  MEM_freeN(sites);
}

/** Use LinkNode linked list as stack of SymEdges, allocating from cdt->listpool. */
typedef LinkNode *Stack;

BLI_INLINE void push(Stack *stack, SymEdge *se, CDT_state *cdt)
{
  BLI_linklist_prepend_pool(stack, se, cdt->listpool);
}

BLI_INLINE SymEdge *pop(Stack *stack, CDT_state *cdt)
{
  return (SymEdge *)BLI_linklist_pop_pool(stack, cdt->listpool);
}

BLI_INLINE bool is_empty(Stack *stack)
{
  return *stack == NULL;
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
  int count;
#ifdef DEBUG_CDT
  SymEdge *last;
  const int dbg_level = 0;
#endif

  if (se->face == cdt->outer_face || sym(se)->face == cdt->outer_face) {
    return;
  }
#ifdef DEBUG_CDT
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
    if (incircle(a->co, b->co, c->co, v->co) > 0.0) {
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

static double tri_orient(const SymEdge *t)
{
  return orient2d(t->vert->co, t->next->vert->co, t->next->next->vert->co);
}

/**
 * The CrossData struct gives defines either an endpoint or an intermediate point
 * in the path we will take to insert an edge constraint.
 * Each such point will either be
 * (a) a vertex or
 * (b) a fraction lambda (0 < lambda < 1) along some SymEdge.]
 *
 * In general, lambda=0 indicates case a and lambda != 0 indicates case be.
 * The 'in' edge gives the destination attachment point of a diagonal from the previous crossing,
 * and the 'out' edge gives the origin attachment point of a diagonal to the next crossing.
 * But in some cases, 'in' and 'out' are undefined or not needed, and will be NULL.
 *
 * For case (a), 'vert' will be the vertex, and lambda will be 0, and 'in' will be the SymEdge from
 * 'vert' that has as face the one that you go through to get to this vertex. If you go exactly
 * along an edge then we set 'in' to NULL, since it won't be needed. The first crossing will have
 * 'in' = NULL. We set 'out' to the SymEdge that has the face we go though to get to the next
 * crossing, or, if the next crossing is a case (a), then it is the edge that goes to that next
 * vertex. 'out' wlll be NULL for the last one.
 *
 * For case (b), vert will be NULL at first, and later filled in with the created split vertex,
 * and 'in' will be the SymEdge that we go through, and lambda will be between 0 and 1,
 * the fraction from in's vert to in->next's vert to put the split vertex.
 * 'out' is not needed in this case, since the attachment point will be the sym of the first
 * half of the split edge.
 */
typedef struct CrossData {
  double lambda;
  CDTVert *vert;
  SymEdge *in;
  SymEdge *out;
} CrossData;

static bool get_next_crossing_from_vert(CDT_state *cdt,
                                        CrossData *cd,
                                        CrossData *cd_next,
                                        const CDTVert *v2);

/**
 * As part of finding crossings, we found a case where the next crossing goes through vert v.
 * If it came from a previous vert in cd, then cd_out is the edge that leads from that to v.
 * Else cd_out can be NULL, because it won't be used.
 * Set *cd_next to indicate this. We can set 'in' but not 'out'.  We can set the 'out' of the
 * current cd.
 */
static void fill_crossdata_for_through_vert(CDTVert *v,
                                            SymEdge *cd_out,
                                            CrossData *cd,
                                            CrossData *cd_next)
{
  SymEdge *se;
#ifdef DEBUG_CDT
  int dbg_level = 0;
#endif

  cd_next->lambda = 0.0;
  cd_next->vert = v;
  cd_next->in = NULL;
  cd_next->out = NULL;
  if (cd->lambda == 0.0) {
    cd->out = cd_out;
  }
  else {
    /* One of the edges in the triangle with edge sym(cd->in) contains v. */
    se = sym(cd->in);
    if (se->vert != v) {
      se = se->next;
      if (se->vert != v) {
        se = se->next;
      }
    }
    BLI_assert(se->vert == v);
    cd_next->in = se;
  }
#ifdef DEBUG_CDT
  if (dbg_level > 0) {
    dump_cross_data(cd, "cd through vert, cd");
    dump_cross_data(cd_next, "cd_next through vert, cd");
  }
#endif
}

/**
 * As part of finding crossings, we found a case where orient tests say that the next crossing
 * is on the SymEdge t, while intersecting with the ray from curco to v2.
 * Find the intersection point and fill in the CrossData for that point.
 * It may turn out that when doing the intersection, we get an answer that says that
 * this case is better handled as through-vertex case instead, so we may do that.
 * In the latter case, we want to avoid a situation where the current crossing is on an edge
 * and the next will be an endpoint of the same edge. When that happens, we "rewrite history"
 * and turn the current crossing into a vert one, and then extend from there.
 *
 * We cannot fill cd_next's 'out' edge yet, in the case that the next one ends up being a vert
 * case. We need to fill in cd's 'out' edge if it was a vert case.
 */
static void fill_crossdata_for_intersect(CDT_state *cdt,
                                         const double *curco,
                                         const CDTVert *v2,
                                         SymEdge *t,
                                         CrossData *cd,
                                         CrossData *cd_next)
{
  CDTVert *va, *vb, *vc;
  double lambda, mu, len_ab;
  SymEdge *se_vcva, *se_vcvb;
  int isect;
#ifdef DEBUG_CDT
  int dbg_level = 0;
#endif

  va = t->vert;
  vb = t->next->vert;
  vc = t->next->next->vert;
  se_vcvb = sym(t->next);
  se_vcva = t->next->next;
  BLI_assert(se_vcva->vert == vc && se_vcva->next->vert == va);
  BLI_assert(se_vcvb->vert == vc && se_vcvb->next->vert == vb);
  isect = isect_seg_seg_v2_lambda_mu_db(va->co, vb->co, curco, v2->co, &lambda, &mu);
#ifdef DEBUG_CDT
  if (dbg_level > 0) {
    double co[2];
    fprintf(stderr, "crossdata for intersect gets lambda=%.17g, mu=%.17g\n", lambda, mu);
    fprintf(stderr,
            "isect=%s\n",
            isect == 2 ? "cross" : (isect == 1 ? "exact" : (isect == 0 ? "none" : "colinear")));
    fprintf(stderr,
            "va=v%d=(%g,%g), vb=v%d=(%g,%g), vc=v%d, curco=(%g,%g), v2=(%g,%g)\n",
            va->index,
            F2(va->co),
            vb->index,
            F2(vb->co),
            vc->index,
            F2(curco),
            F2(v2->co));
    dump_se_short(se_vcva, "vcva=");
    dump_se_short(se_vcvb, " vcvb=");
    interp_v2_v2v2_db(co, va->co, vb->co, lambda);
    fprintf(stderr, "\nco=(%.17g,%.17g)\n", F2(co));
  }
#endif
  switch (isect) {
    case ISECT_LINE_LINE_CROSS:
      len_ab = len_v2v2_db(va->co, vb->co);
#ifdef DEBUG_CDT
      if (dbg_level > 0) {
        fprintf(stderr,
                "len_ab=%g, near a=%g, near b=%g\n",
                len_ab,
                lambda * len_ab,
                (1.0 - lambda) * len_ab);
      }
#endif
      if (lambda * len_ab <= cdt->epsilon) {
        fill_crossdata_for_through_vert(va, se_vcva, cd, cd_next);
      }
      else if ((1.0 - lambda) * len_ab <= cdt->epsilon) {
        fill_crossdata_for_through_vert(vb, se_vcvb, cd, cd_next);
      }
      else {
        *cd_next = (CrossData){lambda, NULL, t, NULL};
        if (cd->lambda == 0.0) {
          cd->out = se_vcva;
        }
      }
      break;
    case ISECT_LINE_LINE_EXACT:
      if (lambda == 0.0) {
        fill_crossdata_for_through_vert(va, se_vcva, cd, cd_next);
      }
      else if (lambda == 1.0) {
        fill_crossdata_for_through_vert(vb, se_vcvb, cd, cd_next);
      }
      else {
        *cd_next = (CrossData){lambda, NULL, t, NULL};
        if (cd->lambda == 0.0) {
          cd->out = se_vcva;
        }
      }
      break;
    case ISECT_LINE_LINE_NONE:
      /* It should be very near one end or other of segment. */
      if (lambda <= 0.5) {
        fill_crossdata_for_through_vert(va, se_vcva, cd, cd_next);
      }
      else {
        fill_crossdata_for_through_vert(vb, se_vcvb, cd, cd_next);
      }
      break;
    case ISECT_LINE_LINE_COLINEAR:
      if (len_squared_v2v2_db(va->co, v2->co) <= len_squared_v2v2_db(vb->co, v2->co)) {
        fill_crossdata_for_through_vert(va, se_vcva, cd, cd_next);
      }
      else {
        fill_crossdata_for_through_vert(vb, se_vcvb, cd, cd_next);
      }
      break;
  }
}

/**
 * As part of finding the crossings of a ray to v2, find the next crossing after 'cd', assuming
 * 'cd' represents a crossing that goes through a vertex.
 *
 * We do a rotational scan around cd's vertex, looking for the triangle where the ray from cd->vert
 * to v2 goes between the two arms from cd->vert, or where it goes along one of the edges.
 */
static bool get_next_crossing_from_vert(CDT_state *cdt,
                                        CrossData *cd,
                                        CrossData *cd_next,
                                        const CDTVert *v2)
{
  SymEdge *t, *tstart;
  CDTVert *vcur, *va, *vb;
  double orient1, orient2;
  bool ok;
#ifdef DEBUG_CDT
  int dbg_level = 0;

  if (dbg_level > 0) {
    fprintf(stderr, "\nget_next_crossing_from_vert\n");
    dump_v(cd->vert, "  cd->vert");
  }
#endif

  t = tstart = cd->vert->symedge;
  vcur = cd->vert;
  ok = false;
  do {
    /*
     * The ray from vcur to v2 has to go either between two successive
     * edges around vcur or exactly along them. This time through the
     * loop, check to see if the ray goes along vcur-va
     * or between vcur-va and vcur-vb, where va is the end of t
     * and vb is the next vertex (on the next rot edge around vcur, but
     * should also be the next vert of triangle starting with vcur-va.
     */
    if (t->face != cdt->outer_face && tri_orient(t) < 0.0) {
      fprintf(stderr, "BAD TRI orientation %g\n", tri_orient(t)); /* TODO: handle this */
#ifdef DEBUG_CDT
      dump_se_cycle(t, "top of ccw scan loop", 4);
#endif
    }
    va = t->next->vert;
    vb = t->next->next->vert;
    orient1 = orient2d(t->vert->co, va->co, v2->co);
#ifdef DEBUG_CDT
    if (dbg_level > 1) {
      fprintf(stderr, "non-final through vert case\n");
      dump_v(va, " va");
      dump_v(vb, " vb");
      fprintf(stderr, "orient1=%g\n", orient1);
    }
#endif
    if (orient1 == 0.0 && in_line(vcur->co, va->co, v2->co)) {
#ifdef DEBUG_CDT
      if (dbg_level > 1) {
        fprintf(stderr, "ray goes through va\n");
      }
#endif
      fill_crossdata_for_through_vert(va, t, cd, cd_next);
      ok = true;
      break;
    }
    else if (t->face != cdt->outer_face) {
      orient2 = orient2d(vcur->co, vb->co, v2->co);
#ifdef DEBUG_CDT
      if (dbg_level > 1) {
        fprintf(stderr, "orient2=%g\n", orient2);
      }
#endif
      /* Don't handle orient2 == 0.0 case here: next rotation will get it. */
      if (orient1 > 0.0 && orient2 < 0.0) {
#ifdef DEBUG_CDT
        if (dbg_level > 1) {
          fprintf(stderr, "segment intersects\n");
        }
#endif
        t = t->next;
        fill_crossdata_for_intersect(cdt, vcur->co, v2, t, cd, cd_next);
        ok = true;
        break;
      }
    }
  } while ((t = t->rot) != tstart);
#ifdef DEBUG_CDT
  if (!ok) {
    /* Didn't find the exit! Shouldn't happen. */
    fprintf(stderr, "shouldn't happen: can't find next crossing from vert\n");
  }
#endif
  return ok;
}

/**
 * As part of finding the crossings of a ray to v2, find the next crossing after 'cd', assuming
 * 'cd' represents a crossing that goes through a an edge, not at either end of that edge.
 *
 * We have the triangle vb-va-vc, where va and vb are the split edge and vc is the third vertex on
 * that new side of the edge (should be closer to v2). The next crossing should be through vc or
 * intersecting vb-vc or va-vc.
 */
static void get_next_crossing_from_edge(CDT_state *cdt,
                                        CrossData *cd,
                                        CrossData *cd_next,
                                        const CDTVert *v2)
{
  double curco[2];
  double orient;
  CDTVert *va, *vb, *vc;
  SymEdge *se_ac;
#ifdef DEBUG_CDT
  int dbg_level = 0;

  if (dbg_level > 0) {
    fprintf(stderr, "\nget_next_crossing_from_edge\n");
    fprintf(stderr, "  lambda=%.17g\n", cd->lambda);
    dump_se_short(cd->in, "  cd->in");
  }
#endif

  va = cd->in->vert;
  vb = cd->in->next->vert;
  interp_v2_v2v2_db(curco, va->co, vb->co, cd->lambda);
#ifdef DEBUG_CDT
  if (dbg_level > 0) {
    fprintf(stderr, "  curco=(%.17g,%.17g)\n", F2(curco));
  }
#endif
  se_ac = sym(cd->in)->next;
  vc = se_ac->next->vert;
  orient = orient2d(curco, v2->co, vc->co);
#ifdef DEBUG_CDT
  if (dbg_level > 1) {
    fprintf(stderr, "now searching with third vertex ");
    dump_v(vc, "vc");
    fprintf(stderr, "orient2d(cur, v2, vc) = %g\n", orient);
  }
#endif
  if (orient < 0.0) {
#ifdef DEBUG_CDT
    if (dbg_level > 1) {
      fprintf(stderr, "curco--v2 intersects vb--vc\n");
    }
#endif
    fill_crossdata_for_intersect(cdt, curco, v2, se_ac->next, cd, cd_next);
  }
  else if (orient > 0.0) {
#ifdef DEBUG_CDT
    if (dbg_level > 1) {
      fprintf(stderr, "curco--v2 intersects va--vc\n");
    }
#endif
    fill_crossdata_for_intersect(cdt, curco, v2, se_ac, cd, cd_next);
  }
  else {
#ifdef DEBUG_CDT
    if (dbg_level > 1) {
      fprintf(stderr, "orient==0 case, so going through or to vc\n");
    }
#endif
    *cd_next = (CrossData){0.0, vc, se_ac->next, NULL};
  }
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
  SymEdge *t, *se, *tstart, *tnext;
  int i, j, n, visit;
  bool ok;
  CrossData *crossings = NULL;
  CrossData *cd, *cd_prev, *cd_next;
  CDTVert *v;
  CDTEdge *edge;
  LinkNodePair edge_list = {NULL, NULL};
  BLI_array_staticdeclare(crossings, 128);
#ifdef DEBUG_CDT
  int dbg_level = 0;

  if (dbg_level > 0) {
    fprintf(stderr, "\nADD_EDGE_CONSTRAINT\n");
    dump_v(v1, "  1");
    dump_v(v2, "  2");
  }
#endif

  if (r_edges) {
    *r_edges = NULL;
  }

  /*
   * Handle two special cases first:
   * 1) The two end vertices are the same (can happen because of merging).
   * 2) There is already an edge between v1 and v2.
   */
  if (v1 == v2) {
    return;
  }
  if ((t = find_symedge_between_verts(v1, v2)) != NULL) {
#ifdef DEBUG_CDT
    if (dbg_level > 0) {
      fprintf(stderr, "segment already there\n");
    }
#endif
    add_to_input_ids(&t->edge->input_ids, input_id, cdt);
    if (r_edges != NULL) {
      BLI_linklist_append_pool(&edge_list, t->edge, cdt->listpool);
      *r_edges = edge_list.list;
    }
    return;
  }

  /*
   * Fill crossings array with CrossData points for intersection path from v1 to v2.
   *
   * At every point, the crossings array has the path so far, except that
   * the .out field of the last element of it may not be known yet -- if that
   * last element is a vertex, then we won't know the output edge until we
   * find the next crossing.
   *
   * To protect against infinite loops, we keep track of which vertices
   * we have visited by setting their visit_index to a new visit epoch.
   *
   * We check a special case first: where the segment is already there in
   * one hop. Saves a bunch of orient2d tests in that common case.
   */
  visit = ++cdt->visit_count;
  BLI_array_grow_one(crossings);
  crossings[0] = (CrossData){0.0, v1, NULL, NULL};
  while (!((n = BLI_array_len(crossings)) > 0 && crossings[n - 1].vert == v2)) {
    BLI_array_grow_one(crossings);
    cd = &crossings[n - 1];
    cd_next = &crossings[n];
    if (crossings[n - 1].lambda == 0.0) {
      ok = get_next_crossing_from_vert(cdt, cd, cd_next, v2);
    }
    else {
      get_next_crossing_from_edge(cdt, cd, cd_next, v2);
    }
    if (!ok || BLI_array_len(crossings) == 100000) {
      /* Shouldn't happen but if does, just bail out. */
#ifdef DEBUG_CDT
      fprintf(stderr, "FAILURE adding segment, bailing out\n");
#endif
      BLI_array_free(crossings);
      return;
    }
#ifdef DEBUG_CDT
    if (dbg_level > 1) {
      fprintf(stderr, "crossings[%d]: ", n);
      dump_cross_data(&crossings[n], "");
    }
#endif
    if (crossings[n].lambda == 0.0) {
      if (crossings[n].vert->visit_index == visit) {
        fprintf(stderr, "WHOOPS, REVISIT. Bailing out.\n"); /*TODO: desperation search here. */
        BLI_array_free(crossings);
        return;
      }
      crossings[n].vert->visit_index = visit;
    }
  }

#ifdef DEBUG_CDT
  if (dbg_level > 0) {
    fprintf(stderr, "\ncrossings found\n");
    for (i = 0; i < BLI_array_len(crossings); i++) {
      fprintf(stderr, "%d: ", i);
      dump_cross_data(&crossings[i], "");
      if (crossings[i].lambda == 0.0) {
        if (i == 0 || crossings[i - 1].lambda == 0.0) {
          BLI_assert(crossings[i].in == NULL);
        }
        else {
          BLI_assert(crossings[i].in != NULL && crossings[i].in->vert == crossings[i].vert);
          BLI_assert(crossings[i].in->face == sym(crossings[i - 1].in)->face);
        }
        if (i == BLI_array_len(crossings) - 1) {
          BLI_assert(crossings[i].vert == v2);
          BLI_assert(crossings[i].out == NULL);
        }
        else {
          BLI_assert(crossings[i].out->vert == crossings[i].vert);
          if (crossings[i + 1].lambda == 0.0) {
            BLI_assert(crossings[i].out->next->vert == crossings[i + 1].vert);
          }
          else {
            BLI_assert(crossings[i].out->face == crossings[i + 1].in->face);
          }
        }
      }
      else {
        if (i > 0 && crossings[i - 1].lambda == 0.0) {
          BLI_assert(crossings[i].in->face == crossings[i - 1].out->face);
        }
        BLI_assert(crossings[i].out == NULL);
      }
    }
  }
#endif

  /*
   * Postprocess crossings.
   * Some crossings may have an intersection crossing followed
   * by a vertex crossing that is on the same edge that was just
   * intersected. We prefer to go directly from the previous
   * crossing directly to the vertex. This may chain backwards.
   *
   * This loop marks certain crossings as "deleted", by setting
   * their lambdas to -1.0.
   */
  for (i = 2; i < BLI_array_len(crossings); i++) {
    cd = &crossings[i];
    if (cd->lambda == 0.0) {
      v = cd->vert;
      for (j = i - 1; j > 0; j--) {
        cd_prev = &crossings[j];
        if ((cd_prev->lambda == 0.0 && cd_prev->vert != v) ||
            (cd_prev->lambda != 0.0 && cd_prev->in->vert != v && cd_prev->in->next->vert != v)) {
          break;
        }
        else {
          cd_prev->lambda = -1.0; /* Mark cd_prev as 'deleted'. */
#ifdef DEBUG_CDT
          if (dbg_level > 0) {
            fprintf(stderr, "deleted crossing %d\n", j);
          }
#endif
        }
      }
      if (j < i - 1) {
        /* Some crossings were deleted. Fix the in and out edges across gap. */
        cd_prev = &crossings[j];
        if (cd_prev->lambda == 0.0) {
          se = find_symedge_between_verts(cd_prev->vert, v);
          if (se == NULL) {
#ifdef DEBUG_CDT
            fprintf(stderr, "FAILURE(a) in delete crossings, bailing out.\n");
#endif
            BLI_array_free(crossings);
            return;
          }
          cd_prev->out = se;
          cd->in = NULL;
        }
        else {
          se = find_symedge_with_face(v, sym(cd_prev->in)->face);
          if (se == NULL) {
#ifdef DEBUG_CDT
            fprintf(stderr, "FAILURE(b) in delete crossings, bailing out.\n");
#endif
            BLI_array_free(crossings);
            return;
          }
          cd->in = se;
        }
#ifdef DEBUG_CDT
        if (dbg_level > 0) {
          fprintf(stderr, "after deleting crossings:\n");
          fprintf(stderr, "cross[%d]: ", j);
          dump_cross_data(cd_prev, "");
          fprintf(stderr, "cross[%d]: ", i);
          dump_cross_data(cd, "");
        }
#endif
      }
    }
  }

  /*
   * Insert all intersection points on constrained edges.
   */
  for (i = 0; i < BLI_array_len(crossings); i++) {
    cd = &crossings[i];
    if (cd->lambda != 0.0 && cd->lambda != -1.0 && is_constrained_edge(cd->in->edge)) {
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

  /*
   * Remove any crossed, non-intersected edges.
   */
  for (i = 0; i < BLI_array_len(crossings); i++) {
    cd = &crossings[i];
    if (cd->lambda != 0.0 && cd->lambda != -1.0 && !is_constrained_edge(cd->in->edge)) {
      delete_edge(cdt, cd->in);
#ifdef DEBUG_CDT
      if (dbg_level > 1) {
        fprintf(stderr, "delete edge for crossing %d\n", i);
      }
#endif
    }
  }

  /*
   * Insert segments for v1->v2.
   */
  tstart = crossings[0].out;
  for (i = 1; i < BLI_array_len(crossings); i++) {
    cd = &crossings[i];
    if (cd->lambda == -1.0) {
      continue; /* This crossing was deleted. */
    }
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
      if (t == NULL) {
        /* Previous non-deleted crossing must also have been a vert, and segment should exist. */
        for (j = i - 1; j >= 0; j--) {
          cd_prev = &crossings[j];
          if (cd_prev->lambda != -1.0) {
            break;
          }
        }
        BLI_assert(cd_prev->lambda == 0.0);
        BLI_assert(cd_prev->out->next->vert == cd->vert);
#ifdef DEBUG_CDT
        if (dbg_level > 1) {
          fprintf(stderr, "edge to crossing %d already there\n", i);
        }
#endif
        edge = cd_prev->out->edge;
        add_to_input_ids(&edge->input_ids, input_id, cdt);
      }
    }
    if (t != NULL) {
#ifdef DEBUG_CDT
      if (dbg_level > 1) {
        fprintf(stderr, "edge to crossing %d: insert diagonal between\n", i);
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
          fprintf(stderr, "already there (b)\n");
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
    if (symse->face->symedge == symse) {
      symse->face->symedge = symse->next;
    }
  }
  delete_edge(cdt, se);
}

/* Slow way to get face and start vertex index within face for edge id e. */
static bool get_face_edge_id_indices(const CDT_input *in, int e, int *r_f, int *r_fi)
{
  int f;
  int id;

  id = in->edges_len;
  if (e < id) {
    return false;
  }
  for (f = 0; f < in->faces_len; f++) {
    if (e < id + in->faces_len_table[f]) {
      *r_f = f;
      *r_fi = e - id;
      return true;
    }
    id += in->faces_len_table[f];
  }
  return false;
}

/* Is pt_co when snapped to segment seg1 seg2 all of:
 * a) strictly within that segment
 * b) within epsilon from original pt_co
 * c) pt_co is not within epsilon of either seg1 or seg2.
 * Return true if so, and return in *r_lambda the fraction of the way from seg1 to seg2 of the
 * snapped point.
 */
static bool check_vert_near_segment(const double *pt_co,
                                    const double *seg1,
                                    const double *seg2,
                                    double epsilon_squared,
                                    double *r_lambda)
{
  double lambda, snap_co[2];

  lambda = closest_to_line_v2_db(snap_co, pt_co, seg1, seg2);
  *r_lambda = lambda;
  if (lambda <= 0.0 || lambda >= 1.0) {
    return false;
  }
  if (len_squared_v2v2_db(pt_co, snap_co) > epsilon_squared) {
    return false;
  }
  if (len_squared_v2v2_db(pt_co, seg1) <= epsilon_squared ||
      len_squared_v2v2_db(pt_co, seg2) <= epsilon_squared) {
    return false;
  }
  return true;
}

typedef struct EdgeVertLambda {
  int e_id;
  int v_id;
  double lambda;
} EdgeVertLambda;

/* For sorting first by edge id, then by lambda, then by vert id. */
static int evl_cmp(const void *a, const void *b)
{
  const EdgeVertLambda *sa = a;
  const EdgeVertLambda *sb = b;

  if (sa->e_id < sb->e_id) {
    return -1;
  }
  else if (sa->e_id > sb->e_id) {
    return 1;
  }
  else if (sa->lambda < sb->lambda) {
    return -1;
  }
  else if (sa->lambda > sb->lambda) {
    return 1;
  }
  else if (sa->v_id < sb->v_id) {
    return -1;
  }
  else if (sa->v_id > sb->v_id) {
    return 1;
  }
  return 0;
}

/**
 * If epsilon > 0, and input doesn't have skip_modify_input == true,
 * check input to see if any constraint edge ends (including face edges) come
 * within epsilon of another edge.
 * For all such cases, we want to split the constraint edge at the point nearest to near vertex
 * and move the vertex coordinates to be on that edge.
 * But exclude cases where they come within epsilon of either end because those will be handled
 * by vertex merging in the main triangulation algorithm.
 *
 * If any such splits are found, make a new CDT_input reflecting this change, and provide an
 * edge map to map from edge ids in the new input space to edge ids in the old input space.
 *
 * TODO: replace naive O(n^2) algorithm with kdopbvh-based one.
 */
static const CDT_input *modify_input_for_near_edge_ends(const CDT_input *input, int **r_edge_map)
{
  CDT_input *new_input = NULL;
  int e, eprev, e1, e2, f, fi, flen, start, i, j;
  int i_new, i_old, i_evl;
  int v11, v12, v21, v22;
  double co11[2], co12[2], co21[2], co22[2];
  double lambda;
  double eps = (double)input->epsilon;
  double eps_sq = eps * eps;
  int tot_edge_constraints, edges_len, tot_face_edges;
  int new_tot_face_edges, new_tot_con_edges;
  int delta_con_edges, delta_face_edges, cur_e_cnt;
  int *edge_map;
  int evl_len;
  EdgeVertLambda *edge_vert_lambda = NULL;
  BLI_array_staticdeclare(edge_vert_lambda, 128);
#ifdef DEBUG_CDT
  EdgeVertLambda *evl;
  int dbg_level = 0;

  if (dbg_level > 0) {
    fprintf(stderr, "\nMODIFY INPUT\n\n");
  }
#endif

  *r_edge_map = NULL;
  if (input->epsilon == 0.0 || input->skip_input_modify ||
      (input->edges_len == 0 && input->faces_len == 0)) {
    return input;
  }

  /* Edge constraints are union of the explicitly provided edges and the implicit edges
   * that are part of the provided faces. We index constraints by have the first input->edges_len
   * ints standing for the explicit edge with the same index, and the rest being face edges in
   * the order that the faces appear and then edges within those faces, with indices offset by
   * input->edges_len.
   * Calculate tot_edge_constraints to be the sum of the two kinds of edges.
   * We first have to count the number of face edges.
   * That is the same as the number of vertices in the faces table, which
   * we can find by adding the last length to the last start.
   */
  edges_len = input->edges_len;
  tot_edge_constraints = edges_len;
  if (input->faces_len > 0) {
    tot_face_edges = input->faces_start_table[input->faces_len - 1] +
                     input->faces_len_table[input->faces_len - 1];
  }
  else {
    tot_face_edges = 0;
  }
  tot_edge_constraints = edges_len + tot_face_edges;

  for (e1 = 0; e1 < tot_edge_constraints - 1; e1++) {
    if (e1 < edges_len) {
      v11 = input->edges[e1][0];
      v12 = input->edges[e1][1];
    }
    else {
      if (!get_face_edge_id_indices(input, e1, &f, &fi)) {
        /* Must be bad input. Will be caught later so don't need to signal here. */
        continue;
      }
      start = input->faces_start_table[f];
      flen = input->faces_len_table[f];
      v11 = input->faces[start + fi];
      v12 = input->faces[(fi == flen - 1) ? start : start + fi + 1];
    }
    for (e2 = e1 + 1; e2 < tot_edge_constraints; e2++) {
      if (e2 < edges_len) {
        v21 = input->edges[e2][0];
        v22 = input->edges[e2][1];
      }
      else {
        if (!get_face_edge_id_indices(input, e2, &f, &fi)) {
          continue;
        }
        start = input->faces_start_table[f];
        flen = input->faces_len_table[f];
        v21 = input->faces[start + fi];
        v22 = input->faces[(fi == flen - 1) ? start : start + fi + 1];
      }
      copy_v2db_v2fl(co11, input->vert_coords[v11]);
      copy_v2db_v2fl(co12, input->vert_coords[v12]);
      copy_v2db_v2fl(co21, input->vert_coords[v21]);
      copy_v2db_v2fl(co22, input->vert_coords[v22]);
      if (check_vert_near_segment(co11, co21, co22, eps_sq, &lambda)) {

        BLI_array_append(edge_vert_lambda, ((EdgeVertLambda){e2, v11, lambda}));
      }
      if (check_vert_near_segment(co12, co21, co22, eps_sq, &lambda)) {
        BLI_array_append(edge_vert_lambda, ((EdgeVertLambda){e2, v12, lambda}));
      }
      if (check_vert_near_segment(co21, co11, co12, eps_sq, &lambda)) {
        BLI_array_append(edge_vert_lambda, ((EdgeVertLambda){e1, v21, lambda}));
      }
      if (check_vert_near_segment(co22, co11, co12, eps_sq, &lambda)) {
        BLI_array_append(edge_vert_lambda, ((EdgeVertLambda){e1, v22, lambda}));
      }
    }
  }

  evl_len = BLI_array_len(edge_vert_lambda);
  if (evl_len > 0) {
    /* Sort to bring splits for each edge together,
     * and for each edge, to be in order of lambda. */
    qsort(edge_vert_lambda, evl_len, sizeof(EdgeVertLambda), evl_cmp);
#ifdef DEBUG_CDT
    if (dbg_level > 0) {
      fprintf(stderr, "\nafter sorting\n");
      for (i = 0; i < evl_len; i++) {
        evl = &edge_vert_lambda[i];
        fprintf(stderr, "e%d, v%d, %g\n", evl->e_id, evl->v_id, evl->lambda);
      }
    }
#endif

    /* Remove dups in edge_vert_lambda, where dup means that the edge is the
     * same, and the verts are either the same or will be merged by epsilon-nearness.
     */
    i = 0;
    j = 0;
    /* In loop, copy from position j to position i. */
    for (j = 0; j < evl_len;) {
      int k;
      if (i != j) {
        memmove(&edge_vert_lambda[i], &edge_vert_lambda[j], sizeof(EdgeVertLambda));
      }
      for (k = j + 1; k < evl_len; k++) {
        int vj = edge_vert_lambda[j].v_id;
        int vk = edge_vert_lambda[k].v_id;
        if (vj != vk) {
          if (len_squared_v2v2(input->vert_coords[vj], input->vert_coords[vk]) > (float)eps_sq) {
            break;
          }
        }
      }
      j = k;
      i++;
    }

    if (i != evl_len) {
      evl_len = i;
#ifdef DEBUG_CDT
      if (dbg_level > 0) {
        fprintf(stderr, "\nduplicates eliminated\n");
        for (i = 0; i < evl_len; i++) {
          evl = &edge_vert_lambda[i];
          fprintf(stderr, "e%d, v%d, %g\n", evl->e_id, evl->v_id, evl->lambda);
        }
      }
#endif
    }
    /* Find delta in number of constraint edges and face edges.
     * This may be overestimates of true number, due to duplicates. */
    delta_con_edges = 0;
    delta_face_edges = 0;
    cur_e_cnt = 0;
    eprev = -1;
    for (i = 0; i < evl_len; i++) {
      e = edge_vert_lambda[i].e_id;
      if (i > 0 && e > eprev) {
        /* New edge group. Previous group had cur_e_cnt split vertices.
         * That is the delta in the number of edges needed in input since
         * there will be cur_e_cnt + 1 edges replacing one edge.
         */
        if (eprev < edges_len) {
          delta_con_edges += cur_e_cnt;
        }
        else {
          delta_face_edges += cur_e_cnt;
        }
        cur_e_cnt = 1;
        ;
      }
      else {
        cur_e_cnt++;
      }
      eprev = e;
    }
    if (eprev < edges_len) {
      delta_con_edges += cur_e_cnt;
    }
    else {
      delta_face_edges += cur_e_cnt;
    }
    new_tot_con_edges = input->edges_len + delta_con_edges;
    if (input->faces_len > 0) {
      new_tot_face_edges = input->faces_start_table[input->faces_len - 1] +
                           input->faces_len_table[input->faces_len - 1] + delta_face_edges;
    }
    else {
      new_tot_face_edges = 0;
    }

    /* Allocate new CDT_input, now we know sizes needed (perhaps overestimated a bit).
     * Caller will be responsible for freeing it and its arrays.
     */
    new_input = MEM_callocN(sizeof(CDT_input), __func__);
    new_input->epsilon = input->epsilon;
    new_input->verts_len = input->verts_len;
    new_input->vert_coords = (float(*)[2])MEM_malloc_arrayN(
        new_input->verts_len, 2 * sizeof(float), __func__);
    /* We don't do it now, but may decide to change coords of snapped verts. */
    memmove(new_input->vert_coords,
            input->vert_coords,
            (size_t)new_input->verts_len * sizeof(float) * 2);

    if (edges_len > 0) {
      new_input->edges_len = new_tot_con_edges;
      new_input->edges = (int(*)[2])MEM_malloc_arrayN(
          new_tot_con_edges, 2 * sizeof(int), __func__);
    }

    if (input->faces_len > 0) {
      new_input->faces_len = input->faces_len;
      new_input->faces_start_table = (int *)MEM_malloc_arrayN(
          new_input->faces_len, sizeof(int), __func__);
      new_input->faces_len_table = (int *)MEM_malloc_arrayN(
          new_input->faces_len, sizeof(int), __func__);
      new_input->faces = (int *)MEM_malloc_arrayN(new_tot_face_edges, sizeof(int), __func__);
    }

    edge_map = (int *)MEM_malloc_arrayN(
        new_tot_con_edges + new_tot_face_edges, sizeof(int), __func__);
    *r_edge_map = edge_map;

    i_new = i_old = i_evl = 0;
    e = edge_vert_lambda[0].e_id;
    /* First do new constraint edges. */
    for (i_old = 0; i_old < edges_len; i_old++) {
      if (i_old < e) {
        /* Edge for i_old not split; copy it into new_input. */
        new_input->edges[i_new][0] = input->edges[i_old][0];
        new_input->edges[i_new][1] = input->edges[i_old][1];
        edge_map[i_new] = i_old;
        i_new++;
      }
      else {
        /* Edge for i_old is split. */
        BLI_assert(i_old == e);
        new_input->edges[i_new][0] = input->edges[i_old][0];
        new_input->edges[i_new][1] = edge_vert_lambda[i_evl].v_id;
        edge_map[i_new] = i_old;
        i_new++;
        i_evl++;
        while (i_evl < evl_len && e == edge_vert_lambda[i_evl].e_id) {
          new_input->edges[i_new][0] = new_input->edges[i_new - 1][1];
          new_input->edges[i_new][1] = edge_vert_lambda[i_evl].v_id;
          edge_map[i_new] = i_old;
          i_new++;
          i_evl++;
        }
        new_input->edges[i_new][0] = new_input->edges[i_new - 1][1];
        new_input->edges[i_new][1] = input->edges[i_old][1];
        edge_map[i_new] = i_old;
        i_new++;
        if (i_evl < evl_len) {
          e = edge_vert_lambda[i_evl].e_id;
        }
        else {
          e = INT_MAX;
        }
      }
    }
    BLI_assert(i_new <= new_tot_con_edges);
    new_input->edges_len = i_new;

    /* Now do face constraints. */
    if (input->faces_len > 0) {
      f = 0;
      i_new = 0; /* Now will index cur place in new_input->faces. */
      while (i_old < tot_edge_constraints) {
        flen = input->faces_len_table[f];
        BLI_assert(i_old - edges_len == input->faces_start_table[f]);
        new_input->faces_start_table[f] = i_new;
        if (i_old + flen - 1 < e) {
          /* Face f is not split. */
          for (j = 0; j < flen; j++) {
            new_input->faces[i_new] = input->faces[i_old - edges_len + j];
            edge_map[i_new + new_input->edges_len] = i_old + j;
            i_new++;
          }
          i_old += flen;
          new_input->faces_len_table[f] = flen;
          f++;
        }
        else {
          /* Face f has at least one split edge. */
          int i_new_start = i_new;
          for (j = 0; j < flen; j++) {
            if (i_old + j < e) {
              /* jth edge of f is not split. */
              new_input->faces[i_new] = input->faces[i_old - edges_len + j];
              edge_map[i_new + new_input->edges_len] = i_old + j;
              i_new++;
            }
            else {
              /* jth edge of f is split. */
              BLI_assert(i_old + j == e);
              new_input->faces[i_new] = input->faces[i_old - edges_len + j];
              edge_map[i_new + new_input->edges_len] = i_old + j;
              i_new++;
              while (i_evl < evl_len && e == edge_vert_lambda[i_evl].e_id) {
                new_input->faces[i_new] = edge_vert_lambda[i_evl].v_id;
                edge_map[i_new + new_input->edges_len] = i_old + j;
                i_new++;
                i_evl++;
              }
              if (i_evl < evl_len) {
                e = edge_vert_lambda[i_evl].e_id;
              }
              else {
                e = INT_MAX;
              }
            }
          }
          new_input->faces_len_table[f] = i_new - i_new_start;
          i_old += flen;
          f++;
        }
      }
    }

#ifdef DEBUG_CDT
    if (dbg_level > 0) {
      fprintf(stderr, "\nnew constraint edges\n");
      for (i = 0; i < new_input->edges_len; i++) {
        fprintf(stderr, "  e%d: (%d,%d)\n", i, new_input->edges[i][0], new_input->edges[i][1]);
      }
      fprintf(stderr, "\nnew faces\n");
      for (f = 0; f < new_input->faces_len; f++) {
        flen = new_input->faces_len_table[f];
        start = new_input->faces_start_table[f];
        fprintf(stderr, "  f%d: start=%d, len=%d\n    ", f, start, flen);
        for (i = start; i < start + flen; i++) {
          fprintf(stderr, "%d ", new_input->faces[i]);
        }
        fprintf(stderr, "\n");
      }
      fprintf(stderr, "\nedge map (new->old)\n");
      for (i = 0; i < new_tot_con_edges + new_tot_face_edges; i++) {
        fprintf(stderr, "  %d->%d\n", i, edge_map[i]);
      }
    }
#endif
  }

  BLI_array_free(edge_vert_lambda);
  if (new_input != NULL) {
    return (const CDT_input *)new_input;
  }
  else {
    return input;
  }
}

static void free_modified_input(CDT_input *input)
{
  MEM_freeN(input->vert_coords);
  if (input->edges != NULL) {
    MEM_freeN(input->edges);
  }
  if (input->faces != NULL) {
    MEM_freeN(input->faces);
    MEM_freeN(input->faces_len_table);
    MEM_freeN(input->faces_start_table);
  }
  MEM_freeN(input);
}

/* Return true if we can merge se's vert into se->next's vert
 * without making the area of any new triangle formed by doing
 * that into a zero or negative area triangle.*/
static bool can_collapse(const SymEdge *se)
{
  SymEdge *loop_se;
  const double *co = se->next->vert->co;

  for (loop_se = se->rot; loop_se != se && loop_se->rot != se; loop_se = loop_se->rot) {
    if (orient2d(co, loop_se->next->vert->co, loop_se->rot->next->vert->co) <= 0.0) {
      return false;
    }
  }
  return true;
}

/*
 * Merge one end of e onto the other, fixing up surrounding faces.
 *
 * General situation looks something like:
 *
 *             c-----e
 *           /  \   / \
 *          /    \ /   \
 *         a------b-----f
 *          \    / \   /
 *           \  /   \ /
 *             d-----g
 *
 * where ab is the tiny edge. We want to merge a and b and delete edge ab.
 * We don't want to change the coordinates of input vertices [We could revisit this
 * in the future, as API def doesn't prohibit this, but callers will appreciate if they
 * don't change.]
 * Sometimes the collapse shouldn't happen because the triangles formed by the changed
 * edges may end up with zero or negative area (see can_collapse, above).
 * So don't choose a collapse direction that is not allowed or one that has an original vertex
 * as origin and a non-original vertex as destination.
 * If both collapse directions are allowed by that rule, pick the one with the lower original
 * index.
 *
 * After merging, the faces abc and adb disappear (if they are not the outer face).
 * Suppose we merge b onto a.
 * Then edges cb and db are deleted. Face cbe becomes cae and face bdg becomes adg.
 * Any other faces attached to b now have a in their place.
 * We can do this by rotating edges round b, replacing their vert references with a.
 * Similar statements can be made about what happens when a merges into b;
 * in code below we'll swap a and b to make above lettering work for a b->a merge.
 * Return the vert at the collapsed edge, if a collapse happens.
 */
static CDTVert *collapse_tiny_edge(CDT_state *cdt, CDTEdge *e)
{
  CDTVert *va, *vb;
  SymEdge *ab_se, *ba_se, *bd_se, *bc_se, *ad_se, *ac_se;
  SymEdge *bg_se, *be_se, *se, *gb_se, *ca_se;
  bool can_collapse_a_to_b, can_collapse_b_to_a;
#ifdef DEBUG_CDT
  int dbg_level = 0;
#endif

  ab_se = &e->symedges[0];
  ba_se = &e->symedges[1];
  va = ab_se->vert;
  vb = ba_se->vert;
#ifdef DEBUG_CDT
  if (dbg_level > 0) {
    fprintf(stderr, "\ncollapse_tiny_edge\n");
    dump_se(&e->symedges[0], "tiny edge");
    fprintf(stderr, "a = [%d], b = [%d]\n", va->index, vb->index);
    validate_cdt(cdt, true, false, true);
  }
#endif
  can_collapse_a_to_b = can_collapse(ab_se);
  can_collapse_b_to_a = can_collapse(ba_se);
  /* Now swap a and b if necessary and possible, so that from this point on we are collapsing b to
   * a. */
  if (va->index > vb->index || !can_collapse_b_to_a) {
    if (can_collapse_a_to_b && !(is_original_vert(va, cdt) && !is_original_vert(vb, cdt))) {
      SWAP(CDTVert *, va, vb);
      ab_se = &e->symedges[1];
      ba_se = &e->symedges[0];
#ifdef DEBUG_CDT
      if (dbg_level > 0) {
        fprintf(stderr, "swapped a and b\n");
      }
#endif
    }
    else {
      /* Neither collapse direction is OK. */
#ifdef DEBUG_CDT
      if (dbg_level > 0) {
        fprintf(stderr, "neither collapse direction ok\n");
      }
#endif
      return NULL;
    }
  }
  bc_se = ab_se->next;
  bd_se = ba_se->rot;
  if (bd_se == ba_se) {
    /* Shouldn't happen. Wire edge in outer face. */
    fprintf(stderr, "unexpected wire edge\n");
    return NULL;
  }
  vb->merge_to_index = va->merge_to_index == -1 ? va->index : va->merge_to_index;
  vb->symedge = NULL;
#ifdef DEBUG_CDT
  if (dbg_level > 0) {
    fprintf(stderr,
            "vb = v[%d] merges to va = v[%d], vb->merge_to_index=%d\n",
            vb->index,
            va->index,
            vb->merge_to_index);
  }
#endif
  /* First fix the vertex of intermediate triangles, like bgf. */
  for (se = bd_se->rot; se != bc_se; se = se->rot) {
#ifdef DEBUG_CDT
    if (dbg_level > 0) {
      dump_se(se, "intermediate tri edge, setting vert to va");
    }
#endif
    se->vert = va;
  }
  ad_se = sym(sym(bd_se)->rot);
  ca_se = bc_se->next;
  ac_se = sym(ca_se);
  if (bd_se->rot != bc_se) {
    bg_se = bd_se->rot;
    be_se = sym(bc_se)->next;
    gb_se = sym(bg_se);
  }
  else {
    bg_se = NULL;
    be_se = NULL;
  }
#ifdef DEBUG_CDT
  if (dbg_level > 0) {
    fprintf(stderr, "delete bd, inputs to ad\n");
    dump_se(bd_se, "  bd");
    dump_se(ad_se, "  ad");
    fprintf(stderr, "delete bc, inputs to ac\n");
    dump_se(bc_se, "  bc");
    dump_se(ac_se, "  ac");
    fprintf(stderr, "delete ab\n");
    dump_se(ab_se, "  ab");
    if (bg_se != NULL) {
      fprintf(stderr, "fix up bg, be\n");
      dump_se(bg_se, "  bg");
      dump_se(be_se, "  be");
    }
  }
#endif
  add_list_to_input_ids(&ad_se->edge->input_ids, bd_se->edge->input_ids, cdt);
  delete_edge(cdt, bd_se);
  add_list_to_input_ids(&ac_se->edge->input_ids, bc_se->edge->input_ids, cdt);
  delete_edge(cdt, sym(bc_se));
  /* At this point we have this:
   *
   *             c-----e
   *           /      / \
   *          /      /   \
   *         a------b-----f
   *          \      \   /
   *           \      \ /
   *             d-----g
   *
   * Or, if there is not bg_se and be_se, like this:
   *
   *             c
   *           /
   *          /
   *         a------b
   *          \
   *           \
   *             d
   *
   * (But we've already changed the vert field for bg, bf, ..., be to be va.)
   */
  if (bg_se != NULL) {
    gb_se->next = ad_se;
    ad_se->rot = bg_se;
    ca_se->next = be_se;
    be_se->rot = ac_se;
    bg_se->vert = va;
    be_se->vert = va;
  }
  else {
    ca_se->next = ad_se;
    ad_se->rot = ac_se;
  }
  /* Don't use delete_edge as it changes too much. */
  ab_se->next = ab_se->rot = NULL;
  ba_se->next = ba_se->rot = NULL;
  if (va->symedge == ab_se) {
    va->symedge = ac_se;
  }
  return va;
}

/*
 * Check to see if e is tiny (length <= epsilon) and queue it if so.
 */
static void maybe_enqueue_small_feature(CDT_state *cdt, CDTEdge *e, LinkNodePair *tiny_edge_queue)
{
  SymEdge *se, *sesym;
#ifdef DEBUG_CDT
  int dbg_level = 0;

  if (dbg_level > 0) {
    fprintf(stderr, "\nmaybe_enqueue_small_features\n");
    dump_se(&e->symedges[0], "  se0");
  }
#endif

  if (is_deleted_edge(e) || e->in_queue) {
#ifdef DEBUG_CDT
    if (dbg_level > 0) {
      fprintf(stderr, "returning because of e conditions\n");
    }
#endif
    return;
  }
  se = &e->symedges[0];
  sesym = &e->symedges[1];
  if (len_squared_v2v2_db(se->vert->co, sesym->vert->co) <= cdt->epsilon_squared) {
    BLI_linklist_append_pool(tiny_edge_queue, e, cdt->listpool);
    e->in_queue = true;
#ifdef DEBUG_CDT
    if (dbg_level > 0) {
      fprintf(stderr, "Queue tiny edge\n");
    }
#endif
  }
}

/* Consider all edges in rot ring around v for possible enqueing as small features .*/
static void maybe_enqueue_small_features(CDT_state *cdt, CDTVert *v, LinkNodePair *tiny_edge_queue)
{
  SymEdge *se, *se_start;

  se = se_start = v->symedge;
  if (!se_start) {
    return;
  }
  do {
    maybe_enqueue_small_feature(cdt, se->edge, tiny_edge_queue);
  } while ((se = se->rot) != se_start);
}

/* Collapse small edges (length <= epsilon) until no more exist.
 */
static void remove_small_features(CDT_state *cdt)
{
  double epsilon = cdt->epsilon;
  LinkNodePair tiny_edge_queue = {NULL, NULL};
  BLI_mempool *pool = cdt->listpool;
  LinkNode *ln;
  CDTEdge *e;
  CDTVert *v_change;
#ifdef DEBUG_CDT
  int dbg_level = 0;

  if (dbg_level > 0) {
    fprintf(stderr, "\nREMOVE_SMALL_FEATURES, epsilon=%g\n", epsilon);
  }
#endif

  if (epsilon == 0.0) {
    return;
  }

  for (ln = cdt->edges; ln; ln = ln->next) {
    e = (CDTEdge *)ln->link;
    maybe_enqueue_small_feature(cdt, e, &tiny_edge_queue);
  }

  while (tiny_edge_queue.list != NULL) {
    e = (CDTEdge *)BLI_linklist_pop_pool(&tiny_edge_queue.list, pool);
    if (tiny_edge_queue.list == NULL) {
      tiny_edge_queue.last_node = NULL;
    }
    e->in_queue = false;
    if (is_deleted_edge(e)) {
      continue;
    }
#ifdef DEBUG_CDT
    if (dbg_level > 0) {
      fprintf(stderr, "collapse tiny edge\n");
      dump_se(&e->symedges[0], "");
    }
#endif
    v_change = collapse_tiny_edge(cdt, e);
    if (v_change) {
      maybe_enqueue_small_features(cdt, v_change, &tiny_edge_queue);
    }
  }
}

/* Remove all non-constraint edges. */
static void remove_non_constraint_edges(CDT_state *cdt)
{
  LinkNode *ln;
  CDTEdge *e;
  SymEdge *se;

  for (ln = cdt->edges; ln; ln = ln->next) {
    e = (CDTEdge *)ln->link;
    se = &e->symedges[0];
    if (!is_deleted_edge(e) && !is_constrained_edge(e)) {
      dissolve_symedge(cdt, se);
    }
  }
}

/*
 * Remove the non-constraint edges, but leave enough of them so that all of the
 * faces that would be bmesh faces (that is, the faces that have some input representative)
 * are valid: they can't have holes, they can't have repeated vertices, and they can't have
 * repeated edges.
 *
 * Not essential, but to make the result look more aesthetically nice,
 * remove the edges in order of decreasing length, so that it is more likely that the
 * final remaining support edges are short, and therefore likely to make a fairly
 * direct path from an outer face to an inner hole face.
 */

/* For sorting edges by decreasing length (squared). */
struct EdgeToSort {
  double len_squared;
  CDTEdge *e;
};

static int edge_to_sort_cmp(const void *a, const void *b)
{
  const struct EdgeToSort *e1 = a;
  const struct EdgeToSort *e2 = b;

  if (e1->len_squared > e2->len_squared) {
    return -1;
  }
  else if (e1->len_squared < e2->len_squared) {
    return 1;
  }
  return 0;
}

static void remove_non_constraint_edges_leave_valid_bmesh(CDT_state *cdt)
{
  LinkNode *ln;
  CDTEdge *e;
  SymEdge *se, *se2;
  CDTFace *fleft, *fright;
  bool dissolve;
  size_t nedges;
  int i, ndissolvable;
  const double *co1, *co2;
  struct EdgeToSort *sorted_edges;

  nedges = 0;
  for (ln = cdt->edges; ln; ln = ln->next) {
    nedges++;
  }
  if (nedges == 0) {
    return;
  }
  sorted_edges = BLI_memarena_alloc(cdt->arena, nedges * sizeof(*sorted_edges));
  i = 0;
  for (ln = cdt->edges; ln; ln = ln->next) {
    e = (CDTEdge *)ln->link;
    if (!is_deleted_edge(e) && !is_constrained_edge(e)) {
      sorted_edges[i].e = e;
      co1 = e->symedges[0].vert->co;
      co2 = e->symedges[1].vert->co;
      sorted_edges[i].len_squared = len_squared_v2v2_db(co1, co2);
      i++;
    }
  }
  ndissolvable = i;
  qsort(sorted_edges, ndissolvable, sizeof(*sorted_edges), edge_to_sort_cmp);
  for (i = 0; i < ndissolvable; i++) {
    e = sorted_edges[i].e;
    se = &e->symedges[0];
    dissolve = true;
    if (true /*!edge_touches_frame(e)*/) {
      fleft = se->face;
      fright = sym(se)->face;
      if (fleft != cdt->outer_face && fright != cdt->outer_face &&
          (fleft->input_ids != NULL || fright->input_ids != NULL)) {
        /* Is there another symedge with same left and right faces?
         * Or is there a vertex not part of e touching the same left and right faces? */
        for (se2 = se->next; dissolve && se2 != se; se2 = se2->next) {
          if (sym(se2)->face == fright ||
              (se2->vert != se->next->vert && vert_touches_face(se2->vert, fright))) {
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

static void remove_outer_edges_until_constraints(CDT_state *cdt)
{
  LinkNode *fstack = NULL;
  SymEdge *se, *se_start;
  CDTFace *f, *fsym;
  int visit = ++cdt->visit_count;
#ifdef DEBUG_CDT
  int dbg_level = 0;

  if (dbg_level > 0) {
    fprintf(stderr, "remove_outer_edges_until_constraints\n");
  }
#endif

  cdt->outer_face->visit_index = visit;
  /* Walk around outer face, adding faces on other side of dissolvable edges to stack. */
  se_start = se = cdt->outer_face->symedge;
  do {
    if (!is_constrained_edge(se->edge)) {
      fsym = sym(se)->face;
      if (fsym->visit_index != visit) {
#ifdef DEBUG_CDT
        if (dbg_level > 0) {
          fprintf(stderr, "pushing f=%p from symedge ", fsym);
          dump_se(se, "an outer edge");
        }
#endif
        BLI_linklist_prepend_pool(&fstack, fsym, cdt->listpool);
      }
    }
  } while ((se = se->next) != se_start);

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
      if (dbg_level > 1) {
        dump_cdt(cdt, "cdt at top of loop");
        cdt_draw(cdt, "top of dissolve loop");
      }
    }
#endif
    f->visit_index = visit;
    se_start = se = f->symedge;
    do {
      dissolvable = !is_constrained_edge(se->edge);
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
  if (cdt->edges == NULL) {
    return;
  }

  /* Make sure all non-deleted faces have a symedge. */
  for (ln = cdt->edges; ln; ln = ln->next) {
    e = (CDTEdge *)ln->link;
    if (!is_deleted_edge(e)) {
      if (e->symedges[0].face->symedge == NULL) {
        e->symedges[0].face->symedge = &e->symedges[0];
      }
      if (e->symedges[1].face->symedge == NULL) {
        e->symedges[1].face->symedge = &e->symedges[1];
      }
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

  if (output_type == CDT_CONSTRAINTS) {
    remove_non_constraint_edges(cdt);
  }
  else if (output_type == CDT_CONSTRAINTS_VALID_BMESH) {
    remove_non_constraint_edges_leave_valid_bmesh(cdt);
  }
  else if (output_type == CDT_INSIDE) {
    remove_outer_edges_until_constraints(cdt);
  }
}

static CDT_result *cdt_get_output(CDT_state *cdt,
                                  const CDT_input *input,
                                  const CDT_output_type output_type)
{
  int i, j, nv, ne, nf, faces_len_total;
  int orig_map_size, orig_map_index;
  int *vert_to_output_map;
  CDT_result *result;
  CDTVert *v;
  LinkNode *lne, *lnf, *ln;
  SymEdge *se, *se_start;
  CDTEdge *e;
  CDTFace *f;
#ifdef DEBUG_CDT
  int dbg_level = 0;

  if (dbg_level > 0) {
    fprintf(stderr, "\nCDT_GET_OUTPUT\n\n");
  }
#endif

  prepare_cdt_for_output(cdt, output_type);

  result = (CDT_result *)MEM_callocN(sizeof(*result), __func__);
  if (cdt->vert_array_len == 0) {
    return result;
  }

#ifdef DEBUG_CDT
  if (dbg_level > 1) {
    dump_cdt(cdt, "cdt to output");
  }
#endif

  /* All verts without a merge_to_index will be output.
   * vert_to_output_map[i] will hold the output vertex index
   * corresponding to the vert in position i in cdt->vert_array.
   * Since merging picked the leftmost-lowermost representative,
   * that is not necessarily the same as the vertex with the lowest original
   * index (i.e., index in cdt->vert_array), so we need two passes:
   * one to get the non-merged-to vertices in vert_to_output_map,
   * and a second to put in the merge targets for merged-to vertices.
   */
  vert_to_output_map = BLI_memarena_alloc(cdt->arena, (size_t)cdt->vert_array_len * sizeof(int *));
  nv = 0;
  for (i = 0; i < cdt->vert_array_len; i++) {
    v = cdt->vert_array[i];
    if (v->merge_to_index == -1) {
      vert_to_output_map[i] = nv;
      nv++;
    }
  }
  if (nv <= 0) {
    return result;
  }
  if (nv < cdt->vert_array_len) {
    for (i = 0; i < input->verts_len; i++) {
      v = cdt->vert_array[i];
      if (v->merge_to_index != -1) {
        add_to_input_ids(&cdt->vert_array[v->merge_to_index]->input_ids, i, cdt);
        vert_to_output_map[i] = vert_to_output_map[v->merge_to_index];
      }
    }
  }

  result->verts_len = nv;
  result->vert_coords = MEM_malloc_arrayN(nv, sizeof(result->vert_coords[0]), __func__);

  /* Make the vertex "orig" map arrays, mapping output verts to lists of input ones. */
  orig_map_size = 0;
  for (i = 0; i < cdt->vert_array_len; i++) {
    if (cdt->vert_array[i]->merge_to_index == -1) {
      orig_map_size += 1 + BLI_linklist_count(cdt->vert_array[i]->input_ids);
    }
  }
  result->verts_orig_len_table = MEM_malloc_arrayN(nv, sizeof(int), __func__);
  result->verts_orig_start_table = MEM_malloc_arrayN(nv, sizeof(int), __func__);
  result->verts_orig = MEM_malloc_arrayN(orig_map_size, sizeof(int), __func__);

  orig_map_index = 0;
  i = 0;
  for (j = 0; j < cdt->vert_array_len; j++) {
    v = cdt->vert_array[j];
    if (v->merge_to_index == -1) {
      result->vert_coords[i][0] = (float)v->co[0];
      result->vert_coords[i][1] = (float)v->co[1];
      result->verts_orig_start_table[i] = orig_map_index;
      if (j < input->verts_len) {
        result->verts_orig[orig_map_index++] = j;
      }
      for (ln = v->input_ids; ln; ln = ln->next) {
        result->verts_orig[orig_map_index++] = POINTER_AS_INT(ln->link);
      }
      result->verts_orig_len_table[i] = orig_map_index - result->verts_orig_start_table[i];
      i++;
    }
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
        result->edges[i][0] = vert_to_output_map[e->symedges[0].vert->index];
        result->edges[i][1] = vert_to_output_map[e->symedges[1].vert->index];
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
          result->faces[j++] = vert_to_output_map[se->vert->index];
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

/**
 * Calculate the Constrained Delaunay Triangulation of the 2d elements given in \a input.
 *
 * A Delaunay triangulation of a set of vertices is a triangulation where no triangle in the
 * triangulation has a circumcircle that strictly contains another vertex. Delaunay triangulations
 * are avoid long skinny triangles: they maximize the minimum angle of all triangles in the
 * triangulation.
 *
 * A Constrained Delaunay Triangulation adds the requirement that user-provided line segments must
 * appear as edges in the output (perhaps divided into several sub-segments). It is not required
 * that the input edges be non-intersecting: this routine will calculate the intersections. This
 * means that besides triangulating, this routine is also useful for general and robust 2d edge and
 * face intersection.
 *
 * This routine also takes an epsilon parameter in the \a input. Input vertices closer than epsilon
 * will be merged, and we collapse tiny edges (less than epsilon length).
 *
 * The current initial Deluanay triangulation algorithm is the Guibas-Stolfi Divide and Conquer
 * algorithm (see "Primitives for the Manipulation of General Subdivisions and the Computation of
 * Voronoi Diagrams"). and uses Shewchuk's exact predicates to issues where numeric errors cause
 * inconsistent geometric judgments. This is followed by inserting edge constraints (including the
 * edges implied by faces) using the algorithms discussed in "Fully Dynamic Constrained Delaunay
 * Triangulations" by Kallmann, Bieri, and Thalmann.
 *
 * \param input: points to a CDT_input struct which contains the vertices, edges, and faces to be
 * triangulated. \param output_type: specifies which edges to remove after doing the triangulation.
 * \return A pointer to an allocated CDT_result struct, which describes the triangulation in terms
 * of vertices, edges, and faces, and also has tables to map output elements back to input
 * elements. The caller must use BLI_delaunay_2d_cdt_free() on the result when done with it.
 *
 * See the header file BLI_delaunay_2d.h for details of the CDT_input and CDT_result structs and
 * the CDT_output_type enum.
 */
CDT_result *BLI_delaunay_2d_cdt_calc(const CDT_input *input, const CDT_output_type output_type)
{
  int nv = input->verts_len;
  int ne = input->edges_len;
  int nf = input->faces_len;
  int i, iv1, iv2, f, fedge_start, fedge_end, ei;
  CDT_state *cdt;
  CDTVert *v1, *v2;
  CDTEdge *face_edge;
  SymEdge *face_symedge;
  LinkNode *edge_list;
  CDT_result *result;
  const CDT_input *input_orig;
  int *new_edge_map;
  static bool called_exactinit = false;
#ifdef DEBUG_CDT
  int dbg_level = 0;
#endif

  /* The exact orientation and incircle primitives need a one-time initialization of certain
   * constants. */
  if (!called_exactinit) {
    exactinit();
    called_exactinit = true;
  }
#ifdef DEBUG_CDT
  if (dbg_level > 0) {
    fprintf(stderr,
            "\n\nCDT CALC, nv=%d, ne=%d, nf=%d, eps=%g\n",
            input->verts_len,
            input->edges_len,
            input->faces_len,
            input->epsilon);
  }
  if (dbg_level == -1) {
    write_cdt_input_to_file(input);
  }
#endif

  if ((nv > 0 && input->vert_coords == NULL) || (ne > 0 && input->edges == NULL) ||
      (nf > 0 && (input->faces == NULL || input->faces_start_table == NULL ||
                  input->faces_len_table == NULL))) {
#ifdef DEBUG_CDT
    fprintf(stderr, "invalid input: unexpected NULL array(s)\n");
#endif
    return NULL;
  }

  input_orig = input;
  input = modify_input_for_near_edge_ends(input, &new_edge_map);
  if (input != input_orig) {
    nv = input->verts_len;
    ne = input->edges_len;
    nf = input->faces_len;
#ifdef DEBUG_CDT
    if (dbg_level > 0) {
      fprintf(stderr, "input modified for near ends; now ne=%d\n", ne);
    }
#endif
  }
  cdt = cdt_init(input);
  initial_triangulation(cdt);
#ifdef DEBUG_CDT
  if (dbg_level > 0) {
    validate_cdt(cdt, true, false, false);
    if (dbg_level > 1) {
      cdt_draw(cdt, "after initial triangulation");
    }
  }
#endif

  for (i = 0; i < ne; i++) {
    iv1 = input->edges[i][0];
    iv2 = input->edges[i][1];
    if (iv1 < 0 || iv1 >= nv || iv2 < 0 || iv2 >= nv) {
#ifdef DEBUG_CDT
      fprintf(stderr, "edge indices for e%d not valid: v1=%d, v2=%d, nv=%d\n", i, iv1, iv2, nv);
#endif
      continue;
    }
    v1 = cdt->vert_array[iv1];
    v2 = cdt->vert_array[iv2];
    if (v1->merge_to_index != -1) {
      v1 = cdt->vert_array[v1->merge_to_index];
    }
    if (v2->merge_to_index != -1) {
      v2 = cdt->vert_array[v2->merge_to_index];
    }
    if (new_edge_map) {
      ei = new_edge_map[i];
    }
    else {
      ei = i;
    }
    add_edge_constraint(cdt, v1, v2, ei, NULL);
#ifdef DEBUG_CDT
    if (dbg_level > 3) {
      char namebuf[60];
      sprintf(namebuf, "after edge constraint %d = (%d,%d)\n", i, iv1, iv2);
      cdt_draw(cdt, namebuf);
      // dump_cdt(cdt, namebuf);
      validate_cdt(cdt, true, true, false);
    }
#endif
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
      if (new_edge_map) {
        face_edge_id = new_edge_map[face_edge_id];
      }
      iv1 = input->faces[fstart + i];
      iv2 = input->faces[fstart + ((i + 1) % flen)];
      if (iv1 < 0 || iv1 >= nv || iv2 < 0 || iv2 >= nv) {
#ifdef DEBUG_CDT
        fprintf(stderr, "face indices not valid: f=%d, iv1=%d, iv2=%d, nv=%d\n", f, iv1, iv2, nv);
#endif
        continue;
      }
      v1 = cdt->vert_array[iv1];
      v2 = cdt->vert_array[iv2];
      if (v1->merge_to_index != -1) {
        v1 = cdt->vert_array[v1->merge_to_index];
      }
      if (v2->merge_to_index != -1) {
        v2 = cdt->vert_array[v2->merge_to_index];
      }
      add_edge_constraint(cdt, v1, v2, face_edge_id, &edge_list);
#ifdef DEBUG_CDT
      if (dbg_level > 2) {
        fprintf(stderr, "edges for edge %d:\n", i);
        for (LinkNode *ln = edge_list; ln; ln = ln->next) {
          CDTEdge *cdt_e = (CDTEdge *)ln->link;
          fprintf(stderr,
                  "  (%.2f,%.2f)->(%.2f,%.2f)\n",
                  F2(cdt_e->symedges[0].vert->co),
                  F2(cdt_e->symedges[1].vert->co));
        }
      }
      if (dbg_level > 2) {
        cdt_draw(cdt, "after a face edge");
        if (dbg_level > 3) {
          dump_cdt(cdt, "after a face edge");
        }
        validate_cdt(cdt, true, true, false);
      }
#endif
      if (i == 0) {
        face_edge = (CDTEdge *)edge_list->link;
        face_symedge = &face_edge->symedges[0];
        if (face_symedge->vert != v1) {
          face_symedge = &face_edge->symedges[1];
          BLI_assert(face_symedge->vert == v1);
        }
      }
      BLI_linklist_free_pool(edge_list, NULL, cdt->listpool);
    }
    fedge_start = cdt->face_edge_offset + fstart;
    fedge_end = fedge_start + flen - 1;
    add_face_ids(cdt, face_symedge, f, fedge_start, fedge_end);
  }
#ifdef DEBUG_CDT
  if (dbg_level > 0) {
    validate_cdt(cdt, true, true, false);
  }
  if (dbg_level > 1) {
    cdt_draw(cdt, "after adding edges and faces");
    if (dbg_level > 2) {
      dump_cdt(cdt, "after adding edges and faces");
    }
  }
#endif

  if (cdt->epsilon > 0.0) {
    remove_small_features(cdt);
#ifdef DEBUG_CDT
    if (dbg_level > 2) {
      cdt_draw(cdt, "after remove small features\n");
      if (dbg_level > 3) {
        dump_cdt(cdt, "after remove small features\n");
      }
    }
#endif
  }

  result = cdt_get_output(cdt, input, output_type);
#ifdef DEBUG_CDT
  if (dbg_level > 0) {
    cdt_draw(cdt, "final");
  }
#endif

  if (input != input_orig) {
    free_modified_input((CDT_input *)input);
  }
  new_cdt_free(cdt);
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

ATTU static const char *vertname(const CDTVert *v)
{
  static char vertnamebuf[20];

  sprintf(vertnamebuf, "[%d]", v->index);
  return vertnamebuf;
}

ATTU static const char *sename(const SymEdge *se)
{
  static char senamebuf[20];

  sprintf(senamebuf, "{%x}", (POINTER_AS_UINT(se)) & 0xFFFF);
  return senamebuf;
}

static void dump_v(const CDTVert *v, const char *lab)
{
  fprintf(stderr, "%s%s(%.10f,%.10f)\n", lab, vertname(v), F2(v->co));
}

static void dump_se(const SymEdge *se, const char *lab)
{
  if (se->next) {
    fprintf(stderr,
            "%s%s((%.10f,%.10f)->(%.10f,%.10f))",
            lab,
            vertname(se->vert),
            F2(se->vert->co),
            F2(se->next->vert->co));
    fprintf(stderr, "%s\n", vertname(se->next->vert));
  }
  else {
    fprintf(stderr, "%s%s((%.10f,%.10f)->NULL)\n", lab, vertname(se->vert), F2(se->vert->co));
  }
}

static void dump_se_short(const SymEdge *se, const char *lab)
{
  if (se == NULL) {
    fprintf(stderr, "%sNULL", lab);
  }
  else {
    fprintf(stderr, "%s%s", lab, vertname(se->vert));
    fprintf(stderr, "%s", se->next == NULL ? "[NULL]" : vertname(se->next->vert));
  }
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

static void dump_cross_data(struct CrossData *cd, const char *lab)
{
  fprintf(stderr, "%s", lab);
  if (cd->lambda == 0.0) {
    fprintf(stderr, "v%d", cd->vert->index);
  }
  else {
    fprintf(stderr, "lambda=%.17g", cd->lambda);
  }
  dump_se_short(cd->in, " in=");
  dump_se_short(cd->out, " out=");
  fprintf(stderr, "\n");
}

/* If filter_fn != NULL, only dump vert v its edges when filter_fn(cdt, v, filter_data) is true. */
#  define PL(p) (POINTER_AS_UINT(p) & 0xFFFF)
static void dump_cdt_filtered(const CDT_state *cdt,
                              bool (*filter_fn)(const CDT_state *, int, void *),
                              void *filter_data,
                              const char *lab)
{
  LinkNode *ln;
  CDTVert *v, *vother;
  CDTEdge *e;
  CDTFace *f;
  SymEdge *se;
  int i, cnt;

  fprintf(stderr, "\nCDT %s\n", lab);
  fprintf(stderr, "\nVERTS\n");
  for (i = 0; i < cdt->vert_array_len; i++) {
    if (filter_fn && !filter_fn(cdt, i, filter_data)) {
      continue;
    }
    v = cdt->vert_array[i];
    fprintf(stderr, "%s %x: (%f,%f) symedge=%x", vertname(v), PL(v), F2(v->co), PL(v->symedge));
    if (v->merge_to_index == -1) {
      fprintf(stderr, "\n");
    }
    else {
      fprintf(stderr, " merge to %s\n", vertname(cdt->vert_array[v->merge_to_index]));
      continue;
    }
    dump_id_list(v->input_ids, "  ");
    se = v->symedge;
    cnt = 0;
    if (se) {
      fprintf(stderr, "  edges out:\n");
      do {
        if (se->next == NULL) {
          fprintf(stderr, "    [NULL next/rot symedge, se=%x\n", PL(se));
          break;
        }
        if (se->next->next == NULL) {
          fprintf(stderr, "    [NULL next-next/rot symedge, se=%x\n", PL(se));
          break;
        }
        vother = sym(se)->vert;
        fprintf(stderr, "    %s (e=%x, se=%x)\n", vertname(vother), PL(se->edge), PL(se));
        se = se->rot;
        cnt++;
      } while (se != v->symedge && cnt < 25);
      fprintf(stderr, "\n");
    }
  }
  if (filter_fn) {
    return;
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
              "  se[%d] @%x: next=%x, rot=%x, vert=%x [%s] (%.2f,%.2f), edge=%x, face=%x\n",
              i,
              PL(se),
              PL(se->next),
              PL(se->rot),
              PL(se->vert),
              vertname(se->vert),
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
      fprintf(stderr, "%x: outer", PL(f));
    }
    fprintf(stderr, " symedge=%x\n", PL(f->symedge));
    dump_id_list(f->input_ids, "  ");
  }
  fprintf(stderr, "\nOTHER\n");
  fprintf(stderr, "outer_face=%x\n", PL(cdt->outer_face));
  fprintf(
      stderr, "minx=%f, maxx=%f, miny=%f, maxy=%f\n", cdt->minx, cdt->maxx, cdt->miny, cdt->maxy);
  fprintf(stderr, "margin=%f\n", cdt->margin);
}
#  undef PL

static void dump_cdt(const CDT_state *cdt, const char *lab)
{
  dump_cdt_filtered(cdt, NULL, NULL, lab);
}

typedef struct ReachableFilterData {
  int vstart_index;
  int maxdist;
} ReachableFilterData;

/* Stupid algorithm will repeat itself. Don't use for large n. */
static bool reachable_filter(const CDT_state *cdt, int v_index, void *filter_data)
{
  CDTVert *v;
  SymEdge *se;
  ReachableFilterData *rfd_in = (ReachableFilterData *)filter_data;
  ReachableFilterData rfd_next;

  if (v_index == rfd_in->vstart_index) {
    return true;
  }
  if (rfd_in->maxdist <= 0 || v_index < 0 || v_index >= cdt->vert_array_len) {
    return false;
  }
  else {
    v = cdt->vert_array[v_index];
    se = v->symedge;
    if (se != NULL) {
      rfd_next.vstart_index = rfd_in->vstart_index;
      rfd_next.maxdist = rfd_in->maxdist - 1;
      do {
        if (reachable_filter(cdt, se->next->vert->index, &rfd_next)) {
          return true;
        }
        se = se->rot;
      } while (se != v->symedge);
    }
  }
  return false;
}

static void set_min_max(CDT_state *cdt)
{
  int i;
  double minx, maxx, miny, maxy;
  double *co;

  minx = miny = DBL_MAX;
  maxx = maxy = -DBL_MAX;
  for (i = 0; i < cdt->vert_array_len; i++) {
    co = cdt->vert_array[i]->co;
    if (co[0] < minx) {
      minx = co[0];
    }
    if (co[0] > maxx) {
      maxx = co[0];
    }
    if (co[1] < miny) {
      miny = co[1];
    }
    if (co[1] > maxy) {
      maxy = co[1];
    }
  }
  if (minx != DBL_MAX) {
    cdt->minx = minx;
    cdt->miny = miny;
    cdt->maxx = maxx;
    cdt->maxy = maxy;
  }
}

static void dump_cdt_vert_neighborhood(CDT_state *cdt, int v, int maxdist, const char *lab)
{
  ReachableFilterData rfd;
  rfd.vstart_index = v;
  rfd.maxdist = maxdist;
  dump_cdt_filtered(cdt, reachable_filter, &rfd, lab);
}

/*
 * Make an html file with svg in it to display the argument cdt.
 * Mouse-overs will reveal the coordinates of vertices and edges.
 * Constraint edges are drawn thicker than non-constraint edges.
 * The first call creates DRAWFILE; subsequent calls append to it.
 */
#  define DRAWFILE "/tmp/debug_draw.html"
#  define MAX_DRAW_WIDTH 2000
#  define MAX_DRAW_HEIGHT 1400
#  define THIN_LINE 1
#  define THICK_LINE 4
#  define VERT_RADIUS 3
#  define DRAW_VERT_LABELS 1
#  define DRAW_EDGE_LABELS 0

static void cdt_draw_region(
    CDT_state *cdt, const char *lab, double minx, double miny, double maxx, double maxy)
{
  static bool append = false;
  FILE *f = fopen(DRAWFILE, append ? "a" : "w");
  int view_width, view_height;
  double width, height, aspect, scale;
  LinkNode *ln;
  CDTVert *v, *u;
  CDTEdge *e;
  int i, strokew;

  width = maxx - minx;
  height = maxy - miny;
  aspect = height / width;
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
    strokew = is_constrained_edge(e) ? THICK_LINE : THIN_LINE;
    fprintf(f,
            "<line fill=\"none\" stroke=\"black\" stroke-width=\"%d\" "
            "x1=\"%f\" y1=\"%f\" x2=\"%f\" y2=\"%f\">\n",
            strokew,
            SX(u->co[0]),
            SY(u->co[1]),
            SX(v->co[0]),
            SY(v->co[1]));
    fprintf(f, "  <title>%s", vertname(u));
    fprintf(f, "%s</title>\n", vertname(v));
    fprintf(f, "</line>\n");
#  if DRAW_EDGE_LABELS
    fprintf(f,
            "<text x=\"%f\" y=\"%f\" font-size=\"small\">",
            SX(0.5 * (u->co[0] + v->co[0])),
            SY(0.5 * (u->co[1] + v->co[1])));
    fprintf(f, "%s", vertname(u));
    fprintf(f, "%s", vertname(v));
    fprintf(f, "%s", sename(&e->symedges[0]));
    fprintf(f, "%s</text>\n", sename(&e->symedges[1]));
#  endif
  }
  i = 0;
  for (; i < cdt->vert_array_len; i++) {
    v = cdt->vert_array[i];
    if (v->merge_to_index != -1) {
      continue;
    }
    fprintf(f,
            "<circle fill=\"black\" cx=\"%f\" cy=\"%f\" r=\"%d\">\n",
            SX(v->co[0]),
            SY(v->co[1]),
            VERT_RADIUS);
    fprintf(f, "  <title>%s(%.10f,%.10f)</title>\n", vertname(v), v->co[0], v->co[1]);
    fprintf(f, "</circle>\n");
#  if DRAW_VERT_LABELS
    fprintf(f,
            "<text x=\"%f\" y=\"%f\" font-size=\"small\">%s</text>\n",
            SX(v->co[0]) + VERT_RADIUS,
            SY(v->co[1]) - VERT_RADIUS,
            vertname(v));
#  endif
  }

  fprintf(f, "</svg>\n</div>\n");
  fclose(f);
  append = true;
#  undef SX
#  undef SY
}

static void cdt_draw(CDT_state *cdt, const char *lab)
{
  double draw_margin, minx, maxx, miny, maxy;

  set_min_max(cdt);
  draw_margin = (cdt->maxx - cdt->minx + cdt->maxy - cdt->miny + 1) * 0.05;
  minx = cdt->minx - draw_margin;
  maxx = cdt->maxx + draw_margin;
  miny = cdt->miny - draw_margin;
  maxy = cdt->maxy + draw_margin;
  cdt_draw_region(cdt, lab, minx, miny, maxx, maxy);
}

static void cdt_draw_vertex_region(CDT_state *cdt, int v, double dist, const char *lab)
{
  const double *co = cdt->vert_array[v]->co;
  cdt_draw_region(cdt, lab, co[0] - dist, co[1] - dist, co[0] + dist, co[1] + dist);
}

static void cdt_draw_edge_region(CDT_state *cdt, int v1, int v2, double dist, const char *lab)
{
  const double *co1 = cdt->vert_array[v1]->co;
  const double *co2 = cdt->vert_array[v2]->co;
  double minx, miny, maxx, maxy;

  minx = min_dd(co1[0], co2[0]);
  miny = min_dd(co1[1], co2[1]);
  maxx = max_dd(co1[0], co2[0]);
  maxy = max_dd(co1[1], co2[1]);
  cdt_draw_region(cdt, lab, minx - dist, miny - dist, maxx + dist, maxy + dist);
}

#  define CDTFILE "/tmp/cdtinput.txt"
static void write_cdt_input_to_file(const CDT_input *inp)
{
  int i, j;
  FILE *f = fopen(CDTFILE, "w");

  fprintf(f, "%d %d %d\n", inp->verts_len, inp->edges_len, inp->faces_len);
  for (i = 0; i < inp->verts_len; i++) {
    fprintf(f, "%.17f %.17f\n", inp->vert_coords[i][0], inp->vert_coords[i][1]);
  }
  for (i = 0; i < inp->edges_len; i++) {
    fprintf(f, "%d %d\n", inp->edges[i][0], inp->edges[i][1]);
  }
  for (i = 0; i < inp->faces_len; i++) {
    for (j = 0; j < inp->faces_len_table[i]; j++) {
      fprintf(f, "%d ", inp->faces[j + inp->faces_start_table[i]]);
    }
    fprintf(f, "\n");
  }
  fclose(f);
}

#  ifndef NDEBUG /* Only used in assert. */
/*
 * Is a visible from b: i.e., ab crosses no edge of cdt?
 * If constrained is true, consider only constrained edges as possible crossed edges.
 * In any case, don't count an edge ab itself.
 * Note: this is an expensive test if there are a lot of edges.
 */
static bool is_visible(const CDTVert *a, const CDTVert *b, bool constrained, const CDT_state *cdt)
{
  const LinkNode *ln;
  const CDTEdge *e;
  const SymEdge *se, *senext;
  double lambda, mu;
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
        a->co, b->co, se->vert->co, senext->vert->co, &lambda, &mu);
    if (ikind != ISECT_LINE_LINE_NONE) {
      if (ikind == ISECT_LINE_LINE_COLINEAR) {
        /* TODO: special test here for overlap. */
        continue;
      }
      /* Allow an intersection very near or at ends, to allow for numerical error. */
      if (lambda > FLT_EPSILON && (1.0 - lambda) > FLT_EPSILON && mu > FLT_EPSILON &&
          (1.0 - mu) > FLT_EPSILON) {
        return false;
      }
    }
  }
  return true;
}
#  endif

#  ifndef NDEBUG /* Only used in assert. */
/*
 * Check that edge ab satisfies constrained delaunay condition:
 * That is, for all non-constraint, non-border edges ab,
 * (1) ab is visible in the constraint graph; and
 * (2) there is a circle through a and b such that any vertex v connected by an edge to a or b
 *     is not inside that circle.
 * The argument 'se' specifies ab by: a is se's vert and b is se->next's vert.
 * Return true if check is OK.
 */
static bool is_delaunay_edge(const SymEdge *se)
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
      ok[i] |= incircle(a->co, b->co, c->co, ss->next->vert->co) <= 0.0;
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

static void validate_cdt(CDT_state *cdt,
                         bool check_all_tris,
                         bool check_delaunay,
                         bool check_visibility)
{
  LinkNode *ln;
  int totedges, totfaces, totverts;
  CDTEdge *e;
  SymEdge *se, *sesym, *s;
  CDTVert *v, *v1, *v2, *v3;
  CDTFace *f;
  int i, limit;
  bool isborder;

  if (cdt->output_prepared) {
    return;
  }
  if (cdt->edges == NULL || cdt->edges->next == NULL) {
    return;
  }

  BLI_assert(cdt != NULL);
  totedges = 0;
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
    BLI_assert(se->vert != sesym->vert);
    BLI_assert(se->edge == sesym->edge && se->edge == e);
    BLI_assert(sym(se) == sesym && sym(sesym) == se);
    for (i = 0; i < 2; i++) {
      se = &e->symedges[i];
      v = se->vert;
      f = se->face;
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
      if (limit == 3) {
        v1 = se->vert;
        v2 = se->next->vert;
        v3 = se->next->next->vert;
        /* The triangle should be positively oriented, but because
         * the insertion of intersection vertices doesn't use exact
         * arithmetic, this may not be true, so allow a little slop. */
        BLI_assert(orient2d(v1->co, v2->co, v3->co) >= -FLT_EPSILON);
        BLI_assert(orient2d(v2->co, v3->co, v1->co) >= -FLT_EPSILON);
        BLI_assert(orient2d(v3->co, v1->co, v2->co) >= -FLT_EPSILON);
      }
      UNUSED_VARS_NDEBUG(limit);
      BLI_assert(se->next->next != se);
      s = se;
      do {
        BLI_assert(prev(s)->next == s);
        BLI_assert(s->rot == sym(prev(s)));
        s = s->next;
      } while (s != se);
    }
    if (check_visibility) {
      BLI_assert(isborder || is_visible(se->vert, se->next->vert, false, cdt));
    }
    if (!isborder && check_delaunay) {
      BLI_assert(is_delaunay_edge(se));
    }
  }
  totverts = 0;
  for (i = 0; i < cdt->vert_array_len; i++) {
    v = cdt->vert_array[i];
    BLI_assert(plausible_non_null_ptr(v));
    if (v->merge_to_index != -1) {
      BLI_assert(v->merge_to_index >= 0 && v->merge_to_index < cdt->vert_array_len);
      continue;
    }
    totverts++;
    BLI_assert(cdt->vert_array_len <= 1 || v->symedge->vert == v);
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
  }
  /* Euler's formula for planar graphs. */
  if (check_all_tris && totfaces > 1) {
    BLI_assert(totverts - totedges + totfaces == 2);
  }
}
#endif

/* Jonathan Shewchuk's adaptive predicates, trimmed to those needed here.
 * Permission obtained by private communication from Jonathan to include this code in Blender.
 */

/*
 *  Routines for Arbitrary Precision Floating-point Arithmetic
 *  and Fast Robust Geometric Predicates
 *  (predicates.c)
 *
 *  May 18, 1996
 *
 *  Placed in the public domain by
 *  Jonathan Richard Shewchuk
 *  School of Computer Science
 *  Carnegie Mellon University
 *  5000 Forbes Avenue
 *  Pittsburgh, Pennsylvania  15213-3891
 *  jrs@cs.cmu.edu
 *
 *  This file contains C implementation of algorithms for exact addition
 *    and multiplication of floating-point numbers, and predicates for
 *    robustly performing the orientation and incircle tests used in
 *    computational geometry.  The algorithms and underlying theory are
 *    described in Jonathan Richard Shewchuk.  "Adaptive Precision Floating-
 *    Point Arithmetic and Fast Robust Geometric Predicates."  Technical
 *    Report CMU-CS-96-140, School of Computer Science, Carnegie Mellon
 *    University, Pittsburgh, Pennsylvania, May 1996.  (Submitted to
 *    Discrete & Computational Geometry.)
 *
 *  This file, the paper listed above, and other information are available
 *    from the Web page http://www.cs.cmu.edu/~quake/robust.html .
 *
 *  Using this code:
 *
 *  First, read the short or long version of the paper (from the Web page
 *    above).
 *
 *  Be sure to call exactinit() once, before calling any of the arithmetic
 *    functions or geometric predicates.  Also be sure to turn on the
 *    optimizer when compiling this file.
 *
 * On some machines, the exact arithmetic routines might be defeated by the
 *   use of internal extended precision floating-point registers.  Sometimes
 *   this problem can be fixed by defining certain values to be volatile,
 *   thus forcing them to be stored to memory and rounded off.  This isn't
 *   a great solution, though, as it slows the arithmetic down.
 *
 * To try this out, write "#define INEXACT volatile" below.  Normally,
 *   however, INEXACT should be defined to be nothing.  ("#define INEXACT".)
 */

#define INEXACT /* Nothing */
/* #define INEXACT volatile */

/* Which of the following two methods of finding the absolute values is
 *   fastest is compiler-dependent.  A few compilers can inline and optimize
 *   the fabs() call; but most will incur the overhead of a function call,
 *   which is disastrously slow.  A faster way on IEEE machines might be to
 *   mask the appropriate bit, but that's difficult to do in C.
 */

#define Absolute(a) ((a) >= 0.0 ? (a) : -(a))
/* #define Absolute(a)  fabs(a) */

/* Many of the operations are broken up into two pieces, a main part that
 *   performs an approximate operation, and a "tail" that computes the
 *   roundoff error of that operation.
 *
 * The operations Fast_Two_Sum(), Fast_Two_Diff(), Two_Sum(), Two_Diff(),
 *   Split(), and Two_Product() are all implemented as described in the
 *   reference.  Each of these macros requires certain variables to be
 *   defined in the calling routine.  The variables `bvirt', `c', `abig',
 *   `_i', `_j', `_k', `_l', `_m', and `_n' are declared `INEXACT' because
 *   they store the result of an operation that may incur roundoff error.
 *   The input parameter `x' (or the highest numbered `x_' parameter) must
 *   also be declared `INEXACT'.
 */

#define Fast_Two_Sum_Tail(a, b, x, y) \
  bvirt = x - a; \
  y = b - bvirt

#define Fast_Two_Sum(a, b, x, y) \
  x = (double)(a + b); \
  Fast_Two_Sum_Tail(a, b, x, y)

#define Fast_Two_Diff_Tail(a, b, x, y) \
  bvirt = a - x; \
  y = bvirt - b

#define Fast_Two_Diff(a, b, x, y) \
  x = (double)(a - b); \
  Fast_Two_Diff_Tail(a, b, x, y)

#define Two_Sum_Tail(a, b, x, y) \
  bvirt = (double)(x - a); \
  avirt = x - bvirt; \
  bround = b - bvirt; \
  around = a - avirt; \
  y = around + bround

#define Two_Sum(a, b, x, y) \
  x = (double)(a + b); \
  Two_Sum_Tail(a, b, x, y)

#define Two_Diff_Tail(a, b, x, y) \
  bvirt = (double)(a - x); \
  avirt = x + bvirt; \
  bround = bvirt - b; \
  around = a - avirt; \
  y = around + bround

#define Two_Diff(a, b, x, y) \
  x = (double)(a - b); \
  Two_Diff_Tail(a, b, x, y)

#define Split(a, ahi, alo) \
  c = (double)(splitter * a); \
  abig = (double)(c - a); \
  ahi = c - abig; \
  alo = a - ahi

#define Two_Product_Tail(a, b, x, y) \
  Split(a, ahi, alo); \
  Split(b, bhi, blo); \
  err1 = x - (ahi * bhi); \
  err2 = err1 - (alo * bhi); \
  err3 = err2 - (ahi * blo); \
  y = (alo * blo) - err3

#define Two_Product(a, b, x, y) \
  x = (double)(a * b); \
  Two_Product_Tail(a, b, x, y)

#define Two_Product_Presplit(a, b, bhi, blo, x, y) \
  x = (double)(a * b); \
  Split(a, ahi, alo); \
  err1 = x - (ahi * bhi); \
  err2 = err1 - (alo * bhi); \
  err3 = err2 - (ahi * blo); \
  y = (alo * blo) - err3

#define Square_Tail(a, x, y) \
  Split(a, ahi, alo); \
  err1 = x - (ahi * ahi); \
  err3 = err1 - ((ahi + ahi) * alo); \
  y = (alo * alo) - err3

#define Square(a, x, y) \
  x = (double)(a * a); \
  Square_Tail(a, x, y)

#define Two_One_Sum(a1, a0, b, x2, x1, x0) \
  Two_Sum(a0, b, _i, x0); \
  Two_Sum(a1, _i, x2, x1)

#define Two_One_Diff(a1, a0, b, x2, x1, x0) \
  Two_Diff(a0, b, _i, x0); \
  Two_Sum(a1, _i, x2, x1)

#define Two_Two_Sum(a1, a0, b1, b0, x3, x2, x1, x0) \
  Two_One_Sum(a1, a0, b0, _j, _0, x0); \
  Two_One_Sum(_j, _0, b1, x3, x2, x1)

#define Two_Two_Diff(a1, a0, b1, b0, x3, x2, x1, x0) \
  Two_One_Diff(a1, a0, b0, _j, _0, x0); \
  Two_One_Diff(_j, _0, b1, x3, x2, x1)

static double splitter;  /* = 2^ceiling(p / 2) + 1.  Used to split floats in half. */
static double m_epsilon; /* = 2^(-p).  Used to estimate roundoff errors. */
/* A set of coefficients used to calculate maximum roundoff errors. */
static double resulterrbound;
static double ccwerrboundA, ccwerrboundB, ccwerrboundC;
static double o3derrboundA, o3derrboundB, o3derrboundC;
static double iccerrboundA, iccerrboundB, iccerrboundC;
static double isperrboundA, isperrboundB, isperrboundC;

/*  exactinit()   Initialize the variables used for exact arithmetic.
 *
 *  `epsilon' is the largest power of two such that 1.0 + epsilon = 1.0 in
 *  floating-point arithmetic.  `epsilon' bounds the relative roundoff
 *  error.  It is used for floating-point error analysis.
 *
 *  `splitter' is used to split floating-point numbers into two
 *  half-length significands for exact multiplication.
 *
 *  I imagine that a highly optimizing compiler might be too smart for its
 *  own good, and somehow cause this routine to fail, if it pretends that
 *  floating-point arithmetic is too much like real arithmetic.
 *
 *  Don't change this routine unless you fully understand it.
 */

static void exactinit(void)
{
  double half;
  double check, lastcheck;
  int every_other;

  every_other = 1;
  half = 0.5;
  m_epsilon = 1.0;
  splitter = 1.0;
  check = 1.0;
  /* Repeatedly divide `epsilon' by two until it is too small to add to
   *   one without causing roundoff.  (Also check if the sum is equal to
   *   the previous sum, for machines that round up instead of using exact
   *   rounding.  Not that this library will work on such machines anyway.
   */
  do {
    lastcheck = check;
    m_epsilon *= half;
    if (every_other) {
      splitter *= 2.0;
    }
    every_other = !every_other;
    check = 1.0 + m_epsilon;
  } while ((check != 1.0) && (check != lastcheck));
  splitter += 1.0;

  /* Error bounds for orientation and incircle tests. */
  resulterrbound = (3.0 + 8.0 * m_epsilon) * m_epsilon;
  ccwerrboundA = (3.0 + 16.0 * m_epsilon) * m_epsilon;
  ccwerrboundB = (2.0 + 12.0 * m_epsilon) * m_epsilon;
  ccwerrboundC = (9.0 + 64.0 * m_epsilon) * m_epsilon * m_epsilon;
  o3derrboundA = (7.0 + 56.0 * m_epsilon) * m_epsilon;
  o3derrboundB = (3.0 + 28.0 * m_epsilon) * m_epsilon;
  o3derrboundC = (26.0 + 288.0 * m_epsilon) * m_epsilon * m_epsilon;
  iccerrboundA = (10.0 + 96.0 * m_epsilon) * m_epsilon;
  iccerrboundB = (4.0 + 48.0 * m_epsilon) * m_epsilon;
  iccerrboundC = (44.0 + 576.0 * m_epsilon) * m_epsilon * m_epsilon;
  isperrboundA = (16.0 + 224.0 * m_epsilon) * m_epsilon;
  isperrboundB = (5.0 + 72.0 * m_epsilon) * m_epsilon;
  isperrboundC = (71.0 + 1408.0 * m_epsilon) * m_epsilon * m_epsilon;
}

/*  fast_expansion_sum_zeroelim()   Sum two expansions, eliminating zero
 *                                  components from the output expansion.
 *
 *  Sets h = e + f.  See the long version of my paper for details.
 *
 *  If round-to-even is used (as with IEEE 754), maintains the strongly
 *  non-overlapping property.  (That is, if e is strongly non-overlapping, h
 *  will be also.)  Does NOT maintain the non-overlapping or non-adjacent
 *  properties.
 */

static int fast_expansion_sum_zeroelim(
    int elen, double *e, int flen, double *f, double *h) /* h cannot be e or f. */
{
  double Q;
  INEXACT double Qnew;
  INEXACT double hh;
  INEXACT double bvirt;
  double avirt, bround, around;
  int eindex, findex, hindex;
  double enow, fnow;

  enow = e[0];
  fnow = f[0];
  eindex = findex = 0;
  if ((fnow > enow) == (fnow > -enow)) {
    Q = enow;
    enow = e[++eindex];
  }
  else {
    Q = fnow;
    fnow = f[++findex];
  }
  hindex = 0;
  if ((eindex < elen) && (findex < flen)) {
    if ((fnow > enow) == (fnow > -enow)) {
      Fast_Two_Sum(enow, Q, Qnew, hh);
      enow = e[++eindex];
    }
    else {
      Fast_Two_Sum(fnow, Q, Qnew, hh);
      fnow = f[++findex];
    }
    Q = Qnew;
    if (hh != 0.0) {
      h[hindex++] = hh;
    }
    while ((eindex < elen) && (findex < flen)) {
      if ((fnow > enow) == (fnow > -enow)) {
        Two_Sum(Q, enow, Qnew, hh);
        enow = e[++eindex];
      }
      else {
        Two_Sum(Q, fnow, Qnew, hh);
        fnow = f[++findex];
      }
      Q = Qnew;
      if (hh != 0.0) {
        h[hindex++] = hh;
      }
    }
  }
  while (eindex < elen) {
    Two_Sum(Q, enow, Qnew, hh);
    enow = e[++eindex];
    Q = Qnew;
    if (hh != 0.0) {
      h[hindex++] = hh;
    }
  }
  while (findex < flen) {
    Two_Sum(Q, fnow, Qnew, hh);
    fnow = f[++findex];
    Q = Qnew;
    if (hh != 0.0) {
      h[hindex++] = hh;
    }
  }
  if ((Q != 0.0) || (hindex == 0)) {
    h[hindex++] = Q;
  }
  return hindex;
}

/*  scale_expansion_zeroelim()   Multiply an expansion by a scalar,
 *                               eliminating zero components from the
 *                               output expansion.
 *
 *  Sets h = be.  See either version of my paper for details.
 *
 *  Maintains the nonoverlapping property.  If round-to-even is used (as
 *  with IEEE 754), maintains the strongly nonoverlapping and nonadjacent
 *  properties as well.  (That is, if e has one of these properties, so
 *  will h.)
 */

static int scale_expansion_zeroelim(int elen,
                                    double *e,
                                    double b,
                                    double *h) /* e and h cannot be the same. */
{
  INEXACT double Q, sum;
  double hh;
  INEXACT double product1;
  double product0;
  int eindex, hindex;
  double enow;
  INEXACT double bvirt;
  double avirt, bround, around;
  INEXACT double c;
  INEXACT double abig;
  double ahi, alo, bhi, blo;
  double err1, err2, err3;

  Split(b, bhi, blo);
  Two_Product_Presplit(e[0], b, bhi, blo, Q, hh);
  hindex = 0;
  if (hh != 0) {
    h[hindex++] = hh;
  }
  for (eindex = 1; eindex < elen; eindex++) {
    enow = e[eindex];
    Two_Product_Presplit(enow, b, bhi, blo, product1, product0);
    Two_Sum(Q, product0, sum, hh);
    if (hh != 0) {
      h[hindex++] = hh;
    }
    Fast_Two_Sum(product1, sum, Q, hh);
    if (hh != 0) {
      h[hindex++] = hh;
    }
  }
  if ((Q != 0.0) || (hindex == 0)) {
    h[hindex++] = Q;
  }
  return hindex;
}

/*  estimate()   Produce a one-word estimate of an expansion's value.
 *
 *  See either version of my paper for details.
 */

static double estimate(int elen, double *e)
{
  double Q;
  int eindex;

  Q = e[0];
  for (eindex = 1; eindex < elen; eindex++) {
    Q += e[eindex];
  }
  return Q;
}

/*  orient2d()   Adaptive exact 2D orientation test.  Robust.
 *
 *               Return a positive value if the points pa, pb, and pc occur
 *               in counterclockwise order; a negative value if they occur
 *               in clockwise order; and zero if they are collinear.  The
 *               result is also a rough approximation of twice the signed
 *               area of the triangle defined by the three points.
 *
 *  This uses exact arithmetic to ensure a correct answer.  The
 *  result returned is the determinant of a matrix.
 *  This determinant is computed adaptively, in the sense that exact
 *  arithmetic is used only to the degree it is needed to ensure that the
 *  returned value has the correct sign.  Hence, orient2d() is usually quite
 *  fast, but will run more slowly when the input points are collinear or
 *  nearly so.
 */

static double orient2dadapt(const double *pa, const double *pb, const double *pc, double detsum)
{
  INEXACT double acx, acy, bcx, bcy;
  double acxtail, acytail, bcxtail, bcytail;
  INEXACT double detleft, detright;
  double detlefttail, detrighttail;
  double det, errbound;
  double B[4], C1[8], C2[12], D[16];
  INEXACT double B3;
  int C1length, C2length, Dlength;
  double u[4];
  INEXACT double u3;
  INEXACT double s1, t1;
  double s0, t0;

  INEXACT double bvirt;
  double avirt, bround, around;
  INEXACT double c;
  INEXACT double abig;
  double ahi, alo, bhi, blo;
  double err1, err2, err3;
  INEXACT double _i, _j;
  double _0;

  acx = (double)(pa[0] - pc[0]);
  bcx = (double)(pb[0] - pc[0]);
  acy = (double)(pa[1] - pc[1]);
  bcy = (double)(pb[1] - pc[1]);

  Two_Product(acx, bcy, detleft, detlefttail);
  Two_Product(acy, bcx, detright, detrighttail);

  Two_Two_Diff(detleft, detlefttail, detright, detrighttail, B3, B[2], B[1], B[0]);
  B[3] = B3;

  det = estimate(4, B);
  errbound = ccwerrboundB * detsum;
  if ((det >= errbound) || (-det >= errbound)) {
    return det;
  }

  Two_Diff_Tail(pa[0], pc[0], acx, acxtail);
  Two_Diff_Tail(pb[0], pc[0], bcx, bcxtail);
  Two_Diff_Tail(pa[1], pc[1], acy, acytail);
  Two_Diff_Tail(pb[1], pc[1], bcy, bcytail);

  if ((acxtail == 0.0) && (acytail == 0.0) && (bcxtail == 0.0) && (bcytail == 0.0)) {
    return det;
  }

  errbound = ccwerrboundC * detsum + resulterrbound * Absolute(det);
  det += (acx * bcytail + bcy * acxtail) - (acy * bcxtail + bcx * acytail);
  if ((det >= errbound) || (-det >= errbound)) {
    return det;
  }

  Two_Product(acxtail, bcy, s1, s0);
  Two_Product(acytail, bcx, t1, t0);
  Two_Two_Diff(s1, s0, t1, t0, u3, u[2], u[1], u[0]);
  u[3] = u3;
  C1length = fast_expansion_sum_zeroelim(4, B, 4, u, C1);

  Two_Product(acx, bcytail, s1, s0);
  Two_Product(acy, bcxtail, t1, t0);
  Two_Two_Diff(s1, s0, t1, t0, u3, u[2], u[1], u[0]);
  u[3] = u3;
  C2length = fast_expansion_sum_zeroelim(C1length, C1, 4, u, C2);

  Two_Product(acxtail, bcytail, s1, s0);
  Two_Product(acytail, bcxtail, t1, t0);
  Two_Two_Diff(s1, s0, t1, t0, u3, u[2], u[1], u[0]);
  u[3] = u3;
  Dlength = fast_expansion_sum_zeroelim(C2length, C2, 4, u, D);

  return (D[Dlength - 1]);
}

static double orient2d(const double *pa, const double *pb, const double *pc)
{
  double detleft, detright, det;
  double detsum, errbound;

  detleft = (pa[0] - pc[0]) * (pb[1] - pc[1]);
  detright = (pa[1] - pc[1]) * (pb[0] - pc[0]);
  det = detleft - detright;

  if (detleft > 0.0) {
    if (detright <= 0.0) {
      return det;
    }
    else {
      detsum = detleft + detright;
    }
  }
  else if (detleft < 0.0) {
    if (detright >= 0.0) {
      return det;
    }
    else {
      detsum = -detleft - detright;
    }
  }
  else {
    return det;
  }

  errbound = ccwerrboundA * detsum;
  if ((det >= errbound) || (-det >= errbound)) {
    return det;
  }

  return orient2dadapt(pa, pb, pc, detsum);
}

/*  incircle()   Adaptive exact 2D incircle test.  Robust.
 *
 *               Return a positive value if the point pd lies inside the
 *               circle passing through pa, pb, and pc; a negative value if
 *               it lies outside; and zero if the four points are cocircular.
 *               The points pa, pb, and pc must be in counterclockwise
 *               order, or the sign of the result will be reversed.
 *
 *  This uses exact arithmetic to ensure a correct answer.
 *  The result returned is the determinant of a matrix.
 *  This determinant is computed adaptively, in the sense that exact
 *  arithmetic is used only to the degree it is needed to ensure that the
 *  returned value has the correct sign.  Hence, incircle() is usually quite
 *  fast, but will run more slowly when the input points are cocircular or
 *  nearly so.
 */

static double incircleadapt(
    const double *pa, const double *pb, const double *pc, const double *pd, double permanent)
{
  INEXACT double adx, bdx, cdx, ady, bdy, cdy;
  double det, errbound;

  INEXACT double bdxcdy1, cdxbdy1, cdxady1, adxcdy1, adxbdy1, bdxady1;
  double bdxcdy0, cdxbdy0, cdxady0, adxcdy0, adxbdy0, bdxady0;
  double bc[4], ca[4], ab[4];
  INEXACT double bc3, ca3, ab3;
  double axbc[8], axxbc[16], aybc[8], ayybc[16], adet[32];
  int axbclen, axxbclen, aybclen, ayybclen, alen;
  double bxca[8], bxxca[16], byca[8], byyca[16], bdet[32];
  int bxcalen, bxxcalen, bycalen, byycalen, blen;
  double cxab[8], cxxab[16], cyab[8], cyyab[16], cdet[32];
  int cxablen, cxxablen, cyablen, cyyablen, clen;
  double abdet[64];
  int ablen;
  double fin1[1152], fin2[1152];
  double *finnow, *finother, *finswap;
  int finlength;

  double adxtail, bdxtail, cdxtail, adytail, bdytail, cdytail;
  INEXACT double adxadx1, adyady1, bdxbdx1, bdybdy1, cdxcdx1, cdycdy1;
  double adxadx0, adyady0, bdxbdx0, bdybdy0, cdxcdx0, cdycdy0;
  double aa[4], bb[4], cc[4];
  INEXACT double aa3, bb3, cc3;
  INEXACT double ti1, tj1;
  double ti0, tj0;
  double u[4], v[4];
  INEXACT double u3, v3;
  double temp8[8], temp16a[16], temp16b[16], temp16c[16];
  double temp32a[32], temp32b[32], temp48[48], temp64[64];
  int temp8len, temp16alen, temp16blen, temp16clen;
  int temp32alen, temp32blen, temp48len, temp64len;
  double axtbb[8], axtcc[8], aytbb[8], aytcc[8];
  int axtbblen, axtcclen, aytbblen, aytcclen;
  double bxtaa[8], bxtcc[8], bytaa[8], bytcc[8];
  int bxtaalen, bxtcclen, bytaalen, bytcclen;
  double cxtaa[8], cxtbb[8], cytaa[8], cytbb[8];
  int cxtaalen, cxtbblen, cytaalen, cytbblen;
  double axtbc[8], aytbc[8], bxtca[8], bytca[8], cxtab[8], cytab[8];
  int axtbclen, aytbclen, bxtcalen, bytcalen, cxtablen, cytablen;
  double axtbct[16], aytbct[16], bxtcat[16], bytcat[16], cxtabt[16], cytabt[16];
  int axtbctlen, aytbctlen, bxtcatlen, bytcatlen, cxtabtlen, cytabtlen;
  double axtbctt[8], aytbctt[8], bxtcatt[8];
  double bytcatt[8], cxtabtt[8], cytabtt[8];
  int axtbcttlen, aytbcttlen, bxtcattlen, bytcattlen, cxtabttlen, cytabttlen;
  double abt[8], bct[8], cat[8];
  int abtlen, bctlen, catlen;
  double abtt[4], bctt[4], catt[4];
  int abttlen, bcttlen, cattlen;
  INEXACT double abtt3, bctt3, catt3;
  double negate;

  INEXACT double bvirt;
  double avirt, bround, around;
  INEXACT double c;
  INEXACT double abig;
  double ahi, alo, bhi, blo;
  double err1, err2, err3;
  INEXACT double _i, _j;
  double _0;

  adx = (double)(pa[0] - pd[0]);
  bdx = (double)(pb[0] - pd[0]);
  cdx = (double)(pc[0] - pd[0]);
  ady = (double)(pa[1] - pd[1]);
  bdy = (double)(pb[1] - pd[1]);
  cdy = (double)(pc[1] - pd[1]);

  Two_Product(bdx, cdy, bdxcdy1, bdxcdy0);
  Two_Product(cdx, bdy, cdxbdy1, cdxbdy0);
  Two_Two_Diff(bdxcdy1, bdxcdy0, cdxbdy1, cdxbdy0, bc3, bc[2], bc[1], bc[0]);
  bc[3] = bc3;
  axbclen = scale_expansion_zeroelim(4, bc, adx, axbc);
  axxbclen = scale_expansion_zeroelim(axbclen, axbc, adx, axxbc);
  aybclen = scale_expansion_zeroelim(4, bc, ady, aybc);
  ayybclen = scale_expansion_zeroelim(aybclen, aybc, ady, ayybc);
  alen = fast_expansion_sum_zeroelim(axxbclen, axxbc, ayybclen, ayybc, adet);

  Two_Product(cdx, ady, cdxady1, cdxady0);
  Two_Product(adx, cdy, adxcdy1, adxcdy0);
  Two_Two_Diff(cdxady1, cdxady0, adxcdy1, adxcdy0, ca3, ca[2], ca[1], ca[0]);
  ca[3] = ca3;
  bxcalen = scale_expansion_zeroelim(4, ca, bdx, bxca);
  bxxcalen = scale_expansion_zeroelim(bxcalen, bxca, bdx, bxxca);
  bycalen = scale_expansion_zeroelim(4, ca, bdy, byca);
  byycalen = scale_expansion_zeroelim(bycalen, byca, bdy, byyca);
  blen = fast_expansion_sum_zeroelim(bxxcalen, bxxca, byycalen, byyca, bdet);

  Two_Product(adx, bdy, adxbdy1, adxbdy0);
  Two_Product(bdx, ady, bdxady1, bdxady0);
  Two_Two_Diff(adxbdy1, adxbdy0, bdxady1, bdxady0, ab3, ab[2], ab[1], ab[0]);
  ab[3] = ab3;
  cxablen = scale_expansion_zeroelim(4, ab, cdx, cxab);
  cxxablen = scale_expansion_zeroelim(cxablen, cxab, cdx, cxxab);
  cyablen = scale_expansion_zeroelim(4, ab, cdy, cyab);
  cyyablen = scale_expansion_zeroelim(cyablen, cyab, cdy, cyyab);
  clen = fast_expansion_sum_zeroelim(cxxablen, cxxab, cyyablen, cyyab, cdet);

  ablen = fast_expansion_sum_zeroelim(alen, adet, blen, bdet, abdet);
  finlength = fast_expansion_sum_zeroelim(ablen, abdet, clen, cdet, fin1);

  det = estimate(finlength, fin1);
  errbound = iccerrboundB * permanent;
  if ((det >= errbound) || (-det >= errbound)) {
    return det;
  }

  Two_Diff_Tail(pa[0], pd[0], adx, adxtail);
  Two_Diff_Tail(pa[1], pd[1], ady, adytail);
  Two_Diff_Tail(pb[0], pd[0], bdx, bdxtail);
  Two_Diff_Tail(pb[1], pd[1], bdy, bdytail);
  Two_Diff_Tail(pc[0], pd[0], cdx, cdxtail);
  Two_Diff_Tail(pc[1], pd[1], cdy, cdytail);
  if ((adxtail == 0.0) && (bdxtail == 0.0) && (cdxtail == 0.0) && (adytail == 0.0) &&
      (bdytail == 0.0) && (cdytail == 0.0)) {
    return det;
  }

  errbound = iccerrboundC * permanent + resulterrbound * Absolute(det);
  det += ((adx * adx + ady * ady) *
              ((bdx * cdytail + cdy * bdxtail) - (bdy * cdxtail + cdx * bdytail)) +
          2.0 * (adx * adxtail + ady * adytail) * (bdx * cdy - bdy * cdx)) +
         ((bdx * bdx + bdy * bdy) *
              ((cdx * adytail + ady * cdxtail) - (cdy * adxtail + adx * cdytail)) +
          2.0 * (bdx * bdxtail + bdy * bdytail) * (cdx * ady - cdy * adx)) +
         ((cdx * cdx + cdy * cdy) *
              ((adx * bdytail + bdy * adxtail) - (ady * bdxtail + bdx * adytail)) +
          2.0 * (cdx * cdxtail + cdy * cdytail) * (adx * bdy - ady * bdx));
  if ((det >= errbound) || (-det >= errbound)) {
    return det;
  }

  finnow = fin1;
  finother = fin2;

  if ((bdxtail != 0.0) || (bdytail != 0.0) || (cdxtail != 0.0) || (cdytail != 0.0)) {
    Square(adx, adxadx1, adxadx0);
    Square(ady, adyady1, adyady0);
    Two_Two_Sum(adxadx1, adxadx0, adyady1, adyady0, aa3, aa[2], aa[1], aa[0]);
    aa[3] = aa3;
  }
  if ((cdxtail != 0.0) || (cdytail != 0.0) || (adxtail != 0.0) || (adytail != 0.0)) {
    Square(bdx, bdxbdx1, bdxbdx0);
    Square(bdy, bdybdy1, bdybdy0);
    Two_Two_Sum(bdxbdx1, bdxbdx0, bdybdy1, bdybdy0, bb3, bb[2], bb[1], bb[0]);
    bb[3] = bb3;
  }
  if ((adxtail != 0.0) || (adytail != 0.0) || (bdxtail != 0.0) || (bdytail != 0.0)) {
    Square(cdx, cdxcdx1, cdxcdx0);
    Square(cdy, cdycdy1, cdycdy0);
    Two_Two_Sum(cdxcdx1, cdxcdx0, cdycdy1, cdycdy0, cc3, cc[2], cc[1], cc[0]);
    cc[3] = cc3;
  }

  if (adxtail != 0.0) {
    axtbclen = scale_expansion_zeroelim(4, bc, adxtail, axtbc);
    temp16alen = scale_expansion_zeroelim(axtbclen, axtbc, 2.0 * adx, temp16a);

    axtcclen = scale_expansion_zeroelim(4, cc, adxtail, axtcc);
    temp16blen = scale_expansion_zeroelim(axtcclen, axtcc, bdy, temp16b);

    axtbblen = scale_expansion_zeroelim(4, bb, adxtail, axtbb);
    temp16clen = scale_expansion_zeroelim(axtbblen, axtbb, -cdy, temp16c);

    temp32alen = fast_expansion_sum_zeroelim(temp16alen, temp16a, temp16blen, temp16b, temp32a);
    temp48len = fast_expansion_sum_zeroelim(temp16clen, temp16c, temp32alen, temp32a, temp48);
    finlength = fast_expansion_sum_zeroelim(finlength, finnow, temp48len, temp48, finother);
    finswap = finnow;
    finnow = finother;
    finother = finswap;
  }
  if (adytail != 0.0) {
    aytbclen = scale_expansion_zeroelim(4, bc, adytail, aytbc);
    temp16alen = scale_expansion_zeroelim(aytbclen, aytbc, 2.0 * ady, temp16a);

    aytbblen = scale_expansion_zeroelim(4, bb, adytail, aytbb);
    temp16blen = scale_expansion_zeroelim(aytbblen, aytbb, cdx, temp16b);

    aytcclen = scale_expansion_zeroelim(4, cc, adytail, aytcc);
    temp16clen = scale_expansion_zeroelim(aytcclen, aytcc, -bdx, temp16c);

    temp32alen = fast_expansion_sum_zeroelim(temp16alen, temp16a, temp16blen, temp16b, temp32a);
    temp48len = fast_expansion_sum_zeroelim(temp16clen, temp16c, temp32alen, temp32a, temp48);
    finlength = fast_expansion_sum_zeroelim(finlength, finnow, temp48len, temp48, finother);
    finswap = finnow;
    finnow = finother;
    finother = finswap;
  }
  if (bdxtail != 0.0) {
    bxtcalen = scale_expansion_zeroelim(4, ca, bdxtail, bxtca);
    temp16alen = scale_expansion_zeroelim(bxtcalen, bxtca, 2.0 * bdx, temp16a);

    bxtaalen = scale_expansion_zeroelim(4, aa, bdxtail, bxtaa);
    temp16blen = scale_expansion_zeroelim(bxtaalen, bxtaa, cdy, temp16b);

    bxtcclen = scale_expansion_zeroelim(4, cc, bdxtail, bxtcc);
    temp16clen = scale_expansion_zeroelim(bxtcclen, bxtcc, -ady, temp16c);

    temp32alen = fast_expansion_sum_zeroelim(temp16alen, temp16a, temp16blen, temp16b, temp32a);
    temp48len = fast_expansion_sum_zeroelim(temp16clen, temp16c, temp32alen, temp32a, temp48);
    finlength = fast_expansion_sum_zeroelim(finlength, finnow, temp48len, temp48, finother);
    finswap = finnow;
    finnow = finother;
    finother = finswap;
  }
  if (bdytail != 0.0) {
    bytcalen = scale_expansion_zeroelim(4, ca, bdytail, bytca);
    temp16alen = scale_expansion_zeroelim(bytcalen, bytca, 2.0 * bdy, temp16a);

    bytcclen = scale_expansion_zeroelim(4, cc, bdytail, bytcc);
    temp16blen = scale_expansion_zeroelim(bytcclen, bytcc, adx, temp16b);

    bytaalen = scale_expansion_zeroelim(4, aa, bdytail, bytaa);
    temp16clen = scale_expansion_zeroelim(bytaalen, bytaa, -cdx, temp16c);

    temp32alen = fast_expansion_sum_zeroelim(temp16alen, temp16a, temp16blen, temp16b, temp32a);
    temp48len = fast_expansion_sum_zeroelim(temp16clen, temp16c, temp32alen, temp32a, temp48);
    finlength = fast_expansion_sum_zeroelim(finlength, finnow, temp48len, temp48, finother);
    finswap = finnow;
    finnow = finother;
    finother = finswap;
  }
  if (cdxtail != 0.0) {
    cxtablen = scale_expansion_zeroelim(4, ab, cdxtail, cxtab);
    temp16alen = scale_expansion_zeroelim(cxtablen, cxtab, 2.0 * cdx, temp16a);

    cxtbblen = scale_expansion_zeroelim(4, bb, cdxtail, cxtbb);
    temp16blen = scale_expansion_zeroelim(cxtbblen, cxtbb, ady, temp16b);

    cxtaalen = scale_expansion_zeroelim(4, aa, cdxtail, cxtaa);
    temp16clen = scale_expansion_zeroelim(cxtaalen, cxtaa, -bdy, temp16c);

    temp32alen = fast_expansion_sum_zeroelim(temp16alen, temp16a, temp16blen, temp16b, temp32a);
    temp48len = fast_expansion_sum_zeroelim(temp16clen, temp16c, temp32alen, temp32a, temp48);
    finlength = fast_expansion_sum_zeroelim(finlength, finnow, temp48len, temp48, finother);
    finswap = finnow;
    finnow = finother;
    finother = finswap;
  }
  if (cdytail != 0.0) {
    cytablen = scale_expansion_zeroelim(4, ab, cdytail, cytab);
    temp16alen = scale_expansion_zeroelim(cytablen, cytab, 2.0 * cdy, temp16a);

    cytaalen = scale_expansion_zeroelim(4, aa, cdytail, cytaa);
    temp16blen = scale_expansion_zeroelim(cytaalen, cytaa, bdx, temp16b);

    cytbblen = scale_expansion_zeroelim(4, bb, cdytail, cytbb);
    temp16clen = scale_expansion_zeroelim(cytbblen, cytbb, -adx, temp16c);

    temp32alen = fast_expansion_sum_zeroelim(temp16alen, temp16a, temp16blen, temp16b, temp32a);
    temp48len = fast_expansion_sum_zeroelim(temp16clen, temp16c, temp32alen, temp32a, temp48);
    finlength = fast_expansion_sum_zeroelim(finlength, finnow, temp48len, temp48, finother);
    finswap = finnow;
    finnow = finother;
    finother = finswap;
  }

  if ((adxtail != 0.0) || (adytail != 0.0)) {
    if ((bdxtail != 0.0) || (bdytail != 0.0) || (cdxtail != 0.0) || (cdytail != 0.0)) {
      Two_Product(bdxtail, cdy, ti1, ti0);
      Two_Product(bdx, cdytail, tj1, tj0);
      Two_Two_Sum(ti1, ti0, tj1, tj0, u3, u[2], u[1], u[0]);
      u[3] = u3;
      negate = -bdy;
      Two_Product(cdxtail, negate, ti1, ti0);
      negate = -bdytail;
      Two_Product(cdx, negate, tj1, tj0);
      Two_Two_Sum(ti1, ti0, tj1, tj0, v3, v[2], v[1], v[0]);
      v[3] = v3;
      bctlen = fast_expansion_sum_zeroelim(4, u, 4, v, bct);

      Two_Product(bdxtail, cdytail, ti1, ti0);
      Two_Product(cdxtail, bdytail, tj1, tj0);
      Two_Two_Diff(ti1, ti0, tj1, tj0, bctt3, bctt[2], bctt[1], bctt[0]);
      bctt[3] = bctt3;
      bcttlen = 4;
    }
    else {
      bct[0] = 0.0;
      bctlen = 1;
      bctt[0] = 0.0;
      bcttlen = 1;
    }

    if (adxtail != 0.0) {
      temp16alen = scale_expansion_zeroelim(axtbclen, axtbc, adxtail, temp16a);
      axtbctlen = scale_expansion_zeroelim(bctlen, bct, adxtail, axtbct);
      temp32alen = scale_expansion_zeroelim(axtbctlen, axtbct, 2.0 * adx, temp32a);
      temp48len = fast_expansion_sum_zeroelim(temp16alen, temp16a, temp32alen, temp32a, temp48);
      finlength = fast_expansion_sum_zeroelim(finlength, finnow, temp48len, temp48, finother);
      finswap = finnow;
      finnow = finother;
      finother = finswap;
      if (bdytail != 0.0) {
        temp8len = scale_expansion_zeroelim(4, cc, adxtail, temp8);
        temp16alen = scale_expansion_zeroelim(temp8len, temp8, bdytail, temp16a);
        finlength = fast_expansion_sum_zeroelim(finlength, finnow, temp16alen, temp16a, finother);
        finswap = finnow;
        finnow = finother;
        finother = finswap;
      }
      if (cdytail != 0.0) {
        temp8len = scale_expansion_zeroelim(4, bb, -adxtail, temp8);
        temp16alen = scale_expansion_zeroelim(temp8len, temp8, cdytail, temp16a);
        finlength = fast_expansion_sum_zeroelim(finlength, finnow, temp16alen, temp16a, finother);
        finswap = finnow;
        finnow = finother;
        finother = finswap;
      }

      temp32alen = scale_expansion_zeroelim(axtbctlen, axtbct, adxtail, temp32a);
      axtbcttlen = scale_expansion_zeroelim(bcttlen, bctt, adxtail, axtbctt);
      temp16alen = scale_expansion_zeroelim(axtbcttlen, axtbctt, 2.0 * adx, temp16a);
      temp16blen = scale_expansion_zeroelim(axtbcttlen, axtbctt, adxtail, temp16b);
      temp32blen = fast_expansion_sum_zeroelim(temp16alen, temp16a, temp16blen, temp16b, temp32b);
      temp64len = fast_expansion_sum_zeroelim(temp32alen, temp32a, temp32blen, temp32b, temp64);
      finlength = fast_expansion_sum_zeroelim(finlength, finnow, temp64len, temp64, finother);
      finswap = finnow;
      finnow = finother;
      finother = finswap;
    }
    if (adytail != 0.0) {
      temp16alen = scale_expansion_zeroelim(aytbclen, aytbc, adytail, temp16a);
      aytbctlen = scale_expansion_zeroelim(bctlen, bct, adytail, aytbct);
      temp32alen = scale_expansion_zeroelim(aytbctlen, aytbct, 2.0 * ady, temp32a);
      temp48len = fast_expansion_sum_zeroelim(temp16alen, temp16a, temp32alen, temp32a, temp48);
      finlength = fast_expansion_sum_zeroelim(finlength, finnow, temp48len, temp48, finother);
      finswap = finnow;
      finnow = finother;
      finother = finswap;

      temp32alen = scale_expansion_zeroelim(aytbctlen, aytbct, adytail, temp32a);
      aytbcttlen = scale_expansion_zeroelim(bcttlen, bctt, adytail, aytbctt);
      temp16alen = scale_expansion_zeroelim(aytbcttlen, aytbctt, 2.0 * ady, temp16a);
      temp16blen = scale_expansion_zeroelim(aytbcttlen, aytbctt, adytail, temp16b);
      temp32blen = fast_expansion_sum_zeroelim(temp16alen, temp16a, temp16blen, temp16b, temp32b);
      temp64len = fast_expansion_sum_zeroelim(temp32alen, temp32a, temp32blen, temp32b, temp64);
      finlength = fast_expansion_sum_zeroelim(finlength, finnow, temp64len, temp64, finother);
      finswap = finnow;
      finnow = finother;
      finother = finswap;
    }
  }
  if ((bdxtail != 0.0) || (bdytail != 0.0)) {
    if ((cdxtail != 0.0) || (cdytail != 0.0) || (adxtail != 0.0) || (adytail != 0.0)) {
      Two_Product(cdxtail, ady, ti1, ti0);
      Two_Product(cdx, adytail, tj1, tj0);
      Two_Two_Sum(ti1, ti0, tj1, tj0, u3, u[2], u[1], u[0]);
      u[3] = u3;
      negate = -cdy;
      Two_Product(adxtail, negate, ti1, ti0);
      negate = -cdytail;
      Two_Product(adx, negate, tj1, tj0);
      Two_Two_Sum(ti1, ti0, tj1, tj0, v3, v[2], v[1], v[0]);
      v[3] = v3;
      catlen = fast_expansion_sum_zeroelim(4, u, 4, v, cat);

      Two_Product(cdxtail, adytail, ti1, ti0);
      Two_Product(adxtail, cdytail, tj1, tj0);
      Two_Two_Diff(ti1, ti0, tj1, tj0, catt3, catt[2], catt[1], catt[0]);
      catt[3] = catt3;
      cattlen = 4;
    }
    else {
      cat[0] = 0.0;
      catlen = 1;
      catt[0] = 0.0;
      cattlen = 1;
    }

    if (bdxtail != 0.0) {
      temp16alen = scale_expansion_zeroelim(bxtcalen, bxtca, bdxtail, temp16a);
      bxtcatlen = scale_expansion_zeroelim(catlen, cat, bdxtail, bxtcat);
      temp32alen = scale_expansion_zeroelim(bxtcatlen, bxtcat, 2.0 * bdx, temp32a);
      temp48len = fast_expansion_sum_zeroelim(temp16alen, temp16a, temp32alen, temp32a, temp48);
      finlength = fast_expansion_sum_zeroelim(finlength, finnow, temp48len, temp48, finother);
      finswap = finnow;
      finnow = finother;
      finother = finswap;
      if (cdytail != 0.0) {
        temp8len = scale_expansion_zeroelim(4, aa, bdxtail, temp8);
        temp16alen = scale_expansion_zeroelim(temp8len, temp8, cdytail, temp16a);
        finlength = fast_expansion_sum_zeroelim(finlength, finnow, temp16alen, temp16a, finother);
        finswap = finnow;
        finnow = finother;
        finother = finswap;
      }
      if (adytail != 0.0) {
        temp8len = scale_expansion_zeroelim(4, cc, -bdxtail, temp8);
        temp16alen = scale_expansion_zeroelim(temp8len, temp8, adytail, temp16a);
        finlength = fast_expansion_sum_zeroelim(finlength, finnow, temp16alen, temp16a, finother);
        finswap = finnow;
        finnow = finother;
        finother = finswap;
      }

      temp32alen = scale_expansion_zeroelim(bxtcatlen, bxtcat, bdxtail, temp32a);
      bxtcattlen = scale_expansion_zeroelim(cattlen, catt, bdxtail, bxtcatt);
      temp16alen = scale_expansion_zeroelim(bxtcattlen, bxtcatt, 2.0 * bdx, temp16a);
      temp16blen = scale_expansion_zeroelim(bxtcattlen, bxtcatt, bdxtail, temp16b);
      temp32blen = fast_expansion_sum_zeroelim(temp16alen, temp16a, temp16blen, temp16b, temp32b);
      temp64len = fast_expansion_sum_zeroelim(temp32alen, temp32a, temp32blen, temp32b, temp64);
      finlength = fast_expansion_sum_zeroelim(finlength, finnow, temp64len, temp64, finother);
      finswap = finnow;
      finnow = finother;
      finother = finswap;
    }
    if (bdytail != 0.0) {
      temp16alen = scale_expansion_zeroelim(bytcalen, bytca, bdytail, temp16a);
      bytcatlen = scale_expansion_zeroelim(catlen, cat, bdytail, bytcat);
      temp32alen = scale_expansion_zeroelim(bytcatlen, bytcat, 2.0 * bdy, temp32a);
      temp48len = fast_expansion_sum_zeroelim(temp16alen, temp16a, temp32alen, temp32a, temp48);
      finlength = fast_expansion_sum_zeroelim(finlength, finnow, temp48len, temp48, finother);
      finswap = finnow;
      finnow = finother;
      finother = finswap;

      temp32alen = scale_expansion_zeroelim(bytcatlen, bytcat, bdytail, temp32a);
      bytcattlen = scale_expansion_zeroelim(cattlen, catt, bdytail, bytcatt);
      temp16alen = scale_expansion_zeroelim(bytcattlen, bytcatt, 2.0 * bdy, temp16a);
      temp16blen = scale_expansion_zeroelim(bytcattlen, bytcatt, bdytail, temp16b);
      temp32blen = fast_expansion_sum_zeroelim(temp16alen, temp16a, temp16blen, temp16b, temp32b);
      temp64len = fast_expansion_sum_zeroelim(temp32alen, temp32a, temp32blen, temp32b, temp64);
      finlength = fast_expansion_sum_zeroelim(finlength, finnow, temp64len, temp64, finother);
      finswap = finnow;
      finnow = finother;
      finother = finswap;
    }
  }
  if ((cdxtail != 0.0) || (cdytail != 0.0)) {
    if ((adxtail != 0.0) || (adytail != 0.0) || (bdxtail != 0.0) || (bdytail != 0.0)) {
      Two_Product(adxtail, bdy, ti1, ti0);
      Two_Product(adx, bdytail, tj1, tj0);
      Two_Two_Sum(ti1, ti0, tj1, tj0, u3, u[2], u[1], u[0]);
      u[3] = u3;
      negate = -ady;
      Two_Product(bdxtail, negate, ti1, ti0);
      negate = -adytail;
      Two_Product(bdx, negate, tj1, tj0);
      Two_Two_Sum(ti1, ti0, tj1, tj0, v3, v[2], v[1], v[0]);
      v[3] = v3;
      abtlen = fast_expansion_sum_zeroelim(4, u, 4, v, abt);

      Two_Product(adxtail, bdytail, ti1, ti0);
      Two_Product(bdxtail, adytail, tj1, tj0);
      Two_Two_Diff(ti1, ti0, tj1, tj0, abtt3, abtt[2], abtt[1], abtt[0]);
      abtt[3] = abtt3;
      abttlen = 4;
    }
    else {
      abt[0] = 0.0;
      abtlen = 1;
      abtt[0] = 0.0;
      abttlen = 1;
    }

    if (cdxtail != 0.0) {
      temp16alen = scale_expansion_zeroelim(cxtablen, cxtab, cdxtail, temp16a);
      cxtabtlen = scale_expansion_zeroelim(abtlen, abt, cdxtail, cxtabt);
      temp32alen = scale_expansion_zeroelim(cxtabtlen, cxtabt, 2.0 * cdx, temp32a);
      temp48len = fast_expansion_sum_zeroelim(temp16alen, temp16a, temp32alen, temp32a, temp48);
      finlength = fast_expansion_sum_zeroelim(finlength, finnow, temp48len, temp48, finother);
      finswap = finnow;
      finnow = finother;
      finother = finswap;
      if (adytail != 0.0) {
        temp8len = scale_expansion_zeroelim(4, bb, cdxtail, temp8);
        temp16alen = scale_expansion_zeroelim(temp8len, temp8, adytail, temp16a);
        finlength = fast_expansion_sum_zeroelim(finlength, finnow, temp16alen, temp16a, finother);
        finswap = finnow;
        finnow = finother;
        finother = finswap;
      }
      if (bdytail != 0.0) {
        temp8len = scale_expansion_zeroelim(4, aa, -cdxtail, temp8);
        temp16alen = scale_expansion_zeroelim(temp8len, temp8, bdytail, temp16a);
        finlength = fast_expansion_sum_zeroelim(finlength, finnow, temp16alen, temp16a, finother);
        finswap = finnow;
        finnow = finother;
        finother = finswap;
      }

      temp32alen = scale_expansion_zeroelim(cxtabtlen, cxtabt, cdxtail, temp32a);
      cxtabttlen = scale_expansion_zeroelim(abttlen, abtt, cdxtail, cxtabtt);
      temp16alen = scale_expansion_zeroelim(cxtabttlen, cxtabtt, 2.0 * cdx, temp16a);
      temp16blen = scale_expansion_zeroelim(cxtabttlen, cxtabtt, cdxtail, temp16b);
      temp32blen = fast_expansion_sum_zeroelim(temp16alen, temp16a, temp16blen, temp16b, temp32b);
      temp64len = fast_expansion_sum_zeroelim(temp32alen, temp32a, temp32blen, temp32b, temp64);
      finlength = fast_expansion_sum_zeroelim(finlength, finnow, temp64len, temp64, finother);
      finswap = finnow;
      finnow = finother;
      finother = finswap;
    }
    if (cdytail != 0.0) {
      temp16alen = scale_expansion_zeroelim(cytablen, cytab, cdytail, temp16a);
      cytabtlen = scale_expansion_zeroelim(abtlen, abt, cdytail, cytabt);
      temp32alen = scale_expansion_zeroelim(cytabtlen, cytabt, 2.0 * cdy, temp32a);
      temp48len = fast_expansion_sum_zeroelim(temp16alen, temp16a, temp32alen, temp32a, temp48);
      finlength = fast_expansion_sum_zeroelim(finlength, finnow, temp48len, temp48, finother);
      finswap = finnow;
      finnow = finother;
      finother = finswap;

      temp32alen = scale_expansion_zeroelim(cytabtlen, cytabt, cdytail, temp32a);
      cytabttlen = scale_expansion_zeroelim(abttlen, abtt, cdytail, cytabtt);
      temp16alen = scale_expansion_zeroelim(cytabttlen, cytabtt, 2.0 * cdy, temp16a);
      temp16blen = scale_expansion_zeroelim(cytabttlen, cytabtt, cdytail, temp16b);
      temp32blen = fast_expansion_sum_zeroelim(temp16alen, temp16a, temp16blen, temp16b, temp32b);
      temp64len = fast_expansion_sum_zeroelim(temp32alen, temp32a, temp32blen, temp32b, temp64);
      finlength = fast_expansion_sum_zeroelim(finlength, finnow, temp64len, temp64, finother);
      finswap = finnow;
      finnow = finother;
      finother = finswap;
    }
  }

  return finnow[finlength - 1];
}

static double incircle(const double *pa, const double *pb, const double *pc, const double *pd)
{
  double adx, bdx, cdx, ady, bdy, cdy;
  double bdxcdy, cdxbdy, cdxady, adxcdy, adxbdy, bdxady;
  double alift, blift, clift;
  double det;
  double permanent, errbound;

  adx = pa[0] - pd[0];
  bdx = pb[0] - pd[0];
  cdx = pc[0] - pd[0];
  ady = pa[1] - pd[1];
  bdy = pb[1] - pd[1];
  cdy = pc[1] - pd[1];

  bdxcdy = bdx * cdy;
  cdxbdy = cdx * bdy;
  alift = adx * adx + ady * ady;

  cdxady = cdx * ady;
  adxcdy = adx * cdy;
  blift = bdx * bdx + bdy * bdy;

  adxbdy = adx * bdy;
  bdxady = bdx * ady;
  clift = cdx * cdx + cdy * cdy;

  det = alift * (bdxcdy - cdxbdy) + blift * (cdxady - adxcdy) + clift * (adxbdy - bdxady);

  permanent = (Absolute(bdxcdy) + Absolute(cdxbdy)) * alift +
              (Absolute(cdxady) + Absolute(adxcdy)) * blift +
              (Absolute(adxbdy) + Absolute(bdxady)) * clift;
  errbound = iccerrboundA * permanent;
  if ((det > errbound) || (-det > errbound)) {
    return det;
  }

  return incircleadapt(pa, pb, pc, pd, permanent);
}
