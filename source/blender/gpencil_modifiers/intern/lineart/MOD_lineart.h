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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup editors
 */

#pragma once

#include "BLI_linklist.h"
#include "BLI_listbase.h"
#include "BLI_math.h" /* Needed here for inline functions. */
#include "BLI_threads.h"

#include <math.h>

typedef struct LineartStaticMemPoolNode {
  Link item;
  size_t size;
  size_t used_byte;
  /* User memory starts here */
} LineartStaticMemPoolNode;

typedef struct LineartStaticMemPool {
  ListBase pools;
  SpinLock lock_mem;
} LineartStaticMemPool;

typedef struct LineartTriangleAdjacent {
  struct LineartEdge *e[3];
} LineartTriangleAdjacent;

typedef struct LineartTriangle {
  struct LineartVert *v[3];

  /* first culled in line list to use adjacent triangle info, then go through triangle list. */
  double gn[3];

  unsigned char material_mask_bits;
  unsigned char intersection_mask;
  unsigned char mat_occlusion;
  unsigned char flags; /* #eLineartTriangleFlags */

  /**
   * Only use single link list, because we don't need to go back in order.
   * This variable is also reused to store the pointer to adjacent lines of this triangle before
   * intersection stage.
   */
  struct LinkNode *intersecting_verts;
} LineartTriangle;

typedef struct LineartTriangleThread {
  struct LineartTriangle base;
  /**
   * This variable is used to store per-thread triangle-line testing pair,
   * also re-used to store triangle-triangle pair for intersection testing stage.
   * Do not directly use #LineartTriangleThread.
   * The size of #LineartTriangle is dynamically allocated to contain set thread number of
   * "testing_e" field. Worker threads will test lines against the "base" triangle.
   * At least one thread is present, thus we always have at least `testing_e[0]`.
   */
  struct LineartEdge *testing_e[1];
} LineartTriangleThread;

typedef enum eLineArtElementNodeFlag {
  LRT_ELEMENT_IS_ADDITIONAL = (1 << 0),
  LRT_ELEMENT_BORDER_ONLY = (1 << 1),
  LRT_ELEMENT_NO_INTERSECTION = (1 << 2),
} eLineArtElementNodeFlag;

typedef struct LineartElementLinkNode {
  struct LineartElementLinkNode *next, *prev;
  void *pointer;
  int element_count;
  void *object_ref;
  eLineArtElementNodeFlag flags;

  /** Per object value, always set, if not enabled by #ObjectLineArt, then it's set to global. */
  float crease_threshold;
} LineartElementLinkNode;

typedef struct LineartEdgeSegment {
  struct LineartEdgeSegment *next, *prev;
  /** at==0: left  at==1: right  (this is in 2D projected space) */
  double at;
  /** Occlusion level after "at" point */
  unsigned char occlusion;

  /* Used to filter line art occlusion edges */
  unsigned char material_mask_bits;
} LineartEdgeSegment;

typedef struct LineartVert {
  double gloc[3];
  double fbcoord[4];

  /* Scene global index. */
  int index;

  /**
   * Intersection data flag is here, when LRT_VERT_HAS_INTERSECTION_DATA is set,
   * size of the struct is extended to include intersection data.
   * See #eLineArtVertFlags.
   */
  char flag;

} LineartVert;

typedef struct LineartVertIntersection {
  struct LineartVert base;
  /** Use vert index because we only use this to check vertex equal. This way we save 8 Bytes. */
  int isec1, isec2;
  struct LineartTriangle *intersecting_with;
} LineartVertIntersection;

typedef enum eLineArtVertFlags {
  LRT_VERT_HAS_INTERSECTION_DATA = (1 << 0),
  LRT_VERT_EDGE_USED = (1 << 1),
} eLineArtVertFlags;

