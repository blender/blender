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
 * Main functions for beveling a BMesh (used by the tool and modifier)
 */

#include "MEM_guardedalloc.h"

#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_scene_types.h"

#include "BLI_alloca.h"
#include "BLI_array.h"
#include "BLI_math.h"
#include "BLI_memarena.h"
#include "BLI_utildefines.h"

#include "BKE_customdata.h"
#include "BKE_deform.h"
#include "BKE_mesh.h"

#include "eigen_capi.h"

#include "BKE_curveprofile.h"
#include "DNA_curveprofile_types.h"

#include "bmesh.h"
#include "bmesh_bevel.h" /* own include */

#include "./intern/bmesh_private.h"

#define BEVEL_EPSILON_D 1e-6
#define BEVEL_EPSILON 1e-6f
#define BEVEL_EPSILON_SQ 1e-12f
#define BEVEL_EPSILON_BIG 1e-4f
#define BEVEL_EPSILON_BIG_SQ 1e-8f
#define BEVEL_EPSILON_ANG DEG2RADF(2.0f)
#define BEVEL_SMALL_ANG DEG2RADF(10.0f)
/** Difference in dot products that corresponds to 10 degree difference between vectors. */
#define BEVEL_SMALL_ANG_DOT 1 - cosf(BEVEL_SMALL_ANG)
#define BEVEL_MAX_ADJUST_PCT 10.0f
#define BEVEL_MAX_AUTO_ADJUST_PCT 300.0f
#define BEVEL_MATCH_SPEC_WEIGHT 0.2

//#define DEBUG_CUSTOM_PROFILE_CUTOFF
/* Happens far too often, uncomment for development. */
// #define BEVEL_ASSERT_PROJECT

/* for testing */
// #pragma GCC diagnostic error "-Wpadded"

/* Constructed vertex, sometimes later instantiated as BMVert. */
typedef struct NewVert {
  BMVert *v;
  float co[3];
  char _pad[4];
} NewVert;

struct BoundVert;

/* Data for one end of an edge involved in a bevel. */
typedef struct EdgeHalf {
  /** Other EdgeHalves connected to the same BevVert, in CCW order. */
  struct EdgeHalf *next, *prev;
  /** Original mesh edge. */
  BMEdge *e;
  /** Face between this edge and previous, if any. */
  BMFace *fprev;
  /** Face between this edge and next, if any. */
  BMFace *fnext;
  /** Left boundary vert (looking along edge to end). */
  struct BoundVert *leftv;
  /** Right boundary vert, if beveled. */
  struct BoundVert *rightv;
  /** Offset into profile to attach non-beveled edge. */
  int profile_index;
  /** How many segments for the bevel. */
  int seg;
  /** Offset for this edge, on left side. */
  float offset_l;
  /** Offset for this edge, on right side. */
  float offset_r;
  /** User specification for offset_l. */
  float offset_l_spec;
  /** User specification for offset_r. */
  float offset_r_spec;
  /** Is this edge beveled? */
  bool is_bev;
  /** Is e->v2 the vertex at this end? */
  bool is_rev;
  /** Is e a seam for custom loopdata (e.g., UVs)? */
  bool is_seam;
  /** Used during the custom profile orientation pass. */
  bool visited_rpo;
  char _pad[4];
} EdgeHalf;

/**
 * Profile specification:
 * The profile is a path defined with start, middle, and end control points projected onto a
 * plane (plane_no is normal, plane_co is a point on it) via lines in a given direction (proj_dir).
 *
 * Many interesting profiles are in family of superellipses:
 *     (abs(x/a))^r + abs(y/b))^r = 1
 * r==2 => ellipse; r==1 => line; r < 1 => concave; r > 1 => bulging out.
 * Special cases: let r==0 mean straight-inward, and r==4 mean straight outward.
 *
 * After the parameters are all set, the actual profile points are calculated and pointed to
 * by prof_co. We also may need profile points for a higher resolution number of segments
 * for the subdivision while making the ADJ vertex mesh pattern, and that goes in prof_co_2.
 */
typedef struct Profile {
  /** Superellipse r parameter. */
  float super_r;
  /** Height for profile cutoff face sides. */
  float height;
  /** Start control point for profile. */
  float start[3];
  /** Mid control point for profile. */
  float middle[3];
  /** End control point for profile. */
  float end[3];
  /** Normal of plane to project to. */
  float plane_no[3];
  /** Coordinate on plane to project to. */
  float plane_co[3];
  /** Direction of projection line. */
  float proj_dir[3];
  /** seg+1 profile coordinates (triples of floats). */
  float *prof_co;
  /** Like prof_co, but for seg power of 2 >= seg. */
  float *prof_co_2;
  /** Mark a special case so the these parameters aren't reset with others. */
  bool special_params;
} Profile;
#define PRO_SQUARE_R 1e4f
#define PRO_CIRCLE_R 2.0f
#define PRO_LINE_R 1.0f
#define PRO_SQUARE_IN_R 0.0f

/**
 * The un-transformed 2D storage of profile vertex locations. Also, for non-custom profiles
 * this serves as a cache for the results of the expensive calculation of u parameter values to
 * get even spacing on superellipse for current BevelParams seg and pro_super_r.
 */
typedef struct ProfileSpacing {
  /** The profile's seg+1 x values. */
  double *xvals;
  /** The profile's seg+1 y values. */
  double *yvals;
  /** The profile's seg_2+1 x values, (seg_2 = power of 2 >= seg). */
  double *xvals_2;
  /** The profile's seg_2+1 y values, (seg_2 = power of 2 >= seg). */
  double *yvals_2;
  /** The power of two greater than or equal to the number of segments. */
  int seg_2;
  /** How far "out" the profile is, used at the start of subdivision. */
  float fullness;
} ProfileSpacing;

/**
 * An element in a cyclic boundary of a Vertex Mesh (VMesh), placed on each side of beveled edges
 * where each profile starts, or on each side of a miter.
 */
typedef struct BoundVert {
  /** In CCW order. */
  struct BoundVert *next, *prev;
  NewVert nv;
  /** First of edges attached here: in CCW order. */
  EdgeHalf *efirst;
  EdgeHalf *elast;
  /** The "edge between" that this boundvert on, in offset_on_edge_between case. */
  EdgeHalf *eon;
  /** Beveled edge whose left side is attached here, if any. */
  EdgeHalf *ebev;
  /** Used for vmesh indexing. */
  int index;
  /** When eon set, ratio of sines of angles to eon edge. */
  float sinratio;
  /** Adjustment chain or cycle link pointer. */
  struct BoundVert *adjchain;
  /** Edge profile between this and next BoundVert. */
  Profile profile;
  /** Are any of the edges attached here seams? */
  bool any_seam;
  /** Used during delta adjust pass. */
  bool visited;
  /** This boundvert begins an arc profile. */
  bool is_arc_start;
  /** This boundvert begins a patch profile. */
  bool is_patch_start;
  /** Is this boundvert the side of the custom profile's start. */
  bool is_profile_start;
  char _pad[3];
  /** Length of seam starting from current boundvert to next boundvert with ccw ordering. */
  int seam_len;
  /** Same as seam_len but defines length of sharp edges. */
  int sharp_len;
} BoundVert;

/** Data for the mesh structure replacing a vertex. */
typedef struct VMesh {
  /** Allocated array - size and structure depends on kind. */
  NewVert *mesh;
  /** Start of boundary double-linked list. */
  BoundVert *boundstart;
  /** Number of vertices in the boundary. */
  int count;
  /** Common number of segments for segmented edges (same as bp->seg). */
  int seg;
  /** The kind of mesh to build at the corner vertex meshes. */
  enum {
    M_NONE,    /* No polygon mesh needed. */
    M_POLY,    /* A simple polygon. */
    M_ADJ,     /* "Adjacent edges" mesh pattern. */
    M_TRI_FAN, /* A simple polygon - fan filled. */
    M_CUTOFF,  /* A triangulated face at the end of each profile. */
  } mesh_kind;

  int _pad;
} VMesh;

/* Data for a vertex involved in a bevel. */
typedef struct BevVert {
  /** Original mesh vertex. */
  BMVert *v;
  /** Total number of edges around the vertex (excluding wire edges if edge beveling). */
  int edgecount;
  /** Number of selected edges around the vertex. */
  int selcount;
  /** Count of wire edges. */
  int wirecount;
  /** Offset for this vertex, if vertex_only bevel. */
  float offset;
  /** Any seams on attached edges? */
  bool any_seam;
  /** Used in graph traversal for adjusting offsets. */
  bool visited;
  /** Array of size edgecount; CCW order from vertex normal side. */
  char _pad[6];
  EdgeHalf *edges;
  /** Array of size wirecount of wire edges. */
  BMEdge **wire_edges;
  /** Mesh structure for replacing vertex. */
  VMesh *vmesh;
} BevVert;

/* Face classification. Note: depends on F_RECON > F_EDGE > F_VERT .*/
typedef enum {
  /** Used when there is no face at all. */
  F_NONE,
  /** Original face, not touched. */
  F_ORIG,
  /** Face for construction around a vert. */
  F_VERT,
  /** Face for a beveled edge. */
  F_EDGE,
  /** Reconstructed original face with some new verts. */
  F_RECON,
} FKind;

/** Helper for keeping track of angle kind. */
enum {
  /** Angle less than 180 degrees. */
  ANGLE_SMALLER = -1,
  /** 180 degree angle. */
  ANGLE_STRAIGHT = 0,
  /** Angle greater than 180 degrees. */
  ANGLE_LARGER = 1,
};

/** Bevel parameters and state. */
typedef struct BevelParams {
  /** Records BevVerts made: key BMVert*, value BevVert* */
  GHash *vert_hash;
  /** Records new faces: key BMFace*, value one of {VERT/EDGE/RECON}_POLY. */
  GHash *face_hash;
  /** Use for all allocs while bevel runs. Note: If we need to free we can switch to mempool. */
  MemArena *mem_arena;
  /** Profile vertex location and spacings. */
  ProfileSpacing pro_spacing;
  /** Parameter values for evenly spaced profile points for the miter profiles. */
  ProfileSpacing pro_spacing_miter;
  /** Blender units to offset each side of a beveled edge. */
  float offset;
  /** How offset is measured; enum defined in bmesh_operators.h. */
  int offset_type;
  /** Number of segments in beveled edge profile. */
  int seg;
  /** User profile setting. */
  float profile;
  /** Superellipse parameter for edge profile. */
  float pro_super_r;
  /** Bevel vertices only. */
  bool vertex_only;
  /** Bevel amount affected by weights on edges or verts. */
  bool use_weights;
  /** Should bevel prefer to slide along edges rather than keep widths spec? */
  bool loop_slide;
  /** Should offsets be limited by collisions? */
  bool limit_offset;
  /** Should offsets be adjusted to try to get even widths? */
  bool offset_adjust;
  /** Should we propagate seam edge markings? */
  bool mark_seam;
  /** Should we propagate sharp edge markings? */
  bool mark_sharp;
  /** Should we harden normals? */
  bool harden_normals;
  /** Should we use the custom profiles feature? */
  bool use_custom_profile;
  char _pad[3];
  /** The struct used to store the custom profile input. */
  const struct CurveProfile *custom_profile;
  /** Vertex group array, maybe set if vertex_only. */
  const struct MDeformVert *dvert;
  /** Vertex group index, maybe set if vertex_only. */
  int vertex_group;
  /** If >= 0, material number for bevel; else material comes from adjacent faces. */
  int mat_nr;
  /** Setting face strength if > 0. */
  int face_strength_mode;
  /** What kind of miter pattern to use on reflex angles. */
  int miter_outer;
  /** What kind of miter pattern to use on non-reflex angles. */
  int miter_inner;
  /** The method to use for vertex mesh creation */
  int vmesh_method;
  /** Amount to spread when doing inside miter. */
  float spread;
  /** Mesh's smoothresh, used if hardening. */
  float smoothresh;
} BevelParams;

// #pragma GCC diagnostic ignored "-Wpadded"

/* Only for debugging, this file shouldn't be in blender repo. */
// #include "bevdebug.c"

/* Use the unused _BM_ELEM_TAG_ALT flag to flag the 'long' loops (parallel to beveled edge)
 * of edge-polygons. */
#define BM_ELEM_LONG_TAG (1 << 6)

/* These flag values will get set on geom we want to return in 'out' slots for edges and verts. */
#define EDGE_OUT 4
#define VERT_OUT 8

/* If we're called from the modifier, tool flags aren't available,
 * but don't need output geometry. */
static void flag_out_edge(BMesh *bm, BMEdge *bme)
{
  if (bm->use_toolflags) {
    BMO_edge_flag_enable(bm, bme, EDGE_OUT);
  }
}

static void flag_out_vert(BMesh *bm, BMVert *bmv)
{
  if (bm->use_toolflags) {
    BMO_vert_flag_enable(bm, bmv, VERT_OUT);
  }
}

static void disable_flag_out_edge(BMesh *bm, BMEdge *bme)
{
  if (bm->use_toolflags) {
    BMO_edge_flag_disable(bm, bme, EDGE_OUT);
  }
}

static void record_face_kind(BevelParams *bp, BMFace *f, FKind fkind)
{
  if (bp->face_hash) {
    BLI_ghash_insert(bp->face_hash, f, POINTER_FROM_INT(fkind));
  }
}

static FKind get_face_kind(BevelParams *bp, BMFace *f)
{
  void *val = BLI_ghash_lookup(bp->face_hash, f);
  return val ? (FKind)POINTER_AS_INT(val) : F_ORIG;
}

/* Are d1 and d2 parallel or nearly so? */
static bool nearly_parallel(const float d1[3], const float d2[3])
{
  float ang;

  ang = angle_v3v3(d1, d2);
  return (fabsf(ang) < BEVEL_EPSILON_ANG) || (fabsf(ang - (float)M_PI) < BEVEL_EPSILON_ANG);
}

/* Make a new BoundVert of the given kind, inserting it at the end of the circular linked
 * list with entry point bv->boundstart, and return it. */
static BoundVert *add_new_bound_vert(MemArena *mem_arena, VMesh *vm, const float co[3])
{
  BoundVert *ans = (BoundVert *)BLI_memarena_alloc(mem_arena, sizeof(BoundVert));

  copy_v3_v3(ans->nv.co, co);
  if (!vm->boundstart) {
    ans->index = 0;
    vm->boundstart = ans;
    ans->next = ans->prev = ans;
  }
  else {
    BoundVert *tail = vm->boundstart->prev;
    ans->index = tail->index + 1;
    ans->prev = tail;
    ans->next = vm->boundstart;
    tail->next = ans;
    vm->boundstart->prev = ans;
  }
  ans->profile.super_r = PRO_LINE_R;
  ans->adjchain = NULL;
  ans->sinratio = 1.0f;
  ans->visited = false;
  ans->any_seam = false;
  ans->is_arc_start = false;
  ans->is_patch_start = false;
  ans->is_profile_start = false;
  vm->count++;
  return ans;
}

BLI_INLINE void adjust_bound_vert(BoundVert *bv, const float co[3])
{
  copy_v3_v3(bv->nv.co, co);
}

/* Mesh verts are indexed (i, j, k) where
 * i = boundvert index (0 <= i < nv)
 * j = ring index (0 <= j <= ns2)
 * k = segment index (0 <= k <= ns)
 * Not all of these are used, and some will share BMVerts. */
static NewVert *mesh_vert(VMesh *vm, int i, int j, int k)
{
  int nj = (vm->seg / 2) + 1;
  int nk = vm->seg + 1;

  return &vm->mesh[i * nk * nj + j * nk + k];
}

static void create_mesh_bmvert(BMesh *bm, VMesh *vm, int i, int j, int k, BMVert *eg)
{
  NewVert *nv = mesh_vert(vm, i, j, k);
  nv->v = BM_vert_create(bm, nv->co, eg, BM_CREATE_NOP);
  BM_elem_flag_disable(nv->v, BM_ELEM_TAG);
  flag_out_vert(bm, nv->v);
}

static void copy_mesh_vert(VMesh *vm, int ito, int jto, int kto, int ifrom, int jfrom, int kfrom)
{
  NewVert *nvto, *nvfrom;

  nvto = mesh_vert(vm, ito, jto, kto);
  nvfrom = mesh_vert(vm, ifrom, jfrom, kfrom);
  nvto->v = nvfrom->v;
  copy_v3_v3(nvto->co, nvfrom->co);
}

/* Find the EdgeHalf in bv's array that has edge bme. */
static EdgeHalf *find_edge_half(BevVert *bv, BMEdge *bme)
{
  int i;

  for (i = 0; i < bv->edgecount; i++) {
    if (bv->edges[i].e == bme) {
      return &bv->edges[i];
    }
  }
  return NULL;
}

/* Find the BevVert corresponding to BMVert bmv. */
static BevVert *find_bevvert(BevelParams *bp, BMVert *bmv)
{
  return BLI_ghash_lookup(bp->vert_hash, bmv);
}

/**
 * Find the EdgeHalf representing the other end of e->e.
 * \return Return other end's BevVert in *r_bvother, if r_bvother is provided. That may not have
 * been constructed yet, in which case return NULL.
 */
static EdgeHalf *find_other_end_edge_half(BevelParams *bp, EdgeHalf *e, BevVert **r_bvother)
{
  BevVert *bvo;
  EdgeHalf *eother;

  bvo = find_bevvert(bp, e->is_rev ? e->e->v1 : e->e->v2);
  if (bvo) {
    if (r_bvother) {
      *r_bvother = bvo;
    }
    eother = find_edge_half(bvo, e->e);
    BLI_assert(eother != NULL);
    return eother;
  }
  else if (r_bvother) {
    *r_bvother = NULL;
  }
  return NULL;
}

/* Return the next EdgeHalf after from_e that is beveled.
 * If from_e is NULL, find the first beveled edge. */
static EdgeHalf *next_bev(BevVert *bv, EdgeHalf *from_e)
{
  EdgeHalf *e;

  if (from_e == NULL) {
    from_e = &bv->edges[bv->edgecount - 1];
  }
  e = from_e;
  do {
    if (e->is_bev) {
      return e;
    }
  } while ((e = e->next) != from_e);
  return NULL;
}

/* Return the count of edges between e1 and e2 when going around bv CCW. */
static int count_ccw_edges_between(EdgeHalf *e1, EdgeHalf *e2)
{
  int cnt = 0;
  EdgeHalf *e = e1;

  do {
    if (e == e2) {
      break;
    }
    e = e->next;
    cnt++;
  } while (e != e1);
  return cnt;
}

/* Assume bme1 and bme2 both share some vert. Do they share a face?
 * If they share a face then there is some loop around bme1 that is in a face
 * where the next or previous edge in the face must be bme2. */
static bool edges_face_connected_at_vert(BMEdge *bme1, BMEdge *bme2)
{
  BMLoop *l;
  BMIter iter;

  BM_ITER_ELEM (l, &iter, bme1, BM_LOOPS_OF_EDGE) {
    if (l->prev->e == bme2 || l->next->e == bme2) {
      return true;
    }
  }
  return false;
}

/**
 * Return a good representative face (for materials, etc.) for faces
 * created around/near BoundVert v.
 * Sometimes care about a second choice, if there is one.
 * If r_fother parameter is non-NULL and there is another, different,
 * possible frep, return the other one in that parameter.
 */
static BMFace *boundvert_rep_face(BoundVert *v, BMFace **r_fother)
{
  BMFace *frep, *frep2;

  frep2 = NULL;
  if (v->ebev) {
    frep = v->ebev->fprev;
    if (v->efirst->fprev != frep) {
      frep2 = v->efirst->fprev;
    }
  }
  else if (v->efirst) {
    frep = v->efirst->fprev;
    if (frep) {
      if (v->elast->fnext != frep) {
        frep2 = v->elast->fnext;
      }
      else if (v->efirst->fnext != frep) {
        frep2 = v->efirst->fnext;
      }
      else if (v->elast->fprev != frep) {
        frep2 = v->efirst->fprev;
      }
    }
    else if (v->efirst->fnext) {
      frep = v->efirst->fnext;
      if (v->elast->fnext != frep) {
        frep2 = v->elast->fnext;
      }
    }
    else if (v->elast->fprev) {
      frep = v->elast->fprev;
    }
  }
  else if (v->prev->elast) {
    frep = v->prev->elast->fnext;
    if (v->next->efirst) {
      if (frep) {
        frep2 = v->next->efirst->fprev;
      }
      else {
        frep = v->next->efirst->fprev;
      }
    }
  }
  else {
    frep = NULL;
  }
  if (r_fother) {
    *r_fother = frep2;
  }
  return frep;
}

/**
 * Make ngon from verts alone.
 * Make sure to properly copy face attributes and do custom data interpolation from
 * corresponding elements of face_arr, if that is non-NULL, else from facerep.
 * If edge_arr is non-NULL, then for interpolation purposes only, the corresponding
 * elements of vert_arr are snapped to any non-NULL edges in that array.
 * If mat_nr >= 0 then the material of the face is set to that.
 *
 * \note ALL face creation goes through this function, this is important to keep!
 */
static BMFace *bev_create_ngon(BMesh *bm,
                               BMVert **vert_arr,
                               const int totv,
                               BMFace **face_arr,
                               BMFace *facerep,
                               BMEdge **edge_arr,
                               int mat_nr,
                               bool do_interp)
{
  BMIter iter;
  BMLoop *l;
  BMFace *f, *interp_f;
  BMEdge *bme;
  float save_co[3];
  int i;

  f = BM_face_create_verts(bm, vert_arr, totv, facerep, BM_CREATE_NOP, true);

  if ((facerep || (face_arr && face_arr[0])) && f) {
    BM_elem_attrs_copy(bm, bm, facerep ? facerep : face_arr[0], f);
    if (do_interp) {
      i = 0;
      BM_ITER_ELEM (l, &iter, f, BM_LOOPS_OF_FACE) {
        if (face_arr) {
          /* Assume loops of created face are in same order as verts. */
          BLI_assert(l->v == vert_arr[i]);
          interp_f = face_arr[i];
        }
        else {
          interp_f = facerep;
        }
        if (interp_f) {
          bme = NULL;
          if (edge_arr) {
            bme = edge_arr[i];
          }
          if (bme) {
            copy_v3_v3(save_co, l->v->co);
            closest_to_line_segment_v3(l->v->co, save_co, bme->v1->co, bme->v2->co);
          }
          BM_loop_interp_from_face(bm, l, interp_f, true, true);
          if (bme) {
            copy_v3_v3(l->v->co, save_co);
          }
        }
        i++;
      }
    }
  }

  /* Not essential for bevels own internal logic,
   * this is done so the operator can select newly created geometry. */
  if (f) {
    BM_elem_flag_enable(f, BM_ELEM_TAG);
    BM_ITER_ELEM (bme, &iter, f, BM_EDGES_OF_FACE) {
      flag_out_edge(bm, bme);
    }
  }

  if (mat_nr >= 0) {
    f->mat_nr = (short)mat_nr;
  }
  return f;
}

static BMFace *bev_create_quad(BMesh *bm,
                               BMVert *v1,
                               BMVert *v2,
                               BMVert *v3,
                               BMVert *v4,
                               BMFace *f1,
                               BMFace *f2,
                               BMFace *f3,
                               BMFace *f4,
                               int mat_nr)
{
  BMVert *varr[4] = {v1, v2, v3, v4};
  BMFace *farr[4] = {f1, f2, f3, f4};
  return bev_create_ngon(bm, varr, 4, farr, f1, NULL, mat_nr, true);
}

static BMFace *bev_create_quad_ex(BMesh *bm,
                                  BMVert *v1,
                                  BMVert *v2,
                                  BMVert *v3,
                                  BMVert *v4,
                                  BMFace *f1,
                                  BMFace *f2,
                                  BMFace *f3,
                                  BMFace *f4,
                                  BMEdge *e1,
                                  BMEdge *e2,
                                  BMEdge *e3,
                                  BMEdge *e4,
                                  int mat_nr)
{
  BMVert *varr[4] = {v1, v2, v3, v4};
  BMFace *farr[4] = {f1, f2, f3, f4};
  BMEdge *earr[4] = {e1, e2, e3, e4};
  return bev_create_ngon(bm, varr, 4, farr, f1, earr, mat_nr, true);
}

/* Is Loop layer layer_index contiguous across shared vertex of l1 and l2? */
static bool contig_ldata_across_loops(BMesh *bm, BMLoop *l1, BMLoop *l2, int layer_index)
{
  const int offset = bm->ldata.layers[layer_index].offset;
  const int type = bm->ldata.layers[layer_index].type;

  return CustomData_data_equals(
      type, (char *)l1->head.data + offset, (char *)l2->head.data + offset);
}

/* Are all loop layers with have math (e.g., UVs)
 * contiguous from face f1 to face f2 across edge e? */
static bool contig_ldata_across_edge(BMesh *bm, BMEdge *e, BMFace *f1, BMFace *f2)
{
  BMLoop *lef1, *lef2;
  BMLoop *lv1f1, *lv1f2, *lv2f1, *lv2f2;
  BMVert *v1, *v2;
  int i;

  if (bm->ldata.totlayer == 0) {
    return true;
  }

  v1 = e->v1;
  v2 = e->v2;
  if (!BM_edge_loop_pair(e, &lef1, &lef2)) {
    return false;
  }
  if (lef1->f == f2) {
    SWAP(BMLoop *, lef1, lef2);
  }

  if (lef1->v == v1) {
    lv1f1 = lef1;
    lv2f1 = BM_face_other_edge_loop(f1, e, v2);
  }
  else {
    lv2f1 = lef1;
    lv1f1 = BM_face_other_edge_loop(f1, e, v1);
  }

  if (lef2->v == v1) {
    lv1f2 = lef2;
    lv2f2 = BM_face_other_edge_loop(f2, e, v2);
  }
  else {
    lv2f2 = lef2;
    lv1f2 = BM_face_other_edge_loop(f2, e, v1);
  }

  for (i = 0; i < bm->ldata.totlayer; i++) {
    if (CustomData_layer_has_math(&bm->ldata, i) &&
        (!contig_ldata_across_loops(bm, lv1f1, lv1f2, i) ||
         !contig_ldata_across_loops(bm, lv2f1, lv2f2, i))) {
      return false;
    }
  }
  return true;
}

/* Merge (using average) all the UV values for loops of v's faces.
 * Caller should ensure that no seams are violated by doing this. */
static void bev_merge_uvs(BMesh *bm, BMVert *v)
{
  BMIter iter;
  MLoopUV *luv;
  BMLoop *l;
  float uv[2];
  int n;
  int num_of_uv_layers = CustomData_number_of_layers(&bm->ldata, CD_MLOOPUV);
  int i;

  for (i = 0; i < num_of_uv_layers; i++) {
    int cd_loop_uv_offset = CustomData_get_n_offset(&bm->ldata, CD_MLOOPUV, i);

    if (cd_loop_uv_offset == -1) {
      return;
    }

    n = 0;
    zero_v2(uv);
    BM_ITER_ELEM (l, &iter, v, BM_LOOPS_OF_VERT) {
      luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
      add_v2_v2(uv, luv->uv);
      n++;
    }
    if (n > 1) {
      mul_v2_fl(uv, 1.0f / (float)n);
      BM_ITER_ELEM (l, &iter, v, BM_LOOPS_OF_VERT) {
        luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
        copy_v2_v2(luv->uv, uv);
      }
    }
  }
}

/* Merge (using average) the UV values for two specific loops of v: those for faces containing v,
 * and part of faces that share edge bme. */
static void bev_merge_edge_uvs(BMesh *bm, BMEdge *bme, BMVert *v)
{
  BMIter iter;
  MLoopUV *luv;
  BMLoop *l, *l1, *l2;
  float uv[2];
  int num_of_uv_layers = CustomData_number_of_layers(&bm->ldata, CD_MLOOPUV);
  int i;

  l1 = NULL;
  l2 = NULL;
  BM_ITER_ELEM (l, &iter, v, BM_LOOPS_OF_VERT) {
    if (l->e == bme) {
      l1 = l;
    }
    else if (l->prev->e == bme) {
      l2 = l;
    }
  }
  if (l1 == NULL || l2 == NULL) {
    return;
  }

  for (i = 0; i < num_of_uv_layers; i++) {
    int cd_loop_uv_offset = CustomData_get_n_offset(&bm->ldata, CD_MLOOPUV, i);

    if (cd_loop_uv_offset == -1) {
      return;
    }

    zero_v2(uv);
    luv = BM_ELEM_CD_GET_VOID_P(l1, cd_loop_uv_offset);
    add_v2_v2(uv, luv->uv);
    luv = BM_ELEM_CD_GET_VOID_P(l2, cd_loop_uv_offset);
    add_v2_v2(uv, luv->uv);
    mul_v2_fl(uv, 0.5f);
    luv = BM_ELEM_CD_GET_VOID_P(l1, cd_loop_uv_offset);
    copy_v2_v2(luv->uv, uv);
    luv = BM_ELEM_CD_GET_VOID_P(l2, cd_loop_uv_offset);
    copy_v2_v2(luv->uv, uv);
  }
}

/* Calculate coordinates of a point a distance d from v on e->e and return it in slideco. */
static void slide_dist(EdgeHalf *e, BMVert *v, float d, float r_slideco[3])
{
  float dir[3], len;

  sub_v3_v3v3(dir, v->co, BM_edge_other_vert(e->e, v)->co);
  len = normalize_v3(dir);
  if (d > len) {
    d = len - (float)(50.0 * BEVEL_EPSILON_D);
  }
  copy_v3_v3(r_slideco, v->co);
  madd_v3_v3fl(r_slideco, dir, -d);
}

/* Is co not on the edge e? If not, return the closer end of e in ret_closer_v. */
static bool is_outside_edge(EdgeHalf *e, const float co[3], BMVert **ret_closer_v)
{
  float h[3], u[3], lambda, lenu, *l1 = e->e->v1->co;

  sub_v3_v3v3(u, e->e->v2->co, l1);
  sub_v3_v3v3(h, co, l1);
  lenu = normalize_v3(u);
  lambda = dot_v3v3(u, h);
  if (lambda <= -BEVEL_EPSILON_BIG * lenu) {
    *ret_closer_v = e->e->v1;
    return true;
  }
  else if (lambda >= (1.0f + BEVEL_EPSILON_BIG) * lenu) {
    *ret_closer_v = e->e->v2;
    return true;
  }
  else {
    return false;
  }
}

/* Return whether the angle is less than, equal to, or larger than 180 degrees. */
static int edges_angle_kind(EdgeHalf *e1, EdgeHalf *e2, BMVert *v)
{
  BMVert *v1, *v2;
  float dir1[3], dir2[3], cross[3], *no, dot;

  v1 = BM_edge_other_vert(e1->e, v);
  v2 = BM_edge_other_vert(e2->e, v);
  sub_v3_v3v3(dir1, v->co, v1->co);
  sub_v3_v3v3(dir2, v->co, v2->co);
  normalize_v3(dir1);
  normalize_v3(dir2);
  /* Angles are in [0,pi]. Need to compare cross product with normal to see if they are reflex. */
  cross_v3_v3v3(cross, dir1, dir2);
  normalize_v3(cross);
  if (e1->fnext) {
    no = e1->fnext->no;
  }
  else if (e2->fprev) {
    no = e2->fprev->no;
  }
  else {
    no = v->no;
  }
  dot = dot_v3v3(cross, no);
  if (fabsf(dot) < BEVEL_EPSILON_BIG) {
    return ANGLE_STRAIGHT;
  }
  else if (dot < 0.0f) {
    return ANGLE_LARGER;
  }
  else {
    return ANGLE_SMALLER;
  }
}

/* co should be approximately on the plane between e1 and e2, which share common vert v and common
 * face f (which cannot be NULL). Is it between those edges, sweeping CCW? */
static bool point_between_edges(float co[3], BMVert *v, BMFace *f, EdgeHalf *e1, EdgeHalf *e2)
{
  BMVert *v1, *v2;
  float dir1[3], dir2[3], dirco[3], no[3];
  float ang11, ang1co;

  v1 = BM_edge_other_vert(e1->e, v);
  v2 = BM_edge_other_vert(e2->e, v);
  sub_v3_v3v3(dir1, v->co, v1->co);
  sub_v3_v3v3(dir2, v->co, v2->co);
  sub_v3_v3v3(dirco, v->co, co);
  normalize_v3(dir1);
  normalize_v3(dir2);
  normalize_v3(dirco);
  ang11 = angle_normalized_v3v3(dir1, dir2);
  ang1co = angle_normalized_v3v3(dir1, dirco);
  /* Angles are in [0,pi]. Need to compare cross product with normal to see if they are reflex. */
  cross_v3_v3v3(no, dir1, dir2);
  if (dot_v3v3(no, f->no) < 0.0f) {
    ang11 = (float)(M_PI * 2.0) - ang11;
  }
  cross_v3_v3v3(no, dir1, dirco);
  if (dot_v3v3(no, f->no) < 0.0f) {
    ang1co = (float)(M_PI * 2.0) - ang1co;
  }
  return (ang11 - ang1co > -BEVEL_EPSILON_ANG);
}

/**
 * Calculate the meeting point between the offset edges for e1 and e2, putting answer in meetco.
 * e1 and e2 share vertex v and face f (may be NULL) and viewed from the normal side of
 * the bevel vertex, e1 precedes e2 in CCW order.
 * Offset edge is on right of both edges, where e1 enters v and e2 leave it.
 * When offsets are equal, the new point is on the edge bisector, with length offset/sin(angle/2),
 * but if the offsets are not equal (we allow for because the bevel modifier has edge weights that
 * may lead to different offsets) then the meeting point can be found by intersecting offset lines.
 * If making the meeting point significantly changes the left or right offset from the user spec,
 * record the change in offset_l (or offset_r); later we can tell that a change has happened
 * because the offset will differ from its original value in offset_l_spec (or offset_r_spec).
 *
 * \param edges_between: If this is true, there are edges between e1 and e2 in CCW order so they
 * don't share a common face. We want the meeting point to be on an existing face so it
 * should be dropped onto one of the intermediate faces, if possible.
 * \param e_in_plane: If we need to drop from the calculated offset lines to one of the faces,
 * we don't want to drop onto the 'in plane' face, so if this is not null skip this edge's faces.
 */
