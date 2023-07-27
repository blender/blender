/* SPDX-FileCopyrightText: 2021 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include <cstdio>
#include <cstring>

#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"

#include "DNA_defaults.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_legacy.h"
#include "BKE_gpencil_modifier_legacy.h"
#include "BKE_lib_query.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_screen.h"

#include "MEM_guardedalloc.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"
#include "RNA_prototypes.h"

#include "BLT_translation.h"

#include "MOD_gpencil_legacy_modifiertypes.h"
#include "MOD_gpencil_legacy_ui_common.h"
#include "MOD_gpencil_legacy_util.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "WM_api.h"

static void initData(GpencilModifierData *md)
{
  DashGpencilModifierData *dmd = (DashGpencilModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(dmd, modifier));

  MEMCPY_STRUCT_AFTER(dmd, DNA_struct_default_get(DashGpencilModifierData), modifier);

  DashGpencilModifierSegment *ds = DNA_struct_default_alloc(DashGpencilModifierSegment);
  ds->dmd = dmd;
  STRNCPY_UTF8(ds->name, DATA_("Segment"));

  dmd->segments = ds;
}

static void copyData(const GpencilModifierData *md, GpencilModifierData *target)
{
  DashGpencilModifierData *dmd = (DashGpencilModifierData *)target;
  const DashGpencilModifierData *dmd_src = (const DashGpencilModifierData *)md;

  BKE_gpencil_modifier_copydata_generic(md, target);

  dmd->segments = static_cast<DashGpencilModifierSegment *>(MEM_dupallocN(dmd_src->segments));
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

  int sequence_length = 0;
  for (int i = 0; i < dmd->segments_len; i++) {
    sequence_length += dmd->segments[i].dash + real_gap(&dmd->segments[i]);
  }
  if (sequence_length < 1) {
    /* This means the whole segment has no length, can't do dot-dash. */
    return false;
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
    stroke->runtime.gps_orig = gps->runtime.gps_orig;
    if (ds->flag & GP_DASH_USE_CYCLIC) {
      stroke->flag |= GP_STROKE_CYCLIC;
    }

    for (int is = 0; is < size; is++) {
      bGPDspoint *p = &gps->points[new_stroke_offset + is];
      stroke->points[is].x = p->x;
      stroke->points[is].y = p->y;
      stroke->points[is].z = p->z;
      stroke->points[is].pressure = p->pressure * ds->radius;
      stroke->points[is].strength = p->strength * ds->opacity;
      /* Assign original point pointers. */
      stroke->points[is].runtime.idx_orig = p->runtime.idx_orig;
      stroke->points[is].runtime.pt_orig = p->runtime.pt_orig;
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

  ListBase result = {nullptr, nullptr};

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
                                       dmd->flag & GP_LENGTH_INVERT_MATERIAL))
    {
      if (stroke_dash(gps, dmd, &result)) {
        BLI_remlink(&gpf->strokes, gps);
        BKE_gpencil_free_stroke(gps);
      }
    }
  }
  bGPDstroke *gps_dash;
  while ((gps_dash = static_cast<bGPDstroke *>(BLI_pophead(&result)))) {
    BLI_addtail(&gpf->strokes, gps_dash);
    BKE_gpencil_stroke_geometry_update(gpd, gps_dash);
  }
}

static void bakeModifier(Main * /*bmain*/,
                         Depsgraph * /*depsgraph*/,
                         GpencilModifierData *md,
                         Object *ob)
{
  bGPdata *gpd = static_cast<bGPdata *>(ob->data);

  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      apply_dash_for_frame(ob, gpl, gpd, gpf, (DashGpencilModifierData *)md);
    }
  }
}

/* -------------------------------- */

static bool isDisabled(GpencilModifierData *md, int /*userRenderParams*/)
{
  DashGpencilModifierData *dmd = (DashGpencilModifierData *)md;

  int sequence_length = 0;
  for (int i = 0; i < dmd->segments_len; i++) {
    sequence_length += dmd->segments[i].dash + real_gap(&dmd->segments[i]);
  }
  /* This means the whole segment has no length, can't do dot-dash. */
  return sequence_length < 1;
}

