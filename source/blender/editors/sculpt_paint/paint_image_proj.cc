/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 * \brief Functions to paint images in 2D and 3D.
 */

#include <algorithm>
#include <cfloat>
#include <climits>
#include <cmath>
#include <cstring>

#include "MEM_guardedalloc.h"

#ifdef WIN32
#  include "BLI_winstuff.h"
#endif

#include "BLI_linklist.h"
#include "BLI_listbase.h"
#include "BLI_math_base_safe.h"
#include "BLI_math_bits.h"
#include "BLI_math_color.h"
#include "BLI_math_color_blend.h"
#include "BLI_math_geom.h"
#include "BLI_math_vector.hh"
#include "BLI_memarena.h"
#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_task.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "atomic_ops.h"

#include "BLT_translation.hh"

#include "IMB_imbuf.hh"
#include "IMB_interp.hh"

#include "DNA_brush_types.h"
#include "DNA_customdata_types.h"
#include "DNA_defs.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_node_types.h"
#include "DNA_object_enums.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BKE_attribute.h"
#include "BKE_attribute.hh"
#include "BKE_attribute_legacy_convert.hh"
#include "BKE_brush.hh"
#include "BKE_camera.h"
#include "BKE_colorband.hh"
#include "BKE_colortools.hh"
#include "BKE_context.hh"
#include "BKE_customdata.hh"
#include "BKE_global.hh"
#include "BKE_idprop.hh"
#include "BKE_image.hh"
#include "BKE_layer.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_main_invariants.hh"
#include "BKE_material.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.hh"
#include "BKE_node.hh"
#include "BKE_node_legacy_types.hh"
#include "BKE_node_runtime.hh"
#include "BKE_object.hh"
#include "BKE_paint.hh"
#include "BKE_paint_types.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"
#include "BKE_screen.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "ED_image.hh"
#include "ED_node.hh"
#include "ED_object.hh"
#include "ED_paint.hh"
#include "ED_screen.hh"
#include "ED_sculpt.hh"
#include "ED_uvedit.hh"
#include "ED_view3d.hh"
#include "ED_view3d_offscreen.hh"

#include "GPU_capabilities.hh"
#include "GPU_init_exit.hh"

#include "NOD_shader.h"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"
#include "RNA_types.hh"

#include "IMB_colormanagement.hh"

// #include "bmesh_tools.hh"

#include "paint_intern.hh"

using blender::int3;

static void partial_redraw_array_init(ImagePaintPartialRedraw *pr);

/* Defines and Structs */
/* unit_float_to_uchar_clamp as inline function */
BLI_INLINE uchar f_to_char(const float val)
{
  return unit_float_to_uchar_clamp(val);
}

/* ProjectionPaint defines */

/* approx the number of buckets to have under the brush,
 * used with the brush size to set the ps->buckets_x and ps->buckets_y value.
 *
 * When 3 - a brush should have ~9 buckets under it at once
 * ...this helps for threading while painting as well as
 * avoiding initializing pixels that won't touch the brush */
#define PROJ_BUCKET_BRUSH_DIV 4

#define PROJ_BUCKET_RECT_MIN 4
#define PROJ_BUCKET_RECT_MAX 256

#define PROJ_BOUNDBOX_DIV 8
#define PROJ_BOUNDBOX_SQUARED (PROJ_BOUNDBOX_DIV * PROJ_BOUNDBOX_DIV)

// #define PROJ_DEBUG_PAINT 1
// #define PROJ_DEBUG_NOSEAMBLEED 1
// #define PROJ_DEBUG_PRINT_CLIP 1
#define PROJ_DEBUG_WINCLIP 1

#ifndef PROJ_DEBUG_NOSEAMBLEED
/* projectFaceSeamFlags options */
// #define PROJ_FACE_IGNORE  (1<<0)  /* When the face is hidden, back-facing or occluded. */
// #define PROJ_FACE_INIT    (1<<1)  /* When we have initialized the faces data */

/* If this face has a seam on any of its edges. */
#  define PROJ_FACE_SEAM0 (1 << 0)
#  define PROJ_FACE_SEAM1 (1 << 1)
#  define PROJ_FACE_SEAM2 (1 << 2)

#  define PROJ_FACE_NOSEAM0 (1 << 4)
#  define PROJ_FACE_NOSEAM1 (1 << 5)
#  define PROJ_FACE_NOSEAM2 (1 << 6)

/* If the seam is completely initialized, including adjacent seams. */
#  define PROJ_FACE_SEAM_INIT0 (1 << 8)
#  define PROJ_FACE_SEAM_INIT1 (1 << 9)
#  define PROJ_FACE_SEAM_INIT2 (1 << 10)

#  define PROJ_FACE_DEGENERATE (1 << 12)

/* face winding */
#  define PROJ_FACE_WINDING_INIT 1
#  define PROJ_FACE_WINDING_CW 2

/* a slightly scaled down face is used to get fake 3D location for edge pixels in the seams
 * as this number approaches  1.0f the likelihood increases of float precision errors where
 * it is occluded by an adjacent face */
#  define PROJ_FACE_SCALE_SEAM 0.99f
#endif /* PROJ_DEBUG_NOSEAMBLEED */

#define PROJ_SRC_VIEW 1
#define PROJ_SRC_IMAGE_CAM 2
#define PROJ_SRC_IMAGE_VIEW 3
#define PROJ_SRC_VIEW_FILL 4

#define PROJ_VIEW_DATA_ID "view_data"
/* viewmat + winmat + clip_start + clip_end + is_ortho */
#define PROJ_VIEW_DATA_SIZE (4 * 4 + 4 * 4 + 3)

#define PROJ_BUCKET_NULL 0
#define PROJ_BUCKET_INIT (1 << 0)
// #define PROJ_BUCKET_CLONE_INIT   (1<<1)

/* used for testing doubles, if a point is on a line etc */
#define PROJ_GEOM_TOLERANCE 0.00075f
#define PROJ_PIXEL_TOLERANCE 0.01f

/* vert flags */
#define PROJ_VERT_CULL 1

/* to avoid locking in tile initialization */
#define TILE_PENDING POINTER_FROM_INT(-1)

struct ProjPaintState;

/**
 * This is mainly a convenience struct used so we can keep an array of images we use -
 * their #ImBuf's, etc, in 1 array, When using threads this array is copied for each thread
 * because 'partRedrawRect' and 'touch' values would not be thread safe.
 */
struct ProjPaintImage {
  Image *ima;
  ImageUser iuser;
  ImBuf *ibuf;
  ImagePaintPartialRedraw *partRedrawRect;
  /** Only used to build undo tiles during painting. */
  volatile void **undoRect;
  /** The mask accumulation must happen on canvas, not on space screen bucket.
   * Here we store the mask rectangle. */
  ushort **maskRect;
  /** Store flag to enforce validation of undo rectangle. */
  bool **valid;
  bool touch;
  /** Paint color in the colorspace of this image, cached for performance. */
  float paint_color_byte[3];
  bool is_data;
  bool is_srgb;
  const ColorSpace *byte_colorspace;
};

/**
 * Handle for stroke (operator customdata)
 */
struct ProjStrokeHandle {
  /* Support for painting from multiple views at once,
   * currently used to implement symmetry painting,
   * we can assume at least the first is set while painting. */
  ProjPaintState *ps_views[8];

  /* Store initial starting points for perlin noise on the beginning of each stroke when using
   * color jitter. */
  std::optional<blender::float3> initial_hsv_jitter;

  int ps_views_tot;
  int symmetry_flags;

  int orig_brush_size;

  bool need_redraw;

  /* trick to bypass regular paint and allow clone picking */
  bool is_clone_cursor_pick;

  /* In ProjPaintState, only here for convenience */
  Scene *scene;
  Paint *paint;
  Brush *brush;
};

struct LoopSeamData {
  float seam_uvs[2][2];
  float seam_puvs[2][2];
  float corner_dist_sq[2];
};

/* Main projection painting struct passed to all projection painting functions */
struct ProjPaintState {
  View3D *v3d;
  RegionView3D *rv3d;
  ARegion *region;
  Depsgraph *depsgraph;
  Scene *scene;
  /* PROJ_SRC_**** */
  int source;

  /* Scene linear paint color. It can change depending on inverted mode or not. */
  float paint_color_linear[3];
  float dither;

  Paint *paint;
  Brush *brush;

  /**
   * Based on #Brush::image_brush_type but may be overridden by mode (#BrushStrokeMode).
   * So check this value instead of `brush->image_brush_type`.
   */
  short brush_type;
  short blend;
  BrushStrokeMode mode;

  float brush_size;
  Object *ob;
  /* for symmetry, we need to store modified object matrix */
  float obmat[4][4];
  float obmat_imat[4][4];
  /* end similarities with ImagePaintState */

  Image *stencil_ima;
  Image *canvas_ima;
  Image *clone_ima;
  float stencil_value;

  /* projection painting only */
  /** For multi-threading, the first item is sometimes used for non threaded cases too. */
  MemArena *arena_mt[BLENDER_MAX_THREADS];
  /** screen sized 2D array, each pixel has a linked list of ProjPixel's */
  LinkNode **bucketRect;
  /** bucketRect aligned array linkList of faces overlapping each bucket. */
  LinkNode **bucketFaces;
  /** store if the bucks have been initialized. */
  uchar *bucketFlags;

  /** store options per vert, now only store if the vert is pointing away from the view. */
  char *vertFlags;
  /** The size of the bucket grid, the grid span's screenMin/screenMax
   * so you can paint outsize the screen or with 2 brushes at once. */
  int buckets_x;
  int buckets_y;

  /** result of project_paint_pixel_sizeof(), constant per stroke. */
  int pixel_sizeof;

  /** size of projectImages array. */
  int image_tot;

  /** verts projected into floating point screen space. */
  float (*screenCoords)[4];
  /** 2D bounds for mesh verts on the screen's plane (screen-space). */
  float screenMin[2];
  float screenMax[2];
  /** Calculated from screenMin & screenMax. */
  float screen_width;
  float screen_height;
  /** From the area or from the projection render. */
  int winx, winy;

  /* options for projection painting */
  bool do_layer_clone;
  bool do_layer_stencil;
  bool do_layer_stencil_inv;
  bool do_stencil_brush;
  bool do_material_slots;

  /** Use ray-traced occlusion? - otherwise will paint right through to the back. */
  bool do_occlude;
  /** ignore faces with normals pointing away,
   * skips a lot of ray-casts if your normals are correctly flipped. */
  bool do_backfacecull;
  /** mask out pixels based on their normals. */
  bool do_mask_normal;
  /** mask out pixels based on cavity. */
  bool do_mask_cavity;
  /** what angle to mask at. */
  float normal_angle;
  /** cos(normal_angle), faster to compare. */
  float normal_angle__cos;
  float normal_angle_inner;
  float normal_angle_inner__cos;
  /** difference between normal_angle and normal_angle_inner, for easy access. */
  float normal_angle_range;

  /** quick access to (me->editflag & ME_EDIT_PAINT_FACE_SEL) */
  bool do_face_sel;
  bool is_ortho;
  /** the object is negative scaled. */
  bool is_flip_object;
  /** use masking during painting. Some operations such as airbrush may disable. */
  bool do_masking;
  /** only to avoid running. */
  bool is_texbrush;
  /** mask brush is applied before masking. */
  bool is_maskbrush;
#ifndef PROJ_DEBUG_NOSEAMBLEED
  float seam_bleed_px;
  float seam_bleed_px_sq;
#endif
  /* clone vars */
  float cloneOffset[2];

  /** Projection matrix, use for getting screen coords. */
  float projectMat[4][4];
  /** inverse of projectMat. */
  float projectMatInv[4][4];
  /** View vector, use for do_backfacecull and for ray casting with an ortho viewport. */
  float viewDir[3];
  /** View location in object relative 3D space, so can compare to verts. */
  float viewPos[3];
  float clip_start, clip_end;

  /* reproject vars */
  Image *reproject_image;
  ImBuf *reproject_ibuf;
  bool reproject_ibuf_free_float;
  bool reproject_ibuf_free_uchar;

  /* threads */
  int thread_tot;
  int bucketMin[2];
  int bucketMax[2];
  /** must lock threads while accessing these. */
  int context_bucket_index;

  CurveMapping *cavity_curve;
  BlurKernel *blurkernel;

  /* -------------------------------------------------------------------- */
  /* Vars shared between multiple views (keep last) */
  /**
   * This data is owned by `ProjStrokeHandle.ps_views[0]`,
   * all other views re-use the data.
   */

#define PROJ_PAINT_STATE_SHARED_MEMCPY(ps_dst, ps_src) \
  MEMCPY_STRUCT_AFTER(ps_dst, ps_src, is_shared_user)

#define PROJ_PAINT_STATE_SHARED_CLEAR(ps) MEMSET_STRUCT_AFTER(ps, 0, is_shared_user)

  bool is_shared_user;

  ProjPaintImage *projImages;
  /** cavity amount for vertices. */
  float *cavities;

#ifndef PROJ_DEBUG_NOSEAMBLEED
  /** Store info about faces, if they are initialized etc. */
  ushort *faceSeamFlags;
  /** save the winding of the face in uv space,
   * helps as an extra validation step for seam detection. */
  char *faceWindingFlags;
  /** expanded UVs for faces to use as seams. */
  LoopSeamData *loopSeamData;
  /** Only needed for when seam_bleed_px is enabled, use to find UV seams. */
  LinkNode **vertFaces;
  /** Seams per vert, to find adjacent seams. */
  ListBase *vertSeams;
#endif

  SpinLock *tile_lock;

  Mesh *mesh_eval;
  int totloop_eval;
  int faces_num_eval;
  int totvert_eval;

  blender::Span<blender::float3> vert_positions_eval;
  blender::Span<blender::float3> vert_normals;
  blender::Span<blender::int2> edges_eval;
  blender::OffsetIndices<int> faces_eval;
  blender::Span<int> corner_verts_eval;
  const bool *select_poly_eval;
  const bool *hide_poly_eval;
  const int *material_indices;
  const bool *sharp_faces_eval;
  blender::Span<int3> corner_tris_eval;
  blender::Span<int> corner_tri_faces_eval;

  const float (*uv_map_stencil_eval)[2];

  /**
   * \note These UV layers are aligned to \a faces_eval
   * but each pointer references the start of the layer,
   * so a loop indirection is needed as well.
   */
  const float (**poly_to_loop_uv)[2];
  /** other UV map, use for cloning between layers. */
  const float (**poly_to_loop_uv_clone)[2];

  /* Actual material for each index, either from object or Mesh datablock... */
  Material **mat_array;
};

union PixelPointer {
  /** float buffer. */
  float *f_pt;
  /** 2 ways to access a char buffer. */
  uint *uint_pt;
  uchar *ch_pt;
};

union PixelStore {
  uchar ch[4];
  uint uint_;
  float f[4];
};

struct ProjPixel {
  /** the floating point screen projection of this pixel. */
  float projCoSS[2];
  float worldCoSS[3];

  short x_px, y_px;

  /**
   * Use a short to reduce memory use.
   * This limits the total number of supported images to 65535 which seems reasonable.
   */
  ushort image_index;
  uchar bb_cell_index;

  /* for various reasons we may want to mask out painting onto this pixel */
  ushort mask;

  /* Only used when the airbrush is disabled.
   * Store the max mask value to avoid painting over an area with a lower opacity
   * with an advantage that we can avoid touching the pixel at all, if the
   * new mask value is lower than mask_accum */
  ushort *mask_accum;

  /* horrible hack, store tile valid flag pointer here to re-validate tiles
   * used for anchored and drag-dot strokes */
  bool *valid;

  PixelPointer origColor;
  PixelStore newColor;
  PixelPointer pixel;
};

struct ProjPixelClone {
  ProjPixel _pp;
  PixelStore clonepx;
};

/* undo tile pushing */
struct TileInfo {
  SpinLock *lock;
  bool masked;
  ushort tile_width;
  ImBuf **tmpibuf;
  ProjPaintImage *pjima;
};

struct VertSeam {
  VertSeam *next, *prev;
  int tri;
  uint loop;
  float angle;
  bool normal_cw;
  float uv[2];
};

/* -------------------------------------------------------------------- */
/** \name Corner triangle accessor functions
 * \{ */

#define PS_CORNER_TRI_AS_VERT_INDEX_3(ps, tri) \
  ps->corner_verts_eval[tri[0]], ps->corner_verts_eval[tri[1]], ps->corner_verts_eval[tri[2]],

#define PS_CORNER_TRI_AS_UV_3(uvlayer, face_i, tri) \
  uvlayer[face_i][tri[0]], uvlayer[face_i][tri[1]], uvlayer[face_i][tri[2]],

#define PS_CORNER_TRI_ASSIGN_UV_3(uv_tri, uvlayer, face_i, tri) \
  { \
    (uv_tri)[0] = uvlayer[face_i][tri[0]]; \
    (uv_tri)[1] = uvlayer[face_i][tri[1]]; \
    (uv_tri)[2] = uvlayer[face_i][tri[2]]; \
  } \
  ((void)0)

/** \} */

/* Finish projection painting structs */

static int project_paint_face_paint_tile(Image *ima, const float *uv)
{
  if (ima == nullptr || ima->source != IMA_SRC_TILED) {
    return 0;
  }

  /* Currently, faces are assumed to belong to one tile, so checking the first loop is enough. */
  int tx = int(uv[0]);
  int ty = int(uv[1]);
  return 1001 + 10 * ty + tx;
}

static Material *tex_get_material(const ProjPaintState *ps, int face_i)
{
  int mat_nr = ps->material_indices == nullptr ? 0 : ps->material_indices[face_i];
  if (mat_nr >= 0 && mat_nr <= ps->ob->totcol) {
    return ps->mat_array[mat_nr];
  }

  return nullptr;
}

static TexPaintSlot *project_paint_face_paint_slot(const ProjPaintState *ps, int tri_index)
{
  const int face_i = ps->corner_tri_faces_eval[tri_index];
  Material *ma = tex_get_material(ps, face_i);
  return ma ? ma->texpaintslot + ma->paint_active_slot : nullptr;
}

static Image *project_paint_face_paint_image(const ProjPaintState *ps, int tri_index)
{
  if (ps->do_stencil_brush) {
    return ps->stencil_ima;
  }

  const int face_i = ps->corner_tri_faces_eval[tri_index];
  Material *ma = tex_get_material(ps, face_i);
  TexPaintSlot *slot = ma ? ma->texpaintslot + ma->paint_active_slot : nullptr;
  return slot ? slot->ima : ps->canvas_ima;
}

static TexPaintSlot *project_paint_face_clone_slot(const ProjPaintState *ps, int tri_index)
{
  const int face_i = ps->corner_tri_faces_eval[tri_index];
  Material *ma = tex_get_material(ps, face_i);
  return ma ? ma->texpaintslot + ma->paint_clone_slot : nullptr;
}

static Image *project_paint_face_clone_image(const ProjPaintState *ps, int tri_index)
{
  const int face_i = ps->corner_tri_faces_eval[tri_index];
  Material *ma = tex_get_material(ps, face_i);
  TexPaintSlot *slot = ma ? ma->texpaintslot + ma->paint_clone_slot : nullptr;
  return slot ? slot->ima : ps->clone_ima;
}

/**
 * Fast projection bucket array lookup, use the safe version for bound checking.
 */
static int project_bucket_offset(const ProjPaintState *ps, const float projCoSS[2])
{
  /* If we were not dealing with screen-space 2D coords we could simple do...
   * ps->bucketRect[x + (y*ps->buckets_y)] */

  /* please explain?
   * projCoSS[0] - ps->screenMin[0]   : zero origin
   * ... / ps->screen_width           : range from 0.0 to 1.0
   * ... * ps->buckets_x              : use as a bucket index
   *
   * Second multiplication does similar but for vertical offset
   */
  return int(((projCoSS[0] - ps->screenMin[0]) / ps->screen_width) * ps->buckets_x) +
         (int(((projCoSS[1] - ps->screenMin[1]) / ps->screen_height) * ps->buckets_y) *
          ps->buckets_x);
}

static int project_bucket_offset_safe(const ProjPaintState *ps, const float projCoSS[2])
{
  int bucket_index = project_bucket_offset(ps, projCoSS);

  if (bucket_index < 0 || bucket_index >= ps->buckets_x * ps->buckets_y) {
    return -1;
  }
  return bucket_index;
}

static float VecZDepthOrtho(
    const float pt[2], const float v1[3], const float v2[3], const float v3[3], float w[3])
{
  barycentric_weights_v2(v1, v2, v3, pt, w);
  return (v1[2] * w[0]) + (v2[2] * w[1]) + (v3[2] * w[2]);
}

static float VecZDepthPersp(
    const float pt[2], const float v1[4], const float v2[4], const float v3[4], float w[3])
{
  float wtot_inv, wtot;
  float w_tmp[3];

  barycentric_weights_v2_persp(v1, v2, v3, pt, w);
  /* for the depth we need the weights to match what
   * barycentric_weights_v2 would return, in this case its easiest just to
   * undo the 4th axis division and make it unit-sum
   *
   * don't call barycentric_weights_v2() because our callers expect 'w'
   * to be weighted from the perspective */
  w_tmp[0] = w[0] * v1[3];
  w_tmp[1] = w[1] * v2[3];
  w_tmp[2] = w[2] * v3[3];

  wtot = w_tmp[0] + w_tmp[1] + w_tmp[2];

  if (wtot != 0.0f) {
    wtot_inv = 1.0f / wtot;

    w_tmp[0] = w_tmp[0] * wtot_inv;
    w_tmp[1] = w_tmp[1] * wtot_inv;
    w_tmp[2] = w_tmp[2] * wtot_inv;
  }
  else { /* dummy values for zero area face */
    w_tmp[0] = w_tmp[1] = w_tmp[2] = 1.0f / 3.0f;
  }
  /* done mimicking barycentric_weights_v2() */

  return (v1[2] * w_tmp[0]) + (v2[2] * w_tmp[1]) + (v3[2] * w_tmp[2]);
}

/* Return the top-most face index that the screen space coord 'pt' touches (or -1) */
static int project_paint_PickFace(const ProjPaintState *ps, const float pt[2], float w[3])
{
  LinkNode *node;
  float w_tmp[3];
  int bucket_index;
  int best_tri_index = -1;
  float z_depth_best = FLT_MAX, z_depth;

  bucket_index = project_bucket_offset_safe(ps, pt);
  if (bucket_index == -1) {
    return -1;
  }

  /* we could return 0 for 1 face buckets, as long as this function assumes
   * that the point its testing is only every originated from an existing face */

  for (node = ps->bucketFaces[bucket_index]; node; node = node->next) {
    const int tri_index = POINTER_AS_INT(node->link);
    const int3 &tri = ps->corner_tris_eval[tri_index];
    const float *vtri_ss[3] = {
        ps->screenCoords[ps->corner_verts_eval[tri[0]]],
        ps->screenCoords[ps->corner_verts_eval[tri[1]]],
        ps->screenCoords[ps->corner_verts_eval[tri[2]]],
    };

    if (isect_point_tri_v2(pt, UNPACK3(vtri_ss))) {
      if (ps->is_ortho) {
        z_depth = VecZDepthOrtho(pt, UNPACK3(vtri_ss), w_tmp);
      }
      else {
        z_depth = VecZDepthPersp(pt, UNPACK3(vtri_ss), w_tmp);
      }

      if (z_depth < z_depth_best) {
        best_tri_index = tri_index;
        z_depth_best = z_depth;
        copy_v3_v3(w, w_tmp);
      }
    }
  }

  /** will be -1 or a valid face. */
  return best_tri_index;
}

/* Set the top-most face color that the screen space coord 'pt' touches
 * (or return 0 if none touch) */
static bool project_paint_PickColor(
    const ProjPaintState *ps, const float pt[2], float *rgba_fp, uchar *rgba, const bool interp)
{
  using namespace blender;
  const float *tri_uv[3];
  float w[3], uv[2];
  int tri_index;
  Image *ima;
  ImBuf *ibuf;

  tri_index = project_paint_PickFace(ps, pt, w);

  if (tri_index == -1) {
    return false;
  }

  const int3 &tri = ps->corner_tris_eval[tri_index];
  PS_CORNER_TRI_ASSIGN_UV_3(
      tri_uv, ps->poly_to_loop_uv, ps->corner_tri_faces_eval[tri_index], tri);

  interp_v2_v2v2v2(uv, UNPACK3(tri_uv), w);

  ima = project_paint_face_paint_image(ps, tri_index);
  /** we must have got the imbuf before getting here. */
  int tile_number = project_paint_face_paint_tile(ima, tri_uv[0]);
  /* XXX get appropriate ImageUser instead */
  ImageUser iuser;
  BKE_imageuser_default(&iuser);
  iuser.tile = tile_number;
  iuser.framenr = ima->lastframe;
  ibuf = BKE_image_acquire_ibuf(ima, &iuser, nullptr);
  if (ibuf == nullptr) {
    return false;
  }

  float x = uv[0] * ibuf->x;
  float y = uv[1] * ibuf->y;
  if (interp) {
    x -= 0.5f;
    y -= 0.5f;
  }

  if (ibuf->float_buffer.data) {
    float4 col = interp ? imbuf::interpolate_bilinear_wrap_fl(ibuf, x, y) :
                          imbuf::interpolate_nearest_wrap_fl(ibuf, x, y);
    col = math::clamp(col, 0.0f, 1.0f);
    if (rgba_fp) {
      memcpy(rgba_fp, &col, sizeof(col));
    }
    else {
      premul_float_to_straight_uchar(rgba, col);
    }
  }
  else {
    uchar4 col = interp ? imbuf::interpolate_bilinear_wrap_byte(ibuf, x, y) :
                          imbuf::interpolate_nearest_wrap_byte(ibuf, x, y);
    if (rgba) {
      memcpy(rgba, &col, sizeof(col));
    }
    else {
      straight_uchar_to_premul_float(rgba_fp, col);
    }
  }
  BKE_image_release_ibuf(ima, ibuf, nullptr);
  return true;
}

/**
 * Check if 'pt' is in front of the 3 verts on the Z axis (used for screen-space occlusion test)
 * \return
 * -  `0`: no occlusion.
 * - `-1`: no occlusion but 2D intersection is true.
 * -  `1`: occluded.
 * -  `2`: occluded with `w[3]` weights set (need to know in some cases).
 */
static int project_paint_occlude_ptv(const float pt[3],
                                     const float v1[4],
                                     const float v2[4],
                                     const float v3[4],
                                     float w[3],
                                     const bool is_ortho)
{
  /* if all are behind us, return false */
  if (v1[2] > pt[2] && v2[2] > pt[2] && v3[2] > pt[2]) {
    return 0;
  }

  /* do a 2D point in try intersection */
  if (!isect_point_tri_v2(pt, v1, v2, v3)) {
    return 0;
  }

  /* From here on we know there IS an intersection */
  /* if ALL of the verts are in front of us then we know it intersects ? */
  if (v1[2] < pt[2] && v2[2] < pt[2] && v3[2] < pt[2]) {
    return 1;
  }

  /* we intersect? - find the exact depth at the point of intersection */
  /* Is this point is occluded by another face? */
  if (is_ortho) {
    if (VecZDepthOrtho(pt, v1, v2, v3, w) < pt[2]) {
      return 2;
    }
  }
  else {
    if (VecZDepthPersp(pt, v1, v2, v3, w) < pt[2]) {
      return 2;
    }
  }
  return -1;
}

static int project_paint_occlude_ptv_clip(const float pt[3],
                                          const float v1[4],
                                          const float v2[4],
                                          const float v3[4],
                                          const float v1_3d[3],
                                          const float v2_3d[3],
                                          const float v3_3d[3],
                                          float w[3],
                                          const bool is_ortho,
                                          RegionView3D *rv3d)
{
  float wco[3];
  int ret = project_paint_occlude_ptv(pt, v1, v2, v3, w, is_ortho);

  if (ret <= 0) {
    return ret;
  }

  if (ret == 1) { /* weights not calculated */
    if (is_ortho) {
      barycentric_weights_v2(v1, v2, v3, pt, w);
    }
    else {
      barycentric_weights_v2_persp(v1, v2, v3, pt, w);
    }
  }

  /* Test if we're in the clipped area, */
  interp_v3_v3v3v3(wco, v1_3d, v2_3d, v3_3d, w);

  if (!ED_view3d_clipping_test(rv3d, wco, true)) {
    return 1;
  }

  return -1;
}

/* Check if a screen-space location is occluded by any other faces
 * check, pixelScreenCo must be in screen-space, its Z-Depth only needs to be used for comparison
 * and doesn't need to be correct in relation to X and Y coords
 * (this is the case in perspective view) */
static bool project_bucket_point_occluded(const ProjPaintState *ps,
                                          LinkNode *bucketFace,
                                          const int orig_face,
                                          const float pixelScreenCo[4])
{
  int isect_ret;
  const bool do_clip = RV3D_CLIPPING_ENABLED(ps->v3d, ps->rv3d);

  /* we could return false for 1 face buckets, as long as this function assumes
   * that the point its testing is only every originated from an existing face */

  for (; bucketFace; bucketFace = bucketFace->next) {
    const int tri_index = POINTER_AS_INT(bucketFace->link);

    if (orig_face != tri_index) {
      const int3 &tri = ps->corner_tris_eval[tri_index];
      const float *vtri_ss[3] = {
          ps->screenCoords[ps->corner_verts_eval[tri[0]]],
          ps->screenCoords[ps->corner_verts_eval[tri[1]]],
          ps->screenCoords[ps->corner_verts_eval[tri[2]]],
      };
      float w[3];

      if (do_clip) {
        const float *vtri_co[3] = {
            ps->vert_positions_eval[ps->corner_verts_eval[tri[0]]],
            ps->vert_positions_eval[ps->corner_verts_eval[tri[1]]],
            ps->vert_positions_eval[ps->corner_verts_eval[tri[2]]],
        };
        isect_ret = project_paint_occlude_ptv_clip(
            pixelScreenCo, UNPACK3(vtri_ss), UNPACK3(vtri_co), w, ps->is_ortho, ps->rv3d);
      }
      else {
        isect_ret = project_paint_occlude_ptv(pixelScreenCo, UNPACK3(vtri_ss), w, ps->is_ortho);
      }

      if (isect_ret >= 1) {
        /* TODO: we may want to cache the first hit,
         * it is not possible to swap the face order in the list anymore */
        return true;
      }
    }
  }
  return false;
}

/* Basic line intersection, could move to math_geom.c, 2 points with a horizontal line
 * 1 for an intersection, 2 if the first point is aligned, 3 if the second point is aligned. */
#define ISECT_TRUE 1
#define ISECT_TRUE_P1 2
#define ISECT_TRUE_P2 3
static int line_isect_y(const float p1[2], const float p2[2], const float y_level, float *x_isect)
{
  float y_diff;

  /* are we touching the first point? - no interpolation needed */
  if (y_level == p1[1]) {
    *x_isect = p1[0];
    return ISECT_TRUE_P1;
  }
  /* are we touching the second point? - no interpolation needed */
  if (y_level == p2[1]) {
    *x_isect = p2[0];
    return ISECT_TRUE_P2;
  }

  /** yuck, horizontal line, we can't do much here. */
  y_diff = fabsf(p1[1] - p2[1]);

  if (y_diff < 0.000001f) {
    *x_isect = (p1[0] + p2[0]) * 0.5f;
    return ISECT_TRUE;
  }

  if (p1[1] > y_level && p2[1] < y_level) {
    /* `p1[1] - p2[1]`. */
    *x_isect = (p2[0] * (p1[1] - y_level) + p1[0] * (y_level - p2[1])) / y_diff;
    return ISECT_TRUE;
  }
  if (p1[1] < y_level && p2[1] > y_level) {
    /* `p2[1] - p1[1]`. */
    *x_isect = (p2[0] * (y_level - p1[1]) + p1[0] * (p2[1] - y_level)) / y_diff;
    return ISECT_TRUE;
  }
  return 0;
}

static int line_isect_x(const float p1[2], const float p2[2], const float x_level, float *y_isect)
{
  float x_diff;

  if (x_level == p1[0]) { /* are we touching the first point? - no interpolation needed */
    *y_isect = p1[1];
    return ISECT_TRUE_P1;
  }
  if (x_level == p2[0]) { /* are we touching the second point? - no interpolation needed */
    *y_isect = p2[1];
    return ISECT_TRUE_P2;
  }

  /* yuck, horizontal line, we can't do much here */
  x_diff = fabsf(p1[0] - p2[0]);

  /* yuck, vertical line, we can't do much here */
  if (x_diff < 0.000001f) {
    *y_isect = (p1[0] + p2[0]) * 0.5f;
    return ISECT_TRUE;
  }

  if (p1[0] > x_level && p2[0] < x_level) {
    /* `p1[0] - p2[0]`. */
    *y_isect = (p2[1] * (p1[0] - x_level) + p1[1] * (x_level - p2[0])) / x_diff;
    return ISECT_TRUE;
  }
  if (p1[0] < x_level && p2[0] > x_level) {
    /* `p2[0] - p1[0]`. */
    *y_isect = (p2[1] * (x_level - p1[0]) + p1[1] * (p2[0] - x_level)) / x_diff;
    return ISECT_TRUE;
  }
  return 0;
}

