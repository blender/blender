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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup spnla
 */

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "DNA_anim_types.h"

#include "BLI_utildefines.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_nla.h"
#include "BKE_screen.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"

#include "ED_anim_api.h"
#include "ED_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "nla_intern.h"  // own include

/* ******************* nla editor space & buttons ************** */

/* -------------- */

static void do_nla_region_buttons(bContext *C, void *UNUSED(arg), int UNUSED(event))
{
  // Scene *scene = CTX_data_scene(C);
#if 0
  switch (event) {
    /* pass */
  }
#endif
  /* default for now */
  WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);
  WM_event_add_notifier(C, NC_SCENE | ND_TRANSFORM, NULL);
}

bool nla_panel_context(const bContext *C,
                       PointerRNA *adt_ptr,
                       PointerRNA *nlt_ptr,
                       PointerRNA *strip_ptr)
{
  bAnimContext ac;
  bAnimListElem *ale = NULL;
  ListBase anim_data = {NULL, NULL};
  short found = 0; /* not bool, since we need to indicate "found but not ideal" status */
  int filter;

  /* For now, only draw if we could init the anim-context info
   * (necessary for all animation-related tools)
   * to work correctly is able to be correctly retrieved. There's no point showing empty panels? */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return false;
  }

  /* extract list of active channel(s), of which we should only take the first one
   * - we need the channels flag to get the active AnimData block when there are no NLA Tracks
   */
  // XXX: double-check active!
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_ACTIVE |
            ANIMFILTER_LIST_CHANNELS);
  ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

  for (ale = anim_data.first; ale; ale = ale->next) {
    switch (ale->type) {
      case ANIMTYPE_NLATRACK: /* NLA Track - The primary data type which should get caught */
      {
        NlaTrack *nlt = (NlaTrack *)ale->data;
        AnimData *adt = ale->adt;

        /* found it, now set the pointers */
        if (adt_ptr) {
          /* AnimData pointer */
          RNA_pointer_create(ale->id, &RNA_AnimData, adt, adt_ptr);
        }
        if (nlt_ptr) {
          /* NLA-Track pointer */
          RNA_pointer_create(ale->id, &RNA_NlaTrack, nlt, nlt_ptr);
        }
        if (strip_ptr) {
          /* NLA-Strip pointer */
          NlaStrip *strip = BKE_nlastrip_find_active(nlt);
          RNA_pointer_create(ale->id, &RNA_NlaStrip, strip, strip_ptr);
        }

        found = 1;
        break;
      }
      case ANIMTYPE_SCENE: /* Top-Level Widgets doubling up as datablocks */
      case ANIMTYPE_OBJECT:
      case ANIMTYPE_DSMAT: /* Datablock AnimData Expanders */
      case ANIMTYPE_DSLAM:
      case ANIMTYPE_DSCAM:
      case ANIMTYPE_DSCACHEFILE:
      case ANIMTYPE_DSCUR:
      case ANIMTYPE_DSSKEY:
      case ANIMTYPE_DSWOR:
      case ANIMTYPE_DSNTREE:
      case ANIMTYPE_DSPART:
      case ANIMTYPE_DSMBALL:
      case ANIMTYPE_DSARM:
      case ANIMTYPE_DSMESH:
      case ANIMTYPE_DSTEX:
      case ANIMTYPE_DSLAT:
      case ANIMTYPE_DSLINESTYLE:
      case ANIMTYPE_DSSPK:
      case ANIMTYPE_DSGPENCIL:
      case ANIMTYPE_PALETTE:
      case ANIMTYPE_DSHAIR:
      case ANIMTYPE_DSPOINTCLOUD:
      case ANIMTYPE_DSVOLUME:
      case ANIMTYPE_DSSIMULATION: {
        /* for these channels, we only do AnimData */
        if (ale->adt && adt_ptr) {
          ID *id;

          if ((ale->data == NULL) || (ale->type == ANIMTYPE_OBJECT)) {
            /* ale->data is not an ID block! */
            id = ale->id;
          }
          else {
            /* ale->data is always the proper ID block we need,
             * but ale->id may not be (i.e. for textures) */
            id = (ID *)ale->data;
          }

          /* AnimData pointer */
          if (adt_ptr) {
            RNA_pointer_create(id, &RNA_AnimData, ale->adt, adt_ptr);
          }

          /* set found status to -1, since setting to 1 would break the loop
           * and potentially skip an active NLA-Track in some cases...
           */
          found = -1;
        }
        break;
      }
    }

    if (found > 0) {
      break;
    }
  }

  /* free temp data */
  ANIM_animdata_freelist(&anim_data);

  return (found != 0);
}