/* Generic "generateStrokes" callback */
static void generateStrokes(GpencilModifierData *md, Depsgraph *depsgraph, Object *ob)
{
  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  bGPdata *gpd = static_cast<bGPdata *>(ob->data);

  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    bGPDframe *gpf = BKE_gpencil_frame_retime_get(depsgraph, scene, ob, gpl);
    if (gpf == nullptr) {
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

static void segment_list_item(uiList * /*ui_list*/,
                              const bContext * /*C*/,
                              uiLayout *layout,
                              PointerRNA * /*idataptr*/,
                              PointerRNA *itemptr,
                              int /*icon*/,
                              PointerRNA * /*active_dataptr*/,
                              const char * /*active_propname*/,
                              int /*index*/,
                              int /*flt_flag*/)
{
  uiLayout *row = uiLayoutRow(layout, true);
  uiItemR(row, itemptr, "name", UI_ITEM_R_NO_BG, "", ICON_NONE);
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, nullptr);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "dash_offset", 0, nullptr, ICON_NONE);

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
                 nullptr,
                 3,
                 10,
                 0,
                 1,
                 UI_TEMPLATE_LIST_FLAG_NONE);

  uiLayout *col = uiLayoutColumn(row, false);
  uiLayout *sub = uiLayoutColumn(col, true);
  uiItemO(sub, "", ICON_ADD, "GPENCIL_OT_segment_add");
  uiItemO(sub, "", ICON_REMOVE, "GPENCIL_OT_segment_remove");
  uiItemS(col);
  sub = uiLayoutColumn(col, true);
  uiItemEnumO_string(sub, "", ICON_TRIA_UP, "GPENCIL_OT_segment_move", "type", "UP");
  uiItemEnumO_string(sub, "", ICON_TRIA_DOWN, "GPENCIL_OT_segment_move", "type", "DOWN");

  DashGpencilModifierData *dmd = static_cast<DashGpencilModifierData *>(ptr->data);

  if (dmd->segment_active_index >= 0 && dmd->segment_active_index < dmd->segments_len) {
    PointerRNA ds_ptr;
    RNA_pointer_create(ptr->owner_id,
                       &RNA_DashGpencilModifierSegment,
                       &dmd->segments[dmd->segment_active_index],
                       &ds_ptr);

    sub = uiLayoutColumn(layout, true);
    uiItemR(sub, &ds_ptr, "dash", 0, nullptr, ICON_NONE);
    uiItemR(sub, &ds_ptr, "gap", 0, nullptr, ICON_NONE);

    sub = uiLayoutColumn(layout, false);
    uiItemR(sub, &ds_ptr, "radius", 0, nullptr, ICON_NONE);
    uiItemR(sub, &ds_ptr, "opacity", 0, nullptr, ICON_NONE);
    uiItemR(sub, &ds_ptr, "material_index", 0, nullptr, ICON_NONE);
    uiItemR(sub, &ds_ptr, "use_cyclic", 0, nullptr, ICON_NONE);
  }

  gpencil_modifier_panel_end(layout, ptr);
}

static void mask_panel_draw(const bContext * /*C*/, Panel *panel)
{
  gpencil_modifier_masking_panel_draw(panel, true, false);
}

static void panelRegister(ARegionType *region_type)
{
  PanelType *panel_type = gpencil_modifier_panel_register(
      region_type, eGpencilModifierType_Dash, panel_draw);
  gpencil_modifier_subpanel_register(
      region_type, "mask", "Influence", nullptr, mask_panel_draw, panel_type);

  uiListType *list_type = static_cast<uiListType *>(
      MEM_callocN(sizeof(uiListType), "dash modifier segment uilist"));
  STRNCPY(list_type->idname, "MOD_UL_dash_segment");
  list_type->draw_item = segment_list_item;
  WM_uilisttype_add(list_type);
}

GpencilModifierTypeInfo modifierType_Gpencil_Dash = {
    /*name*/ N_("Dot Dash"),
    /*struct_name*/ "DashGpencilModifierData",
    /*struct_size*/ sizeof(DashGpencilModifierData),
    /*type*/ eGpencilModifierTypeType_Gpencil,
    /*flags*/ eGpencilModifierTypeFlag_SupportsEditmode,

    /*copyData*/ copyData,

    /*deformStroke*/ nullptr,
    /*generateStrokes*/ generateStrokes,
    /*bakeModifier*/ bakeModifier,
    /*remapTime*/ nullptr,

    /*initData*/ initData,
    /*freeData*/ freeData,
    /*isDisabled*/ isDisabled,
    /*updateDepsgraph*/ nullptr,
    /*dependsOnTime*/ nullptr,
    /*foreachIDLink*/ foreachIDLink,
    /*foreachTexLink*/ nullptr,
    /*panelRegister*/ panelRegister,
};
