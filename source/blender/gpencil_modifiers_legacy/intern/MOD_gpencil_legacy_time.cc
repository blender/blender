/* SPDX-FileCopyrightText: 2018 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_defaults.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.h"
#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_legacy.h"
#include "BKE_gpencil_modifier_legacy.h"
#include "BKE_lib_query.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_screen.h"

#include "RNA_access.hh"
#include "RNA_prototypes.h"

#include "MOD_gpencil_legacy_modifiertypes.h"
#include "MOD_gpencil_legacy_ui_common.h"
#include "MOD_gpencil_legacy_util.h"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "WM_api.hh"

#include "DEG_depsgraph.h"

static void init_data(GpencilModifierData *md)
{
  TimeGpencilModifierData *gpmd = (TimeGpencilModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(gpmd, modifier));

  MEMCPY_STRUCT_AFTER(gpmd, DNA_struct_default_get(TimeGpencilModifierData), modifier);
  TimeGpencilModifierSegment *ds = DNA_struct_default_alloc(TimeGpencilModifierSegment);
  ds->gpmd = gpmd;
  STRNCPY_UTF8(ds->name, DATA_("Segment"));

  gpmd->segments = ds;
}

static void copy_data(const GpencilModifierData *md, GpencilModifierData *target)
{
  TimeGpencilModifierData *gpmd = (TimeGpencilModifierData *)target;
  const TimeGpencilModifierData *gpmd_src = (const TimeGpencilModifierData *)md;
  BKE_gpencil_modifier_copydata_generic(md, target);
  gpmd->segments = static_cast<TimeGpencilModifierSegment *>(MEM_dupallocN(gpmd_src->segments));
}

static void free_data(GpencilModifierData *md)
{
  TimeGpencilModifierData *gpmd = (TimeGpencilModifierData *)md;

  MEM_SAFE_FREE(gpmd->segments);
}

static int remap_time(GpencilModifierData *md,
                      Depsgraph * /*depsgraph*/,
                      Scene *scene,
                      Object * /*ob*/,
                      bGPDlayer *gpl,
                      int cfra)
{
  TimeGpencilModifierData *mmd = (TimeGpencilModifierData *)md;
  const bool custom = mmd->flag & GP_TIME_CUSTOM_RANGE;
  const bool invgpl = mmd->flag & GP_TIME_INVERT_LAYER;
  const bool invpass = mmd->flag & GP_TIME_INVERT_LAYERPASS;
  int sfra = custom ? mmd->sfra : scene->r.sfra;
  int efra = custom ? mmd->efra : scene->r.efra;
  int offset = mmd->offset;
  int nfra = 0;

  CLAMP_MIN(sfra, 0);
  CLAMP_MIN(efra, 0);

  if (offset < 0) {
    offset = abs(efra - sfra + offset + 1);
  }
  /* Avoid inverse ranges. */
  if (efra <= sfra) {
    return cfra;
  }

  /* omit if filter by layer */
  if (mmd->layername[0] != '\0') {
    if (invgpl == false) {
      if (!STREQ(mmd->layername, gpl->info)) {
        return cfra;
      }
    }
    else {
      if (STREQ(mmd->layername, gpl->info)) {
        return cfra;
      }
    }
  }
  /* verify pass */
  if (mmd->layer_pass > 0) {
    if (invpass == false) {
      if (gpl->pass_index != mmd->layer_pass) {
        return cfra;
      }
    }
    else {
      if (gpl->pass_index == mmd->layer_pass) {
        return cfra;
      }
    }
  }

  /* apply frame scale */
  cfra *= mmd->frame_scale;
  CLAMP_MIN(cfra, 1);

  /* if fix mode, return predefined frame number */
  if (mmd->mode == GP_TIME_MODE_FIX) {
    return offset;
  }

  if (mmd->mode == GP_TIME_MODE_NORMAL) {
    if ((mmd->flag & GP_TIME_KEEP_LOOP) == 0) {
      nfra = cfra + sfra + offset - 1 < efra ? cfra + sfra + offset - 1 : efra;
    }
    else {
      nfra = (offset + cfra - 1) % (efra - sfra + 1) + sfra;
    }
  }
  if (mmd->mode == GP_TIME_MODE_REVERSE) {
    if ((mmd->flag & GP_TIME_KEEP_LOOP) == 0) {
      nfra = efra - cfra - offset > sfra ? efra - cfra - offset + 1 : sfra;
    }
    else {
      nfra = (efra + 1 - (cfra + offset - 1) % (efra - sfra + 1)) - 1;
    }
  }

  if (mmd->mode == GP_TIME_MODE_PINGPONG) {
    if ((mmd->flag & GP_TIME_KEEP_LOOP) == 0) {
      if ((int(cfra + offset - 1) / (efra - sfra)) % (2)) {
        nfra = efra - (cfra + offset - 1) % (efra - sfra);
      }
      else {
        nfra = sfra + (cfra + offset - 1) % (efra - sfra);
      }
      if (cfra > (efra - sfra) * 2) {
        nfra = sfra + offset;
      }
    }
    else {

      if ((int(cfra + offset - 1) / (efra - sfra)) % (2)) {
        nfra = efra - (cfra + offset - 1) % (efra - sfra);
      }
      else {
        nfra = sfra + (cfra + offset - 1) % (efra - sfra);
      }
    }
  }

  if (mmd->mode == GP_TIME_MODE_CHAIN) {
    int sequence_length = 0;
    int frame_key = 0;
    int *segment_arr;
    int start, end;
    if (mmd->segments_len > 0) {
      for (int i = 0; i < mmd->segments_len; i++) {
        start = mmd->segments[i].seg_start;
        end = mmd->segments[i].seg_end;
        if (mmd->segments[i].seg_end < mmd->segments[i].seg_start) {
          start = mmd->segments[i].seg_end;
          end = mmd->segments[i].seg_start;
        }

        if (ELEM(mmd->segments[i].seg_mode, GP_TIME_SEG_MODE_PINGPONG)) {
          sequence_length += ((end - start) * mmd->segments[i].seg_repeat) * 2 + 1;
        }
        else {
          sequence_length += ((end - start + 1) * mmd->segments[i].seg_repeat);
        }
      }
      segment_arr = static_cast<int *>(
          MEM_malloc_arrayN(sequence_length, sizeof(int *), __func__));

      for (int i = 0; i < mmd->segments_len; i++) {

        if (mmd->segments[i].seg_end < mmd->segments[i].seg_start) {
          start = mmd->segments[i].seg_end;
          end = mmd->segments[i].seg_start;
        }
        else {
          start = mmd->segments[i].seg_start;
          end = mmd->segments[i].seg_end;
        }
        for (int a = 0; a < mmd->segments[i].seg_repeat; a++) {
          switch (mmd->segments[i].seg_mode) {
            case GP_TIME_SEG_MODE_NORMAL:
              for (int b = 0; b < end - start + 1; b++) {
                segment_arr[frame_key] = start + b;
                frame_key++;
              }
              break;
            case GP_TIME_SEG_MODE_REVERSE:
              for (int b = 0; b < end - start + 1; b++) {
                segment_arr[frame_key] = end - b;
                frame_key++;
              }
              break;
            case GP_TIME_SEG_MODE_PINGPONG:
              for (int b = 0; b < end - start; b++) {
                segment_arr[frame_key] = start + b;
                frame_key++;
              }
              for (int b = 0; b < end - start; b++) {
                segment_arr[frame_key] = end - b;
                frame_key++;
                if (a == mmd->segments[i].seg_repeat - 1 && b == end - start - 1) {
                  segment_arr[frame_key] = start;
                  frame_key++;
                }
              }
              break;
          }
        }
      }

      if ((mmd->flag & GP_TIME_KEEP_LOOP) == 0) {
        if ((cfra + offset - 1) < sequence_length) {
          nfra = segment_arr[(cfra - 1 + offset)];
        }
        else {
          nfra = segment_arr[frame_key - 1];
        }
      }
      else {
        nfra = segment_arr[(cfra - 1 + offset) % sequence_length];
      }

      MEM_freeN(segment_arr);
    }
  }

  return nfra;
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
static void foreach_ID_link(GpencilModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  TimeGpencilModifierData *mmd = (TimeGpencilModifierData *)md;

  walk(user_data, ob, (ID **)&mmd->material, IDWALK_CB_USER);
}
static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *row, *col;
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, nullptr);

  int mode = RNA_enum_get(ptr, "mode");

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "mode", UI_ITEM_NONE, nullptr, ICON_NONE);

  col = uiLayoutColumn(layout, false);

  const char *text = (mode == GP_TIME_MODE_FIX) ? IFACE_("Frame") : IFACE_("Frame Offset");
  uiItemR(col, ptr, "offset", UI_ITEM_NONE, text, ICON_NONE);

  row = uiLayoutRow(col, false);
  uiLayoutSetActive(row, mode != GP_TIME_MODE_FIX);
  uiItemR(row, ptr, "frame_scale", UI_ITEM_NONE, IFACE_("Scale"), ICON_NONE);

  row = uiLayoutRow(layout, false);
  uiLayoutSetActive(row, mode != GP_TIME_MODE_FIX);
  uiItemR(row, ptr, "use_keep_loop", UI_ITEM_NONE, nullptr, ICON_NONE);

  if (mode == GP_TIME_MODE_CHAIN) {

    row = uiLayoutRow(layout, false);
    uiLayoutSetPropSep(row, false);

    uiTemplateList(row,
                   (bContext *)C,
                   "MOD_UL_time_segment",
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

    col = uiLayoutColumn(row, false);
    uiLayoutSetContextPointer(col, "modifier", ptr);

    uiLayout *sub = uiLayoutColumn(col, true);
    uiItemO(sub, "", ICON_ADD, "GPENCIL_OT_time_segment_add");
    uiItemO(sub, "", ICON_REMOVE, "GPENCIL_OT_time_segment_remove");
    uiItemS(col);
    sub = uiLayoutColumn(col, true);
    uiItemEnumO_string(sub, "", ICON_TRIA_UP, "GPENCIL_OT_time_segment_move", "type", "UP");
    uiItemEnumO_string(sub, "", ICON_TRIA_DOWN, "GPENCIL_OT_time_segment_move", "type", "DOWN");

    TimeGpencilModifierData *gpmd = static_cast<TimeGpencilModifierData *>(ptr->data);
    if (gpmd->segment_active_index >= 0 && gpmd->segment_active_index < gpmd->segments_len) {
      PointerRNA ds_ptr;
      RNA_pointer_create(ptr->owner_id,
                         &RNA_TimeGpencilModifierSegment,
                         &gpmd->segments[gpmd->segment_active_index],
                         &ds_ptr);

      sub = uiLayoutColumn(layout, true);
      uiItemR(sub, &ds_ptr, "seg_mode", UI_ITEM_NONE, nullptr, ICON_NONE);
      sub = uiLayoutColumn(layout, true);
      uiItemR(sub, &ds_ptr, "seg_start", UI_ITEM_NONE, nullptr, ICON_NONE);
      uiItemR(sub, &ds_ptr, "seg_end", UI_ITEM_NONE, nullptr, ICON_NONE);
      uiItemR(sub, &ds_ptr, "seg_repeat", UI_ITEM_NONE, nullptr, ICON_NONE);
    }

    gpencil_modifier_panel_end(layout, ptr);
  }

  gpencil_modifier_panel_end(layout, ptr);
}

