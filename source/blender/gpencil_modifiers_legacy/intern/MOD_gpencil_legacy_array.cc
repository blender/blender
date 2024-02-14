/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include <cstdio>

#include "MEM_guardedalloc.h"

#include "BLI_ghash.h"
#include "BLI_hash.h"
#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_rand.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "DNA_defaults.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_legacy.h"
#include "BKE_gpencil_modifier_legacy.h"
#include "BKE_lib_query.hh"
#include "BKE_modifier.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"
#include "DEG_depsgraph_query.hh"

#include "MOD_gpencil_legacy_ui_common.h"
#include "MOD_gpencil_legacy_util.h"

struct tmpStrokes {
  tmpStrokes *next, *prev;
  bGPDframe *gpf;
  bGPDstroke *gps;
};

static void init_data(GpencilModifierData *md)
{
  ArrayGpencilModifierData *gpmd = (ArrayGpencilModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(gpmd, modifier));

  MEMCPY_STRUCT_AFTER(gpmd, DNA_struct_default_get(ArrayGpencilModifierData), modifier);

  /* Open the first subpanel too, because it's activated by default. */
  md->ui_expand_flag = UI_PANEL_DATA_EXPAND_ROOT | UI_SUBPANEL_DATA_EXPAND_1;
}

static void copy_data(const GpencilModifierData *md, GpencilModifierData *target)
{
  BKE_gpencil_modifier_copydata_generic(md, target);
}