#if 0
static bool nla_panel_poll(const bContext *C, PanelType *pt)
{
  return nla_panel_context(C, NULL, NULL);
}
#endif

static bool nla_animdata_panel_poll(const bContext *C, PanelType *UNUSED(pt))
{
  PointerRNA ptr;
  return (nla_panel_context(C, &ptr, NULL, NULL) && (ptr.data != NULL));
}

static bool nla_strip_panel_poll(const bContext *C, PanelType *UNUSED(pt))
{
  PointerRNA ptr;
  return (nla_panel_context(C, NULL, NULL, &ptr) && (ptr.data != NULL));
}

static bool nla_strip_actclip_panel_poll(const bContext *C, PanelType *UNUSED(pt))
{
  PointerRNA ptr;
  NlaStrip *strip;

  if (!nla_panel_context(C, NULL, NULL, &ptr)) {
    return 0;
  }
  if (ptr.data == NULL) {
    return 0;
  }

  strip = ptr.data;
  return (strip->type == NLASTRIP_TYPE_CLIP);
}

static bool nla_strip_eval_panel_poll(const bContext *C, PanelType *UNUSED(pt))
{
  PointerRNA ptr;
  NlaStrip *strip;

  if (!nla_panel_context(C, NULL, NULL, &ptr)) {
    return 0;
  }
  if (ptr.data == NULL) {
    return 0;
  }

  strip = ptr.data;

  if (strip->type == NLASTRIP_TYPE_SOUND) {
    return 0;
  }

  return 1;
}

/* -------------- */

/* active AnimData */
static void nla_panel_animdata(const bContext *C, Panel *panel)
{
  PointerRNA adt_ptr;
  /* AnimData *adt; */
  uiLayout *layout = panel->layout;
  uiLayout *row;
  uiBlock *block;

  /* check context and also validity of pointer */
  if (!nla_panel_context(C, &adt_ptr, NULL, NULL)) {
    return;
  }

  /* adt = adt_ptr.data; */

  block = uiLayoutGetBlock(layout);
  UI_block_func_handle_set(block, do_nla_region_buttons, NULL);
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);

  /* AnimData Source Properties ----------------------------------- */

  /* icon + id-block name of block where AnimData came from to prevent
   * accidentally changing the properties of the wrong action
   */
  if (adt_ptr.owner_id) {
    ID *id = adt_ptr.owner_id;
    PointerRNA id_ptr;

    RNA_id_pointer_create(id, &id_ptr);

    /* ID-block name > AnimData */
    row = uiLayoutRow(layout, true);
    uiLayoutSetAlignment(row, UI_LAYOUT_ALIGN_LEFT);

    uiItemL(row, id->name + 2, RNA_struct_ui_icon(id_ptr.type)); /* id-block (src) */
    uiItemL(row, "", ICON_SMALL_TRI_RIGHT_VEC);                  /* expander */
    uiItemL(row, IFACE_("Animation Data"), ICON_ANIM_DATA);      /* animdata */

    uiItemS(layout);
  }

  /* Active Action Properties ------------------------------------- */
  /* action */
  row = uiLayoutRow(layout, true);
  uiTemplateID(row,
               (bContext *)C,
               &adt_ptr,
               "action",
               "ACTION_OT_new",
               NULL,
               "NLA_OT_action_unlink",
               UI_TEMPLATE_ID_FILTER_ALL,
               false,
               NULL);

  /* extrapolation */
  row = uiLayoutRow(layout, true);
  uiItemR(row, &adt_ptr, "action_extrapolation", 0, IFACE_("Extrapolation"), ICON_NONE);

  /* blending */
  row = uiLayoutRow(layout, true);
  uiItemR(row, &adt_ptr, "action_blend_type", 0, IFACE_("Blending"), ICON_NONE);

  /* influence */
  row = uiLayoutRow(layout, true);
  uiItemR(row, &adt_ptr, "action_influence", 0, IFACE_("Influence"), ICON_NONE);
}

