/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include <cstdio>

#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "DNA_defaults.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "BKE_deform.hh"
#include "BKE_gpencil_legacy.h"
#include "BKE_gpencil_modifier_legacy.h"
#include "BKE_lib_query.hh"
#include "BKE_modifier.hh"

#include "DEG_depsgraph_build.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"

#include "MOD_gpencil_legacy_ui_common.h"
#include "MOD_gpencil_legacy_util.h"

static void init_data(GpencilModifierData *md)
{
  WeightProxGpencilModifierData *gpmd = (WeightProxGpencilModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(gpmd, modifier));

  MEMCPY_STRUCT_AFTER(gpmd, DNA_struct_default_get(WeightProxGpencilModifierData), modifier);
}

static void copy_data(const GpencilModifierData *md, GpencilModifierData *target)
{
  BKE_gpencil_modifier_copydata_generic(md, target);
}

/* Calc distance between point and target object. */
static float calc_point_weight_by_distance(Object *ob,
                                           WeightProxGpencilModifierData *mmd,
                                           const float dist_max,
                                           const float dist_min,
                                           bGPDspoint *pt)
{
  float weight;
  float gvert[3];
  mul_v3_m4v3(gvert, ob->object_to_world, &pt->x);
  float dist = len_v3v3(mmd->object->object_to_world[3], gvert);

  if (dist > dist_max) {
    weight = 1.0f;
  }
  else if (dist <= dist_max && dist > dist_min) {
    weight = 1.0f - ((dist_max - dist) / max_ff((dist_max - dist_min), 0.0001f));
  }
  else {
    weight = 0.0f;
  }

  return weight;
}

