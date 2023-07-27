/* SPDX-FileCopyrightText: 2017 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include <cstdio>

#include "BLI_utildefines.h"

#include "BLI_math_color.h"
#include "BLI_math_vector.h"

#include "BLT_translation.h"

#include "DNA_defaults.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_gpencil_modifier_legacy.h"
#include "BKE_lib_query.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "BKE_modifier.h"

#include "DEG_depsgraph.h"

#include "MOD_gpencil_legacy_modifiertypes.h"
#include "MOD_gpencil_legacy_ui_common.h"
#include "MOD_gpencil_legacy_util.h"

static void init_data(GpencilModifierData *md)
{
  ColorGpencilModifierData *gpmd = (ColorGpencilModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(gpmd, modifier));

  MEMCPY_STRUCT_AFTER(gpmd, DNA_struct_default_get(ColorGpencilModifierData), modifier);

  gpmd->curve_intensity = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
  BKE_curvemapping_init(gpmd->curve_intensity);
}

static void copy_data(const GpencilModifierData *md, GpencilModifierData *target)
{
  ColorGpencilModifierData *gmd = (ColorGpencilModifierData *)md;
  ColorGpencilModifierData *tgmd = (ColorGpencilModifierData *)target;

  if (tgmd->curve_intensity != nullptr) {
    BKE_curvemapping_free(tgmd->curve_intensity);
    tgmd->curve_intensity = nullptr;
  }

  BKE_gpencil_modifier_copydata_generic(md, target);

  tgmd->curve_intensity = BKE_curvemapping_copy(gmd->curve_intensity);
}

/* color correction strokes */
static void deform_stroke(GpencilModifierData *md,
                          Depsgraph * /*depsgraph*/,
                          Object *ob,
                          bGPDlayer *gpl,
                          bGPDframe * /*gpf*/,
                          bGPDstroke *gps)
{

  ColorGpencilModifierData *mmd = (ColorGpencilModifierData *)md;
  float hsv[3], factor[3];
  const bool use_curve = (mmd->flag & GP_COLOR_CUSTOM_CURVE) != 0 && mmd->curve_intensity;

  if (!is_stroke_affected_by_modifier(ob,
                                      mmd->layername,
                                      mmd->material,
                                      mmd->pass_index,
                                      mmd->layer_pass,
                                      1,
                                      gpl,
                                      gps,
                                      mmd->flag & GP_COLOR_INVERT_LAYER,
                                      mmd->flag & GP_COLOR_INVERT_PASS,
                                      mmd->flag & GP_COLOR_INVERT_LAYERPASS,
                                      mmd->flag & GP_COLOR_INVERT_MATERIAL))
  {
    return;
  }

  copy_v3_v3(factor, mmd->hsv);
  MaterialGPencilStyle *gp_style = BKE_gpencil_material_settings(ob, gps->mat_nr + 1);

  /* Apply to Vertex Color. */
  /* Fill */
  if (mmd->modify_color != GP_MODIFY_COLOR_STROKE) {
    /* If not using Vertex Color, use the material color. */
    if ((gp_style != nullptr) && (gps->vert_color_fill[3] == 0.0f) &&
        (gp_style->fill_rgba[3] > 0.0f)) {
      copy_v4_v4(gps->vert_color_fill, gp_style->fill_rgba);
      gps->vert_color_fill[3] = 1.0f;
    }

    rgb_to_hsv_v(gps->vert_color_fill, hsv);
    hsv[0] = fractf(hsv[0] + factor[0] + 0.5f);
    hsv[1] = clamp_f(hsv[1] * factor[1], 0.0f, 1.0f);
    hsv[2] = hsv[2] * factor[2];
    hsv_to_rgb_v(hsv, gps->vert_color_fill);
  }

  /* Stroke */
  if (mmd->modify_color != GP_MODIFY_COLOR_FILL) {

    for (int i = 0; i < gps->totpoints; i++) {
      bGPDspoint *pt = &gps->points[i];
      /* If not using Vertex Color, use the material color. */
      if ((gp_style != nullptr) && (pt->vert_color[3] == 0.0f) &&
          (gp_style->stroke_rgba[3] > 0.0f)) {
        copy_v4_v4(pt->vert_color, gp_style->stroke_rgba);
        pt->vert_color[3] = 1.0f;
      }

      /* Custom curve to modulate value. */
      float factor_value[3];
      copy_v3_v3(factor_value, factor);
      if (use_curve) {
        float value = float(i) / (gps->totpoints - 1);
        float mixfac = BKE_curvemapping_evaluateF(mmd->curve_intensity, 0, value);
        mul_v3_fl(factor_value, mixfac);
      }

      rgb_to_hsv_v(pt->vert_color, hsv);
      hsv[0] = fractf(hsv[0] + factor_value[0] + 0.5f);
      hsv[1] = clamp_f(hsv[1] * factor_value[1], 0.0f, 1.0f);
      hsv[2] = hsv[2] * factor_value[2];
      hsv_to_rgb_v(hsv, pt->vert_color);
    }
  }
}