/* generic settings for active NLA-Strip */
static void nla_panel_stripname(const bContext *C, Panel *panel)
{
  PointerRNA strip_ptr;
  uiLayout *layout = panel->layout;
  uiLayout *row;
  uiBlock *block;

  if (!nla_panel_context(C, NULL, NULL, &strip_ptr)) {
    return;
  }

  block = uiLayoutGetBlock(layout);
  UI_block_func_handle_set(block, do_nla_region_buttons, NULL);

  /* Strip Properties ------------------------------------- */
  /* strip type */
  row = uiLayoutRow(layout, false);
  if (RNA_enum_get(&strip_ptr, "type") == NLASTRIP_TYPE_CLIP) {
    uiItemL(row, "", ICON_ANIM);
  }
  else if (RNA_enum_get(&strip_ptr, "type") == NLASTRIP_TYPE_TRANSITION) {
    uiItemL(row, "", ICON_ARROW_LEFTRIGHT);
  }
  else if (RNA_enum_get(&strip_ptr, "type") == NLASTRIP_TYPE_META) {
    uiItemL(row, "", ICON_SEQ_STRIP_META);
  }
  else if (RNA_enum_get(&strip_ptr, "type") == NLASTRIP_TYPE_SOUND) {
    uiItemL(row, "", ICON_SOUND);
  }

  uiItemR(row, &strip_ptr, "name", 0, "", ICON_NLA);

  UI_block_emboss_set(block, UI_EMBOSS_NONE);
  uiItemR(row, &strip_ptr, "mute", 0, "", ICON_NONE);
  UI_block_emboss_set(block, UI_EMBOSS);
}

