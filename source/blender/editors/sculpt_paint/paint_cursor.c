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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2009 by Nicholas Bishop
 * All rights reserved.
 */

/** \file
 * \ingroup edsculpt
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_rect.h"
#include "BLI_task.h"
#include "BLI_utildefines.h"

#include "DNA_brush_types.h"
#include "DNA_customdata_types.h"
#include "DNA_color_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"

#include "BKE_brush.h"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_image.h"
#include "BKE_node.h"
#include "BKE_paint.h"
#include "BKE_colortools.h"

#include "WM_api.h"

#include "IMB_imbuf_types.h"

#include "ED_view3d.h"

#include "GPU_draw.h"
#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "UI_resources.h"

#include "paint_intern.h"
/* still needed for sculpt_stroke_get_location, should be
 * removed eventually (TODO) */
#include "sculpt_intern.h"

/* TODOs:
 *
 * Some of the cursor drawing code is doing non-draw stuff
 * (e.g. updating the brush rake angle). This should be cleaned up
 * still.
 *
 * There is also some ugliness with sculpt-specific code.
 */

typedef struct TexSnapshot {
  GLuint overlay_texture;
  int winx;
  int winy;
  int old_size;
  float old_zoom;
  bool old_col;
} TexSnapshot;

typedef struct CursorSnapshot {
  GLuint overlay_texture;
  int size;
  int zoom;
} CursorSnapshot;

static TexSnapshot primary_snap = {0};
static TexSnapshot secondary_snap = {0};
static CursorSnapshot cursor_snap = {0};

/* delete overlay cursor textures to preserve memory and invalidate all overlay flags */
void paint_cursor_delete_textures(void)
{
  if (primary_snap.overlay_texture) {
    glDeleteTextures(1, &primary_snap.overlay_texture);
  }
  if (secondary_snap.overlay_texture) {
    glDeleteTextures(1, &secondary_snap.overlay_texture);
  }
  if (cursor_snap.overlay_texture) {
    glDeleteTextures(1, &cursor_snap.overlay_texture);
  }

  memset(&primary_snap, 0, sizeof(TexSnapshot));
  memset(&secondary_snap, 0, sizeof(TexSnapshot));
  memset(&cursor_snap, 0, sizeof(CursorSnapshot));

  BKE_paint_invalidate_overlay_all();
}

static int same_tex_snap(TexSnapshot *snap, MTex *mtex, ViewContext *vc, bool col, float zoom)
{
  return (/* make brush smaller shouldn't cause a resample */
          //(mtex->brush_map_mode != MTEX_MAP_MODE_VIEW ||
          //(BKE_brush_size_get(vc->scene, brush) <= snap->BKE_brush_size_get)) &&

          (mtex->brush_map_mode != MTEX_MAP_MODE_TILED ||
           (vc->ar->winx == snap->winx && vc->ar->winy == snap->winy)) &&
          (mtex->brush_map_mode == MTEX_MAP_MODE_STENCIL || snap->old_zoom == zoom) &&
          snap->old_col == col);
}

static void make_tex_snap(TexSnapshot *snap, ViewContext *vc, float zoom)
{
  snap->old_zoom = zoom;
  snap->winx = vc->ar->winx;
  snap->winy = vc->ar->winy;
}

typedef struct LoadTexData {
  Brush *br;
  ViewContext *vc;

  MTex *mtex;
  GLubyte *buffer;
  bool col;

  struct ImagePool *pool;
  int size;
  float rotation;
  float radius;
} LoadTexData;

