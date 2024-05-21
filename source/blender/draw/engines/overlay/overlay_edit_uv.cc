/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */
#include "DRW_render.hh"

#include "draw_cache_impl.hh"
#include "draw_manager_text.hh"

#include "BLI_math_color.h"

#include "BKE_customdata.hh"
#include "BKE_editmesh.hh"
#include "BKE_image.h"
#include "BKE_layer.hh"
#include "BKE_mask.h"
#include "BKE_mesh_types.hh"
#include "BKE_object.hh"
#include "BKE_paint.hh"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"

#include "DEG_depsgraph_query.hh"

#include "ED_image.hh"

#include "IMB_imbuf_types.hh"

#include "GPU_batch.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "overlay_private.hh"

using blender::Vector;

/* Forward declarations. */
static void overlay_edit_uv_cache_populate(OVERLAY_Data *vedata, Object &ob);

struct OVERLAY_StretchingAreaTotals {
  void *next, *prev;
  float *total_area;
  float *total_area_uv;
};

static OVERLAY_UVLineStyle edit_uv_line_style_from_space_image(const SpaceImage *sima)
{
  const bool is_uv_editor = sima->mode == SI_MODE_UV;
  if (is_uv_editor) {
    switch (sima->dt_uv) {
      case SI_UVDT_OUTLINE:
        return OVERLAY_UV_LINE_STYLE_OUTLINE;
      case SI_UVDT_BLACK:
        return OVERLAY_UV_LINE_STYLE_BLACK;
      case SI_UVDT_WHITE:
        return OVERLAY_UV_LINE_STYLE_WHITE;
      case SI_UVDT_DASH:
        return OVERLAY_UV_LINE_STYLE_DASH;
      default:
        return OVERLAY_UV_LINE_STYLE_BLACK;
    }
  }
  else {
    return OVERLAY_UV_LINE_STYLE_SHADOW;
  }
}

/* TODO(jbakker): the GPU texture should be cached with the mask. */
static GPUTexture *edit_uv_mask_texture(
    Mask *mask, const int width, const int height_, const float aspx, const float aspy)
{
  const int height = float(height_) * (aspy / aspx);
  MaskRasterHandle *handle;
  float *buffer = static_cast<float *>(MEM_mallocN(sizeof(float) * height * width, __func__));

  /* Initialize rasterization handle. */
  handle = BKE_maskrasterize_handle_new();
  BKE_maskrasterize_handle_init(handle, mask, width, height, true, true, true);

  BKE_maskrasterize_buffer(handle, width, height, buffer);

  /* Free memory. */
  BKE_maskrasterize_handle_free(handle);
  GPUTexture *texture = GPU_texture_create_2d(
      mask->id.name, width, height, 1, GPU_R16F, GPU_TEXTURE_USAGE_SHADER_READ, buffer);
  MEM_freeN(buffer);
  return texture;
}

/* -------------------------------------------------------------------- */
/** \name Internal API
 * \{ */