typedef struct LineartEdge {
  /** We only need link node kind of list here. */
  struct LineartEdge *next;
  struct LineartVert *v1, *v2;
  /**
   * Local vertex index for two ends, not pouting in #RenderVert because all verts are loaded, so
   * as long as fewer than half of the mesh edges are becoming a feature line, we save more memory.
   */
  int v1_obindex, v2_obindex;
  struct LineartTriangle *t1, *t2;
  ListBase segments;
  char min_occ;

  /** Also for line type determination on chaining. */
  unsigned char flags;
  unsigned char intersection_mask;

  /**
   * Still need this entry because culled lines will not add to object
   * #LineartElementLinkNode node (known as `eln` internally).
   *
   * TODO: If really need more savings, we can allocate this in a "extended" way too, but we need
   * another bit in flags to be able to show the difference.
   */
  struct Object *object_ref;
} LineartEdge;

typedef struct LineartEdgeChain {
  struct LineartEdgeChain *next, *prev;
  ListBase chain;

  /** Calculated before draw command. */
  float length;

  /** Used when re-connecting and grease-pencil stroke generation. */
  char picked;
  char level;

  /** Chain now only contains one type of segments */
  int type;
  unsigned char material_mask_bits;
  unsigned char intersection_mask;

  struct Object *object_ref;
} LineartEdgeChain;

typedef struct LineartEdgeChainItem {
  struct LineartEdgeChainItem *next, *prev;
  /** Need z value for fading, w value for image frame clipping. */
  float pos[4];
  /** For restoring position to 3d space. */
  float gpos[3];
  float normal[3];
  unsigned char line_type;
  char occlusion;
  unsigned char material_mask_bits;
  unsigned char intersection_mask;
  size_t index;
} LineartEdgeChainItem;

typedef struct LineartChainRegisterEntry {
  struct LineartChainRegisterEntry *next, *prev;
  LineartEdgeChain *ec;
  LineartEdgeChainItem *eci;
  char picked;

  /* left/right mark.
   * Because we revert list in chaining so we need the flag. */
  char is_left;
} LineartChainRegisterEntry;

enum eLineArtTileRecursiveLimit {
  /* If tile gets this small, it's already much smaller than a pixel. No need to continue
   * splitting. */
  LRT_TILE_RECURSIVE_PERSPECTIVE = 16,
  /* This is a tried-and-true safe value for high poly models that also needed ortho rendering. */
  LRT_TILE_RECURSIVE_ORTHO = 10,
};

#define LRT_TILE_SPLITTING_TRIANGLE_LIMIT 100
#define LRT_TILE_EDGE_COUNT_INITIAL 32

typedef struct LineartRenderBuffer {
  struct LineartRenderBuffer *prev, *next;

  int thread_count;

  int w, h;
  int tile_size_w, tile_size_h;
  int tile_count_x, tile_count_y;
  double width_per_tile, height_per_tile;
  double view_projection[4][4];
  double view[4][4];

  float overscan;

  struct LineartBoundingArea *initial_bounding_areas;
  unsigned int bounding_area_count;

  /* When splitting bounding areas, if there's an ortho camera placed at a straight angle, there
   * will be a lot of triangles aligned in line which can not be separated by continue subdividing
   * the tile. So we set a strict limit when using ortho camera. See eLineArtTileRecursiveLimit. */
  int tile_recursive_level;

  ListBase vertex_buffer_pointers;
  ListBase line_buffer_pointers;
  ListBase triangle_buffer_pointers;

  /** This one's memory is not from main pool and is free()ed after culling stage. */
  ListBase triangle_adjacent_pointers;

  ListBase intersecting_vertex_buffer;
  /** Use the one comes with Line Art. */
  LineartStaticMemPool render_data_pool;
  ListBase wasted_cuts;
  SpinLock lock_cuts;

  /* This is just a pointer to LineartCache::chain_data_pool, which acts as a cache for line
   * chains. */
  LineartStaticMemPool *chain_data_pool;

  /*  Render status */
  double view_vector[3];

  int triangle_size;

  /* Although using ListBase here, LineartEdge is single linked list.
   * list.last is used to store worker progress along the list.
   * See lineart_main_occlusion_begin() for more info. */
  ListBase contour;
  ListBase intersection;
  ListBase crease;
  ListBase material;
  ListBase edge_mark;
  ListBase floating;

  ListBase chains;

  /* For managing calculation tasks for multiple threads. */
  SpinLock lock_task;

  /*  settings */

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

  bool filter_face_mark;
  bool filter_face_mark_invert;
  bool filter_face_mark_boundaries;

  bool force_crease;
  bool sharp_as_crease;

  /* Keep an copy of these data so when line art is running it's self-contained. */
  bool cam_is_persp;
  float cam_obmat[4][4];
  double camera_pos[3];
  double active_camera_pos[3]; /* Stroke offset calculation may use active or selected camera. */
  double near_clip, far_clip;
  float shift_x, shift_y;
  float crease_threshold;
  float chaining_image_threshold;
  float angle_splitting_threshold;

  float chain_smooth_tolerance;

  /* FIXME(Yiming): Temporary solution for speeding up calculation by not including lines that
   * are not in the selected source. This will not be needed after we have a proper scene-wise
   * cache running because multiple modifiers can then select results from that without further
   * calculation. */
  int _source_type;
  struct Collection *_source_collection;
  struct Object *_source_object;

} LineartRenderBuffer;

