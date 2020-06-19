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
 * The Original Code is Copyright (C) 2018, Blender Foundation
 * This is a new part of Blender
 */

/** \file
 * \ingroup modifiers
 */

#include <stdio.h>

#include "BLI_utildefines.h"

#include "BLI_listbase.h"
#include "BLI_math.h"

#include "BLT_translation.h"

#include "DNA_gpencil_modifier_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_gpencil.h"
#include "BKE_gpencil_modifier.h"
#include "BKE_layer.h"
#include "BKE_lib_query.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_scene.h"
#include "BKE_screen.h"

#include "MEM_guardedalloc.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "MOD_gpencil_modifiertypes.h"
#include "MOD_gpencil_ui_common.h"
#include "MOD_gpencil_util.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

static void initData(GpencilModifierData *md)
{
  MirrorGpencilModifierData *gpmd = (MirrorGpencilModifierData *)md;
  gpmd->pass_index = 0;
  gpmd->material = NULL;
  gpmd->object = NULL;
  gpmd->flag |= GP_MIRROR_AXIS_X;
}

static void copyData(const GpencilModifierData *md, GpencilModifierData *target)
{
  BKE_gpencil_modifier_copydata_generic(md, target);
}

/* Mirror is using current object as origin. */
static void update_mirror_local(bGPDstroke *gps, int axis)
{
  int i;
  bGPDspoint *pt;
  float factor[3] = {1.0f, 1.0f, 1.0f};
  factor[axis] = -1.0f;

  for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
    mul_v3_v3(&pt->x, factor);
  }
}

/* Mirror is using other object as origin. */
static void update_mirror_object(Object *ob,
                                 MirrorGpencilModifierData *mmd,
                                 bGPDstroke *gps,
                                 int axis)
{
  /* Calculate local matrix transformation. */
  float mat[3][3], inv_mat[3][3];
  BKE_object_to_mat3(ob, mat);
  invert_m3_m3(inv_mat, mat);

  int i;
  bGPDspoint *pt;
  float factor[3] = {1.0f, 1.0f, 1.0f};
  factor[axis] = -1.0f;

  float clear[3] = {0.0f, 0.0f, 0.0f};
  clear[axis] = 1.0f;

  float ob_origin[3];
  float pt_origin[3];
  float half_origin[3];
  float rot_mat[3][3];

  float eul[3];
  mat4_to_eul(eul, mmd->object->obmat);
  mul_v3_fl(eul, 2.0f);
  eul_to_mat3(rot_mat, eul);
  sub_v3_v3v3(ob_origin, ob->obmat[3], mmd->object->obmat[3]);

  /* Only works with current axis. */
  mul_v3_v3(ob_origin, clear);

  /* Invert the origin. */
  mul_v3_v3fl(pt_origin, ob_origin, -2.0f);
  mul_v3_v3fl(half_origin, pt_origin, 0.5f);

  for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
    /* Apply any local transformation. */
    mul_m3_v3(mat, &pt->x);

    /* Apply mirror effect. */
    mul_v3_v3(&pt->x, factor);
    /* Apply location. */
    add_v3_v3(&pt->x, pt_origin);
    /* Apply rotation (around new center). */
    sub_v3_v3(&pt->x, half_origin);
    mul_m3_v3(rot_mat, &pt->x);
    add_v3_v3(&pt->x, half_origin);

    /* Undo local transformation to avoid double transform in drawing. */
    mul_m3_v3(inv_mat, &pt->x);
  }
}

static void update_position(Object *ob, MirrorGpencilModifierData *mmd, bGPDstroke *gps, int axis)
{
  if (mmd->object == NULL) {
    update_mirror_local(gps, axis);
  }
  else {
    update_mirror_object(ob, mmd, gps, axis);
  }
}

static void generate_geometry(GpencilModifierData *md, Object *ob, bGPDlayer *gpl, bGPDframe *gpf)
{
  MirrorGpencilModifierData *mmd = (MirrorGpencilModifierData *)md;
  bGPDstroke *gps, *gps_new = NULL;
  int tot_strokes;
  int i;

  /* check each axis for mirroring */
  for (int xi = 0; xi < 3; xi++) {
    if (mmd->flag & (GP_MIRROR_AXIS_X << xi)) {

      /* count strokes to avoid infinite loop after adding new strokes to tail of listbase */
      tot_strokes = BLI_listbase_count(&gpf->strokes);

      for (i = 0, gps = gpf->strokes.first; i < tot_strokes; i++, gps = gps->next) {
        if (is_stroke_affected_by_modifier(ob,
                                           mmd->layername,
                                           mmd->material,
                                           mmd->pass_index,
                                           mmd->layer_pass,
                                           1,
                                           gpl,
                                           gps,
                                           mmd->flag & GP_MIRROR_INVERT_LAYER,
                                           mmd->flag & GP_MIRROR_INVERT_PASS,
                                           mmd->flag & GP_MIRROR_INVERT_LAYERPASS,
                                           mmd->flag & GP_MIRROR_INVERT_MATERIAL)) {
          gps_new = BKE_gpencil_stroke_duplicate(gps, true);
          update_position(ob, mmd, gps_new, xi);
          BLI_addtail(&gpf->strokes, gps_new);
        }
      }
    }
  }
}

