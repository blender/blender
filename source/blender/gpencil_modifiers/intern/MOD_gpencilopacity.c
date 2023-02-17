/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2017 Blender Foundation. */

/** \file
 * \ingroup modifiers
 */

#include <stdio.h>
#include <string.h>

#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_defaults.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_gpencil_modifier.h"
#include "BKE_lib_query.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_screen.h"

#include "DEG_depsgraph.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "MOD_gpencil_modifiertypes.h"
#include "MOD_gpencil_ui_common.h"
#include "MOD_gpencil_util.h"

static void initData(GpencilModifierData *md)
{
  OpacityGpencilModifierData *gpmd = (OpacityGpencilModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(gpmd, modifier));

  MEMCPY_STRUCT_AFTER(gpmd, DNA_struct_default_get(OpacityGpencilModifierData), modifier);

  gpmd->curve_intensity = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
  BKE_curvemapping_init(gpmd->curve_intensity);
}

static void copyData(const GpencilModifierData *md, GpencilModifierData *target)
{
  OpacityGpencilModifierData *gmd = (OpacityGpencilModifierData *)md;
  OpacityGpencilModifierData *tgmd = (OpacityGpencilModifierData *)target;

  if (tgmd->curve_intensity != NULL) {
    BKE_curvemapping_free(tgmd->curve_intensity);
    tgmd->curve_intensity = NULL;
  }

  BKE_gpencil_modifier_copydata_generic(md, target);

  tgmd->curve_intensity = BKE_curvemapping_copy(gmd->curve_intensity);
}

/* opacity strokes */
static void deformStroke(GpencilModifierData *md,
                         Depsgraph *UNUSED(depsgraph),
                         Object *ob,
                         bGPDlayer *gpl,
                         bGPDframe *UNUSED(gpf),
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
                                      mmd->flag & GP_OPACITY_INVERT_MATERIAL)) {
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
    MDeformVert *dvert = gps->dvert != NULL ? &gps->dvert[i] : NULL;

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
        float value = (float)i / (gps->totpoints - 1);
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
      MDeformVert *dvert = (gps->dvert != NULL) ? &gps->dvert[0] : NULL;
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

static void bakeModifier(Main *UNUSED(bmain),
                         Depsgraph *depsgraph,
                         GpencilModifierData *md,
                         Object *ob)
{
  generic_bake_deform_stroke(depsgraph, md, ob, false, deformStroke);
}

static void freeData(GpencilModifierData *md)
{
  OpacityGpencilModifierData *gpmd = (OpacityGpencilModifierData *)md;

  if (gpmd->curve_intensity) {
    BKE_curvemapping_free(gpmd->curve_intensity);
  }
}

static void foreachIDLink(GpencilModifierData *md, Object *ob, IDWalkFunc walk, void *userData)
{
  OpacityGpencilModifierData *mmd = (OpacityGpencilModifierData *)md;

  walk(userData, ob, (ID **)&mmd->material, IDWALK_CB_USER);
}

static void panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, NULL);

  uiLayoutSetPropSep(layout, true);

  int modify_color = RNA_enum_get(ptr, "modify_color");

  uiItemR(layout, ptr, "modify_color", 0, NULL, ICON_NONE);

  if (modify_color == GP_MODIFY_COLOR_HARDNESS) {
    uiItemR(layout, ptr, "hardness", 0, NULL, ICON_NONE);
  }
  else {
    const bool is_normalized = RNA_boolean_get(ptr, "use_normalized_opacity");
    const bool is_weighted = RNA_boolean_get(ptr, "use_weight_factor");

    uiItemR(layout, ptr, "use_normalized_opacity", 0, NULL, ICON_NONE);
    const char *text = (is_normalized) ? IFACE_("Strength") : IFACE_("Opacity Factor");

    uiLayout *row = uiLayoutRow(layout, true);
    uiLayoutSetActive(row, !is_weighted || is_normalized);
    uiItemR(row, ptr, "factor", 0, text, ICON_NONE);
    if (!is_normalized) {
      uiLayout *sub = uiLayoutRow(row, true);
      uiLayoutSetActive(sub, true);
      uiItemR(row, ptr, "use_weight_factor", 0, "", ICON_MOD_VERTEX_WEIGHT);
    }
  }

  gpencil_modifier_panel_end(layout, ptr);
}

static void mask_panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, NULL);

  int modify_color = RNA_enum_get(ptr, "modify_color");
  bool show_vertex = (modify_color != GP_MODIFY_COLOR_HARDNESS);

  gpencil_modifier_masking_panel_draw(panel, true, show_vertex);
}

static void curve_header_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, NULL);

  int modify_color = RNA_enum_get(ptr, "modify_color");
  uiLayoutSetActive(layout, modify_color != GP_MODIFY_COLOR_HARDNESS);

  gpencil_modifier_curve_header_draw(C, panel);
}

static void curve_panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, NULL);

  int modify_color = RNA_enum_get(ptr, "modify_color");
  uiLayoutSetActive(layout, modify_color != GP_MODIFY_COLOR_HARDNESS);

  gpencil_modifier_curve_panel_draw(C, panel);
}

static void panelRegister(ARegionType *region_type)
{
  PanelType *panel_type = gpencil_modifier_panel_register(
      region_type, eGpencilModifierType_Opacity, panel_draw);

  PanelType *mask_panel_type = gpencil_modifier_subpanel_register(
      region_type, "mask", "Influence", NULL, mask_panel_draw, panel_type);
  gpencil_modifier_subpanel_register(
      region_type, "curve", "", curve_header_draw, curve_panel_draw, mask_panel_type);
}

GpencilModifierTypeInfo modifierType_Gpencil_Opacity = {
    /*name*/ N_("Opacity"),
    /*structName*/ "OpacityGpencilModifierData",
    /*structSize*/ sizeof(OpacityGpencilModifierData),
    /*type*/ eGpencilModifierTypeType_Gpencil,
    /*flags*/ eGpencilModifierTypeFlag_SupportsEditmode,

    /*copyData*/ copyData,

    /*deformStroke*/ deformStroke,
    /*generateStrokes*/ NULL,
    /*bakeModifier*/ bakeModifier,
    /*remapTime*/ NULL,

    /*initData*/ initData,
    /*freeData*/ freeData,
    /*isDisabled*/ NULL,
    /*updateDepsgraph*/ NULL,
    /*dependsOnTime*/ NULL,
    /*foreachIDLink*/ foreachIDLink,
    /*foreachTexLink*/ NULL,
    /*panelRegister*/ panelRegister,
};