typedef struct LineartCache {
  /** Separate memory pool for chain data, this goes to the cache, so when we free the main pool,
   * chains will still be available. */
  LineartStaticMemPool chain_data_pool;

  /** A copy of rb->chains so we have that data available after rb has been destroyed. */
  ListBase chains;

  /** Cache only contains edge types specified in this variable. */
  char rb_edge_types;
} LineartCache;

#define DBL_TRIANGLE_LIM 1e-8
#define DBL_EDGE_LIM 1e-9

#define LRT_MEMORY_POOL_1MB (1 << 20)

typedef enum eLineartTriangleFlags {
  LRT_CULL_DONT_CARE = 0,
  LRT_CULL_USED = (1 << 0),
  LRT_CULL_DISCARD = (1 << 1),
  LRT_CULL_GENERATED = (1 << 2),
  LRT_TRIANGLE_INTERSECTION_ONLY = (1 << 3),
  LRT_TRIANGLE_NO_INTERSECTION = (1 << 4),
} eLineartTriangleFlags;

/**
 * Controls how many edges a worker thread is processing at one request.
 * There's no significant performance impact on choosing different values.
 * Don't make it too small so that the worker thread won't request too many times.
 */
#define LRT_THREAD_EDGE_COUNT 1000

typedef struct LineartRenderTaskInfo {
  struct LineartRenderBuffer *rb;

  int thread_id;

  /* These lists only denote the part of the main edge list that the thread should iterate over.
   * Be careful to not iterate outside of these bounds as it is not thread safe to do so. */
  ListBase contour;
  ListBase intersection;
  ListBase crease;
  ListBase material;
  ListBase edge_mark;
  ListBase floating;

} LineartRenderTaskInfo;

typedef struct LineartObjectInfo {
  struct LineartObjectInfo *next;
  struct Object *original_ob;
  struct Mesh *original_me;
  double model_view_proj[4][4];
  double model_view[4][4];
  double normal[4][4];
  LineartElementLinkNode *v_eln;
  int usage;
  uint8_t override_intersection_mask;
  int global_i_offset;

  bool free_use_mesh;

  /* Threads will add lines inside here, when all threads are done, we combine those into the
   * ones in LineartRenderBuffer. */
  ListBase contour;
  ListBase intersection;
  ListBase crease;
  ListBase material;
  ListBase edge_mark;
  ListBase floating;

} LineartObjectInfo;

typedef struct LineartObjectLoadTaskInfo {
  struct LineartRenderBuffer *rb;
  struct Depsgraph *dg;
  /* LinkNode styled list */
  LineartObjectInfo *pending;
  /* Used to spread the load across several threads. This can not overflow. */
  long unsigned int total_faces;
} LineartObjectLoadTaskInfo;

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
typedef struct LineartBoundingArea {
  double l, r, u, b;
  double cx, cy;

  /** 1,2,3,4 quadrant */
  struct LineartBoundingArea *child;

  ListBase lp;
  ListBase rp;
  ListBase up;
  ListBase bp;

  uint16_t triangle_count;
  uint16_t max_triangle_count;
  uint16_t line_count;
  uint16_t max_line_count;

  /* Use array for speeding up multiple accesses. */
  struct LineartTriangle **linked_triangles;
  struct LineartEdge **linked_lines;

  /** Reserved for image space reduction && multi-thread chaining. */
  ListBase linked_chains;
} LineartBoundingArea;