/* simple func use for comparing UV locations to check if there are seams.
 * Its possible this gives incorrect results, when the UVs for 1 face go into the next
 * tile, but do not do this for the adjacent face, it could return a false positive.
 * This is so unlikely that Id not worry about it. */
#ifndef PROJ_DEBUG_NOSEAMBLEED
static bool cmp_uv(const float vec2a[2], const float vec2b[2])
{
  /* if the UVs are not between 0.0 and 1.0 */
  float xa = fmodf(vec2a[0], 1.0f);
  float ya = fmodf(vec2a[1], 1.0f);

  float xb = fmodf(vec2b[0], 1.0f);
  float yb = fmodf(vec2b[1], 1.0f);

  if (xa < 0.0f) {
    xa += 1.0f;
  }
  if (ya < 0.0f) {
    ya += 1.0f;
  }

  if (xb < 0.0f) {
    xb += 1.0f;
  }
  if (yb < 0.0f) {
    yb += 1.0f;
  }

  return ((fabsf(xa - xb) < PROJ_GEOM_TOLERANCE) && (fabsf(ya - yb) < PROJ_GEOM_TOLERANCE)) ?
             true :
             false;
}
#endif

/* set min_px and max_px to the image space bounds of the UV coords
 * return zero if there is no area in the returned rectangle */
#ifndef PROJ_DEBUG_NOSEAMBLEED
static bool pixel_bounds_uv(const float uv_quad[4][2],
                            const int ibuf_x,
                            const int ibuf_y,
                            rcti *r_bounds_px)
{
  /* UV bounds */
  float min_uv[2], max_uv[2];

  INIT_MINMAX2(min_uv, max_uv);

  minmax_v2v2_v2(min_uv, max_uv, uv_quad[0]);
  minmax_v2v2_v2(min_uv, max_uv, uv_quad[1]);
  minmax_v2v2_v2(min_uv, max_uv, uv_quad[2]);
  minmax_v2v2_v2(min_uv, max_uv, uv_quad[3]);

  r_bounds_px->xmin = int(ibuf_x * min_uv[0]);
  r_bounds_px->ymin = int(ibuf_y * min_uv[1]);

  r_bounds_px->xmax = int(ibuf_x * max_uv[0]) + 1;
  r_bounds_px->ymax = int(ibuf_y * max_uv[1]) + 1;

  // printf("%d %d %d %d\n", min_px[0], min_px[1], max_px[0], max_px[1]);

  /* face uses no UV area when quantized to pixels? */
  return (r_bounds_px->xmin == r_bounds_px->xmax || r_bounds_px->ymin == r_bounds_px->ymax) ?
             false :
             true;
}
#endif

static bool pixel_bounds_array(
    float (*uv)[2], const int ibuf_x, const int ibuf_y, int tot, rcti *r_bounds_px)
{
  /* UV bounds */
  float min_uv[2], max_uv[2];

  if (tot == 0) {
    return false;
  }

  INIT_MINMAX2(min_uv, max_uv);

  while (tot--) {
    minmax_v2v2_v2(min_uv, max_uv, (*uv));
    uv++;
  }

  r_bounds_px->xmin = int(ibuf_x * min_uv[0]);
  r_bounds_px->ymin = int(ibuf_y * min_uv[1]);

  r_bounds_px->xmax = int(ibuf_x * max_uv[0]) + 1;
  r_bounds_px->ymax = int(ibuf_y * max_uv[1]) + 1;

  // printf("%d %d %d %d\n", min_px[0], min_px[1], max_px[0], max_px[1]);

  /* face uses no UV area when quantized to pixels? */
  return (r_bounds_px->xmin == r_bounds_px->xmax || r_bounds_px->ymin == r_bounds_px->ymax) ?
             false :
             true;
}

#ifndef PROJ_DEBUG_NOSEAMBLEED

static void project_face_winding_init(const ProjPaintState *ps, const int tri_index)
{
  /* detect the winding of faces in uv space */
  const int3 &tri = ps->corner_tris_eval[tri_index];
  const int face_i = ps->corner_tri_faces_eval[tri_index];
  const float *tri_uv[3] = {PS_CORNER_TRI_AS_UV_3(ps->poly_to_loop_uv, face_i, tri)};
  float winding = cross_tri_v2(tri_uv[0], tri_uv[1], tri_uv[2]);

  if (winding > 0) {
    ps->faceWindingFlags[tri_index] |= PROJ_FACE_WINDING_CW;
  }

  ps->faceWindingFlags[tri_index] |= PROJ_FACE_WINDING_INIT;
}

/* This function returns 1 if this face has a seam along the 2 face-vert indices
 * 'orig_i1_fidx' and 'orig_i2_fidx' */
static bool check_seam(const ProjPaintState *ps,
                       const int orig_face,
                       const int orig_i1_fidx,
                       const int orig_i2_fidx,
                       int *other_face,
                       int *orig_fidx)
{
  const int3 &orig_tri = ps->corner_tris_eval[orig_face];
  const int orig_poly_i = ps->corner_tri_faces_eval[orig_face];
  const float *orig_tri_uv[3] = {
      PS_CORNER_TRI_AS_UV_3(ps->poly_to_loop_uv, orig_poly_i, orig_tri)};
  /* vert indices from face vert order indices */
  const uint i1 = ps->corner_verts_eval[orig_tri[orig_i1_fidx]];
  const uint i2 = ps->corner_verts_eval[orig_tri[orig_i2_fidx]];
  LinkNode *node;
  /* index in face */
  int i1_fidx = -1, i2_fidx = -1;

  for (node = ps->vertFaces[i1]; node; node = node->next) {
    const int tri_index = POINTER_AS_INT(node->link);

    if (tri_index != orig_face) {
      const int3 &tri = ps->corner_tris_eval[tri_index];
      const int face_i = ps->corner_tri_faces_eval[tri_index];
      const int vert_tri[3] = {PS_CORNER_TRI_AS_VERT_INDEX_3(ps, tri)};
      /* could check if the 2 faces images match here,
       * but then there wouldn't be a way to return the opposite face's info */

      /* We need to know the order of the verts in the adjacent face
       * set the i1_fidx and i2_fidx to (0,1,2,3) */
      i1_fidx = BKE_MESH_TESSTRI_VINDEX_ORDER(vert_tri, i1);
      i2_fidx = BKE_MESH_TESSTRI_VINDEX_ORDER(vert_tri, i2);

      /* Only need to check if 'i2_fidx' is valid because
       * we know i1_fidx is the same vert on both faces. */
      if (i2_fidx != -1) {
        const float *tri_uv[3] = {PS_CORNER_TRI_AS_UV_3(ps->poly_to_loop_uv, face_i, tri)};
        Image *tpage = project_paint_face_paint_image(ps, tri_index);
        Image *orig_tpage = project_paint_face_paint_image(ps, orig_face);
        int tile = project_paint_face_paint_tile(tpage, tri_uv[0]);
        int orig_tile = project_paint_face_paint_tile(orig_tpage, orig_tri_uv[0]);

        BLI_assert(i1_fidx != -1);

        /* This IS an adjacent face!, now lets check if the UVs are ok */

        /* set up the other face */
        *other_face = tri_index;

        /* we check if difference is 1 here, else we might have a case of edge 2-0 for a tri */
        *orig_fidx = (i1_fidx < i2_fidx && (i2_fidx - i1_fidx == 1)) ? i1_fidx : i2_fidx;

        /* initialize face winding if needed */
        if ((ps->faceWindingFlags[tri_index] & PROJ_FACE_WINDING_INIT) == 0) {
          project_face_winding_init(ps, tri_index);
        }

        /* first test if they have the same image */
        if ((orig_tpage == tpage) && (orig_tile == tile) &&
            cmp_uv(orig_tri_uv[orig_i1_fidx], tri_uv[i1_fidx]) &&
            cmp_uv(orig_tri_uv[orig_i2_fidx], tri_uv[i2_fidx]))
        {
          /* if faces don't have the same winding in uv space,
           * they are on the same side so edge is boundary */
          if ((ps->faceWindingFlags[tri_index] & PROJ_FACE_WINDING_CW) !=
              (ps->faceWindingFlags[orig_face] & PROJ_FACE_WINDING_CW))
          {
            return true;
          }

          // printf("SEAM (NONE)\n");
          return false;
        }
        // printf("SEAM (UV GAP)\n");
        return true;
      }
    }
  }
  // printf("SEAM (NO FACE)\n");
  *other_face = -1;
  return true;
}

static VertSeam *find_adjacent_seam(const ProjPaintState *ps,
                                    uint loop_index,
                                    uint vert_index,
                                    VertSeam **r_seam)
{
  ListBase *vert_seams = &ps->vertSeams[vert_index];
  VertSeam *seam = static_cast<VertSeam *>(vert_seams->first);
  VertSeam *adjacent = nullptr;

  while (seam->loop != loop_index) {
    seam = seam->next;
  }

  if (r_seam) {
    *r_seam = seam;
  }

  /* Circulate through the (sorted) vert seam array, in the direction of the seam normal,
   * until we find the first opposing seam, matching in UV space. */
  if (seam->normal_cw) {
    LISTBASE_CIRCULAR_BACKWARD_BEGIN (VertSeam *, vert_seams, adjacent, seam) {
      if ((adjacent->normal_cw != seam->normal_cw) && cmp_uv(adjacent->uv, seam->uv)) {
        break;
      }
    }
    LISTBASE_CIRCULAR_BACKWARD_END(VertSeam *, vert_seams, adjacent, seam);
  }
  else {
    LISTBASE_CIRCULAR_FORWARD_BEGIN (VertSeam *, vert_seams, adjacent, seam) {
      if ((adjacent->normal_cw != seam->normal_cw) && cmp_uv(adjacent->uv, seam->uv)) {
        break;
      }
    }
    LISTBASE_CIRCULAR_FORWARD_END(VertSeam *, vert_seams, adjacent, seam);
  }

  BLI_assert(adjacent);

  return adjacent;
}

/* Computes the normal of two seams at their intersection,
 * and returns the angle between the seam and its normal. */
static float compute_seam_normal(VertSeam *seam, VertSeam *adj, float r_no[2])
{
  const float PI_2 = M_PI * 2.0f;
  float angle[2];
  float angle_rel, angle_no;

  if (seam->normal_cw) {
    angle[0] = adj->angle;
    angle[1] = seam->angle;
  }
  else {
    angle[0] = seam->angle;
    angle[1] = adj->angle;
  }

  angle_rel = angle[1] - angle[0];

  if (angle_rel < 0.0f) {
    angle_rel += PI_2;
  }

  angle_rel *= 0.5f;

  angle_no = angle_rel + angle[0];

  if (angle_no > M_PI) {
    angle_no -= PI_2;
  }

  r_no[0] = cosf(angle_no);
  r_no[1] = sinf(angle_no);

  return angle_rel;
}

/* Calculate outset UVs, this is not the same as simply scaling the UVs,
 * since the outset coords are a margin that keep an even distance from the original UVs,
 * note that the image aspect is taken into account */
static void uv_image_outset(const ProjPaintState *ps,
                            float (*orig_uv)[2],
                            float (*puv)[2],
                            uint tri_index,
                            const int ibuf_x,
                            const int ibuf_y)
{
  int fidx[2];
  uint loop_index;
  uint vert[2];
  const int3 &tri = ps->corner_tris_eval[tri_index];

  float ibuf_inv[2];

  ibuf_inv[0] = 1.0f / float(ibuf_x);
  ibuf_inv[1] = 1.0f / float(ibuf_y);

  for (fidx[0] = 0; fidx[0] < 3; fidx[0]++) {
    LoopSeamData *seam_data;
    float (*seam_uvs)[2];
    float ang[2];

    if ((ps->faceSeamFlags[tri_index] & (PROJ_FACE_SEAM0 << fidx[0])) == 0) {
      continue;
    }

    loop_index = tri[fidx[0]];

    seam_data = &ps->loopSeamData[loop_index];
    seam_uvs = seam_data->seam_uvs;

    if (seam_uvs[0][0] != FLT_MAX) {
      continue;
    }

    fidx[1] = (fidx[0] == 2) ? 0 : fidx[0] + 1;

    vert[0] = ps->corner_verts_eval[loop_index];
    vert[1] = ps->corner_verts_eval[tri[fidx[1]]];

    for (uint i = 0; i < 2; i++) {
      VertSeam *seam;
      VertSeam *adj = find_adjacent_seam(ps, loop_index, vert[i], &seam);
      float no[2];
      float len_fact;
      float tri_ang;

      ang[i] = compute_seam_normal(seam, adj, no);
      tri_ang = ang[i] - M_PI_2;

      if (tri_ang > 0.0f) {
        const float dist = ps->seam_bleed_px * tanf(tri_ang);
        seam_data->corner_dist_sq[i] = square_f(dist);
      }
      else {
        seam_data->corner_dist_sq[i] = 0.0f;
      }

      len_fact = cosf(tri_ang);
      len_fact = UNLIKELY(len_fact < FLT_EPSILON) ? FLT_MAX : (1.0f / len_fact);

      /* Clamp the length factor, see: #62236. */
      len_fact = std::min(len_fact, 10.0f);

      mul_v2_fl(no, ps->seam_bleed_px * len_fact);

      add_v2_v2v2(seam_data->seam_puvs[i], puv[fidx[i]], no);

      mul_v2_v2v2(seam_uvs[i], seam_data->seam_puvs[i], ibuf_inv);
    }

    /* Handle convergent normals (can self-intersect). */
    if ((ang[0] + ang[1]) < M_PI) {
      if (isect_seg_seg_v2_simple(orig_uv[fidx[0]], seam_uvs[0], orig_uv[fidx[1]], seam_uvs[1])) {
        float isect_co[2];

        isect_seg_seg_v2_point(
            orig_uv[fidx[0]], seam_uvs[0], orig_uv[fidx[1]], seam_uvs[1], isect_co);

        copy_v2_v2(seam_uvs[0], isect_co);
        copy_v2_v2(seam_uvs[1], isect_co);
      }
    }
  }
}

static void insert_seam_vert_array(const ProjPaintState *ps,
                                   MemArena *arena,
                                   const int tri_index,
                                   const int fidx1,
                                   const int ibuf_x,
                                   const int ibuf_y)
{
  const int3 &tri = ps->corner_tris_eval[tri_index];
  const int face_i = ps->corner_tri_faces_eval[tri_index];
  const float *tri_uv[3] = {PS_CORNER_TRI_AS_UV_3(ps->poly_to_loop_uv, face_i, tri)};
  const int fidx[2] = {fidx1, ((fidx1 + 1) % 3)};
  float vec[2];

  VertSeam *vseam = static_cast<VertSeam *>(BLI_memarena_alloc(arena, sizeof(VertSeam[2])));

  vseam->prev = nullptr;
  vseam->next = nullptr;

  vseam->tri = tri_index;
  vseam->loop = tri[fidx[0]];

  sub_v2_v2v2(vec, tri_uv[fidx[1]], tri_uv[fidx[0]]);
  vec[0] *= ibuf_x;
  vec[1] *= ibuf_y;
  vseam->angle = atan2f(vec[1], vec[0]);

  /* If the face winding data is not initialized, something must be wrong. */
  BLI_assert((ps->faceWindingFlags[tri_index] & PROJ_FACE_WINDING_INIT) != 0);
  vseam->normal_cw = (ps->faceWindingFlags[tri_index] & PROJ_FACE_WINDING_CW);

  copy_v2_v2(vseam->uv, tri_uv[fidx[0]]);

  vseam[1] = vseam[0];
  vseam[1].angle += vseam[1].angle > 0.0f ? -M_PI : M_PI;
  vseam[1].normal_cw = !vseam[1].normal_cw;
  copy_v2_v2(vseam[1].uv, tri_uv[fidx[1]]);

  for (uint i = 0; i < 2; i++) {
    const int vert = ps->corner_verts_eval[tri[fidx[i]]];
    ListBase *list = &ps->vertSeams[vert];
    VertSeam *item = static_cast<VertSeam *>(list->first);

    while (item && item->angle < vseam[i].angle) {
      item = item->next;
    }

    BLI_insertlinkbefore(list, item, &vseam[i]);
  }
}

/**
 * Be tricky with flags, first 4 bits are #PROJ_FACE_SEAM0 to 4,
 * last 4 bits are #PROJ_FACE_NOSEAM0 to 4. `1 << i` - where i is `(0..3)`.
 *
 * If we're multi-threading, make sure threads are locked when this is called.
 */
static void project_face_seams_init(const ProjPaintState *ps,
                                    MemArena *arena,
                                    const int tri_index,
                                    const uint vert_index,
                                    bool init_all,
                                    const int ibuf_x,
                                    const int ibuf_y)
{
  /* vars for the other face, we also set its flag */
  int other_face, other_fidx;
  /* next fidx in the face (0,1,2,3) -> (1,2,3,0) or (0,1,2) -> (1,2,0) for a tri */
  int fidx[2] = {2, 0};
  const int3 &tri = ps->corner_tris_eval[tri_index];
  LinkNode *node;

  /* initialize face winding if needed */
  if ((ps->faceWindingFlags[tri_index] & PROJ_FACE_WINDING_INIT) == 0) {
    project_face_winding_init(ps, tri_index);
  }

  do {
    if (init_all || (ps->corner_verts_eval[tri[fidx[0]]] == vert_index) ||
        (ps->corner_verts_eval[tri[fidx[1]]] == vert_index))
    {
      if ((ps->faceSeamFlags[tri_index] &
           (PROJ_FACE_SEAM0 << fidx[0] | PROJ_FACE_NOSEAM0 << fidx[0])) == 0)
      {
        if (check_seam(ps, tri_index, fidx[0], fidx[1], &other_face, &other_fidx)) {
          ps->faceSeamFlags[tri_index] |= PROJ_FACE_SEAM0 << fidx[0];
          insert_seam_vert_array(ps, arena, tri_index, fidx[0], ibuf_x, ibuf_y);

          if (other_face != -1) {
            /* Check if the other seam is already set.
             * We don't want to insert it in the list twice. */
            if ((ps->faceSeamFlags[other_face] & (PROJ_FACE_SEAM0 << other_fidx)) == 0) {
              ps->faceSeamFlags[other_face] |= PROJ_FACE_SEAM0 << other_fidx;
              insert_seam_vert_array(ps, arena, other_face, other_fidx, ibuf_x, ibuf_y);
            }
          }
        }
        else {
          ps->faceSeamFlags[tri_index] |= PROJ_FACE_NOSEAM0 << fidx[0];
          ps->faceSeamFlags[tri_index] |= PROJ_FACE_SEAM_INIT0 << fidx[0];

          if (other_face != -1) {
            /* second 4 bits for disabled */
            ps->faceSeamFlags[other_face] |= PROJ_FACE_NOSEAM0 << other_fidx;
            ps->faceSeamFlags[other_face] |= PROJ_FACE_SEAM_INIT0 << other_fidx;
          }
        }
      }
    }

    fidx[1] = fidx[0];
  } while (fidx[0]--);

  if (init_all) {
    char checked_verts = 0;

    fidx[0] = 2;
    fidx[1] = 0;

    do {
      if ((ps->faceSeamFlags[tri_index] & (PROJ_FACE_SEAM_INIT0 << fidx[0])) == 0) {
        for (uint i = 0; i < 2; i++) {
          uint vert;

          if ((checked_verts & (1 << fidx[i])) != 0) {
            continue;
          }

          vert = ps->corner_verts_eval[tri[fidx[i]]];

          for (node = ps->vertFaces[vert]; node; node = node->next) {
            const int tri = POINTER_AS_INT(node->link);

            project_face_seams_init(ps, arena, tri, vert, false, ibuf_x, ibuf_y);
          }

          checked_verts |= 1 << fidx[i];
        }

        ps->faceSeamFlags[tri_index] |= PROJ_FACE_SEAM_INIT0 << fidx[0];
      }

      fidx[1] = fidx[0];
    } while (fidx[0]--);
  }
}
#endif  // PROJ_DEBUG_NOSEAMBLEED

/* Converts a UV location to a 3D screen-space location
 * Takes a 'uv' and 3 UV coords, and sets the values of pixelScreenCo
 *
 * This is used for finding a pixels location in screen-space for painting */
static void screen_px_from_ortho(const float uv[2],
                                 const float v1co[3],
                                 const float v2co[3],
                                 const float v3co[3], /* Screen-space coords */
                                 const float uv1co[2],
                                 const float uv2co[2],
                                 const float uv3co[2],
                                 float pixelScreenCo[4],
                                 float w[3])
{
  barycentric_weights_v2(uv1co, uv2co, uv3co, uv, w);
  interp_v3_v3v3v3(pixelScreenCo, v1co, v2co, v3co, w);
}

/* same as screen_px_from_ortho except we
 * do perspective correction on the pixel coordinate */
static void screen_px_from_persp(const float uv[2],
                                 const float v1co[4],
                                 const float v2co[4],
                                 const float v3co[4], /* screen-space coords */
                                 const float uv1co[2],
                                 const float uv2co[2],
                                 const float uv3co[2],
                                 float pixelScreenCo[4],
                                 float w[3])
{
  float w_int[3];
  float wtot_inv, wtot;
  barycentric_weights_v2(uv1co, uv2co, uv3co, uv, w);

  /* re-weight from the 4th coord of each screen vert */
  w_int[0] = w[0] * v1co[3];
  w_int[1] = w[1] * v2co[3];
  w_int[2] = w[2] * v3co[3];

  wtot = w_int[0] + w_int[1] + w_int[2];

  if (wtot > 0.0f) {
    wtot_inv = 1.0f / wtot;
    w_int[0] *= wtot_inv;
    w_int[1] *= wtot_inv;
    w_int[2] *= wtot_inv;
  }
  else {
    /* Dummy values for zero area face. */
    w[0] = w[1] = w[2] = w_int[0] = w_int[1] = w_int[2] = 1.0f / 3.0f;
  }
  /* done re-weighting */

  /* do interpolation based on projected weight */
  interp_v3_v3v3v3(pixelScreenCo, v1co, v2co, v3co, w_int);
}

/**
 * Set a direction vector based on a screen location.
 * (use for perspective view, else we can simply use `ps->viewDir`)
 *
 * Similar functionality to #ED_view3d_win_to_vector
 *
 * \param r_dir: Resulting direction (length is undefined).
 */
static void screen_px_to_vector_persp(int winx,
                                      int winy,
                                      const float projmat_inv[4][4],
                                      const float view_pos[3],
                                      const float co_px[2],
                                      float r_dir[3])
{
  r_dir[0] = 2.0f * (co_px[0] / winx) - 1.0f;
  r_dir[1] = 2.0f * (co_px[1] / winy) - 1.0f;
  r_dir[2] = -0.5f;
  mul_project_m4_v3((float (*)[4])projmat_inv, r_dir);
  sub_v3_v3(r_dir, view_pos);
}

/**
 * Special function to return the factor to a point along a line in pixel space.
 *
 * This is needed since we can't use #line_point_factor_v2 for perspective screen-space coords.
 *
 * \param p: 2D screen-space location.
 * \param v1, v2: 3D object-space locations.
 */
static float screen_px_line_point_factor_v2_persp(const ProjPaintState *ps,
                                                  const float p[2],
                                                  const float v1[3],
                                                  const float v2[3])
{
  const float zero[3] = {0};
  float v1_proj[3], v2_proj[3];
  float dir[3];

  screen_px_to_vector_persp(ps->winx, ps->winy, ps->projectMatInv, ps->viewPos, p, dir);

  sub_v3_v3v3(v1_proj, v1, ps->viewPos);
  sub_v3_v3v3(v2_proj, v2, ps->viewPos);

  project_plane_v3_v3v3(v1_proj, v1_proj, dir);
  project_plane_v3_v3v3(v2_proj, v2_proj, dir);

  return line_point_factor_v2(zero, v1_proj, v2_proj);
}

static void project_face_pixel(
    const float *tri_uv[3], ImBuf *ibuf_other, const float w[3], uchar rgba_ub[4], float rgba_f[4])
{
  using namespace blender;
  float uv_other[2];

  interp_v2_v2v2v2(uv_other, UNPACK3(tri_uv), w);

  float x = uv_other[0] * ibuf_other->x - 0.5f;
  float y = uv_other[1] * ibuf_other->y - 0.5f;

  if (ibuf_other->float_buffer.data) {
    float4 col = imbuf::interpolate_bilinear_wrap_fl(ibuf_other, x, y);
    col = math::clamp(col, 0.0f, 1.0f);
    memcpy(rgba_f, &col, sizeof(col));
  }
  else {
    uchar4 col = imbuf::interpolate_bilinear_wrap_byte(ibuf_other, x, y);
    memcpy(rgba_ub, &col, sizeof(col));
  }
}

/* run this outside project_paint_uvpixel_init since pixels with mask 0 don't need init */
static float project_paint_uvpixel_mask(const ProjPaintState *ps,
                                        const int tri_index,
                                        const float w[3])
{
  float mask;

  /* Image Mask */
  if (ps->do_layer_stencil) {
    /* another UV maps image is masking this one's */
    ImBuf *ibuf_other;
    Image *other_tpage = ps->stencil_ima;

    if (other_tpage && (ibuf_other = BKE_image_acquire_ibuf(other_tpage, nullptr, nullptr))) {
      const int3 &tri_other = ps->corner_tris_eval[tri_index];
      const float *other_tri_uv[3] = {ps->uv_map_stencil_eval[tri_other[0]],
                                      ps->uv_map_stencil_eval[tri_other[1]],
                                      ps->uv_map_stencil_eval[tri_other[2]]};

      /* #BKE_image_acquire_ibuf - TODO: this may be slow. */
      uchar rgba_ub[4];
      float rgba_f[4];

      project_face_pixel(other_tri_uv, ibuf_other, w, rgba_ub, rgba_f);

      if (ibuf_other->float_buffer.data) { /* from float to float */
        mask = ((rgba_f[0] + rgba_f[1] + rgba_f[2]) * (1.0f / 3.0f)) * rgba_f[3];
      }
      else { /* from char to float */
        mask = ((rgba_ub[0] + rgba_ub[1] + rgba_ub[2]) * (1.0f / (255.0f * 3.0f))) *
               (rgba_ub[3] * (1.0f / 255.0f));
      }

      BKE_image_release_ibuf(other_tpage, ibuf_other, nullptr);

      if (!ps->do_layer_stencil_inv) {
        /* matching the gimps layer mask black/white rules, white==full opacity */
        mask = (1.0f - mask);
      }

      if (mask == 0.0f) {
        return 0.0f;
      }
    }
    else {
      return 0.0f;
    }
  }
  else {
    mask = 1.0f;
  }

  if (ps->do_mask_cavity) {
    const int3 &tri = ps->corner_tris_eval[tri_index];
    const int vert_tri[3] = {PS_CORNER_TRI_AS_VERT_INDEX_3(ps, tri)};
    float ca1, ca2, ca3, ca_mask;
    ca1 = ps->cavities[vert_tri[0]];
    ca2 = ps->cavities[vert_tri[1]];
    ca3 = ps->cavities[vert_tri[2]];

    ca_mask = w[0] * ca1 + w[1] * ca2 + w[2] * ca3;
    ca_mask = BKE_curvemapping_evaluateF(ps->cavity_curve, 0, ca_mask);
    CLAMP(ca_mask, 0.0f, 1.0f);
    mask *= ca_mask;
  }

  /* calculate mask */
  if (ps->do_mask_normal) {
    const int3 &tri = ps->corner_tris_eval[tri_index];
    const int face_i = ps->corner_tri_faces_eval[tri_index];
    const int vert_tri[3] = {PS_CORNER_TRI_AS_VERT_INDEX_3(ps, tri)};
    float no[3], angle_cos;

    if (!(ps->sharp_faces_eval && ps->sharp_faces_eval[face_i])) {
      const float *no1, *no2, *no3;
      no1 = ps->vert_normals[vert_tri[0]];
      no2 = ps->vert_normals[vert_tri[1]];
      no3 = ps->vert_normals[vert_tri[2]];

      no[0] = w[0] * no1[0] + w[1] * no2[0] + w[2] * no3[0];
      no[1] = w[0] * no1[1] + w[1] * no2[1] + w[2] * no3[1];
      no[2] = w[0] * no1[2] + w[1] * no2[2] + w[2] * no3[2];
      normalize_v3(no);
    }
    else {
      /* In case the normalizing per pixel isn't optimal,
       * we could cache or access from evaluated mesh. */
      normal_tri_v3(no,
                    ps->vert_positions_eval[vert_tri[0]],
                    ps->vert_positions_eval[vert_tri[1]],
                    ps->vert_positions_eval[vert_tri[2]]);
    }

    if (UNLIKELY(ps->is_flip_object)) {
      negate_v3(no);
    }

    /* now we can use the normal as a mask */
    if (ps->is_ortho) {
      angle_cos = dot_v3v3(ps->viewDir, no);
    }
    else {
      /* Annoying but for the perspective view we need to get the pixels location in 3D space :/ */
      float viewDirPersp[3];
      const float *co1, *co2, *co3;
      co1 = ps->vert_positions_eval[vert_tri[0]];
      co2 = ps->vert_positions_eval[vert_tri[1]];
      co3 = ps->vert_positions_eval[vert_tri[2]];

      /* Get the direction from the viewPoint to the pixel and normalize */
      viewDirPersp[0] = (ps->viewPos[0] - (w[0] * co1[0] + w[1] * co2[0] + w[2] * co3[0]));
      viewDirPersp[1] = (ps->viewPos[1] - (w[0] * co1[1] + w[1] * co2[1] + w[2] * co3[1]));
      viewDirPersp[2] = (ps->viewPos[2] - (w[0] * co1[2] + w[1] * co2[2] + w[2] * co3[2]));
      normalize_v3(viewDirPersp);
      if (UNLIKELY(ps->is_flip_object)) {
        negate_v3(viewDirPersp);
      }

      angle_cos = dot_v3v3(viewDirPersp, no);
    }

    /* If back-face culling is disabled, allow painting on back faces. */
    if (!ps->do_backfacecull) {
      angle_cos = fabsf(angle_cos);
    }

    if (angle_cos <= ps->normal_angle__cos) {
      /* Outsize the normal limit. */
      return 0.0f;
    }
    if (angle_cos < ps->normal_angle_inner__cos) {
      mask *= (ps->normal_angle - acosf(angle_cos)) / ps->normal_angle_range;
    } /* otherwise no mask normal is needed, we're within the limit */
  }

  /* This only works when the opacity doesn't change while painting, stylus pressure messes with
   * this so don't use it. */
  // if (ps->is_airbrush == 0) mask *= BKE_brush_alpha_get(ps->brush);

  return mask;
}

static int project_paint_pixel_sizeof(const short brush_type)
{
  if (ELEM(brush_type, IMAGE_PAINT_BRUSH_TYPE_CLONE, IMAGE_PAINT_BRUSH_TYPE_SMEAR)) {
    return sizeof(ProjPixelClone);
  }
  return sizeof(ProjPixel);
}

static int project_paint_undo_subtiles(const TileInfo *tinf, int tx, int ty)
{
  ProjPaintImage *pjIma = tinf->pjima;
  int tile_index = tx + ty * tinf->tile_width;
  bool generate_tile = false;

  /* double check lock to avoid locking */
  if (UNLIKELY(!pjIma->undoRect[tile_index])) {
    if (tinf->lock) {
      BLI_spin_lock(tinf->lock);
    }
    if (LIKELY(!pjIma->undoRect[tile_index])) {
      pjIma->undoRect[tile_index] = TILE_PENDING;
      generate_tile = true;
    }
    if (tinf->lock) {
      BLI_spin_unlock(tinf->lock);
    }
  }

  if (generate_tile) {
    PaintTileMap *undo_tiles = ED_image_paint_tile_map_get();
    volatile void *undorect;
    if (tinf->masked) {
      undorect = ED_image_paint_tile_push(undo_tiles,
                                          pjIma->ima,
                                          pjIma->ibuf,
                                          tinf->tmpibuf,
                                          &pjIma->iuser,
                                          tx,
                                          ty,
                                          &pjIma->maskRect[tile_index],
                                          &pjIma->valid[tile_index],
                                          true,
                                          false);
    }
    else {
      undorect = ED_image_paint_tile_push(undo_tiles,
                                          pjIma->ima,
                                          pjIma->ibuf,
                                          tinf->tmpibuf,
                                          &pjIma->iuser,
                                          tx,
                                          ty,
                                          nullptr,
                                          &pjIma->valid[tile_index],
                                          true,
                                          false);
    }

    BKE_image_mark_dirty(pjIma->ima, pjIma->ibuf);
    /* tile ready, publish */
    if (tinf->lock) {
      BLI_spin_lock(tinf->lock);
    }
    pjIma->undoRect[tile_index] = undorect;
    if (tinf->lock) {
      BLI_spin_unlock(tinf->lock);
    }
  }

  return tile_index;
}

/* run this function when we know a bucket's, face's pixel can be initialized,
 * return the ProjPixel which is added to 'ps->bucketRect[bucket_index]' */