static void offset_meet(EdgeHalf *e1,
                        EdgeHalf *e2,
                        BMVert *v,
                        BMFace *f,
                        bool edges_between,
                        float meetco[3],
                        const EdgeHalf *e_in_plane)
{
  float dir1[3], dir2[3], dir1n[3], dir2p[3], norm_v[3], norm_v1[3], norm_v2[3];
  float norm_perp1[3], norm_perp2[3], off1a[3], off1b[3], off2a[3], off2b[3];
  float isect2[3], dropco[3], plane[4];
  float ang, d;
  BMVert *closer_v;
  EdgeHalf *e, *e1next, *e2prev;
  BMFace *fnext;
  int isect_kind;

  /* Get direction vectors for two offset lines. */
  sub_v3_v3v3(dir1, v->co, BM_edge_other_vert(e1->e, v)->co);
  sub_v3_v3v3(dir2, BM_edge_other_vert(e2->e, v)->co, v->co);

  if (edges_between) {
    e1next = e1->next;
    e2prev = e2->prev;
    sub_v3_v3v3(dir1n, BM_edge_other_vert(e1next->e, v)->co, v->co);
    sub_v3_v3v3(dir2p, v->co, BM_edge_other_vert(e2prev->e, v)->co);
  }
  else {
    /* Shut up 'maybe unused' warnings. */
    zero_v3(dir1n);
    zero_v3(dir2p);
  }

  ang = angle_v3v3(dir1, dir2);
  if (ang < BEVEL_EPSILON_ANG) {
    /* Special case: e1 and e2 are parallel; put offset point perp to both, from v.
     * need to find a suitable plane.
     * This code used to just use offset and dir1, but that makes for visible errors
     * on a circle with > 200 sides, which trips this "nearly perp" code (see T61214).
     * so use the average of the two, and the offset formula for angle bisector.
     * If offsets are different, we're out of luck:
     * Use the max of the two (so get consistent looking results if the same situation
     * arises elsewhere in the object but with opposite roles for e1 and e2. */
    if (f) {
      copy_v3_v3(norm_v, f->no);
    }
    else {
      copy_v3_v3(norm_v, v->no);
    }
    add_v3_v3(dir1, dir2);
    cross_v3_v3v3(norm_perp1, dir1, norm_v);
    normalize_v3(norm_perp1);
    copy_v3_v3(off1a, v->co);
    d = max_ff(e1->offset_r, e2->offset_l);
    d = d / cosf(ang / 2.0f);
    madd_v3_v3fl(off1a, norm_perp1, d);
    copy_v3_v3(meetco, off1a);
  }
  else if (fabsf(ang - (float)M_PI) < BEVEL_EPSILON_ANG) {
    /* Special case: e1 and e2 are antiparallel, so bevel is into a zero-area face.
     * Just make the offset point on the common line, at offset distance from v. */
    d = max_ff(e1->offset_r, e2->offset_l);
    slide_dist(e2, v, d, meetco);
  }
  else {
    /* Get normal to plane where meet point should be, using cross product instead of f->no
     * in case f is non-planar.
     * Except: sometimes locally there can be a small angle between dir1 and dir2 that leads
     * to a normal that is actually almost perpendicular to the face normal;
     * in this case it looks wrong to use the local (cross-product) normal, so use the face normal
     * if the angle between dir1 and dir2 is smallish.
     * If e1-v-e2 is a reflex angle (viewed from vertex normal side), need to flip.
     * Use f->no to figure out which side to look at angle from, as even if f is non-planar,
     * will be more accurate than vertex normal. */
    if (f && ang < BEVEL_SMALL_ANG) {
      copy_v3_v3(norm_v1, f->no);
      copy_v3_v3(norm_v2, f->no);
    }
    else if (!edges_between) {
      cross_v3_v3v3(norm_v1, dir2, dir1);
      normalize_v3(norm_v1);
      if (dot_v3v3(norm_v1, f ? f->no : v->no) < 0.0f) {
        negate_v3(norm_v1);
      }
      copy_v3_v3(norm_v2, norm_v1);
    }
    else {
      /* Separate faces; get face norms at corners for each separately. */
      cross_v3_v3v3(norm_v1, dir1n, dir1);
      normalize_v3(norm_v1);
      f = e1->fnext;
      if (dot_v3v3(norm_v1, f ? f->no : v->no) < 0.0f) {
        negate_v3(norm_v1);
      }
      cross_v3_v3v3(norm_v2, dir2, dir2p);
      normalize_v3(norm_v2);
      f = e2->fprev;
      if (dot_v3v3(norm_v2, f ? f->no : v->no) < 0.0f) {
        negate_v3(norm_v2);
      }
    }

    /* Get vectors perp to each edge, perp to norm_v, and pointing into face. */
    cross_v3_v3v3(norm_perp1, dir1, norm_v1);
    cross_v3_v3v3(norm_perp2, dir2, norm_v2);
    normalize_v3(norm_perp1);
    normalize_v3(norm_perp2);

    /* Get points that are offset distances from each line, then another point on each line. */
    copy_v3_v3(off1a, v->co);
    madd_v3_v3fl(off1a, norm_perp1, e1->offset_r);
    add_v3_v3v3(off1b, off1a, dir1);
    copy_v3_v3(off2a, v->co);
    madd_v3_v3fl(off2a, norm_perp2, e2->offset_l);
    add_v3_v3v3(off2b, off2a, dir2);

    /* Intersect the offset lines. */
    isect_kind = isect_line_line_v3(off1a, off1b, off2a, off2b, meetco, isect2);
    if (isect_kind == 0) {
      /* Lines are collinear: we already tested for this, but this used a different epsilon. */
      copy_v3_v3(meetco, off1a); /* Just to do something. */
    }
    else {
      /* The lines intersect, but is it at a reasonable place?
       * One problem to check: if one of the offsets is 0, then we don't want an intersection
       * that is outside that edge itself. This can happen if angle between them is > 180 degrees,
       * or if the offset amount is > the edge length. */
      if (e1->offset_r == 0.0f && is_outside_edge(e1, meetco, &closer_v)) {
        copy_v3_v3(meetco, closer_v->co);
      }
      if (e2->offset_l == 0.0f && is_outside_edge(e2, meetco, &closer_v)) {
        copy_v3_v3(meetco, closer_v->co);
      }
      if (edges_between && e1->offset_r > 0.0f && e2->offset_l > 0.0f) {
        /* Try to drop meetco to a face between e1 and e2. */
        if (isect_kind == 2) {
          /* Lines didn't meet in 3d: get average of meetco and isect2. */
          mid_v3_v3v3(meetco, meetco, isect2);
        }
        for (e = e1; e != e2; e = e->next) {
          fnext = e->fnext;
          if (!fnext) {
            continue;
          }
          plane_from_point_normal_v3(plane, v->co, fnext->no);
          closest_to_plane_normalized_v3(dropco, plane, meetco);
          /* Don't drop to the faces next to the in plane edge. */
          if (e_in_plane) {
            ang = angle_v3v3(fnext->no, e_in_plane->fnext->no);
            if ((fabsf(ang) < BEVEL_SMALL_ANG) || (fabsf(ang - (float)M_PI) < BEVEL_SMALL_ANG)) {
              continue;
            }
          }
          if (point_between_edges(dropco, v, fnext, e, e->next)) {
            copy_v3_v3(meetco, dropco);
            break;
          }
        }
      }
    }
  }
}

/* Chosen so 1/sin(BEVEL_GOOD_ANGLE) is about 4, giving that expansion factor to bevel width. */
#define BEVEL_GOOD_ANGLE 0.25f

/**
 * Calculate the meeting point between e1 and e2 (one of which should have zero offsets),
 * where e1 precedes e2 in CCW order around their common vertex v (viewed from normal side).
 * If r_angle is provided, return the angle between e and emeet in *r_angle.
 * If the angle is 0, or it is 180 degrees or larger, there will be no meeting point;
 * return false in that case, else true.
 */
static bool offset_meet_edge(
    EdgeHalf *e1, EdgeHalf *e2, BMVert *v, float meetco[3], float *r_angle)
{
  float dir1[3], dir2[3], fno[3], ang, sinang;

  sub_v3_v3v3(dir1, BM_edge_other_vert(e1->e, v)->co, v->co);
  sub_v3_v3v3(dir2, BM_edge_other_vert(e2->e, v)->co, v->co);
  normalize_v3(dir1);
  normalize_v3(dir2);

  /* Find angle from dir1 to dir2 as viewed from vertex normal side. */
  ang = angle_normalized_v3v3(dir1, dir2);
  if (fabsf(ang) < BEVEL_GOOD_ANGLE) {
    if (r_angle) {
      *r_angle = 0.0f;
    }
    return false;
  }
  cross_v3_v3v3(fno, dir1, dir2);
  if (dot_v3v3(fno, v->no) < 0.0f) {
    ang = 2.0f * (float)M_PI - ang; /* Angle is reflex. */
    if (r_angle) {
      *r_angle = ang;
    }
    return false;
  }
  if (r_angle) {
    *r_angle = ang;
  }

  if (fabsf(ang - (float)M_PI) < BEVEL_GOOD_ANGLE) {
    return false;
  }

  sinang = sinf(ang);

  copy_v3_v3(meetco, v->co);
  if (e1->offset_r == 0.0f) {
    madd_v3_v3fl(meetco, dir1, e2->offset_l / sinang);
  }
  else {
    madd_v3_v3fl(meetco, dir2, e1->offset_r / sinang);
  }
  return true;
}

/**
 * Return true if it will look good to put the meeting point where offset_on_edge_between
 * would put it. This means that neither side sees a reflex angle.
 */
static bool good_offset_on_edge_between(EdgeHalf *e1, EdgeHalf *e2, EdgeHalf *emid, BMVert *v)
{
  float ang;
  float meet[3];

  return offset_meet_edge(e1, emid, v, meet, &ang) && offset_meet_edge(emid, e2, v, meet, &ang);
}

/**
 * Calculate the best place for a meeting point for the offsets from edges e1 and e2 on the
 * in-between edge emid. Viewed from the vertex normal side, the CCW order of these edges is e1,
 * emid, e2. Return true if we placed meetco as compromise between where two edges met. If we did,
 * put the ratio of sines of angles in *r_sinratio too.
 */
static bool offset_on_edge_between(
    EdgeHalf *e1, EdgeHalf *e2, EdgeHalf *emid, BMVert *v, float meetco[3], float *r_sinratio)
{
  float ang1, ang2;
  float meet1[3], meet2[3];
  bool ok1, ok2;
  bool retval = false;

  BLI_assert(e1->is_bev && e2->is_bev && !emid->is_bev);

  ok1 = offset_meet_edge(e1, emid, v, meet1, &ang1);
  ok2 = offset_meet_edge(emid, e2, v, meet2, &ang2);
  if (ok1 && ok2) {
    mid_v3_v3v3(meetco, meet1, meet2);
    if (r_sinratio) {
      /* ang1 should not be 0, but be paranoid. */
      *r_sinratio = (ang1 == 0.0f) ? 1.0f : sinf(ang2) / sinf(ang1);
    }
    retval = true;
  }
  else if (ok1 && !ok2) {
    copy_v3_v3(meetco, meet1);
  }
  else if (!ok1 && ok2) {
    copy_v3_v3(meetco, meet2);
  }
  else {
    /* Neither offset line met emid.
     * This should only happen if all three lines are on top of each other. */
    slide_dist(emid, v, e1->offset_r, meetco);
  }

  return retval;
}

/* Offset by e->offset in plane with normal plane_no, on left if left==true, else on right.
 * If plane_no is NULL, choose an arbitrary plane different from eh's direction. */
static void offset_in_plane(EdgeHalf *e, const float plane_no[3], bool left, float r_co[3])
{
  float dir[3], no[3], fdir[3];
  BMVert *v;

  v = e->is_rev ? e->e->v2 : e->e->v1;

  sub_v3_v3v3(dir, BM_edge_other_vert(e->e, v)->co, v->co);
  normalize_v3(dir);
  if (plane_no) {
    copy_v3_v3(no, plane_no);
  }
  else {
    zero_v3(no);
    if (fabsf(dir[0]) < fabsf(dir[1])) {
      no[0] = 1.0f;
    }
    else {
      no[1] = 1.0f;
    }
  }
  if (left) {
    cross_v3_v3v3(fdir, dir, no);
  }
  else {
    cross_v3_v3v3(fdir, no, dir);
  }
  normalize_v3(fdir);
  copy_v3_v3(r_co, v->co);
  madd_v3_v3fl(r_co, fdir, left ? e->offset_l : e->offset_r);
}

/* Calculate the point on e where line (co_a, co_b) comes closest to and return it in projco. */
static void project_to_edge(const BMEdge *e,
                            const float co_a[3],
                            const float co_b[3],
                            float projco[3])
{
  float otherco[3];

  if (!isect_line_line_v3(e->v1->co, e->v2->co, co_a, co_b, projco, otherco)) {
#ifdef BEVEL_ASSERT_PROJECT
    BLI_assert(!"project meet failure");
#endif
    copy_v3_v3(projco, e->v1->co);
  }
}

/* If there is a bndv->ebev edge, find the mid control point if necessary.
 * It is the closest point on the beveled edge to the line segment between bndv and bndv->next.  */
static void set_profile_params(BevelParams *bp, BevVert *bv, BoundVert *bndv)
{
  float start[3], end[3], co3[3], d1[3], d2[3];
  bool do_linear_interp = true;
  EdgeHalf *e = bndv->ebev;
  Profile *pro = &bndv->profile;

  copy_v3_v3(start, bndv->nv.co);
  copy_v3_v3(end, bndv->next->nv.co);
  if (e) {
    do_linear_interp = false;
    pro->super_r = bp->pro_super_r;
    /* Projection direction is direction of the edge. */
    sub_v3_v3v3(pro->proj_dir, e->e->v1->co, e->e->v2->co);
    if (e->is_rev) {
      negate_v3(pro->proj_dir);
    }
    normalize_v3(pro->proj_dir);
    project_to_edge(e->e, start, end, pro->middle);
    copy_v3_v3(pro->start, start);
    copy_v3_v3(pro->end, end);
    /* Default plane to project onto is the one with triangle start - middle - end in it. */
    sub_v3_v3v3(d1, pro->middle, start);
    sub_v3_v3v3(d2, pro->middle, end);
    normalize_v3(d1);
    normalize_v3(d2);
    cross_v3_v3v3(pro->plane_no, d1, d2);
    normalize_v3(pro->plane_no);
    if (nearly_parallel(d1, d2)) {
      /* Start - middle - end are collinear.
       * It should be the case that beveled edge is coplanar with two boundary verts.
       * We want to move the profile to that common plane, if possible.
       * That makes the multi-segment bevels curve nicely in that plane, as users expect.
       * The new middle should be either v (when neighbor edges are unbeveled)
       * or the intersection of the offset lines (if they are).
       * If the profile is going to lead into unbeveled edges on each side
       * (that is, both BoundVerts are "on-edge" points on non-beveled edges). */
      copy_v3_v3(pro->middle, bv->v->co);
      if (e->prev->is_bev && e->next->is_bev && bv->selcount >= 3) {
        /* Want mid at the meet point of next and prev offset edges. */
        float d3[3], d4[3], co4[3], meetco[3], isect2[3];
        int isect_kind;

        sub_v3_v3v3(d3, e->prev->e->v1->co, e->prev->e->v2->co);
        sub_v3_v3v3(d4, e->next->e->v1->co, e->next->e->v2->co);
        normalize_v3(d3);
        normalize_v3(d4);
        if (nearly_parallel(d3, d4)) {
          /* Offset lines are collinear - want linear interpolation. */
          mid_v3_v3v3(pro->middle, start, end);
          do_linear_interp = true;
        }
        else {
          add_v3_v3v3(co3, start, d3);
          add_v3_v3v3(co4, end, d4);
          isect_kind = isect_line_line_v3(start, co3, end, co4, meetco, isect2);
          if (isect_kind != 0) {
            copy_v3_v3(pro->middle, meetco);
          }
          else {
            /* Offset lines don't intersect - want linear interpolation. */
            mid_v3_v3v3(pro->middle, start, end);
            do_linear_interp = true;
          }
        }
      }
      copy_v3_v3(pro->end, end);
      sub_v3_v3v3(d1, pro->middle, start);
      normalize_v3(d1);
      sub_v3_v3v3(d2, pro->middle, end);
      normalize_v3(d2);
      cross_v3_v3v3(pro->plane_no, d1, d2);
      normalize_v3(pro->plane_no);
      if (nearly_parallel(d1, d2)) {
        /* Whole profile is collinear with edge: just interpolate. */
        do_linear_interp = true;
      }
      else {
        copy_v3_v3(pro->plane_co, bv->v->co);
        copy_v3_v3(pro->proj_dir, pro->plane_no);
      }
    }
    copy_v3_v3(pro->plane_co, start);
  }
  else if (bndv->is_arc_start) {
    /* Assume pro->middle was already set. */
    copy_v3_v3(pro->start, start);
    copy_v3_v3(pro->end, end);
    pro->super_r = PRO_CIRCLE_R;
    zero_v3(pro->plane_co);
    zero_v3(pro->plane_no);
    zero_v3(pro->proj_dir);
    do_linear_interp = false;
  }
  else if (bp->vertex_only) {
    copy_v3_v3(pro->start, start);
    copy_v3_v3(pro->middle, bv->v->co);
    copy_v3_v3(pro->end, end);
    pro->super_r = bp->pro_super_r;
    zero_v3(pro->plane_co);
    zero_v3(pro->plane_no);
    zero_v3(pro->proj_dir);
    do_linear_interp = false;
  }

  if (do_linear_interp) {
    pro->super_r = PRO_LINE_R;
    copy_v3_v3(pro->start, start);
    copy_v3_v3(pro->end, end);
    mid_v3_v3v3(pro->middle, start, end);
    /* Won't use projection for this line profile. */
    zero_v3(pro->plane_co);
    zero_v3(pro->plane_no);
    zero_v3(pro->proj_dir);
  }
}

/**
 * Maybe move the profile plane for bndv->ebev to the plane its profile's start, and the
 * original beveled vert, bmv. This will usually be the plane containing its adjacent
 * non-beveled edges, but sometimes the start and the end are not on those edges.
 *
 * Currently just used in #build_boundary_terminal_edge.
 */
static void move_profile_plane(BoundVert *bndv, BMVert *bmvert)
{
  float d1[3], d2[3], no[3], no2[3], no3[3], dot2, dot3;
  Profile *pro = &bndv->profile;

  /* Only do this if projecting, and start, end, and proj_dir are not coplanar. */
  if (is_zero_v3(pro->proj_dir)) {
    return;
  }
  sub_v3_v3v3(d1, bmvert->co, pro->start);
  normalize_v3(d1);
  sub_v3_v3v3(d2, bmvert->co, pro->end);
  normalize_v3(d2);
  cross_v3_v3v3(no, d1, d2);
  cross_v3_v3v3(no2, d1, pro->proj_dir);
  cross_v3_v3v3(no3, d2, pro->proj_dir);
  if (normalize_v3(no) > BEVEL_EPSILON_BIG && normalize_v3(no2) > BEVEL_EPSILON_BIG &&
      normalize_v3(no3) > BEVEL_EPSILON_BIG) {
    dot2 = dot_v3v3(no, no2);
    dot3 = dot_v3v3(no, no3);
    if (fabsf(dot2) < (1 - BEVEL_EPSILON_BIG) && fabsf(dot3) < (1 - BEVEL_EPSILON_BIG)) {
      copy_v3_v3(bndv->profile.plane_no, no);
    }
  }

  /* We've changed the parameters from their defaults, so don't recalculate them later. */
  pro->special_params = true;
}

/**
 * Move the profile plane for the two BoundVerts involved in a weld.
 * We want the plane that is most likely to have the intersections of the
 * two edges' profile projections on it. bndv1 and bndv2 are by construction the
 * intersection points of the outside parts of the profiles.
 * The original vertex should form a third point of the desired plane.
 */
static void move_weld_profile_planes(BevVert *bv, BoundVert *bndv1, BoundVert *bndv2)
{
  float d1[3], d2[3], no[3], no2[3], no3[3], dot1, dot2, l1, l2, l3;

  /* Only do this if projecting, and d1, d2, and proj_dir are not coplanar. */
  if (is_zero_v3(bndv1->profile.proj_dir) || is_zero_v3(bndv2->profile.proj_dir)) {
    return;
  }
  sub_v3_v3v3(d1, bv->v->co, bndv1->nv.co);
  sub_v3_v3v3(d2, bv->v->co, bndv2->nv.co);
  cross_v3_v3v3(no, d1, d2);
  l1 = normalize_v3(no);
  /* "no" is new normal projection plane, but don't move if it is coplanar with both of the
   * projection dirs. */
  cross_v3_v3v3(no2, d1, bndv1->profile.proj_dir);
  l2 = normalize_v3(no2);
  cross_v3_v3v3(no3, d2, bndv2->profile.proj_dir);
  l3 = normalize_v3(no3);
  if (l1 > BEVEL_EPSILON && (l2 > BEVEL_EPSILON || l3 > BEVEL_EPSILON)) {
    dot1 = fabsf(dot_v3v3(no, no2));
    dot2 = fabsf(dot_v3v3(no, no3));
    if (fabsf(dot1 - 1.0f) > BEVEL_EPSILON) {
      copy_v3_v3(bndv1->profile.plane_no, no);
    }
    if (fabsf(dot2 - 1.0f) > BEVEL_EPSILON) {
      copy_v3_v3(bndv2->profile.plane_no, no);
    }
  }

  /* We've changed the parameters from their defaults, so don't recalculate them later. */
  bndv1->profile.special_params = true;
  bndv2->profile.special_params = true;
}

/* Return 1 if a and b are in CCW order on the normal side of f,
 * and -1 if they are reversed, and 0 if there is no shared face f. */
static int bev_ccw_test(BMEdge *a, BMEdge *b, BMFace *f)
{
  BMLoop *la, *lb;

  if (!f) {
    return 0;
  }
  la = BM_face_edge_share_loop(f, a);
  lb = BM_face_edge_share_loop(f, b);
  if (!la || !lb) {
    return 0;
  }
  return lb->next == la ? 1 : -1;
}

/**
 * Fill matrix r_mat so that a point in the sheared parallelogram with corners
 * va, vmid, vb (and the 4th that is implied by it being a parallelogram)
 * is the result of transforming the unit square by multiplication with r_mat.
 * If it can't be done because the parallelogram is degenerate, return false,
 * else return true.
 * Method:
 * Find vo, the origin of the parallelogram with other three points va, vmid, vb.
 * Also find vd, which is in direction normal to parallelogram and 1 unit away
 * from the origin.
 * The quarter circle in first quadrant of unit square will be mapped to the
 * quadrant of a sheared ellipse in the parallelogram, using a matrix.
 * The matrix mat is calculated to map:
 *    (0,1,0) -> va
 *    (1,1,0) -> vmid
 *    (1,0,0) -> vb
 *    (0,1,1) -> vd
 * We want M to make M*A=B where A has the left side above, as columns
 * and B has the right side as columns - both extended into homogeneous coords.
 * So M = B*(Ainverse).  Doing Ainverse by hand gives the code below.
 */
static bool make_unit_square_map(const float va[3],
                                 const float vmid[3],
                                 const float vb[3],
                                 float r_mat[4][4])
{
  float vo[3], vd[3], vb_vmid[3], va_vmid[3], vddir[3];

  sub_v3_v3v3(va_vmid, vmid, va);
  sub_v3_v3v3(vb_vmid, vmid, vb);

  if (is_zero_v3(va_vmid) || is_zero_v3(vb_vmid)) {
    return false;
  }

  if (fabsf(angle_v3v3(va_vmid, vb_vmid) - (float)M_PI) <= BEVEL_EPSILON_ANG) {
    return false;
  }

  sub_v3_v3v3(vo, va, vb_vmid);
  cross_v3_v3v3(vddir, vb_vmid, va_vmid);
  normalize_v3(vddir);
  add_v3_v3v3(vd, vo, vddir);

  /* The cols of m are: {vmid - va, vmid - vb, vmid + vd - va -vb, va + vb - vmid;
   * Blender transform matrices are stored such that m[i][*] is ith column;
   * the last elements of each col remain as they are in unity matrix. */
  sub_v3_v3v3(&r_mat[0][0], vmid, va);
  r_mat[0][3] = 0.0f;
  sub_v3_v3v3(&r_mat[1][0], vmid, vb);
  r_mat[1][3] = 0.0f;
  add_v3_v3v3(&r_mat[2][0], vmid, vd);
  sub_v3_v3(&r_mat[2][0], va);
  sub_v3_v3(&r_mat[2][0], vb);
  r_mat[2][3] = 0.0f;
  add_v3_v3v3(&r_mat[3][0], va, vb);
  sub_v3_v3(&r_mat[3][0], vmid);
  r_mat[3][3] = 1.0f;

  return true;
}

/**
 * Like make_unit_square_map, but this one makes a matrix that transforms the
 * (1,1,1) corner of a unit cube into an arbitrary corner with corner vert d
 * and verts around it a, b, c (in ccw order, viewed from d normal dir).
 * The matrix mat is calculated to map:
 *    (1,0,0) -> va
 *    (0,1,0) -> vb
 *    (0,0,1) -> vc
 *    (1,1,1) -> vd
 * We want M to make M*A=B where A has the left side above, as columns
 * and B has the right side as columns - both extended into homogeneous coords.
 * So M = B*(Ainverse).  Doing Ainverse by hand gives the code below.
 * The cols of M are 1/2{va-vb+vc-vd}, 1/2{-va+vb-vc+vd}, 1/2{-va-vb+vc+vd},
 * and 1/2{va+vb+vc-vd}
 * and Blender matrices have cols at m[i][*].
 */
static void make_unit_cube_map(
    const float va[3], const float vb[3], const float vc[3], const float vd[3], float r_mat[4][4])
{
  copy_v3_v3(r_mat[0], va);
  sub_v3_v3(r_mat[0], vb);
  sub_v3_v3(r_mat[0], vc);
  add_v3_v3(r_mat[0], vd);
  mul_v3_fl(r_mat[0], 0.5f);
  r_mat[0][3] = 0.0f;
  copy_v3_v3(r_mat[1], vb);
  sub_v3_v3(r_mat[1], va);
  sub_v3_v3(r_mat[1], vc);
  add_v3_v3(r_mat[1], vd);
  mul_v3_fl(r_mat[1], 0.5f);
  r_mat[1][3] = 0.0f;
  copy_v3_v3(r_mat[2], vc);
  sub_v3_v3(r_mat[2], va);
  sub_v3_v3(r_mat[2], vb);
  add_v3_v3(r_mat[2], vd);
  mul_v3_fl(r_mat[2], 0.5f);
  r_mat[2][3] = 0.0f;
  copy_v3_v3(r_mat[3], va);
  add_v3_v3(r_mat[3], vb);
  add_v3_v3(r_mat[3], vc);
  sub_v3_v3(r_mat[3], vd);
  mul_v3_fl(r_mat[3], 0.5f);
  r_mat[3][3] = 1.0f;
}

/**
 * Get the coordinate on the superellipse (x^r + y^r = 1), at parameter value x
 * (or, if !rbig, mirrored (y=x)-line).
 * rbig should be true if r > 1.0 and false if <= 1.0.
 * Assume r > 0.0.
 */
static double superellipse_co(double x, float r, bool rbig)
{
  BLI_assert(r > 0.0f);

  /* If r<1, mirror the superellipse function by (y=x)-line to get a numerically stable range
   * Possible because of symmetry, later mirror back. */
  if (rbig) {
    return pow((1.0 - pow(x, r)), (1.0 / r));
  }
  else {
    return 1.0 - pow((1.0 - pow(1.0 - x, r)), (1.0 / r));
  }
}

/**
 * Find the point on given profile at parameter i which goes from 0 to nseg as
 * the profile moves from pro->start to pro->end.
 * We assume that nseg is either the global seg number or a power of 2 less than
 * or equal to the power of 2 >= seg.
 * In the latter case, we subsample the profile for seg_2, which will not necessarily
 * give equal spaced chords, but is in fact more what is desired by the cubic subdivision
 * method used to make the vmesh pattern.
 */
static void get_profile_point(BevelParams *bp, const Profile *pro, int i, int nseg, float r_co[3])
{
  int subsample_spacing;

  if (bp->seg == 1) {
    if (i == 0) {
      copy_v3_v3(r_co, pro->start);
    }
    else {
      copy_v3_v3(r_co, pro->end);
    }
  }

  else {
    if (nseg == bp->seg) {
      BLI_assert(pro->prof_co != NULL);
      copy_v3_v3(r_co, pro->prof_co + 3 * i);
    }
    else {
      BLI_assert(is_power_of_2_i(nseg) && nseg <= bp->pro_spacing.seg_2);
      /* Find spacing between subsamples in prof_co_2. */
      subsample_spacing = bp->pro_spacing.seg_2 / nseg;
      copy_v3_v3(r_co, pro->prof_co_2 + 3 * i * subsample_spacing);
    }
  }
}

/**
 * Calculate the actual coordinate values for bndv's profile.
 * This is only needed if bp->seg > 1.
 * Allocate the space for them if that hasn't been done already.
 * If bp->seg is not a power of 2, also need to calculate
 * the coordinate values for the power of 2 >= bp->seg, because the ADJ pattern needs power-of-2
 * boundaries during construction.
 */
static void calculate_profile(BevelParams *bp, BoundVert *bndv, bool reversed, bool miter)
{
  int i, k, ns;
  const double *xvals, *yvals;
  float co[3], co2[3], p[3], map[4][4], bottom_corner[3], top_corner[3];
  float *prof_co, *prof_co_k;
  float r;
  bool need_2, map_ok;
  Profile *pro = &bndv->profile;
  ProfileSpacing *pro_spacing = (miter) ? &bp->pro_spacing_miter : &bp->pro_spacing;

  if (bp->seg == 1) {
    return;
  }

  need_2 = bp->seg != bp->pro_spacing.seg_2;
  if (!pro->prof_co) {
    pro->prof_co = (float *)BLI_memarena_alloc(bp->mem_arena,
                                               ((size_t)bp->seg + 1) * 3 * sizeof(float));
    if (need_2) {
      pro->prof_co_2 = (float *)BLI_memarena_alloc(
          bp->mem_arena, ((size_t)bp->pro_spacing.seg_2 + 1) * 3 * sizeof(float));
    }
    else {
      pro->prof_co_2 = pro->prof_co;
    }
  }
  r = pro->super_r;
  if (!bp->use_custom_profile && r == PRO_LINE_R) {
    map_ok = false;
  }
  else {
    map_ok = make_unit_square_map(pro->start, pro->middle, pro->end, map);
  }

  if (bp->vmesh_method == BEVEL_VMESH_CUTOFF && map_ok) {
    /* Calculate the "height" of the profile by putting the (0,0) and (1,1) corners of the
     * un-transformed profile throughout the 2D->3D map and calculating the distance between them.
     */
    zero_v3(p);
    mul_v3_m4v3(bottom_corner, map, p);
    p[0] = 1.0f;
    p[1] = 1.0f;
    mul_v3_m4v3(top_corner, map, p);
    pro->height = len_v3v3(bottom_corner, top_corner);
  }

  /* The first iteration is the nseg case, the second is the seg_2 case (if it's needed) .*/
  for (i = 0; i < 2; i++) {
    if (i == 0) {
      ns = bp->seg;
      xvals = pro_spacing->xvals;
      yvals = pro_spacing->yvals;
      prof_co = pro->prof_co;
    }
    else {
      if (!need_2) {
        break; /* Shares coords with pro->prof_co. */
      }
      ns = bp->pro_spacing.seg_2;
      xvals = pro_spacing->xvals_2;
      yvals = pro_spacing->yvals_2;
      prof_co = pro->prof_co_2;
    }

    /* Iterate over the vertices along the boundary arc. */
    for (k = 0; k <= ns; k++) {
      if (k == 0) {
        copy_v3_v3(co, pro->start);
      }
      else if (k == ns) {
        copy_v3_v3(co, pro->end);
      }
      else {
        if (map_ok) {
          if (reversed) {
            p[0] = (float)yvals[ns - k];
            p[1] = (float)xvals[ns - k];
          }
          else {
            p[0] = (float)xvals[k];
            p[1] = (float)yvals[k];
          }
          p[2] = 0.0f;
          /* Do the 2D->3D transformation of the profile coordinates. */
          mul_v3_m4v3(co, map, p);
        }
        else {
          interp_v3_v3v3(co, pro->start, pro->end, (float)k / (float)ns);
        }
      }
      /* Finish the 2D->3D transformation by projecting onto the final profile plane. */
      prof_co_k = prof_co + 3 * k; /* Each coord takes up 3 spaces. */
      if (!is_zero_v3(pro->proj_dir)) {
        add_v3_v3v3(co2, co, pro->proj_dir);
        /* pro->plane_co and pro->plane_no are filled in "set_profile_params". */
        if (!isect_line_plane_v3(prof_co_k, co, co2, pro->plane_co, pro->plane_no)) {
          /* Shouldn't happen. */
          copy_v3_v3(prof_co_k, co);
        }
      }
      else {
        copy_v3_v3(prof_co_k, co);
      }
    }
  }
}

/**
 * Snap a direction co to a superellipsoid with parameter super_r.
 * For square profiles, midline says whether or not to snap to both planes.
 *
 * Only currently used for the pipe and cube corner special cases.
 */
static void snap_to_superellipsoid(float co[3], const float super_r, bool midline)
{
  float a, b, c, x, y, z, r, rinv, dx, dy;
  r = super_r;
  if (r == PRO_CIRCLE_R) {
    normalize_v3(co);
    return;
  }

  x = a = max_ff(0.0f, co[0]);
  y = b = max_ff(0.0f, co[1]);
  z = c = max_ff(0.0f, co[2]);
  if (r == PRO_SQUARE_R || r == PRO_SQUARE_IN_R) {
    /* Will only be called for 2d profile. */
    BLI_assert(fabsf(z) < BEVEL_EPSILON);
    z = 0.0f;
    x = min_ff(1.0f, x);
    y = min_ff(1.0f, y);
    if (r == PRO_SQUARE_R) {
      /* Snap to closer of x==1 and y==1 lines, or maybe both. */
      dx = 1.0f - x;
      dy = 1.0f - y;
      if (dx < dy) {
        x = 1.0f;
        y = midline ? 1.0f : y;
      }
      else {
        y = 1.0f;
        x = midline ? 1.0f : x;
      }
    }
    else {
      /* Snap to closer of x==0 and y==0 lines, or maybe both. */
      if (x < y) {
        x = 0.0f;
        y = midline ? 0.0f : y;
      }
      else {
        y = 0.0f;
        x = midline ? 0.0f : x;
      }
    }
  }
  else {
    rinv = 1.0f / r;
    if (a == 0.0f) {
      if (b == 0.0f) {
        x = 0.0f;
        y = 0.0f;
        z = powf(c, rinv);
      }
      else {
        x = 0.0f;
        y = powf(1.0f / (1.0f + powf(c / b, r)), rinv);
        z = c * y / b;
      }
    }
    else {
      x = powf(1.0f / (1.0f + powf(b / a, r) + powf(c / a, r)), rinv);
      y = b * x / a;
      z = c * x / a;
    }
  }
  co[0] = x;
  co[1] = y;
  co[2] = z;
}

