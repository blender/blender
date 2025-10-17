/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

#include "BLI_enum_flags.hh"
#include "BLI_linklist.h"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector.h"
#include "BLI_set.hh"
#include "BLI_threads.h"

#include "ED_grease_pencil.hh"

#include <algorithm>
#include <cmath>

struct LineartBoundingArea;
struct LineartEdge;
struct LineartVert;
struct Mesh;
struct Object;

struct LineartModifierRuntime {
  /* This list is constructed during `update_depsgraph()` call, and stays valid until the next
   * update. This way line art can load objects from this list instead of iterating over all
   * objects that may or may not have finished evaluating. */
  blender::Set<const Object *> object_dependencies;
};

struct LineartStaticMemPoolNode {
  Link item;
  size_t size;
  size_t used_byte;
  /* User memory starts here */
};

struct LineartStaticMemPool {
  ListBase pools;
  SpinLock lock_mem;
};

struct LineartTriangleAdjacent {
  LineartEdge *e[3];
};

struct LineartTriangle {
  LineartVert *v[3];

  /* first culled in line list to use adjacent triangle info, then go through triangle list. */
  double gn[3];

  uint8_t material_mask_bits;
  uint8_t intersection_mask;
  uint8_t mat_occlusion;
  uint8_t flags; /* #eLineartTriangleFlags */

  /* target_reference = (obi->obindex | triangle_index) */
  /*        higher 12 bits-------^         ^-----index in object, lower 20 bits */
  uint32_t target_reference;

  uint8_t intersection_priority;

  /**
   * Only use single link list, because we don't need to go back in order.
   * This variable is also reused to store the pointer to adjacent lines of this triangle before
   * intersection stage.
   */
  LinkNode *intersecting_verts;
};

struct LineartTriangleThread {
  LineartTriangle base;
  /**
   * This variable is used to store per-thread triangle-line testing pair,
   * also re-used to store triangle-triangle pair for intersection testing stage.
   * Do not directly use #LineartTriangleThread.
   * The size of #LineartTriangle is dynamically allocated to contain set thread number of
   * "testing_e" field. Worker threads will test lines against the "base" triangle.
   * At least one thread is present, thus we always have at least `testing_e[0]`.
   */
  LineartEdge *testing_e[1];
};

enum eLineArtElementNodeFlag {
  LRT_ELEMENT_IS_ADDITIONAL = (1 << 0),
  LRT_ELEMENT_BORDER_ONLY = (1 << 1),
  LRT_ELEMENT_NO_INTERSECTION = (1 << 2),
  LRT_ELEMENT_INTERSECTION_DATA = (1 << 3),
};
ENUM_OPERATORS(eLineArtElementNodeFlag);

struct LineartElementLinkNode {
  LineartElementLinkNode *next, *prev;
  void *pointer;
  int element_count;
  void *object_ref;
  eLineArtElementNodeFlag flags;

  /* For edge element link nodes, used for shadow edge matching. */
  int obindex;
  int global_index_offset;

  /** Per object value, always set, if not enabled by #ObjectLineArt, then it's set to global. */
  float crease_threshold;
};

struct LineartEdgeSegment {
  LineartEdgeSegment *next, *prev;
  /** The point after which a property of the segment is changed, e.g. occlusion/material mask etc.
   * ratio==0: v1  ratio==1: v2 (this is in 2D projected space), */
  double ratio;
  /** Occlusion level after "ratio" point */
  uint8_t occlusion;

  /* Used to filter line art occlusion edges */
  uint8_t material_mask_bits;

  /* Lit/shaded flag for shadow is stored here.
   * TODO(Yiming): Transfer material masks from shadow results
   * onto here so then we can even filter transparent shadows. */
  uint32_t shadow_mask_bits;
};

struct LineartShadowEdge {
  LineartShadowEdge *next, *prev;
  /* Two end points in frame-buffer coordinates viewed from the light source. */
  double fbc1[4], fbc2[4];
  double g1[3], g2[3];
  bool orig1, orig2;
  LineartEdge *e_ref;
  LineartEdge *e_ref_light_contour;
  LineartEdgeSegment *es_ref; /* Only for 3rd stage casting. */
  ListBase shadow_segments;
};

enum eLineartShadowSegmentFlag {
  LRT_SHADOW_CASTED = 1,
  LRT_SHADOW_FACING_LIGHT = 2,
};