static ProjPixel *project_paint_uvpixel_init(const ProjPaintState *ps,
                                             MemArena *arena,
                                             const TileInfo *tinf,
                                             int x_px,
                                             int y_px,
                                             const float mask,
                                             const int tri_index,
                                             const float pixelScreenCo[4],
                                             const float world_spaceCo[3],
                                             const float w[3])
{
  ProjPixel *projPixel;
  int x_tile, y_tile;
  int x_round, y_round;
  int tile_offset;
  /* Volatile is important here to ensure pending check is not optimized away by compiler. */
  volatile int tile_index;

  ProjPaintImage *projima = tinf->pjima;
  ImBuf *ibuf = projima->ibuf;
  /* wrap pixel location */

  x_px = mod_i(x_px, ibuf->x);
  y_px = mod_i(y_px, ibuf->y);

  BLI_assert(ps->pixel_sizeof == project_paint_pixel_sizeof(ps->brush_type));
  projPixel = static_cast<ProjPixel *>(BLI_memarena_alloc(arena, ps->pixel_sizeof));

  /* calculate the undo tile offset of the pixel, used to store the original
   * pixel color and accumulated mask if any */
  x_tile = x_px >> ED_IMAGE_UNDO_TILE_BITS;
  y_tile = y_px >> ED_IMAGE_UNDO_TILE_BITS;

  x_round = x_tile * ED_IMAGE_UNDO_TILE_SIZE;
  y_round = y_tile * ED_IMAGE_UNDO_TILE_SIZE;
  // memset(projPixel, 0, size);

  tile_offset = (x_px - x_round) + (y_px - y_round) * ED_IMAGE_UNDO_TILE_SIZE;
  tile_index = project_paint_undo_subtiles(tinf, x_tile, y_tile);

  /* other thread may be initializing the tile so wait here */
  while (projima->undoRect[tile_index] == TILE_PENDING) {
    /* pass */
  }

  BLI_assert(tile_index <
             (ED_IMAGE_UNDO_TILE_NUMBER(ibuf->x) * ED_IMAGE_UNDO_TILE_NUMBER(ibuf->y)));
  BLI_assert(tile_offset < (ED_IMAGE_UNDO_TILE_SIZE * ED_IMAGE_UNDO_TILE_SIZE));

  projPixel->valid = projima->valid[tile_index];

  if (ibuf->float_buffer.data) {
    projPixel->pixel.f_pt = ibuf->float_buffer.data + ((x_px + y_px * ibuf->x) * 4);
    projPixel->origColor.f_pt = (float *)projima->undoRect[tile_index] + 4 * tile_offset;
    zero_v4(projPixel->newColor.f);
  }
  else {
    projPixel->pixel.ch_pt = ibuf->byte_buffer.data + (x_px + y_px * ibuf->x) * 4;
    projPixel->origColor.uint_pt = (uint *)projima->undoRect[tile_index] + tile_offset;
    projPixel->newColor.uint_ = 0;
  }

  /* Screen-space unclamped, we could keep its z and w values but don't need them at the moment. */
  if (ps->brush->mtex.brush_map_mode == MTEX_MAP_MODE_3D) {
    copy_v3_v3(projPixel->worldCoSS, world_spaceCo);
  }

  copy_v2_v2(projPixel->projCoSS, pixelScreenCo);

  projPixel->x_px = x_px;
  projPixel->y_px = y_px;

  projPixel->mask = ushort(mask * 65535);
  if (ps->do_masking) {
    projPixel->mask_accum = projima->maskRect[tile_index] + tile_offset;
  }
  else {
    projPixel->mask_accum = nullptr;
  }

  /* which bounding box cell are we in?, needed for undo */
  projPixel->bb_cell_index = int((float(x_px) / float(ibuf->x)) * PROJ_BOUNDBOX_DIV) +
                             int((float(y_px) / float(ibuf->y)) * PROJ_BOUNDBOX_DIV) *
                                 PROJ_BOUNDBOX_DIV;

  /* done with view3d_project_float inline */
  if (ps->brush_type == IMAGE_PAINT_BRUSH_TYPE_CLONE) {
    if (ps->poly_to_loop_uv_clone) {
      ImBuf *ibuf_other;
      Image *other_tpage = project_paint_face_clone_image(ps, tri_index);

      if (other_tpage && (ibuf_other = BKE_image_acquire_ibuf(other_tpage, nullptr, nullptr))) {
        const int3 &tri_other = ps->corner_tris_eval[tri_index];
        const int poly_other_i = ps->corner_tri_faces_eval[tri_index];
        const float *other_tri_uv[3] = {
            PS_CORNER_TRI_AS_UV_3(ps->poly_to_loop_uv_clone, poly_other_i, tri_other)};

        /* #BKE_image_acquire_ibuf - TODO: this may be slow. */

        if (ibuf->float_buffer.data) {
          if (ibuf_other->float_buffer.data) { /* from float to float */
            project_face_pixel(
                other_tri_uv, ibuf_other, w, nullptr, ((ProjPixelClone *)projPixel)->clonepx.f);
          }
          else { /* from char to float */
            uchar rgba_ub[4];
            float rgba[4];
            project_face_pixel(other_tri_uv, ibuf_other, w, rgba_ub, nullptr);
            rgba_uchar_to_float(rgba, rgba_ub);
            IMB_colormanagement_colorspace_to_scene_linear_v3(rgba,
                                                              ibuf_other->byte_buffer.colorspace);
            straight_to_premul_v4_v4(((ProjPixelClone *)projPixel)->clonepx.f, rgba);
          }
        }
        else {
          if (ibuf_other->float_buffer.data) { /* float to char */
            float rgba[4];
            project_face_pixel(other_tri_uv, ibuf_other, w, nullptr, rgba);
            premul_to_straight_v4(rgba);
            IMB_colormanagement_scene_linear_to_colorspace_v3(rgba, ibuf->byte_buffer.colorspace);
            rgba_float_to_uchar(((ProjPixelClone *)projPixel)->clonepx.ch, rgba);
          }
          else { /* char to char */
            project_face_pixel(
                other_tri_uv, ibuf_other, w, ((ProjPixelClone *)projPixel)->clonepx.ch, nullptr);
          }
        }

        BKE_image_release_ibuf(other_tpage, ibuf_other, nullptr);
      }
      else {
        if (ibuf->float_buffer.data) {
          ((ProjPixelClone *)projPixel)->clonepx.f[3] = 0;
        }
        else {
          ((ProjPixelClone *)projPixel)->clonepx.ch[3] = 0;
        }
      }
    }
    else {
      float co[2];
      sub_v2_v2v2(co, projPixel->projCoSS, ps->cloneOffset);

      /* no need to initialize the bucket, we're only checking buckets faces and for this
       * the faces are already initialized in project_paint_delayed_face_init(...) */
      if (ibuf->float_buffer.data) {
        if (!project_paint_PickColor(
                ps, co, ((ProjPixelClone *)projPixel)->clonepx.f, nullptr, true))
        {
          /* zero alpha - ignore */
          ((ProjPixelClone *)projPixel)->clonepx.f[3] = 0;
        }
      }
      else {
        if (!project_paint_PickColor(
                ps, co, nullptr, ((ProjPixelClone *)projPixel)->clonepx.ch, true))
        {
          /* zero alpha - ignore */
          ((ProjPixelClone *)projPixel)->clonepx.ch[3] = 0;
        }
      }
    }
  }

#ifdef PROJ_DEBUG_PAINT
  if (ibuf->float_buffer.data) {
    projPixel->pixel.f_pt[0] = 0;
  }
  else {
    projPixel->pixel.ch_pt[0] = 0;
  }
#endif
  /* pointer arithmetic */
  projPixel->image_index = projima - ps->projImages;

  return projPixel;
}

static bool line_clip_rect2f(const rctf *cliprect,
                             const rctf *rect,
                             const float l1[2],
                             const float l2[2],
                             float l1_clip[2],
                             float l2_clip[2])
{
  /* first account for horizontal, then vertical lines */
  /* Horizontal. */
  if (fabsf(l1[1] - l2[1]) < PROJ_PIXEL_TOLERANCE) {
    /* is the line out of range on its Y axis? */
    if (l1[1] < rect->ymin || l1[1] > rect->ymax) {
      return false;
    }
    /* line is out of range on its X axis */
    if ((l1[0] < rect->xmin && l2[0] < rect->xmin) || (l1[0] > rect->xmax && l2[0] > rect->xmax)) {
      return false;
    }

    /* This is a single point  (or close to). */
    if (fabsf(l1[0] - l2[0]) < PROJ_PIXEL_TOLERANCE) {
      if (BLI_rctf_isect_pt_v(rect, l1)) {
        copy_v2_v2(l1_clip, l1);
        copy_v2_v2(l2_clip, l2);
        return true;
      }
      return false;
    }

    copy_v2_v2(l1_clip, l1);
    copy_v2_v2(l2_clip, l2);
    CLAMP(l1_clip[0], rect->xmin, rect->xmax);
    CLAMP(l2_clip[0], rect->xmin, rect->xmax);
    return true;
  }
  if (fabsf(l1[0] - l2[0]) < PROJ_PIXEL_TOLERANCE) {
    /* is the line out of range on its X axis? */
    if (l1[0] < rect->xmin || l1[0] > rect->xmax) {
      return false;
    }

    /* line is out of range on its Y axis */
    if ((l1[1] < rect->ymin && l2[1] < rect->ymin) || (l1[1] > rect->ymax && l2[1] > rect->ymax)) {
      return false;
    }

    /* This is a single point  (or close to). */
    if (fabsf(l1[1] - l2[1]) < PROJ_PIXEL_TOLERANCE) {
      if (BLI_rctf_isect_pt_v(rect, l1)) {
        copy_v2_v2(l1_clip, l1);
        copy_v2_v2(l2_clip, l2);
        return true;
      }
      return false;
    }

    copy_v2_v2(l1_clip, l1);
    copy_v2_v2(l2_clip, l2);
    CLAMP(l1_clip[1], rect->ymin, rect->ymax);
    CLAMP(l2_clip[1], rect->ymin, rect->ymax);
    return true;
  }

  float isect;
  short ok1 = 0;
  short ok2 = 0;

  /* Done with vertical lines */

  /* are either of the points inside the rectangle ? */
  if (BLI_rctf_isect_pt_v(rect, l1)) {
    copy_v2_v2(l1_clip, l1);
    ok1 = 1;
  }

  if (BLI_rctf_isect_pt_v(rect, l2)) {
    copy_v2_v2(l2_clip, l2);
    ok2 = 1;
  }

  /* line inside rect */
  if (ok1 && ok2) {
    return true;
  }

  /* top/bottom */
  if (line_isect_y(l1, l2, rect->ymin, &isect) && (isect >= cliprect->xmin) &&
      (isect <= cliprect->xmax))
  {
    if (l1[1] < l2[1]) { /* line 1 is outside */
      l1_clip[0] = isect;
      l1_clip[1] = rect->ymin;
      ok1 = 1;
    }
    else {
      l2_clip[0] = isect;
      l2_clip[1] = rect->ymin;
      ok2 = 2;
    }
  }

  if (ok1 && ok2) {
    return true;
  }

  if (line_isect_y(l1, l2, rect->ymax, &isect) && (isect >= cliprect->xmin) &&
      (isect <= cliprect->xmax))
  {
    if (l1[1] > l2[1]) { /* line 1 is outside */
      l1_clip[0] = isect;
      l1_clip[1] = rect->ymax;
      ok1 = 1;
    }
    else {
      l2_clip[0] = isect;
      l2_clip[1] = rect->ymax;
      ok2 = 2;
    }
  }

  if (ok1 && ok2) {
    return true;
  }

  /* left/right */
  if (line_isect_x(l1, l2, rect->xmin, &isect) && (isect >= cliprect->ymin) &&
      (isect <= cliprect->ymax))
  {
    if (l1[0] < l2[0]) { /* line 1 is outside */
      l1_clip[0] = rect->xmin;
      l1_clip[1] = isect;
      ok1 = 1;
    }
    else {
      l2_clip[0] = rect->xmin;
      l2_clip[1] = isect;
      ok2 = 2;
    }
  }

  if (ok1 && ok2) {
    return true;
  }

  if (line_isect_x(l1, l2, rect->xmax, &isect) && (isect >= cliprect->ymin) &&
      (isect <= cliprect->ymax))
  {
    if (l1[0] > l2[0]) { /* line 1 is outside */
      l1_clip[0] = rect->xmax;
      l1_clip[1] = isect;
      ok1 = 1;
    }
    else {
      l2_clip[0] = rect->xmax;
      l2_clip[1] = isect;
      ok2 = 2;
    }
  }

  if (ok1 && ok2) {
    return true;
  }
  return false;
}

/**
 * Scale the tri about its center
 * scaling by #PROJ_FACE_SCALE_SEAM (0.99x) is used for getting fake UV pixel coords that are on
 * the edge of the face but slightly inside it occlusion tests don't return hits on adjacent faces.
 */
#ifndef PROJ_DEBUG_NOSEAMBLEED

static void scale_tri(float insetCos[3][3], const float *origCos[3], const float inset)
{
  float cent[3];
  cent[0] = (origCos[0][0] + origCos[1][0] + origCos[2][0]) * (1.0f / 3.0f);
  cent[1] = (origCos[0][1] + origCos[1][1] + origCos[2][1]) * (1.0f / 3.0f);
  cent[2] = (origCos[0][2] + origCos[1][2] + origCos[2][2]) * (1.0f / 3.0f);

  sub_v3_v3v3(insetCos[0], origCos[0], cent);
  sub_v3_v3v3(insetCos[1], origCos[1], cent);
  sub_v3_v3v3(insetCos[2], origCos[2], cent);

  mul_v3_fl(insetCos[0], inset);
  mul_v3_fl(insetCos[1], inset);
  mul_v3_fl(insetCos[2], inset);

  add_v3_v3(insetCos[0], cent);
  add_v3_v3(insetCos[1], cent);
  add_v3_v3(insetCos[2], cent);
}
#endif  // PROJ_DEBUG_NOSEAMBLEED

static float len_squared_v2v2_alt(const float v1[2], const float v2_1, const float v2_2)
{
  float x, y;

  x = v1[0] - v2_1;
  y = v1[1] - v2_2;
  return x * x + y * y;
}

/**
 * \note Use a squared value so we can use #len_squared_v2v2
 * be sure that you have done a bounds check first or this may fail.
 *
 * Only give \a bucket_bounds as an arg because we need it elsewhere.
 */
static bool project_bucket_isect_circle(const float cent[2],
                                        const float radius_squared,
                                        const rctf *bucket_bounds)
{

  /* Would normally to a simple intersection test,
   * however we know the bounds of these 2 already intersect so we only need to test
   * if the center is inside the vertical or horizontal bounds on either axis,
   * this is even less work than an intersection test.
   */
#if 0
  if (BLI_rctf_isect_pt_v(bucket_bounds, cent)) {
    return true;
  }
#endif

  if ((bucket_bounds->xmin <= cent[0] && bucket_bounds->xmax >= cent[0]) ||
      (bucket_bounds->ymin <= cent[1] && bucket_bounds->ymax >= cent[1]))
  {
    return true;
  }

  /* out of bounds left */
  if (cent[0] < bucket_bounds->xmin) {
    /* lower left out of radius test */
    if (cent[1] < bucket_bounds->ymin) {
      return (len_squared_v2v2_alt(cent, bucket_bounds->xmin, bucket_bounds->ymin) <
              radius_squared) ?
                 true :
                 false;
    }
    /* top left test */
    if (cent[1] > bucket_bounds->ymax) {
      return (len_squared_v2v2_alt(cent, bucket_bounds->xmin, bucket_bounds->ymax) <
              radius_squared) ?
                 true :
                 false;
    }
  }
  else if (cent[0] > bucket_bounds->xmax) {
    /* lower right out of radius test */
    if (cent[1] < bucket_bounds->ymin) {
      return (len_squared_v2v2_alt(cent, bucket_bounds->xmax, bucket_bounds->ymin) <
              radius_squared) ?
                 true :
                 false;
    }
    /* top right test */
    if (cent[1] > bucket_bounds->ymax) {
      return (len_squared_v2v2_alt(cent, bucket_bounds->xmax, bucket_bounds->ymax) <
              radius_squared) ?
                 true :
                 false;
    }
  }

  return false;
}

/* Note for #rect_to_uvspace_ortho() and #rect_to_uvspace_persp()
 * in ortho view this function gives good results when bucket_bounds are outside the triangle
 * however in some cases, perspective view will mess up with faces
 * that have minimal screen-space area (viewed from the side).
 *
 * for this reason its not reliable in this case so we'll use the Simple Barycentric'
 * functions that only account for points inside the triangle.
 * however switching back to this for ortho is always an option. */

static void rect_to_uvspace_ortho(const rctf *bucket_bounds,
                                  const float *v1coSS,
                                  const float *v2coSS,
                                  const float *v3coSS,
                                  const float *uv1co,
                                  const float *uv2co,
                                  const float *uv3co,
                                  float bucket_bounds_uv[4][2],
                                  const int flip)
{
  float uv[2];
  float w[3];

  /* get the UV space bounding box */
  uv[0] = bucket_bounds->xmax;
  uv[1] = bucket_bounds->ymin;
  barycentric_weights_v2(v1coSS, v2coSS, v3coSS, uv, w);
  interp_v2_v2v2v2(bucket_bounds_uv[flip ? 3 : 0], uv1co, uv2co, uv3co, w);

  // uv[0] = bucket_bounds->xmax; // set above
  uv[1] = bucket_bounds->ymax;
  barycentric_weights_v2(v1coSS, v2coSS, v3coSS, uv, w);
  interp_v2_v2v2v2(bucket_bounds_uv[flip ? 2 : 1], uv1co, uv2co, uv3co, w);

  uv[0] = bucket_bounds->xmin;
  // uv[1] = bucket_bounds->ymax; // set above
  barycentric_weights_v2(v1coSS, v2coSS, v3coSS, uv, w);
  interp_v2_v2v2v2(bucket_bounds_uv[flip ? 1 : 2], uv1co, uv2co, uv3co, w);

  // uv[0] = bucket_bounds->xmin; // set above
  uv[1] = bucket_bounds->ymin;
  barycentric_weights_v2(v1coSS, v2coSS, v3coSS, uv, w);
  interp_v2_v2v2v2(bucket_bounds_uv[flip ? 0 : 3], uv1co, uv2co, uv3co, w);
}

/**
 * Same as #rect_to_uvspace_ortho but use #barycentric_weights_v2_persp.
 */
static void rect_to_uvspace_persp(const rctf *bucket_bounds,
                                  const float *v1coSS,
                                  const float *v2coSS,
                                  const float *v3coSS,
                                  const float *uv1co,
                                  const float *uv2co,
                                  const float *uv3co,
                                  float bucket_bounds_uv[4][2],
                                  const int flip)
{
  float uv[2];
  float w[3];

  /* get the UV space bounding box */
  uv[0] = bucket_bounds->xmax;
  uv[1] = bucket_bounds->ymin;
  barycentric_weights_v2_persp(v1coSS, v2coSS, v3coSS, uv, w);
  interp_v2_v2v2v2(bucket_bounds_uv[flip ? 3 : 0], uv1co, uv2co, uv3co, w);

  // uv[0] = bucket_bounds->xmax; // set above
  uv[1] = bucket_bounds->ymax;
  barycentric_weights_v2_persp(v1coSS, v2coSS, v3coSS, uv, w);
  interp_v2_v2v2v2(bucket_bounds_uv[flip ? 2 : 1], uv1co, uv2co, uv3co, w);

  uv[0] = bucket_bounds->xmin;
  // uv[1] = bucket_bounds->ymax; // set above
  barycentric_weights_v2_persp(v1coSS, v2coSS, v3coSS, uv, w);
  interp_v2_v2v2v2(bucket_bounds_uv[flip ? 1 : 2], uv1co, uv2co, uv3co, w);

  // uv[0] = bucket_bounds->xmin; // set above
  uv[1] = bucket_bounds->ymin;
  barycentric_weights_v2_persp(v1coSS, v2coSS, v3coSS, uv, w);
  interp_v2_v2v2v2(bucket_bounds_uv[flip ? 0 : 3], uv1co, uv2co, uv3co, w);
}

/* This works as we need it to but we can save a few steps and not use it */

#if 0
static float angle_2d_clockwise(const float p1[2], const float p2[2], const float p3[2])
{
  float v1[2], v2[2];

  v1[0] = p1[0] - p2[0];
  v1[1] = p1[1] - p2[1];
  v2[0] = p3[0] - p2[0];
  v2[1] = p3[1] - p2[1];

  return -atan2f(v1[0] * v2[1] - v1[1] * v2[0], v1[0] * v2[0] + v1[1] * v2[1]);
}
#endif

#define ISECT_1 (1)
#define ISECT_2 (1 << 1)
#define ISECT_3 (1 << 2)
#define ISECT_4 (1 << 3)
#define ISECT_ALL3 ((1 << 3) - 1)
#define ISECT_ALL4 ((1 << 4) - 1)

/* limit must be a fraction over 1.0f */
static bool IsectPT2Df_limit(
    const float pt[2], const float v1[2], const float v2[2], const float v3[2], const float limit)
{
  return ((area_tri_v2(pt, v1, v2) + area_tri_v2(pt, v2, v3) + area_tri_v2(pt, v3, v1)) /
          area_tri_v2(v1, v2, v3)) < limit;
}

/**
 * Clip the face by a bucket and set the uv-space bucket_bounds_uv
 * so we have the clipped UVs to do pixel intersection tests with
 */
static int float_z_sort_flip(const void *p1, const void *p2)
{
  return (((float *)p1)[2] < ((float *)p2)[2] ? 1 : -1);
}

static int float_z_sort(const void *p1, const void *p2)
{
  return (((float *)p1)[2] < ((float *)p2)[2] ? -1 : 1);
}

/* assumes one point is within the rectangle */
static bool line_rect_clip(const rctf *rect,
                           const float l1[4],
                           const float l2[4],
                           const float uv1[2],
                           const float uv2[2],
                           float uv[2],
                           bool is_ortho)
{
  float min = FLT_MAX, tmp;
  float xlen = l2[0] - l1[0];
  float ylen = l2[1] - l1[1];

  /* 0.1 might seem too much, but remember, this is pixels! */
  if (xlen > 0.1f) {
    if ((l1[0] - rect->xmin) * (l2[0] - rect->xmin) <= 0) {
      tmp = rect->xmin;
      min = min_ff((tmp - l1[0]) / xlen, min);
    }
    else if ((l1[0] - rect->xmax) * (l2[0] - rect->xmax) < 0) {
      tmp = rect->xmax;
      min = min_ff((tmp - l1[0]) / xlen, min);
    }
  }

  if (ylen > 0.1f) {
    if ((l1[1] - rect->ymin) * (l2[1] - rect->ymin) <= 0) {
      tmp = rect->ymin;
      min = min_ff((tmp - l1[1]) / ylen, min);
    }
    else if ((l1[1] - rect->ymax) * (l2[1] - rect->ymax) < 0) {
      tmp = rect->ymax;
      min = min_ff((tmp - l1[1]) / ylen, min);
    }
  }

  if (min == FLT_MAX) {
    return false;
  }

  tmp = (is_ortho) ? 1.0f : (l1[3] + min * (l2[3] - l1[3]));

  uv[0] = (uv1[0] + min / tmp * (uv2[0] - uv1[0]));
  uv[1] = (uv1[1] + min / tmp * (uv2[1] - uv1[1]));

  return true;
}

static void project_bucket_clip_face(const bool is_ortho,
                                     const bool is_flip_object,
                                     const rctf *cliprect,
                                     const rctf *bucket_bounds,
                                     const float *v1coSS,
                                     const float *v2coSS,
                                     const float *v3coSS,
                                     const float *uv1co,
                                     const float *uv2co,
                                     const float *uv3co,
                                     float bucket_bounds_uv[8][2],
                                     int *tot,
                                     bool cull)
{
  int inside_bucket_flag = 0;
  int inside_face_flag = 0;
  int flip;
  bool collinear = false;

  float bucket_bounds_ss[4][2];

  /* detect pathological case where face the three vertices are almost collinear in screen space.
   * mostly those will be culled but when flood filling or with
   * smooth shading it's a possibility */
  if (min_fff(dist_squared_to_line_v2(v1coSS, v2coSS, v3coSS),
              dist_squared_to_line_v2(v2coSS, v3coSS, v1coSS),
              dist_squared_to_line_v2(v3coSS, v1coSS, v2coSS)) < PROJ_PIXEL_TOLERANCE)
  {
    collinear = true;
  }

  /* get the UV space bounding box */
  inside_bucket_flag |= int(BLI_rctf_isect_pt_v(bucket_bounds, v1coSS));
  inside_bucket_flag |= int(BLI_rctf_isect_pt_v(bucket_bounds, v2coSS)) << 1;
  inside_bucket_flag |= int(BLI_rctf_isect_pt_v(bucket_bounds, v3coSS)) << 2;

  if (inside_bucket_flag == ISECT_ALL3) {
    /* is_flip_object is used here because we use the face winding */
    flip = (((line_point_side_v2(v1coSS, v2coSS, v3coSS) > 0.0f) != is_flip_object) !=
            (line_point_side_v2(uv1co, uv2co, uv3co) > 0.0f));

    /* All screen-space points are inside the bucket bounding box,
     * this means we don't need to clip and can simply return the UVs. */
    if (flip) { /* facing the back? */
      copy_v2_v2(bucket_bounds_uv[0], uv3co);
      copy_v2_v2(bucket_bounds_uv[1], uv2co);
      copy_v2_v2(bucket_bounds_uv[2], uv1co);
    }
    else {
      copy_v2_v2(bucket_bounds_uv[0], uv1co);
      copy_v2_v2(bucket_bounds_uv[1], uv2co);
      copy_v2_v2(bucket_bounds_uv[2], uv3co);
    }

    *tot = 3;
    return;
  }
  /* Handle pathological case here,
   * no need for further intersections below since triangle area is almost zero. */
  if (collinear) {
    int flag;

    (*tot) = 0;

    if (cull) {
      return;
    }

    if (inside_bucket_flag & ISECT_1) {
      copy_v2_v2(bucket_bounds_uv[*tot], uv1co);
      (*tot)++;
    }

    flag = inside_bucket_flag & (ISECT_1 | ISECT_2);
    if (flag && flag != (ISECT_1 | ISECT_2)) {
      if (line_rect_clip(
              bucket_bounds, v1coSS, v2coSS, uv1co, uv2co, bucket_bounds_uv[*tot], is_ortho))
      {
        (*tot)++;
      }
    }

    if (inside_bucket_flag & ISECT_2) {
      copy_v2_v2(bucket_bounds_uv[*tot], uv2co);
      (*tot)++;
    }

    flag = inside_bucket_flag & (ISECT_2 | ISECT_3);
    if (flag && flag != (ISECT_2 | ISECT_3)) {
      if (line_rect_clip(
              bucket_bounds, v2coSS, v3coSS, uv2co, uv3co, bucket_bounds_uv[*tot], is_ortho))
      {
        (*tot)++;
      }
    }

    if (inside_bucket_flag & ISECT_3) {
      copy_v2_v2(bucket_bounds_uv[*tot], uv3co);
      (*tot)++;
    }

    flag = inside_bucket_flag & (ISECT_3 | ISECT_1);
    if (flag && flag != (ISECT_3 | ISECT_1)) {
      if (line_rect_clip(
              bucket_bounds, v3coSS, v1coSS, uv3co, uv1co, bucket_bounds_uv[*tot], is_ortho))
      {
        (*tot)++;
      }
    }

    if ((*tot) < 3) {
      /* no intersections to speak of, but more probable is that all face is just outside the
       * rectangle and culled due to float precision issues. Since above tests have failed,
       * just dump triangle as is for painting */
      *tot = 0;
      copy_v2_v2(bucket_bounds_uv[*tot], uv1co);
      (*tot)++;
      copy_v2_v2(bucket_bounds_uv[*tot], uv2co);
      (*tot)++;
      copy_v2_v2(bucket_bounds_uv[*tot], uv3co);
      (*tot)++;
      return;
    }

    return;
  }

  /* Get the UV space bounding box. */
  /* Use #IsectPT2Df_limit here so we catch points are touching the triangles edge
   * (or a small fraction over) */
  bucket_bounds_ss[0][0] = bucket_bounds->xmax;
  bucket_bounds_ss[0][1] = bucket_bounds->ymin;
  inside_face_flag |= (IsectPT2Df_limit(
                           bucket_bounds_ss[0], v1coSS, v2coSS, v3coSS, 1 + PROJ_GEOM_TOLERANCE) ?
                           ISECT_1 :
                           0);

  bucket_bounds_ss[1][0] = bucket_bounds->xmax;
  bucket_bounds_ss[1][1] = bucket_bounds->ymax;
  inside_face_flag |= (IsectPT2Df_limit(
                           bucket_bounds_ss[1], v1coSS, v2coSS, v3coSS, 1 + PROJ_GEOM_TOLERANCE) ?
                           ISECT_2 :
                           0);

  bucket_bounds_ss[2][0] = bucket_bounds->xmin;
  bucket_bounds_ss[2][1] = bucket_bounds->ymax;
  inside_face_flag |= (IsectPT2Df_limit(
                           bucket_bounds_ss[2], v1coSS, v2coSS, v3coSS, 1 + PROJ_GEOM_TOLERANCE) ?
                           ISECT_3 :
                           0);

  bucket_bounds_ss[3][0] = bucket_bounds->xmin;
  bucket_bounds_ss[3][1] = bucket_bounds->ymin;
  inside_face_flag |= (IsectPT2Df_limit(
                           bucket_bounds_ss[3], v1coSS, v2coSS, v3coSS, 1 + PROJ_GEOM_TOLERANCE) ?
                           ISECT_4 :
                           0);

  flip = ((line_point_side_v2(v1coSS, v2coSS, v3coSS) > 0.0f) !=
          (line_point_side_v2(uv1co, uv2co, uv3co) > 0.0f));

  if (inside_face_flag == ISECT_ALL4) {
    /* Bucket is totally inside the screen-space face, we can safely use weights. */

    if (is_ortho) {
      rect_to_uvspace_ortho(
          bucket_bounds, v1coSS, v2coSS, v3coSS, uv1co, uv2co, uv3co, bucket_bounds_uv, flip);
    }
    else {
      rect_to_uvspace_persp(
          bucket_bounds, v1coSS, v2coSS, v3coSS, uv1co, uv2co, uv3co, bucket_bounds_uv, flip);
    }

    *tot = 4;
    return;
  }

  {
    /* The Complicated Case!
     *
     * The 2 cases above are where the face is inside the bucket
     * or the bucket is inside the face.
     *
     * we need to make a convex poly-line from the intersection between the screen-space face
     * and the bucket bounds.
     *
     * There are a number of ways this could be done, currently it just collects all
     * intersecting verts, and line intersections, then sorts them clockwise, this is
     * a lot easier than evaluating the geometry to do a correct clipping on both shapes.
     */

    /* Add a bunch of points, we know must make up the convex hull
     * which is the clipped rect and triangle */

    /* Maximum possible 6 intersections when using a rectangle and triangle */

    /* The 3rd float is used to store angle for qsort(), NOT as a Z location */
    float isectVCosSS[8][3];
    float v1_clipSS[2], v2_clipSS[2];
    float w[3];

    /* calc center */
    float cent[2] = {0.0f, 0.0f};
    // float up[2] = {0.0f, 1.0f};
    bool doubles;

    (*tot) = 0;

    if (inside_face_flag & ISECT_1) {
      copy_v2_v2(isectVCosSS[*tot], bucket_bounds_ss[0]);
      (*tot)++;
    }
    if (inside_face_flag & ISECT_2) {
      copy_v2_v2(isectVCosSS[*tot], bucket_bounds_ss[1]);
      (*tot)++;
    }
    if (inside_face_flag & ISECT_3) {
      copy_v2_v2(isectVCosSS[*tot], bucket_bounds_ss[2]);
      (*tot)++;
    }
    if (inside_face_flag & ISECT_4) {
      copy_v2_v2(isectVCosSS[*tot], bucket_bounds_ss[3]);
      (*tot)++;
    }

    if (inside_bucket_flag & ISECT_1) {
      copy_v2_v2(isectVCosSS[*tot], v1coSS);
      (*tot)++;
    }
    if (inside_bucket_flag & ISECT_2) {
      copy_v2_v2(isectVCosSS[*tot], v2coSS);
      (*tot)++;
    }
    if (inside_bucket_flag & ISECT_3) {
      copy_v2_v2(isectVCosSS[*tot], v3coSS);
      (*tot)++;
    }

    if ((inside_bucket_flag & (ISECT_1 | ISECT_2)) != (ISECT_1 | ISECT_2)) {
      if (line_clip_rect2f(cliprect, bucket_bounds, v1coSS, v2coSS, v1_clipSS, v2_clipSS)) {
        if ((inside_bucket_flag & ISECT_1) == 0) {
          copy_v2_v2(isectVCosSS[*tot], v1_clipSS);
          (*tot)++;
        }
        if ((inside_bucket_flag & ISECT_2) == 0) {
          copy_v2_v2(isectVCosSS[*tot], v2_clipSS);
          (*tot)++;
        }
      }
    }

    if ((inside_bucket_flag & (ISECT_2 | ISECT_3)) != (ISECT_2 | ISECT_3)) {
      if (line_clip_rect2f(cliprect, bucket_bounds, v2coSS, v3coSS, v1_clipSS, v2_clipSS)) {
        if ((inside_bucket_flag & ISECT_2) == 0) {
          copy_v2_v2(isectVCosSS[*tot], v1_clipSS);
          (*tot)++;
        }
        if ((inside_bucket_flag & ISECT_3) == 0) {
          copy_v2_v2(isectVCosSS[*tot], v2_clipSS);
          (*tot)++;
        }
      }
    }

    if ((inside_bucket_flag & (ISECT_3 | ISECT_1)) != (ISECT_3 | ISECT_1)) {
      if (line_clip_rect2f(cliprect, bucket_bounds, v3coSS, v1coSS, v1_clipSS, v2_clipSS)) {
        if ((inside_bucket_flag & ISECT_3) == 0) {
          copy_v2_v2(isectVCosSS[*tot], v1_clipSS);
          (*tot)++;
        }
        if ((inside_bucket_flag & ISECT_1) == 0) {
          copy_v2_v2(isectVCosSS[*tot], v2_clipSS);
          (*tot)++;
        }
      }
    }

    if ((*tot) < 3) { /* no intersections to speak of */
      *tot = 0;
      return;
    }

    /* now we have all points we need, collect their angles and sort them clockwise */

    for (int i = 0; i < (*tot); i++) {
      cent[0] += isectVCosSS[i][0];
      cent[1] += isectVCosSS[i][1];
    }
    cent[0] = cent[0] / float(*tot);
    cent[1] = cent[1] / float(*tot);

    /* Collect angles for every point around the center point */

#if 0 /* uses a few more cycles than the above loop */
    for (int i = 0; i < (*tot); i++) {
      isectVCosSS[i][2] = angle_2d_clockwise(up, cent, isectVCosSS[i]);
    }
#endif

    /* Abuse this var for the loop below */
    v1_clipSS[0] = cent[0];
    v1_clipSS[1] = cent[1] + 1.0f;

    for (int i = 0; i < (*tot); i++) {
      v2_clipSS[0] = isectVCosSS[i][0] - cent[0];
      v2_clipSS[1] = isectVCosSS[i][1] - cent[1];
      isectVCosSS[i][2] = atan2f(v1_clipSS[0] * v2_clipSS[1] - v1_clipSS[1] * v2_clipSS[0],
                                 v1_clipSS[0] * v2_clipSS[0] + v1_clipSS[1] * v2_clipSS[1]);
    }

    if (flip) {
      qsort(isectVCosSS, *tot, sizeof(float[3]), float_z_sort_flip);
    }
    else {
      qsort(isectVCosSS, *tot, sizeof(float[3]), float_z_sort);
    }

    doubles = true;
    while (doubles == true) {
      doubles = false;

      for (int i = 0; i < (*tot); i++) {
        if (fabsf(isectVCosSS[(i + 1) % *tot][0] - isectVCosSS[i][0]) < PROJ_PIXEL_TOLERANCE &&
            fabsf(isectVCosSS[(i + 1) % *tot][1] - isectVCosSS[i][1]) < PROJ_PIXEL_TOLERANCE)
        {
          for (int j = i; j < (*tot) - 1; j++) {
            isectVCosSS[j][0] = isectVCosSS[j + 1][0];
            isectVCosSS[j][1] = isectVCosSS[j + 1][1];
          }
          /* keep looking for more doubles */
          doubles = true;
          (*tot)--;
        }
      }

      /* its possible there is only a few left after remove doubles */
      if ((*tot) < 3) {
        // printf("removed too many doubles B\n");
        *tot = 0;
        return;
      }
    }

    if (is_ortho) {
      for (int i = 0; i < (*tot); i++) {
        barycentric_weights_v2(v1coSS, v2coSS, v3coSS, isectVCosSS[i], w);
        interp_v2_v2v2v2(bucket_bounds_uv[i], uv1co, uv2co, uv3co, w);
      }
    }
    else {
      for (int i = 0; i < (*tot); i++) {
        barycentric_weights_v2_persp(v1coSS, v2coSS, v3coSS, isectVCosSS[i], w);
        interp_v2_v2v2v2(bucket_bounds_uv[i], uv1co, uv2co, uv3co, w);
      }
    }
  }

#ifdef PROJ_DEBUG_PRINT_CLIP
  /* include this at the bottom of the above function to debug the output */

  {
    /* If there are ever any problems, */
    float test_uv[4][2];
    int i;
    if (is_ortho) {
      rect_to_uvspace_ortho(
          bucket_bounds, v1coSS, v2coSS, v3coSS, uv1co, uv2co, uv3co, test_uv, flip);
    }
    else {
      rect_to_uvspace_persp(
          bucket_bounds, v1coSS, v2coSS, v3coSS, uv1co, uv2co, uv3co, test_uv, flip);
    }
    printf("(  [(%f,%f), (%f,%f), (%f,%f), (%f,%f)], ",
           test_uv[0][0],
           test_uv[0][1],
           test_uv[1][0],
           test_uv[1][1],
           test_uv[2][0],
           test_uv[2][1],
           test_uv[3][0],
           test_uv[3][1]);

    printf("  [(%f,%f), (%f,%f), (%f,%f)], ",
           uv1co[0],
           uv1co[1],
           uv2co[0],
           uv2co[1],
           uv3co[0],
           uv3co[1]);

    printf("[");
    for (int i = 0; i < (*tot); i++) {
      printf("(%f, %f),", bucket_bounds_uv[i][0], bucket_bounds_uv[i][1]);
    }
    printf("]),\\\n");
  }
#endif
}