#define BEV_EXTEND_EDGE_DATA_CHECK(eh, flag) (BM_elem_flag_test(eh->e, flag))

static void check_edge_data_seam_sharp_edges(BevVert *bv, int flag, bool neg)
{
  EdgeHalf *e = &bv->edges[0], *efirst = &bv->edges[0];

  /* First first edge with seam or sharp edge data. */
  while ((!neg && !BEV_EXTEND_EDGE_DATA_CHECK(e, flag)) ||
         (neg && BEV_EXTEND_EDGE_DATA_CHECK(e, flag))) {
    e = e->next;
    if (e == efirst) {
      break;
    }
  }

  /* If no such edge found, return. */
  if ((!neg && !BEV_EXTEND_EDGE_DATA_CHECK(e, flag)) ||
      (neg && BEV_EXTEND_EDGE_DATA_CHECK(e, flag))) {
    return;
  }

  /* Set efirst to this first encountered edge. */
  efirst = e;

  do {
    int flag_count = 0;
    EdgeHalf *ne = e->next;

    while (((!neg && !BEV_EXTEND_EDGE_DATA_CHECK(ne, flag)) ||
            (neg && BEV_EXTEND_EDGE_DATA_CHECK(ne, flag))) &&
           ne != efirst) {
      if (ne->is_bev) {
        flag_count++;
      }
      ne = ne->next;
    }
    if (ne == e || (ne == efirst && ((!neg && !BEV_EXTEND_EDGE_DATA_CHECK(efirst, flag)) ||
                                     (neg && BEV_EXTEND_EDGE_DATA_CHECK(efirst, flag))))) {
      break;
    }
    /* Set seam_len / sharp_len of starting edge. */
    if (flag == BM_ELEM_SEAM) {
      e->rightv->seam_len = flag_count;
    }
    else if (flag == BM_ELEM_SMOOTH) {
      e->rightv->sharp_len = flag_count;
    }
    e = ne;
  } while (e != efirst);
}

static void bevel_extend_edge_data(BevVert *bv)
{
  VMesh *vm = bv->vmesh;

  if (vm->mesh_kind == M_TRI_FAN) {
    return;
  }

  BoundVert *bcur = bv->vmesh->boundstart, *start = bcur;

  do {
    /* If current boundvert has a seam length > 0 then it has a seam running along its edges. */
    if (bcur->seam_len) {
      if (!bv->vmesh->boundstart->seam_len && start == bv->vmesh->boundstart) {
        start = bcur; /* Set start to first boundvert with seam_len > 0. */
      }

      /* Now for all the mesh_verts starting at current index and ending at idxlen
       * we go through outermost ring and through all its segments and add seams
       * for those edges. */
      int idxlen = bcur->index + bcur->seam_len;
      for (int i = bcur->index; i < idxlen; i++) {
        BMVert *v1 = mesh_vert(vm, i % vm->count, 0, 0)->v, *v2;
        BMEdge *e;
        for (int k = 1; k < vm->seg; k++) {
          v2 = mesh_vert(vm, i % vm->count, 0, k)->v;

          /* Here v1 & v2 are current and next BMverts,
           * we find common edge and set its edge data. */
          e = v1->e;
          while (e->v1 != v2 && e->v2 != v2) {
            if (e->v1 == v1) {
              e = e->v1_disk_link.next;
            }
            else {
              e = e->v2_disk_link.next;
            }
          }
          BM_elem_flag_set(e, BM_ELEM_SEAM, true);
          v1 = v2;
        }
        BMVert *v3 = mesh_vert(vm, (i + 1) % vm->count, 0, 0)->v;
        e = v1->e; /* Do same as above for first and last vert. */
        while (e->v1 != v3 && e->v2 != v3) {
          if (e->v1 == v1) {
            e = e->v1_disk_link.next;
          }
          else {
            e = e->v2_disk_link.next;
          }
        }
        BM_elem_flag_set(e, BM_ELEM_SEAM, true);
        bcur = bcur->next;
      }
    }
    else {
      bcur = bcur->next;
    }
  } while (bcur != start);

  bcur = bv->vmesh->boundstart;
  start = bcur;
  do {
    if (bcur->sharp_len) {
      if (!bv->vmesh->boundstart->sharp_len && start == bv->vmesh->boundstart) {
        start = bcur;
      }

      int idxlen = bcur->index + bcur->sharp_len;
      for (int i = bcur->index; i < idxlen; i++) {
        BMVert *v1 = mesh_vert(vm, i % vm->count, 0, 0)->v, *v2;
        BMEdge *e;
        for (int k = 1; k < vm->seg; k++) {
          v2 = mesh_vert(vm, i % vm->count, 0, k)->v;

          e = v1->e;
          while (e->v1 != v2 && e->v2 != v2) {
            if (e->v1 == v1) {
              e = e->v1_disk_link.next;
            }
            else {
              e = e->v2_disk_link.next;
            }
          }
          BM_elem_flag_set(e, BM_ELEM_SMOOTH, false);
          v1 = v2;
        }
        BMVert *v3 = mesh_vert(vm, (i + 1) % vm->count, 0, 0)->v;
        e = v1->e;
        while (e->v1 != v3 && e->v2 != v3) {
          if (e->v1 == v1) {
            e = e->v1_disk_link.next;
          }
          else {
            e = e->v2_disk_link.next;
          }
        }
        BM_elem_flag_set(e, BM_ELEM_SMOOTH, false);
        bcur = bcur->next;
      }
    }
    else {
      bcur = bcur->next;
    }
  } while (bcur != start);
}

/* Mark edges as sharp if they are between a smooth reconstructed face and a new face. */
static void bevel_edges_sharp_boundary(BMesh *bm, BevelParams *bp)
{
  BMIter fiter, liter;
  BMFace *f, *fother;
  BMLoop *l, *lother;
  FKind fkind;

  BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
    if (!BM_elem_flag_test(f, BM_ELEM_SMOOTH)) {
      continue;
    }
    if (get_face_kind(bp, f) != F_RECON) {
      continue;
    }
    BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
      /* Cases we care about will have exactly one adjacent face. */
      lother = l->radial_next;
      fother = lother->f;
      if (lother != l && fother) {
        fkind = get_face_kind(bp, lother->f);
        if (ELEM(fkind, F_EDGE, F_VERT)) {
          BM_elem_flag_disable(l->e, BM_ELEM_SMOOTH);
        }
      }
    }
  }
}

/**
 * \brief Harden normals for bevel.
 *
 * The desired effect is that the newly created #F_EDGE and #F_VERT faces appear smoothly shaded
 * with the normals at the boundaries with #F_RECON faces matching those recon faces.
 * And at boundaries between #F_EDGE and #F_VERT faces, the normals should match the #F_EDGE ones.
 * Assumes custom loop normals are in use.
 */
static void bevel_harden_normals(BevelParams *bp, BMesh *bm)
{
  BMIter liter, fiter;
  BMFace *f;
  BMLoop *l, *lnext, *lprev, *lprevprev, *lnextnext;
  BMEdge *estep;
  FKind fkind, fprevkind, fnextkind, fprevprevkind, fnextnextkind;
  int cd_clnors_offset, l_index;
  short *clnors;
  float *pnorm, norm[3];

  if (bp->offset == 0.0 || !bp->harden_normals) {
    return;
  }

  /* Recalculate all face and vertex normals. Side effect: ensures vertex, edge, face indices. */
  /* I suspect this is not necessary. TODO: test that guess. */
  BM_mesh_normals_update(bm);

  cd_clnors_offset = CustomData_get_offset(&bm->ldata, CD_CUSTOMLOOPNORMAL);

  /* If there is not already a custom split normal layer then making one (with BM_lnorspace_update)
   * will not respect the autosmooth angle between smooth faces. To get that to happen, we have
   * to mark the sharpen the edges that are only sharp because of the angle test -- otherwise would
   * be smooth. */
  if (cd_clnors_offset == -1) {
    BM_edges_sharp_from_angle_set(bm, bp->smoothresh);
    bevel_edges_sharp_boundary(bm, bp);
  }

  /* Ensure that bm->lnor_spacearr has properly stored loop normals.
   * Side effect: ensures loop indices. */
  BM_lnorspace_update(bm);

  if (cd_clnors_offset == -1) {
    cd_clnors_offset = CustomData_get_offset(&bm->ldata, CD_CUSTOMLOOPNORMAL);
  }

  BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
    fkind = get_face_kind(bp, f);
    if (fkind == F_ORIG || fkind == F_RECON) {
      continue;
    }
    BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
      estep = l->prev->e; /* Causes CW walk around l->v fan. */
      lprev = BM_vert_step_fan_loop(l, &estep);
      estep = l->e; /* Causes CCW walk around l->v fan. */
      lnext = BM_vert_step_fan_loop(l, &estep);
      fprevkind = lprev ? get_face_kind(bp, lprev->f) : F_NONE;
      fnextkind = lnext ? get_face_kind(bp, lnext->f) : F_NONE;
      pnorm = NULL;
      if (fkind == F_EDGE) {
        if (fprevkind == F_EDGE && BM_elem_flag_test(l, BM_ELEM_LONG_TAG)) {
          add_v3_v3v3(norm, f->no, lprev->f->no);
          pnorm = norm;
        }
        else if (fnextkind == F_EDGE && BM_elem_flag_test(lnext, BM_ELEM_LONG_TAG)) {
          add_v3_v3v3(norm, f->no, lnext->f->no);
          pnorm = norm;
        }
        else if (fprevkind == F_RECON && BM_elem_flag_test(l, BM_ELEM_LONG_TAG)) {
          pnorm = lprev->f->no;
        }
        else if (fnextkind == F_RECON && BM_elem_flag_test(l->prev, BM_ELEM_LONG_TAG)) {
          pnorm = lnext->f->no;
        }
        else {
          /* printf("unexpected harden case (edge)\n"); */
        }
      }
      else if (fkind == F_VERT) {
        if (fprevkind == F_VERT && fnextkind == F_VERT) {
          pnorm = l->v->no;
        }
        else if (fprevkind == F_RECON) {
          pnorm = lprev->f->no;
        }
        else if (fnextkind == F_RECON) {
          pnorm = lnext->f->no;
        }
        else {
          if (lprev) {
            estep = lprev->prev->e;
            lprevprev = BM_vert_step_fan_loop(lprev, &estep);
          }
          else {
            lprevprev = NULL;
          }
          if (lnext) {
            estep = lnext->e;
            lnextnext = BM_vert_step_fan_loop(lnext, &estep);
          }
          else {
            lnextnext = NULL;
          }
          fprevprevkind = lprevprev ? get_face_kind(bp, lprevprev->f) : F_NONE;
          fnextnextkind = lnextnext ? get_face_kind(bp, lnextnext->f) : F_NONE;
          if (fprevkind == F_EDGE && fprevprevkind == F_RECON) {
            pnorm = lprevprev->f->no;
          }
          else if (fprevkind == F_EDGE && fnextkind == F_VERT && fprevprevkind == F_EDGE) {
            add_v3_v3v3(norm, lprev->f->no, lprevprev->f->no);
            pnorm = norm;
          }
          else if (fnextkind == F_EDGE && fprevkind == F_VERT && fnextnextkind == F_EDGE) {
            add_v3_v3v3(norm, lnext->f->no, lnextnext->f->no);
            pnorm = norm;
          }
          else {
            /* printf("unexpected harden case (vert)\n"); */
          }
        }
      }
      if (pnorm) {
        if (pnorm == norm) {
          normalize_v3(norm);
        }
        l_index = BM_elem_index_get(l);
        clnors = BM_ELEM_CD_GET_VOID_P(l, cd_clnors_offset);
        BKE_lnor_space_custom_normal_to_data(bm->lnor_spacearr->lspacearr[l_index], pnorm, clnors);
      }
    }
  }
}

static void bevel_set_weighted_normal_face_strength(BMesh *bm, BevelParams *bp)
{
  BMFace *f;
  BMIter fiter;
  FKind fkind;
  int strength;
  int mode = bp->face_strength_mode;
  bool do_set_strength;
  const char *wn_layer_id = MOD_WEIGHTEDNORMALS_FACEWEIGHT_CDLAYER_ID;
  int cd_prop_int_idx = CustomData_get_named_layer_index(&bm->pdata, CD_PROP_INT32, wn_layer_id);

  if (cd_prop_int_idx == -1) {
    BM_data_layer_add_named(bm, &bm->pdata, CD_PROP_INT32, wn_layer_id);
    cd_prop_int_idx = CustomData_get_named_layer_index(&bm->pdata, CD_PROP_INT32, wn_layer_id);
  }
  cd_prop_int_idx -= CustomData_get_layer_index(&bm->pdata, CD_PROP_INT32);
  const int cd_prop_int_offset = CustomData_get_n_offset(
      &bm->pdata, CD_PROP_INT32, cd_prop_int_idx);

  BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
    fkind = get_face_kind(bp, f);
    do_set_strength = true;
    switch (fkind) {
      case F_VERT:
        strength = FACE_STRENGTH_WEAK;
        do_set_strength = (mode >= BEVEL_FACE_STRENGTH_NEW);
        break;
      case F_EDGE:
        strength = FACE_STRENGTH_MEDIUM;
        do_set_strength = (mode >= BEVEL_FACE_STRENGTH_NEW);
        break;
      case F_RECON:
        strength = FACE_STRENGTH_STRONG;
        do_set_strength = (mode >= BEVEL_FACE_STRENGTH_AFFECTED);
        break;
      case F_ORIG:
        strength = FACE_STRENGTH_STRONG;
        do_set_strength = (mode == BEVEL_FACE_STRENGTH_ALL);
        break;
      default:
        do_set_strength = false;
    }
    if (do_set_strength) {
      int *strength_ptr = BM_ELEM_CD_GET_VOID_P(f, cd_prop_int_offset);
      *strength_ptr = strength;
    }
  }
}

/* Set the any_seam property for a BevVert and all its BoundVerts. */
static void set_bound_vert_seams(BevVert *bv, bool mark_seam, bool mark_sharp)
{
  BoundVert *v;
  EdgeHalf *e;

  bv->any_seam = false;
  v = bv->vmesh->boundstart;
  do {
    v->any_seam = false;
    for (e = v->efirst; e; e = e->next) {
      v->any_seam |= e->is_seam;
      if (e == v->elast) {
        break;
      }
    }
    bv->any_seam |= v->any_seam;
  } while ((v = v->next) != bv->vmesh->boundstart);

  if (mark_seam) {
    check_edge_data_seam_sharp_edges(bv, BM_ELEM_SEAM, false);
  }
  if (mark_sharp) {
    check_edge_data_seam_sharp_edges(bv, BM_ELEM_SMOOTH, true);
  }
}

static int count_bound_vert_seams(BevVert *bv)
{
  int ans, i;

  if (!bv->any_seam) {
    return 0;
  }

  ans = 0;
  for (i = 0; i < bv->edgecount; i++) {
    if (bv->edges[i].is_seam) {
      ans++;
    }
  }
  return ans;
}

/* Is e between two faces with a 180 degree angle between their normals? */
static bool eh_on_plane(EdgeHalf *e)
{
  float dot;

  if (e->fprev && e->fnext) {
    dot = dot_v3v3(e->fprev->no, e->fnext->no);
    if (fabsf(dot + 1.0f) <= BEVEL_EPSILON_BIG || fabsf(dot - 1.0f) <= BEVEL_EPSILON_BIG) {
      return true;
    }
  }
  return false;
}

/**
 * Calculate the profiles for all the BoundVerts of VMesh vm.
 *
 * \note This should only be called once for each BevVert, after all changes have been made to the
 * profile's parameters.
 */
static void calculate_vm_profiles(BevelParams *bp, BevVert *bv, VMesh *vm)
{
  BoundVert *bndv = vm->boundstart;
  do {
    /* In special cases the params will have already been set. */
    if (!bndv->profile.special_params) {
      set_profile_params(bp, bv, bndv);
    }
    bool miter_profile = false;
    bool reverse_profile = false;
    if (bp->use_custom_profile) {
      /* Use the miter profile spacing struct if the default is filled with the custom profile. */
      miter_profile = (bndv->is_arc_start || bndv->is_patch_start);
      /* Don't bother reversing the profile if it's a miter profile */
      reverse_profile = !bndv->is_profile_start && !miter_profile;
    }
    calculate_profile(bp, bndv, reverse_profile, miter_profile);
  } while ((bndv = bndv->next) != vm->boundstart);
}

/* Implements build_boundary for the vertex-only case. */
static void build_boundary_vertex_only(BevelParams *bp, BevVert *bv, bool construct)
{
  VMesh *vm = bv->vmesh;
  EdgeHalf *efirst, *e;
  BoundVert *v;
  float co[3];

  BLI_assert(bp->vertex_only);

  e = efirst = &bv->edges[0];
  do {
    slide_dist(e, bv->v, e->offset_l, co);
    if (construct) {
      v = add_new_bound_vert(bp->mem_arena, vm, co);
      v->efirst = v->elast = e;
      e->leftv = e->rightv = v;
    }
    else {
      adjust_bound_vert(e->leftv, co);
    }
  } while ((e = e->next) != efirst);

  if (construct) {
    set_bound_vert_seams(bv, bp->mark_seam, bp->mark_sharp);
    if (vm->count == 2) {
      vm->mesh_kind = M_NONE;
    }
    else if (bp->seg == 1) {
      vm->mesh_kind = M_POLY;
    }
    else {
      vm->mesh_kind = M_ADJ;
    }
  }
}

/**
 * Special case of build_boundary when a single edge is beveled.
 * The 'width adjust' part of build_boundary has been done already,
 * and \a efirst is the first beveled edge at vertex \a bv.
 */
static void build_boundary_terminal_edge(BevelParams *bp,
                                         BevVert *bv,
                                         EdgeHalf *efirst,
                                         const bool construct)
{
  MemArena *mem_arena = bp->mem_arena;
  VMesh *vm = bv->vmesh;
  BoundVert *bndv;
  EdgeHalf *e;
  const float *no;
  float co[3], d;
  bool use_tri_fan;

  e = efirst;
  if (bv->edgecount == 2) {
    /* Only 2 edges in, so terminate the edge with an artificial vertex on the unbeveled edge. */
    no = e->fprev ? e->fprev->no : (e->fnext ? e->fnext->no : NULL);
    offset_in_plane(e, no, true, co);
    if (construct) {
      bndv = add_new_bound_vert(mem_arena, vm, co);
      bndv->efirst = bndv->elast = bndv->ebev = e;
      e->leftv = bndv;
    }
    else {
      adjust_bound_vert(e->leftv, co);
    }
    no = e->fnext ? e->fnext->no : (e->fprev ? e->fprev->no : NULL);
    offset_in_plane(e, no, false, co);
    if (construct) {
      bndv = add_new_bound_vert(mem_arena, vm, co);
      bndv->efirst = bndv->elast = e;
      e->rightv = bndv;
    }
    else {
      adjust_bound_vert(e->rightv, co);
    }
    /* Make artificial extra point along unbeveled edge, and form triangle. */
    slide_dist(e->next, bv->v, e->offset_l, co);
    if (construct) {
      bndv = add_new_bound_vert(mem_arena, vm, co);
      bndv->efirst = bndv->elast = e->next;
      e->next->leftv = e->next->rightv = bndv;
      set_bound_vert_seams(bv, bp->mark_seam, bp->mark_sharp);
    }
    else {
      adjust_bound_vert(e->next->leftv, co);
    }
  }
  else {
    /* More than 2 edges in. Put on-edge verts on all the other edges and join with the beveled
     * edge to make a poly or adj mesh, because e->prev has offset 0, offset_meet will put co on
     * that edge. */
    /* TODO: should do something else if angle between e and e->prev > 180 */
    offset_meet(e->prev, e, bv->v, e->fprev, false, co, NULL);
    if (construct) {
      bndv = add_new_bound_vert(mem_arena, vm, co);
      bndv->efirst = e->prev;
      bndv->elast = bndv->ebev = e;
      e->leftv = bndv;
      e->prev->leftv = e->prev->rightv = bndv;
    }
    else {
      adjust_bound_vert(e->leftv, co);
    }
    e = e->next;
    offset_meet(e->prev, e, bv->v, e->fprev, false, co, NULL);
    if (construct) {
      bndv = add_new_bound_vert(mem_arena, vm, co);
      bndv->efirst = e->prev;
      bndv->elast = e;
      e->leftv = e->rightv = bndv;
      e->prev->rightv = bndv;
    }
    else {
      adjust_bound_vert(e->leftv, co);
    }
    /* For the edges not adjacent to the beveled edge, slide the bevel amount along. */
    d = efirst->offset_l_spec;
    if (bp->use_custom_profile || bp->profile < 0.25f) {
      d *= sqrtf(2.0f); /* Need to go further along the edge to make room for full profile area. */
    }
    for (e = e->next; e->next != efirst; e = e->next) {
      slide_dist(e, bv->v, d, co);
      if (construct) {
        bndv = add_new_bound_vert(mem_arena, vm, co);
        bndv->efirst = bndv->elast = e;
        e->leftv = e->rightv = bndv;
      }
      else {
        adjust_bound_vert(e->leftv, co);
      }
    }
  }

  if (bv->edgecount >= 3) {
    /* Special case: snap profile to plane of adjacent two edges. */
    bndv = vm->boundstart;
    BLI_assert(bndv->ebev != NULL);
    set_profile_params(bp, bv, bndv);
    move_profile_plane(bndv, bv->v);
  }

  if (construct) {
    set_bound_vert_seams(bv, bp->mark_seam, bp->mark_sharp);

    if (vm->count == 2 && bv->edgecount == 3) {
      vm->mesh_kind = M_NONE;
    }
    else if (vm->count == 3) {
      use_tri_fan = true;
      if (bp->use_custom_profile) {
        /* Prevent overhanging edges: use M_POLY if the extra point is planar with the profile. */
        bndv = efirst->leftv;
        float profile_plane[4];
        plane_from_point_normal_v3(profile_plane, bndv->profile.plane_co, bndv->profile.plane_no);
        bndv = efirst->rightv->next; /* The added boundvert placed along the non-adjacent edge. */
        if (dist_squared_to_plane_v3(bndv->nv.co, profile_plane) < BEVEL_EPSILON_BIG) {
          use_tri_fan = false;
        }
      }
      vm->mesh_kind = (use_tri_fan) ? M_TRI_FAN : M_POLY;
    }
    else {
      vm->mesh_kind = M_POLY;
    }
  }
}

/* Helper for build_boundary to handle special miters. */
static void adjust_miter_coords(BevelParams *bp, BevVert *bv, EdgeHalf *emiter)
{
  float co1[3], co2[3], co3[3], edge_dir[3], line_p[3];
  BoundVert *v1, *v2, *v3, *v1prev, *v3next;
  BMVert *vother;
  EdgeHalf *emiter_other;
  int miter_outer = bp->miter_outer;

  v1 = emiter->rightv;
  if (miter_outer == BEVEL_MITER_PATCH) {
    v2 = v1->next;
    v3 = v2->next;
  }
  else {
    BLI_assert(miter_outer == BEVEL_MITER_ARC);
    v2 = NULL;
    v3 = v1->next;
  }
  v1prev = v1->prev;
  v3next = v3->next;
  copy_v3_v3(co2, v1->nv.co);
  if (v1->is_arc_start) {
    copy_v3_v3(v1->profile.middle, co2);
  }

  /* co1 is intersection of line through co2 in dir of emiter->e
   * and plane with normal the dir of emiter->e and through v1prev. */
  vother = BM_edge_other_vert(emiter->e, bv->v);
  sub_v3_v3v3(edge_dir, bv->v->co, vother->co);
  normalize_v3(edge_dir);
  float d = bp->offset / (bp->seg / 2.0f); /* A fallback amount to move. */
  madd_v3_v3v3fl(line_p, co2, edge_dir, d);
  if (!isect_line_plane_v3(co1, co2, line_p, v1prev->nv.co, edge_dir)) {
    copy_v3_v3(co1, line_p);
  }
  adjust_bound_vert(v1, co1);

  /* co3 is similar, but plane is through v3next and line is other side of miter edge. */
  emiter_other = v3->elast;
  vother = BM_edge_other_vert(emiter_other->e, bv->v);
  sub_v3_v3v3(edge_dir, bv->v->co, vother->co);
  normalize_v3(edge_dir);
  madd_v3_v3v3fl(line_p, co2, edge_dir, d);
  if (!isect_line_plane_v3(co3, co2, line_p, v3next->nv.co, edge_dir)) {
    copy_v3_v3(co1, line_p);
  }
  adjust_bound_vert(v3, co3);
}

static void adjust_miter_inner_coords(BevelParams *bp, BevVert *bv, EdgeHalf *emiter)
{
  BoundVert *v, *vstart, *v3;
  EdgeHalf *e;
  BMVert *vother;
  float edge_dir[3], co[3];

  v = vstart = bv->vmesh->boundstart;
  do {
    if (v->is_arc_start) {
      v3 = v->next;
      e = v->efirst;
      if (e != emiter) {
        copy_v3_v3(co, v->nv.co);
        vother = BM_edge_other_vert(e->e, bv->v);
        sub_v3_v3v3(edge_dir, vother->co, bv->v->co);
        normalize_v3(edge_dir);
        madd_v3_v3v3fl(v->nv.co, co, edge_dir, bp->spread);
        e = v3->elast;
        vother = BM_edge_other_vert(e->e, bv->v);
        sub_v3_v3v3(edge_dir, vother->co, bv->v->co);
        normalize_v3(edge_dir);
        madd_v3_v3v3fl(v3->nv.co, co, edge_dir, bp->spread);
      }
      v = v3->next;
    }
    else {
      v = v->next;
    }
  } while (v != vstart);
}

/**
 * Make a circular list of BoundVerts for bv, each of which has the coordinates of a vertex on the
 * boundary of the beveled vertex bv->v. This may adjust some EdgeHalf widths, and there might have
 * to be a subsequent pass to make the widths as consistent as possible.
 * Doesn't make the actual BMVerts.
 *
 * For a width consistency pass, we just recalculate the coordinates of the #BoundVerts. If the
 * other ends have been (re)built already, then we copy the offsets from there to match, else we
 * use the ideal (user-specified) widths.
 *
 * \param construct: The first time through, construct will be true and we are making the
 * #BoundVerts and setting up the #BoundVert and #EdgeHalf pointers appropriately.
 * Also, if construct, decide on the mesh pattern that will be used inside the boundary.
 */
static void build_boundary(BevelParams *bp, BevVert *bv, bool construct)
{
  MemArena *mem_arena = bp->mem_arena;
  EdgeHalf *efirst, *e, *e2, *e3, *enip, *eip, *eon, *emiter;
  BoundVert *v, *v1, *v2, *v3;
  VMesh *vm;
  float co[3], r;
  int in_plane, not_in_plane, miter_outer, miter_inner;
  int ang_kind;

  /* Current bevel does nothing if only one edge into a vertex. */
  if (bv->edgecount <= 1) {
    return;
  }

  if (bp->vertex_only) {
    build_boundary_vertex_only(bp, bv, construct);
    return;
  }

  vm = bv->vmesh;

  /* Find a beveled edge to be efirst. */
  e = efirst = next_bev(bv, NULL);
  BLI_assert(e->is_bev);

  if (bv->selcount == 1) {
    /* Special case: only one beveled edge in. */
    build_boundary_terminal_edge(bp, bv, efirst, construct);
    return;
  }

  /* Special miters outside only for 3 or more beveled edges. */
  miter_outer = (bv->selcount >= 3) ? bp->miter_outer : BEVEL_MITER_SHARP;
  miter_inner = bp->miter_inner;

  /* Keep track of the first beveled edge of an outside miter (there can be at most 1 per bv). */
  emiter = NULL;

  /* There is more than one beveled edge.
   * We make BoundVerts to connect the sides of the beveled edges.
   * Non-beveled edges in between will just join to the appropriate juncture point. */
  e = efirst;
  do {
    BLI_assert(e->is_bev);
    eon = NULL;
    /* Make the BoundVert for the right side of e; the other side will be made when the beveled
     * edge to the left of e is handled.
     * Analyze edges until next beveled edge: They are either "in plane" (preceding and subsequent
     * faces are coplanar) or not. The "non-in-plane" edges affect the silhouette and we prefer to
     * slide along one of those if possible. */
    in_plane = not_in_plane = 0; /* Counts of in-plane / not-in-plane. */
    enip = eip = NULL;           /* Representatives of each type. */
    for (e2 = e->next; !e2->is_bev; e2 = e2->next) {
      if (eh_on_plane(e2)) {
        in_plane++;
        eip = e2;
      }
      else {
        not_in_plane++;
        enip = e2;
      }
    }

    if (in_plane == 0 && not_in_plane == 0) {
      offset_meet(e, e2, bv->v, e->fnext, false, co, NULL);
    }
    else if (not_in_plane > 0) {
      if (bp->loop_slide && not_in_plane == 1 && good_offset_on_edge_between(e, e2, enip, bv->v)) {
        if (offset_on_edge_between(e, e2, enip, bv->v, co, &r)) {
          eon = enip;
        }
      }
      else {
        offset_meet(e, e2, bv->v, NULL, true, co, eip);
      }
    }
    else {
      /* n_in_plane > 0 and n_not_in_plane == 0. */
      if (bp->loop_slide && in_plane == 1 && good_offset_on_edge_between(e, e2, eip, bv->v)) {
        if (offset_on_edge_between(e, e2, eip, bv->v, co, &r)) {
          eon = eip;
        }
      }
      else {
        offset_meet(e, e2, bv->v, e->fnext, true, co, eip);
      }
    }

    if (construct) {
      v = add_new_bound_vert(mem_arena, vm, co);
      v->efirst = e;
      v->elast = e2;
      v->ebev = e2;
      v->eon = eon;
      if (eon) {
        v->sinratio = r;
      }
      e->rightv = v;
      e2->leftv = v;
      for (e3 = e->next; e3 != e2; e3 = e3->next) {
        e3->leftv = e3->rightv = v;
      }
      ang_kind = edges_angle_kind(e, e2, bv->v);

      /* Are we doing special mitering?
       * There can only be one outer reflex angle, so only one outer miter,
       * and emiter will be set to the first edge of such an edge.
       * A miter kind of BEVEL_MITER_SHARP means no special miter */
      if ((miter_outer != BEVEL_MITER_SHARP && !emiter && ang_kind == ANGLE_LARGER) ||
          (miter_inner != BEVEL_MITER_SHARP && ang_kind == ANGLE_SMALLER)) {
        if (ang_kind == ANGLE_LARGER) {
          emiter = e;
        }
        /* Make one or two more boundverts; for now all will have same co. */
        v1 = v;
        v1->ebev = NULL;
        if (ang_kind == ANGLE_LARGER && miter_outer == BEVEL_MITER_PATCH) {
          v2 = add_new_bound_vert(mem_arena, vm, co);
        }
        else {
          v2 = NULL;
        }
        v3 = add_new_bound_vert(mem_arena, vm, co);
        v3->ebev = e2;
        v3->efirst = e2;
        v3->elast = e2;
        v3->eon = NULL;
        e2->leftv = v3;
        if (ang_kind == ANGLE_LARGER && miter_outer == BEVEL_MITER_PATCH) {
          v1->is_patch_start = true;
          v2->eon = v1->eon;
          v2->sinratio = v1->sinratio;
          v2->ebev = NULL;
          v1->eon = NULL;
          v1->sinratio = 1.0f;
          v1->elast = e;
          if (e->next == e2) {
            v2->efirst = NULL;
            v2->elast = NULL;
          }
          else {
            v2->efirst = e->next;
            for (e3 = e->next; e3 != e2; e3 = e3->next) {
              e3->leftv = e3->rightv = v2;
              v2->elast = e3;
            }
          }
        }
        else {
          v1->is_arc_start = true;
          copy_v3_v3(v1->profile.middle, co);
          if (e->next == e2) {
            v1->elast = v1->efirst;
          }
          else {
            int between = in_plane + not_in_plane;
            int bet2 = between / 2;
            bool betodd = (between % 2) == 1;
            int i = 0;
            /* Put first half of in-between edges at index 0, second half at index bp->seg.
             * If between is odd, put middle one at midindex. */
            for (e3 = e->next; e3 != e2; e3 = e3->next) {
              v1->elast = e3;
              if (i < bet2) {
                e3->profile_index = 0;
              }
              else if (betodd && i == bet2) {
                e3->profile_index = bp->seg / 2;
              }
              else {
                e3->profile_index = bp->seg;
              }
              i++;
            }
          }
        }
      }
    }
    else { /* construct == false. */
      ang_kind = edges_angle_kind(e, e2, bv->v);
      if ((miter_outer != BEVEL_MITER_SHARP && !emiter && ang_kind == ANGLE_LARGER) ||
          (miter_inner != BEVEL_MITER_SHARP && ang_kind == ANGLE_SMALLER)) {
        if (ang_kind == ANGLE_LARGER) {
          emiter = e;
        }
        v1 = e->rightv;
        if (ang_kind == ANGLE_LARGER && miter_outer == BEVEL_MITER_PATCH) {
          v2 = v1->next;
          v3 = v2->next;
        }
        else {
          v2 = NULL;
          v3 = v1->next;
        }
        adjust_bound_vert(v1, co);
        if (v2) {
          adjust_bound_vert(v2, co);
        }
        adjust_bound_vert(v3, co);
      }
      else {
        adjust_bound_vert(e->rightv, co);
      }
    }
    e = e2;
  } while (e != efirst);

  if (miter_inner != BEVEL_MITER_SHARP) {
    adjust_miter_inner_coords(bp, bv, emiter);
  }
  if (emiter) {
    adjust_miter_coords(bp, bv, emiter);
  }

  if (construct) {
    set_bound_vert_seams(bv, bp->mark_seam, bp->mark_sharp);

    if (vm->count == 2) {
      vm->mesh_kind = M_NONE;
    }
    else if (efirst->seg == 1) {
      vm->mesh_kind = M_POLY;
    }
    else {
      switch (bp->vmesh_method) {
        case BEVEL_VMESH_ADJ:
          vm->mesh_kind = M_ADJ;
          break;
        case BEVEL_VMESH_CUTOFF:
          vm->mesh_kind = M_CUTOFF;
          break;
      }
    }
  }
}