void OVERLAY_edit_uv_init(OVERLAY_Data *vedata)
{
  OVERLAY_StorageList *stl = vedata->stl;
  OVERLAY_PrivateData *pd = stl->pd;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  SpaceImage *sima = (SpaceImage *)draw_ctx->space_data;
  const Scene *scene = draw_ctx->scene;
  ToolSettings *ts = scene->toolsettings;
  const Brush *brush = BKE_paint_brush(&ts->imapaint.paint);
  const bool show_overlays = !pd->hide_overlays;

  Image *image = sima->image;
  /* By design no image is an image type. This so editor shows UVs by default. */
  const bool is_image_type = (image == nullptr) || ELEM(image->type,
                                                        IMA_TYPE_IMAGE,
                                                        IMA_TYPE_MULTILAYER,
                                                        IMA_TYPE_UV_TEST);
  const bool is_uv_editor = sima->mode == SI_MODE_UV;
  const bool has_edit_object = (draw_ctx->object_edit) != nullptr;
  const bool is_paint_mode = sima->mode == SI_MODE_PAINT;
  const bool is_view_mode = sima->mode == SI_MODE_VIEW;
  const bool is_mask_mode = sima->mode == SI_MODE_MASK;
  const bool is_edit_mode = draw_ctx->object_mode == OB_MODE_EDIT;
  const bool do_uv_overlay = is_image_type && is_uv_editor && has_edit_object;
  const bool show_modified_uvs = sima->flag & SI_DRAWSHADOW;
  const bool is_tiled_image = image && (image->source == IMA_SRC_TILED);
  const bool do_edges_only = (ts->uv_flag & UV_SYNC_SELECTION) ?
                                 /* NOTE: Ignore #SCE_SELECT_EDGE because a single selected edge
                                  * on the mesh may cause single UV vertices to be selected. */
                                 false :
                                 (ts->uv_selectmode == UV_SELECT_EDGE);
  const bool do_faces = ((sima->flag & SI_NO_DRAWFACES) == 0);
  const bool do_face_dots = (ts->uv_flag & UV_SYNC_SELECTION) ?
                                (ts->selectmode & SCE_SELECT_FACE) != 0 :
                                (ts->uv_selectmode == UV_SELECT_FACE);
  const bool do_uvstretching_overlay = is_image_type && is_uv_editor && is_edit_mode &&
                                       ((sima->flag & SI_DRAW_STRETCH) != 0);
  const bool do_tex_paint_shadows = (sima->flag & SI_NO_DRAW_TEXPAINT) == 0;
  const bool do_stencil_overlay = is_paint_mode && is_image_type && brush &&
                                  (brush->imagepaint_tool == PAINT_TOOL_CLONE) &&
                                  brush->clone.image;

  pd->edit_uv.do_verts = show_overlays && (!do_edges_only);
  pd->edit_uv.do_faces = show_overlays && do_faces && !do_uvstretching_overlay;
  pd->edit_uv.do_face_dots = show_overlays && do_faces && do_face_dots;
  pd->edit_uv.do_uv_overlay = show_overlays && do_uv_overlay;
  pd->edit_uv.do_uv_shadow_overlay = show_overlays && is_image_type &&
                                     ((is_paint_mode && do_tex_paint_shadows &&
                                       ((draw_ctx->object_mode &
                                         (OB_MODE_TEXTURE_PAINT | OB_MODE_EDIT)) != 0)) ||
                                      (is_uv_editor && do_tex_paint_shadows &&
                                       ((draw_ctx->object_mode & (OB_MODE_TEXTURE_PAINT)) != 0)) ||
                                      (is_view_mode && do_tex_paint_shadows &&
                                       ((draw_ctx->object_mode & (OB_MODE_TEXTURE_PAINT)) != 0)) ||
                                      (do_uv_overlay && (show_modified_uvs)));

  pd->edit_uv.do_mask_overlay = show_overlays && is_mask_mode &&
                                (sima->mask_info.mask != nullptr) &&
                                ((sima->mask_info.draw_flag & MASK_DRAWFLAG_OVERLAY) != 0);
  pd->edit_uv.mask_overlay_mode = eMaskOverlayMode(sima->mask_info.overlay_mode);
  pd->edit_uv.mask = sima->mask_info.mask ? (Mask *)DEG_get_evaluated_id(
                                                draw_ctx->depsgraph, &sima->mask_info.mask->id) :
                                            nullptr;

  pd->edit_uv.do_uv_stretching_overlay = show_overlays && do_uvstretching_overlay;
  pd->edit_uv.uv_opacity = sima->uv_opacity;
  pd->edit_uv.stretch_opacity = sima->stretch_opacity;
  pd->edit_uv.do_tiled_image_overlay = show_overlays && is_image_type && is_tiled_image;
  pd->edit_uv.do_tiled_image_border_overlay = is_image_type && is_tiled_image;
  pd->edit_uv.dash_length = 4.0f * UI_SCALE_FAC;
  pd->edit_uv.line_style = edit_uv_line_style_from_space_image(sima);
  pd->edit_uv.do_smooth_wire = ((U.gpu_flag & USER_GPU_FLAG_OVERLAY_SMOOTH_WIRE) != 0);
  pd->edit_uv.do_stencil_overlay = show_overlays && do_stencil_overlay;

  pd->edit_uv.draw_type = eSpaceImage_UVDT_Stretch(sima->dt_uvstretch);
  BLI_listbase_clear(&pd->edit_uv.totals);
  pd->edit_uv.total_area_ratio = 0.0f;

  /* During engine initialization phase the `sima` isn't locked and
   * we are able to retrieve the needed data.
   * During cache_init the image engine locks the `sima` and makes it impossible
   * to retrieve the data. */
  ED_space_image_get_uv_aspect(sima, &pd->edit_uv.uv_aspect[0], &pd->edit_uv.uv_aspect[1]);
  ED_space_image_get_size(sima, &pd->edit_uv.image_size[0], &pd->edit_uv.image_size[1]);
  ED_space_image_get_aspect(sima, &pd->edit_uv.image_aspect[0], &pd->edit_uv.image_aspect[1]);
}

