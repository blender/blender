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
 *
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup edmesh
 *
 * Interactive editmesh knife tool.
 */

#ifdef _MSC_VER
#  define _USE_MATH_DEFINES
#endif

#include "MEM_guardedalloc.h"

#include "BLI_alloca.h"
#include "BLI_array.h"
#include "BLI_linklist.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_memarena.h"
#include "BLI_smallhash.h"
#include "BLI_string.h"

#include "BLT_translation.h"

#include "BKE_bvhutils.h"
#include "BKE_context.h"
#include "BKE_editmesh.h"
#include "BKE_editmesh_bvh.h"
#include "BKE_report.h"

#include "GPU_immediate.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_space_api.h"
#include "ED_view3d.h"

#include "WM_api.h"
#include "WM_types.h"

#include "DNA_object_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "mesh_intern.h" /* own include */

/* detect isolated holes and fill them */
#define USE_NET_ISLAND_CONNECT

#define KMAXDIST 10 /* max mouse distance from edge before not detecting it */

/* WARNING: knife float precision is fragile:
 * be careful before making changes here see: (T43229, T42864, T42459, T41164).
 */
#define KNIFE_FLT_EPS 0.00001f
#define KNIFE_FLT_EPS_SQUARED (KNIFE_FLT_EPS * KNIFE_FLT_EPS)
#define KNIFE_FLT_EPSBIG 0.0005f

#define KNIFE_FLT_EPS_PX_VERT 0.5f
#define KNIFE_FLT_EPS_PX_EDGE 0.05f
#define KNIFE_FLT_EPS_PX_FACE 0.05f

typedef struct KnifeColors {
  uchar line[3];
  uchar edge[3];
  uchar curpoint[3];
  uchar curpoint_a[4];
  uchar point[3];
  uchar point_a[4];
} KnifeColors;

/* knifetool operator */
typedef struct KnifeVert {
  BMVert *v; /* non-NULL if this is an original vert */
  ListBase edges;
  ListBase faces;

  float co[3], cageco[3], sco[2]; /* sco is screen coordinates for cageco */
  bool is_face, in_space;
  bool is_cut; /* along a cut created by user input (will draw too) */
} KnifeVert;

typedef struct Ref {
  struct Ref *next, *prev;
  void *ref;
} Ref;

typedef struct KnifeEdge {
  KnifeVert *v1, *v2;
  BMFace *basef; /* face to restrict face fill to */
  ListBase faces;

  BMEdge *e /* , *e_old */; /* non-NULL if this is an original edge */
  bool is_cut;              /* along a cut created by user input (will draw too) */
} KnifeEdge;

typedef struct KnifeLineHit {
  float hit[3], cagehit[3];
  float schit[2]; /* screen coordinates for cagehit */
  float l;        /* lambda along cut line */
  float perc;     /* lambda along hit line */
  float m;        /* depth front-to-back */

  /* Exactly one of kfe, v, or f should be non-NULL,
   * saying whether cut line crosses and edge,
   * is snapped to a vert, or is in the middle of some face. */
  KnifeEdge *kfe;
  KnifeVert *v;
  BMFace *f;
} KnifeLineHit;

typedef struct KnifePosData {
  float co[3];
  float cage[3];

  /* At most one of vert, edge, or bmface should be non-NULL,
   * saying whether the point is snapped to a vertex, edge, or in a face.
   * If none are set, this point is in space and is_space should be true. */
  KnifeVert *vert;
  KnifeEdge *edge;
  BMFace *bmface;
  bool is_space;

  float mval[2]; /* mouse screen position (may be non-integral if snapped to something) */
} KnifePosData;

/* struct for properties used while drawing */
typedef struct KnifeTool_OpData {
  ARegion *region;   /* region that knifetool was activated in */
  void *draw_handle; /* for drawing preview loop */
  ViewContext vc;    /* note: _don't_ use 'mval', instead use the one we define below */
  float mval[2];     /* mouse value with snapping applied */
  // bContext *C;

  Scene *scene;
  Object *ob;
  BMEditMesh *em;

  MemArena *arena;

  /* reused for edge-net filling */
  struct {
    /* cleared each use */
    GSet *edge_visit;
#ifdef USE_NET_ISLAND_CONNECT
    MemArena *arena;
#endif
  } edgenet;

  GHash *origvertmap;
  GHash *origedgemap;
  GHash *kedgefacemap;
  GHash *facetrimap;

  BMBVHTree *bmbvh;

  BLI_mempool *kverts;
  BLI_mempool *kedges;

  float vthresh;
  float ethresh;

  /* used for drag-cutting */
  KnifeLineHit *linehits;
  int totlinehit;

  /* Data for mouse-position-derived data */
  KnifePosData curr; /* current point under the cursor */
  KnifePosData prev; /* last added cut (a line draws from the cursor to this) */
  KnifePosData init; /* the first point in the cut-list, used for closing the loop */

  int totkedge, totkvert;

  BLI_mempool *refs;

  float projmat[4][4];
  float projmat_inv[4][4];
  /* vector along view z axis (object space, normalized) */
  float proj_zaxis[3];

  KnifeColors colors;

  /* run by the UI or not */
  bool is_interactive;

  /* operatpr options */
  bool cut_through;   /* preference, can be modified at runtime (that feature may go) */
  bool only_select;   /* set on initialization */
  bool select_result; /* set on initialization */

  bool is_ortho;
  float ortho_extent;
  float ortho_extent_center[3];

  float clipsta, clipend;

  enum { MODE_IDLE, MODE_DRAGGING, MODE_CONNECT, MODE_PANNING } mode;
  bool is_drag_hold;

  int prevmode;
  bool snap_midpoints;
  bool ignore_edge_snapping;
  bool ignore_vert_snapping;

  /* use to check if we're currently dragging an angle snapped line */
  bool is_angle_snapping;
  bool angle_snapping;
  float angle;

  const float (*cagecos)[3];
} KnifeTool_OpData;

enum {
  KNF_MODAL_CANCEL = 1,
  KNF_MODAL_CONFIRM,
  KNF_MODAL_MIDPOINT_ON,
  KNF_MODAL_MIDPOINT_OFF,
  KNF_MODAL_NEW_CUT,
  KNF_MODEL_IGNORE_SNAP_ON,
  KNF_MODEL_IGNORE_SNAP_OFF,
  KNF_MODAL_ADD_CUT,
  KNF_MODAL_ANGLE_SNAP_TOGGLE,
  KNF_MODAL_CUT_THROUGH_TOGGLE,
  KNF_MODAL_PANNING,
  KNF_MODAL_ADD_CUT_CLOSED,
};

static ListBase *knife_get_face_kedges(KnifeTool_OpData *kcd, BMFace *f);

static void knife_input_ray_segment(KnifeTool_OpData *kcd,
                                    const float mval[2],
                                    const float ofs,
                                    float r_origin[3],
                                    float r_dest[3]);

static bool knife_verts_edge_in_face(KnifeVert *v1, KnifeVert *v2, BMFace *f);

static void knifetool_free_bmbvh(KnifeTool_OpData *kcd);

static int knifetool_modal(bContext *C, wmOperator *op, const wmEvent *event);

static void knife_update_header(bContext *C, wmOperator *op, KnifeTool_OpData *kcd)
{
  char header[UI_MAX_DRAW_STR];
  char buf[UI_MAX_DRAW_STR];

  char *p = buf;
  int available_len = sizeof(buf);

#define WM_MODALKEY(_id) \
  WM_modalkeymap_operator_items_to_string_buf( \
      op->type, (_id), true, UI_MAX_SHORTCUT_STR, &available_len, &p)

  BLI_snprintf(header,
               sizeof(header),
               TIP_("%s: confirm, %s: cancel, "
                    "%s: start/define cut, %s: close cut, %s: new cut, "
                    "%s: midpoint snap (%s), %s: ignore snap (%s), "
                    "%s: angle constraint (%s), %s: cut through (%s), "
                    "%s: panning"),
               WM_MODALKEY(KNF_MODAL_CONFIRM),
               WM_MODALKEY(KNF_MODAL_CANCEL),
               WM_MODALKEY(KNF_MODAL_ADD_CUT),
               WM_MODALKEY(KNF_MODAL_ADD_CUT_CLOSED),
               WM_MODALKEY(KNF_MODAL_NEW_CUT),
               WM_MODALKEY(KNF_MODAL_MIDPOINT_ON),
               WM_bool_as_string(kcd->snap_midpoints),
               WM_MODALKEY(KNF_MODEL_IGNORE_SNAP_ON),
               WM_bool_as_string(kcd->ignore_edge_snapping),
               WM_MODALKEY(KNF_MODAL_ANGLE_SNAP_TOGGLE),
               WM_bool_as_string(kcd->angle_snapping),
               WM_MODALKEY(KNF_MODAL_CUT_THROUGH_TOGGLE),
               WM_bool_as_string(kcd->cut_through),
               WM_MODALKEY(KNF_MODAL_PANNING));

#undef WM_MODALKEY

  ED_workspace_status_text(C, header);
}

static void knife_project_v2(const KnifeTool_OpData *kcd, const float co[3], float sco[2])
{
  ED_view3d_project_float_v2_m4(kcd->region, co, sco, (float(*)[4])kcd->projmat);
}

/* use when lambda is in screen-space */
static void knife_interp_v3_v3v3(const KnifeTool_OpData *kcd,
                                 float r_co[3],
                                 const float v1[3],
                                 const float v2[3],
                                 float lambda_ss)
{
  if (kcd->is_ortho) {
    interp_v3_v3v3(r_co, v1, v2, lambda_ss);
  }
  else {
    /* transform into screen-space, interp, then transform back */
    float v1_ss[3], v2_ss[3];

    mul_v3_project_m4_v3(v1_ss, (float(*)[4])kcd->projmat, v1);
    mul_v3_project_m4_v3(v2_ss, (float(*)[4])kcd->projmat, v2);

    interp_v3_v3v3(r_co, v1_ss, v2_ss, lambda_ss);

    mul_project_m4_v3((float(*)[4])kcd->projmat_inv, r_co);
  }
}

static void knife_pos_data_clear(KnifePosData *kpd)
{
  zero_v3(kpd->co);
  zero_v3(kpd->cage);
  kpd->vert = NULL;
  kpd->edge = NULL;
  kpd->bmface = NULL;
  zero_v2(kpd->mval);
}

static ListBase *knife_empty_list(KnifeTool_OpData *kcd)
{
  ListBase *lst;

  lst = BLI_memarena_alloc(kcd->arena, sizeof(ListBase));
  BLI_listbase_clear(lst);
  return lst;
}

static void knife_append_list(KnifeTool_OpData *kcd, ListBase *lst, void *elem)
{
  Ref *ref;

  ref = BLI_mempool_calloc(kcd->refs);
  ref->ref = elem;
  BLI_addtail(lst, ref);
}

static Ref *find_ref(ListBase *lb, void *ref)
{
  Ref *ref1;

  for (ref1 = lb->first; ref1; ref1 = ref1->next) {
    if (ref1->ref == ref) {
      return ref1;
    }
  }

  return NULL;
}

static void knife_append_list_no_dup(KnifeTool_OpData *kcd, ListBase *lst, void *elem)
{
  if (!find_ref(lst, elem)) {
    knife_append_list(kcd, lst, elem);
  }
}

static KnifeEdge *new_knife_edge(KnifeTool_OpData *kcd)
{
  kcd->totkedge++;
  return BLI_mempool_calloc(kcd->kedges);
}

static void knife_add_to_vert_edges(KnifeTool_OpData *kcd, KnifeEdge *kfe)
{
  knife_append_list(kcd, &kfe->v1->edges, kfe);
  knife_append_list(kcd, &kfe->v2->edges, kfe);
}

/* Add faces of an edge to a KnifeVert's faces list.  No checks for dups. */
static void knife_add_edge_faces_to_vert(KnifeTool_OpData *kcd, KnifeVert *kfv, BMEdge *e)
{
  BMIter bmiter;
  BMFace *f;

  BM_ITER_ELEM (f, &bmiter, e, BM_FACES_OF_EDGE) {
    knife_append_list(kcd, &kfv->faces, f);
  }
}

/* Find a face in common in the two faces lists.
 * If more than one, return the first; if none, return NULL */
static BMFace *knife_find_common_face(ListBase *faces1, ListBase *faces2)
{
  Ref *ref1, *ref2;

  for (ref1 = faces1->first; ref1; ref1 = ref1->next) {
    for (ref2 = faces2->first; ref2; ref2 = ref2->next) {
      if (ref1->ref == ref2->ref) {
        return (BMFace *)(ref1->ref);
      }
    }
  }
  return NULL;
}

static KnifeVert *new_knife_vert(KnifeTool_OpData *kcd, const float co[3], const float cageco[3])
{
  KnifeVert *kfv = BLI_mempool_calloc(kcd->kverts);

  kcd->totkvert++;

  copy_v3_v3(kfv->co, co);
  copy_v3_v3(kfv->cageco, cageco);

  knife_project_v2(kcd, kfv->cageco, kfv->sco);

  return kfv;
}

/* get a KnifeVert wrapper for an existing BMVert */
static KnifeVert *get_bm_knife_vert(KnifeTool_OpData *kcd, BMVert *v)
{
  KnifeVert *kfv = BLI_ghash_lookup(kcd->origvertmap, v);
  const float *cageco;

  if (!kfv) {
    BMIter bmiter;
    BMFace *f;

    if (BM_elem_index_get(v) >= 0) {
      cageco = kcd->cagecos[BM_elem_index_get(v)];
    }
    else {
      cageco = v->co;
    }
    kfv = new_knife_vert(kcd, v->co, cageco);
    kfv->v = v;
    BLI_ghash_insert(kcd->origvertmap, v, kfv);
    BM_ITER_ELEM (f, &bmiter, v, BM_FACES_OF_VERT) {
      knife_append_list(kcd, &kfv->faces, f);
    }
  }

  return kfv;
}

