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
 * The Original Code is Copyright (C) 2021, Blender Foundation
 * This is a new part of Blender
 */

/** \file
 * \ingroup modifiers
 */

#include <stdio.h>
#include <string.h>

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_string.h"

#include "BLT_translation.h"

#include "DNA_defaults.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "BKE_gpencil.h"
#include "BKE_gpencil_geom.h"
#include "BKE_gpencil_modifier.h"
#include "BKE_lib_query.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_screen.h"

#include "MEM_guardedalloc.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "BLT_translation.h"

#include "MOD_gpencil_modifiertypes.h"
#include "MOD_gpencil_ui_common.h"
#include "MOD_gpencil_util.h"

#include "DEG_depsgraph.h"

#include "WM_api.h"

static void initData(GpencilModifierData *md)
{
  DashGpencilModifierData *dmd = (DashGpencilModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(dmd, modifier));

  MEMCPY_STRUCT_AFTER(dmd, DNA_struct_default_get(DashGpencilModifierData), modifier);

  DashGpencilModifierSegment *ds = DNA_struct_default_alloc(DashGpencilModifierSegment);
  ds->dmd = dmd;
  BLI_strncpy(ds->name, DATA_("Segment"), sizeof(ds->name));

  dmd->segments = ds;
}

static void copyData(const GpencilModifierData *md, GpencilModifierData *target)
{
  DashGpencilModifierData *dmd = (DashGpencilModifierData *)target;
  const DashGpencilModifierData *dmd_src = (const DashGpencilModifierData *)md;

  BKE_gpencil_modifier_copydata_generic(md, target);

  dmd->segments = MEM_dupallocN(dmd_src->segments);
}

static void freeData(GpencilModifierData *md)
{
  DashGpencilModifierData *dmd = (DashGpencilModifierData *)md;

  MEM_SAFE_FREE(dmd->segments);
}

/**
 * Gap==0 means to start the next segment at the immediate next point, which will leave a visual
 * gap of "1 point". This makes the algorithm give the same visual appearance as displayed on the
 * UI and also simplifies the check for "no-length" situation where SEG==0 (which will not produce
 * any effective dash).
 */
static int real_gap(const DashGpencilModifierSegment *ds)
{
  return ds->gap - 1;
}

static bool stroke_dash(const bGPDstroke *gps,
                        const DashGpencilModifierData *dmd,
                        ListBase *r_strokes)
{
  int new_stroke_offset = 0;
  int trim_start = 0;

  for (int i = 0; i < dmd->segments_len; i++) {
    if (dmd->segments[i].dash + real_gap(&dmd->segments[i]) < 1) {
      BLI_assert_unreachable();
      /* This means there's a part that doesn't have any length, can't do dot-dash. */
      return false;
    }
  }

  const DashGpencilModifierSegment *const first_segment = &dmd->segments[0];
  const DashGpencilModifierSegment *const last_segment = &dmd->segments[dmd->segments_len - 1];
  const DashGpencilModifierSegment *ds = first_segment;

  /* Determine starting configuration using offset. */
  int offset_trim = dmd->dash_offset;
  while (offset_trim < 0) {
    ds = (ds == first_segment) ? last_segment : ds - 1;
    offset_trim += ds->dash + real_gap(ds);
  }

  /* This segment is completely removed from view by the index offset, ignore it. */
  while (ds->dash + real_gap(ds) < offset_trim) {
    offset_trim -= ds->dash + real_gap(ds);
    ds = (ds == last_segment) ? first_segment : ds + 1;
  }

  /* This segment is partially visible at the beginning of the stroke. */
  if (ds->dash > offset_trim) {
    trim_start = offset_trim;
  }
  else {
    /* This segment is not visible but the gap immediately after this segment is partially visible,
     * use next segment's dash. */
    new_stroke_offset += ds->dash + real_gap(ds) - offset_trim;
    ds = (ds == last_segment) ? first_segment : ds + 1;
  }

  while (new_stroke_offset < gps->totpoints - 1) {
    const int seg = ds->dash - trim_start;
    if (!(seg || real_gap(ds))) {
      ds = (ds == last_segment) ? first_segment : ds + 1;
      continue;
    }

    const int size = MIN2(gps->totpoints - new_stroke_offset, seg);
    if (size == 0) {
      continue;
    }

    bGPDstroke *stroke = BKE_gpencil_stroke_new(
        ds->mat_nr < 0 ? gps->mat_nr : ds->mat_nr, size, gps->thickness);

    for (int is = 0; is < size; is++) {
      bGPDspoint *p = &gps->points[new_stroke_offset + is];
      stroke->points[is].x = p->x;
      stroke->points[is].y = p->y;
      stroke->points[is].z = p->z;
      stroke->points[is].pressure = p->pressure * ds->radius;
      stroke->points[is].strength = p->strength * ds->opacity;
      copy_v4_v4(stroke->points[is].vert_color, p->vert_color);
    }
    BLI_addtail(r_strokes, stroke);

    if (gps->dvert) {
      BKE_gpencil_dvert_ensure(stroke);
      for (int di = 0; di < stroke->totpoints; di++) {
        MDeformVert *dv = &gps->dvert[new_stroke_offset + di];
        if (dv && dv->totweight && dv->dw) {
          MDeformWeight *dw = (MDeformWeight *)MEM_callocN(sizeof(MDeformWeight) * dv->totweight,
                                                           __func__);
          memcpy(dw, dv->dw, sizeof(MDeformWeight) * dv->totweight);
          stroke->dvert[di].dw = dw;
          stroke->dvert[di].totweight = dv->totweight;
          stroke->dvert[di].flag = dv->flag;
        }
      }
    }

    new_stroke_offset += seg + real_gap(ds);
    ds = (ds == last_segment) ? first_segment : ds + 1;
    trim_start = 0;
  }

  return true;
}