/**
 * \code{.py}
 * # This script creates faces in a blender scene from printed data above.
 *
 * project_ls = [
 * ...(output from above block)...
 * ]
 *
 * from Blender import Scene, Mesh, Window, sys, Mathutils
 *
 * import bpy
 *
 * V = Mathutils.Vector
 *
 * def main():
 *     scene = bpy.data.scenes.active
 *
 *     for item in project_ls:
 *         bb = item[0]
 *         uv = item[1]
 *         poly = item[2]
 *
 *         me = bpy.data.meshes.new()
 *         ob = scene.objects.new(me)
 *
 *         me.verts.extend([V(bb[0]).xyz, V(bb[1]).xyz, V(bb[2]).xyz, V(bb[3]).xyz])
 *         me.faces.extend([(0,1,2,3),])
 *         me.verts.extend([V(uv[0]).xyz, V(uv[1]).xyz, V(uv[2]).xyz])
 *         me.faces.extend([(4,5,6),])
 *
 *         vs = [V(p).xyz for p in poly]
 *         print len(vs)
 *         l = len(me.verts)
 *         me.verts.extend(vs)
 *
 *         i = l
 *         while i < len(me.verts):
 *             ii = i + 1
 *             if ii == len(me.verts):
 *                 ii = l
 *             me.edges.extend([i, ii])
 *             i += 1
 *
 * if __name__ == '__main__':
 *     main()
 * \endcode
 */

#undef ISECT_1
#undef ISECT_2
#undef ISECT_3
#undef ISECT_4
#undef ISECT_ALL3
#undef ISECT_ALL4

/* checks if pt is inside a convex 2D polyline, the polyline must be ordered rotating clockwise
 * otherwise it would have to test for mixed (line_point_side_v2 > 0.0f) cases */
static bool IsectPoly2Df(const float pt[2], const float uv[][2], const int tot)
{
  int i;
  if (line_point_side_v2(uv[tot - 1], uv[0], pt) < 0.0f) {
    return false;
  }

  for (i = 1; i < tot; i++) {
    if (line_point_side_v2(uv[i - 1], uv[i], pt) < 0.0f) {
      return false;
    }
  }

  return true;
}
static bool IsectPoly2Df_twoside(const float pt[2], const float uv[][2], const int tot)
{
  const bool side = (line_point_side_v2(uv[tot - 1], uv[0], pt) > 0.0f);

  for (int i = 1; i < tot; i++) {
    if ((line_point_side_v2(uv[i - 1], uv[i], pt) > 0.0f) != side) {
      return false;
    }
  }

  return true;
}

/* One of the most important function for projection painting,
 * since it selects the pixels to be added into each bucket.
 *
 * initialize pixels from this face where it intersects with the bucket_index,
 * optionally initialize pixels for removing seams */
static void project_paint_face_init(const ProjPaintState *ps,
                                    const int thread_index,
                                    const int bucket_index,
                                    const int tri_index,
                                    const int image_index,
                                    const rctf *clip_rect,
                                    const rctf *bucket_bounds,
                                    ImBuf *ibuf,
                                    ImBuf **tmpibuf)
{
  /* Projection vars, to get the 3D locations into screen space. */
  MemArena *arena = ps->arena_mt[thread_index];
  LinkNode **bucketPixelNodes = ps->bucketRect + bucket_index;
  LinkNode *bucketFaceNodes = ps->bucketFaces[bucket_index];
  bool threaded = (ps->thread_tot > 1);

  TileInfo tinf = {
      ps->tile_lock,
      ps->do_masking,
      ushort(ED_IMAGE_UNDO_TILE_NUMBER(ibuf->x)),
      tmpibuf,
      ps->projImages + image_index,
  };

  const int3 &tri = ps->corner_tris_eval[tri_index];
  const int face_i = ps->corner_tri_faces_eval[tri_index];
  const int vert_tri[3] = {PS_CORNER_TRI_AS_VERT_INDEX_3(ps, tri)};
  const float *tri_uv[3] = {PS_CORNER_TRI_AS_UV_3(ps->poly_to_loop_uv, face_i, tri)};

  /* UV/pixel seeking data */
  /* Image X/Y-Pixel */
  int x, y;
  float mask;
  /* Image floating point UV - same as x, y but from 0.0-1.0 */
  float uv[2];

  /* vert co screen-space, these will be assigned to vert_tri[0-2] */
  const float *v1coSS, *v2coSS, *v3coSS;

  /* Vertex screen-space coords. */
  const float *vCo[3];

  float w[3], wco[3];

  /* for convenience only, these will be assigned to tri_uv[0],1,2 or tri_uv[0],2,3 */
  float *uv1co, *uv2co, *uv3co;
  float pixelScreenCo[4];
  bool do_3d_mapping = ps->brush->mtex.brush_map_mode == MTEX_MAP_MODE_3D;

  /* Image-space bounds. */
  rcti bounds_px;
  /* Variables for getting UV-space bounds. */

  /* Bucket bounds in UV space so we can init pixels only for this face. */
  float tri_uv_pxoffset[3][2];
  float xhalfpx, yhalfpx;
  const float ibuf_xf = float(ibuf->x), ibuf_yf = float(ibuf->y);

  /* for early loop exit */
  int has_x_isect = 0, has_isect = 0;

  float uv_clip[8][2];
  int uv_clip_tot;
  const bool is_ortho = ps->is_ortho;
  const bool is_flip_object = ps->is_flip_object;
  const bool do_backfacecull = ps->do_backfacecull;
  const bool do_clip = RV3D_CLIPPING_ENABLED(ps->v3d, ps->rv3d);

  vCo[0] = ps->vert_positions_eval[vert_tri[0]];
  vCo[1] = ps->vert_positions_eval[vert_tri[1]];
  vCo[2] = ps->vert_positions_eval[vert_tri[2]];

  /* Use tri_uv_pxoffset instead of tri_uv so we can offset the UV half a pixel
   * this is done so we can avoid offsetting all the pixels by 0.5 which causes
   * problems when wrapping negative coords */
  xhalfpx = (0.5f + (PROJ_PIXEL_TOLERANCE * (1.0f / 3.0f))) / ibuf_xf;
  yhalfpx = (0.5f + (PROJ_PIXEL_TOLERANCE * (1.0f / 4.0f))) / ibuf_yf;

  /* Note about (PROJ_GEOM_TOLERANCE/x) above...
   * Needed to add this offset since UV coords are often quads aligned to pixels.
   * In this case pixels can be exactly between 2 triangles causing nasty
   * artifacts.
   *
   * This workaround can be removed and painting will still work on most cases
   * but since the first thing most people try is painting onto a quad- better make it work.
   */

  tri_uv_pxoffset[0][0] = tri_uv[0][0] - xhalfpx;
  tri_uv_pxoffset[0][1] = tri_uv[0][1] - yhalfpx;

  tri_uv_pxoffset[1][0] = tri_uv[1][0] - xhalfpx;
  tri_uv_pxoffset[1][1] = tri_uv[1][1] - yhalfpx;

  tri_uv_pxoffset[2][0] = tri_uv[2][0] - xhalfpx;
  tri_uv_pxoffset[2][1] = tri_uv[2][1] - yhalfpx;

  {
    uv1co = tri_uv_pxoffset[0]; /* Was `tri_uv[i1];`. */
    uv2co = tri_uv_pxoffset[1]; /* Was `tri_uv[i2];`. */
    uv3co = tri_uv_pxoffset[2]; /* Was `tri_uv[i3];`. */

    v1coSS = ps->screenCoords[vert_tri[0]];
    v2coSS = ps->screenCoords[vert_tri[1]];
    v3coSS = ps->screenCoords[vert_tri[2]];

    /* This function gives is a concave polyline in UV space from the clipped tri. */
    project_bucket_clip_face(is_ortho,
                             is_flip_object,
                             clip_rect,
                             bucket_bounds,
                             v1coSS,
                             v2coSS,
                             v3coSS,
                             uv1co,
                             uv2co,
                             uv3co,
                             uv_clip,
                             &uv_clip_tot,
                             do_backfacecull || ps->do_occlude);

    /* Sometimes this happens, better just allow for 8 intersections
     * even though there should be max 6 */
#if 0
    if (uv_clip_tot > 6) {
      printf("this should never happen! %d\n", uv_clip_tot);
    }
#endif

    if (pixel_bounds_array(uv_clip, ibuf->x, ibuf->y, uv_clip_tot, &bounds_px)) {
#if 0
      project_paint_undo_tiles_init(
          &bounds_px, ps->projImages + image_index, tmpibuf, tile_width, threaded, ps->do_masking);
#endif
      /* clip face and */

      has_isect = 0;
      for (y = bounds_px.ymin; y < bounds_px.ymax; y++) {
        // uv[1] = (float(y) + 0.5f) / float(ibuf->y);
        /* use pixel offset UV coords instead */
        uv[1] = float(y) / ibuf_yf;

        has_x_isect = 0;
        for (x = bounds_px.xmin; x < bounds_px.xmax; x++) {
          // uv[0] = (float(x) + 0.5f) / float(ibuf->x);
          /* use pixel offset UV coords instead */
          uv[0] = float(x) / ibuf_xf;

          /* Note about IsectPoly2Df_twoside, checking the face or uv flipping doesn't work,
           * could check the poly direction but better to do this */
          if ((do_backfacecull == true && IsectPoly2Df(uv, uv_clip, uv_clip_tot)) ||
              (do_backfacecull == false && IsectPoly2Df_twoside(uv, uv_clip, uv_clip_tot)))
          {

            has_x_isect = has_isect = 1;

            if (is_ortho) {
              screen_px_from_ortho(
                  uv, v1coSS, v2coSS, v3coSS, uv1co, uv2co, uv3co, pixelScreenCo, w);
            }
            else {
              screen_px_from_persp(
                  uv, v1coSS, v2coSS, v3coSS, uv1co, uv2co, uv3co, pixelScreenCo, w);
            }

            /* A pity we need to get the world-space pixel location here
             * because it is a relatively expensive operation. */
            if (do_clip || do_3d_mapping) {
              interp_v3_v3v3v3(wco,
                               ps->vert_positions_eval[vert_tri[0]],
                               ps->vert_positions_eval[vert_tri[1]],
                               ps->vert_positions_eval[vert_tri[2]],
                               w);
              if (do_clip && ED_view3d_clipping_test(ps->rv3d, wco, true)) {
                /* Watch out that no code below this needs to run */
                continue;
              }
            }

            /* Is this UV visible from the view? - ray-trace */
            /* project_paint_PickFace is less complex, use for testing */
            // if (project_paint_PickFace(ps, pixelScreenCo, w, &side) == tri_index) {
            if ((ps->do_occlude == false) ||
                !project_bucket_point_occluded(ps, bucketFaceNodes, tri_index, pixelScreenCo))
            {
              mask = project_paint_uvpixel_mask(ps, tri_index, w);

              if (mask > 0.0f) {
                BLI_linklist_prepend_arena(
                    bucketPixelNodes,
                    project_paint_uvpixel_init(
                        ps, arena, &tinf, x, y, mask, tri_index, pixelScreenCo, wco, w),
                    arena);
              }
            }
          }
          // #if 0
          else if (has_x_isect) {
            /* assuming the face is not a bow-tie - we know we can't intersect again on the X */
            break;
          }
          // #endif
        }

#if 0 /* TODO: investigate why this doesn't work sometimes! it should! */
        /* no intersection for this entire row,
         * after some intersection above means we can quit now */
        if (has_x_isect == 0 && has_isect) {
          break;
        }
#endif
      }
    }
  }

#ifndef PROJ_DEBUG_NOSEAMBLEED
  if (ps->seam_bleed_px > 0.0f && !(ps->faceSeamFlags[tri_index] & PROJ_FACE_DEGENERATE)) {
    int face_seam_flag;

    if (threaded) {
      /* Other threads could be modifying these vars. */
      BLI_thread_lock(LOCK_CUSTOM1);
    }

    face_seam_flag = ps->faceSeamFlags[tri_index];

    /* are any of our edges un-initialized? */
    if ((face_seam_flag & PROJ_FACE_SEAM_INIT0) == 0 ||
        (face_seam_flag & PROJ_FACE_SEAM_INIT1) == 0 ||
        (face_seam_flag & PROJ_FACE_SEAM_INIT2) == 0)
    {
      project_face_seams_init(ps, arena, tri_index, 0, true, ibuf->x, ibuf->y);
      face_seam_flag = ps->faceSeamFlags[tri_index];
#  if 0
      printf("seams - %d %d %d %d\n",
             flag & PROJ_FACE_SEAM0,
             flag & PROJ_FACE_SEAM1,
             flag & PROJ_FACE_SEAM2);
#  endif
    }

    if ((face_seam_flag & (PROJ_FACE_SEAM0 | PROJ_FACE_SEAM1 | PROJ_FACE_SEAM2)) == 0) {

      if (threaded) {
        /* Other threads could be modifying these vars. */
        BLI_thread_unlock(LOCK_CUSTOM1);
      }
    }
    else {
      /* we have a seam - deal with it! */

      /* Inset face coords.
       * - screen-space in orthographic view.
       * - world-space in perspective view.
       */
      float insetCos[3][3];

      /* Vertex screen-space coords. */
      const float *vCoSS[3];

      /* Store the screen-space coords of the face,
       * clipped by the bucket's screen aligned rectangle. */
      float bucket_clip_edges[2][2];
      float edge_verts_inset_clip[2][3];
      /* face edge pairs - loop through these:
       * ((0,1), (1,2), (2,3), (3,0)) or ((0,1), (1,2), (2,0)) for a tri */
      int fidx1, fidx2;

      float seam_subsection[4][2];
      float fac1, fac2;

      /* Pixel-space UVs. */
      float tri_puv[3][2];

      tri_puv[0][0] = tri_uv_pxoffset[0][0] * ibuf->x;
      tri_puv[0][1] = tri_uv_pxoffset[0][1] * ibuf->y;

      tri_puv[1][0] = tri_uv_pxoffset[1][0] * ibuf->x;
      tri_puv[1][1] = tri_uv_pxoffset[1][1] * ibuf->y;

      tri_puv[2][0] = tri_uv_pxoffset[2][0] * ibuf->x;
      tri_puv[2][1] = tri_uv_pxoffset[2][1] * ibuf->y;

      if ((ps->faceSeamFlags[tri_index] & PROJ_FACE_SEAM0) ||
          (ps->faceSeamFlags[tri_index] & PROJ_FACE_SEAM1) ||
          (ps->faceSeamFlags[tri_index] & PROJ_FACE_SEAM2))
      {
        uv_image_outset(ps, tri_uv_pxoffset, tri_puv, tri_index, ibuf->x, ibuf->y);
      }

      /* ps->loopSeamUVs can't be modified when threading, now this is done we can unlock. */
      if (threaded) {
        /* Other threads could be modifying these vars */
        BLI_thread_unlock(LOCK_CUSTOM1);
      }

      vCoSS[0] = ps->screenCoords[vert_tri[0]];
      vCoSS[1] = ps->screenCoords[vert_tri[1]];
      vCoSS[2] = ps->screenCoords[vert_tri[2]];

      /* PROJ_FACE_SCALE_SEAM must be slightly less than 1.0f */
      if (is_ortho) {
        scale_tri(insetCos, vCoSS, PROJ_FACE_SCALE_SEAM);
      }
      else {
        scale_tri(insetCos, vCo, PROJ_FACE_SCALE_SEAM);
      }

      for (fidx1 = 0; fidx1 < 3; fidx1++) {
        /* next fidx in the face (0,1,2) -> (1,2,0) */
        fidx2 = (fidx1 == 2) ? 0 : fidx1 + 1;

        if ((face_seam_flag & (1 << fidx1)) && /* 1<<fidx1 -> PROJ_FACE_SEAM# */
            line_clip_rect2f(clip_rect,
                             bucket_bounds,
                             vCoSS[fidx1],
                             vCoSS[fidx2],
                             bucket_clip_edges[0],
                             bucket_clip_edges[1]))
        {
          /* Avoid div by zero. */
          if (len_squared_v2v2(vCoSS[fidx1], vCoSS[fidx2]) > FLT_EPSILON) {
            uint loop_idx = ps->corner_tris_eval[tri_index][fidx1];
            LoopSeamData *seam_data = &ps->loopSeamData[loop_idx];
            float (*seam_uvs)[2] = seam_data->seam_uvs;

            if (is_ortho) {
              fac1 = line_point_factor_v2(bucket_clip_edges[0], vCoSS[fidx1], vCoSS[fidx2]);
              fac2 = line_point_factor_v2(bucket_clip_edges[1], vCoSS[fidx1], vCoSS[fidx2]);
            }
            else {
              fac1 = screen_px_line_point_factor_v2_persp(
                  ps, bucket_clip_edges[0], vCo[fidx1], vCo[fidx2]);
              fac2 = screen_px_line_point_factor_v2_persp(
                  ps, bucket_clip_edges[1], vCo[fidx1], vCo[fidx2]);
            }

            interp_v2_v2v2(
                seam_subsection[0], tri_uv_pxoffset[fidx1], tri_uv_pxoffset[fidx2], fac1);
            interp_v2_v2v2(
                seam_subsection[1], tri_uv_pxoffset[fidx1], tri_uv_pxoffset[fidx2], fac2);

            interp_v2_v2v2(seam_subsection[2], seam_uvs[0], seam_uvs[1], fac2);
            interp_v2_v2v2(seam_subsection[3], seam_uvs[0], seam_uvs[1], fac1);

            /* if the bucket_clip_edges values Z values was kept we could avoid this
             * Inset needs to be added so occlusion tests won't hit adjacent faces */
            interp_v3_v3v3(edge_verts_inset_clip[0], insetCos[fidx1], insetCos[fidx2], fac1);
            interp_v3_v3v3(edge_verts_inset_clip[1], insetCos[fidx1], insetCos[fidx2], fac2);

            if (pixel_bounds_uv(seam_subsection, ibuf->x, ibuf->y, &bounds_px)) {
              /* bounds between the seam rect and the uvspace bucket pixels */

              has_isect = 0;
              for (y = bounds_px.ymin; y < bounds_px.ymax; y++) {
                // uv[1] = (float(y) + 0.5f) / float(ibuf->y);
                /* use offset uvs instead */
                uv[1] = float(y) / ibuf_yf;

                has_x_isect = 0;
                for (x = bounds_px.xmin; x < bounds_px.xmax; x++) {
                  const float puv[2] = {float(x), float(y)};
                  bool in_bounds;
                  // uv[0] = (float(x) + 0.5f) / float(ibuf->x);
                  /* use offset uvs instead */
                  uv[0] = float(x) / ibuf_xf;

                  /* test we're inside uvspace bucket and triangle bounds */
                  if (equals_v2v2(seam_uvs[0], seam_uvs[1])) {
                    in_bounds = isect_point_tri_v2(uv, UNPACK3(seam_subsection));
                  }
                  else {
                    in_bounds = isect_point_quad_v2(uv, UNPACK4(seam_subsection));
                  }

                  if (in_bounds) {
                    if ((seam_data->corner_dist_sq[0] > 0.0f) &&
                        (len_squared_v2v2(puv, seam_data->seam_puvs[0]) <
                         seam_data->corner_dist_sq[0]) &&
                        (len_squared_v2v2(puv, tri_puv[fidx1]) > ps->seam_bleed_px_sq))
                    {
                      in_bounds = false;
                    }
                    else if ((seam_data->corner_dist_sq[1] > 0.0f) &&
                             (len_squared_v2v2(puv, seam_data->seam_puvs[1]) <
                              seam_data->corner_dist_sq[1]) &&
                             (len_squared_v2v2(puv, tri_puv[fidx2]) > ps->seam_bleed_px_sq))
                    {
                      in_bounds = false;
                    }
                  }

                  if (in_bounds) {
                    float pixel_on_edge[4];
                    float fac;

                    if (is_ortho) {
                      screen_px_from_ortho(
                          uv, v1coSS, v2coSS, v3coSS, uv1co, uv2co, uv3co, pixelScreenCo, w);
                    }
                    else {
                      screen_px_from_persp(
                          uv, v1coSS, v2coSS, v3coSS, uv1co, uv2co, uv3co, pixelScreenCo, w);
                    }

                    /* We need the coord of the pixel on the edge, for the occlusion query. */
                    fac = resolve_quad_u_v2(uv, UNPACK4(seam_subsection));
                    interp_v3_v3v3(
                        pixel_on_edge, edge_verts_inset_clip[0], edge_verts_inset_clip[1], fac);

                    if (!is_ortho) {
                      pixel_on_edge[3] = 1.0f;
                      /* cast because of const */
                      mul_m4_v4((float (*)[4])ps->projectMat, pixel_on_edge);
                      pixel_on_edge[0] = float(ps->winx * 0.5f) +
                                         (ps->winx * 0.5f) * pixel_on_edge[0] / pixel_on_edge[3];
                      pixel_on_edge[1] = float(ps->winy * 0.5f) +
                                         (ps->winy * 0.5f) * pixel_on_edge[1] / pixel_on_edge[3];
                      /* Use the depth for bucket point occlusion */
                      pixel_on_edge[2] = pixel_on_edge[2] / pixel_on_edge[3];
                    }

                    if ((ps->do_occlude == false) ||
                        !project_bucket_point_occluded(
                            ps, bucketFaceNodes, tri_index, pixel_on_edge))
                    {
                      /* A pity we need to get the world-space pixel location here
                       * because it is a relatively expensive operation. */
                      if (do_clip || do_3d_mapping) {
                        interp_v3_v3v3v3(wco, vCo[0], vCo[1], vCo[2], w);

                        if (do_clip && ED_view3d_clipping_test(ps->rv3d, wco, true)) {
                          /* Watch out that no code below
                           * this needs to run */
                          continue;
                        }
                      }

                      mask = project_paint_uvpixel_mask(ps, tri_index, w);

                      if (mask > 0.0f) {
                        BLI_linklist_prepend_arena(
                            bucketPixelNodes,
                            project_paint_uvpixel_init(
                                ps, arena, &tinf, x, y, mask, tri_index, pixelScreenCo, wco, w),
                            arena);
                      }
                    }
                  }
                  else if (has_x_isect) {
                    /* assuming the face is not a bow-tie - we know
                     * we can't intersect again on the X */
                    break;
                  }
                }

#  if 0 /* TODO: investigate why this doesn't work sometimes! it should! */
                /* no intersection for this entire row,
                 * after some intersection above means we can quit now */
                if (has_x_isect == 0 && has_isect) {
                  break;
                }
#  endif
              }
            }
          }
        }
      }
    }
  }
#else
  UNUSED_VARS(vCo, threaded);
#endif /* PROJ_DEBUG_NOSEAMBLEED */
}

/**
 * Takes floating point screen-space min/max and
 * returns int min/max to be used as indices for ps->bucketRect, ps->bucketFlags
 */
static void project_paint_bucket_bounds(const ProjPaintState *ps,
                                        const float min[2],
                                        const float max[2],
                                        int bucketMin[2],
                                        int bucketMax[2])
{
  /* divide by bucketWidth & bucketHeight so the bounds are offset in bucket grid units */

  /* XXX(jwilkins ): the offset of 0.5 is always truncated to zero and the offset of 1.5f
   * is always truncated to 1, is this really correct? */

  /* these offsets of 0.5 and 1.5 seem odd but they are correct */
  bucketMin[0] = int(int((float(min[0] - ps->screenMin[0]) / ps->screen_width) * ps->buckets_x) +
                     0.5f);
  bucketMin[1] = int(int((float(min[1] - ps->screenMin[1]) / ps->screen_height) * ps->buckets_y) +
                     0.5f);

  bucketMax[0] = int(int((float(max[0] - ps->screenMin[0]) / ps->screen_width) * ps->buckets_x) +
                     1.5f);
  bucketMax[1] = int(int((float(max[1] - ps->screenMin[1]) / ps->screen_height) * ps->buckets_y) +
                     1.5f);

  /* in case the rect is outside the mesh 2d bounds */
  CLAMP(bucketMin[0], 0, ps->buckets_x);
  CLAMP(bucketMin[1], 0, ps->buckets_y);

  CLAMP(bucketMax[0], 0, ps->buckets_x);
  CLAMP(bucketMax[1], 0, ps->buckets_y);
}

/* set bucket_bounds to a screen space-aligned floating point bound-box */
static void project_bucket_bounds(const ProjPaintState *ps,
                                  const int bucket_x,
                                  const int bucket_y,
                                  rctf *r_bucket_bounds)
{
  /* left */
  r_bucket_bounds->xmin = (ps->screenMin[0] + ((bucket_x) * (ps->screen_width / ps->buckets_x)));
  /* right */
  r_bucket_bounds->xmax = (ps->screenMin[0] +
                           ((bucket_x + 1) * (ps->screen_width / ps->buckets_x)));

  /* bottom */
  r_bucket_bounds->ymin = (ps->screenMin[1] + ((bucket_y) * (ps->screen_height / ps->buckets_y)));
  /* top */
  r_bucket_bounds->ymax = (ps->screenMin[1] +
                           ((bucket_y + 1) * (ps->screen_height / ps->buckets_y)));
}

/* Fill this bucket with pixels from the faces that intersect it.
 *
 * have bucket_bounds as an argument so we don't need to give bucket_x/y the rect function needs */
static void project_bucket_init(const ProjPaintState *ps,
                                const int thread_index,
                                const int bucket_index,
                                const rctf *clip_rect,
                                const rctf *bucket_bounds)
{
  LinkNode *node;
  int tri_index, image_index = 0;
  ImBuf *ibuf = nullptr;
  Image *tpage_last = nullptr, *tpage;
  ImBuf *tmpibuf = nullptr;
  int tile_last = 0;

  if (ps->image_tot == 1) {
    /* Simple loop, no context switching */
    ibuf = ps->projImages[0].ibuf;

    for (node = ps->bucketFaces[bucket_index]; node; node = node->next) {
      project_paint_face_init(ps,
                              thread_index,
                              bucket_index,
                              POINTER_AS_INT(node->link),
                              0,
                              clip_rect,
                              bucket_bounds,
                              ibuf,
                              &tmpibuf);
    }
  }
  else {

    /* More complicated loop, switch between images */
    for (node = ps->bucketFaces[bucket_index]; node; node = node->next) {
      tri_index = POINTER_AS_INT(node->link);

      const int3 &tri = ps->corner_tris_eval[tri_index];
      const int face_i = ps->corner_tri_faces_eval[tri_index];
      const float *tri_uv[3] = {PS_CORNER_TRI_AS_UV_3(ps->poly_to_loop_uv, face_i, tri)};

      /* Image context switching */
      tpage = project_paint_face_paint_image(ps, tri_index);
      int tile = project_paint_face_paint_tile(tpage, tri_uv[0]);
      if (tpage_last != tpage || tile_last != tile) {
        tpage_last = tpage;
        tile_last = tile;

        ibuf = nullptr;
        for (image_index = 0; image_index < ps->image_tot; image_index++) {
          ProjPaintImage *projIma = &ps->projImages[image_index];
          if ((projIma->ima == tpage) && (projIma->iuser.tile == tile)) {
            ibuf = projIma->ibuf;
            break;
          }
        }
        BLI_assert(ibuf != nullptr);
      }
      /* context switching done */

      project_paint_face_init(ps,
                              thread_index,
                              bucket_index,
                              tri_index,
                              image_index,
                              clip_rect,
                              bucket_bounds,
                              ibuf,
                              &tmpibuf);
    }
  }

  if (tmpibuf) {
    IMB_freeImBuf(tmpibuf);
  }

  ps->bucketFlags[bucket_index] |= PROJ_BUCKET_INIT;
}

/* We want to know if a bucket and a face overlap in screen-space.
 *
 * NOTE: if this ever returns false positives its not that bad, since a face in the bounding area
 * will have its pixels calculated when it might not be needed later, (at the moment at least)
 * obviously it shouldn't have bugs though. */