/* Represents a cutting point on a #LineartShadowEdge */
struct LineartShadowSegment {
  LineartShadowSegment *next, *prev;
  /* eLineartShadowSegmentFlag */
  int flag;
  /* The point after which a property of the segment is changed. e.g. shadow mask/target_ref etc.
   * Coordinates in NDC during shadow calculation but transformed to global linear before cutting
   * onto edges during the loading stage of the "actual" rendering. */
  double ratio;
  /* Left and right pos, because when casting shadows at some point there will be
   * non-continuous cuts, see #lineart_shadow_edge_cut for detailed explanation. */
  double fbc1[4], fbc2[4];
  /* Global position. */
  double g1[4], g2[4];
  uint32_t target_reference;
  uint32_t shadow_mask_bits;
};

struct LineartVert {
  double gloc[3];
  double fbcoord[4];

  /* Scene global index. */
  int index;
};

struct LineartEdge {
  LineartVert *v1, *v2;

  /** These two variables are also used to specify original edge and segment during 3rd stage
   * reprojection, So we can easily find out the line which results come from. */
  LineartTriangle *t1, *t2;

  ListBase segments;
  int8_t min_occ;

  /** Also for line type determination on chaining. */
  uint16_t flags;
  uint8_t intersection_mask;

  /**
   * Matches the shadow result, used to determine whether a line is in the shadow or not.
   * #edge_identifier usages:
   * - Intersection lines:
   *    ((e->t1->target_reference << 32) | e->t2->target_reference);
   * - Other lines: LRT_EDGE_IDENTIFIER(obi, e);
   * - After shadow calculation: (search the shadow result and set reference to that);
   */
  uint64_t edge_identifier;

  /**
   * - Light contour: original_e->t1->target_reference | original_e->t2->target_reference.
   * - Cast shadow: triangle_projected_onto->target_reference.
   */
  uint64_t target_reference;

  /**
   * Still need this entry because culled lines will not add to object
   * #LineartElementLinkNode node (known as `eln` internally).
   *
   * TODO: If really need more savings, we can allocate this in a "extended" way too, but we need
   * another bit in flags to be able to show the difference.
   */
  Object *object_ref;
};

struct LineartEdgeChain {
  LineartEdgeChain *next, *prev;
  ListBase chain;

  /** Calculated before draw command. */
  float length;

  /** Used when re-connecting and grease-pencil stroke generation. */
  uint8_t picked;
  uint8_t level;

  /** Chain now only contains one type of segments */
  int type;
  /** Will only connect chains that has the same loop id. */
  int loop_id;
  uint8_t material_mask_bits;
  uint8_t intersection_mask;
  uint32_t shadow_mask_bits;

  /* We need local index for correct weight transfer, line art index is global, thus
   * local_index=lineart_index-index_offset. */
  uint32_t index_offset;

  Object *object_ref;
  Object *silhouette_backdrop;
};

struct LineartEdgeChainItem {
  LineartEdgeChainItem *next, *prev;
  /** Need z value for fading, w value for image frame clipping. */
  float pos[4];
  /** For restoring position to 3d space. */
  float gpos[3];
  float normal[3];
  uint16_t line_type;
  uint8_t occlusion;
  uint8_t material_mask_bits;
  uint8_t intersection_mask;
  uint32_t shadow_mask_bits;
  size_t index;
};

struct LineartChainRegisterEntry {
  LineartChainRegisterEntry *next, *prev;
  LineartEdgeChain *ec;
  LineartEdgeChainItem *eci;
  int8_t picked;

  /* left/right mark.
   * Because we revert list in chaining so we need the flag. */
  int8_t is_left;
};

struct LineartAdjacentEdge {
  uint32_t v1;
  uint32_t v2;
  uint32_t e;
};

enum eLineArtTileRecursiveLimit {
  /* If tile gets this small, it's already much smaller than a pixel. No need to continue
   * splitting. */
  LRT_TILE_RECURSIVE_PERSPECTIVE = 16,
  /* This is a tried-and-true safe value for high poly models that also needed ortho rendering. */
  LRT_TILE_RECURSIVE_ORTHO = 10,
};

#define LRT_TILE_SPLITTING_TRIANGLE_LIMIT 100
#define LRT_TILE_EDGE_COUNT_INITIAL 32

