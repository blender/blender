/* SPDX-FileCopyrightText: 2012-2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "BLI_array.hh"
#include "BLI_hash.hh"
#include "BLI_listbase.h"
#include "BLI_rand.hh"
#include "BLI_set.hh"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.hh"
#include "BLI_task.hh"

#include "BLT_translation.hh"

#include "DNA_mask_types.h"
#include "DNA_sequence_types.h"
#include "DNA_space_types.h"

#include "BKE_colortools.hh"
#include "BKE_screen.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "SEQ_modifier.hh"
#include "SEQ_modifiertypes.hh"
#include "SEQ_render.hh"
#include "SEQ_select.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_time.hh"
#include "SEQ_utils.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"

#include "BLO_read_write.hh"

#include "WM_api.hh"

#include "modifier.hh"
#include "render.hh"

namespace blender::seq {

/* -------------------------------------------------------------------- */

static bool modifier_has_persistent_uid(const Strip &strip, int uid)
{
  LISTBASE_FOREACH (StripModifierData *, smd, &strip.modifiers) {
    if (smd->persistent_uid == uid) {
      return true;
    }
  }
  return false;
}

void modifier_persistent_uid_init(const Strip &strip, StripModifierData &smd)
{
  uint64_t hash = get_default_hash(StringRef(smd.name));
  RandomNumberGenerator rng{uint32_t(hash)};
  while (true) {
    const int new_uid = rng.get_int32();
    if (new_uid <= 0) {
      continue;
    }
    if (modifier_has_persistent_uid(strip, new_uid)) {
      continue;
    }
    smd.persistent_uid = new_uid;
    break;
  }
}

bool modifier_persistent_uids_are_valid(const Strip &strip)
{
  Set<int> uids;
  int modifiers_num = 0;
  LISTBASE_FOREACH (StripModifierData *, smd, &strip.modifiers) {
    if (smd->persistent_uid <= 0) {
      return false;
    }
    uids.add(smd->persistent_uid);
    modifiers_num++;
  }
  if (uids.size() != modifiers_num) {
    return false;
  }
  return true;
}

static void modifier_panel_header(const bContext * /*C*/, Panel *panel)
{
  uiLayout *row, *sub, *name_row;
  uiLayout *layout = panel->layout;

  /* Don't use #modifier_panel_get_property_pointers, we don't want to lock the header. */
  PointerRNA *ptr = UI_panel_custom_data_get(panel);
  StripModifierData *smd = reinterpret_cast<StripModifierData *>(ptr->data);

  UI_panel_context_pointer_set(panel, "modifier", ptr);

  /* Modifier Icon. */
  sub = &layout->row(true);
  sub->emboss_set(ui::EmbossType::None);
  PointerRNA active_op_ptr = sub->op(
      "SEQUENCER_OT_strip_modifier_set_active", "", RNA_struct_ui_icon(ptr->type));
  RNA_string_set(&active_op_ptr, "modifier", smd->name);

  row = &layout->row(true);

  /* Modifier Name.
   * Count how many buttons are added to the header to check if there is enough space. */
  int buttons_number = 0;
  name_row = &row->row(true);

  sub = &row->row(true);
  sub->prop(ptr, "enable", UI_ITEM_NONE, "", ICON_NONE);
  buttons_number++;

  /* Delete button. */
  sub = &row->row(false);
  sub->emboss_set(ui::EmbossType::None);
  PointerRNA remove_op_ptr = sub->op("SEQUENCER_OT_strip_modifier_remove", "", ICON_X);
  RNA_string_set(&remove_op_ptr, "name", smd->name);
  buttons_number++;

  bool display_name = (panel->sizex / UI_UNIT_X - buttons_number > 5) || (panel->sizex == 0);
  if (display_name) {
    name_row->prop(ptr, "name", UI_ITEM_NONE, "", ICON_NONE);
  }
  else {
    row->alignment_set(ui::LayoutAlign::Right);
  }

  /* Extra padding for delete button. */
  layout->separator();
}