/* Generic "generateStrokes" callback */
static void generateStrokes(GpencilModifierData *md, Depsgraph *depsgraph, Object *ob)
{
  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  bGPdata *gpd = (bGPdata *)ob->data;

  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    bGPDframe *gpf = BKE_gpencil_frame_retime_get(depsgraph, scene, ob, gpl);
    if (gpf == NULL) {
      continue;
    }
    generate_geometry(md, ob, gpl, gpf);
  }
}

static void bakeModifier(Main *bmain, Depsgraph *depsgraph, GpencilModifierData *md, Object *ob)
{
  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  bGPdata *gpd = ob->data;
  int oldframe = (int)DEG_get_ctime(depsgraph);

  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      /* apply mirror effects on this frame */
      CFRA = gpf->framenum;
      BKE_scene_graph_update_for_newframe(depsgraph, bmain);

      /* compute mirror effects on this frame */
      generate_geometry(md, ob, gpl, gpf);
    }
  }

  /* return frame state and DB to original state */
  CFRA = oldframe;
  BKE_scene_graph_update_for_newframe(depsgraph, bmain);
}

static bool isDisabled(GpencilModifierData *UNUSED(md), int UNUSED(userRenderParams))
{
  // MirrorGpencilModifierData *mmd = (MirrorGpencilModifierData *)md;

  return false;
}

static void updateDepsgraph(GpencilModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  MirrorGpencilModifierData *lmd = (MirrorGpencilModifierData *)md;
  if (lmd->object != NULL) {
    DEG_add_object_relation(ctx->node, lmd->object, DEG_OB_COMP_GEOMETRY, "Mirror Modifier");
    DEG_add_object_relation(ctx->node, lmd->object, DEG_OB_COMP_TRANSFORM, "Mirror Modifier");
  }
  DEG_add_object_relation(ctx->node, ctx->object, DEG_OB_COMP_TRANSFORM, "Mirror Modifier");
}

static void foreachObjectLink(GpencilModifierData *md,
                              Object *ob,
                              ObjectWalkFunc walk,
                              void *userData)
{
  MirrorGpencilModifierData *mmd = (MirrorGpencilModifierData *)md;

  walk(userData, ob, &mmd->object, IDWALK_CB_NOP);
}

static void foreachIDLink(GpencilModifierData *md, Object *ob, IDWalkFunc walk, void *userData)
{
  MirrorGpencilModifierData *mmd = (MirrorGpencilModifierData *)md;

  walk(userData, ob, (ID **)&mmd->material, IDWALK_CB_USER);

  foreachObjectLink(md, ob, (ObjectWalkFunc)walk, userData);
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *row;
  uiLayout *layout = panel->layout;
  int toggles_flag = UI_ITEM_R_TOGGLE | UI_ITEM_R_FORCE_BLANK_DECORATE;

  PointerRNA ptr;
  gpencil_modifier_panel_get_property_pointers(C, panel, NULL, &ptr);

  uiLayoutSetPropSep(layout, true);

  row = uiLayoutRowWithHeading(layout, true, IFACE_("Axis"));
  uiItemR(row, &ptr, "x_axis", toggles_flag, NULL, ICON_NONE);
  uiItemR(row, &ptr, "y_axis", toggles_flag, NULL, ICON_NONE);
  uiItemR(row, &ptr, "z_axis", toggles_flag, NULL, ICON_NONE);

  uiItemR(layout, &ptr, "object", 0, NULL, ICON_NONE);

  gpencil_modifier_panel_end(layout, &ptr);
}

static void mask_panel_draw(const bContext *C, Panel *panel)
{
  gpencil_modifier_masking_panel_draw(C, panel, true, false);
}

static void panelRegister(ARegionType *region_type)
{
  PanelType *panel_type = gpencil_modifier_panel_register(
      region_type, eGpencilModifierType_Mirror, panel_draw);
  gpencil_modifier_subpanel_register(
      region_type, "mask", "Influence", NULL, mask_panel_draw, panel_type);
}

GpencilModifierTypeInfo modifierType_Gpencil_Mirror = {
    /* name */ "Mirror",
    /* structName */ "MirrorGpencilModifierData",
    /* structSize */ sizeof(MirrorGpencilModifierData),
    /* type */ eGpencilModifierTypeType_Gpencil,
    /* flags */ eGpencilModifierTypeFlag_SupportsEditmode,

    /* copyData */ copyData,

    /* deformStroke */ NULL,
    /* generateStrokes */ generateStrokes,
    /* bakeModifier */ bakeModifier,
    /* remapTime */ NULL,

    /* initData */ initData,
    /* freeData */ NULL,
    /* isDisabled */ isDisabled,
    /* updateDepsgraph */ updateDepsgraph,
    /* dependsOnTime */ NULL,
    /* foreachObjectLink */ foreachObjectLink,
    /* foreachIDLink */ foreachIDLink,
    /* foreachTexLink */ NULL,
    /* panelRegister */ panelRegister,
};