static bool project_bucket_face_isect(ProjPaintState *ps,
                                      int bucket_x,
                                      int bucket_y,
                                      const int3 &tri)
{
  /* TODO: replace this with a trickier method that uses side-of-line for all
   * #ProjPaintState.screenCoords edges against the closest bucket corner. */
  const int vert_tri[3] = {PS_CORNER_TRI_AS_VERT_INDEX_3(ps, tri)};
  rctf bucket_bounds;
  float p1[2], p2[2], p3[2], p4[2];
  const float *v, *v1, *v2, *v3;
  int fidx;

  project_bucket_bounds(ps, bucket_x, bucket_y, &bucket_bounds);

  /* Is one of the faces verts in the bucket bounds? */

  fidx = 2;
  do {
    v = ps->screenCoords[vert_tri[fidx]];
    if (BLI_rctf_isect_pt_v(&bucket_bounds, v)) {
      return true;
    }
  } while (fidx--);

  v1 = ps->screenCoords[vert_tri[0]];
  v2 = ps->screenCoords[vert_tri[1]];
  v3 = ps->screenCoords[vert_tri[2]];

  p1[0] = bucket_bounds.xmin;
  p1[1] = bucket_bounds.ymin;
  p2[0] = bucket_bounds.xmin;
  p2[1] = bucket_bounds.ymax;
  p3[0] = bucket_bounds.xmax;
  p3[1] = bucket_bounds.ymax;
  p4[0] = bucket_bounds.xmax;
  p4[1] = bucket_bounds.ymin;

  if (isect_point_tri_v2(p1, v1, v2, v3) || isect_point_tri_v2(p2, v1, v2, v3) ||
      isect_point_tri_v2(p3, v1, v2, v3) || isect_point_tri_v2(p4, v1, v2, v3) ||
      /* we can avoid testing v3,v1 because another intersection MUST exist if this intersects */
      (isect_seg_seg_v2(p1, p2, v1, v2) || isect_seg_seg_v2(p1, p2, v2, v3)) ||
      (isect_seg_seg_v2(p2, p3, v1, v2) || isect_seg_seg_v2(p2, p3, v2, v3)) ||
      (isect_seg_seg_v2(p3, p4, v1, v2) || isect_seg_seg_v2(p3, p4, v2, v3)) ||
      (isect_seg_seg_v2(p4, p1, v1, v2) || isect_seg_seg_v2(p4, p1, v2, v3)))
  {
    return true;
  }

  return false;
}

/* Add faces to the bucket but don't initialize its pixels
 * TODO: when painting occluded, sort the faces on their min-Z
 * and only add faces that faces that are not occluded */
static void project_paint_delayed_face_init(ProjPaintState *ps,
                                            const int3 &corner_tri,
                                            const int tri_index)
{
  const int vert_tri[3] = {PS_CORNER_TRI_AS_VERT_INDEX_3(ps, corner_tri)};
  float min[2], max[2], *vCoSS;
  /* for ps->bucketRect indexing */
  int bucketMin[2], bucketMax[2];
  int fidx, bucket_x, bucket_y;
  /* for early loop exit */
  int has_x_isect = -1, has_isect = 0;
  /* just use the first thread arena since threading has not started yet */
  MemArena *arena = ps->arena_mt[0];

  INIT_MINMAX2(min, max);

  fidx = 2;
  do {
    vCoSS = ps->screenCoords[vert_tri[fidx]];
    minmax_v2v2_v2(min, max, vCoSS);
  } while (fidx--);

  project_paint_bucket_bounds(ps, min, max, bucketMin, bucketMax);

  for (bucket_y = bucketMin[1]; bucket_y < bucketMax[1]; bucket_y++) {
    has_x_isect = 0;
    for (bucket_x = bucketMin[0]; bucket_x < bucketMax[0]; bucket_x++) {
      if (project_bucket_face_isect(ps, bucket_x, bucket_y, corner_tri)) {
        int bucket_index = bucket_x + (bucket_y * ps->buckets_x);
        BLI_linklist_prepend_arena(&ps->bucketFaces[bucket_index],
                                   /* cast to a pointer to shut up the compiler */
                                   POINTER_FROM_INT(tri_index),
                                   arena);

        has_x_isect = has_isect = 1;
      }
      else if (has_x_isect) {
        /* assuming the face is not a bow-tie - we know we can't intersect again on the X */
        break;
      }
    }

    /* no intersection for this entire row,
     * after some intersection above means we can quit now */
    if (has_x_isect == 0 && has_isect) {
      break;
    }
  }

#ifndef PROJ_DEBUG_NOSEAMBLEED
  if (ps->seam_bleed_px > 0.0f) {
    /* set as uninitialized */
    ps->loopSeamData[corner_tri[0]].seam_uvs[0][0] = FLT_MAX;
    ps->loopSeamData[corner_tri[1]].seam_uvs[0][0] = FLT_MAX;
    ps->loopSeamData[corner_tri[2]].seam_uvs[0][0] = FLT_MAX;
  }
#endif
}

static void proj_paint_state_viewport_init(ProjPaintState *ps, const char symmetry_flag)
{
  float mat[3][3];
  float viewmat[4][4];
  float viewinv[4][4];

  ps->viewDir[0] = 0.0f;
  ps->viewDir[1] = 0.0f;
  ps->viewDir[2] = 1.0f;

  copy_m4_m4(ps->obmat, ps->ob->object_to_world().ptr());

  if (symmetry_flag) {
    int i;
    for (i = 0; i < 3; i++) {
      if ((symmetry_flag >> i) & 1) {
        negate_v3(ps->obmat[i]);
        ps->is_flip_object = !ps->is_flip_object;
      }
    }
  }

  invert_m4_m4(ps->obmat_imat, ps->obmat);

  if (ELEM(ps->source, PROJ_SRC_VIEW, PROJ_SRC_VIEW_FILL)) {
    /* normal drawing */
    ps->winx = ps->region->winx;
    ps->winy = ps->region->winy;

    copy_m4_m4(viewmat, ps->rv3d->viewmat);
    copy_m4_m4(viewinv, ps->rv3d->viewinv);

    blender::float4x4 projection = ED_view3d_ob_project_mat_get_from_obmat(
        ps->rv3d, blender::float4x4(ps->obmat));
    copy_m4_m4(ps->projectMat, projection.ptr());

    ps->is_ortho = ED_view3d_clip_range_get(
        ps->depsgraph, ps->v3d, ps->rv3d, true, &ps->clip_start, &ps->clip_end);
  }
  else {
    /* re-projection */
    float winmat[4][4];
    float vmat[4][4];

    ps->winx = ps->reproject_ibuf->x;
    ps->winy = ps->reproject_ibuf->y;

    if (ps->source == PROJ_SRC_IMAGE_VIEW) {
      /* image stores camera data, tricky */
      IDProperty *idgroup = IDP_GetProperties(&ps->reproject_image->id);
      IDProperty *view_data = IDP_GetPropertyFromGroup(idgroup, PROJ_VIEW_DATA_ID);

      const float *array = IDP_array_float_get(view_data);

      /* use image array, written when creating image */
      memcpy(winmat, array, sizeof(winmat));
      array += sizeof(winmat) / sizeof(float);
      memcpy(viewmat, array, sizeof(viewmat));
      array += sizeof(viewmat) / sizeof(float);
      ps->clip_start = array[0];
      ps->clip_end = array[1];
      ps->is_ortho = bool(array[2]);

      invert_m4_m4(viewinv, viewmat);
    }
    else if (ps->source == PROJ_SRC_IMAGE_CAM) {
      Object *cam_ob_eval = DEG_get_evaluated(ps->depsgraph, ps->scene->camera);
      CameraParams params;

      /* viewmat & viewinv */
      copy_m4_m4(viewinv, cam_ob_eval->object_to_world().ptr());
      normalize_m4(viewinv);
      invert_m4_m4(viewmat, viewinv);

      /* window matrix, clipping and ortho */
      BKE_camera_params_init(&params);
      BKE_camera_params_from_object(&params, cam_ob_eval);
      BKE_camera_params_compute_viewplane(&params, ps->winx, ps->winy, 1.0f, 1.0f);
      BKE_camera_params_compute_matrix(&params);

      copy_m4_m4(winmat, params.winmat);
      ps->clip_start = params.clip_start;
      ps->clip_end = params.clip_end;
      ps->is_ortho = params.is_ortho;
    }
    else {
      BLI_assert(0);
    }

    /* same as #ED_view3d_ob_project_mat_get */
    mul_m4_m4m4(vmat, viewmat, ps->obmat);
    mul_m4_m4m4(ps->projectMat, winmat, vmat);
  }

  invert_m4_m4(ps->projectMatInv, ps->projectMat);

  /* viewDir - object relative */
  copy_m3_m4(mat, viewinv);
  mul_m3_v3(mat, ps->viewDir);
  copy_m3_m4(mat, ps->obmat_imat);
  mul_m3_v3(mat, ps->viewDir);
  normalize_v3(ps->viewDir);

  if (UNLIKELY(ps->is_flip_object)) {
    negate_v3(ps->viewDir);
  }

  /* viewPos - object relative */
  copy_v3_v3(ps->viewPos, viewinv[3]);
  copy_m3_m4(mat, ps->obmat_imat);
  mul_m3_v3(mat, ps->viewPos);
  add_v3_v3(ps->viewPos, ps->obmat_imat[3]);
}

static void proj_paint_state_screen_coords_init(ProjPaintState *ps, const int diameter)
{
  float *projScreenCo;
  float projMargin;
  int a;

  INIT_MINMAX2(ps->screenMin, ps->screenMax);

  ps->screenCoords = static_cast<float (*)[4]>(
      MEM_mallocN(sizeof(float) * ps->totvert_eval * 4, "ProjectPaint ScreenVerts"));
  projScreenCo = *ps->screenCoords;

  if (ps->is_ortho) {
    for (a = 0; a < ps->totvert_eval; a++, projScreenCo += 4) {
      mul_v3_m4v3(projScreenCo, ps->projectMat, ps->vert_positions_eval[a]);

      /* screen space, not clamped */
      projScreenCo[0] = float(ps->winx * 0.5f) + (ps->winx * 0.5f) * projScreenCo[0];
      projScreenCo[1] = float(ps->winy * 0.5f) + (ps->winy * 0.5f) * projScreenCo[1];
      minmax_v2v2_v2(ps->screenMin, ps->screenMax, projScreenCo);
    }
  }
  else {
    for (a = 0; a < ps->totvert_eval; a++, projScreenCo += 4) {
      copy_v3_v3(projScreenCo, ps->vert_positions_eval[a]);
      projScreenCo[3] = 1.0f;

      mul_m4_v4(ps->projectMat, projScreenCo);

      if (projScreenCo[3] > ps->clip_start) {
        /* screen space, not clamped */
        projScreenCo[0] = float(ps->winx * 0.5f) +
                          (ps->winx * 0.5f) * projScreenCo[0] / projScreenCo[3];
        projScreenCo[1] = float(ps->winy * 0.5f) +
                          (ps->winy * 0.5f) * projScreenCo[1] / projScreenCo[3];
        /* Use the depth for bucket point occlusion */
        projScreenCo[2] = projScreenCo[2] / projScreenCo[3];
        minmax_v2v2_v2(ps->screenMin, ps->screenMax, projScreenCo);
      }
      else {
        /* TODO: deal with cases where 1 side of a face goes behind the view ?
         *
         * After some research this is actually very tricky, only option is to
         * clip the derived mesh before painting, which is a Pain */
        projScreenCo[0] = FLT_MAX;
      }
    }
  }

  /* If this border is not added we get artifacts for faces that
   * have a parallel edge and at the bounds of the 2D projected verts eg
   * - a single screen aligned quad */
  projMargin = (ps->screenMax[0] - ps->screenMin[0]) * 0.000001f;
  ps->screenMax[0] += projMargin;
  ps->screenMin[0] -= projMargin;
  projMargin = (ps->screenMax[1] - ps->screenMin[1]) * 0.000001f;
  ps->screenMax[1] += projMargin;
  ps->screenMin[1] -= projMargin;

  if (ps->source == PROJ_SRC_VIEW) {
#ifdef PROJ_DEBUG_WINCLIP
    CLAMP(ps->screenMin[0], float(-diameter), float(ps->winx + diameter));
    CLAMP(ps->screenMax[0], float(-diameter), float(ps->winx + diameter));

    CLAMP(ps->screenMin[1], float(-diameter), float(ps->winy + diameter));
    CLAMP(ps->screenMax[1], float(-diameter), float(ps->winy + diameter));
#else
    UNUSED_VARS(diameter);
#endif
  }
  else if (ps->source != PROJ_SRC_VIEW_FILL) { /* re-projection, use bounds */
    ps->screenMin[0] = 0;
    ps->screenMax[0] = float(ps->winx);

    ps->screenMin[1] = 0;
    ps->screenMax[1] = float(ps->winy);
  }
}

static void proj_paint_state_cavity_init(ProjPaintState *ps)
{
  float *cavities;
  int a;

  if (ps->do_mask_cavity) {
    int *counter = MEM_calloc_arrayN<int>(ps->totvert_eval, "counter");
    float (*edges)[3] = static_cast<float (*)[3]>(
        MEM_callocN(sizeof(float[3]) * ps->totvert_eval, "edges"));
    ps->cavities = MEM_malloc_arrayN<float>(ps->totvert_eval, "ProjectPaint Cavities");
    cavities = ps->cavities;

    for (const int64_t i : ps->edges_eval.index_range()) {
      const blender::int2 &edge = ps->edges_eval[i];
      float e[3];
      sub_v3_v3v3(e, ps->vert_positions_eval[edge[0]], ps->vert_positions_eval[edge[1]]);
      normalize_v3(e);
      add_v3_v3(edges[edge[1]], e);
      counter[edge[1]]++;
      sub_v3_v3(edges[edge[0]], e);
      counter[edge[0]]++;
    }
    for (a = 0; a < ps->totvert_eval; a++) {
      if (counter[a] > 0) {
        mul_v3_fl(edges[a], 1.0f / counter[a]);
        /* Augment the difference. */
        cavities[a] = safe_acosf(10.0f * dot_v3v3(ps->vert_normals[a], edges[a])) * float(M_1_PI);
      }
      else {
        cavities[a] = 0.0;
      }
    }

    MEM_freeN(counter);
    MEM_freeN(edges);
  }
}

#ifndef PROJ_DEBUG_NOSEAMBLEED
static void proj_paint_state_seam_bleed_init(ProjPaintState *ps)
{
  if (ps->seam_bleed_px > 0.0f) {
    ps->vertFaces = MEM_calloc_arrayN<LinkNode *>(ps->totvert_eval, "paint-vertFaces");
    ps->faceSeamFlags = MEM_calloc_arrayN<ushort>(ps->corner_tris_eval.size(), __func__);
    ps->faceWindingFlags = MEM_calloc_arrayN<char>(ps->corner_tris_eval.size(), __func__);
    ps->loopSeamData = MEM_malloc_arrayN<LoopSeamData>(ps->totloop_eval, "paint-loopSeamUVs");
    ps->vertSeams = MEM_calloc_arrayN<ListBase>(ps->totvert_eval, "paint-vertSeams");
  }
}
#endif

static void proj_paint_state_thread_init(ProjPaintState *ps, const bool reset_threads)
{
  /* Thread stuff
   *
   * very small brushes run a lot slower multi-threaded since the advantage with
   * threads is being able to fill in multiple buckets at once.
   * Only use threads for bigger brushes. */

  ps->thread_tot = BKE_scene_num_threads(ps->scene);

  /* workaround for #35057, disable threading if diameter is less than is possible for
   * optimum bucket number generation */
  if (reset_threads) {
    ps->thread_tot = 1;
  }

  if (ps->is_shared_user == false) {
    if (ps->thread_tot > 1) {
      ps->tile_lock = MEM_mallocN<SpinLock>("projpaint_tile_lock");
      BLI_spin_init(ps->tile_lock);
    }

    ED_image_paint_tile_lock_init();
  }

  for (int a = 0; a < ps->thread_tot; a++) {
    ps->arena_mt[a] = BLI_memarena_new(MEM_SIZE_OPTIMAL(1 << 16), "project paint arena");
  }
}

static void proj_paint_state_vert_flags_init(ProjPaintState *ps)
{
  if (ps->do_backfacecull && ps->do_mask_normal) {
    float viewDirPersp[3];
    float no[3];
    int a;

    ps->vertFlags = MEM_calloc_arrayN<char>(ps->totvert_eval, "paint-vertFlags");

    for (a = 0; a < ps->totvert_eval; a++) {
      copy_v3_v3(no, ps->vert_normals[a]);
      if (UNLIKELY(ps->is_flip_object)) {
        negate_v3(no);
      }

      if (ps->is_ortho) {
        if (dot_v3v3(ps->viewDir, no) <= ps->normal_angle__cos) {
          /* 1 vert of this face is towards us */
          ps->vertFlags[a] |= PROJ_VERT_CULL;
        }
      }
      else {
        sub_v3_v3v3(viewDirPersp, ps->viewPos, ps->vert_positions_eval[a]);
        normalize_v3(viewDirPersp);
        if (UNLIKELY(ps->is_flip_object)) {
          negate_v3(viewDirPersp);
        }
        if (dot_v3v3(viewDirPersp, no) <= ps->normal_angle__cos) {
          /* 1 vert of this face is towards us */
          ps->vertFlags[a] |= PROJ_VERT_CULL;
        }
      }
    }
  }
  else {
    ps->vertFlags = nullptr;
  }
}

#ifndef PROJ_DEBUG_NOSEAMBLEED
static void project_paint_bleed_add_face_user(const ProjPaintState *ps,
                                              MemArena *arena,
                                              const int3 &corner_tri,
                                              const int tri_index)
{
  /* add face user if we have bleed enabled, set the UV seam flags later */
  /* annoying but we need to add all faces even ones we never use elsewhere */
  if (ps->seam_bleed_px > 0.0f) {
    const int face_i = ps->corner_tri_faces_eval[tri_index];
    const float *tri_uv[3] = {PS_CORNER_TRI_AS_UV_3(ps->poly_to_loop_uv, face_i, corner_tri)};

    /* Check for degenerate triangles. Degenerate faces cause trouble with bleed computations.
     * Ideally this would be checked later, not to add to the cost of computing non-degenerate
     * triangles, but that would allow other triangles to still find adjacent seams on degenerate
     * triangles, potentially causing incorrect results. */
    if (area_tri_v2(UNPACK3(tri_uv)) > 0.0f) {
      const int vert_tri[3] = {PS_CORNER_TRI_AS_VERT_INDEX_3(ps, corner_tri)};
      void *tri_index_p = POINTER_FROM_INT(tri_index);

      BLI_linklist_prepend_arena(&ps->vertFaces[vert_tri[0]], tri_index_p, arena);
      BLI_linklist_prepend_arena(&ps->vertFaces[vert_tri[1]], tri_index_p, arena);
      BLI_linklist_prepend_arena(&ps->vertFaces[vert_tri[2]], tri_index_p, arena);
    }
    else {
      ps->faceSeamFlags[tri_index] |= PROJ_FACE_DEGENERATE;
    }
  }
}
#endif

/* Return true if evaluated mesh can be painted on, false otherwise */
static bool proj_paint_state_mesh_eval_init(const bContext *C, ProjPaintState *ps)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Object *ob = ps->ob;

  const Object *ob_eval = DEG_get_evaluated(depsgraph, ob);
  ps->mesh_eval = BKE_object_get_evaluated_mesh(ob_eval);
  if (!ps->mesh_eval) {
    return false;
  }

  if (ps->mesh_eval->uv_map_names().is_empty()) {
    ps->mesh_eval = nullptr;
    return false;
  }

  /* Build final material array, we use this a lot here. */
  /* materials start from 1, default material is 0 */
  const int totmat = ob->totcol + 1;
  ps->mat_array = static_cast<Material **>(
      MEM_malloc_arrayN(totmat, sizeof(*ps->mat_array), __func__));
  /* We leave last material as empty - rationale here is being able to index
   * the materials by using the mf->mat_nr directly and leaving the last
   * material as nullptr in case no materials exist on mesh, so indexing will not fail. */
  for (int i = 0; i < totmat - 1; i++) {
    ps->mat_array[i] = BKE_object_material_get(ob, i + 1);
  }
  ps->mat_array[totmat - 1] = nullptr;

  ps->vert_positions_eval = ps->mesh_eval->vert_positions();
  ps->vert_normals = ps->mesh_eval->vert_normals();
  ps->edges_eval = ps->mesh_eval->edges();
  ps->faces_eval = ps->mesh_eval->faces();
  ps->corner_verts_eval = ps->mesh_eval->corner_verts();
  ps->select_poly_eval = (const bool *)CustomData_get_layer_named(
      &ps->mesh_eval->face_data, CD_PROP_BOOL, ".select_poly");
  ps->hide_poly_eval = (const bool *)CustomData_get_layer_named(
      &ps->mesh_eval->face_data, CD_PROP_BOOL, ".hide_poly");
  ps->material_indices = (const int *)CustomData_get_layer_named(
      &ps->mesh_eval->face_data, CD_PROP_INT32, "material_index");
  ps->sharp_faces_eval = static_cast<const bool *>(
      CustomData_get_layer_named(&ps->mesh_eval->face_data, CD_PROP_BOOL, "sharp_face"));

  ps->totvert_eval = ps->mesh_eval->verts_num;
  ps->faces_num_eval = ps->mesh_eval->faces_num;
  ps->totloop_eval = ps->mesh_eval->corners_num;

  ps->corner_tris_eval = ps->mesh_eval->corner_tris();
  ps->corner_tri_faces_eval = ps->mesh_eval->corner_tri_faces();

  ps->poly_to_loop_uv = static_cast<const float (**)[2]>(
      MEM_mallocN(ps->faces_num_eval * sizeof(float (*)[2]), "proj_paint_mtfaces"));

  return true;
}

struct ProjPaintLayerClone {
  const float (*uv_map_clone_base)[2];
  const TexPaintSlot *slot_last_clone;
  const TexPaintSlot *slot_clone;
};

static void proj_paint_layer_clone_init(ProjPaintState *ps, ProjPaintLayerClone *layer_clone)
{
  const float (*uv_map_clone_base)[2] = nullptr;

  /* use clone mtface? */
  if (ps->do_layer_clone) {
    const int layer_num = CustomData_get_clone_layer(&((Mesh *)ps->ob->data)->corner_data,
                                                     CD_PROP_FLOAT2);

    ps->poly_to_loop_uv_clone = static_cast<const float (**)[2]>(
        MEM_mallocN(ps->faces_num_eval * sizeof(float (*)[2]), "proj_paint_mtfaces"));

    if (layer_num != -1) {
      uv_map_clone_base = static_cast<const float (*)[2]>(
          CustomData_get_layer_n(&ps->mesh_eval->corner_data, CD_PROP_FLOAT2, layer_num));
    }

    if (uv_map_clone_base == nullptr) {
      /* get active instead */
      uv_map_clone_base = static_cast<const float (*)[2]>(
          CustomData_get_layer(&ps->mesh_eval->corner_data, CD_PROP_FLOAT2));
    }
  }

  memset(layer_clone, 0, sizeof(*layer_clone));
  layer_clone->uv_map_clone_base = uv_map_clone_base;
}

/* Return true if face should be skipped, false otherwise */
static bool project_paint_clone_face_skip(ProjPaintState *ps,
                                          ProjPaintLayerClone *lc,
                                          const TexPaintSlot *slot,
                                          const int tri_index)
{
  if (ps->do_layer_clone) {
    if (ps->do_material_slots) {
      lc->slot_clone = project_paint_face_clone_slot(ps, tri_index);
      /* all faces should have a valid slot, reassert here */
      if (ELEM(lc->slot_clone, nullptr, slot)) {
        return true;
      }
    }
    else if (ps->clone_ima == ps->canvas_ima) {
      return true;
    }

    if (ps->do_material_slots) {
      if (lc->slot_clone != lc->slot_last_clone) {
        if (!lc->slot_clone->uvname ||
            !(lc->uv_map_clone_base = static_cast<const float (*)[2]>(CustomData_get_layer_named(
                  &ps->mesh_eval->corner_data, CD_PROP_FLOAT2, lc->slot_clone->uvname))))
        {
          lc->uv_map_clone_base = static_cast<const float (*)[2]>(
              CustomData_get_layer(&ps->mesh_eval->corner_data, CD_PROP_FLOAT2));
        }
        lc->slot_last_clone = lc->slot_clone;
      }
    }

    /* will set multiple times for 4+ sided poly */
    ps->poly_to_loop_uv_clone[ps->corner_tri_faces_eval[tri_index]] = lc->uv_map_clone_base;
  }
  return false;
}

struct ProjPaintFaceLookup {
  const bool *select_poly_orig;
  const bool *hide_poly_orig;
  const int *index_mp_to_orig;
};

static void proj_paint_face_lookup_init(const ProjPaintState *ps, ProjPaintFaceLookup *face_lookup)
{
  memset(face_lookup, 0, sizeof(*face_lookup));
  Mesh *orig_mesh = (Mesh *)ps->ob->data;
  face_lookup->index_mp_to_orig = static_cast<const int *>(
      CustomData_get_layer(&ps->mesh_eval->face_data, CD_ORIGINDEX));
  if (ps->do_face_sel) {
    face_lookup->select_poly_orig = static_cast<const bool *>(
        CustomData_get_layer_named(&orig_mesh->face_data, CD_PROP_BOOL, ".select_poly"));
  }
  face_lookup->hide_poly_orig = static_cast<const bool *>(
      CustomData_get_layer_named(&orig_mesh->face_data, CD_PROP_BOOL, ".hide_poly"));
}

/* Return true if face should be considered paintable, false otherwise */
static bool project_paint_check_face_paintable(const ProjPaintState *ps,
                                               const ProjPaintFaceLookup *face_lookup,
                                               const int tri_i)
{
  if (ps->do_face_sel) {
    int orig_index;
    const int face_i = ps->corner_tri_faces_eval[tri_i];
    if ((face_lookup->index_mp_to_orig != nullptr) &&
        ((orig_index = (face_lookup->index_mp_to_orig[face_i])) != ORIGINDEX_NONE))
    {
      return face_lookup->select_poly_orig && face_lookup->select_poly_orig[orig_index];
    }
    return ps->select_poly_eval && ps->select_poly_eval[face_i];
  }
  int orig_index;
  const int face_i = ps->corner_tri_faces_eval[tri_i];
  if ((face_lookup->index_mp_to_orig != nullptr) &&
      ((orig_index = (face_lookup->index_mp_to_orig[face_i])) != ORIGINDEX_NONE))
  {
    return !(face_lookup->hide_poly_orig && face_lookup->hide_poly_orig[orig_index]);
  }
  return !(ps->hide_poly_eval && ps->hide_poly_eval[face_i]);
}

struct ProjPaintFaceCoSS {
  const float *v1;
  const float *v2;
  const float *v3;
};

static void proj_paint_face_coSS_init(const ProjPaintState *ps,
                                      const int3 &corner_tri,
                                      ProjPaintFaceCoSS *coSS)
{
  const int vert_tri[3] = {PS_CORNER_TRI_AS_VERT_INDEX_3(ps, corner_tri)};
  coSS->v1 = ps->screenCoords[vert_tri[0]];
  coSS->v2 = ps->screenCoords[vert_tri[1]];
  coSS->v3 = ps->screenCoords[vert_tri[2]];
}

/* Return true if face should be culled, false otherwise */
static bool project_paint_flt_max_cull(const ProjPaintState *ps, const ProjPaintFaceCoSS *coSS)
{
  if (!ps->is_ortho) {
    if (coSS->v1[0] == FLT_MAX || coSS->v2[0] == FLT_MAX || coSS->v3[0] == FLT_MAX) {
      return true;
    }
  }
  return false;
}

#ifdef PROJ_DEBUG_WINCLIP
/* Return true if face should be culled, false otherwise */
static bool project_paint_winclip(const ProjPaintState *ps, const ProjPaintFaceCoSS *coSS)
{
  /* ignore faces outside the view */
  return ((ps->source != PROJ_SRC_VIEW_FILL) &&
          ((coSS->v1[0] < ps->screenMin[0] && coSS->v2[0] < ps->screenMin[0] &&
            coSS->v3[0] < ps->screenMin[0]) ||

           (coSS->v1[0] > ps->screenMax[0] && coSS->v2[0] > ps->screenMax[0] &&
            coSS->v3[0] > ps->screenMax[0]) ||

           (coSS->v1[1] < ps->screenMin[1] && coSS->v2[1] < ps->screenMin[1] &&
            coSS->v3[1] < ps->screenMin[1]) ||

           (coSS->v1[1] > ps->screenMax[1] && coSS->v2[1] > ps->screenMax[1] &&
            coSS->v3[1] > ps->screenMax[1])));
}
#endif /* PROJ_DEBUG_WINCLIP */

struct PrepareImageEntry {
  PrepareImageEntry *next, *prev;
  Image *ima;
  ImageUser iuser;
};

static void project_paint_build_proj_ima(ProjPaintState *ps,
                                         MemArena *arena,
                                         ListBase *used_images)
{
  ProjPaintImage *projIma;
  PrepareImageEntry *entry;
  int i;

  /* build an array of images we use */
  projIma = ps->projImages = static_cast<ProjPaintImage *>(
      BLI_memarena_alloc(arena, sizeof(ProjPaintImage) * ps->image_tot));

  for (entry = static_cast<PrepareImageEntry *>(used_images->first), i = 0; entry;
       entry = entry->next, i++, projIma++)
  {
    projIma->iuser = entry->iuser;
    int size;
    projIma->ima = entry->ima;
    projIma->touch = false;
    projIma->ibuf = BKE_image_acquire_ibuf(projIma->ima, &projIma->iuser, nullptr);
    if (projIma->ibuf == nullptr) {
      projIma->iuser.tile = 0;
      projIma->ibuf = BKE_image_acquire_ibuf(projIma->ima, &projIma->iuser, nullptr);
      BLI_assert(projIma->ibuf != nullptr);
    }
    size = sizeof(void **) * ED_IMAGE_UNDO_TILE_NUMBER(projIma->ibuf->x) *
           ED_IMAGE_UNDO_TILE_NUMBER(projIma->ibuf->y);
    projIma->partRedrawRect = static_cast<ImagePaintPartialRedraw *>(
        BLI_memarena_alloc(arena, sizeof(ImagePaintPartialRedraw) * PROJ_BOUNDBOX_SQUARED));
    partial_redraw_array_init(projIma->partRedrawRect);
    projIma->undoRect = (volatile void **)BLI_memarena_alloc(arena, size);
    memset((void *)projIma->undoRect, 0, size);
    projIma->maskRect = static_cast<ushort **>(BLI_memarena_alloc(arena, size));
    memset(projIma->maskRect, 0, size);
    projIma->valid = static_cast<bool **>(BLI_memarena_alloc(arena, size));
    memset(projIma->valid, 0, size);
  }
}