enum eLineartShadowCameraType {
  LRT_SHADOW_CAMERA_DIRECTIONAL = 1,
  LRT_SHADOW_CAMERA_POINT = 2,
};

struct LineartPendingEdges {
  LineartEdge **array;
  int max;
  int next;
};

struct LineartData {
  int w, h;
  int thread_count;
  int sizeof_triangle;

  LineartStaticMemPool render_data_pool;
  /* A pointer to LineartCache::chain_data_pool, which acts as a cache for edge chains. */
  LineartStaticMemPool *chain_data_pool;
  /* Reference to LineartCache::shadow_data_pool, stay available until the final round of line art
   * calculation is finished. */
  LineartStaticMemPool *shadow_data_pool;

  /* Storing shadow edge eln, array, and cuts for shadow information, so it's available when line
   * art runs the second time for occlusion. Either a reference to LineartCache::shadow_data_pool
   * (shadow stage) or a reference to LineartData::render_data_pool (final stage). */
  LineartStaticMemPool *edge_data_pool;

  struct _qtree {

    int count_x, count_y;
    double tile_width, tile_height;

    /* When splitting bounding areas, if there's an ortho camera placed at a straight angle, there
     * will be a lot of triangles aligned in line which can not be separated by continue
     * subdividing the tile. So we set a strict limit when using ortho camera. See
     * eLineArtTileRecursiveLimit. */
    int recursive_level;

    LineartBoundingArea *initials;

    uint32_t initial_tile_count;

  } qtree;

  struct _geom {

    ListBase vertex_buffer_pointers;
    ListBase line_buffer_pointers;
    ListBase triangle_buffer_pointers;

    /** This one's memory is not from main pool and is free()ed after culling stage. */
    ListBase triangle_adjacent_pointers;

    ListBase intersecting_vertex_buffer;

  } geom;

  struct _conf {

    double view_projection[4][4];
    double view[4][4];

    float overscan;

    int max_occlusion_level;
    double crease_angle;
    double crease_cos;

    int draw_material_preview;
    double material_transparency;

    bool use_contour;
    bool use_crease;
    bool use_material;
    bool use_edge_marks;
    bool use_intersections;
    bool use_loose;
    bool use_light_contour;
    bool use_shadow;
    bool use_contour_secondary; /* From viewing camera, during shadow calculation. */

    int shadow_selection; /* Needs to be numeric because it's not just on/off. */
    bool shadow_enclose_shapes;
    bool shadow_use_silhouette;

    bool fuzzy_intersections;
    bool fuzzy_everything;
    bool allow_boundaries;
    bool allow_overlapping_edges;
    bool allow_duplicated_types;
    bool remove_doubles;
    bool use_loose_as_contour;
    bool use_loose_edge_chain;
    bool use_geometry_space_chain;
    bool use_image_boundary_trimming;
    bool use_back_face_culling;

    bool filter_face_mark;
    bool filter_face_mark_invert;
    bool filter_face_mark_boundaries;
    bool filter_face_mark_keep_contour;

    bool force_crease;
    bool sharp_as_crease;

    bool chain_preserve_details;

    bool do_shadow_cast;
    bool light_reference_available;

    /* Keep an copy of these data so when line art is running itself contained. */
    bool cam_is_persp;
    /* "Secondary" ones are from viewing camera
     * (as opposed to shadow camera), during shadow calculation. */
    bool cam_is_persp_secondary;
    float cam_obmat[4][4];
    float cam_obmat_secondary[4][4];
    double camera_pos[3];
    double camera_pos_secondary[3];
    double active_camera_pos[3]; /* Stroke offset calculation may use active or selected camera. */
    double near_clip, far_clip;
    float shift_x, shift_y;

    float crease_threshold;
    float chaining_image_threshold;
    float angle_splitting_threshold;

    float chain_smooth_tolerance;

    double view_vector[3];
    double view_vector_secondary[3]; /* For shadow. */
  } conf;

  LineartElementLinkNode *isect_scheduled_up_to;
  int isect_scheduled_up_to_index;

  /* NOTE: Data inside #pending_edges are allocated with MEM_xxx call instead of in pool. */
  struct LineartPendingEdges pending_edges;
  int scheduled_count;

  /* Intermediate shadow results, list of LineartShadowEdge */
  LineartShadowEdge *shadow_edges;
  int shadow_edges_count;

  ListBase chains;

