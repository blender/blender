/* SPDX-FileCopyrightText: 2021 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include <stdio.h>

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

#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_gpencil_legacy.h"
#include "BKE_gpencil_modifier_legacy.h"
#include "BKE_lib_query.h"
#include "BKE_modifier.h"
#include "BKE_screen.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "MOD_gpencil_legacy_modifiertypes.h"
#include "MOD_gpencil_legacy_ui_common.h"
#include "MOD_gpencil_legacy_util.h"

static void initData(GpencilModifierData *md)
{
  WeightProxGpencilModifierData *gpmd = (WeightProxGpencilModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(gpmd, modifier));

  MEMCPY_STRUCT_AFTER(gpmd, DNA_struct_default_get(WeightProxGpencilModifierData), modifier);
}

static void copyData(const GpencilModifierData *md, GpencilModifierData *target)
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
static void deformStroke(GpencilModifierData *md,
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

  const float dist_max = MAX2(mmd->dist_start, mmd->dist_end);
  const float dist_min = MIN2(mmd->dist_start, mmd->dist_end);
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

static void bakeModifier(struct Main * /*bmain*/,
                         Depsgraph *depsgraph,
                         GpencilModifierData *md,
                         Object *ob)
{
  generic_bake_deform_stroke(depsgraph, md, ob, false, deformStroke);
}

static void foreachIDLink(GpencilModifierData *md, Object *ob, IDWalkFunc walk, void *userData)
{
  WeightProxGpencilModifierData *mmd = (WeightProxGpencilModifierData *)md;

  walk(userData, ob, (ID **)&mmd->material, IDWALK_CB_USER);
  walk(userData, ob, (ID **)&mmd->object, IDWALK_CB_NOP);
}

static void updateDepsgraph(GpencilModifierData *md,
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

static bool isDisabled(GpencilModifierData *md, int /*userRenderParams*/)
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
  uiItemR(sub, ptr, "use_invert_output", 0, "", ICON_ARROW_LEFTRIGHT);

  uiItemR(layout, ptr, "object", 0, nullptr, ICON_NONE);

  sub = uiLayoutColumn(layout, true);
  uiItemR(sub, ptr, "distance_start", 0, nullptr, ICON_NONE);
  uiItemR(sub, ptr, "distance_end", 0, nullptr, ICON_NONE);

  uiItemR(layout, ptr, "minimum_weight", 0, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "use_multiply", 0, nullptr, ICON_NONE);

  gpencil_modifier_panel_end(layout, ptr);
}

static void mask_panel_draw(const bContext * /*C*/, Panel *panel)
{
  gpencil_modifier_masking_panel_draw(panel, true, true);
}

static void panelRegister(ARegionType *region_type)
{
  PanelType *panel_type = gpencil_modifier_panel_register(
      region_type, eGpencilModifierType_WeightProximity, panel_draw);

  gpencil_modifier_subpanel_register(
      region_type, "mask", "Influence", nullptr, mask_panel_draw, panel_type);
}

GpencilModifierTypeInfo modifierType_Gpencil_WeightProximity = {
    /*name*/ N_("Vertex Weight Proximity"),
    /*structName*/ "WeightProxGpencilModifierData",
    /*structSize*/ sizeof(WeightProxGpencilModifierData),
    /*type*/ eGpencilModifierTypeType_Gpencil,
    /*flags*/ GpencilModifierTypeFlag(0),

    /*copyData*/ copyData,

    /*deformStroke*/ deformStroke,
    /*generateStrokes*/ nullptr,
    /*bakeModifier*/ bakeModifier,
    /*remapTime*/ nullptr,

    /*initData*/ initData,
    /*freeData*/ nullptr,
    /*isDisabled*/ isDisabled,
    /*updateDepsgraph*/ updateDepsgraph,
    /*dependsOnTime*/ nullptr,
    /*foreachIDLink*/ foreachIDLink,
    /*foreachTexLink*/ nullptr,
    /*panelRegister*/ panelRegister,
};
