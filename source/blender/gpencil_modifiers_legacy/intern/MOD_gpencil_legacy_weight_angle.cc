/* SPDX-FileCopyrightText: 2021 Blender Foundation
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

static void init_data(GpencilModifierData *md)
{
  WeightAngleGpencilModifierData *gpmd = (WeightAngleGpencilModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(gpmd, modifier));

  MEMCPY_STRUCT_AFTER(gpmd, DNA_struct_default_get(WeightAngleGpencilModifierData), modifier);
}

static void copy_data(const GpencilModifierData *md, GpencilModifierData *target)
{
  BKE_gpencil_modifier_copydata_generic(md, target);
}

/* change stroke thickness */
static void deform_stroke(GpencilModifierData *md,
                          Depsgraph * /*depsgraph*/,
                          Object *ob,
                          bGPDlayer *gpl,
                          bGPDframe * /*gpf*/,
                          bGPDstroke *gps)
{
  WeightAngleGpencilModifierData *mmd = (WeightAngleGpencilModifierData *)md;
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

  const int target_def_nr = BKE_object_defgroup_name_index(ob, mmd->target_vgname);

  if (target_def_nr == -1) {
    return;
  }

  /* Use default Z up. */
  float vec_axis[3] = {0.0f, 0.0f, 1.0f};
  float axis[3] = {0.0f, 0.0f, 0.0f};
  axis[mmd->axis] = 1.0f;
  float vec_ref[3];
  /* Apply modifier rotation (sub 90 degrees for Y axis due Z-Up vector). */
  float rot_angle = mmd->angle - ((mmd->axis == 1) ? M_PI_2 : 0.0f);
  rotate_normalized_v3_v3v3fl(vec_ref, vec_axis, axis, rot_angle);

  /* Apply the rotation of the object. */
  if (mmd->space == GP_SPACE_LOCAL) {
    mul_mat3_m4_v3(ob->object_to_world, vec_ref);
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

    /* Special case for single points. */
    if (gps->totpoints == 1) {
      weight_pt = 1.0f;
      break;
    }

    bGPDspoint *pt1 = (i > 0) ? &gps->points[i] : &gps->points[i + 1];
    bGPDspoint *pt2 = (i > 0) ? &gps->points[i - 1] : &gps->points[i];
    float fpt1[3], fpt2[3];
    mul_v3_m4v3(fpt1, ob->object_to_world, &pt1->x);
    mul_v3_m4v3(fpt2, ob->object_to_world, &pt2->x);

    float vec[3];
    sub_v3_v3v3(vec, fpt1, fpt2);
    float angle = angle_on_axis_v3v3_v3(vec_ref, vec, axis);
    /* Use sin to get a value between 0 and 1. */
    weight_pt = 1.0f - sin(angle);

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
  WeightAngleGpencilModifierData *mmd = (WeightAngleGpencilModifierData *)md;

  walk(user_data, ob, (ID **)&mmd->material, IDWALK_CB_USER);
}

static bool is_disabled(GpencilModifierData *md, bool /*use_render_params*/)
{
  WeightAngleGpencilModifierData *mmd = (WeightAngleGpencilModifierData *)md;

  return (mmd->target_vgname[0] == '\0');
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

  uiItemR(layout, ptr, "angle", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "axis", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "space", UI_ITEM_NONE, nullptr, ICON_NONE);

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
      region_type, eGpencilModifierType_WeightAngle, panel_draw);

  gpencil_modifier_subpanel_register(
      region_type, "mask", "Influence", nullptr, mask_panel_draw, panel_type);
}

GpencilModifierTypeInfo modifierType_Gpencil_WeightAngle = {
    /*name*/ N_("Vertex Weight Angle"),
    /*struct_name*/ "WeightAngleGpencilModifierData",
    /*struct_size*/ sizeof(WeightAngleGpencilModifierData),
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
    /*update_depsgraph*/ nullptr,
    /*depends_on_time*/ nullptr,
    /*foreach_ID_link*/ foreach_ID_link,
    /*foreach_tex_link*/ nullptr,
    /*panel_register*/ panel_register,
};