  ListBase wasted_cuts;
  ListBase wasted_shadow_cuts;
  SpinLock lock_cuts;
  SpinLock lock_task;
};

struct LineartCache {
  blender::ed::greasepencil::LineartLimitInfo LimitInfo;
  /** Separate memory pool for chain data and shadow, this goes to the cache, so when we free the
   * main pool, chains and shadows will still be available. */
  LineartStaticMemPool chain_data_pool;
  LineartStaticMemPool shadow_data_pool;

  /** A copy of ld->chains so we have that data available after ld has been destroyed. */
  ListBase chains;

  /** Shadow-computed feature lines from original meshes to be matched with the second load of
   * meshes thus providing lit/shade info in the second run of line art. */
  ListBase shadow_elns;

  /** Cache only contains edge types specified in this variable. */
  uint16_t all_enabled_edge_types;
};

#define DBL_TRIANGLE_LIM 1e-8
#define DBL_EDGE_LIM 1e-9

#define LRT_MEMORY_POOL_1MB (1 << 20)

enum eLineartTriangleFlags {
  LRT_CULL_DONT_CARE = 0,
  LRT_CULL_USED = (1 << 0),
  LRT_CULL_DISCARD = (1 << 1),
  LRT_CULL_GENERATED = (1 << 2),
  LRT_TRIANGLE_INTERSECTION_ONLY = (1 << 3),
  LRT_TRIANGLE_NO_INTERSECTION = (1 << 4),
  LRT_TRIANGLE_MAT_BACK_FACE_CULLING = (1 << 5),
  LRT_TRIANGLE_FORCE_INTERSECTION = (1 << 6),
};

#define LRT_SHADOW_MASK_UNDEFINED 0
#define LRT_SHADOW_MASK_ILLUMINATED (1 << 0)
#define LRT_SHADOW_MASK_SHADED (1 << 1)
#define LRT_SHADOW_MASK_ENCLOSED_SHAPE (1 << 2)
#define LRT_SHADOW_MASK_INHIBITED (1 << 3)
#define LRT_SHADOW_SILHOUETTE_ERASED_GROUP (1 << 4)
#define LRT_SHADOW_SILHOUETTE_ERASED_OBJECT (1 << 5)
#define LRT_SHADOW_MASK_ILLUMINATED_SHAPE (1 << 6)

#define LRT_SHADOW_TEST_SHAPE_BITS \
  (LRT_SHADOW_MASK_ILLUMINATED | LRT_SHADOW_MASK_SHADED | LRT_SHADOW_MASK_INHIBITED | \
   LRT_SHADOW_MASK_ILLUMINATED_SHAPE)

/**
 * Controls how many edges a worker thread is processing at one request.
 * There's no significant performance impact on choosing different values.
 * Don't make it too small so that the worker thread won't request too many times.
 */
#define LRT_THREAD_EDGE_COUNT 1000

struct LineartRenderTaskInfo {
  struct LineartData *ld;

  int thread_id;

  /**
   * #pending_edges here only stores a reference to a portion in
   * LineartData::pending_edges, assigned by the occlusion scheduler.
   */
  struct LineartPendingEdges pending_edges;
};

#define LRT_OBINDEX_SHIFT 20
#define LRT_OBINDEX_LOWER 0x0FFFFF    /* Lower 20 bits. */
#define LRT_OBINDEX_HIGHER 0xFFF00000 /* Higher 12 bits. */
#define LRT_EDGE_IDENTIFIER(obi, e) \
  (((uint64_t)(obi->obindex | (e->v1->index & LRT_OBINDEX_LOWER)) << 32) | \
   (obi->obindex | (e->v2->index & LRT_OBINDEX_LOWER)))
#define LRT_LIGHT_CONTOUR_TARGET 0xFFFFFFFF

struct LineartObjectInfo {
  LineartObjectInfo *next;
  Object *original_ob;
  Object *original_ob_eval; /* For evaluated materials */
  Mesh *original_me;
  double model_view_proj[4][4];
  double model_view[4][4];
  double normal[4][4];
  LineartElementLinkNode *v_eln;
  int usage;
  uint8_t override_intersection_mask;
  uint8_t intersection_priority;
  int global_i_offset;

  /* Shifted LRT_OBINDEX_SHIFT bits to be combined with object triangle index. */
  int obindex;

  bool free_use_mesh;

