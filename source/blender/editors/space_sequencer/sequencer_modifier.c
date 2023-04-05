/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2012 Blender Foundation */

/** \file
 * \ingroup spseq
 */

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "DNA_scene_types.h"

#include "BKE_context.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "SEQ_iterator.h"
#include "SEQ_modifier.h"
#include "SEQ_relations.h"
#include "SEQ_select.h"
#include "SEQ_sequencer.h"

/* Own include. */
#include "sequencer_intern.h"

/*********************** Add modifier operator *************************/

static bool strip_modifier_active_poll(bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_get(scene);

  if (ed) {
    Sequence *seq = SEQ_select_active_get(scene);

    if (seq) {
      return SEQ_sequence_supports_modifiers(seq);
    }
  }

  return false;
}

static int strip_modifier_add_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  Sequence *seq = SEQ_select_active_get(scene);
  int type = RNA_enum_get(op->ptr, "type");

  SEQ_modifier_new(seq, NULL, type);

  SEQ_relations_invalidate_cache_preprocessed(scene, seq);
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_strip_modifier_add(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Add Strip Modifier";
  ot->idname = "SEQUENCER_OT_strip_modifier_add";
  ot->description = "Add a modifier to the strip";

  /* api callbacks */
  ot->exec = strip_modifier_add_exec;
  ot->poll = strip_modifier_active_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  prop = RNA_def_enum(ot->srna,
                      "type",
                      rna_enum_sequence_modifier_type_items,
                      seqModifierType_ColorBalance,
                      "Type",
                      "");
  ot->prop = prop;
}

/*********************** Remove modifier operator *************************/

static int strip_modifier_remove_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  Sequence *seq = SEQ_select_active_get(scene);
  char name[MAX_NAME];
  SequenceModifierData *smd;

  RNA_string_get(op->ptr, "name", name);

  smd = SEQ_modifier_find_by_name(seq, name);
  if (!smd) {
    return OPERATOR_CANCELLED;
  }

  BLI_remlink(&seq->modifiers, smd);
  SEQ_modifier_free(smd);

  SEQ_relations_invalidate_cache_preprocessed(scene, seq);
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

  /* api callbacks */
  ot->exec = strip_modifier_remove_exec;
  ot->poll = strip_modifier_active_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  prop = RNA_def_string(ot->srna, "name", "Name", MAX_NAME, "Name", "Name of modifier to remove");
  RNA_def_property_flag(prop, PROP_HIDDEN);
}

/*********************** Move operator *************************/

enum {
  SEQ_MODIFIER_MOVE_UP = 0,
  SEQ_MODIFIER_MOVE_DOWN,
};

static int strip_modifier_move_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  Sequence *seq = SEQ_select_active_get(scene);
  char name[MAX_NAME];
  int direction;
  SequenceModifierData *smd;

  RNA_string_get(op->ptr, "name", name);
  direction = RNA_enum_get(op->ptr, "direction");

  smd = SEQ_modifier_find_by_name(seq, name);
  if (!smd) {
    return OPERATOR_CANCELLED;
  }

  if (direction == SEQ_MODIFIER_MOVE_UP) {
    if (smd->prev) {
      BLI_remlink(&seq->modifiers, smd);
      BLI_insertlinkbefore(&seq->modifiers, smd->prev, smd);
    }
  }
  else if (direction == SEQ_MODIFIER_MOVE_DOWN) {
    if (smd->next) {
      BLI_remlink(&seq->modifiers, smd);
      BLI_insertlinkafter(&seq->modifiers, smd->next, smd);
    }
  }

  SEQ_relations_invalidate_cache_preprocessed(scene, seq);
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_strip_modifier_move(wmOperatorType *ot)
{
  PropertyRNA *prop;

  static const EnumPropertyItem direction_items[] = {
      {SEQ_MODIFIER_MOVE_UP, "UP", 0, "Up", "Move modifier up in the stack"},
      {SEQ_MODIFIER_MOVE_DOWN, "DOWN", 0, "Down", "Move modifier down in the stack"},
      {0, NULL, 0, NULL, NULL},
  };

  /* identifiers */
  ot->name = "Move Strip Modifier";
  ot->idname = "SEQUENCER_OT_strip_modifier_move";
  ot->description = "Move modifier up and down in the stack";

  /* api callbacks */
  ot->exec = strip_modifier_move_exec;
  ot->poll = strip_modifier_active_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  prop = RNA_def_string(ot->srna, "name", "Name", MAX_NAME, "Name", "Name of modifier to remove");
  RNA_def_property_flag(prop, PROP_HIDDEN);
  prop = RNA_def_enum(ot->srna, "direction", direction_items, SEQ_MODIFIER_MOVE_UP, "Type", "");
  RNA_def_property_flag(prop, PROP_HIDDEN);
}

/*********************** Copy to selected operator *************************/

enum {
  SEQ_MODIFIER_COPY_REPLACE = 0,
  SEQ_MODIFIER_COPY_APPEND = 1,
};

static int strip_modifier_copy_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = scene->ed;
  Sequence *seq = SEQ_select_active_get(scene);
  const int type = RNA_enum_get(op->ptr, "type");

  if (!seq || !seq->modifiers.first) {
    return OPERATOR_CANCELLED;
  }

  LISTBASE_FOREACH (Sequence *, seq_iter, SEQ_active_seqbase_get(ed)) {
    if (seq_iter->flag & SELECT) {
      if (seq_iter == seq) {
        continue;
      }

      if (type == SEQ_MODIFIER_COPY_REPLACE) {
        if (seq_iter->modifiers.first) {
          SequenceModifierData *smd_tmp, *smd = seq_iter->modifiers.first;
          while (smd) {
            smd_tmp = smd->next;
            BLI_remlink(&seq_iter->modifiers, smd);
            SEQ_modifier_free(smd);
            smd = smd_tmp;
          }
          BLI_listbase_clear(&seq_iter->modifiers);
        }
      }

      SEQ_modifier_list_copy(seq_iter, seq);
    }
  }

  SEQ_relations_invalidate_cache_preprocessed(scene, seq);
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
      {0, NULL, 0, NULL, NULL},
  };

  /* identifiers */
  ot->name = "Copy to Selected Strips";
  ot->idname = "SEQUENCER_OT_strip_modifier_copy";
  ot->description = "Copy modifiers of the active strip to all selected strips";

  /* api callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = strip_modifier_copy_exec;
  ot->poll = strip_modifier_active_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(ot->srna, "type", type_items, SEQ_MODIFIER_COPY_REPLACE, "Type", "");
}