/* get a KnifeEdge wrapper for an existing BMEdge */
static KnifeEdge *get_bm_knife_edge(KnifeTool_OpData *kcd, BMEdge *e)
{
  KnifeEdge *kfe = BLI_ghash_lookup(kcd->origedgemap, e);
  if (!kfe) {
    BMIter bmiter;
    BMFace *f;

    kfe = new_knife_edge(kcd);
    kfe->e = e;
    kfe->v1 = get_bm_knife_vert(kcd, e->v1);
    kfe->v2 = get_bm_knife_vert(kcd, e->v2);

    knife_add_to_vert_edges(kcd, kfe);

    BLI_ghash_insert(kcd->origedgemap, e, kfe);

    BM_ITER_ELEM (f, &bmiter, e, BM_FACES_OF_EDGE) {
      knife_append_list(kcd, &kfe->faces, f);
    }
  }

  return kfe;
}

/* Record the index in kcd->em->looptris of first looptri triple for a given face,
 * given an index for some triple in that array.
 * This assumes that all of the triangles for a given face are contiguous
 * in that array (as they are by the current tessellation routines).
 * Actually store index + 1 in the hash, because 0 looks like "no entry"
 * to hash lookup routine; will reverse this in the get routine.
 * Doing this lazily rather than all at once for all faces.
 */
static void set_lowest_face_tri(KnifeTool_OpData *kcd, BMFace *f, int index)
{
  int i;

  if (BLI_ghash_lookup(kcd->facetrimap, f)) {
    return;
  }

  BLI_assert(index >= 0 && index < kcd->em->tottri);
  BLI_assert(kcd->em->looptris[index][0]->f == f);
  for (i = index - 1; i >= 0; i--) {
    if (kcd->em->looptris[i][0]->f != f) {
      i++;
      break;
    }
  }
  if (i == -1) {
    i++;
  }

  BLI_ghash_insert(kcd->facetrimap, f, POINTER_FROM_INT(i + 1));
}

/* This should only be called for faces that have had a lowest face tri set by previous function */
static int get_lowest_face_tri(KnifeTool_OpData *kcd, BMFace *f)
{
  int ans;

  ans = POINTER_AS_INT(BLI_ghash_lookup(kcd->facetrimap, f));
  BLI_assert(ans != 0);
  return ans - 1;
}

/* User has just clicked for first time or first time after a restart (E key).
 * Copy the current position data into prev. */
static void knife_start_cut(KnifeTool_OpData *kcd)
{
  kcd->prev = kcd->curr;
  kcd->curr.is_space = 0; /*TODO: why do we do this? */

  if (kcd->prev.vert == NULL && kcd->prev.edge == NULL) {
    float origin[3], origin_ofs[3];
    float ofs_local[3];

    negate_v3_v3(ofs_local, kcd->vc.rv3d->ofs);
    invert_m4_m4(kcd->ob->imat, kcd->ob->obmat);
    mul_m4_v3(kcd->ob->imat, ofs_local);

    knife_input_ray_segment(kcd, kcd->curr.mval, 1.0f, origin, origin_ofs);

    if (!isect_line_plane_v3(kcd->prev.cage, origin, origin_ofs, ofs_local, kcd->proj_zaxis)) {
      zero_v3(kcd->prev.cage);
    }

    copy_v3_v3(kcd->prev.co, kcd->prev.cage); /*TODO: do we need this? */
    copy_v3_v3(kcd->curr.cage, kcd->prev.cage);
    copy_v3_v3(kcd->curr.co, kcd->prev.co);
  }
}

static ListBase *knife_get_face_kedges(KnifeTool_OpData *kcd, BMFace *f)
{
  ListBase *lst = BLI_ghash_lookup(kcd->kedgefacemap, f);

  if (!lst) {
    BMIter bmiter;
    BMEdge *e;

    lst = knife_empty_list(kcd);

    BM_ITER_ELEM (e, &bmiter, f, BM_EDGES_OF_FACE) {
      knife_append_list(kcd, lst, get_bm_knife_edge(kcd, e));
    }

    BLI_ghash_insert(kcd->kedgefacemap, f, lst);
  }

  return lst;
}

static void knife_edge_append_face(KnifeTool_OpData *kcd, KnifeEdge *kfe, BMFace *f)
{
  knife_append_list(kcd, knife_get_face_kedges(kcd, f), kfe);
  knife_append_list(kcd, &kfe->faces, f);
}

static KnifeVert *knife_split_edge(KnifeTool_OpData *kcd,
                                   KnifeEdge *kfe,
                                   const float co[3],
                                   const float cageco[3],
                                   KnifeEdge **r_kfe)
{
  KnifeEdge *newkfe = new_knife_edge(kcd);
  Ref *ref;
  BMFace *f;

  newkfe->v1 = kfe->v1;
  newkfe->v2 = new_knife_vert(kcd, co, cageco);
  newkfe->v2->is_cut = true;
  if (kfe->e) {
    knife_add_edge_faces_to_vert(kcd, newkfe->v2, kfe->e);
  }
  else {
    /* kfe cuts across an existing face.
     * If v1 and v2 are in multiple faces together (e.g., if they
     * are in doubled polys) then this arbitrarily chooses one of them */
    f = knife_find_common_face(&kfe->v1->faces, &kfe->v2->faces);
    if (f) {
      knife_append_list(kcd, &newkfe->v2->faces, f);
    }
  }
  newkfe->basef = kfe->basef;

  ref = find_ref(&kfe->v1->edges, kfe);
  BLI_remlink(&kfe->v1->edges, ref);

  kfe->v1 = newkfe->v2;
  BLI_addtail(&kfe->v1->edges, ref);

  for (ref = kfe->faces.first; ref; ref = ref->next) {
    knife_edge_append_face(kcd, newkfe, ref->ref);
  }

  knife_add_to_vert_edges(kcd, newkfe);

  newkfe->is_cut = kfe->is_cut;
  newkfe->e = kfe->e;

  *r_kfe = newkfe;

  return newkfe->v2;
}

static void linehit_to_knifepos(KnifePosData *kpos, KnifeLineHit *lh)
{
  kpos->bmface = lh->f;
  kpos->vert = lh->v;
  kpos->edge = lh->kfe;
  copy_v3_v3(kpos->cage, lh->cagehit);
  copy_v3_v3(kpos->co, lh->hit);
  copy_v2_v2(kpos->mval, lh->schit);
}

/* primary key: lambda along cut
 * secondary key: lambda along depth
 * tertiary key: pointer comparisons of verts if both snapped to verts
 */
static int linehit_compare(const void *vlh1, const void *vlh2)
{
  const KnifeLineHit *lh1 = vlh1;
  const KnifeLineHit *lh2 = vlh2;

  if (lh1->l < lh2->l) {
    return -1;
  }
  else if (lh1->l > lh2->l) {
    return 1;
  }
  else {
    if (lh1->m < lh2->m) {
      return -1;
    }
    else if (lh1->m > lh2->m) {
      return 1;
    }
    else {
      if (lh1->v < lh2->v) {
        return -1;
      }
      else if (lh1->v > lh2->v) {
        return 1;
      }
      else {
        return 0;
      }
    }
  }
}

/*
 * Sort linehits by distance along cut line, and secondarily from
 * front to back (from eye), and tertiarily by snap vertex,
 * and remove any duplicates.
 */
static void prepare_linehits_for_cut(KnifeTool_OpData *kcd)
{
  KnifeLineHit *linehits, *lhi, *lhj;
  int i, j, n;
  bool is_double = false;

  n = kcd->totlinehit;
  linehits = kcd->linehits;
  if (n == 0) {
    return;
  }

  qsort(linehits, n, sizeof(KnifeLineHit), linehit_compare);

  /* Remove any edge hits that are preceded or followed
   * by a vertex hit that is very near. Mark such edge hits using
   * l == -1 and then do another pass to actually remove.
   * Also remove all but one of a series of vertex hits for the same vertex. */
  for (i = 0; i < n; i++) {
    lhi = &linehits[i];
    if (lhi->v) {
      for (j = i - 1; j >= 0; j--) {
        lhj = &linehits[j];
        if (!lhj->kfe || fabsf(lhi->l - lhj->l) > KNIFE_FLT_EPSBIG ||
            fabsf(lhi->m - lhj->m) > KNIFE_FLT_EPSBIG) {
          break;
        }

        if (lhi->kfe == lhj->kfe) {
          lhj->l = -1.0f;
          is_double = true;
        }
      }
      for (j = i + 1; j < n; j++) {
        lhj = &linehits[j];
        if (fabsf(lhi->l - lhj->l) > KNIFE_FLT_EPSBIG ||
            fabsf(lhi->m - lhj->m) > KNIFE_FLT_EPSBIG) {
          break;
        }
        if ((lhj->kfe && (lhi->kfe == lhj->kfe)) || (lhi->v == lhj->v)) {
          lhj->l = -1.0f;
          is_double = true;
        }
      }
    }
  }

  if (is_double) {
    /* delete-in-place loop: copying from pos j to pos i+1 */
    i = 0;
    j = 1;
    while (j < n) {
      lhi = &linehits[i];
      lhj = &linehits[j];
      if (lhj->l == -1.0f) {
        j++; /* skip copying this one */
      }
      else {
        /* copy unless a no-op */
        if (lhi->l == -1.0f) {
          /* could happen if linehits[0] is being deleted */
          memcpy(&linehits[i], &linehits[j], sizeof(KnifeLineHit));
        }
        else {
          if (i + 1 != j) {
            memcpy(&linehits[i + 1], &linehits[j], sizeof(KnifeLineHit));
          }
          i++;
        }
        j++;
      }
    }
    kcd->totlinehit = i + 1;
  }
}

/* Add hit to list of hits in facehits[f], where facehits is a map, if not already there */
static void add_hit_to_facehits(KnifeTool_OpData *kcd,
                                GHash *facehits,
                                BMFace *f,
                                KnifeLineHit *hit)
{
  ListBase *lst = BLI_ghash_lookup(facehits, f);

  if (!lst) {
    lst = knife_empty_list(kcd);
    BLI_ghash_insert(facehits, f, lst);
  }
  knife_append_list_no_dup(kcd, lst, hit);
}

/**
 * special purpose function, if the linehit is connected to a real edge/vert
 * return true if \a co is outside the face.
 */
static bool knife_add_single_cut__is_linehit_outside_face(BMFace *f,
                                                          const KnifeLineHit *lh,
                                                          const float co[3])
{

  if (lh->v && lh->v->v) {
    BMLoop *l; /* side-of-loop */
    if ((l = BM_face_vert_share_loop(f, lh->v->v)) &&
        (BM_loop_point_side_of_loop_test(l, co) < 0.0f)) {
      return true;
    }
  }
  else if ((lh->kfe && lh->kfe->e)) {
    BMLoop *l; /* side-of-edge */
    if ((l = BM_face_edge_share_loop(f, lh->kfe->e)) &&
        (BM_loop_point_side_of_edge_test(l, co) < 0.0f)) {
      return true;
    }
  }

  return false;
}

static void knife_add_single_cut(KnifeTool_OpData *kcd,
                                 KnifeLineHit *lh1,
                                 KnifeLineHit *lh2,
                                 BMFace *f)
{
  KnifeEdge *kfe, *kfe2;
  BMEdge *e_base;

  if ((lh1->v && lh1->v == lh2->v) || (lh1->kfe && lh1->kfe == lh2->kfe)) {
    return;
  }

  /* if the cut is on an edge, just tag that its a cut and return */
  if ((lh1->v && lh2->v) && (lh1->v->v && lh2->v && lh2->v->v) &&
      (e_base = BM_edge_exists(lh1->v->v, lh2->v->v))) {
    kfe = get_bm_knife_edge(kcd, e_base);
    kfe->is_cut = true;
    kfe->e = e_base;
    return;
  }
  else {
    if (knife_add_single_cut__is_linehit_outside_face(f, lh1, lh2->hit) ||
        knife_add_single_cut__is_linehit_outside_face(f, lh2, lh1->hit)) {
      return;
    }
  }

  /* Check if edge actually lies within face (might not, if this face is concave) */
  if ((lh1->v && !lh1->kfe) && (lh2->v && !lh2->kfe)) {
    if (!knife_verts_edge_in_face(lh1->v, lh2->v, f)) {
      return;
    }
  }

  kfe = new_knife_edge(kcd);
  kfe->is_cut = true;
  kfe->basef = f;

  if (lh1->v) {
    kfe->v1 = lh1->v;
  }
  else if (lh1->kfe) {
    kfe->v1 = knife_split_edge(kcd, lh1->kfe, lh1->hit, lh1->cagehit, &kfe2);
    lh1->v = kfe->v1; /* record the KnifeVert for this hit  */
  }
  else {
    BLI_assert(lh1->f);
    kfe->v1 = new_knife_vert(kcd, lh1->hit, lh1->cagehit);
    kfe->v1->is_cut = true;
    kfe->v1->is_face = true;
    knife_append_list(kcd, &kfe->v1->faces, lh1->f);
    lh1->v = kfe->v1; /* record the KnifeVert for this hit */
  }

  if (lh2->v) {
    kfe->v2 = lh2->v;
  }
  else if (lh2->kfe) {
    kfe->v2 = knife_split_edge(kcd, lh2->kfe, lh2->hit, lh2->cagehit, &kfe2);
    lh2->v = kfe->v2; /* future uses of lh2 won't split again */
  }
  else {
    BLI_assert(lh2->f);
    kfe->v2 = new_knife_vert(kcd, lh2->hit, lh2->cagehit);
    kfe->v2->is_cut = true;
    kfe->v2->is_face = true;
    knife_append_list(kcd, &kfe->v2->faces, lh2->f);
    lh2->v = kfe->v2; /* record the KnifeVert for this hit */
  }

  knife_add_to_vert_edges(kcd, kfe);

  /* TODO: check if this is ever needed */
  if (kfe->basef && !find_ref(&kfe->faces, kfe->basef)) {
    knife_edge_append_face(kcd, kfe, kfe->basef);
  }
}

/* Given a list of KnifeLineHits for one face, sorted by l
 * and then by m, make the required KnifeVerts and
 * KnifeEdges.
 */
