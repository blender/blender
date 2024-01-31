/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include <cstdio>

#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_defaults.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "BKE_colortools.hh"
#include "BKE_context.hh"
#include "BKE_deform.hh"
#include "BKE_gpencil_modifier_legacy.h"
#include "BKE_lib_query.hh"
#include "BKE_modifier.hh"
#include "BKE_screen.hh"

#include "DEG_depsgraph.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"

#include "MOD_gpencil_legacy_modifiertypes.h"
#include "MOD_gpencil_legacy_ui_common.h"
#include "MOD_gpencil_legacy_util.h"

static void init_data(GpencilModifierData *md)
{
  ThickGpencilModifierData *gpmd = (ThickGpencilModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(gpmd, modifier));

  MEMCPY_STRUCT_AFTER(gpmd, DNA_struct_default_get(ThickGpencilModifierData), modifier);

  gpmd->curve_thickness = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
  BKE_curvemapping_init(gpmd->curve_thickness);
}

static void free_data(GpencilModifierData *md)
{
  ThickGpencilModifierData *gpmd = (ThickGpencilModifierData *)md;

  if (gpmd->curve_thickness) {
    BKE_curvemapping_free(gpmd->curve_thickness);
  }
}

static void copy_data(const GpencilModifierData *md, GpencilModifierData *target)
{
  ThickGpencilModifierData *gmd = (ThickGpencilModifierData *)md;
  ThickGpencilModifierData *tgmd = (ThickGpencilModifierData *)target;

  if (tgmd->curve_thickness != nullptr) {
    BKE_curvemapping_free(tgmd->curve_thickness);
    tgmd->curve_thickness = nullptr;
  }

  BKE_gpencil_modifier_copydata_generic(md, target);

  tgmd->curve_thickness = BKE_curvemapping_copy(gmd->curve_thickness);
}

/* change stroke thickness */
static void deform_stroke(GpencilModifierData *md,
                          Depsgraph * /*depsgraph*/,
                          Object *ob,
                          bGPDlayer *gpl,
                          bGPDframe * /*gpf*/,
                          bGPDstroke *gps)
{
  ThickGpencilModifierData *mmd = (ThickGpencilModifierData *)md;
  const int def_nr = BKE_object_defgroup_name_index(ob, mmd->vgname);

  if (!is_stroke_affected_by_modifier(ob,
                                      mmd->layername,
                                      mmd->material,
                                      mmd->pass_index,
                                      mmd->layer_pass,
                                      1,
                                      gpl,
                                      gps,
                                      mmd->flag & GP_THICK_INVERT_LAYER,
                                      mmd->flag & GP_THICK_INVERT_PASS,
                                      mmd->flag & GP_THICK_INVERT_LAYERPASS,
                                      mmd->flag & GP_THICK_INVERT_MATERIAL))
  {
    return;
  }

  float stroke_thickness_inv = 1.0f / max_ii(gps->thickness, 1);
  const bool is_normalized = (mmd->flag & GP_THICK_NORMALIZE);
  bool is_inverted = ((mmd->flag & GP_THICK_WEIGHT_FACTOR) == 0) &&
                     ((mmd->flag & GP_THICK_INVERT_VGROUP) != 0);

  for (int i = 0; i < gps->totpoints; i++) {
    bGPDspoint *pt = &gps->points[i];
    MDeformVert *dvert = gps->dvert != nullptr ? &gps->dvert[i] : nullptr;
    /* Verify point is part of vertex group. */
    float weight = get_modifier_point_weight(dvert, is_inverted, def_nr);
    if (weight < 0.0f) {
      continue;
    }

    /* Apply weight directly. */
    if ((!is_normalized) && (mmd->flag & GP_THICK_WEIGHT_FACTOR)) {
      pt->pressure *= ((mmd->flag & GP_THICK_INVERT_VGROUP) ? 1.0f - weight : weight);
      CLAMP_MIN(pt->pressure, 0.0f);
      continue;
    }

    float curvef = 1.0f;

    if ((mmd->flag & GP_THICK_CUSTOM_CURVE) && (mmd->curve_thickness)) {
      /* Normalize value to evaluate curve. */
      float value = float(i) / (gps->totpoints - 1);
      curvef = BKE_curvemapping_evaluateF(mmd->curve_thickness, 0, value);
    }

    float target;
    if (is_normalized) {
      target = mmd->thickness * stroke_thickness_inv;
      target *= curvef;
    }
    else {
      target = pt->pressure * mmd->thickness_fac;
      weight *= curvef;
    }

    pt->pressure = interpf(target, pt->pressure, weight);

    CLAMP_MIN(pt->pressure, 0.0f);
  }
}

static void bake_modifier(Main * /*bmain*/,
                          Depsgraph *depsgraph,
                          GpencilModifierData *md,
                          Object *ob)
{
  generic_bake_deform_stroke(depsgraph, md, ob, false, deform_stroke);
}

static void foreach_ID_link(GpencilModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  ThickGpencilModifierData *mmd = (ThickGpencilModifierData *)md;

  walk(user_data, ob, (ID **)&mmd->material, IDWALK_CB_USER);
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, nullptr);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "use_normalized_thickness", UI_ITEM_NONE, nullptr, ICON_NONE);
  if (RNA_boolean_get(ptr, "use_normalized_thickness")) {
    uiItemR(layout, ptr, "thickness", UI_ITEM_NONE, nullptr, ICON_NONE);
  }
  else {
    const bool is_weighted = !RNA_boolean_get(ptr, "use_weight_factor");
    uiLayout *row = uiLayoutRow(layout, true);
    uiLayoutSetActive(row, is_weighted);
    uiItemR(row, ptr, "thickness_factor", UI_ITEM_NONE, nullptr, ICON_NONE);
    uiLayout *sub = uiLayoutRow(row, true);
    uiLayoutSetActive(sub, true);
    uiItemR(row, ptr, "use_weight_factor", UI_ITEM_NONE, "", ICON_MOD_VERTEX_WEIGHT);
  }

  gpencil_modifier_panel_end(layout, ptr);
}

static void mask_panel_draw(const bContext * /*C*/, Panel *panel)
{
  gpencil_modifier_masking_panel_draw(panel, true, true);
}

static void panel_register(ARegionType *region_type)
{
  PanelType *panel_type = gpencil_modifier_panel_register(
      region_type, eGpencilModifierType_Thick, panel_draw);
  PanelType *mask_panel_type = gpencil_modifier_subpanel_register(
      region_type, "mask", "Influence", nullptr, mask_panel_draw, panel_type);
  gpencil_modifier_subpanel_register(region_type,
                                     "curve",
                                     "",
                                     gpencil_modifier_curve_header_draw,
                                     gpencil_modifier_curve_panel_draw,
                                     mask_panel_type);
}

GpencilModifierTypeInfo modifierType_Gpencil_Thick = {
    /*name*/ N_("Thickness"),
    /*struct_name*/ "ThickGpencilModifierData",
    /*struct_size*/ sizeof(ThickGpencilModifierData),
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
