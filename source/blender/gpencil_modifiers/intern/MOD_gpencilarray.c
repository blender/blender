/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2017, Blender Foundation
 * This is a new part of Blender
 */

/** \file
 * \ingroup modifiers
 */

#include <stdio.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "BLI_ghash.h"
#include "BLI_hash.h"
#include "BLI_rand.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_rand.h"

#include "BLT_translation.h"

#include "DNA_gpencil_modifier_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_collection.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_gpencil.h"
#include "BKE_gpencil_geom.h"
#include "BKE_gpencil_modifier.h"
#include "BKE_layer.h"
#include "BKE_lib_query.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_scene.h"
#include "BKE_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

#include "MOD_gpencil_modifiertypes.h"
#include "MOD_gpencil_ui_common.h"
#include "MOD_gpencil_util.h"

typedef struct tmpStrokes {
  struct tmpStrokes *next, *prev;
  bGPDframe *gpf;
  bGPDstroke *gps;
} tmpStrokes;

static void initData(GpencilModifierData *md)
{
  ArrayGpencilModifierData *gpmd = (ArrayGpencilModifierData *)md;
  gpmd->count = 2;
  gpmd->shift[0] = 1.0f;
  gpmd->shift[1] = 0.0f;
  gpmd->shift[2] = 0.0f;
  zero_v3(gpmd->offset);
  zero_v3(gpmd->rnd_scale);
  gpmd->object = NULL;
  gpmd->flag |= GP_ARRAY_USE_RELATIVE;
  gpmd->seed = 1;
  gpmd->material = NULL;

  /* Open the first subpanel too, because it's activated by default. */
  md->ui_expand_flag = (1 << 0) | (1 << 1);
}

static void copyData(const GpencilModifierData *md, GpencilModifierData *target)
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
    add_v3_v3(mat_offset[3], mmd->offset);
    invert_m4_m4(obinv, ob->obmat);

    mul_m4_series(r_offset, mat_offset, obinv, mmd->object->obmat);
    copy_m4_m4(mat_offset, r_offset);

    /* clear r_mat locations to avoid double transform */
    zero_v3(r_mat[3]);
  }
}

/* array modifier - generate geometry callback (for viewport/rendering) */
static void generate_geometry(GpencilModifierData *md,
                              Depsgraph *depsgraph,
                              Scene *scene,
                              Object *ob)
{
  ArrayGpencilModifierData *mmd = (ArrayGpencilModifierData *)md;
  ListBase stroke_cache = {NULL, NULL};
  /* Load the strokes to be duplicated. */
  bGPdata *gpd = (bGPdata *)ob->data;
  bool found = false;

  /* Get bounbox for relative offset. */
  float size[3] = {0.0f, 0.0f, 0.0f};
  if (mmd->flag & GP_ARRAY_USE_RELATIVE) {
    BoundBox *bb = BKE_object_boundbox_get(ob);
    const float min[3] = {-1.0f, -1.0f, -1.0f}, max[3] = {1.0f, 1.0f, 1.0f};
    BKE_boundbox_init_from_minmax(bb, min, max);
    BKE_boundbox_calc_size_aabb(bb, size);
    mul_v3_fl(size, 2.0f);
    /* Need a minimum size (for flat drawings). */
    CLAMP3_MIN(size, 0.01f);
  }

  int seed = mmd->seed;
  /* Make sure different modifiers get different seeds. */
  seed += BLI_hash_string(ob->id.name + 2);
  seed += BLI_hash_string(md->name);

  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    bGPDframe *gpf = BKE_gpencil_frame_retime_get(depsgraph, scene, ob, gpl);
    if (gpf == NULL) {
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
                                         mmd->flag & GP_ARRAY_INVERT_MATERIAL)) {
        tmpStrokes *tmp = MEM_callocN(sizeof(tmpStrokes), __func__);
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
        uint primes[3] = {2, 3, 7};
        double offset[3] = {0.0, 0.0, 0.0};
        double r[3];
        /* To ensure a nice distribution, we use halton sequence and offset using the seed. */
        BLI_halton_3d(primes, offset, x, r);

        for (int i = 0; i < 3; i++) {
          rand[j][i] = fmodf(r[i] * 2.0 - 1.0 + rand_offset, 1.0f);
          rand[j][i] = fmodf(sin(rand[j][i] * 12.9898 + j * 78.233) * 43758.5453, 1.0f);
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
        bGPDstroke *gps_dst = BKE_gpencil_stroke_duplicate(iter->gps, true);

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

static void bakeModifier(Main *UNUSED(bmain),
                         Depsgraph *depsgraph,
                         GpencilModifierData *md,
                         Object *ob)
{
  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  generate_geometry(md, depsgraph, scene, ob);
}

/* -------------------------------- */

/* Generic "generateStrokes" callback */
static void generateStrokes(GpencilModifierData *md, Depsgraph *depsgraph, Object *ob)
{
  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  generate_geometry(md, depsgraph, scene, ob);
}

static void updateDepsgraph(GpencilModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  ArrayGpencilModifierData *lmd = (ArrayGpencilModifierData *)md;
  if (lmd->object != NULL) {
    DEG_add_object_relation(ctx->node, lmd->object, DEG_OB_COMP_GEOMETRY, "Array Modifier");
    DEG_add_object_relation(ctx->node, lmd->object, DEG_OB_COMP_TRANSFORM, "Array Modifier");
  }
  DEG_add_object_relation(ctx->node, ctx->object, DEG_OB_COMP_TRANSFORM, "Array Modifier");
}

static void foreachObjectLink(GpencilModifierData *md,
                              Object *ob,
                              ObjectWalkFunc walk,
                              void *userData)
{
  ArrayGpencilModifierData *mmd = (ArrayGpencilModifierData *)md;

  walk(userData, ob, &mmd->object, IDWALK_CB_NOP);
}

static void foreachIDLink(GpencilModifierData *md, Object *ob, IDWalkFunc walk, void *userData)
{
  ArrayGpencilModifierData *mmd = (ArrayGpencilModifierData *)md;

  walk(userData, ob, (ID **)&mmd->material, IDWALK_CB_USER);

  foreachObjectLink(md, ob, (ObjectWalkFunc)walk, userData);
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  gpencil_modifier_panel_get_property_pointers(C, panel, NULL, &ptr);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, &ptr, "count", 0, NULL, ICON_NONE);
  uiItemR(layout, &ptr, "replace_material", 0, IFACE_("Material Override"), ICON_NONE);

  gpencil_modifier_panel_end(layout, &ptr);
}

static void relative_offset_header_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  gpencil_modifier_panel_get_property_pointers(C, panel, NULL, &ptr);

  uiItemR(layout, &ptr, "use_relative_offset", 0, IFACE_("Relative Offset"), ICON_NONE);
}