static void knife_cut_face(KnifeTool_OpData *kcd, BMFace *f, ListBase *hits)
{
  Ref *r;

  if (BLI_listbase_count_at_most(hits, 2) != 2) {
    return;
  }

  for (r = hits->first; r->next; r = r->next) {
    knife_add_single_cut(kcd, r->ref, r->next->ref, f);
  }
}

/* User has just left-clicked after the first time.
 * Add all knife cuts implied by line from prev to curr.
 * If that line crossed edges then kcd->linehits will be non-NULL.
 * Make all of the KnifeVerts and KnifeEdges implied by this cut.
 */
static void knife_add_cut(KnifeTool_OpData *kcd)
{
  int i;
  GHash *facehits;
  BMFace *f;
  Ref *r;
  GHashIterator giter;
  ListBase *lst;

  prepare_linehits_for_cut(kcd);
  if (kcd->totlinehit == 0) {
    if (kcd->is_drag_hold == false) {
      kcd->prev = kcd->curr;
    }
    return;
  }

  /* make facehits: map face -> list of linehits touching it */
  facehits = BLI_ghash_ptr_new("knife facehits");
  for (i = 0; i < kcd->totlinehit; i++) {
    KnifeLineHit *lh = &kcd->linehits[i];
    if (lh->f) {
      add_hit_to_facehits(kcd, facehits, lh->f, lh);
    }
    if (lh->v) {
      for (r = lh->v->faces.first; r; r = r->next) {
        add_hit_to_facehits(kcd, facehits, r->ref, lh);
      }
    }
    if (lh->kfe) {
      for (r = lh->kfe->faces.first; r; r = r->next) {
        add_hit_to_facehits(kcd, facehits, r->ref, lh);
      }
    }
  }

  /* Note: as following loop progresses, the 'v' fields of
   * the linehits will be filled in (as edges are split or
   * in-face verts are made), so it may be true that both
   * the v and the kfe or f fields will be non-NULL. */
  GHASH_ITER (giter, facehits) {
    f = (BMFace *)BLI_ghashIterator_getKey(&giter);
    lst = (ListBase *)BLI_ghashIterator_getValue(&giter);
    knife_cut_face(kcd, f, lst);
  }

  /* set up for next cut */
  kcd->prev = kcd->curr;

  if (kcd->prev.bmface) {
    /* was "in face" but now we have a KnifeVert it is snapped to */
    KnifeLineHit *lh = &kcd->linehits[kcd->totlinehit - 1];
    kcd->prev.vert = lh->v;
    kcd->prev.bmface = NULL;
  }

  if (kcd->is_drag_hold) {
    KnifeLineHit *lh = &kcd->linehits[kcd->totlinehit - 1];
    linehit_to_knifepos(&kcd->prev, lh);
  }

  BLI_ghash_free(facehits, NULL, NULL);
  MEM_freeN(kcd->linehits);
  kcd->linehits = NULL;
  kcd->totlinehit = 0;
}

static void knife_finish_cut(KnifeTool_OpData *kcd)
{
  if (kcd->linehits) {
    MEM_freeN(kcd->linehits);
    kcd->linehits = NULL;
    kcd->totlinehit = 0;
  }
}

static void knifetool_draw_angle_snapping(const KnifeTool_OpData *kcd)
{
  float v1[3], v2[3];
  float planes[4][4];

  planes_from_projmat(
      (float(*)[4])kcd->projmat, planes[2], planes[0], planes[3], planes[1], NULL, NULL);

  /* ray-cast all planes */
  {
    float ray_dir[3];
    float ray_hit_best[2][3] = {{UNPACK3(kcd->prev.cage)}, {UNPACK3(kcd->curr.cage)}};
    float lambda_best[2] = {-FLT_MAX, FLT_MAX};
    int i;

    /* we (sometimes) need the lines to be at the same depth before projecting */
#if 0
    sub_v3_v3v3(ray_dir, kcd->curr.cage, kcd->prev.cage);
#else
    {
      float curr_cage_adjust[3];
      float co_depth[3];

      copy_v3_v3(co_depth, kcd->prev.cage);
      mul_m4_v3(kcd->ob->obmat, co_depth);
      ED_view3d_win_to_3d(kcd->vc.v3d, kcd->region, co_depth, kcd->curr.mval, curr_cage_adjust);
      mul_m4_v3(kcd->ob->imat, curr_cage_adjust);

      sub_v3_v3v3(ray_dir, curr_cage_adjust, kcd->prev.cage);
    }
#endif

    for (i = 0; i < 4; i++) {
      float ray_hit[3];
      float lambda_test;
      if (isect_ray_plane_v3(kcd->prev.cage, ray_dir, planes[i], &lambda_test, false)) {
        madd_v3_v3v3fl(ray_hit, kcd->prev.cage, ray_dir, lambda_test);
        if (lambda_test < 0.0f) {
          if (lambda_test > lambda_best[0]) {
            copy_v3_v3(ray_hit_best[0], ray_hit);
            lambda_best[0] = lambda_test;
          }
        }
        else {
          if (lambda_test < lambda_best[1]) {
            copy_v3_v3(ray_hit_best[1], ray_hit);
            lambda_best[1] = lambda_test;
          }
        }
      }
    }

    copy_v3_v3(v1, ray_hit_best[0]);
    copy_v3_v3(v2, ray_hit_best[1]);
  }

  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  immUniformThemeColor3(TH_TRANSFORM);
  GPU_line_width(2.0);

  immBegin(GPU_PRIM_LINES, 2);
  immVertex3fv(pos, v1);
  immVertex3fv(pos, v2);
  immEnd();

  immUnbindProgram();
}

static void knife_init_colors(KnifeColors *colors)
{
  /* possible BMESH_TODO: add explicit themes or calculate these by
   * figuring out contrasting colors with grid / edges / verts
   * a la UI_make_axis_color */
  UI_GetThemeColorType3ubv(TH_NURB_VLINE, SPACE_VIEW3D, colors->line);
  UI_GetThemeColorType3ubv(TH_NURB_ULINE, SPACE_VIEW3D, colors->edge);
  UI_GetThemeColorType3ubv(TH_HANDLE_SEL_VECT, SPACE_VIEW3D, colors->curpoint);
  UI_GetThemeColorType3ubv(TH_HANDLE_SEL_VECT, SPACE_VIEW3D, colors->curpoint_a);
  colors->curpoint_a[3] = 102;
  UI_GetThemeColorType3ubv(TH_ACTIVE_SPLINE, SPACE_VIEW3D, colors->point);
  UI_GetThemeColorType3ubv(TH_ACTIVE_SPLINE, SPACE_VIEW3D, colors->point_a);
  colors->point_a[3] = 102;
}

/* modal loop selection drawing callback */
static void knifetool_draw(const bContext *UNUSED(C), ARegion *UNUSED(region), void *arg)
{
  const KnifeTool_OpData *kcd = arg;
  GPU_depth_test(false);

  glPolygonOffset(1.0f, 1.0f);

  GPU_matrix_push();
  GPU_matrix_mul(kcd->ob->obmat);

  if (kcd->mode == MODE_DRAGGING && kcd->is_angle_snapping) {
    knifetool_draw_angle_snapping(kcd);
  }

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  if (kcd->mode == MODE_DRAGGING) {
    immUniformColor3ubv(kcd->colors.line);
    GPU_line_width(2.0);

    immBegin(GPU_PRIM_LINES, 2);
    immVertex3fv(pos, kcd->prev.cage);
    immVertex3fv(pos, kcd->curr.cage);
    immEnd();
  }

  if (kcd->prev.vert) {
    immUniformColor3ubv(kcd->colors.point);
    GPU_point_size(11);

    immBegin(GPU_PRIM_POINTS, 1);
    immVertex3fv(pos, kcd->prev.cage);
    immEnd();
  }

  if (kcd->prev.bmface) {
    immUniformColor3ubv(kcd->colors.curpoint);
    GPU_point_size(9);

    immBegin(GPU_PRIM_POINTS, 1);
    immVertex3fv(pos, kcd->prev.cage);
    immEnd();
  }

  if (kcd->curr.edge) {
    immUniformColor3ubv(kcd->colors.edge);
    GPU_line_width(2.0);

    immBegin(GPU_PRIM_LINES, 2);
    immVertex3fv(pos, kcd->curr.edge->v1->cageco);
    immVertex3fv(pos, kcd->curr.edge->v2->cageco);
    immEnd();
  }
  else if (kcd->curr.vert) {
    immUniformColor3ubv(kcd->colors.point);
    GPU_point_size(11);

    immBegin(GPU_PRIM_POINTS, 1);
    immVertex3fv(pos, kcd->curr.cage);
    immEnd();
  }

  if (kcd->curr.bmface) {
    immUniformColor3ubv(kcd->colors.curpoint);
    GPU_point_size(9);

    immBegin(GPU_PRIM_POINTS, 1);
    immVertex3fv(pos, kcd->curr.cage);
    immEnd();
  }

  if (kcd->totlinehit > 0) {
    KnifeLineHit *lh;
    int i, snapped_verts_count, other_verts_count;
    float fcol[4];

    GPU_blend(true);
    GPU_blend_set_func_separate(
        GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);

    GPUVertBuf *vert = GPU_vertbuf_create_with_format(format);
    GPU_vertbuf_data_alloc(vert, kcd->totlinehit);

    lh = kcd->linehits;
    for (i = 0, snapped_verts_count = 0, other_verts_count = 0; i < kcd->totlinehit; i++, lh++) {
      if (lh->v) {
        GPU_vertbuf_attr_set(vert, pos, snapped_verts_count++, lh->cagehit);
      }
      else {
        GPU_vertbuf_attr_set(vert, pos, kcd->totlinehit - 1 - other_verts_count++, lh->cagehit);
      }
    }

    GPUBatch *batch = GPU_batch_create_ex(GPU_PRIM_POINTS, vert, NULL, GPU_BATCH_OWNS_VBO);
    GPU_batch_program_set_builtin(batch, GPU_SHADER_3D_UNIFORM_COLOR);
    GPU_batch_bind(batch);

    /* draw any snapped verts first */
    rgba_uchar_to_float(fcol, kcd->colors.point_a);
    GPU_batch_uniform_4fv(batch, "color", fcol);
    GPU_matrix_bind(batch->interface);
    GPU_shader_set_srgb_uniform(batch->interface);
    GPU_point_size(11);
    if (snapped_verts_count > 0) {
      GPU_batch_draw_advanced(batch, 0, snapped_verts_count, 0, 0);
    }

    /* now draw the rest */
    rgba_uchar_to_float(fcol, kcd->colors.curpoint_a);
    GPU_batch_uniform_4fv(batch, "color", fcol);
    GPU_point_size(7);
    if (other_verts_count > 0) {
      GPU_batch_draw_advanced(batch, snapped_verts_count, other_verts_count, 0, 0);
    }

    GPU_batch_program_use_end(batch);
    GPU_batch_discard(batch);

    GPU_blend(false);
  }

  if (kcd->totkedge > 0) {
    BLI_mempool_iter iter;
    KnifeEdge *kfe;

    immUniformColor3ubv(kcd->colors.line);
    GPU_line_width(1.0);

    GPUBatch *batch = immBeginBatchAtMost(GPU_PRIM_LINES, BLI_mempool_len(kcd->kedges) * 2);

    BLI_mempool_iternew(kcd->kedges, &iter);
    for (kfe = BLI_mempool_iterstep(&iter); kfe; kfe = BLI_mempool_iterstep(&iter)) {
      if (!kfe->is_cut) {
        continue;
      }

      immVertex3fv(pos, kfe->v1->cageco);
      immVertex3fv(pos, kfe->v2->cageco);
    }

    immEnd();

    GPU_batch_draw(batch);
    GPU_batch_discard(batch);
  }

  if (kcd->totkvert > 0) {
    BLI_mempool_iter iter;
    KnifeVert *kfv;

    immUniformColor3ubv(kcd->colors.point);
    GPU_point_size(5.0);

    GPUBatch *batch = immBeginBatchAtMost(GPU_PRIM_POINTS, BLI_mempool_len(kcd->kverts));

    BLI_mempool_iternew(kcd->kverts, &iter);
    for (kfv = BLI_mempool_iterstep(&iter); kfv; kfv = BLI_mempool_iterstep(&iter)) {
      if (!kfv->is_cut) {
        continue;
      }

      immVertex3fv(pos, kfv->cageco);
    }

    immEnd();

    GPU_batch_draw(batch);
    GPU_batch_discard(batch);
  }

  immUnbindProgram();

  GPU_matrix_pop();

  /* Reset default */
  GPU_depth_test(true);
}

/**
 * Find intersection of v1-v2 with face f.
 * Only take intersections that are at least \a face_tol_sq (in screen space) away
 * from other intersection elements.
 * If v1-v2 is coplanar with f, call that "no intersection though
 * it really means "infinite number of intersections".
 * In such a case we should have gotten hits on edges or verts of the face.
 */