  /** NOTE: Data inside #pending_edges are allocated with MEM_xxx call instead of in pool. */
  LineartPendingEdges pending_edges;
};

struct LineartObjectLoadTaskInfo {
  LineartData *ld;
  int thread_id;
  /* LinkNode styled list */
  LineartObjectInfo *pending;
  /* Used to spread the load across several threads. This can not overflow. */
  uint64_t total_faces;
  ListBase *shadow_elns;
};

/**
 * Bounding area diagram:
 * \code{.txt}
 * +----+ <----U (Upper edge Y value)
 * |    |
 * +----+ <----B (Bottom edge Y value)
 * ^    ^
 * L    R (Left/Right edge X value)
 * \endcode
 *
 * Example structure when subdividing 1 bounding areas:
 * 1 area can be divided into 4 smaller children to
 * accommodate image areas with denser triangle distribution.
 * \code{.txt}
 * +--+--+-----+
 * +--+--+     |
 * +--+--+-----+
 * |     |     |
 * +-----+-----+
 * \endcode
 *
 * lp/rp/up/bp is the list for
 * storing pointers to adjacent bounding areas.
 */
struct LineartBoundingArea {
  double l, r, u, b;
  double cx, cy;

  /** 1,2,3,4 quadrant */
  LineartBoundingArea *child;

  SpinLock lock;

  ListBase lp;
  ListBase rp;
  ListBase up;
  ListBase bp;

  uint32_t triangle_count;
  uint32_t max_triangle_count;
  uint32_t line_count;
  uint32_t max_line_count;
  uint32_t insider_triangle_count;

  /* Use array for speeding up multiple accesses. */
  LineartTriangle **linked_triangles;
  LineartEdge **linked_lines;

  /** Reserved for image space reduction && multi-thread chaining. */
  ListBase linked_chains;
};

#define LRT_TILE(tile, r, c, CCount) tile[r * CCount + c]

#define LRT_CLAMP(a, Min, Max) a = a < Min ? Min : (a > Max ? Max : a)

#define LRT_MAX3_INDEX(a, b, c) (a > b ? (a > c ? 0 : (b > c ? 1 : 2)) : (b > c ? 1 : 2))

#define LRT_MIN3_INDEX(a, b, c) (a < b ? (a < c ? 0 : (b < c ? 1 : 2)) : (b < c ? 1 : 2))

#define LRT_MAX3_INDEX_ABC(x, y, z) (x > y ? (x > z ? a : (y > z ? b : c)) : (y > z ? b : c))

#define LRT_MIN3_INDEX_ABC(x, y, z) (x < y ? (x < z ? a : (y < z ? b : c)) : (y < z ? b : c))

#define DBL_LOOSER 1e-5
#define LRT_DOUBLE_CLOSE_LOOSER(a, b) (((a) + DBL_LOOSER) >= (b) && ((a) - DBL_LOOSER) <= (b))
#define LRT_DOUBLE_CLOSE_ENOUGH(a, b) (((a) + DBL_EDGE_LIM) >= (b) && ((a) - DBL_EDGE_LIM) <= (b))
#define LRT_DOUBLE_CLOSE_ENOUGH_TRI(a, b) \
  (((a) + DBL_TRIANGLE_LIM) >= (b) && ((a) - DBL_TRIANGLE_LIM) <= (b))

#define LRT_CLOSE_LOOSER_v3(a, b) \
  (LRT_DOUBLE_CLOSE_LOOSER(a[0], b[0]) && LRT_DOUBLE_CLOSE_LOOSER(a[1], b[1]) && \
   LRT_DOUBLE_CLOSE_LOOSER(a[2], b[2]))

/* Notes on this function:
 *
 * r_ratio: The ratio on segment a1-a2. When r_ratio is very close to zero or one, it
 * fixes the value to zero or one, this makes it easier to identify "on the tip" situations.
 *
 * r_aligned: True when 1) a and b is exactly on the same straight line and 2) a and b share a
 * common end-point.
 *
 * IMPORTANT: if r_aligned is true, r_ratio will be either 0 or 1 depending on which point from
 * segment a is shared with segment b. If it's a1 then r_ratio is 0, else then r_ratio is 1. This
 * extra information is needed for line art occlusion stage to work correctly in such cases.
 */