/* -------------------------------- */
/* helper function for per-instance positioning */
static void BKE_gpencil_instance_modifier_instance_tfm(Object *ob,
                                                       ArrayGpencilModifierData *mmd,
                                                       const int elem_idx,
                                                       float r_mat[4][4],
                                                       float r_offset[4][4])
{
  float offset[3], rot[3], scale[3];
  ARRAY_SET_ITEMS(scale, 1.0f, 1.0f, 1.0f);
  zero_v3(rot);

  if (mmd->flag & GP_ARRAY_USE_OFFSET) {
    offset[0] = mmd->offset[0] * elem_idx;
    offset[1] = mmd->offset[1] * elem_idx;
    offset[2] = mmd->offset[2] * elem_idx;
  }
  else {
    zero_v3(offset);
  }

  /* Calculate matrix */
  loc_eul_size_to_mat4(r_mat, offset, rot, scale);
  copy_m4_m4(r_offset, r_mat);

  /* offset object */
  if ((mmd->flag & GP_ARRAY_USE_OB_OFFSET) && (mmd->object)) {
    float mat_offset[4][4];
    float obinv[4][4];

    unit_m4(mat_offset);
    if (mmd->flag & GP_ARRAY_USE_OFFSET) {
      add_v3_v3(mat_offset[3], mmd->offset);
    }
    invert_m4_m4(obinv, ob->object_to_world().ptr());

    mul_m4_series(r_offset, mat_offset, obinv, mmd->object->object_to_world().ptr());
    copy_m4_m4(mat_offset, r_offset);

    /* clear r_mat locations to avoid double transform */
    zero_v3(r_mat[3]);
  }
}
static bool gpencil_data_selected_minmax(
    ArrayGpencilModifierData *mmd, Object *ob, float r_min[3], float r_max[3], const int cfra)
{
  bGPdata *gpd = (bGPdata *)ob->data;
  bool changed = false;

  INIT_MINMAX(r_min, r_max);

  if (gpd == nullptr) {
    return changed;
  }

  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    bGPDframe *gpf = BKE_gpencil_layer_frame_get(gpl, cfra, GP_GETFRAME_USE_PREV);

    if (gpf != nullptr) {
      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        if (is_stroke_affected_by_modifier(ob,
                                           mmd->layername,
                                           mmd->material,
                                           mmd->pass_index,
                                           mmd->layer_pass,
                                           1,
                                           gpl,
                                           gps,
                                           mmd->flag & GP_ARRAY_INVERT_LAYER,
                                           mmd->flag & GP_ARRAY_INVERT_PASS,
                                           mmd->flag & GP_ARRAY_INVERT_LAYERPASS,
                                           mmd->flag & GP_ARRAY_INVERT_MATERIAL))
        {
          changed |= BKE_gpencil_stroke_minmax(gps, false, r_min, r_max);
        }
      }
    }
  }

  return changed;
}
/* array modifier - generate geometry callback (for viewport/rendering) */
static void generate_geometry(GpencilModifierData *md,
                              Depsgraph *depsgraph,
                              Scene *scene,
                              Object *ob,
                              const bool apply,
                              int cfra)
{
  ArrayGpencilModifierData *mmd = (ArrayGpencilModifierData *)md;
  ListBase stroke_cache = {nullptr, nullptr};
  /* Load the strokes to be duplicated. */
  bGPdata *gpd = (bGPdata *)ob->data;
  bool found = false;

  const int active_cfra = (apply) ? cfra : scene->r.cfra;

  /* Get bound-box for relative offset. */
  float size[3] = {0.0f, 0.0f, 0.0f};
  if (mmd->flag & GP_ARRAY_USE_RELATIVE) {
    float min[3];
    float max[3];
    if (gpencil_data_selected_minmax(mmd, ob, min, max, active_cfra)) {
      sub_v3_v3v3(size, max, min);
      /* Need a minimum size (for flat drawings). */
      CLAMP3_MIN(size, 0.01f);
    }
  }

  int seed = mmd->seed;
  /* Make sure different modifiers get different seeds. */
  seed += BLI_hash_string(ob->id.name + 2);
  seed += BLI_hash_string(md->name);

  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    bGPDframe *gpf = (apply) ? BKE_gpencil_layer_frame_get(gpl, cfra, GP_GETFRAME_USE_PREV) :
                               BKE_gpencil_frame_retime_get(depsgraph, scene, ob, gpl);
    if (gpf == nullptr) {
      continue;
    }
    LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
      if (is_stroke_affected_by_modifier(ob,
                                         mmd->layername,
                                         mmd->material,
                                         mmd->pass_index,
                                         mmd->layer_pass,
                                         1,
                                         gpl,
                                         gps,
                                         mmd->flag & GP_ARRAY_INVERT_LAYER,
                                         mmd->flag & GP_ARRAY_INVERT_PASS,
                                         mmd->flag & GP_ARRAY_INVERT_LAYERPASS,
                                         mmd->flag & GP_ARRAY_INVERT_MATERIAL))
      {
        tmpStrokes *tmp = static_cast<tmpStrokes *>(MEM_callocN(sizeof(tmpStrokes), __func__));
        tmp->gpf = gpf;
        tmp->gps = gps;
        BLI_addtail(&stroke_cache, tmp);

        found = true;
      }
    }
  }

  if (found) {
    /* Generate new instances of all existing strokes,
     * keeping each instance together so they maintain
     * the correct ordering relative to each other
     */
    float current_offset[4][4];
    unit_m4(current_offset);

    float rand_offset = BLI_hash_int_01(seed);

    for (int x = 0; x < mmd->count; x++) {
      /* original strokes are at index = 0 */
      if (x == 0) {
        continue;
      }

      /* Compute transforms for this instance */
      float mat[4][4];
      float mat_offset[4][4];
      BKE_gpencil_instance_modifier_instance_tfm(ob, mmd, x, mat, mat_offset);

      if ((mmd->flag & GP_ARRAY_USE_OB_OFFSET) && (mmd->object)) {
        /* recalculate cumulative offset here */
        mul_m4_m4m4(current_offset, current_offset, mat_offset);
      }
      else {
        copy_m4_m4(current_offset, mat);
      }

      /* Apply relative offset. */
      if (mmd->flag & GP_ARRAY_USE_RELATIVE) {
        float relative[3];
        mul_v3_v3v3(relative, mmd->shift, size);
        madd_v3_v3fl(current_offset[3], relative, x);
      }

      float rand[3][3];
      for (int j = 0; j < 3; j++) {
        const uint primes[3] = {2, 3, 7};
        double offset[3] = {0.0, 0.0, 0.0};
        double r[3];
        /* To ensure a nice distribution, we use halton sequence and offset using the seed. */
        BLI_halton_3d(primes, offset, x, r);

        if ((mmd->flag & GP_ARRAY_UNIFORM_RANDOM_SCALE) && j == 2) {
          float rand_value;
          rand_value = fmodf(r[0] * 2.0 - 1.0 + rand_offset, 1.0f);
          rand_value = fmodf(sin(rand_value * 12.9898 + j * 78.233) * 43758.5453, 1.0f);
          copy_v3_fl(rand[j], rand_value);
        }
        else {
          for (int i = 0; i < 3; i++) {
            rand[j][i] = fmodf(r[i] * 2.0 - 1.0 + rand_offset, 1.0f);
            rand[j][i] = fmodf(sin(rand[j][i] * 12.9898 + j * 78.233) * 43758.5453, 1.0f);
          }
        }
      }
      /* Calculate Random matrix. */
      float mat_rnd[4][4];
      float loc[3], rot[3];
      float scale[3] = {1.0f, 1.0f, 1.0f};
      mul_v3_v3v3(loc, mmd->rnd_offset, rand[0]);
      mul_v3_v3v3(rot, mmd->rnd_rot, rand[1]);
      madd_v3_v3v3(scale, mmd->rnd_scale, rand[2]);

      loc_eul_size_to_mat4(mat_rnd, loc, rot, scale);

      /* Duplicate original strokes to create this instance. */
      LISTBASE_FOREACH_BACKWARD (tmpStrokes *, iter, &stroke_cache) {
        /* Duplicate stroke */
        bGPDstroke *gps_dst = BKE_gpencil_stroke_duplicate(iter->gps, true, true);

        /* Move points */
        for (int i = 0; i < iter->gps->totpoints; i++) {
          bGPDspoint *pt = &gps_dst->points[i];
          /* Apply randomness matrix. */
          mul_m4_v3(mat_rnd, &pt->x);

          /* Apply object local transform (Rot/Scale). */
          if ((mmd->flag & GP_ARRAY_USE_OB_OFFSET) && (mmd->object)) {
            mul_m4_v3(mat, &pt->x);
          }
          /* Global Rotate and scale. */
          mul_mat3_m4_v3(current_offset, &pt->x);
          /* Global translate. */
          add_v3_v3(&pt->x, current_offset[3]);
        }

        /* If replace material, use new one. */
        if ((mmd->mat_rpl > 0) && (mmd->mat_rpl <= ob->totcol)) {
          gps_dst->mat_nr = mmd->mat_rpl - 1;
        }

        /* Add new stroke. */
        BLI_addhead(&iter->gpf->strokes, gps_dst);
        /* Calc bounding box. */
        BKE_gpencil_stroke_boundingbox_calc(gps_dst);
      }
    }

    /* Free temp data. */
    LISTBASE_FOREACH_MUTABLE (tmpStrokes *, tmp, &stroke_cache) {
      BLI_freelinkN(&stroke_cache, tmp);
    }
  }
}