static bool knife_ray_intersect_face(KnifeTool_OpData *kcd,
                                     const float s[2],
                                     const float v1[3],
                                     const float v2[3],
                                     BMFace *f,
                                     const float face_tol_sq,
                                     float hit_co[3],
                                     float hit_cageco[3])
{
  int tottri, tri_i;
  float raydir[3];
  float tri_norm[3], tri_plane[4];
  float se1[2], se2[2];
  float d, lambda;
  BMLoop **tri;
  ListBase *lst;
  Ref *ref;
  KnifeEdge *kfe;

  sub_v3_v3v3(raydir, v2, v1);
  normalize_v3(raydir);
  tri_i = get_lowest_face_tri(kcd, f);
  tottri = kcd->em->tottri;
  BLI_assert(tri_i >= 0 && tri_i < tottri);

  for (; tri_i < tottri; tri_i++) {
    const float *lv1, *lv2, *lv3;
    float ray_tri_uv[2];

    tri = kcd->em->looptris[tri_i];
    if (tri[0]->f != f) {
      break;
    }
    lv1 = kcd->cagecos[BM_elem_index_get(tri[0]->v)];
    lv2 = kcd->cagecos[BM_elem_index_get(tri[1]->v)];
    lv3 = kcd->cagecos[BM_elem_index_get(tri[2]->v)];
    /* using epsilon test in case ray is directly through an internal
     * tessellation edge and might not hit either tessellation tri with
     * an exact test;
     * we will exclude hits near real edges by a later test */
    if (isect_ray_tri_epsilon_v3(v1, raydir, lv1, lv2, lv3, &lambda, ray_tri_uv, KNIFE_FLT_EPS)) {
      /* check if line coplanar with tri */
      normal_tri_v3(tri_norm, lv1, lv2, lv3);
      plane_from_point_normal_v3(tri_plane, lv1, tri_norm);
      if ((dist_squared_to_plane_v3(v1, tri_plane) < KNIFE_FLT_EPS) &&
          (dist_squared_to_plane_v3(v2, tri_plane) < KNIFE_FLT_EPS)) {
        return false;
      }
      interp_v3_v3v3v3_uv(hit_cageco, lv1, lv2, lv3, ray_tri_uv);
      /* Now check that far enough away from verts and edges */
      lst = knife_get_face_kedges(kcd, f);
      for (ref = lst->first; ref; ref = ref->next) {
        kfe = ref->ref;
        knife_project_v2(kcd, kfe->v1->cageco, se1);
        knife_project_v2(kcd, kfe->v2->cageco, se2);
        d = dist_squared_to_line_segment_v2(s, se1, se2);
        if (d < face_tol_sq) {
          return false;
        }
      }
      interp_v3_v3v3v3_uv(hit_co, tri[0]->v->co, tri[1]->v->co, tri[2]->v->co, ray_tri_uv);
      return true;
    }
  }
  return false;
}

/**
 * Calculate the center and maximum excursion of mesh.
 */
static void calc_ortho_extent(KnifeTool_OpData *kcd)
{
  BMIter iter;
  BMVert *v;
  BMesh *bm = kcd->em->bm;
  float min[3], max[3];

  INIT_MINMAX(min, max);

  if (kcd->cagecos) {
    minmax_v3v3_v3_array(min, max, kcd->cagecos, bm->totvert);
  }
  else {
    BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
      minmax_v3v3_v3(min, max, v->co);
    }
  }

  kcd->ortho_extent = len_v3v3(min, max) / 2;
  mid_v3_v3v3(kcd->ortho_extent_center, min, max);
}

static BMElem *bm_elem_from_knife_vert(KnifeVert *kfv, KnifeEdge **r_kfe)
{
  BMElem *ele_test;
  KnifeEdge *kfe = NULL;

  /* vert? */
  ele_test = (BMElem *)kfv->v;

  if (r_kfe || ele_test == NULL) {
    if (kfv->v == NULL) {
      Ref *ref;
      for (ref = kfv->edges.first; ref; ref = ref->next) {
        kfe = ref->ref;
        if (kfe->e) {
          if (r_kfe) {
            *r_kfe = kfe;
          }
          break;
        }
      }
    }
  }

  /* edge? */
  if (ele_test == NULL) {
    if (kfe) {
      ele_test = (BMElem *)kfe->e;
    }
  }

  /* face? */
  if (ele_test == NULL) {
    if (BLI_listbase_is_single(&kfe->faces)) {
      ele_test = ((Ref *)kfe->faces.first)->ref;
    }
  }

  return ele_test;
}

static BMElem *bm_elem_from_knife_edge(KnifeEdge *kfe)
{
  BMElem *ele_test;

  ele_test = (BMElem *)kfe->e;

  if (ele_test == NULL) {
    ele_test = (BMElem *)kfe->basef;
  }

  return ele_test;
}

/* Do edges e1 and e2 go between exactly the same coordinates? */
static bool coinciding_edges(BMEdge *e1, BMEdge *e2)
{
  const float *co11, *co12, *co21, *co22;

  co11 = e1->v1->co;
  co12 = e1->v2->co;
  co21 = e2->v1->co;
  co22 = e2->v2->co;
  if ((equals_v3v3(co11, co21) && equals_v3v3(co12, co22)) ||
      (equals_v3v3(co11, co22) && equals_v3v3(co12, co21))) {
    return true;
  }
  else {
    return false;
  }
}

/* Callback used in point_is_visible to exclude hits on the faces that are the same
 * as or contain the hitting element (which is in user_data).
 * Also (see T44492) want to exclude hits on faces that butt up to the hitting element
 * (e.g., when you double an edge by an edge split).
 */
static bool bm_ray_cast_cb_elem_not_in_face_check(BMFace *f, void *user_data)
{
  bool ans;
  BMEdge *e, *e2;
  BMIter iter;

  switch (((BMElem *)user_data)->head.htype) {
    case BM_FACE:
      ans = (BMFace *)user_data != f;
      break;
    case BM_EDGE:
      e = (BMEdge *)user_data;
      ans = !BM_edge_in_face(e, f);
      if (ans) {
        /* Is it a boundary edge, coincident with a split edge? */
        if (BM_edge_is_boundary(e)) {
          BM_ITER_ELEM (e2, &iter, f, BM_EDGES_OF_FACE) {
            if (coinciding_edges(e, e2)) {
              ans = false;
              break;
            }
          }
        }
      }
      break;
    case BM_VERT:
      ans = !BM_vert_in_face((BMVert *)user_data, f);
      break;
    default:
      ans = true;
      break;
  }
  return ans;
}

/**
 * Check if \a p is visible (not clipped, not occluded by another face).
 * s in screen projection of p.
 *
 * \param ele_test: Optional vert/edge/face to use when \a p is on the surface of the geometry,
 * intersecting faces matching this face (or connected when an vert/edge) will be ignored.
 */
static bool point_is_visible(KnifeTool_OpData *kcd,
                             const float p[3],
                             const float s[2],
                             BMElem *ele_test)
{
  BMFace *f_hit;

  /* If box clipping on, make sure p is not clipped */
  if (RV3D_CLIPPING_ENABLED(kcd->vc.v3d, kcd->vc.rv3d) &&
      ED_view3d_clipping_test(kcd->vc.rv3d, p, true)) {
    return false;
  }

  /* If not cutting through, make sure no face is in front of p */
  if (!kcd->cut_through) {
    float dist;
    float view[3], p_ofs[3];

    /* TODO: I think there's a simpler way to get the required raycast ray */
    ED_view3d_unproject(kcd->vc.region, s[0], s[1], 0.0f, view);

    mul_m4_v3(kcd->ob->imat, view);

    /* make p_ofs a little towards view, so ray doesn't hit p's face. */
    sub_v3_v3(view, p);
    dist = normalize_v3(view);
    copy_v3_v3(p_ofs, p);

    /* avoid projecting behind the viewpoint */
    if (kcd->is_ortho && (kcd->vc.rv3d->persp != RV3D_CAMOB)) {
      dist = kcd->vc.v3d->clip_end * 2.0f;
    }

    if (RV3D_CLIPPING_ENABLED(kcd->vc.v3d, kcd->vc.rv3d)) {
      float view_clip[2][3];
      /* note: view_clip[0] should never get clipped */
      copy_v3_v3(view_clip[0], p_ofs);
      madd_v3_v3v3fl(view_clip[1], p_ofs, view, dist);

      if (clip_segment_v3_plane_n(view_clip[0],
                                  view_clip[1],
                                  kcd->vc.rv3d->clip_local,
                                  6,
                                  view_clip[0],
                                  view_clip[1])) {
        dist = len_v3v3(p_ofs, view_clip[1]);
      }
    }

    /* see if there's a face hit between p1 and the view */
    if (ele_test) {
      f_hit = BKE_bmbvh_ray_cast_filter(kcd->bmbvh,
                                        p_ofs,
                                        view,
                                        KNIFE_FLT_EPS,
                                        &dist,
                                        NULL,
                                        NULL,
                                        bm_ray_cast_cb_elem_not_in_face_check,
                                        ele_test);
    }
    else {
      f_hit = BKE_bmbvh_ray_cast(kcd->bmbvh, p_ofs, view, KNIFE_FLT_EPS, &dist, NULL, NULL);
    }

    if (f_hit) {
      return false;
    }
  }

  return true;
}

/* Clip the line (v1, v2) to planes perpendicular to it and distances d from
 * the closest point on the line to the origin */
static void clip_to_ortho_planes(float v1[3], float v2[3], const float center[3], const float d)
{
  float closest[3], dir[3];

  sub_v3_v3v3(dir, v1, v2);
  normalize_v3(dir);

  /* could be v1 or v2 */
  sub_v3_v3(v1, center);
  project_plane_normalized_v3_v3v3(closest, v1, dir);
  add_v3_v3(closest, center);

  madd_v3_v3v3fl(v1, closest, dir, d);
  madd_v3_v3v3fl(v2, closest, dir, -d);
}

static void set_linehit_depth(KnifeTool_OpData *kcd, KnifeLineHit *lh)
{
  lh->m = dot_m4_v3_row_z(kcd->vc.rv3d->persmatob, lh->cagehit);
}

