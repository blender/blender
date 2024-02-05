/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include <cstdio>

#include "BLI_hash.h"
#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_rand.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BLI_hash.h"
#include "BLI_rand.h"
#include "DNA_defaults.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "RNA_access.hh"

#include "BKE_context.hh"
#include "BKE_deform.hh"
#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_modifier_legacy.h"
#include "BKE_lib_query.hh"
#include "BKE_modifier.hh"
#include "BKE_screen.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"
#include "DEG_depsgraph_query.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "MOD_gpencil_legacy_modifiertypes.h"
#include "MOD_gpencil_legacy_ui_common.h"
#include "MOD_gpencil_legacy_util.h"

static void init_data(GpencilModifierData *md)
{
  OffsetGpencilModifierData *gpmd = (OffsetGpencilModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(gpmd, modifier));

  MEMCPY_STRUCT_AFTER(gpmd, DNA_struct_default_get(OffsetGpencilModifierData), modifier);
  /* Open the first subpanel too, because it's activated by default. */
  md->ui_expand_flag = UI_PANEL_DATA_EXPAND_ROOT | UI_SUBPANEL_DATA_EXPAND_1;
}

static void copy_data(const GpencilModifierData *md, GpencilModifierData *target)
{
  BKE_gpencil_modifier_copydata_generic(md, target);
}