void draw_mask_input_type_settings(const bContext *C, uiLayout *layout, PointerRNA *ptr)
{
  Scene *sequencer_scene = CTX_data_sequencer_scene(C);
  Editing *ed = seq::editing_get(sequencer_scene);
  uiLayout *row, *col;

  const int input_mask_type = RNA_enum_get(ptr, "input_mask_type");

  layout->use_property_split_set(true);

  col = &layout->column(false);
  row = &col->row(true);
  row->prop(ptr, "input_mask_type", UI_ITEM_R_EXPAND, IFACE_("Type"), ICON_NONE);

  if (input_mask_type == STRIP_MASK_INPUT_STRIP) {
    PointerRNA sequences_object = RNA_pointer_create_discrete(
        &sequencer_scene->id, &RNA_SequenceEditor, ed);
    col->prop_search(
        ptr, "input_mask_strip", &sequences_object, "strips_all", IFACE_("Mask"), ICON_NONE);
  }
  else {
    col->prop(ptr, "input_mask_id", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    row = &col->row(true);
    row->prop(ptr, "mask_time", UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);
  }
}

bool modifier_ui_poll(const bContext *C, PanelType * /*pt*/)
{
  Scene *sequencer_scene = CTX_data_sequencer_scene(C);
  if (!sequencer_scene) {
    return false;
  }
  Strip *active_strip = seq::select_active_get(sequencer_scene);
  return active_strip != nullptr;
}

/**
 * Move a modifier to the index it's moved to after a drag and drop.
 */
static void modifier_reorder(bContext *C, Panel *panel, const int new_index)
{
  PointerRNA *smd_ptr = UI_panel_custom_data_get(panel);
  StripModifierData *smd = reinterpret_cast<StripModifierData *>(smd_ptr->data);

  PointerRNA props_ptr;
  wmOperatorType *ot = WM_operatortype_find("SEQUENCER_OT_strip_modifier_move_to_index", false);
  WM_operator_properties_create_ptr(&props_ptr, ot);
  RNA_string_set(&props_ptr, "modifier", smd->name);
  RNA_int_set(&props_ptr, "index", new_index);
  WM_operator_name_call_ptr(C, ot, wm::OpCallContext::InvokeDefault, &props_ptr, nullptr);
  WM_operator_properties_free(&props_ptr);
}

static short get_strip_modifier_expand_flag(const bContext * /*C*/, Panel *panel)
{
  PointerRNA *smd_ptr = UI_panel_custom_data_get(panel);
  StripModifierData *smd = reinterpret_cast<StripModifierData *>(smd_ptr->data);
  return smd->ui_expand_flag;
}

static void set_strip_modifier_expand_flag(const bContext * /*C*/, Panel *panel, short expand_flag)
{
  PointerRNA *smd_ptr = UI_panel_custom_data_get(panel);
  StripModifierData *smd = reinterpret_cast<StripModifierData *>(smd_ptr->data);
  smd->ui_expand_flag = expand_flag;
}

PanelType *modifier_panel_register(ARegionType *region_type,
                                   const eStripModifierType type,
                                   PanelDrawFn draw)
{
  PanelType *panel_type = MEM_callocN<PanelType>(__func__);

  modifier_type_panel_id(type, panel_type->idname);
  STRNCPY_UTF8(panel_type->label, "");
  STRNCPY_UTF8(panel_type->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  STRNCPY_UTF8(panel_type->active_property, "is_active");
  STRNCPY_UTF8(panel_type->context, "strip_modifier");

  panel_type->draw_header = modifier_panel_header;
  panel_type->draw = draw;
  panel_type->poll = modifier_ui_poll;

  /* Give the panel the special flag that says it was built here and corresponds to a
   * modifier rather than a #PanelType. */
  panel_type->flag = PANEL_TYPE_HEADER_EXPAND | PANEL_TYPE_INSTANCED;
  panel_type->reorder = modifier_reorder;
  panel_type->get_list_data_expand_flag = get_strip_modifier_expand_flag;
  panel_type->set_list_data_expand_flag = set_strip_modifier_expand_flag;

  BLI_addtail(&region_type->paneltypes, panel_type);

  return panel_type;
}

/* -------------------------------------------------------------------- */

float4 load_pixel_premul(const uchar *ptr)
{
  float4 res;
  straight_uchar_to_premul_float(res, ptr);
  return res;
}

float4 load_pixel_premul(const float *ptr)
{
  return float4(ptr);
}

void store_pixel_premul(float4 pix, uchar *ptr)
{
  premul_float_to_straight_uchar(ptr, pix);
}

void store_pixel_premul(float4 pix, float *ptr)
{
  *reinterpret_cast<float4 *>(ptr) = pix;
}

float4 load_pixel_raw(const uchar *ptr)
{
  float4 res;
  rgba_uchar_to_float(res, ptr);
  return res;
}

float4 load_pixel_raw(const float *ptr)
{
  return float4(ptr);
}

void store_pixel_raw(float4 pix, uchar *ptr)
{
  rgba_float_to_uchar(ptr, pix);
}

void store_pixel_raw(float4 pix, float *ptr)
{
  *reinterpret_cast<float4 *>(ptr) = pix;
}

/**
 * \a timeline_frame is offset by \a fra_offset only in case we are using a real mask.
 */
static ImBuf *modifier_render_mask_input(const RenderData &context,
                                         SeqRenderState &state,
                                         int mask_input_type,
                                         Strip *mask_strip,
                                         Mask *mask_id,
                                         int timeline_frame,
                                         int fra_offset)
{
  ImBuf *mask_input = nullptr;

  if (mask_input_type == STRIP_MASK_INPUT_STRIP) {
    if (mask_strip) {
      mask_input = seq_render_strip(&context, &state, mask_strip, timeline_frame);
    }
  }
  else if (mask_input_type == STRIP_MASK_INPUT_ID) {
    /* Note that we do not request mask to be float image: if it is that is
     * fine, but if it is a byte image then we also just take that without
     * extra memory allocations or conversions. All modifiers are expected
     * to handle mask being either type. */
    mask_input = seq_render_mask(context.depsgraph,
                                 context.rectx,
                                 context.recty,
                                 mask_id,
                                 timeline_frame - fra_offset,
                                 false);
  }

  return mask_input;
}

/* -------------------------------------------------------------------- */
/** \name Public Modifier Functions
 * \{ */

static StripModifierTypeInfo *modifiersTypes[NUM_STRIP_MODIFIER_TYPES] = {nullptr};

static void modifier_types_init(StripModifierTypeInfo *types[])
{
#define INIT_TYPE(typeName) (types[eSeqModifierType_##typeName] = &seqModifierType_##typeName)
  INIT_TYPE(None);
  INIT_TYPE(BrightContrast);
  INIT_TYPE(ColorBalance);
  INIT_TYPE(Compositor);
  INIT_TYPE(Curves);
  INIT_TYPE(HueCorrect);
  INIT_TYPE(Mask);
  INIT_TYPE(SoundEqualizer);
  INIT_TYPE(Tonemap);
  INIT_TYPE(WhiteBalance);
#undef INIT_TYPE
}

void modifiers_init()
{
  modifier_types_init(modifiersTypes);
}

const StripModifierTypeInfo *modifier_type_info_get(int type)
{
  if (type <= 0 || type >= NUM_STRIP_MODIFIER_TYPES) {
    return nullptr;
  }
  return modifiersTypes[type];
}

StripModifierData *modifier_new(Strip *strip, const char *name, int type)
{
  StripModifierData *smd;
  const StripModifierTypeInfo *smti = modifier_type_info_get(type);

  smd = static_cast<StripModifierData *>(MEM_callocN(smti->struct_size, "sequence modifier"));

  smd->type = type;
  smd->flag |= STRIP_MODIFIER_FLAG_EXPANDED;
  smd->ui_expand_flag |= UI_PANEL_DATA_EXPAND_ROOT;

  if (!name || !name[0]) {
    STRNCPY_UTF8(smd->name, CTX_DATA_(BLT_I18NCONTEXT_ID_SEQUENCE, smti->name));
  }
  else {
    STRNCPY_UTF8(smd->name, name);
  }

  BLI_addtail(&strip->modifiers, smd);

  modifier_unique_name(strip, smd);

  if (smti->init_data) {
    smti->init_data(smd);
  }

  modifier_set_active(strip, smd);

  return smd;
}

bool modifier_remove(Strip *strip, StripModifierData *smd)
{
  if (BLI_findindex(&strip->modifiers, smd) == -1) {
    return false;
  }

  BLI_remlink(&strip->modifiers, smd);
  modifier_free(smd);

  return true;
}

void modifier_clear(Strip *strip)
{
  StripModifierData *smd, *smd_next;

  for (smd = static_cast<StripModifierData *>(strip->modifiers.first); smd; smd = smd_next) {
    smd_next = smd->next;
    modifier_free(smd);
  }

  BLI_listbase_clear(&strip->modifiers);
}

void modifier_free(StripModifierData *smd)
{
  const StripModifierTypeInfo *smti = modifier_type_info_get(smd->type);

  if (smti && smti->free_data) {
    smti->free_data(smd);
  }

  MEM_freeN(smd);
}

void modifier_unique_name(Strip *strip, StripModifierData *smd)
{
  const StripModifierTypeInfo *smti = modifier_type_info_get(smd->type);

  BLI_uniquename(&strip->modifiers,
                 smd,
                 CTX_DATA_(BLT_I18NCONTEXT_ID_SEQUENCE, smti->name),
                 '.',
                 offsetof(StripModifierData, name),
                 sizeof(smd->name));
}

StripModifierData *modifier_find_by_name(Strip *strip, const char *name)
{
  return static_cast<StripModifierData *>(
      BLI_findstring(&(strip->modifiers), name, offsetof(StripModifierData, name)));
}

static bool skip_modifier(Scene *scene, const StripModifierData *smd, int timeline_frame)
{
  using namespace blender::seq;

  if (smd->mask_strip == nullptr) {
    return false;
  }
  const bool strip_has_ended_skip = smd->mask_input_type == STRIP_MASK_INPUT_STRIP &&
                                    smd->mask_time == STRIP_MASK_TIME_RELATIVE &&
                                    !time_strip_intersects_frame(
                                        scene, smd->mask_strip, timeline_frame);
  const bool missing_data_skip = !strip_has_valid_data(smd->mask_strip) ||
                                 media_presence_is_missing(scene, smd->mask_strip);

  return strip_has_ended_skip || missing_data_skip;
}

void modifier_apply_stack(ModifierApplyContext &context, int timeline_frame)
{
  if (context.strip.modifiers.first == nullptr) {
    return;
  }

  if (context.strip.flag & SEQ_USE_LINEAR_MODIFIERS) {
    render_imbuf_from_sequencer_space(context.render_data.scene, context.image);
  }

  LISTBASE_FOREACH (StripModifierData *, smd, &context.strip.modifiers) {
    const StripModifierTypeInfo *smti = modifier_type_info_get(smd->type);

    /* could happen if modifier is being removed or not exists in current version of blender */
    if (!smti) {
      continue;
    }

    /* modifier is muted, do nothing */
    if (smd->flag & STRIP_MODIFIER_FLAG_MUTE) {
      continue;
    }

    if (smti->apply && !skip_modifier(context.render_data.scene, smd, timeline_frame)) {
      int frame_offset;
      if (smd->mask_time == STRIP_MASK_TIME_RELATIVE) {
        frame_offset = context.strip.start;
      }
      else /* if (smd->mask_time == STRIP_MASK_TIME_ABSOLUTE) */ {
        frame_offset = smd->mask_id ? ((Mask *)smd->mask_id)->sfra : 0;
      }

      ImBuf *mask = modifier_render_mask_input(context.render_data,
                                               context.render_state,
                                               smd->mask_input_type,
                                               smd->mask_strip,
                                               smd->mask_id,
                                               timeline_frame,
                                               frame_offset);
      smti->apply(context, smd, mask);
      if (mask) {
        IMB_freeImBuf(mask);
      }
    }
  }

  if (context.strip.flag & SEQ_USE_LINEAR_MODIFIERS) {
    seq_imbuf_to_sequencer_space(context.render_data.scene, context.image, false);
  }
}

StripModifierData *modifier_copy(Strip &strip_dst, StripModifierData *mod_src)
{
  const StripModifierTypeInfo *smti = modifier_type_info_get(mod_src->type);
  StripModifierData *mod_new = static_cast<StripModifierData *>(MEM_dupallocN(mod_src));

  if (smti && smti->copy_data) {
    smti->copy_data(mod_new, mod_src);
  }

  BLI_addtail(&strip_dst.modifiers, mod_new);
  BLI_uniquename(&strip_dst.modifiers,
                 mod_new,
                 "Strip Modifier",
                 '.',
                 offsetof(StripModifierData, name),
                 sizeof(StripModifierData::name));
  return mod_new;
}

void modifier_list_copy(Strip *strip_new, Strip *strip)
{
  LISTBASE_FOREACH (StripModifierData *, smd, &strip->modifiers) {
    modifier_copy(*strip_new, smd);
  }
}

int sequence_supports_modifiers(Strip *strip)
{
  return (strip->type != STRIP_TYPE_SOUND_RAM);
}

bool modifier_move_to_index(Strip *strip, StripModifierData *smd, const int new_index)
{
  const int current_index = BLI_findindex(&strip->modifiers, smd);
  return BLI_listbase_move_index(&strip->modifiers, current_index, new_index);
}

StripModifierData *modifier_get_active(const Strip *strip)
{
  /* In debug mode, check for only one active modifier. */
#ifndef NDEBUG
  int active_count = 0;
  LISTBASE_FOREACH (StripModifierData *, smd, &strip->modifiers) {
    if (smd->flag & STRIP_MODIFIER_FLAG_ACTIVE) {
      active_count++;
    }
  }
  BLI_assert(ELEM(active_count, 0, 1));
#endif

  LISTBASE_FOREACH (StripModifierData *, smd, &strip->modifiers) {
    if (smd->flag & STRIP_MODIFIER_FLAG_ACTIVE) {
      return smd;
    }
  }

  return nullptr;
}

void modifier_set_active(Strip *strip, StripModifierData *smd)
{
  LISTBASE_FOREACH (StripModifierData *, smd_iter, &strip->modifiers) {
    smd_iter->flag &= ~STRIP_MODIFIER_FLAG_ACTIVE;
  }

  if (smd != nullptr) {
    BLI_assert(BLI_findindex(&strip->modifiers, smd) != -1);
    smd->flag |= STRIP_MODIFIER_FLAG_ACTIVE;
  }
}

void modifier_type_panel_id(eStripModifierType type, char *r_idname)
{
  const StripModifierTypeInfo *mti = modifier_type_info_get(type);
  BLI_string_join(
      r_idname, sizeof(PanelType::idname), STRIP_MODIFIER_TYPE_PANEL_PREFIX, mti->idname);
}

void foreach_strip_modifier_id(Strip *strip, const FunctionRef<void(ID *)> fn)
{
  LISTBASE_FOREACH (StripModifierData *, smd, &strip->modifiers) {
    if (smd->mask_id) {
      fn(reinterpret_cast<ID *>(smd->mask_id));
    }
    if (smd->type == eSeqModifierType_Compositor) {
      auto *modifier_data = reinterpret_cast<SequencerCompositorModifierData *>(smd);
      if (modifier_data->node_group) {
        fn(reinterpret_cast<ID *>(modifier_data->node_group));
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name .blend File I/O
 * \{ */

void modifier_blend_write(BlendWriter *writer, ListBase *modbase)
{
  LISTBASE_FOREACH (StripModifierData *, smd, modbase) {
    const StripModifierTypeInfo *smti = modifier_type_info_get(smd->type);

    if (smti) {
      BLO_write_struct_by_name(writer, smti->struct_name, smd);
      if (smti->blend_write) {
        smti->blend_write(writer, smd);
      }
    }
    else {
      BLO_write_struct(writer, StripModifierData, smd);
    }
  }
}

void modifier_blend_read_data(BlendDataReader *reader, ListBase *lb)
{
  BLO_read_struct_list(reader, StripModifierData, lb);

  LISTBASE_FOREACH (StripModifierData *, smd, lb) {
    if (smd->mask_strip) {
      BLO_read_struct(reader, Strip, &smd->mask_strip);
    }

    const StripModifierTypeInfo *smti = modifier_type_info_get(smd->type);
    if (smti && smti->blend_read) {
      smti->blend_read(reader, smd);
    }
  }
}

/** \} */

}  // namespace blender::seq