/* generic settings for active NLA-Strip */
static void nla_panel_properties(const bContext *C, Panel *panel)
{
  PointerRNA strip_ptr;
  uiLayout *layout = panel->layout;
  uiLayout *column, *row;
  uiBlock *block;
  short showEvalProps = 1;

  if (!nla_panel_context(C, NULL, NULL, &strip_ptr)) {
    return;
  }

  block = uiLayoutGetBlock(layout);
  UI_block_func_handle_set(block, do_nla_region_buttons, NULL);

  /* Strip Properties ------------------------------------- */
  /* strip type */

  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);

  /* strip extents */
  column = uiLayoutColumn(layout, true);
  uiItemR(column, &strip_ptr, "frame_start", 0, IFACE_("Frame Start"), ICON_NONE);
  uiItemR(column, &strip_ptr, "frame_end", 0, IFACE_("End"), ICON_NONE);

  /* Evaluation-Related Strip Properties ------------------ */

  /* sound properties strips don't have these settings */
  if (RNA_enum_get(&strip_ptr, "type") == NLASTRIP_TYPE_SOUND) {
    showEvalProps = 0;
  }

  /* only show if allowed to... */
  if (showEvalProps) {
    /* extrapolation */
    column = uiLayoutColumn(layout, false);
    uiItemR(column, &strip_ptr, "extrapolation", 0, NULL, ICON_NONE);
    uiItemR(column, &strip_ptr, "blend_type", 0, NULL, ICON_NONE);

    /* Blend in/out + auto-blending:
     * - blend in/out can only be set when autoblending is off
     */

    uiItemS(layout);

    column = uiLayoutColumn(layout, true);
    uiLayoutSetActive(column, RNA_boolean_get(&strip_ptr, "use_auto_blend") == false);
    uiItemR(column, &strip_ptr, "blend_in", 0, IFACE_("Blend In"), ICON_NONE);
    uiItemR(column, &strip_ptr, "blend_out", 0, IFACE_("Out"), ICON_NONE);

    row = uiLayoutRow(column, true);
    uiLayoutSetActive(row, RNA_boolean_get(&strip_ptr, "use_animated_influence") == false);
    uiItemR(row, &strip_ptr, "use_auto_blend", 0, NULL, ICON_NONE);  // XXX as toggle?

    /* settings */
    column = uiLayoutColumnWithHeading(layout, true, IFACE_("Playback"));
    row = uiLayoutRow(column, true);
    uiLayoutSetActive(row,
                      !(RNA_boolean_get(&strip_ptr, "use_animated_influence") ||
                        RNA_boolean_get(&strip_ptr, "use_animated_time")));
    uiItemR(row, &strip_ptr, "use_reverse", 0, NULL, ICON_NONE);

    uiItemR(column, &strip_ptr, "use_animated_time_cyclic", 0, NULL, ICON_NONE);
  }
}

/* action-clip only settings for active NLA-Strip */
static void nla_panel_actclip(const bContext *C, Panel *panel)
{
  PointerRNA strip_ptr;
  uiLayout *layout = panel->layout;
  uiLayout *column, *row;
  uiBlock *block;

  /* check context and also validity of pointer */
  if (!nla_panel_context(C, NULL, NULL, &strip_ptr)) {
    return;
  }

  block = uiLayoutGetBlock(layout);
  UI_block_func_handle_set(block, do_nla_region_buttons, NULL);
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);

  /* Strip Properties ------------------------------------- */
  /* action pointer */
  row = uiLayoutRow(layout, true);
  uiItemR(row, &strip_ptr, "action", 0, NULL, ICON_ACTION);

  /* action extents */
  column = uiLayoutColumn(layout, true);
  uiItemR(column, &strip_ptr, "action_frame_start", 0, IFACE_("Frame Start"), ICON_NONE);
  uiItemR(column, &strip_ptr, "action_frame_end", 0, IFACE_("End"), ICON_NONE);

  row = uiLayoutRowWithHeading(layout, false, IFACE_("Sync Length"));
  uiItemR(row, &strip_ptr, "use_sync_length", 0, "", ICON_NONE);
  uiItemO(row, IFACE_("Now"), ICON_FILE_REFRESH, "NLA_OT_action_sync_length");

  /* action usage */
  column = uiLayoutColumn(layout, true);
  uiLayoutSetActive(column, RNA_boolean_get(&strip_ptr, "use_animated_time") == false);
  uiItemR(column, &strip_ptr, "scale", 0, IFACE_("Playback Scale"), ICON_NONE);
  uiItemR(column, &strip_ptr, "repeat", 0, NULL, ICON_NONE);
}

/* evaluation settings for active NLA-Strip */
static void nla_panel_animated_influence_header(const bContext *C, Panel *panel)
{
  PointerRNA strip_ptr;
  uiLayout *layout = panel->layout;
  uiLayout *col;
  uiBlock *block;

  /* check context and also validity of pointer */
  if (!nla_panel_context(C, NULL, NULL, &strip_ptr)) {
    return;
  }

  block = uiLayoutGetBlock(layout);
  UI_block_func_handle_set(block, do_nla_region_buttons, NULL);

  col = uiLayoutColumn(layout, true);
  uiItemR(col, &strip_ptr, "use_animated_influence", 0, "", ICON_NONE);
}

