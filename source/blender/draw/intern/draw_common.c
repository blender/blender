/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "DRW_render.h"

#include "GPU_matrix.h"
#include "GPU_shader.h"
#include "GPU_texture.h"

#include "UI_resources.h"

#include "BKE_colorband.h"
#include "BKE_global.h"
#include "BKE_object.h"

#include "draw_common.h"

#if 0
#  define UI_COLOR_RGB_FROM_U8(r, g, b, v4) \
    ARRAY_SET_ITEMS(v4, (float)r / 255.0f, (float)g / 255.0f, (float)b / 255.0f, 1.0)
#endif
#define UI_COLOR_RGBA_FROM_U8(r, g, b, a, v4) \
  ARRAY_SET_ITEMS(v4, (float)r / 255.0f, (float)g / 255.0f, (float)b / 255.0f, (float)a / 255.0f)

/**
 * Colors & Constant.
 */
struct DRW_Global G_draw = {{{0}}};

static bool weight_ramp_custom = false;
static ColorBand weight_ramp_copy;

static GPUTexture *DRW_create_weight_colorramp_texture(void);

void DRW_globals_update(void)
{
  GlobalsUboStorage *gb = &G_draw.block;

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
  UI_GetThemeColor4fv(TH_GP_VERTEX, gb->color_gpencil_vertex);
  UI_GetThemeColor4fv(TH_GP_VERTEX_SELECT, gb->color_gpencil_vertex_select);

  UI_GetThemeColor4fv(TH_EDGE_SEAM, gb->color_edge_seam);
  UI_GetThemeColor4fv(TH_EDGE_SHARP, gb->color_edge_sharp);
  UI_GetThemeColor4fv(TH_EDGE_CREASE, gb->color_edge_crease);
  UI_GetThemeColor4fv(TH_EDGE_BEVEL, gb->color_edge_bweight);
  UI_GetThemeColor4fv(TH_EDGE_FACESEL, gb->color_edge_face_select);
  UI_GetThemeColor4fv(TH_FACE, gb->color_face);
  UI_GetThemeColor4fv(TH_FACE_SELECT, gb->color_face_select);
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
  UI_GetThemeColor4fv(TH_V3D_CLIPPING_BORDER, gb->color_clipping_border);

  /* Custom median color to slightly affect the edit mesh colors. */
  interp_v4_v4v4(gb->color_edit_mesh_middle, gb->color_vertex_select, gb->color_wire_edit, 0.35f);
  copy_v3_fl(gb->color_edit_mesh_middle,
             dot_v3v3(gb->color_edit_mesh_middle,
                      (float[3]){0.3333f, 0.3333f, 0.3333f})); /* Desaturate */

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
  UI_COLOR_RGBA_FROM_U8(255, 150, 0, 80, gb->color_bone_pose_target);
  UI_COLOR_RGBA_FROM_U8(255, 255, 0, 80, gb->color_bone_pose_ik);
  UI_COLOR_RGBA_FROM_U8(200, 255, 0, 80, gb->color_bone_pose_spline_ik);
  UI_COLOR_RGBA_FROM_U8(0, 255, 120, 80, gb->color_bone_pose_constraint);
  UI_GetThemeColor4fv(TH_BONE_SOLID, gb->color_bone_solid);
  UI_GetThemeColor4fv(TH_BONE_LOCKED_WEIGHT, gb->color_bone_locked);
  copy_v4_fl4(gb->color_bone_ik_line, 0.8f, 0.5f, 0.0f, 1.0f);
  copy_v4_fl4(gb->color_bone_ik_line_no_target, 0.8f, 0.8f, 0.2f, 1.0f);
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
                    max_ff(1.0f, UI_GetThemeValuef(TH_VERTEX_SIZE) * (float)M_SQRT2 / 2.0f);
  gb->size_vertex_gpencil = U.pixelsize * UI_GetThemeValuef(TH_GP_VERTEX_SIZE);
  gb->size_face_dot = U.pixelsize * UI_GetThemeValuef(TH_FACEDOT_SIZE);
  gb->size_edge = U.pixelsize * max_ff(1.0f, UI_GetThemeValuef(TH_EDGE_WIDTH)) / 2.0f;
  gb->size_edge_fix = U.pixelsize * (0.5f + 2.0f * (1.0f * (gb->size_edge * (float)M_SQRT1_2)));

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

  if (G_draw.block_ubo == NULL) {
    G_draw.block_ubo = GPU_uniformbuf_create_ex(
        sizeof(GlobalsUboStorage), gb, "GlobalsUboStorage");
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
    DRW_TEXTURE_FREE_SAFE(G_draw.weight_ramp);
  }

  if (G_draw.weight_ramp == NULL) {
    weight_ramp_custom = user_weight_ramp;
    memcpy(&weight_ramp_copy, &U.coba_weight, sizeof(ColorBand));

    G_draw.weight_ramp = DRW_create_weight_colorramp_texture();
  }
}

/* ********************************* SHGROUP ************************************* */

void DRW_globals_free(void) {}

DRWView *DRW_view_create_with_zoffset(const DRWView *parent_view,
                                      const RegionView3D *rv3d,
                                      float offset)
{
  /* Create view with depth offset */
  float viewmat[4][4], winmat[4][4];
  DRW_view_viewmat_get(parent_view, viewmat, false);
  DRW_view_winmat_get(parent_view, winmat, false);

  float viewdist = rv3d->dist;

  /* special exception for ortho camera (`viewdist` isn't used for perspective cameras). */
  if (rv3d->persp == RV3D_CAMOB && rv3d->is_persp == false) {
    viewdist = 1.0f / max_ff(fabsf(winmat[0][0]), fabsf(winmat[1][1]));
  }

  winmat[3][2] -= GPU_polygon_offset_calc(winmat, viewdist, offset);

  return DRW_view_create_sub(parent_view, viewmat, winmat);
}

