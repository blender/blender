/* SPDX-FileCopyrightText: 2017 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include <cstdio>
#include <cstring>

#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_defaults.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_gpencil_modifier_legacy.h"
#include "BKE_lib_query.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_screen.h"

#include "DEG_depsgraph.h"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"

#include "MOD_gpencil_legacy_modifiertypes.h"
#include "MOD_gpencil_legacy_ui_common.h"
#include "MOD_gpencil_legacy_util.h"

static void init_data(GpencilModifierData *md)
{
  OpacityGpencilModifierData *gpmd = (OpacityGpencilModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(gpmd, modifier));

  MEMCPY_STRUCT_AFTER(gpmd, DNA_struct_default_get(OpacityGpencilModifierData), modifier);

  gpmd->curve_intensity = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
  BKE_curvemapping_init(gpmd->curve_intensity);
}

static void copy_data(const GpencilModifierData *md, GpencilModifierData *target)
{
  OpacityGpencilModifierData *gmd = (OpacityGpencilModifierData *)md;
  OpacityGpencilModifierData *tgmd = (OpacityGpencilModifierData *)target;

  if (tgmd->curve_intensity != nullptr) {
    BKE_curvemapping_free(tgmd->curve_intensity);
    tgmd->curve_intensity = nullptr;
  }

  BKE_gpencil_modifier_copydata_generic(md, target);

  tgmd->curve_intensity = BKE_curvemapping_copy(gmd->curve_intensity);
}

/* opacity strokes */
static void deform_stroke(GpencilModifierData *md,
                          Depsgraph * /*depsgraph*/,
                          Object *ob,
                          bGPDlayer *gpl,
                          bGPDframe * /*gpf*/,
                          bGPDstroke *gps)
{
  OpacityGpencilModifierData *mmd = (OpacityGpencilModifierData *)md;
  const int def_nr = BKE_object_defgroup_name_index(ob, mmd->vgname);
  const bool use_curve = (mmd->flag & GP_OPACITY_CUSTOM_CURVE) != 0 && mmd->curve_intensity;
  const bool is_normalized = (mmd->flag & GP_OPACITY_NORMALIZE);
  bool is_inverted = ((mmd->flag & GP_OPACITY_WEIGHT_FACTOR) == 0) &&
                     ((mmd->flag & GP_OPACITY_INVERT_VGROUP) != 0);

  if (!is_stroke_affected_by_modifier(ob,
                                      mmd->layername,
                                      mmd->material,
                                      mmd->pass_index,
                                      mmd->layer_pass,
                                      1,
                                      gpl,
                                      gps,
                                      mmd->flag & GP_OPACITY_INVERT_LAYER,
                                      mmd->flag & GP_OPACITY_INVERT_PASS,
                                      mmd->flag & GP_OPACITY_INVERT_LAYERPASS,
                                      mmd->flag & GP_OPACITY_INVERT_MATERIAL))
  {
    return;
  }

  /* Hardness (at stroke level). */
  if (mmd->modify_color == GP_MODIFY_COLOR_HARDNESS) {
    gps->hardeness *= mmd->hardeness;
    CLAMP(gps->hardeness, 0.0f, 1.0f);

    return;
  }

  for (int i = 0; i < gps->totpoints; i++) {
    bGPDspoint *pt = &gps->points[i];
    MDeformVert *dvert = gps->dvert != nullptr ? &gps->dvert[i] : nullptr;

    /* Stroke using strength. */
    if (mmd->modify_color != GP_MODIFY_COLOR_FILL) {
      /* verify vertex group */
      float weight = get_modifier_point_weight(dvert, is_inverted, def_nr);
      if (weight < 0.0f) {
        continue;
      }

      /* Apply weight directly. */
      if ((mmd->flag & GP_OPACITY_WEIGHT_FACTOR) && (!is_normalized)) {
        pt->strength *= ((mmd->flag & GP_OPACITY_INVERT_VGROUP) ? 1.0f - weight : weight);
        continue;
      }

      /* Custom curve to modulate value. */
      float factor_curve = mmd->factor;
      if (use_curve) {
        float value = float(i) / (gps->totpoints - 1);
        factor_curve *= BKE_curvemapping_evaluateF(mmd->curve_intensity, 0, value);
      }

      if (def_nr < 0) {
        if (mmd->flag & GP_OPACITY_NORMALIZE) {
          pt->strength = factor_curve;
        }
        else {
          pt->strength += factor_curve - 1.0f;
        }
      }
      else {
        /* High factor values, change weight too. */
        if ((factor_curve > 1.0f) && (weight < 1.0f)) {
          weight += factor_curve - 1.0f;
          CLAMP(weight, 0.0f, 1.0f);
        }
        if (mmd->flag & GP_OPACITY_NORMALIZE) {
          pt->strength = factor_curve;
        }
        else {
          pt->strength += (factor_curve - 1) * weight;
        }
      }

      CLAMP(pt->strength, 0.0f, 1.0f);
    }
  }

  /* Fill using opacity factor. */
  if (mmd->modify_color != GP_MODIFY_COLOR_STROKE) {
    float fill_factor = mmd->factor;

    if ((mmd->flag & GP_OPACITY_WEIGHT_FACTOR) && (!is_normalized)) {
      /* Use first point for weight. */
      MDeformVert *dvert = (gps->dvert != nullptr) ? &gps->dvert[0] : nullptr;
      float weight = get_modifier_point_weight(
          dvert, (mmd->flag & GP_OPACITY_INVERT_VGROUP) != 0, def_nr);
      if (weight >= 0.0f) {
        fill_factor = ((mmd->flag & GP_OPACITY_INVERT_VGROUP) ? 1.0f - weight : weight);
      }
    }

    gps->fill_opacity_fac = fill_factor;
    CLAMP(gps->fill_opacity_fac, 0.0f, 1.0f);
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
  OpacityGpencilModifierData *gpmd = (OpacityGpencilModifierData *)md;

  if (gpmd->curve_intensity) {
    BKE_curvemapping_free(gpmd->curve_intensity);
  }
}

static void foreach_ID_link(GpencilModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  OpacityGpencilModifierData *mmd = (OpacityGpencilModifierData *)md;

  walk(user_data, ob, (ID **)&mmd->material, IDWALK_CB_USER);
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, nullptr);

  uiLayoutSetPropSep(layout, true);

  int modify_color = RNA_enum_get(ptr, "modify_color");

  uiItemR(layout, ptr, "modify_color", UI_ITEM_NONE, nullptr, ICON_NONE);

  if (modify_color == GP_MODIFY_COLOR_HARDNESS) {
    uiItemR(layout, ptr, "hardness", UI_ITEM_NONE, nullptr, ICON_NONE);
  }
  else {
    const bool is_normalized = RNA_boolean_get(ptr, "use_normalized_opacity");
    const bool is_weighted = RNA_boolean_get(ptr, "use_weight_factor");

    uiItemR(layout, ptr, "use_normalized_opacity", UI_ITEM_NONE, nullptr, ICON_NONE);
    const char *text = (is_normalized) ? IFACE_("Strength") : IFACE_("Opacity Factor");

    uiLayout *row = uiLayoutRow(layout, true);
    uiLayoutSetActive(row, !is_weighted || is_normalized);
    uiItemR(row, ptr, "factor", UI_ITEM_NONE, text, ICON_NONE);
    if (!is_normalized) {
      uiLayout *sub = uiLayoutRow(row, true);
      uiLayoutSetActive(sub, true);
      uiItemR(row, ptr, "use_weight_factor", UI_ITEM_NONE, "", ICON_MOD_VERTEX_WEIGHT);
    }
  }

  gpencil_modifier_panel_end(layout, ptr);
}