/* evaluation settings for active NLA-Strip */
static void nla_panel_evaluation(const bContext *C, Panel *panel)
{
  PointerRNA strip_ptr;
  uiLayout *layout = panel->layout;
  uiBlock *block;

  /* check context and also validity of pointer */
  if (!nla_panel_context(C, NULL, NULL, &strip_ptr)) {
    return;
  }

  block = uiLayoutGetBlock(layout);
  UI_block_func_handle_set(block, do_nla_region_buttons, NULL);
  uiLayoutSetPropSep(layout, true);

  uiLayoutSetEnabled(layout, RNA_boolean_get(&strip_ptr, "use_animated_influence"));
  uiItemR(layout, &strip_ptr, "influence", 0, NULL, ICON_NONE);
}

static void nla_panel_animated_strip_time_header(const bContext *C, Panel *panel)
{
  PointerRNA strip_ptr;
  uiLayout *layout = panel->layout;
  uiLayout *col;
  uiBlock *block;

  /* check context and also validity of pointer */
  if (!nla_panel_context(C, NULL, NULL, &strip_ptr)) {
    return;
  }

  block = uiLayoutGetBlock(layout);
  UI_block_func_handle_set(block, do_nla_region_buttons, NULL);

  col = uiLayoutColumn(layout, true);
  uiItemR(col, &strip_ptr, "use_animated_time", 0, "", ICON_NONE);
}

static void nla_panel_animated_strip_time(const bContext *C, Panel *panel)
{
  PointerRNA strip_ptr;
  uiLayout *layout = panel->layout;
  uiBlock *block;

  /* check context and also validity of pointer */
  if (!nla_panel_context(C, NULL, NULL, &strip_ptr)) {
    return;
  }

  block = uiLayoutGetBlock(layout);
  UI_block_func_handle_set(block, do_nla_region_buttons, NULL);
  uiLayoutSetPropSep(layout, true);

  uiLayoutSetEnabled(layout, RNA_boolean_get(&strip_ptr, "use_animated_time"));
  uiItemR(layout, &strip_ptr, "strip_time", 0, NULL, ICON_NONE);
}

/* F-Modifiers for active NLA-Strip */
static void nla_panel_modifiers(const bContext *C, Panel *panel)
{
  PointerRNA strip_ptr;
  NlaStrip *strip;
  FModifier *fcm;
  uiLayout *col, *row;
  uiBlock *block;

  /* check context and also validity of pointer */
  if (!nla_panel_context(C, NULL, NULL, &strip_ptr)) {
    return;
  }
  strip = strip_ptr.data;

  block = uiLayoutGetBlock(panel->layout);
  UI_block_func_handle_set(block, do_nla_region_buttons, NULL);

  /* 'add modifier' button at top of panel */
  {
    row = uiLayoutRow(panel->layout, false);
    block = uiLayoutGetBlock(row);

    // FIXME: we need to set the only-active property so that this
    // will only add modifiers for the active strip (not all selected).
    uiItemMenuEnumO(
        row, (bContext *)C, "NLA_OT_fmodifier_add", "type", IFACE_("Add Modifier"), ICON_NONE);

    /* copy/paste (as sub-row) */
    row = uiLayoutRow(row, true);
    uiItemO(row, "", ICON_COPYDOWN, "NLA_OT_fmodifier_copy");
    uiItemO(row, "", ICON_PASTEDOWN, "NLA_OT_fmodifier_paste");
  }

  /* draw each modifier */
  for (fcm = strip->modifiers.first; fcm; fcm = fcm->next) {
    col = uiLayoutColumn(panel->layout, true);

    ANIM_uiTemplate_fmodifier_draw(col, strip_ptr.owner_id, &strip->modifiers, fcm);
  }
}

/* ******************* general ******************************** */