/* Change stroke offset. */
static void deform_stroke(GpencilModifierData *md,
                          Depsgraph * /*depsgraph*/,
                          Object *ob,
                          bGPDlayer *gpl,
                          bGPDframe *gpf,
                          bGPDstroke *gps)
{
  OffsetGpencilModifierData *mmd = (OffsetGpencilModifierData *)md;
  const int def_nr = BKE_object_defgroup_name_index(ob, mmd->vgname);

  float mat[4][4];
  float loc[3], rot[3], scale[3];

  if (!is_stroke_affected_by_modifier(ob,
                                      mmd->layername,
                                      mmd->material,
                                      mmd->pass_index,
                                      mmd->layer_pass,
                                      1,
                                      gpl,
                                      gps,
                                      mmd->flag & GP_OFFSET_INVERT_LAYER,
                                      mmd->flag & GP_OFFSET_INVERT_PASS,
                                      mmd->flag & GP_OFFSET_INVERT_LAYERPASS,
                                      mmd->flag & GP_OFFSET_INVERT_MATERIAL))
  {
    return;
  }

  const bool is_randomized = !(is_zero_v3(mmd->rnd_offset) && is_zero_v3(mmd->rnd_rot) &&
                               is_zero_v3(mmd->rnd_scale));
  const bool is_general = !(is_zero_v3(mmd->loc) && is_zero_v3(mmd->rot) &&
                            is_zero_v3(mmd->scale));

  int seed = mmd->seed;
  /* Make sure different modifiers get different seeds. */
  seed += BLI_hash_string(ob->id.name + 2);
  seed += BLI_hash_string(md->name);

  float rand[3][3];
  float rand_offset = BLI_hash_int_01(seed);
  bGPdata *gpd = static_cast<bGPdata *>(ob->data);

  if (is_randomized && mmd->mode == GP_OFFSET_RANDOM) {
    /* Get stroke index for random offset. */
    int rnd_index = BLI_findindex(&gpf->strokes, gps);
    for (int j = 0; j < 3; j++) {
      const uint primes[3] = {2, 3, 7};
      double offset[3] = {0.0f, 0.0f, 0.0f};
      double r[3];
      /* To ensure a nice distribution, we use halton sequence and offset using the seed. */
      BLI_halton_3d(primes, offset, rnd_index, r);

      if ((mmd->flag & GP_OFFSET_UNIFORM_RANDOM_SCALE) && j == 2) {
        float rand_value;
        rand_value = fmodf(r[0] * 2.0f - 1.0f + rand_offset, 1.0f);
        rand_value = fmodf(sin(rand_value * 12.9898f + j * 78.233f) * 43758.5453f, 1.0f);
        copy_v3_fl(rand[j], rand_value);
      }
      else {
        for (int i = 0; i < 3; i++) {
          rand[j][i] = fmodf(r[i] * 2.0f - 1.0f + rand_offset, 1.0f);
          rand[j][i] = fmodf(sin(rand[j][i] * 12.9898f + j * 78.233f) * 43758.5453f, 1.0f);
        }
      }
    }
  }
  else {
    if (is_randomized) {
      const int step = max_ii(mmd->stroke_step, 1);
      const int start_offset = mmd->stroke_start_offset;
      int offset_index;
      int offset_size;
      float offset_factor;
      switch (mmd->mode) {
        case GP_OFFSET_STROKE:
          offset_size = max_ii(BLI_listbase_count(&gpf->strokes), 1);
          offset_index = max_ii(BLI_findindex(&gpf->strokes, gps), 0);
          break;
        case GP_OFFSET_MATERIAL:
          offset_size = max_ii(gpd->totcol, 1);
          offset_index = max_ii(gps->mat_nr, 0);
          break;
        case GP_OFFSET_LAYER:
          offset_size = max_ii(BLI_listbase_count(&gpd->layers), 1);
          offset_index = max_ii(BLI_findindex(&gpd->layers, gpl), 0);
          break;
      }

      offset_factor = ((offset_size - (offset_index / step + start_offset % offset_size) %
                                          offset_size * step % offset_size) -
                       1) /
                      float(offset_size);
      for (int j = 0; j < 3; j++) {
        for (int i = 0; i < 3; i++) {
          rand[j][i] = offset_factor;
        }
      }
    }
  }
  for (int i = 0; i < gps->totpoints; i++) {
    bGPDspoint *pt = &gps->points[i];
    MDeformVert *dvert = gps->dvert != nullptr ? &gps->dvert[i] : nullptr;

    /* Verify vertex group. */
    const float weight = get_modifier_point_weight(
        dvert, (mmd->flag & GP_OFFSET_INVERT_VGROUP) != 0, def_nr);
    if (weight < 0.0f) {
      continue;
    }

    /* Calculate Random matrix. */
    if (is_randomized) {
      float mat_rnd[4][4];
      float rnd_loc[3], rnd_rot[3], rnd_scale_weight[3];
      float rnd_scale[3] = {1.0f, 1.0f, 1.0f};

      mul_v3_v3fl(rnd_loc, rand[0], weight);
      mul_v3_v3fl(rnd_rot, rand[1], weight);
      mul_v3_v3fl(rnd_scale_weight, rand[2], weight);

      mul_v3_v3v3(rnd_loc, mmd->rnd_offset, rnd_loc);
      mul_v3_v3v3(rnd_rot, mmd->rnd_rot, rnd_rot);
      madd_v3_v3v3(rnd_scale, mmd->rnd_scale, rnd_scale_weight);

      loc_eul_size_to_mat4(mat_rnd, rnd_loc, rnd_rot, rnd_scale);
      /* Apply randomness matrix. */
      mul_m4_v3(mat_rnd, &pt->x);
    }

    /* Calculate matrix. */
    if (is_general) {
      mul_v3_v3fl(loc, mmd->loc, weight);
      mul_v3_v3fl(rot, mmd->rot, weight);
      mul_v3_v3fl(scale, mmd->scale, weight);
      add_v3_fl(scale, 1.0f);
      loc_eul_size_to_mat4(mat, loc, rot, scale);

      /* Apply scale to thickness. */
      float unit_scale = (fabsf(scale[0]) + fabsf(scale[1]) + fabsf(scale[2])) / 3.0f;
      pt->pressure *= unit_scale;

      mul_m4_v3(mat, &pt->x);
    }
  }
  /* Calc geometry data. */
  BKE_gpencil_stroke_geometry_update(gpd, gps);
}