static void bake_modifier(Main * /*bmain*/,
                          Depsgraph *depsgraph,
                          GpencilModifierData *md,
                          Object *ob)
{
  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  bGPdata *gpd = static_cast<bGPdata *>(ob->data);

  /* Get list of frames. */
  GHash *keyframe_list = BLI_ghash_int_new(__func__);
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      if (!BLI_ghash_haskey(keyframe_list, POINTER_FROM_INT(gpf->framenum))) {
        BLI_ghash_insert(
            keyframe_list, POINTER_FROM_INT(gpf->framenum), POINTER_FROM_INT(gpf->framenum));
      }
    }
  }

  /* Loop all frames and apply. */
  GHashIterator gh_iter;
  GHASH_ITER (gh_iter, keyframe_list) {
    int cfra = POINTER_AS_INT(BLI_ghashIterator_getKey(&gh_iter));
    generate_geometry(md, depsgraph, scene, ob, true, cfra);
  }

  /* Free temp hash table. */
  if (keyframe_list != nullptr) {
    BLI_ghash_free(keyframe_list, nullptr, nullptr);
  }
}

/* -------------------------------- */

/* Generic "generate_strokes" callback */
static void generate_strokes(GpencilModifierData *md, Depsgraph *depsgraph, Object *ob)
{
  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  generate_geometry(md, depsgraph, scene, ob, false, 0);
}

static void update_depsgraph(GpencilModifierData *md,
                             const ModifierUpdateDepsgraphContext *ctx,
                             const int /*mode*/)
{
  ArrayGpencilModifierData *lmd = (ArrayGpencilModifierData *)md;
  if (lmd->object != nullptr) {
    DEG_add_object_relation(ctx->node, lmd->object, DEG_OB_COMP_GEOMETRY, "Array Modifier");
    DEG_add_object_relation(ctx->node, lmd->object, DEG_OB_COMP_TRANSFORM, "Array Modifier");
  }
  DEG_add_object_relation(ctx->node, ctx->object, DEG_OB_COMP_TRANSFORM, "Array Modifier");
}