static void project_paint_prepare_all_faces(ProjPaintState *ps,
                                            MemArena *arena,
                                            const ProjPaintFaceLookup *face_lookup,
                                            ProjPaintLayerClone *layer_clone,
                                            const float (*uv_map_base)[2],
                                            const bool is_multi_view)
{
  /* Image Vars - keep track of images we have used */
  ListBase used_images = {nullptr};

  Image *tpage_last = nullptr, *tpage;
  TexPaintSlot *slot_last = nullptr;
  TexPaintSlot *slot = nullptr;
  int tile_last = -1, tile;
  int image_index = -1, tri_index;
  int prev_poly = -1;
  const blender::Span<int3> corner_tris = ps->corner_tris_eval;
  const blender::Span<int> tri_faces = ps->corner_tri_faces_eval;

  BLI_assert(ps->image_tot == 0);

  for (tri_index = 0; tri_index < ps->corner_tris_eval.size(); tri_index++) {
    bool is_face_paintable;
    bool skip_tri = false;

    is_face_paintable = project_paint_check_face_paintable(ps, face_lookup, tri_index);

    if (!ps->do_stencil_brush) {
      slot = project_paint_face_paint_slot(ps, tri_index);
      /* all faces should have a valid slot, reassert here */
      if (slot == nullptr) {
        uv_map_base = static_cast<const float (*)[2]>(
            CustomData_get_layer(&ps->mesh_eval->corner_data, CD_PROP_FLOAT2));
        tpage = ps->canvas_ima;
      }
      else {
        if (slot != slot_last) {
          if (!slot->uvname ||
              !(uv_map_base = static_cast<const float (*)[2]>(CustomData_get_layer_named(
                    &ps->mesh_eval->corner_data, CD_PROP_FLOAT2, slot->uvname))))
          {
            uv_map_base = static_cast<const float (*)[2]>(
                CustomData_get_layer(&ps->mesh_eval->corner_data, CD_PROP_FLOAT2));
          }
          slot_last = slot;
        }

        /* Don't allow painting on linked images. */
        if (slot->ima != nullptr &&
            (!ID_IS_EDITABLE(slot->ima) || ID_IS_OVERRIDE_LIBRARY(slot->ima)))
        {
          skip_tri = true;
          tpage = nullptr;
        }

        /* Don't allow using the same image for painting and stenciling. */
        if (slot->ima == ps->stencil_ima) {
          /* Delay continuing the loop until after loop_uvs and bleed faces are initialized.
           * While this shouldn't be used, face-winding reads all faces.
           * It's less trouble to set all faces to valid UVs,
           * avoiding nullptr checks all over. */
          skip_tri = true;
          tpage = nullptr;
        }
        else {
          tpage = slot->ima;
        }
      }
    }
    else {
      tpage = ps->stencil_ima;
    }

    ps->poly_to_loop_uv[tri_faces[tri_index]] = uv_map_base;

    tile = project_paint_face_paint_tile(tpage, uv_map_base[corner_tris[tri_index][0]]);

#ifndef PROJ_DEBUG_NOSEAMBLEED
    project_paint_bleed_add_face_user(ps, arena, corner_tris[tri_index], tri_index);
#endif

    if (skip_tri || project_paint_clone_face_skip(ps, layer_clone, slot, tri_index)) {
      continue;
    }

    BLI_assert(uv_map_base != nullptr);

    if (is_face_paintable && tpage) {
      ProjPaintFaceCoSS coSS;
      proj_paint_face_coSS_init(ps, corner_tris[tri_index], &coSS);

      if (is_multi_view == false) {
        if (project_paint_flt_max_cull(ps, &coSS)) {
          continue;
        }

#ifdef PROJ_DEBUG_WINCLIP
        if (project_paint_winclip(ps, &coSS)) {
          continue;
        }

#endif  // PROJ_DEBUG_WINCLIP

        /* Back-face culls individual triangles but mask normal will use face. */
        if (ps->do_backfacecull) {
          if (ps->do_mask_normal) {
            if (prev_poly != tri_faces[tri_index]) {
              bool culled = true;
              const blender::IndexRange poly = ps->faces_eval[tri_faces[tri_index]];
              prev_poly = tri_faces[tri_index];
              for (const int corner : poly) {
                if (!(ps->vertFlags[ps->corner_verts_eval[corner]] & PROJ_VERT_CULL)) {
                  culled = false;
                  break;
                }
              }

              if (culled) {
                /* poly loops - 2 is number of triangles for poly,
                 * but counter gets incremented when continuing, so decrease by 3 */
                int poly_tri = poly.size() - 3;
                tri_index += poly_tri;
                continue;
              }
            }
          }
          else {
            if ((line_point_side_v2(coSS.v1, coSS.v2, coSS.v3) < 0.0f) != ps->is_flip_object) {
              continue;
            }
          }
        }
      }

      if (tpage_last != tpage || tile_last != tile) {
        image_index = 0;
        for (PrepareImageEntry *e = static_cast<PrepareImageEntry *>(used_images.first); e;
             e = e->next, image_index++)
        {
          if (e->ima == tpage && e->iuser.tile == tile) {
            break;
          }
        }

        if (image_index == ps->image_tot) {
          /* XXX get appropriate ImageUser instead */
          ImageUser iuser;
          BKE_imageuser_default(&iuser);
          iuser.tile = tile;
          iuser.framenr = tpage->lastframe;
          if (BKE_image_has_ibuf(tpage, &iuser)) {
            PrepareImageEntry *e = MEM_callocN<PrepareImageEntry>("PrepareImageEntry");
            e->ima = tpage;
            e->iuser = iuser;
            BLI_addtail(&used_images, e);
            ps->image_tot++;
          }
          else {
            image_index = -1;
          }
        }

        tpage_last = tpage;
        tile_last = tile;
      }

      if (image_index != -1) {
        /* Initialize the faces screen pixels */
        /* Add this to a list to initialize later */
        project_paint_delayed_face_init(ps, corner_tris[tri_index], tri_index);
      }
    }
  }

  /* Build an array of images we use. */
  if (ps->is_shared_user == false) {
    project_paint_build_proj_ima(ps, arena, &used_images);
  }

  /* we have built the array, discard the linked list */
  BLI_freelistN(&used_images);
}

/* run once per stroke before projection painting */
static void project_paint_begin(const bContext *C,
                                ProjPaintState *ps,
                                const bool is_multi_view,
                                const char symmetry_flag)
{
  ProjPaintLayerClone layer_clone;
  ProjPaintFaceLookup face_lookup;
  const float (*uv_map_base)[2] = nullptr;

  /* At the moment this is just ps->arena_mt[0], but use this to show were not multi-threading. */
  MemArena *arena;

  const int diameter = BKE_brush_size_get(ps->paint, ps->brush);

  bool reset_threads = false;

  /* ---- end defines ---- */

  if (ps->source == PROJ_SRC_VIEW) {
    /* faster clipping lookups */
    ED_view3d_clipping_local(ps->rv3d, ps->ob->object_to_world().ptr());
  }

  ps->do_face_sel = ((((Mesh *)ps->ob->data)->editflag & ME_EDIT_PAINT_FACE_SEL) != 0);
  ps->is_flip_object = (ps->ob->transflag & OB_NEG_SCALE) != 0;

  /* paint onto the derived mesh */
  if (ps->is_shared_user == false) {
    if (!proj_paint_state_mesh_eval_init(C, ps)) {
      return;
    }
  }

  proj_paint_face_lookup_init(ps, &face_lookup);
  proj_paint_layer_clone_init(ps, &layer_clone);

  if (ps->do_layer_stencil || ps->do_stencil_brush) {
    // int layer_num = CustomData_get_stencil_layer(&ps->mesh_eval->ldata, CD_PROP_FLOAT2);
    int layer_num = CustomData_get_stencil_layer(&((Mesh *)ps->ob->data)->corner_data,
                                                 CD_PROP_FLOAT2);
    if (layer_num != -1) {
      ps->uv_map_stencil_eval = static_cast<const float (*)[2]>(
          CustomData_get_layer_n(&ps->mesh_eval->corner_data, CD_PROP_FLOAT2, layer_num));
    }

    if (ps->uv_map_stencil_eval == nullptr) {
      /* get active instead */
      ps->uv_map_stencil_eval = static_cast<const float (*)[2]>(
          CustomData_get_layer(&ps->mesh_eval->corner_data, CD_PROP_FLOAT2));
    }

    if (ps->do_stencil_brush) {
      uv_map_base = ps->uv_map_stencil_eval;
    }
  }

  /* when using sub-surface or multi-resolution,
   * mesh-data arrays are thrown away, we need to keep a copy. */
  if (ps->is_shared_user == false) {
    proj_paint_state_cavity_init(ps);
  }

  proj_paint_state_viewport_init(ps, symmetry_flag);

  /* calculate vert screen coords
   * run this early so we can calculate the x/y resolution of our bucket rect */
  proj_paint_state_screen_coords_init(ps, diameter);

  /* only for convenience */
  ps->screen_width = ps->screenMax[0] - ps->screenMin[0];
  ps->screen_height = ps->screenMax[1] - ps->screenMin[1];

  ps->buckets_x = int(ps->screen_width / (float(diameter) / PROJ_BUCKET_BRUSH_DIV));
  ps->buckets_y = int(ps->screen_height / (float(diameter) / PROJ_BUCKET_BRUSH_DIV));

  // printf("\tscreenspace bucket division x:%d y:%d\n", ps->buckets_x, ps->buckets_y);

  if (ps->buckets_x > PROJ_BUCKET_RECT_MAX || ps->buckets_y > PROJ_BUCKET_RECT_MAX) {
    reset_threads = true;
  }

  /* Really high values could cause problems since it has to allocate a few
   * `(ps->buckets_x * ps->buckets_y)` sized arrays. */
  CLAMP(ps->buckets_x, PROJ_BUCKET_RECT_MIN, PROJ_BUCKET_RECT_MAX);
  CLAMP(ps->buckets_y, PROJ_BUCKET_RECT_MIN, PROJ_BUCKET_RECT_MAX);

  ps->bucketRect = MEM_calloc_arrayN<LinkNode *>(ps->buckets_x * ps->buckets_y,
                                                 "paint-bucketRect");
  ps->bucketFaces = MEM_calloc_arrayN<LinkNode *>(ps->buckets_x * ps->buckets_y,
                                                  "paint-bucketFaces");

  ps->bucketFlags = MEM_calloc_arrayN<uchar>(ps->buckets_x * ps->buckets_y, "paint-bucketFaces");
#ifndef PROJ_DEBUG_NOSEAMBLEED
  if (ps->is_shared_user == false) {
    proj_paint_state_seam_bleed_init(ps);
  }
#endif

  proj_paint_state_thread_init(ps, reset_threads);
  arena = ps->arena_mt[0];

  proj_paint_state_vert_flags_init(ps);

  project_paint_prepare_all_faces(
      ps, arena, &face_lookup, &layer_clone, uv_map_base, is_multi_view);
}

static void paint_proj_begin_clone(ProjPaintState *ps, const float mouse[2])
{
  /* setup clone offset */
  if (ps->brush_type == IMAGE_PAINT_BRUSH_TYPE_CLONE) {
    float projCo[4];
    copy_v3_v3(projCo, ps->scene->cursor.location);
    mul_m4_v3(ps->obmat_imat, projCo);

    projCo[3] = 1.0f;
    mul_m4_v4(ps->projectMat, projCo);
    ps->cloneOffset[0] = mouse[0] -
                         (float(ps->winx * 0.5f) + (ps->winx * 0.5f) * projCo[0] / projCo[3]);
    ps->cloneOffset[1] = mouse[1] -
                         (float(ps->winy * 0.5f) + (ps->winy * 0.5f) * projCo[1] / projCo[3]);
  }
}

static void project_paint_end(ProjPaintState *ps)
{
  int a;

  /* dereference used image buffers */
  if (ps->is_shared_user == false) {
    ProjPaintImage *projIma;
    for (a = 0, projIma = ps->projImages; a < ps->image_tot; a++, projIma++) {
      BKE_image_release_ibuf(projIma->ima, projIma->ibuf, nullptr);
      DEG_id_tag_update(&projIma->ima->id, 0);
    }
  }

  if (ps->reproject_ibuf_free_float) {
    IMB_free_float_pixels(ps->reproject_ibuf);
  }
  if (ps->reproject_ibuf_free_uchar) {
    IMB_free_byte_pixels(ps->reproject_ibuf);
  }
  BKE_image_release_ibuf(ps->reproject_image, ps->reproject_ibuf, nullptr);

  MEM_freeN(ps->screenCoords);
  MEM_freeN(ps->bucketRect);
  MEM_freeN(ps->bucketFaces);
  MEM_freeN(ps->bucketFlags);

  if (ps->is_shared_user == false) {
    if (ps->mat_array != nullptr) {
      MEM_freeN(ps->mat_array);
    }

    /* must be set for non-shared */
    BLI_assert(ps->poly_to_loop_uv || ps->is_shared_user);
    if (ps->poly_to_loop_uv) {
      MEM_freeN(ps->poly_to_loop_uv);
    }

    if (ps->do_layer_clone) {
      MEM_freeN(ps->poly_to_loop_uv_clone);
    }
    if (ps->thread_tot > 1) {
      BLI_spin_end(ps->tile_lock);
      /* The void cast is needed when building without TBB. */
      MEM_freeN((void *)ps->tile_lock);
    }

    ED_image_paint_tile_lock_end();

#ifndef PROJ_DEBUG_NOSEAMBLEED
    if (ps->seam_bleed_px > 0.0f) {
      MEM_freeN(ps->vertFaces);
      MEM_freeN(ps->faceSeamFlags);
      MEM_freeN(ps->faceWindingFlags);
      MEM_freeN(ps->loopSeamData);
      MEM_freeN(ps->vertSeams);
    }
#endif

    if (ps->do_mask_cavity) {
      MEM_freeN(ps->cavities);
    }

    ps->mesh_eval = nullptr;
  }

  if (ps->blurkernel) {
    paint_delete_blur_kernel(ps->blurkernel);
    MEM_delete(ps->blurkernel);
  }

  if (ps->vertFlags) {
    MEM_freeN(ps->vertFlags);
  }

  for (a = 0; a < ps->thread_tot; a++) {
    BLI_memarena_free(ps->arena_mt[a]);
  }
}

/* 1 = an undo, -1 is a redo. */
static void partial_redraw_single_init(ImagePaintPartialRedraw *pr)
{
  BLI_rcti_init_minmax(&pr->dirty_region);
}

static void partial_redraw_array_init(ImagePaintPartialRedraw *pr)
{
  int tot = PROJ_BOUNDBOX_SQUARED;
  while (tot--) {
    partial_redraw_single_init(pr);
    pr++;
  }
}

static bool partial_redraw_array_merge(ImagePaintPartialRedraw *pr,
                                       ImagePaintPartialRedraw *pr_other,
                                       int tot)
{
  bool touch = false;
  while (tot--) {
    BLI_rcti_do_minmax_rcti(&pr->dirty_region, &pr_other->dirty_region);
    if (!BLI_rcti_is_empty(&pr->dirty_region)) {
      touch = true;
    }

    pr++;
    pr_other++;
  }

  return touch;
}

/* Loop over all images on this mesh and update any we have touched */
static bool project_image_refresh_tagged(ProjPaintState *ps)
{
  ImagePaintPartialRedraw *pr;
  ProjPaintImage *projIma;
  int a, i;
  bool redraw = false;

  for (a = 0, projIma = ps->projImages; a < ps->image_tot; a++, projIma++) {
    if (projIma->touch) {
      /* look over each bound cell */
      for (i = 0; i < PROJ_BOUNDBOX_SQUARED; i++) {
        pr = &(projIma->partRedrawRect[i]);
        if (BLI_rcti_is_valid(&pr->dirty_region)) {
          set_imapaintpartial(pr);
          imapaint_image_update(nullptr, projIma->ima, projIma->ibuf, &projIma->iuser, true);
          redraw = true;
        }

        partial_redraw_single_init(pr);
      }

      /* clear for reuse */
      projIma->touch = false;
    }
  }

  return redraw;
}

/* run this per painting onto each mouse location */
static bool project_bucket_iter_init(ProjPaintState *ps, const float mval_f[2])
{
  if (ps->source == PROJ_SRC_VIEW) {
    float min_brush[2], max_brush[2];
    const float radius = ps->brush_size;

    /* so we don't have a bucket bounds that is way too small to paint into */
#if 0
    /* This doesn't work yet. */
    if (radius < 1.0f) {
      radius = 1.0f;
    }
#endif

    min_brush[0] = mval_f[0] - radius;
    min_brush[1] = mval_f[1] - radius;

    max_brush[0] = mval_f[0] + radius;
    max_brush[1] = mval_f[1] + radius;

    /* offset to make this a valid bucket index */
    project_paint_bucket_bounds(ps, min_brush, max_brush, ps->bucketMin, ps->bucketMax);

    /* mouse outside the model areas? */
    if (ps->bucketMin[0] == ps->bucketMax[0] || ps->bucketMin[1] == ps->bucketMax[1]) {
      return false;
    }
  }
  else { /* reproject: PROJ_SRC_* */
    ps->bucketMin[0] = 0;
    ps->bucketMin[1] = 0;

    ps->bucketMax[0] = ps->buckets_x;
    ps->bucketMax[1] = ps->buckets_y;
  }

  ps->context_bucket_index = ps->bucketMin[0] + ps->bucketMin[1] * ps->buckets_x;
  return true;
}

static bool project_bucket_iter_next(ProjPaintState *ps,
                                     int *bucket_index,
                                     rctf *bucket_bounds,
                                     const float mval[2])
{
  const int diameter = 2 * ps->brush_size;

  const int max_bucket_idx = ps->bucketMax[0] + (ps->bucketMax[1] - 1) * ps->buckets_x;

  for (int bidx = atomic_fetch_and_add_int32(&ps->context_bucket_index, 1); bidx < max_bucket_idx;
       bidx = atomic_fetch_and_add_int32(&ps->context_bucket_index, 1))
  {
    const int bucket_y = bidx / ps->buckets_x;
    const int bucket_x = bidx - (bucket_y * ps->buckets_x);

    BLI_assert(bucket_y >= ps->bucketMin[1] && bucket_y < ps->bucketMax[1]);
    if (bucket_x >= ps->bucketMin[0] && bucket_x < ps->bucketMax[0]) {
      /* Use bucket_bounds for #project_bucket_isect_circle and #project_bucket_init. */
      project_bucket_bounds(ps, bucket_x, bucket_y, bucket_bounds);

      if ((ps->source != PROJ_SRC_VIEW) ||
          project_bucket_isect_circle(mval, float(diameter * diameter), bucket_bounds))
      {
        *bucket_index = bidx;

        return true;
      }
    }
  }

  return false;
}

/* Each thread gets one of these, also used as an argument to pass to project_paint_op */
struct ProjectHandle {
  /* args */
  ProjPaintState *ps;
  float prevmval[2];
  float mval[2];

  /* Annoying but we need to have image bounds per thread,
   * then merge into ps->projectPartialRedraws. */

  /* array of partial redraws */
  ProjPaintImage *projImages;

  /* thread settings */
  int thread_index;

  ImagePool *pool;
};

static void do_projectpaint_clone(ProjPaintState *ps, ProjPixel *projPixel, float mask)
{
  const uchar *clone_pt = ((ProjPixelClone *)projPixel)->clonepx.ch;

  if (clone_pt[3]) {
    uchar clone_rgba[4];

    clone_rgba[0] = clone_pt[0];
    clone_rgba[1] = clone_pt[1];
    clone_rgba[2] = clone_pt[2];
    clone_rgba[3] = uchar(clone_pt[3] * mask);

    if (ps->do_masking) {
      IMB_blend_color_byte(projPixel->pixel.ch_pt,
                           projPixel->origColor.ch_pt,
                           clone_rgba,
                           IMB_BlendMode(ps->blend));
    }
    else {
      IMB_blend_color_byte(
          projPixel->pixel.ch_pt, projPixel->pixel.ch_pt, clone_rgba, IMB_BlendMode(ps->blend));
    }
  }
}

static void do_projectpaint_clone_f(ProjPaintState *ps, ProjPixel *projPixel, float mask)
{
  const float *clone_pt = ((ProjPixelClone *)projPixel)->clonepx.f;

  if (clone_pt[3]) {
    float clone_rgba[4];

    mul_v4_v4fl(clone_rgba, clone_pt, mask);

    if (ps->do_masking) {
      IMB_blend_color_float(
          projPixel->pixel.f_pt, projPixel->origColor.f_pt, clone_rgba, IMB_BlendMode(ps->blend));
    }
    else {
      IMB_blend_color_float(
          projPixel->pixel.f_pt, projPixel->pixel.f_pt, clone_rgba, IMB_BlendMode(ps->blend));
    }
  }
}

/**
 * \note mask is used to modify the alpha here, this is not correct since it allows
 * accumulation of color greater than 'projPixel->mask' however in the case of smear its not
 * really that important to be correct as it is with clone and painting
 */
static void do_projectpaint_smear(ProjPaintState *ps,
                                  ProjPixel *projPixel,
                                  float mask,
                                  MemArena *smearArena,
                                  LinkNode **smearPixels,
                                  const float co[2])
{
  uchar rgba_ub[4];

  if (project_paint_PickColor(ps, co, nullptr, rgba_ub, true) == 0) {
    return;
  }

  blend_color_interpolate_byte(
      ((ProjPixelClone *)projPixel)->clonepx.ch, projPixel->pixel.ch_pt, rgba_ub, mask);
  BLI_linklist_prepend_arena(smearPixels, (void *)projPixel, smearArena);
}

static void do_projectpaint_smear_f(ProjPaintState *ps,
                                    ProjPixel *projPixel,
                                    float mask,
                                    MemArena *smearArena,
                                    LinkNode **smearPixels_f,
                                    const float co[2])
{
  float rgba[4];

  if (project_paint_PickColor(ps, co, rgba, nullptr, true) == 0) {
    return;
  }

  blend_color_interpolate_float(
      ((ProjPixelClone *)projPixel)->clonepx.f, projPixel->pixel.f_pt, rgba, mask);
  BLI_linklist_prepend_arena(smearPixels_f, (void *)projPixel, smearArena);
}

static void do_projectpaint_soften_f(ProjPaintState *ps,
                                     ProjPixel *projPixel,
                                     float mask,
                                     MemArena *softenArena,
                                     LinkNode **softenPixels)
{
  float accum_tot = 0.0f;
  int xk, yk;
  BlurKernel *kernel = ps->blurkernel;
  float *rgba = projPixel->newColor.f;

  /* rather than painting, accumulate surrounding colors */
  zero_v4(rgba);

  for (yk = 0; yk < kernel->side; yk++) {
    for (xk = 0; xk < kernel->side; xk++) {
      float rgba_tmp[4];
      float co_ofs[2] = {2.0f * xk - 1.0f, 2.0f * yk - 1.0f};

      add_v2_v2(co_ofs, projPixel->projCoSS);

      if (project_paint_PickColor(ps, co_ofs, rgba_tmp, nullptr, true)) {
        float weight = kernel->wdata[xk + yk * kernel->side];
        mul_v4_fl(rgba_tmp, weight);
        add_v4_v4(rgba, rgba_tmp);
        accum_tot += weight;
      }
    }
  }

  if (LIKELY(accum_tot != 0)) {
    mul_v4_fl(rgba, 1.0f / accum_tot);

    if (ps->mode == BRUSH_STROKE_INVERT) {
      /* subtract blurred image from normal image gives high pass filter */
      sub_v3_v3v3(rgba, projPixel->pixel.f_pt, rgba);

      /* now rgba_ub contains the edge result, but this should be converted to luminance to avoid
       * colored speckles appearing in final image, and also to check for threshold */
      rgba[0] = rgba[1] = rgba[2] = IMB_colormanagement_get_luminance(rgba);
      if (fabsf(rgba[0]) > ps->brush->sharp_threshold) {
        float alpha = projPixel->pixel.f_pt[3];
        projPixel->pixel.f_pt[3] = rgba[3] = mask;

        /* add to enhance edges */
        blend_color_add_float(rgba, projPixel->pixel.f_pt, rgba);
        rgba[3] = alpha;
      }
      else {
        return;
      }
    }
    else {
      blend_color_interpolate_float(rgba, projPixel->pixel.f_pt, rgba, mask);
    }

    BLI_linklist_prepend_arena(softenPixels, (void *)projPixel, softenArena);
  }
}

static void do_projectpaint_soften(ProjPaintState *ps,
                                   ProjPixel *projPixel,
                                   float mask,
                                   MemArena *softenArena,
                                   LinkNode **softenPixels)
{
  float accum_tot = 0;
  int xk, yk;
  BlurKernel *kernel = ps->blurkernel;
  /* convert to byte after */
  float rgba[4];

  /* rather than painting, accumulate surrounding colors */
  zero_v4(rgba);

  for (yk = 0; yk < kernel->side; yk++) {
    for (xk = 0; xk < kernel->side; xk++) {
      float rgba_tmp[4];
      float co_ofs[2] = {2.0f * xk - 1.0f, 2.0f * yk - 1.0f};

      add_v2_v2(co_ofs, projPixel->projCoSS);

      if (project_paint_PickColor(ps, co_ofs, rgba_tmp, nullptr, true)) {
        float weight = kernel->wdata[xk + yk * kernel->side];
        mul_v4_fl(rgba_tmp, weight);
        add_v4_v4(rgba, rgba_tmp);
        accum_tot += weight;
      }
    }
  }

  if (LIKELY(accum_tot != 0)) {
    uchar *rgba_ub = projPixel->newColor.ch;

    mul_v4_fl(rgba, 1.0f / accum_tot);

    if (ps->mode == BRUSH_STROKE_INVERT) {
      float rgba_pixel[4];

      straight_uchar_to_premul_float(rgba_pixel, projPixel->pixel.ch_pt);

      /* subtract blurred image from normal image gives high pass filter */
      sub_v3_v3v3(rgba, rgba_pixel, rgba);
      /* now rgba_ub contains the edge result, but this should be converted to luminance to avoid
       * colored speckles appearing in final image, and also to check for threshold */
      rgba[0] = rgba[1] = rgba[2] = IMB_colormanagement_get_luminance(rgba);
      if (fabsf(rgba[0]) > ps->brush->sharp_threshold) {
        float alpha = rgba_pixel[3];
        rgba[3] = rgba_pixel[3] = mask;

        /* add to enhance edges */
        blend_color_add_float(rgba, rgba_pixel, rgba);

        rgba[3] = alpha;
        premul_float_to_straight_uchar(rgba_ub, rgba);
      }
      else {
        return;
      }
    }
    else {
      premul_float_to_straight_uchar(rgba_ub, rgba);
      blend_color_interpolate_byte(rgba_ub, projPixel->pixel.ch_pt, rgba_ub, mask);
    }
    BLI_linklist_prepend_arena(softenPixels, (void *)projPixel, softenArena);
  }
}

static void do_projectpaint_draw(ProjPaintState *ps,
                                 ProjPixel *projPixel,
                                 const float texrgb[3],
                                 float mask,
                                 float dither,
                                 int u,
                                 int v)
{
  const ProjPaintImage *img = &ps->projImages[projPixel->image_index];
  float rgb[3];
  uchar rgba_ub[4];

  if (ps->is_texbrush) {
    mul_v3_v3v3(rgb, texrgb, ps->paint_color_linear);
    if (img->is_srgb) {
      /* Fast-ish path for sRGB. */
      IMB_colormanagement_scene_linear_to_srgb_v3(rgb, rgb);
    }
    else if (img->byte_colorspace) {
      /* Slow path with arbitrary colorspace. */
      IMB_colormanagement_scene_linear_to_colorspace_v3(rgb, img->byte_colorspace);
    }
  }
  else {
    copy_v3_v3(rgb, img->paint_color_byte);
  }

  if (dither > 0.0f) {
    float_to_byte_dither_v3(rgba_ub, rgb, dither, u, v);
  }
  else {
    unit_float_to_uchar_clamp_v3(rgba_ub, rgb);
  }
  rgba_ub[3] = f_to_char(mask);

  if (ps->do_masking) {
    IMB_blend_color_byte(
        projPixel->pixel.ch_pt, projPixel->origColor.ch_pt, rgba_ub, IMB_BlendMode(ps->blend));
  }
  else {
    IMB_blend_color_byte(
        projPixel->pixel.ch_pt, projPixel->pixel.ch_pt, rgba_ub, IMB_BlendMode(ps->blend));
  }
}

static void do_projectpaint_draw_f(ProjPaintState *ps,
                                   ProjPixel *projPixel,
                                   const float texrgb[3],
                                   float mask)
{
  float rgba[4];

  copy_v3_v3(rgba, ps->paint_color_linear);

  if (ps->is_texbrush) {
    mul_v3_v3(rgba, texrgb);
  }

  mul_v3_fl(rgba, mask);
  rgba[3] = mask;

  if (ps->do_masking) {
    IMB_blend_color_float(
        projPixel->pixel.f_pt, projPixel->origColor.f_pt, rgba, IMB_BlendMode(ps->blend));
  }
  else {
    IMB_blend_color_float(
        projPixel->pixel.f_pt, projPixel->pixel.f_pt, rgba, IMB_BlendMode(ps->blend));
  }
}

static void do_projectpaint_mask(ProjPaintState *ps, ProjPixel *projPixel, float mask)
{
  uchar rgba_ub[4];
  rgba_ub[0] = rgba_ub[1] = rgba_ub[2] = ps->stencil_value * 255.0f;
  rgba_ub[3] = f_to_char(mask);

  if (ps->do_masking) {
    IMB_blend_color_byte(
        projPixel->pixel.ch_pt, projPixel->origColor.ch_pt, rgba_ub, IMB_BlendMode(ps->blend));
  }
  else {
    IMB_blend_color_byte(
        projPixel->pixel.ch_pt, projPixel->pixel.ch_pt, rgba_ub, IMB_BlendMode(ps->blend));
  }
}

static void do_projectpaint_mask_f(ProjPaintState *ps, ProjPixel *projPixel, float mask)
{
  float rgba[4];
  rgba[0] = rgba[1] = rgba[2] = ps->stencil_value;
  rgba[3] = mask;

  if (ps->do_masking) {
    IMB_blend_color_float(
        projPixel->pixel.f_pt, projPixel->origColor.f_pt, rgba, IMB_BlendMode(ps->blend));
  }
  else {
    IMB_blend_color_float(
        projPixel->pixel.f_pt, projPixel->pixel.f_pt, rgba, IMB_BlendMode(ps->blend));
  }
}

static void image_paint_partial_redraw_expand(ImagePaintPartialRedraw *cell,
                                              const ProjPixel *projPixel)
{
  rcti rect_to_add;
  BLI_rcti_init(
      &rect_to_add, projPixel->x_px, projPixel->x_px + 1, projPixel->y_px, projPixel->y_px + 1);
  BLI_rcti_do_minmax_rcti(&cell->dirty_region, &rect_to_add);
}

static void copy_original_alpha_channel(ProjPixel *pixel, bool is_floatbuf)
{
  /* Use the original alpha channel data instead of the modified one */
  if (is_floatbuf) {
    /* slightly more involved case since floats are in premultiplied space we need
     * to make sure alpha is consistent, see #44627 */
    float rgb_straight[4];
    premul_to_straight_v4_v4(rgb_straight, pixel->pixel.f_pt);
    rgb_straight[3] = pixel->origColor.f_pt[3];
    straight_to_premul_v4_v4(pixel->pixel.f_pt, rgb_straight);
  }
  else {
    pixel->pixel.ch_pt[3] = pixel->origColor.ch_pt[3];
  }
}