static void relative_offset_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  gpencil_modifier_panel_get_property_pointers(C, panel, NULL, &ptr);

  uiLayoutSetPropSep(layout, true);

  uiLayout *col = uiLayoutColumn(layout, false);

  uiLayoutSetActive(col, RNA_boolean_get(&ptr, "use_relative_offset"));
  uiItemR(col, &ptr, "relative_offset", 0, IFACE_("Factor"), ICON_NONE);
}

static void constant_offset_header_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  gpencil_modifier_panel_get_property_pointers(C, panel, NULL, &ptr);

  uiItemR(layout, &ptr, "use_constant_offset", 0, IFACE_("Constant Offset"), ICON_NONE);
}

static void constant_offset_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  gpencil_modifier_panel_get_property_pointers(C, panel, NULL, &ptr);

  uiLayoutSetPropSep(layout, true);

  uiLayout *col = uiLayoutColumn(layout, false);

  uiLayoutSetActive(col, RNA_boolean_get(&ptr, "use_constant_offset"));
  uiItemR(col, &ptr, "constant_offset", 0, IFACE_("Distance"), ICON_NONE);
}

/**
 * Object offset in a subpanel for consistency with the other offset types.
 */
static void object_offset_header_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  gpencil_modifier_panel_get_property_pointers(C, panel, NULL, &ptr);

  uiItemR(layout, &ptr, "use_object_offset", 0, NULL, ICON_NONE);
}

static void object_offset_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  gpencil_modifier_panel_get_property_pointers(C, panel, NULL, &ptr);

  uiLayoutSetPropSep(layout, true);

  uiLayout *col = uiLayoutColumn(layout, false);

  uiLayoutSetActive(col, RNA_boolean_get(&ptr, "use_object_offset"));
  uiItemR(col, &ptr, "offset_object", 0, NULL, ICON_NONE);
}

static void random_panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  gpencil_modifier_panel_get_property_pointers(C, panel, NULL, &ptr);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, &ptr, "random_offset", 0, IFACE_("Offset"), ICON_NONE);
  uiItemR(layout, &ptr, "random_rotation", 0, IFACE_("Rotation"), ICON_NONE);
  uiItemR(layout, &ptr, "random_scale", 0, IFACE_("Scale"), ICON_NONE);
  uiItemR(layout, &ptr, "seed", 0, NULL, ICON_NONE);
}

static void mask_panel_draw(const bContext *C, Panel *panel)
{
  gpencil_modifier_masking_panel_draw(C, panel, true, false);
}

static void panelRegister(ARegionType *region_type)
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
      region_type, "randomize", "Randomize", NULL, random_panel_draw, panel_type);
  gpencil_modifier_subpanel_register(
      region_type, "mask", "Influence", NULL, mask_panel_draw, panel_type);
}

GpencilModifierTypeInfo modifierType_Gpencil_Array = {
    /* name */ "Array",
    /* structName */ "ArrayGpencilModifierData",
    /* structSize */ sizeof(ArrayGpencilModifierData),
    /* type */ eGpencilModifierTypeType_Gpencil,
    /* flags */ eGpencilModifierTypeFlag_SupportsEditmode,

    /* copyData */ copyData,

    /* deformStroke */ NULL,
    /* generateStrokes */ generateStrokes,
    /* bakeModifier */ bakeModifier,
    /* remapTime */ NULL,

    /* initData */ initData,
    /* freeData */ NULL,
    /* isDisabled */ NULL,
    /* updateDepsgraph */ updateDepsgraph,
    /* dependsOnTime */ NULL,
    /* foreachObjectLink */ foreachObjectLink,
    /* foreachIDLink */ foreachIDLink,
    /* foreachTexLink */ NULL,
    /* panelRegister */ panelRegister,
};