void OVERLAY_edit_uv_cache_init(OVERLAY_Data *vedata)
{
  using namespace blender::draw;
  OVERLAY_StorageList *stl = vedata->stl;
  OVERLAY_PassList *psl = vedata->psl;
  OVERLAY_PrivateData *pd = stl->pd;

  const DRWContextState *draw_ctx = DRW_context_state_get();
  SpaceImage *sima = (SpaceImage *)draw_ctx->space_data;
  ::Image *image = sima->image;
  const Scene *scene = draw_ctx->scene;
  ToolSettings *ts = scene->toolsettings;

  if (pd->edit_uv.do_uv_overlay || pd->edit_uv.do_uv_shadow_overlay) {
    /* uv edges */
    {
      DRW_PASS_CREATE(psl->edit_uv_edges_ps,
                      DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL |
                          DRW_STATE_BLEND_ALPHA);
      const bool do_edges_only = (ts->uv_flag & UV_SYNC_SELECTION) ?
                                     false :
                                     (ts->uv_selectmode & UV_SELECT_EDGE);
      GPUShader *sh = do_edges_only ? OVERLAY_shader_edit_uv_edges_for_edge_select_get() :
                                      OVERLAY_shader_edit_uv_edges_get();
      if (pd->edit_uv.do_uv_shadow_overlay) {
        pd->edit_uv_shadow_edges_grp = DRW_shgroup_create(sh, psl->edit_uv_edges_ps);
        DRW_shgroup_uniform_block(pd->edit_uv_shadow_edges_grp, "globalsBlock", G_draw.block_ubo);
        DRW_shgroup_uniform_int_copy(
            pd->edit_uv_shadow_edges_grp, "lineStyle", OVERLAY_UV_LINE_STYLE_SHADOW);
        DRW_shgroup_uniform_float_copy(
            pd->edit_uv_shadow_edges_grp, "alpha", pd->edit_uv.uv_opacity);
        DRW_shgroup_uniform_float(
            pd->edit_uv_shadow_edges_grp, "dashLength", &pd->edit_uv.dash_length, 1);
        DRW_shgroup_uniform_bool(
            pd->edit_uv_shadow_edges_grp, "doSmoothWire", &pd->edit_uv.do_smooth_wire, 1);
      }

      if (pd->edit_uv.do_uv_overlay) {
        pd->edit_uv_edges_grp = DRW_shgroup_create(sh, psl->edit_uv_edges_ps);
        DRW_shgroup_uniform_block(pd->edit_uv_edges_grp, "globalsBlock", G_draw.block_ubo);
        DRW_shgroup_uniform_int_copy(pd->edit_uv_edges_grp, "lineStyle", pd->edit_uv.line_style);
        DRW_shgroup_uniform_float_copy(pd->edit_uv_edges_grp, "alpha", pd->edit_uv.uv_opacity);
        DRW_shgroup_uniform_float(
            pd->edit_uv_edges_grp, "dashLength", &pd->edit_uv.dash_length, 1);
        DRW_shgroup_uniform_bool(
            pd->edit_uv_edges_grp, "doSmoothWire", &pd->edit_uv.do_smooth_wire, 1);
      }
    }
  }

  if (pd->edit_uv.do_uv_overlay) {
    if (pd->edit_uv.do_verts || pd->edit_uv.do_face_dots) {
      DRW_PASS_CREATE(psl->edit_uv_verts_ps,
                      DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL |
                          DRW_STATE_BLEND_ALPHA);
    }

    /* uv verts */
    if (pd->edit_uv.do_verts) {
      GPUShader *sh = OVERLAY_shader_edit_uv_verts_get();
      pd->edit_uv_verts_grp = DRW_shgroup_create(sh, psl->edit_uv_verts_ps);

      const float point_size = UI_GetThemeValuef(TH_VERTEX_SIZE) * UI_SCALE_FAC;

      DRW_shgroup_uniform_block(pd->edit_uv_verts_grp, "globalsBlock", G_draw.block_ubo);
      DRW_shgroup_uniform_float_copy(
          pd->edit_uv_verts_grp, "pointSize", (point_size + 1.5f) * M_SQRT2);
      DRW_shgroup_uniform_float_copy(pd->edit_uv_verts_grp, "outlineWidth", 0.75f);
      float theme_color[4];
      UI_GetThemeColor4fv(TH_VERTEX, theme_color);
      srgb_to_linearrgb_v4(theme_color, theme_color);
      DRW_shgroup_uniform_vec4_copy(pd->edit_uv_verts_grp, "color", theme_color);
    }

    /* uv faces */
    if (pd->edit_uv.do_faces) {
      DRW_PASS_CREATE(psl->edit_uv_faces_ps,
                      DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_ALWAYS | DRW_STATE_BLEND_ALPHA);
      GPUShader *sh = OVERLAY_shader_edit_uv_face_get();
      pd->edit_uv_faces_grp = DRW_shgroup_create(sh, psl->edit_uv_faces_ps);
      DRW_shgroup_uniform_block(pd->edit_uv_faces_grp, "globalsBlock", G_draw.block_ubo);
      DRW_shgroup_uniform_float(pd->edit_uv_faces_grp, "uvOpacity", &pd->edit_uv.uv_opacity, 1);
    }

    /* uv face dots */
    if (pd->edit_uv.do_face_dots) {
      const float point_size = UI_GetThemeValuef(TH_FACEDOT_SIZE) * UI_SCALE_FAC;
      GPUShader *sh = OVERLAY_shader_edit_uv_face_dots_get();
      pd->edit_uv_face_dots_grp = DRW_shgroup_create(sh, psl->edit_uv_verts_ps);
      DRW_shgroup_uniform_block(pd->edit_uv_face_dots_grp, "globalsBlock", G_draw.block_ubo);
      DRW_shgroup_uniform_float_copy(pd->edit_uv_face_dots_grp, "pointSize", point_size);
    }
  }

  /* uv stretching */
  if (pd->edit_uv.do_uv_stretching_overlay) {
    DRW_PASS_CREATE(psl->edit_uv_stretching_ps,
                    DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_ALWAYS | DRW_STATE_BLEND_ALPHA);
    if (pd->edit_uv.draw_type == SI_UVDT_STRETCH_ANGLE) {
      GPUShader *sh = OVERLAY_shader_edit_uv_stretching_angle_get();
      pd->edit_uv_stretching_grp = DRW_shgroup_create(sh, psl->edit_uv_stretching_ps);
      DRW_shgroup_uniform_block(pd->edit_uv_stretching_grp, "globalsBlock", G_draw.block_ubo);
      DRW_shgroup_uniform_vec2_copy(pd->edit_uv_stretching_grp, "aspect", pd->edit_uv.uv_aspect);
      DRW_shgroup_uniform_float_copy(
          pd->edit_uv_stretching_grp, "stretch_opacity", pd->edit_uv.stretch_opacity);
    }
    else /* SI_UVDT_STRETCH_AREA */ {
      GPUShader *sh = OVERLAY_shader_edit_uv_stretching_area_get();
      pd->edit_uv_stretching_grp = DRW_shgroup_create(sh, psl->edit_uv_stretching_ps);
      DRW_shgroup_uniform_block(pd->edit_uv_stretching_grp, "globalsBlock", G_draw.block_ubo);
      DRW_shgroup_uniform_float(
          pd->edit_uv_stretching_grp, "totalAreaRatio", &pd->edit_uv.total_area_ratio, 1);
      DRW_shgroup_uniform_float_copy(
          pd->edit_uv_stretching_grp, "stretch_opacity", pd->edit_uv.stretch_opacity);
    }
  }

  if (pd->edit_uv.do_tiled_image_border_overlay) {
    blender::gpu::Batch *geom = DRW_cache_quad_wires_get();
    float obmat[4][4];
    unit_m4(obmat);

    DRW_PASS_CREATE(psl->edit_uv_tiled_image_borders_ps,
                    DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_ALWAYS);
    GPUShader *sh = OVERLAY_shader_edit_uv_tiled_image_borders_get();

    float theme_color[4], selected_color[4];
    UI_GetThemeColorShade4fv(TH_BACK, 60, theme_color);
    UI_GetThemeColor4fv(TH_FACE_SELECT, selected_color);
    srgb_to_linearrgb_v4(theme_color, theme_color);
    srgb_to_linearrgb_v4(selected_color, selected_color);

    DRWShadingGroup *grp = DRW_shgroup_create(sh, psl->edit_uv_tiled_image_borders_ps);
    DRW_shgroup_uniform_vec4_copy(grp, "ucolor", theme_color);
    const float3 offset = {0.0f, 0.0f, 0.0f};
    DRW_shgroup_uniform_vec3_copy(grp, "offset", offset);

    LISTBASE_FOREACH (ImageTile *, tile, &image->tiles) {
      const int tile_x = ((tile->tile_number - 1001) % 10);
      const int tile_y = ((tile->tile_number - 1001) / 10);
      obmat[3][1] = float(tile_y);
      obmat[3][0] = float(tile_x);
      DRW_shgroup_call_obmat(grp, geom, obmat);
    }
    /* Only mark active border when overlays are enabled. */
    if (pd->edit_uv.do_tiled_image_overlay) {
      /* Active tile border */
      ImageTile *active_tile = static_cast<ImageTile *>(
          BLI_findlink(&image->tiles, image->active_tile_index));
      if (active_tile) {
        obmat[3][0] = float((active_tile->tile_number - 1001) % 10);
        obmat[3][1] = float((active_tile->tile_number - 1001) / 10);
        grp = DRW_shgroup_create(sh, psl->edit_uv_tiled_image_borders_ps);
        DRW_shgroup_uniform_vec4_copy(grp, "ucolor", selected_color);
        DRW_shgroup_call_obmat(grp, geom, obmat);
      }
    }
  }

  if (pd->edit_uv.do_tiled_image_overlay) {
    DRWTextStore *dt = DRW_text_cache_ensure();
    uchar color[4];
    /* Color Management: Exception here as texts are drawn in sRGB space directly. */
    UI_GetThemeColorShade4ubv(TH_BACK, 60, color);
    char text[16];
    LISTBASE_FOREACH (ImageTile *, tile, &image->tiles) {
      BLI_snprintf(text, 5, "%d", tile->tile_number);
      float tile_location[3] = {
          float((tile->tile_number - 1001) % 10), float((tile->tile_number - 1001) / 10), 0.0f};
      DRW_text_cache_add(
          dt, tile_location, text, strlen(text), 10, 10, DRW_TEXT_CACHE_GLOBALSPACE, color);
    }
  }

  if (pd->edit_uv.do_stencil_overlay) {
    const Brush *brush = BKE_paint_brush(&ts->imapaint.paint);
    ::Image *stencil_image = brush->clone.image;
    GPUTexture *stencil_texture = BKE_image_get_gpu_texture(stencil_image, nullptr);

    if (stencil_texture != nullptr) {
      DRW_PASS_CREATE(psl->edit_uv_stencil_ps,
                      DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_ALWAYS |
                          DRW_STATE_BLEND_ALPHA_PREMUL);
      GPUShader *sh = OVERLAY_shader_edit_uv_stencil_image();
      blender::gpu::Batch *geom = DRW_cache_quad_get();
      DRWShadingGroup *grp = DRW_shgroup_create(sh, psl->edit_uv_stencil_ps);
      DRW_shgroup_uniform_texture(grp, "imgTexture", stencil_texture);
      DRW_shgroup_uniform_bool_copy(grp, "imgPremultiplied", true);
      DRW_shgroup_uniform_bool_copy(grp, "imgAlphaBlend", true);
      const float4 color = {1.0f, 1.0f, 1.0f, brush->clone.alpha};
      DRW_shgroup_uniform_vec4_copy(grp, "ucolor", color);

      float size_image[2];
      BKE_image_get_size_fl(image, nullptr, size_image);
      float size_stencil_image[2] = {float(GPU_texture_original_width(stencil_texture)),
                                     float(GPU_texture_original_height(stencil_texture))};

      float obmat[4][4];
      unit_m4(obmat);
      obmat[3][1] = brush->clone.offset[1];
      obmat[3][0] = brush->clone.offset[0];
      obmat[0][0] = size_stencil_image[0] / size_image[0];
      obmat[1][1] = size_stencil_image[1] / size_image[1];

      DRW_shgroup_call_obmat(grp, geom, obmat);
    }
  }

  if (pd->edit_uv.do_mask_overlay) {
    const bool is_combined_overlay = pd->edit_uv.mask_overlay_mode == MASK_OVERLAY_COMBINED;
    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_ALWAYS;
    state |= is_combined_overlay ? DRW_STATE_BLEND_MUL : DRW_STATE_BLEND_ALPHA;
    DRW_PASS_CREATE(psl->edit_uv_mask_ps, state);

    GPUShader *sh = OVERLAY_shader_edit_uv_mask_image();
    blender::gpu::Batch *geom = DRW_cache_quad_get();
    DRWShadingGroup *grp = DRW_shgroup_create(sh, psl->edit_uv_mask_ps);
    GPUTexture *mask_texture = edit_uv_mask_texture(pd->edit_uv.mask,
                                                    pd->edit_uv.image_size[0],
                                                    pd->edit_uv.image_size[1],
                                                    pd->edit_uv.image_aspect[1],
                                                    pd->edit_uv.image_aspect[1]);
    pd->edit_uv.mask_texture = mask_texture;
    DRW_shgroup_uniform_texture(grp, "imgTexture", mask_texture);
    const float4 color = {1.0f, 1.0f, 1.0f, 1.0f};
    DRW_shgroup_uniform_vec4_copy(grp, "color", color);
    DRW_shgroup_call_obmat(grp, geom, nullptr);
  }

  /* HACK: When editing objects that share the same mesh we should only draw the
   * first one in the order that is used during uv editing. We can only trust that the first object
   * has the correct batches with the correct selection state. See #83187. */
  if ((pd->edit_uv.do_uv_overlay || pd->edit_uv.do_uv_shadow_overlay) &&
      draw_ctx->obact->type == OB_MESH)
  {
    Vector<Object *> objects = BKE_view_layer_array_from_objects_in_mode_unique_data(
        draw_ctx->scene, draw_ctx->view_layer, nullptr, draw_ctx->object_mode);
    for (Object *object : objects) {
      Object *object_eval = DEG_get_evaluated_object(draw_ctx->depsgraph, object);
      DRW_mesh_batch_cache_validate(*object_eval, *(Mesh *)object_eval->data);
      overlay_edit_uv_cache_populate(vedata, *object_eval);
    }
  }
}