/* Run this for single and multi-threaded painting. */
static void do_projectpaint_thread(TaskPool *__restrict /*pool*/, void *ph_v)
{
  /* First unpack args from the struct */
  ProjPaintState *ps = ((ProjectHandle *)ph_v)->ps;
  ProjPaintImage *projImages = ((ProjectHandle *)ph_v)->projImages;
  const float *lastpos = ((ProjectHandle *)ph_v)->prevmval;
  const float *pos = ((ProjectHandle *)ph_v)->mval;
  const int thread_index = ((ProjectHandle *)ph_v)->thread_index;
  ImagePool *pool = ((ProjectHandle *)ph_v)->pool;
  /* Done with args from ProjectHandle */

  LinkNode *node;
  ProjPixel *projPixel;
  Brush *brush = ps->brush;

  int last_index = -1;
  ProjPaintImage *last_projIma = nullptr;
  ImagePaintPartialRedraw *last_partial_redraw_cell;

  float dist_sq, dist;

  float falloff;
  int bucket_index;
  bool is_floatbuf = false;
  const short brush_type = ps->brush_type;
  rctf bucket_bounds;

  /* for smear only */
  float pos_ofs[2] = {0};
  float co[2];
  ushort mask_short;
  const float brush_alpha = BKE_brush_alpha_get(ps->paint, brush);
  const float brush_radius = ps->brush_size;
  /* avoid a square root with every dist comparison */
  const float brush_radius_sq = brush_radius * brush_radius;

  const bool lock_alpha = ELEM(brush->blend, IMB_BLEND_ERASE_ALPHA, IMB_BLEND_ADD_ALPHA) ?
                              false :
                              (brush->flag & BRUSH_LOCK_ALPHA) != 0;

  LinkNode *smearPixels = nullptr;
  LinkNode *smearPixels_f = nullptr;
  /* mem arena for this brush projection only */
  MemArena *smearArena = nullptr;

  LinkNode *softenPixels = nullptr;
  LinkNode *softenPixels_f = nullptr;
  /* mem arena for this brush projection only */
  MemArena *softenArena = nullptr;

  if (brush_type == IMAGE_PAINT_BRUSH_TYPE_SMEAR) {
    pos_ofs[0] = pos[0] - lastpos[0];
    pos_ofs[1] = pos[1] - lastpos[1];

    smearArena = BLI_memarena_new(MEM_SIZE_OPTIMAL(1 << 16), "paint smear arena");
  }
  else if (brush_type == IMAGE_PAINT_BRUSH_TYPE_SOFTEN) {
    softenArena = BLI_memarena_new(MEM_SIZE_OPTIMAL(1 << 16), "paint soften arena");
  }

  // printf("brush bounds %d %d %d %d\n",
  //        bucketMin[0], bucketMin[1], bucketMax[0], bucketMax[1]);

  while (project_bucket_iter_next(ps, &bucket_index, &bucket_bounds, pos)) {

    /* Check this bucket and its faces are initialized */
    if (ps->bucketFlags[bucket_index] == PROJ_BUCKET_NULL) {
      rctf clip_rect = bucket_bounds;
      clip_rect.xmin -= PROJ_PIXEL_TOLERANCE;
      clip_rect.xmax += PROJ_PIXEL_TOLERANCE;
      clip_rect.ymin -= PROJ_PIXEL_TOLERANCE;
      clip_rect.ymax += PROJ_PIXEL_TOLERANCE;
      /* No pixels initialized */
      project_bucket_init(ps, thread_index, bucket_index, &clip_rect, &bucket_bounds);
    }

    if (ps->source != PROJ_SRC_VIEW) {

      /* Re-Projection, simple, no brushes! */

      for (node = ps->bucketRect[bucket_index]; node; node = node->next) {
        projPixel = (ProjPixel *)node->link;

        /* copy of code below */
        if (last_index != projPixel->image_index) {
          last_index = projPixel->image_index;
          last_projIma = projImages + last_index;

          last_projIma->touch = true;
          is_floatbuf = (last_projIma->ibuf->float_buffer.data != nullptr);
        }
        /* end copy */

        /* fill brushes */
        if (ps->source == PROJ_SRC_VIEW_FILL) {
          if (brush->flag & BRUSH_USE_GRADIENT) {
            /* these could probably be cached instead of being done per pixel */
            float tangent[2];
            float line_len_sq_inv, line_len;
            float f;
            float color_f[4];
            const float p[2] = {
                projPixel->projCoSS[0] - lastpos[0],
                projPixel->projCoSS[1] - lastpos[1],
            };

            sub_v2_v2v2(tangent, pos, lastpos);
            line_len = len_squared_v2(tangent);
            line_len_sq_inv = 1.0f / line_len;
            line_len = sqrtf(line_len);

            switch (brush->gradient_fill_mode) {
              case BRUSH_GRADIENT_LINEAR: {
                f = dot_v2v2(p, tangent) * line_len_sq_inv;
                break;
              }
              case BRUSH_GRADIENT_RADIAL:
              default: {
                f = len_v2(p) / line_len;
                break;
              }
            }
            BKE_colorband_evaluate(brush->gradient, f, color_f);
            color_f[3] *= float(projPixel->mask) * (1.0f / 65535.0f) * brush_alpha;

            if (is_floatbuf) {
              /* Convert to premutliplied. */
              mul_v3_fl(color_f, color_f[3]);
              IMB_blend_color_float(projPixel->pixel.f_pt,
                                    projPixel->origColor.f_pt,
                                    color_f,
                                    IMB_BlendMode(ps->blend));
            }
            else {
              const ProjPaintImage *img = &ps->projImages[projPixel->image_index];
              if (img->is_srgb) {
                IMB_colormanagement_scene_linear_to_srgb_v3(color_f, color_f);
              }
              else if (img->byte_colorspace) {
                IMB_colormanagement_scene_linear_to_colorspace_v3(color_f, img->byte_colorspace);
              }

              if (ps->dither > 0.0f) {
                float_to_byte_dither_v3(
                    projPixel->newColor.ch, color_f, ps->dither, projPixel->x_px, projPixel->y_px);
              }
              else {
                unit_float_to_uchar_clamp_v3(projPixel->newColor.ch, color_f);
              }
              projPixel->newColor.ch[3] = unit_float_to_uchar_clamp(color_f[3]);
              IMB_blend_color_byte(projPixel->pixel.ch_pt,
                                   projPixel->origColor.ch_pt,
                                   projPixel->newColor.ch,
                                   IMB_BlendMode(ps->blend));
            }
          }
          else {
            if (is_floatbuf) {
              float newColor_f[4];
              newColor_f[3] = float(projPixel->mask) * (1.0f / 65535.0f) * brush_alpha;
              copy_v3_v3(newColor_f, ps->paint_color_linear);

              IMB_blend_color_float(projPixel->pixel.f_pt,
                                    projPixel->origColor.f_pt,
                                    newColor_f,
                                    IMB_BlendMode(ps->blend));
            }
            else {
              const ProjPaintImage *img = &ps->projImages[projPixel->image_index];
              float mask = float(projPixel->mask) * (1.0f / 65535.0f);
              projPixel->newColor.ch[3] = mask * 255 * brush_alpha;

              rgb_float_to_uchar(projPixel->newColor.ch, img->paint_color_byte);
              IMB_blend_color_byte(projPixel->pixel.ch_pt,
                                   projPixel->origColor.ch_pt,
                                   projPixel->newColor.ch,
                                   IMB_BlendMode(ps->blend));
            }
          }

          if (lock_alpha) {
            copy_original_alpha_channel(projPixel, is_floatbuf);
          }

          last_partial_redraw_cell = last_projIma->partRedrawRect + projPixel->bb_cell_index;
          image_paint_partial_redraw_expand(last_partial_redraw_cell, projPixel);
        }
        else {
          if (is_floatbuf) {
            BLI_assert(ps->reproject_ibuf->float_buffer.data != nullptr);

            blender::imbuf::interpolate_cubic_bspline_fl(ps->reproject_ibuf,
                                                         projPixel->newColor.f,
                                                         projPixel->projCoSS[0],
                                                         projPixel->projCoSS[1]);
            if (projPixel->newColor.f[3]) {
              float mask = float(projPixel->mask) * (1.0f / 65535.0f);

              mul_v4_v4fl(projPixel->newColor.f, projPixel->newColor.f, mask);

              blend_color_mix_float(
                  projPixel->pixel.f_pt, projPixel->origColor.f_pt, projPixel->newColor.f);
            }
          }
          else {
            BLI_assert(ps->reproject_ibuf->byte_buffer.data != nullptr);
            blender::imbuf::interpolate_cubic_bspline_byte(ps->reproject_ibuf,
                                                           projPixel->newColor.ch,
                                                           projPixel->projCoSS[0],
                                                           projPixel->projCoSS[1]);
            if (projPixel->newColor.ch[3]) {
              float mask = float(projPixel->mask) * (1.0f / 65535.0f);
              projPixel->newColor.ch[3] *= mask;

              blend_color_mix_byte(
                  projPixel->pixel.ch_pt, projPixel->origColor.ch_pt, projPixel->newColor.ch);
            }
          }
        }
      }
    }
    else {
      /* Normal brush painting */

      for (node = ps->bucketRect[bucket_index]; node; node = node->next) {

        projPixel = (ProjPixel *)node->link;

        dist_sq = len_squared_v2v2(projPixel->projCoSS, pos);

        /* Faster alternative to `dist < radius` without a #sqrtf. */
        if (dist_sq <= brush_radius_sq) {
          dist = sqrtf(dist_sq);

          falloff = BKE_brush_curve_strength_clamped(ps->brush, dist, brush_radius);

          if (falloff > 0.0f) {
            float texrgb[3];
            float mask;

            /* Extra mask for normal, layer stencil, etc. */
            float custom_mask = float(projPixel->mask) * (1.0f / 65535.0f);

            /* Mask texture. */
            if (ps->is_maskbrush) {
              float texmask = BKE_brush_sample_masktex(
                  ps->paint, ps->brush, projPixel->projCoSS, thread_index, pool);
              CLAMP(texmask, 0.0f, 1.0f);
              custom_mask *= texmask;
            }

            /* Color texture (alpha used as mask). */
            if (ps->is_texbrush) {
              const MTex *mtex = BKE_brush_color_texture_get(brush, OB_MODE_TEXTURE_PAINT);
              float samplecos[3];
              float texrgba[4];

              /* taking 3d copy to account for 3D mapping too.
               * It gets concatenated during sampling */
              if (mtex->brush_map_mode == MTEX_MAP_MODE_3D) {
                copy_v3_v3(samplecos, projPixel->worldCoSS);
              }
              else {
                copy_v2_v2(samplecos, projPixel->projCoSS);
                samplecos[2] = 0.0f;
              }

              /* NOTE: for clone and smear,
               * we only use the alpha, could be a special function */
              BKE_brush_sample_tex_3d(
                  ps->paint, brush, mtex, samplecos, texrgba, thread_index, pool);

              copy_v3_v3(texrgb, texrgba);
              custom_mask *= texrgba[3];
            }
            else {
              zero_v3(texrgb);
            }

            if (ps->do_masking) {
              /* masking to keep brush contribution to a pixel limited. note we do not do
               * a simple max(mask, mask_accum), as this is very sensitive to spacing and
               * gives poor results for strokes crossing themselves.
               *
               * Instead we use a formula that adds up but approaches brush_alpha slowly
               * and never exceeds it, which gives nice smooth results. */
              float mask_accum = *projPixel->mask_accum;
              float max_mask = brush_alpha * custom_mask * falloff * 65535.0f;

              if (brush->flag & BRUSH_ACCUMULATE) {
                mask = mask_accum + max_mask;
              }
              else {
                mask = mask_accum + (max_mask - mask_accum * falloff);
              }

              mask = min_ff(mask, 65535.0f);
              mask_short = ushort(mask);

              if (mask_short > *projPixel->mask_accum) {
                *projPixel->mask_accum = mask_short;
                mask = mask_short * (1.0f / 65535.0f);
              }
              else {
                /* Go onto the next pixel */
                continue;
              }
            }
            else {
              mask = brush_alpha * custom_mask * falloff;
            }

            if (mask > 0.0f) {

              /* copy of code above */
              if (last_index != projPixel->image_index) {
                last_index = projPixel->image_index;
                last_projIma = projImages + last_index;

                last_projIma->touch = true;
                is_floatbuf = (last_projIma->ibuf->float_buffer.data != nullptr);
              }
              /* end copy */

              /* Validate undo tile, since we will modify it. */
              *projPixel->valid = true;

              last_partial_redraw_cell = last_projIma->partRedrawRect + projPixel->bb_cell_index;
              image_paint_partial_redraw_expand(last_partial_redraw_cell, projPixel);

              /* texrgb is not used for clone, smear or soften */
              switch (brush_type) {
                case IMAGE_PAINT_BRUSH_TYPE_CLONE:
                  if (is_floatbuf) {
                    do_projectpaint_clone_f(ps, projPixel, mask);
                  }
                  else {
                    do_projectpaint_clone(ps, projPixel, mask);
                  }
                  break;
                case IMAGE_PAINT_BRUSH_TYPE_SMEAR:
                  sub_v2_v2v2(co, projPixel->projCoSS, pos_ofs);

                  if (is_floatbuf) {
                    do_projectpaint_smear_f(ps, projPixel, mask, smearArena, &smearPixels_f, co);
                  }
                  else {
                    do_projectpaint_smear(ps, projPixel, mask, smearArena, &smearPixels, co);
                  }
                  break;
                case IMAGE_PAINT_BRUSH_TYPE_SOFTEN:
                  if (is_floatbuf) {
                    do_projectpaint_soften_f(ps, projPixel, mask, softenArena, &softenPixels_f);
                  }
                  else {
                    do_projectpaint_soften(ps, projPixel, mask, softenArena, &softenPixels);
                  }
                  break;
                case IMAGE_PAINT_BRUSH_TYPE_MASK:
                  if (is_floatbuf) {
                    do_projectpaint_mask_f(ps, projPixel, mask);
                  }
                  else {
                    do_projectpaint_mask(ps, projPixel, mask);
                  }
                  break;
                default:
                  if (is_floatbuf) {
                    do_projectpaint_draw_f(ps, projPixel, texrgb, mask);
                  }
                  else {
                    do_projectpaint_draw(
                        ps, projPixel, texrgb, mask, ps->dither, projPixel->x_px, projPixel->y_px);
                  }
                  break;
              }

              if (lock_alpha) {
                copy_original_alpha_channel(projPixel, is_floatbuf);
              }
            }

            /* done painting */
          }
        }
      }
    }
  }

  if (brush_type == IMAGE_PAINT_BRUSH_TYPE_SMEAR) {

    for (node = smearPixels; node; node = node->next) { /* this won't run for a float image */
      projPixel = static_cast<ProjPixel *>(node->link);
      *projPixel->pixel.uint_pt = ((ProjPixelClone *)projPixel)->clonepx.uint_;
      if (lock_alpha) {
        copy_original_alpha_channel(projPixel, false);
      }
    }

    for (node = smearPixels_f; node; node = node->next) {
      projPixel = static_cast<ProjPixel *>(node->link);
      copy_v4_v4(projPixel->pixel.f_pt, ((ProjPixelClone *)projPixel)->clonepx.f);
      if (lock_alpha) {
        copy_original_alpha_channel(projPixel, true);
      }
    }

    BLI_memarena_free(smearArena);
  }
  else if (brush_type == IMAGE_PAINT_BRUSH_TYPE_SOFTEN) {

    for (node = softenPixels; node; node = node->next) { /* this won't run for a float image */
      projPixel = static_cast<ProjPixel *>(node->link);
      *projPixel->pixel.uint_pt = projPixel->newColor.uint_;
      if (lock_alpha) {
        copy_original_alpha_channel(projPixel, false);
      }
    }

    for (node = softenPixels_f; node; node = node->next) {
      projPixel = static_cast<ProjPixel *>(node->link);
      copy_v4_v4(projPixel->pixel.f_pt, projPixel->newColor.f);
      if (lock_alpha) {
        copy_original_alpha_channel(projPixel, true);
      }
    }

    BLI_memarena_free(softenArena);
  }
}

static bool project_paint_op(void *state, const float lastpos[2], const float pos[2])
{
  /* First unpack args from the struct */
  ProjPaintState *ps = (ProjPaintState *)state;
  bool touch_any = false;

  ProjectHandle handles[BLENDER_MAX_THREADS];
  TaskPool *task_pool = nullptr;
  int a, i;

  ImagePool *image_pool;

  if (!project_bucket_iter_init(ps, pos)) {
    return touch_any;
  }

  if (ps->thread_tot > 1) {
    task_pool = BLI_task_pool_create_suspended(nullptr, TASK_PRIORITY_HIGH);
  }

  image_pool = BKE_image_pool_new();

  if (!ELEM(ps->source, PROJ_SRC_VIEW, PROJ_SRC_VIEW_FILL)) {
    /* This means we are reprojecting an image, make sure the image has the needed data available.
     */
    bool float_dest = false;
    bool uchar_dest = false;
    /* Check if the destination images are float or uchar. */
    for (i = 0; i < ps->image_tot; i++) {
      if (ps->projImages[i].ibuf->byte_buffer.data != nullptr) {
        uchar_dest = true;
      }
      if (ps->projImages[i].ibuf->float_buffer.data != nullptr) {
        float_dest = true;
      }
    }

    /* Generate missing data if needed. */
    if (float_dest && ps->reproject_ibuf->float_buffer.data == nullptr) {
      IMB_float_from_byte(ps->reproject_ibuf);
      ps->reproject_ibuf_free_float = true;
    }
    if (uchar_dest && ps->reproject_ibuf->byte_buffer.data == nullptr) {
      IMB_byte_from_float(ps->reproject_ibuf);
      ps->reproject_ibuf_free_uchar = true;
    }
  }

  /* get the threads running */
  for (a = 0; a < ps->thread_tot; a++) {

    /* set defaults in handles */
    // memset(&handles[a], 0, sizeof(BakeShade));

    handles[a].ps = ps;
    copy_v2_v2(handles[a].mval, pos);
    copy_v2_v2(handles[a].prevmval, lastpos);

    /* thread specific */
    handles[a].thread_index = a;

    handles[a].projImages = static_cast<ProjPaintImage *>(
        BLI_memarena_alloc(ps->arena_mt[a], ps->image_tot * sizeof(ProjPaintImage)));

    memcpy(handles[a].projImages, ps->projImages, ps->image_tot * sizeof(ProjPaintImage));

    /* image bounds */
    for (i = 0; i < ps->image_tot; i++) {
      handles[a].projImages[i].partRedrawRect = static_cast<ImagePaintPartialRedraw *>(
          BLI_memarena_alloc(ps->arena_mt[a],
                             sizeof(ImagePaintPartialRedraw) * PROJ_BOUNDBOX_SQUARED));
      memcpy(handles[a].projImages[i].partRedrawRect,
             ps->projImages[i].partRedrawRect,
             sizeof(ImagePaintPartialRedraw) * PROJ_BOUNDBOX_SQUARED);
    }

    handles[a].pool = image_pool;

    if (task_pool != nullptr) {
      BLI_task_pool_push(task_pool, do_projectpaint_thread, &handles[a], false, nullptr);
    }
  }

  if (task_pool != nullptr) { /* wait for everything to be done */
    BLI_task_pool_work_and_wait(task_pool);
    BLI_task_pool_free(task_pool);
  }
  else {
    do_projectpaint_thread(nullptr, &handles[0]);
  }

  BKE_image_pool_free(image_pool);

  /* move threaded bounds back into ps->projectPartialRedraws */
  for (i = 0; i < ps->image_tot; i++) {
    int touch = false;
    for (a = 0; a < ps->thread_tot; a++) {
      touch |= int(partial_redraw_array_merge(ps->projImages[i].partRedrawRect,
                                              handles[a].projImages[i].partRedrawRect,
                                              PROJ_BOUNDBOX_SQUARED));
    }

    if (touch) {
      ps->projImages[i].touch = true;
      touch_any = true;
    }
  }

  /* Calculate pivot for rotation around selection if needed. */
  if (U.uiflag & USER_ORBIT_SELECTION) {
    float w[3];
    int tri_index;

    tri_index = project_paint_PickFace(ps, pos, w);

    if (tri_index != -1) {
      const int3 &tri = ps->corner_tris_eval[tri_index];
      const int vert_tri[3] = {PS_CORNER_TRI_AS_VERT_INDEX_3(ps, tri)};
      float world[3];
      blender::bke::PaintRuntime *paint_runtime = ps->paint->runtime;

      interp_v3_v3v3v3(world,
                       ps->vert_positions_eval[vert_tri[0]],
                       ps->vert_positions_eval[vert_tri[1]],
                       ps->vert_positions_eval[vert_tri[2]],
                       w);

      paint_runtime->average_stroke_counter++;
      mul_m4_v3(ps->obmat, world);
      add_v3_v3(paint_runtime->average_stroke_accum, world);
      paint_runtime->last_stroke_valid = true;
    }
  }

  return touch_any;
}

static void paint_proj_stroke_ps(const bContext * /*C*/,
                                 void *ps_handle_p,
                                 const float prev_pos[2],
                                 const float pos[2],
                                 const bool eraser,
                                 float pressure,
                                 float distance,
                                 float size,
                                 /* extra view */
                                 ProjPaintState *ps)
{
  ProjStrokeHandle *ps_handle = static_cast<ProjStrokeHandle *>(ps_handle_p);
  const Paint *paint = ps->paint;
  Brush *brush = ps->brush;
  Scene *scene = ps->scene;

  ps->brush_size = size;
  ps->blend = brush->blend;
  if (eraser) {
    ps->blend = IMB_BLEND_ERASE_ALPHA;
  }

  /* handle gradient and inverted stroke color here */
  if (ELEM(ps->brush_type, IMAGE_PAINT_BRUSH_TYPE_DRAW, IMAGE_PAINT_BRUSH_TYPE_FILL)) {
    paint_brush_color_get(paint,
                          brush,
                          ps_handle->initial_hsv_jitter,
                          ps->mode == BRUSH_STROKE_INVERT,
                          distance,
                          pressure,
                          ps->paint_color_linear);

    /* Cache colorspace info per image for performance. */
    for (int i = 0; i < ps->image_tot; i++) {
      ProjPaintImage *img = &ps->projImages[i];
      const ImBuf *ibuf = img->ibuf;

      copy_v3_v3(img->paint_color_byte, ps->paint_color_linear);
      img->byte_colorspace = nullptr;
      img->is_data = false;
      img->is_srgb = false;

      if (ibuf->colormanage_flag & IMB_COLORMANAGE_IS_DATA) {
        img->is_data = true;
      }
      else if (ibuf->byte_buffer.data && ibuf->byte_buffer.colorspace) {
        img->byte_colorspace = ibuf->byte_buffer.colorspace;
        img->is_srgb = IMB_colormanagement_space_is_srgb(img->byte_colorspace);
        if (img->is_srgb) {
          IMB_colormanagement_scene_linear_to_srgb_v3(img->paint_color_byte,
                                                      img->paint_color_byte);
        }
        else {
          IMB_colormanagement_scene_linear_to_colorspace_v3(img->paint_color_byte,
                                                            img->byte_colorspace);
        }
      }
    }
  }
  else if (ps->brush_type == IMAGE_PAINT_BRUSH_TYPE_MASK) {
    ps->stencil_value = brush->weight;

    if ((ps->mode == BRUSH_STROKE_INVERT) ^
        ((scene->toolsettings->imapaint.flag & IMAGEPAINT_PROJECT_LAYER_STENCIL_INV) != 0))
    {
      ps->stencil_value = 1.0f - ps->stencil_value;
    }
  }

  if (project_paint_op(ps, prev_pos, pos)) {
    ps_handle->need_redraw = true;
    project_image_refresh_tagged(ps);
  }
}

void paint_proj_stroke(const bContext *C,
                       void *ps_handle_p,
                       const float prev_pos[2],
                       const float pos[2],
                       const bool eraser,
                       float pressure,
                       float distance,
                       float size)
{
  int i;
  ProjStrokeHandle *ps_handle = static_cast<ProjStrokeHandle *>(ps_handle_p);

  /* clone gets special treatment here to avoid going through image initialization */
  if (ps_handle->is_clone_cursor_pick) {
    Scene *scene = ps_handle->scene;
    Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
    View3D *v3d = CTX_wm_view3d(C);
    ARegion *region = CTX_wm_region(C);
    float *cursor = scene->cursor.location;
    const int mval_i[2] = {int(pos[0]), int(pos[1])};

    view3d_operator_needs_gpu(C);

    /* Ensure the depth buffer is updated for #ED_view3d_autodist. */
    ED_view3d_depth_override(
        depsgraph, region, v3d, nullptr, V3D_DEPTH_NO_GPENCIL, false, nullptr);

    if (!ED_view3d_autodist(region, v3d, mval_i, cursor, nullptr)) {
      return;
    }

    DEG_id_tag_update(&scene->id, ID_RECALC_SYNC_TO_EVAL);
    ED_region_tag_redraw(region);

    return;
  }

  for (i = 0; i < ps_handle->ps_views_tot; i++) {
    ProjPaintState *ps = ps_handle->ps_views[i];
    paint_proj_stroke_ps(C, ps_handle_p, prev_pos, pos, eraser, pressure, distance, size, ps);
  }
}

/* initialize project paint settings from context */
static void project_state_init(bContext *C, Object *ob, ProjPaintState *ps, int mode)
{
  Scene *scene = CTX_data_scene(C);
  ToolSettings *settings = scene->toolsettings;

  /* brush */
  ps->mode = BrushStrokeMode(mode);
  ps->paint = BKE_paint_get_active_from_context(C);
  ps->brush = BKE_paint_brush(&settings->imapaint.paint);
  if (ps->brush) {
    Brush *brush = ps->brush;
    ps->brush_type = brush->image_brush_type;
    ps->blend = brush->blend;
    if (mode == BRUSH_STROKE_SMOOTH) {
      ps->brush_type = IMAGE_PAINT_BRUSH_TYPE_SOFTEN;
    }
    /* only check for inversion for the soften brush, elsewhere,
     * a resident brush inversion flag can cause issues */
    if (ps->brush_type == IMAGE_PAINT_BRUSH_TYPE_SOFTEN) {
      ps->mode = (((ps->mode == BRUSH_STROKE_INVERT) ^ ((brush->flag & BRUSH_DIR_IN) != 0)) ?
                      BRUSH_STROKE_INVERT :
                      BRUSH_STROKE_NORMAL);

      ps->blurkernel = paint_new_blur_kernel(brush, true);
    }

    /* disable for 3d mapping also because painting on mirrored mesh can create "stripes" */
    ps->do_masking = paint_use_opacity_masking(ps->paint, brush);
    ps->is_texbrush = (brush->mtex.tex && ps->brush_type == IMAGE_PAINT_BRUSH_TYPE_DRAW) ? true :
                                                                                           false;
    ps->is_maskbrush = (brush->mask_mtex.tex) ? true : false;
  }
  else {
    /* Brush may be nullptr. */
    ps->do_masking = false;
    ps->is_texbrush = false;
    ps->is_maskbrush = false;
  }

  /* sizeof(ProjPixel), since we alloc this a _lot_ */
  ps->pixel_sizeof = project_paint_pixel_sizeof(ps->brush_type);
  BLI_assert(ps->pixel_sizeof >= sizeof(ProjPixel));

  /* these can be nullptr */
  ps->v3d = CTX_wm_view3d(C);
  ps->rv3d = CTX_wm_region_view3d(C);
  ps->region = CTX_wm_region(C);

  ps->depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  ps->scene = scene;
  /* allow override of active object */
  ps->ob = ob;

  ps->do_material_slots = (settings->imapaint.mode == IMAGEPAINT_MODE_MATERIAL);
  ps->stencil_ima = settings->imapaint.stencil;
  ps->canvas_ima = (!ps->do_material_slots) ? settings->imapaint.canvas : nullptr;
  ps->clone_ima = (!ps->do_material_slots) ? settings->imapaint.clone : nullptr;

  ps->do_mask_cavity = (settings->imapaint.paint.flags & PAINT_USE_CAVITY_MASK);
  ps->cavity_curve = settings->imapaint.paint.cavity_curve;

  /* setup projection painting data */
  if (ps->brush_type != IMAGE_PAINT_BRUSH_TYPE_FILL) {
    ps->do_backfacecull = !(settings->imapaint.flag & IMAGEPAINT_PROJECT_BACKFACE);
    ps->do_occlude = !(settings->imapaint.flag & IMAGEPAINT_PROJECT_XRAY);
    ps->do_mask_normal = !(settings->imapaint.flag & IMAGEPAINT_PROJECT_FLAT);
  }
  else {
    ps->do_backfacecull = ps->do_occlude = ps->do_mask_normal = false;
  }

  if (ps->brush_type == IMAGE_PAINT_BRUSH_TYPE_CLONE) {
    ps->do_layer_clone = (settings->imapaint.flag & IMAGEPAINT_PROJECT_LAYER_CLONE);
  }

  ps->do_stencil_brush = (ps->brush_type == IMAGE_PAINT_BRUSH_TYPE_MASK);
  /* deactivate stenciling for the stencil brush :) */
  ps->do_layer_stencil = ((settings->imapaint.flag & IMAGEPAINT_PROJECT_LAYER_STENCIL) &&
                          !(ps->do_stencil_brush) && ps->stencil_ima);
  ps->do_layer_stencil_inv = ((settings->imapaint.flag & IMAGEPAINT_PROJECT_LAYER_STENCIL_INV) !=
                              0);

#ifndef PROJ_DEBUG_NOSEAMBLEED
  /* pixel num to bleed */
  ps->seam_bleed_px = settings->imapaint.seam_bleed;
  ps->seam_bleed_px_sq = square_s(settings->imapaint.seam_bleed);
#endif

  if (ps->do_mask_normal) {
    ps->normal_angle_inner = settings->imapaint.normal_angle;
    ps->normal_angle = (ps->normal_angle_inner + 90.0f) * 0.5f;
  }
  else {
    ps->normal_angle_inner = ps->normal_angle = settings->imapaint.normal_angle;
  }

  ps->normal_angle_inner *= float(M_PI_2 / 90);
  ps->normal_angle *= float(M_PI_2 / 90);
  ps->normal_angle_range = ps->normal_angle - ps->normal_angle_inner;

  if (ps->normal_angle_range <= 0.0f) {
    /* no need to do blending */
    ps->do_mask_normal = false;
  }

  ps->normal_angle__cos = cosf(ps->normal_angle);
  ps->normal_angle_inner__cos = cosf(ps->normal_angle_inner);

  ps->dither = settings->imapaint.dither;
}

void *paint_proj_new_stroke(bContext *C, Object *ob, const float mouse[2], int mode)
{
  ProjStrokeHandle *ps_handle;
  Scene *scene = CTX_data_scene(C);
  ToolSettings *settings = scene->toolsettings;
  char symmetry_flag_views[BOUNDED_ARRAY_TYPE_SIZE<decltype(ps_handle->ps_views)>()] = {0};

  ps_handle = MEM_new<ProjStrokeHandle>("ProjStrokeHandle");
  ps_handle->scene = scene;
  ps_handle->paint = BKE_paint_get_active_from_context(C);
  ps_handle->brush = BKE_paint_brush(&settings->imapaint.paint);

  if (BKE_brush_color_jitter_get_settings(&settings->imapaint.paint, ps_handle->brush)) {
    ps_handle->initial_hsv_jitter = seed_hsv_jitter();
  }

  if (mode == BRUSH_STROKE_INVERT) {
    /* Bypass regular stroke logic. */
    if (ps_handle->brush->image_brush_type == IMAGE_PAINT_BRUSH_TYPE_CLONE) {
      view3d_operator_needs_gpu(C);
      ps_handle->is_clone_cursor_pick = true;
      return ps_handle;
    }
  }

  ps_handle->orig_brush_size = BKE_brush_size_get(ps_handle->paint, ps_handle->brush);

  Mesh *mesh = BKE_mesh_from_object(ob);
  ps_handle->symmetry_flags = mesh->symmetry;
  ps_handle->ps_views_tot = 1 + (pow_i(2, count_bits_i(ps_handle->symmetry_flags)) - 1);
  bool is_multi_view = (ps_handle->ps_views_tot != 1);

  for (int i = 0; i < ps_handle->ps_views_tot; i++) {
    ProjPaintState *ps = MEM_new<ProjPaintState>("ProjectionPaintState");
    ps_handle->ps_views[i] = ps;
  }

  if (ps_handle->symmetry_flags) {
    int index = 0;

    int x = 0;
    do {
      int y = 0;
      do {
        int z = 0;
        do {
          symmetry_flag_views[index++] = ((x ? PAINT_SYMM_X : 0) | (y ? PAINT_SYMM_Y : 0) |
                                          (z ? PAINT_SYMM_Z : 0));
          BLI_assert(index <= ps_handle->ps_views_tot);
        } while ((z++ == 0) && (ps_handle->symmetry_flags & PAINT_SYMM_Z));
      } while ((y++ == 0) && (ps_handle->symmetry_flags & PAINT_SYMM_Y));
    } while ((x++ == 0) && (ps_handle->symmetry_flags & PAINT_SYMM_X));
    BLI_assert(index == ps_handle->ps_views_tot);
  }

  for (int i = 0; i < ps_handle->ps_views_tot; i++) {
    ProjPaintState *ps = ps_handle->ps_views[i];

    project_state_init(C, ob, ps, mode);

    if (ps->ob == nullptr) {
      ps_handle->ps_views_tot = i + 1;
      goto fail;
    }
  }

  /* TODO: Inspect this further. */
  /* Don't allow brush size below 2 */
  if (BKE_brush_size_get(&settings->imapaint.paint, ps_handle->brush) < 2) {
    BKE_brush_size_set(&settings->imapaint.paint, ps_handle->brush, 2 * U.pixelsize);
  }

  /* allocate and initialize spatial data structures */

  for (int i = 0; i < ps_handle->ps_views_tot; i++) {
    ProjPaintState *ps = ps_handle->ps_views[i];

    ps->source = (ps->brush_type == IMAGE_PAINT_BRUSH_TYPE_FILL) ? PROJ_SRC_VIEW_FILL :
                                                                   PROJ_SRC_VIEW;
    project_image_refresh_tagged(ps);

    /* re-use! */
    if (i != 0) {
      ps->is_shared_user = true;
      PROJ_PAINT_STATE_SHARED_MEMCPY(ps, ps_handle->ps_views[0]);
    }

    project_paint_begin(C, ps, is_multi_view, symmetry_flag_views[i]);
    if (ps->mesh_eval == nullptr) {
      goto fail;
    }

    paint_proj_begin_clone(ps, mouse);
  }

  paint_brush_init_tex(ps_handle->brush);

  return ps_handle;

fail:
  for (int i = 0; i < ps_handle->ps_views_tot; i++) {
    MEM_delete(ps_handle->ps_views[i]);
  }
  MEM_delete(ps_handle);
  return nullptr;
}

void paint_proj_redraw(const bContext *C, void *ps_handle_p, bool final)
{
  ProjStrokeHandle *ps_handle = static_cast<ProjStrokeHandle *>(ps_handle_p);

  if (ps_handle->need_redraw) {
    ps_handle->need_redraw = false;
  }
  else if (!final) {
    return;
  }

  if (final) {
    /* compositor listener deals with updating */
    WM_event_add_notifier(C, NC_IMAGE | NA_EDITED, nullptr);
  }
  else {
    ED_region_tag_redraw(CTX_wm_region(C));
  }
}