BLI_INLINE int lineart_intersect_seg_seg(const double a1[2],
                                         const double a2[2],
                                         const double b1[2],
                                         const double b2[2],
                                         double *r_ratio,
                                         bool *r_aligned)
{
/* Legacy intersection math aligns better with occlusion function quirks. */
// #define USE_VECTOR_LINE_INTERSECTION
#ifdef USE_VECTOR_LINE_INTERSECTION

  /* from isect_line_line_v2_point() */

  double s10[2], s32[2];
  double div;

  sub_v2_v2v2_db(s10, a2, a1);
  sub_v2_v2v2_db(s32, b2, b1);

  div = cross_v2v2_db(s10, s32);
  if (div != 0.0f) {
    const double u = cross_v2v2_db(a2, a1);
    const double v = cross_v2v2_db(b2, b1);

    const double rx = ((s32[0] * u) - (s10[0] * v)) / div;
    const double ry = ((s32[1] * u) - (s10[1] * v)) / div;
    double rr;

    if (fabs(a2[0] - a1[0]) > fabs(a2[1] - a1[1])) {
      *r_ratio = ratiod(a1[0], a2[0], rx);
      if (fabs(b2[0] - b1[0]) > fabs(b2[1] - b1[1])) {
        rr = ratiod(b1[0], b2[0], rx);
      }
      else {
        rr = ratiod(b1[1], b2[1], ry);
      }
      if ((*r_ratio) > 0 && (*r_ratio) < 1 && rr > 0 && rr < 1) {
        return 1;
      }
      return 0;
    }

    *r_ratio = ratiod(a1[1], a2[1], ry);
    if (fabs(b2[0] - b1[0]) > fabs(b2[1] - b1[1])) {
      rr = ratiod(b1[0], b2[0], rx);
    }
    else {
      rr = ratiod(b1[1], b2[1], ry);
    }
    if ((*r_ratio) > 0 && (*r_ratio) < 1 && rr > 0 && rr < 1) {
      return 1;
    }
    return 0;
  }
  return 0;

#else
  double k1, k2;
  double x;
  double y;
  double ratio;
  double x_diff = (a2[0] - a1[0]);
  double x_diff2 = (b2[0] - b1[0]);

  *r_aligned = false;

  if (LRT_DOUBLE_CLOSE_ENOUGH(x_diff, 0)) {
    if (LRT_DOUBLE_CLOSE_ENOUGH(x_diff2, 0)) {
      /* This means two segments are both vertical. */
      if ((LRT_DOUBLE_CLOSE_ENOUGH(a2[0], b1[0]) && LRT_DOUBLE_CLOSE_ENOUGH(a2[1], b1[1])) ||
          (LRT_DOUBLE_CLOSE_ENOUGH(a2[0], b2[0]) && LRT_DOUBLE_CLOSE_ENOUGH(a2[1], b2[1])))
      {
        *r_aligned = true;
        *r_ratio = 1;
      }
      else if ((LRT_DOUBLE_CLOSE_ENOUGH(a1[0], b1[0]) && LRT_DOUBLE_CLOSE_ENOUGH(a1[1], b1[1])) ||
               (LRT_DOUBLE_CLOSE_ENOUGH(a1[0], b2[0]) && LRT_DOUBLE_CLOSE_ENOUGH(a1[1], b2[1])))
      {
        *r_aligned = true;
        *r_ratio = 0;
      }
      return 0;
    }
    double r2 = ratiod(b1[0], b2[0], a1[0]);
    x = interpd(b2[0], b1[0], r2);
    y = interpd(b2[1], b1[1], r2);
    *r_ratio = ratio = ratiod(a1[1], a2[1], y);
  }
  else {
    if (LRT_DOUBLE_CLOSE_ENOUGH(x_diff2, 0)) {
      ratio = ratiod(a1[0], a2[0], b1[0]);
      x = interpd(a2[0], a1[0], ratio);
      *r_ratio = ratio;
    }
    else {
      double y_diff = a2[1] - a1[1], y_diff2 = b2[1] - b1[1];
      k1 = y_diff / x_diff;
      k2 = y_diff2 / x_diff2;

      if (LRT_DOUBLE_CLOSE_ENOUGH_TRI(k2, k1)) {
        /* This means two segments are parallel. This also handles k==0 (both completely
         * horizontal) cases. */
        if ((LRT_DOUBLE_CLOSE_ENOUGH(a2[0], b1[0]) && LRT_DOUBLE_CLOSE_ENOUGH(a2[1], b1[1])) ||
            (LRT_DOUBLE_CLOSE_ENOUGH(a2[0], b2[0]) && LRT_DOUBLE_CLOSE_ENOUGH(a2[1], b2[1])))
        {
          *r_aligned = true;
          *r_ratio = 1;
        }
        else if ((LRT_DOUBLE_CLOSE_ENOUGH(a1[0], b1[0]) &&
                  LRT_DOUBLE_CLOSE_ENOUGH(a1[1], b1[1])) ||
                 (LRT_DOUBLE_CLOSE_ENOUGH(a1[0], b2[0]) && LRT_DOUBLE_CLOSE_ENOUGH(a1[1], b2[1])))
        {
          *r_aligned = true;
          *r_ratio = 0;
        }
        return 0;
      }

      x = (a1[1] - b1[1] - k1 * a1[0] + k2 * b1[0]) / (k2 - k1);

      ratio = (x - a1[0]) / x_diff;

      *r_ratio = ratio;
    }
  }

  if (LRT_DOUBLE_CLOSE_ENOUGH(b1[0], b2[0])) {
    y = interpd(a2[1], a1[1], ratio);
    if (y > std::max(b1[1], b2[1]) || y < std::min(b1[1], b2[1])) {
      return 0;
    }
  }
  else if (ratio <= 0 || ratio > 1 || (b1[0] > b2[0] && x > b1[0]) ||
           (b1[0] < b2[0] && x < b1[0]) || (b2[0] > b1[0] && x > b2[0]) ||
           (b2[0] < b1[0] && x < b2[0]))
  {
    return 0;
  }

  if (LRT_DOUBLE_CLOSE_ENOUGH_TRI(*r_ratio, 1)) {
    *r_ratio = 1;
  }
  else if (LRT_DOUBLE_CLOSE_ENOUGH_TRI(*r_ratio, 0)) {
    *r_ratio = 0;
  }

  return 1;
#endif
}

