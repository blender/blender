/* SPDX-FileCopyrightText: 2009 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spnla
 */

#include <cfloat>
#include <cstring>

#include "DNA_anim_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_string_utf8.h"

#include "BLT_translation.hh"

#include "BKE_context.hh"
#include "BKE_fcurve.hh"
#include "BKE_nla.hh"
#include "BKE_screen.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "ANIM_action.hh"
#include "ANIM_action_legacy.hh"

#include "ED_anim_api.hh"

#include "UI_interface_c.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "nla_intern.hh" /* own include */

using namespace blender;

/* ******************* nla editor space & buttons ************** */

/* -------------- */

static void do_nla_region_buttons(bContext *C, void * /*arg*/, int /*event*/)
{
  // Scene *scene = CTX_data_scene(C);
#if 0
  switch (event) {
    /* pass */
  }
#endif
  /* default for now */
  WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, nullptr);
  WM_event_add_notifier(C, NC_SCENE | ND_TRANSFORM, nullptr);
}

bool nla_panel_context(const bContext *C,
                       PointerRNA *adt_ptr,
                       PointerRNA *nlt_ptr,
                       PointerRNA *strip_ptr)
{
  bAnimContext ac;
  ListBase anim_data = {nullptr, nullptr};
  short found = 0; /* not bool, since we need to indicate "found but not ideal" status */

  /* For now, only draw if we could init the anim-context info
   * (necessary for all animation-related tools)
   * to work correctly is able to be correctly retrieved. There's no point showing empty panels? */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return false;
  }

  /* extract list of active channel(s), of which we should only take the first one
   * - we need the channels flag to get the active AnimData block when there are no NLA Tracks
   */
  /* XXX: double-check active! */
  eAnimFilter_Flags filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE |
                              ANIMFILTER_ACTIVE | ANIMFILTER_LIST_CHANNELS |
                              ANIMFILTER_FCURVESONLY);
  ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, eAnimCont_Types(ac.datatype));

  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    switch (ale->type) {
      case ANIMTYPE_NLATRACK: /* NLA Track - The primary data type which should get caught */
      {
        NlaTrack *nlt = static_cast<NlaTrack *>(ale->data);
        AnimData *adt = ale->adt;

        /* found it, now set the pointers */
        if (adt_ptr) {
          /* AnimData pointer */
          *adt_ptr = RNA_pointer_create_discrete(ale->id, &RNA_AnimData, adt);
        }
        if (nlt_ptr) {
          /* NLA-Track pointer */
          *nlt_ptr = RNA_pointer_create_discrete(ale->id, &RNA_NlaTrack, nlt);
        }
        if (strip_ptr) {
          /* NLA-Strip pointer */
          NlaStrip *strip = BKE_nlastrip_find_active(nlt);
          *strip_ptr = RNA_pointer_create_discrete(ale->id, &RNA_NlaStrip, strip);
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
      case ANIMTYPE_DSLIGHTPROBE: {
        /* for these channels, we only do AnimData */
        if (ale->adt && adt_ptr) {
          ID *id;

          if ((ale->data == nullptr) || (ale->type == ANIMTYPE_OBJECT)) {
            /* ale->data is not an ID block! */
            id = ale->id;
          }
          else {
            /* ale->data is always the proper ID block we need,
             * but ale->id may not be (i.e. for textures) */
            id = static_cast<ID *>(ale->data);
          }

          /* AnimData pointer */
          if (adt_ptr) {
            *adt_ptr = RNA_pointer_create_discrete(id, &RNA_AnimData, ale->adt);
          }

          /* set found status to -1, since setting to 1 would break the loop
           * and potentially skip an active NLA-Track in some cases...
           */
          found = -1;
        }
        break;
      }
      /* Don't set a pointer for NLA Actions.
       * This will break the dependency graph for the context menu.
       */
      case ANIMTYPE_NLAACTION:
        break;

      case ANIMTYPE_NONE:
      case ANIMTYPE_ANIMDATA:
      case ANIMTYPE_SPECIALDATA__UNUSED:
      case ANIMTYPE_SUMMARY:
      case ANIMTYPE_GROUP:
      case ANIMTYPE_FCURVE:
      case ANIMTYPE_NLACONTROLS:
      case ANIMTYPE_NLACURVE:
      case ANIMTYPE_FILLACT_LAYERED:
      case ANIMTYPE_ACTION_SLOT:
      case ANIMTYPE_FILLACTD:
      case ANIMTYPE_FILLDRIVERS:
      case ANIMTYPE_DSMCLIP:
      case ANIMTYPE_SHAPEKEY:
      case ANIMTYPE_GPLAYER:
      case ANIMTYPE_GREASE_PENCIL_DATABLOCK:
      case ANIMTYPE_GREASE_PENCIL_LAYER_GROUP:
      case ANIMTYPE_GREASE_PENCIL_LAYER:
      case ANIMTYPE_MASKDATABLOCK:
      case ANIMTYPE_MASKLAYER:
      case ANIMTYPE_NUM_TYPES:
        break;
    }

    if (found > 0) {
      break;
    }
  }

  /* free temp data */
  ANIM_animdata_freelist(&anim_data);

  return (found != 0);
}