#ifdef DEBUG_ADJUST
static void print_adjust_stats(BoundVert *vstart)
{
  BoundVert *v;
  EdgeHalf *eleft, *eright;
  double even_residual2, spec_residual2;
  double max_even_r, max_even_r_pct;
  double max_spec_r, max_spec_r_pct;
  double delta, delta_pct;

  printf("\nSolution analysis\n");
  even_residual2 = 0.0;
  spec_residual2 = 0.0;
  max_even_r = 0.0;
  max_even_r_pct = 0.0;
  max_spec_r = 0.0;
  max_spec_r_pct = 0.0;
  printf("width matching\n");
  v = vstart;
  do {
    if (v->adjchain != NULL) {
      eright = v->efirst;
      eleft = v->adjchain->elast;
      delta = fabs(eright->offset_r - eleft->offset_l);
      delta_pct = 100.0 * delta / eright->offset_r_spec;
      printf("e%d r(%f) vs l(%f): abs(delta)=%f, delta_pct=%f\n",
             BM_elem_index_get(eright->e),
             eright->offset_r,
             eleft->offset_l,
             delta,
             delta_pct);
      even_residual2 += delta * delta;
      if (delta > max_even_r) {
        max_even_r = delta;
      }
      if (delta_pct > max_even_r_pct) {
        max_even_r_pct = delta_pct;
      }
    }
    v = v->adjchain;
  } while (v && v != vstart);

  printf("spec matching\n");
  v = vstart;
  do {
    if (v->adjchain != NULL) {
      eright = v->efirst;
      eleft = v->adjchain->elast;
      delta = eright->offset_r - eright->offset_r_spec;
      delta_pct = 100.0 * delta / eright->offset_r_spec;
      printf("e%d r(%f) vs r spec(%f): delta=%f, delta_pct=%f\n",
             BM_elem_index_get(eright->e),
             eright->offset_r,
             eright->offset_r_spec,
             delta,
             delta_pct);
      spec_residual2 += delta * delta;
      delta = fabs(delta);
      delta_pct = fabs(delta_pct);
      if (delta > max_spec_r) {
        max_spec_r = delta;
      }
      if (delta_pct > max_spec_r_pct) {
        max_spec_r_pct = delta_pct;
      }

      delta = eleft->offset_l - eleft->offset_l_spec;
      delta_pct = 100.0 * delta / eright->offset_l_spec;
      printf("e%d l(%f) vs l spec(%f): delta=%f, delta_pct=%f\n",
             BM_elem_index_get(eright->e),
             eleft->offset_l,
             eleft->offset_l_spec,
             delta,
             delta_pct);
      spec_residual2 += delta * delta;
      delta = fabs(delta);
      delta_pct = fabs(delta_pct);
      if (delta > max_spec_r) {
        max_spec_r = delta;
      }
      if (delta_pct > max_spec_r_pct) {
        max_spec_r_pct = delta_pct;
      }
    }
    v = v->adjchain;
  } while (v && v != vstart);

  printf("Analysis Result:\n");
  printf("even residual2 = %f,  spec residual2 = %f\n", even_residual2, spec_residual2);
  printf("max even delta = %f, max as percent of spec = %f\n", max_even_r, max_even_r_pct);
  printf("max spec delta = %f, max as percent of spec = %f\n", max_spec_r, max_spec_r_pct);
}
#endif

#ifdef FAST_ADJUST_CODE
/* This code uses a direct solution to the adjustment problem for chains and certain cycles.
 * It is a two-step approach: first solve for the exact solution of the 'match widths' constraints
 * using the one degree of freedom that allows for expressing all other widths in terms of that.
 * And then minimize the spec-matching constraints using the derivative of the least squares
 * residual in terms of that one degree of freedom.
 * Unfortunately, the results are in some cases worse than the general least squares solution
 * for the combined (with weights) problem, so this code is not used.
 * But keep it here for a while in case performance issues demand that it be used sometimes. */
static bool adjust_the_cycle_or_chain_fast(BoundVert *vstart, int np, bool iscycle)
{
  BoundVert *v;
  EdgeHalf *eleft, *eright;
  float *g;
  float *g_prod;
  float gprod, gprod_sum, spec_sum, p;
  int i;

  g = MEM_mallocN(np * sizeof(float), "beveladjust");
  g_prod = MEM_mallocN(np * sizeof(float), "beveladjust");

  v = vstart;
  spec_sum = 0.0f;
  i = 0;
  do {
    g[i] = v->sinratio;
    if (iscycle || v->adjchain != NULL) {
      spec_sum += v->efirst->offset_r;
    }
    else {
      spec_sum += v->elast->offset_l;
    }
    i++;
    v = v->adjchain;
  } while (v && v != vstart);

  gprod = 1.00f;
  gprod_sum = 1.0f;
  for (i = np - 1; i > 0; i--) {
    gprod *= g[i];
    g_prod[i] = gprod;
    gprod_sum += gprod;
  }
  g_prod[0] = 1.0f;
  if (iscycle) {
    gprod *= g[0];
    if (fabs(gprod - 1.0f) > BEVEL_EPSILON) {
      /* Fast cycle calc only works if total product is 1. */
      MEM_freeN(g);
      MEM_freeN(g_prod);
      return false;
    }
  }
  if (gprod_sum == 0.0f) {
    MEM_freeN(g);
    MEM_freeN(g_prod);
    return false;
  }
  p = spec_sum / gprod_sum;

  /* Apply the new offsets. */
  v = vstart;
  i = 0;
  do {
    if (iscycle || v->adjchain != NULL) {
      eright = v->efirst;
      eleft = v->elast;
      eright->offset_r = g_prod[(i + 1) % np] * p;
      if (iscycle || v != vstart) {
        eleft->offset_l = v->sinratio * eright->offset_r;
      }
    }
    else {
      /* Not a cycle, and last of chain. */
      eleft = v->elast;
      eleft->offset_l = p;
    }
    i++;
    v = v->adjchain;
  } while (v && v != vstart);

  MEM_freeN(g);
  MEM_freeN(g_prod);
  return true;
}
#endif

/**
 * Helper function to return the next Beveled EdgeHalf along a path.
 *
 * \param toward_bv: Whether the direction to travel points toward or away from the BevVert
 * connected to the current EdgeHalf.
 * \param r_bv: The BevVert connected to the EdgeHalf -- updated if we're traveling to the other
 * EdgeHalf of an original edge.
 *
 * \note This only returns the most parallel edge if it's the most parallel by
 * at least 10 degrees. This is a somewhat arbitrary choice, but it makes sure that consistent
 * orientation paths only continue in obvious ways.
 */
static EdgeHalf *next_edgehalf_bev(BevelParams *bp,
                                   EdgeHalf *start_edge,
                                   bool toward_bv,
                                   BevVert **r_bv)
{
  EdgeHalf *new_edge;
  EdgeHalf *next_edge = NULL;
  float dir_start_edge[3], dir_new_edge[3];
  float second_best_dot = 0.0f, best_dot = 0.0f;
  float new_dot;

  /* Case 1: The next EdgeHalf is across a BevVert from the current EdgeHalf. */
  if (toward_bv) {
    /* Skip all the logic if there's only one beveled edge at the vertex, we're at an end. */
    if ((*r_bv)->selcount == 1) {
      return NULL; /* No other edges to go to. */
    }

    /* The case with only one other edge connected to the vertex is special too. */
    if ((*r_bv)->selcount == 2) {
      /* Just find the next beveled edge, that's the only other option. */
      new_edge = start_edge;
      do {
        new_edge = new_edge->next;
      } while (!new_edge->is_bev);

      return new_edge;
    }

    /* Find the direction vector of the current edge (pointing INTO the BevVert).
     * v1 and v2 don't necessarily have an order, so we need to check which is closer to bv. */
    if (start_edge->e->v1 == (*r_bv)->v) {
      sub_v3_v3v3(dir_start_edge, start_edge->e->v1->co, start_edge->e->v2->co);
    }
    else {
      sub_v3_v3v3(dir_start_edge, start_edge->e->v2->co, start_edge->e->v1->co);
    }
    normalize_v3(dir_start_edge);

    /* Find the beveled edge coming out of the BevVert that's most parallel to the current edge. */
    new_edge = start_edge->next;
    while (new_edge != start_edge) {
      if (!new_edge->is_bev) {
        new_edge = new_edge->next;
        continue;
      }
      /* Find direction vector of the possible next edge (pointing OUT of the BevVert). */
      if (new_edge->e->v2 == (*r_bv)->v) {
        sub_v3_v3v3(dir_new_edge, new_edge->e->v1->co, new_edge->e->v2->co);
      }
      else {
        sub_v3_v3v3(dir_new_edge, new_edge->e->v2->co, new_edge->e->v1->co);
      }
      normalize_v3(dir_new_edge);

      /* Use this edge if it is the most parallel to the orignial so far. */
      new_dot = dot_v3v3(dir_new_edge, dir_start_edge);
      if (new_dot > best_dot) {
        second_best_dot = best_dot; /* For remembering if the choice was too close. */
        best_dot = new_dot;
        next_edge = new_edge;
      }
      else if (new_dot > second_best_dot) {
        second_best_dot = new_dot;
      }

      new_edge = new_edge->next;
    }

    /* Only return a new Edge if one was found and if the choice of next edge was not too close. */
    if ((next_edge != NULL) && compare_ff(best_dot, second_best_dot, BEVEL_SMALL_ANG_DOT)) {
      return NULL;
    }
    else {
      return next_edge;
    }
  }
  else {
    /* Case 2: The next EdgeHalf is the other side of the BMEdge.
     * It's part of the same BMEdge, so we know the other EdgeHalf is also beveled. */
    next_edge = find_other_end_edge_half(bp, start_edge, r_bv);
    return next_edge;
  }
}

/**
 * Starting along any beveled edge, travel along the chain / cycle of beveled edges including that
 * edge, marking consistent profile orientations along the way. Orientations are marked by setting
 * whether the BoundVert that contains each profile's information is the side of the profile's
 * start or not.
 */
static void regularize_profile_orientation(BevelParams *bp, BMEdge *bme)
{
  BevVert *start_bv = find_bevvert(bp, bme->v1);
  EdgeHalf *start_edgehalf = find_edge_half(start_bv, bme);
  if (!start_edgehalf->is_bev || start_edgehalf->visited_rpo) {
    return;
  }

  /* Pick a BoundVert on one side of the profile to use for the starting side. Use the one highest
   * on the Z axis because even any rule is better than an arbitrary decision. */
  bool right_highest = start_edgehalf->leftv->nv.co[2] < start_edgehalf->rightv->nv.co[2];
  start_edgehalf->leftv->is_profile_start = right_highest;
  start_edgehalf->visited_rpo = true;

  /* First loop starts in the away from BevVert direction and the second starts toward it. */
  bool toward_bv;
  BevVert *bv;
  EdgeHalf *edgehalf;
  for (int i = 0; i < 2; i++) {
    edgehalf = start_edgehalf;
    bv = start_bv;
    toward_bv = (i == 0);
    edgehalf = next_edgehalf_bev(bp, edgehalf, toward_bv, &bv);

    /* Keep traveling until there is no unvisited beveled edgehalf to visit next. */
    while (edgehalf && !edgehalf->visited_rpo) {
      /* Mark the correct BoundVert as the start of the newly visited profile.
       * The direction relative to the BevVert switches every step, so also switch
       * the orientation every step. */
      if (i == 0) {
        edgehalf->leftv->is_profile_start = toward_bv ^ right_highest;
      }
      else {
        /* The opposite side as the first direction because we're moving the other way. */
        edgehalf->leftv->is_profile_start = (!toward_bv) ^ right_highest;
      }

      /* The next jump will in the opposite direction relative to the BevVert. */
      toward_bv = !toward_bv;

      edgehalf->visited_rpo = true;
      edgehalf = next_edgehalf_bev(bp, edgehalf, toward_bv, &bv);
    }
  }
}

/**
 * Adjust the offsets for a single cycle or chain.
 * For chains and some cycles, a fast solution exists.
 * Otherwise, we set up and solve a linear least squares problem
 * that tries to minimize the squared differences of lengths
 * at each end of an edge, and (with smaller weight) the
 * squared differences of the offsets from their specs.
 */
static void adjust_the_cycle_or_chain(BoundVert *vstart, bool iscycle)
{
  BoundVert *v;
  EdgeHalf *eleft, *eright, *enextleft;
  LinearSolver *solver;
  double weight, val;
  int i, np, nrows, row;

  np = 0;
#ifdef DEBUG_ADJUST
  printf("\nadjust the %s (with eigen)\n", iscycle ? "cycle" : "chain");
#endif
  v = vstart;
  do {
#ifdef DEBUG_ADJUST
    eleft = v->elast;
    eright = v->efirst;
    printf(" (left=e%d, right=e%d)", BM_elem_index_get(eleft->e), BM_elem_index_get(eright->e));
#endif
    np++;
    v = v->adjchain;
  } while (v && v != vstart);
#ifdef DEBUG_ADJUST
  printf(" -> %d parms\n", np);
#endif

#ifdef FAST_ADJUST_CODE
  if (adjust_the_cycle_or_chain_fast(vstart, np, iscycle)) {
    return;
  }
#endif

  nrows = iscycle ? 3 * np : 3 * np - 3;

  solver = EIG_linear_least_squares_solver_new(nrows, np, 1);

  v = vstart;
  i = 0;
  weight = BEVEL_MATCH_SPEC_WEIGHT; /* Sqrt of factor to weight down importance of spec match. */
  do {
    /* Except at end of chain, v's indep variable is offset_r of v->efirst. */
    if (iscycle || i < np - 1) {
      eright = v->efirst;
      eleft = v->elast;
      enextleft = v->adjchain->elast;
#ifdef DEBUG_ADJUST
      printf("p%d: e%d->offset_r = %f\n", i, BM_elem_index_get(eright->e), eright->offset_r);
      if (iscycle || v != vstart) {
        printf("  dependent: e%d->offset_l = %f * p%d\n",
               BM_elem_index_get(eleft->e),
               v->sinratio,
               i);
      }
#endif

      /* Residue i: width difference between eright and eleft of next. */
      EIG_linear_solver_matrix_add(solver, i, i, 1.0);
      EIG_linear_solver_right_hand_side_add(solver, 0, i, 0.0);
      if (iscycle) {
        EIG_linear_solver_matrix_add(solver, i > 0 ? i - 1 : np - 1, i, -v->sinratio);
      }
      else {
        if (i > 0) {
          EIG_linear_solver_matrix_add(solver, i - 1, i, -v->sinratio);
        }
      }

      /* Residue np + 2*i (if cycle) else np - 1 + 2*i:
       * right offset for parm i matches its spec; weighted. */
      row = iscycle ? np + 2 * i : np - 1 + 2 * i;
      EIG_linear_solver_matrix_add(solver, row, i, weight);
      EIG_linear_solver_right_hand_side_add(solver, 0, row, weight * eright->offset_r);
#ifdef DEBUG_ADJUST
      printf("b[%d]=%f * %f, for e%d->offset_r\n",
             row,
             weight,
             eright->offset_r,
             BM_elem_index_get(eright->e));
#endif

      /* Residue np + 2*i + 1 (if cycle) else np - 1 + 2*i + 1:
       * left offset for parm i matches its spec; weighted. */
      row = row + 1;
      EIG_linear_solver_matrix_add(
          solver, row, (i == np - 1) ? 0 : i + 1, weight * v->adjchain->sinratio);
      EIG_linear_solver_right_hand_side_add(solver, 0, row, weight * enextleft->offset_l);
#ifdef DEBUG_ADJUST
      printf("b[%d]=%f * %f, for e%d->offset_l\n",
             row,
             weight,
             enextleft->offset_l,
             BM_elem_index_get(enextleft->e));
#endif
    }
    else {
      /* Not a cycle, and last of chain. */
      eleft = v->elast;
#ifdef DEBUG_ADJUST
      printf("p%d: e%d->offset_l = %f\n", i, BM_elem_index_get(eleft->e), eleft->offset_l);
#endif
      /* Second part of residue i for last i. */
      EIG_linear_solver_matrix_add(solver, i - 1, i, -1.0);
    }
    i++;
    v = v->adjchain;
  } while (v && v != vstart);
  EIG_linear_solver_solve(solver);
#ifdef DEBUG_ADJUST
  /* Note: this print only works after solve, but by that time b has been cleared. */
  EIG_linear_solver_print_matrix(solver);
  printf("\nSolution:\n");
  for (i = 0; i < np; i++) {
    printf("p%d = %f\n", i, EIG_linear_solver_variable_get(solver, 0, i));
  }
#endif

  /* Use the solution to set new widths. */
  v = vstart;
  i = 0;
  do {
    val = EIG_linear_solver_variable_get(solver, 0, i);
    if (iscycle || i < np - 1) {
      eright = v->efirst;
      eleft = v->elast;
      eright->offset_r = (float)val;
#ifdef DEBUG_ADJUST
      printf("e%d->offset_r = %f\n", BM_elem_index_get(eright->e), eright->offset_r);
#endif
      if (iscycle || v != vstart) {
        eleft->offset_l = (float)(v->sinratio * val);
#ifdef DEBUG_ADJUST
        printf("e%d->offset_l = %f\n", BM_elem_index_get(eleft->e), eleft->offset_l);
#endif
      }
    }
    else {
      /* Not a cycle, and last of chain. */
      eleft = v->elast;
      eleft->offset_l = (float)val;
#ifdef DEBUG_ADJUST
      printf("e%d->offset_l = %f\n", BM_elem_index_get(eleft->e), eleft->offset_l);
#endif
    }
    i++;
    v = v->adjchain;
  } while (v && v != vstart);

#ifdef DEBUG_ADJUST
  print_adjust_stats(vstart);
  EIG_linear_solver_print_matrix(solver);
#endif

  EIG_linear_solver_delete(solver);
}

/**
 * Adjust the offsets to try to make them, as much as possible,
 * have even-width bevels with offsets that match their specs.
 * The problem that we can try to ameliorate is that when loop slide
 * is active, the meet point will probably not be the one that makes
 * both sides have their specified width. And because both ends may be
 * on loop slide edges, the widths at each end could be different.
 *
 * It turns out that the dependent offsets either form chains or
 * cycles, and we can process each of those separately.
 */
static void adjust_offsets(BevelParams *bp, BMesh *bm)
{
  BMVert *bmv;
  BevVert *bv, *bvcur;
  BoundVert *v, *vanchor, *vchainstart, *vchainend, *vnext;
  EdgeHalf *enext;
  BMIter iter;
  bool iscycle;
  int chainlen;

  /* Find and process chains and cycles of unvisited BoundVerts that have eon set. */
  /* Note: for repeatability, iterate over all verts of mesh rather than over ghash'ed BMVerts. */
  BM_ITER_MESH (bmv, &iter, bm, BM_VERTS_OF_MESH) {
    if (!BM_elem_flag_test(bmv, BM_ELEM_TAG)) {
      continue;
    }
    bv = bvcur = find_bevvert(bp, bmv);
    if (!bv) {
      continue;
    }
    vanchor = bv->vmesh->boundstart;
    do {
      if (vanchor->visited || !vanchor->eon) {
        continue;
      }

      /* Find one of (1) a cycle that starts and ends at v
       * where each v has v->eon set and had not been visited before;
       * or (2) a chain of v's where the start and end of the chain do not have
       * v->eon set but all else do.
       * It is OK for the first and last elements to
       * have been visited before, but not any of the inner ones.
       * We chain the v's together through v->adjchain, and are following
       * them in left->right direction, meaning that the left side of one edge
       * pairs with the right side of the next edge in the cycle or chain. */

      /* First follow paired edges in left->right direction. */
      v = vchainstart = vchainend = vanchor;
      iscycle = false;
      chainlen = 1;
      while (v->eon && !v->visited && !iscycle) {
        v->visited = true;
        if (!v->efirst) {
          break;
        }
        enext = find_other_end_edge_half(bp, v->efirst, &bvcur);
        if (!enext) {
          break;
        }
        BLI_assert(enext != NULL);
        vnext = enext->leftv;
        v->adjchain = vnext;
        vchainend = vnext;
        chainlen++;
        if (vnext->visited) {
          if (vnext != vchainstart) {
            break;
          }
          adjust_the_cycle_or_chain(vchainstart, true);
          iscycle = true;
        }
        v = vnext;
      }
      if (!iscycle) {
        /* right->left direction, changing vchainstart at each step. */
        v->adjchain = NULL;
        v = vchainstart;
        bvcur = bv;
        do {
          v->visited = true;
          if (!v->elast) {
            break;
          }
          enext = find_other_end_edge_half(bp, v->elast, &bvcur);
          if (!enext) {
            break;
          }
          vnext = enext->rightv;
          vnext->adjchain = v;
          chainlen++;
          vchainstart = vnext;
          v = vnext;
        } while (!v->visited && v->eon);
        if (chainlen >= 3 && !vchainstart->eon && !vchainend->eon) {
          adjust_the_cycle_or_chain(vchainstart, false);
        }
      }
    } while ((vanchor = vanchor->next) != bv->vmesh->boundstart);
  }

  /* Rebuild boundaries with new width specs. */
  BM_ITER_MESH (bmv, &iter, bm, BM_VERTS_OF_MESH) {
    if (BM_elem_flag_test(bmv, BM_ELEM_TAG)) {
      bv = find_bevvert(bp, bmv);
      if (bv) {
        build_boundary(bp, bv, false);
      }
    }
  }
}

/**
 * Do the edges at bv form a "pipe"?
 * Current definition: 3 or 4 beveled edges, 2 in line with each other,
 * with other edges on opposite sides of the pipe if there are 4.
 * Also, the vertex boundary should have 3 or 4 vertices in it,
 * and all of the faces involved should be parallel to the pipe edges.
 * Return the boundary vert whose ebev is one of the pipe edges, and
 * whose next boundary vert has a beveled, non-pipe edge.
 */
static BoundVert *pipe_test(BevVert *bv)
{
  EdgeHalf *e, *epipe;
  VMesh *vm;
  BoundVert *v1, *v2, *v3;
  float dir1[3], dir3[3];

  vm = bv->vmesh;
  if (vm->count < 3 || vm->count > 4 || bv->selcount < 3 || bv->selcount > 4) {
    return NULL;
  }

  /* Find v1, v2, v3 all with beveled edges, where v1 and v3 have collinear edges. */
  epipe = NULL;
  v1 = vm->boundstart;
  do {
    v2 = v1->next;
    v3 = v2->next;
    if (v1->ebev && v2->ebev && v3->ebev) {
      sub_v3_v3v3(dir1, bv->v->co, BM_edge_other_vert(v1->ebev->e, bv->v)->co);
      sub_v3_v3v3(dir3, BM_edge_other_vert(v3->ebev->e, bv->v)->co, bv->v->co);
      normalize_v3(dir1);
      normalize_v3(dir3);
      if (angle_normalized_v3v3(dir1, dir3) < BEVEL_EPSILON_ANG) {
        epipe = v1->ebev;
        break;
      }
    }
  } while ((v1 = v1->next) != vm->boundstart);

  if (!epipe) {
    return NULL;
  }

  /* Check face planes: all should have normals perpendicular to epipe. */
  for (e = &bv->edges[0]; e != &bv->edges[bv->edgecount]; e++) {
    if (e->fnext) {
      if (fabsf(dot_v3v3(dir1, e->fnext->no)) > BEVEL_EPSILON_BIG) {
        return NULL;
      }
    }
  }
  return v1;
}

static VMesh *new_adj_vmesh(MemArena *mem_arena, int count, int seg, BoundVert *bounds)
{
  VMesh *vm;

  vm = (VMesh *)BLI_memarena_alloc(mem_arena, sizeof(VMesh));
  vm->count = count;
  vm->seg = seg;
  vm->boundstart = bounds;
  vm->mesh = (NewVert *)BLI_memarena_alloc(
      mem_arena, (size_t)(count * (1 + seg / 2) * (1 + seg)) * sizeof(NewVert));
  vm->mesh_kind = M_ADJ;
  return vm;
}

/**
 * VMesh verts for vertex i have data for (i, 0 <= j <= ns2, 0 <= k <= ns),
 * where ns2 = floor(nseg / 2).
 * But these overlap data from previous and next i: there are some forced equivalences.
 * Let's call these indices the canonical ones: we will just calculate data for these
 *    0 <= j <= ns2, 0 <= k < ns2  (for odd ns2)
 *    0 <= j < ns2, 0 <= k <= ns2  (for even ns2)
 *    also (j=ns2, k=ns2) at i=0 (for even ns2)
 * This function returns the canonical one for any i, j, k in [0,n],[0,ns],[0,ns].
 */
static NewVert *mesh_vert_canon(VMesh *vm, int i, int j, int k)
{
  int n, ns, ns2, odd;
  NewVert *ans;

  n = vm->count;
  ns = vm->seg;
  ns2 = ns / 2;
  odd = ns % 2;
  BLI_assert(0 <= i && i <= n && 0 <= j && j <= ns && 0 <= k && k <= ns);

  if (!odd && j == ns2 && k == ns2) {
    ans = mesh_vert(vm, 0, j, k);
  }
  else if (j <= ns2 - 1 + odd && k <= ns2) {
    ans = mesh_vert(vm, i, j, k);
  }
  else if (k <= ns2) {
    ans = mesh_vert(vm, (i + n - 1) % n, k, ns - j);
  }
  else {
    ans = mesh_vert(vm, (i + 1) % n, ns - k, j);
  }
  return ans;
}

static bool is_canon(VMesh *vm, int i, int j, int k)
{
  int ns2 = vm->seg / 2;
  if (vm->seg % 2 == 1) { /* Odd. */
    return (j <= ns2 && k <= ns2);
  }
  else { /* Even. */
    return ((j < ns2 && k <= ns2) || (j == ns2 && k == ns2 && i == 0));
  }
}

/* Copy the vertex data to all of vm verts from canonical ones. */
static void vmesh_copy_equiv_verts(VMesh *vm)
{
  int n, ns, ns2, i, j, k;
  NewVert *v0, *v1;

  n = vm->count;
  ns = vm->seg;
  ns2 = ns / 2;
  for (i = 0; i < n; i++) {
    for (j = 0; j <= ns2; j++) {
      for (k = 0; k <= ns; k++) {
        if (is_canon(vm, i, j, k)) {
          continue;
        }
        v1 = mesh_vert(vm, i, j, k);
        v0 = mesh_vert_canon(vm, i, j, k);
        copy_v3_v3(v1->co, v0->co);
        v1->v = v0->v;
      }
    }
  }
}

/* Calculate and return in r_cent the centroid of the center poly. */
static void vmesh_center(VMesh *vm, float r_cent[3])
{
  int n, ns2, i;

  n = vm->count;
  ns2 = vm->seg / 2;
  if (vm->seg % 2) {
    zero_v3(r_cent);
    for (i = 0; i < n; i++) {
      add_v3_v3(r_cent, mesh_vert(vm, i, ns2, ns2)->co);
    }
    mul_v3_fl(r_cent, 1.0f / (float)n);
  }
  else {
    copy_v3_v3(r_cent, mesh_vert(vm, 0, ns2, ns2)->co);
  }
}

static void avg4(
    float co[3], const NewVert *v0, const NewVert *v1, const NewVert *v2, const NewVert *v3)
{
  add_v3_v3v3(co, v0->co, v1->co);
  add_v3_v3(co, v2->co);
  add_v3_v3(co, v3->co);
  mul_v3_fl(co, 0.25f);
}

/* Gamma needed for smooth Catmull-Clark, Sabin modification. */
static float sabin_gamma(int n)
{
  double ans, k, k2, k4, k6, x, y;

  /* pPrecalculated for common cases of n. */
  if (n < 3) {
    return 0.0f;
  }
  else if (n == 3) {
    ans = 0.065247584f;
  }
  else if (n == 4) {
    ans = 0.25f;
  }
  else if (n == 5) {
    ans = 0.401983447f;
  }
  else if (n == 6) {
    ans = 0.523423277f;
  }
  else {
    k = cos(M_PI / (double)n);
    /* Need x, real root of x^3 + (4k^2 - 3)x - 2k = 0.
     * Answer calculated via Wolfram Alpha. */
    k2 = k * k;
    k4 = k2 * k2;
    k6 = k4 * k2;
    y = pow(M_SQRT3 * sqrt(64.0 * k6 - 144.0 * k4 + 135.0 * k2 - 27.0) + 9.0 * k, 1.0 / 3.0);
    x = 0.480749856769136 * y - (0.231120424783545 * (12.0 * k2 - 9.0)) / y;
    ans = (k * x + 2.0 * k2 - 1.0) / (x * x * (k * x + 1.0));
  }
  return (float)ans;
}

/* Fill frac with fractions of the way along ring 0 for vertex i, for use with interp_range
 * function. */
static void fill_vmesh_fracs(VMesh *vm, float *frac, int i)
{
  int k, ns;
  float total = 0.0f;

  ns = vm->seg;
  frac[0] = 0.0f;
  for (k = 0; k < ns; k++) {
    total += len_v3v3(mesh_vert(vm, i, 0, k)->co, mesh_vert(vm, i, 0, k + 1)->co);
    frac[k + 1] = total;
  }
  if (total > 0.0f) {
    for (k = 1; k <= ns; k++) {
      frac[k] /= total;
    }
  }
  else {
    frac[ns] = 1.0f;
  }
}

/* Like fill_vmesh_fracs but want fractions for profile points of bndv, with ns segments. */
static void fill_profile_fracs(BevelParams *bp, BoundVert *bndv, float *frac, int ns)
{
  int k;
  float co[3], nextco[3];
  float total = 0.0f;

  frac[0] = 0.0f;
  copy_v3_v3(co, bndv->nv.co);
  for (k = 0; k < ns; k++) {
    get_profile_point(bp, &bndv->profile, k + 1, ns, nextco);
    total += len_v3v3(co, nextco);
    frac[k + 1] = total;
    copy_v3_v3(co, nextco);
  }
  if (total > 0.0f) {
    for (k = 1; k <= ns; k++) {
      frac[k] /= total;
    }
  }
  else {
    frac[ns] = 1.0f;
  }
}

/* Return i such that frac[i] <= f <= frac[i + 1], where frac[n] == 1.0
 * and put fraction of rest of way between frac[i] and frac[i + 1] into r_rest. */
static int interp_range(const float *frac, int n, const float f, float *r_rest)
{
  int i;
  float rest;

  /* Could binary search in frac, but expect n to be reasonably small. */
  for (i = 0; i < n; i++) {
    if (f <= frac[i + 1]) {
      rest = f - frac[i];
      if (rest == 0) {
        *r_rest = 0.0f;
      }
      else {
        *r_rest = rest / (frac[i + 1] - frac[i]);
      }
      if (i == n - 1 && *r_rest == 1.0f) {
        i = n;
        *r_rest = 0.0f;
      }
      return i;
    }
  }
  *r_rest = 0.0f;
  return n;
}

/* Interpolate given vmesh to make one with target nseg border vertices on the profiles.
 * TODO(Hans): This puts the center mesh vert at a slightly off location sometimes, which seems to
 * be associated with the rest of that ring being shifted or connected slightly incorrectly to its
 * neighbors. */
static VMesh *interp_vmesh(BevelParams *bp, VMesh *vm_in, int nseg)
{
  int n_bndv, ns_in, nseg2, odd, i, j, k, j_in, k_in, k_in_prev, j0inc, k0inc;
  float *prev_frac, *frac, *new_frac, *prev_new_frac;
  float fraction, restj, restk, restkprev;
  float quad[4][3], co[3], center[3];
  VMesh *vm_out;
  BoundVert *bndv;

  n_bndv = vm_in->count;
  ns_in = vm_in->seg;
  nseg2 = nseg / 2;
  odd = nseg % 2;
  vm_out = new_adj_vmesh(bp->mem_arena, n_bndv, nseg, vm_in->boundstart);

  prev_frac = BLI_array_alloca(prev_frac, (ns_in + 1));
  frac = BLI_array_alloca(frac, (ns_in + 1));
  new_frac = BLI_array_alloca(new_frac, (nseg + 1));
  prev_new_frac = BLI_array_alloca(prev_new_frac, (nseg + 1));

  fill_vmesh_fracs(vm_in, prev_frac, n_bndv - 1);
  bndv = vm_in->boundstart;
  fill_profile_fracs(bp, bndv->prev, prev_new_frac, nseg);
  for (i = 0; i < n_bndv; i++) {
    fill_vmesh_fracs(vm_in, frac, i);
    fill_profile_fracs(bp, bndv, new_frac, nseg);
    for (j = 0; j <= nseg2 - 1 + odd; j++) {
      for (k = 0; k <= nseg2; k++) {
        /* Finding the locations where "fraction" fits into previous and current "frac". */
        fraction = new_frac[k];
        k_in = interp_range(frac, ns_in, fraction, &restk);
        fraction = prev_new_frac[nseg - j];
        k_in_prev = interp_range(prev_frac, ns_in, fraction, &restkprev);
        j_in = ns_in - k_in_prev;
        restj = -restkprev;
        if (restj > -BEVEL_EPSILON) {
          restj = 0.0f;
        }
        else {
          j_in = j_in - 1;
          restj = 1.0f + restj;
        }
        /* Use bilinear interpolation within the source quad; could be smarter here. */
        if (restj < BEVEL_EPSILON && restk < BEVEL_EPSILON) {
          copy_v3_v3(co, mesh_vert_canon(vm_in, i, j_in, k_in)->co);
        }
        else {
          j0inc = (restj < BEVEL_EPSILON || j_in == ns_in) ? 0 : 1;
          k0inc = (restk < BEVEL_EPSILON || k_in == ns_in) ? 0 : 1;
          copy_v3_v3(quad[0], mesh_vert_canon(vm_in, i, j_in, k_in)->co);
          copy_v3_v3(quad[1], mesh_vert_canon(vm_in, i, j_in, k_in + k0inc)->co);
          copy_v3_v3(quad[2], mesh_vert_canon(vm_in, i, j_in + j0inc, k_in + k0inc)->co);
          copy_v3_v3(quad[3], mesh_vert_canon(vm_in, i, j_in + j0inc, k_in)->co);
          interp_bilinear_quad_v3(quad, restk, restj, co);
        }
        copy_v3_v3(mesh_vert(vm_out, i, j, k)->co, co);
      }
    }
    bndv = bndv->next;
    memcpy(prev_frac, frac, (size_t)(ns_in + 1) * sizeof(float));
    memcpy(prev_new_frac, new_frac, (size_t)(nseg + 1) * sizeof(float));
  }
  if (!odd) {
    vmesh_center(vm_in, center);
    copy_v3_v3(mesh_vert(vm_out, 0, nseg2, nseg2)->co, center);
  }
  vmesh_copy_equiv_verts(vm_out);
  return vm_out;
}

/* Do one step of cubic subdivision (Catmull-Clark), with special rules at boundaries.
 * For now, this is written assuming vm0->nseg is even and > 0.
 * We are allowed to modify vm_in, as it will not be used after this call.
 * See Levin 1999 paper: "Filling an N-sided hole using combined subdivision schemes". */