static void mask_panel_draw(const bContext * /*C*/, Panel *panel)
{
  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, nullptr);

  int modify_color = RNA_enum_get(ptr, "modify_color");
  bool show_vertex = (modify_color != GP_MODIFY_COLOR_HARDNESS);

  gpencil_modifier_masking_panel_draw(panel, true, show_vertex);
}

static void curve_header_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, nullptr);

  int modify_color = RNA_enum_get(ptr, "modify_color");
  uiLayoutSetActive(layout, modify_color != GP_MODIFY_COLOR_HARDNESS);

  gpencil_modifier_curve_header_draw(C, panel);
}

static void curve_panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, nullptr);

  int modify_color = RNA_enum_get(ptr, "modify_color");
  uiLayoutSetActive(layout, modify_color != GP_MODIFY_COLOR_HARDNESS);

  gpencil_modifier_curve_panel_draw(C, panel);
}

static void panel_register(ARegionType *region_type)
{
  PanelType *panel_type = gpencil_modifier_panel_register(
      region_type, eGpencilModifierType_Opacity, panel_draw);

  PanelType *mask_panel_type = gpencil_modifier_subpanel_register(
      region_type, "mask", "Influence", nullptr, mask_panel_draw, panel_type);
  gpencil_modifier_subpanel_register(
      region_type, "curve", "", curve_header_draw, curve_panel_draw, mask_panel_type);
}

GpencilModifierTypeInfo modifierType_Gpencil_Opacity = {
    /*name*/ N_("Opacity"),
    /*struct_name*/ "OpacityGpencilModifierData",
    /*struct_size*/ sizeof(OpacityGpencilModifierData),
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