bool ANIM_nla_context_track_ptr(const bContext *C, PointerRNA *r_ptr)
{
  return nla_panel_context(C, nullptr, r_ptr, nullptr);
}

bool ANIM_nla_context_strip_ptr(const bContext *C, PointerRNA *r_ptr)
{
  return nla_panel_context(C, nullptr, nullptr, r_ptr);
}

NlaTrack *ANIM_nla_context_track(const bContext *C)
{
  PointerRNA track_ptr;
  if (!ANIM_nla_context_track_ptr(C, &track_ptr)) {
    return nullptr;
  }
  return static_cast<NlaTrack *>(track_ptr.data);
}

NlaStrip *ANIM_nla_context_strip(const bContext *C)
{
  PointerRNA strip_ptr;
  if (!ANIM_nla_context_strip_ptr(C, &strip_ptr)) {
    return nullptr;
  }
  return static_cast<NlaStrip *>(strip_ptr.data);
}

#if 0
static bool nla_panel_poll(const bContext *C, PanelType *pt)
{
  return nla_panel_context(C, nullptr, nullptr);
}
#endif

static bool nla_animdata_panel_poll(const bContext *C, PanelType * /*pt*/)
{
  PointerRNA ptr;
  PointerRNA strip_ptr;
  return (nla_panel_context(C, &ptr, nullptr, &strip_ptr) && (ptr.data != nullptr) &&
          (ptr.owner_id != strip_ptr.owner_id));
}

static bool nla_strip_panel_poll(const bContext *C, PanelType * /*pt*/)
{
  PointerRNA ptr;
  return (nla_panel_context(C, nullptr, nullptr, &ptr) && (ptr.data != nullptr));
}

static bool nla_strip_actclip_panel_poll(const bContext *C, PanelType * /*pt*/)
{
  PointerRNA ptr;

  if (!nla_panel_context(C, nullptr, nullptr, &ptr)) {
    return false;
  }
  if (ptr.data == nullptr) {
    return false;
  }

  NlaStrip *strip = static_cast<NlaStrip *>(ptr.data);
  return eNlaStrip_Type(strip->type) == NLASTRIP_TYPE_CLIP;
}

static bool nla_strip_eval_panel_poll(const bContext *C, PanelType * /*pt*/)
{
  PointerRNA ptr;

  if (!nla_panel_context(C, nullptr, nullptr, &ptr)) {
    return false;
  }
  if (ptr.data == nullptr) {
    return false;
  }

  NlaStrip *strip = static_cast<NlaStrip *>(ptr.data);

  if (strip->type == NLASTRIP_TYPE_SOUND) {
    return false;
  }

  return true;
}

/* -------------- */