static VMesh *cubic_subdiv(BevelParams *bp, VMesh *vm_in)
{
  int n_boundary, ns_in, ns_in2, ns_out;
  int i, j, k, inext;
  float co[3], co1[3], co2[3], acc[3];
  float beta, gamma;
  VMesh *vm_out;
  BoundVert *bndv;

  n_boundary = vm_in->count;
  ns_in = vm_in->seg;
  ns_in2 = ns_in / 2;
  BLI_assert(ns_in % 2 == 0);
  ns_out = 2 * ns_in;
  vm_out = new_adj_vmesh(bp->mem_arena, n_boundary, ns_out, vm_in->boundstart);

  /* First we adjust the boundary vertices of the input mesh, storing in output mesh. */
  for (i = 0; i < n_boundary; i++) {
    copy_v3_v3(mesh_vert(vm_out, i, 0, 0)->co, mesh_vert(vm_in, i, 0, 0)->co);
    for (k = 1; k < ns_in; k++) {
      copy_v3_v3(co, mesh_vert(vm_in, i, 0, k)->co);

      /* Smooth boundary rule. Custom profiles shouldn't be smoothed. */
      if (!bp->use_custom_profile) {
        copy_v3_v3(co1, mesh_vert(vm_in, i, 0, k - 1)->co);
        copy_v3_v3(co2, mesh_vert(vm_in, i, 0, k + 1)->co);

        add_v3_v3v3(acc, co1, co2);
        madd_v3_v3fl(acc, co, -2.0f);
        madd_v3_v3fl(co, acc, -1.0f / 6.0f);
      }

      copy_v3_v3(mesh_vert_canon(vm_out, i, 0, 2 * k)->co, co);
    }
  }
  /* Now adjust odd boundary vertices in output mesh, based on even ones. */
  bndv = vm_out->boundstart;
  for (i = 0; i < n_boundary; i++) {
    for (k = 1; k < ns_out; k += 2) {
      get_profile_point(bp, &bndv->profile, k, ns_out, co);

      /* Smooth if using a non-custom profile. */
      if (!bp->use_custom_profile) {
        copy_v3_v3(co1, mesh_vert_canon(vm_out, i, 0, k - 1)->co);
        copy_v3_v3(co2, mesh_vert_canon(vm_out, i, 0, k + 1)->co);

        add_v3_v3v3(acc, co1, co2);
        madd_v3_v3fl(acc, co, -2.0f);
        madd_v3_v3fl(co, acc, -1.0f / 6.0f);
      }

      copy_v3_v3(mesh_vert_canon(vm_out, i, 0, k)->co, co);
    }
    bndv = bndv->next;
  }
  vmesh_copy_equiv_verts(vm_out);

  /* Copy adjusted verts back into vm_in. */
  for (i = 0; i < n_boundary; i++) {
    for (k = 0; k < ns_in; k++) {
      copy_v3_v3(mesh_vert(vm_in, i, 0, k)->co, mesh_vert(vm_out, i, 0, 2 * k)->co);
    }
  }

  vmesh_copy_equiv_verts(vm_in);

  /* Now we do the internal vertices, using standard Catmull-Clark
   * and assuming all boundary vertices have valence 4. */

  /* The new face vertices. */
  for (i = 0; i < n_boundary; i++) {
    for (j = 0; j < ns_in2; j++) {
      for (k = 0; k < ns_in2; k++) {
        /* Face up and right from (j, k). */
        avg4(co,
             mesh_vert(vm_in, i, j, k),
             mesh_vert(vm_in, i, j, k + 1),
             mesh_vert(vm_in, i, j + 1, k),
             mesh_vert(vm_in, i, j + 1, k + 1));
        copy_v3_v3(mesh_vert(vm_out, i, 2 * j + 1, 2 * k + 1)->co, co);
      }
    }
  }

  /* The new vertical edge vertices. */
  for (i = 0; i < n_boundary; i++) {
    for (j = 0; j < ns_in2; j++) {
      for (k = 1; k <= ns_in2; k++) {
        /* Vertical edge between (j, k) and (j+1, k). */
        avg4(co,
             mesh_vert(vm_in, i, j, k),
             mesh_vert(vm_in, i, j + 1, k),
             mesh_vert_canon(vm_out, i, 2 * j + 1, 2 * k - 1),
             mesh_vert_canon(vm_out, i, 2 * j + 1, 2 * k + 1));
        copy_v3_v3(mesh_vert(vm_out, i, 2 * j + 1, 2 * k)->co, co);
      }
    }
  }

  /* The new horizontal edge vertices. */
  for (i = 0; i < n_boundary; i++) {
    for (j = 1; j < ns_in2; j++) {
      for (k = 0; k < ns_in2; k++) {
        /* Horizontal edge between (j, k) and (j, k+1). */
        avg4(co,
             mesh_vert(vm_in, i, j, k),
             mesh_vert(vm_in, i, j, k + 1),
             mesh_vert_canon(vm_out, i, 2 * j - 1, 2 * k + 1),
             mesh_vert_canon(vm_out, i, 2 * j + 1, 2 * k + 1));
        copy_v3_v3(mesh_vert(vm_out, i, 2 * j, 2 * k + 1)->co, co);
      }
    }
  }

  /* The new vertices, not on border. */
  gamma = 0.25f;
  beta = -gamma;
  for (i = 0; i < n_boundary; i++) {
    for (j = 1; j < ns_in2; j++) {
      for (k = 1; k <= ns_in2; k++) {
        /* co1 = centroid of adjacent new edge verts. */
        avg4(co1,
             mesh_vert_canon(vm_out, i, 2 * j, 2 * k - 1),
             mesh_vert_canon(vm_out, i, 2 * j, 2 * k + 1),
             mesh_vert_canon(vm_out, i, 2 * j - 1, 2 * k),
             mesh_vert_canon(vm_out, i, 2 * j + 1, 2 * k));
        /* co2 = centroid of adjacent new face verts. */
        avg4(co2,
             mesh_vert_canon(vm_out, i, 2 * j - 1, 2 * k - 1),
             mesh_vert_canon(vm_out, i, 2 * j + 1, 2 * k - 1),
             mesh_vert_canon(vm_out, i, 2 * j - 1, 2 * k + 1),
             mesh_vert_canon(vm_out, i, 2 * j + 1, 2 * k + 1));
        /* Combine with original vert with alpha, beta, gamma factors. */
        copy_v3_v3(co, co1); /* Alpha = 1.0. */
        madd_v3_v3fl(co, co2, beta);
        madd_v3_v3fl(co, mesh_vert(vm_in, i, j, k)->co, gamma);
        copy_v3_v3(mesh_vert(vm_out, i, 2 * j, 2 * k)->co, co);
      }
    }
  }

  vmesh_copy_equiv_verts(vm_out);

  /* The center vertex is special. */
  gamma = sabin_gamma(n_boundary);
  beta = -gamma;
  /* Accumulate edge verts in co1, face verts in co2. */
  zero_v3(co1);
  zero_v3(co2);
  for (i = 0; i < n_boundary; i++) {
    add_v3_v3(co1, mesh_vert(vm_out, i, ns_in, ns_in - 1)->co);
    add_v3_v3(co2, mesh_vert(vm_out, i, ns_in - 1, ns_in - 1)->co);
    add_v3_v3(co2, mesh_vert(vm_out, i, ns_in - 1, ns_in + 1)->co);
  }
  copy_v3_v3(co, co1);
  mul_v3_fl(co, 1.0f / (float)n_boundary);
  madd_v3_v3fl(co, co2, beta / (2.0f * (float)n_boundary));
  madd_v3_v3fl(co, mesh_vert(vm_in, 0, ns_in2, ns_in2)->co, gamma);
  for (i = 0; i < n_boundary; i++) {
    copy_v3_v3(mesh_vert(vm_out, i, ns_in, ns_in)->co, co);
  }

  /* Final step: Copy the profile vertices to the VMesh's boundary. */
  bndv = vm_out->boundstart;
  for (i = 0; i < n_boundary; i++) {
    inext = (i + 1) % n_boundary;
    for (k = 0; k <= ns_out; k++) {
      get_profile_point(bp, &bndv->profile, k, ns_out, co);
      copy_v3_v3(mesh_vert(vm_out, i, 0, k)->co, co);
      if (k >= ns_in && k < ns_out) {
        copy_v3_v3(mesh_vert(vm_out, inext, ns_out - k, 0)->co, co);
      }
    }
    bndv = bndv->next;
  }

  return vm_out;
}

/* Special case for cube corner, when r is PRO_SQUARE_R, meaning straight sides. */
static VMesh *make_cube_corner_square(MemArena *mem_arena, int nseg)
{
  VMesh *vm;
  float co[3];
  int i, j, k, ns2;

  ns2 = nseg / 2;
  vm = new_adj_vmesh(mem_arena, 3, nseg, NULL);
  vm->count = 0; /* Reset, so the following loop will end up with correct count. */
  for (i = 0; i < 3; i++) {
    zero_v3(co);
    co[i] = 1.0f;
    add_new_bound_vert(mem_arena, vm, co);
  }
  for (i = 0; i < 3; i++) {
    for (j = 0; j <= ns2; j++) {
      for (k = 0; k <= ns2; k++) {
        if (!is_canon(vm, i, j, k)) {
          continue;
        }
        co[i] = 1.0f;
        co[(i + 1) % 3] = (float)k * 2.0f / (float)nseg;
        co[(i + 2) % 3] = (float)j * 2.0f / (float)nseg;
        copy_v3_v3(mesh_vert(vm, i, j, k)->co, co);
      }
    }
  }
  vmesh_copy_equiv_verts(vm);
  return vm;
}

/**
 * Special case for cube corner, when r is PRO_SQUARE_IN_R, meaning inward
 * straight sides.
 * We mostly don't want a VMesh at all for this case -- just a three-way weld
 * with a triangle in the middle for odd nseg.
 */
static VMesh *make_cube_corner_square_in(MemArena *mem_arena, int nseg)
{
  VMesh *vm;
  float co[3];
  float b;
  int i, k, ns2, odd;

  ns2 = nseg / 2;
  odd = nseg % 2;
  vm = new_adj_vmesh(mem_arena, 3, nseg, NULL);
  vm->count = 0; /* Reset, so following loop will end up with correct count. */
  for (i = 0; i < 3; i++) {
    zero_v3(co);
    co[i] = 1.0f;
    add_new_bound_vert(mem_arena, vm, co);
  }
  if (odd) {
    b = 2.0f / (2.0f * (float)ns2 + (float)M_SQRT2);
  }
  else {
    b = 2.0f / (float)nseg;
  }
  for (i = 0; i < 3; i++) {
    for (k = 0; k <= ns2; k++) {
      co[i] = 1.0f - (float)k * b;
      co[(i + 1) % 3] = 0.0f;
      co[(i + 2) % 3] = 0.0f;
      copy_v3_v3(mesh_vert(vm, i, 0, k)->co, co);
      co[(i + 1) % 3] = 1.0f - (float)k * b;
      co[(i + 2) % 3] = 0.0f;
      co[i] = 0.0f;
      copy_v3_v3(mesh_vert(vm, i, 0, nseg - k)->co, co);
    }
  }
  return vm;
}

/**
 * Make a VMesh with nseg segments that covers the unit radius sphere octant
 * with center at (0,0,0).
 * This has BoundVerts at (1,0,0), (0,1,0) and (0,0,1), with quarter circle arcs
 * on the faces for the orthogonal planes through the origin.
 */
static VMesh *make_cube_corner_adj_vmesh(BevelParams *bp)
{
  MemArena *mem_arena = bp->mem_arena;
  int nseg = bp->seg;
  float r = bp->pro_super_r;
  VMesh *vm0, *vm1;
  BoundVert *bndv;
  int i, j, k, ns2;
  float co[3], coc[3];

  if (!bp->use_custom_profile) {
    if (r == PRO_SQUARE_R) {
      return make_cube_corner_square(mem_arena, nseg);
    }
    else if (r == PRO_SQUARE_IN_R) {
      return make_cube_corner_square_in(mem_arena, nseg);
    }
  }

  /* Initial mesh has 3 sides and 2 segments on each side. */
  vm0 = new_adj_vmesh(mem_arena, 3, 2, NULL);
  vm0->count = 0; /* Reset, so the following loop will end up with correct count. */
  for (i = 0; i < 3; i++) {
    zero_v3(co);
    co[i] = 1.0f;
    add_new_bound_vert(mem_arena, vm0, co);
  }
  bndv = vm0->boundstart;
  for (i = 0; i < 3; i++) {
    /* Get point, 1/2 of the way around profile, on arc between this and next. */
    coc[i] = 1.0f;
    coc[(i + 1) % 3] = 1.0f;
    coc[(i + 2) % 3] = 0.0f;
    bndv->profile.super_r = r;
    copy_v3_v3(bndv->profile.start, bndv->nv.co);
    copy_v3_v3(bndv->profile.end, bndv->next->nv.co);
    copy_v3_v3(bndv->profile.middle, coc);
    copy_v3_v3(mesh_vert(vm0, i, 0, 0)->co, bndv->profile.start);
    copy_v3_v3(bndv->profile.plane_co, bndv->profile.start);
    cross_v3_v3v3(bndv->profile.plane_no, bndv->profile.start, bndv->profile.end);
    copy_v3_v3(bndv->profile.proj_dir, bndv->profile.plane_no);
    /* Calculate profiles again because we started over with new boundverts. */
    calculate_profile(bp, bndv, false, false); /* No custom profiles in this case. */

    /* Just building the boundaries here, so sample the profile halfway through. */
    get_profile_point(bp, &bndv->profile, 1, 2, mesh_vert(vm0, i, 0, 1)->co);

    bndv = bndv->next;
  }
  /* Center vertex. */
  copy_v3_fl(co, (float)M_SQRT1_3);

  if (nseg > 2) {
    if (r > 1.5f) {
      mul_v3_fl(co, 1.4f);
    }
    else if (r < 0.75f) {
      mul_v3_fl(co, 0.6f);
    }
  }
  copy_v3_v3(mesh_vert(vm0, 0, 1, 1)->co, co);

  vmesh_copy_equiv_verts(vm0);

  vm1 = vm0;
  while (vm1->seg < nseg) {
    vm1 = cubic_subdiv(bp, vm1);
  }
  if (vm1->seg != nseg) {
    vm1 = interp_vmesh(bp, vm1, nseg);
  }

  /* Now snap each vertex to the superellipsoid. */
  ns2 = nseg / 2;
  for (i = 0; i < 3; i++) {
    for (j = 0; j <= ns2; j++) {
      for (k = 0; k <= nseg; k++) {
        snap_to_superellipsoid(mesh_vert(vm1, i, j, k)->co, r, false);
      }
    }
  }

  return vm1;
}

/* Is this a good candidate for using tri_corner_adj_vmesh? */
static int tri_corner_test(BevelParams *bp, BevVert *bv)
{
  float ang, absang, totang, angdiff;
  EdgeHalf *e;
  int i;
  int in_plane_e = 0;

  /* The superellipse snapping of this case isn't helpful with custom profiles enabled. */
  if (bp->vertex_only || bp->use_custom_profile) {
    return -1;
  }
  if (bv->vmesh->count != 3) {
    return 0;
  }

  /* Only use the tri-corner special case if the offset is the same for every edge. */
  float offset = bv->edges[0].offset_l;

  totang = 0.0f;
  for (i = 0; i < bv->edgecount; i++) {
    e = &bv->edges[i];
    ang = BM_edge_calc_face_angle_signed_ex(e->e, 0.0f);
    absang = fabsf(ang);
    if (absang <= M_PI_4) {
      in_plane_e++;
    }
    else if (absang >= 3.0f * (float)M_PI_4) {
      return -1;
    }

    if (e->is_bev && !compare_ff(e->offset_l, offset, BEVEL_EPSILON)) {
      return -1;
    }

    totang += ang;
  }
  if (in_plane_e != bv->edgecount - 3) {
    return -1;
  }
  angdiff = fabsf(fabsf(totang) - 3.0f * (float)M_PI_2);
  if ((bp->pro_super_r == PRO_SQUARE_R && angdiff > (float)M_PI / 16.0f) ||
      (angdiff > (float)M_PI_4)) {
    return -1;
  }
  if (bv->edgecount != 3 || bv->selcount != 3) {
    return 0;
  }
  return 1;
}

static VMesh *tri_corner_adj_vmesh(BevelParams *bp, BevVert *bv)
{
  int i, j, k, ns, ns2;
  float co0[3], co1[3], co2[3];
  float mat[4][4], v[4];
  VMesh *vm;
  BoundVert *bndv;

  bndv = bv->vmesh->boundstart;
  copy_v3_v3(co0, bndv->nv.co);
  bndv = bndv->next;
  copy_v3_v3(co1, bndv->nv.co);
  bndv = bndv->next;
  copy_v3_v3(co2, bndv->nv.co);
  make_unit_cube_map(co0, co1, co2, bv->v->co, mat);
  ns = bp->seg;
  ns2 = ns / 2;
  vm = make_cube_corner_adj_vmesh(bp);
  for (i = 0; i < 3; i++) {
    for (j = 0; j <= ns2; j++) {
      for (k = 0; k <= ns; k++) {
        copy_v3_v3(v, mesh_vert(vm, i, j, k)->co);
        v[3] = 1.0f;
        mul_m4_v4(mat, v);
        copy_v3_v3(mesh_vert(vm, i, j, k)->co, v);
      }
    }
  }

  return vm;
}

/* Makes the mesh that replaces the original vertex, bounded by the profiles on the sides. */
static VMesh *adj_vmesh(BevelParams *bp, BevVert *bv)
{
  int n_bndv, nseg, i;
  VMesh *vm0, *vm1;
  float boundverts_center[3], original_vertex[3], negative_fullest[3], center_direction[3];
  BoundVert *bndv;
  MemArena *mem_arena = bp->mem_arena;
  float fullness;

  n_bndv = bv->vmesh->count;

  /* Same bevel as that of 3 edges of vert in a cube. */
  if (n_bndv == 3 && tri_corner_test(bp, bv) != -1 && bp->pro_super_r != PRO_SQUARE_IN_R) {
    return tri_corner_adj_vmesh(bp, bv);
  }

  /* First construct an initial control mesh, with nseg == 2. */
  nseg = bv->vmesh->seg;
  vm0 = new_adj_vmesh(mem_arena, n_bndv, 2, bv->vmesh->boundstart);

  /* Find the center of the boundverts that make up the vmesh. */
  bndv = vm0->boundstart;
  zero_v3(boundverts_center);
  for (i = 0; i < n_bndv; i++) {
    /* Boundaries just divide input polygon edges into 2 even segments. */
    copy_v3_v3(mesh_vert(vm0, i, 0, 0)->co, bndv->nv.co);
    get_profile_point(bp, &bndv->profile, 1, 2, mesh_vert(vm0, i, 0, 1)->co);
    add_v3_v3(boundverts_center, bndv->nv.co);
    bndv = bndv->next;
  }
  mul_v3_fl(boundverts_center, 1.0f / (float)n_bndv);

  /* To place the center vertex:
   * 'negative_fullest' is the reflection of the original vertex across the boundverts' center.
   * 'fullness' is the fraction of the way from the boundvert's centroid to the original vertex
   * (if positive) or to negative_fullest (if negative). */
  copy_v3_v3(original_vertex, bv->v->co);
  sub_v3_v3v3(negative_fullest, boundverts_center, original_vertex);
  add_v3_v3(negative_fullest, boundverts_center);

  /* Find the vertex mesh's start center with the profile's fullness. */
  fullness = bp->pro_spacing.fullness;
  sub_v3_v3v3(center_direction, original_vertex, boundverts_center);
  if (len_squared_v3(center_direction) > BEVEL_EPSILON_SQ) {
    if (bp->use_custom_profile) {
      fullness *= 2.0f;
      madd_v3_v3v3fl(mesh_vert(vm0, 0, 1, 1)->co, negative_fullest, center_direction, fullness);
    }
    else {
      madd_v3_v3v3fl(mesh_vert(vm0, 0, 1, 1)->co, boundverts_center, center_direction, fullness);
    }
  }
  else {
    copy_v3_v3(mesh_vert(vm0, 0, 1, 1)->co, boundverts_center);
  }
  vmesh_copy_equiv_verts(vm0);

  /* Do the subdivision process to go from the two segment start mesh to the final vertex mesh. */
  vm1 = vm0;
  do {
    vm1 = cubic_subdiv(bp, vm1);
  } while (vm1->seg < nseg);
  if (vm1->seg != nseg) {
    vm1 = interp_vmesh(bp, vm1, nseg);
  }
  return vm1;
}

/**
 * Snap co to the closest point on the profile for vpipe projected onto the plane
 * containing co with normal in the direction of edge vpipe->ebev.
 * For the square profiles, need to decide whether to snap to just one plane
 * or to the midpoint of the profile; do so if midline is true.
 */
static void snap_to_pipe_profile(BoundVert *vpipe, bool midline, float co[3])
{
  float va[3], vb[3], edir[3], va0[3], vb0[3], vmid0[3];
  float plane[4], m[4][4], minv[4][4], p[3], snap[3];
  Profile *pro = &vpipe->profile;
  EdgeHalf *e = vpipe->ebev;

  copy_v3_v3(va, pro->start);
  copy_v3_v3(vb, pro->end);
  if (compare_v3v3(va, vb, BEVEL_EPSILON_D)) {
    copy_v3_v3(co, va);
    return;
  }

  /* Get a plane with the normal pointing along the beveled edge. */
  sub_v3_v3v3(edir, e->e->v1->co, e->e->v2->co);
  plane_from_point_normal_v3(plane, co, edir);

  closest_to_plane_v3(va0, plane, va);
  closest_to_plane_v3(vb0, plane, vb);
  closest_to_plane_v3(vmid0, plane, pro->middle);
  if (make_unit_square_map(va0, vmid0, vb0, m) && invert_m4_m4(minv, m)) {
    /* Transform co and project it onto superellipse. */
    mul_v3_m4v3(p, minv, co);
    snap_to_superellipsoid(p, pro->super_r, midline);

    mul_v3_m4v3(snap, m, p);
    copy_v3_v3(co, snap);
  }
  else {
    /* Planar case: just snap to line va0--vb0. */
    closest_to_line_segment_v3(p, co, va0, vb0);
    copy_v3_v3(co, p);
  }
}

/**
 * See pipe_test for conditions that make 'pipe'; vpipe is the return value from that.
 * We want to make an ADJ mesh but then snap the vertices to the profile in a plane
 * perpendicular to the pipes.
 */
static VMesh *pipe_adj_vmesh(BevelParams *bp, BevVert *bv, BoundVert *vpipe)
{
  int i, j, k, n_bndv, ns, half_ns, ipipe1, ipipe2, ring;
  VMesh *vm;
  bool even, midline;
  float *profile_point_pipe1, *profile_point_pipe2, f;

  /* Some unecessary overhead running this subdivision with custom profile snapping later on. */
  vm = adj_vmesh(bp, bv);

  /* Now snap all interior coordinates to be on the epipe profile. */
  n_bndv = bv->vmesh->count;
  ns = bv->vmesh->seg;
  half_ns = ns / 2;
  even = (ns % 2) == 0;
  ipipe1 = vpipe->index;
  ipipe2 = vpipe->next->next->index;

  for (i = 0; i < n_bndv; i++) {
    for (j = 1; j <= half_ns; j++) {
      for (k = 0; k <= half_ns; k++) {
        if (!is_canon(vm, i, j, k)) {
          continue;
        }
        /* With a custom profile just copy the shape of the profile at each ring. */
        if (bp->use_custom_profile) {
          /* Find both profile vertices that correspond to this point. */
          if (i == ipipe1 || i == ipipe2) {
            if (n_bndv == 3 && i == ipipe1) {
              /* This part of the vmesh is the triangular corner between the two pipe profiles. */
              ring = max_ii(j, k);
              profile_point_pipe2 = mesh_vert(vm, i, 0, ring)->co;
              profile_point_pipe1 = mesh_vert(vm, i, ring, 0)->co;
              /* End profile index increases with k on one side and j on the other. */
              f = ((k < j) ? min_ff(j, k) : ((2.0f * ring) - j)) / (2.0f * ring);
            }
            else {
              /* This is part of either pipe profile boundvert area in the 4-way intersection. */
              profile_point_pipe1 = mesh_vert(vm, i, 0, k)->co;
              profile_point_pipe2 = mesh_vert(vm, (i == ipipe1) ? ipipe2 : ipipe1, 0, ns - k)->co;
              f = (float)j / (float)ns; /* The ring index brings us closer to the other side. */
            }
          }
          else {
            /* The profile vertices are on both ends of each of the side profile's rings. */
            profile_point_pipe1 = mesh_vert(vm, i, j, 0)->co;
            profile_point_pipe2 = mesh_vert(vm, i, j, ns)->co;
            f = (float)k / (float)ns; /* Ring runs along the pipe, so segment is used here. */
          }

          /* Place the vertex by interpolatin between the two profile points using the factor. */
          interp_v3_v3v3(mesh_vert(vm, i, j, k)->co, profile_point_pipe1, profile_point_pipe2, f);
        }
        else {
          /* A tricky case is for the 'square' profiles and an even nseg: we want certain
           * vertices to snap to the midline on the pipe, not just to one plane or the other. */
          midline = even && k == half_ns &&
                    ((i == 0 && j == half_ns) || (i == ipipe1 || i == ipipe2));
          snap_to_pipe_profile(vpipe, midline, mesh_vert(vm, i, j, k)->co);
        }
      }
    }
  }
  return vm;
}

static void get_incident_edges(BMFace *f, BMVert *v, BMEdge **r_e1, BMEdge **r_e2)
{
  BMIter iter;
  BMEdge *e;

  *r_e1 = NULL;
  *r_e2 = NULL;
  if (!f) {
    return;
  }
  BM_ITER_ELEM (e, &iter, f, BM_EDGES_OF_FACE) {
    if (e->v1 == v || e->v2 == v) {
      if (*r_e1 == NULL) {
        *r_e1 = e;
      }
      else if (*r_e2 == NULL) {
        *r_e2 = e;
      }
    }
  }
}

static BMEdge *find_closer_edge(float *co, BMEdge *e1, BMEdge *e2)
{
  float dsq1, dsq2;

  BLI_assert(e1 != NULL && e2 != NULL);
  dsq1 = dist_squared_to_line_segment_v3(co, e1->v1->co, e1->v2->co);
  dsq2 = dist_squared_to_line_segment_v3(co, e2->v1->co, e2->v2->co);
  if (dsq1 < dsq2) {
    return e1;
  }
  else {
    return e2;
  }
}

/* Snap co to the closest edge of face f. Return the edge in *r_snap_e,
 * the coordinates of snap point in r_ snap_co,
 * and the distance squared to the snap point as function return */
static float snap_face_dist_squared(float *co, BMFace *f, BMEdge **r_snap_e, float *r_snap_co)
{
  BMIter iter;
  BMEdge *beste = NULL;
  float d2, beste_d2;
  BMEdge *e;
  float closest[3];

  beste_d2 = 1e20f;
  BM_ITER_ELEM (e, &iter, f, BM_EDGES_OF_FACE) {
    closest_to_line_segment_v3(closest, co, e->v1->co, e->v2->co);
    d2 = len_squared_v3v3(closest, co);
    if (d2 < beste_d2) {
      beste_d2 = d2;
      beste = e;
      copy_v3_v3(r_snap_co, closest);
    }
  }
  *r_snap_e = beste;
  return beste_d2;
}

static void build_center_ngon(BevelParams *bp, BMesh *bm, BevVert *bv, int mat_nr)
{
  VMesh *vm = bv->vmesh;
  BoundVert *v;
  int i, ns2;
  BMFace *frep, *f;
  BMEdge *frep_e1, *frep_e2, *frep_e;
  BMVert **vv = NULL;
  BMFace **vf = NULL;
  BMEdge **ve = NULL;
  BLI_array_staticdeclare(vv, BM_DEFAULT_NGON_STACK_SIZE);
  BLI_array_staticdeclare(vf, BM_DEFAULT_NGON_STACK_SIZE);
  BLI_array_staticdeclare(ve, BM_DEFAULT_NGON_STACK_SIZE);

  ns2 = vm->seg / 2;
  if (bv->any_seam) {
    frep = boundvert_rep_face(vm->boundstart, NULL);
    get_incident_edges(frep, bv->v, &frep_e1, &frep_e2);
  }
  else {
    frep = NULL;
    frep_e1 = frep_e2 = NULL;
  }
  v = vm->boundstart;
  do {
    i = v->index;
    BLI_array_append(vv, mesh_vert(vm, i, ns2, ns2)->v);
    if (frep) {
      BLI_array_append(vf, frep);
      frep_e = find_closer_edge(mesh_vert(vm, i, ns2, ns2)->v->co, frep_e1, frep_e2);
      BLI_array_append(ve, v == vm->boundstart ? NULL : frep_e);
    }
    else {
      BLI_array_append(vf, boundvert_rep_face(v, NULL));
      BLI_array_append(ve, NULL);
    }
  } while ((v = v->next) != vm->boundstart);
  f = bev_create_ngon(bm, vv, BLI_array_len(vv), vf, frep, ve, mat_nr, true);
  record_face_kind(bp, f, F_VERT);

  BLI_array_free(vv);
  BLI_array_free(vf);
  BLI_array_free(ve);
}

/* Special case of bevel_build_rings when tri-corner and profile is 0.
 * There is no corner mesh except, if nseg odd, for a center poly.
 * Boundary verts merge with previous ones according to pattern:
 * (i, 0, k) merged with (i+1, 0, ns-k) for k <= ns/2. */
static void build_square_in_vmesh(BevelParams *bp, BMesh *bm, BevVert *bv, VMesh *vm1)
{
  int n, ns, ns2, odd, i, k;
  VMesh *vm;

  vm = bv->vmesh;
  n = vm->count;
  ns = vm->seg;
  ns2 = ns / 2;
  odd = ns % 2;

  for (i = 0; i < n; i++) {
    for (k = 1; k < ns; k++) {
      copy_v3_v3(mesh_vert(vm, i, 0, k)->co, mesh_vert(vm1, i, 0, k)->co);
      if (i > 0 && k <= ns2) {
        mesh_vert(vm, i, 0, k)->v = mesh_vert(vm, i - 1, 0, ns - k)->v;
      }
      else if (i == n - 1 && k > ns2) {
        mesh_vert(vm, i, 0, k)->v = mesh_vert(vm, 0, 0, ns - k)->v;
      }
      else {
        create_mesh_bmvert(bm, vm, i, 0, k, bv->v);
      }
    }
  }
  if (odd) {
    for (i = 0; i < n; i++) {
      mesh_vert(vm, i, ns2, ns2)->v = mesh_vert(vm, i, 0, ns2)->v;
    }
    build_center_ngon(bp, bm, bv, bp->mat_nr);
  }
}

/* Copy whichever of a and b is closer to v into r. */
static void closer_v3_v3v3v3(float r[3], float a[3], float b[3], float v[3])
{
  if (len_squared_v3v3(a, v) <= len_squared_v3v3(b, v)) {
    copy_v3_v3(r, a);
  }
  else {
    copy_v3_v3(r, b);
  }
}

/**
 * Special case of VMesh when profile == 1 and there are 3 or more beveled edges.
 * We want the effect of parallel offset lines (n/2 of them)
 * on each side of the center, for even n.
 * Wherever they intersect with each other between two successive beveled edges,
 * those intersections are part of the vmesh rings.
 * We have to move the boundary edges too -- the usual method is to make one profile plane between
 * successive BoundVerts, but for the effect we want here, there will be two planes,
 * one on each side of the original edge.
 * At the moment, this is not called for odd number of segments, though code does something if it
 * is.
 */