static void custom_range_header_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, nullptr);

  int mode = RNA_enum_get(ptr, "mode");

  uiLayoutSetActive(layout, !ELEM(mode, GP_TIME_MODE_FIX, GP_TIME_MODE_CHAIN));

  uiItemR(layout, ptr, "use_custom_frame_range", UI_ITEM_NONE, nullptr, ICON_NONE);
}

static void custom_range_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, nullptr);

  int mode = RNA_enum_get(ptr, "mode");

  uiLayoutSetPropSep(layout, true);
  uiLayoutSetActive(layout,
                    !ELEM(mode, GP_TIME_MODE_FIX, GP_TIME_MODE_CHAIN) &&
                        RNA_boolean_get(ptr, "use_custom_frame_range"));

  col = uiLayoutColumn(layout, true);
  uiItemR(col, ptr, "frame_start", UI_ITEM_NONE, IFACE_("Frame Start"), ICON_NONE);
  uiItemR(col, ptr, "frame_end", UI_ITEM_NONE, IFACE_("End"), ICON_NONE);
}

static void mask_panel_draw(const bContext * /*C*/, Panel *panel)
{
  gpencil_modifier_masking_panel_draw(panel, false, false);
}

static void panel_register(ARegionType *region_type)
{
  PanelType *panel_type = gpencil_modifier_panel_register(
      region_type, eGpencilModifierType_Time, panel_draw);
  gpencil_modifier_subpanel_register(region_type,
                                     "custom_range",
                                     "",
                                     custom_range_header_draw,
                                     custom_range_panel_draw,
                                     panel_type);
  gpencil_modifier_subpanel_register(
      region_type, "mask", "Influence", nullptr, mask_panel_draw, panel_type);

  uiListType *list_type = static_cast<uiListType *>(
      MEM_callocN(sizeof(uiListType), "time modifier segment uilist"));
  STRNCPY(list_type->idname, "MOD_UL_time_segment");
  list_type->draw_item = segment_list_item;
  WM_uilisttype_add(list_type);
}

GpencilModifierTypeInfo modifierType_Gpencil_Time = {
    /*name*/ N_("TimeOffset"),
    /*struct_name*/ "TimeGpencilModifierData",
    /*struct_size*/ sizeof(TimeGpencilModifierData),
    /*type*/ eGpencilModifierTypeType_Gpencil,
    /*flags*/ eGpencilModifierTypeFlag_NoApply,

    /*copy_data*/ copy_data,

    /*deform_stroke*/ nullptr,
    /*generate_strokes*/ nullptr,
    /*bake_modifier*/ nullptr,
    /*remap_time*/ remap_time,

    /*init_data*/ init_data,
    /*free_data*/ free_data,
    /*is_disabled*/ nullptr,
    /*update_depsgraph*/ nullptr,
    /*depends_on_time*/ nullptr,
    /*foreach_ID_link*/ foreach_ID_link,
    /*foreach_tex_link*/ nullptr,
    /*panel_register*/ panel_register,
};
