/* SPDX-FileCopyrightText: 2012 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spseq
 */

#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "DNA_scene_types.h"

#include "DEG_depsgraph.hh"

#include "BKE_context.hh"

#include "ED_sequencer.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "SEQ_modifier.hh"
#include "SEQ_relations.hh"
#include "SEQ_select.hh"
#include "SEQ_sound.hh"

#include "UI_interface_c.hh"

/* Own include. */
#include "sequencer_intern.hh"

namespace blender::ed::vse {

/* -------------------------------------------------------------------- */
/** \name Add modifier operator
 * \{ */

static wmOperatorStatus strip_modifier_add_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  Strip *strip = seq::select_active_get(scene);
  int type = RNA_enum_get(op->ptr, "type");

  StripModifierData *smd = seq::modifier_new(strip, nullptr, type);
  seq::modifier_persistent_uid_init(*strip, *smd);

  seq::relations_invalidate_cache(scene, strip);
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

static const EnumPropertyItem *filter_modifiers_by_sequence_type_itemf(bContext *C,
                                                                       PointerRNA * /*ptr*/,
                                                                       PropertyRNA * /*prop*/,
                                                                       bool * /*r_free*/)
{
  if (C == nullptr) {
    return rna_enum_strip_modifier_type_items;
  }

  Scene *scene = CTX_data_sequencer_scene(C);
  Strip *strip = seq::select_active_get(scene);
  if (strip) {
    if (ELEM(strip->type, STRIP_TYPE_SOUND_RAM)) {
      return rna_enum_strip_sound_modifier_type_items;
    }
  }
  return rna_enum_strip_video_modifier_type_items;
}

void SEQUENCER_OT_strip_modifier_add(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Add Strip Modifier";
  ot->idname = "SEQUENCER_OT_strip_modifier_add";
  ot->description = "Add a modifier to the strip";

  /* API callbacks. */
  ot->exec = strip_modifier_add_exec;
  ot->poll = sequencer_strip_editable_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  prop = RNA_def_enum(ot->srna, "type", rna_enum_dummy_NULL_items, 0, "Type", "");
  RNA_def_enum_funcs(prop, filter_modifiers_by_sequence_type_itemf);
  ot->prop = prop;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Remove Modifier Operator
 * \{ */

static wmOperatorStatus strip_modifier_remove_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  Strip *strip = seq::select_active_get(scene);
  char name[MAX_NAME];
  StripModifierData *smd;

  RNA_string_get(op->ptr, "name", name);

  smd = seq::modifier_find_by_name(strip, name);
  if (!smd) {
    return OPERATOR_CANCELLED;
  }

  BLI_remlink(&strip->modifiers, smd);
  seq::modifier_free(smd);

  if (ELEM(strip->type, STRIP_TYPE_SOUND_RAM)) {
    DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS | ID_RECALC_AUDIO);
  }
  else {
    seq::relations_invalidate_cache(scene, strip);
  }
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_strip_modifier_remove(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Remove Strip Modifier";
  ot->idname = "SEQUENCER_OT_strip_modifier_remove";
  ot->description = "Remove a modifier from the strip";

  /* API callbacks. */
  ot->exec = strip_modifier_remove_exec;
  ot->poll = sequencer_strip_editable_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  prop = RNA_def_string(ot->srna, "name", "Name", MAX_NAME, "Name", "Name of modifier to remove");
  RNA_def_property_flag(prop, PROP_HIDDEN);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Move Operator
 * \{ */

enum {
  SEQ_MODIFIER_MOVE_UP = 0,
  SEQ_MODIFIER_MOVE_DOWN,
};

static wmOperatorStatus strip_modifier_move_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  Strip *strip = seq::select_active_get(scene);
  char name[MAX_NAME];
  int direction;
  StripModifierData *smd;

  RNA_string_get(op->ptr, "name", name);
  direction = RNA_enum_get(op->ptr, "direction");

  smd = seq::modifier_find_by_name(strip, name);
  if (!smd) {
    return OPERATOR_CANCELLED;
  }

  if (direction == SEQ_MODIFIER_MOVE_UP) {
    if (smd->prev) {
      BLI_remlink(&strip->modifiers, smd);
      BLI_insertlinkbefore(&strip->modifiers, smd->prev, smd);
    }
  }
  else if (direction == SEQ_MODIFIER_MOVE_DOWN) {
    if (smd->next) {
      BLI_remlink(&strip->modifiers, smd);
      BLI_insertlinkafter(&strip->modifiers, smd->next, smd);
    }
  }

  if (ELEM(strip->type, STRIP_TYPE_SOUND_RAM)) {
    DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS | ID_RECALC_AUDIO);
  }
  else {
    seq::relations_invalidate_cache(scene, strip);
  }

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_strip_modifier_move(wmOperatorType *ot)
{
  PropertyRNA *prop;

  static const EnumPropertyItem direction_items[] = {
      {SEQ_MODIFIER_MOVE_UP, "UP", 0, "Up", "Move modifier up in the stack"},
      {SEQ_MODIFIER_MOVE_DOWN, "DOWN", 0, "Down", "Move modifier down in the stack"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Move Strip Modifier";
  ot->idname = "SEQUENCER_OT_strip_modifier_move";
  ot->description = "Move modifier up and down in the stack";

  /* API callbacks. */
  ot->exec = strip_modifier_move_exec;
  ot->poll = sequencer_strip_editable_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  prop = RNA_def_string(ot->srna, "name", "Name", MAX_NAME, "Name", "Name of modifier to remove");
  RNA_def_property_flag(prop, PROP_HIDDEN);
  prop = RNA_def_enum(ot->srna, "direction", direction_items, SEQ_MODIFIER_MOVE_UP, "Type", "");
  RNA_def_property_flag(prop, PROP_HIDDEN);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Copy to Selected Operator
 * \{ */

enum {
  SEQ_MODIFIER_COPY_REPLACE = 0,
  SEQ_MODIFIER_COPY_APPEND = 1,
};

static wmOperatorStatus strip_modifier_copy_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  Strip *active_strip = seq::select_active_get(scene);
  const int type = RNA_enum_get(op->ptr, "type");

  if (!active_strip || !active_strip->modifiers.first) {
    return OPERATOR_CANCELLED;
  }

  int isSound = ELEM(active_strip->type, STRIP_TYPE_SOUND_RAM);

  VectorSet<Strip *> selected = selected_strips_from_context(C);
  selected.remove(active_strip);

  for (Strip *strip_iter : selected) {
    int strip_iter_is_sound = ELEM(strip_iter->type, STRIP_TYPE_SOUND_RAM);
    /* If original is sound, only copy to "sound" strips
     * If original is not sound, only copy to "not sound" strips
     */
    if (isSound != strip_iter_is_sound) {
      continue;
    }

    if (type == SEQ_MODIFIER_COPY_REPLACE) {
      if (strip_iter->modifiers.first) {
        StripModifierData *smd_tmp,
            *smd = static_cast<StripModifierData *>(strip_iter->modifiers.first);
        while (smd) {
          smd_tmp = smd->next;
          BLI_remlink(&strip_iter->modifiers, smd);
          seq::modifier_free(smd);
          smd = smd_tmp;
        }
        BLI_listbase_clear(&strip_iter->modifiers);
      }
    }

    LISTBASE_FOREACH (StripModifierData *, smd, &active_strip->modifiers) {
      StripModifierData *smd_new = seq::modifier_copy(*strip_iter, smd);
      seq::modifier_persistent_uid_init(*strip_iter, *smd_new);
    }
  }

  if (ELEM(active_strip->type, STRIP_TYPE_SOUND_RAM)) {
    DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS | ID_RECALC_AUDIO);
  }
  else {
    seq::relations_invalidate_cache(scene, active_strip);
  }

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_strip_modifier_copy(wmOperatorType *ot)
{
  static const EnumPropertyItem type_items[] = {
      {SEQ_MODIFIER_COPY_REPLACE, "REPLACE", 0, "Replace", "Replace modifiers in destination"},
      {SEQ_MODIFIER_COPY_APPEND,
       "APPEND",
       0,
       "Append",
       "Append active modifiers to selected strips"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Copy to Selected Strips";
  ot->idname = "SEQUENCER_OT_strip_modifier_copy";
  ot->description = "Copy modifiers of the active strip to all selected strips";

  /* API callbacks. */
  ot->invoke = WM_menu_invoke;
  ot->exec = strip_modifier_copy_exec;
  ot->poll = sequencer_strip_editable_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(ot->srna, "type", type_items, SEQ_MODIFIER_COPY_REPLACE, "Type", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Redefine Equalizer Graphs Operator
 * \{ */

static wmOperatorStatus strip_modifier_equalizer_redefine_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  Strip *strip = seq::select_active_get(scene);
  StripModifierData *smd;
  char name[MAX_NAME];
  RNA_string_get(op->ptr, "name", name);
  int number = RNA_enum_get(op->ptr, "graphs");

  smd = seq::modifier_find_by_name(strip, name);
  if (!smd) {
    return OPERATOR_CANCELLED;
  }

  seq::sound_equalizermodifier_set_graphs((SoundEqualizerModifierData *)smd, number);

  seq::relations_invalidate_cache(scene, strip);
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_strip_modifier_equalizer_redefine(wmOperatorType *ot)
{

  static const EnumPropertyItem enum_modifier_equalizer_presets_items[] = {
      {1, "SIMPLE", 0, "Unique", "One unique graphical definition"},
      {2, "DOUBLE", 0, "Double", "Graphical definition in 2 sections"},
      {3, "TRIPLE", 0, "Triplet", "Graphical definition in 3 sections"},
      {0, nullptr, 0, nullptr, nullptr},
  };
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Redefine Equalizer Graphs";
  ot->idname = "SEQUENCER_OT_strip_modifier_equalizer_redefine";
  ot->description = "Redefine equalizer graphs";

  /* API callbacks. */
  ot->exec = strip_modifier_equalizer_redefine_exec;
  ot->poll = sequencer_strip_editable_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  prop = RNA_def_enum(
      ot->srna, "graphs", enum_modifier_equalizer_presets_items, 1, "Graphs", "Number of graphs");
  ot->prop = prop;
  prop = RNA_def_string(
      ot->srna, "name", "Name", MAX_NAME, "Name", "Name of modifier to redefine");
  RNA_def_property_flag(prop, PROP_HIDDEN);
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Move to Index Modifier Operator
 * \{ */

static wmOperatorStatus modifier_move_to_index_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  Strip *strip = seq::select_active_get(scene);

  char name[MAX_NAME];
  RNA_string_get(op->ptr, "modifier", name);
  const int index = RNA_int_get(op->ptr, "index");

  StripModifierData *smd = seq::modifier_find_by_name(strip, name);
  if (!smd) {
    return OPERATOR_CANCELLED;
  }

  if (!seq::modifier_move_to_index(strip, smd, index)) {
    return OPERATOR_CANCELLED;
  }

  if (ELEM(strip->type, STRIP_TYPE_SOUND_RAM)) {
    DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS | ID_RECALC_AUDIO);
  }
  else {
    seq::relations_invalidate_cache(scene, strip);
  }

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus modifier_move_to_index_invoke(bContext *C,
                                                      wmOperator *op,
                                                      const wmEvent * /*event*/)
{
  BLI_assert(RNA_struct_property_is_set(op->ptr, "modifier"));
  return modifier_move_to_index_exec(C, op);
}

void SEQUENCER_OT_strip_modifier_move_to_index(wmOperatorType *ot)
{
  PropertyRNA *prop;

  ot->name = "Move Active Strip Modifier to Index";
  ot->description =
      "Change the strip modifier's index in the stack so it evaluates after the set number of "
      "others";
  ot->idname = "SEQUENCER_OT_strip_modifier_move_to_index";

  ot->invoke = modifier_move_to_index_invoke;
  ot->exec = modifier_move_to_index_exec;
  ot->poll = sequencer_strip_editable_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;

  prop = RNA_def_string(
      ot->srna, "modifier", nullptr, MAX_NAME, "Modifier", "Name of the modifier to edit");
  RNA_def_property_flag(prop, PROP_HIDDEN);
  RNA_def_int(
      ot->srna, "index", 0, 0, INT_MAX, "Index", "The index to move the modifier to", 0, INT_MAX);
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Set Active Modifier Operator
 * \{ */

static wmOperatorStatus modifier_set_active_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  Strip *strip = seq::select_active_get(scene);

  char name[MAX_NAME];
  RNA_string_get(op->ptr, "modifier", name);

  StripModifierData *smd = seq::modifier_find_by_name(strip, name);
  /* If there is no modifier set for this operator, clear the active modifier field. */
  seq::modifier_set_active(strip, smd);

  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus modifier_set_active_invoke(bContext *C,
                                                   wmOperator *op,
                                                   const wmEvent * /*event*/)
{
  BLI_assert(RNA_struct_property_is_set(op->ptr, "modifier"));
  return modifier_set_active_exec(C, op);
}

void SEQUENCER_OT_strip_modifier_set_active(wmOperatorType *ot)
{
  ot->name = "Set Active Strip Modifier";
  ot->description = "Activate the strip modifier to use as the context";
  ot->idname = "SEQUENCER_OT_strip_modifier_set_active";

  ot->invoke = modifier_set_active_invoke;
  ot->exec = modifier_set_active_exec;
  ot->poll = sequencer_strip_editable_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;

  ot->prop = RNA_def_string(
      ot->srna, "modifier", nullptr, MAX_NAME, "Modifier", "Name of the strip modifier to edit");
  RNA_def_property_flag(ot->prop, PROP_HIDDEN);
}

/** \} */

}  // namespace blender::ed::vse