static VMesh *square_out_adj_vmesh(BevelParams *bp, BevVert *bv)
{
  int n_bndv, ns, ns2, odd, i, j, k, ikind, im1, clstride, iprev, ang_kind;
  float bndco[3], dir1[3], dir2[3], co1[3], co2[3], meet1[3], meet2[3], v1co[3], v2co[3];
  float *on_edge_cur, *on_edge_prev, *p;
  float ns2inv, finalfrac, ang;
  BoundVert *bndv;
  EdgeHalf *e1, *e2;
  VMesh *vm;
  float *centerline;
  bool *cset, v1set, v2set;

  n_bndv = bv->vmesh->count;
  ns = bv->vmesh->seg;
  ns2 = ns / 2;
  odd = ns % 2;
  ns2inv = 1.0f / (float)ns2;
  vm = new_adj_vmesh(bp->mem_arena, n_bndv, ns, bv->vmesh->boundstart);
  clstride = 3 * (ns2 + 1);
  centerline = MEM_mallocN((size_t)(clstride * n_bndv) * sizeof(float), "bevel");
  cset = MEM_callocN((size_t)n_bndv * sizeof(bool), "bevel");

  /* Find on_edge, place on bndv[i]'s elast where offset line would meet,
   * taking min-distance-to bv->v with position where next sector's offset line would meet. */
  bndv = vm->boundstart;
  for (i = 0; i < n_bndv; i++) {
    copy_v3_v3(bndco, bndv->nv.co);
    e1 = bndv->efirst;
    e2 = bndv->elast;
    ang_kind = ANGLE_STRAIGHT;
    if (e1 && e2) {
      ang_kind = edges_angle_kind(e1, e2, bv->v);
    }
    if (bndv->is_patch_start) {
      mid_v3_v3v3(centerline + clstride * i, bndv->nv.co, bndv->next->nv.co);
      cset[i] = true;
      bndv = bndv->next;
      i++;
      mid_v3_v3v3(centerline + clstride * i, bndv->nv.co, bndv->next->nv.co);
      cset[i] = true;
      bndv = bndv->next;
      i++;
      /* Leave cset[i] where it was - probably false, unless i == n - 1. */
    }
    else if (bndv->is_arc_start) {
      e1 = bndv->efirst;
      e2 = bndv->next->efirst;
      copy_v3_v3(centerline + clstride * i, bndv->profile.middle);
      bndv = bndv->next;
      cset[i] = true;
      i++;
      /* Leave cset[i] where it was - probably false, unless i == n - 1. */
    }
    else if (ang_kind == ANGLE_SMALLER) {
      sub_v3_v3v3(dir1, e1->e->v1->co, e1->e->v2->co);
      sub_v3_v3v3(dir2, e2->e->v1->co, e2->e->v2->co);
      add_v3_v3v3(co1, bndco, dir1);
      add_v3_v3v3(co2, bndco, dir2);
      /* Intersect e1 with line through bndv parallel to e2 to get v1co. */
      ikind = isect_line_line_v3(e1->e->v1->co, e1->e->v2->co, bndco, co2, meet1, meet2);
      if (ikind == 0) {
        v1set = false;
      }
      else {
        /* If the lines are skew (ikind == 2), want meet1 which is on e1. */
        copy_v3_v3(v1co, meet1);
        v1set = true;
      }
      /* Intersect e2 with line through bndv parallel to e1 to get v2co. */
      ikind = isect_line_line_v3(e2->e->v1->co, e2->e->v2->co, bndco, co1, meet1, meet2);
      if (ikind == 0) {
        v2set = false;
      }
      else {
        v2set = true;
        copy_v3_v3(v2co, meet1);
      }

      /* We want on_edge[i] to be min dist to bv->v of v2co and the v1co of next iteration. */
      on_edge_cur = centerline + clstride * i;
      iprev = (i == 0) ? n_bndv - 1 : i - 1;
      on_edge_prev = centerline + clstride * iprev;
      if (v2set) {
        if (cset[i]) {
          closer_v3_v3v3v3(on_edge_cur, on_edge_cur, v2co, bv->v->co);
        }
        else {
          copy_v3_v3(on_edge_cur, v2co);
          cset[i] = true;
        }
      }
      if (v1set) {
        if (cset[iprev]) {
          closer_v3_v3v3v3(on_edge_prev, on_edge_prev, v1co, bv->v->co);
        }
        else {
          copy_v3_v3(on_edge_prev, v1co);
          cset[iprev] = true;
        }
      }
    }
    bndv = bndv->next;
  }
  /* Maybe not everything was set by the previous loop. */
  bndv = vm->boundstart;
  for (i = 0; i < n_bndv; i++) {
    if (!cset[i]) {
      on_edge_cur = centerline + clstride * i;
      e1 = bndv->next->efirst;
      copy_v3_v3(co1, bndv->nv.co);
      copy_v3_v3(co2, bndv->next->nv.co);
      if (e1) {
        if (bndv->prev->is_arc_start && bndv->next->is_arc_start) {
          ikind = isect_line_line_v3(e1->e->v1->co, e1->e->v2->co, co1, co2, meet1, meet2);
          if (ikind != 0) {
            copy_v3_v3(on_edge_cur, meet1);
            cset[i] = true;
          }
        }
        else {
          if (bndv->prev->is_arc_start) {
            closest_to_line_segment_v3(on_edge_cur, co1, e1->e->v1->co, e1->e->v2->co);
          }
          else {
            closest_to_line_segment_v3(on_edge_cur, co2, e1->e->v1->co, e1->e->v2->co);
          }
          cset[i] = true;
        }
      }
      if (!cset[i]) {
        mid_v3_v3v3(on_edge_cur, co1, co2);
        cset[i] = true;
      }
    }
    bndv = bndv->next;
  }

  /* Fill in rest of centerlines by interpolation. */
  copy_v3_v3(co2, bv->v->co);
  bndv = vm->boundstart;
  for (i = 0; i < n_bndv; i++) {
    if (odd) {
      ang = 0.5f * angle_v3v3v3(bndv->nv.co, co1, bndv->next->nv.co);
      if (ang > BEVEL_SMALL_ANG) {
        /* finalfrac is the length along arms of isosceles triangle with top angle 2*ang
         * such that the base of the triangle is 1.
         * This is used in interpolation along center-line in odd case.
         * To avoid too big a drop from bv, cap finalfrac a 0.8 arbitrarily */
        finalfrac = 0.5f / sinf(ang);
        if (finalfrac > 0.8f) {
          finalfrac = 0.8f;
        }
      }
      else {
        finalfrac = 0.8f;
      }
      ns2inv = 1.0f / (ns2 + finalfrac);
    }

    p = centerline + clstride * i;
    copy_v3_v3(co1, p);
    p += 3;
    for (j = 1; j <= ns2; j++) {
      interp_v3_v3v3(p, co1, co2, j * ns2inv);
      p += 3;
    }
    bndv = bndv->next;
  }

  /* Coords of edges and mid or near-mid line. */
  bndv = vm->boundstart;
  for (i = 0; i < n_bndv; i++) {
    copy_v3_v3(co1, bndv->nv.co);
    copy_v3_v3(co2, centerline + clstride * (i == 0 ? n_bndv - 1 : i - 1));
    for (j = 0; j < ns2 + odd; j++) {
      interp_v3_v3v3(mesh_vert(vm, i, j, 0)->co, co1, co2, j * ns2inv);
    }
    copy_v3_v3(co2, centerline + clstride * i);
    for (k = 1; k <= ns2; k++) {
      interp_v3_v3v3(mesh_vert(vm, i, 0, k)->co, co1, co2, k * ns2inv);
    }
    bndv = bndv->next;
  }
  if (!odd) {
    copy_v3_v3(mesh_vert(vm, 0, ns2, ns2)->co, bv->v->co);
  }
  vmesh_copy_equiv_verts(vm);

  /* Fill in interior points by interpolation from edges to centerlines. */
  bndv = vm->boundstart;
  for (i = 0; i < n_bndv; i++) {
    im1 = (i == 0) ? n_bndv - 1 : i - 1;
    for (j = 1; j < ns2 + odd; j++) {
      for (k = 1; k <= ns2; k++) {
        ikind = isect_line_line_v3(mesh_vert(vm, i, 0, k)->co,
                                   centerline + clstride * im1 + 3 * k,
                                   mesh_vert(vm, i, j, 0)->co,
                                   centerline + clstride * i + 3 * j,
                                   meet1,
                                   meet2);
        if (ikind == 0) {
          /* How can this happen? fall back on interpolation in one direction if it does. */
          interp_v3_v3v3(mesh_vert(vm, i, j, k)->co,
                         mesh_vert(vm, i, 0, k)->co,
                         centerline + clstride * im1 + 3 * k,
                         j * ns2inv);
        }
        else if (ikind == 1) {
          copy_v3_v3(mesh_vert(vm, i, j, k)->co, meet1);
        }
        else {
          mid_v3_v3v3(mesh_vert(vm, i, j, k)->co, meet1, meet2);
        }
      }
    }
    bndv = bndv->next;
  }

  vmesh_copy_equiv_verts(vm);

  MEM_freeN(centerline);
  MEM_freeN(cset);
  return vm;
}

/**
 * Given that the boundary is built and the boundary BMVerts have been made,
 * calculate the positions of the interior mesh points for the M_ADJ pattern,
 * using cubic subdivision, then make the BMVerts and the new faces.
 */
static void bevel_build_rings(BevelParams *bp, BMesh *bm, BevVert *bv, BoundVert *vpipe)
{
  int n_bndv, ns, ns2, odd, i, j, k, ring;
  VMesh *vm1, *vm;
  BoundVert *bndv;
  BMVert *bmv1, *bmv2, *bmv3, *bmv4;
  BMFace *f, *f2, *r_f;
  BMEdge *bme, *bme1, *bme2, *bme3;
  EdgeHalf *e;
  int mat_nr = bp->mat_nr;

  n_bndv = bv->vmesh->count;
  ns = bv->vmesh->seg;
  ns2 = ns / 2;
  odd = ns % 2;
  BLI_assert(n_bndv >= 3 && ns > 1);

  if (bp->pro_super_r == PRO_SQUARE_R && bv->selcount >= 3 && !odd && !bp->use_custom_profile) {
    vm1 = square_out_adj_vmesh(bp, bv);
  }
  else if (vpipe) {
    vm1 = pipe_adj_vmesh(bp, bv, vpipe);
  }
  else if (tri_corner_test(bp, bv) == 1) {
    vm1 = tri_corner_adj_vmesh(bp, bv);
    /* The PRO_SQUARE_IN_R profile has boundary edges that merge
     * and no internal ring polys except possibly center ngon. */
    if (bp->pro_super_r == PRO_SQUARE_IN_R && !bp->use_custom_profile) {
      build_square_in_vmesh(bp, bm, bv, vm1);
      return;
    }
  }
  else {
    vm1 = adj_vmesh(bp, bv);
  }

  /* Copy final vmesh into bv->vmesh, make BMVerts and BMFaces. */
  vm = bv->vmesh;
  for (i = 0; i < n_bndv; i++) {
    for (j = 0; j <= ns2; j++) {
      for (k = 0; k <= ns; k++) {
        if (j == 0 && (k == 0 || k == ns)) {
          continue; /* Boundary corners already made. */
        }
        if (!is_canon(vm, i, j, k)) {
          continue;
        }
        copy_v3_v3(mesh_vert(vm, i, j, k)->co, mesh_vert(vm1, i, j, k)->co);
        create_mesh_bmvert(bm, vm, i, j, k, bv->v);
      }
    }
  }
  vmesh_copy_equiv_verts(vm);
  /* Make the polygons. */
  bndv = vm->boundstart;
  do {
    i = bndv->index;
    f = boundvert_rep_face(bndv, NULL);
    f2 = boundvert_rep_face(bndv->next, NULL);
    if (bp->vertex_only) {
      e = bndv->efirst;
    }
    else {
      e = bndv->ebev;
    }
    bme = e ? e->e : NULL;
    /* For odd ns, make polys with lower left corner at (i,j,k) for
     *    j in [0, ns2-1], k in [0, ns2].  And then the center ngon.
     * For even ns,
     *    j in [0, ns2-1], k in [0, ns2-1]. */
    for (j = 0; j < ns2; j++) {
      for (k = 0; k < ns2 + odd; k++) {
        bmv1 = mesh_vert(vm, i, j, k)->v;
        bmv2 = mesh_vert(vm, i, j, k + 1)->v;
        bmv3 = mesh_vert(vm, i, j + 1, k + 1)->v;
        bmv4 = mesh_vert(vm, i, j + 1, k)->v;
        BLI_assert(bmv1 && bmv2 && bmv3 && bmv4);
        if (bp->vertex_only) {
          if (j < k) {
            if (k == ns2 && j == ns2 - 1) {
              r_f = bev_create_quad_ex(bm,
                                       bmv1,
                                       bmv2,
                                       bmv3,
                                       bmv4,
                                       f2,
                                       f2,
                                       f2,
                                       f2,
                                       NULL,
                                       NULL,
                                       bndv->next->efirst->e,
                                       bme,
                                       mat_nr);
            }
            else {
              r_f = bev_create_quad(bm, bmv1, bmv2, bmv3, bmv4, f2, f2, f2, f2, mat_nr);
            }
          }
          else if (j > k) {
            r_f = bev_create_quad(bm, bmv1, bmv2, bmv3, bmv4, f2, f2, f2, f2, mat_nr);
          }
          else { /* j == k */
            /* Only one edge attached to v, since vertex_only. */
            if (e->is_seam) {
              r_f = bev_create_quad_ex(
                  bm, bmv1, bmv2, bmv3, bmv4, f2, f2, f2, f2, bme, NULL, bme, NULL, mat_nr);
            }
            else {
              r_f = bev_create_quad_ex(
                  bm, bmv1, bmv2, bmv3, bmv4, f2, f2, f2, f, bme, NULL, bme, NULL, mat_nr);
            }
          }
        }
        else { /* Edge bevel. */
          if (odd) {
            if (k == ns2) {
              if (e && e->is_seam) {
                r_f = bev_create_quad_ex(
                    bm, bmv1, bmv2, bmv3, bmv4, f, f, f, f, NULL, bme, bme, NULL, mat_nr);
              }
              else {
                r_f = bev_create_quad(bm, bmv1, bmv2, bmv3, bmv4, f, f2, f2, f, mat_nr);
              }
            }
            else {
              r_f = bev_create_quad(bm, bmv1, bmv2, bmv3, bmv4, f, f, f, f, mat_nr);
            }
          }
          else {
            bme1 = k == ns2 - 1 ? bme : NULL;
            bme3 = NULL;
            if (j == ns2 - 1 && bndv->prev->ebev) {
              bme3 = bndv->prev->ebev->e;
            }
            bme2 = bme1 != NULL ? bme1 : bme3;
            r_f = bev_create_quad_ex(
                bm, bmv1, bmv2, bmv3, bmv4, f, f, f, f, NULL, bme1, bme2, bme3, mat_nr);
          }
        }
        record_face_kind(bp, r_f, F_VERT);
      }
    }
  } while ((bndv = bndv->next) != vm->boundstart);

  /* Fix UVs along center lines if even number of segments. */
  if (!odd) {
    bndv = vm->boundstart;
    do {
      i = bndv->index;
      if (!bndv->any_seam) {
        for (ring = 1; ring < ns2; ring++) {
          BMVert *v_uv = mesh_vert(vm, i, ring, ns2)->v;
          if (v_uv) {
            bev_merge_uvs(bm, v_uv);
          }
        }
      }
    } while ((bndv = bndv->next) != vm->boundstart);
    bmv1 = mesh_vert(vm, 0, ns2, ns2)->v;
    if (bp->vertex_only || count_bound_vert_seams(bv) <= 1) {
      bev_merge_uvs(bm, bmv1);
    }
  }

  /* Center ngon. */
  if (odd) {
    build_center_ngon(bp, bm, bv, mat_nr);
  }
}

/**
 * Builds the vertex mesh when the vertex mesh type is set to "cut off" with a face closing
 * off each incoming edge's profile.
 *
 * TODO(Hans): Make cutoff VMesh work with outer miter != sharp. This should be possible but there
 * are two problems currently:
 *  - Miter profiles don't have plane_no filled, so down direction is incorrect.
 *  - Indexing profile points of miters with (i, 0, k) seems to return zero except for the first
 *    and last profile point.
 * TODO(Hans): Use repface / edge arrays for UV interpolation properly.
 */
static void bevel_build_cutoff(BevelParams *bp, BMesh *bm, BevVert *bv)
{
#ifdef DEBUG_CUSTOM_PROFILE_CUTOFF
  printf("BEVEL BUILD CUTOFF\n");
#  define F3(v) (v)[0], (v)[1], (v)[2]
  int j;
#endif
  int i;
  int n_bndv = bv->vmesh->count;
  BoundVert *bndv;
  float length;
  float down_direction[3], new_vert[3];
  bool build_center_face;
  /* BMFace *repface; */
  BMVert **face_bmverts = NULL;
  BMEdge **bmedges = NULL;
  BMFace **bmfaces = NULL;

  /* Find the locations for the corner vertices at the bottom of the cutoff faces. */
  bndv = bv->vmesh->boundstart;
  do {
    i = bndv->index;

    /* Find the "down" direction for this side of the cutoff face. */
    /* Find the direction along the intersection of the two adjecent profile normals. */
    cross_v3_v3v3(down_direction, bndv->profile.plane_no, bndv->prev->profile.plane_no);
    if (dot_v3v3(down_direction, bv->v->no) > 0.0f) {
      negate_v3(down_direction);
    }

    /* Move down from the boundvert by average profile height from the two adjacent profiles. */
    length = (bndv->profile.height / sqrtf(2.0f) + bndv->prev->profile.height / sqrtf(2.0f)) / 2;
    madd_v3_v3v3fl(new_vert, bndv->nv.co, down_direction, length);

    /* Use this location for this profile's first corner vert and the last profile's second. */
    copy_v3_v3(mesh_vert(bv->vmesh, i, 1, 0)->co, new_vert);
    copy_v3_v3(mesh_vert(bv->vmesh, bndv->prev->index, 1, 1)->co, new_vert);

  } while ((bndv = bndv->next) != bv->vmesh->boundstart);

#ifdef DEBUG_CUSTOM_PROFILE_CUTOFF
  printf("Corner vertices:\n");
  for (j = 0; j < n_bndv; j++) {
    printf("  (%.3f, %.3f, %.3f)\n", F3(mesh_vert(bv->vmesh, j, 1, 0)->co));
  }
#endif

  /* Disable the center face if the corner vertices share the same location. */
  build_center_face = true;
  if (n_bndv == 3) { /* Vertices only collapse with a 3-way VMesh. */
    build_center_face &= len_squared_v3v3(mesh_vert(bv->vmesh, 0, 1, 0)->co,
                                          mesh_vert(bv->vmesh, 1, 1, 0)->co) > BEVEL_EPSILON;
    build_center_face &= len_squared_v3v3(mesh_vert(bv->vmesh, 0, 1, 0)->co,
                                          mesh_vert(bv->vmesh, 2, 1, 0)->co) > BEVEL_EPSILON;
    build_center_face &= len_squared_v3v3(mesh_vert(bv->vmesh, 1, 1, 0)->co,
                                          mesh_vert(bv->vmesh, 2, 1, 0)->co) > BEVEL_EPSILON;
  }
#ifdef DEBUG_CUSTOM_PROFILE_CUTOFF
  printf("build_center_face: %d\n", build_center_face);
#endif

  /* Create the corner vertex BMVerts. */
  if (build_center_face) {
    do {
      i = bndv->index;
      create_mesh_bmvert(bm, bv->vmesh, i, 1, 0, bv->v);
      /* The second corner vertex for the previous profile shares this BMVert. */
      mesh_vert(bv->vmesh, bndv->prev->index, 1, 1)->v = mesh_vert(bv->vmesh, i, 1, 0)->v;

    } while ((bndv = bndv->next) != bv->vmesh->boundstart);
  }
  else {
    /* Use the same BMVert for all of the corner vertices. */
    create_mesh_bmvert(bm, bv->vmesh, 0, 1, 0, bv->v);
    for (i = 1; i < n_bndv; i++) {
      mesh_vert(bv->vmesh, i, 1, 0)->v = mesh_vert(bv->vmesh, 0, 1, 0)->v;
    }
  }

  /* Build the profile cutoff faces. */
  /* Extra one or two for corner vertices and one for last point along profile, or the size of the
   * center face array if it's bigger. */
#ifdef DEBUG_CUSTOM_PROFILE_CUTOFF
  printf("Building profile cutoff faces.\n");
#endif
  face_bmverts = BLI_memarena_alloc(
      bp->mem_arena, ((size_t)max_ii(bp->seg + 2 + build_center_face, n_bndv) * sizeof(BMVert *)));
  bndv = bv->vmesh->boundstart;
  do {
    i = bndv->index;
    BLI_array_staticdeclare(bmedges, BM_DEFAULT_NGON_STACK_SIZE);
    BLI_array_staticdeclare(bmfaces, BM_DEFAULT_NGON_STACK_SIZE);

    /* Add the first corner vertex under this boundvert. */
    face_bmverts[0] = mesh_vert(bv->vmesh, i, 1, 0)->v;

#ifdef DEBUG_CUSTOM_PROFILE_CUTOFF
    printf("Profile Number %d:\n", i);
    if (bndv->is_patch_start || bndv->is_arc_start) {
      printf("  Miter profile\n");
    }
    printf("  Corner 1: (%0.3f, %0.3f, %0.3f)\n", F3(mesh_vert(bv->vmesh, i, 1, 0)->co));
#endif

    /* Add profile point vertices to the face, including the last one. */
    for (int k = 0; k < bp->seg + 1; k++) {
      face_bmverts[k + 1] = mesh_vert(bv->vmesh, i, 0, k)->v; /* Leave room for first vert. */
#ifdef DEBUG_CUSTOM_PROFILE_CUTOFF
      printf("  Profile %d: (%0.3f, %0.3f, %0.3f)\n", k, F3(mesh_vert(bv->vmesh, i, 0, k)->co));
#endif
    }

    /* Add the second corner vert to complete the bottom of the face. */
    if (build_center_face) {
      face_bmverts[bp->seg + 2] = mesh_vert(bv->vmesh, i, 1, 1)->v;
#ifdef DEBUG_CUSTOM_PROFILE_CUTOFF
      printf("  Corner 2: (%0.3f, %0.3f, %0.3f)\n", F3(mesh_vert(bv->vmesh, i, 1, 1)->co));
#endif
    }

    /* Create the profile cutoff face for this boundvert. */
    /* repface = boundvert_rep_face(bndv, NULL); */
    bev_create_ngon(bm,
                    face_bmverts,
                    bp->seg + 2 + build_center_face,
                    bmfaces,
                    NULL,
                    bmedges,
                    bp->mat_nr,
                    true);

    BLI_array_free(bmedges);
    BLI_array_free(bmfaces);
  } while ((bndv = bndv->next) != bv->vmesh->boundstart);

  /* Create the bottom face if it should be built, reusing previous face_bmverts allocation. */
  if (build_center_face) {
    BLI_array_staticdeclare(bmedges, BM_DEFAULT_NGON_STACK_SIZE);
    BLI_array_staticdeclare(bmfaces, BM_DEFAULT_NGON_STACK_SIZE);

    /* Add all of the corner vertices to this face. */
    for (i = 0; i < n_bndv; i++) {
      /* Add verts from each cutoff face. */
      face_bmverts[i] = mesh_vert(bv->vmesh, i, 1, 0)->v;
    }
    /* BLI_array_append(bmfaces, repface); */
    bev_create_ngon(bm, face_bmverts, n_bndv, bmfaces, NULL, bmedges, bp->mat_nr, true);

    BLI_array_free(bmedges);
    BLI_array_free(bmfaces);
  }
}

/**
 * If we make a poly out of verts around bv, snapping to rep frep, will uv poly have zero area?
 * The uv poly is made by snapping all outside-of-frep vertices to the closest edge in frep.
 * Assume that this function is called when the only inside-of-frep vertex is vm->boundstart.
 * The poly will have zero area if the distance of that first vertex to some edge e is zero,
 * and all the other vertices snap to e or snap to an edge
 * at a point that is essentially on e too.
 */
static bool is_bad_uv_poly(BevVert *bv, BMFace *frep)
{
  BoundVert *v;
  BMEdge *snape, *firste;
  float co[3];
  VMesh *vm = bv->vmesh;
  float d2;

  v = vm->boundstart;
  d2 = snap_face_dist_squared(v->nv.v->co, frep, &firste, co);
  if (d2 > BEVEL_EPSILON_BIG_SQ || firste == NULL) {
    return false;
  }

  for (v = v->next; v != vm->boundstart; v = v->next) {
    snap_face_dist_squared(v->nv.v->co, frep, &snape, co);
    if (snape != firste) {
      d2 = dist_to_line_v3(co, firste->v1->co, firste->v2->co);
      if (d2 > BEVEL_EPSILON_BIG_SQ) {
        return false;
      }
    }
  }
  return true;
}

static BMFace *bevel_build_poly(BevelParams *bp, BMesh *bm, BevVert *bv)
{
  BMFace *f, *repface, *frep2;
  int n, k;
  VMesh *vm = bv->vmesh;
  BoundVert *bndv;
  BMEdge *repface_e1, *repface_e2, *frep_e;
  BMVert **bmverts = NULL;
  BMEdge **bmedges = NULL;
  BMFace **bmfaces = NULL;
  BLI_array_staticdeclare(bmverts, BM_DEFAULT_NGON_STACK_SIZE);
  BLI_array_staticdeclare(bmedges, BM_DEFAULT_NGON_STACK_SIZE);
  BLI_array_staticdeclare(bmfaces, BM_DEFAULT_NGON_STACK_SIZE);

  if (bv->any_seam) {
    repface = boundvert_rep_face(vm->boundstart, &frep2);
    if (frep2 && repface && is_bad_uv_poly(bv, repface)) {
      repface = frep2;
    }
    get_incident_edges(repface, bv->v, &repface_e1, &repface_e2);
  }
  else {
    repface = NULL;
    repface_e1 = repface_e2 = NULL;
  }
  bndv = vm->boundstart;
  n = 0;
  do {
    /* Accumulate vertices for vertex ngon. */
    /* Also accumulate faces in which uv interpolation is to happen for each. */
    BLI_array_append(bmverts, bndv->nv.v);
    if (repface) {
      BLI_array_append(bmfaces, repface);
      frep_e = find_closer_edge(bndv->nv.v->co, repface_e1, repface_e2);
      BLI_array_append(bmedges, n > 0 ? frep_e : NULL);
    }
    else {
      BLI_array_append(bmfaces, boundvert_rep_face(bndv, NULL));
      BLI_array_append(bmedges, NULL);
    }
    n++;
    if (bndv->ebev && bndv->ebev->seg > 1) {
      for (k = 1; k < bndv->ebev->seg; k++) {
        BLI_array_append(bmverts, mesh_vert(vm, bndv->index, 0, k)->v);
        if (repface) {
          BLI_array_append(bmfaces, repface);
          frep_e = find_closer_edge(
              mesh_vert(vm, bndv->index, 0, k)->v->co, repface_e1, repface_e2);
          BLI_array_append(bmedges, k < bndv->ebev->seg / 2 ? NULL : frep_e);
        }
        else {
          BLI_array_append(bmfaces, boundvert_rep_face(bndv, NULL));
          BLI_array_append(bmedges, NULL);
        }
        n++;
      }
    }
  } while ((bndv = bndv->next) != vm->boundstart);
  if (n > 2) {
    f = bev_create_ngon(bm, bmverts, n, bmfaces, repface, bmedges, bp->mat_nr, true);
    record_face_kind(bp, f, F_VERT);
  }
  else {
    f = NULL;
  }
  BLI_array_free(bmverts);
  BLI_array_free(bmedges);
  BLI_array_free(bmfaces);
  return f;
}

static void bevel_build_trifan(BevelParams *bp, BMesh *bm, BevVert *bv)
{
  BMFace *f;
  BLI_assert(next_bev(bv, NULL)->seg == 1 || bv->selcount == 1);

  f = bevel_build_poly(bp, bm, bv);

  if (f) {
    /* We have a polygon which we know starts at the previous vertex, make it into a fan. */
    BMLoop *l_fan = BM_FACE_FIRST_LOOP(f)->prev;
    BMVert *v_fan = l_fan->v;

    while (f->len > 3) {
      BMLoop *l_new;
      BMFace *f_new;
      BLI_assert(v_fan == l_fan->v);
      f_new = BM_face_split(bm, f, l_fan, l_fan->next->next, &l_new, NULL, false);
      flag_out_edge(bm, l_new->e);

      if (f_new->len > f->len) {
        f = f_new;
        if (l_new->v == v_fan) {
          l_fan = l_new;
        }
        else if (l_new->next->v == v_fan) {
          l_fan = l_new->next;
        }
        else if (l_new->prev->v == v_fan) {
          l_fan = l_new->prev;
        }
        else {
          BLI_assert(0);
        }
      }
      else {
        if (l_fan->v == v_fan) { /* l_fan = l_fan. */
        }
        else if (l_fan->next->v == v_fan) {
          l_fan = l_fan->next;
        }
        else if (l_fan->prev->v == v_fan) {
          l_fan = l_fan->prev;
        }
        else {
          BLI_assert(0);
        }
      }
      record_face_kind(bp, f_new, F_VERT);
    }
  }
}

/* Special case: vertex bevel with only two boundary verts.
 * Want to make a curved edge if seg > 0.
 * If there are no faces in the original mesh at the original vertex,
 * there will be no rebuilt face to make the edge between the boundary verts,
 * we have to make it here. */
static void bevel_vert_two_edges(BevelParams *bp, BMesh *bm, BevVert *bv)
{
  VMesh *vm = bv->vmesh;
  BMVert *v1, *v2;
  BMEdge *e_eg, *bme;
  Profile *pro;
  float co[3];
  BoundVert *bndv;
  int ns, k;

  BLI_assert(vm->count == 2 && bp->vertex_only);

  v1 = mesh_vert(vm, 0, 0, 0)->v;
  v2 = mesh_vert(vm, 1, 0, 0)->v;

  ns = vm->seg;
  if (ns > 1) {
    /* Set up profile parameters. */
    bndv = vm->boundstart;
    pro = &bndv->profile;
    pro->super_r = bp->pro_super_r;
    copy_v3_v3(pro->start, v1->co);
    copy_v3_v3(pro->end, v2->co);
    copy_v3_v3(pro->middle, bv->v->co);
    /* Don't use projection. */
    zero_v3(pro->plane_co);
    zero_v3(pro->plane_no);
    zero_v3(pro->proj_dir);

    for (k = 1; k < ns; k++) {
      get_profile_point(bp, pro, k, ns, co);
      copy_v3_v3(mesh_vert(vm, 0, 0, k)->co, co);
      create_mesh_bmvert(bm, vm, 0, 0, k, bv->v);
    }
    copy_v3_v3(mesh_vert(vm, 0, 0, ns)->co, v2->co);
    for (k = 1; k < ns; k++) {
      copy_mesh_vert(vm, 1, 0, ns - k, 0, 0, k);
    }
  }

  if (BM_vert_face_check(bv->v) == false) {
    e_eg = bv->edges[0].e;
    BLI_assert(e_eg != NULL);
    for (k = 0; k < ns; k++) {
      v1 = mesh_vert(vm, 0, 0, k)->v;
      v2 = mesh_vert(vm, 0, 0, k + 1)->v;
      BLI_assert(v1 != NULL && v2 != NULL);
      bme = BM_edge_create(bm, v1, v2, e_eg, BM_CREATE_NO_DOUBLE);
      if (bme) {
        flag_out_edge(bm, bme);
      }
    }
  }
}

/* Given that the boundary is built, now make the actual BMVerts
 * for the boundary and the interior of the vertex mesh. */
static void build_vmesh(BevelParams *bp, BMesh *bm, BevVert *bv)
{
  VMesh *vm = bv->vmesh;
  BoundVert *bndv, *weld1, *weld2, *vpipe;
  int n, ns, ns2, i, k, weld;
  float *v_weld1, *v_weld2, co[3];

  n = vm->count;
  ns = vm->seg;
  ns2 = ns / 2;

  vm->mesh = (NewVert *)BLI_memarena_alloc(bp->mem_arena,
                                           (size_t)(n * (ns2 + 1) * (ns + 1)) * sizeof(NewVert));

  /* Special case: just two beveled edges welded together. */
  weld = (bv->selcount == 2) && (vm->count == 2);
  weld1 = weld2 = NULL; /* Will hold two BoundVerts involved in weld. */

  /* Make (i, 0, 0) mesh verts for all i boundverts. */
  bndv = vm->boundstart;
  do {
    i = bndv->index;
    copy_v3_v3(mesh_vert(vm, i, 0, 0)->co, bndv->nv.co); /* Mesh NewVert to boundary NewVert. */
    create_mesh_bmvert(bm, vm, i, 0, 0, bv->v);          /* Create BMVert for that NewVert. */
    bndv->nv.v = mesh_vert(vm, i, 0, 0)->v; /* Use the BMVert for the BoundVert's NewVert. */

    /* Find boundverts and move profile planes if this is a weld case. */
    if (weld && bndv->ebev) {
      if (!weld1) {
        weld1 = bndv;
      }
      else { /* Get the last of the two BoundVerts. */
        weld2 = bndv;
        set_profile_params(bp, bv, weld1);
        set_profile_params(bp, bv, weld2);
        move_weld_profile_planes(bv, weld1, weld2);
      }
    }
  } while ((bndv = bndv->next) != vm->boundstart);

  /* It's simpler to calculate all profiles only once at a single moment, so keep just a single
   * profile calculation here, the last point before actual mesh verts are created. */
  calculate_vm_profiles(bp, bv, vm);

  /* Create new vertices and place them based on the profiles. */
  /* Copy other ends to (i, 0, ns) for all i, and fill in profiles for edges. */
  bndv = vm->boundstart;
  do {
    i = bndv->index;
    /* bndv's last vert along the boundary arc is the first of the next BoundVert's arc. */
    copy_mesh_vert(vm, i, 0, ns, bndv->next->index, 0, 0);

    if (vm->mesh_kind != M_ADJ) {
      for (k = 1; k < ns; k++) {
        if (bndv->ebev) {
          get_profile_point(bp, &bndv->profile, k, ns, co);
          copy_v3_v3(mesh_vert(vm, i, 0, k)->co, co);
          if (!weld) {
            /* This is done later with (possibly) better positions for the weld case. */
            create_mesh_bmvert(bm, vm, i, 0, k, bv->v);
          }
        }
        else if (n == 2 && !bndv->ebev) {
          /* case of one edge beveled and this is the v without ebev */
          /* want to copy the verts from other v, in reverse order */
          copy_mesh_vert(bv->vmesh, i, 0, k, 1 - i, 0, ns - k);
        }
      }
    }
  } while ((bndv = bndv->next) != vm->boundstart);

  /* Build the profile for the weld case (just a connection between the two boundverts). */
  if (weld) {
    bv->vmesh->mesh_kind = M_NONE;
    for (k = 1; k < ns; k++) {
      v_weld1 = mesh_vert(bv->vmesh, weld1->index, 0, k)->co;
      v_weld2 = mesh_vert(bv->vmesh, weld2->index, 0, ns - k)->co;
      if (bp->use_custom_profile) {
        /* Don't bother with special case profile check from below. */
        mid_v3_v3v3(co, v_weld1, v_weld2);
      }
      else {
        /* Use the point from the other profile if one is in a special case. */
        if (weld1->profile.super_r == PRO_LINE_R && weld2->profile.super_r != PRO_LINE_R) {
          copy_v3_v3(co, v_weld2);
        }
        else if (weld2->profile.super_r == PRO_LINE_R && weld1->profile.super_r != PRO_LINE_R) {
          copy_v3_v3(co, v_weld1);
        }
        else {
          /* In case the profiles aren't snapped to the same plane, use their midpoint. */
          mid_v3_v3v3(co, v_weld1, v_weld2);
        }
      }
      copy_v3_v3(mesh_vert(bv->vmesh, weld1->index, 0, k)->co, co);
      create_mesh_bmvert(bm, bv->vmesh, weld1->index, 0, k, bv->v);
    }
    for (k = 1; k < ns; k++) {
      copy_mesh_vert(bv->vmesh, weld2->index, 0, ns - k, weld1->index, 0, k);
    }
  }

  /* Make sure the pipe case ADJ mesh is used for both the "Grid Fill" (ADJ) and cutoff options. */
  vpipe = NULL;
  if ((vm->count == 3 || vm->count == 4) && bp->seg > 1) {
    /* Result is passed to bevel_build_rings to avoid overhead. */
    vpipe = pipe_test(bv);
    if (vpipe) {
      vm->mesh_kind = M_ADJ;
    }
  }

  switch (vm->mesh_kind) {
    case M_NONE:
      if (n == 2 && bp->vertex_only) {
        bevel_vert_two_edges(bp, bm, bv);
      }
      break;
    case M_POLY:
      bevel_build_poly(bp, bm, bv);
      break;
    case M_ADJ:
      bevel_build_rings(bp, bm, bv, vpipe);
      break;
    case M_TRI_FAN:
      bevel_build_trifan(bp, bm, bv);
      break;
    case M_CUTOFF:
      bevel_build_cutoff(bp, bm, bv);
  }
}

/* Return the angle between the two faces adjacent to e.
 * If there are not two, return 0. */
static float edge_face_angle(EdgeHalf *e)
{
  if (e->fprev && e->fnext) {
    /* Angle between faces is supplement of angle between face normals. */
    return (float)M_PI - angle_normalized_v3v3(e->fprev->no, e->fnext->no);
  }
  else {
    return 0.0f;
  }
}