/* change stroke thickness */
static void deform_stroke(GpencilModifierData *md,
                          Depsgraph * /*depsgraph*/,
                          Object *ob,
                          bGPDlayer *gpl,
                          bGPDframe * /*gpf*/,
                          bGPDstroke *gps)
{
  WeightProxGpencilModifierData *mmd = (WeightProxGpencilModifierData *)md;
  const int def_nr = BKE_object_defgroup_name_index(ob, mmd->vgname);

  if (!is_stroke_affected_by_modifier(ob,
                                      mmd->layername,
                                      mmd->material,
                                      mmd->pass_index,
                                      mmd->layer_pass,
                                      1,
                                      gpl,
                                      gps,
                                      mmd->flag & GP_WEIGHT_INVERT_LAYER,
                                      mmd->flag & GP_WEIGHT_INVERT_PASS,
                                      mmd->flag & GP_WEIGHT_INVERT_LAYERPASS,
                                      mmd->flag & GP_WEIGHT_INVERT_MATERIAL))
  {
    return;
  }

  const float dist_max = std::max(mmd->dist_start, mmd->dist_end);
  const float dist_min = std::min(mmd->dist_start, mmd->dist_end);
  const int target_def_nr = BKE_object_defgroup_name_index(ob, mmd->target_vgname);

  if (target_def_nr == -1) {
    return;
  }

  /* Ensure there is a vertex group. */
  BKE_gpencil_dvert_ensure(gps);

  float weight_pt = 1.0f;
  for (int i = 0; i < gps->totpoints; i++) {
    MDeformVert *dvert = gps->dvert != nullptr ? &gps->dvert[i] : nullptr;
    /* Verify point is part of vertex group. */
    float weight = get_modifier_point_weight(
        dvert, (mmd->flag & GP_WEIGHT_INVERT_VGROUP) != 0, def_nr);
    if (weight < 0.0f) {
      continue;
    }

    if (mmd->object) {
      bGPDspoint *pt = &gps->points[i];
      weight_pt = calc_point_weight_by_distance(ob, mmd, dist_max, dist_min, pt);
    }

    /* Invert weight if required. */
    if (mmd->flag & GP_WEIGHT_INVERT_OUTPUT) {
      weight_pt = 1.0f - weight_pt;
    }
    /* Assign weight. */
    dvert = gps->dvert != nullptr ? &gps->dvert[i] : nullptr;
    if (dvert != nullptr) {
      MDeformWeight *dw = BKE_defvert_ensure_index(dvert, target_def_nr);
      if (dw) {
        dw->weight = (mmd->flag & GP_WEIGHT_MULTIPLY_DATA) ? dw->weight * weight_pt : weight_pt;
        CLAMP(dw->weight, mmd->min_weight, 1.0f);
      }
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

static void foreach_ID_link(GpencilModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  WeightProxGpencilModifierData *mmd = (WeightProxGpencilModifierData *)md;

  walk(user_data, ob, (ID **)&mmd->material, IDWALK_CB_USER);
  walk(user_data, ob, (ID **)&mmd->object, IDWALK_CB_NOP);
}

static void update_depsgraph(GpencilModifierData *md,
                             const ModifierUpdateDepsgraphContext *ctx,
                             const int /*mode*/)
{
  WeightProxGpencilModifierData *mmd = (WeightProxGpencilModifierData *)md;
  if (mmd->object != nullptr) {
    DEG_add_object_relation(
        ctx->node, mmd->object, DEG_OB_COMP_TRANSFORM, "GPencil Weight Modifier");
  }
  DEG_add_object_relation(
      ctx->node, ctx->object, DEG_OB_COMP_TRANSFORM, "GPencil Weight Modifier");
}

static bool is_disabled(GpencilModifierData *md, bool /*use_render_params*/)
{
  WeightProxGpencilModifierData *mmd = (WeightProxGpencilModifierData *)md;

  return ((mmd->target_vgname[0] == '\0') || (mmd->object == nullptr));
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *row, *sub;
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, &ob_ptr);

  uiLayoutSetPropSep(layout, true);
  row = uiLayoutRow(layout, true);
  uiItemPointerR(row, ptr, "target_vertex_group", &ob_ptr, "vertex_groups", nullptr, ICON_NONE);
  sub = uiLayoutRow(row, true);
  bool has_output = RNA_string_length(ptr, "target_vertex_group") != 0;
  uiLayoutSetPropDecorate(sub, false);
  uiLayoutSetActive(sub, has_output);
  uiItemR(sub, ptr, "use_invert_output", UI_ITEM_NONE, "", ICON_ARROW_LEFTRIGHT);

  uiItemR(layout, ptr, "object", UI_ITEM_NONE, nullptr, ICON_NONE);

  sub = uiLayoutColumn(layout, true);
  uiItemR(sub, ptr, "distance_start", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(sub, ptr, "distance_end", UI_ITEM_NONE, nullptr, ICON_NONE);

  uiItemR(layout, ptr, "minimum_weight", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "use_multiply", UI_ITEM_NONE, nullptr, ICON_NONE);

  gpencil_modifier_panel_end(layout, ptr);
}

static void mask_panel_draw(const bContext * /*C*/, Panel *panel)
{
  gpencil_modifier_masking_panel_draw(panel, true, true);
}

static void panel_register(ARegionType *region_type)
{
  PanelType *panel_type = gpencil_modifier_panel_register(
      region_type, eGpencilModifierType_WeightProximity, panel_draw);

  gpencil_modifier_subpanel_register(
      region_type, "mask", "Influence", nullptr, mask_panel_draw, panel_type);
}

GpencilModifierTypeInfo modifierType_Gpencil_WeightProximity = {
    /*name*/ N_("Vertex Weight Proximity"),
    /*struct_name*/ "WeightProxGpencilModifierData",
    /*struct_size*/ sizeof(WeightProxGpencilModifierData),
    /*type*/ eGpencilModifierTypeType_Gpencil,
    /*flags*/ GpencilModifierTypeFlag(0),

    /*copy_data*/ copy_data,

    /*deform_stroke*/ deform_stroke,
    /*generate_strokes*/ nullptr,
    /*bake_modifier*/ bake_modifier,
    /*remap_time*/ nullptr,

    /*init_data*/ init_data,
    /*free_data*/ nullptr,
    /*is_disabled*/ is_disabled,
    /*update_depsgraph*/ update_depsgraph,
    /*depends_on_time*/ nullptr,
    /*foreach_ID_link*/ foreach_ID_link,
    /*foreach_tex_link*/ nullptr,
    /*panel_register*/ panel_register,
};