/* This is a special convenience function to lineart_intersect_seg_seg which will return true when
 * the intersection point falls in the range of a1-a2 but not necessarily in the range of b1-b2. */
BLI_INLINE int lineart_line_isec_2d_ignore_line2pos(const double a1[2],
                                                    const double a2[2],
                                                    const double b1[2],
                                                    const double b2[2],
                                                    double *r_a_ratio)
{
  /* The define here is used to check how vector or slope method handles boundary cases. The result
   * of `lim(div->0)` and `lim(k->0)` could both produce some unwanted flickers in line art, the
   * influence of which is still not fully understood, so keep the switch there for further
   * investigations. */
#define USE_VECTOR_LINE_INTERSECTION_IGN
#ifdef USE_VECTOR_LINE_INTERSECTION_IGN

  /* from isect_line_line_v2_point() */

  double s10[2], s32[2];
  double div;

  sub_v2_v2v2_db(s10, a2, a1);
  sub_v2_v2v2_db(s32, b2, b1);

  div = cross_v2v2_db(s10, s32);
  if (div != 0.0f) {
    const double u = cross_v2v2_db(a2, a1);
    const double v = cross_v2v2_db(b2, b1);

    const double rx = ((s32[0] * u) - (s10[0] * v)) / div;
    const double ry = ((s32[1] * u) - (s10[1] * v)) / div;

    if (fabs(a2[0] - a1[0]) > fabs(a2[1] - a1[1])) {
      *r_a_ratio = ratiod(a1[0], a2[0], rx);
      if ((*r_a_ratio) >= -DBL_EDGE_LIM && (*r_a_ratio) <= 1 + DBL_EDGE_LIM) {
        return 1;
      }
      return 0;
    }

    *r_a_ratio = ratiod(a1[1], a2[1], ry);
    if ((*r_a_ratio) >= -DBL_EDGE_LIM && (*r_a_ratio) <= 1 + DBL_EDGE_LIM) {
      return 1;
    }
    return 0;
  }
  return 0;

#else
  double k1, k2;
  double x;
  double y;
  double ratio;
  double x_diff = (a2[0] - a1[0]);
  double x_diff2 = (b2[0] - b1[0]);

  if (LRT_DOUBLE_CLOSE_ENOUGH(x_diff, 0)) {
    if (LRT_DOUBLE_CLOSE_ENOUGH(x_diff2, 0)) {
      *r_a_ratio = 0;
      return 0;
    }
    double r2 = ratiod(b1[0], b2[0], a1[0]);
    x = interpd(b2[0], b1[0], r2);
    y = interpd(b2[1], b1[1], r2);
    *r_a_ratio = ratio = ratiod(a1[1], a2[1], y);
  }
  else {
    if (LRT_DOUBLE_CLOSE_ENOUGH(x_diff2, 0)) {
      ratio = ratiod(a1[0], a2[0], b1[0]);
      x = interpd(a2[0], a1[0], ratio);
      *r_a_ratio = ratio;
    }
    else {
      k1 = (a2[1] - a1[1]) / x_diff;
      k2 = (b2[1] - b1[1]) / x_diff2;

      if ((k1 == k2)) {
        return 0;
      }
      x = (a1[1] - b1[1] - k1 * a1[0] + k2 * b1[0]) / (k2 - k1);

      ratio = (x - a1[0]) / x_diff;

      *r_a_ratio = ratio;
    }
  }

  if (ratio <= 0 || ratio >= 1) {
    return 0;
  }
  return 1;
#endif
}