static void load_tex_task_cb_ex(void *__restrict userdata,
                                const int j,
                                const ParallelRangeTLS *__restrict tls)
{
  LoadTexData *data = userdata;
  Brush *br = data->br;
  ViewContext *vc = data->vc;

  MTex *mtex = data->mtex;
  GLubyte *buffer = data->buffer;
  const bool col = data->col;

  struct ImagePool *pool = data->pool;
  const int size = data->size;
  const float rotation = data->rotation;
  const float radius = data->radius;

  bool convert_to_linear = false;
  struct ColorSpace *colorspace = NULL;

  if (mtex->tex && mtex->tex->type == TEX_IMAGE && mtex->tex->ima) {
    ImBuf *tex_ibuf = BKE_image_pool_acquire_ibuf(mtex->tex->ima, &mtex->tex->iuser, pool);
    /* For consistency, sampling always returns color in linear space */
    if (tex_ibuf && tex_ibuf->rect_float == NULL) {
      convert_to_linear = true;
      colorspace = tex_ibuf->rect_colorspace;
    }
    BKE_image_pool_release_ibuf(mtex->tex->ima, tex_ibuf, pool);
  }

  for (int i = 0; i < size; i++) {
    // largely duplicated from tex_strength

    int index = j * size + i;

    float x = (float)i / size;
    float y = (float)j / size;
    float len;

    if (mtex->brush_map_mode == MTEX_MAP_MODE_TILED) {
      x *= vc->ar->winx / radius;
      y *= vc->ar->winy / radius;
    }
    else {
      x = (x - 0.5f) * 2.0f;
      y = (y - 0.5f) * 2.0f;
    }

    len = sqrtf(x * x + y * y);

    if (ELEM(mtex->brush_map_mode, MTEX_MAP_MODE_TILED, MTEX_MAP_MODE_STENCIL) || len <= 1.0f) {
      /* It is probably worth optimizing for those cases where the texture is not rotated by
       * skipping the calls to atan2, sqrtf, sin, and cos. */
      if (mtex->tex && (rotation > 0.001f || rotation < -0.001f)) {
        const float angle = atan2f(y, x) + rotation;

        x = len * cosf(angle);
        y = len * sinf(angle);
      }

      if (col) {
        float rgba[4];

        paint_get_tex_pixel_col(
            mtex, x, y, rgba, pool, tls->thread_id, convert_to_linear, colorspace);

        buffer[index * 4] = rgba[0] * 255;
        buffer[index * 4 + 1] = rgba[1] * 255;
        buffer[index * 4 + 2] = rgba[2] * 255;
        buffer[index * 4 + 3] = rgba[3] * 255;
      }
      else {
        float avg = paint_get_tex_pixel(mtex, x, y, pool, tls->thread_id);

        avg += br->texture_sample_bias;

        /* clamp to avoid precision overflow */
        CLAMP(avg, 0.0f, 1.0f);
        buffer[index] = 255 - (GLubyte)(255 * avg);
      }
    }
    else {
      if (col) {
        buffer[index * 4] = 0;
        buffer[index * 4 + 1] = 0;
        buffer[index * 4 + 2] = 0;
        buffer[index * 4 + 3] = 0;
      }
      else {
        buffer[index] = 0;
      }
    }
  }
}

static int load_tex(Brush *br, ViewContext *vc, float zoom, bool col, bool primary)
{
  bool init;
  TexSnapshot *target;

  MTex *mtex = (primary) ? &br->mtex : &br->mask_mtex;
  eOverlayControlFlags overlay_flags = BKE_paint_get_overlay_flags();
  GLubyte *buffer = NULL;

  int size;
  bool refresh;
  eOverlayControlFlags invalid = ((primary) ?
                                      (overlay_flags & PAINT_OVERLAY_INVALID_TEXTURE_PRIMARY) :
                                      (overlay_flags & PAINT_OVERLAY_INVALID_TEXTURE_SECONDARY));
  target = (primary) ? &primary_snap : &secondary_snap;

  refresh = !target->overlay_texture || (invalid != 0) ||
            !same_tex_snap(target, mtex, vc, col, zoom);

  init = (target->overlay_texture != 0);

  if (refresh) {
    struct ImagePool *pool = NULL;
    /* stencil is rotated later */
    const float rotation = (mtex->brush_map_mode != MTEX_MAP_MODE_STENCIL) ? -mtex->rot : 0.0f;
    const float radius = BKE_brush_size_get(vc->scene, br) * zoom;

    make_tex_snap(target, vc, zoom);

    if (mtex->brush_map_mode == MTEX_MAP_MODE_VIEW) {
      int s = BKE_brush_size_get(vc->scene, br);
      int r = 1;

      for (s >>= 1; s > 0; s >>= 1) {
        r++;
      }

      size = (1 << r);

      if (size < 256) {
        size = 256;
      }

      if (size < target->old_size) {
        size = target->old_size;
      }
    }
    else {
      size = 512;
    }

    if (target->old_size != size) {
      if (target->overlay_texture) {
        glDeleteTextures(1, &target->overlay_texture);
        target->overlay_texture = 0;
      }

      init = false;

      target->old_size = size;
    }
    if (col) {
      buffer = MEM_mallocN(sizeof(GLubyte) * size * size * 4, "load_tex");
    }
    else {
      buffer = MEM_mallocN(sizeof(GLubyte) * size * size, "load_tex");
    }

    pool = BKE_image_pool_new();

    if (mtex->tex && mtex->tex->nodetree) {
      /* has internal flag to detect it only does it once */
      ntreeTexBeginExecTree(mtex->tex->nodetree);
    }

    LoadTexData data = {
        .br = br,
        .vc = vc,
        .mtex = mtex,
        .buffer = buffer,
        .col = col,
        .pool = pool,
        .size = size,
        .rotation = rotation,
        .radius = radius,
    };

    ParallelRangeSettings settings;
    BLI_parallel_range_settings_defaults(&settings);
    BLI_task_parallel_range(0, size, &data, load_tex_task_cb_ex, &settings);

    if (mtex->tex && mtex->tex->nodetree) {
      ntreeTexEndExecTree(mtex->tex->nodetree->execdata);
    }

    if (pool) {
      BKE_image_pool_free(pool);
    }

    if (!target->overlay_texture) {
      glGenTextures(1, &target->overlay_texture);
    }
  }
  else {
    size = target->old_size;
  }

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, target->overlay_texture);

  if (refresh) {
    GLenum format = col ? GL_RGBA : GL_RED;
    GLenum internalformat = col ? GL_RGBA8 : GL_R8;

    if (!init || (target->old_col != col)) {
      glTexImage2D(
          GL_TEXTURE_2D, 0, internalformat, size, size, 0, format, GL_UNSIGNED_BYTE, buffer);
    }
    else {
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, size, size, format, GL_UNSIGNED_BYTE, buffer);
    }

    if (buffer) {
      MEM_freeN(buffer);
    }

    target->old_col = col;
  }

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  if (mtex->brush_map_mode == MTEX_MAP_MODE_VIEW) {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
  }

  BKE_paint_reset_overlay_invalid(invalid);

  return 1;
}