void paint_proj_stroke_done(void *ps_handle_p)
{
  ProjStrokeHandle *ps_handle = static_cast<ProjStrokeHandle *>(ps_handle_p);

  if (ps_handle->is_clone_cursor_pick) {
    MEM_delete(ps_handle);
    return;
  }

  for (int i = 1; i < ps_handle->ps_views_tot; i++) {
    PROJ_PAINT_STATE_SHARED_CLEAR(ps_handle->ps_views[i]);
  }

  BKE_brush_size_set(ps_handle->paint, ps_handle->brush, ps_handle->orig_brush_size);

  paint_brush_exit_tex(ps_handle->brush);

  for (int i = 0; i < ps_handle->ps_views_tot; i++) {
    ProjPaintState *ps;
    ps = ps_handle->ps_views[i];
    project_paint_end(ps);
    MEM_delete(ps);
  }

  MEM_delete(ps_handle);
}
/* use project paint to re-apply an image */
static wmOperatorStatus texture_paint_camera_project_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Image *image = static_cast<Image *>(
      BLI_findlink(&bmain->images, RNA_enum_get(op->ptr, "image")));
  Scene &scene = *CTX_data_scene(C);
  ViewLayer &view_layer = *CTX_data_view_layer(C);
  ProjPaintState ps = {nullptr};
  int orig_brush_size;
  IDProperty *idgroup;
  IDProperty *view_data = nullptr;
  BKE_view_layer_synced_ensure(&scene, &view_layer);
  Object *ob = BKE_view_layer_active_object_get(&view_layer);
  bool uvs, mat, tex;

  if (ob == nullptr || ob->type != OB_MESH) {
    BKE_report(op->reports, RPT_ERROR, "No active mesh object");
    return OPERATOR_CANCELLED;
  }

  if (!ED_paint_proj_mesh_data_check(scene, *ob, &uvs, &mat, &tex, nullptr)) {
    ED_paint_data_warning(op->reports, uvs, mat, tex, true);
    WM_event_add_notifier(C, NC_SCENE | ND_TOOLSETTINGS, nullptr);
    return OPERATOR_CANCELLED;
  }

  project_state_init(C, ob, &ps, BRUSH_STROKE_NORMAL);

  if (image == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "Image could not be found");
    return OPERATOR_CANCELLED;
  }

  ps.reproject_image = image;
  ps.reproject_ibuf = BKE_image_acquire_ibuf(image, nullptr, nullptr);

  if ((ps.reproject_ibuf == nullptr) ||
      ((ps.reproject_ibuf->byte_buffer.data || ps.reproject_ibuf->float_buffer.data) == false))
  {
    BKE_report(op->reports, RPT_ERROR, "Image data could not be found");
    return OPERATOR_CANCELLED;
  }

  idgroup = IDP_GetProperties(&image->id);

  if (idgroup) {
    view_data = IDP_GetPropertyTypeFromGroup(idgroup, PROJ_VIEW_DATA_ID, IDP_ARRAY);

    /* type check to make sure its ok */
    if (view_data != nullptr &&
        (view_data->len != PROJ_VIEW_DATA_SIZE || view_data->subtype != IDP_FLOAT))
    {
      BKE_report(op->reports, RPT_ERROR, "Image project data invalid");
      return OPERATOR_CANCELLED;
    }
  }

  if (view_data) {
    /* image has stored view projection info */
    ps.source = PROJ_SRC_IMAGE_VIEW;
  }
  else {
    ps.source = PROJ_SRC_IMAGE_CAM;

    if (scene.camera == nullptr) {
      BKE_report(op->reports, RPT_ERROR, "No active camera set");
      return OPERATOR_CANCELLED;
    }
  }

  /* override */
  ps.is_texbrush = false;
  ps.is_maskbrush = false;
  ps.do_masking = false;
  orig_brush_size = BKE_brush_size_get(ps.paint, ps.brush);
  /* cover the whole image */
  BKE_brush_size_set(ps.paint, ps.brush, 32 * U.pixelsize);

  /* so pixels are initialized with minimal info */
  ps.brush_type = IMAGE_PAINT_BRUSH_TYPE_DRAW;

  scene.toolsettings->imapaint.flag |= IMAGEPAINT_DRAWING;

  /* allocate and initialize spatial data structures */
  project_paint_begin(C, &ps, false, 0);

  if (ps.mesh_eval == nullptr) {
    BKE_brush_size_set(ps.paint, ps.brush, orig_brush_size);
    BKE_report(op->reports, RPT_ERROR, "Could not get valid evaluated mesh");
    return OPERATOR_CANCELLED;
  }

  ED_image_undo_push_begin(op->type->name, PaintMode::Texture3D);

  const float pos[2] = {0.0, 0.0};
  const float lastpos[2] = {0.0, 0.0};
  int a;

  project_paint_op(&ps, lastpos, pos);

  project_image_refresh_tagged(&ps);

  for (a = 0; a < ps.image_tot; a++) {
    BKE_image_free_gputextures(ps.projImages[a].ima);
    WM_event_add_notifier(C, NC_IMAGE | NA_EDITED, ps.projImages[a].ima);
  }

  project_paint_end(&ps);

  ED_image_undo_push_end();

  scene.toolsettings->imapaint.flag &= ~IMAGEPAINT_DRAWING;
  BKE_brush_size_set(ps.paint, ps.brush, orig_brush_size);

  return OPERATOR_FINISHED;
}

void PAINT_OT_project_image(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Project Image";
  ot->idname = "PAINT_OT_project_image";
  ot->description = "Project an edited render from the active camera back onto the object";

  /* API callbacks. */
  ot->invoke = WM_enum_search_invoke;
  ot->exec = texture_paint_camera_project_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  prop = RNA_def_enum(ot->srna, "image", rna_enum_dummy_NULL_items, 0, "Image", "");
  RNA_def_enum_funcs(prop, RNA_image_itemf);
  RNA_def_property_flag(prop, PROP_ENUM_NO_TRANSLATE);
  ot->prop = prop;
}

static bool texture_paint_image_from_view_poll(bContext *C)
{
  bScreen *screen = CTX_wm_screen(C);
  if (!(screen && BKE_screen_find_big_area(screen, SPACE_VIEW3D, 0))) {
    CTX_wm_operator_poll_msg_set(C, "No 3D viewport found to create image from");
    return false;
  }
  if (G.background || !GPU_is_init()) {
    return false;
  }
  return true;
}

static wmOperatorStatus texture_paint_image_from_view_exec(bContext *C, wmOperator *op)
{
  using namespace blender;
  Image *image;
  ImBuf *ibuf;
  char filepath[FILE_MAX];

  Main *bmain = CTX_data_main(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  ToolSettings *settings = scene->toolsettings;
  int w = settings->imapaint.screen_grab_size[0];
  int h = settings->imapaint.screen_grab_size[1];
  int maxsize;
  char err_out[256] = "unknown";

  ScrArea *area = BKE_screen_find_big_area(CTX_wm_screen(C), SPACE_VIEW3D, 0);
  if (!area) {
    BKE_report(op->reports, RPT_ERROR, "No 3D viewport found to create image from");
    return OPERATOR_CANCELLED;
  }

  ARegion *region = BKE_area_find_region_active_win(area);
  if (!region) {
    BKE_report(op->reports, RPT_ERROR, "No 3D viewport found to create image from");
    return OPERATOR_CANCELLED;
  }
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);

  RNA_string_get(op->ptr, "filepath", filepath);

  maxsize = GPU_max_texture_size();

  w = std::min(w, maxsize);
  h = std::min(h, maxsize);

  /* Create a copy of the overlays where they are all turned off, except the
   * texture paint overlay opacity */
  View3D *v3d = static_cast<View3D *>(area->spacedata.first);
  View3D v3d_copy = blender::dna::shallow_copy(*v3d);
  v3d_copy.gridflag = 0;
  v3d_copy.flag2 = 0;
  v3d_copy.flag = V3D_HIDE_HELPLINES;
  v3d_copy.gizmo_flag = V3D_GIZMO_HIDE;

  memset(&v3d_copy.overlay, 0, sizeof(View3DOverlay));
  v3d_copy.overlay.flag = V3D_OVERLAY_HIDE_CURSOR | V3D_OVERLAY_HIDE_TEXT |
                          V3D_OVERLAY_HIDE_MOTION_PATHS | V3D_OVERLAY_HIDE_BONES |
                          V3D_OVERLAY_HIDE_OBJECT_XTRAS | V3D_OVERLAY_HIDE_OBJECT_ORIGINS;
  v3d_copy.overlay.texture_paint_mode_opacity = v3d->overlay.texture_paint_mode_opacity;

  ibuf = ED_view3d_draw_offscreen_imbuf(depsgraph,
                                        scene,
                                        eDrawType(v3d_copy.shading.type),
                                        &v3d_copy,
                                        region,
                                        w,
                                        h,
                                        IB_byte_data,
                                        R_ALPHAPREMUL,
                                        nullptr,
                                        false,
                                        nullptr,
                                        nullptr,
                                        err_out);

  if (!ibuf) {
    /* NOTE(@sergey): Mostly happens when OpenGL off-screen buffer was failed to create, */
    /* but could be other reasons. Should be handled in the future. */
    BKE_reportf(op->reports, RPT_ERROR, "Failed to create OpenGL off-screen buffer: %s", err_out);
    return OPERATOR_CANCELLED;
  }

  STRNCPY(ibuf->filepath, filepath);

  image = BKE_image_add_from_imbuf(bmain, ibuf, "image_view");

  /* Drop reference to ibuf so that the image owns it */
  IMB_freeImBuf(ibuf);

  if (image) {
    /* now for the trickiness. store the view projection here!
     * re-projection will reuse this */
    IDProperty *idgroup = IDP_EnsureProperties(&image->id);

    blender::Vector<float, PROJ_VIEW_DATA_SIZE> array;
    array.extend(Span(reinterpret_cast<float *>(rv3d->winmat), 16));
    array.extend(Span(reinterpret_cast<float *>(rv3d->viewmat), 16));
    float clip_start;
    float clip_end;
    const bool is_ortho = ED_view3d_clip_range_get(
        depsgraph, v3d, rv3d, true, &clip_start, &clip_end);
    array.append(clip_start);
    array.append(clip_end);
    /* using float for a bool is dodgy but since its an extra member in the array...
     * easier than adding a single bool prop */
    array.append(is_ortho ? 1.0f : 0.0f);
    IDP_AddToGroup(idgroup, bke::idprop::create(PROJ_VIEW_DATA_ID, array.as_span()).release());
  }

  return OPERATOR_FINISHED;
}

void PAINT_OT_image_from_view(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Image from View";
  ot->idname = "PAINT_OT_image_from_view";
  ot->description = "Make an image from biggest 3D view for reprojection";

  /* API callbacks. */
  ot->exec = texture_paint_image_from_view_exec;
  ot->poll = texture_paint_image_from_view_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER;

  RNA_def_string_file_name(
      ot->srna, "filepath", nullptr, FILE_MAX, "File Path", "Name of the file");
}

/*********************************************
 * Data generation for projective texturing  *
 * *******************************************/

void ED_paint_data_warning(
    ReportList *reports, bool has_uvs, bool has_mat, bool has_tex, bool has_stencil)
{
  BKE_reportf(reports,
              RPT_WARNING,
              "Missing%s%s%s%s detected!",
              !has_uvs ? RPT_(" UVs,") : "",
              !has_mat ? RPT_(" Materials,") : "",
              !has_tex ? RPT_(" Textures (or linked),") : "",
              !has_stencil ? RPT_(" Stencil,") : "");
}

bool ED_paint_proj_mesh_data_check(Scene &scene,
                                   Object &ob,
                                   bool *r_has_uvs,
                                   bool *r_has_mat,
                                   bool *r_has_tex,
                                   bool *r_has_stencil)
{
  ImagePaintSettings &imapaint = scene.toolsettings->imapaint;
  const Brush *br = BKE_paint_brush(&imapaint.paint);
  bool has_mat = true;
  bool has_tex = true;
  bool has_stencil = true;
  bool has_uvs = true;

  imapaint.missing_data = 0;

  BLI_assert(ob.type == OB_MESH);

  if (imapaint.mode == IMAGEPAINT_MODE_MATERIAL) {
    /* no material, add one */
    if (ob.totcol == 0) {
      has_mat = false;
      has_tex = false;
    }
    else {
      /* there may be material slots but they may be empty, check */
      has_mat = false;
      has_tex = false;

      for (int i = 1; i < ob.totcol + 1; i++) {
        Material *ma = BKE_object_material_get(&ob, i);

        if (ma && ID_IS_EDITABLE(ma) && !ID_IS_OVERRIDE_LIBRARY(ma)) {
          has_mat = true;
          if (ma->texpaintslot == nullptr) {
            /* refresh here just in case */
            BKE_texpaint_slot_refresh_cache(&scene, ma, &ob);
          }
          if (ma->texpaintslot != nullptr &&
              ma->texpaintslot[ma->paint_active_slot].ima != nullptr &&
              ID_IS_EDITABLE(ma->texpaintslot[ma->paint_active_slot].ima) &&
              !ID_IS_OVERRIDE_LIBRARY(ma->texpaintslot[ma->paint_active_slot].ima))
          {
            has_tex = true;
            break;
          }
        }
      }
    }
  }
  else if (imapaint.mode == IMAGEPAINT_MODE_IMAGE) {
    if (imapaint.canvas == nullptr || !ID_IS_EDITABLE(imapaint.canvas)) {
      has_tex = false;
    }
  }

  Mesh *mesh = BKE_mesh_from_object(&ob);
  int layernum = mesh->uv_map_names().size();

  if (layernum == 0) {
    has_uvs = false;
  }

  /* Make sure we have a stencil to paint on! */
  if (br && br->image_brush_type == IMAGE_PAINT_BRUSH_TYPE_MASK) {
    imapaint.flag |= IMAGEPAINT_PROJECT_LAYER_STENCIL;

    if (imapaint.stencil == nullptr) {
      has_stencil = false;
    }
  }

  if (!has_uvs) {
    imapaint.missing_data |= IMAGEPAINT_MISSING_UVS;
  }
  if (!has_mat) {
    imapaint.missing_data |= IMAGEPAINT_MISSING_MATERIAL;
  }
  if (!has_tex) {
    imapaint.missing_data |= IMAGEPAINT_MISSING_TEX;
  }
  if (!has_stencil) {
    imapaint.missing_data |= IMAGEPAINT_MISSING_STENCIL;
  }

  if (r_has_uvs) {
    *r_has_uvs = has_uvs;
  }
  if (r_has_mat) {
    *r_has_mat = has_mat;
  }
  if (r_has_tex) {
    *r_has_tex = has_tex;
  }
  if (r_has_stencil) {
    *r_has_stencil = has_stencil;
  }

  return has_uvs && has_mat && has_tex && has_stencil;
}

/* Add layer operator */
enum {
  LAYER_BASE_COLOR,
  LAYER_SPECULAR,
  LAYER_ROUGHNESS,
  LAYER_METALLIC,
  LAYER_NORMAL,
  LAYER_BUMP,
  LAYER_DISPLACEMENT,
};

static const EnumPropertyItem layer_type_items[] = {
    {LAYER_BASE_COLOR, "BASE_COLOR", 0, "Base Color", ""},
    {LAYER_SPECULAR, "SPECULAR", 0, "Specular IOR Level", ""},
    {LAYER_ROUGHNESS, "ROUGHNESS", 0, "Roughness", ""},
    {LAYER_METALLIC, "METALLIC", 0, "Metallic", ""},
    {LAYER_NORMAL, "NORMAL", 0, "Normal", ""},
    {LAYER_BUMP, "BUMP", 0, "Bump", ""},
    {LAYER_DISPLACEMENT, "DISPLACEMENT", 0, "Displacement", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static Material *get_or_create_current_material(bContext *C, Object *ob)
{
  Material *ma = BKE_object_material_get(ob, ob->actcol);
  if (!ma) {
    Main *bmain = CTX_data_main(C);
    ma = BKE_material_add(bmain, "Material");
    BKE_object_material_assign(bmain, ob, ma, ob->actcol, BKE_MAT_ASSIGN_USERPREF);
  }
  return ma;
}

static Image *proj_paint_image_create(wmOperator *op, Main *bmain, bool is_data)
{
  Image *ima;
  float color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
  char imagename[MAX_ID_NAME - 2] = "Material Diffuse Color";
  int width = 1024;
  int height = 1024;
  bool use_float = false;
  short gen_type = IMA_GENTYPE_BLANK;
  bool alpha = false;

  if (op) {
    width = RNA_int_get(op->ptr, "width");
    height = RNA_int_get(op->ptr, "height");
    use_float = RNA_boolean_get(op->ptr, "float");
    gen_type = RNA_enum_get(op->ptr, "generated_type");
    RNA_float_get_array(op->ptr, "color", color);
    alpha = RNA_boolean_get(op->ptr, "alpha");
    RNA_string_get(op->ptr, "name", imagename);
  }

  if (!alpha) {
    color[3] = 1.0f;
  }

  /* TODO(lukas): Add option for tiled image. */
  ima = BKE_image_add_generated(bmain,
                                width,
                                height,
                                imagename,
                                alpha ? 32 : 24,
                                use_float,
                                gen_type,
                                color,
                                false,
                                is_data,
                                false);

  return ima;
}

/**
 * \return The name of the new attribute.
 */
static std::optional<std::string> proj_paint_color_attribute_create(wmOperator *op, Object &ob)
{
  using namespace blender;
  char name[MAX_NAME] = "";
  float color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
  bke::AttrDomain domain = bke::AttrDomain::Point;
  eCustomDataType type = CD_PROP_COLOR;

  if (op) {
    RNA_string_get(op->ptr, "name", name);
    RNA_float_get_array(op->ptr, "color", color);
    domain = bke::AttrDomain(RNA_enum_get(op->ptr, "domain"));
    type = eCustomDataType(RNA_enum_get(op->ptr, "data_type"));
  }

  Mesh *mesh = static_cast<Mesh *>(ob.data);
  AttributeOwner owner = AttributeOwner::from_id(&mesh->id);
  std::string unique_name = BKE_attribute_calc_unique_name(owner, name);
  bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  bke::GSpanAttributeWriter attr = attributes.lookup_or_add_for_write_span(
      unique_name, domain, *bke::custom_data_type_to_attr_type(type));
  if (!attr) {
    return std::nullopt;
  }

  BKE_id_attributes_active_color_set(&mesh->id, unique_name);
  if (!mesh->default_color_attribute) {
    BKE_id_attributes_default_color_set(&mesh->id, unique_name);
  }

  ed::sculpt_paint::object_active_color_fill(ob, color, false);

  return unique_name;
}

/**
 * Get a default color for the paint slot layer from a material's Principled BSDF.
 *
 * \param layer_type: The layer type of the paint slot
 * \param ma: The material to attempt using as the default color source.
 *            If this fails or \p ma is null, a default Principled BSDF is used instead.
 */
static void default_paint_slot_color_get(int layer_type, Material *ma, float color[4])
{
  switch (layer_type) {
    case LAYER_BASE_COLOR:
    case LAYER_SPECULAR:
    case LAYER_ROUGHNESS:
    case LAYER_METALLIC: {
      bNodeTree *ntree = nullptr;
      bNode *in_node = nullptr;
      if (ma && ma->nodetree) {
        ma->nodetree->ensure_topology_cache();
        const blender::Span<bNode *> nodes = ma->nodetree->nodes_by_type(
            "ShaderNodeBsdfPrincipled");
        in_node = nodes.is_empty() ? nullptr : nodes.first();
      }
      if (!in_node) {
        /* An existing material or Principled BSDF node could not be found.
         * Copy default color values from a default Principled BSDF instead. */
        ntree = blender::bke::node_tree_add_tree(
            nullptr, "Temporary Shader Nodetree", ntreeType_Shader->idname);
        in_node = blender::bke::node_add_static_node(nullptr, *ntree, SH_NODE_BSDF_PRINCIPLED);
      }
      bNodeSocket *in_sock = blender::bke::node_find_socket(
          *in_node, SOCK_IN, layer_type_items[layer_type].name);
      switch (in_sock->type) {
        case SOCK_FLOAT: {
          bNodeSocketValueFloat *socket_data = static_cast<bNodeSocketValueFloat *>(
              in_sock->default_value);
          copy_v3_fl(color, socket_data->value);
          color[3] = 1.0f;
          break;
        }
        case SOCK_VECTOR:
        case SOCK_RGBA: {
          bNodeSocketValueRGBA *socket_data = static_cast<bNodeSocketValueRGBA *>(
              in_sock->default_value);
          copy_v3_v3(color, socket_data->value);
          color[3] = 1.0f;
          break;
        }
        default:
          BLI_assert_unreachable();
          rgba_float_args_set(color, 0.0f, 0.0f, 0.0f, 1.0f);
          break;
      }
      /* Cleanup */
      if (ntree) {
        blender::bke::node_tree_free_tree(*ntree);
        MEM_freeN(ntree);
      }
      return;
    }
    case LAYER_NORMAL:
      /* Neutral tangent space normal map. */
      rgba_float_args_set(color, 0.5f, 0.5f, 1.0f, 1.0f);
      break;
    case LAYER_BUMP:
    case LAYER_DISPLACEMENT:
      /* Neutral displacement and bump map. */
      rgba_float_args_set(color, 0.5f, 0.5f, 0.5f, 1.0f);
      break;
  }
}

static bool proj_paint_add_slot(bContext *C, wmOperator *op)
{
  Object *ob = blender::ed::object::context_active_object(C);
  Scene *scene = CTX_data_scene(C);
  Material *ma;
  Image *ima = nullptr;
  CustomDataLayer *layer = nullptr;

  if (!ob) {
    return false;
  }

  ma = get_or_create_current_material(C, ob);

  if (ma) {
    Main *bmain = CTX_data_main(C);
    int type = RNA_enum_get(op->ptr, "type");
    bool is_data = (type > LAYER_BASE_COLOR);

    bNode *new_node;
    bNodeTree *ntree = ma->nodetree;

    if (!ntree) {
      ED_node_shader_default(C, bmain, &ma->id);
      ntree = ma->nodetree;
    }

    const ePaintCanvasSource slot_type = ob->mode == OB_MODE_SCULPT ?
                                             (ePaintCanvasSource)RNA_enum_get(op->ptr,
                                                                              "slot_type") :
                                             PAINT_CANVAS_SOURCE_IMAGE;

    /* Create a new node. */
    switch (slot_type) {
      case PAINT_CANVAS_SOURCE_IMAGE: {
        new_node = blender::bke::node_add_static_node(C, *ntree, SH_NODE_TEX_IMAGE);
        ima = proj_paint_image_create(op, bmain, is_data);
        new_node->id = &ima->id;
        break;
      }
      case PAINT_CANVAS_SOURCE_COLOR_ATTRIBUTE: {
        new_node = blender::bke::node_add_static_node(C, *ntree, SH_NODE_ATTRIBUTE);
        if (const std::optional<std::string> name = proj_paint_color_attribute_create(op, *ob)) {
          STRNCPY_UTF8(((NodeShaderAttribute *)new_node->storage)->name, name->c_str());
        }
        break;
      }
      case PAINT_CANVAS_SOURCE_MATERIAL:
        BLI_assert_unreachable();
        return false;
    }
    blender::bke::node_set_active(*ntree, *new_node);

    /* Connect to first available principled BSDF node. */
    ntree->ensure_topology_cache();
    const blender::Span<bNode *> bsdf_nodes = ntree->nodes_by_type("ShaderNodeBsdfPrincipled");
    bNode *in_node = bsdf_nodes.is_empty() ? nullptr : bsdf_nodes.first();
    bNode *out_node = new_node;

    if (in_node != nullptr) {
      bNodeSocket *out_sock = blender::bke::node_find_socket(*out_node, SOCK_OUT, "Color");
      bNodeSocket *in_sock = nullptr;

      if (type >= LAYER_BASE_COLOR && type < LAYER_NORMAL) {
        in_sock = blender::bke::node_find_socket(*in_node, SOCK_IN, layer_type_items[type].name);
      }
      else if (type == LAYER_NORMAL) {
        bNode *nor_node;
        nor_node = blender::bke::node_add_static_node(C, *ntree, SH_NODE_NORMAL_MAP);

        in_sock = blender::bke::node_find_socket(*nor_node, SOCK_IN, "Color");
        blender::bke::node_add_link(*ntree, *out_node, *out_sock, *nor_node, *in_sock);

        in_sock = blender::bke::node_find_socket(*in_node, SOCK_IN, "Normal");
        out_sock = blender::bke::node_find_socket(*nor_node, SOCK_OUT, "Normal");

        out_node = nor_node;
      }
      else if (type == LAYER_BUMP) {
        bNode *bump_node;
        bump_node = blender::bke::node_add_static_node(C, *ntree, SH_NODE_BUMP);

        in_sock = blender::bke::node_find_socket(*bump_node, SOCK_IN, "Height");
        blender::bke::node_add_link(*ntree, *out_node, *out_sock, *bump_node, *in_sock);

        in_sock = blender::bke::node_find_socket(*in_node, SOCK_IN, "Normal");
        out_sock = blender::bke::node_find_socket(*bump_node, SOCK_OUT, "Normal");

        out_node = bump_node;
      }
      else if (type == LAYER_DISPLACEMENT) {
        /* Connect to the displacement output socket */
        const blender::Span<bNode *> output_nodes = ntree->nodes_by_type(
            "ShaderNodeOutputMaterial");
        in_node = output_nodes.is_empty() ? nullptr : output_nodes.first();

        if (in_node != nullptr) {
          in_sock = blender::bke::node_find_socket(*in_node, SOCK_IN, layer_type_items[type].name);
        }
        else {
          in_sock = nullptr;
        }
      }

      /* Check if the socket in already connected to something */
      bNodeLink *link = in_sock ? in_sock->link : nullptr;
      if (in_sock != nullptr && link == nullptr) {
        blender::bke::node_add_link(*ntree, *out_node, *out_sock, *in_node, *in_sock);

        blender::bke::node_position_relative(*out_node, *in_node, out_sock, *in_sock);
      }
    }

    BKE_main_ensure_invariants(*bmain);
    /* In case we added more than one node, position them too. */
    blender::bke::node_position_propagate(*out_node);

    if (ima) {
      BKE_texpaint_slot_refresh_cache(scene, ma, ob);
      BKE_image_signal(bmain, ima, nullptr, IMA_SIGNAL_USER_NEW_IMAGE);
      WM_event_add_notifier(C, NC_IMAGE | NA_ADDED, ima);
      ED_space_image_sync(bmain, ima, false);
    }
    if (layer) {
      BKE_texpaint_slot_refresh_cache(scene, ma, ob);
      DEG_id_tag_update(static_cast<ID *>(ob->data), ID_RECALC_GEOMETRY);
      WM_main_add_notifier(NC_GEOM | ND_DATA, ob->data);
    }

    DEG_id_tag_update(&ntree->id, 0);
    DEG_id_tag_update(&ma->id, ID_RECALC_SHADING);
    DEG_relations_tag_update(bmain);
    ED_area_tag_redraw(CTX_wm_area(C));

    ED_paint_proj_mesh_data_check(*scene, *ob, nullptr, nullptr, nullptr, nullptr);

    return true;
  }

  return false;
}

static int get_texture_layer_type(wmOperator *op, const char *prop_name)
{
  int type_value = RNA_enum_get(op->ptr, prop_name);
  int type = RNA_enum_from_value(layer_type_items, type_value);
  BLI_assert(type != -1);
  return type;
}

static wmOperatorStatus texture_paint_add_texture_paint_slot_exec(bContext *C, wmOperator *op)
{
  if (proj_paint_add_slot(C, op)) {
    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

static void get_default_texture_layer_name_for_object(Object *ob,
                                                      int texture_type,
                                                      char *dst,
                                                      int dst_maxncpy)
{
  Material *ma = BKE_object_material_get(ob, ob->actcol);
  const char *base_name = ma ? &ma->id.name[2] : &ob->id.name[2];
  BLI_snprintf_utf8(
      dst, dst_maxncpy, "%s %s", base_name, DATA_(layer_type_items[texture_type].name));
}

static wmOperatorStatus texture_paint_add_texture_paint_slot_invoke(bContext *C,
                                                                    wmOperator *op,
                                                                    const wmEvent * /*event*/)
{
  Object *ob = blender::ed::object::context_active_object(C);
  Material *ma = BKE_object_material_get(ob, ob->actcol);

  int type = get_texture_layer_type(op, "type");

  /* Set default name. */
  char imagename[MAX_ID_NAME - 2];
  get_default_texture_layer_name_for_object(ob, type, (char *)&imagename, sizeof(imagename));
  RNA_string_set(op->ptr, "name", imagename);

  /* Set default color. Copy the color from nodes, so it matches the existing material.
   * Material could be null so we should have a default color. */
  float color[4];
  default_paint_slot_color_get(type, ma, color);
  RNA_float_set_array(op->ptr, "color", color);

  return WM_operator_props_dialog_popup(
      C, op, 300, IFACE_("Add Paint Slot"), CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Add"));
}

static void texture_paint_add_texture_paint_slot_ui(bContext *C, wmOperator *op)
{
  uiLayout *layout = op->layout;
  layout->use_property_split_set(true);
  layout->use_property_decorate_set(false);
  Object *ob = blender::ed::object::context_active_object(C);
  ePaintCanvasSource slot_type = PAINT_CANVAS_SOURCE_IMAGE;

  if (ob->mode == OB_MODE_SCULPT) {
    slot_type = (ePaintCanvasSource)RNA_enum_get(op->ptr, "slot_type");
    layout->prop(op->ptr, "slot_type", UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);
  }

  layout->prop(op->ptr, "name", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  switch (slot_type) {
    case PAINT_CANVAS_SOURCE_IMAGE: {
      uiLayout *col = &layout->column(true);
      col->prop(op->ptr, "width", UI_ITEM_NONE, std::nullopt, ICON_NONE);
      col->prop(op->ptr, "height", UI_ITEM_NONE, std::nullopt, ICON_NONE);

      layout->prop(op->ptr, "alpha", UI_ITEM_NONE, std::nullopt, ICON_NONE);
      layout->prop(op->ptr, "generated_type", UI_ITEM_NONE, std::nullopt, ICON_NONE);
      layout->prop(op->ptr, "float", UI_ITEM_NONE, std::nullopt, ICON_NONE);
      break;
    }
    case PAINT_CANVAS_SOURCE_COLOR_ATTRIBUTE:
      layout->prop(op->ptr, "domain", UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);
      layout->prop(op->ptr, "data_type", UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);
      break;
    case PAINT_CANVAS_SOURCE_MATERIAL:
      BLI_assert_unreachable();
      break;
  }

  layout->prop(op->ptr, "color", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

#define IMA_DEF_NAME N_("Untitled")

void PAINT_OT_add_texture_paint_slot(wmOperatorType *ot)
{
  using namespace blender;
  PropertyRNA *prop;
  static float default_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};

  static const EnumPropertyItem slot_type_items[3] = {
      {PAINT_CANVAS_SOURCE_IMAGE, "IMAGE", 0, "Image", ""},
      {PAINT_CANVAS_SOURCE_COLOR_ATTRIBUTE, "COLOR_ATTRIBUTE", 0, "Color Attribute", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Add Paint Slot";
  ot->description = "Add a paint slot";
  ot->idname = "PAINT_OT_add_texture_paint_slot";

  /* API callbacks. */
  ot->invoke = texture_paint_add_texture_paint_slot_invoke;
  ot->exec = texture_paint_add_texture_paint_slot_exec;
  ot->poll = ED_operator_object_active_editable_mesh;
  ot->ui = texture_paint_add_texture_paint_slot_ui;

  /* flags */
  ot->flag = OPTYPE_UNDO;

  /* Shared Properties */
  prop = RNA_def_enum(ot->srna,
                      "type",
                      layer_type_items,
                      0,
                      "Material Layer Type",
                      "Material layer type of new paint slot");
  RNA_def_property_flag(prop, PROP_HIDDEN);

  prop = RNA_def_enum(
      ot->srna, "slot_type", slot_type_items, 0, "Slot Type", "Type of new paint slot");

  prop = RNA_def_string(
      ot->srna, "name", IMA_DEF_NAME, MAX_NAME, "Name", "Name for new paint slot source");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_float_color(
      ot->srna, "color", 4, nullptr, 0.0f, FLT_MAX, "Color", "Default fill color", 0.0f, 1.0f);
  RNA_def_property_subtype(prop, PROP_COLOR_GAMMA);
  RNA_def_property_float_array_default(prop, default_color);

  /* Image Properties */
  prop = RNA_def_int(ot->srna, "width", 1024, 1, INT_MAX, "Width", "Image width", 1, 16384);
  RNA_def_property_subtype(prop, PROP_PIXEL);

  prop = RNA_def_int(ot->srna, "height", 1024, 1, INT_MAX, "Height", "Image height", 1, 16384);
  RNA_def_property_subtype(prop, PROP_PIXEL);

  RNA_def_boolean(ot->srna, "alpha", true, "Alpha", "Create an image with an alpha channel");

  RNA_def_enum(ot->srna,
               "generated_type",
               rna_enum_image_generated_type_items,
               IMA_GENTYPE_BLANK,
               "Generated Type",
               "Fill the image with a grid for UV map testing");

  RNA_def_boolean(ot->srna,
                  "float",
                  false,
                  "32-bit Float",
                  "Create image with 32-bit floating-point bit depth");

  /* Color Attribute Properties */
  RNA_def_enum(ot->srna,
               "domain",
               rna_enum_color_attribute_domain_items,
               int(bke::AttrDomain::Point),
               "Domain",
               "Type of element that attribute is stored on");

  RNA_def_enum(ot->srna,
               "data_type",
               rna_enum_color_attribute_type_items,
               CD_PROP_COLOR,
               "Data Type",
               "Type of data stored in attribute");
}

static wmOperatorStatus add_simple_uvs_exec(bContext *C, wmOperator * /*op*/)
{
  /* no checks here, poll function does them for us */
  Main *bmain = CTX_data_main(C);
  Object *ob = CTX_data_active_object(C);
  Scene *scene = CTX_data_scene(C);

  ED_uvedit_add_simple_uvs(bmain, scene, ob);

  ED_paint_proj_mesh_data_check(*scene, *ob, nullptr, nullptr, nullptr, nullptr);

  DEG_id_tag_update(static_cast<ID *>(ob->data), 0);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);
  WM_event_add_notifier(C, NC_SCENE | ND_TOOLSETTINGS, scene);
  return OPERATOR_FINISHED;
}

static bool add_simple_uvs_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);

  if (!ob || ob->type != OB_MESH || ob->mode != OB_MODE_TEXTURE_PAINT) {
    return false;
  }
  return true;
}

void PAINT_OT_add_simple_uvs(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Simple UVs";
  ot->description = "Add cube map UVs on mesh";
  ot->idname = "PAINT_OT_add_simple_uvs";

  /* API callbacks. */
  ot->exec = add_simple_uvs_exec;
  ot->poll = add_simple_uvs_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}