/* Finds visible (or all, if cutting through) edges that intersects the current screen drag line */
static void knife_find_line_hits(KnifeTool_OpData *kcd)
{
  SmallHash faces, kfes, kfvs;
  float v1[3], v2[3], v3[3], v4[3], s1[2], s2[2];
  BVHTree *planetree, *tree;
  BVHTreeOverlap *results, *result;
  BMLoop **ls;
  BMFace *f;
  KnifeEdge *kfe;
  KnifeVert *v;
  ListBase *lst;
  Ref *ref;
  KnifeLineHit *linehits = NULL;
  BLI_array_declare(linehits);
  SmallHashIter hiter;
  KnifeLineHit hit;
  void *val;
  void **val_p;
  float plane_cos[12];
  float s[2], se1[2], se2[2], sint[2];
  float r1[3], r2[3];
  float d, d1, d2, lambda;
  float vert_tol, vert_tol_sq;
  float line_tol, line_tol_sq;
  float face_tol, face_tol_sq;
  int isect_kind;
  uint tot;
  int i;
  const bool use_hit_prev = true;
  const bool use_hit_curr = (kcd->is_drag_hold == false);

  if (kcd->linehits) {
    MEM_freeN(kcd->linehits);
    kcd->linehits = NULL;
    kcd->totlinehit = 0;
  }

  copy_v3_v3(v1, kcd->prev.cage);
  copy_v3_v3(v2, kcd->curr.cage);

  /* project screen line's 3d coordinates back into 2d */
  knife_project_v2(kcd, v1, s1);
  knife_project_v2(kcd, v2, s2);

  if (kcd->is_interactive) {
    if (len_squared_v2v2(s1, s2) < 1.0f) {
      return;
    }
  }
  else {
    if (len_squared_v2v2(s1, s2) < KNIFE_FLT_EPS_SQUARED) {
      return;
    }
  }

  /* unproject screen line */
  ED_view3d_win_to_segment_clipped(kcd->vc.depsgraph, kcd->region, kcd->vc.v3d, s1, v1, v3, true);
  ED_view3d_win_to_segment_clipped(kcd->vc.depsgraph, kcd->region, kcd->vc.v3d, s2, v2, v4, true);

  mul_m4_v3(kcd->ob->imat, v1);
  mul_m4_v3(kcd->ob->imat, v2);
  mul_m4_v3(kcd->ob->imat, v3);
  mul_m4_v3(kcd->ob->imat, v4);

  /* Numeric error, 'v1' -> 'v2', 'v2' -> 'v4'
   * can end up being ~2000 units apart with an orthogonal perspective.
   *
   * (from ED_view3d_win_to_segment_clipped() above)
   * this gives precision error; rather then solving properly
   * (which may involve using doubles everywhere!),
   * limit the distance between these points */
  if (kcd->is_ortho && (kcd->vc.rv3d->persp != RV3D_CAMOB)) {
    if (kcd->ortho_extent == 0.0f) {
      calc_ortho_extent(kcd);
    }
    clip_to_ortho_planes(v1, v3, kcd->ortho_extent_center, kcd->ortho_extent + 10.0f);
    clip_to_ortho_planes(v2, v4, kcd->ortho_extent_center, kcd->ortho_extent + 10.0f);
  }

  /* First use bvh tree to find faces, knife edges, and knife verts that might
   * intersect the cut plane with rays v1-v3 and v2-v4.
   * This deduplicates the candidates before doing more expensive intersection tests. */

  tree = BKE_bmbvh_tree_get(kcd->bmbvh);
  planetree = BLI_bvhtree_new(4, FLT_EPSILON * 4, 8, 8);
  copy_v3_v3(plane_cos + 0, v1);
  copy_v3_v3(plane_cos + 3, v2);
  copy_v3_v3(plane_cos + 6, v3);
  copy_v3_v3(plane_cos + 9, v4);
  BLI_bvhtree_insert(planetree, 0, plane_cos, 4);
  BLI_bvhtree_balance(planetree);

  results = BLI_bvhtree_overlap(tree, planetree, &tot, NULL, NULL);
  if (!results) {
    BLI_bvhtree_free(planetree);
    return;
  }

  BLI_smallhash_init(&faces);
  BLI_smallhash_init(&kfes);
  BLI_smallhash_init(&kfvs);

  for (i = 0, result = results; i < tot; i++, result++) {
    ls = (BMLoop **)kcd->em->looptris[result->indexA];
    f = ls[0]->f;
    set_lowest_face_tri(kcd, f, result->indexA);

    /* occlude but never cut unselected faces (when only_select is used) */
    if (kcd->only_select && !BM_elem_flag_test(f, BM_ELEM_SELECT)) {
      continue;
    }
    /* for faces, store index of lowest hit looptri in hash */
    if (BLI_smallhash_haskey(&faces, (uintptr_t)f)) {
      continue;
    }
    /* don't care what the value is except that it is non-NULL, for iterator */
    BLI_smallhash_insert(&faces, (uintptr_t)f, f);

    lst = knife_get_face_kedges(kcd, f);
    for (ref = lst->first; ref; ref = ref->next) {
      kfe = ref->ref;
      if (BLI_smallhash_haskey(&kfes, (uintptr_t)kfe)) {
        continue;
      }
      BLI_smallhash_insert(&kfes, (uintptr_t)kfe, kfe);
      v = kfe->v1;
      BLI_smallhash_reinsert(&kfvs, (uintptr_t)v, v);
      v = kfe->v2;
      BLI_smallhash_reinsert(&kfvs, (uintptr_t)v, v);
    }
  }

  /* Now go through the candidates and find intersections */
  /* These tolerances, in screen space, are for intermediate hits,
   * as ends are already snapped to screen. */

  if (kcd->is_interactive) {
    vert_tol = KNIFE_FLT_EPS_PX_VERT;
    line_tol = KNIFE_FLT_EPS_PX_EDGE;
    face_tol = KNIFE_FLT_EPS_PX_FACE;
  }
  else {
    /* Use 1/100th of a pixel, see T43896 (too big), T47910 (too small).
     *
     * Update, leave this as is until we investigate not using pixel coords
     * for geometry calculations: T48023. */
    vert_tol = line_tol = face_tol = 0.5f;
  }

  vert_tol_sq = vert_tol * vert_tol;
  line_tol_sq = line_tol * line_tol;
  face_tol_sq = face_tol * face_tol;

  /* Assume these tolerances swamp floating point rounding errors in calculations below */

  /* first look for vertex hits */
  for (val_p = BLI_smallhash_iternew_p(&kfvs, &hiter, (uintptr_t *)&v); val_p;
       val_p = BLI_smallhash_iternext_p(&hiter, (uintptr_t *)&v)) {
    KnifeEdge *kfe_hit = NULL;

    knife_project_v2(kcd, v->cageco, s);
    d = dist_squared_to_line_segment_v2(s, s1, s2);
    if ((d <= vert_tol_sq) &&
        (point_is_visible(kcd, v->cageco, s, bm_elem_from_knife_vert(v, &kfe_hit)))) {
      memset(&hit, 0, sizeof(hit));
      hit.v = v;

      /* If this isn't from an existing BMVert, it may have been added to a BMEdge originally.
       * knowing if the hit comes from an edge is important for edge-in-face checks later on
       * see: #knife_add_single_cut -> #knife_verts_edge_in_face, T42611 */
      if (kfe_hit) {
        hit.kfe = kfe_hit;
      }

      copy_v3_v3(hit.hit, v->co);
      copy_v3_v3(hit.cagehit, v->cageco);
      copy_v2_v2(hit.schit, s);
      set_linehit_depth(kcd, &hit);
      BLI_array_append(linehits, hit);
    }
    else {
      /* note that these vertes aren't used */
      *val_p = NULL;
    }
  }

  /* now edge hits; don't add if a vertex at end of edge should have hit */
  for (val = BLI_smallhash_iternew(&kfes, &hiter, (uintptr_t *)&kfe); val;
       val = BLI_smallhash_iternext(&hiter, (uintptr_t *)&kfe)) {
    int kfe_verts_in_cut;
    /* if we intersect both verts, don't attempt to intersect the edge */

    kfe_verts_in_cut = (BLI_smallhash_lookup(&kfvs, (intptr_t)kfe->v1) != NULL) +
                       (BLI_smallhash_lookup(&kfvs, (intptr_t)kfe->v2) != NULL);

    if (kfe_verts_in_cut == 2) {
      continue;
    }

    knife_project_v2(kcd, kfe->v1->cageco, se1);
    knife_project_v2(kcd, kfe->v2->cageco, se2);
    isect_kind = (kfe_verts_in_cut) ? -1 : isect_seg_seg_v2_point(s1, s2, se1, se2, sint);
    if (isect_kind == -1) {
      /* isect_seg_seg_v2_simple doesn't do tolerance test around ends of s1-s2 */
      closest_to_line_segment_v2(sint, s1, se1, se2);
      if (len_squared_v2v2(sint, s1) <= line_tol_sq) {
        isect_kind = 1;
      }
      else {
        closest_to_line_segment_v2(sint, s2, se1, se2);
        if (len_squared_v2v2(sint, s2) <= line_tol_sq) {
          isect_kind = 1;
        }
      }
    }
    if (isect_kind == 1) {
      d1 = len_v2v2(sint, se1);
      d2 = len_v2v2(se2, se1);
      if (!(d1 <= line_tol || d2 <= line_tol || fabsf(d1 - d2) <= line_tol)) {
        float p_cage[3], p_cage_tmp[3];
        lambda = d1 / d2;
        /* Can't just interpolate between ends of kfe because
         * that doesn't work with perspective transformation.
         * Need to find 3d intersection of ray through sint */
        knife_input_ray_segment(kcd, sint, 1.0f, r1, r2);
        isect_kind = isect_line_line_v3(
            kfe->v1->cageco, kfe->v2->cageco, r1, r2, p_cage, p_cage_tmp);
        if (isect_kind >= 1 && point_is_visible(kcd, p_cage, sint, bm_elem_from_knife_edge(kfe))) {
          memset(&hit, 0, sizeof(hit));
          if (kcd->snap_midpoints) {
            /* choose intermediate point snap too */
            mid_v3_v3v3(p_cage, kfe->v1->cageco, kfe->v2->cageco);
            mid_v2_v2v2(sint, se1, se2);
            lambda = 0.5f;
          }
          hit.kfe = kfe;
          transform_point_by_seg_v3(
              hit.hit, p_cage, kfe->v1->co, kfe->v2->co, kfe->v1->cageco, kfe->v2->cageco);
          copy_v3_v3(hit.cagehit, p_cage);
          copy_v2_v2(hit.schit, sint);
          hit.perc = lambda;
          set_linehit_depth(kcd, &hit);
          BLI_array_append(linehits, hit);
        }
      }
    }
  }
  /* now face hits; don't add if a vertex or edge in face should have hit */
  for (val = BLI_smallhash_iternew(&faces, &hiter, (uintptr_t *)&f); val;
       val = BLI_smallhash_iternext(&hiter, (uintptr_t *)&f)) {
    float p[3], p_cage[3];

    if (use_hit_prev && knife_ray_intersect_face(kcd, s1, v1, v3, f, face_tol_sq, p, p_cage)) {
      if (point_is_visible(kcd, p_cage, s1, (BMElem *)f)) {
        memset(&hit, 0, sizeof(hit));
        hit.f = f;
        copy_v3_v3(hit.hit, p);
        copy_v3_v3(hit.cagehit, p_cage);
        copy_v2_v2(hit.schit, s1);
        set_linehit_depth(kcd, &hit);
        BLI_array_append(linehits, hit);
      }
    }

    if (use_hit_curr && knife_ray_intersect_face(kcd, s2, v2, v4, f, face_tol_sq, p, p_cage)) {
      if (point_is_visible(kcd, p_cage, s2, (BMElem *)f)) {
        memset(&hit, 0, sizeof(hit));
        hit.f = f;
        copy_v3_v3(hit.hit, p);
        copy_v3_v3(hit.cagehit, p_cage);
        copy_v2_v2(hit.schit, s2);
        set_linehit_depth(kcd, &hit);
        BLI_array_append(linehits, hit);
      }
    }
  }

  kcd->linehits = linehits;
  kcd->totlinehit = BLI_array_len(linehits);

  /* find position along screen line, used for sorting */
  for (i = 0; i < kcd->totlinehit; i++) {
    KnifeLineHit *lh = kcd->linehits + i;

    lh->l = len_v2v2(lh->schit, s1) / len_v2v2(s2, s1);
  }

  BLI_smallhash_release(&faces);
  BLI_smallhash_release(&kfes);
  BLI_smallhash_release(&kfvs);
  BLI_bvhtree_free(planetree);
  if (results) {
    MEM_freeN(results);
  }
}

static void knife_input_ray_segment(KnifeTool_OpData *kcd,
                                    const float mval[2],
                                    const float ofs,
                                    float r_origin[3],
                                    float r_origin_ofs[3])
{
  /* unproject to find view ray */
  ED_view3d_unproject(kcd->vc.region, mval[0], mval[1], 0.0f, r_origin);
  ED_view3d_unproject(kcd->vc.region, mval[0], mval[1], ofs, r_origin_ofs);

  /* transform into object space */
  invert_m4_m4(kcd->ob->imat, kcd->ob->obmat);

  mul_m4_v3(kcd->ob->imat, r_origin);
  mul_m4_v3(kcd->ob->imat, r_origin_ofs);
}

static BMFace *knife_find_closest_face(KnifeTool_OpData *kcd,
                                       float co[3],
                                       float cageco[3],
                                       bool *is_space)
{
  BMFace *f;
  float dist = KMAXDIST;
  float origin[3];
  float origin_ofs[3];
  float ray[3], ray_normal[3];

  /* unproject to find view ray */
  knife_input_ray_segment(kcd, kcd->curr.mval, 1.0f, origin, origin_ofs);
  sub_v3_v3v3(ray, origin_ofs, origin);
  normalize_v3_v3(ray_normal, ray);

  f = BKE_bmbvh_ray_cast(kcd->bmbvh, origin, ray_normal, 0.0f, NULL, co, cageco);

  if (f && kcd->only_select && BM_elem_flag_test(f, BM_ELEM_SELECT) == 0) {
    f = NULL;
  }

  if (is_space) {
    *is_space = !f;
  }

  if (!f) {
    if (kcd->is_interactive) {
      /* try to use backbuffer selection method if ray casting failed */
      f = EDBM_face_find_nearest(&kcd->vc, &dist);

      /* cheat for now; just put in the origin instead
       * of a true coordinate on the face.
       * This just puts a point 1.0f infront of the view. */
      add_v3_v3v3(co, origin, ray);
    }
  }

  return f;
}

/* find the 2d screen space density of vertices within a radius.  used to scale snapping
 * distance for picking edges/verts.*/
static int knife_sample_screen_density(KnifeTool_OpData *kcd, const float radius)
{
  BMFace *f;
  bool is_space;
  float co[3], cageco[3], sco[2];

  BLI_assert(kcd->is_interactive == true);

  f = knife_find_closest_face(kcd, co, cageco, &is_space);

  if (f && !is_space) {
    const float radius_sq = radius * radius;
    ListBase *lst;
    Ref *ref;
    float dis_sq;
    int c = 0;

    knife_project_v2(kcd, cageco, sco);

    lst = knife_get_face_kedges(kcd, f);
    for (ref = lst->first; ref; ref = ref->next) {
      KnifeEdge *kfe = ref->ref;
      int i;

      for (i = 0; i < 2; i++) {
        KnifeVert *kfv = i ? kfe->v2 : kfe->v1;

        knife_project_v2(kcd, kfv->cageco, kfv->sco);

        dis_sq = len_squared_v2v2(kfv->sco, sco);
        if (dis_sq < radius_sq) {
          if (RV3D_CLIPPING_ENABLED(kcd->vc.v3d, kcd->vc.rv3d)) {
            if (ED_view3d_clipping_test(kcd->vc.rv3d, kfv->cageco, true) == 0) {
              c++;
            }
          }
          else {
            c++;
          }
        }
      }
    }

    return c;
  }

  return 0;
}

/* returns snapping distance for edges/verts, scaled by the density of the
 * surrounding mesh (in screen space)*/
static float knife_snap_size(KnifeTool_OpData *kcd, float maxsize)
{
  float density = (float)knife_sample_screen_density(kcd, maxsize * 2.0f);

  return min_ff(maxsize / (density * 0.5f), maxsize);
}

/* p is closest point on edge to the mouse cursor */
static KnifeEdge *knife_find_closest_edge(
    KnifeTool_OpData *kcd, float p[3], float cagep[3], BMFace **fptr, bool *is_space)
{
  BMFace *f;
  float co[3], cageco[3], sco[2];
  float maxdist;

  if (kcd->is_interactive) {
    maxdist = knife_snap_size(kcd, kcd->ethresh);

    if (kcd->ignore_vert_snapping) {
      maxdist *= 0.5f;
    }
  }
  else {
    maxdist = KNIFE_FLT_EPS;
  }

  f = knife_find_closest_face(kcd, co, cageco, NULL);
  *is_space = !f;

  kcd->curr.bmface = f;

  if (f) {
    const float maxdist_sq = maxdist * maxdist;
    KnifeEdge *cure = NULL;
    float cur_cagep[3];
    ListBase *lst;
    Ref *ref;
    float dis_sq, curdis_sq = FLT_MAX;

    /* set p to co, in case we don't find anything, means a face cut */
    copy_v3_v3(p, co);
    copy_v3_v3(cagep, cageco);

    knife_project_v2(kcd, cageco, sco);

    /* look through all edges associated with this face */
    lst = knife_get_face_kedges(kcd, f);
    for (ref = lst->first; ref; ref = ref->next) {
      KnifeEdge *kfe = ref->ref;
      float test_cagep[3];
      float lambda;

      /* project edge vertices into screen space */
      knife_project_v2(kcd, kfe->v1->cageco, kfe->v1->sco);
      knife_project_v2(kcd, kfe->v2->cageco, kfe->v2->sco);

      /* check if we're close enough and calculate 'lambda' */
      if (kcd->is_angle_snapping) {
        /* if snapping, check we're in bounds */
        float sco_snap[2];
        isect_line_line_v2_point(
            kfe->v1->sco, kfe->v2->sco, kcd->prev.mval, kcd->curr.mval, sco_snap);
        lambda = line_point_factor_v2(sco_snap, kfe->v1->sco, kfe->v2->sco);

        /* be strict about angle-snapping within edge */
        if ((lambda < 0.0f - KNIFE_FLT_EPSBIG) || (lambda > 1.0f + KNIFE_FLT_EPSBIG)) {
          continue;
        }

        dis_sq = len_squared_v2v2(sco, sco_snap);
        if (dis_sq < curdis_sq && dis_sq < maxdist_sq) {
          /* we already have 'lambda' */
        }
        else {
          continue;
        }
      }
      else {
        dis_sq = dist_squared_to_line_segment_v2(sco, kfe->v1->sco, kfe->v2->sco);
        if (dis_sq < curdis_sq && dis_sq < maxdist_sq) {
          lambda = line_point_factor_v2(sco, kfe->v1->sco, kfe->v2->sco);
        }
        else {
          continue;
        }
      }

      /* now we have 'lambda' calculated (in screen-space) */
      knife_interp_v3_v3v3(kcd, test_cagep, kfe->v1->cageco, kfe->v2->cageco, lambda);

      if (RV3D_CLIPPING_ENABLED(kcd->vc.v3d, kcd->vc.rv3d)) {
        /* check we're in the view */
        if (ED_view3d_clipping_test(kcd->vc.rv3d, test_cagep, true)) {
          continue;
        }
      }

      cure = kfe;
      curdis_sq = dis_sq;
      copy_v3_v3(cur_cagep, test_cagep);
    }

    if (fptr) {
      *fptr = f;
    }

    if (cure) {
      if (!kcd->ignore_edge_snapping || !(cure->e)) {
        KnifeVert *edgesnap = NULL;

        if (kcd->snap_midpoints) {
          mid_v3_v3v3(p, cure->v1->co, cure->v2->co);
          mid_v3_v3v3(cagep, cure->v1->cageco, cure->v2->cageco);
        }
        else {
          float lambda = line_point_factor_v3(cur_cagep, cure->v1->cageco, cure->v2->cageco);
          copy_v3_v3(cagep, cur_cagep);
          interp_v3_v3v3(p, cure->v1->co, cure->v2->co, lambda);
        }

        /* update mouse coordinates to the snapped-to edge's screen coordinates
         * this is important for angle snap, which uses the previous mouse position */
        edgesnap = new_knife_vert(kcd, p, cagep);
        kcd->curr.mval[0] = edgesnap->sco[0];
        kcd->curr.mval[1] = edgesnap->sco[1];
      }
      else {
        return NULL;
      }
    }

    return cure;
  }

  if (fptr) {
    *fptr = NULL;
  }

  return NULL;
}