/* Take care, this flag isn't cleared before use, it just so happens that its not set. */
#define BM_BEVEL_EDGE_TAG_ENABLE(bme) BM_ELEM_API_FLAG_ENABLE((bme), _FLAG_OVERLAP)
#define BM_BEVEL_EDGE_TAG_DISABLE(bme) BM_ELEM_API_FLAG_DISABLE((bme), _FLAG_OVERLAP)
#define BM_BEVEL_EDGE_TAG_TEST(bme) BM_ELEM_API_FLAG_TEST((bme), _FLAG_OVERLAP)

/**
 * Try to extend the bv->edges[] array beyond i by finding more successor edges.
 * This is a possibly exponential-time search, but it is only exponential in the number
 * of "internal faces" at a vertex -- i.e., faces that bridge between the edges that naturally
 * form a manifold cap around bv. It is rare to have more than one of these, so unlikely
 * that the exponential time case will be hit in practice.
 * Returns the new index i' where bv->edges[i'] ends the best path found.
 * The path will have the tags of all of its edges set.
 */
static int bevel_edge_order_extend(BMesh *bm, BevVert *bv, int i)
{
  BMEdge *bme, *bme2, *nextbme;
  BMLoop *l;
  BMIter iter;
  int j, tryj, bestj, nsucs, sucindex, k;
  BMEdge **sucs = NULL;
  BMEdge **save_path = NULL;
  BLI_array_staticdeclare(sucs, 4); /* Likely very few faces attached to same edge. */
  BLI_array_staticdeclare(save_path, BM_DEFAULT_NGON_STACK_SIZE);

  bme = bv->edges[i].e;
  /* Fill sucs with all unmarked edges of bmesh. */
  BM_ITER_ELEM (l, &iter, bme, BM_LOOPS_OF_EDGE) {
    bme2 = (l->v == bv->v) ? l->prev->e : l->next->e;
    if (!BM_BEVEL_EDGE_TAG_TEST(bme2)) {
      BLI_array_append(sucs, bme2);
    }
  }
  nsucs = BLI_array_len(sucs);

  bestj = j = i;
  for (sucindex = 0; sucindex < nsucs; sucindex++) {
    nextbme = sucs[sucindex];
    BLI_assert(nextbme != NULL);
    BLI_assert(!BM_BEVEL_EDGE_TAG_TEST(nextbme));
    BLI_assert(j + 1 < bv->edgecount);
    bv->edges[j + 1].e = nextbme;
    BM_BEVEL_EDGE_TAG_ENABLE(nextbme);
    tryj = bevel_edge_order_extend(bm, bv, j + 1);
    if (tryj > bestj ||
        (tryj == bestj && edges_face_connected_at_vert(bv->edges[tryj].e, bv->edges[0].e))) {
      bestj = tryj;
      BLI_array_clear(save_path);
      for (k = j + 1; k <= bestj; k++) {
        BLI_array_append(save_path, bv->edges[k].e);
      }
    }
    /* Now reset to path only-going-to-j state. */
    for (k = j + 1; k <= tryj; k++) {
      BM_BEVEL_EDGE_TAG_DISABLE(bv->edges[k].e);
      bv->edges[k].e = NULL;
    }
  }
  /* At this point we should be back at invariant on entrance: path up to j. */
  if (bestj > j) {
    /* Save_path should have from j + 1 to bestj inclusive.
     * Edges to add to edges[] before returning. */
    for (k = j + 1; k <= bestj; k++) {
      BLI_assert(save_path[k - (j + 1)] != NULL);
      bv->edges[k].e = save_path[k - (j + 1)];
      BM_BEVEL_EDGE_TAG_ENABLE(bv->edges[k].e);
    }
  }
  BLI_array_free(sucs);
  BLI_array_free(save_path);
  return bestj;
}

/* See if we have usual case for bevel edge order:
 * there is an ordering such that all the faces are between
 * successive edges and form a manifold "cap" at bv.
 * If this is the case, set bv->edges to such an order
 * and return true; else return unmark any partial path and return false.
 * Assume the first edge is already in bv->edges[0].e and it is tagged. */
#ifdef FASTER_FASTORDER
/* The alternative older code is O(n^2) where n = # of edges incident to bv->v.
 * This implementation is O(n * m) where m = average number of faces attached to an edge incident
 * to bv->v, which is almost certainly a small constant except in very strange cases.
 * But this code produces different choices of ordering than the legacy system,
 * leading to differences in vertex orders etc. in user models,
 * so for now will continue to use the legacy code. */
static bool fast_bevel_edge_order(BevVert *bv)
{
  int j, k, nsucs;
  BMEdge *bme, *bme2, *bmenext;
  BMIter iter;
  BMLoop *l;

  for (j = 1; j < bv->edgecount; j++) {
    bme = bv->edges[j - 1].e;
    bmenext = NULL;
    nsucs = 0;
    BM_ITER_ELEM (l, &iter, bme, BM_LOOPS_OF_EDGE) {
      bme2 = (l->v == bv->v) ? l->prev->e : l->next->e;
      if (!BM_BEVEL_EDGE_TAG_TEST(bme2)) {
        nsucs++;
        if (bmenext == NULL) {
          bmenext = bme2;
        }
      }
    }
    if (nsucs == 0 || (nsucs == 2 && j != 1) || nsucs > 2 ||
        (j == bv->edgecount - 1 && !edges_face_connected_at_vert(bmenext, bv->edges[0].e))) {
      for (k = 1; k < j; k++) {
        BM_BEVEL_EDGE_TAG_DISABLE(bv->edges[k].e);
        bv->edges[k].e = NULL;
      }
      return false;
    }
    bv->edges[j].e = bmenext;
    BM_BEVEL_EDGE_TAG_ENABLE(bmenext);
  }
  return true;
}
#else
static bool fast_bevel_edge_order(BevVert *bv)
{
  BMEdge *bme, *bme2, *first_suc;
  BMIter iter, iter2;
  BMFace *f;
  EdgeHalf *e;
  int i, k, ntot, num_shared_face;

  ntot = bv->edgecount;

  /* Add edges to bv->edges in order that keeps adjacent edges sharing
   * a unique face, if possible. */
  e = &bv->edges[0];
  bme = e->e;
  if (!bme->l) {
    return false;
  }
  for (i = 1; i < ntot; i++) {
    /* Find an unflagged edge bme2 that shares a face f with previous bme. */
    num_shared_face = 0;
    first_suc = NULL; /* Keep track of first successor to match legacy behavior. */
    BM_ITER_ELEM (bme2, &iter, bv->v, BM_EDGES_OF_VERT) {
      if (BM_BEVEL_EDGE_TAG_TEST(bme2)) {
        continue;
      }
      BM_ITER_ELEM (f, &iter2, bme2, BM_FACES_OF_EDGE) {
        if (BM_face_edge_share_loop(f, bme)) {
          num_shared_face++;
          if (first_suc == NULL) {
            first_suc = bme2;
          }
        }
      }
      if (num_shared_face >= 3) {
        break;
      }
    }
    if (num_shared_face == 1 || (i == 1 && num_shared_face == 2)) {
      e = &bv->edges[i];
      e->e = bme = first_suc;
      BM_BEVEL_EDGE_TAG_ENABLE(bme);
    }
    else {
      for (k = 1; k < i; k++) {
        BM_BEVEL_EDGE_TAG_DISABLE(bv->edges[k].e);
        bv->edges[k].e = NULL;
      }
      return false;
    }
  }
  return true;
}
#endif

/* Fill in bv->edges with a good ordering of non-wire edges around bv->v.
 * Use only edges where BM_BEVEL_EDGE_TAG is disabled so far (if edge beveling, others are wire).
 * first_bme is a good edge to start with. */
static void find_bevel_edge_order(BMesh *bm, BevVert *bv, BMEdge *first_bme)
{
  BMEdge *bme, *bme2;
  BMIter iter;
  BMFace *f, *bestf;
  EdgeHalf *e;
  EdgeHalf *e2;
  BMLoop *l;
  int i, ntot;

  ntot = bv->edgecount;
  i = 0;
  for (;;) {
    BLI_assert(first_bme != NULL);
    bv->edges[i].e = first_bme;
    BM_BEVEL_EDGE_TAG_ENABLE(first_bme);
    if (i == 0 && fast_bevel_edge_order(bv)) {
      break;
    }
    i = bevel_edge_order_extend(bm, bv, i);
    i++;
    if (i >= bv->edgecount) {
      break;
    }
    /* Not done yet: find a new first_bme. */
    first_bme = NULL;
    BM_ITER_ELEM (bme, &iter, bv->v, BM_EDGES_OF_VERT) {
      if (BM_BEVEL_EDGE_TAG_TEST(bme)) {
        continue;
      }
      if (!first_bme) {
        first_bme = bme;
      }
      if (BM_edge_face_count(bme) == 1) {
        first_bme = bme;
        break;
      }
    }
  }
  /* Now fill in the faces. */
  for (i = 0; i < ntot; i++) {
    e = &bv->edges[i];
    e2 = (i == bv->edgecount - 1) ? &bv->edges[0] : &bv->edges[i + 1];
    bme = e->e;
    bme2 = e2->e;
    BLI_assert(bme != NULL);
    if (e->fnext != NULL || e2->fprev != NULL) {
      continue;
    }
    /* Which faces have successive loops that are for bme and bme2?
     * There could be more than one. E.g., in manifold ntot==2 case.
     * Prefer one that has loop in same direction as e. */
    bestf = NULL;
    BM_ITER_ELEM (l, &iter, bme, BM_LOOPS_OF_EDGE) {
      f = l->f;
      if ((l->prev->e == bme2 || l->next->e == bme2)) {
        if (!bestf || l->v == bv->v) {
          bestf = f;
        }
      }
      if (bestf) {
        e->fnext = e2->fprev = bestf;
      }
    }
  }
}

/* Construction around the vertex. */
static BevVert *bevel_vert_construct(BMesh *bm, BevelParams *bp, BMVert *v)
{
  BMEdge *bme;
  BevVert *bv;
  BMEdge *first_bme;
  BMVert *v1, *v2;
  BMIter iter;
  EdgeHalf *e;
  float weight, z;
  float vert_axis[3] = {0, 0, 0};
  int i, ccw_test_sum;
  int nsel = 0;
  int ntot = 0;
  int nwire = 0;
  int fcnt;

  /* Gather input selected edges.
   * Only bevel selected edges that have exactly two incident faces.
   * Want edges to be ordered so that they share faces.
   * There may be one or more chains of shared faces broken by
   * gaps where there are no faces.
   * Want to ignore wire edges completely for edge beveling.
   * TODO: make following work when more than one gap. */

  first_bme = NULL;
  BM_ITER_ELEM (bme, &iter, v, BM_EDGES_OF_VERT) {
    fcnt = BM_edge_face_count(bme);
    BM_BEVEL_EDGE_TAG_DISABLE(bme);
    if (BM_elem_flag_test(bme, BM_ELEM_TAG) && !bp->vertex_only) {
      BLI_assert(fcnt == 2);
      nsel++;
      if (!first_bme) {
        first_bme = bme;
      }
    }
    if (fcnt == 1) {
      /* Good to start face chain from this edge. */
      first_bme = bme;
    }
    if (fcnt > 0 || bp->vertex_only) {
      ntot++;
    }
    if (BM_edge_is_wire(bme)) {
      nwire++;
      /* If edge beveling, exclude wire edges from edges array.
       * Mark this edge as "chosen" so loop below won't choose it. */
      if (!bp->vertex_only) {
        BM_BEVEL_EDGE_TAG_ENABLE(bme);
      }
    }
  }
  if (!first_bme) {
    first_bme = v->e;
  }

  if ((nsel == 0 && !bp->vertex_only) || (ntot < 2 && bp->vertex_only)) {
    /* Signal this vert isn't being beveled. */
    BM_elem_flag_disable(v, BM_ELEM_TAG);
    return NULL;
  }

  bv = (BevVert *)BLI_memarena_alloc(bp->mem_arena, (sizeof(BevVert)));
  bv->v = v;
  bv->edgecount = ntot;
  bv->selcount = nsel;
  bv->wirecount = nwire;
  bv->offset = bp->offset;
  bv->edges = (EdgeHalf *)BLI_memarena_alloc(bp->mem_arena, ntot * sizeof(EdgeHalf));
  if (nwire) {
    bv->wire_edges = (BMEdge **)BLI_memarena_alloc(bp->mem_arena, nwire * sizeof(BMEdge *));
  }
  else {
    bv->wire_edges = NULL;
  }
  bv->vmesh = (VMesh *)BLI_memarena_alloc(bp->mem_arena, sizeof(VMesh));
  bv->vmesh->seg = bp->seg;

  BLI_ghash_insert(bp->vert_hash, v, bv);

  find_bevel_edge_order(bm, bv, first_bme);

  /* Fill in other attributes of EdgeHalfs. */
  for (i = 0; i < ntot; i++) {
    e = &bv->edges[i];
    bme = e->e;
    if (BM_elem_flag_test(bme, BM_ELEM_TAG) && !bp->vertex_only) {
      e->is_bev = true;
      e->seg = bp->seg;
    }
    else {
      e->is_bev = false;
      e->seg = 0;
    }
    e->is_rev = (bme->v2 == v);
    e->leftv = e->rightv = NULL;
    e->profile_index = 0;
  }

  /* Now done with tag flag. */
  BM_ITER_ELEM (bme, &iter, v, BM_EDGES_OF_VERT) {
    BM_BEVEL_EDGE_TAG_DISABLE(bme);
  }

  /* If edge array doesn't go CCW around vertex from average normal side,
   * reverse the array, being careful to reverse face pointers too. */
  if (ntot > 1) {
    ccw_test_sum = 0;
    for (i = 0; i < ntot; i++) {
      ccw_test_sum += bev_ccw_test(
          bv->edges[i].e, bv->edges[(i + 1) % ntot].e, bv->edges[i].fnext);
    }
    if (ccw_test_sum < 0) {
      for (i = 0; i <= (ntot / 2) - 1; i++) {
        SWAP(EdgeHalf, bv->edges[i], bv->edges[ntot - i - 1]);
        SWAP(BMFace *, bv->edges[i].fprev, bv->edges[i].fnext);
        SWAP(BMFace *, bv->edges[ntot - i - 1].fprev, bv->edges[ntot - i - 1].fnext);
      }
      if (ntot % 2 == 1) {
        i = ntot / 2;
        SWAP(BMFace *, bv->edges[i].fprev, bv->edges[i].fnext);
      }
    }
  }

  if (bp->vertex_only) {
    /* If weighted, modify offset by weight. */
    if (bp->dvert != NULL && bp->vertex_group != -1) {
      weight = BKE_defvert_find_weight(bp->dvert + BM_elem_index_get(v), bp->vertex_group);
      bv->offset *= weight;
    }
    else if (bp->use_weights) {
      weight = BM_elem_float_data_get(&bm->vdata, v, CD_BWEIGHT);
      bv->offset *= weight;
    }
    /* Find center axis. Note: Don't use vert normal, can give unwanted results. */
    if (ELEM(bp->offset_type, BEVEL_AMT_WIDTH, BEVEL_AMT_DEPTH)) {
      float edge_dir[3];
      for (i = 0, e = bv->edges; i < ntot; i++, e++) {
        v2 = BM_edge_other_vert(e->e, bv->v);
        sub_v3_v3v3(edge_dir, bv->v->co, v2->co);
        normalize_v3(edge_dir);
        add_v3_v3v3(vert_axis, vert_axis, edge_dir);
      }
    }
  }

  for (i = 0, e = bv->edges; i < ntot; i++, e++) {
    e->next = &bv->edges[(i + 1) % ntot];
    e->prev = &bv->edges[(i + ntot - 1) % ntot];

    /* Set offsets.  */
    if (e->is_bev) {
      /* Convert distance as specified by user into offsets along
       * faces on left side and right side of this edgehalf.
       * Except for percent method, offset will be same on each side. */

      switch (bp->offset_type) {
        case BEVEL_AMT_OFFSET:
          e->offset_l_spec = bp->offset;
          break;
        case BEVEL_AMT_WIDTH:
          z = fabsf(2.0f * sinf(edge_face_angle(e) / 2.0f));
          if (z < BEVEL_EPSILON) {
            e->offset_l_spec = 0.01f * bp->offset; /* Undefined behavior, so tiny bevel. */
          }
          else {
            e->offset_l_spec = bp->offset / z;
          }
          break;
        case BEVEL_AMT_DEPTH:
          z = fabsf(cosf(edge_face_angle(e) / 2.0f));
          if (z < BEVEL_EPSILON) {
            e->offset_l_spec = 0.01f * bp->offset; /* Undefined behavior, so tiny bevel. */
          }
          else {
            e->offset_l_spec = bp->offset / z;
          }
          break;
        case BEVEL_AMT_PERCENT:
          /* Offset needs to be such that it meets adjacent edges at percentage of their lengths.
           */
          v1 = BM_edge_other_vert(e->prev->e, v);
          v2 = BM_edge_other_vert(e->e, v);
          z = sinf(angle_v3v3v3(v1->co, v->co, v2->co));
          e->offset_l_spec = BM_edge_calc_length(e->prev->e) * bp->offset * z / 100.0f;
          v1 = BM_edge_other_vert(e->e, v);
          v2 = BM_edge_other_vert(e->next->e, v);
          z = sinf(angle_v3v3v3(v1->co, v->co, v2->co));
          e->offset_r_spec = BM_edge_calc_length(e->next->e) * bp->offset * z / 100.0f;
          break;
        case BEVEL_AMT_ABSOLUTE:
          /* Like Percent, but the amount gives the absolute distance along adjacent edges. */
          v1 = BM_edge_other_vert(e->prev->e, v);
          v2 = BM_edge_other_vert(e->e, v);
          z = sinf(angle_v3v3v3(v1->co, v->co, v2->co));
          e->offset_l_spec = bp->offset * z;
          v1 = BM_edge_other_vert(e->e, v);
          v2 = BM_edge_other_vert(e->next->e, v);
          z = sinf(angle_v3v3v3(v1->co, v->co, v2->co));
          e->offset_r_spec = bp->offset * z;
          break;
        default:
          BLI_assert(!"bad bevel offset kind");
          e->offset_l_spec = bp->offset;
          break;
      }
      if (bp->offset_type != BEVEL_AMT_PERCENT && bp->offset_type != BEVEL_AMT_ABSOLUTE) {
        e->offset_r_spec = e->offset_l_spec;
      }
      if (bp->use_weights) {
        weight = BM_elem_float_data_get(&bm->edata, e->e, CD_BWEIGHT);
        e->offset_l_spec *= weight;
        e->offset_r_spec *= weight;
      }
    }
    else if (bp->vertex_only) {
      /* Weight has already been applied to bv->offset, if present.
       * Transfer to e->offset_[lr]_spec according to offset_type. */
      float edge_dir[3];
      switch (bp->offset_type) {
        case BEVEL_AMT_OFFSET: {
          e->offset_l_spec = bv->offset;
          break;
        }
        case BEVEL_AMT_WIDTH: {
          v2 = BM_edge_other_vert(e->e, bv->v);
          sub_v3_v3v3(edge_dir, bv->v->co, v2->co);
          z = fabsf(2.0f * sinf(angle_v3v3(vert_axis, edge_dir)));
          if (z < BEVEL_EPSILON) {
            e->offset_l_spec = 0.01f * bp->offset; /* Undefined behavior, so tiny bevel. */
          }
          else {
            e->offset_l_spec = bp->offset / z;
          }
          break;
        }
        case BEVEL_AMT_DEPTH: {
          v2 = BM_edge_other_vert(e->e, bv->v);
          sub_v3_v3v3(edge_dir, bv->v->co, v2->co);
          z = fabsf(cosf(angle_v3v3(vert_axis, edge_dir)));
          if (z < BEVEL_EPSILON) {
            e->offset_l_spec = 0.01f * bp->offset; /* Undefined behavior, so tiny bevel. */
          }
          else {
            e->offset_l_spec = bp->offset / z;
          }
          break;
        }
        case BEVEL_AMT_PERCENT: {
          e->offset_l_spec = BM_edge_calc_length(e->e) * bv->offset / 100.0f;
          break;
        }
        case BEVEL_AMT_ABSOLUTE: {
          e->offset_l_spec = bv->offset;
          break;
        }
      }
      e->offset_r_spec = e->offset_l_spec;
    }
    else {
      e->offset_l_spec = e->offset_r_spec = 0.0f;
    }
    e->offset_l = e->offset_l_spec;
    e->offset_r = e->offset_r_spec;

    if (e->fprev && e->fnext) {
      e->is_seam = !contig_ldata_across_edge(bm, e->e, e->fprev, e->fnext);
    }
    else {
      e->is_seam = true;
    }
  }

  if (nwire) {
    i = 0;
    BM_ITER_ELEM (bme, &iter, v, BM_EDGES_OF_VERT) {
      if (BM_edge_is_wire(bme)) {
        BLI_assert(i < bv->wirecount);
        bv->wire_edges[i++] = bme;
      }
    }
    BLI_assert(i == bv->wirecount);
  }

  return bv;
}

/* Face f has at least one beveled vertex. Rebuild f. */
static bool bev_rebuild_polygon(BMesh *bm, BevelParams *bp, BMFace *f)
{
  BMIter liter, eiter, fiter;
  BMLoop *l, *lprev;
  BevVert *bv;
  BoundVert *v, *vstart, *vend;
  EdgeHalf *e, *eprev;
  VMesh *vm;
  int i, k, n, kstart, kend;
  bool do_rebuild = false;
  bool go_ccw, corner3special, keep, on_profile_start;
  BMVert *bmv;
  BMEdge *bme, *bme_new, *bme_prev;
  BMFace *f_new, *f_other;
  BMVert **vv = NULL;
  BMVert **vv_fix = NULL;
  BMEdge **ee = NULL;
  BLI_array_staticdeclare(vv, BM_DEFAULT_NGON_STACK_SIZE);
  BLI_array_staticdeclare(vv_fix, BM_DEFAULT_NGON_STACK_SIZE);
  BLI_array_staticdeclare(ee, BM_DEFAULT_NGON_STACK_SIZE);

  BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
    if (BM_elem_flag_test(l->v, BM_ELEM_TAG)) {
      lprev = l->prev;
      bv = find_bevvert(bp, l->v);
      vm = bv->vmesh;
      e = find_edge_half(bv, l->e);
      BLI_assert(e != NULL);
      bme = e->e;
      eprev = find_edge_half(bv, lprev->e);
      BLI_assert(eprev != NULL);

      /* Which direction around our vertex do we travel to match orientation of f? */
      if (e->prev == eprev) {
        if (eprev->prev == e) {
          /* Valence 2 vertex: use f is one of e->fnext or e->fprev to break tie. */
          go_ccw = (e->fnext != f);
        }
        else {
          go_ccw = true; /* Going ccw around bv to trace this corner. */
        }
      }
      else if (eprev->prev == e) {
        go_ccw = false; /* Going cw around bv to trace this corner. */
      }
      else {
        /* Edges in face are non-contiguous in our ordering around bv.
         * Which way should we go when going from eprev to e? */
        if (count_ccw_edges_between(eprev, e) < count_ccw_edges_between(e, eprev)) {
          /* Go counterclockewise from eprev to e. */
          go_ccw = true;
        }
        else {
          /* Go clockwise from eprev to e. */
          go_ccw = false;
        }
      }
      on_profile_start = false;
      if (go_ccw) {
        vstart = eprev->rightv;
        vend = e->leftv;
        if (e->profile_index > 0) {
          vstart = vstart->prev;
          on_profile_start = true;
        }
      }
      else {
        vstart = eprev->leftv;
        vend = e->rightv;
        if (eprev->profile_index > 0) {
          vstart = vstart->next;
          on_profile_start = true;
        }
      }
      BLI_assert(vstart != NULL && vend != NULL);
      v = vstart;
      if (!on_profile_start) {
        BLI_array_append(vv, v->nv.v);
        BLI_array_append(ee, bme);
      }
      while (v != vend) {
        /* Check for special case: multisegment 3rd face opposite a beveled edge with no vmesh. */
        corner3special = (vm->mesh_kind == M_NONE && v->ebev != e && v->ebev != eprev);
        if (go_ccw) {
          i = v->index;
          if (on_profile_start) {
            kstart = e->profile_index;
            on_profile_start = false;
          }
          else {
            kstart = 1;
          }
          if (eprev->rightv == v && eprev->profile_index > 0) {
            kend = eprev->profile_index;
          }
          else {
            kend = vm->seg;
          }
          for (k = kstart; k <= kend; k++) {
            bmv = mesh_vert(vm, i, 0, k)->v;
            if (bmv) {
              BLI_array_append(vv, bmv);
              BLI_array_append(ee, bme); /* TODO: Maybe better edge here. */
              if (corner3special && v->ebev && !v->ebev->is_seam && k != vm->seg) {
                BLI_array_append(vv_fix, bmv);
              }
            }
          }
          v = v->next;
        }
        else {
          /* Going cw. */
          i = v->prev->index;
          if (on_profile_start) {
            kstart = eprev->profile_index;
            on_profile_start = false;
          }
          else {
            kstart = vm->seg - 1;
          }
          if (e->rightv == v->prev && e->profile_index > 0) {
            kend = e->profile_index;
          }
          else {
            kend = 0;
          }
          for (k = kstart; k >= kend; k--) {
            bmv = mesh_vert(vm, i, 0, k)->v;
            if (bmv) {
              BLI_array_append(vv, bmv);
              BLI_array_append(ee, bme);
              if (corner3special && v->ebev && !v->ebev->is_seam && k != 0) {
                BLI_array_append(vv_fix, bmv);
              }
            }
          }
          v = v->prev;
        }
      }
      do_rebuild = true;
    }
    else {
      BLI_array_append(vv, l->v);
      BLI_array_append(ee, l->e);
    }
  }
  if (do_rebuild) {
    n = BLI_array_len(vv);
    f_new = bev_create_ngon(bm, vv, n, NULL, f, NULL, -1, true);

    for (k = 0; k < BLI_array_len(vv_fix); k++) {
      bev_merge_uvs(bm, vv_fix[k]);
    }

    /* Copy attributes from old edges. */
    BLI_assert(n == BLI_array_len(ee));
    bme_prev = ee[n - 1];
    for (k = 0; k < n; k++) {
      bme_new = BM_edge_exists(vv[k], vv[(k + 1) % n]);
      BLI_assert(ee[k] && bme_new);
      if (ee[k] != bme_new) {
        BM_elem_attrs_copy(bm, bm, ee[k], bme_new);
        /* Want to undo seam and smooth for corner segments
         * if those attrs aren't contiguous around face. */
        if (k < n - 1 && ee[k] == ee[k + 1]) {
          if (BM_elem_flag_test(ee[k], BM_ELEM_SEAM) &&
              !BM_elem_flag_test(bme_prev, BM_ELEM_SEAM)) {
            BM_elem_flag_disable(bme_new, BM_ELEM_SEAM);
          }
          /* Actually want "sharp" to be contiguous, so reverse the test. */
          if (!BM_elem_flag_test(ee[k], BM_ELEM_SMOOTH) &&
              BM_elem_flag_test(bme_prev, BM_ELEM_SMOOTH)) {
            BM_elem_flag_enable(bme_new, BM_ELEM_SMOOTH);
          }
        }
        else {
          bme_prev = ee[k];
        }
      }
    }

    /* Don't select newly or return created boundary faces. */
    if (f_new) {
      record_face_kind(bp, f_new, F_RECON);
      BM_elem_flag_disable(f_new, BM_ELEM_TAG);
      /* Also don't want new edges that aren't part of a new bevel face. */
      BM_ITER_ELEM (bme, &eiter, f_new, BM_EDGES_OF_FACE) {
        keep = false;
        BM_ITER_ELEM (f_other, &fiter, bme, BM_FACES_OF_EDGE) {
          if (BM_elem_flag_test(f_other, BM_ELEM_TAG)) {
            keep = true;
            break;
          }
        }
        if (!keep) {
          disable_flag_out_edge(bm, bme);
        }
      }
    }
  }

  BLI_array_free(vv);
  BLI_array_free(vv_fix);
  BLI_array_free(ee);
  return do_rebuild;
}

/* All polygons touching v need rebuilding because beveling v has made new vertices. */
static void bevel_rebuild_existing_polygons(BMesh *bm, BevelParams *bp, BMVert *v)
{
  void *faces_stack[BM_DEFAULT_ITER_STACK_SIZE];
  int faces_len, f_index;
  BMFace **faces = BM_iter_as_arrayN(
      bm, BM_FACES_OF_VERT, v, &faces_len, faces_stack, BM_DEFAULT_ITER_STACK_SIZE);

  if (LIKELY(faces != NULL)) {
    for (f_index = 0; f_index < faces_len; f_index++) {
      BMFace *f = faces[f_index];
      if (bev_rebuild_polygon(bm, bp, f)) {
        BM_face_kill(bm, f);
      }
    }

    if (faces != (BMFace **)faces_stack) {
      MEM_freeN(faces);
    }
  }
}

/* If there were any wire edges, they need to be reattached somewhere. */
static void bevel_reattach_wires(BMesh *bm, BevelParams *bp, BMVert *v)
{
  BMEdge *e;
  BMVert *vclosest, *vother, *votherclosest;
  BevVert *bv, *bvother;
  BoundVert *bndv, *bndvother;
  float d, dclosest;
  int i;

  bv = find_bevvert(bp, v);
  if (!bv || bv->wirecount == 0 || !bv->vmesh) {
    return;
  }

  for (i = 0; i < bv->wirecount; i++) {
    e = bv->wire_edges[i];
    /* Look for the new vertex closest to the other end of e. */
    vclosest = NULL;
    dclosest = FLT_MAX;
    votherclosest = NULL;
    vother = BM_edge_other_vert(e, v);
    bvother = NULL;
    if (BM_elem_flag_test(vother, BM_ELEM_TAG)) {
      bvother = find_bevvert(bp, vother);
      if (!bvother || !bvother->vmesh) {
        return; /* Shouldn't happen. */
      }
    }
    bndv = bv->vmesh->boundstart;
    do {
      if (bvother) {
        bndvother = bvother->vmesh->boundstart;
        do {
          d = len_squared_v3v3(bndvother->nv.co, bndv->nv.co);
          if (d < dclosest) {
            vclosest = bndv->nv.v;
            votherclosest = bndvother->nv.v;
            dclosest = d;
          }
        } while ((bndvother = bndvother->next) != bvother->vmesh->boundstart);
      }
      else {
        d = len_squared_v3v3(vother->co, bndv->nv.co);
        if (d < dclosest) {
          vclosest = bndv->nv.v;
          votherclosest = vother;
          dclosest = d;
        }
      }
    } while ((bndv = bndv->next) != bv->vmesh->boundstart);
    if (vclosest) {
      BM_edge_create(bm, vclosest, votherclosest, e, BM_CREATE_NO_DOUBLE);
    }
  }
}

static void bev_merge_end_uvs(BMesh *bm, BevVert *bv, EdgeHalf *e)
{
  VMesh *vm = bv->vmesh;
  int i, k, nseg;

  nseg = e->seg;
  i = e->leftv->index;
  for (k = 1; k < nseg; k++) {
    bev_merge_uvs(bm, mesh_vert(vm, i, 0, k)->v);
  }
}

/*
 * Is this BevVert the special case of a weld (no vmesh) where there are
 * four edges total, two are beveled, and the other two are on opposite sides?
 */
static bool bevvert_is_weld_cross(BevVert *bv)
{
  return (bv->edgecount == 4 && bv->selcount == 2 &&
          ((bv->edges[0].is_bev && bv->edges[2].is_bev) ||
           (bv->edges[1].is_bev && bv->edges[3].is_bev)));
}

/**
 * Copy edge attribute data across the non-beveled crossing edges of a cross weld.
 *
 * Situation looks like this:
 *
 *      e->next
 *        |
 * -------3-------
 * -------2-------
 * -------1------- e
 * -------0------
 *        |
 *      e->prev
 *
 * where e is the EdgeHalf of one of the beveled edges,
 * e->next and e->prev are EdgeHalfs for the unbeveled edges of the cross
 * and their attributes are to be copied to the edges 01, 12, 23.
 * The vert i is mesh_vert(vm, vmindex, 0, i)->v.
 */
static void weld_cross_attrs_copy(BMesh *bm, BevVert *bv, VMesh *vm, int vmindex, EdgeHalf *e)
{
  BMEdge *bme_prev, *bme_next, *bme;
  int i, nseg;
  bool disable_seam, enable_smooth;

  bme_prev = bme_next = NULL;
  for (i = 0; i < 4; i++) {
    if (&bv->edges[i] == e) {
      bme_prev = bv->edges[(i + 3) % 4].e;
      bme_next = bv->edges[(i + 1) % 4].e;
      break;
    }
  }
  BLI_assert(bme_prev && bme_next);

  /* Want seams and sharp edges to cross only if that way on both sides. */
  disable_seam = BM_elem_flag_test(bme_prev, BM_ELEM_SEAM) !=
                 BM_elem_flag_test(bme_next, BM_ELEM_SEAM);
  enable_smooth = BM_elem_flag_test(bme_prev, BM_ELEM_SMOOTH) !=
                  BM_elem_flag_test(bme_next, BM_ELEM_SMOOTH);

  nseg = e->seg;
  for (i = 0; i < nseg; i++) {
    bme = BM_edge_exists(mesh_vert(vm, vmindex, 0, i)->v, mesh_vert(vm, vmindex, 0, i + 1)->v);
    BLI_assert(bme);
    BM_elem_attrs_copy(bm, bm, bme_prev, bme);
    if (disable_seam) {
      BM_elem_flag_disable(bme, BM_ELEM_SEAM);
    }
    if (enable_smooth) {
      BM_elem_flag_enable(bme, BM_ELEM_SMOOTH);
    }
  }
}

/**
 * Build the bevel polygons along the selected Edge.
 */