static void load_tex_cursor_task_cb(void *__restrict userdata,
                                    const int j,
                                    const ParallelRangeTLS *__restrict UNUSED(tls))
{
  LoadTexData *data = userdata;
  Brush *br = data->br;

  GLubyte *buffer = data->buffer;

  const int size = data->size;

  for (int i = 0; i < size; i++) {
    // largely duplicated from tex_strength

    const int index = j * size + i;
    const float x = (((float)i / size) - 0.5f) * 2.0f;
    const float y = (((float)j / size) - 0.5f) * 2.0f;
    const float len = sqrtf(x * x + y * y);

    if (len <= 1.0f) {
      float avg = BKE_brush_curve_strength_clamped(br, len, 1.0f); /* Falloff curve */

      buffer[index] = 255 - (GLubyte)(255 * avg);
    }
    else {
      buffer[index] = 0;
    }
  }
}

static int load_tex_cursor(Brush *br, ViewContext *vc, float zoom)
{
  bool init;

  eOverlayControlFlags overlay_flags = BKE_paint_get_overlay_flags();
  GLubyte *buffer = NULL;

  int size;
  const bool refresh = !cursor_snap.overlay_texture ||
                       (overlay_flags & PAINT_OVERLAY_INVALID_CURVE) || cursor_snap.zoom != zoom;

  init = (cursor_snap.overlay_texture != 0);

  if (refresh) {
    int s, r;

    cursor_snap.zoom = zoom;

    s = BKE_brush_size_get(vc->scene, br);
    r = 1;

    for (s >>= 1; s > 0; s >>= 1) {
      r++;
    }

    size = (1 << r);

    if (size < 256) {
      size = 256;
    }

    if (size < cursor_snap.size) {
      size = cursor_snap.size;
    }

    if (cursor_snap.size != size) {
      if (cursor_snap.overlay_texture) {
        glDeleteTextures(1, &cursor_snap.overlay_texture);
        cursor_snap.overlay_texture = 0;
      }

      init = false;

      cursor_snap.size = size;
    }
    buffer = MEM_mallocN(sizeof(GLubyte) * size * size, "load_tex");

    curvemapping_initialize(br->curve);

    LoadTexData data = {
        .br = br,
        .buffer = buffer,
        .size = size,
    };

    ParallelRangeSettings settings;
    BLI_parallel_range_settings_defaults(&settings);
    BLI_task_parallel_range(0, size, &data, load_tex_cursor_task_cb, &settings);

    if (!cursor_snap.overlay_texture) {
      glGenTextures(1, &cursor_snap.overlay_texture);
    }
  }
  else {
    size = cursor_snap.size;
  }

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, cursor_snap.overlay_texture);

  if (refresh) {
    if (!init) {
      glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, size, size, 0, GL_RED, GL_UNSIGNED_BYTE, buffer);
    }
    else {
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, size, size, GL_RED, GL_UNSIGNED_BYTE, buffer);
    }

    if (buffer) {
      MEM_freeN(buffer);
    }
  }

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

  BKE_paint_reset_overlay_invalid(PAINT_OVERLAY_INVALID_CURVE);

  return 1;
}

static int project_brush_radius(ViewContext *vc, float radius, const float location[3])
{
  float view[3], nonortho[3], ortho[3], offset[3], p1[2], p2[2];

  ED_view3d_global_to_vector(vc->rv3d, location, view);

  /* create a vector that is not orthogonal to view */

  if (fabsf(view[0]) < 0.1f) {
    nonortho[0] = view[0] + 1.0f;
    nonortho[1] = view[1];
    nonortho[2] = view[2];
  }
  else if (fabsf(view[1]) < 0.1f) {
    nonortho[0] = view[0];
    nonortho[1] = view[1] + 1.0f;
    nonortho[2] = view[2];
  }
  else {
    nonortho[0] = view[0];
    nonortho[1] = view[1];
    nonortho[2] = view[2] + 1.0f;
  }

  /* get a vector in the plane of the view */
  cross_v3_v3v3(ortho, nonortho, view);
  normalize_v3(ortho);

  /* make a point on the surface of the brush tangent to the view */
  mul_v3_fl(ortho, radius);
  add_v3_v3v3(offset, location, ortho);

  /* project the center of the brush, and the tangent point to the view onto the screen */
  if ((ED_view3d_project_float_global(vc->ar, location, p1, V3D_PROJ_TEST_NOP) ==
       V3D_PROJ_RET_OK) &&
      (ED_view3d_project_float_global(vc->ar, offset, p2, V3D_PROJ_TEST_NOP) == V3D_PROJ_RET_OK)) {
    /* the distance between these points is the size of the projected brush in pixels */
    return len_v2v2(p1, p2);
  }
  else {
    BLI_assert(0); /* assert because the code that sets up the vectors should disallow this */
    return 0;
  }
}

