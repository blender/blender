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

  /* Material flag is removed to save space. */
  unsigned char transparency_mask;
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

typedef struct LineartLineSegment {
  struct LineartLineSegment *next, *prev;
  /** at==0: left  at==1: right  (this is in 2D projected space) */
  double at;
  /** Occlusion level after "at" point */
  unsigned char occlusion;

  /**
   * For determining lines behind a glass window material.
   * the size of this variable should also be dynamically decided, 1 byte to 8 byte,
   * allows 8 to 64 materials for "transparent mask". 1 byte (8 materials) should be
   * enough for most cases.
   */
  unsigned char transparency_mask;
} LineartLineSegment;

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

  /**
   * Still need this entry because culled lines will not add to object
   * #LineartElementLinkNode node (known as `reln` internally).
   *
   * TODO: If really need more savings, we can allocate this in a "extended" way too, but we need
   * another bit in flags to be able to show the difference.
   */
  struct Object *object_ref;
} LineartEdge;

typedef struct LineartLineChain {
  struct LineartLineChain *next, *prev;
  ListBase chain;

  /** Calculated before draw command. */
  float length;

  /** Used when re-connecting and grease-pencil stroke generation. */
  char picked;
  char level;

  /** Chain now only contains one type of segments */
  int type;
  unsigned char transparency_mask;

  struct Object *object_ref;
} LineartLineChain;

typedef struct LineartLineChainItem {
  struct LineartLineChainItem *next, *prev;
  /** Need z value for fading */
  float pos[3];
  /** For restoring position to 3d space */
  float gpos[3];
  float normal[3];
  char line_type;
  char occlusion;
  unsigned char transparency_mask;
  size_t index;
} LineartLineChainItem;

typedef struct LineartChainRegisterEntry {
  struct LineartChainRegisterEntry *next, *prev;
  LineartLineChain *rlc;
  LineartLineChainItem *rlci;
  char picked;

  /* left/right mark.
   * Because we revert list in chaining so we need the flag. */
  char is_left;
} LineartChainRegisterEntry;

typedef struct LineartRenderBuffer {
  struct LineartRenderBuffer *prev, *next;

  int thread_count;

  int w, h;
  int tile_size_w, tile_size_h;
  int tile_count_x, tile_count_y;
  double width_per_tile, height_per_tile;
  double view_projection[4][4];

  struct LineartBoundingArea *initial_bounding_areas;
  unsigned int bounding_area_count;

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

  /*  Render status */
  double view_vector[3];

  int triangle_size;

  unsigned int contour_count;
  unsigned int contour_processed;
  LineartEdge *contour_managed;
  /** A single linked list (cast to #LinkNode). */
  LineartEdge *contours;

  unsigned int intersection_count;
  unsigned int intersection_processed;
  LineartEdge *intersection_managed;
  LineartEdge *intersection_lines;

  unsigned int crease_count;
  unsigned int crease_processed;
  LineartEdge *crease_managed;
  LineartEdge *crease_lines;

  unsigned int material_line_count;
  unsigned int material_processed;
  LineartEdge *material_managed;
  LineartEdge *material_lines;

  unsigned int edge_mark_count;
  unsigned int edge_mark_processed;
  LineartEdge *edge_mark_managed;
  LineartEdge *edge_marks;

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
  bool fuzzy_intersections;
  bool fuzzy_everything;
  bool allow_boundaries;
  bool allow_overlapping_edges;
  bool remove_doubles;

  /* Keep an copy of these data so when line art is running it's self-contained. */
  bool cam_is_persp;
  float cam_obmat[4][4];
  double camera_pos[3];
  double near_clip, far_clip;
  float shift_x, shift_y;
  float crease_threshold;
  float chaining_image_threshold;
  float angle_splitting_threshold;

  /* FIXME(Yiming): Temporary solution for speeding up calculation by not including lines that
   * are not in the selected source. This will not be needed after we have a proper scene-wise
   * cache running because multiple modifiers can then select results from that without further
   * calculation. */
  int _source_type;
  struct Collection *_source_collection;
  struct Object *_source_object;

} LineartRenderBuffer;

#define DBL_TRIANGLE_LIM 1e-8
#define DBL_EDGE_LIM 1e-9

#define LRT_MEMORY_POOL_64MB (1 << 26)

typedef enum eLineartTriangleFlags {
  LRT_CULL_DONT_CARE = 0,
  LRT_CULL_USED = (1 << 0),
  LRT_CULL_DISCARD = (1 << 1),
  LRT_CULL_GENERATED = (1 << 2),
  LRT_TRIANGLE_INTERSECTION_ONLY = (1 << 3),
  LRT_TRIANGLE_NO_INTERSECTION = (1 << 4),
} eLineartTriangleFlags;