static void overlay_edit_uv_cache_populate(OVERLAY_Data *vedata, Object &ob)
{
  using namespace blender::draw;
  if (!(DRW_object_visibility_in_active_context(&ob) & OB_VISIBLE_SELF)) {
    return;
  }

  OVERLAY_StorageList *stl = vedata->stl;
  OVERLAY_PrivateData *pd = stl->pd;
  blender::gpu::Batch *geom;

  const DRWContextState *draw_ctx = DRW_context_state_get();
  const bool is_edit_object = DRW_object_is_in_edit_mode(&ob);
  Mesh &mesh = *(Mesh *)ob.data;
  const bool has_active_object_uvmap = CustomData_get_active_layer(&mesh.corner_data,
                                                                   CD_PROP_FLOAT2) != -1;
  const bool has_active_edit_uvmap = is_edit_object && (CustomData_get_active_layer(
                                                            &mesh.runtime->edit_mesh->bm->ldata,
                                                            CD_PROP_FLOAT2) != -1);
  const bool draw_shadows = (draw_ctx->object_mode != OB_MODE_OBJECT) &&
                            (ob.mode == draw_ctx->object_mode);

  if (has_active_edit_uvmap) {
    if (pd->edit_uv.do_uv_overlay) {
      geom = DRW_mesh_batch_cache_get_edituv_edges(ob, mesh);
      if (geom) {
        DRW_shgroup_call_obmat(pd->edit_uv_edges_grp, geom, nullptr);
      }
      if (pd->edit_uv.do_verts) {
        geom = DRW_mesh_batch_cache_get_edituv_verts(ob, mesh);
        if (geom) {
          DRW_shgroup_call_obmat(pd->edit_uv_verts_grp, geom, nullptr);
        }
      }
      if (pd->edit_uv.do_faces) {
        geom = DRW_mesh_batch_cache_get_edituv_faces(ob, mesh);
        if (geom) {
          DRW_shgroup_call_obmat(pd->edit_uv_faces_grp, geom, nullptr);
        }
      }
      if (pd->edit_uv.do_face_dots) {
        geom = DRW_mesh_batch_cache_get_edituv_facedots(ob, mesh);
        if (geom) {
          DRW_shgroup_call_obmat(pd->edit_uv_face_dots_grp, geom, nullptr);
        }
      }
    }

    if (pd->edit_uv.do_uv_stretching_overlay) {
      if (pd->edit_uv.draw_type == SI_UVDT_STRETCH_ANGLE) {
        geom = DRW_mesh_batch_cache_get_edituv_faces_stretch_angle(ob, mesh);
      }
      else /* SI_UVDT_STRETCH_AREA */ {
        OVERLAY_StretchingAreaTotals *totals = static_cast<OVERLAY_StretchingAreaTotals *>(
            MEM_mallocN(sizeof(OVERLAY_StretchingAreaTotals), __func__));
        BLI_addtail(&pd->edit_uv.totals, totals);
        geom = DRW_mesh_batch_cache_get_edituv_faces_stretch_area(
            ob, mesh, &totals->total_area, &totals->total_area_uv);
      }
      if (geom) {
        DRW_shgroup_call_obmat(pd->edit_uv_stretching_grp, geom, nullptr);
      }
    }
  }

  if (draw_shadows && (has_active_object_uvmap || has_active_edit_uvmap)) {
    if (pd->edit_uv.do_uv_shadow_overlay) {
      geom = DRW_mesh_batch_cache_get_uv_edges(ob, mesh);
      if (geom) {
        DRW_shgroup_call_obmat(pd->edit_uv_shadow_edges_grp, geom, nullptr);
      }
    }
  }
}