/* active AnimData */
static void nla_panel_animdata(const bContext *C, Panel *panel)
{
  PointerRNA adt_ptr;
  PointerRNA strip_ptr;
  // AnimData *adt;
  uiLayout *layout = panel->layout;
  uiLayout *row;
  uiBlock *block;

  /* check context and also validity of pointer */
  if (!nla_panel_context(C, &adt_ptr, nullptr, &strip_ptr)) {
    return;
  }

  if (adt_ptr.owner_id == strip_ptr.owner_id) {
    return;
  }

  // adt = adt_ptr.data;

  block = layout->block();
  UI_block_func_handle_set(block, do_nla_region_buttons, nullptr);
  layout->use_property_split_set(true);
  layout->use_property_decorate_set(false);

  /* AnimData Source Properties ----------------------------------- */

  /* icon + id-block name of block where AnimData came from to prevent
   * accidentally changing the properties of the wrong action
   */
  if (adt_ptr.owner_id) {
    ID *id = adt_ptr.owner_id;
    PointerRNA id_ptr = RNA_id_pointer_create(id);

    /* ID-block name > AnimData */
    row = &layout->row(true);
    row->alignment_set(blender::ui::LayoutAlign::Left);

    row->label(id->name + 2, RNA_struct_ui_icon(id_ptr.type)); /* id-block (src) */
    row->label("", ICON_RIGHTARROW);                           /* expander */
    row->label(IFACE_("Animation Data"), ICON_ANIM_DATA);      /* animdata */

    layout->separator();
  }

  /* Active Action Properties ------------------------------------- */
  /* action */
  uiLayout *col = &layout->column(true);
  uiTemplateID(col, C, &adt_ptr, "action", "ACTION_OT_new", nullptr, "NLA_OT_action_unlink");
  uiTemplateSearch(col,
                   C,
                   &adt_ptr,
                   "action_slot",
                   &adt_ptr,
                   "action_suitable_slots",
                   nullptr,
                   nullptr,
                   IFACE_("Slot"));

  /* extrapolation */
  row = &layout->row(true);
  row->prop(&adt_ptr, "action_extrapolation", UI_ITEM_NONE, IFACE_("Extrapolation"), ICON_NONE);

  /* blending */
  row = &layout->row(true);
  row->prop(&adt_ptr, "action_blend_type", UI_ITEM_NONE, IFACE_("Blending"), ICON_NONE);

  /* influence */
  row = &layout->row(true);
  row->prop(&adt_ptr, "action_influence", UI_ITEM_NONE, IFACE_("Influence"), ICON_NONE);
}

/* generic settings for active NLA-Strip */
static void nla_panel_stripname(const bContext *C, Panel *panel)
{
  PointerRNA strip_ptr;
  uiLayout *layout = panel->layout;
  uiLayout *row;
  uiBlock *block;

  if (!nla_panel_context(C, nullptr, nullptr, &strip_ptr)) {
    return;
  }

  block = layout->block();
  UI_block_func_handle_set(block, do_nla_region_buttons, nullptr);

  /* Strip Properties ------------------------------------- */
  /* strip type */
  row = &layout->row(false);
  if (RNA_enum_get(&strip_ptr, "type") == NLASTRIP_TYPE_CLIP) {
    row->label("", ICON_ANIM);
  }
  else if (RNA_enum_get(&strip_ptr, "type") == NLASTRIP_TYPE_TRANSITION) {
    row->label("", ICON_ARROW_LEFTRIGHT);
  }
  else if (RNA_enum_get(&strip_ptr, "type") == NLASTRIP_TYPE_META) {
    row->label("", ICON_SEQ_STRIP_META);
  }
  else if (RNA_enum_get(&strip_ptr, "type") == NLASTRIP_TYPE_SOUND) {
    row->label("", ICON_SOUND);
  }

  row->prop(&strip_ptr, "name", UI_ITEM_NONE, "", ICON_NLA);

  UI_block_emboss_set(block, blender::ui::EmbossType::NoneOrStatus);
  row->prop(&strip_ptr, "mute", UI_ITEM_NONE, "", ICON_NONE);
  UI_block_emboss_set(block, blender::ui::EmbossType::Emboss);
}