static void bake_modifier(Main * /*bmain*/,
                          Depsgraph *depsgraph,
                          GpencilModifierData *md,
                          Object *ob)
{
  generic_bake_deform_stroke(depsgraph, md, ob, false, deform_stroke);
}

static void update_depsgraph(GpencilModifierData * /*md*/,
                             const ModifierUpdateDepsgraphContext *ctx,
                             const int /*mode*/)
{
  DEG_add_object_relation(ctx->node, ctx->object, DEG_OB_COMP_TRANSFORM, "Offset Modifier");
}

static void foreach_ID_link(GpencilModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  OffsetGpencilModifierData *mmd = (OffsetGpencilModifierData *)md;

  walk(user_data, ob, (ID **)&mmd->material, IDWALK_CB_USER);
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, nullptr);
  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "location", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "rotation", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "scale", UI_ITEM_NONE, nullptr, ICON_NONE);

  gpencil_modifier_panel_end(layout, ptr);
  uiLayoutSetActive(layout, true);
}

static void empty_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, nullptr);
  uiLayoutSetPropSep(layout, true);

  gpencil_modifier_panel_end(layout, ptr);
}

static void random_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;
  uiLayout *col;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, nullptr);
  int mode = RNA_enum_get(ptr, "mode");
  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "mode", UI_ITEM_NONE, nullptr, ICON_NONE);

  uiItemR(layout, ptr, "random_offset", UI_ITEM_NONE, IFACE_("Offset"), ICON_NONE);
  uiItemR(layout, ptr, "random_rotation", UI_ITEM_NONE, IFACE_("Rotation"), ICON_NONE);
  uiItemR(layout, ptr, "random_scale", UI_ITEM_NONE, IFACE_("Scale"), ICON_NONE);

  col = uiLayoutColumn(layout, true);
  switch (mode) {
    case GP_OFFSET_RANDOM:
      uiItemR(layout, ptr, "use_uniform_random_scale", UI_ITEM_NONE, nullptr, ICON_NONE);
      uiItemR(layout, ptr, "seed", UI_ITEM_NONE, nullptr, ICON_NONE);
      break;
    case GP_OFFSET_STROKE:
      uiItemR(col, ptr, "stroke_step", UI_ITEM_NONE, IFACE_("Stroke Step"), ICON_NONE);
      uiItemR(col, ptr, "stroke_start_offset", UI_ITEM_NONE, IFACE_("Offset"), ICON_NONE);
      break;
    case GP_OFFSET_MATERIAL:
      uiItemR(col, ptr, "stroke_step", UI_ITEM_NONE, IFACE_("Material Step"), ICON_NONE);
      uiItemR(col, ptr, "stroke_start_offset", UI_ITEM_NONE, IFACE_("Offset"), ICON_NONE);
      break;
    case GP_OFFSET_LAYER:
      uiItemR(col, ptr, "stroke_step", UI_ITEM_NONE, IFACE_("Layer Step"), ICON_NONE);
      uiItemR(col, ptr, "stroke_start_offset", UI_ITEM_NONE, IFACE_("Offset"), ICON_NONE);
      break;
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
      region_type, eGpencilModifierType_Offset, empty_panel_draw);
  gpencil_modifier_subpanel_register(
      region_type, "general", "General", nullptr, panel_draw, panel_type);
  gpencil_modifier_subpanel_register(
      region_type, "randomize", "Advanced", nullptr, random_panel_draw, panel_type);
  gpencil_modifier_subpanel_register(
      region_type, "mask", "Influence", nullptr, mask_panel_draw, panel_type);
}

GpencilModifierTypeInfo modifierType_Gpencil_Offset = {
    /*name*/ N_("Offset"),
    /*struct_name*/ "OffsetGpencilModifierData",
    /*struct_size*/ sizeof(OffsetGpencilModifierData),
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
    /*update_depsgraph*/ update_depsgraph,
    /*depends_on_time*/ nullptr,
    /*foreach_ID_link*/ foreach_ID_link,
    /*foreach_tex_link*/ nullptr,
    /*panel_register*/ panel_register,
};
