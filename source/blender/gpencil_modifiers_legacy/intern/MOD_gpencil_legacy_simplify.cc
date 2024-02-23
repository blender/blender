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
#include "DNA_gpencil_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_modifier_legacy.h"
#include "BKE_lib_query.hh"
#include "BKE_modifier.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"

#include "MOD_gpencil_legacy_ui_common.h"
#include "MOD_gpencil_legacy_util.h"

static void init_data(GpencilModifierData *md)
{
  SimplifyGpencilModifierData *gpmd = (SimplifyGpencilModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(gpmd, modifier));

  MEMCPY_STRUCT_AFTER(gpmd, DNA_struct_default_get(SimplifyGpencilModifierData), modifier);
}

static void copy_data(const GpencilModifierData *md, GpencilModifierData *target)
{
  BKE_gpencil_modifier_copydata_generic(md, target);
}

static void deform_stroke(GpencilModifierData *md,
                          Depsgraph * /*depsgraph*/,
                          Object *ob,
                          bGPDlayer *gpl,
                          bGPDframe *gpf,
                          bGPDstroke *gps)
{
  SimplifyGpencilModifierData *mmd = (SimplifyGpencilModifierData *)md;

  if (!is_stroke_affected_by_modifier(ob,
                                      mmd->layername,
                                      mmd->material,
                                      mmd->pass_index,
                                      mmd->layer_pass,
                                      mmd->mode == GP_SIMPLIFY_SAMPLE ? 2 : 3,
                                      gpl,
                                      gps,
                                      mmd->flag & GP_SIMPLIFY_INVERT_LAYER,
                                      mmd->flag & GP_SIMPLIFY_INVERT_PASS,
                                      mmd->flag & GP_SIMPLIFY_INVERT_LAYERPASS,
                                      mmd->flag & GP_SIMPLIFY_INVERT_MATERIAL))
  {
    return;
  }
  bGPdata *gpd = static_cast<bGPdata *>(ob->data);
  /* Select simplification mode. */
  switch (mmd->mode) {
    case GP_SIMPLIFY_FIXED: {
      for (int i = 0; i < mmd->step; i++) {
        BKE_gpencil_stroke_simplify_fixed(gpd, gps);
      }
      break;
    }
    case GP_SIMPLIFY_ADAPTIVE: {
      /* simplify stroke using Ramer-Douglas-Peucker algorithm */
      BKE_gpencil_stroke_simplify_adaptive(gpd, gps, mmd->factor);
      break;
    }
    case GP_SIMPLIFY_SAMPLE: {
      BKE_gpencil_stroke_sample(gpd, gps, mmd->length, false, mmd->sharp_threshold);
      break;
    }
    case GP_SIMPLIFY_MERGE: {
      BKE_gpencil_stroke_merge_distance(gpd, gpf, gps, mmd->distance, true);
      break;
    }
    default:
      break;
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
  SimplifyGpencilModifierData *mmd = (SimplifyGpencilModifierData *)md;

  walk(user_data, ob, (ID **)&mmd->material, IDWALK_CB_USER);
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, nullptr);

  int mode = RNA_enum_get(ptr, "mode");

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "mode", UI_ITEM_NONE, nullptr, ICON_NONE);

  if (mode == GP_SIMPLIFY_FIXED) {
    uiItemR(layout, ptr, "step", UI_ITEM_NONE, nullptr, ICON_NONE);
  }
  else if (mode == GP_SIMPLIFY_ADAPTIVE) {
    uiItemR(layout, ptr, "factor", UI_ITEM_NONE, nullptr, ICON_NONE);
  }
  else if (mode == GP_SIMPLIFY_SAMPLE) {
    uiItemR(layout, ptr, "length", UI_ITEM_NONE, nullptr, ICON_NONE);
    uiItemR(layout, ptr, "sharp_threshold", UI_ITEM_NONE, nullptr, ICON_NONE);
  }
  else if (mode == GP_SIMPLIFY_MERGE) {
    uiItemR(layout, ptr, "distance", UI_ITEM_NONE, nullptr, ICON_NONE);
  }

  gpencil_modifier_panel_end(layout, ptr);
}

static void mask_panel_draw(const bContext * /*C*/, Panel *panel)
{
  gpencil_modifier_masking_panel_draw(panel, true, false);
}

static void panel_register(ARegionType *region_type)
{
  PanelType *panel_type = gpencil_modifier_panel_register(
      region_type, eGpencilModifierType_Simplify, panel_draw);
  gpencil_modifier_subpanel_register(
      region_type, "mask", "Influence", nullptr, mask_panel_draw, panel_type);
}

GpencilModifierTypeInfo modifierType_Gpencil_Simplify = {
    /*name*/ N_("Simplify"),
    /*struct_name*/ "SimplifyGpencilModifierData",
    /*struct_size*/ sizeof(SimplifyGpencilModifierData),
    /*type*/ eGpencilModifierTypeType_Gpencil,
    /*flags*/ eGpencilModifierTypeFlag_SupportsEditmode,

    /*copy_data*/ copy_data,

    /*deform_stroke*/ deform_stroke,
    /*generate_strokes*/ nullptr,
    /*bake_modifier*/ bake_modifier,
    /*remap_time*/ nullptr,

    /*init_data*/ init_data,
    /*free_data*/ nullptr,
    /*is_disabled*/ nullptr,
    /*update_depsgraph*/ nullptr,
    /*depends_on_time*/ nullptr,
    /*foreach_ID_link*/ foreach_ID_link,
    /*foreach_tex_link*/ nullptr,
    /*panel_register*/ panel_register,
};