/* generic settings for active NLA-Strip */
static void nla_panel_properties(const bContext *C, Panel *panel)
{
  PointerRNA strip_ptr;
  uiLayout *layout = panel->layout;
  uiLayout *column, *row;
  uiBlock *block;
  short showEvalProps = 1;

  if (!nla_panel_context(C, nullptr, nullptr, &strip_ptr)) {
    return;
  }

  block = layout->block();
  UI_block_func_handle_set(block, do_nla_region_buttons, nullptr);

  /* Strip Properties ------------------------------------- */
  /* strip type */

  layout->use_property_split_set(true);
  layout->use_property_decorate_set(false);

  /* strip extents */
  column = &layout->column(true);
  column->prop(&strip_ptr, "frame_start_ui", UI_ITEM_NONE, IFACE_("Frame Start"), ICON_NONE);
  column->prop(&strip_ptr, "frame_end_ui", UI_ITEM_NONE, IFACE_("End"), ICON_NONE);

  /* Evaluation-Related Strip Properties ------------------ */

  /* sound properties strips don't have these settings */
  if (RNA_enum_get(&strip_ptr, "type") == NLASTRIP_TYPE_SOUND) {
    showEvalProps = 0;
  }

  /* only show if allowed to... */
  if (showEvalProps) {
    /* extrapolation */
    column = &layout->column(false);
    column->prop(&strip_ptr, "extrapolation", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    column->prop(&strip_ptr, "blend_type", UI_ITEM_NONE, std::nullopt, ICON_NONE);

    /* Blend in/out + auto-blending:
     * - blend in/out can only be set when auto-blending is off.
     */

    layout->separator();

    column = &layout->column(true);
    column->active_set(RNA_boolean_get(&strip_ptr, "use_auto_blend") == false);
    column->prop(&strip_ptr, "blend_in", UI_ITEM_NONE, IFACE_("Blend In"), ICON_NONE);
    column->prop(&strip_ptr, "blend_out", UI_ITEM_NONE, IFACE_("Out"), ICON_NONE);

    row = &column->row(true);
    row->active_set(RNA_boolean_get(&strip_ptr, "use_animated_influence") == false);
    row->prop(
        &strip_ptr, "use_auto_blend", UI_ITEM_NONE, std::nullopt, ICON_NONE); /* XXX as toggle? */

    /* settings */
    column = &layout->column(true, IFACE_("Playback"));
    row = &column->row(true);
    row->active_set(!(RNA_boolean_get(&strip_ptr, "use_animated_influence") ||
                      RNA_boolean_get(&strip_ptr, "use_animated_time")));
    row->prop(&strip_ptr, "use_reverse", UI_ITEM_NONE, std::nullopt, ICON_NONE);

    column->prop(&strip_ptr, "use_animated_time_cyclic", UI_ITEM_NONE, std::nullopt, ICON_NONE);
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
  if (!nla_panel_context(C, nullptr, nullptr, &strip_ptr)) {
    return;
  }

  block = layout->block();
  UI_block_func_handle_set(block, do_nla_region_buttons, nullptr);
  layout->use_property_split_set(true);
  layout->use_property_decorate_set(true);

  /* Strip Properties ------------------------------------- */
  /* action pointer */
  column = &layout->column(true);
  column->prop(&strip_ptr, "action", UI_ITEM_NONE, std::nullopt, ICON_ACTION);

  NlaStrip *strip = static_cast<NlaStrip *>(strip_ptr.data);
  if (strip->act) {
    BLI_assert(strip_ptr.owner_id);

    animrig::Action &action = strip->act->wrap();
    ID &animated_id = *strip_ptr.owner_id;
    if (!blender::animrig::legacy::action_treat_as_legacy(action)) {
      PointerRNA animated_id_ptr = RNA_id_pointer_create(&animated_id);
      column->context_ptr_set("animated_id", &animated_id_ptr);
      column->context_ptr_set("nla_strip", &strip_ptr);
      uiTemplateSearch(column,
                       C,
                       &strip_ptr,
                       "action_slot",
                       &strip_ptr,
                       "action_suitable_slots",
                       nullptr,
                       "anim.slot_unassign_from_nla_strip",
                       "Slot");
    }
  }

  /* action extents */
  column = &layout->column(true);
  column->prop(&strip_ptr, "action_frame_start", UI_ITEM_NONE, IFACE_("Frame Start"), ICON_NONE);
  column->prop(&strip_ptr, "action_frame_end", UI_ITEM_NONE, IFACE_("End"), ICON_NONE);

  row = &layout->row(false, IFACE_("Sync Length"));
  row->prop(&strip_ptr, "use_sync_length", UI_ITEM_NONE, "", ICON_NONE);
  row->op("NLA_OT_action_sync_length", IFACE_("Now"), ICON_FILE_REFRESH);

  /* action usage */
  column = &layout->column(true);
  column->active_set(RNA_boolean_get(&strip_ptr, "use_animated_time") == false);
  column->prop(&strip_ptr, "scale", UI_ITEM_NONE, IFACE_("Playback Scale"), ICON_NONE);
  column->prop(&strip_ptr, "repeat", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

/* evaluation settings for active NLA-Strip */
static void nla_panel_animated_influence_header(const bContext *C, Panel *panel)
{
  PointerRNA strip_ptr;
  uiLayout *layout = panel->layout;
  uiLayout *col;
  uiBlock *block;

  /* check context and also validity of pointer */
  if (!nla_panel_context(C, nullptr, nullptr, &strip_ptr)) {
    return;
  }

  block = layout->block();
  UI_block_func_handle_set(block, do_nla_region_buttons, nullptr);

  col = &layout->column(true);
  col->prop(&strip_ptr, "use_animated_influence", UI_ITEM_NONE, "", ICON_NONE);
}

/* evaluation settings for active NLA-Strip */
static void nla_panel_evaluation(const bContext *C, Panel *panel)
{
  PointerRNA strip_ptr;
  uiLayout *layout = panel->layout;
  uiBlock *block;

  /* check context and also validity of pointer */
  if (!nla_panel_context(C, nullptr, nullptr, &strip_ptr)) {
    return;
  }

  block = layout->block();
  UI_block_func_handle_set(block, do_nla_region_buttons, nullptr);
  layout->use_property_split_set(true);

  layout->enabled_set(RNA_boolean_get(&strip_ptr, "use_animated_influence"));
  layout->prop(&strip_ptr, "influence", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

static void nla_panel_animated_strip_time_header(const bContext *C, Panel *panel)
{
  PointerRNA strip_ptr;
  uiLayout *layout = panel->layout;
  uiLayout *col;
  uiBlock *block;

  /* check context and also validity of pointer */
  if (!nla_panel_context(C, nullptr, nullptr, &strip_ptr)) {
    return;
  }

  block = layout->block();
  UI_block_func_handle_set(block, do_nla_region_buttons, nullptr);

  col = &layout->column(true);
  col->prop(&strip_ptr, "use_animated_time", UI_ITEM_NONE, "", ICON_NONE);
}

static void nla_panel_animated_strip_time(const bContext *C, Panel *panel)
{
  PointerRNA strip_ptr;
  uiLayout *layout = panel->layout;
  uiBlock *block;

  /* check context and also validity of pointer */
  if (!nla_panel_context(C, nullptr, nullptr, &strip_ptr)) {
    return;
  }

  block = layout->block();
  UI_block_func_handle_set(block, do_nla_region_buttons, nullptr);
  layout->use_property_split_set(true);

  layout->enabled_set(RNA_boolean_get(&strip_ptr, "use_animated_time"));
  layout->prop(&strip_ptr, "strip_time", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

#define NLA_FMODIFIER_PANEL_PREFIX "NLA"

static void nla_fmodifier_panel_id(void *fcm_link, char *r_name)
{
  FModifier *fcm = static_cast<FModifier *>(fcm_link);
  eFModifier_Types type = eFModifier_Types(fcm->type);
  const FModifierTypeInfo *fmi = get_fmodifier_typeinfo(type);
  BLI_snprintf_utf8(r_name, BKE_ST_MAXNAME, "%s_PT_%s", NLA_FMODIFIER_PANEL_PREFIX, fmi->name);
}

/* F-Modifiers for active NLA-Strip */
static void nla_panel_modifiers(const bContext *C, Panel *panel)
{
  PointerRNA strip_ptr;
  uiLayout *row;
  uiBlock *block;

  /* check context and also validity of pointer */
  if (!nla_panel_context(C, nullptr, nullptr, &strip_ptr)) {
    return;
  }
  NlaStrip *strip = static_cast<NlaStrip *>(strip_ptr.data);

  block = panel->layout->block();
  UI_block_func_handle_set(block, do_nla_region_buttons, nullptr);

  /* 'add modifier' button at top of panel */
  {
    row = &panel->layout->row(false);
    block = row->block();

    /* FIXME: we need to set the only-active property so that this
     * will only add modifiers for the active strip (not all selected). */
    row->op_menu_enum(C, "NLA_OT_fmodifier_add", "type", IFACE_("Add Modifier"), ICON_NONE);

    /* copy/paste (as sub-row) */
    row = &row->row(true);
    row->op("NLA_OT_fmodifier_copy", "", ICON_COPYDOWN);
    row->op("NLA_OT_fmodifier_paste", "", ICON_PASTEDOWN);
  }

  ANIM_fmodifier_panels(C, strip_ptr.owner_id, &strip->modifiers, nla_fmodifier_panel_id);
}

/* ******************* general ******************************** */

void nla_buttons_register(ARegionType *art)
{
  PanelType *pt;

  pt = MEM_callocN<PanelType>("spacetype nla panel animdata");
  STRNCPY_UTF8(pt->idname, "NLA_PT_animdata");
  STRNCPY_UTF8(pt->label, N_("Animation Data"));
  STRNCPY_UTF8(pt->category, "Edited Action");
  STRNCPY_UTF8(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  pt->flag = PANEL_TYPE_NO_HEADER;
  pt->draw = nla_panel_animdata;
  pt->poll = nla_animdata_panel_poll;
  BLI_addtail(&art->paneltypes, pt);

  pt = MEM_callocN<PanelType>("spacetype nla panel properties");
  STRNCPY_UTF8(pt->idname, "NLA_PT_stripname");
  STRNCPY_UTF8(pt->label, N_("Active Strip Name"));
  STRNCPY_UTF8(pt->category, "Strip");
  STRNCPY_UTF8(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  pt->flag = PANEL_TYPE_NO_HEADER;
  pt->draw = nla_panel_stripname;
  pt->poll = nla_strip_panel_poll;
  BLI_addtail(&art->paneltypes, pt);

  PanelType *pt_properties = pt = MEM_callocN<PanelType>("spacetype nla panel properties");
  STRNCPY_UTF8(pt->idname, "NLA_PT_properties");
  STRNCPY_UTF8(pt->label, N_("Active Strip"));
  STRNCPY_UTF8(pt->category, "Strip");
  STRNCPY_UTF8(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  pt->draw = nla_panel_properties;
  pt->poll = nla_strip_panel_poll;
  BLI_addtail(&art->paneltypes, pt);

  pt = MEM_callocN<PanelType>("spacetype nla panel properties");
  STRNCPY_UTF8(pt->idname, "NLA_PT_actionclip");
  STRNCPY_UTF8(pt->label, N_("Action Clip"));
  STRNCPY_UTF8(pt->category, "Strip");
  STRNCPY_UTF8(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  pt->draw = nla_panel_actclip;
  pt->flag = PANEL_TYPE_DEFAULT_CLOSED;
  pt->poll = nla_strip_actclip_panel_poll;
  BLI_addtail(&art->paneltypes, pt);

  pt = MEM_callocN<PanelType>("spacetype nla panel evaluation");
  STRNCPY_UTF8(pt->idname, "NLA_PT_evaluation");
  STRNCPY_UTF8(pt->parent_id, "NLA_PT_properties");
  STRNCPY_UTF8(pt->label, N_("Animated Influence"));
  STRNCPY_UTF8(pt->category, "Strip");
  STRNCPY_UTF8(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  pt->draw = nla_panel_evaluation;
  pt->draw_header = nla_panel_animated_influence_header;
  pt->parent = pt_properties;
  pt->flag = PANEL_TYPE_DEFAULT_CLOSED;
  pt->poll = nla_strip_eval_panel_poll;
  BLI_addtail(&pt_properties->children, BLI_genericNodeN(pt));
  BLI_addtail(&art->paneltypes, pt);

  pt = MEM_callocN<PanelType>("spacetype nla panel animated strip time");
  STRNCPY_UTF8(pt->idname, "NLA_PT_animated_strip_time");
  STRNCPY_UTF8(pt->parent_id, "NLA_PT_properties");
  STRNCPY_UTF8(pt->label, N_("Animated Strip Time"));
  STRNCPY_UTF8(pt->category, "Strip");
  STRNCPY_UTF8(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  pt->draw = nla_panel_animated_strip_time;
  pt->draw_header = nla_panel_animated_strip_time_header;
  pt->parent = pt_properties;
  pt->flag = PANEL_TYPE_DEFAULT_CLOSED;
  pt->poll = nla_strip_eval_panel_poll;
  BLI_addtail(&pt_properties->children, BLI_genericNodeN(pt));
  BLI_addtail(&art->paneltypes, pt);

  pt = MEM_callocN<PanelType>("spacetype nla panel modifiers");
  STRNCPY_UTF8(pt->idname, "NLA_PT_modifiers");
  STRNCPY_UTF8(pt->label, N_("Modifiers"));
  STRNCPY_UTF8(pt->category, "Modifiers");
  STRNCPY_UTF8(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  pt->draw = nla_panel_modifiers;
  pt->poll = nla_strip_eval_panel_poll;
  pt->flag = PANEL_TYPE_NO_HEADER;
  BLI_addtail(&art->paneltypes, pt);

  ANIM_modifier_panels_register_graph_and_NLA(
      art, NLA_FMODIFIER_PANEL_PREFIX, nla_strip_eval_panel_poll);
}