static void edit_uv_stretching_update_ratios(OVERLAY_Data *vedata)
{
  OVERLAY_StorageList *stl = vedata->stl;
  OVERLAY_PrivateData *pd = stl->pd;

  if (pd->edit_uv.draw_type == SI_UVDT_STRETCH_AREA) {
    float total_area = 0.0f;
    float total_area_uv = 0.0f;

    LISTBASE_FOREACH (OVERLAY_StretchingAreaTotals *, totals, &pd->edit_uv.totals) {
      total_area += *totals->total_area;
      total_area_uv += *totals->total_area_uv;
    }

    if (total_area > FLT_EPSILON && total_area_uv > FLT_EPSILON) {
      pd->edit_uv.total_area_ratio = total_area / total_area_uv;
    }
  }
  BLI_freelistN(&pd->edit_uv.totals);
}

void OVERLAY_edit_uv_cache_finish(OVERLAY_Data *vedata)
{
  OVERLAY_StorageList *stl = vedata->stl;
  OVERLAY_PrivateData *pd = stl->pd;

  if (pd->edit_uv.do_uv_stretching_overlay) {
    edit_uv_stretching_update_ratios(vedata);
  }
}

static void OVERLAY_edit_uv_draw_finish(OVERLAY_Data *vedata)
{
  OVERLAY_StorageList *stl = vedata->stl;
  OVERLAY_PrivateData *pd = stl->pd;

  DRW_TEXTURE_FREE_SAFE(pd->edit_uv.mask_texture);
}