/* ******************************************** COLOR UTILS ************************************ */

/* TODO: FINISH. */
int DRW_object_wire_theme_get(Object *ob, ViewLayer *view_layer, float **r_color)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const bool is_edit = (draw_ctx->object_mode & OB_MODE_EDIT) && (ob->mode & OB_MODE_EDIT);
  BKE_view_layer_synced_ensure(draw_ctx->scene, view_layer);
  const Base *base = BKE_view_layer_active_base_get(view_layer);
  const bool active = base && ((ob->base_flag & BASE_FROM_DUPLI) ?
                                   (DRW_object_get_dupli_parent(ob) == base->object) :
                                   (base->object == ob));

  /* confusing logic here, there are 2 methods of setting the color
   * 'colortab[colindex]' and 'theme_id', colindex overrides theme_id.
   *
   * NOTE: no theme yet for 'colindex'. */
  int theme_id = is_edit ? TH_WIRE_EDIT : TH_WIRE;

  if (is_edit) {
    /* fallback to TH_WIRE */
  }
  else if (((G.moving & G_TRANSFORM_OBJ) != 0) && ((ob->base_flag & BASE_SELECTED) != 0)) {
    theme_id = TH_TRANSFORM;
  }
  else {
    /* Sets the 'theme_id' or fallback to wire */
    if ((ob->base_flag & BASE_SELECTED) != 0) {
      theme_id = (active) ? TH_ACTIVE : TH_SELECT;
    }
    else {
      switch (ob->type) {
        case OB_LAMP:
          theme_id = TH_LIGHT;
          break;
        case OB_SPEAKER:
          theme_id = TH_SPEAKER;
          break;
        case OB_CAMERA:
          theme_id = TH_CAMERA;
          break;
        case OB_EMPTY:
          theme_id = TH_EMPTY;
          break;
        case OB_LIGHTPROBE:
          /* TODO: add light-probe color. */
          theme_id = TH_EMPTY;
          break;
        default:
          /* fallback to TH_WIRE */
          break;
      }
    }
  }

  if (r_color != NULL) {
    if (UNLIKELY(ob->base_flag & BASE_FROM_SET)) {
      *r_color = G_draw.block.color_wire;
    }
    else {
      switch (theme_id) {
        case TH_WIRE_EDIT:
          *r_color = G_draw.block.color_wire_edit;
          break;
        case TH_ACTIVE:
          *r_color = G_draw.block.color_active;
          break;
        case TH_SELECT:
          *r_color = G_draw.block.color_select;
          break;
        case TH_TRANSFORM:
          *r_color = G_draw.block.color_transform;
          break;
        case TH_SPEAKER:
          *r_color = G_draw.block.color_speaker;
          break;
        case TH_CAMERA:
          *r_color = G_draw.block.color_camera;
          break;
        case TH_EMPTY:
          *r_color = G_draw.block.color_empty;
          break;
        case TH_LIGHT:
          *r_color = G_draw.block.color_light;
          break;
        default:
          *r_color = G_draw.block.color_wire;
          break;
      }
    }
  }

  return theme_id;
}

/* XXX This is very stupid, better find something more general. */
float *DRW_color_background_blend_get(int theme_id)
{
  static float colors[11][4];
  float *ret;

  switch (theme_id) {
    case TH_WIRE_EDIT:
      ret = colors[0];
      break;
    case TH_ACTIVE:
      ret = colors[1];
      break;
    case TH_SELECT:
      ret = colors[2];
      break;
    case TH_TRANSFORM:
      ret = colors[5];
      break;
    case TH_SPEAKER:
      ret = colors[6];
      break;
    case TH_CAMERA:
      ret = colors[7];
      break;
    case TH_EMPTY:
      ret = colors[8];
      break;
    case TH_LIGHT:
      ret = colors[9];
      break;
    default:
      ret = colors[10];
      break;
  }

  UI_GetThemeColorBlendShade4fv(theme_id, TH_BACK, 0.5, 0, ret);

  return ret;
}

bool DRW_object_is_flat(Object *ob, int *r_axis)
{
  float dim[3];

  if (!ELEM(ob->type,
            OB_MESH,
            OB_CURVES_LEGACY,
            OB_SURF,
            OB_FONT,
            OB_CURVES,
            OB_POINTCLOUD,
            OB_VOLUME))
  {
    /* Non-meshes object cannot be considered as flat. */
    return false;
  }

  BKE_object_dimensions_get(ob, dim);
  if (dim[0] == 0.0f) {
    *r_axis = 0;
    return true;
  }
  if (dim[1] == 0.0f) {
    *r_axis = 1;
    return true;
  }
  if (dim[2] == 0.0f) {
    *r_axis = 2;
    return true;
  }
  return false;
}

bool DRW_object_axis_orthogonal_to_view(Object *ob, int axis)
{
  float ob_rot[3][3], invviewmat[4][4];
  DRW_view_viewmat_get(NULL, invviewmat, true);
  BKE_object_rot_to_mat3(ob, ob_rot, true);
  float dot = dot_v3v3(ob_rot[axis], invviewmat[2]);
  if (fabsf(dot) < 1e-3) {
    return true;
  }

  return false;
}

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

static GPUTexture *DRW_create_weight_colorramp_texture(void)
{
  float pixels[256][4];
  for (int i = 0; i < 256; i++) {
    DRW_evaluate_weight_to_color(i / 255.0f, pixels[i]);
    pixels[i][3] = 1.0f;
  }

  return GPU_texture_create_1d(
      "weight_color_ramp", 256, 1, GPU_SRGB8_A8, GPU_TEXTURE_USAGE_SHADER_READ, pixels[0]);
}