static void foreach_ID_link(GpencilModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  ArrayGpencilModifierData *mmd = (ArrayGpencilModifierData *)md;

  walk(user_data, ob, (ID **)&mmd->material, IDWALK_CB_USER);
  walk(user_data, ob, (ID **)&mmd->object, IDWALK_CB_NOP);
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, nullptr);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "count", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "replace_material", UI_ITEM_NONE, IFACE_("Material Override"), ICON_NONE);

  gpencil_modifier_panel_end(layout, ptr);
}

static void relative_offset_header_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, nullptr);

  uiItemR(layout, ptr, "use_relative_offset", UI_ITEM_NONE, IFACE_("Relative Offset"), ICON_NONE);
}

static void relative_offset_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, nullptr);

  uiLayoutSetPropSep(layout, true);

  uiLayout *col = uiLayoutColumn(layout, false);

  uiLayoutSetActive(col, RNA_boolean_get(ptr, "use_relative_offset"));
  uiItemR(col, ptr, "relative_offset", UI_ITEM_NONE, IFACE_("Factor"), ICON_NONE);
}

static void constant_offset_header_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, nullptr);

  uiItemR(layout, ptr, "use_constant_offset", UI_ITEM_NONE, IFACE_("Constant Offset"), ICON_NONE);
}

static void constant_offset_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, nullptr);

  uiLayoutSetPropSep(layout, true);

  uiLayout *col = uiLayoutColumn(layout, false);

  uiLayoutSetActive(col, RNA_boolean_get(ptr, "use_constant_offset"));
  uiItemR(col, ptr, "constant_offset", UI_ITEM_NONE, IFACE_("Distance"), ICON_NONE);
}

/**
 * Object offset in a subpanel for consistency with the other offset types.
 */
static void object_offset_header_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, nullptr);

  uiItemR(layout, ptr, "use_object_offset", UI_ITEM_NONE, IFACE_("Object Offset"), ICON_NONE);
}

static void object_offset_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, nullptr);

  uiLayoutSetPropSep(layout, true);

  uiLayout *col = uiLayoutColumn(layout, false);

  uiLayoutSetActive(col, RNA_boolean_get(ptr, "use_object_offset"));
  uiItemR(col, ptr, "offset_object", UI_ITEM_NONE, IFACE_("Object"), ICON_NONE);
}

static void random_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, nullptr);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "random_offset", UI_ITEM_NONE, IFACE_("Offset"), ICON_NONE);
  uiItemR(layout, ptr, "random_rotation", UI_ITEM_NONE, IFACE_("Rotation"), ICON_NONE);
  uiItemR(layout, ptr, "random_scale", UI_ITEM_NONE, IFACE_("Scale"), ICON_NONE);
  uiItemR(layout, ptr, "use_uniform_random_scale", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "seed", UI_ITEM_NONE, nullptr, ICON_NONE);
}

static void mask_panel_draw(const bContext * /*C*/, Panel *panel)
{
  gpencil_modifier_masking_panel_draw(panel, true, false);
}

static void panel_register(ARegionType *region_type)
{
  PanelType *panel_type = gpencil_modifier_panel_register(
      region_type, eGpencilModifierType_Array, panel_draw);
  gpencil_modifier_subpanel_register(region_type,
                                     "relative_offset",
                                     "",
                                     relative_offset_header_draw,
                                     relative_offset_draw,
                                     panel_type);
  gpencil_modifier_subpanel_register(region_type,
                                     "constant_offset",
                                     "",
                                     constant_offset_header_draw,
                                     constant_offset_draw,
                                     panel_type);
  gpencil_modifier_subpanel_register(
      region_type, "object_offset", "", object_offset_header_draw, object_offset_draw, panel_type);
  gpencil_modifier_subpanel_register(
      region_type, "randomize", "Randomize", nullptr, random_panel_draw, panel_type);
  gpencil_modifier_subpanel_register(
      region_type, "mask", "Influence", nullptr, mask_panel_draw, panel_type);
}

GpencilModifierTypeInfo modifierType_Gpencil_Array = {
    /*name*/ N_("Array"),
    /*struct_name*/ "ArrayGpencilModifierData",
    /*struct_size*/ sizeof(ArrayGpencilModifierData),
    /*type*/ eGpencilModifierTypeType_Gpencil,
    /*flags*/ eGpencilModifierTypeFlag_SupportsEditmode,

    /*copy_data*/ copy_data,

    /*deform_stroke*/ nullptr,
    /*generate_strokes*/ generate_strokes,
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
