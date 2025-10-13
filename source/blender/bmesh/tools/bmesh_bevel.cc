/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bmesh
 *
 * Main functions for beveling a BMesh (used by the tool and modifier)
 */

#include <algorithm>

#include "MEM_guardedalloc.h"

#include "DNA_curveprofile_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"

#include "BLI_alloca.h"
#include "BLI_map.hh"
#include "BLI_math_base.h"
#include "BLI_math_base_safe.h"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_memarena.h"
#include "BLI_set.hh"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include "BKE_curveprofile.h"
#include "BKE_customdata.hh"
#include "BKE_deform.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.hh"

#include "eigen_capi.h"

#include "bmesh.hh"
#include "bmesh_bevel.hh" /* own include */

#include "./intern/bmesh_private.hh"

using blender::Map;
using blender::Set;
using blender::Vector;

// #define BEVEL_DEBUG_TIME
#ifdef BEVEL_DEBUG_TIME
#  include "BLI_time.h"
#endif

#define BEVEL_EPSILON_D 1e-6
#define BEVEL_EPSILON 1e-6f
#define BEVEL_EPSILON_SQ 1e-12f
#define BEVEL_EPSILON_BIG 1e-4f
#define BEVEL_EPSILON_BIG_SQ 1e-8f
#define BEVEL_EPSILON_ANG DEG2RADF(2.0f)
#define BEVEL_SMALL_ANG DEG2RADF(10.0f)
/** Difference in dot products that corresponds to 10 degree difference between vectors. */
#define BEVEL_SMALL_ANG_DOT (1.0f - cosf(BEVEL_SMALL_ANG))
/** Difference in dot products that corresponds to 2.0 degree difference between vectors. */
#define BEVEL_EPSILON_ANG_DOT (1.0f - cosf(BEVEL_EPSILON_ANG))
#define BEVEL_MAX_ADJUST_PCT 10.0f
#define BEVEL_MAX_AUTO_ADJUST_PCT 300.0f
#define BEVEL_MATCH_SPEC_WEIGHT 0.2

// #define DEBUG_CUSTOM_PROFILE_CUTOFF
/* Happens far too often, uncomment for development. */
// #define BEVEL_ASSERT_PROJECT

/* for testing */
// #pragma GCC diagnostic error "-Wpadded"

/* Constructed vertex, sometimes later instantiated as BMVert. */
struct NewVert {
  BMVert *v;
  float co[3];
  char _pad[4];
};

struct BoundVert;

/* Data for one end of an edge involved in a bevel. */
struct EdgeHalf {
  /** Other EdgeHalves connected to the same BevVert, in CCW order. */
  EdgeHalf *next, *prev;
  /** Original mesh edge. */
  BMEdge *e;
  /** Face between this edge and previous, if any. */
  BMFace *fprev;
  /** Face between this edge and next, if any. */
  BMFace *fnext;
  /** Left boundary vert (looking along edge to end). */
  BoundVert *leftv;
  /** Right boundary vert, if beveled. */
  BoundVert *rightv;
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
  /** Is e a seam for custom loop-data (e.g., UVs). */
  bool is_seam;
  /** Used during the custom profile orientation pass. */
  bool visited_rpo;
  char _pad[4];
};

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
struct Profile {
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
  /** Mark a special case so these parameters aren't reset with others. */
  bool special_params;
};
#define PRO_SQUARE_R 1e4f
#define PRO_CIRCLE_R 2.0f
#define PRO_LINE_R 1.0f
#define PRO_SQUARE_IN_R 0.0f

/**
 * The un-transformed 2D storage of profile vertex locations. Also, for non-custom profiles
 * this serves as a cache for the results of the expensive calculation of u parameter values to
 * get even spacing on superellipse for current BevelParams seg and pro_super_r.
 */
struct ProfileSpacing {
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
};

/**
 * If the mesh has custom data Loop layers that 'have math' we use this
 * data to help decide which face to use as representative when there
 * is an ambiguous choice as to which face to use, which happens
 * when there is an odd number of segments.
 *
 * The face_compent field of the following will only be set if there are an odd
 * number of segments. The it uses BMFace indices to index into it, so will
 * only be valid as long BMFaces are not added or deleted in the BMesh.
 * "Connected Component" here means connected in UV space:
 * i.e., one face is directly connected to another if they share an edge and
 * all of Loop UV custom layers are contiguous across that edge.
 */
struct MathLayerInfo {
  /** A connected-component id for each BMFace in the mesh. */
  int *face_component;
  /** Does the mesh have any custom loop uv layers? */
  bool has_math_layers;
};

/**
 * Auxiliary structure representing bevel face created by `bev_create_ngon` function. It holds
 * reference to both newly create ngon and a representative face (from the original mesh) it is
 * attached to. This information helps with merging UVs - bevel faces that share the same
 * `attached_frep` pointer should have their neighboring UV verts connected.
 */
struct UVFace {
  /** `BMesh` face which this `UVFace` represents. */
  BMFace *f;
  /** `BMFace` of the original mesh to which bevel face `f` is attached in UV space. */
  BMFace *attached_frep;
};

/**
 * An element in a cyclic boundary of a Vertex Mesh (VMesh), placed on each side of beveled edges
 * where each profile starts, or on each side of a miter.
 */
struct BoundVert {
  /** In CCW order. */
  BoundVert *next, *prev;
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
  BoundVert *adjchain;
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
  /** Length of seam starting from current boundvert to next boundvert with CCW ordering. */
  int seam_len;
  /** Same as seam_len but defines length of sharp edges. */
  int sharp_len;
};

enum MeshKind {
  M_NONE,    /* No polygon mesh needed. */
  M_POLY,    /* A simple polygon. */
  M_ADJ,     /* "Adjacent edges" mesh pattern. */
  M_TRI_FAN, /* A simple polygon - fan filled. */
  M_CUTOFF,  /* A triangulated face at the end of each profile. */
};

/** Data for the mesh structure replacing a vertex. */
struct VMesh {
  /** Allocated array - size and structure depends on kind. */
  NewVert *mesh;
  /** Start of boundary double-linked list. */
  BoundVert *boundstart;
  /** Number of vertices in the boundary. */
  int count;
  /** Common number of segments for segmented edges (same as bp->seg). */
  int seg;
  /** The kind of mesh to build at the corner vertex meshes. */
  MeshKind mesh_kind;

  int _pad;
};

/* Data for a vertex involved in a bevel. */
struct BevVert {
  /** Original mesh vertex. */
  BMVert *v;
  /** Total number of edges around the vertex (excluding wire edges if edge beveling). */
  int edgecount;
  /** Number of selected edges around the vertex. */
  int selcount;
  /** Count of wire edges. */
  int wirecount;
  /** Offset for this vertex, if vertex only bevel. */
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
};

/**
 * Face classification.
 * \note depends on `F_RECON > F_EDGE > F_VERT`.
 */
enum FKind {
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
};

/** Helper for keeping track of angle kind. */
enum AngleKind {
  /** Angle less than 180 degrees. */
  ANGLE_SMALLER = -1,
  /** 180 degree angle. */
  ANGLE_STRAIGHT = 0,
  /** Angle greater than 180 degrees. */
  ANGLE_LARGER = 1,
};

/** Container for loops representing UV verts which should be merged together in a UV map. */
using UVVertBucket = Set<BMLoop *>;

/** Mapping of vertex to UV vert buckets (i.e. loops belonging to that `BMVert` key). */
using UVVertMap = Map<BMVert *, Vector<UVVertBucket>>;

/** Bevel parameters and state. */
struct BevelParams {
  /** Records BevVerts made: key BMVert*, value BevVert* */
  GHash *vert_hash;
  /** Records new faces: key BMFace*, value one of {VERT/EDGE/RECON}_POLY. */
  GHash *face_hash;
  /** Records `UVFace` made: key `BMFace*`, value `UVFace*`. */
  GHash *uv_face_hash;
  /** Container which keeps track of UV vert connectivity in different UV maps. */
  Vector<UVVertMap> uv_vert_maps;
  /**
   * Use for all allocations while bevel runs.
   * \note If we need to free we can switch to `BLI_mempool`.
   */
  MemArena *mem_arena;
  /** Profile vertex location and spacings. */
  ProfileSpacing pro_spacing;
  /** Parameter values for evenly spaced profile points for the miter profiles. */
  ProfileSpacing pro_spacing_miter;
  /** Information about 'math' loop layers, like UV layers. */
  MathLayerInfo math_layer_info;
  /** The argument BMesh. */
  BMesh *bm;
  /** Blender units to offset each side of a beveled edge. */
  float offset;
  /** How offset is measured; enum defined in bmesh_operators.hh. */
  int offset_type;
  /** Profile type: radius, superellipse, or custom */
  int profile_type;
  /** Bevel vertices only or edges. */
  int affect_type;
  /** Number of segments in beveled edge profile. */
  int seg;
  /** User profile setting. */
  float profile;
  /** Superellipse parameter for edge profile. */
  float pro_super_r;
  /** Bevel amount affected by weights on edges or verts. */
  bool use_weights;
  int bweight_offset_vert;
  int bweight_offset_edge;
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
  char _pad[1];
  /** The struct used to store the custom profile input. */
  const CurveProfile *custom_profile;
  /** Vertex group array, maybe set if vertex only. */
  const MDeformVert *dvert;
  /** Vertex group index, maybe set if vertex only. */
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
};

// #pragma GCC diagnostic ignored "-Wpadded"

/* Only for debugging, this file shouldn't be in blender repository. */
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
  float ang = angle_v3v3(d1, d2);

  return (fabsf(ang) < BEVEL_EPSILON_ANG) || (fabsf(ang - float(M_PI)) < BEVEL_EPSILON_ANG);
}

/**
 * \return True if d1 and d2 are parallel or nearly parallel.
 */
static bool nearly_parallel_normalized(const float d1[3], const float d2[3])
{
  BLI_ASSERT_UNIT_V3(d1);
  BLI_ASSERT_UNIT_V3(d2);

  const float direction_dot = dot_v3v3(d1, d2);
  return compare_ff(fabsf(direction_dot), 1.0f, BEVEL_EPSILON_ANG_DOT);
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
  ans->adjchain = nullptr;
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
  NewVert *nvto = mesh_vert(vm, ito, jto, kto);
  NewVert *nvfrom = mesh_vert(vm, ifrom, jfrom, kfrom);
  nvto->v = nvfrom->v;
  copy_v3_v3(nvto->co, nvfrom->co);
}

/* Find the EdgeHalf in bv's array that has edge bme. */
static EdgeHalf *find_edge_half(BevVert *bv, BMEdge *bme)
{
  for (int i = 0; i < bv->edgecount; i++) {
    if (bv->edges[i].e == bme) {
      return &bv->edges[i];
    }
  }
  return nullptr;
}

/* Find the BevVert corresponding to BMVert bmv. */
static BevVert *find_bevvert(BevelParams *bp, BMVert *bmv)
{
  return static_cast<BevVert *>(BLI_ghash_lookup(bp->vert_hash, bmv));
}

/* Find the `UVFace` corresponding to `bmf` face. */
static UVFace *find_uv_face(BevelParams *bp, BMFace *bmf)
{
  return static_cast<UVFace *>(BLI_ghash_lookup(bp->uv_face_hash, bmf));
}

/**
 * Find the EdgeHalf representing the other end of e->e.
 * \return other end's BevVert in *r_bvother, if r_bvother is provided. That may not have
 * been constructed yet, in which case return nullptr.
 */
static EdgeHalf *find_other_end_edge_half(BevelParams *bp, EdgeHalf *e, BevVert **r_bvother)
{
  BevVert *bvo = find_bevvert(bp, e->is_rev ? e->e->v1 : e->e->v2);
  if (bvo) {
    if (r_bvother) {
      *r_bvother = bvo;
    }
    EdgeHalf *eother = find_edge_half(bvo, e->e);
    BLI_assert(eother != nullptr);
    return eother;
  }
  if (r_bvother) {
    *r_bvother = nullptr;
  }
  return nullptr;
}

/* Return the next EdgeHalf after from_e that is beveled.
 * If from_e is nullptr, find the first beveled edge. */
static EdgeHalf *next_bev(BevVert *bv, EdgeHalf *from_e)
{
  if (from_e == nullptr) {
    from_e = &bv->edges[bv->edgecount - 1];
  }
  EdgeHalf *e = from_e;
  do {
    if (e->is_bev) {
      return e;
    }
  } while ((e = e->next) != from_e);
  return nullptr;
}

/* Return the count of edges between e1 and e2 when going around bv CCW. */
static int count_ccw_edges_between(EdgeHalf *e1, EdgeHalf *e2)
{
  int count = 0;
  EdgeHalf *e = e1;

  do {
    if (e == e2) {
      break;
    }
    e = e->next;
    count++;
  } while (e != e1);
  return count;
}

/* Assume bme1 and bme2 both share some vert. Do they share a face?
 * If they share a face then there is some loop around bme1 that is in a face
 * where the next or previous edge in the face must be bme2. */
static bool edges_face_connected_at_vert(BMEdge *bme1, BMEdge *bme2)
{
  BMIter iter;
  BMLoop *l;
  BM_ITER_ELEM (l, &iter, bme1, BM_LOOPS_OF_EDGE) {
    if (l->prev->e == bme2 || l->next->e == bme2) {
      return true;
    }
  }
  return false;
}

/**
 * Create and register new `UVFace` object based on a new face (`BMFace *fnew`); and assign proper
 * representative face from either `frep` or `frep_arr` arguments.
 */
static UVFace *register_uv_face(BevelParams *bp, BMFace *fnew, BMFace *frep, BMFace **frep_arr)
{
  if (!fnew) {
    return nullptr;
  }

  UVFace *uv_face = (UVFace *)BLI_memarena_alloc(bp->mem_arena, sizeof(UVFace));
  uv_face->f = fnew;
  uv_face->attached_frep = nullptr;
  if (frep_arr && frep_arr[0]) {
    /* Choosing first face from `frep_arr` is an arbitrary choice but for our algorithm it doesn't
     * matter. Usually the difference in `frep` and `frep_arr` is that the latter is used when
     * loops' custom data for a new bevel face is interpolated between multiple original mesh faces
     * on top of which this new bevel face is being constructed; in such case original faces
     * _should_ be already connected in UV space (i.e. no seam) and we handle such scenarios when
     * setting proper value for `is_orig_uv_verts_connected` variable. */
    uv_face->attached_frep = frep_arr[0];
  }
  else if (frep) {
    uv_face->attached_frep = frep;
  }

  BLI_ghash_insert(bp->uv_face_hash, fnew, uv_face);
  return uv_face;
}

/**
 * Update UV vert map with new loops (`BMLoop`) from a face (`uv_face->f`) to keep track of proper
 * UV connectivity. This data will help with merging UV verts later. Loops stored in the same UV
 * vert bucket will be merged together (their UV positions).
 */
static void update_uv_vert_map(BevelParams *bp,
                               UVFace *uv_face,
                               BMVert *bv,
                               Map<BMVert *, BMVert *> *nv_bv_map)
{
  if (!uv_face || !uv_face->attached_frep) {
    return;
  }

  for (UVVertMap &uv_vert_map : bp->uv_vert_maps) {
    BMIter iter;
    BMLoop *l;
    BM_ITER_ELEM (l, &iter, uv_face->f, BM_LOOPS_OF_FACE) {
      Vector<UVVertBucket> *uv_vert_buckets = uv_vert_map.lookup_ptr(l->v);
      if (!uv_vert_buckets) {
        /* New vertex (and a corresponding loop) needs to be registered. No need for further UV
         * connectivity search, we just create a new bucket and move on. */
        uv_vert_map.add(l->v, Vector<UVVertBucket>{{l}});
        continue;
      }

      /* `orig_v` should always point to a vertex which takes part in a bevel operation and comes
       * from the original mesh. This vertex is equivalent to what is stored in the `BevVert::v`
       * field. */
      BMVert *orig_v = nv_bv_map ? nv_bv_map->lookup(l->v) : bv;
      BMLoop *orig_l = BM_face_vert_share_loop(uv_face->attached_frep, orig_v);
      BLI_assert(orig_l != nullptr);

      bool is_bucket_found = false;
      BMIter iter2;
      BMLoop *l2;
      BM_ITER_ELEM (l2, &iter2, l->v, BM_LOOPS_OF_VERT) {
        if (l == l2) {
          continue;
        }

        UVFace *uv_face2 = find_uv_face(bp, l2->f);
        if (!uv_face2 || !uv_face2->attached_frep) {
          continue;
        }

        BMLoop *orig_l2 = BM_face_vert_share_loop(uv_face2->attached_frep, orig_v);
        BLI_assert(orig_l2 != nullptr);

        bool is_orig_uv_verts_connected = false;
        Vector<UVVertBucket> &orig_uv_vert_buckets = uv_vert_map.lookup(orig_v);
        for (UVVertBucket &orig_uv_vert_bucket : orig_uv_vert_buckets) {
          if (orig_uv_vert_bucket.contains(orig_l) && orig_uv_vert_bucket.contains(orig_l2)) {
            is_orig_uv_verts_connected = true;
            break;
          }
        }

        /* Add loop `l` to the existing bucket containing neighboring loop `l2` if either of those
         * conditions are met:
         * 1. Neighboring faces (represented by `UVFace` objects) have the same representative face
         *    attached to them.
         * 2. If representative faces attached to faces containing both loops (`l` and `l2`) are
         *    different but otherwise connected in UV space (`orig_l` and `orig_l2` are
         *    overlapping) for original input mesh. */
        if (uv_face->attached_frep == uv_face2->attached_frep || is_orig_uv_verts_connected) {
          for (UVVertBucket &uv_vert_bucket : *uv_vert_buckets) {
            if (uv_vert_bucket.contains(l2)) {
              uv_vert_bucket.add(l);
              is_bucket_found = true;
              break;
            }
          }
        }
        if (is_bucket_found) {
          break;
        }
      }
      if (!is_bucket_found) {
        uv_vert_buckets->append(UVVertBucket{l});
      }
    }
  }
}

/**
 * Determine UV vert connectivity based on provided `BMVert *v`. If UV loop data is available,
 * iterate through loops of vertex `v`, fetching UV position for each loop and checking against
 * already evaluated ones. If UV coords are overlapping (delta smaller than `STD_UV_CONNECT_LIMIT`)
 * then add those loops to the same UV vert bucket. If UV verts are not overlapping they will end
 * up in separate buckets. Those buckets are later utilized during UV merging process, i.e. UV
 * verts which will end up in the same bucket will be merged together.
 */
static void determine_uv_vert_connectivity(BevelParams *bp, BMesh *bm, BMVert *v)
{
  int num_uv_layers = CustomData_number_of_layers(&bm->ldata, CD_PROP_FLOAT2);
  BLI_assert(bp->uv_vert_maps.size() == num_uv_layers);

  for (int i = 0; i < num_uv_layers; ++i) {
    int uv_data_offset = CustomData_get_n_offset(&bm->ldata, CD_PROP_FLOAT2, i);
    Vector<UVVertBucket> uv_vert_buckets;
    BMIter iter;
    BMLoop *l;
    BM_ITER_ELEM (l, &iter, v, BM_LOOPS_OF_VERT) {
      float *luv = BM_ELEM_CD_GET_FLOAT_P(l, uv_data_offset);
      bool is_overlap_found = false;
      for (UVVertBucket &uv_vert_bucket : uv_vert_buckets) {
        for (BMLoop *l2 : uv_vert_bucket) {
          float *luv2 = BM_ELEM_CD_GET_FLOAT_P(l2, uv_data_offset);
          if (compare_v2v2(luv, luv2, STD_UV_CONNECT_LIMIT)) {
            uv_vert_bucket.add(l);
            is_overlap_found = true;
            break;
          }
        }
        if (is_overlap_found) {
          break;
        }
      }
      if (!is_overlap_found) {
        uv_vert_buckets.append(UVVertBucket{l});
      }
    }

    /* For now `determine_uv_vert_connectivity` is expected to be run at the beginning of the bevel
     * operation, determining connectivity for each vertex `v` (from the original mesh). We expect
     * `bp->uv_vert_maps[i]` to not contain vertex `v` at this point in time. */
    BLI_assert(bp->uv_vert_maps[i].contains(v) == false);
    bp->uv_vert_maps[i].add_new(v, uv_vert_buckets);
  }
}

/**
 * Merge UVs based on data gathered in `bm->uv_vert_maps`. If UV verts are in the same bucket,
 * merge them together. Note that this function exists purely because of imperfections in initial
 * UV position calculations using interpolation approach (`BM_loop_interp_from_face` function).
 */
static void bevel_merge_uvs(BevelParams *bp, BMesh *bm)
{
  int num_uv_layers = CustomData_number_of_layers(&bm->ldata, CD_PROP_FLOAT2);
  BLI_assert(bp->uv_vert_maps.size() == num_uv_layers);

  for (int i = 0; i < num_uv_layers; ++i) {
    int uv_data_offset = CustomData_get_n_offset(&bm->ldata, CD_PROP_FLOAT2, i);
    for (Vector<UVVertBucket> &uv_vert_buckets : bp->uv_vert_maps[i].values()) {
      for (UVVertBucket &uv_vert_bucket : uv_vert_buckets) {
        int num_uv_verts = uv_vert_bucket.size();
        if (num_uv_verts <= 1) {
          continue;
        }
        float uv[2] = {0.0f, 0.0f};
        for (BMLoop *l : uv_vert_bucket) {
          float *luv = BM_ELEM_CD_GET_FLOAT_P(l, uv_data_offset);
          add_v2_v2(uv, luv);
        }
        mul_v2_fl(uv, 1.0f / float(num_uv_verts));
        for (BMLoop *l : uv_vert_bucket) {
          float *luv = BM_ELEM_CD_GET_FLOAT_P(l, uv_data_offset);
          copy_v2_v2(luv, uv);
        }
      }
    }
  }
}

/**
 * Return a good representative face (for materials, etc.) for faces
 * created around/near BoundVert v.
 * Sometimes care about a second choice, if there is one.
 * If r_fother parameter is non-nullptr and there is another, different,
 * possible frep, return the other one in that parameter.
 */
static BMFace *boundvert_rep_face(BoundVert *v, BMFace **r_fother)
{
  BMFace *frep;

  BMFace *frep2 = nullptr;
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
    frep = nullptr;
  }
  if (r_fother) {
    *r_fother = frep2;
  }
  return frep;
}

/**
 * Make ngon from verts alone.
 * Make sure to properly copy face attributes and do custom data interpolation from
 * corresponding elements of face_arr, if that is non-nullptr, else from facerep.
 * If edge_arr is non-nullptr, then for interpolation purposes only, the corresponding
 * elements of vert_arr are snapped to any non-nullptr edges in that array.
 * If mat_nr >= 0 then the material of the face is set to that.
 *
 * \note ALL face creation goes through this function, this is important to keep!
 */