static void apply_dash_for_frame(
    Object *ob, bGPDlayer *gpl, bGPdata *gpd, bGPDframe *gpf, DashGpencilModifierData *dmd)
{
  if (dmd->segments_len == 0) {
    return;
  }

  ListBase result = {NULL, NULL};

  LISTBASE_FOREACH_MUTABLE (bGPDstroke *, gps, &gpf->strokes) {
    if (is_stroke_affected_by_modifier(ob,
                                       dmd->layername,
                                       dmd->material,
                                       dmd->pass_index,
                                       dmd->layer_pass,
                                       1,
                                       gpl,
                                       gps,
                                       dmd->flag & GP_LENGTH_INVERT_LAYER,
                                       dmd->flag & GP_LENGTH_INVERT_PASS,
                                       dmd->flag & GP_LENGTH_INVERT_LAYERPASS,
                                       dmd->flag & GP_LENGTH_INVERT_MATERIAL)) {
      stroke_dash(gps, dmd, &result);
      BLI_remlink(&gpf->strokes, gps);
      BKE_gpencil_free_stroke(gps);
    }
  }
  bGPDstroke *gps_dash;
  while ((gps_dash = BLI_pophead(&result))) {
    BLI_addtail(&gpf->strokes, gps_dash);
    BKE_gpencil_stroke_geometry_update(gpd, gps_dash);
  }
}

static void bakeModifier(Main *UNUSED(bmain),
                         Depsgraph *UNUSED(depsgraph),
                         GpencilModifierData *md,
                         Object *ob)
{
  bGPdata *gpd = ob->data;

  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      apply_dash_for_frame(ob, gpl, gpd, gpf, (DashGpencilModifierData *)md);
    }
  }
}

/* -------------------------------- */

/* Generic "generateStrokes" callback */
static void generateStrokes(GpencilModifierData *md, Depsgraph *depsgraph, Object *ob)
{
  bGPdata *gpd = ob->data;

  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    BKE_gpencil_frame_active_set(depsgraph, gpd);
    bGPDframe *gpf = gpl->actframe;
    if (gpf == NULL) {
      continue;
    }
    apply_dash_for_frame(ob, gpl, gpd, gpf, (DashGpencilModifierData *)md);
  }
}

static void foreachIDLink(GpencilModifierData *md, Object *ob, IDWalkFunc walk, void *userData)
{
  DashGpencilModifierData *mmd = (DashGpencilModifierData *)md;

  walk(userData, ob, (ID **)&mmd->material, IDWALK_CB_USER);
}