#define LRT_TILE(tile, r, c, CCount) tile[r * CCount + c]

#define LRT_CLAMP(a, Min, Max) a = a < Min ? Min : (a > Max ? Max : a)

#define LRT_MAX3_INDEX(a, b, c) (a > b ? (a > c ? 0 : (b > c ? 1 : 2)) : (b > c ? 1 : 2))

#define LRT_MIN3_INDEX(a, b, c) (a < b ? (a < c ? 0 : (b < c ? 1 : 2)) : (b < c ? 1 : 2))

#define LRT_MAX3_INDEX_ABC(x, y, z) (x > y ? (x > z ? a : (y > z ? b : c)) : (y > z ? b : c))

#define LRT_MIN3_INDEX_ABC(x, y, z) (x < y ? (x < z ? a : (y < z ? b : c)) : (y < z ? b : c))

#define LRT_ABC(index) (index == 0 ? a : (index == 1 ? b : c))
#define LRT_PABC(index) (index == 0 ? pa : (index == 1 ? pb : pc))

#define DBL_LOOSER 1e-5
#define LRT_DOUBLE_CLOSE_LOOSER(a, b) (((a) + DBL_LOOSER) >= (b) && ((a)-DBL_LOOSER) <= (b))
#define LRT_DOUBLE_CLOSE_ENOUGH(a, b) (((a) + DBL_EDGE_LIM) >= (b) && ((a)-DBL_EDGE_LIM) <= (b))
#define LRT_DOUBLE_CLOSE_ENOUGH_TRI(a, b) \
  (((a) + DBL_TRIANGLE_LIM) >= (b) && ((a)-DBL_TRIANGLE_LIM) <= (b))

/* Notes on this function:
 *
 * r_ratio: The ratio on segment a1-a2. When r_ratio is very close to zero or one, it
 * fixes the value to zero or one, this makes it easier to identify "on the tip" situations.
 *
 * r_aligned: True when 1) a and b is exactly on the same straight line and 2) a and b share a
 * common end-point.
 *
 * Important: if r_aligned is true, r_ratio will be either 0 or 1 depending on which point from
 * segment a is shared with segment b. If it's a1 then r_ratio is 0, else then r_ratio is 1. This
 * extra information is needed for line art occlusion stage to work correctly in such cases.
 */
BLI_INLINE int lineart_intersect_seg_seg(const double *a1,
                                         const double *a2,
                                         const double *b1,
                                         const double *b2,
                                         double *r_ratio,
                                         bool *r_aligned)
{
/* Legacy intersection math aligns better with occlusion function quirks. */
/* #define USE_VECTOR_LINE_INTERSECTION */
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
          (LRT_DOUBLE_CLOSE_ENOUGH(a2[0], b2[0]) && LRT_DOUBLE_CLOSE_ENOUGH(a2[1], b2[1]))) {
        *r_aligned = true;
        *r_ratio = 1;
      }
      else if ((LRT_DOUBLE_CLOSE_ENOUGH(a1[0], b1[0]) && LRT_DOUBLE_CLOSE_ENOUGH(a1[1], b1[1])) ||
               (LRT_DOUBLE_CLOSE_ENOUGH(a1[0], b2[0]) && LRT_DOUBLE_CLOSE_ENOUGH(a1[1], b2[1]))) {
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
            (LRT_DOUBLE_CLOSE_ENOUGH(a2[0], b2[0]) && LRT_DOUBLE_CLOSE_ENOUGH(a2[1], b2[1]))) {
          *r_aligned = true;
          *r_ratio = 1;
        }
        else if ((LRT_DOUBLE_CLOSE_ENOUGH(a1[0], b1[0]) &&
                  LRT_DOUBLE_CLOSE_ENOUGH(a1[1], b1[1])) ||
                 (LRT_DOUBLE_CLOSE_ENOUGH(a1[0], b2[0]) &&
                  LRT_DOUBLE_CLOSE_ENOUGH(a1[1], b2[1]))) {
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
    if (y > MAX2(b1[1], b2[1]) || y < MIN2(b1[1], b2[1]))
      return 0;
  }
  else if (ratio <= 0 || ratio > 1 || (b1[0] > b2[0] && x > b1[0]) ||
           (b1[0] < b2[0] && x < b1[0]) || (b2[0] > b1[0] && x > b2[0]) ||
           (b2[0] < b1[0] && x < b2[0]))
    return 0;

  if (LRT_DOUBLE_CLOSE_ENOUGH_TRI(*r_ratio, 1)) {
    *r_ratio = 1;
  }
  else if (LRT_DOUBLE_CLOSE_ENOUGH_TRI(*r_ratio, 0)) {
    *r_ratio = 0;
  }

  return 1;