static void bake_modifier(Main * /*bmain*/,
                          Depsgraph *depsgraph,
                          GpencilModifierData *md,
                          Object *ob)
{
  generic_bake_deform_stroke(depsgraph, md, ob, false, deform_stroke);
}

static void free_data(GpencilModifierData *md)
{
  ColorGpencilModifierData *gpmd = (ColorGpencilModifierData *)md;

  if (gpmd->curve_intensity) {
    BKE_curvemapping_free(gpmd->curve_intensity);
  }
}

static void foreach_ID_link(GpencilModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  ColorGpencilModifierData *mmd = (ColorGpencilModifierData *)md;

  walk(user_data, ob, (ID **)&mmd->material, IDWALK_CB_USER);
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, nullptr);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "modify_color", 0, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "hue", UI_ITEM_R_SLIDER, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "saturation", UI_ITEM_R_SLIDER, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "value", UI_ITEM_R_SLIDER, nullptr, ICON_NONE);

  gpencil_modifier_panel_end(layout, ptr);
}

static void mask_panel_draw(const bContext * /*C*/, Panel *panel)
{
  gpencil_modifier_masking_panel_draw(panel, true, false);
}

static void panel_register(ARegionType *region_type)
{
  PanelType *panel_type = gpencil_modifier_panel_register(
      region_type, eGpencilModifierType_Color, panel_draw);
  PanelType *mask_panel_type = gpencil_modifier_subpanel_register(
      region_type, "mask", "Influence", nullptr, mask_panel_draw, panel_type);
  gpencil_modifier_subpanel_register(region_type,
                                     "curve",
                                     "",
                                     gpencil_modifier_curve_header_draw,
                                     gpencil_modifier_curve_panel_draw,
                                     mask_panel_type);
}

GpencilModifierTypeInfo modifierType_Gpencil_Color = {
    /*name*/ N_("Hue/Saturation"),
    /*struct_name*/ "ColorGpencilModifierData",
    /*struct_size*/ sizeof(ColorGpencilModifierData),
    /*type*/ eGpencilModifierTypeType_Gpencil,
    /*flags*/ eGpencilModifierTypeFlag_SupportsEditmode,

    /*copy_data*/ copy_data,

    /*deform_stroke*/ deform_stroke,
    /*generate_strokes*/ nullptr,
    /*bake_modifier*/ bake_modifier,
    /*remap_time*/ nullptr,

    /*init_data*/ init_data,
    /*free_data*/ free_data,
    /*is_disabled*/ nullptr,
    /*update_depsgraph*/ nullptr,
    /*depends_on_time*/ nullptr,
    /*foreach_ID_link*/ foreach_ID_link,
    /*foreach_tex_link*/ nullptr,
    /*panel_register*/ panel_register,
};
