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
 * Copyright 2016, Blender Foundation.
 */

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

/* Colors & Constant */
struct DRW_Global G_draw = {{{0}}};

static bool weight_ramp_custom = false;
static ColorBand weight_ramp_copy;

static struct GPUTexture *DRW_create_weight_colorramp_texture(void);

void DRW_globals_update(void)
{
  GlobalsUboStorage *gb = &G_draw.block;

  UI_GetThemeColor4fv(TH_WIRE, gb->colorWire);
  UI_GetThemeColor4fv(TH_WIRE_EDIT, gb->colorWireEdit);
  UI_GetThemeColor4fv(TH_ACTIVE, gb->colorActive);
  UI_GetThemeColor4fv(TH_SELECT, gb->colorSelect);
  UI_COLOR_RGBA_FROM_U8(0x88, 0xFF, 0xFF, 155, gb->colorLibrarySelect);
  UI_COLOR_RGBA_FROM_U8(0x55, 0xCC, 0xCC, 155, gb->colorLibrary);
  UI_GetThemeColor4fv(TH_TRANSFORM, gb->colorTransform);
  UI_GetThemeColor4fv(TH_LIGHT, gb->colorLight);
  UI_GetThemeColor4fv(TH_SPEAKER, gb->colorSpeaker);
  UI_GetThemeColor4fv(TH_CAMERA, gb->colorCamera);
  UI_GetThemeColor4fv(TH_CAMERA_PATH, gb->colorCameraPath);
  UI_GetThemeColor4fv(TH_EMPTY, gb->colorEmpty);
  UI_GetThemeColor4fv(TH_VERTEX, gb->colorVertex);
  UI_GetThemeColor4fv(TH_VERTEX_SELECT, gb->colorVertexSelect);
  UI_GetThemeColor4fv(TH_VERTEX_UNREFERENCED, gb->colorVertexUnreferenced);
  UI_COLOR_RGBA_FROM_U8(0xB0, 0x00, 0xB0, 0xFF, gb->colorVertexMissingData);
  UI_GetThemeColor4fv(TH_EDITMESH_ACTIVE, gb->colorEditMeshActive);
  UI_GetThemeColor4fv(TH_EDGE_SELECT, gb->colorEdgeSelect);
  UI_GetThemeColor4fv(TH_GP_VERTEX, gb->colorGpencilVertex);
  UI_GetThemeColor4fv(TH_GP_VERTEX_SELECT, gb->colorGpencilVertexSelect);

  UI_GetThemeColor4fv(TH_EDGE_SEAM, gb->colorEdgeSeam);
  UI_GetThemeColor4fv(TH_EDGE_SHARP, gb->colorEdgeSharp);
  UI_GetThemeColor4fv(TH_EDGE_CREASE, gb->colorEdgeCrease);
  UI_GetThemeColor4fv(TH_EDGE_BEVEL, gb->colorEdgeBWeight);
  UI_GetThemeColor4fv(TH_EDGE_FACESEL, gb->colorEdgeFaceSelect);
  UI_GetThemeColor4fv(TH_FACE, gb->colorFace);
  UI_GetThemeColor4fv(TH_FACE_SELECT, gb->colorFaceSelect);
  UI_GetThemeColor4fv(TH_FACE_BACK, gb->colorFaceBack);
  UI_GetThemeColor4fv(TH_FACE_FRONT, gb->colorFaceFront);
  UI_GetThemeColor4fv(TH_NORMAL, gb->colorNormal);
  UI_GetThemeColor4fv(TH_VNORMAL, gb->colorVNormal);
  UI_GetThemeColor4fv(TH_LNORMAL, gb->colorLNormal);
  UI_GetThemeColor4fv(TH_FACE_DOT, gb->colorFaceDot);
  UI_GetThemeColor4fv(TH_SKIN_ROOT, gb->colorSkinRoot);
  UI_GetThemeColor4fv(TH_BACK, gb->colorBackground);
  UI_GetThemeColor4fv(TH_BACK_GRAD, gb->colorBackgroundGradient);
  UI_GetThemeColor4fv(TH_TRANSPARENT_CHECKER_PRIMARY, gb->colorCheckerPrimary);
  UI_GetThemeColor4fv(TH_TRANSPARENT_CHECKER_SECONDARY, gb->colorCheckerSecondary);
  gb->sizeChecker = UI_GetThemeValuef(TH_TRANSPARENT_CHECKER_SIZE);
  UI_GetThemeColor4fv(TH_V3D_CLIPPING_BORDER, gb->colorClippingBorder);

  /* Custom median color to slightly affect the edit mesh colors. */
  interp_v4_v4v4(gb->colorEditMeshMiddle, gb->colorVertexSelect, gb->colorWireEdit, 0.35f);
  copy_v3_fl(
      gb->colorEditMeshMiddle,
      dot_v3v3(gb->colorEditMeshMiddle, (float[3]){0.3333f, 0.3333f, 0.3333f})); /* Desaturate */

  interp_v4_v4v4(gb->colorDupliSelect, gb->colorBackground, gb->colorSelect, 0.5f);
  /* Was 50% in 2.7x since the background was lighter making it easier to tell the color from
   * black, with a darker background we need a more faded color. */
  interp_v4_v4v4(gb->colorDupli, gb->colorBackground, gb->colorWire, 0.3f);

#ifdef WITH_FREESTYLE
  UI_GetThemeColor4fv(TH_FREESTYLE_EDGE_MARK, gb->colorEdgeFreestyle);
  UI_GetThemeColor4fv(TH_FREESTYLE_FACE_MARK, gb->colorFaceFreestyle);
#else
  zero_v4(gb->colorEdgeFreestyle);
  zero_v4(gb->colorFaceFreestyle);
#endif

  UI_GetThemeColor4fv(TH_TEXT, gb->colorText);
  UI_GetThemeColor4fv(TH_TEXT_HI, gb->colorTextHi);

  /* Bone colors */
  UI_GetThemeColor4fv(TH_BONE_POSE, gb->colorBonePose);
  UI_GetThemeColor4fv(TH_BONE_POSE_ACTIVE, gb->colorBonePoseActive);
  UI_GetThemeColorShade4fv(TH_EDGE_SELECT, 60, gb->colorBoneActive);
  UI_GetThemeColorShade4fv(TH_EDGE_SELECT, -20, gb->colorBoneSelect);
  UI_GetThemeColorBlendShade4fv(TH_WIRE, TH_BONE_POSE, 0.15f, 0, gb->colorBonePoseActiveUnsel);
  UI_GetThemeColorBlendShade3fv(TH_WIRE_EDIT, TH_EDGE_SELECT, 0.15f, 0, gb->colorBoneActiveUnsel);
  UI_COLOR_RGBA_FROM_U8(255, 150, 0, 80, gb->colorBonePoseTarget);
  UI_COLOR_RGBA_FROM_U8(255, 255, 0, 80, gb->colorBonePoseIK);
  UI_COLOR_RGBA_FROM_U8(200, 255, 0, 80, gb->colorBonePoseSplineIK);
  UI_COLOR_RGBA_FROM_U8(0, 255, 120, 80, gb->colorBonePoseConstraint);
  UI_GetThemeColor4fv(TH_BONE_SOLID, gb->colorBoneSolid);
  UI_GetThemeColor4fv(TH_BONE_LOCKED_WEIGHT, gb->colorBoneLocked);
  copy_v4_fl4(gb->colorBoneIKLine, 0.8f, 0.5f, 0.0f, 1.0f);
  copy_v4_fl4(gb->colorBoneIKLineNoTarget, 0.8f, 0.8f, 0.2f, 1.0f);
  copy_v4_fl4(gb->colorBoneIKLineSpline, 0.8f, 0.8f, 0.2f, 1.0f);

  /* Curve */
  UI_GetThemeColor4fv(TH_HANDLE_FREE, gb->colorHandleFree);
  UI_GetThemeColor4fv(TH_HANDLE_AUTO, gb->colorHandleAuto);
  UI_GetThemeColor4fv(TH_HANDLE_VECT, gb->colorHandleVect);
  UI_GetThemeColor4fv(TH_HANDLE_ALIGN, gb->colorHandleAlign);
  UI_GetThemeColor4fv(TH_HANDLE_AUTOCLAMP, gb->colorHandleAutoclamp);
  UI_GetThemeColor4fv(TH_HANDLE_SEL_FREE, gb->colorHandleSelFree);
  UI_GetThemeColor4fv(TH_HANDLE_SEL_AUTO, gb->colorHandleSelAuto);
  UI_GetThemeColor4fv(TH_HANDLE_SEL_VECT, gb->colorHandleSelVect);
  UI_GetThemeColor4fv(TH_HANDLE_SEL_ALIGN, gb->colorHandleSelAlign);
  UI_GetThemeColor4fv(TH_HANDLE_SEL_AUTOCLAMP, gb->colorHandleSelAutoclamp);
  UI_GetThemeColor4fv(TH_NURB_ULINE, gb->colorNurbUline);
  UI_GetThemeColor4fv(TH_NURB_VLINE, gb->colorNurbVline);
  UI_GetThemeColor4fv(TH_NURB_SEL_ULINE, gb->colorNurbSelUline);
  UI_GetThemeColor4fv(TH_NURB_SEL_VLINE, gb->colorNurbSelVline);
  UI_GetThemeColor4fv(TH_ACTIVE_SPLINE, gb->colorActiveSpline);

  UI_GetThemeColor4fv(TH_CFRAME, gb->colorCurrentFrame);

  /* Metaball */
  UI_COLOR_RGBA_FROM_U8(0xA0, 0x30, 0x30, 0xFF, gb->colorMballRadius);
  UI_COLOR_RGBA_FROM_U8(0xF0, 0xA0, 0xA0, 0xFF, gb->colorMballRadiusSelect);
  UI_COLOR_RGBA_FROM_U8(0x30, 0xA0, 0x30, 0xFF, gb->colorMballStiffness);
  UI_COLOR_RGBA_FROM_U8(0xA0, 0xF0, 0xA0, 0xFF, gb->colorMballStiffnessSelect);

  /* Grid */
  UI_GetThemeColorShade4fv(TH_GRID, 10, gb->colorGrid);
  /* Emphasize division lines lighter instead of darker, if background is darker than grid. */
  UI_GetThemeColorShade4fv(
      TH_GRID,
      (gb->colorGrid[0] + gb->colorGrid[1] + gb->colorGrid[2] + 0.12f >
       gb->colorBackground[0] + gb->colorBackground[1] + gb->colorBackground[2]) ?
          20 :
          -10,
      gb->colorGridEmphasis);
  /* Grid Axis */
  UI_GetThemeColorBlendShade4fv(TH_GRID, TH_AXIS_X, 0.5f, -10, gb->colorGridAxisX);
  UI_GetThemeColorBlendShade4fv(TH_GRID, TH_AXIS_Y, 0.5f, -10, gb->colorGridAxisY);
  UI_GetThemeColorBlendShade4fv(TH_GRID, TH_AXIS_Z, 0.5f, -10, gb->colorGridAxisZ);

  UI_GetThemeColorShadeAlpha4fv(TH_TRANSFORM, 0, -80, gb->colorDeselect);
  UI_GetThemeColorShadeAlpha4fv(TH_WIRE, 0, -30, gb->colorOutline);
  UI_GetThemeColorShadeAlpha4fv(TH_LIGHT, 0, 255, gb->colorLightNoAlpha);

  /* UV colors */
  UI_GetThemeColor4fv(TH_UV_SHADOW, gb->colorUVShadow);

  gb->sizePixel = U.pixelsize;
  gb->sizeObjectCenter = (UI_GetThemeValuef(TH_OBCENTER_DIA) + 1.0f) * U.pixelsize;
  gb->sizeLightCenter = (UI_GetThemeValuef(TH_OBCENTER_DIA) + 1.5f) * U.pixelsize;
  gb->sizeLightCircle = U.pixelsize * 9.0f;
  gb->sizeLightCircleShadow = gb->sizeLightCircle + U.pixelsize * 3.0f;

  /* M_SQRT2 to be at least the same size of the old square */
  gb->sizeVertex = U.pixelsize *
                   (max_ff(1.0f, UI_GetThemeValuef(TH_VERTEX_SIZE) * (float)M_SQRT2 / 2.0f));
  gb->sizeVertexGpencil = U.pixelsize * UI_GetThemeValuef(TH_GP_VERTEX_SIZE);
  gb->sizeFaceDot = U.pixelsize * UI_GetThemeValuef(TH_FACEDOT_SIZE);
  gb->sizeEdge = U.pixelsize * (1.0f / 2.0f); /* TODO Theme */
  gb->sizeEdgeFix = U.pixelsize * (0.5f + 2.0f * (2.0f * (gb->sizeEdge * (float)M_SQRT1_2)));

  const float(*screen_vecs)[3] = (float(*)[3])DRW_viewport_screenvecs_get();
  for (int i = 0; i < 2; i++) {
    copy_v3_v3(gb->screenVecs[i], screen_vecs[i]);
  }

  gb->pixelFac = *DRW_viewport_pixelsize_get();

  copy_v2_v2(gb->sizeViewport, DRW_viewport_size_get());
  copy_v2_v2(gb->sizeViewportInv, gb->sizeViewport);
  invert_v2(gb->sizeViewportInv);

  /* Color management. */
  {
    float *color = gb->UBO_FIRST_COLOR;
    do {
      /* TODO more accurate transform. */
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

    G_draw.ramp = GPU_texture_create_1d("ramp", col_size, 1, GPU_RGBA8, colors);

    MEM_freeN(colors);
  }

  /* Weight Painting color ramp texture */
  bool user_weight_ramp = (U.flag & USER_CUSTOM_RANGE) != 0;

  if (weight_ramp_custom != user_weight_ramp ||
      (user_weight_ramp && memcmp(&weight_ramp_copy, &U.coba_weight, sizeof(ColorBand)) != 0)) {
    DRW_TEXTURE_FREE_SAFE(G_draw.weight_ramp);
  }

  if (G_draw.weight_ramp == NULL) {
    weight_ramp_custom = user_weight_ramp;
    memcpy(&weight_ramp_copy, &U.coba_weight, sizeof(ColorBand));

    G_draw.weight_ramp = DRW_create_weight_colorramp_texture();
  }
}

/* ********************************* SHGROUP ************************************* */

void DRW_globals_free(void)
{
}

DRWView *DRW_view_create_with_zoffset(const DRWView *parent_view,
                                      const RegionView3D *rv3d,
                                      float offset)
{
  /* Create view with depth offset */
  float viewmat[4][4], winmat[4][4];
  DRW_view_viewmat_get(parent_view, viewmat, false);
  DRW_view_winmat_get(parent_view, winmat, false);

  float viewdist = rv3d->dist;

  /* special exception for ortho camera (viewdist isnt used for perspective cameras) */
  if (rv3d->persp == RV3D_CAMOB && rv3d->is_persp == false) {
    viewdist = 1.0f / max_ff(fabsf(winmat[0][0]), fabsf(winmat[1][1]));
  }

  winmat[3][2] -= GPU_polygon_offset_calc(winmat, viewdist, offset);

  return DRW_view_create_sub(parent_view, viewmat, winmat);
}

/* ******************************************** COLOR UTILS ************************************ */

/* TODO FINISH */
/**
 * Get the wire color theme_id of an object based on its state
 * \a r_color is a way to get a pointer to the static color var associated
 */
int DRW_object_wire_theme_get(Object *ob, ViewLayer *view_layer, float **r_color)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const bool is_edit = (draw_ctx->object_mode & OB_MODE_EDIT) && (ob->mode & OB_MODE_EDIT);
  const bool active = (view_layer->basact && view_layer->basact->object == ob);
  /* confusing logic here, there are 2 methods of setting the color
   * 'colortab[colindex]' and 'theme_id', colindex overrides theme_id.
   *
   * note: no theme yet for 'colindex' */
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
      *r_color = G_draw.block.colorDupli;
    }
    else if (UNLIKELY(ob->base_flag & BASE_FROM_DUPLI)) {
      switch (theme_id) {
        case TH_ACTIVE:
        case TH_SELECT:
          *r_color = G_draw.block.colorDupliSelect;
          break;
        case TH_TRANSFORM:
          *r_color = G_draw.block.colorTransform;
          break;
        default:
          *r_color = G_draw.block.colorDupli;
          break;
      }
    }
    else {
      switch (theme_id) {
        case TH_WIRE_EDIT:
          *r_color = G_draw.block.colorWireEdit;
          break;
        case TH_ACTIVE:
          *r_color = G_draw.block.colorActive;
          break;
        case TH_SELECT:
          *r_color = G_draw.block.colorSelect;
          break;
        case TH_TRANSFORM:
          *r_color = G_draw.block.colorTransform;
          break;
        case TH_SPEAKER:
          *r_color = G_draw.block.colorSpeaker;
          break;
        case TH_CAMERA:
          *r_color = G_draw.block.colorCamera;
          break;
        case TH_EMPTY:
          *r_color = G_draw.block.colorEmpty;
          break;
        case TH_LIGHT:
          *r_color = G_draw.block.colorLight;
          break;
        default:
          *r_color = G_draw.block.colorWire;
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
            OB_CURVE,
            OB_SURF,
            OB_FONT,
            OB_MBALL,
            OB_HAIR,
            OB_POINTCLOUD,
            OB_VOLUME)) {
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

  return GPU_texture_create_1d("weight_color_ramp", 256, 1, GPU_SRGB8_A8, pixels[0]);
}