void nla_buttons_register(ARegionType *art)
{
  PanelType *pt;

  pt = MEM_callocN(sizeof(PanelType), "spacetype nla panel animdata");
  strcpy(pt->idname, "NLA_PT_animdata");
  strcpy(pt->label, N_("Animation Data"));
  strcpy(pt->category, "Edited Action");
  strcpy(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  pt->flag = PNL_NO_HEADER;
  pt->draw = nla_panel_animdata;
  pt->poll = nla_animdata_panel_poll;
  BLI_addtail(&art->paneltypes, pt);

  pt = MEM_callocN(sizeof(PanelType), "spacetype nla panel properties");
  strcpy(pt->idname, "NLA_PT_stripname");
  strcpy(pt->label, N_("Active Strip Name"));
  strcpy(pt->category, "Strip");
  strcpy(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  pt->flag = PNL_NO_HEADER;
  pt->draw = nla_panel_stripname;
  pt->poll = nla_strip_panel_poll;
  BLI_addtail(&art->paneltypes, pt);

  PanelType *pt_properties = pt = MEM_callocN(sizeof(PanelType), "spacetype nla panel properties");
  strcpy(pt->idname, "NLA_PT_properties");
  strcpy(pt->label, N_("Active Strip"));
  strcpy(pt->category, "Strip");
  strcpy(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  pt->draw = nla_panel_properties;
  pt->poll = nla_strip_panel_poll;
  BLI_addtail(&art->paneltypes, pt);

  pt = MEM_callocN(sizeof(PanelType), "spacetype nla panel properties");
  strcpy(pt->idname, "NLA_PT_actionclip");
  strcpy(pt->label, N_("Action Clip"));
  strcpy(pt->category, "Strip");
  strcpy(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  pt->draw = nla_panel_actclip;
  pt->flag = PNL_DEFAULT_CLOSED;
  pt->poll = nla_strip_actclip_panel_poll;
  BLI_addtail(&art->paneltypes, pt);

  pt = MEM_callocN(sizeof(PanelType), "spacetype nla panel evaluation");
  strcpy(pt->idname, "NLA_PT_evaluation");
  strcpy(pt->parent_id, "NLA_PT_properties");
  strcpy(pt->label, N_("Animated Influence"));
  strcpy(pt->category, "Strip");
  strcpy(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  pt->draw = nla_panel_evaluation;
  pt->draw_header = nla_panel_animated_influence_header;
  pt->parent = pt_properties;
  pt->flag = PNL_DEFAULT_CLOSED;
  pt->poll = nla_strip_eval_panel_poll;
  BLI_addtail(&pt_properties->children, BLI_genericNodeN(pt));
  BLI_addtail(&art->paneltypes, pt);

  pt = MEM_callocN(sizeof(PanelType), "spacetype nla panel animated strip time");
  strcpy(pt->idname, "NLA_PT_animated_strip_time");
  strcpy(pt->parent_id, "NLA_PT_properties");
  strcpy(pt->label, N_("Animated Strip Time"));
  strcpy(pt->category, "Strip");
  strcpy(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  pt->draw = nla_panel_animated_strip_time;
  pt->draw_header = nla_panel_animated_strip_time_header;
  pt->parent = pt_properties;
  pt->flag = PNL_DEFAULT_CLOSED;
  pt->poll = nla_strip_eval_panel_poll;
  BLI_addtail(&pt_properties->children, BLI_genericNodeN(pt));
  BLI_addtail(&art->paneltypes, pt);

  pt = MEM_callocN(sizeof(PanelType), "spacetype nla panel modifiers");
  strcpy(pt->idname, "NLA_PT_modifiers");
  strcpy(pt->label, N_("Modifiers"));
  strcpy(pt->category, "Modifiers");
  strcpy(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  pt->draw = nla_panel_modifiers;
  pt->poll = nla_strip_eval_panel_poll;
  BLI_addtail(&art->paneltypes, pt);
}