/* find a vertex near the mouse cursor, if it exists */
static KnifeVert *knife_find_closest_vert(
    KnifeTool_OpData *kcd, float p[3], float cagep[3], BMFace **fptr, bool *is_space)
{
  BMFace *f;
  float co[3], cageco[3], sco[2];
  float maxdist;

  if (kcd->is_interactive) {
    maxdist = knife_snap_size(kcd, kcd->vthresh);
    if (kcd->ignore_vert_snapping) {
      maxdist *= 0.5f;
    }
  }
  else {
    maxdist = KNIFE_FLT_EPS;
  }

  f = knife_find_closest_face(kcd, co, cageco, is_space);

  kcd->curr.bmface = f;

  if (f) {
    const float maxdist_sq = maxdist * maxdist;
    ListBase *lst;
    Ref *ref;
    KnifeVert *curv = NULL;
    float dis_sq, curdis_sq = FLT_MAX;

    /* set p to co, in case we don't find anything, means a face cut */
    copy_v3_v3(p, co);
    copy_v3_v3(cagep, cageco);

    knife_project_v2(kcd, cageco, sco);

    lst = knife_get_face_kedges(kcd, f);
    for (ref = lst->first; ref; ref = ref->next) {
      KnifeEdge *kfe = ref->ref;
      int i;

      for (i = 0; i < 2; i++) {
        KnifeVert *kfv = i ? kfe->v2 : kfe->v1;

        knife_project_v2(kcd, kfv->cageco, kfv->sco);

        /* be strict about angle snapping, the vertex needs to be very close to the angle,
         * or we ignore */
        if (kcd->is_angle_snapping) {
          if (dist_squared_to_line_segment_v2(kfv->sco, kcd->prev.mval, kcd->curr.mval) >
              KNIFE_FLT_EPSBIG) {
            continue;
          }
        }

        dis_sq = len_squared_v2v2(kfv->sco, sco);
        if (dis_sq < curdis_sq && dis_sq < maxdist_sq) {
          if (RV3D_CLIPPING_ENABLED(kcd->vc.v3d, kcd->vc.rv3d)) {
            if (ED_view3d_clipping_test(kcd->vc.rv3d, kfv->cageco, true) == 0) {
              curv = kfv;
              curdis_sq = dis_sq;
            }
          }
          else {
            curv = kfv;
            curdis_sq = dis_sq;
          }
        }
      }
    }

    if (!kcd->ignore_vert_snapping || !(curv && curv->v)) {
      if (fptr) {
        *fptr = f;
      }

      if (curv) {
        copy_v3_v3(p, curv->co);
        copy_v3_v3(cagep, curv->cageco);

        /* update mouse coordinates to the snapped-to vertex's screen coordinates
         * this is important for angle snap, which uses the previous mouse position */
        kcd->curr.mval[0] = curv->sco[0];
        kcd->curr.mval[1] = curv->sco[1];
      }

      return curv;
    }
    else {
      if (fptr) {
        *fptr = f;
      }

      return NULL;
    }
  }

  if (fptr) {
    *fptr = NULL;
  }

  return NULL;
}

/**
 * Snaps a 2d vector to an angle, relative to \a v_ref.
 */
static float snap_v2_angle(float r[2], const float v[2], const float v_ref[2], float angle_snap)
{
  float m2[2][2];
  float v_unit[2];
  float angle, angle_delta;

  BLI_ASSERT_UNIT_V2(v_ref);

  normalize_v2_v2(v_unit, v);
  angle = angle_signed_v2v2(v_unit, v_ref);
  angle_delta = (roundf(angle / angle_snap) * angle_snap) - angle;
  angle_to_mat2(m2, angle_delta);

  mul_v2_m2v2(r, m2, v);
  return angle + angle_delta;
}

/* update both kcd->curr.mval and kcd->mval to snap to required angle */
static bool knife_snap_angle(KnifeTool_OpData *kcd)
{
  const float dvec_ref[2] = {0.0f, 1.0f};
  float dvec[2], dvec_snap[2];
  float snap_step = DEG2RADF(45);

  sub_v2_v2v2(dvec, kcd->curr.mval, kcd->prev.mval);
  if (is_zero_v2(dvec)) {
    return false;
  }

  kcd->angle = snap_v2_angle(dvec_snap, dvec, dvec_ref, snap_step);

  add_v2_v2v2(kcd->curr.mval, kcd->prev.mval, dvec_snap);

  copy_v2_v2(kcd->mval, kcd->curr.mval);

  return true;
}

/* update active knife edge/vert pointers */
static int knife_update_active(KnifeTool_OpData *kcd)
{
  knife_pos_data_clear(&kcd->curr);
  copy_v2_v2(kcd->curr.mval, kcd->mval);

  /* view matrix may have changed, reproject */
  knife_project_v2(kcd, kcd->prev.cage, kcd->prev.mval);

  if (kcd->angle_snapping && (kcd->mode == MODE_DRAGGING)) {
    kcd->is_angle_snapping = knife_snap_angle(kcd);
  }
  else {
    kcd->is_angle_snapping = false;
  }

  kcd->curr.vert = knife_find_closest_vert(
      kcd, kcd->curr.co, kcd->curr.cage, &kcd->curr.bmface, &kcd->curr.is_space);

  if (!kcd->curr.vert &&
      /* no edge snapping while dragging (edges are too sticky when cuts are immediate) */
      !kcd->is_drag_hold) {
    kcd->curr.edge = knife_find_closest_edge(
        kcd, kcd->curr.co, kcd->curr.cage, &kcd->curr.bmface, &kcd->curr.is_space);
  }

  /* if no hits are found this would normally default to (0, 0, 0) so instead
   * get a point at the mouse ray closest to the previous point.
   * Note that drawing lines in `free-space` isn't properly supported
   * but there's no guarantee (0, 0, 0) has any geometry either - campbell */
  if (kcd->curr.vert == NULL && kcd->curr.edge == NULL && kcd->curr.bmface == NULL) {
    float origin[3];
    float origin_ofs[3];

    knife_input_ray_segment(kcd, kcd->curr.mval, 1.0f, origin, origin_ofs);

    if (!isect_line_plane_v3(
            kcd->curr.cage, origin, origin_ofs, kcd->prev.cage, kcd->proj_zaxis)) {
      copy_v3_v3(kcd->curr.cage, kcd->prev.cage);

      /* should never fail! */
      BLI_assert(0);
    }
  }

  if (kcd->mode == MODE_DRAGGING) {
    knife_find_line_hits(kcd);
  }
  return 1;
}

static int sort_verts_by_dist_cb(void *co_p, const void *cur_a_p, const void *cur_b_p)
{
  const KnifeVert *cur_a = ((const Ref *)cur_a_p)->ref;
  const KnifeVert *cur_b = ((const Ref *)cur_b_p)->ref;
  const float *co = co_p;
  const float a_sq = len_squared_v3v3(co, cur_a->co);
  const float b_sq = len_squared_v3v3(co, cur_b->co);

  if (a_sq < b_sq) {
    return -1;
  }
  else if (a_sq > b_sq) {
    return 1;
  }
  else {
    return 0;
  }
}

static bool knife_verts_edge_in_face(KnifeVert *v1, KnifeVert *v2, BMFace *f)
{
  bool v1_inside, v2_inside;
  bool v1_inface, v2_inface;
  BMLoop *l1, *l2;

  if (!f || !v1 || !v2) {
    return false;
  }

  l1 = v1->v ? BM_face_vert_share_loop(f, v1->v) : NULL;
  l2 = v2->v ? BM_face_vert_share_loop(f, v2->v) : NULL;

  if ((l1 && l2) && BM_loop_is_adjacent(l1, l2)) {
    /* boundary-case, always false to avoid edge-in-face checks below */
    return false;
  }

  /* find out if v1 and v2, if set, are part of the face */
  v1_inface = (l1 != NULL);
  v2_inface = (l2 != NULL);

  /* BM_face_point_inside_test uses best-axis projection so this isn't most accurate test... */
  v1_inside = v1_inface ? false : BM_face_point_inside_test(f, v1->co);
  v2_inside = v2_inface ? false : BM_face_point_inside_test(f, v2->co);
  if ((v1_inface && v2_inside) || (v2_inface && v1_inside) || (v1_inside && v2_inside)) {
    return true;
  }

  if (v1_inface && v2_inface) {
    float mid[3];
    /* Can have case where v1 and v2 are on shared chain between two faces.
     * BM_face_splits_check_legal does visibility and self-intersection tests,
     * but it is expensive and maybe a bit buggy, so use a simple
     * "is the midpoint in the face" test */
    mid_v3_v3v3(mid, v1->co, v2->co);
    return BM_face_point_inside_test(f, mid);
  }
  return false;
}

static void knife_make_face_cuts(KnifeTool_OpData *kcd, BMFace *f, ListBase *kfedges)
{
  BMesh *bm = kcd->em->bm;
  KnifeEdge *kfe;
  Ref *ref;
  int edge_array_len = BLI_listbase_count(kfedges);
  int i;

  BMEdge **edge_array = BLI_array_alloca(edge_array, edge_array_len);

  /* point to knife edges we've created edges in, edge_array aligned */
  KnifeEdge **kfe_array = BLI_array_alloca(kfe_array, edge_array_len);

  BLI_assert(BLI_gset_len(kcd->edgenet.edge_visit) == 0);

  i = 0;
  for (ref = kfedges->first; ref; ref = ref->next) {
    bool is_new_edge = false;
    kfe = ref->ref;

    if (kfe->e == NULL) {
      if (kfe->v1->v && kfe->v2->v) {
        kfe->e = BM_edge_exists(kfe->v1->v, kfe->v2->v);
      }
    }

    if (kfe->e) {
      if (BM_edge_in_face(kfe->e, f)) {
        /* shouldn't happen, but in this case - just ignore */
        continue;
      }
    }
    else {
      if (kfe->v1->v == NULL) {
        kfe->v1->v = BM_vert_create(bm, kfe->v1->co, NULL, 0);
      }
      if (kfe->v2->v == NULL) {
        kfe->v2->v = BM_vert_create(bm, kfe->v2->co, NULL, 0);
      }
      BLI_assert(kfe->e == NULL);
      kfe->e = BM_edge_create(bm, kfe->v1->v, kfe->v2->v, NULL, 0);
      if (kfe->e) {
        if (kcd->select_result || BM_elem_flag_test(f, BM_ELEM_SELECT)) {
          BM_edge_select_set(bm, kfe->e, true);
        }
        is_new_edge = true;
      }
    }

    BLI_assert(kfe->e);

    if (BLI_gset_add(kcd->edgenet.edge_visit, kfe->e)) {
      kfe_array[i] = is_new_edge ? kfe : 0;
      edge_array[i] = kfe->e;
      i += 1;
    }
  }

  if (i) {
    const int edge_array_len_orig = i;
    edge_array_len = i;

#ifdef USE_NET_ISLAND_CONNECT
    uint edge_array_holes_len;
    BMEdge **edge_array_holes;
    if (BM_face_split_edgenet_connect_islands(bm,
                                              f,
                                              edge_array,
                                              edge_array_len,
                                              true,
                                              kcd->edgenet.arena,
                                              &edge_array_holes,
                                              &edge_array_holes_len)) {
      if (BM_elem_flag_test(f, BM_ELEM_SELECT)) {
        for (i = edge_array_len; i < edge_array_holes_len; i++) {
          BM_edge_select_set(bm, edge_array_holes[i], true);
        }
      }

      edge_array_len = edge_array_holes_len;
      edge_array = edge_array_holes; /* owned by the arena */
    }
#endif

    {
      BMFace **face_arr = NULL;
      int face_arr_len;

      BM_face_split_edgenet(bm, f, edge_array, edge_array_len, &face_arr, &face_arr_len);

      if (face_arr) {
        MEM_freeN(face_arr);
      }
    }

    /* remove dangling edges, not essential - but nice for users */
    for (i = 0; i < edge_array_len_orig; i++) {
      if (kfe_array[i]) {
        if (BM_edge_is_wire(kfe_array[i]->e)) {
          BM_edge_kill(bm, kfe_array[i]->e);
          kfe_array[i]->e = NULL;
        }
      }
    }

#ifdef USE_NET_ISLAND_CONNECT
    BLI_memarena_clear(kcd->edgenet.arena);
#endif
  }

  BLI_gset_clear(kcd->edgenet.edge_visit, NULL);
}