/** Controls how many edges a worker thread is processing at one request.
 * There's no significant performance impact on choosing different values.
 * Don't make it too small so that the worker thread won't request too many times. */
#define LRT_THREAD_EDGE_COUNT 1000

typedef struct LineartRenderTaskInfo {
  struct LineartRenderBuffer *rb;

  int thread_id;

  LineartEdge *contour;
  LineartEdge *contour_end;

  LineartEdge *intersection;
  LineartEdge *intersection_end;

  LineartEdge *crease;
  LineartEdge *crease_end;

  LineartEdge *material;
  LineartEdge *material_end;

  LineartEdge *edge_mark;
  LineartEdge *edge_mark_end;

} LineartRenderTaskInfo;

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

  short triangle_count;

  ListBase linked_triangles;
  ListBase linked_lines;

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

#define LRT_DOUBLE_CLOSE_ENOUGH(a, b) (((a) + DBL_EDGE_LIM) >= (b) && ((a)-DBL_EDGE_LIM) <= (b))

BLI_INLINE int lineart_LineIntersectTest2d(
    const double *a1, const double *a2, const double *b1, const double *b2, double *aRatio)
{
#define USE_VECTOR_LINE_INTERSECTION
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
      *aRatio = ratiod(a1[0], a2[0], rx);
      if (fabs(b2[0] - b1[0]) > fabs(b2[1] - b1[1])) {
        rr = ratiod(b1[0], b2[0], rx);
      }
      else {
        rr = ratiod(b1[1], b2[1], ry);
      }
      if ((*aRatio) > 0 && (*aRatio) < 1 && rr > 0 && rr < 1) {
        return 1;
      }
      return 0;
    }

    *aRatio = ratiod(a1[1], a2[1], ry);
    if (fabs(b2[0] - b1[0]) > fabs(b2[1] - b1[1])) {
      rr = ratiod(b1[0], b2[0], rx);
    }
    else {
      rr = ratiod(b1[1], b2[1], ry);
    }
    if ((*aRatio) > 0 && (*aRatio) < 1 && rr > 0 && rr < 1) {
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
      *aRatio = 0;
      return 0;
    }
    double r2 = ratiod(b1[0], b2[0], a1[0]);
    x = interpd(b2[0], b1[0], r2);
    y = interpd(b2[1], b1[1], r2);
    *aRatio = ratio = ratiod(a1[1], a2[1], y);
  }
  else {
    if (LRT_DOUBLE_CLOSE_ENOUGH(x_diff2, 0)) {
      ratio = ratiod(a1[0], a2[0], b1[0]);
      x = interpd(a2[0], a1[0], ratio);
      *aRatio = ratio;
    }
    else {
      k1 = (a2[1] - a1[1]) / x_diff;
      k2 = (b2[1] - b1[1]) / x_diff2;

      if ((k1 == k2))
        return 0;

      x = (a1[1] - b1[1] - k1 * a1[0] + k2 * b1[0]) / (k2 - k1);

      ratio = (x - a1[0]) / x_diff;

      *aRatio = ratio;
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

  return 1;
#endif
}

struct Depsgraph;
struct Scene;
struct LineartRenderBuffer;
struct LineartGpencilModifierData;

void MOD_lineart_destroy_render_data(struct LineartGpencilModifierData *lmd);

void MOD_lineart_chain_feature_lines(LineartRenderBuffer *rb);
void MOD_lineart_chain_split_for_fixed_occlusion(LineartRenderBuffer *rb);
void MOD_lineart_chain_connect(LineartRenderBuffer *rb);
void MOD_lineart_chain_discard_short(LineartRenderBuffer *rb, const float threshold);
void MOD_lineart_chain_split_angle(LineartRenderBuffer *rb, float angle_threshold_rad);

int MOD_lineart_chain_count(const LineartLineChain *rlc);
void MOD_lineart_chain_clear_picked_flag(struct LineartRenderBuffer *rb);

bool MOD_lineart_compute_feature_lines(struct Depsgraph *depsgraph,
                                       struct LineartGpencilModifierData *lmd);

struct Scene;

LineartBoundingArea *MOD_lineart_get_parent_bounding_area(LineartRenderBuffer *rb,
                                                          double x,
                                                          double y);

LineartBoundingArea *MOD_lineart_get_bounding_area(LineartRenderBuffer *rb, double x, double y);

struct bGPDlayer;
struct bGPDframe;

void MOD_lineart_gpencil_generate(LineartRenderBuffer *rb,
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
                                  unsigned char transparency_flags,
                                  unsigned char transparency_mask,
                                  short thickness,
                                  float opacity,
                                  const char *source_vgname,
                                  const char *vgname,
                                  int modifier_flags);

float MOD_lineart_chain_compute_length(LineartLineChain *rlc);

void ED_operatortypes_lineart(void);