static bool sculpt_get_brush_geometry(bContext *C,
                                      ViewContext *vc,
                                      int x,
                                      int y,
                                      int *pixel_radius,
                                      float location[3],
                                      UnifiedPaintSettings *ups)
{
  Scene *scene = CTX_data_scene(C);
  Paint *paint = BKE_paint_get_active_from_context(C);
  float mouse[2];
  bool hit = false;

  mouse[0] = x;
  mouse[1] = y;

  if (vc->obact->sculpt && vc->obact->sculpt->pbvh) {
    if (!ups->stroke_active) {
      hit = sculpt_stroke_get_location(C, location, mouse);
    }
    else {
      hit = ups->last_hit;
      copy_v3_v3(location, ups->last_location);
    }
  }

  if (hit) {
    Brush *brush = BKE_paint_brush(paint);

    *pixel_radius = project_brush_radius(
        vc, BKE_brush_unprojected_radius_get(scene, brush), location);

    if (*pixel_radius == 0) {
      *pixel_radius = BKE_brush_size_get(scene, brush);
    }

    mul_m4_v3(vc->obact->obmat, location);
  }
  else {
    Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
    Brush *brush = BKE_paint_brush(&sd->paint);

    *pixel_radius = BKE_brush_size_get(scene, brush);
  }

  return hit;
}

/* Draw an overlay that shows what effect the brush's texture will
 * have on brush strength */