/* Use the network of KnifeEdges and KnifeVerts accumulated to make real BMVerts and BMEdedges */
static void knife_make_cuts(KnifeTool_OpData *kcd)
{
  BMesh *bm = kcd->em->bm;
  KnifeEdge *kfe;
  KnifeVert *kfv;
  BMFace *f;
  BMEdge *e, *enew;
  ListBase *lst;
  Ref *ref;
  float pct;
  SmallHashIter hiter;
  BLI_mempool_iter iter;
  SmallHash fhash_, *fhash = &fhash_;
  SmallHash ehash_, *ehash = &ehash_;

  BLI_smallhash_init(fhash);
  BLI_smallhash_init(ehash);

  /* put list of cutting edges for a face into fhash, keyed by face */
  BLI_mempool_iternew(kcd->kedges, &iter);
  for (kfe = BLI_mempool_iterstep(&iter); kfe; kfe = BLI_mempool_iterstep(&iter)) {

    /* select edges that lie directly on the cut */
    if (kcd->select_result) {
      if (kfe->e && kfe->is_cut) {
        BM_edge_select_set(bm, kfe->e, true);
      }
    }

    f = kfe->basef;
    if (!f || kfe->e) {
      continue;
    }
    lst = BLI_smallhash_lookup(fhash, (uintptr_t)f);
    if (!lst) {
      lst = knife_empty_list(kcd);
      BLI_smallhash_insert(fhash, (uintptr_t)f, lst);
    }
    knife_append_list(kcd, lst, kfe);
  }

  /* put list of splitting vertices for an edge into ehash, keyed by edge */
  BLI_mempool_iternew(kcd->kverts, &iter);
  for (kfv = BLI_mempool_iterstep(&iter); kfv; kfv = BLI_mempool_iterstep(&iter)) {
    if (kfv->v) {
      continue; /* already have a BMVert */
    }
    for (ref = kfv->edges.first; ref; ref = ref->next) {
      kfe = ref->ref;
      e = kfe->e;
      if (!e) {
        continue;
      }
      lst = BLI_smallhash_lookup(ehash, (uintptr_t)e);
      if (!lst) {
        lst = knife_empty_list(kcd);
        BLI_smallhash_insert(ehash, (uintptr_t)e, lst);
      }
      /* there can be more than one kfe in kfv's list with same e */
      if (!find_ref(lst, kfv)) {
        knife_append_list(kcd, lst, kfv);
      }
    }
  }

  /* split bmesh edges where needed */
  for (lst = BLI_smallhash_iternew(ehash, &hiter, (uintptr_t *)&e); lst;
       lst = BLI_smallhash_iternext(&hiter, (uintptr_t *)&e)) {
    BLI_listbase_sort_r(lst, sort_verts_by_dist_cb, e->v1->co);

    for (ref = lst->first; ref; ref = ref->next) {
      kfv = ref->ref;
      pct = line_point_factor_v3(kfv->co, e->v1->co, e->v2->co);
      kfv->v = BM_edge_split(bm, e, e->v1, &enew, pct);
    }
  }

  if (kcd->only_select) {
    EDBM_flag_disable_all(kcd->em, BM_ELEM_SELECT);
  }

  /* do cuts for each face */
  for (lst = BLI_smallhash_iternew(fhash, &hiter, (uintptr_t *)&f); lst;
       lst = BLI_smallhash_iternext(&hiter, (uintptr_t *)&f)) {
    knife_make_face_cuts(kcd, f, lst);
  }

  BLI_smallhash_release(fhash);
  BLI_smallhash_release(ehash);
}

/* called on tool confirmation */
static void knifetool_finish_ex(KnifeTool_OpData *kcd)
{
  knife_make_cuts(kcd);

  EDBM_selectmode_flush(kcd->em);
  EDBM_mesh_normals_update(kcd->em);
  EDBM_update_generic(kcd->ob->data, true, true);

  /* re-tessellating makes this invalid, dont use again by accident */
  knifetool_free_bmbvh(kcd);
}
static void knifetool_finish(wmOperator *op)
{
  KnifeTool_OpData *kcd = op->customdata;
  knifetool_finish_ex(kcd);
}

static void knife_recalc_projmat(KnifeTool_OpData *kcd)
{
  invert_m4_m4(kcd->ob->imat, kcd->ob->obmat);
  ED_view3d_ob_project_mat_get(kcd->region->regiondata, kcd->ob, kcd->projmat);
  invert_m4_m4(kcd->projmat_inv, kcd->projmat);

  mul_v3_mat3_m4v3(kcd->proj_zaxis, kcd->ob->imat, kcd->vc.rv3d->viewinv[2]);
  normalize_v3(kcd->proj_zaxis);

  kcd->is_ortho = ED_view3d_clip_range_get(
      kcd->vc.depsgraph, kcd->vc.v3d, kcd->vc.rv3d, &kcd->clipsta, &kcd->clipend, true);
}

/* called when modal loop selection is done... */
static void knifetool_exit_ex(bContext *C, KnifeTool_OpData *kcd)
{
  if (!kcd) {
    return;
  }

  if (kcd->is_interactive) {
    WM_cursor_modal_restore(CTX_wm_window(C));

    /* deactivate the extra drawing stuff in 3D-View */
    ED_region_draw_cb_exit(kcd->region->type, kcd->draw_handle);
  }

  /* free the custom data */
  BLI_mempool_destroy(kcd->refs);
  BLI_mempool_destroy(kcd->kverts);
  BLI_mempool_destroy(kcd->kedges);

  BLI_ghash_free(kcd->origedgemap, NULL, NULL);
  BLI_ghash_free(kcd->origvertmap, NULL, NULL);
  BLI_ghash_free(kcd->kedgefacemap, NULL, NULL);
  BLI_ghash_free(kcd->facetrimap, NULL, NULL);

  BLI_memarena_free(kcd->arena);
#ifdef USE_NET_ISLAND_CONNECT
  BLI_memarena_free(kcd->edgenet.arena);
#endif
  BLI_gset_free(kcd->edgenet.edge_visit, NULL);

  /* tag for redraw */
  ED_region_tag_redraw(kcd->region);

  knifetool_free_bmbvh(kcd);

  if (kcd->linehits) {
    MEM_freeN(kcd->linehits);
  }

  /* destroy kcd itself */
  MEM_freeN(kcd);
}
static void knifetool_exit(bContext *C, wmOperator *op)
{
  KnifeTool_OpData *kcd = op->customdata;
  knifetool_exit_ex(C, kcd);
  op->customdata = NULL;
}

static void knifetool_update_mval(KnifeTool_OpData *kcd, const float mval[2])
{
  knife_recalc_projmat(kcd);
  copy_v2_v2(kcd->mval, mval);

  if (knife_update_active(kcd)) {
    ED_region_tag_redraw(kcd->region);
  }
}

static void knifetool_update_mval_i(KnifeTool_OpData *kcd, const int mval_i[2])
{
  float mval[2] = {UNPACK2(mval_i)};
  knifetool_update_mval(kcd, mval);
}

static void knifetool_init_bmbvh(KnifeTool_OpData *kcd)
{
  BM_mesh_elem_index_ensure(kcd->em->bm, BM_VERT);

  Scene *scene_eval = (Scene *)DEG_get_evaluated_id(kcd->vc.depsgraph, &kcd->scene->id);
  Object *obedit_eval = (Object *)DEG_get_evaluated_id(kcd->vc.depsgraph, &kcd->ob->id);
  BMEditMesh *em_eval = BKE_editmesh_from_object(obedit_eval);

  kcd->cagecos = (const float(*)[3])BKE_editmesh_vert_coords_alloc(
      kcd->vc.depsgraph, em_eval, scene_eval, obedit_eval, NULL);

  kcd->bmbvh = BKE_bmbvh_new_from_editmesh(
      kcd->em,
      BMBVH_RETURN_ORIG |
          ((kcd->only_select && kcd->cut_through) ? BMBVH_RESPECT_SELECT : BMBVH_RESPECT_HIDDEN),
      kcd->cagecos,
      false);
}

static void knifetool_free_bmbvh(KnifeTool_OpData *kcd)
{
  if (kcd->bmbvh) {
    BKE_bmbvh_free(kcd->bmbvh);
    kcd->bmbvh = NULL;
  }

  if (kcd->cagecos) {
    MEM_freeN((void *)kcd->cagecos);
    kcd->cagecos = NULL;
  }
}

/* called when modal loop selection gets set up... */
static void knifetool_init(bContext *C,
                           KnifeTool_OpData *kcd,
                           const bool only_select,
                           const bool cut_through,
                           const bool is_interactive)
{
  Scene *scene = CTX_data_scene(C);
  Object *obedit = CTX_data_edit_object(C);

  /* assign the drawing handle for drawing preview line... */
  kcd->scene = scene;
  kcd->ob = obedit;
  kcd->region = CTX_wm_region(C);

  em_setup_viewcontext(C, &kcd->vc);

  kcd->em = BKE_editmesh_from_object(kcd->ob);

  /* cut all the way through the mesh if use_occlude_geometry button not pushed */
  kcd->is_interactive = is_interactive;
  kcd->cut_through = cut_through;
  kcd->only_select = only_select;

  knifetool_init_bmbvh(kcd);

  kcd->arena = BLI_memarena_new(MEM_SIZE_OPTIMAL(1 << 15), "knife");
#ifdef USE_NET_ISLAND_CONNECT
  kcd->edgenet.arena = BLI_memarena_new(MEM_SIZE_OPTIMAL(1 << 15), __func__);
#endif
  kcd->edgenet.edge_visit = BLI_gset_ptr_new(__func__);

  kcd->vthresh = KMAXDIST - 1;
  kcd->ethresh = KMAXDIST;

  knife_recalc_projmat(kcd);

  ED_region_tag_redraw(kcd->region);

  kcd->refs = BLI_mempool_create(sizeof(Ref), 0, 2048, 0);
  kcd->kverts = BLI_mempool_create(sizeof(KnifeVert), 0, 512, BLI_MEMPOOL_ALLOW_ITER);
  kcd->kedges = BLI_mempool_create(sizeof(KnifeEdge), 0, 512, BLI_MEMPOOL_ALLOW_ITER);

  kcd->origedgemap = BLI_ghash_ptr_new("knife origedgemap");
  kcd->origvertmap = BLI_ghash_ptr_new("knife origvertmap");
  kcd->kedgefacemap = BLI_ghash_ptr_new("knife kedgefacemap");
  kcd->facetrimap = BLI_ghash_ptr_new("knife facetrimap");

  /* can't usefully select resulting edges in face mode */
  kcd->select_result = (kcd->em->selectmode != SCE_SELECT_FACE);

  knife_pos_data_clear(&kcd->curr);
  knife_pos_data_clear(&kcd->prev);

  if (is_interactive) {
    kcd->draw_handle = ED_region_draw_cb_activate(
        kcd->region->type, knifetool_draw, kcd, REGION_DRAW_POST_VIEW);

    knife_init_colors(&kcd->colors);
  }
}

static void knifetool_cancel(bContext *C, wmOperator *op)
{
  /* this is just a wrapper around exit() */
  knifetool_exit(C, op);
}

static int knifetool_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  const bool only_select = RNA_boolean_get(op->ptr, "only_selected");
  const bool cut_through = !RNA_boolean_get(op->ptr, "use_occlude_geometry");
  const bool wait_for_input = RNA_boolean_get(op->ptr, "wait_for_input");

  KnifeTool_OpData *kcd;

  if (only_select) {
    Object *obedit = CTX_data_edit_object(C);
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    if (em->bm->totfacesel == 0) {
      BKE_report(op->reports, RPT_ERROR, "Selected faces required");
      return OPERATOR_CANCELLED;
    }
  }

  view3d_operator_needs_opengl(C);

  /* alloc new customdata */
  kcd = op->customdata = MEM_callocN(sizeof(KnifeTool_OpData), __func__);

  knifetool_init(C, kcd, only_select, cut_through, true);

  op->flag |= OP_IS_MODAL_CURSOR_REGION;

  /* add a modal handler for this operator - handles loop selection */
  WM_cursor_modal_set(CTX_wm_window(C), WM_CURSOR_KNIFE);
  WM_event_add_modal_handler(C, op);

  knifetool_update_mval_i(kcd, event->mval);

  if (wait_for_input == false) {
    /* Avoid copy-paste logic. */
    wmEvent event_modal = {
        .prevval = KM_NOTHING,
        .type = EVT_MODAL_MAP,
        .val = KNF_MODAL_ADD_CUT,
    };
    int ret = knifetool_modal(C, op, &event_modal);
    BLI_assert(ret == OPERATOR_RUNNING_MODAL);
    UNUSED_VARS_NDEBUG(ret);
  }

  knife_update_header(C, op, kcd);

  return OPERATOR_RUNNING_MODAL;
}

wmKeyMap *knifetool_modal_keymap(wmKeyConfig *keyconf)
{
  static const EnumPropertyItem modal_items[] = {
      {KNF_MODAL_CANCEL, "CANCEL", 0, "Cancel", ""},
      {KNF_MODAL_CONFIRM, "CONFIRM", 0, "Confirm", ""},
      {KNF_MODAL_MIDPOINT_ON, "SNAP_MIDPOINTS_ON", 0, "Snap to Midpoints On", ""},
      {KNF_MODAL_MIDPOINT_OFF, "SNAP_MIDPOINTS_OFF", 0, "Snap to Midpoints Off", ""},
      {KNF_MODEL_IGNORE_SNAP_ON, "IGNORE_SNAP_ON", 0, "Ignore Snapping On", ""},
      {KNF_MODEL_IGNORE_SNAP_OFF, "IGNORE_SNAP_OFF", 0, "Ignore Snapping Off", ""},
      {KNF_MODAL_ANGLE_SNAP_TOGGLE, "ANGLE_SNAP_TOGGLE", 0, "Toggle Angle Snapping", ""},
      {KNF_MODAL_CUT_THROUGH_TOGGLE, "CUT_THROUGH_TOGGLE", 0, "Toggle Cut Through", ""},
      {KNF_MODAL_NEW_CUT, "NEW_CUT", 0, "End Current Cut", ""},
      {KNF_MODAL_ADD_CUT, "ADD_CUT", 0, "Add Cut", ""},
      {KNF_MODAL_ADD_CUT_CLOSED, "ADD_CUT_CLOSED", 0, "Add Cut Closed", ""},
      {KNF_MODAL_PANNING, "PANNING", 0, "Panning", ""},
      {0, NULL, 0, NULL, NULL},
  };

  wmKeyMap *keymap = WM_modalkeymap_find(keyconf, "Knife Tool Modal Map");

  /* this function is called for each spacetype, only needs to add map once */
  if (keymap && keymap->modal_items) {
    return NULL;
  }

  keymap = WM_modalkeymap_ensure(keyconf, "Knife Tool Modal Map", modal_items);

  WM_modalkeymap_assign(keymap, "MESH_OT_knife_tool");

  return keymap;
}