#endif
}

struct Depsgraph;
struct LineartGpencilModifierData;
struct LineartRenderBuffer;
struct Scene;

void MOD_lineart_destroy_render_data(struct LineartGpencilModifierData *lmd);

void MOD_lineart_chain_feature_lines(LineartRenderBuffer *rb);
void MOD_lineart_chain_split_for_fixed_occlusion(LineartRenderBuffer *rb);
/**
 * This function only connects two different chains. It will not do any clean up or smart chaining.
 * So no: removing overlapping chains, removal of short isolated segments, and no loop reduction is
 * implemented yet.
 */
void MOD_lineart_chain_connect(LineartRenderBuffer *rb);
void MOD_lineart_chain_discard_short(LineartRenderBuffer *rb, const float threshold);
void MOD_lineart_chain_clip_at_border(LineartRenderBuffer *rb);
/**
 * This should always be the last stage!, see the end of
 * #MOD_lineart_chain_split_for_fixed_occlusion().
 */
void MOD_lineart_chain_split_angle(LineartRenderBuffer *rb, float angle_threshold_rad);
void MOD_lineart_smooth_chains(LineartRenderBuffer *rb, float tolerance);
void MOD_lineart_chain_offset_towards_camera(LineartRenderBuffer *rb,
                                             float dist,
                                             bool use_custom_camera);

int MOD_lineart_chain_count(const LineartEdgeChain *ec);
void MOD_lineart_chain_clear_picked_flag(LineartCache *lc);

/**
 * This is the entry point of all line art calculations.
 *
 * \return True when a change is made.
 */
bool MOD_lineart_compute_feature_lines(struct Depsgraph *depsgraph,
                                       struct LineartGpencilModifierData *lmd,
                                       struct LineartCache **cached_result,
                                       bool enable_stroke_offset);

struct Scene;

/**
 * This only gets initial "biggest" tile.
 */
LineartBoundingArea *MOD_lineart_get_parent_bounding_area(LineartRenderBuffer *rb,
                                                          double x,
                                                          double y);

/**
 * Wrapper for more convenience.
 */
LineartBoundingArea *MOD_lineart_get_bounding_area(LineartRenderBuffer *rb, double x, double y);

struct bGPDframe;
struct bGPDlayer;

/**
 * Wrapper for external calls.
 */
void MOD_lineart_gpencil_generate(LineartCache *cache,
                                  struct Depsgraph *depsgraph,
                                  struct Object *ob,
                                  struct bGPDlayer *gpl,
                                  struct bGPDframe *gpf,
                                  char source_type,
                                  void *source_reference,
                                  int level_start,
                                  int level_end,
                                  int mat_nr,
                                  short edge_types,
                                  unsigned char mask_switches,
                                  unsigned char material_mask_bits,
                                  unsigned char intersection_mask,
                                  short thickness,
                                  float opacity,
                                  const char *source_vgname,
                                  const char *vgname,
                                  int modifier_flags);

/**
 * Length is in image space.
 */
float MOD_lineart_chain_compute_length(LineartEdgeChain *ec);

void ED_operatortypes_lineart(void);