static BMFace *bev_create_ngon(BevelParams *bp,
                               BMesh *bm,
                               BMVert **vert_arr,
                               const int totv,
                               BMFace **face_arr,
                               BMFace *facerep,
                               BMEdge **snap_edge_arr,
                               BMVert *bv,
                               Map<BMVert *, BMVert *> *nv_bv_map,
                               int mat_nr,
                               bool do_interp)
{
  BMFace *f = BM_face_create_verts(bm, vert_arr, totv, facerep, BM_CREATE_NOP, true);
  if (!f) {
    return nullptr;
  }

  if (facerep || (face_arr && face_arr[0])) {
    BM_elem_attrs_copy(bm, facerep ? facerep : face_arr[0], f);
    if (do_interp) {
      int i = 0;
      BMIter iter;
      BMLoop *l;
      BM_ITER_ELEM (l, &iter, f, BM_LOOPS_OF_FACE) {
        BMFace *interp_f;
        if (face_arr) {
          /* Assume loops of created face are in same order as verts. */
          BLI_assert(l->v == vert_arr[i]);
          interp_f = face_arr[i];
        }
        else {
          interp_f = facerep;
        }
        if (interp_f) {
          BMEdge *bme = nullptr;
          if (snap_edge_arr) {
            bme = snap_edge_arr[i];
          }
          float save_co[3];
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

  BM_elem_flag_enable(f, BM_ELEM_TAG);
  BMIter iter;
  BMEdge *bme;
  BM_ITER_ELEM (bme, &iter, f, BM_EDGES_OF_FACE) {
    /* Not essential for bevels own internal logic, this is done so the operator can select newly
     * created geometry. */
    flag_out_edge(bm, bme);
  }

  if (mat_nr >= 0) {
    f->mat_nr = short(mat_nr);
  }

  UVFace *uv_face = register_uv_face(bp, f, facerep, face_arr);
  update_uv_vert_map(bp, uv_face, bv, nv_bv_map);

  return f;
}

/* Is Loop layer layer_index contiguous across shared vertex of l1 and l2? */
static bool contig_ldata_across_loops(BMesh *bm, BMLoop *l1, BMLoop *l2, int layer_index)
{
  const int offset = bm->ldata.layers[layer_index].offset;
  const int type = bm->ldata.layers[layer_index].type;

  return CustomData_data_equals(
      eCustomDataType(type), (char *)l1->head.data + offset, (char *)l2->head.data + offset);
}

/* Are all loop layers with have math (e.g., UVs)
 * contiguous from face f1 to face f2 across edge e?
 */
static bool contig_ldata_across_edge(BMesh *bm, BMEdge *e, BMFace *f1, BMFace *f2)
{
  if (bm->ldata.totlayer == 0) {
    return true;
  }

  BMLoop *lef1, *lef2;
  if (!BM_edge_loop_pair(e, &lef1, &lef2)) {
    return false;
  }
  /* If faces are oriented consistently around e,
   * should now have lef1 and lef2 being f1 and f2 in either order.
   */
  if (lef1->f == f2) {
    std::swap(lef1, lef2);
  }
  if (lef1->f != f1 || lef2->f != f2) {
    return false;
  }
  BMVert *v1 = lef1->v;
  BMVert *v2 = lef2->v;
  if (v1 == v2) {
    return false;
  }
  BLI_assert((v1 == e->v1 && v2 == e->v2) || (v1 == e->v2 && v2 == e->v1));
  UNUSED_VARS_NDEBUG(v1, v2);
  BMLoop *lv1f1 = lef1;
  BMLoop *lv2f1 = lef1->next;
  BMLoop *lv1f2 = lef2->next;
  BMLoop *lv2f2 = lef2;
  BLI_assert(lv1f1->v == v1 && lv1f1->f == f1 && lv2f1->v == v2 && lv2f1->f == f1 &&
             lv1f2->v == v1 && lv1f2->f == f2 && lv2f2->v == v2 && lv2f2->f == f2);
  for (int i = 0; i < bm->ldata.totlayer; i++) {
    if (CustomData_layer_has_math(&bm->ldata, i)) {
      if (!contig_ldata_across_loops(bm, lv1f1, lv1f2, i) ||
          !contig_ldata_across_loops(bm, lv2f1, lv2f2, i))
      {
        return false;
      }
    }
  }
  return true;
}

/**
 * In array face_component of total `totface` elements, swap values c1 and c2
 * wherever they occur.
 */
static void swap_face_components(int *face_component, int totface, int c1, int c2)
{
  if (c1 == c2) {
    return; /* Nothing to do. */
  }
  for (int f = 0; f < totface; f++) {
    if (face_component[f] == c1) {
      face_component[f] = c2;
    }
    else if (face_component[f] == c2) {
      face_component[f] = c1;
    }
  }
}

/**
 * Initialize `bp->uv_vert_maps` to the size equal to the number of UV layers.
 */
static void uv_vert_map_init(BevelParams *bp, BMesh *bm)
{
  int num_uv_layers = CustomData_number_of_layers(&bm->ldata, CD_PROP_FLOAT2);
  bp->uv_vert_maps.clear();
  bp->uv_vert_maps.resize(num_uv_layers);
}

/**
 * Remove vertex `v` from all UV maps in `bp->uv_vert_maps`.
 */
static void uv_vert_map_pop(BevelParams *bp, BMVert *v)
{
  for (UVVertMap &uv_vert_map : bp->uv_vert_maps) {
    uv_vert_map.pop_try(v);
  }
}

/*
 * Set up the fields of bp->math_layer_info.
 * We always set has_math_layers to the correct value.
 * Only if there are UV layers and the number of segments is odd,
 * we need to calculate connected face components in UV space.
 */
static void math_layer_info_init(BevelParams *bp, BMesh *bm)
{
  int f;
  bp->math_layer_info.has_math_layers = false;
  bp->math_layer_info.face_component = nullptr;
  for (int i = 0; i < bm->ldata.totlayer; i++) {
    if (CustomData_has_layer(&bm->ldata, CD_PROP_FLOAT2)) {
      bp->math_layer_info.has_math_layers = true;
      break;
    }
  }
  if (!bp->math_layer_info.has_math_layers || (bp->seg % 2) == 0) {
    return;
  }

  BM_mesh_elem_index_ensure(bm, BM_FACE);
  BM_mesh_elem_table_ensure(bm, BM_FACE);
  int totface = bm->totface;
  int *face_component = static_cast<int *>(
      BLI_memarena_alloc(bp->mem_arena, sizeof(int) * totface));
  bp->math_layer_info.face_component = face_component;

  /* Use an array as a stack. Stack size can't exceed total faces if keep track of what is in
   * stack. */
  BMFace **stack = MEM_malloc_arrayN<BMFace *>(totface, __func__);
  bool *in_stack = MEM_malloc_arrayN<bool>(totface, __func__);

  /* Set all component ids by DFS from faces with unassigned components. */
  for (f = 0; f < totface; f++) {
    face_component[f] = -1;
    in_stack[f] = false;
  }
  int current_component = -1;
  for (f = 0; f < totface; f++) {
    if (face_component[f] == -1 && !in_stack[f]) {
      int stack_top = 0;
      current_component++;
      BLI_assert(stack_top < totface);
      stack[stack_top] = BM_face_at_index(bm, f);
      in_stack[f] = true;
      while (stack_top >= 0) {
        BMFace *bmf = stack[stack_top];
        stack_top--;
        int bmf_index = BM_elem_index_get(bmf);
        in_stack[bmf_index] = false;
        if (face_component[bmf_index] != -1) {
          continue;
        }
        face_component[bmf_index] = current_component;
        /* Neighbors are faces that share an edge with bmf and
         * are where contig_ldata_across_edge(...) is true for the
         * shared edge and two faces.
         */
        BMIter eiter;
        BMEdge *bme;
        BM_ITER_ELEM (bme, &eiter, bmf, BM_EDGES_OF_FACE) {
          BMIter fiter;
          BMFace *bmf_other;
          BM_ITER_ELEM (bmf_other, &fiter, bme, BM_FACES_OF_EDGE) {
            if (bmf_other != bmf) {
              int bmf_other_index = BM_elem_index_get(bmf_other);
              if (face_component[bmf_other_index] != -1 || in_stack[bmf_other_index]) {
                continue;
              }
              if (contig_ldata_across_edge(bm, bme, bmf, bmf_other)) {
                stack_top++;
                BLI_assert(stack_top < totface);
                stack[stack_top] = bmf_other;
                in_stack[bmf_other_index] = true;
              }
            }
          }
        }
      }
    }
  }
  MEM_freeN(stack);
  MEM_freeN(in_stack);
  /* We can usually get more pleasing result if components 0 and 1
   * are the topmost and bottom-most (in z-coordinate) components,
   * so adjust component indices to make that so. */
  if (current_component <= 0) {
    return; /* Only one component, so no need to do this. */
  }
  BMFace *top_face = nullptr;
  float top_face_z = -1e30f;
  int top_face_component = -1;
  BMFace *bot_face = nullptr;
  float bot_face_z = 1e30f;
  int bot_face_component = -1;
  for (f = 0; f < totface; f++) {
    float cent[3];
    BMFace *bmf = BM_face_at_index(bm, f);
    BM_face_calc_center_bounds(bmf, cent);
    float fz = cent[2];
    if (fz > top_face_z) {
      top_face_z = fz;
      top_face = bmf;
      top_face_component = face_component[f];
    }
    if (fz < bot_face_z) {
      bot_face_z = fz;
      bot_face = bmf;
      bot_face_component = face_component[f];
    }
  }
  BLI_assert(top_face != nullptr && bot_face != nullptr);
  UNUSED_VARS_NDEBUG(top_face, bot_face);
  swap_face_components(face_component, totface, face_component[0], top_face_component);
  if (bot_face_component != top_face_component) {
    if (bot_face_component == 0) {
      /* It was swapped with old top_face_component. */
      bot_face_component = top_face_component;
    }
    swap_face_components(face_component, totface, face_component[1], bot_face_component);
  }
}

/**
 * Use a tie-breaking rule to choose a representative face when
 * there are number of choices, `face[0]`, `face[1]`, ..., `face[nfaces]`.
 * This is needed when there are an odd number of segments, and the center
 * segment (and its continuation into vmesh) can usually arbitrarily be
 * the previous face or the next face.
 * Or, for the center polygon of a corner, all of the faces around
 * the vertex are possibleface_component choices.
 * If we just choose randomly, the resulting UV maps or material
 * assignment can look ugly/inconsistent.
 * Allow for the case when arguments are null.
 */
static BMFace *choose_rep_face(BevelParams *bp, BMFace **face, int nfaces)
{
#define VEC_VALUE_LEN 6
  float (*value_vecs)[VEC_VALUE_LEN] = nullptr;
  int num_viable = 0;

  value_vecs = BLI_array_alloca(value_vecs, nfaces);
  bool *still_viable = BLI_array_alloca(still_viable, nfaces);
  for (int f = 0; f < nfaces; f++) {
    BMFace *bmf = face[f];
    if (bmf == nullptr) {
      still_viable[f] = false;
      continue;
    }
    still_viable[f] = true;
    num_viable++;
    int bmf_index = BM_elem_index_get(bmf);
    int value_index = 0;
    /* First tie-breaker: lower math-layer connected component id. */
    value_vecs[f][value_index++] = bp->math_layer_info.face_component ?
                                       float(bp->math_layer_info.face_component[bmf_index]) :
                                       0.0f;
    /* Next tie-breaker: selected face beats unselected one. */
    value_vecs[f][value_index++] = BM_elem_flag_test(bmf, BM_ELEM_SELECT) ? 0.0f : 1.0f;
    /* Next tie-breaker: lower material index. */
    value_vecs[f][value_index++] = bmf->mat_nr >= 0 ? float(bmf->mat_nr) : 0.0f;
    /* Next three tie-breakers: z, x, y components of face center. */
    float cent[3];
    BM_face_calc_center_bounds(bmf, cent);
    value_vecs[f][value_index++] = cent[2];
    value_vecs[f][value_index++] = cent[0];
    value_vecs[f][value_index++] = cent[1];
    BLI_assert(value_index == VEC_VALUE_LEN);
  }

  /* Look for a face that has a unique minimum value for in a value_index,
   * trying each value_index in turn until find a unique minimum.
   */
  int best_f = -1;
  for (int value_index = 0; num_viable > 1 && value_index < VEC_VALUE_LEN; value_index++) {
    for (int f = 0; f < nfaces; f++) {
      if (!still_viable[f] || f == best_f) {
        continue;
      }
      if (best_f == -1) {
        best_f = f;
        continue;
      }
      if (value_vecs[f][value_index] < value_vecs[best_f][value_index]) {
        best_f = f;
        /* Previous f's are now not viable any more. */
        for (int i = f - 1; i >= 0; i--) {
          if (still_viable[i]) {
            still_viable[i] = false;
            num_viable--;
          }
        }
      }
      else if (value_vecs[f][value_index] > value_vecs[best_f][value_index]) {
        still_viable[f] = false;
        num_viable--;
      }
    }
  }
  if (best_f == -1) {
    best_f = 0;
  }
  return face[best_f];
#undef VEC_VALUE_LEN
}

/* Calculate coordinates of a point a distance d from v on e->e and return it in slideco. */
static void slide_dist(EdgeHalf *e, BMVert *v, float d, float r_slideco[3])
{
  float dir[3];
  sub_v3_v3v3(dir, v->co, BM_edge_other_vert(e->e, v)->co);
  float len = normalize_v3(dir);

  if (d > len) {
    d = len - float(50.0 * BEVEL_EPSILON_D);
  }
  copy_v3_v3(r_slideco, v->co);
  madd_v3_v3fl(r_slideco, dir, -d);
}

/* Is co not on the edge e? If not, return the closer end of e in ret_closer_v. */
static bool is_outside_edge(EdgeHalf *e, const float co[3], BMVert **ret_closer_v)
{
  float h[3], u[3];
  float *l1 = e->e->v1->co;

  sub_v3_v3v3(u, e->e->v2->co, l1);
  sub_v3_v3v3(h, co, l1);
  float lenu = normalize_v3(u);
  float lambda = dot_v3v3(u, h);
  if (lambda <= -BEVEL_EPSILON_BIG * lenu) {
    *ret_closer_v = e->e->v1;
    return true;
  }
  if (lambda >= (1.0f + BEVEL_EPSILON_BIG) * lenu) {
    *ret_closer_v = e->e->v2;
    return true;
  }
  return false;
}

/* Return whether the angle is less than, equal to, or larger than 180 degrees. */
static AngleKind edges_angle_kind(EdgeHalf *e1, EdgeHalf *e2, BMVert *v)
{
  BMVert *v1 = BM_edge_other_vert(e1->e, v);
  BMVert *v2 = BM_edge_other_vert(e2->e, v);
  float dir1[3], dir2[3];
  sub_v3_v3v3(dir1, v->co, v1->co);
  sub_v3_v3v3(dir2, v->co, v2->co);
  normalize_v3(dir1);
  normalize_v3(dir2);

  /* First check for in-line edges using a simpler test. */
  if (nearly_parallel_normalized(dir1, dir2)) {
    return ANGLE_STRAIGHT;
  }

  /* Angles are in [0,pi]. Need to compare cross product with normal to see if they are reflex. */
  float cross[3];
  cross_v3_v3v3(cross, dir1, dir2);
  normalize_v3(cross);
  float *no;
  if (e1->fnext) {
    no = e1->fnext->no;
  }
  else if (e2->fprev) {
    no = e2->fprev->no;
  }
  else {
    no = v->no;
  }

  if (dot_v3v3(cross, no) < 0.0f) {
    return ANGLE_LARGER;
  }
  return ANGLE_SMALLER;
}

/* co should be approximately on the plane between e1 and e2, which share common vert v and common
 * face f (which cannot be nullptr). Is it between those edges, sweeping CCW? */
static bool point_between_edges(
    const float co[3], BMVert *v, BMFace *f, EdgeHalf *e1, EdgeHalf *e2)
{
  float dir1[3], dir2[3], dirco[3], no[3];

  BMVert *v1 = BM_edge_other_vert(e1->e, v);
  BMVert *v2 = BM_edge_other_vert(e2->e, v);
  sub_v3_v3v3(dir1, v->co, v1->co);
  sub_v3_v3v3(dir2, v->co, v2->co);
  sub_v3_v3v3(dirco, v->co, co);
  normalize_v3(dir1);
  normalize_v3(dir2);
  normalize_v3(dirco);
  float ang11 = angle_normalized_v3v3(dir1, dir2);
  float ang1co = angle_normalized_v3v3(dir1, dirco);
  /* Angles are in [0,pi]. Need to compare cross product with normal to see if they are reflex. */
  cross_v3_v3v3(no, dir1, dir2);
  if (dot_v3v3(no, f->no) < 0.0f) {
    ang11 = float(M_PI * 2.0) - ang11;
  }
  cross_v3_v3v3(no, dir1, dirco);
  if (dot_v3v3(no, f->no) < 0.0f) {
    ang1co = float(M_PI * 2.0) - ang1co;
  }
  return (ang11 - ang1co > -BEVEL_EPSILON_ANG);
}

/* Is the angle swept from e1 to e2, CCW when viewed from the normal side of f,
 * not a reflex angle or a straight angle? Assume e1 and e2 share a vert. */
static bool edge_edge_angle_less_than_180(const BMEdge *e1, const BMEdge *e2, const BMFace *f)
{
  float dir1[3], dir2[3], cross[3];
  BLI_assert(f != nullptr);
  BMVert *v, *v1, *v2;
  if (e1->v1 == e2->v1) {
    v = e1->v1;
    v1 = e1->v2;
    v2 = e2->v2;
  }
  else if (e1->v1 == e2->v2) {
    v = e1->v1;
    v1 = e1->v2;
    v2 = e2->v1;
  }
  else if (e1->v2 == e2->v1) {
    v = e1->v2;
    v1 = e1->v1;
    v2 = e2->v2;
  }
  else if (e1->v2 == e2->v2) {
    v = e1->v2;
    v1 = e1->v1;
    v2 = e2->v1;
  }
  else {
    BLI_assert(false);
    return false;
  }
  sub_v3_v3v3(dir1, v1->co, v->co);
  sub_v3_v3v3(dir2, v2->co, v->co);
  cross_v3_v3v3(cross, dir1, dir2);
  return dot_v3v3(cross, f->no) > 0.0f;
}

/* When the offset_type is BEVEL_AMT_PERCENT or BEVEL_AMT_ABSOLUTE, fill in the coordinates
 * of the lines whose intersection defines the boundary point between e1 and e2 with common
 * vert v, as defined in the parameters of offset_meet.
 */
static void offset_meet_lines_percent_or_absolute(BevelParams *bp,
                                                  EdgeHalf *e1,
                                                  EdgeHalf *e2,
                                                  BMVert *v,
                                                  float r_l1a[3],
                                                  float r_l1b[3],
                                                  float r_l2a[3],
                                                  float r_l2b[3])
{
  /* Get points the specified distance along each leg.
   * NOTE: not all BevVerts and EdgeHalfs have been made yet, so we have
   * to find required edges by moving around faces and use fake EdgeHalfs for
   * some of the edges. If there aren't faces to move around, we have to give up.
   * The legs we need are:
   *   e0 : the next edge around e1->fnext (==f1) after e1.
   *   e3 : the prev edge around e2->fprev (==f2) before e2.
   *   e4 : the previous edge around f1 before e1 (may be e2).
   *   e5 : the next edge around f2 after e2 (may be e1).
   */
  BMVert *v1, *v2;
  EdgeHalf e0, e3, e4, e5;
  BMFace *f1, *f2;
  float d0, d3, d4, d5;
  float e1_wt, e2_wt;
  v1 = BM_edge_other_vert(e1->e, v);
  v2 = BM_edge_other_vert(e2->e, v);
  f1 = e1->fnext;
  f2 = e2->fprev;
  bool no_offsets = f1 == nullptr || f2 == nullptr;
  if (!no_offsets) {
    BMLoop *l = BM_face_vert_share_loop(f1, v1);
    e0.e = l->e;
    l = BM_face_vert_share_loop(f2, v2);
    e3.e = l->prev->e;
    l = BM_face_vert_share_loop(f1, v);
    e4.e = l->prev->e;
    l = BM_face_vert_share_loop(f2, v);
    e5.e = l->e;
    /* All the legs must be visible from their opposite legs. */
    no_offsets = !edge_edge_angle_less_than_180(e0.e, e1->e, f1) ||
                 !edge_edge_angle_less_than_180(e1->e, e4.e, f1) ||
                 !edge_edge_angle_less_than_180(e2->e, e3.e, f2) ||
                 !edge_edge_angle_less_than_180(e5.e, e2->e, f1);
    if (!no_offsets) {
      if (bp->offset_type == BEVEL_AMT_ABSOLUTE) {
        d0 = d3 = d4 = d5 = bp->offset;
      }
      else {
        d0 = bp->offset * BM_edge_calc_length(e0.e) / 100.0f;
        d3 = bp->offset * BM_edge_calc_length(e3.e) / 100.0f;
        d4 = bp->offset * BM_edge_calc_length(e4.e) / 100.0f;
        d5 = bp->offset * BM_edge_calc_length(e5.e) / 100.0f;
      }
      if (bp->use_weights) {
        e1_wt = bp->bweight_offset_edge == -1 ?
                    0.0f :
                    BM_ELEM_CD_GET_FLOAT(e1->e, bp->bweight_offset_edge);
        e2_wt = bp->bweight_offset_edge == -1 ?
                    0.0f :
                    BM_ELEM_CD_GET_FLOAT(e2->e, bp->bweight_offset_edge);
      }
      else {
        e1_wt = 1.0f;
        e2_wt = 1.0f;
      }
      slide_dist(&e4, v, d4 * e1_wt, r_l1a);
      slide_dist(&e0, v1, d0 * e1_wt, r_l1b);
      slide_dist(&e5, v, d5 * e2_wt, r_l2a);
      slide_dist(&e3, v2, d3 * e2_wt, r_l2b);
    }
  }
  if (no_offsets) {
    copy_v3_v3(r_l1a, v->co);
    copy_v3_v3(r_l1b, v1->co);
    copy_v3_v3(r_l2a, v->co);
    copy_v3_v3(r_l2b, v2->co);
  }
}

/**
 * Calculate the meeting point between the offset edges for e1 and e2, putting answer in meetco.
 * e1 and e2 share vertex v and face f (may be nullptr) and viewed from the normal side of
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
static void offset_meet(BevelParams *bp,
                        EdgeHalf *e1,
                        EdgeHalf *e2,
                        BMVert *v,
                        BMFace *f,
                        bool edges_between,
                        float meetco[3],
                        const EdgeHalf *e_in_plane)
{
  /* Get direction vectors for two offset lines. */
  float dir1[3], dir2[3];
  sub_v3_v3v3(dir1, v->co, BM_edge_other_vert(e1->e, v)->co);
  sub_v3_v3v3(dir2, BM_edge_other_vert(e2->e, v)->co, v->co);

  float dir1n[3], dir2p[3];
  if (edges_between) {
    EdgeHalf *e1next = e1->next;
    EdgeHalf *e2prev = e2->prev;
    sub_v3_v3v3(dir1n, BM_edge_other_vert(e1next->e, v)->co, v->co);
    sub_v3_v3v3(dir2p, v->co, BM_edge_other_vert(e2prev->e, v)->co);
  }
  else {
    /* Shut up 'maybe unused' warnings. */
    zero_v3(dir1n);
    zero_v3(dir2p);
  }

  float ang = angle_v3v3(dir1, dir2);
  float norm_perp1[3];
  if (ang < BEVEL_EPSILON_ANG) {
    /* Special case: e1 and e2 are parallel; put offset point perp to both, from v.
     * need to find a suitable plane.
     * This code used to just use offset and dir1, but that makes for visible errors
     * on a circle with > 200 sides, which trips this "nearly perp" code (see #61214).
     * so use the average of the two, and the offset formula for angle bisector.
     * If offsets are different, we're out of luck:
     * Use the max of the two (so get consistent looking results if the same situation
     * arises elsewhere in the object but with opposite roles for e1 and e2. */
    float norm_v[3];
    if (f) {
      copy_v3_v3(norm_v, f->no);
    }
    else {
      /* Get average of face norms of faces between e and e2. */
      int fcount = 0;
      zero_v3(norm_v);
      for (EdgeHalf *eloop = e1; eloop != e2; eloop = eloop->next) {
        if (eloop->fnext != nullptr) {
          add_v3_v3(norm_v, eloop->fnext->no);
          fcount++;
        }
      }
      if (fcount == 0) {
        copy_v3_v3(norm_v, v->no);
      }
      else {
        mul_v3_fl(norm_v, 1.0f / fcount);
      }
    }
    add_v3_v3(dir1, dir2);
    cross_v3_v3v3(norm_perp1, dir1, norm_v);
    normalize_v3(norm_perp1);
    float off1a[3];
    copy_v3_v3(off1a, v->co);
    float d = max_ff(e1->offset_r, e2->offset_l);
    d = d / cosf(ang / 2.0f);
    madd_v3_v3fl(off1a, norm_perp1, d);
    copy_v3_v3(meetco, off1a);
  }
  else if (fabsf(ang - float(M_PI)) < BEVEL_EPSILON_ANG) {
    /* Special case: e1 and e2 are anti-parallel, so bevel is into a zero-area face.
     * Just make the offset point on the common line, at offset distance from v. */
    float d = max_ff(e1->offset_r, e2->offset_l);
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
    float norm_v1[3], norm_v2[3];
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
    float norm_perp2[3];
    cross_v3_v3v3(norm_perp1, dir1, norm_v1);
    cross_v3_v3v3(norm_perp2, dir2, norm_v2);
    normalize_v3(norm_perp1);
    normalize_v3(norm_perp2);

    float off1a[3], off1b[3], off2a[3], off2b[3];
    if (ELEM(bp->offset_type, BEVEL_AMT_PERCENT, BEVEL_AMT_ABSOLUTE)) {
      offset_meet_lines_percent_or_absolute(bp, e1, e2, v, off1a, off1b, off2a, off2b);
    }
    else {
      /* Get points that are offset distances from each line, then another point on each line. */
      copy_v3_v3(off1a, v->co);
      madd_v3_v3fl(off1a, norm_perp1, e1->offset_r);
      add_v3_v3v3(off1b, off1a, dir1);
      copy_v3_v3(off2a, v->co);
      madd_v3_v3fl(off2a, norm_perp2, e2->offset_l);
      add_v3_v3v3(off2b, off2a, dir2);
    }

    /* Intersect the offset lines. */
    float isect2[3];
    int isect_kind = isect_line_line_v3(off1a, off1b, off2a, off2b, meetco, isect2);
    if (isect_kind == 0) {
      /* Lines are collinear: we already tested for this, but this used a different epsilon. */
      copy_v3_v3(meetco, off1a); /* Just to do something. */
    }
    else {
      /* The lines intersect, but is it at a reasonable place?
       * One problem to check: if one of the offsets is 0, then we don't want an intersection
       * that is outside that edge itself. This can happen if angle between them is > 180 degrees,
       * or if the offset amount is > the edge length. */
      BMVert *closer_v;
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
        for (EdgeHalf *e = e1; e != e2; e = e->next) {
          BMFace *fnext = e->fnext;
          if (!fnext) {
            continue;
          }
          float plane[4];
          plane_from_point_normal_v3(plane, v->co, fnext->no);
          float dropco[3];
          closest_to_plane_normalized_v3(dropco, plane, meetco);
          /* Don't drop to the faces next to the in plane edge. */
          if (e_in_plane) {
            ang = angle_v3v3(fnext->no, e_in_plane->fnext->no);
            if ((fabsf(ang) < BEVEL_SMALL_ANG) || (fabsf(ang - float(M_PI)) < BEVEL_SMALL_ANG)) {
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

/* This was changed from 0.25f to fix bug #86768.
 * Original bug #44961 remains fixed with this value.
 * Update: changed again from 0.0001f to fix bug #95335.
 * Original two bugs remained fixed.
 */
#define BEVEL_GOOD_ANGLE 0.1f

/**
 * Calculate the meeting point between e1 and e2 (one of which should have zero offsets),
 * where \a e1 precedes \a e2 in CCW order around their common vertex \a v
 * (viewed from normal side).
 * If \a r_angle is provided, return the angle between \a e and \a meetco in `*r_angle`.
 * If the angle is 0, or it is 180 degrees or larger, there will be no meeting point;
 * return false in that case, else true.
 */
static bool offset_meet_edge(
    EdgeHalf *e1, EdgeHalf *e2, BMVert *v, float meetco[3], float *r_angle)
{
  float dir1[3], dir2[3];
  sub_v3_v3v3(dir1, BM_edge_other_vert(e1->e, v)->co, v->co);
  sub_v3_v3v3(dir2, BM_edge_other_vert(e2->e, v)->co, v->co);
  normalize_v3(dir1);
  normalize_v3(dir2);

  /* Find angle from dir1 to dir2 as viewed from vertex normal side. */
  float ang = angle_normalized_v3v3(dir1, dir2);
  if (fabsf(ang) < BEVEL_GOOD_ANGLE) {
    if (r_angle) {
      *r_angle = 0.0f;
    }
    return false;
  }
  float fno[3];
  cross_v3_v3v3(fno, dir1, dir2);
  if (dot_v3v3(fno, v->no) < 0.0f) {
    ang = 2.0f * float(M_PI) - ang; /* Angle is reflex. */
    if (r_angle) {
      *r_angle = ang;
    }
    return false;
  }
  if (r_angle) {
    *r_angle = ang;
  }

  if (fabsf(ang - float(M_PI)) < BEVEL_GOOD_ANGLE) {
    return false;
  }

  float sinang = sinf(ang);

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
 * However, if the bp->offset_type is BEVEL_AMT_PERCENT or BEVEL_AMT_ABSOLUTE, we just slide
 * along emid by the specified amount.
 */
static bool offset_on_edge_between(BevelParams *bp,
                                   EdgeHalf *e1,
                                   EdgeHalf *e2,
                                   EdgeHalf *emid,
                                   BMVert *v,
                                   float meetco[3],
                                   float *r_sinratio)
{
  bool retval = false;

  BLI_assert(e1->is_bev && e2->is_bev && !emid->is_bev);

  float ang1, ang2;
  float meet1[3], meet2[3];
  bool ok1 = offset_meet_edge(e1, emid, v, meet1, &ang1);
  bool ok2 = offset_meet_edge(emid, e2, v, meet2, &ang2);
  if (ELEM(bp->offset_type, BEVEL_AMT_PERCENT, BEVEL_AMT_ABSOLUTE)) {
    BMVert *v2 = BM_edge_other_vert(emid->e, v);
    if (bp->offset_type == BEVEL_AMT_PERCENT) {
      float wt = 1.0;
      if (bp->use_weights) {
        wt = bp->bweight_offset_edge == -1 ?
                 0.0f :
                 0.5f * (BM_ELEM_CD_GET_FLOAT(e1->e, bp->bweight_offset_edge) +
                         BM_ELEM_CD_GET_FLOAT(e2->e, bp->bweight_offset_edge));
      }
      interp_v3_v3v3(meetco, v->co, v2->co, wt * bp->offset / 100.0f);
    }
    else {
      float dir[3];
      sub_v3_v3v3(dir, v2->co, v->co);
      normalize_v3(dir);
      madd_v3_v3v3fl(meetco, v->co, dir, bp->offset);
    }
    if (r_sinratio) {
      *r_sinratio = (ang1 == 0.0f) ? 1.0f : sinf(ang2) / sinf(ang1);
    }
    return true;
  }
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
 * If plane_no is nullptr, choose an arbitrary plane different from eh's direction. */
static void offset_in_plane(EdgeHalf *e, const float plane_no[3], bool left, float r_co[3])
{
  BMVert *v = e->is_rev ? e->e->v2 : e->e->v1;

  float dir[3], no[3];
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

  float fdir[3];
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
    BLI_assert_msg(0, "project meet failure");
#endif
    copy_v3_v3(projco, e->v1->co);
  }
}

/* If there is a bndv->ebev edge, find the mid control point if necessary.
 * It is the closest point on the beveled edge to the line segment between bndv and bndv->next. */
static void set_profile_params(BevelParams *bp, BevVert *bv, BoundVert *bndv)
{
  bool do_linear_interp = true;
  EdgeHalf *e = bndv->ebev;
  Profile *pro = &bndv->profile;

  float start[3], end[3];
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
    float d1[3], d2[3];
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
          float co3[3];
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
  else if (bp->affect_type == BEVEL_AFFECT_VERTICES) {
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
  Profile *pro = &bndv->profile;

  /* Only do this if projecting, and start, end, and proj_dir are not coplanar. */
  if (is_zero_v3(pro->proj_dir)) {
    return;
  }

  float d1[3], d2[3];
  sub_v3_v3v3(d1, bmvert->co, pro->start);
  normalize_v3(d1);
  sub_v3_v3v3(d2, bmvert->co, pro->end);
  normalize_v3(d2);
  float no[3], no2[3], no3[3];
  cross_v3_v3v3(no, d1, d2);
  cross_v3_v3v3(no2, d1, pro->proj_dir);
  cross_v3_v3v3(no3, d2, pro->proj_dir);

  if (normalize_v3(no) > BEVEL_EPSILON_BIG && normalize_v3(no2) > BEVEL_EPSILON_BIG &&
      normalize_v3(no3) > BEVEL_EPSILON_BIG)
  {
    float dot2 = dot_v3v3(no, no2);
    float dot3 = dot_v3v3(no, no3);
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
  /* Only do this if projecting, and d1, d2, and proj_dir are not coplanar. */
  if (is_zero_v3(bndv1->profile.proj_dir) || is_zero_v3(bndv2->profile.proj_dir)) {
    return;
  }
  float d1[3], d2[3], no[3];
  sub_v3_v3v3(d1, bv->v->co, bndv1->nv.co);
  sub_v3_v3v3(d2, bv->v->co, bndv2->nv.co);
  cross_v3_v3v3(no, d1, d2);
  float l1 = normalize_v3(no);

  /* "no" is new normal projection plane, but don't move if it is coplanar with both of the
   * projection directions. */
  float no2[3], no3[3];
  cross_v3_v3v3(no2, d1, bndv1->profile.proj_dir);
  float l2 = normalize_v3(no2);
  cross_v3_v3v3(no3, d2, bndv2->profile.proj_dir);
  float l3 = normalize_v3(no3);
  if (l1 != 0.0f && (l2 != 0.0f || l3 != 0.0f)) {
    float dot1 = fabsf(dot_v3v3(no, no2));
    float dot2 = fabsf(dot_v3v3(no, no3));
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
  if (!f) {
    return 0;
  }
  BMLoop *la = BM_face_edge_share_loop(f, a);
  BMLoop *lb = BM_face_edge_share_loop(f, b);
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
  float vb_vmid[3], va_vmid[3];
  sub_v3_v3v3(va_vmid, vmid, va);
  sub_v3_v3v3(vb_vmid, vmid, vb);

  if (is_zero_v3(va_vmid) || is_zero_v3(vb_vmid)) {
    return false;
  }

  if (fabsf(angle_v3v3(va_vmid, vb_vmid) - float(M_PI)) <= BEVEL_EPSILON_ANG) {
    return false;
  }

  float vo[3], vd[3], vddir[3];
  sub_v3_v3v3(vo, va, vb_vmid);
  cross_v3_v3v3(vddir, vb_vmid, va_vmid);
  normalize_v3(vddir);
  add_v3_v3v3(vd, vo, vddir);

  /* The cols of m are: `vmid - va, vmid - vb, vmid + vd - va -vb, va + vb - vmid`;
   * Blender transform matrices are stored such that `m[i][*]` is `i-th` column;
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
 * and verts around it a, b, c (in CCW order, viewed from d normal dir).
 * The matrix mat is calculated to map:
 *    (1,0,0) -> va
 *    (0,1,0) -> vb
 *    (0,0,1) -> vc
 *    (1,1,1) -> vd
 * We want M to make M*A=B where A has the left side above, as columns
 * and B has the right side as columns - both extended into homogeneous coords.
 * So `M = B*(Ainverse)`.  Doing `Ainverse` by hand gives the code below.
 * The cols of M are `1/2{va-vb+vc-vd}`, `1/2{-va+vb-vc+vd}`, `1/2{-va-vb+vc+vd}`,
 * and `1/2{va+vb+vc-vd}`
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
  return 1.0 - pow((1.0 - pow(1.0 - x, r)), (1.0 / r));
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
      BLI_assert(pro->prof_co != nullptr);
      copy_v3_v3(r_co, pro->prof_co + 3 * i);
    }
    else {
      BLI_assert(is_power_of_2_i(nseg) && nseg <= bp->pro_spacing.seg_2);
      /* Find spacing between sub-samples in `prof_co_2`. */
      int subsample_spacing = bp->pro_spacing.seg_2 / nseg;
      copy_v3_v3(r_co, pro->prof_co_2 + 3 * i * subsample_spacing);
    }
  }
}

/**
 * Helper for #calculate_profile that builds the 3D locations for the segments
 * and the higher power of 2 segments.
 */
static void calculate_profile_segments(const Profile *profile,
                                       const float map[4][4],
                                       const bool use_map,
                                       const bool reversed,
                                       const int ns,
                                       const double *xvals,
                                       const double *yvals,
                                       float *r_prof_co)
{
  /* Iterate over the vertices along the boundary arc. */
  for (int k = 0; k <= ns; k++) {
    float co[3];
    if (k == 0) {
      copy_v3_v3(co, profile->start);
    }
    else if (k == ns) {
      copy_v3_v3(co, profile->end);
    }
    else {
      if (use_map) {
        const float p[3] = {
            reversed ? float(yvals[ns - k]) : float(xvals[k]),
            reversed ? float(xvals[ns - k]) : float(yvals[k]),
            0.0f,
        };
        /* Do the 2D->3D transformation of the profile coordinates. */
        mul_v3_m4v3(co, map, p);
      }
      else {
        interp_v3_v3v3(co, profile->start, profile->end, float(k) / float(ns));
      }
    }
    /* Finish the 2D->3D transformation by projecting onto the final profile plane. */
    float *prof_co_k = r_prof_co + 3 * k;
    if (!is_zero_v3(profile->proj_dir)) {
      float co2[3];
      add_v3_v3v3(co2, co, profile->proj_dir);
      /* pro->plane_co and pro->plane_no are filled in #set_profile_params. */
      if (!isect_line_plane_v3(prof_co_k, co, co2, profile->plane_co, profile->plane_no)) {
        /* Shouldn't happen. */
        copy_v3_v3(prof_co_k, co);
      }
    }
    else {
      copy_v3_v3(prof_co_k, co);
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
  Profile *pro = &bndv->profile;
  ProfileSpacing *pro_spacing = (miter) ? &bp->pro_spacing_miter : &bp->pro_spacing;

  if (bp->seg == 1) {
    return;
  }

  bool need_2 = bp->seg != bp->pro_spacing.seg_2;
  if (pro->prof_co == nullptr) {
    pro->prof_co = (float *)BLI_memarena_alloc(bp->mem_arena, sizeof(float[3]) * (bp->seg + 1));
    if (need_2) {
      pro->prof_co_2 = (float *)BLI_memarena_alloc(bp->mem_arena,
                                                   sizeof(float[3]) * (bp->pro_spacing.seg_2 + 1));
    }
    else {
      pro->prof_co_2 = pro->prof_co;
    }
  }

  bool use_map;
  float map[4][4];
  if (bp->profile_type == BEVEL_PROFILE_SUPERELLIPSE && pro->super_r == PRO_LINE_R) {
    use_map = false;
  }
  else {
    use_map = make_unit_square_map(pro->start, pro->middle, pro->end, map);
  }

  if (bp->vmesh_method == BEVEL_VMESH_CUTOFF && use_map) {
    /* Calculate the "height" of the profile by putting the (0,0) and (1,1) corners of the
     * un-transformed profile through the 2D->3D map and calculating the distance between them. */
    float bottom_corner[3] = {0.0f, 0.0f, 0.0f};
    mul_v3_m4v3(bottom_corner, map, bottom_corner);
    float top_corner[3] = {1.0f, 1.0f, 0.0f};
    mul_v3_m4v3(top_corner, map, top_corner);

    pro->height = len_v3v3(bottom_corner, top_corner);
  }

  /* Calculate the 3D locations for the profile points */
  calculate_profile_segments(
      pro, map, use_map, reversed, bp->seg, pro_spacing->xvals, pro_spacing->yvals, pro->prof_co);
  /* Also calculate for the seg_2 case if it's needed. */
  if (need_2) {
    calculate_profile_segments(pro,
                               map,
                               use_map,
                               reversed,
                               bp->pro_spacing.seg_2,
                               pro_spacing->xvals_2,
                               pro_spacing->yvals_2,
                               pro->prof_co_2);
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
  float r = super_r;
  if (r == PRO_CIRCLE_R) {
    normalize_v3(co);
    return;
  }

  float a = max_ff(0.0f, co[0]);
  float b = max_ff(0.0f, co[1]);
  float c = max_ff(0.0f, co[2]);
  float x = a;
  float y = b;
  float z = c;
  if (ELEM(r, PRO_SQUARE_R, PRO_SQUARE_IN_R)) {
    /* Will only be called for 2d profile. */
    BLI_assert(fabsf(z) < BEVEL_EPSILON);
    z = 0.0f;
    x = min_ff(1.0f, x);
    y = min_ff(1.0f, y);
    if (r == PRO_SQUARE_R) {
      /* Snap to closer of x==1 and y==1 lines, or maybe both. */
      float dx = 1.0f - x;
      float dy = 1.0f - y;
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
    float rinv = 1.0f / r;
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

#define BEV_EXTEND_EDGE_DATA_CHECK(eh, flag) BM_elem_flag_test(eh->e, flag)

/* If a beveled edge has a seam (flag == BM_ELEM_SEAM) or a sharp
 * (flag == BM_ELEM_SMOOTH and the test is for the negation of that flag),
 * then we may need to correct for discontinuities in those edge flags after
 * beveling. The code will automatically make the outer edges of a multi-segment
 * beveled edge have the same flags. So beveled edges next to each other will not
 * lead to discontinuities. But if there are beveled edges that do NOT have a seam
 * (or sharp), then we need to mark all the edge segments of such beveled edges
 * with seam (or sharp) until we hit the next beveled edge that has such a mark.
 * This routine sets, for each rightv of a beveled edge that has seam (or sharp),
 * how many edges follow without the corresponding property. The count is put in
 * the seam_len field for seams and the sharp_len field for sharps.
 *
 * TODO: This approach doesn't work for terminal edges or miters.
 */
#define HASNOT_SEAMSHARP(eh, flag) \
  ((flag == BM_ELEM_SEAM && !BM_elem_flag_test(eh->e, BM_ELEM_SEAM)) || \
   (flag == BM_ELEM_SMOOTH && BM_elem_flag_test(eh->e, BM_ELEM_SMOOTH)))
static void check_edge_data_seam_sharp_edges(BevVert *bv, int flag)
{
  EdgeHalf *e = &bv->edges[0], *efirst = &bv->edges[0];

  /* Get to first edge with seam or sharp edge data. */
  while (HASNOT_SEAMSHARP(e, flag)) {
    e = e->next;
    if (e == efirst) {
      break;
    }
  }

  /* If no such edge found, return. */
  if (HASNOT_SEAMSHARP(e, flag)) {
    return;
  }

  /* Set efirst to this first encountered edge. */
  efirst = e;

  do {
    int flag_count = 0;
    EdgeHalf *ne = e->next;

    while (HASNOT_SEAMSHARP(ne, flag) && ne != efirst) {
      if (ne->is_bev) {
        flag_count++;
      }
      ne = ne->next;
    }
    if (ne == e || (ne == efirst && HASNOT_SEAMSHARP(efirst, flag))) {
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

/* Extend the marking of edges as seam (if flag == BM_ELEM_SEAM) or sharp
 * (if flag == BM_ELEM_SMOOTH) around the appropriate edges added as part
 * of doing a bevel at vert bv. */

static void bevel_extend_edge_data_ex(BevVert *bv, int flag)
{
  BLI_assert(ELEM(flag, BM_ELEM_SEAM, BM_ELEM_SMOOTH));
  VMesh *vm = bv->vmesh;

  BoundVert *bcur = bv->vmesh->boundstart, *start = bcur;

  do {
    /* If current boundvert has a seam/sharp length > 0 then we need to extend here. */
    int extend_len = flag == BM_ELEM_SEAM ? bcur->seam_len : bcur->sharp_len;
    if (extend_len) {
      if (!bv->vmesh->boundstart->seam_len && start == bv->vmesh->boundstart) {
        start = bcur; /* Set start to first boundvert with seam_len > 0. */
      }

      /* Now for all the mesh_verts starting at current index and ending at `idx_end`
       * we go through outermost ring and through all its segments and add seams
       * for those edges. */
      int idx_end = bcur->index + extend_len;
      for (int i = bcur->index; i < idx_end; i++) {
        BMVert *v1 = mesh_vert(vm, i % vm->count, 0, 0)->v, *v2;
        BMEdge *e;
        for (int k = 1; k < vm->seg; k++) {
          v2 = mesh_vert(vm, i % vm->count, 0, k)->v;

          /* Here v1 & v2 are current and next BMverts,
           * we find common edge and set its edge data. */
          e = v1->e;
          while (e->v1 != v2 && e->v2 != v2) {
            e = BM_DISK_EDGE_NEXT(e, v1);
          }
          if (flag == BM_ELEM_SEAM) {
            BM_elem_flag_set(e, BM_ELEM_SEAM, true);
          }
          else {
            BM_elem_flag_set(e, BM_ELEM_SMOOTH, false);
          }
          v1 = v2;
        }
        BMVert *v3 = mesh_vert(vm, (i + 1) % vm->count, 0, 0)->v;
        e = v1->e; /* Do same as above for first and last vert. */
        while (e->v1 != v3 && e->v2 != v3) {
          e = BM_DISK_EDGE_NEXT(e, v1);
        }
        if (flag == BM_ELEM_SEAM) {
          BM_elem_flag_set(e, BM_ELEM_SEAM, true);
        }
        else {
          BM_elem_flag_set(e, BM_ELEM_SMOOTH, false);
        }
        bcur = bcur->next;
      }
    }
    else {
      bcur = bcur->next;
    }
  } while (bcur != start);
}

static void bevel_extend_edge_data(BevVert *bv)
{
  VMesh *vm = bv->vmesh;

  if (vm->mesh_kind == M_TRI_FAN || bv->selcount < 2) {
    return;
  }

  bevel_extend_edge_data_ex(bv, BM_ELEM_SEAM);
  bevel_extend_edge_data_ex(bv, BM_ELEM_SMOOTH);
}

/* Mark edges as sharp if they are between a smooth reconstructed face and a new face. */
static void bevel_edges_sharp_boundary(BMesh *bm, BevelParams *bp)
{
  BMIter fiter;
  BMFace *f;
  BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
    if (!BM_elem_flag_test(f, BM_ELEM_SMOOTH)) {
      continue;
    }
    if (get_face_kind(bp, f) != F_RECON) {
      continue;
    }
    BMIter liter;
    BMLoop *l;
    BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
      /* Cases we care about will have exactly one adjacent face. */
      BMLoop *lother = l->radial_next;
      BMFace *fother = lother->f;
      if (lother != l && fother) {
        FKind fkind = get_face_kind(bp, lother->f);
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
  if (bp->offset == 0.0 || !bp->harden_normals) {
    return;
  }

  /* Recalculate all face and vertex normals. Side effect: ensures vertex, edge, face indices. */
  /* I suspect this is not necessary. TODO: test that guess. */
  BM_mesh_normals_update(bm);

  int cd_clnors_offset = CustomData_get_offset_named(
      &bm->ldata, CD_PROP_INT16_2D, "custom_normal");

  /* If there is not already a custom normal layer then making one
   * (with #BM_lnorspace_update) will not respect the auto-smooth angle between smooth faces.
   * To get that to happen, we have to mark the sharpen the edges that are only sharp because
   * of the angle test -- otherwise would be smooth. */
  if (cd_clnors_offset == -1) {
    bevel_edges_sharp_boundary(bm, bp);
  }

  /* Ensure that `bm->lnor_spacearr` has properly stored loop normals.
   * Side effect: ensures loop indices. */
  BM_lnorspace_update(bm);

  if (cd_clnors_offset == -1) {
    cd_clnors_offset = CustomData_get_offset_named(&bm->ldata, CD_PROP_INT16_2D, "custom_normal");
  }

  /* If the custom normals attribute still hasn't been added with the correct type, at least don't
   * crash. */
  if (cd_clnors_offset == -1) {
    return;
  }

  BMIter fiter;
  BMFace *f;
  BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
    FKind fkind = get_face_kind(bp, f);
    if (ELEM(fkind, F_ORIG, F_RECON)) {
      continue;
    }
    BMIter liter;
    BMLoop *l;
    BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
      BMEdge *estep = l->prev->e; /* Causes CW walk around l->v fan. */
      BMLoop *lprev = BM_vert_step_fan_loop(l, &estep);
      estep = l->e; /* Causes CCW walk around l->v fan. */
      BMLoop *lnext = BM_vert_step_fan_loop(l, &estep);
      FKind fprevkind = lprev ? get_face_kind(bp, lprev->f) : F_NONE;
      FKind fnextkind = lnext ? get_face_kind(bp, lnext->f) : F_NONE;

      float norm[3];
      float *pnorm = nullptr;
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
          // printf("unexpected harden case (edge)\n");
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
          BMLoop *lprevprev, *lnextnext;
          if (lprev) {
            estep = lprev->prev->e;
            lprevprev = BM_vert_step_fan_loop(lprev, &estep);
          }
          else {
            lprevprev = nullptr;
          }
          if (lnext) {
            estep = lnext->e;
            lnextnext = BM_vert_step_fan_loop(lnext, &estep);
          }
          else {
            lnextnext = nullptr;
          }
          FKind fprevprevkind = lprevprev ? get_face_kind(bp, lprevprev->f) : F_NONE;
          FKind fnextnextkind = lnextnext ? get_face_kind(bp, lnextnext->f) : F_NONE;
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
            // printf("unexpected harden case (vert)\n");
          }
        }
      }
      if (pnorm) {
        if (pnorm == norm) {
          normalize_v3(norm);
        }
        int l_index = BM_elem_index_get(l);
        short *clnors = static_cast<short *>(BM_ELEM_CD_GET_VOID_P(l, cd_clnors_offset));
        BKE_lnor_space_custom_normal_to_data(bm->lnor_spacearr->lspacearr[l_index], pnorm, clnors);
      }
    }
  }
}

static void bevel_set_weighted_normal_face_strength(BMesh *bm, BevelParams *bp)
{
  const int mode = bp->face_strength_mode;
  const char *wn_layer_id = MOD_WEIGHTEDNORMALS_FACEWEIGHT_CDLAYER_ID;
  int cd_prop_int_idx = CustomData_get_named_layer_index(&bm->pdata, CD_PROP_INT32, wn_layer_id);

  if (cd_prop_int_idx == -1) {
    BM_data_layer_add_named(bm, &bm->pdata, CD_PROP_INT32, wn_layer_id);
    cd_prop_int_idx = CustomData_get_named_layer_index(&bm->pdata, CD_PROP_INT32, wn_layer_id);
  }
  cd_prop_int_idx -= CustomData_get_layer_index(&bm->pdata, CD_PROP_INT32);
  const int cd_prop_int_offset = CustomData_get_n_offset(
      &bm->pdata, CD_PROP_INT32, cd_prop_int_idx);

  BMIter fiter;
  BMFace *f;
  BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
    FKind fkind = get_face_kind(bp, f);
    bool do_set_strength = true;
    int strength;
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
      int *strength_ptr = static_cast<int *>(BM_ELEM_CD_GET_VOID_P(f, cd_prop_int_offset));
      *strength_ptr = strength;
    }
  }
}

/* Set the any_seam property for a BevVert and all its BoundVerts. */
static void set_bound_vert_seams(BevVert *bv, bool mark_seam, bool mark_sharp)
{
  bv->any_seam = false;
  BoundVert *v = bv->vmesh->boundstart;
  do {
    v->any_seam = false;
    for (EdgeHalf *e = v->efirst; e; e = e->next) {
      v->any_seam |= e->is_seam;
      if (e == v->elast) {
        break;
      }
    }
    bv->any_seam |= v->any_seam;
  } while ((v = v->next) != bv->vmesh->boundstart);

  if (mark_seam) {
    check_edge_data_seam_sharp_edges(bv, BM_ELEM_SEAM);
  }
  if (mark_sharp) {
    check_edge_data_seam_sharp_edges(bv, BM_ELEM_SMOOTH);
  }
}

/* Is e between two faces with a 180 degree angle between their normals? */
static bool eh_on_plane(EdgeHalf *e)
{
  if (e->fprev && e->fnext) {
    float dot = dot_v3v3(e->fprev->no, e->fnext->no);
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
    if (bp->profile_type == BEVEL_PROFILE_CUSTOM) {
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

  BLI_assert(bp->affect_type == BEVEL_AFFECT_VERTICES);

  EdgeHalf *efirst = &bv->edges[0];
  EdgeHalf *e = efirst;
  do {
    float co[3];
    slide_dist(e, bv->v, e->offset_l, co);
    if (construct) {
      BoundVert *v = add_new_bound_vert(bp->mem_arena, vm, co);
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

  EdgeHalf *e = efirst;
  float co[3];
  if (bv->edgecount == 2) {
    /* Only 2 edges in, so terminate the edge with an artificial vertex on the unbeveled edge.
     * If the offset type is BEVEL_AMT_PERCENT or BEVEL_AMT_ABSOLUTE, what to do is a bit
     * undefined (there aren't two "legs"), so just let the code do what it does. */
    const float *no = e->fprev ? e->fprev->no : (e->fnext ? e->fnext->no : nullptr);
    offset_in_plane(e, no, true, co);
    if (construct) {
      BoundVert *bndv = add_new_bound_vert(mem_arena, vm, co);
      bndv->efirst = bndv->elast = bndv->ebev = e;
      e->leftv = bndv;
    }
    else {
      adjust_bound_vert(e->leftv, co);
    }
    no = e->fnext ? e->fnext->no : (e->fprev ? e->fprev->no : nullptr);
    offset_in_plane(e, no, false, co);
    if (construct) {
      BoundVert *bndv = add_new_bound_vert(mem_arena, vm, co);
      bndv->efirst = bndv->elast = e;
      e->rightv = bndv;
    }
    else {
      adjust_bound_vert(e->rightv, co);
    }
    /* Make artificial extra point along unbeveled edge, and form triangle. */
    slide_dist(e->next, bv->v, e->offset_l, co);
    if (construct) {
      BoundVert *bndv = add_new_bound_vert(mem_arena, vm, co);
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
    bool leg_slide = bp->offset_type == BEVEL_AMT_PERCENT || bp->offset_type == BEVEL_AMT_ABSOLUTE;
    if (leg_slide) {
      slide_dist(e->prev, bv->v, e->offset_l, co);
    }
    else {
      offset_meet(bp, e->prev, e, bv->v, e->fprev, false, co, nullptr);
    }
    if (construct) {
      BoundVert *bndv = add_new_bound_vert(mem_arena, vm, co);
      bndv->efirst = e->prev;
      bndv->elast = bndv->ebev = e;
      e->leftv = bndv;
      e->prev->leftv = e->prev->rightv = bndv;
    }
    else {
      adjust_bound_vert(e->leftv, co);
    }
    e = e->next;
    if (leg_slide) {
      slide_dist(e, bv->v, e->prev->offset_r, co);
    }
    else {
      offset_meet(bp, e->prev, e, bv->v, e->fprev, false, co, nullptr);
    }
    if (construct) {
      BoundVert *bndv = add_new_bound_vert(mem_arena, vm, co);
      bndv->efirst = e->prev;
      bndv->elast = e;
      e->leftv = e->rightv = bndv;
      e->prev->rightv = bndv;
    }
    else {
      adjust_bound_vert(e->leftv, co);
    }
    /* For the edges not adjacent to the beveled edge, slide the bevel amount along. */
    float d = efirst->offset_l_spec;
    if (bp->profile_type == BEVEL_PROFILE_CUSTOM || bp->profile < 0.25f) {
      d *= sqrtf(2.0f); /* Need to go further along the edge to make room for full profile area. */
    }
    for (e = e->next; e->next != efirst; e = e->next) {
      slide_dist(e, bv->v, d, co);
      if (construct) {
        BoundVert *bndv = add_new_bound_vert(mem_arena, vm, co);
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
    BoundVert *bndv = vm->boundstart;
    BLI_assert(bndv->ebev != nullptr);
    set_profile_params(bp, bv, bndv);
    move_profile_plane(bndv, bv->v);
  }

  if (construct) {
    set_bound_vert_seams(bv, bp->mark_seam, bp->mark_sharp);

    if (vm->count == 2 && bv->edgecount == 3) {
      vm->mesh_kind = M_NONE;
    }
    else if (vm->count == 3) {
      bool use_tri_fan = true;
      if (bp->profile_type == BEVEL_PROFILE_CUSTOM) {
        /* Prevent overhanging edges: use M_POLY if the extra point is planar with the profile. */
        BoundVert *bndv = efirst->leftv;
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
  int miter_outer = bp->miter_outer;

  BoundVert *v1 = emiter->rightv;
  BoundVert *v2, *v3;
  if (miter_outer == BEVEL_MITER_PATCH) {
    v2 = v1->next;
    v3 = v2->next;
  }
  else {
    BLI_assert(miter_outer == BEVEL_MITER_ARC);
    v2 = nullptr;
    v3 = v1->next;
  }
  BoundVert *v1prev = v1->prev;
  BoundVert *v3next = v3->next;
  float co2[3];
  copy_v3_v3(co2, v1->nv.co);
  if (v1->is_arc_start) {
    copy_v3_v3(v1->profile.middle, co2);
  }

  /* co1 is intersection of line through co2 in dir of emiter->e
   * and plane with normal the dir of emiter->e and through v1prev. */
  float co1[3], edge_dir[3], line_p[3];
  BMVert *vother = BM_edge_other_vert(emiter->e, bv->v);
  sub_v3_v3v3(edge_dir, bv->v->co, vother->co);
  normalize_v3(edge_dir);
  float d = bp->offset / (bp->seg / 2.0f); /* A fallback amount to move. */
  madd_v3_v3v3fl(line_p, co2, edge_dir, d);
  if (!isect_line_plane_v3(co1, co2, line_p, v1prev->nv.co, edge_dir)) {
    copy_v3_v3(co1, line_p);
  }
  adjust_bound_vert(v1, co1);

  /* co3 is similar, but plane is through v3next and line is other side of miter edge. */
  float co3[3];
  EdgeHalf *emiter_other = v3->elast;
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
  BoundVert *vstart = bv->vmesh->boundstart;
  BoundVert *v = vstart;
  do {
    if (v->is_arc_start) {
      BoundVert *v3 = v->next;
      EdgeHalf *e = v->efirst;
      if (e != emiter) {
        float edge_dir[3], co[3];
        copy_v3_v3(co, v->nv.co);
        BMVert *vother = BM_edge_other_vert(e->e, bv->v);
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

  /* Current bevel does nothing if only one edge into a vertex. */
  if (bv->edgecount <= 1) {
    return;
  }

  if (bp->affect_type == BEVEL_AFFECT_VERTICES) {
    build_boundary_vertex_only(bp, bv, construct);
    return;
  }

  VMesh *vm = bv->vmesh;

  /* Find a beveled edge to be efirst. */
  EdgeHalf *efirst = next_bev(bv, nullptr);
  BLI_assert(efirst->is_bev);

  if (bv->selcount == 1) {
    /* Special case: only one beveled edge in. */
    build_boundary_terminal_edge(bp, bv, efirst, construct);
    return;
  }

  /* Special miters outside only for 3 or more beveled edges. */
  int miter_outer = (bv->selcount >= 3) ? bp->miter_outer : BEVEL_MITER_SHARP;
  int miter_inner = bp->miter_inner;

  /* Keep track of the first beveled edge of an outside miter (there can be at most 1 per bv). */
  EdgeHalf *emiter = nullptr;

  /* There is more than one beveled edge.
   * We make BoundVerts to connect the sides of the beveled edges.
   * Non-beveled edges in between will just join to the appropriate juncture point. */
  EdgeHalf *e = efirst;
  do {
    BLI_assert(e->is_bev);
    EdgeHalf *eon = nullptr;
    /* Make the BoundVert for the right side of e; the other side will be made when the beveled
     * edge to the left of e is handled.
     * Analyze edges until next beveled edge: They are either "in plane" (preceding and subsequent
     * faces are coplanar) or not. The "non-in-plane" edges affect the silhouette and we prefer to
     * slide along one of those if possible. */
    int in_plane = 0; /* Counts of in-plane / not-in-plane. */
    int not_in_plane = 0;
    EdgeHalf *enip = nullptr; /* Representatives of each type. */
    EdgeHalf *eip = nullptr;
    EdgeHalf *e2;
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

    float r, co[3];
    if (in_plane == 0 && not_in_plane == 0) {
      offset_meet(bp, e, e2, bv->v, e->fnext, false, co, nullptr);
    }
    else if (not_in_plane > 0) {
      if (bp->loop_slide && not_in_plane == 1 && good_offset_on_edge_between(e, e2, enip, bv->v)) {
        if (offset_on_edge_between(bp, e, e2, enip, bv->v, co, &r)) {
          eon = enip;
        }
      }
      else {
        offset_meet(bp, e, e2, bv->v, nullptr, true, co, eip);
      }
    }
    else {
      /* n_in_plane > 0 and n_not_in_plane == 0. */
      if (bp->loop_slide && in_plane == 1 && good_offset_on_edge_between(e, e2, eip, bv->v)) {
        if (offset_on_edge_between(bp, e, e2, eip, bv->v, co, &r)) {
          eon = eip;
        }
      }
      else {
        /* Since all edges between e and e2 are in the same plane, it is OK
         * to treat this like the case where there are no edges between. */
        offset_meet(bp, e, e2, bv->v, e->fnext, false, co, nullptr);
      }
    }

    if (construct) {
      BoundVert *v = add_new_bound_vert(mem_arena, vm, co);
      v->efirst = e;
      v->elast = e2;
      v->ebev = e2;
      v->eon = eon;
      if (eon) {
        v->sinratio = r;
      }
      e->rightv = v;
      e2->leftv = v;
      for (EdgeHalf *e3 = e->next; e3 != e2; e3 = e3->next) {
        e3->leftv = e3->rightv = v;
      }
      AngleKind ang_kind = edges_angle_kind(e, e2, bv->v);

      /* Are we doing special mitering?
       * There can only be one outer reflex angle, so only one outer miter,
       * and emiter will be set to the first edge of such an edge.
       * A miter kind of BEVEL_MITER_SHARP means no special miter */
      if ((miter_outer != BEVEL_MITER_SHARP && !emiter && ang_kind == ANGLE_LARGER) ||
          (miter_inner != BEVEL_MITER_SHARP && ang_kind == ANGLE_SMALLER))
      {
        if (ang_kind == ANGLE_LARGER) {
          emiter = e;
        }
        /* Make one or two more boundverts; for now all will have same co. */
        BoundVert *v1 = v;
        v1->ebev = nullptr;
        BoundVert *v2;
        if (ang_kind == ANGLE_LARGER && miter_outer == BEVEL_MITER_PATCH) {
          v2 = add_new_bound_vert(mem_arena, vm, co);
        }
        else {
          v2 = nullptr;
        }
        BoundVert *v3 = add_new_bound_vert(mem_arena, vm, co);
        v3->ebev = e2;
        v3->efirst = e2;
        v3->elast = e2;
        v3->eon = nullptr;
        e2->leftv = v3;
        if (ang_kind == ANGLE_LARGER && miter_outer == BEVEL_MITER_PATCH) {
          v1->is_patch_start = true;
          v2->eon = v1->eon;
          v2->sinratio = v1->sinratio;
          v2->ebev = nullptr;
          v1->eon = nullptr;
          v1->sinratio = 1.0f;
          v1->elast = e;
          if (e->next == e2) {
            v2->efirst = nullptr;
            v2->elast = nullptr;
          }
          else {
            v2->efirst = e->next;
            for (EdgeHalf *e3 = e->next; e3 != e2; e3 = e3->next) {
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
             * If between is odd, put middle one at mid-index. */
            for (EdgeHalf *e3 = e->next; e3 != e2; e3 = e3->next) {
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
      AngleKind ang_kind = edges_angle_kind(e, e2, bv->v);
      if ((miter_outer != BEVEL_MITER_SHARP && !emiter && ang_kind == ANGLE_LARGER) ||
          (miter_inner != BEVEL_MITER_SHARP && ang_kind == ANGLE_SMALLER))
      {
        if (ang_kind == ANGLE_LARGER) {
          emiter = e;
        }
        BoundVert *v1 = e->rightv;
        BoundVert *v2;
        BoundVert *v3;
        if (ang_kind == ANGLE_LARGER && miter_outer == BEVEL_MITER_PATCH) {
          v2 = v1->next;
          v3 = v2->next;
        }
        else {
          v2 = nullptr;
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
  printf("\nSolution analysis\n");
  double even_residual2 = 0.0;
  double spec_residual2 = 0.0;
  double max_even_r = 0.0;
  double max_even_r_pct = 0.0;
  double max_spec_r = 0.0;
  double max_spec_r_pct = 0.0;
  printf("width matching\n");
  BoundVert *v = vstart;
  do {
    if (v->adjchain != nullptr) {
      EdgeHalf *eright = v->efirst;
      EdgeHalf *eleft = v->adjchain->elast;
      double delta = fabs(eright->offset_r - eleft->offset_l);
      double delta_pct = 100.0 * delta / eright->offset_r_spec;
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
    if (v->adjchain != nullptr) {
      EdgeHalf *eright = v->efirst;
      EdgeHalf *eleft = v->adjchain->elast;
      double delta = eright->offset_r - eright->offset_r_spec;
      double delta_pct = 100.0 * delta / eright->offset_r_spec;
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
  float *g = MEM_mallocN(np * sizeof(float), "beveladjust");
  float *g_prod = MEM_mallocN(np * sizeof(float), "beveladjust");

  BoundVert *v = vstart;
  float spec_sum = 0.0f;
  int i = 0;
  do {
    g[i] = v->sinratio;
    if (iscycle || v->adjchain != nullptr) {
      spec_sum += v->efirst->offset_r;
    }
    else {
      spec_sum += v->elast->offset_l;
    }
    i++;
    v = v->adjchain;
  } while (v && v != vstart);

  float gprod = 1.00f;
  float gprod_sum = 1.0f;
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
  float p = spec_sum / gprod_sum;

  /* Apply the new offsets. */
  v = vstart;
  i = 0;
  do {
    if (iscycle || v->adjchain != nullptr) {
      EdgeHalf *eright = v->efirst;
      EdgeHalf *eleft = v->elast;
      eright->offset_r = g_prod[(i + 1) % np] * p;
      if (iscycle || v != vstart) {
        eleft->offset_l = v->sinratio * eright->offset_r;
      }
    }
    else {
      /* Not a cycle, and last of chain. */
      EdgeHalf *eleft = v->elast;
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
  /* Case 1: The next EdgeHalf is the other side of the BMEdge.
   * It's part of the same BMEdge, so we know the other EdgeHalf is also beveled. */
  if (!toward_bv) {
    return find_other_end_edge_half(bp, start_edge, r_bv);
  }

  /* Case 2: The next EdgeHalf is across a BevVert from the current EdgeHalf. */
  /* Skip all the logic if there's only one beveled edge at the vertex, we're at an end. */
  if ((*r_bv)->selcount == 1) {
    return nullptr; /* No other edges to go to. */
  }

  /* The case with only one other edge connected to the vertex is special too. */
  if ((*r_bv)->selcount == 2) {
    /* Just find the next beveled edge, that's the only other option. */
    EdgeHalf *new_edge = start_edge;
    do {
      new_edge = new_edge->next;
    } while (!new_edge->is_bev);

    return new_edge;
  }

  /* Find the direction vector of the current edge (pointing INTO the BevVert).
   * v1 and v2 don't necessarily have an order, so we need to check which is closer to bv. */
  float dir_start_edge[3];
  if (start_edge->e->v1 == (*r_bv)->v) {
    sub_v3_v3v3(dir_start_edge, start_edge->e->v1->co, start_edge->e->v2->co);
  }
  else {
    sub_v3_v3v3(dir_start_edge, start_edge->e->v2->co, start_edge->e->v1->co);
  }
  normalize_v3(dir_start_edge);

  /* Find the beveled edge coming out of the BevVert that's most parallel to the current edge. */
  EdgeHalf *new_edge = start_edge->next;
  float second_best_dot = 0.0f, best_dot = 0.0f;
  EdgeHalf *next_edge = nullptr;
  while (new_edge != start_edge) {
    if (!new_edge->is_bev) {
      new_edge = new_edge->next;
      continue;
    }
    /* Find direction vector of the possible next edge (pointing OUT of the BevVert). */
    float dir_new_edge[3];
    if (new_edge->e->v2 == (*r_bv)->v) {
      sub_v3_v3v3(dir_new_edge, new_edge->e->v1->co, new_edge->e->v2->co);
    }
    else {
      sub_v3_v3v3(dir_new_edge, new_edge->e->v2->co, new_edge->e->v1->co);
    }
    normalize_v3(dir_new_edge);

    /* Use this edge if it is the most parallel to the original so far. */
    float new_dot = dot_v3v3(dir_new_edge, dir_start_edge);
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
  if ((next_edge != nullptr) && compare_ff(best_dot, second_best_dot, BEVEL_SMALL_ANG_DOT)) {
    return nullptr;
  }
  return next_edge;
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
  for (int i = 0; i < 2; i++) {
    EdgeHalf *edgehalf = start_edgehalf;
    BevVert *bv = start_bv;
    bool toward_bv = (i == 0);
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
  int np = 0;
#ifdef DEBUG_ADJUST
  printf("\nadjust the %s (with eigen)\n", iscycle ? "cycle" : "chain");
#endif
  BoundVert *v = vstart;
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

  int nrows = iscycle ? 3 * np : 3 * np - 3;

  LinearSolver *solver = EIG_linear_least_squares_solver_new(nrows, np, 1);

  v = vstart;
  int i = 0;
  /* Square root of factor to weight down importance of spec match. */
  double weight = BEVEL_MATCH_SPEC_WEIGHT;
  EdgeHalf *eleft, *eright, *enextleft;
  do {
    /* Except at end of chain, v's indep variable is offset_r of `v->efirst`. */
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
       * right offset for parameter i matches its spec; weighted. */
      int row = iscycle ? np + 2 * i : np - 1 + 2 * i;
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
       * left offset for parameter i matches its spec; weighted. */
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
  /* NOTE: this print only works after solve, but by that time b has been cleared. */
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
    double val = EIG_linear_solver_variable_get(solver, 0, i);
    if (iscycle || i < np - 1) {
      eright = v->efirst;
      eleft = v->elast;
      eright->offset_r = float(val);
#ifdef DEBUG_ADJUST
      printf("e%d->offset_r = %f\n", BM_elem_index_get(eright->e), eright->offset_r);
#endif
      if (iscycle || v != vstart) {
        eleft->offset_l = float(v->sinratio * val);
#ifdef DEBUG_ADJUST
        printf("e%d->offset_l = %f\n", BM_elem_index_get(eleft->e), eleft->offset_l);
#endif
      }
    }
    else {
      /* Not a cycle, and last of chain. */
      eleft = v->elast;
      eleft->offset_l = float(val);
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
  /* Find and process chains and cycles of unvisited BoundVerts that have eon set. */
  /* NOTE: for repeatability, iterate over all verts of mesh rather than over ghash'ed BMVerts. */
  BMIter iter;
  BMVert *bmv;
  BM_ITER_MESH (bmv, &iter, bm, BM_VERTS_OF_MESH) {
    if (!BM_elem_flag_test(bmv, BM_ELEM_TAG)) {
      continue;
    }
    BevVert *bv = find_bevvert(bp, bmv);
    BevVert *bvcur = bv;
    if (!bv) {
      continue;
    }
    BoundVert *vanchor = bv->vmesh->boundstart;
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
      BoundVert *v, *vchainstart, *vchainend;
      v = vchainstart = vchainend = vanchor;

      bool iscycle = false;
      int chainlen = 1;
      while (v->eon && !v->visited && !iscycle) {
        v->visited = true;
        if (!v->efirst) {
          break;
        }
        EdgeHalf *enext = find_other_end_edge_half(bp, v->efirst, &bvcur);
        if (!enext) {
          break;
        }
        BLI_assert(enext != nullptr);
        BoundVert *vnext = enext->leftv;
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
        v->adjchain = nullptr;
        v = vchainstart;
        bvcur = bv;
        do {
          v->visited = true;
          if (!v->elast) {
            break;
          }
          EdgeHalf *enext = find_other_end_edge_half(bp, v->elast, &bvcur);
          if (!enext) {
            break;
          }
          BoundVert *vnext = enext->rightv;
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
      BevVert *bv = find_bevvert(bp, bmv);
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
  VMesh *vm = bv->vmesh;
  if (vm->count < 3 || vm->count > 4 || bv->selcount < 3 || bv->selcount > 4) {
    return nullptr;
  }

  /* Find v1, v2, v3 all with beveled edges, where v1 and v3 have collinear edges. */
  EdgeHalf *epipe = nullptr;
  BoundVert *v1 = vm->boundstart;
  float dir1[3], dir3[3];
  do {
    BoundVert *v2 = v1->next;
    BoundVert *v3 = v2->next;
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
    return nullptr;
  }

  /* Check face planes: all should have normals perpendicular to epipe. */
  for (EdgeHalf *e = &bv->edges[0]; e != &bv->edges[bv->edgecount]; e++) {
    if (e->fnext) {
      if (fabsf(dot_v3v3(dir1, e->fnext->no)) > BEVEL_EPSILON_BIG) {
        return nullptr;
      }
    }
  }
  return v1;
}

static VMesh *new_adj_vmesh(MemArena *mem_arena, int count, int seg, BoundVert *bounds)
{
  VMesh *vm = (VMesh *)BLI_memarena_alloc(mem_arena, sizeof(VMesh));
  vm->count = count;
  vm->seg = seg;
  vm->boundstart = bounds;
  vm->mesh = (NewVert *)BLI_memarena_alloc(mem_arena,
                                           sizeof(NewVert) * count * (1 + seg / 2) * (1 + seg));
  vm->mesh_kind = M_ADJ;
  return vm;
}

/**
 * VMesh verts for vertex i have data for (i, 0 <= j <= ns2, 0 <= k <= ns),
 * where ns2 = floor(nseg / 2).
 * But these overlap data from previous and next i: there are some forced equivalences.
 * Let's call these indices the canonical ones: we will just calculate data for these
 *    0 <= j <= ns2, 0 <= k <= ns2  (for odd ns)
 *    0 <= j < ns2, 0 <= k <= ns2  (for even ns)
 *    also (j=ns2, k=ns2) at i=0 (for even ns2)
 * This function returns the canonical one for any i, j, k in [0,n],[0,ns],[0,ns].
 */
static NewVert *mesh_vert_canon(VMesh *vm, int i, int j, int k)
{
  int n = vm->count;
  int ns = vm->seg;
  int ns2 = ns / 2;
  int odd = ns % 2;
  BLI_assert(0 <= i && i <= n && 0 <= j && j <= ns && 0 <= k && k <= ns);

  if (!odd && j == ns2 && k == ns2) {
    return mesh_vert(vm, 0, j, k);
  }
  if (j <= ns2 - 1 + odd && k <= ns2) {
    return mesh_vert(vm, i, j, k);
  }
  if (k <= ns2) {
    return mesh_vert(vm, (i + n - 1) % n, k, ns - j);
  }
  return mesh_vert(vm, (i + 1) % n, ns - k, j);
}

static bool is_canon(VMesh *vm, int i, int j, int k)
{
  int ns2 = vm->seg / 2;
  if (vm->seg % 2 == 1) { /* Odd. */
    return (j <= ns2 && k <= ns2);
  }
  /* Even. */
  return ((j < ns2 && k <= ns2) || (j == ns2 && k == ns2 && i == 0));
}

/* Copy the vertex data to all of vm verts from canonical ones. */
static void vmesh_copy_equiv_verts(VMesh *vm)
{
  int n = vm->count;
  int ns = vm->seg;
  int ns2 = ns / 2;
  for (int i = 0; i < n; i++) {
    for (int j = 0; j <= ns2; j++) {
      for (int k = 0; k <= ns; k++) {
        if (is_canon(vm, i, j, k)) {
          continue;
        }
        NewVert *v1 = mesh_vert(vm, i, j, k);
        NewVert *v0 = mesh_vert_canon(vm, i, j, k);
        copy_v3_v3(v1->co, v0->co);
        v1->v = v0->v;
      }
    }
  }
}

/* Calculate and return in r_cent the centroid of the center poly. */
static void vmesh_center(VMesh *vm, float r_cent[3])
{
  int n = vm->count;
  int ns2 = vm->seg / 2;
  if (vm->seg % 2) {
    zero_v3(r_cent);
    for (int i = 0; i < n; i++) {
      add_v3_v3(r_cent, mesh_vert(vm, i, ns2, ns2)->co);
    }
    mul_v3_fl(r_cent, 1.0f / float(n));
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
  /* pPrecalculated for common cases of n. */
  if (n < 3) {
    return 0.0f;
  }
  if (n == 3) {
    return 0.065247584f;
  }
  if (n == 4) {
    return 0.25f;
  }
  if (n == 5) {
    return 0.401983447f;
  }
  if (n == 6) {
    return 0.523423277f;
  }
  double k = cos(M_PI / double(n));
  /* Need x, real root of x^3 + (4k^2 - 3)x - 2k = 0.
   * Answer calculated via Wolfram Alpha. */
  double k2 = k * k;
  double k4 = k2 * k2;
  double k6 = k4 * k2;
  double y = pow(M_SQRT3 * sqrt(64.0 * k6 - 144.0 * k4 + 135.0 * k2 - 27.0) + 9.0 * k, 1.0 / 3.0);
  double x = 0.480749856769136 * y - (0.231120424783545 * (12.0 * k2 - 9.0)) / y;
  return (k * x + 2.0 * k2 - 1.0) / (x * x * (k * x + 1.0));
}

/* Fill frac with fractions of the way along ring 0 for vertex i, for use with interp_range
 * function. */
static void fill_vmesh_fracs(VMesh *vm, float *frac, int i)
{
  float total = 0.0f;

  int ns = vm->seg;
  frac[0] = 0.0f;
  for (int k = 0; k < ns; k++) {
    total += len_v3v3(mesh_vert(vm, i, 0, k)->co, mesh_vert(vm, i, 0, k + 1)->co);
    frac[k + 1] = total;
  }
  if (total > 0.0f) {
    for (int k = 1; k <= ns; k++) {
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
  float co[3], nextco[3];
  float total = 0.0f;

  frac[0] = 0.0f;
  copy_v3_v3(co, bndv->nv.co);
  for (int k = 0; k < ns; k++) {
    get_profile_point(bp, &bndv->profile, k + 1, ns, nextco);
    total += len_v3v3(co, nextco);
    frac[k + 1] = total;
    copy_v3_v3(co, nextco);
  }
  if (total > 0.0f) {
    for (int k = 1; k <= ns; k++) {
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
  /* Could binary search in frac, but expect n to be reasonably small. */
  for (int i = 0; i < n; i++) {
    if (f <= frac[i + 1]) {
      float rest = f - frac[i];
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
  int n_bndv = vm_in->count;
  int ns_in = vm_in->seg;
  int nseg2 = nseg / 2;
  int odd = nseg % 2;
  VMesh *vm_out = new_adj_vmesh(bp->mem_arena, n_bndv, nseg, vm_in->boundstart);

  float *prev_frac = BLI_array_alloca(prev_frac, (ns_in + 1));
  float *frac = BLI_array_alloca(frac, (ns_in + 1));
  float *new_frac = BLI_array_alloca(new_frac, (nseg + 1));
  float *prev_new_frac = BLI_array_alloca(prev_new_frac, (nseg + 1));

  fill_vmesh_fracs(vm_in, prev_frac, n_bndv - 1);
  BoundVert *bndv = vm_in->boundstart;
  fill_profile_fracs(bp, bndv->prev, prev_new_frac, nseg);
  for (int i = 0; i < n_bndv; i++) {
    fill_vmesh_fracs(vm_in, frac, i);
    fill_profile_fracs(bp, bndv, new_frac, nseg);
    for (int j = 0; j <= nseg2 - 1 + odd; j++) {
      for (int k = 0; k <= nseg2; k++) {
        /* Finding the locations where "fraction" fits into previous and current "frac". */
        float fraction = new_frac[k];
        float restk;
        float restkprev;
        int k_in = interp_range(frac, ns_in, fraction, &restk);
        fraction = prev_new_frac[nseg - j];
        int k_in_prev = interp_range(prev_frac, ns_in, fraction, &restkprev);
        int j_in = ns_in - k_in_prev;
        float restj = -restkprev;
        if (restj > -BEVEL_EPSILON) {
          restj = 0.0f;
        }
        else {
          j_in = j_in - 1;
          restj = 1.0f + restj;
        }
        /* Use bilinear interpolation within the source quad; could be smarter here. */
        float co[3];
        if (restj < BEVEL_EPSILON && restk < BEVEL_EPSILON) {
          copy_v3_v3(co, mesh_vert_canon(vm_in, i, j_in, k_in)->co);
        }
        else {
          int j0inc = (restj < BEVEL_EPSILON || j_in == ns_in) ? 0 : 1;
          int k0inc = (restk < BEVEL_EPSILON || k_in == ns_in) ? 0 : 1;
          float quad[4][3];
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
    memcpy(prev_frac, frac, sizeof(float) * (ns_in + 1));
    memcpy(prev_new_frac, new_frac, sizeof(float) * (nseg + 1));
  }
  if (!odd) {
    float center[3];
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
  float co[3];

  int n_boundary = vm_in->count;
  int ns_in = vm_in->seg;
  int ns_in2 = ns_in / 2;
  BLI_assert(ns_in % 2 == 0);
  int ns_out = 2 * ns_in;
  VMesh *vm_out = new_adj_vmesh(bp->mem_arena, n_boundary, ns_out, vm_in->boundstart);

  /* First we adjust the boundary vertices of the input mesh, storing in output mesh. */
  for (int i = 0; i < n_boundary; i++) {
    copy_v3_v3(mesh_vert(vm_out, i, 0, 0)->co, mesh_vert(vm_in, i, 0, 0)->co);
    for (int k = 1; k < ns_in; k++) {
      copy_v3_v3(co, mesh_vert(vm_in, i, 0, k)->co);

      /* Smooth boundary rule. Custom profiles shouldn't be smoothed. */
      if (bp->profile_type != BEVEL_PROFILE_CUSTOM) {
        float co1[3], co2[3], acc[3];
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
  BoundVert *bndv = vm_out->boundstart;
  for (int i = 0; i < n_boundary; i++) {
    for (int k = 1; k < ns_out; k += 2) {
      get_profile_point(bp, &bndv->profile, k, ns_out, co);

      /* Smooth if using a non-custom profile. */
      if (bp->profile_type != BEVEL_PROFILE_CUSTOM) {
        float co1[3], co2[3], acc[3];
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
  for (int i = 0; i < n_boundary; i++) {
    for (int k = 0; k < ns_in; k++) {
      copy_v3_v3(mesh_vert(vm_in, i, 0, k)->co, mesh_vert(vm_out, i, 0, 2 * k)->co);
    }
  }

  vmesh_copy_equiv_verts(vm_in);

  /* Now we do the internal vertices, using standard Catmull-Clark
   * and assuming all boundary vertices have valence 4. */

  /* The new face vertices. */
  for (int i = 0; i < n_boundary; i++) {
    for (int j = 0; j < ns_in2; j++) {
      for (int k = 0; k < ns_in2; k++) {
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
  for (int i = 0; i < n_boundary; i++) {
    for (int j = 0; j < ns_in2; j++) {
      for (int k = 1; k <= ns_in2; k++) {
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
  for (int i = 0; i < n_boundary; i++) {
    for (int j = 1; j < ns_in2; j++) {
      for (int k = 0; k < ns_in2; k++) {
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
  float gamma = 0.25f;
  float beta = -gamma;
  for (int i = 0; i < n_boundary; i++) {
    for (int j = 1; j < ns_in2; j++) {
      for (int k = 1; k <= ns_in2; k++) {
        float co1[3], co2[3];
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
  float co1[3], co2[3];
  zero_v3(co1);
  zero_v3(co2);
  for (int i = 0; i < n_boundary; i++) {
    add_v3_v3(co1, mesh_vert(vm_out, i, ns_in, ns_in - 1)->co);
    add_v3_v3(co2, mesh_vert(vm_out, i, ns_in - 1, ns_in - 1)->co);
    add_v3_v3(co2, mesh_vert(vm_out, i, ns_in - 1, ns_in + 1)->co);
  }
  copy_v3_v3(co, co1);
  mul_v3_fl(co, 1.0f / float(n_boundary));
  madd_v3_v3fl(co, co2, beta / (2.0f * float(n_boundary)));
  madd_v3_v3fl(co, mesh_vert(vm_in, 0, ns_in2, ns_in2)->co, gamma);
  for (int i = 0; i < n_boundary; i++) {
    copy_v3_v3(mesh_vert(vm_out, i, ns_in, ns_in)->co, co);
  }

  /* Final step: Copy the profile vertices to the VMesh's boundary. */
  bndv = vm_out->boundstart;
  for (int i = 0; i < n_boundary; i++) {
    int inext = (i + 1) % n_boundary;
    for (int k = 0; k <= ns_out; k++) {
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
  int ns2 = nseg / 2;
  VMesh *vm = new_adj_vmesh(mem_arena, 3, nseg, nullptr);
  vm->count = 0; /* Reset, so the following loop will end up with correct count. */
  for (int i = 0; i < 3; i++) {
    float co[3] = {0.0f, 0.0f, 0.0f};
    co[i] = 1.0f;
    add_new_bound_vert(mem_arena, vm, co);
  }
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j <= ns2; j++) {
      for (int k = 0; k <= ns2; k++) {
        if (!is_canon(vm, i, j, k)) {
          continue;
        }
        float co[3];
        co[i] = 1.0f;
        co[(i + 1) % 3] = float(k) * 2.0f / float(nseg);
        co[(i + 2) % 3] = float(j) * 2.0f / float(nseg);
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
  int ns2 = nseg / 2;
  int odd = nseg % 2;
  VMesh *vm = new_adj_vmesh(mem_arena, 3, nseg, nullptr);
  vm->count = 0; /* Reset, so following loop will end up with correct count. */
  for (int i = 0; i < 3; i++) {
    float co[3] = {0.0f, 0.0f, 0.0f};
    co[i] = 1.0f;
    add_new_bound_vert(mem_arena, vm, co);
  }

  float b;
  if (odd) {
    b = 2.0f / (2.0f * float(ns2) + float(M_SQRT2));
  }
  else {
    b = 2.0f / float(nseg);
  }
  for (int i = 0; i < 3; i++) {
    for (int k = 0; k <= ns2; k++) {
      float co[3];
      co[i] = 1.0f - float(k) * b;
      co[(i + 1) % 3] = 0.0f;
      co[(i + 2) % 3] = 0.0f;
      copy_v3_v3(mesh_vert(vm, i, 0, k)->co, co);
      co[(i + 1) % 3] = 1.0f - float(k) * b;
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

  if (bp->profile_type != BEVEL_PROFILE_CUSTOM) {
    if (r == PRO_SQUARE_R) {
      return make_cube_corner_square(mem_arena, nseg);
    }
    if (r == PRO_SQUARE_IN_R) {
      return make_cube_corner_square_in(mem_arena, nseg);
    }
  }

  /* Initial mesh has 3 sides and 2 segments on each side. */
  VMesh *vm0 = new_adj_vmesh(mem_arena, 3, 2, nullptr);
  vm0->count = 0; /* Reset, so the following loop will end up with correct count. */
  for (int i = 0; i < 3; i++) {
    float co[3] = {0.0f, 0.0f, 0.0f};
    co[i] = 1.0f;
    add_new_bound_vert(mem_arena, vm0, co);
  }
  BoundVert *bndv = vm0->boundstart;
  for (int i = 0; i < 3; i++) {
    float coc[3];
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
  float co[3];
  copy_v3_fl(co, float(M_SQRT1_3));

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

  VMesh *vm1 = vm0;
  while (vm1->seg < nseg) {
    vm1 = cubic_subdiv(bp, vm1);
  }
  if (vm1->seg != nseg) {
    vm1 = interp_vmesh(bp, vm1, nseg);
  }

  /* Now snap each vertex to the superellipsoid. */
  int ns2 = nseg / 2;
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j <= ns2; j++) {
      for (int k = 0; k <= nseg; k++) {
        snap_to_superellipsoid(mesh_vert(vm1, i, j, k)->co, r, false);
      }
    }
  }

  return vm1;
}

/* Is this a good candidate for using tri_corner_adj_vmesh? */
static int tri_corner_test(BevelParams *bp, BevVert *bv)
{
  int in_plane_e = 0;

  /* The superellipse snapping of this case isn't helpful with custom profiles enabled. */
  if (bp->affect_type == BEVEL_AFFECT_VERTICES || bp->profile_type == BEVEL_PROFILE_CUSTOM) {
    return -1;
  }
  if (bv->vmesh->count != 3) {
    return 0;
  }

  /* Only use the tri-corner special case if the offset is the same for every edge. */
  float offset = bv->edges[0].offset_l;

  float totang = 0.0f;
  for (int i = 0; i < bv->edgecount; i++) {
    EdgeHalf *e = &bv->edges[i];
    float ang = BM_edge_calc_face_angle_signed_ex(e->e, 0.0f);
    float absang = fabsf(ang);
    if (absang <= M_PI_4) {
      in_plane_e++;
    }
    else if (absang >= 3.0f * float(M_PI_4)) {
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
  float angdiff = fabsf(fabsf(totang) - 3.0f * float(M_PI_2));
  if ((bp->pro_super_r == PRO_SQUARE_R && angdiff > float(M_PI) / 16.0f) ||
      (angdiff > float(M_PI_4)))
  {
    return -1;
  }
  if (bv->edgecount != 3 || bv->selcount != 3) {
    return 0;
  }
  return 1;
}

static VMesh *tri_corner_adj_vmesh(BevelParams *bp, BevVert *bv)
{
  BoundVert *bndv = bv->vmesh->boundstart;

  float co0[3], co1[3], co2[3];
  copy_v3_v3(co0, bndv->nv.co);
  bndv = bndv->next;
  copy_v3_v3(co1, bndv->nv.co);
  bndv = bndv->next;
  copy_v3_v3(co2, bndv->nv.co);

  float mat[4][4];
  make_unit_cube_map(co0, co1, co2, bv->v->co, mat);
  int ns = bp->seg;
  int ns2 = ns / 2;
  VMesh *vm = make_cube_corner_adj_vmesh(bp);
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j <= ns2; j++) {
      for (int k = 0; k <= ns; k++) {
        float v[4];
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
  MemArena *mem_arena = bp->mem_arena;

  int n_bndv = bv->vmesh->count;

  /* Same bevel as that of 3 edges of vert in a cube. */
  if (n_bndv == 3 && tri_corner_test(bp, bv) != -1 && bp->pro_super_r != PRO_SQUARE_IN_R) {
    return tri_corner_adj_vmesh(bp, bv);
  }

  /* First construct an initial control mesh, with nseg == 2. */
  int nseg = bv->vmesh->seg;
  VMesh *vm0 = new_adj_vmesh(mem_arena, n_bndv, 2, bv->vmesh->boundstart);

  /* Find the center of the boundverts that make up the vmesh. */
  BoundVert *bndv = vm0->boundstart;
  float boundverts_center[3] = {0.0f, 0.0f, 0.0f};
  for (int i = 0; i < n_bndv; i++) {
    /* Boundaries just divide input polygon edges into 2 even segments. */
    copy_v3_v3(mesh_vert(vm0, i, 0, 0)->co, bndv->nv.co);
    get_profile_point(bp, &bndv->profile, 1, 2, mesh_vert(vm0, i, 0, 1)->co);
    add_v3_v3(boundverts_center, bndv->nv.co);
    bndv = bndv->next;
  }
  mul_v3_fl(boundverts_center, 1.0f / float(n_bndv));

  /* To place the center vertex:
   * 'negative_fullest' is the reflection of the original vertex across the boundverts' center.
   * 'fullness' is the fraction of the way from the boundvert's centroid to the original vertex
   * (if positive) or to negative_fullest (if negative). */
  float original_vertex[3], negative_fullest[3];
  copy_v3_v3(original_vertex, bv->v->co);
  sub_v3_v3v3(negative_fullest, boundverts_center, original_vertex);
  add_v3_v3(negative_fullest, boundverts_center);

  /* Find the vertex mesh's start center with the profile's fullness. */
  float fullness = bp->pro_spacing.fullness;
  float center_direction[3];
  sub_v3_v3v3(center_direction, original_vertex, boundverts_center);
  if (len_squared_v3(center_direction) > BEVEL_EPSILON_SQ) {
    if (bp->profile_type == BEVEL_PROFILE_CUSTOM) {
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
  VMesh *vm1 = vm0;
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
  Profile *pro = &vpipe->profile;
  EdgeHalf *e = vpipe->ebev;

  if (compare_v3v3(pro->start, pro->end, BEVEL_EPSILON_D)) {
    copy_v3_v3(co, pro->start);
    return;
  }

  /* Get a plane with the normal pointing along the beveled edge. */
  float edir[3], plane[4];
  sub_v3_v3v3(edir, e->e->v1->co, e->e->v2->co);
  plane_from_point_normal_v3(plane, co, edir);

  float start_plane[3], end_plane[3], middle_plane[3];
  closest_to_plane_v3(start_plane, plane, pro->start);
  closest_to_plane_v3(end_plane, plane, pro->end);
  closest_to_plane_v3(middle_plane, plane, pro->middle);

  float m[4][4], minv[4][4];
  if (make_unit_square_map(start_plane, middle_plane, end_plane, m) && invert_m4_m4(minv, m)) {
    /* Transform co and project it onto superellipse. */
    float p[3];
    mul_v3_m4v3(p, minv, co);
    snap_to_superellipsoid(p, pro->super_r, midline);

    float snap[3];
    mul_v3_m4v3(snap, m, p);
    copy_v3_v3(co, snap);
  }
  else {
    /* Planar case: just snap to line start_plane--end_plane. */
    float p[3];
    closest_to_line_segment_v3(p, co, start_plane, end_plane);
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
  /* Some unnecessary overhead running this subdivision with custom profile snapping later on. */
  VMesh *vm = adj_vmesh(bp, bv);

  /* Now snap all interior coordinates to be on the epipe profile. */
  int n_bndv = bv->vmesh->count;
  int ns = bv->vmesh->seg;
  int half_ns = ns / 2;
  int ipipe1 = vpipe->index;
  int ipipe2 = vpipe->next->next->index;

  for (int i = 0; i < n_bndv; i++) {
    for (int j = 1; j <= half_ns; j++) {
      for (int k = 0; k <= half_ns; k++) {
        if (!is_canon(vm, i, j, k)) {
          continue;
        }
        /* With a custom profile just copy the shape of the profile at each ring. */
        if (bp->profile_type == BEVEL_PROFILE_CUSTOM) {
          /* Find both profile vertices that correspond to this point. */
          float *profile_point_pipe1, *profile_point_pipe2, f;
          if (ELEM(i, ipipe1, ipipe2)) {
            if (n_bndv == 3 && i == ipipe1) {
              /* This part of the vmesh is the triangular corner between the two pipe profiles. */
              int ring = max_ii(j, k);
              profile_point_pipe2 = mesh_vert(vm, i, 0, ring)->co;
              profile_point_pipe1 = mesh_vert(vm, i, ring, 0)->co;
              /* End profile index increases with k on one side and j on the other. */
              f = ((k < j) ? min_ff(j, k) : ((2.0f * ring) - j)) / (2.0f * ring);
            }
            else {
              /* This is part of either pipe profile boundvert area in the 4-way intersection. */
              profile_point_pipe1 = mesh_vert(vm, i, 0, k)->co;
              profile_point_pipe2 = mesh_vert(vm, (i == ipipe1) ? ipipe2 : ipipe1, 0, ns - k)->co;
              f = float(j) / float(ns); /* The ring index brings us closer to the other side. */
            }
          }
          else {
            /* The profile vertices are on both ends of each of the side profile's rings. */
            profile_point_pipe1 = mesh_vert(vm, i, j, 0)->co;
            profile_point_pipe2 = mesh_vert(vm, i, j, ns)->co;
            f = float(k) / float(ns); /* Ring runs along the pipe, so segment is used here. */
          }

          /* Place the vertex by interpolating between the two profile points using the factor. */
          interp_v3_v3v3(mesh_vert(vm, i, j, k)->co, profile_point_pipe1, profile_point_pipe2, f);
        }
        else {
          /* A tricky case is for the 'square' profiles and an even nseg: we want certain
           * vertices to snap to the midline on the pipe, not just to one plane or the other. */
          bool even = (ns % 2) == 0;
          bool midline = even && k == half_ns &&
                         ((i == 0 && j == half_ns) || ELEM(i, ipipe1, ipipe2));
          snap_to_pipe_profile(vpipe, midline, mesh_vert(vm, i, j, k)->co);
        }
      }
    }
  }
  return vm;
}

static void get_incident_edges(BMFace *f, BMVert *v, BMEdge **r_e1, BMEdge **r_e2)
{
  *r_e1 = nullptr;
  *r_e2 = nullptr;
  if (!f) {
    return;
  }

  BMIter iter;
  BMEdge *e;
  BM_ITER_ELEM (e, &iter, f, BM_EDGES_OF_FACE) {
    if (e->v1 == v || e->v2 == v) {
      if (*r_e1 == nullptr) {
        *r_e1 = e;
      }
      else if (*r_e2 == nullptr) {
        *r_e2 = e;
      }
    }
  }
}

static BMEdge *find_closer_edge(float *co, BMEdge *e1, BMEdge *e2)
{
  BLI_assert(e1 != nullptr && e2 != nullptr);
  float dsq1 = dist_squared_to_line_segment_v3(co, e1->v1->co, e1->v2->co);
  float dsq2 = dist_squared_to_line_segment_v3(co, e2->v1->co, e2->v2->co);
  if (dsq1 < dsq2) {
    return e1;
  }
  return e2;
}

/**
 * Find which BoundVerts of \a bv are internal to face \a f.
 * That is, when both the face and the point are projected to 2d,
 * the point is on the boundary of or inside the projected face.
 * There can only be up to three of then, since, including miters,
 * that is the maximum number of BoundVerts that can be between two edges.
 * Return the number of face-internal vertices found.
 */
static int find_face_internal_boundverts(const BevVert *bv,
                                         const BMFace *f,
                                         BoundVert *(r_internal[3]))
{
  if (f == nullptr) {
    return 0;
  }
  int n_internal = 0;
  VMesh *vm = bv->vmesh;
  BLI_assert(vm != nullptr);
  BoundVert *v = vm->boundstart;
  do {
    /* Possible speedup: do the matrix projection done by the following
     * once, outside the loop, or even better, cache it if ever done
     * in the course of Bevel. */
    if (BM_face_point_inside_test(f, v->nv.co)) {
      r_internal[n_internal++] = v;
      if (n_internal == 3) {
        break;
      }
    }
  } while ((v = v->next) != vm->boundstart);
  for (int i = n_internal; i < 3; i++) {
    r_internal[i] = nullptr;
  }
  return n_internal;
}

/**
 * Find where the coordinates of the BndVerts in \a bv should snap to in face \a f.
 * Face \a f should contain vertex `bv->v`.
 * Project the snapped verts to 2d, then return the area of the resulting polygon.
 * Usually one BndVert is inside the face, sometimes up to 3 (if there are miters),
 * so don't snap those to an edge; all the rest snap to one of the edges of \a bmf
 * incident on `bv->v`.
 */
static float projected_boundary_area(BevVert *bv, BMFace *f)
{
  BMEdge *e1, *e2;
  VMesh *vm = bv->vmesh;
  float (*proj_co)[2] = BLI_array_alloca(proj_co, vm->count);
  float axis_mat[3][3];
  axis_dominant_v3_to_m3(axis_mat, f->no);
  get_incident_edges(f, bv->v, &e1, &e2);
  BLI_assert(e1 != nullptr && e2 != nullptr);
  BLI_assert(vm != nullptr);
  BoundVert *v = vm->boundstart;
  int i = 0;
  BoundVert *unsnapped[3];
  find_face_internal_boundverts(bv, f, unsnapped);
  do {
    float *co = v->nv.v->co;
    if (ELEM(v, unsnapped[0], unsnapped[1], unsnapped[2])) {
      mul_v2_m3v3(proj_co[i], axis_mat, co);
    }
    else {
      float snap1[3], snap2[3];
      closest_to_line_segment_v3(snap1, co, e1->v1->co, e1->v2->co);
      closest_to_line_segment_v3(snap2, co, e2->v1->co, e2->v2->co);
      float d1_sq = len_squared_v3v3(snap1, co);
      float d2_sq = len_squared_v3v3(snap2, co);
      if (d1_sq <= d2_sq) {
        mul_v2_m3v3(proj_co[i], axis_mat, snap1);
      }
      else {
        mul_v2_m3v3(proj_co[i], axis_mat, snap2);
      }
    }
    ++i;
  } while ((v = v->next) != vm->boundstart);
  float area = area_poly_v2(proj_co, vm->count);
  return area;
}

/**
 * If we make a poly out of verts around \a bv, snapping to rep \a frep,
 * will uv poly have zero area?
 * The uv poly is made by snapping all `outside-of-frep` vertices to the closest edge in \a frep.
 * Sometimes this results in a zero or very small area polygon, which translates to a zero
 * or very small area polygon in UV space -- not good for interpolating textures.
 */
static bool is_bad_uv_poly(BevVert *bv, BMFace *frep)
{
  BLI_assert(bv->vmesh != nullptr);
  float area = projected_boundary_area(bv, frep);
  return area < BEVEL_EPSILON_BIG;
}

/**
 * Pick a good face from all the faces around \a bv to use for
 * a representative face, using choose_rep_face.
 * We want to choose from among the faces that would be
 * chosen for a single-segment edge polygon between two successive
 * Boundverts.
 * But the single beveled edge is a special case,
 * where we also want to consider the third face (else can get
 * zero-area UV interpolated face).
 *
 * If there are math-having custom loop layers, like UV, then
 * don't include faces that would result in zero-area UV polygons
 * if chosen as the rep.
 */
static BMFace *frep_for_center_poly(BevelParams *bp, BevVert *bv)
{
  int fcount = 0;
  BMFace *any_bmf = nullptr;
  bool consider_all_faces = bv->selcount == 1;
  /* Make an array that can hold maximum possible number of choices. */
  BMFace **fchoices = BLI_array_alloca(fchoices, bv->edgecount);
  /* For each choice, need to remember the unsnapped BoundVerts. */

  for (int i = 0; i < bv->edgecount; i++) {
    if (!bv->edges[i].is_bev && !consider_all_faces) {
      continue;
    }
    BMFace *bmf1 = bv->edges[i].fprev;
    BMFace *bmf2 = bv->edges[i].fnext;
    BMFace *ftwo[2] = {bmf1, bmf2};
    BMFace *bmf = choose_rep_face(bp, ftwo, 2);
    if (bmf != nullptr) {
      if (any_bmf == nullptr) {
        any_bmf = bmf;
      }
      bool already_there = false;
      for (int j = fcount - 1; j >= 0; j--) {
        if (fchoices[j] == bmf) {
          already_there = true;
          break;
        }
      }
      if (!already_there) {
        if (bp->math_layer_info.has_math_layers) {
          if (is_bad_uv_poly(bv, bmf)) {
            continue;
          }
        }
        fchoices[fcount++] = bmf;
      }
    }
  }
  if (fcount == 0) {
    return any_bmf;
  }
  return choose_rep_face(bp, fchoices, fcount);
}

static void build_center_ngon(BevelParams *bp, BMesh *bm, BevVert *bv, int mat_nr)
{
  VMesh *vm = bv->vmesh;
  Vector<BMVert *, BM_DEFAULT_NGON_STACK_SIZE> vv;
  Vector<BMFace *, BM_DEFAULT_NGON_STACK_SIZE> vf;
  Vector<BMEdge *, BM_DEFAULT_NGON_STACK_SIZE> ve;

  int ns2 = vm->seg / 2;
  BMFace *frep;
  BMEdge *frep_e1, *frep_e2;
  BoundVert *frep_unsnapped[3];
  if (bv->any_seam) {
    frep = frep_for_center_poly(bp, bv);
    get_incident_edges(frep, bv->v, &frep_e1, &frep_e2);
    find_face_internal_boundverts(bv, frep, frep_unsnapped);
  }
  else {
    frep = nullptr;
    frep_e1 = frep_e2 = nullptr;
  }
  BoundVert *v = vm->boundstart;
  do {
    int i = v->index;
    vv.append(mesh_vert(vm, i, ns2, ns2)->v);
    if (frep) {
      vf.append(frep);
      if (ELEM(v, frep_unsnapped[0], frep_unsnapped[1], frep_unsnapped[2])) {
        ve.append(nullptr);
      }
      else {
        BMEdge *frep_e = find_closer_edge(mesh_vert(vm, i, ns2, ns2)->v->co, frep_e1, frep_e2);
        ve.append(frep_e);
      }
    }
    else {
      vf.append(boundvert_rep_face(v, nullptr));
      ve.append(nullptr);
    }
  } while ((v = v->next) != vm->boundstart);
  BMFace *f = bev_create_ngon(
      bp, bm, vv.data(), vv.size(), vf.data(), frep, ve.data(), bv->v, nullptr, mat_nr, true);
  record_face_kind(bp, f, F_VERT);
}

/**
 * Special case of #bevel_build_rings when triangle-corner and profile is 0.
 * There is no corner mesh except, if nseg odd, for a center poly.
 * Boundary verts merge with previous ones according to pattern:
 * (i, 0, k) merged with (i+1, 0, ns-k) for k <= ns/2.
 */
static void build_square_in_vmesh(BevelParams *bp, BMesh *bm, BevVert *bv, VMesh *vm1)
{
  VMesh *vm = bv->vmesh;
  int n = vm->count;
  int ns = vm->seg;
  int ns2 = ns / 2;
  int odd = ns % 2;

  for (int i = 0; i < n; i++) {
    for (int k = 1; k < ns; k++) {
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
    for (int i = 0; i < n; i++) {
      mesh_vert(vm, i, ns2, ns2)->v = mesh_vert(vm, i, 0, ns2)->v;
    }
    build_center_ngon(bp, bm, bv, bp->mat_nr);
  }
}

/**
 * Copy whichever of \a a and \a b is closer to v into \a r.
 */
static void closer_v3_v3v3v3(float r[3], const float a[3], const float b[3], const float v[3])
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
  int n_bndv = bv->vmesh->count;
  int ns = bv->vmesh->seg;
  int ns2 = ns / 2;
  int odd = ns % 2;
  float ns2inv = 1.0f / float(ns2);
  VMesh *vm = new_adj_vmesh(bp->mem_arena, n_bndv, ns, bv->vmesh->boundstart);
  int clstride = 3 * (ns2 + 1);
  float *centerline = MEM_malloc_arrayN<float>(clstride * n_bndv, "bevel");
  bool *cset = MEM_calloc_arrayN<bool>(n_bndv, "bevel");

  /* Find on_edge, place on bndv[i]'s elast where offset line would meet,
   * taking min-distance-to bv->v with position where next sector's offset line would meet. */
  BoundVert *bndv = vm->boundstart;
  for (int i = 0; i < n_bndv; i++) {
    float bndco[3];
    copy_v3_v3(bndco, bndv->nv.co);
    EdgeHalf *e1 = bndv->efirst;
    EdgeHalf *e2 = bndv->elast;
    AngleKind ang_kind = ANGLE_STRAIGHT;
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
      float dir1[3], dir2[3], co1[3], co2[3];
      sub_v3_v3v3(dir1, e1->e->v1->co, e1->e->v2->co);
      sub_v3_v3v3(dir2, e2->e->v1->co, e2->e->v2->co);
      add_v3_v3v3(co1, bndco, dir1);
      add_v3_v3v3(co2, bndco, dir2);
      /* Intersect e1 with line through bndv parallel to e2 to get v1co. */
      float meet1[3], meet2[3];
      int ikind = isect_line_line_v3(e1->e->v1->co, e1->e->v2->co, bndco, co2, meet1, meet2);
      float v1co[3];
      bool v1set;
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
      float v2co[3];
      bool v2set;
      if (ikind == 0) {
        v2set = false;
      }
      else {
        v2set = true;
        copy_v3_v3(v2co, meet1);
      }

      /* We want on_edge[i] to be min dist to bv->v of v2co and the v1co of next iteration. */
      float *on_edge_cur = centerline + clstride * i;
      int iprev = (i == 0) ? n_bndv - 1 : i - 1;
      float *on_edge_prev = centerline + clstride * iprev;
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
  for (int i = 0; i < n_bndv; i++) {
    if (!cset[i]) {
      float *on_edge_cur = centerline + clstride * i;
      EdgeHalf *e1 = bndv->next->efirst;
      float co1[3], co2[3];
      copy_v3_v3(co1, bndv->nv.co);
      copy_v3_v3(co2, bndv->next->nv.co);
      if (e1) {
        if (bndv->prev->is_arc_start && bndv->next->is_arc_start) {
          float meet1[3], meet2[3];
          int ikind = isect_line_line_v3(e1->e->v1->co, e1->e->v2->co, co1, co2, meet1, meet2);
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

  /* Fill in rest of center-lines by interpolation. */
  float co1[3], co2[3];
  copy_v3_v3(co2, bv->v->co);
  bndv = vm->boundstart;
  for (int i = 0; i < n_bndv; i++) {
    if (odd) {
      float ang = 0.5f * angle_v3v3v3(bndv->nv.co, co1, bndv->next->nv.co);
      float finalfrac;
      if (ang > BEVEL_SMALL_ANG) {
        /* finalfrac is the length along arms of isosceles triangle with top angle 2*ang
         * such that the base of the triangle is 1.
         * This is used in interpolation along center-line in odd case.
         * To avoid too big a drop from bv, cap finalfrac a 0.8 arbitrarily */
        finalfrac = 0.5f / sinf(ang);
        finalfrac = std::min(finalfrac, 0.8f);
      }
      else {
        finalfrac = 0.8f;
      }
      ns2inv = 1.0f / (ns2 + finalfrac);
    }

    float *p = centerline + clstride * i;
    copy_v3_v3(co1, p);
    p += 3;
    for (int j = 1; j <= ns2; j++) {
      interp_v3_v3v3(p, co1, co2, j * ns2inv);
      p += 3;
    }
    bndv = bndv->next;
  }

  /* Coords of edges and mid or near-mid line. */
  bndv = vm->boundstart;
  for (int i = 0; i < n_bndv; i++) {
    copy_v3_v3(co1, bndv->nv.co);
    copy_v3_v3(co2, centerline + clstride * (i == 0 ? n_bndv - 1 : i - 1));
    for (int j = 0; j < ns2 + odd; j++) {
      interp_v3_v3v3(mesh_vert(vm, i, j, 0)->co, co1, co2, j * ns2inv);
    }
    copy_v3_v3(co2, centerline + clstride * i);
    for (int k = 1; k <= ns2; k++) {
      interp_v3_v3v3(mesh_vert(vm, i, 0, k)->co, co1, co2, k * ns2inv);
    }
    bndv = bndv->next;
  }
  if (!odd) {
    copy_v3_v3(mesh_vert(vm, 0, ns2, ns2)->co, bv->v->co);
  }
  vmesh_copy_equiv_verts(vm);

  /* Fill in interior points by interpolation from edges to center-lines. */
  bndv = vm->boundstart;
  for (int i = 0; i < n_bndv; i++) {
    int im1 = (i == 0) ? n_bndv - 1 : i - 1;
    for (int j = 1; j < ns2 + odd; j++) {
      for (int k = 1; k <= ns2; k++) {
        float meet1[3], meet2[3];
        int ikind = isect_line_line_v3(mesh_vert(vm, i, 0, k)->co,
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

static BMEdge *snap_edge_for_center_vmesh_vert(int i,
                                               int n_bndv,
                                               BMEdge *eprev,
                                               BMEdge *enext,
                                               BMFace **bndv_rep_faces,
                                               BMFace *center_frep,
                                               const bool *frep_beats_next)
{
  int previ = (i + n_bndv - 1) % n_bndv;
  int nexti = (i + 1) % n_bndv;

  if (frep_beats_next[previ] && bndv_rep_faces[previ] == center_frep) {
    return eprev;
  }
  if (!frep_beats_next[i] && bndv_rep_faces[nexti] == center_frep) {
    return enext;
  }
  /* If n_bndv > 3 then we won't snap in the boundvert regions
   * that are not directly adjacent to the center-winning boundvert.
   * This is probably wrong, maybe getting UV positions outside the
   * original area, but the alternative may be even worse. */
  return nullptr;
}

/**
 * Fill the r_snap_edges array with the edges to snap to (or NUL, if no snapping)
 * for the adj mesh face with lower left corner at (i, ring j, segment k).
 * The indices of the four corners are (i,j,k), (i,j,k+1), (i,j+1,k+1), (i,j+1,k).
 * The answer will be one of nullptr (don't snap), eprev (the edge between
 * boundvert i and boundvert i-1), or enext (the edge between boundvert i
 * and boundvert i+1).
 * When n is odd, the center column (seg ns2) is ambiguous as to whether it
 * interpolates in the current boundvert's frep [= interpolation face] or the next one's.
 * Similarly, when n is odd, the center row (ring ns2) is ambiguous as to
 * whether it interpolates in the current boundvert's frep or the previous one's.
 * Parameter frep_beats_next should have an array of size n_bndv of booleans
 * that say whether the tie should be broken in favor of the next boundvert's
 * frep (if true) or the current one's.
 * For vertices in the center polygon (when ns is odd), the snapping edge depends
 * on where the boundvert is in relation to the boundvert that has the center face's frep,
 * so the arguments bndv_rep_faces is an array of size n_bndv give the freps for each i,
 * and center_frep is the frep for the center.
 *
 * NOTE: this function is for edge bevels only, at the moment.
 */
static void snap_edges_for_vmesh_vert(int i,
                                      int j,
                                      int k,
                                      int ns,
                                      int ns2,
                                      int n_bndv,
                                      BMEdge *eprev,
                                      BMEdge *enext,
                                      BMEdge *enextnext,
                                      BMFace **bndv_rep_faces,
                                      BMFace *center_frep,
                                      const bool *frep_beats_next,
                                      BMEdge *r_snap_edges[4])
{
  BLI_assert(0 <= i && i < n_bndv && 0 <= j && j < ns2 && 0 <= k && k <= ns2);
  for (int corner = 0; corner < 4; corner++) {
    r_snap_edges[corner] = nullptr;
    if (ns % 2 == 0) {
      continue;
    }
    int previ = (i + n_bndv - 1) % n_bndv;
    /* Make jj and kk be the j and k indices for this corner. */
    int jj = corner < 2 ? j : j + 1;
    int kk = ELEM(corner, 0, 3) ? k : k + 1;
    if (jj < ns2 && kk < ns2) {
      ; /* No snap. */
    }
    else if (jj < ns2 && kk == ns2) {
      /* On the left side of the center strip quads, but not on center poly. */
      if (!frep_beats_next[i]) {
        r_snap_edges[corner] = enext;
      }
    }
    else if (jj < ns2 && kk == ns2 + 1) {
      /* On the right side of the center strip quads, but not on center poly. */
      if (frep_beats_next[i]) {
        r_snap_edges[corner] = enext;
      }
    }
    else if (jj == ns2 && kk < ns2) {
      /* On the top of the top strip quads, but not on center poly. */
      if (frep_beats_next[previ]) {
        r_snap_edges[corner] = eprev;
      }
    }
    else if (jj == ns2 && kk == ns2) {
      /* Center poly vert for boundvert i. */
      r_snap_edges[corner] = snap_edge_for_center_vmesh_vert(
          i, n_bndv, eprev, enext, bndv_rep_faces, center_frep, frep_beats_next);
    }
    else if (jj == ns2 && kk == ns2 + 1) {
      /* Center poly vert for boundvert i+1. */
      int nexti = (i + 1) % n_bndv;
      r_snap_edges[corner] = snap_edge_for_center_vmesh_vert(
          nexti, n_bndv, enext, enextnext, bndv_rep_faces, center_frep, frep_beats_next);
    }
  }
}

/**
 * Given that the boundary is built and the boundary #BMVert's have been made,
 * calculate the positions of the interior mesh points for the M_ADJ pattern,
 * using cubic subdivision, then make the #BMVert's and the new faces.
 */
static void bevel_build_rings(BevelParams *bp, BMesh *bm, BevVert *bv, BoundVert *vpipe)
{
  int mat_nr = bp->mat_nr;

  int n_bndv = bv->vmesh->count;
  int ns = bv->vmesh->seg;
  int ns2 = ns / 2;
  int odd = ns % 2;
  BLI_assert(n_bndv >= 3 && ns > 1);

  VMesh *vm1;
  if (bp->pro_super_r == PRO_SQUARE_R && bv->selcount >= 3 && !odd &&
      bp->profile_type != BEVEL_PROFILE_CUSTOM)
  {
    vm1 = square_out_adj_vmesh(bp, bv);
  }
  else if (vpipe) {
    vm1 = pipe_adj_vmesh(bp, bv, vpipe);
  }
  else if (tri_corner_test(bp, bv) == 1) {
    vm1 = tri_corner_adj_vmesh(bp, bv);
    /* The PRO_SQUARE_IN_R profile has boundary edges that merge
     * and no internal ring polys except possibly center ngon. */
    if (bp->pro_super_r == PRO_SQUARE_IN_R && bp->profile_type != BEVEL_PROFILE_CUSTOM) {
      build_square_in_vmesh(bp, bm, bv, vm1);
      return;
    }
  }
  else {
    vm1 = adj_vmesh(bp, bv);
  }

  /* Copy final vmesh into bv->vmesh, make BMVerts and BMFaces. */
  VMesh *vm = bv->vmesh;
  for (int i = 0; i < n_bndv; i++) {
    for (int j = 0; j <= ns2; j++) {
      for (int k = 0; k <= ns; k++) {
        if (j == 0 && ELEM(k, 0, ns)) {
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

  /* Find and store the interpolation face for each BoundVert. */
  BMFace **bndv_rep_faces = BLI_array_alloca(bndv_rep_faces, n_bndv);
  BoundVert *bndv = vm->boundstart;
  do {
    int i = bndv->index;
    bndv_rep_faces[i] = boundvert_rep_face(bndv, nullptr);
  } while ((bndv = bndv->next) != vm->boundstart);

  /* If odd number of segments, need data to break interpolation ties. */
  BMVert **center_verts = nullptr;
  BMEdge **center_edge_snaps = nullptr;
  BMFace **center_face_interps = nullptr;
  bool *frep_beats_next = nullptr;
  BMFace *center_frep = nullptr;
  if (odd && bp->affect_type == BEVEL_AFFECT_EDGES) {
    center_verts = BLI_array_alloca(center_verts, n_bndv);
    center_edge_snaps = BLI_array_alloca(center_edge_snaps, n_bndv);
    center_face_interps = BLI_array_alloca(center_face_interps, n_bndv);
    frep_beats_next = BLI_array_alloca(frep_beats_next, n_bndv);
    center_frep = frep_for_center_poly(bp, bv);
    for (int i = 0; i < n_bndv; i++) {
      center_edge_snaps[i] = nullptr;
      /* frep_beats_next[i] == true if frep for i is chosen over that for i + 1. */
      int inext = (i + 1) % n_bndv;
      BMFace *fchoices[2] = {bndv_rep_faces[i], bndv_rep_faces[inext]};
      BMFace *fwinner = choose_rep_face(bp, fchoices, 2);
      frep_beats_next[i] = fwinner == bndv_rep_faces[i];
    }
  }
  /* Make the polygons. */
  bndv = vm->boundstart;
  do {
    int i = bndv->index;
    int inext = bndv->next->index;
    BMFace *f = bndv_rep_faces[i];
    BMFace *f2 = bndv_rep_faces[inext];
    BMFace *fc = nullptr;
    if (odd && bp->affect_type == BEVEL_AFFECT_EDGES) {
      fc = frep_beats_next[i] ? f : f2;
    }

    EdgeHalf *e, *eprev, *enext;
    if (bp->affect_type == BEVEL_AFFECT_VERTICES) {
      e = bndv->efirst;
      eprev = bndv->prev->efirst;
      enext = bndv->next->efirst;
    }
    else {
      e = bndv->ebev;
      eprev = bndv->prev->ebev;
      enext = bndv->next->ebev;
    }
    BMEdge *bme = e ? e->e : nullptr;
    BMEdge *bmeprev = eprev ? eprev->e : nullptr;
    BMEdge *bmenext = enext ? enext->e : nullptr;
    /* For odd ns, make polys with lower left corner at (i,j,k) for
     *    j in [0, ns2-1], k in [0, ns2].  And then the center ngon.
     * For even ns,
     *    j in [0, ns2-1], k in [0, ns2-1].
     *
     * Recall: j is ring index, k is segment index.
     */
    for (int j = 0; j < ns2; j++) {
      for (int k = 0; k < ns2 + odd; k++) {
        /* We will create a quad with these four corners. */
        BMVert *bmv1 = mesh_vert(vm, i, j, k)->v;
        BMVert *bmv2 = mesh_vert(vm, i, j, k + 1)->v;
        BMVert *bmv3 = mesh_vert(vm, i, j + 1, k + 1)->v;
        BMVert *bmv4 = mesh_vert(vm, i, j + 1, k)->v;
        BMVert *bmvs[4] = {bmv1, bmv2, bmv3, bmv4};
        BLI_assert(bmv1 && bmv2 && bmv3 && bmv4);
        /* For each created quad, the UVs etc. will be interpolated
         * in potentially a different face for each corner and may need
         * to snap to a particular edge before interpolating.
         * The fr and se arrays will be filled with the interpolation faces
         * and snapping edges for the for corners in the order given
         * in the bmvs array.
         */
        BMFace *fr[4];
        BMEdge *se[4] = {nullptr, nullptr, nullptr, nullptr};
        if (bp->affect_type == BEVEL_AFFECT_VERTICES) {
          fr[0] = fr[1] = fr[2] = fr[3] = f2;
          if (j < k) {
            if (k == ns2 && j == ns2 - 1) {
              se[2] = bndv->next->efirst->e;
              se[3] = bme;
            }
          }
          else if (j == k) {
            /* Only one edge attached to v, since vertex only. */
            se[0] = se[2] = bme;
            if (!e->is_seam) {
              fr[3] = f;
            }
          }
        }
        else { /* Edge bevel. */
          fr[0] = fr[1] = fr[2] = fr[3] = f;
          if (odd) {
            BMEdge *b1 = (eprev && eprev->is_seam) ? bmeprev : nullptr;
            BMEdge *b2 = (e && e->is_seam) ? bme : nullptr;
            BMEdge *b3 = (enext && enext->is_seam) ? bmenext : nullptr;
            snap_edges_for_vmesh_vert(i,
                                      j,
                                      k,
                                      ns,
                                      ns2,
                                      n_bndv,
                                      b1,
                                      b2,
                                      b3,
                                      bndv_rep_faces,
                                      center_frep,
                                      frep_beats_next,
                                      se);
            if (k == ns2) {
              if (!e || e->is_seam) {
                fr[0] = fr[1] = fr[2] = fr[3] = fc;
              }
              else {
                fr[0] = fr[3] = f;
                fr[1] = fr[2] = f2;
              }
              if (j == ns2 - 1) {
                /* Use the 4th vertex of these faces as the ones used for the center polygon. */
                center_verts[i] = bmvs[3];
                center_edge_snaps[i] = se[3];
                center_face_interps[i] = bv->any_seam ? center_frep : f;
              }
            }
          }
          else { /* Edge bevel, Even number of segments. */
            if (k == ns2 - 1) {
              se[1] = bme;
            }
            if (j == ns2 - 1 && bndv->prev->ebev) {
              se[3] = bmeprev;
            }
            se[2] = se[1] != nullptr ? se[1] : se[3];
          }
        }
        BMFace *r_f = bev_create_ngon(
            bp, bm, bmvs, 4, fr, nullptr, se, bv->v, nullptr, mat_nr, true);
        record_face_kind(bp, r_f, F_VERT);
      }
    }
  } while ((bndv = bndv->next) != vm->boundstart);

  /* Center ngon. */
  if (odd) {
    if (bp->affect_type == BEVEL_AFFECT_EDGES) {
      BMFace *frep = nullptr;
      if (bv->any_seam) {
        frep = frep_for_center_poly(bp, bv);
      }
      BMFace *cen_f = bev_create_ngon(bp,
                                      bm,
                                      center_verts,
                                      n_bndv,
                                      center_face_interps,
                                      frep,
                                      center_edge_snaps,
                                      bv->v,
                                      nullptr,
                                      mat_nr,
                                      true);
      record_face_kind(bp, cen_f, F_VERT);
    }
    else {
      build_center_ngon(bp, bm, bv, mat_nr);
    }
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
 *   and last profile point.
 * TODO(Hans): Use repface / edge arrays for UV interpolation properly.
 */
static void bevel_build_cutoff(BevelParams *bp, BMesh *bm, BevVert *bv)
{
#ifdef DEBUG_CUSTOM_PROFILE_CUTOFF
  printf("BEVEL BUILD CUTOFF\n");
#  define F3(v) (v)[0], (v)[1], (v)[2]
#endif
  int n_bndv = bv->vmesh->count;

  /* Find the locations for the corner vertices at the bottom of the cutoff faces. */
  BoundVert *bndv = bv->vmesh->boundstart;
  do {
    int i = bndv->index;

    /* Find the "down" direction for this side of the cutoff face. */
    /* Find the direction along the intersection of the two adjacent profile normals. */
    float down_direction[3];
    cross_v3_v3v3(down_direction, bndv->profile.plane_no, bndv->prev->profile.plane_no);
    if (dot_v3v3(down_direction, bv->v->no) > 0.0f) {
      negate_v3(down_direction);
    }

    /* Move down from the boundvert by average profile height from the two adjacent profiles. */
    float length = (bndv->profile.height / sqrtf(2.0f) +
                    bndv->prev->profile.height / sqrtf(2.0f)) /
                   2;
    float new_vert[3];
    madd_v3_v3v3fl(new_vert, bndv->nv.co, down_direction, length);

    /* Use this location for this profile's first corner vert and the last profile's second. */
    copy_v3_v3(mesh_vert(bv->vmesh, i, 1, 0)->co, new_vert);
    copy_v3_v3(mesh_vert(bv->vmesh, bndv->prev->index, 1, 1)->co, new_vert);

  } while ((bndv = bndv->next) != bv->vmesh->boundstart);

#ifdef DEBUG_CUSTOM_PROFILE_CUTOFF
  printf("Corner vertices:\n");
  for (int j = 0; j < n_bndv; j++) {
    printf("  (%.3f, %.3f, %.3f)\n", F3(mesh_vert(bv->vmesh, j, 1, 0)->co));
  }
#endif

  /* Disable the center face if the corner vertices share the same location. */
  bool build_center_face = true;
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
      int i = bndv->index;
      create_mesh_bmvert(bm, bv->vmesh, i, 1, 0, bv->v);
      /* The second corner vertex for the previous profile shares this BMVert. */
      mesh_vert(bv->vmesh, bndv->prev->index, 1, 1)->v = mesh_vert(bv->vmesh, i, 1, 0)->v;

    } while ((bndv = bndv->next) != bv->vmesh->boundstart);
  }
  else {
    /* Use the same BMVert for all of the corner vertices. */
    create_mesh_bmvert(bm, bv->vmesh, 0, 1, 0, bv->v);
    for (int i = 1; i < n_bndv; i++) {
      mesh_vert(bv->vmesh, i, 1, 0)->v = mesh_vert(bv->vmesh, 0, 1, 0)->v;
    }
  }

/* Build the profile cutoff faces. */
/* Extra one or two for corner vertices and one for last point along profile, or the size of the
 * center face array if it's bigger. */
#ifdef DEBUG_CUSTOM_PROFILE_CUTOFF
  printf("Building profile cutoff faces.\n");
#endif
  BMVert **face_bmverts = static_cast<BMVert **>(BLI_memarena_alloc(
      bp->mem_arena, sizeof(BMVert *) * max_ii(bp->seg + 2 + build_center_face, n_bndv)));
  bndv = bv->vmesh->boundstart;
  do {
    int i = bndv->index;

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
    // repface = boundvert_rep_face(bndv, nullptr);
    bev_create_ngon(bp,
                    bm,
                    face_bmverts,
                    bp->seg + 2 + build_center_face,
                    nullptr,
                    nullptr,
                    nullptr,
                    bv->v,
                    nullptr,
                    bp->mat_nr,
                    true);
  } while ((bndv = bndv->next) != bv->vmesh->boundstart);

  /* Create the bottom face if it should be built, reusing previous face_bmverts allocation. */
  if (build_center_face) {
    /* Add all of the corner vertices to this face. */
    for (int i = 0; i < n_bndv; i++) {
      /* Add verts from each cutoff face. */
      face_bmverts[i] = mesh_vert(bv->vmesh, i, 1, 0)->v;
    }

    bev_create_ngon(
        bp, bm, face_bmverts, n_bndv, nullptr, nullptr, nullptr, bv->v, nullptr, bp->mat_nr, true);
  }
}

static BMFace *bevel_build_poly(BevelParams *bp, BMesh *bm, BevVert *bv)
{
  VMesh *vm = bv->vmesh;
  Vector<BMVert *, BM_DEFAULT_NGON_STACK_SIZE> bmverts;
  Vector<BMEdge *, BM_DEFAULT_NGON_STACK_SIZE> bmedges;
  Vector<BMFace *, BM_DEFAULT_NGON_STACK_SIZE> bmfaces;

  BMFace *repface;
  BMEdge *repface_e1, *repface_e2;
  BoundVert *unsnapped[3];
  if (bv->any_seam) {
    repface = frep_for_center_poly(bp, bv);
    get_incident_edges(repface, bv->v, &repface_e1, &repface_e2);
    find_face_internal_boundverts(bv, repface, unsnapped);
  }
  else {
    repface = nullptr;
    repface_e1 = repface_e2 = nullptr;
  }
  BoundVert *bndv = vm->boundstart;
  int n = 0;
  do {
    /* Accumulate vertices for vertex ngon. */
    /* Also accumulate faces in which uv interpolation is to happen for each. */
    bmverts.append(bndv->nv.v);
    if (repface) {
      bmfaces.append(repface);
      if (ELEM(bndv, unsnapped[0], unsnapped[1], unsnapped[2])) {
        bmedges.append(nullptr);
      }
      else {
        BMEdge *frep_e = find_closer_edge(bndv->nv.v->co, repface_e1, repface_e2);
        bmedges.append(frep_e);
      }
    }
    else {
      bmfaces.append(boundvert_rep_face(bndv, nullptr));
      bmedges.append(nullptr);
    }
    n++;
    if (bndv->ebev && bndv->ebev->seg > 1) {
      for (int k = 1; k < bndv->ebev->seg; k++) {
        bmverts.append(mesh_vert(vm, bndv->index, 0, k)->v);
        if (repface) {
          bmfaces.append(repface);
          BMEdge *frep_e = find_closer_edge(
              mesh_vert(vm, bndv->index, 0, k)->v->co, repface_e1, repface_e2);
          bmedges.append(k < bndv->ebev->seg / 2 ? nullptr : frep_e);
        }
        else {
          bmfaces.append(boundvert_rep_face(bndv, nullptr));
          bmedges.append(nullptr);
        }
        n++;
      }
    }
  } while ((bndv = bndv->next) != vm->boundstart);

  BMFace *f;
  if (n > 2) {
    f = bev_create_ngon(bp,
                        bm,
                        bmverts.data(),
                        n,
                        bmfaces.data(),
                        repface,
                        bmedges.data(),
                        bv->v,
                        nullptr,
                        bp->mat_nr,
                        true);
    record_face_kind(bp, f, F_VERT);
  }
  else {
    f = nullptr;
  }
  return f;
}

static void bevel_build_trifan(BevelParams *bp, BMesh *bm, BevVert *bv)
{
  BLI_assert(next_bev(bv, nullptr)->seg == 1 || bv->selcount == 1);

  BMFace *f = bevel_build_poly(bp, bm, bv);

  if (f == nullptr) {
    return;
  }

  /* We have a polygon which we know starts at the previous vertex, make it into a fan. */
  BMLoop *l_fan = BM_FACE_FIRST_LOOP(f)->prev;
  BMVert *v_fan = l_fan->v;

  while (f->len > 3) {
    BMLoop *l_new;
    BMFace *f_new;
    BLI_assert(v_fan == l_fan->v);
    f_new = BM_face_split(bm, f, l_fan, l_fan->next->next, &l_new, nullptr, false);
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

/* Special case: vertex bevel with only two boundary verts.
 * Want to make a curved edge if seg > 0.
 * If there are no faces in the original mesh at the original vertex,
 * there will be no rebuilt face to make the edge between the boundary verts,
 * we have to make it here. */
static void bevel_vert_two_edges(BevelParams *bp, BMesh *bm, BevVert *bv)
{
  VMesh *vm = bv->vmesh;

  BLI_assert(vm->count == 2 && bp->affect_type == BEVEL_AFFECT_VERTICES);

  BMVert *v1 = mesh_vert(vm, 0, 0, 0)->v;
  BMVert *v2 = mesh_vert(vm, 1, 0, 0)->v;

  int ns = vm->seg;
  if (ns > 1) {
    /* Set up profile parameters. */
    BoundVert *bndv = vm->boundstart;
    Profile *pro = &bndv->profile;
    pro->super_r = bp->pro_super_r;
    copy_v3_v3(pro->start, v1->co);
    copy_v3_v3(pro->end, v2->co);
    copy_v3_v3(pro->middle, bv->v->co);
    /* Don't use projection. */
    zero_v3(pro->plane_co);
    zero_v3(pro->plane_no);
    zero_v3(pro->proj_dir);

    for (int k = 1; k < ns; k++) {
      float co[3];
      get_profile_point(bp, pro, k, ns, co);
      copy_v3_v3(mesh_vert(vm, 0, 0, k)->co, co);
      create_mesh_bmvert(bm, vm, 0, 0, k, bv->v);
    }
    copy_v3_v3(mesh_vert(vm, 0, 0, ns)->co, v2->co);
    for (int k = 1; k < ns; k++) {
      copy_mesh_vert(vm, 1, 0, ns - k, 0, 0, k);
    }
  }

  if (BM_vert_face_check(bv->v) == false) {
    BMEdge *e_eg = bv->edges[0].e;
    BLI_assert(e_eg != nullptr);
    for (int k = 0; k < ns; k++) {
      v1 = mesh_vert(vm, 0, 0, k)->v;
      v2 = mesh_vert(vm, 0, 0, k + 1)->v;
      BLI_assert(v1 != nullptr && v2 != nullptr);
      BMEdge *bme = BM_edge_create(bm, v1, v2, e_eg, BM_CREATE_NO_DOUBLE);
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
  float co[3];

  int n = vm->count;
  int ns = vm->seg;
  int ns2 = ns / 2;

  vm->mesh = (NewVert *)BLI_memarena_alloc(bp->mem_arena,
                                           sizeof(NewVert) * n * (ns2 + 1) * (ns + 1));

  /* Special case: just two beveled edges welded together. */
  const bool weld = (bv->selcount == 2) && (vm->count == 2);
  BoundVert *weld1 = nullptr; /* Will hold two BoundVerts involved in weld. */
  BoundVert *weld2 = nullptr;

  /* Make (i, 0, 0) mesh verts for all i boundverts. */
  BoundVert *bndv = vm->boundstart;
  do {
    int i = bndv->index;
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
    int i = bndv->index;
    /* bndv's last vert along the boundary arc is the first of the next BoundVert's arc. */
    copy_mesh_vert(vm, i, 0, ns, bndv->next->index, 0, 0);

    if (vm->mesh_kind != M_ADJ) {
      for (int k = 1; k < ns; k++) {
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
    for (int k = 1; k < ns; k++) {
      float *v_weld1 = mesh_vert(bv->vmesh, weld1->index, 0, k)->co;
      float *v_weld2 = mesh_vert(bv->vmesh, weld2->index, 0, ns - k)->co;
      if (bp->profile_type == BEVEL_PROFILE_CUSTOM) {
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
    for (int k = 1; k < ns; k++) {
      copy_mesh_vert(bv->vmesh, weld2->index, 0, ns - k, weld1->index, 0, k);
    }
  }

  /* Make sure the pipe case ADJ mesh is used for both the "Grid Fill" (ADJ) and cutoff options. */
  BoundVert *vpipe = nullptr;
  if (ELEM(vm->count, 3, 4) && bp->seg > 1) {
    /* Result is passed to bevel_build_rings to avoid overhead. */
    vpipe = pipe_test(bv);
    if (vpipe) {
      vm->mesh_kind = M_ADJ;
    }
  }

  switch (vm->mesh_kind) {
    case M_NONE:
      if (n == 2 && bp->affect_type == BEVEL_AFFECT_VERTICES) {
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
    return float(M_PI) - angle_normalized_v3v3(e->fprev->no, e->fnext->no);
  }
  return 0.0f;
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
  Vector<BMEdge *, 4> sucs; /* Likely very few faces attached to same edge. */
  Vector<BMEdge *, BM_DEFAULT_NGON_STACK_SIZE> save_path;

  /* Fill sucs with all unmarked edges of bmesh. */
  BMEdge *bme = bv->edges[i].e;
  BMIter iter;
  BMLoop *l;
  BM_ITER_ELEM (l, &iter, bme, BM_LOOPS_OF_EDGE) {
    BMEdge *bme2 = (l->v == bv->v) ? l->prev->e : l->next->e;
    if (!BM_BEVEL_EDGE_TAG_TEST(bme2)) {
      sucs.append(bme2);
    }
  }
  const int64_t nsucs = sucs.size();

  int bestj = i;
  int j = i;
  for (int sucindex = 0; sucindex < nsucs; sucindex++) {
    BMEdge *nextbme = sucs[sucindex];
    BLI_assert(nextbme != nullptr);
    BLI_assert(!BM_BEVEL_EDGE_TAG_TEST(nextbme));
    BLI_assert(j + 1 < bv->edgecount);
    bv->edges[j + 1].e = nextbme;
    BM_BEVEL_EDGE_TAG_ENABLE(nextbme);
    int tryj = bevel_edge_order_extend(bm, bv, j + 1);
    if (tryj > bestj ||
        (tryj == bestj && edges_face_connected_at_vert(bv->edges[tryj].e, bv->edges[0].e)))
    {
      bestj = tryj;
      save_path.clear();
      for (int k = j + 1; k <= bestj; k++) {
        save_path.append(bv->edges[k].e);
      }
    }
    /* Now reset to path only-going-to-j state. */
    for (int k = j + 1; k <= tryj; k++) {
      BM_BEVEL_EDGE_TAG_DISABLE(bv->edges[k].e);
      bv->edges[k].e = nullptr;
    }
  }
  /* At this point we should be back at invariant on entrance: path up to j. */
  if (bestj > j) {
    /* Save_path should have from j + 1 to bestj inclusive.
     * Edges to add to edges[] before returning. */
    for (int k = j + 1; k <= bestj; k++) {
      BLI_assert(save_path[k - (j + 1)] != nullptr);
      bv->edges[k].e = save_path[k - (j + 1)];
      BM_BEVEL_EDGE_TAG_ENABLE(bv->edges[k].e);
    }
  }
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
  for (int j = 1; j < bv->edgecount; j++) {
    BMEdge *bme = bv->edges[j - 1].e;
    BMEdge *bmenext = nullptr;
    int nsucs = 0;
    BMIter iter;
    BMLoop *l;
    BM_ITER_ELEM (l, &iter, bme, BM_LOOPS_OF_EDGE) {
      BMEdge *bme2 = (l->v == bv->v) ? l->prev->e : l->next->e;
      if (!BM_BEVEL_EDGE_TAG_TEST(bme2)) {
        nsucs++;
        if (bmenext == nullptr) {
          bmenext = bme2;
        }
      }
    }
    if (nsucs == 0 || (nsucs == 2 && j != 1) || nsucs > 2 ||
        (j == bv->edgecount - 1 && !edges_face_connected_at_vert(bmenext, bv->edges[0].e)))
    {
      for (int k = 1; k < j; k++) {
        BM_BEVEL_EDGE_TAG_DISABLE(bv->edges[k].e);
        bv->edges[k].e = nullptr;
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
  int ntot = bv->edgecount;

  /* Add edges to bv->edges in order that keeps adjacent edges sharing
   * a unique face, if possible. */
  EdgeHalf *e = &bv->edges[0];
  BMEdge *bme = e->e;
  if (!bme->l) {
    return false;
  }

  for (int i = 1; i < ntot; i++) {
    /* Find an unflagged edge bme2 that shares a face f with previous bme. */
    int num_shared_face = 0;
    BMEdge *first_suc = nullptr; /* Keep track of first successor to match legacy behavior. */
    BMIter iter;
    BMEdge *bme2;
    BM_ITER_ELEM (bme2, &iter, bv->v, BM_EDGES_OF_VERT) {
      if (BM_BEVEL_EDGE_TAG_TEST(bme2)) {
        continue;
      }

      BMIter iter2;
      BMFace *f;
      BM_ITER_ELEM (f, &iter2, bme2, BM_FACES_OF_EDGE) {
        if (BM_face_edge_share_loop(f, bme)) {
          num_shared_face++;
          if (first_suc == nullptr) {
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
      for (int k = 1; k < i; k++) {
        BM_BEVEL_EDGE_TAG_DISABLE(bv->edges[k].e);
        bv->edges[k].e = nullptr;
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
  int ntot = bv->edgecount;
  for (int i = 0;;) {
    BLI_assert(first_bme != nullptr);
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
    first_bme = nullptr;
    BMIter iter;
    BMEdge *bme;
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
  for (int i = 0; i < ntot; i++) {
    EdgeHalf *e = &bv->edges[i];
    EdgeHalf *e2 = (i == bv->edgecount - 1) ? &bv->edges[0] : &bv->edges[i + 1];
    BMEdge *bme = e->e;
    BMEdge *bme2 = e2->e;
    BLI_assert(bme != nullptr);
    if (e->fnext != nullptr || e2->fprev != nullptr) {
      continue;
    }
    /* Which faces have successive loops that are for bme and bme2?
     * There could be more than one. E.g., in manifold ntot==2 case.
     * Prefer one that has loop in same direction as e. */
    BMFace *bestf = nullptr;
    BMIter iter;
    BMLoop *l;
    BM_ITER_ELEM (l, &iter, bme, BM_LOOPS_OF_EDGE) {
      BMFace *f = l->f;
      if (l->prev->e == bme2 || l->next->e == bme2) {
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
  /* Gather input selected edges.
   * Only bevel selected edges that have exactly two incident faces.
   * Want edges to be ordered so that they share faces.
   * There may be one or more chains of shared faces broken by
   * gaps where there are no faces.
   * Want to ignore wire edges completely for edge beveling.
   * TODO: make following work when more than one gap. */

  int nsel = 0;
  int tot_edges = 0;
  int tot_wire = 0;
  BMEdge *first_bme = nullptr;
  BMIter iter;
  BMEdge *bme;
  BM_ITER_ELEM (bme, &iter, v, BM_EDGES_OF_VERT) {
    int face_count = BM_edge_face_count(bme);
    BM_BEVEL_EDGE_TAG_DISABLE(bme);
    if (BM_elem_flag_test(bme, BM_ELEM_TAG) && bp->affect_type != BEVEL_AFFECT_VERTICES) {
      BLI_assert(face_count == 2);
      nsel++;
      if (!first_bme) {
        first_bme = bme;
      }
    }
    if (face_count == 1) {
      /* Good to start face chain from this edge. */
      first_bme = bme;
    }
    if (face_count > 0 || bp->affect_type == BEVEL_AFFECT_VERTICES) {
      tot_edges++;
    }
    if (BM_edge_is_wire(bme)) {
      tot_wire++;
      /* If edge beveling, exclude wire edges from edges array.
       * Mark this edge as "chosen" so loop below won't choose it. */
      if (bp->affect_type != BEVEL_AFFECT_VERTICES) {
        BM_BEVEL_EDGE_TAG_ENABLE(bme);
      }
    }
  }
  if (!first_bme) {
    first_bme = v->e;
  }

  if ((nsel == 0 && bp->affect_type != BEVEL_AFFECT_VERTICES) ||
      (tot_edges < 2 && bp->affect_type == BEVEL_AFFECT_VERTICES))
  {
    /* Signal this vert isn't being beveled. */
    BM_elem_flag_disable(v, BM_ELEM_TAG);
    return nullptr;
  }

  BevVert *bv = (BevVert *)BLI_memarena_alloc(bp->mem_arena, sizeof(BevVert));
  bv->v = v;
  bv->edgecount = tot_edges;
  bv->selcount = nsel;
  bv->wirecount = tot_wire;
  bv->offset = bp->offset;
  bv->edges = (EdgeHalf *)BLI_memarena_alloc(bp->mem_arena, sizeof(EdgeHalf) * tot_edges);
  if (tot_wire) {
    bv->wire_edges = (BMEdge **)BLI_memarena_alloc(bp->mem_arena, sizeof(BMEdge *) * tot_wire);
  }
  else {
    bv->wire_edges = nullptr;
  }
  bv->vmesh = (VMesh *)BLI_memarena_alloc(bp->mem_arena, sizeof(VMesh));
  bv->vmesh->seg = bp->seg;

  BLI_ghash_insert(bp->vert_hash, v, bv);

  find_bevel_edge_order(bm, bv, first_bme);

  /* Fill in other attributes of EdgeHalfs. */
  for (int i = 0; i < tot_edges; i++) {
    EdgeHalf *e = &bv->edges[i];
    bme = e->e;
    if (BM_elem_flag_test(bme, BM_ELEM_TAG) && bp->affect_type != BEVEL_AFFECT_VERTICES) {
      e->is_bev = true;
      e->seg = bp->seg;
    }
    else {
      e->is_bev = false;
      e->seg = 0;
    }
    e->is_rev = (bme->v2 == v);
    e->leftv = e->rightv = nullptr;
    e->profile_index = 0;
  }

  /* Now done with tag flag. */
  BM_ITER_ELEM (bme, &iter, v, BM_EDGES_OF_VERT) {
    BM_BEVEL_EDGE_TAG_DISABLE(bme);
  }

  /* If edge array doesn't go CCW around vertex from average normal side,
   * reverse the array, being careful to reverse face pointers too. */
  if (tot_edges > 1) {
    int ccw_test_sum = 0;
    for (int i = 0; i < tot_edges; i++) {
      ccw_test_sum += bev_ccw_test(
          bv->edges[i].e, bv->edges[(i + 1) % tot_edges].e, bv->edges[i].fnext);
    }
    if (ccw_test_sum < 0) {
      for (int i = 0; i <= (tot_edges / 2) - 1; i++) {
        std::swap(bv->edges[i], bv->edges[tot_edges - i - 1]);
        std::swap(bv->edges[i].fprev, bv->edges[i].fnext);
        std::swap(bv->edges[tot_edges - i - 1].fprev, bv->edges[tot_edges - i - 1].fnext);
      }
      if (tot_edges % 2 == 1) {
        int i = tot_edges / 2;
        std::swap(bv->edges[i].fprev, bv->edges[i].fnext);
      }
    }
  }

  float weight;
  float vert_axis[3] = {0, 0, 0};
  if (bp->affect_type == BEVEL_AFFECT_VERTICES) {
    /* Modify the offset by the vertex group or bevel weight if they are specified. */
    if (bp->dvert != nullptr && bp->vertex_group != -1) {
      weight = BKE_defvert_find_weight(bp->dvert + BM_elem_index_get(v), bp->vertex_group);
      bv->offset *= weight;
    }
    else if (bp->use_weights) {
      weight = bp->bweight_offset_vert == -1 ? 0.0f :
                                               BM_ELEM_CD_GET_FLOAT(v, bp->bweight_offset_vert);
      bv->offset *= weight;
    }
    /* Find center axis. NOTE: Don't use vert normal, can give unwanted results. */
    if (ELEM(bp->offset_type, BEVEL_AMT_WIDTH, BEVEL_AMT_DEPTH)) {
      float edge_dir[3];
      EdgeHalf *e = bv->edges;
      for (int i = 0; i < tot_edges; i++, e++) {
        BMVert *v2 = BM_edge_other_vert(e->e, bv->v);
        sub_v3_v3v3(edge_dir, bv->v->co, v2->co);
        normalize_v3(edge_dir);
        add_v3_v3v3(vert_axis, vert_axis, edge_dir);
      }
    }
  }

  /* Set offsets for each beveled edge. */
  EdgeHalf *e = bv->edges;
  for (int i = 0; i < tot_edges; i++, e++) {
    e->next = &bv->edges[(i + 1) % tot_edges];
    e->prev = &bv->edges[(i + tot_edges - 1) % tot_edges];

    if (e->is_bev) {
      /* Convert distance as specified by user into offsets along
       * faces on the left side and right sides of this edgehalf.
       * Except for percent method, offset will be same on each side. */

      switch (bp->offset_type) {
        case BEVEL_AMT_OFFSET: {
          e->offset_l_spec = bp->offset;
          break;
        }
        case BEVEL_AMT_WIDTH: {
          float z = fabsf(2.0f * sinf(edge_face_angle(e) / 2.0f));
          if (z < BEVEL_EPSILON) {
            e->offset_l_spec = 0.01f * bp->offset; /* Undefined behavior, so tiny bevel. */
          }
          else {
            e->offset_l_spec = bp->offset / z;
          }
          break;
        }
        case BEVEL_AMT_DEPTH: {
          float z = fabsf(cosf(edge_face_angle(e) / 2.0f));
          if (z < BEVEL_EPSILON) {
            e->offset_l_spec = 0.01f * bp->offset; /* Undefined behavior, so tiny bevel. */
          }
          else {
            e->offset_l_spec = bp->offset / z;
          }
          break;
        }
        case BEVEL_AMT_PERCENT: {
          /* Offset needs to meet adjacent edges at percentage of their lengths.
           * Since the width isn't constant, we don't store a width at all, but
           * rather the distance along the adjacent edge that we need to go
           * at this end of the edge.
           */

          e->offset_l_spec = BM_edge_calc_length(e->prev->e) * bp->offset / 100.0f;
          e->offset_r_spec = BM_edge_calc_length(e->next->e) * bp->offset / 100.0f;

          break;
        }
        case BEVEL_AMT_ABSOLUTE: {
          /* Like Percent, but the amount gives the absolute distance along adjacent edges. */
          e->offset_l_spec = bp->offset;
          e->offset_r_spec = bp->offset;
          break;
        }
        default: {
          BLI_assert_msg(0, "bad bevel offset kind");
          e->offset_l_spec = bp->offset;
          break;
        }
      }
      if (!ELEM(bp->offset_type, BEVEL_AMT_PERCENT, BEVEL_AMT_ABSOLUTE)) {
        e->offset_r_spec = e->offset_l_spec;
      }
      if (bp->use_weights) {
        weight = bp->bweight_offset_edge == -1 ?
                     0.0f :
                     BM_ELEM_CD_GET_FLOAT(e->e, bp->bweight_offset_edge);
        e->offset_l_spec *= weight;
        e->offset_r_spec *= weight;
      }
    }
    else if (bp->affect_type == BEVEL_AFFECT_VERTICES) {
      /* Weight has already been applied to bv->offset, if present.
       * Transfer to e->offset_[lr]_spec according to offset_type. */
      float edge_dir[3];
      switch (bp->offset_type) {
        case BEVEL_AMT_OFFSET: {
          e->offset_l_spec = bv->offset;
          break;
        }
        case BEVEL_AMT_WIDTH: {
          BMVert *v2 = BM_edge_other_vert(e->e, bv->v);
          sub_v3_v3v3(edge_dir, bv->v->co, v2->co);
          float z = fabsf(2.0f * sinf(angle_v3v3(vert_axis, edge_dir)));
          if (z < BEVEL_EPSILON) {
            e->offset_l_spec = 0.01f * bp->offset; /* Undefined behavior, so tiny bevel. */
          }
          else {
            e->offset_l_spec = bp->offset / z;
          }
          break;
        }
        case BEVEL_AMT_DEPTH: {
          BMVert *v2 = BM_edge_other_vert(e->e, bv->v);
          sub_v3_v3v3(edge_dir, bv->v->co, v2->co);
          float z = fabsf(cosf(angle_v3v3(vert_axis, edge_dir)));
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

  /* Collect wire edges if we found any earlier. */
  if (tot_wire != 0) {
    int i = 0;
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
  bool do_rebuild = false;
  Vector<BMVert *, BM_DEFAULT_NGON_STACK_SIZE> vv;
  Vector<BMEdge *, BM_DEFAULT_NGON_STACK_SIZE> ee;
  Map<BMVert *, BMVert *> nv_bv_map; /* New vertex to the (original) bevel vertex mapping. */

  BMIter liter;
  BMLoop *l;
  BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
    if (BM_elem_flag_test(l->v, BM_ELEM_TAG)) {
      BMLoop *lprev = l->prev;
      BevVert *bv = find_bevvert(bp, l->v);
      VMesh *vm = bv->vmesh;
      EdgeHalf *e = find_edge_half(bv, l->e);
      BLI_assert(e != nullptr);
      BMEdge *bme = e->e;
      EdgeHalf *eprev = find_edge_half(bv, lprev->e);
      BLI_assert(eprev != nullptr);

      /* Which direction around our vertex do we travel to match orientation of f? */
      bool go_ccw;
      if (e->prev == eprev) {
        if (eprev->prev == e) {
          /* Valence 2 vertex: use f is one of e->fnext or e->fprev to break tie. */
          go_ccw = (e->fnext != f);
        }
        else {
          go_ccw = true; /* Going CCW around bv to trace this corner. */
        }
      }
      else if (eprev->prev == e) {
        go_ccw = false; /* Going cw around bv to trace this corner. */
      }
      else {
        /* Edges in face are non-contiguous in our ordering around bv.
         * Which way should we go when going from eprev to e? */
        if (count_ccw_edges_between(eprev, e) < count_ccw_edges_between(e, eprev)) {
          /* Go counter-clockwise from eprev to e. */
          go_ccw = true;
        }
        else {
          /* Go clockwise from eprev to e. */
          go_ccw = false;
        }
      }
      bool on_profile_start = false;
      BoundVert *vstart;
      BoundVert *vend;
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
      BLI_assert(vstart != nullptr && vend != nullptr);
      BoundVert *v = vstart;
      if (!on_profile_start) {
        vv.append(v->nv.v);
        ee.append(bme);
        nv_bv_map.add(v->nv.v, l->v);
      }
      while (v != vend) {
        if (go_ccw) {
          int i = v->index;
          int kstart, kend;
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
          for (int k = kstart; k <= kend; k++) {
            BMVert *bmv = mesh_vert(vm, i, 0, k)->v;
            if (bmv) {
              vv.append(bmv);
              ee.append(bme); /* TODO: Maybe better edge here. */
              nv_bv_map.add(bmv, l->v);
            }
          }
          v = v->next;
        }
        else {
          /* Going cw. */
          int i = v->prev->index;
          int kstart, kend;
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
          for (int k = kstart; k >= kend; k--) {
            BMVert *bmv = mesh_vert(vm, i, 0, k)->v;
            if (bmv) {
              vv.append(bmv);
              ee.append(bme);
              nv_bv_map.add(bmv, l->v);
            }
          }
          v = v->prev;
        }
      }
      do_rebuild = true;
    }
    else {
      vv.append(l->v);
      ee.append(l->e);
      nv_bv_map.add(l->v, l->v);  // We keep the old vertex, i.e. mapping to itself.
    }
  }
  if (do_rebuild) {
    const int64_t n = vv.size();
    BMFace *f_new = bev_create_ngon(
        bp, bm, vv.data(), n, nullptr, f, nullptr, nullptr, &nv_bv_map, -1, true);

    /* Copy attributes from old edges. */
    BLI_assert(n == ee.size());
    BMEdge *bme_prev = ee[n - 1];
    for (int k = 0; k < n; k++) {
      BMEdge *bme_new = BM_edge_exists(vv[k], vv[(k + 1) % n]);
      BLI_assert(ee[k] && bme_new);
      if (ee[k] != bme_new) {
        BM_elem_attrs_copy(bm, ee[k], bme_new);
        /* Want to undo seam and smooth for corner segments
         * if those attrs aren't contiguous around face. */
        if (k < n - 1 && ee[k] == ee[k + 1]) {
          if (BM_elem_flag_test(ee[k], BM_ELEM_SEAM) && !BM_elem_flag_test(bme_prev, BM_ELEM_SEAM))
          {
            BM_elem_flag_disable(bme_new, BM_ELEM_SEAM);
          }
          /* Actually want "sharp" to be contiguous, so reverse the test. */
          if (!BM_elem_flag_test(ee[k], BM_ELEM_SMOOTH) &&
              BM_elem_flag_test(bme_prev, BM_ELEM_SMOOTH))
          {
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
      BMIter eiter;
      BMEdge *bme;
      BM_ITER_ELEM (bme, &eiter, f_new, BM_EDGES_OF_FACE) {
        bool keep = false;
        BMIter fiter;
        BMFace *f_other;
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

  return do_rebuild;
}

/* All polygons touching `v` need rebuilding because beveling `v` has made new vertices. */
static void bevel_rebuild_existing_polygons(BMesh *bm,
                                            BevelParams *bp,
                                            BMVert *v,
                                            Set<BMFace *> &rebuilt_orig_faces)
{
  BMIter iter;
  BMFace *f;
  BM_ITER_ELEM (f, &iter, v, BM_FACES_OF_VERT) {
    /* Deletion of original mesh faces that are being rebuild is deferred thus we have to perform
     * a check against `rebuilt_orig_faces` container - previous calls to
     * `bevel_rebuild_existing_polygons` could have already rebuilt faces touching vertex `v`. */
    if (!rebuilt_orig_faces.contains(f)) {
      if (bev_rebuild_polygon(bm, bp, f)) {
        rebuilt_orig_faces.add(f);
      }
    }
  }
}

/* If there were any wire edges, they need to be reattached somewhere. */
static void bevel_reattach_wires(BMesh *bm, BevelParams *bp, BMVert *v)
{
  BevVert *bv = find_bevvert(bp, v);
  if (!bv || bv->wirecount == 0 || !bv->vmesh) {
    return;
  }

  for (int i = 0; i < bv->wirecount; i++) {
    BMEdge *e = bv->wire_edges[i];
    /* Look for the new vertex closest to the other end of e. */
    BMVert *vclosest = nullptr;
    float dclosest = FLT_MAX;
    BMVert *votherclosest = nullptr;
    BMVert *vother = BM_edge_other_vert(e, v);
    BevVert *bvother = nullptr;
    if (BM_elem_flag_test(vother, BM_ELEM_TAG)) {
      bvother = find_bevvert(bp, vother);
      if (!bvother || !bvother->vmesh) {
        return; /* Shouldn't happen. */
      }
    }
    BoundVert *bndv = bv->vmesh->boundstart;
    do {
      if (bvother) {
        BoundVert *bndvother = bvother->vmesh->boundstart;
        do {
          float d = len_squared_v3v3(bndvother->nv.co, bndv->nv.co);
          if (d < dclosest) {
            vclosest = bndv->nv.v;
            votherclosest = bndvother->nv.v;
            dclosest = d;
          }
        } while ((bndvother = bndvother->next) != bvother->vmesh->boundstart);
      }
      else {
        float d = len_squared_v3v3(vother->co, bndv->nv.co);
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
  BMEdge *bme_prev = nullptr;
  BMEdge *bme_next = nullptr;
  for (int i = 0; i < 4; i++) {
    if (&bv->edges[i] == e) {
      bme_prev = bv->edges[(i + 3) % 4].e;
      bme_next = bv->edges[(i + 1) % 4].e;
      break;
    }
  }
  BLI_assert(bme_prev && bme_next);

  /* Want seams and sharp edges to cross only if that way on both sides. */
  bool disable_seam = BM_elem_flag_test(bme_prev, BM_ELEM_SEAM) !=
                      BM_elem_flag_test(bme_next, BM_ELEM_SEAM);
  bool enable_smooth = BM_elem_flag_test(bme_prev, BM_ELEM_SMOOTH) !=
                       BM_elem_flag_test(bme_next, BM_ELEM_SMOOTH);

  int nseg = e->seg;
  for (int i = 0; i < nseg; i++) {
    BMEdge *bme = BM_edge_exists(mesh_vert(vm, vmindex, 0, i)->v,
                                 mesh_vert(vm, vmindex, 0, i + 1)->v);
    BLI_assert(bme);
    BM_elem_attrs_copy(bm, bme_prev, bme);
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
  int mat_nr = bp->mat_nr;

  if (!BM_edge_is_manifold(bme)) {
    return;
  }

  BevVert *bv1 = find_bevvert(bp, bme->v1);
  BevVert *bv2 = find_bevvert(bp, bme->v2);

  BLI_assert(bv1 && bv2);

  EdgeHalf *e1 = find_edge_half(bv1, bme);
  EdgeHalf *e2 = find_edge_half(bv2, bme);

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
  int nseg = e1->seg;
  BLI_assert(nseg > 0 && nseg == e2->seg);

  BMVert *bmv1 = e1->leftv->nv.v;
  BMVert *bmv4 = e1->rightv->nv.v;
  BMVert *bmv2 = e2->rightv->nv.v;
  BMVert *bmv3 = e2->leftv->nv.v;

  BLI_assert(bmv1 && bmv2 && bmv3 && bmv4);

  BMFace *f1 = e1->fprev;
  BMFace *f2 = e1->fnext;
  BMFace *faces[4] = {f1, f1, f2, f2};

  int i1 = e1->leftv->index;
  int i2 = e2->leftv->index;
  VMesh *vm1 = bv1->vmesh;
  VMesh *vm2 = bv2->vmesh;

  BMVert *verts[4];
  verts[0] = bmv1;
  verts[1] = bmv2;

  Map<BMVert *, BMVert *> nv_bv_map; /* New vertex to the (original) bevel vertex mapping. */
  nv_bv_map.add(verts[0], bv1->v);
  nv_bv_map.add(verts[1], bv2->v);

  int odd = nseg % 2;
  int mid = nseg / 2;
  BMFace *fchoices[2] = {f1, f2};
  BMFace *f_choice = nullptr;
  int center_adj_k = -1;
  if (odd && e1->is_seam) {
    f_choice = choose_rep_face(bp, fchoices, 2);
    if (nseg > 1) {
      center_adj_k = f_choice == f1 ? mid + 2 : mid;
    }
  }
  for (int k = 1; k <= nseg; k++) {
    verts[3] = mesh_vert(vm1, i1, 0, k)->v;
    verts[2] = mesh_vert(vm2, i2, 0, nseg - k)->v;
    nv_bv_map.add(verts[3], bv1->v);
    nv_bv_map.add(verts[2], bv2->v);
    BMFace *r_f;
    if (odd && k == mid + 1) {
      if (e1->is_seam) {
        /* Straddles a seam: choose to interpolate in f_choice and snap the loops whose verts
         * are in the non-chosen face to bme for interpolation purposes.
         */
        BMEdge *edges[4];
        if (f_choice == f1) {
          edges[0] = edges[1] = nullptr;
          edges[2] = edges[3] = bme;
        }
        else {
          edges[0] = edges[1] = bme;
          edges[2] = edges[3] = nullptr;
        }
        r_f = bev_create_ngon(
            bp, bm, verts, 4, nullptr, f_choice, edges, nullptr, &nv_bv_map, mat_nr, true);
      }
      else {
        /* Straddles but not a seam: interpolate left half in f1, right half in f2. */
        r_f = bev_create_ngon(
            bp, bm, verts, 4, faces, f_choice, nullptr, nullptr, &nv_bv_map, mat_nr, true);
      }
    }
    else if (odd && k == center_adj_k && e1->is_seam) {
      /* The strip adjacent to the center one, in another UV island.
       * Snap the edge near the seam to bme to match what happens in
       * the bevel rings.
       */
      BMEdge *edges[4];
      BMFace *f_interp;
      if (k == mid) {
        edges[0] = edges[1] = nullptr;
        edges[2] = edges[3] = bme;
        f_interp = f1;
      }
      else {
        edges[0] = edges[1] = bme;
        edges[2] = edges[3] = nullptr;
        f_interp = f2;
      }
      r_f = bev_create_ngon(
          bp, bm, verts, 4, nullptr, f_interp, edges, nullptr, &nv_bv_map, mat_nr, true);
    }
    else if (!odd && k == mid) {
      /* Left poly that touches an even center line on right. */
      BMEdge *edges[4] = {nullptr, nullptr, bme, bme};
      r_f = bev_create_ngon(
          bp, bm, verts, 4, nullptr, f1, edges, nullptr, &nv_bv_map, mat_nr, true);
    }
    else if (!odd && k == mid + 1) {
      /* Right poly that touches an even center line on left. */
      BMEdge *edges[4] = {bme, bme, nullptr, nullptr};
      r_f = bev_create_ngon(
          bp, bm, verts, 4, nullptr, f2, edges, nullptr, &nv_bv_map, mat_nr, true);
    }
    else {
      /* Doesn't cross or touch the center line, so interpolate in appropriate f1 or f2. */
      BMFace *f = (k <= mid) ? f1 : f2;
      r_f = bev_create_ngon(
          bp, bm, verts, 4, nullptr, f, nullptr, nullptr, &nv_bv_map, mat_nr, true);
    }
    record_face_kind(bp, r_f, F_EDGE);
    /* Tag the long edges: those out of verts[0] and verts[2]. */
    BMIter iter;
    BMLoop *l;
    BM_ITER_ELEM (l, &iter, r_f, BM_LOOPS_OF_FACE) {
      if (ELEM(l->v, verts[0], verts[2])) {
        BM_elem_flag_enable(l, BM_ELEM_LONG_TAG);
      }
    }
    verts[0] = verts[3];
    verts[1] = verts[2];
  }

  /* Copy edge data to first and last edge. */
  BMEdge *bme1 = BM_edge_exists(bmv1, bmv2);
  BMEdge *bme2 = BM_edge_exists(bmv3, bmv4);
  BLI_assert(bme1 && bme2);
  BM_elem_attrs_copy(bm, bme, bme1);
  BM_elem_attrs_copy(bm, bme, bme2);

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
  double y0 = superellipse_co(x0, r, rbig);
  const double tol = 1e-13; /* accumulates for many segments so use low value. */
  const int maxiter = 10;

  /* For gradient between -1 and 1, xnew can only be in [x0 + sqrt(2)/2*dtarget, x0 + dtarget]. */
  double xmin = x0 + M_SQRT2 / 2.0 * dtarget;
  xmin = std::min(xmin, 1.0);
  double xmax = x0 + dtarget;
  xmax = std::min(xmax, 1.0);
  double ymin = superellipse_co(xmin, r, rbig);
  double ymax = superellipse_co(xmax, r, rbig);

  /* NOTE: using distance**2 (no sqrt needed) does not converge that well. */
  double dmaxerr = sqrt(pow((xmax - x0), 2) + pow((ymax - y0), 2)) - dtarget;
  double dminerr = sqrt(pow((xmin - x0), 2) + pow((ymin - y0), 2)) - dtarget;

  double xnew = xmax - dmaxerr * (xmax - xmin) / (dmaxerr - dminerr);
  bool lastupdated_upper = true;

  for (int iter = 0; iter < maxiter; iter++) {
    double ynew = superellipse_co(xnew, r, rbig);
    double dnewerr = sqrt(pow((xnew - x0), 2) + pow((ynew - y0), 2)) - dtarget;
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
  int imax = (seg + 1) / 2 - 1; /* Ceiling division - 1. */

  bool seg_odd = seg % 2;

  bool rbig;
  double mx;
  if (r > 1.0f) {
    rbig = true;
    mx = pow(0.5, 1.0 / r);
  }
  else {
    rbig = false;
    mx = 1 - pow(0.5, 1.0 / r);
  }

  /* Initial positions, linear spacing along x axis. */
  for (int i = 0; i <= imax; i++) {
    xvals[i] = i * mx / seg * 2;
    yvals[i] = superellipse_co(xvals[i], r, rbig);
  }
  yvals[0] = 1;

  /* Smooth distance loop. */
  for (int iter = 0; iter < smoothitermax; iter++) {
    double sum = 0.0;
    double dmin = 2.0;
    double dmax = 0.0;
    /* Update distances between neighbor points. Store the highest and
     * lowest to see if the maximum error to average distance (which isn't
     * known yet) is below required precision. */
    for (int i = 0; i < imax; i++) {
      double d = sqrt(pow((xvals[i + 1] - xvals[i]), 2) + pow((yvals[i + 1] - yvals[i]), 2));
      sum += d;
      dmax = std::max(d, dmax);
      dmin = std::min(d, dmin);
    }
    /* For last distance, weight with 1/2 if seg_odd. */
    double davg;
    if (seg_odd) {
      sum += M_SQRT2 / 2 * (yvals[imax] - xvals[imax]);
      davg = sum / (imax + 0.5);
    }
    else {
      sum += sqrt(pow((xvals[imax] - mx), 2) + pow((yvals[imax] - mx), 2));
      davg = sum / (imax + 1.0);
    }
    /* Max error in tolerance? -> Quit. */
    bool precision_reached = true;
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
    for (int i = 1; i <= imax; i++) {
      xvals[i] = find_superellipse_chord_endpoint(xvals[i - 1], davg, r, rbig);
      yvals[i] = superellipse_co(xvals[i], r, rbig);
    }
  }

  /* Fill remaining. */
  if (!seg_odd) {
    xvals[imax + 1] = mx;
    yvals[imax + 1] = mx;
  }
  for (int i = imax + 1; i <= seg; i++) {
    yvals[i] = xvals[seg - i];
    xvals[i] = yvals[seg - i];
  }

  if (!rbig) {
    for (int i = 0; i <= seg; i++) {
      double temp = xvals[i];
      xvals[i] = 1.0 - yvals[i];
      yvals[i] = 1.0 - temp;
    }
  }
}

/**
 * Find equidistant points `(x0,y0), (x1,y1)... (xn,yn)` on the superellipse
 * function in the first quadrant. For special profiles (linear, arc,
 * rectangle) the point can be calculated easily, for any other profile a more
 * expensive search procedure must be used because there is no known closed
 * form for equidistant parametrization.
 * `xvals` and `yvals` should be size `n+1`.
 */
static void find_even_superellipse_chords(int n, float r, double *xvals, double *yvals)
{
  bool seg_odd = n % 2;
  int n2 = n / 2;

  /* Special cases. */
  if (r == PRO_LINE_R) {
    /* Linear spacing. */
    for (int i = 0; i <= n; i++) {
      xvals[i] = double(i) / n;
      yvals[i] = 1.0 - double(i) / n;
    }
    return;
  }
  if (r == PRO_CIRCLE_R) {
    double temp = M_PI_2 / n;
    /* Angle spacing. */
    for (int i = 0; i <= n; i++) {
      xvals[i] = sin(i * temp);
      yvals[i] = cos(i * temp);
    }
    return;
  }
  if (r == PRO_SQUARE_IN_R) {
    /* n is even, distribute first and second half linear. */
    if (!seg_odd) {
      for (int i = 0; i <= n2; i++) {
        xvals[i] = 0.0;
        yvals[i] = 1.0 - double(i) / n2;
        xvals[n - i] = yvals[i];
        yvals[n - i] = xvals[i];
      }
    }
    /* n is odd, so get one corner-cut chord. */
    else {
      double temp = 1.0 / (n2 + M_SQRT2 / 2.0);
      for (int i = 0; i <= n2; i++) {
        xvals[i] = 0.0;
        yvals[i] = 1.0 - double(i) * temp;
        xvals[n - i] = yvals[i];
        yvals[n - i] = xvals[i];
      }
    }
    return;
  }
  if (r == PRO_SQUARE_R) {
    /* n is even, distribute first and second half linear. */
    if (!seg_odd) {
      for (int i = 0; i <= n2; i++) {
        xvals[i] = double(i) / n2;
        yvals[i] = 1.0;
        xvals[n - i] = yvals[i];
        yvals[n - i] = xvals[i];
      }
    }
    /* n is odd, so get one corner-cut chord. */
    else {
      double temp = 1.0 / (n2 + M_SQRT2 / 2);
      for (int i = 0; i <= n2; i++) {
        xvals[i] = double(i) * temp;
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

  float fullness;
  if (bp->profile_type == BEVEL_PROFILE_CUSTOM) {
    /* Set fullness to the average "height" of the profile's sampled points. */
    fullness = 0.0f;
    for (int i = 0; i < nseg; i++) { /* Don't use the end points. */
      fullness += float(bp->pro_spacing.xvals[i] + bp->pro_spacing.yvals[i]) / (2.0f * nseg);
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
 * The superellipse used for multi-segment profiles does not have a closed-form way
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
  int seg = bp->seg;

  if (seg <= 1) {
    /* Only 1 segment, we don't need any profile information. */
    pro_spacing->xvals = nullptr;
    pro_spacing->yvals = nullptr;
    pro_spacing->xvals_2 = nullptr;
    pro_spacing->yvals_2 = nullptr;
    pro_spacing->seg_2 = 0;
    return;
  }

  int seg_2 = max_ii(power_of_2_max_i(bp->seg), 4);

  /* Sample the seg_2 segments used during vertex mesh subdivision. */
  bp->pro_spacing.seg_2 = seg_2;
  if (seg_2 == seg) {
    pro_spacing->xvals_2 = pro_spacing->xvals;
    pro_spacing->yvals_2 = pro_spacing->yvals;
  }
  else {
    pro_spacing->xvals_2 = (double *)BLI_memarena_alloc(bp->mem_arena,
                                                        sizeof(double) * (seg_2 + 1));
    pro_spacing->yvals_2 = (double *)BLI_memarena_alloc(bp->mem_arena,
                                                        sizeof(double) * (seg_2 + 1));
    if (custom) {
      /* Make sure the curve profile widget's sample table is full of the seg_2 samples. */
      BKE_curveprofile_init((CurveProfile *)bp->custom_profile, short(seg_2));

      /* Copy segment locations into the profile spacing struct. */
      for (int i = 0; i < seg_2 + 1; i++) {
        pro_spacing->xvals_2[i] = double(bp->custom_profile->segments[i].y);
        pro_spacing->yvals_2[i] = double(bp->custom_profile->segments[i].x);
      }
    }
    else {
      find_even_superellipse_chords(
          seg_2, bp->pro_super_r, pro_spacing->xvals_2, pro_spacing->yvals_2);
    }
  }

  /* Sample the input number of segments. */
  pro_spacing->xvals = (double *)BLI_memarena_alloc(bp->mem_arena, sizeof(double) * (seg + 1));
  pro_spacing->yvals = (double *)BLI_memarena_alloc(bp->mem_arena, sizeof(double) * (seg + 1));
  if (custom) {
    /* Make sure the curve profile's sample table is full. */
    if (bp->custom_profile->segments_len != seg || !bp->custom_profile->segments) {
      BKE_curveprofile_init((CurveProfile *)bp->custom_profile, short(seg));
    }

    /* Copy segment locations into the profile spacing struct. */
    for (int i = 0; i < seg + 1; i++) {
      pro_spacing->xvals[i] = double(bp->custom_profile->segments[i].y);
      pro_spacing->yvals[i] = double(bp->custom_profile->segments[i].x);
    }
  }
  else {
    find_even_superellipse_chords(seg, bp->pro_super_r, pro_spacing->xvals, pro_spacing->yvals);
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
 * where edges are A, B, and C, following a face around vertices `a, b, c, d`.
 * `th1` is angle `abc` and th2 is angle `bcd`;
 * and the argument `EdgeHalf eb` is B, going from b to c.
 * In general case, edge offset specs for A, B, C have
 * the form `ka*t`, `kb*t`, `kc*t` where `ka`, `kb`, `kc` are some factors
 * (may be 0) and t is the current bp->offset.
 * We want to calculate t at which the clone of B parallel
 * to it collapses. This can be calculated using trig.
 * Another case of geometry collision that can happen is
 * When B slides along A because A is un-beveled.
 * Then it might collide with a. Similarly for B sliding along C.
 */
static float geometry_collide_offset(BevelParams *bp, EdgeHalf *eb)
{
  float no_collide_offset = bp->offset + 1e6;
  float limit = no_collide_offset;
  if (bp->offset == 0.0f) {
    return no_collide_offset;
  }
  float kb = eb->offset_l_spec;
  EdgeHalf *ea = eb->next; /* NOTE: this is in direction b --> a. */
  float ka = ea->offset_r_spec;
  BMVert *vb, *vc;
  if (eb->is_rev) {
    vc = eb->e->v1;
    vb = eb->e->v2;
  }
  else {
    vb = eb->e->v1;
    vc = eb->e->v2;
  }
  BMVert *va = ea->is_rev ? ea->e->v1 : ea->e->v2;
  BevVert *bvc = nullptr;
  EdgeHalf *ebother = find_other_end_edge_half(bp, eb, &bvc);
  EdgeHalf *ec;
  BMVert *vd;
  float kc;
  if (ELEM(bp->offset_type, BEVEL_AMT_PERCENT, BEVEL_AMT_ABSOLUTE)) {
    if (ea->is_bev && ebother != nullptr && ebother->prev->is_bev) {
      if (bp->offset_type == BEVEL_AMT_PERCENT) {
        return 50.0f;
      }
      /* This is only right sometimes. The exact answer is very hard to calculate. */
      float blen = BM_edge_calc_length(eb->e);
      return bp->offset > blen / 2.0f ? blen / 2.0f : blen;
    }
    return no_collide_offset;
  }
  if (ebother != nullptr) {
    ec = ebother->prev; /* NOTE: this is in direction c --> d. */
    vc = bvc->v;
    kc = ec->offset_l_spec;
    vd = ec->is_rev ? ec->e->v1 : ec->e->v2;
  }
  else {
    /* No bevvert for w, so C can't be beveled. */
    kc = 0.0f;
    ec = nullptr;
    /* Find an edge from c that has same face. */
    if (eb->fnext == nullptr) {
      return no_collide_offset;
    }
    BMLoop *lb = BM_face_edge_share_loop(eb->fnext, eb->e);
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
  float th1 = angle_v3v3v3(va->co, vb->co, vc->co);
  float th2 = angle_v3v3v3(vb->co, vc->co, vd->co);

  /* First calculate offset at which edge B collapses, which happens
   * when advancing clones of A, B, C all meet at a point. */
  float sin1 = sinf(th1);
  float sin2 = sinf(th2);
  float cos1 = cosf(th1);
  float cos2 = cosf(th2);
  /* The side offsets, overlap at the two corners, to create two corner vectors.
   * The intersection of these two corner vectors is the collapse point.
   * The length of edge B divided by the projection of these vectors onto edge B
   * is the number of 'offsets' that can be accommodated. */
  float offsets_projected_on_B = safe_divide(ka + cos1 * kb, sin1) +
                                 safe_divide(kc + cos2 * kb, sin2);
  if (offsets_projected_on_B > BEVEL_EPSILON) {
    offsets_projected_on_B = bp->offset * (len_v3v3(vb->co, vc->co) / offsets_projected_on_B);
    if (offsets_projected_on_B > BEVEL_EPSILON) {
      limit = offsets_projected_on_B;
    }
  }

  /* Now check edge slide cases.
   * where side edges are in line with edge B and are not beveled, we should continue
   * iterating until we find a return edge (not in line with B) to provide a minimum offset
   * to the far side of the N-gon. This is not perfect, but is simpler and will catch many
   * more overlap issues. */
  if (kb > FLT_EPSILON && (ka == 0.0f || kc == 0.0f)) {
    // use bevel weight offsets and not the full offset where weights are used
    kb = bp->offset / kb;

    if (ka == 0.0f) {
      BMLoop *la = BM_face_edge_share_loop(eb->fnext, ea->e);
      if (la) {
        float A_side_slide = 0.0f;
        float exterior_angle = 0.0f;
        bool first = true;

        while (exterior_angle < 0.0001f) {
          if (first) {
            exterior_angle = float(M_PI) - th1;
            first = false;
          }
          else {
            la = la->prev;
            exterior_angle += float(M_PI) -
                              angle_v3v3v3(la->v->co, la->next->v->co, la->next->next->v->co);
          }
          A_side_slide += BM_edge_calc_length(la->e) * sinf(exterior_angle);
        }
        limit = std::min(A_side_slide * kb, limit);
      }
    }

    if (kc == 0.0f) {
      BMLoop *lc = BM_face_edge_share_loop(eb->fnext, eb->e);
      if (lc) {
        lc = lc->next;
        float C_side_slide = 0.0f;
        float exterior_angle = 0.0f;
        bool first = true;
        while (exterior_angle < 0.0001f) {
          if (first) {
            exterior_angle = float(M_PI) - th2;
            first = false;
          }
          else {
            lc = lc->next;
            exterior_angle += float(M_PI) -
                              angle_v3v3v3(lc->prev->v->co, lc->v->co, lc->next->v->co);
          }
          C_side_slide += BM_edge_calc_length(lc->e) * sinf(exterior_angle);
        }
        limit = std::min(C_side_slide * kb, limit);
      }
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
  float no_collide_offset = bp->offset + 1e6;
  if (bp->offset == 0.0f) {
    return no_collide_offset;
  }
  float ka = ea->offset_l_spec / bp->offset;
  EdgeHalf *eb = find_other_end_edge_half(bp, ea, nullptr);
  float kb = eb ? eb->offset_l_spec / bp->offset : 0.0f;
  float kab = ka + kb;
  float la = BM_edge_calc_length(ea->e);
  if (kab <= 0.0f) {
    return no_collide_offset;
  }
  return la / kab;
}

/**
 * Calculate an offset that is the lesser of the current bp.offset and the maximum possible offset
 * before geometry collisions happen. If the offset changes as a result of this, adjust the current
 * edge offset specs to reflect this clamping, and store the new offset in bp.offset.
 */
static void bevel_limit_offset(BevelParams *bp, BMesh *bm)
{
  float limited_offset = bp->offset;
  BMIter iter;
  BMVert *bmv;
  BM_ITER_MESH (bmv, &iter, bm, BM_VERTS_OF_MESH) {
    if (!BM_elem_flag_test(bmv, BM_ELEM_TAG)) {
      continue;
    }
    BevVert *bv = find_bevvert(bp, bmv);
    if (!bv) {
      continue;
    }
    for (int i = 0; i < bv->edgecount; i++) {
      EdgeHalf *eh = &bv->edges[i];
      if (bp->affect_type == BEVEL_AFFECT_VERTICES) {
        float collision_offset = vertex_collide_offset(bp, eh);
        limited_offset = std::min(collision_offset, limited_offset);
      }
      else {
        float collision_offset = geometry_collide_offset(bp, eh);
        limited_offset = std::min(collision_offset, limited_offset);
      }
    }
  }

  if (limited_offset < bp->offset) {
    /* All current offset specs have some number times bp->offset,
     * so we can just multiply them all by the reduction factor
     * of the offset to have the effect of recalculating the specs
     * with the new limited_offset.
     */
    float offset_factor = limited_offset / bp->offset;
    BM_ITER_MESH (bmv, &iter, bm, BM_VERTS_OF_MESH) {
      if (!BM_elem_flag_test(bmv, BM_ELEM_TAG)) {
        continue;
      }
      BevVert *bv = find_bevvert(bp, bmv);
      if (!bv) {
        continue;
      }
      for (int i = 0; i < bv->edgecount; i++) {
        EdgeHalf *eh = &bv->edges[i];
        eh->offset_l_spec *= offset_factor;
        eh->offset_r_spec *= offset_factor;
        eh->offset_l *= offset_factor;
        eh->offset_r *= offset_factor;
      }
    }
    bp->offset = limited_offset;
  }
}

void BM_mesh_bevel(BMesh *bm,
                   const float offset,
                   const int offset_type,
                   const int profile_type,
                   const int segments,
                   const float profile,
                   const bool affect_type,
                   const bool use_weights,
                   const bool limit_offset,
                   const MDeformVert *dvert,
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
                   const CurveProfile *custom_profile,
                   const int vmesh_method,
                   const int bweight_offset_vert,
                   const int bweight_offset_edge)
{
  BMIter iter, liter;
  BMVert *v, *v_next;
  BMEdge *e;
  BMFace *f;
  BMLoop *l;
  BevVert *bv;
  BevelParams bp{};
  bp.bm = bm;
  bp.offset = offset;
  bp.offset_type = offset_type;
  bp.seg = max_ii(segments, 1);
  bp.profile = profile;
  bp.pro_super_r = -logf(2.0) / logf(sqrtf(profile)); /* Convert to superellipse exponent. */
  bp.affect_type = affect_type;
  bp.use_weights = use_weights;
  bp.bweight_offset_vert = bweight_offset_vert;
  bp.bweight_offset_edge = bweight_offset_edge;
  bp.loop_slide = loop_slide;
  bp.limit_offset = limit_offset;
  bp.offset_adjust = (bp.affect_type != BEVEL_AFFECT_VERTICES) &&
                     !ELEM(offset_type, BEVEL_AMT_PERCENT, BEVEL_AMT_ABSOLUTE);
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
  bp.face_hash = nullptr;
  bp.profile_type = profile_type;
  bp.custom_profile = custom_profile;
  bp.vmesh_method = vmesh_method;

  if (bp.offset <= 0) {
    return;
  }

#ifdef BEVEL_DEBUG_TIME
  double start_time = BLI_time_now_seconds();
#endif

  /* Disable the miters with the cutoff vertex mesh method, the combination isn't useful anyway. */
  if (bp.vmesh_method == BEVEL_VMESH_CUTOFF) {
    bp.miter_outer = BEVEL_MITER_SHARP;
    bp.miter_inner = BEVEL_MITER_SHARP;
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

  /* Primary alloc. */
  bp.vert_hash = BLI_ghash_ptr_new(__func__);
  bp.mem_arena = BLI_memarena_new(MEM_SIZE_OPTIMAL(1 << 16), __func__);
  BLI_memarena_use_calloc(bp.mem_arena);

  /* Get the 2D profile point locations from either the superellipse or the custom profile. */
  set_profile_spacing(&bp, &bp.pro_spacing, bp.profile_type == BEVEL_PROFILE_CUSTOM);

  /* Get the 'fullness' of the profile for the ADJ vertex mesh method. */
  if (bp.seg > 1) {
    bp.pro_spacing.fullness = find_profile_fullness(&bp);
  }

  /* Get separate non-custom profile samples for the miter profiles if they are needed */
  if (bp.profile_type == BEVEL_PROFILE_CUSTOM &&
      (bp.miter_inner != BEVEL_MITER_SHARP || bp.miter_outer != BEVEL_MITER_SHARP))
  {
    set_profile_spacing(&bp, &bp.pro_spacing_miter, false);
  }

  bp.face_hash = BLI_ghash_ptr_new(__func__);
  BLI_ghash_flag_set(bp.face_hash, GHASH_FLAG_ALLOW_DUPES);
  bp.uv_face_hash = BLI_ghash_ptr_new(__func__);

  math_layer_info_init(&bp, bm);
  uv_vert_map_init(&bp, bm);

  /* Analyze input vertices, sorting edges and assigning initial new vertex positions. */
  BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
    if (BM_elem_flag_test(v, BM_ELEM_TAG)) {
      bv = bevel_vert_construct(bm, &bp, v);
      if (!limit_offset && bv) {
        build_boundary(&bp, bv, true);
        determine_uv_vert_connectivity(&bp, bm, v);
      }
    }
  }

  /* Perhaps clamp offset to avoid geometry collisions. */
  if (limit_offset) {
    bevel_limit_offset(&bp, bm);

    /* Assign initial new vertex positions. */
    BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
      if (BM_elem_flag_test(v, BM_ELEM_TAG)) {
        bv = find_bevvert(&bp, v);
        if (bv) {
          build_boundary(&bp, bv, true);
          determine_uv_vert_connectivity(&bp, bm, v);
        }
      }
    }
  }

  /* Perhaps do a pass to try to even out widths. */
  if (bp.offset_adjust) {
    adjust_offsets(&bp, bm);
  }

  /* Maintain consistent orientations for the asymmetrical custom profiles. */
  if (bp.profile_type == BEVEL_PROFILE_CUSTOM) {
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
  if (bp.affect_type != BEVEL_AFFECT_VERTICES) {
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
  Set<BMFace *> rebuilt_orig_faces;
  BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
    if (BM_elem_flag_test(v, BM_ELEM_TAG)) {
      bevel_rebuild_existing_polygons(bm, &bp, v, rebuilt_orig_faces);
      bevel_reattach_wires(bm, &bp, v);
    }
  }

  for (BMFace *f : rebuilt_orig_faces) {
    BM_face_kill(bm, f);
  }

  BM_ITER_MESH_MUTABLE (v, v_next, &iter, bm, BM_VERTS_OF_MESH) {
    if (BM_elem_flag_test(v, BM_ELEM_TAG)) {
      BLI_assert(find_bevvert(&bp, v) != nullptr);
      uv_vert_map_pop(&bp, v);
      BM_vert_kill(bm, v);
    }
  }

  bevel_merge_uvs(&bp, bm);

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
  BLI_ghash_free(bp.vert_hash, nullptr, nullptr);
  BLI_ghash_free(bp.face_hash, nullptr, nullptr);
  BLI_ghash_free(bp.uv_face_hash, nullptr, nullptr);
  BLI_memarena_free(bp.mem_arena);

#ifdef BEVEL_DEBUG_TIME
  double end_time = BLI_time_now_seconds();
  printf("BMESH BEVEL TIME = %.3f\n", end_time - start_time);
#endif
}
