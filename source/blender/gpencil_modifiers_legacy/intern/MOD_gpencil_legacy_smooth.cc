/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include <cstdio>
#include <cstring> /* For #MEMCPY_STRUCT_AFTER. */

#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "DNA_defaults.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_screen_types.h"

#include "BKE_colortools.hh"
#include "BKE_deform.hh"
#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_modifier_legacy.h"
#include "BKE_lib_query.hh"
#include "BKE_modifier.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"

#include "MOD_gpencil_legacy_ui_common.h"
#include "MOD_gpencil_legacy_util.h"

#include "MEM_guardedalloc.h"

static void init_data(GpencilModifierData *md)
{
  SmoothGpencilModifierData *gpmd = (SmoothGpencilModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(gpmd, modifier));

  MEMCPY_STRUCT_AFTER(gpmd, DNA_struct_default_get(SmoothGpencilModifierData), modifier);

  gpmd->curve_intensity = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
  BKE_curvemapping_init(gpmd->curve_intensity);
}

static void copy_data(const GpencilModifierData *md, GpencilModifierData *target)
{
  SmoothGpencilModifierData *gmd = (SmoothGpencilModifierData *)md;
  SmoothGpencilModifierData *tgmd = (SmoothGpencilModifierData *)target;

  if (tgmd->curve_intensity != nullptr) {
    BKE_curvemapping_free(tgmd->curve_intensity);
    tgmd->curve_intensity = nullptr;
  }

  BKE_gpencil_modifier_copydata_generic(md, target);

  tgmd->curve_intensity = BKE_curvemapping_copy(gmd->curve_intensity);
}

/**
 * Apply smooth effect based on stroke direction.
 */
static void deform_stroke(GpencilModifierData *md,
                          Depsgraph * /*depsgraph*/,
                          Object *ob,
                          bGPDlayer *gpl,
                          bGPDframe * /*gpf*/,
                          bGPDstroke *gps)
{
  SmoothGpencilModifierData *mmd = (SmoothGpencilModifierData *)md;
  const int def_nr = BKE_object_defgroup_name_index(ob, mmd->vgname);
  const bool use_curve = (mmd->flag & GP_SMOOTH_CUSTOM_CURVE) != 0 && mmd->curve_intensity;

  if (!is_stroke_affected_by_modifier(ob,
                                      mmd->layername,
                                      mmd->material,
                                      mmd->pass_index,
                                      mmd->layer_pass,
                                      3,
                                      gpl,
                                      gps,
                                      mmd->flag & GP_SMOOTH_INVERT_LAYER,
                                      mmd->flag & GP_SMOOTH_INVERT_PASS,
                                      mmd->flag & GP_SMOOTH_INVERT_LAYERPASS,
                                      mmd->flag & GP_SMOOTH_INVERT_MATERIAL))
  {
    return;
  }

  if (mmd->factor <= 0.0f || mmd->step <= 0) {
    return;
  }

  float *weights = nullptr;
  if (def_nr != -1 || use_curve) {
    weights = static_cast<float *>(MEM_malloc_arrayN(gps->totpoints, sizeof(*weights), __func__));
    /* Calculate weights. */
    for (int i = 0; i < gps->totpoints; i++) {
      MDeformVert *dvert = gps->dvert != nullptr ? &gps->dvert[i] : nullptr;

      /* Verify vertex group. */
      float weight = get_modifier_point_weight(
          dvert, (mmd->flag & GP_SMOOTH_INVERT_VGROUP) != 0, def_nr);

      /* Custom curve to modulate value. */
      if (use_curve && weight > 0.0f) {
        float value = float(i) / (gps->totpoints - 1);
        weight *= BKE_curvemapping_evaluateF(mmd->curve_intensity, 0, value);
      }

      weights[i] = weight;
    }
  }
  BKE_gpencil_stroke_smooth(gps,
                            mmd->factor,
                            mmd->step,
                            mmd->flag & GP_SMOOTH_MOD_LOCATION,
                            mmd->flag & GP_SMOOTH_MOD_STRENGTH,
                            mmd->flag & GP_SMOOTH_MOD_THICKNESS,
                            mmd->flag & GP_SMOOTH_MOD_UV,
                            mmd->flag & GP_SMOOTH_KEEP_SHAPE,
                            weights);
  MEM_SAFE_FREE(weights);
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
  SmoothGpencilModifierData *gpmd = (SmoothGpencilModifierData *)md;

  if (gpmd->curve_intensity) {
    BKE_curvemapping_free(gpmd->curve_intensity);
  }
}

static void foreach_ID_link(GpencilModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  SmoothGpencilModifierData *mmd = (SmoothGpencilModifierData *)md;

  walk(user_data, ob, (ID **)&mmd->material, IDWALK_CB_USER);
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *row, *col;
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, nullptr);

  row = uiLayoutRow(layout, true);
  uiItemR(row, ptr, "use_edit_position", UI_ITEM_R_TOGGLE, IFACE_("Position"), ICON_NONE);
  uiItemR(row, ptr, "use_edit_strength", UI_ITEM_R_TOGGLE, IFACE_("Strength"), ICON_NONE);
  uiItemR(row, ptr, "use_edit_thickness", UI_ITEM_R_TOGGLE, IFACE_("Thickness"), ICON_NONE);
  uiItemR(row, ptr, "use_edit_uv", UI_ITEM_R_TOGGLE, IFACE_("UV"), ICON_NONE);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "factor", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "step", UI_ITEM_NONE, IFACE_("Repeat"), ICON_NONE);

  col = uiLayoutColumn(layout, false);
  uiLayoutSetActive(col, RNA_boolean_get(ptr, "use_edit_position"));
  uiItemR(col, ptr, "use_keep_shape", UI_ITEM_NONE, nullptr, ICON_NONE);

  gpencil_modifier_panel_end(layout, ptr);
}

static void mask_panel_draw(const bContext * /*C*/, Panel *panel)
{
  gpencil_modifier_masking_panel_draw(panel, true, true);
}

static void panel_register(ARegionType *region_type)
{
  PanelType *panel_type = gpencil_modifier_panel_register(
      region_type, eGpencilModifierType_Smooth, panel_draw);
  PanelType *mask_panel_type = gpencil_modifier_subpanel_register(
      region_type, "mask", "Influence", nullptr, mask_panel_draw, panel_type);
  gpencil_modifier_subpanel_register(region_type,
                                     "curve",
                                     "",
                                     gpencil_modifier_curve_header_draw,
                                     gpencil_modifier_curve_panel_draw,
                                     mask_panel_type);
}

GpencilModifierTypeInfo modifierType_Gpencil_Smooth = {
    /*name*/ N_("Smooth"),
    /*struct_name*/ "SmoothGpencilModifierData",
    /*struct_size*/ sizeof(SmoothGpencilModifierData),
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