struct bGPDframe;
struct bGPDlayer;
struct Depsgraph;
struct LineartGpencilModifierData;
struct GreasePencilLineartModifierData;
struct LineartData;
struct Scene;

void MOD_lineart_destroy_render_data_v3(GreasePencilLineartModifierData *lmd);

void MOD_lineart_chain_feature_lines(LineartData *ld);
void MOD_lineart_chain_split_for_fixed_occlusion(LineartData *ld);
/**
 * This function only connects two different chains. It will not do any clean up or smart chaining.
 * So no: removing overlapping chains, removal of short isolated segments, and no loop reduction is
 * implemented yet.
 */
void MOD_lineart_chain_connect(LineartData *ld);
void MOD_lineart_chain_discard_unused(LineartData *ld, float threshold, uint8_t max_occlusion);
void MOD_lineart_chain_clip_at_border(LineartData *ld);
/**
 * This should always be the last stage!, see the end of
 * #MOD_lineart_chain_split_for_fixed_occlusion().
 */
void MOD_lineart_chain_split_angle(LineartData *ld, float angle_threshold_rad);
void MOD_lineart_smooth_chains(LineartData *ld, float tolerance);
void MOD_lineart_chain_offset_towards_camera(LineartData *ld, float dist, bool use_custom_camera);
void MOD_lineart_chain_find_silhouette_backdrop_objects(LineartData *ld);

int MOD_lineart_chain_count(const LineartEdgeChain *ec);
void MOD_lineart_chain_clear_picked_flag(LineartCache *lc);
void MOD_lineart_finalize_chains(LineartData *ld);

/**
 * This is the entry point of all line art calculations.
 *
 * \return True when a change is made.
 */
bool MOD_lineart_compute_feature_lines_v3(Depsgraph *depsgraph,
                                          GreasePencilLineartModifierData &lmd,
                                          LineartCache **cached_result,
                                          bool enable_stroke_depth_offset);

/**
 * This only gets initial "biggest" tile.
 */
LineartBoundingArea *MOD_lineart_get_parent_bounding_area(LineartData *ld, double x, double y);

/**
 * Wrapper for more convenience.
 */
LineartBoundingArea *MOD_lineart_get_bounding_area(LineartData *ld, double x, double y);

namespace blender::bke::greasepencil {
class Drawing;
}
void MOD_lineart_gpencil_generate_v3(const LineartCache *cache,
                                     const blender::float4x4 &mat,
                                     Depsgraph *depsgraph,
                                     blender::bke::greasepencil::Drawing &drawing,
                                     int8_t source_type,
                                     Object *source_object,
                                     Collection *source_collection,
                                     int level_start,
                                     int level_end,
                                     int mat_nr,
                                     int16_t edge_types,
                                     uchar mask_switches,
                                     uchar material_mask_bits,
                                     uchar intersection_mask,
                                     float thickness,
                                     float opacity,
                                     uchar shadow_selection,
                                     uchar silhouette_mode,
                                     const char *source_vgname,
                                     const char *vgname,
                                     int modifier_flags,
                                     int modifier_calculation_flags);

/**
 * Length is in image space.
 */
float MOD_lineart_chain_compute_length(LineartEdgeChain *ec);

LineartCache *MOD_lineart_init_cache();
void MOD_lineart_clear_cache(LineartCache **lc);
