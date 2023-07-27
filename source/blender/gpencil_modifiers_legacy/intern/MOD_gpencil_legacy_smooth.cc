/* SPDX-FileCopyrightText: 2017 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include <cstdio>
#include <cstring> /* For #MEMCPY_STRUCT_AFTER. */

#include "BLI_listbase.h"
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
#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_modifier_legacy.h"
#include "BKE_lib_query.h"
#include "BKE_modifier.h"
#include "BKE_screen.h"

#include "DEG_depsgraph.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "MOD_gpencil_legacy_modifiertypes.h"
#include "MOD_gpencil_legacy_ui_common.h"
#include "MOD_gpencil_legacy_util.h"

#include "MEM_guardedalloc.h"

static void initData(GpencilModifierData *md)
{
  SmoothGpencilModifierData *gpmd = (SmoothGpencilModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(gpmd, modifier));

  MEMCPY_STRUCT_AFTER(gpmd, DNA_struct_default_get(SmoothGpencilModifierData), modifier);

  gpmd->curve_intensity = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
  BKE_curvemapping_init(gpmd->curve_intensity);
}

static void copyData(const GpencilModifierData *md, GpencilModifierData *target)
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
static void deformStroke(GpencilModifierData *md,
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

static void bakeModifier(Main * /*bmain*/,
                         Depsgraph *depsgraph,
                         GpencilModifierData *md,
                         Object *ob)
{
  generic_bake_deform_stroke(depsgraph, md, ob, false, deformStroke);
}

static void freeData(GpencilModifierData *md)
{
  SmoothGpencilModifierData *gpmd = (SmoothGpencilModifierData *)md;

  if (gpmd->curve_intensity) {
    BKE_curvemapping_free(gpmd->curve_intensity);
  }
}

static void foreachIDLink(GpencilModifierData *md, Object *ob, IDWalkFunc walk, void *userData)
{
  SmoothGpencilModifierData *mmd = (SmoothGpencilModifierData *)md;

  walk(userData, ob, (ID **)&mmd->material, IDWALK_CB_USER);
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

  uiItemR(layout, ptr, "factor", 0, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "step", 0, IFACE_("Repeat"), ICON_NONE);

  col = uiLayoutColumn(layout, false);
  uiLayoutSetActive(col, RNA_boolean_get(ptr, "use_edit_position"));
  uiItemR(col, ptr, "use_keep_shape", 0, nullptr, ICON_NONE);

  gpencil_modifier_panel_end(layout, ptr);
}

static void mask_panel_draw(const bContext * /*C*/, Panel *panel)
{
  gpencil_modifier_masking_panel_draw(panel, true, true);
}

static void panelRegister(ARegionType *region_type)
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

    /*copyData*/ copyData,

    /*deformStroke*/ deformStroke,
    /*generateStrokes*/ nullptr,
    /*bakeModifier*/ bakeModifier,
    /*remapTime*/ nullptr,

    /*initData*/ initData,
    /*freeData*/ freeData,
    /*isDisabled*/ nullptr,
    /*updateDepsgraph*/ nullptr,
    /*dependsOnTime*/ nullptr,
    /*foreachIDLink*/ foreachIDLink,
    /*foreachTexLink*/ nullptr,
    /*panelRegister*/ panelRegister,
};
