/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "DNA_texture_types.h"
#include "DNA_view3d_types.h"
#include "DRW_render.hh"

#include "GPU_texture.hh"
#include "GPU_uniform_buffer.hh"

#include "UI_resources.hh"

#include "BLI_index_range.hh"
#include "BLI_math_color.h"
#include "BLI_math_vector.h"

#include "BKE_colorband.hh"

#include "draw_common_c.hh"

#if 0
#  define UI_COLOR_RGB_FROM_U8(r, g, b, v4) \
    ARRAY_SET_ITEMS(v4, float(r) / 255.0f, float(g) / 255.0f, float(b) / 255.0f, 1.0)
#endif
#define UI_COLOR_RGBA_FROM_U8(r, g, b, a, v4) \
  ARRAY_SET_ITEMS(v4, float(r) / 255.0f, float(g) / 255.0f, float(b) / 255.0f, float(a) / 255.0f)

/**
 * Colors & Constant.
 */

DRW_Global G_draw{};

static bool weight_ramp_custom = false;
static ColorBand weight_ramp_copy;

static GPUTexture *DRW_create_weight_colorramp_texture();

using namespace blender;

void DRW_globals_update()
{
  GlobalsUboStorage *gb = &G_draw.block;

  const DRWContextState *ctx = DRW_context_state_get();
  if (ctx->rv3d != nullptr) {
    int plane_len = (RV3D_LOCK_FLAGS(ctx->rv3d) & RV3D_BOXCLIP) ? 4 : 6;
    for (auto i : IndexRange(plane_len)) {
      gb->clip_planes[i] = float4(ctx->rv3d->clip[i]);
    }
    if (plane_len < 6) {
      for (auto i : IndexRange(plane_len, 6 - plane_len)) {
        /* Fill other planes with same valid planes. Avoid changing further logic. */
        gb->clip_planes[i] = gb->clip_planes[plane_len - 1];
      }
    }
  }

  UI_GetThemeColor4fv(TH_WIRE, gb->color_wire);
  UI_GetThemeColor4fv(TH_WIRE_EDIT, gb->color_wire_edit);
  UI_GetThemeColor4fv(TH_ACTIVE, gb->color_active);
  UI_GetThemeColor4fv(TH_SELECT, gb->color_select);
  UI_COLOR_RGBA_FROM_U8(0x88, 0xFF, 0xFF, 155, gb->color_library_select);
  UI_COLOR_RGBA_FROM_U8(0x55, 0xCC, 0xCC, 155, gb->color_library);
  UI_GetThemeColor4fv(TH_TRANSFORM, gb->color_transform);
  UI_GetThemeColor4fv(TH_LIGHT, gb->color_light);
  UI_GetThemeColor4fv(TH_SPEAKER, gb->color_speaker);
  UI_GetThemeColor4fv(TH_CAMERA, gb->color_camera);
  UI_GetThemeColor4fv(TH_CAMERA_PATH, gb->color_camera_path);
  UI_GetThemeColor4fv(TH_EMPTY, gb->color_empty);
  UI_GetThemeColor4fv(TH_VERTEX, gb->color_vertex);
  UI_GetThemeColor4fv(TH_VERTEX_SELECT, gb->color_vertex_select);
  UI_GetThemeColor4fv(TH_VERTEX_UNREFERENCED, gb->color_vertex_unreferenced);
  UI_COLOR_RGBA_FROM_U8(0xB0, 0x00, 0xB0, 0xFF, gb->color_vertex_missing_data);
  UI_GetThemeColor4fv(TH_EDITMESH_ACTIVE, gb->color_edit_mesh_active);
  UI_GetThemeColor4fv(TH_EDGE_SELECT, gb->color_edge_select);
  UI_GetThemeColor4fv(TH_EDGE_MODE_SELECT, gb->color_edge_mode_select);
  UI_GetThemeColor4fv(TH_GP_VERTEX, gb->color_gpencil_vertex);
  UI_GetThemeColor4fv(TH_GP_VERTEX_SELECT, gb->color_gpencil_vertex_select);

  UI_GetThemeColor4fv(TH_EDGE_SEAM, gb->color_edge_seam);
  UI_GetThemeColor4fv(TH_EDGE_SHARP, gb->color_edge_sharp);
  UI_GetThemeColor4fv(TH_EDGE_CREASE, gb->color_edge_crease);
  UI_GetThemeColor4fv(TH_EDGE_BEVEL, gb->color_edge_bweight);
  UI_GetThemeColor4fv(TH_EDGE_FACESEL, gb->color_edge_face_select);
  UI_GetThemeColor4fv(TH_FACE, gb->color_face);
  UI_GetThemeColor4fv(TH_FACE_SELECT, gb->color_face_select);
  UI_GetThemeColor4fv(TH_FACE_MODE_SELECT, gb->color_face_mode_select);
  UI_GetThemeColor4fv(TH_FACE_RETOPOLOGY, gb->color_face_retopology);
  UI_GetThemeColor4fv(TH_FACE_BACK, gb->color_face_back);
  UI_GetThemeColor4fv(TH_FACE_FRONT, gb->color_face_front);
  UI_GetThemeColor4fv(TH_NORMAL, gb->color_normal);
  UI_GetThemeColor4fv(TH_VNORMAL, gb->color_vnormal);
  UI_GetThemeColor4fv(TH_LNORMAL, gb->color_lnormal);
  UI_GetThemeColor4fv(TH_FACE_DOT, gb->color_facedot);
  UI_GetThemeColor4fv(TH_SKIN_ROOT, gb->color_skinroot);
  UI_GetThemeColor4fv(TH_BACK, gb->color_background);
  UI_GetThemeColor4fv(TH_BACK_GRAD, gb->color_background_gradient);
  UI_GetThemeColor4fv(TH_TRANSPARENT_CHECKER_PRIMARY, gb->color_checker_primary);
  UI_GetThemeColor4fv(TH_TRANSPARENT_CHECKER_SECONDARY, gb->color_checker_secondary);
  gb->size_checker = UI_GetThemeValuef(TH_TRANSPARENT_CHECKER_SIZE);
  gb->fresnel_mix_edit = ((U.gpu_flag & USER_GPU_FLAG_FRESNEL_EDIT) == 0) ? 0.0f : 1.0f;
  UI_GetThemeColor4fv(TH_V3D_CLIPPING_BORDER, gb->color_clipping_border);

  /* Custom median color to slightly affect the edit mesh colors. */
  interp_v4_v4v4(gb->color_edit_mesh_middle, gb->color_vertex_select, gb->color_wire_edit, 0.35f);
  copy_v3_fl(gb->color_edit_mesh_middle,
             dot_v3v3(gb->color_edit_mesh_middle,
                      blender::float3{0.3333f, 0.3333f, 0.3333f})); /* Desaturate */

#ifdef WITH_FREESTYLE
  UI_GetThemeColor4fv(TH_FREESTYLE_EDGE_MARK, gb->color_edge_freestyle);
  UI_GetThemeColor4fv(TH_FREESTYLE_FACE_MARK, gb->color_face_freestyle);
#else
  zero_v4(gb->color_edge_freestyle);
  zero_v4(gb->color_face_freestyle);
#endif

  UI_GetThemeColor4fv(TH_TEXT, gb->color_text);
  UI_GetThemeColor4fv(TH_TEXT_HI, gb->color_text_hi);

  /* Bone colors */
  UI_GetThemeColor4fv(TH_BONE_POSE, gb->color_bone_pose);
  UI_GetThemeColor4fv(TH_BONE_POSE_ACTIVE, gb->color_bone_pose_active);
  UI_GetThemeColorShade4fv(TH_EDGE_SELECT, 60, gb->color_bone_active);
  UI_GetThemeColorShade4fv(TH_EDGE_SELECT, -20, gb->color_bone_select);
  UI_GetThemeColorBlendShade4fv(TH_WIRE, TH_BONE_POSE, 0.15f, 0, gb->color_bone_pose_active_unsel);
  UI_GetThemeColorBlendShade3fv(
      TH_WIRE_EDIT, TH_EDGE_SELECT, 0.15f, 0, gb->color_bone_active_unsel);
  UI_COLOR_RGBA_FROM_U8(255, 150, 0, 80, gb->color_bone_pose_no_target);
  UI_COLOR_RGBA_FROM_U8(255, 255, 0, 80, gb->color_bone_pose_ik);
  UI_COLOR_RGBA_FROM_U8(200, 255, 0, 80, gb->color_bone_pose_spline_ik);
  UI_COLOR_RGBA_FROM_U8(0, 255, 120, 80, gb->color_bone_pose_constraint);
  UI_GetThemeColor4fv(TH_BONE_SOLID, gb->color_bone_solid);
  UI_GetThemeColor4fv(TH_BONE_LOCKED_WEIGHT, gb->color_bone_locked);
  copy_v4_fl4(gb->color_bone_ik_line, 0.8f, 0.8f, 0.0f, 1.0f);
  copy_v4_fl4(gb->color_bone_ik_line_no_target, 0.8f, 0.5f, 0.2f, 1.0f);
  copy_v4_fl4(gb->color_bone_ik_line_spline, 0.8f, 0.8f, 0.2f, 1.0f);

  /* Curve */
  UI_GetThemeColor4fv(TH_HANDLE_FREE, gb->color_handle_free);
  UI_GetThemeColor4fv(TH_HANDLE_AUTO, gb->color_handle_auto);
  UI_GetThemeColor4fv(TH_HANDLE_VECT, gb->color_handle_vect);
  UI_GetThemeColor4fv(TH_HANDLE_ALIGN, gb->color_handle_align);
  UI_GetThemeColor4fv(TH_HANDLE_AUTOCLAMP, gb->color_handle_autoclamp);
  UI_GetThemeColor4fv(TH_HANDLE_SEL_FREE, gb->color_handle_sel_free);
  UI_GetThemeColor4fv(TH_HANDLE_SEL_AUTO, gb->color_handle_sel_auto);
  UI_GetThemeColor4fv(TH_HANDLE_SEL_VECT, gb->color_handle_sel_vect);
  UI_GetThemeColor4fv(TH_HANDLE_SEL_ALIGN, gb->color_handle_sel_align);
  UI_GetThemeColor4fv(TH_HANDLE_SEL_AUTOCLAMP, gb->color_handle_sel_autoclamp);
  UI_GetThemeColor4fv(TH_NURB_ULINE, gb->color_nurb_uline);
  UI_GetThemeColor4fv(TH_NURB_VLINE, gb->color_nurb_vline);
  UI_GetThemeColor4fv(TH_NURB_SEL_ULINE, gb->color_nurb_sel_uline);
  UI_GetThemeColor4fv(TH_NURB_SEL_VLINE, gb->color_nurb_sel_vline);
  UI_GetThemeColor4fv(TH_ACTIVE_SPLINE, gb->color_active_spline);

  UI_GetThemeColor4fv(TH_CFRAME, gb->color_current_frame);
  UI_GetThemeColor4fv(TH_FRAME_BEFORE, gb->color_before_frame);
  UI_GetThemeColor4fv(TH_FRAME_AFTER, gb->color_after_frame);

  /* Meta-ball. */
  UI_COLOR_RGBA_FROM_U8(0xA0, 0x30, 0x30, 0xFF, gb->color_mball_radius);
  UI_COLOR_RGBA_FROM_U8(0xF0, 0xA0, 0xA0, 0xFF, gb->color_mball_radius_select);
  UI_COLOR_RGBA_FROM_U8(0x30, 0xA0, 0x30, 0xFF, gb->color_mball_stiffness);
  UI_COLOR_RGBA_FROM_U8(0xA0, 0xF0, 0xA0, 0xFF, gb->color_mball_stiffness_select);

  /* Grid */
  UI_GetThemeColorShade4fv(TH_GRID, 10, gb->color_grid);
  /* Emphasize division lines lighter instead of darker, if background is darker than grid. */
  UI_GetThemeColorShade4fv(
      TH_GRID,
      (gb->color_grid[0] + gb->color_grid[1] + gb->color_grid[2] + 0.12f >
       gb->color_background[0] + gb->color_background[1] + gb->color_background[2]) ?
          20 :
          -10,
      gb->color_grid_emphasis);
  /* Grid Axis */
  UI_GetThemeColorBlendShade4fv(TH_GRID, TH_AXIS_X, 0.5f, -10, gb->color_grid_axis_x);
  UI_GetThemeColorBlendShade4fv(TH_GRID, TH_AXIS_Y, 0.5f, -10, gb->color_grid_axis_y);
  UI_GetThemeColorBlendShade4fv(TH_GRID, TH_AXIS_Z, 0.5f, -10, gb->color_grid_axis_z);

  UI_GetThemeColorShadeAlpha4fv(TH_TRANSFORM, 0, -80, gb->color_deselect);
  UI_GetThemeColorShadeAlpha4fv(TH_WIRE, 0, -30, gb->color_outline);
  UI_GetThemeColorShadeAlpha4fv(TH_LIGHT, 0, 255, gb->color_light_no_alpha);

  /* UV colors */
  UI_GetThemeColor4fv(TH_UV_SHADOW, gb->color_uv_shadow);

  gb->size_pixel = U.pixelsize;
  gb->size_object_center = (UI_GetThemeValuef(TH_OBCENTER_DIA) + 1.0f) * U.pixelsize;
  gb->size_light_center = (UI_GetThemeValuef(TH_OBCENTER_DIA) + 1.5f) * U.pixelsize;
  gb->size_light_circle = U.pixelsize * 9.0f;
  gb->size_light_circle_shadow = gb->size_light_circle + U.pixelsize * 3.0f;

  /* M_SQRT2 to be at least the same size of the old square */
  gb->size_vertex = U.pixelsize *
                    max_ff(1.0f, UI_GetThemeValuef(TH_VERTEX_SIZE) * float(M_SQRT2) / 2.0f);
  gb->size_vertex_gpencil = U.pixelsize * UI_GetThemeValuef(TH_GP_VERTEX_SIZE);
  gb->size_face_dot = U.pixelsize * UI_GetThemeValuef(TH_FACEDOT_SIZE);
  gb->size_edge = U.pixelsize * max_ff(1.0f, UI_GetThemeValuef(TH_EDGE_WIDTH)) / 2.0f;
  gb->size_edge_fix = U.pixelsize * (0.5f + 2.0f * (1.0f * (gb->size_edge * float(M_SQRT1_2))));

  gb->pixel_fac = *DRW_viewport_pixelsize_get();

  copy_v2_v2(&gb->size_viewport[0], DRW_viewport_size_get());
  copy_v2_v2(&gb->size_viewport[2], &gb->size_viewport[0]);
  invert_v2(&gb->size_viewport[2]);

  /* Color management. */
  {
    float *color = gb->UBO_FIRST_COLOR;
    do {
      /* TODO: more accurate transform. */
      srgb_to_linearrgb_v4(color, color);
      color += 4;
    } while (color <= gb->UBO_LAST_COLOR);
  }

  if (G_draw.block_ubo == nullptr) {
    G_draw.block_ubo = GPU_uniformbuf_create_ex(
        sizeof(GlobalsUboStorage), gb, "GlobalsUboStorage");
  }

  if (ctx->v3d) {
    const View3DShading &shading = ctx->v3d->shading;
    gb->backface_culling = (shading.type == OB_SOLID) &&
                           (shading.flag & V3D_SHADING_BACKFACE_CULLING);
  }
  else {
    gb->backface_culling = false;
  }

  GPU_uniformbuf_update(G_draw.block_ubo, gb);

  if (!G_draw.ramp) {
    ColorBand ramp = {0};
    float *colors;
    int col_size;

    ramp.tot = 3;
    ramp.data[0].a = 1.0f;
    ramp.data[0].b = 1.0f;
    ramp.data[0].pos = 0.0f;
    ramp.data[1].a = 1.0f;
    ramp.data[1].g = 1.0f;
    ramp.data[1].pos = 0.5f;
    ramp.data[2].a = 1.0f;
    ramp.data[2].r = 1.0f;
    ramp.data[2].pos = 1.0f;

    BKE_colorband_evaluate_table_rgba(&ramp, &colors, &col_size);

    G_draw.ramp = GPU_texture_create_1d(
        "ramp", col_size, 1, GPU_RGBA8, GPU_TEXTURE_USAGE_SHADER_READ, colors);

    MEM_freeN(colors);
  }

  /* Weight Painting color ramp texture */
  bool user_weight_ramp = (U.flag & USER_CUSTOM_RANGE) != 0;

  if (weight_ramp_custom != user_weight_ramp ||
      (user_weight_ramp && memcmp(&weight_ramp_copy, &U.coba_weight, sizeof(ColorBand)) != 0))
  {
    GPU_TEXTURE_FREE_SAFE(G_draw.weight_ramp);
  }

  if (G_draw.weight_ramp == nullptr) {
    weight_ramp_custom = user_weight_ramp;
    memcpy(&weight_ramp_copy, &U.coba_weight, sizeof(ColorBand));

    G_draw.weight_ramp = DRW_create_weight_colorramp_texture();
  }
}