void OVERLAY_edit_uv_draw(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;
  OVERLAY_StorageList *stl = vedata->stl;
  OVERLAY_PrivateData *pd = stl->pd;

  if (pd->edit_uv.do_tiled_image_border_overlay) {
    DRW_draw_pass(psl->edit_uv_tiled_image_borders_ps);
  }
  if (pd->edit_uv.do_mask_overlay) {
    /* Combined overlay renders in the default framebuffer and modifies the image in SRS.
     * The alpha overlay renders in the overlay framebuffer. */
    const bool is_combined_overlay = pd->edit_uv.mask_overlay_mode == MASK_OVERLAY_COMBINED;
    GPUFrameBuffer *previous_framebuffer = nullptr;
    if (is_combined_overlay) {
      DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();
      previous_framebuffer = GPU_framebuffer_active_get();
      GPU_framebuffer_bind(dfbl->default_fb);
    }
    DRW_draw_pass(psl->edit_uv_mask_ps);
    if (previous_framebuffer) {
      GPU_framebuffer_bind(previous_framebuffer);
    }
  }

  if (pd->edit_uv.do_uv_stretching_overlay) {
    DRW_draw_pass(psl->edit_uv_stretching_ps);
  }

  if (pd->edit_uv.do_uv_overlay) {
    if (pd->edit_uv.do_faces) {
      DRW_draw_pass(psl->edit_uv_faces_ps);
    }
    DRW_draw_pass(psl->edit_uv_edges_ps);

    DRW_draw_pass(psl->edit_uv_verts_ps);
  }
  else if (pd->edit_uv.do_uv_shadow_overlay) {
    DRW_draw_pass(psl->edit_uv_edges_ps);
  }
  if (pd->edit_uv.do_stencil_overlay) {
    DRW_draw_pass(psl->edit_uv_stencil_ps);
  }
  OVERLAY_edit_uv_draw_finish(vedata);
}

/** \} */