static void bevel_build_edge_polygons(BMesh *bm, BevelParams *bp, BMEdge *bme)
{
  BevVert *bv1, *bv2;
  BMVert *bmv1, *bmv2, *bmv3, *bmv4;
  VMesh *vm1, *vm2;
  EdgeHalf *e1, *e2;
  BMEdge *bme1, *bme2, *center_bme;
  BMFace *f1, *f2, *f, *r_f;
  BMVert *verts[4];
  BMFace *faces[4];
  BMEdge *edges[4];
  BMLoop *l;
  BMIter iter;
  int k, nseg, i1, i2, odd, mid;
  int mat_nr = bp->mat_nr;

  if (!BM_edge_is_manifold(bme)) {
    return;
  }

  bv1 = find_bevvert(bp, bme->v1);
  bv2 = find_bevvert(bp, bme->v2);

  BLI_assert(bv1 && bv2);

  e1 = find_edge_half(bv1, bme);
  e2 = find_edge_half(bv2, bme);

  BLI_assert(e1 && e2);

  /*
   *      bme->v1
   *     / | \
   *   v1--|--v4
   *   |   |   |
   *   |   |   |
   *   v2--|--v3
   *     \ | /
   *      bme->v2
   */
  nseg = e1->seg;
  BLI_assert(nseg > 0 && nseg == e2->seg);

  bmv1 = e1->leftv->nv.v;
  bmv4 = e1->rightv->nv.v;
  bmv2 = e2->rightv->nv.v;
  bmv3 = e2->leftv->nv.v;

  BLI_assert(bmv1 && bmv2 && bmv3 && bmv4);

  f1 = e1->fprev;
  f2 = e1->fnext;
  faces[0] = faces[1] = f1;
  faces[2] = faces[3] = f2;
  i1 = e1->leftv->index;
  i2 = e2->leftv->index;
  vm1 = bv1->vmesh;
  vm2 = bv2->vmesh;

  verts[0] = bmv1;
  verts[1] = bmv2;
  odd = nseg % 2;
  mid = nseg / 2;
  center_bme = NULL;
  for (k = 1; k <= nseg; k++) {
    verts[3] = mesh_vert(vm1, i1, 0, k)->v;
    verts[2] = mesh_vert(vm2, i2, 0, nseg - k)->v;
    if (odd && k == mid + 1) {
      if (e1->is_seam) {
        /* Straddles a seam: choose to interpolate in f1 and snap right edge to bme. */
        edges[0] = edges[1] = NULL;
        edges[2] = edges[3] = bme;
        r_f = bev_create_ngon(bm, verts, 4, NULL, f1, edges, mat_nr, true);
      }
      else {
        /* Straddles but not a seam: interpolate left half in f1, right half in f2. */
        r_f = bev_create_ngon(bm, verts, 4, faces, NULL, NULL, mat_nr, true);
      }
    }
    else if (!odd && k == mid) {
      /* Left poly that touches an even center line on right. */
      edges[0] = edges[1] = NULL;
      edges[2] = edges[3] = bme;
      r_f = bev_create_ngon(bm, verts, 4, NULL, f1, edges, mat_nr, true);
      center_bme = BM_edge_exists(verts[2], verts[3]);
      BLI_assert(center_bme != NULL);
    }
    else if (!odd && k == mid + 1) {
      /* Right poly that touches an even center line on left. */
      edges[0] = edges[1] = bme;
      edges[2] = edges[3] = NULL;
      r_f = bev_create_ngon(bm, verts, 4, NULL, f2, edges, mat_nr, true);
    }
    else {
      /* Doesn't cross or touch the center line, so interpolate in appropriate f1 or f2. */
      f = (k <= mid) ? f1 : f2;
      r_f = bev_create_ngon(bm, verts, 4, NULL, f, NULL, mat_nr, true);
    }
    record_face_kind(bp, r_f, F_EDGE);
    /* Tag the long edges: those out of verts[0] and verts[2]. */
    BM_ITER_ELEM (l, &iter, r_f, BM_LOOPS_OF_FACE) {
      if (l->v == verts[0] || l->v == verts[2]) {
        BM_elem_flag_enable(l, BM_ELEM_LONG_TAG);
      }
    }
    verts[0] = verts[3];
    verts[1] = verts[2];
  }
  if (!odd) {
    if (!e1->is_seam) {
      bev_merge_edge_uvs(bm, center_bme, mesh_vert(vm1, i1, 0, mid)->v);
    }
    if (!e2->is_seam) {
      bev_merge_edge_uvs(bm, center_bme, mesh_vert(vm2, i2, 0, mid)->v);
    }
  }

  /* Fix UVs along end edge joints. A nop unless other side built already. */
  /* TODO: If some seam, may want to do selective merge. */
  if (!bv1->any_seam && bv1->vmesh->mesh_kind == M_NONE) {
    bev_merge_end_uvs(bm, bv1, e1);
  }
  if (!bv2->any_seam && bv2->vmesh->mesh_kind == M_NONE) {
    bev_merge_end_uvs(bm, bv2, e2);
  }

  /* Copy edge data to first and last edge. */
  bme1 = BM_edge_exists(bmv1, bmv2);
  bme2 = BM_edge_exists(bmv3, bmv4);
  BLI_assert(bme1 && bme2);
  BM_elem_attrs_copy(bm, bm, bme, bme1);
  BM_elem_attrs_copy(bm, bm, bme, bme2);

  /* If either end is a "weld cross", want continuity of edge attributes across end edge(s). */
  if (bevvert_is_weld_cross(bv1)) {
    weld_cross_attrs_copy(bm, bv1, vm1, i1, e1);
  }
  if (bevvert_is_weld_cross(bv2)) {
    weld_cross_attrs_copy(bm, bv2, vm2, i2, e2);
  }
}

/* Find xnew > x0 so that distance((x0,y0), (xnew, ynew)) = dtarget.
 * False position Illinois method used because the function is somewhat linear
 * -> linear interpolation converges fast.
 * Assumes that the gradient is always between 1 and -1 for x in [x0, x0+dtarget]. */
static double find_superellipse_chord_endpoint(double x0, double dtarget, float r, bool rbig)
{
  double xmin, xmax, ymin, ymax, dmaxerr, dminerr, dnewerr, xnew, ynew;
  double y0 = superellipse_co(x0, r, rbig);
  const double tol = 1e-13; /* accumulates for many segments so use low value. */
  const int maxiter = 10;
  bool lastupdated_upper;

  /* For gradient between -1 and 1, xnew can only be in [x0 + sqrt(2)/2*dtarget, x0 + dtarget]. */
  xmin = x0 + M_SQRT2 / 2.0 * dtarget;
  if (xmin > 1.0) {
    xmin = 1.0;
  }
  xmax = x0 + dtarget;
  if (xmax > 1.0) {
    xmax = 1.0;
  }
  ymin = superellipse_co(xmin, r, rbig);
  ymax = superellipse_co(xmax, r, rbig);

  /* Note: using distance**2 (no sqrt needed) does not converge that well. */
  dmaxerr = sqrt(pow((xmax - x0), 2) + pow((ymax - y0), 2)) - dtarget;
  dminerr = sqrt(pow((xmin - x0), 2) + pow((ymin - y0), 2)) - dtarget;

  xnew = xmax - dmaxerr * (xmax - xmin) / (dmaxerr - dminerr);
  lastupdated_upper = true;

  for (int iter = 0; iter < maxiter; iter++) {
    ynew = superellipse_co(xnew, r, rbig);
    dnewerr = sqrt(pow((xnew - x0), 2) + pow((ynew - y0), 2)) - dtarget;
    if (fabs(dnewerr) < tol) {
      break;
    }
    if (dnewerr < 0) {
      xmin = xnew;
      ymin = ynew;
      dminerr = dnewerr;
      if (!lastupdated_upper) {
        xnew = (dmaxerr / 2 * xmin - dminerr * xmax) / (dmaxerr / 2 - dminerr);
      }
      else {
        xnew = xmax - dmaxerr * (xmax - xmin) / (dmaxerr - dminerr);
      }
      lastupdated_upper = false;
    }
    else {
      xmax = xnew;
      ymax = ynew;
      dmaxerr = dnewerr;
      if (lastupdated_upper) {
        xnew = (dmaxerr * xmin - dminerr / 2 * xmax) / (dmaxerr - dminerr / 2);
      }
      else {
        xnew = xmax - dmaxerr * (xmax - xmin) / (dmaxerr - dminerr);
      }
      lastupdated_upper = true;
    }
  }
  return xnew;
}

/**
 * This search procedure to find equidistant points (x,y) in the first
 * superellipse quadrant works for every superellipse exponent but is more
 * expensive than known solutions for special cases.
 * Call the point on superellipse that intersects x=y line mx.
 * For r>=1 use only the range x in [0,mx] and mirror the rest along x=y line,
 * for r<1 use only x in [mx,1]. Points are initially spaced and iteratively
 * repositioned to have the same distance.
 */
static void find_even_superellipse_chords_general(int seg, float r, double *xvals, double *yvals)
{
  const int smoothitermax = 10;
  const double error_tol = 1e-7;
  int i;
  int imax = (seg + 1) / 2 - 1; /* Ceiling division - 1. */

  double d, dmin, dmax;
  double davg;
  double mx;
  double sum;
  double temp;

  bool precision_reached = true;
  bool seg_odd = seg % 2;
  bool rbig;

  if (r > 1.0f) {
    rbig = true;
    mx = pow(0.5, 1.0 / r);
  }
  else {
    rbig = false;
    mx = 1 - pow(0.5, 1.0 / r);
  }

  /* Initial positions, linear spacing along x axis. */
  for (i = 0; i <= imax; i++) {
    xvals[i] = i * mx / seg * 2;
    yvals[i] = superellipse_co(xvals[i], r, rbig);
  }
  yvals[0] = 1;

  /* Smooth distance loop. */
  for (int iter = 0; iter < smoothitermax; iter++) {
    sum = 0.0;
    dmin = 2.0;
    dmax = 0.0;
    /* Update distances between neighbor points. Store the highest and
     * lowest to see if the maximum error to average distance (which isn't
     * known yet) is below required precision. */
    for (i = 0; i < imax; i++) {
      d = sqrt(pow((xvals[i + 1] - xvals[i]), 2) + pow((yvals[i + 1] - yvals[i]), 2));
      sum += d;
      if (d > dmax) {
        dmax = d;
      }
      if (d < dmin) {
        dmin = d;
      }
    }
    /* For last distance, weight with 1/2 if seg_odd. */
    if (seg_odd) {
      sum += M_SQRT2 / 2 * (yvals[imax] - xvals[imax]);
      davg = sum / (imax + 0.5);
    }
    else {
      sum += sqrt(pow((xvals[imax] - mx), 2) + pow((yvals[imax] - mx), 2));
      davg = sum / (imax + 1.0);
    }
    /* Max error in tolerance? -> Quit. */
    if (dmax - davg > error_tol) {
      precision_reached = false;
    }
    if (dmin - davg < error_tol) {
      precision_reached = false;
    }
    if (precision_reached) {
      break;
    }

    /* Update new coordinates. */
    for (i = 1; i <= imax; i++) {
      xvals[i] = find_superellipse_chord_endpoint(xvals[i - 1], davg, r, rbig);
      yvals[i] = superellipse_co(xvals[i], r, rbig);
    }
  }

  /* Fill remaining. */
  if (!seg_odd) {
    xvals[imax + 1] = mx;
    yvals[imax + 1] = mx;
  }
  for (i = imax + 1; i <= seg; i++) {
    yvals[i] = xvals[seg - i];
    xvals[i] = yvals[seg - i];
  }

  if (!rbig) {
    for (i = 0; i <= seg; i++) {
      temp = xvals[i];
      xvals[i] = 1.0 - yvals[i];
      yvals[i] = 1.0 - temp;
    }
  }
}

/**
 * Find equidistant points (x0,y0), (x1,y1)... (xn,yn) on the superellipse
 * function in the first quadrant. For special profiles (linear, arc,
 * rectangle) the point can be calculated easily, for any other profile a more
 * expensive search procedure must be used because there is no known closed
 * form for equidistant parametrization.
 * xvals and yvals should be size n+1.
 */
static void find_even_superellipse_chords(int n, float r, double *xvals, double *yvals)
{
  int i, n2;
  double temp;
  bool seg_odd = n % 2;

  n2 = n / 2;

  /* Special cases. */
  if (r == PRO_LINE_R) {
    /* Linear spacing. */
    for (i = 0; i <= n; i++) {
      xvals[i] = (double)i / n;
      yvals[i] = 1.0 - (double)i / n;
    }
    return;
  }
  if (r == PRO_CIRCLE_R) {
    temp = (M_PI / 2) / n;
    /* Angle spacing. */
    for (i = 0; i <= n; i++) {
      xvals[i] = sin(i * temp);
      yvals[i] = cos(i * temp);
    }
    return;
  }
  if (r == PRO_SQUARE_IN_R) {
    /* n is even, distribute first and second half linear. */
    if (!seg_odd) {
      for (i = 0; i <= n2; i++) {
        xvals[i] = 0.0;
        yvals[i] = 1.0 - (double)i / n2;
        xvals[n - i] = yvals[i];
        yvals[n - i] = xvals[i];
      }
    }
    /* n is odd, so get one corner-cut chord. */
    else {
      temp = 1.0 / (n2 + M_SQRT2 / 2.0);
      for (i = 0; i <= n2; i++) {
        xvals[i] = 0.0;
        yvals[i] = 1.0 - (double)i * temp;
        xvals[n - i] = yvals[i];
        yvals[n - i] = xvals[i];
      }
    }
    return;
  }
  if (r == PRO_SQUARE_R) {
    /* n is even, distribute first and second half linear. */
    if (!seg_odd) {
      for (i = 0; i <= n2; i++) {
        xvals[i] = (double)i / n2;
        yvals[i] = 1.0;
        xvals[n - i] = yvals[i];
        yvals[n - i] = xvals[i];
      }
    }
    /* n is odd, so get one corner-cut chord. */
    else {
      temp = 1.0 / (n2 + M_SQRT2 / 2);
      for (i = 0; i <= n2; i++) {
        xvals[i] = (double)i * temp;
        yvals[i] = 1.0;
        xvals[n - i] = yvals[i];
        yvals[n - i] = xvals[i];
      }
    }
    return;
  }
  /* For general case use the more expensive search algorithm. */
  find_even_superellipse_chords_general(n, r, xvals, yvals);
}

/**
 * Find the profile's "fullness," which is the fraction of the space it takes up way from the
 * boundvert's centroid to the original vertex for a non-custom profile, or in the case of a
 * custom profile, the average "height" of the profile points along its centerline.
 */
static float find_profile_fullness(BevelParams *bp)
{
  float fullness;
  int nseg = bp->seg;

  /* Precalculated fullness for circle profile radius and more common low seg values. */
#define CIRCLE_FULLNESS_SEGS 11
  static const float circle_fullness[CIRCLE_FULLNESS_SEGS] = {
      0.0f,   /* nsegs == 1 */
      0.559f, /* 2 */
      0.642f, /* 3 */
      0.551f, /* 4 */
      0.646f, /* 5 */
      0.624f, /* 6 */
      0.646f, /* 7 */
      0.619f, /* 8 */
      0.647f, /* 9 */
      0.639f, /* 10 */
      0.647f, /* 11 */
  };

  if (bp->use_custom_profile) {
    /* Set fullness to the average "height" of the profile's sampled points. */
    fullness = 0.0f;
    for (int i = 0; i < nseg; i++) { /* Don't use the end points. */
      fullness += (float)(bp->pro_spacing.xvals[i] + bp->pro_spacing.yvals[i]) / (2.0f * nseg);
    }
  }
  else {
    /* An offline optimization process found fullness that led to closest fit to sphere as
     * a function of r and ns (for case of cube corner). */
    if (bp->pro_super_r == PRO_LINE_R) {
      fullness = 0.0f;
    }
    else if (bp->pro_super_r == PRO_CIRCLE_R && nseg > 0 && nseg <= CIRCLE_FULLNESS_SEGS) {
      fullness = circle_fullness[nseg - 1];
    }
    else {
      /* Linear regression fit found best linear function, separately for even/odd segs. */
      if (nseg % 2 == 0) {
        fullness = 2.4506f * bp->profile - 0.00000300f * nseg - 0.6266f;
      }
      else {
        fullness = 2.3635f * bp->profile + 0.000152f * nseg - 0.6060f;
      }
    }
  }
  return fullness;
}

/**
 * Fills the ProfileSpacing struct with the 2D coordinates for the profile's vertices.
 * The superellipse used for multisegment profiles does not have a closed-form way
 * to generate evenly spaced points along an arc. We use an expensive search procedure
 * to find the parameter values that lead to bp->seg even chords.
 * We also want spacing for a number of segments that is a power of 2 >= bp->seg (but at least 4).
 * Use doubles because otherwise we cannot come close to float precision for final results.
 *
 * \param pro_spacing: The struct to fill. Changes depending on whether there needs
 * to be a separate miter profile.
 */
static void set_profile_spacing(BevelParams *bp, ProfileSpacing *pro_spacing, bool custom)
{
  int seg, seg_2;

  seg = bp->seg;
  seg_2 = power_of_2_max_i(bp->seg);
  if (seg > 1) {
    /* Sample the input number of segments. */
    pro_spacing->xvals = (double *)BLI_memarena_alloc(bp->mem_arena,
                                                      (size_t)(seg + 1) * sizeof(double));
    pro_spacing->yvals = (double *)BLI_memarena_alloc(bp->mem_arena,
                                                      (size_t)(seg + 1) * sizeof(double));
    if (custom) {
      /* Make sure the curve profile's sample table is full. */
      if (bp->custom_profile->segments_len != seg || !bp->custom_profile->segments) {
        BKE_curveprofile_initialize((CurveProfile *)bp->custom_profile, (short)seg);
      }

      /* Copy segment locations into the profile spacing struct. */
      for (int i = 0; i < seg + 1; i++) {
        pro_spacing->xvals[i] = (double)bp->custom_profile->segments[i].y;
        pro_spacing->yvals[i] = (double)bp->custom_profile->segments[i].x;
      }
    }
    else {
      find_even_superellipse_chords(seg, bp->pro_super_r, pro_spacing->xvals, pro_spacing->yvals);
    }

    /* Sample the seg_2 segments used for subdividing the vertex meshes. */
    if (seg_2 == 2) {
      seg_2 = 4;
    }
    bp->pro_spacing.seg_2 = seg_2;
    if (seg_2 == seg) {
      pro_spacing->xvals_2 = pro_spacing->xvals;
      pro_spacing->yvals_2 = pro_spacing->yvals;
    }
    else {
      pro_spacing->xvals_2 = (double *)BLI_memarena_alloc(bp->mem_arena,
                                                          (size_t)(seg_2 + 1) * sizeof(double));
      pro_spacing->yvals_2 = (double *)BLI_memarena_alloc(bp->mem_arena,
                                                          (size_t)(seg_2 + 1) * sizeof(double));
      if (custom) {
        /* Make sure the curve profile widget's sample table is full of the seg_2 samples. */
        BKE_curveprofile_initialize((CurveProfile *)bp->custom_profile, (short)seg_2);

        /* Copy segment locations into the profile spacing struct. */
        for (int i = 0; i < seg_2 + 1; i++) {
          pro_spacing->xvals_2[i] = (double)bp->custom_profile->segments[i].y;
          pro_spacing->yvals_2[i] = (double)bp->custom_profile->segments[i].x;
        }
      }
      else {
        find_even_superellipse_chords(
            seg_2, bp->pro_super_r, pro_spacing->xvals_2, pro_spacing->yvals_2);
      }
    }
  }
  else { /* Only 1 segment, we don't need any profile information. */
    pro_spacing->xvals = NULL;
    pro_spacing->yvals = NULL;
    pro_spacing->xvals_2 = NULL;
    pro_spacing->yvals_2 = NULL;
    pro_spacing->seg_2 = 0;
  }
}

/**
 * Assume we have a situation like:
 * <pre>
 * a                 d
 *  \               /
 * A \             / C
 *    \ th1    th2/
 *     b---------c
 *          B
 * </pre>
 *
 * where edges are A, B, and C, following a face around vertices a, b, c, d.
 * th1 is angle abc and th2 is angle bcd;
 * and the argument EdgeHalf eb is B, going from b to c.
 * In general case, edge offset specs for A, B, C have
 * the form ka*t, kb*t, kc*t where ka, kb, kc are some factors
 * (may be 0) and t is the current bp->offset.
 * We want to calculate t at which the clone of B parallel
 * to it collapses. This can be calculated using trig.
 * Another case of geometry collision that can happen is
 * When B slides along A because A is unbeveled.
 * Then it might collide with a.  Similarly for B sliding along C.
 */
static float geometry_collide_offset(BevelParams *bp, EdgeHalf *eb)
{
  EdgeHalf *ea, *ec, *ebother;
  BevVert *bvc;
  BMLoop *lb;
  BMVert *va, *vb, *vc, *vd;
  float ka, kb, kc, g, h, t, den, no_collide_offset, th1, th2, sin1, sin2, tan1, tan2, limit;

  limit = no_collide_offset = bp->offset + 1e6;
  if (bp->offset == 0.0f) {
    return no_collide_offset;
  }
  kb = eb->offset_l_spec;
  ea = eb->next; /* Note: this is in direction b --> a. */
  ka = ea->offset_r_spec;
  if (eb->is_rev) {
    vc = eb->e->v1;
    vb = eb->e->v2;
  }
  else {
    vb = eb->e->v1;
    vc = eb->e->v2;
  }
  va = ea->is_rev ? ea->e->v1 : ea->e->v2;
  bvc = NULL;
  ebother = find_other_end_edge_half(bp, eb, &bvc);
  if (ebother != NULL) {
    ec = ebother->prev; /* Note: this is in direction c --> d. */
    vc = bvc->v;
    kc = ec->offset_l_spec;
    vd = ec->is_rev ? ec->e->v1 : ec->e->v2;
  }
  else {
    /* No bevvert for w, so C can't be beveled. */
    kc = 0.0f;
    ec = NULL;
    /* Find an edge from c that has same face. */
    lb = BM_face_edge_share_loop(eb->fnext, eb->e);
    if (!lb) {
      return no_collide_offset;
    }
    if (lb->next->v == vc) {
      vd = lb->next->next->v;
    }
    else if (lb->v == vc) {
      vd = lb->prev->v;
    }
    else {
      return no_collide_offset;
    }
  }
  if (ea->e == eb->e || (ec && ec->e == eb->e)) {
    return no_collide_offset;
  }
  ka = ka / bp->offset;
  kb = kb / bp->offset;
  kc = kc / bp->offset;
  th1 = angle_v3v3v3(va->co, vb->co, vc->co);
  th2 = angle_v3v3v3(vb->co, vc->co, vd->co);

  /* First calculate offset at which edge B collapses, which happens
   * when advancing clones of A, B, C all meet at a point.
   * This only happens if at least two of those three edges have non-zero k's. */
  sin1 = sinf(th1);
  sin2 = sinf(th2);
  if ((ka > 0.0f) + (kb > 0.0f) + (kc > 0.0f) >= 2) {
    tan1 = tanf(th1);
    tan2 = tanf(th2);
    g = tan1 * tan2;
    h = sin1 * sin2;
    den = g * (ka * sin2 + kc * sin1) + kb * h * (tan1 + tan2);
    if (den != 0.0f) {
      t = BM_edge_calc_length(eb->e);
      t *= g * h / den;
      if (t >= 0.0f) {
        limit = t;
      }
    }
  }

  /* Now check edge slide cases. */
  if (kb > 0.0f && ka == 0.0f /*&& bvb->selcount == 1 && bvb->edgecount > 2 */) {
    t = BM_edge_calc_length(ea->e);
    t *= sin1 / kb;
    if (t >= 0.0f && t < limit) {
      limit = t;
    }
  }
  if (kb > 0.0f && kc == 0.0f /* && bvc && ec && bvc->selcount == 1 && bvc->edgecount > 2 */) {
    t = BM_edge_calc_length(ec->e);
    t *= sin2 / kb;
    if (t >= 0.0f && t < limit) {
      limit = t;
    }
  }
  return limit;
}

/**
 * We have an edge A between vertices a and b, where EdgeHalf ea is the half of A that starts at a.
 * For vertex-only bevels, the new vertices slide from a at a rate ka*t and from b at a rate kb*t.
 * We want to calculate the t at which the two meet.
 */
static float vertex_collide_offset(BevelParams *bp, EdgeHalf *ea)
{
  float limit, ka, kb, no_collide_offset, la, kab;
  EdgeHalf *eb;

  limit = no_collide_offset = bp->offset + 1e6;
  if (bp->offset == 0.0f) {
    return no_collide_offset;
  }
  ka = ea->offset_l_spec / bp->offset;
  eb = find_other_end_edge_half(bp, ea, NULL);
  kb = eb ? eb->offset_l_spec / bp->offset : 0.0f;
  kab = ka + kb;
  la = BM_edge_calc_length(ea->e);
  if (kab <= 0.0f) {
    return no_collide_offset;
  }
  limit = la / kab;
  return limit;
}

/**
 * Calculate an offset that is the lesser of the current bp.offset and the maximum possible offset
 * before geometry collisions happen. If the offset changes as a result of this, adjust the current
 * edge offset specs to reflect this clamping, and store the new offset in bp.offset.
 */
static void bevel_limit_offset(BevelParams *bp, BMesh *bm)
{
  BevVert *bv;
  EdgeHalf *eh;
  BMIter iter;
  BMVert *bmv;
  float limited_offset, offset_factor, collision_offset;
  int i;

  limited_offset = bp->offset;
  BM_ITER_MESH (bmv, &iter, bm, BM_VERTS_OF_MESH) {
    if (!BM_elem_flag_test(bmv, BM_ELEM_TAG)) {
      continue;
    }
    bv = find_bevvert(bp, bmv);
    if (!bv) {
      continue;
    }
    for (i = 0; i < bv->edgecount; i++) {
      eh = &bv->edges[i];
      if (bp->vertex_only) {
        collision_offset = vertex_collide_offset(bp, eh);
        if (collision_offset < limited_offset) {
          limited_offset = collision_offset;
        }
      }
      else {
        collision_offset = geometry_collide_offset(bp, eh);
        if (collision_offset < limited_offset) {
          limited_offset = collision_offset;
        }
      }
    }
  }

  if (limited_offset < bp->offset) {
    /* All current offset specs have some number times bp->offset,
     * so we can just multiply them all by the reduction factor
     * of the offset to have the effect of recalculating the specs
     * with the new limited_offset.
     */
    offset_factor = limited_offset / bp->offset;
    BM_ITER_MESH (bmv, &iter, bm, BM_VERTS_OF_MESH) {
      if (!BM_elem_flag_test(bmv, BM_ELEM_TAG)) {
        continue;
      }
      bv = find_bevvert(bp, bmv);
      if (!bv) {
        continue;
      }
      for (i = 0; i < bv->edgecount; i++) {
        eh = &bv->edges[i];
        eh->offset_l_spec *= offset_factor;
        eh->offset_r_spec *= offset_factor;
        eh->offset_l *= offset_factor;
        eh->offset_r *= offset_factor;
      }
    }
    bp->offset = limited_offset;
  }
}

/**
 * - Currently only bevels BM_ELEM_TAG'd verts and edges.
 *
 * - Newly created faces, edges, and verts are BM_ELEM_TAG'd too,
 *   the caller needs to ensure these are cleared before calling
 *   if its going to use this tag.
 *
 * - If limit_offset is set, adjusts offset down if necessary
 *   to avoid geometry collisions.
 *
 * \warning all tagged edges _must_ be manifold.
 */
void BM_mesh_bevel(BMesh *bm,
                   const float offset,
                   const int offset_type,
                   const int segments,
                   const float profile,
                   const bool vertex_only,
                   const bool use_weights,
                   const bool limit_offset,
                   const struct MDeformVert *dvert,
                   const int vertex_group,
                   const int mat,
                   const bool loop_slide,
                   const bool mark_seam,
                   const bool mark_sharp,
                   const bool harden_normals,
                   const int face_strength_mode,
                   const int miter_outer,
                   const int miter_inner,
                   const float spread,
                   const float smoothresh,
                   const bool use_custom_profile,
                   const struct CurveProfile *custom_profile,
                   const int vmesh_method)
{
  BMIter iter, liter;
  BMVert *v, *v_next;
  BMEdge *e;
  BMFace *f;
  BMLoop *l;
  BevVert *bv;
  BevelParams bp = {NULL};

  bp.offset = offset;
  bp.offset_type = offset_type;
  bp.seg = segments;
  bp.profile = profile;
  bp.pro_super_r = -logf(2.0) / logf(sqrtf(profile)); /* Convert to superellipse exponent. */
  bp.vertex_only = vertex_only;
  bp.use_weights = use_weights;
  bp.loop_slide = loop_slide;
  bp.limit_offset = limit_offset;
  bp.offset_adjust = true;
  bp.dvert = dvert;
  bp.vertex_group = vertex_group;
  bp.mat_nr = mat;
  bp.mark_seam = mark_seam;
  bp.mark_sharp = mark_sharp;
  bp.harden_normals = harden_normals;
  bp.face_strength_mode = face_strength_mode;
  bp.miter_outer = miter_outer;
  bp.miter_inner = miter_inner;
  bp.spread = spread;
  bp.smoothresh = smoothresh;
  bp.face_hash = NULL;
  bp.use_custom_profile = use_custom_profile;
  bp.custom_profile = custom_profile;
  bp.vmesh_method = vmesh_method;

  /* Disable the miters with the cutoff vertex mesh method, this combination isn't useful anyway.
   */
  if (bp.vmesh_method == BEVEL_VMESH_CUTOFF) {
    bp.miter_outer = BEVEL_MITER_SHARP;
    bp.miter_inner = BEVEL_MITER_SHARP;
  }

  if (bp.seg <= 1) {
    bp.seg = 1;
  }

  if (profile >= 0.950f) { /* r ~ 692, so PRO_SQUARE_R is 1e4 */
    bp.pro_super_r = PRO_SQUARE_R;
  }
  else if (fabsf(bp.pro_super_r - PRO_CIRCLE_R) < 1e-4) {
    bp.pro_super_r = PRO_CIRCLE_R;
  }
  else if (fabsf(bp.pro_super_r - PRO_LINE_R) < 1e-4) {
    bp.pro_super_r = PRO_LINE_R;
  }
  else if (bp.pro_super_r < 1e-4) {
    bp.pro_super_r = PRO_SQUARE_IN_R;
  }

  if (bp.offset > 0) {
    /* Primary alloc. */
    bp.vert_hash = BLI_ghash_ptr_new(__func__);
    bp.mem_arena = BLI_memarena_new(MEM_SIZE_OPTIMAL(1 << 16), __func__);
    BLI_memarena_use_calloc(bp.mem_arena);

    /* Get the 2D profile point locations from either the superellipse or the custom profile. */
    set_profile_spacing(&bp, &bp.pro_spacing, bp.use_custom_profile);
    if (bp.seg > 1) {
      bp.pro_spacing.fullness = find_profile_fullness(&bp);
    }

    /* Get separate non-custom profile samples for the miter profiles if they are needed. */
    if (bp.use_custom_profile &&
        (bp.miter_inner != BEVEL_MITER_SHARP || bp.miter_outer != BEVEL_MITER_SHARP)) {
      set_profile_spacing(&bp, &bp.pro_spacing_miter, false);
    }

    bp.face_hash = BLI_ghash_ptr_new(__func__);
    BLI_ghash_flag_set(bp.face_hash, GHASH_FLAG_ALLOW_DUPES);

    /* Analyze input vertices, sorting edges and assigning initial new vertex positions. */
    BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
      if (BM_elem_flag_test(v, BM_ELEM_TAG)) {
        bv = bevel_vert_construct(bm, &bp, v);
        if (!limit_offset && bv) {
          build_boundary(&bp, bv, true);
        }
      }
    }

    /* Perhaps clamp offset to avoid geometry colliisions. */
    if (limit_offset) {
      bevel_limit_offset(&bp, bm);

      /* Assign initial new vertex positions. */
      BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
        if (BM_elem_flag_test(v, BM_ELEM_TAG)) {
          bv = find_bevvert(&bp, v);
          if (bv) {
            build_boundary(&bp, bv, true);
          }
        }
      }
    }

    /* Perhaps do a pass to try to even out widths. */
    if (!bp.vertex_only && bp.offset_adjust && bp.offset_type != BEVEL_AMT_PERCENT &&
        bp.offset_type != BEVEL_AMT_ABSOLUTE) {
      adjust_offsets(&bp, bm);
    }

    /* Maintain consistent orientations for the asymmetrical custom profiles. */
    if (bp.use_custom_profile) {
      BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
        if (BM_elem_flag_test(e, BM_ELEM_TAG)) {
          regularize_profile_orientation(&bp, e);
        }
      }
    }

    /* Build the meshes around vertices, now that positions are final. */
    BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
      if (BM_elem_flag_test(v, BM_ELEM_TAG)) {
        bv = find_bevvert(&bp, v);
        if (bv) {
          build_vmesh(&bp, bm, bv);
        }
      }
    }

    /* Build polygons for edges. */
    if (!bp.vertex_only) {
      BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
        if (BM_elem_flag_test(e, BM_ELEM_TAG)) {
          bevel_build_edge_polygons(bm, &bp, e);
        }
      }
    }

    /* Extend edge data like sharp edges and precompute normals for harden. */
    BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
      if (BM_elem_flag_test(v, BM_ELEM_TAG)) {
        bv = find_bevvert(&bp, v);
        if (bv) {
          bevel_extend_edge_data(bv);
        }
      }
    }

    /* Rebuild face polygons around affected vertices. */
    BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
      if (BM_elem_flag_test(v, BM_ELEM_TAG)) {
        bevel_rebuild_existing_polygons(bm, &bp, v);
        bevel_reattach_wires(bm, &bp, v);
      }
    }

    BM_ITER_MESH_MUTABLE (v, v_next, &iter, bm, BM_VERTS_OF_MESH) {
      if (BM_elem_flag_test(v, BM_ELEM_TAG)) {
        BLI_assert(find_bevvert(&bp, v) != NULL);
        BM_vert_kill(bm, v);
      }
    }

    if (bp.harden_normals) {
      bevel_harden_normals(&bp, bm);
    }
    if (bp.face_strength_mode != BEVEL_FACE_STRENGTH_NONE) {
      bevel_set_weighted_normal_face_strength(bm, &bp);
    }

    /* When called from operator (as opposed to modifier), bm->use_toolflags
     * will be set, and we need to transfer the oflags to BM_ELEM_TAGs. */
    if (bm->use_toolflags) {
      BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
        if (BMO_vert_flag_test(bm, v, VERT_OUT)) {
          BM_elem_flag_enable(v, BM_ELEM_TAG);
        }
      }
      BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
        if (BMO_edge_flag_test(bm, e, EDGE_OUT)) {
          BM_elem_flag_enable(e, BM_ELEM_TAG);
        }
      }
    }

    /* Clear the BM_ELEM_LONG_TAG tags, which were only set on some edges in F_EDGE faces. */
    BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
      if (get_face_kind(&bp, f) != F_EDGE) {
        continue;
      }
      BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
        BM_elem_flag_disable(l, BM_ELEM_LONG_TAG);
      }
    }

    /* Primary free. */
    BLI_ghash_free(bp.vert_hash, NULL, NULL);
    BLI_ghash_free(bp.face_hash, NULL, NULL);
    BLI_memarena_free(bp.mem_arena);
  }
}