static int knifetool_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  Object *obedit = CTX_data_edit_object(C);
  KnifeTool_OpData *kcd = op->customdata;
  bool do_refresh = false;

  if (!obedit || obedit->type != OB_MESH || BKE_editmesh_from_object(obedit) != kcd->em) {
    knifetool_exit(C, op);
    ED_workspace_status_text(C, NULL);
    return OPERATOR_FINISHED;
  }

  em_setup_viewcontext(C, &kcd->vc);
  kcd->region = kcd->vc.region;

  view3d_operator_needs_opengl(C);
  ED_view3d_init_mats_rv3d(obedit, kcd->vc.rv3d); /* needed to initialize clipping */

  if (kcd->mode == MODE_PANNING) {
    kcd->mode = kcd->prevmode;
  }

  /* handle modal keymap */
  if (event->type == EVT_MODAL_MAP) {
    switch (event->val) {
      case KNF_MODAL_CANCEL:
        /* finish */
        ED_region_tag_redraw(kcd->region);

        knifetool_exit(C, op);
        ED_workspace_status_text(C, NULL);

        return OPERATOR_CANCELLED;
      case KNF_MODAL_CONFIRM:
        /* finish */
        ED_region_tag_redraw(kcd->region);

        knifetool_finish(op);
        knifetool_exit(C, op);
        ED_workspace_status_text(C, NULL);

        return OPERATOR_FINISHED;
      case KNF_MODAL_MIDPOINT_ON:
        kcd->snap_midpoints = true;

        knife_recalc_projmat(kcd);
        knife_update_active(kcd);
        knife_update_header(C, op, kcd);
        ED_region_tag_redraw(kcd->region);
        do_refresh = true;
        break;
      case KNF_MODAL_MIDPOINT_OFF:
        kcd->snap_midpoints = false;

        knife_recalc_projmat(kcd);
        knife_update_active(kcd);
        knife_update_header(C, op, kcd);
        ED_region_tag_redraw(kcd->region);
        do_refresh = true;
        break;
      case KNF_MODEL_IGNORE_SNAP_ON:
        ED_region_tag_redraw(kcd->region);
        kcd->ignore_vert_snapping = kcd->ignore_edge_snapping = true;
        knife_update_header(C, op, kcd);
        do_refresh = true;
        break;
      case KNF_MODEL_IGNORE_SNAP_OFF:
        ED_region_tag_redraw(kcd->region);
        kcd->ignore_vert_snapping = kcd->ignore_edge_snapping = false;
        knife_update_header(C, op, kcd);
        do_refresh = true;
        break;
      case KNF_MODAL_ANGLE_SNAP_TOGGLE:
        kcd->angle_snapping = !kcd->angle_snapping;
        knife_update_header(C, op, kcd);
        do_refresh = true;
        break;
      case KNF_MODAL_CUT_THROUGH_TOGGLE:
        kcd->cut_through = !kcd->cut_through;
        knife_update_header(C, op, kcd);
        do_refresh = true;
        break;
      case KNF_MODAL_NEW_CUT:
        ED_region_tag_redraw(kcd->region);
        knife_finish_cut(kcd);
        kcd->mode = MODE_IDLE;
        break;
      case KNF_MODAL_ADD_CUT:
        knife_recalc_projmat(kcd);

        /* get the value of the event which triggered this one */
        if (event->prevval != KM_RELEASE) {
          if (kcd->mode == MODE_DRAGGING) {
            knife_add_cut(kcd);
          }
          else if (kcd->mode != MODE_PANNING) {
            knife_start_cut(kcd);
            kcd->mode = MODE_DRAGGING;
            kcd->init = kcd->curr;
          }

          /* freehand drawing is incompatible with cut-through */
          if (kcd->cut_through == false) {
            kcd->is_drag_hold = true;
          }
        }
        else {
          kcd->is_drag_hold = false;

          /* needed because the last face 'hit' is ignored when dragging */
          knifetool_update_mval(kcd, kcd->curr.mval);
        }

        ED_region_tag_redraw(kcd->region);
        break;
      case KNF_MODAL_ADD_CUT_CLOSED:
        if (kcd->mode == MODE_DRAGGING) {

          /* Shouldn't be possible with default key-layout, just in case. */
          if (kcd->is_drag_hold) {
            kcd->is_drag_hold = false;
            knifetool_update_mval(kcd, kcd->curr.mval);
          }

          kcd->prev = kcd->curr;
          kcd->curr = kcd->init;

          knife_project_v2(kcd, kcd->curr.cage, kcd->curr.mval);
          knifetool_update_mval(kcd, kcd->curr.mval);

          knife_add_cut(kcd);

          /* KNF_MODAL_NEW_CUT */
          knife_finish_cut(kcd);
          kcd->mode = MODE_IDLE;
        }
        break;
      case KNF_MODAL_PANNING:
        if (event->val != KM_RELEASE) {
          if (kcd->mode != MODE_PANNING) {
            kcd->prevmode = kcd->mode;
            kcd->mode = MODE_PANNING;
          }
        }
        else {
          kcd->mode = kcd->prevmode;
        }

        ED_region_tag_redraw(kcd->region);
        return OPERATOR_PASS_THROUGH;
    }
  }
  else { /* non-modal-mapped events */
    switch (event->type) {
      case MOUSEPAN:
      case MOUSEZOOM:
      case MOUSEROTATE:
      case WHEELUPMOUSE:
      case WHEELDOWNMOUSE:
      case NDOF_MOTION:
        return OPERATOR_PASS_THROUGH;
      case MOUSEMOVE: /* mouse moved somewhere to select another loop */
        if (kcd->mode != MODE_PANNING) {
          knifetool_update_mval_i(kcd, event->mval);

          if (kcd->is_drag_hold) {
            if (kcd->totlinehit >= 2) {
              knife_add_cut(kcd);
            }
          }
        }

        break;
    }
  }

  if (kcd->mode == MODE_DRAGGING) {
    op->flag &= ~OP_IS_MODAL_CURSOR_REGION;
  }
  else {
    op->flag |= OP_IS_MODAL_CURSOR_REGION;
  }

  if (do_refresh) {
    /* we don't really need to update mval,
     * but this happens to be the best way to refresh at the moment */
    knifetool_update_mval_i(kcd, event->mval);
  }

  /* keep going until the user confirms */
  return OPERATOR_RUNNING_MODAL;
}

void MESH_OT_knife_tool(wmOperatorType *ot)
{
  /* description */
  ot->name = "Knife Topology Tool";
  ot->idname = "MESH_OT_knife_tool";
  ot->description = "Cut new topology";

  /* callbacks */
  ot->invoke = knifetool_invoke;
  ot->modal = knifetool_modal;
  ot->cancel = knifetool_cancel;
  ot->poll = ED_operator_editmesh_view3d;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

  /* properties */
  PropertyRNA *prop;
  RNA_def_boolean(ot->srna,
                  "use_occlude_geometry",
                  true,
                  "Occlude Geometry",
                  "Only cut the front most geometry");
  RNA_def_boolean(ot->srna, "only_selected", false, "Only Selected", "Only cut selected geometry");

  prop = RNA_def_boolean(ot->srna, "wait_for_input", true, "Wait for Input", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

/* -------------------------------------------------------------------- */
/* Knife tool as a utility function
 * that can be used for internal slicing operations */

static bool edbm_mesh_knife_point_isect(LinkNode *polys, const float cent_ss[2])
{
  LinkNode *p = polys;
  int isect = 0;

  while (p) {
    const float(*mval_fl)[2] = p->link;
    const int mval_tot = MEM_allocN_len(mval_fl) / sizeof(*mval_fl);
    isect += (int)isect_point_poly_v2(cent_ss, mval_fl, mval_tot - 1, false);
    p = p->next;
  }

  if (isect % 2) {
    return true;
  }
  return false;
}

/**
 * \param use_tag: When set, tag all faces inside the polylines.
 */
void EDBM_mesh_knife(bContext *C, LinkNode *polys, bool use_tag, bool cut_through)
{
  KnifeTool_OpData *kcd;

  view3d_operator_needs_opengl(C);

  /* init */
  {
    const bool only_select = false;
    const bool is_interactive = false; /* can enable for testing */

    kcd = MEM_callocN(sizeof(KnifeTool_OpData), __func__);

    knifetool_init(C, kcd, only_select, cut_through, is_interactive);

    kcd->ignore_edge_snapping = true;
    kcd->ignore_vert_snapping = true;

    if (use_tag) {
      BM_mesh_elem_hflag_enable_all(kcd->em->bm, BM_EDGE, BM_ELEM_TAG, false);
    }
  }

  /* execute */
  {
    LinkNode *p = polys;

    knife_recalc_projmat(kcd);

    while (p) {
      const float(*mval_fl)[2] = p->link;
      const int mval_tot = MEM_allocN_len(mval_fl) / sizeof(*mval_fl);
      int i;

      for (i = 0; i < mval_tot; i++) {
        knifetool_update_mval(kcd, mval_fl[i]);
        if (i == 0) {
          knife_start_cut(kcd);
          kcd->mode = MODE_DRAGGING;
        }
        else {
          knife_add_cut(kcd);
        }
      }
      knife_finish_cut(kcd);
      kcd->mode = MODE_IDLE;
      p = p->next;
    }
  }

  /* finish */
  {
    knifetool_finish_ex(kcd);

    /* tag faces inside! */
    if (use_tag) {
      BMesh *bm = kcd->em->bm;
      float projmat[4][4];

      BMEdge *e;
      BMIter iter;

      bool keep_search;

      /* freed on knifetool_finish_ex, but we need again to check if points are visible */
      if (kcd->cut_through == false) {
        knifetool_init_bmbvh(kcd);
      }

      ED_view3d_ob_project_mat_get(kcd->region->regiondata, kcd->ob, projmat);

      /* use face-loop tag to store if we have intersected */
#define F_ISECT_IS_UNKNOWN(f) BM_elem_flag_test(BM_FACE_FIRST_LOOP(f), BM_ELEM_TAG)
#define F_ISECT_SET_UNKNOWN(f) BM_elem_flag_enable(BM_FACE_FIRST_LOOP(f), BM_ELEM_TAG)
#define F_ISECT_SET_OUTSIDE(f) BM_elem_flag_disable(BM_FACE_FIRST_LOOP(f), BM_ELEM_TAG)
      {
        BMFace *f;
        BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
          F_ISECT_SET_UNKNOWN(f);
          BM_elem_flag_disable(f, BM_ELEM_TAG);
        }
      }

      /* tag all faces linked to cut edges */
      BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
        /* check are we tagged?, then we are an original face */
        if (BM_elem_flag_test(e, BM_ELEM_TAG) == false) {
          BMFace *f;
          BMIter fiter;
          BM_ITER_ELEM (f, &fiter, e, BM_FACES_OF_EDGE) {
            float cent[3], cent_ss[2];
            BM_face_calc_point_in_face(f, cent);
            knife_project_v2(kcd, cent, cent_ss);
            if (edbm_mesh_knife_point_isect(polys, cent_ss)) {
              BM_elem_flag_enable(f, BM_ELEM_TAG);
            }
          }
        }
      }

      /* expand tags for faces which are not cut, but are inside the polys */
      do {
        BMFace *f;
        keep_search = false;
        BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
          if (BM_elem_flag_test(f, BM_ELEM_TAG) == false && (F_ISECT_IS_UNKNOWN(f))) {
            /* am I connected to a tagged face via an un-tagged edge
             * (ie, not across a cut) */
            BMLoop *l_first = BM_FACE_FIRST_LOOP(f);
            BMLoop *l_iter = l_first;
            bool found = false;

            do {
              if (BM_elem_flag_test(l_iter->e, BM_ELEM_TAG) != false) {
                /* now check if the adjacent faces is tagged */
                BMLoop *l_radial_iter = l_iter->radial_next;
                if (l_radial_iter != l_iter) {
                  do {
                    if (BM_elem_flag_test(l_radial_iter->f, BM_ELEM_TAG)) {
                      found = true;
                    }
                  } while ((l_radial_iter = l_radial_iter->radial_next) != l_iter &&
                           (found == false));
                }
              }
            } while ((l_iter = l_iter->next) != l_first && (found == false));

            if (found) {
              float cent[3], cent_ss[2];
              BM_face_calc_point_in_face(f, cent);
              knife_project_v2(kcd, cent, cent_ss);
              if ((kcd->cut_through || point_is_visible(kcd, cent, cent_ss, (BMElem *)f)) &&
                  edbm_mesh_knife_point_isect(polys, cent_ss)) {
                BM_elem_flag_enable(f, BM_ELEM_TAG);
                keep_search = true;
              }
              else {
                /* don't loose time on this face again, set it as outside */
                F_ISECT_SET_OUTSIDE(f);
              }
            }
          }
        }
      } while (keep_search);

#undef F_ISECT_IS_UNKNOWN
#undef F_ISECT_SET_UNKNOWN
#undef F_ISECT_SET_OUTSIDE
    }

    knifetool_exit_ex(C, kcd);
    kcd = NULL;
  }
}