static void paint_draw_tex_overlay(UnifiedPaintSettings *ups,
                                   Brush *brush,
                                   ViewContext *vc,
                                   int x,
                                   int y,
                                   float zoom,
                                   bool col,
                                   bool primary)
{
  rctf quad;
  /* check for overlay mode */

  MTex *mtex = (primary) ? &brush->mtex : &brush->mask_mtex;
  bool valid = ((primary) ? (brush->overlay_flags & BRUSH_OVERLAY_PRIMARY) != 0 :
                            (brush->overlay_flags & BRUSH_OVERLAY_SECONDARY) != 0);
  int overlay_alpha = (primary) ? brush->texture_overlay_alpha : brush->mask_overlay_alpha;

  if (!(mtex->tex) ||
      !((mtex->brush_map_mode == MTEX_MAP_MODE_STENCIL) ||
        (valid && ELEM(mtex->brush_map_mode, MTEX_MAP_MODE_VIEW, MTEX_MAP_MODE_TILED)))) {
    return;
  }

  if (load_tex(brush, vc, zoom, col, primary)) {
    GPU_blend(true);

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_FALSE);
    glDepthFunc(GL_ALWAYS);

    if (mtex->brush_map_mode == MTEX_MAP_MODE_VIEW) {
      GPU_matrix_push();

      /* brush rotation */
      GPU_matrix_translate_2f(x, y);
      GPU_matrix_rotate_2d(-RAD2DEGF(primary ? ups->brush_rotation : ups->brush_rotation_sec));
      GPU_matrix_translate_2f(-x, -y);

      /* scale based on tablet pressure */
      if (primary && ups->stroke_active && BKE_brush_use_size_pressure(vc->scene, brush)) {
        const float scale = ups->size_pressure_value;
        GPU_matrix_translate_2f(x, y);
        GPU_matrix_scale_2f(scale, scale);
        GPU_matrix_translate_2f(-x, -y);
      }

      if (ups->draw_anchored) {
        quad.xmin = ups->anchored_initial_mouse[0] - ups->anchored_size;
        quad.ymin = ups->anchored_initial_mouse[1] - ups->anchored_size;
        quad.xmax = ups->anchored_initial_mouse[0] + ups->anchored_size;
        quad.ymax = ups->anchored_initial_mouse[1] + ups->anchored_size;
      }
      else {
        const int radius = BKE_brush_size_get(vc->scene, brush) * zoom;
        quad.xmin = x - radius;
        quad.ymin = y - radius;
        quad.xmax = x + radius;
        quad.ymax = y + radius;
      }
    }
    else if (mtex->brush_map_mode == MTEX_MAP_MODE_TILED) {
      quad.xmin = 0;
      quad.ymin = 0;
      quad.xmax = BLI_rcti_size_x(&vc->ar->winrct);
      quad.ymax = BLI_rcti_size_y(&vc->ar->winrct);
    }
    /* Stencil code goes here */
    else {
      if (primary) {
        quad.xmin = -brush->stencil_dimension[0];
        quad.ymin = -brush->stencil_dimension[1];
        quad.xmax = brush->stencil_dimension[0];
        quad.ymax = brush->stencil_dimension[1];
      }
      else {
        quad.xmin = -brush->mask_stencil_dimension[0];
        quad.ymin = -brush->mask_stencil_dimension[1];
        quad.xmax = brush->mask_stencil_dimension[0];
        quad.ymax = brush->mask_stencil_dimension[1];
      }
      GPU_matrix_push();
      if (primary) {
        GPU_matrix_translate_2fv(brush->stencil_pos);
      }
      else {
        GPU_matrix_translate_2fv(brush->mask_stencil_pos);
      }
      GPU_matrix_rotate_2d(RAD2DEGF(mtex->rot));
    }

    /* set quad color. Colored overlay does not get blending */
    GPUVertFormat *format = immVertexFormat();
    uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
    uint texCoord = GPU_vertformat_attr_add(format, "texCoord", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

    if (col) {
      immBindBuiltinProgram(GPU_SHADER_2D_IMAGE_COLOR);
      immUniformColor4f(1.0f, 1.0f, 1.0f, overlay_alpha * 0.01f);
    }
    else {
      GPU_blend_set_func(GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
      immBindBuiltinProgram(GPU_SHADER_2D_IMAGE_ALPHA_COLOR);
      immUniformColor3fvAlpha(U.sculpt_paint_overlay_col, overlay_alpha * 0.01f);
    }

    /* draw textured quad */
    immUniform1i("image", 0);

    immBegin(GPU_PRIM_TRI_FAN, 4);
    immAttr2f(texCoord, 0.0f, 0.0f);
    immVertex2f(pos, quad.xmin, quad.ymin);
    immAttr2f(texCoord, 1.0f, 0.0f);
    immVertex2f(pos, quad.xmax, quad.ymin);
    immAttr2f(texCoord, 1.0f, 1.0f);
    immVertex2f(pos, quad.xmax, quad.ymax);
    immAttr2f(texCoord, 0.0f, 1.0f);
    immVertex2f(pos, quad.xmin, quad.ymax);
    immEnd();

    immUnbindProgram();
    GPU_blend_set_func(GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);

    if (ELEM(mtex->brush_map_mode, MTEX_MAP_MODE_STENCIL, MTEX_MAP_MODE_VIEW)) {
      GPU_matrix_pop();
    }
  }
}

/* Draw an overlay that shows what effect the brush's texture will
 * have on brush strength */
static void paint_draw_cursor_overlay(
    UnifiedPaintSettings *ups, Brush *brush, ViewContext *vc, int x, int y, float zoom)
{
  rctf quad;
  /* check for overlay mode */

  if (!(brush->overlay_flags & BRUSH_OVERLAY_CURSOR)) {
    return;
  }

  if (load_tex_cursor(brush, vc, zoom)) {
    bool do_pop = false;
    float center[2];
    GPU_blend(true);

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_FALSE);
    glDepthFunc(GL_ALWAYS);

    if (ups->draw_anchored) {
      copy_v2_v2(center, ups->anchored_initial_mouse);
      quad.xmin = ups->anchored_initial_mouse[0] - ups->anchored_size;
      quad.ymin = ups->anchored_initial_mouse[1] - ups->anchored_size;
      quad.xmax = ups->anchored_initial_mouse[0] + ups->anchored_size;
      quad.ymax = ups->anchored_initial_mouse[1] + ups->anchored_size;
    }
    else {
      const int radius = BKE_brush_size_get(vc->scene, brush) * zoom;
      center[0] = x;
      center[1] = y;

      quad.xmin = x - radius;
      quad.ymin = y - radius;
      quad.xmax = x + radius;
      quad.ymax = y + radius;
    }

    /* scale based on tablet pressure */
    if (ups->stroke_active && BKE_brush_use_size_pressure(vc->scene, brush)) {
      do_pop = true;
      GPU_matrix_push();
      GPU_matrix_translate_2fv(center);
      GPU_matrix_scale_1f(ups->size_pressure_value);
      GPU_matrix_translate_2f(-center[0], -center[1]);
    }

    GPUVertFormat *format = immVertexFormat();
    uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
    uint texCoord = GPU_vertformat_attr_add(format, "texCoord", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

    GPU_blend_set_func(GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
    immBindBuiltinProgram(GPU_SHADER_2D_IMAGE_ALPHA_COLOR);

    immUniformColor3fvAlpha(U.sculpt_paint_overlay_col, brush->cursor_overlay_alpha * 0.01f);

    /* draw textured quad */

    /* draw textured quad */
    immUniform1i("image", 0);

    immBegin(GPU_PRIM_TRI_FAN, 4);
    immAttr2f(texCoord, 0.0f, 0.0f);
    immVertex2f(pos, quad.xmin, quad.ymin);
    immAttr2f(texCoord, 1.0f, 0.0f);
    immVertex2f(pos, quad.xmax, quad.ymin);
    immAttr2f(texCoord, 1.0f, 1.0f);
    immVertex2f(pos, quad.xmax, quad.ymax);
    immAttr2f(texCoord, 0.0f, 1.0f);
    immVertex2f(pos, quad.xmin, quad.ymax);
    immEnd();

    immUnbindProgram();

    GPU_blend_set_func(GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);

    if (do_pop) {
      GPU_matrix_pop();
    }
  }
}

static void paint_draw_alpha_overlay(UnifiedPaintSettings *ups,
                                     Brush *brush,
                                     ViewContext *vc,
                                     int x,
                                     int y,
                                     float zoom,
                                     ePaintMode mode)
{
  /* Color means that primary brush texture is colored and
   * secondary is used for alpha/mask control. */
  bool col = ELEM(mode, PAINT_MODE_TEXTURE_3D, PAINT_MODE_TEXTURE_2D, PAINT_MODE_VERTEX);
  eOverlayControlFlags flags = BKE_paint_get_overlay_flags();
  gpuPushAttr(GPU_DEPTH_BUFFER_BIT | GPU_BLEND_BIT);

  /* Translate to region. */
  GPU_matrix_push();
  GPU_matrix_translate_2f(vc->ar->winrct.xmin, vc->ar->winrct.ymin);
  x -= vc->ar->winrct.xmin;
  y -= vc->ar->winrct.ymin;

  /* Colored overlay should be drawn separately. */
  if (col) {
    if (!(flags & PAINT_OVERLAY_OVERRIDE_PRIMARY)) {
      paint_draw_tex_overlay(ups, brush, vc, x, y, zoom, true, true);
    }
    if (!(flags & PAINT_OVERLAY_OVERRIDE_SECONDARY)) {
      paint_draw_tex_overlay(ups, brush, vc, x, y, zoom, false, false);
    }
    if (!(flags & PAINT_OVERLAY_OVERRIDE_CURSOR)) {
      paint_draw_cursor_overlay(ups, brush, vc, x, y, zoom);
    }
  }
  else {
    if (!(flags & PAINT_OVERLAY_OVERRIDE_PRIMARY) && (mode != PAINT_MODE_WEIGHT)) {
      paint_draw_tex_overlay(ups, brush, vc, x, y, zoom, false, true);
    }
    if (!(flags & PAINT_OVERLAY_OVERRIDE_CURSOR)) {
      paint_draw_cursor_overlay(ups, brush, vc, x, y, zoom);
    }
  }

  GPU_matrix_pop();
  gpuPopAttr();
}

BLI_INLINE void draw_tri_point(
    unsigned int pos, float sel_col[4], float pivot_col[4], float *co, float width, bool selected)
{
  immUniformColor4fv(selected ? sel_col : pivot_col);

  GPU_line_width(3.0f);

  float w = width / 2.0f;
  float tri[3][2] = {
      {co[0], co[1] + w},
      {co[0] - w, co[1] - w},
      {co[0] + w, co[1] - w},
  };

  immBegin(GPU_PRIM_LINE_LOOP, 3);
  immVertex2fv(pos, tri[0]);
  immVertex2fv(pos, tri[1]);
  immVertex2fv(pos, tri[2]);
  immEnd();

  immUniformColor4f(1.0f, 1.0f, 1.0f, 0.5f);
  GPU_line_width(1.0f);

  immBegin(GPU_PRIM_LINE_LOOP, 3);
  immVertex2fv(pos, tri[0]);
  immVertex2fv(pos, tri[1]);
  immVertex2fv(pos, tri[2]);
  immEnd();
}

BLI_INLINE void draw_rect_point(
    unsigned int pos, float sel_col[4], float handle_col[4], float *co, float width, bool selected)
{
  immUniformColor4fv(selected ? sel_col : handle_col);

  GPU_line_width(3.0f);

  float w = width / 2.0f;
  float minx = co[0] - w;
  float miny = co[1] - w;
  float maxx = co[0] + w;
  float maxy = co[1] + w;

  imm_draw_box_wire_2d(pos, minx, miny, maxx, maxy);

  immUniformColor4f(1.0f, 1.0f, 1.0f, 0.5f);
  GPU_line_width(1.0f);

  imm_draw_box_wire_2d(pos, minx, miny, maxx, maxy);
}

BLI_INLINE void draw_bezier_handle_lines(unsigned int pos, float sel_col[4], BezTriple *bez)
{
  immUniformColor4f(0.0f, 0.0f, 0.0f, 0.5f);
  GPU_line_width(3.0f);

  immBegin(GPU_PRIM_LINE_STRIP, 3);
  immVertex2fv(pos, bez->vec[0]);
  immVertex2fv(pos, bez->vec[1]);
  immVertex2fv(pos, bez->vec[2]);
  immEnd();

  GPU_line_width(1.0f);

  if (bez->f1 || bez->f2) {
    immUniformColor4fv(sel_col);
  }
  else {
    immUniformColor4f(1.0f, 1.0f, 1.0f, 0.5f);
  }
  immBegin(GPU_PRIM_LINES, 2);
  immVertex2fv(pos, bez->vec[0]);
  immVertex2fv(pos, bez->vec[1]);
  immEnd();

  if (bez->f3 || bez->f2) {
    immUniformColor4fv(sel_col);
  }
  else {
    immUniformColor4f(1.0f, 1.0f, 1.0f, 0.5f);
  }
  immBegin(GPU_PRIM_LINES, 2);
  immVertex2fv(pos, bez->vec[1]);
  immVertex2fv(pos, bez->vec[2]);
  immEnd();
}

static void paint_draw_curve_cursor(Brush *brush, ViewContext *vc)
{
  GPU_matrix_push();
  GPU_matrix_translate_2f(vc->ar->winrct.xmin, vc->ar->winrct.ymin);

  if (brush->paint_curve && brush->paint_curve->points) {
    int i;
    PaintCurve *pc = brush->paint_curve;
    PaintCurvePoint *cp = pc->points;

    GPU_line_smooth(true);
    GPU_blend(true);

    /* draw the bezier handles and the curve segment between the current and next point */
    uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

    immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

    float selec_col[4], handle_col[4], pivot_col[4];
    UI_GetThemeColor4fv(TH_VERTEX_SELECT, selec_col);
    UI_GetThemeColor4fv(TH_PAINT_CURVE_HANDLE, handle_col);
    UI_GetThemeColor4fv(TH_PAINT_CURVE_PIVOT, pivot_col);

    for (i = 0; i < pc->tot_points - 1; i++, cp++) {
      int j;
      PaintCurvePoint *cp_next = cp + 1;
      float data[(PAINT_CURVE_NUM_SEGMENTS + 1) * 2];
      /* use color coding to distinguish handles vs curve segments  */
      draw_bezier_handle_lines(pos, selec_col, &cp->bez);
      draw_tri_point(pos, selec_col, pivot_col, &cp->bez.vec[1][0], 10.0f, cp->bez.f2);
      draw_rect_point(
          pos, selec_col, handle_col, &cp->bez.vec[0][0], 8.0f, cp->bez.f1 || cp->bez.f2);
      draw_rect_point(
          pos, selec_col, handle_col, &cp->bez.vec[2][0], 8.0f, cp->bez.f3 || cp->bez.f2);

      for (j = 0; j < 2; j++) {
        BKE_curve_forward_diff_bezier(cp->bez.vec[1][j],
                                      cp->bez.vec[2][j],
                                      cp_next->bez.vec[0][j],
                                      cp_next->bez.vec[1][j],
                                      data + j,
                                      PAINT_CURVE_NUM_SEGMENTS,
                                      sizeof(float[2]));
      }

      float(*v)[2] = (float(*)[2])data;

      immUniformColor4f(0.0f, 0.0f, 0.0f, 0.5f);
      GPU_line_width(3.0f);
      immBegin(GPU_PRIM_LINE_STRIP, PAINT_CURVE_NUM_SEGMENTS + 1);
      for (j = 0; j <= PAINT_CURVE_NUM_SEGMENTS; j++) {
        immVertex2fv(pos, v[j]);
      }
      immEnd();

      immUniformColor4f(0.9f, 0.9f, 1.0f, 0.5f);
      GPU_line_width(1.0f);
      immBegin(GPU_PRIM_LINE_STRIP, PAINT_CURVE_NUM_SEGMENTS + 1);
      for (j = 0; j <= PAINT_CURVE_NUM_SEGMENTS; j++) {
        immVertex2fv(pos, v[j]);
      }
      immEnd();
    }

    /* draw last line segment */
    draw_bezier_handle_lines(pos, selec_col, &cp->bez);
    draw_tri_point(pos, selec_col, pivot_col, &cp->bez.vec[1][0], 10.0f, cp->bez.f2);
    draw_rect_point(
        pos, selec_col, handle_col, &cp->bez.vec[0][0], 8.0f, cp->bez.f1 || cp->bez.f2);
    draw_rect_point(
        pos, selec_col, handle_col, &cp->bez.vec[2][0], 8.0f, cp->bez.f3 || cp->bez.f2);

    GPU_blend(false);
    GPU_line_smooth(false);

    immUnbindProgram();
  }
  GPU_matrix_pop();
}

/* Special actions taken when paint cursor goes over mesh */
/* TODO: sculpt only for now */
static void paint_cursor_on_hit(UnifiedPaintSettings *ups,
                                Brush *brush,
                                ViewContext *vc,
                                const float location[3])
{
  float unprojected_radius, projected_radius;

  /* update the brush's cached 3D radius */
  if (!BKE_brush_use_locked_size(vc->scene, brush)) {
    /* get 2D brush radius */
    if (ups->draw_anchored) {
      projected_radius = ups->anchored_size;
    }
    else {
      if (brush->flag & BRUSH_ANCHORED) {
        projected_radius = 8;
      }
      else {
        projected_radius = BKE_brush_size_get(vc->scene, brush);
      }
    }

    /* convert brush radius from 2D to 3D */
    unprojected_radius = paint_calc_object_space_radius(vc, location, projected_radius);

    /* scale 3D brush radius by pressure */
    if (ups->stroke_active && BKE_brush_use_size_pressure(vc->scene, brush)) {
      unprojected_radius *= ups->size_pressure_value;
    }

    /* set cached value in either Brush or UnifiedPaintSettings */
    BKE_brush_unprojected_radius_set(vc->scene, brush, unprojected_radius);
  }
}

static bool ommit_cursor_drawing(Paint *paint, ePaintMode mode, Brush *brush)
{
  if (paint->flags & PAINT_SHOW_BRUSH) {
    if (ELEM(mode, PAINT_MODE_TEXTURE_2D, PAINT_MODE_TEXTURE_3D) &&
        brush->imagepaint_tool == PAINT_TOOL_FILL) {
      return true;
    }
    return false;
  }
  return true;
}

static void paint_draw_cursor(bContext *C, int x, int y, void *UNUSED(unused))
{
  Scene *scene = CTX_data_scene(C);
  ARegion *ar = CTX_wm_region(C);
  UnifiedPaintSettings *ups = &scene->toolsettings->unified_paint_settings;
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *brush = BKE_paint_brush(paint);
  ePaintMode mode = BKE_paintmode_get_active_from_context(C);

  /* check that brush drawing is enabled */
  if (ommit_cursor_drawing(paint, mode, brush)) {
    return;
  }

  /* can't use stroke vc here because this will be called during
   * mouse over too, not just during a stroke */
  ViewContext vc;
  ED_view3d_viewcontext_init(C, &vc);

  if (vc.rv3d && (vc.rv3d->rflag & RV3D_NAVIGATING)) {
    return;
  }

  /* skip everything and draw brush here */
  if (brush->flag & BRUSH_CURVE) {
    paint_draw_curve_cursor(brush, &vc);
    return;
  }

  float zoomx, zoomy;
  get_imapaint_zoom(C, &zoomx, &zoomy);
  zoomx = max_ff(zoomx, zoomy);

  /* set various defaults */
  const float *outline_col = brush->add_col;
  const float outline_alpha = 0.5f;
  float translation[2] = {x, y};
  float final_radius = (BKE_brush_size_get(scene, brush) * zoomx);

  /* don't calculate rake angles while a stroke is active because the rake variables are global
   * and we may get interference with the stroke itself.
   * For line strokes, such interference is visible */
  if (!ups->stroke_active) {
    paint_calculate_rake_rotation(ups, brush, translation);
  }

  /* draw overlay */
  paint_draw_alpha_overlay(ups, brush, &vc, x, y, zoomx, mode);

  /* TODO: as sculpt and other paint modes are unified, this
   * special mode of drawing will go away */
  if ((mode == PAINT_MODE_SCULPT) && vc.obact->sculpt) {
    float location[3];
    int pixel_radius;

    /* test if brush is over the mesh */
    bool hit = sculpt_get_brush_geometry(C, &vc, x, y, &pixel_radius, location, ups);

    if (BKE_brush_use_locked_size(scene, brush)) {
      BKE_brush_size_set(scene, brush, pixel_radius);
    }

    /* check if brush is subtracting, use different color then */
    /* TODO: no way currently to know state of pen flip or
     * invert key modifier without starting a stroke */
    if (((ups->draw_inverted == 0) ^ ((brush->flag & BRUSH_DIR_IN) == 0)) &&
        BKE_brush_sculpt_has_secondary_color(brush)) {
      outline_col = brush->sub_col;
    }

    /* only do if brush is over the mesh */
    if (hit) {
      paint_cursor_on_hit(ups, brush, &vc, location);
    }
  }

  if (ups->draw_anchored) {
    final_radius = ups->anchored_size;
    copy_v2_fl2(translation,
                ups->anchored_initial_mouse[0] + ar->winrct.xmin,
                ups->anchored_initial_mouse[1] + ar->winrct.ymin);
  }

  /* make lines pretty */
  GPU_line_width(1.0f);
  GPU_blend(true); /* TODO: also set blend mode? */
  GPU_line_smooth(true);

  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

  /* set brush color */
  immUniformColor3fvAlpha(outline_col, outline_alpha);

  /* draw brush outline */
  if (ups->stroke_active && BKE_brush_use_size_pressure(scene, brush)) {
    /* inner at full alpha */
    imm_draw_circle_wire_2d(
        pos, translation[0], translation[1], final_radius * ups->size_pressure_value, 40);
    /* outer at half alpha */
    immUniformColor3fvAlpha(outline_col, outline_alpha * 0.5f);
  }
  imm_draw_circle_wire_2d(pos, translation[0], translation[1], final_radius, 40);

  immUnbindProgram();

  /* restore GL state */
  GPU_blend(false);
  GPU_line_smooth(false);
}

/* Public API */

void paint_cursor_start(bContext *C, bool (*poll)(bContext *C))
{
  Paint *p = BKE_paint_get_active_from_context(C);

  if (p && !p->paint_cursor) {
    p->paint_cursor = WM_paint_cursor_activate(
        CTX_wm_manager(C), SPACE_TYPE_ANY, RGN_TYPE_ANY, poll, paint_draw_cursor, NULL);
  }

  /* invalidate the paint cursors */
  BKE_paint_invalidate_overlay_all();
}

void paint_cursor_start_explicit(Paint *p, wmWindowManager *wm, bool (*poll)(bContext *C))
{
  if (p && !p->paint_cursor) {
    p->paint_cursor = WM_paint_cursor_activate(
        wm, SPACE_TYPE_ANY, RGN_TYPE_ANY, poll, paint_draw_cursor, NULL);
  }
}