static void segment_list_item(struct uiList *UNUSED(ui_list),
                              struct bContext *UNUSED(C),
                              struct uiLayout *layout,
                              struct PointerRNA *UNUSED(idataptr),
                              struct PointerRNA *itemptr,
                              int UNUSED(icon),
                              struct PointerRNA *UNUSED(active_dataptr),
                              const char *UNUSED(active_propname),
                              int UNUSED(index),
                              int UNUSED(flt_flag))
{
  uiLayout *row = uiLayoutRow(layout, true);
  uiItemR(row, itemptr, "name", UI_ITEM_R_NO_BG, "", ICON_NONE);
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, NULL);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "dash_offset", 0, NULL, ICON_NONE);

  uiLayout *row = uiLayoutRow(layout, false);
  uiLayoutSetPropSep(row, false);

  uiTemplateList(row,
                 (bContext *)C,
                 "MOD_UL_dash_segment",
                 "",
                 ptr,
                 "segments",
                 ptr,
                 "segment_active_index",
                 NULL,
                 3,
                 10,
                 0,
                 1,
                 UI_TEMPLATE_LIST_FLAG_NONE);

  uiLayout *col = uiLayoutColumn(row, false);
  uiLayoutSetContextPointer(col, "modifier", ptr);

  uiLayout *sub = uiLayoutColumn(col, true);
  uiItemO(sub, "", ICON_ADD, "GPENCIL_OT_segment_add");
  uiItemO(sub, "", ICON_REMOVE, "GPENCIL_OT_segment_remove");
  uiItemS(col);
  sub = uiLayoutColumn(col, true);
  uiItemEnumO_string(sub, "", ICON_TRIA_UP, "GPENCIL_OT_segment_move", "type", "UP");
  uiItemEnumO_string(sub, "", ICON_TRIA_DOWN, "GPENCIL_OT_segment_move", "type", "DOWN");

  DashGpencilModifierData *dmd = ptr->data;

  if (dmd->segment_active_index >= 0 && dmd->segment_active_index < dmd->segments_len) {
    PointerRNA ds_ptr;
    RNA_pointer_create(ptr->owner_id,
                       &RNA_DashGpencilModifierSegment,
                       &dmd->segments[dmd->segment_active_index],
                       &ds_ptr);

    sub = uiLayoutColumn(layout, true);
    uiItemR(sub, &ds_ptr, "dash", 0, NULL, ICON_NONE);
    uiItemR(sub, &ds_ptr, "gap", 0, NULL, ICON_NONE);

    sub = uiLayoutColumn(layout, false);
    uiItemR(sub, &ds_ptr, "radius", 0, NULL, ICON_NONE);
    uiItemR(sub, &ds_ptr, "opacity", 0, NULL, ICON_NONE);
    uiItemR(sub, &ds_ptr, "material_index", 0, NULL, ICON_NONE);
  }

  gpencil_modifier_panel_end(layout, ptr);
}

static void mask_panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  gpencil_modifier_masking_panel_draw(panel, true, false);
}

static void panelRegister(ARegionType *region_type)
{
  PanelType *panel_type = gpencil_modifier_panel_register(
      region_type, eGpencilModifierType_Dash, panel_draw);
  gpencil_modifier_subpanel_register(
      region_type, "mask", "Influence", NULL, mask_panel_draw, panel_type);

  uiListType *list_type = MEM_callocN(sizeof(uiListType), "dash modifier segment uilist");
  strcpy(list_type->idname, "MOD_UL_dash_segment");
  list_type->draw_item = segment_list_item;
  WM_uilisttype_add(list_type);
}

GpencilModifierTypeInfo modifierType_Gpencil_Dash = {
    /* name */ "Dot Dash",
    /* structName */ "DashGpencilModifierData",
    /* structSize */ sizeof(DashGpencilModifierData),
    /* type */ eGpencilModifierTypeType_Gpencil,
    /* flags */ eGpencilModifierTypeFlag_SupportsEditmode,

    /* copyData */ copyData,

    /* deformStroke */ NULL,
    /* generateStrokes */ generateStrokes,
    /* bakeModifier */ bakeModifier,
    /* remapTime */ NULL,

    /* initData */ initData,
    /* freeData */ freeData,
    /* isDisabled */ NULL,
    /* updateDepsgraph */ NULL,
    /* dependsOnTime */ NULL,
    /* foreachIDLink */ foreachIDLink,
    /* foreachTexLink */ NULL,
    /* panelRegister */ panelRegister,
};