/* ********************************* SHGROUP ************************************* */

void DRW_globals_free() {}

/* ******************************************** COLOR UTILS ************************************ */

static void DRW_evaluate_weight_to_color(const float weight, float result[4])
{
  if (U.flag & USER_CUSTOM_RANGE) {
    BKE_colorband_evaluate(&U.coba_weight, weight, result);
  }
  else {
    /* Use gamma correction to even out the color bands:
     * increasing widens yellow/cyan vs red/green/blue.
     * Gamma 1.0 produces the original 2.79 color ramp. */
    const float gamma = 1.5f;
    const float hsv[3] = {(2.0f / 3.0f) * (1.0f - weight), 1.0f, pow(0.5f + 0.5f * weight, gamma)};

    hsv_to_rgb_v(hsv, result);

    for (int i = 0; i < 3; i++) {
      result[i] = pow(result[i], 1.0f / gamma);
    }
  }
}

static GPUTexture *DRW_create_weight_colorramp_texture()
{
  float pixels[256][4];
  for (int i = 0; i < 256; i++) {
    DRW_evaluate_weight_to_color(i / 255.0f, pixels[i]);
    pixels[i][3] = 1.0f;
  }

  uchar4 pixels_ubyte[256];
  for (int i = 0; i < 256; i++) {
    unit_float_to_uchar_clamp_v4(pixels_ubyte[i], pixels[i]);
  }

  eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ;
  GPUTexture *tx = GPU_texture_create_1d("weight_ramp", 256, 1, GPU_SRGB8_A8, usage, nullptr);
  GPU_texture_update(tx, GPU_DATA_UBYTE, pixels_ubyte);
  return tx;
}
