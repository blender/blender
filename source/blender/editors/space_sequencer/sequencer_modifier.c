/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 * The Original Code is Copyright (C) 2012 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */


/** \file blender/editors/space_sequencer/sequencer_modifier.c
 *  \ingroup spseq
 */


#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "DNA_scene_types.h"
#include "DNA_userdef_types.h"

#include "BKE_context.h"
#include "BKE_sequencer.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_define.h"
#include "RNA_enum_types.h"


/* own include */
#include "sequencer_intern.h"

/*********************** Add modifier operator *************************/

static int strip_modifier_active_poll(bContext *C)
{
	Scene *scene = CTX_data_scene(C);
	Editing *ed = BKE_sequencer_editing_get(scene, false);

	if (ed) {
		Sequence *seq = BKE_sequencer_active_get(scene);

		if (seq)
			return BKE_sequence_supports_modifiers(seq);
	}

	return false;
}

static int strip_modifier_add_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Sequence *seq = BKE_sequencer_active_get(scene);
	int type = RNA_enum_get(op->ptr, "type");

	BKE_sequence_modifier_new(seq, NULL, type);

	BKE_sequence_invalidate_cache(scene, seq);
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
	prop = RNA_def_enum(ot->srna, "type", sequence_modifier_type_items, seqModifierType_ColorBalance, "Type", "");
	ot->prop = prop;
}

/*********************** Remove modifier operator *************************/

static int strip_modifier_remove_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Sequence *seq = BKE_sequencer_active_get(scene);
	char name[MAX_NAME];
	SequenceModifierData *smd;

	RNA_string_get(op->ptr, "name", name);

	smd = BKE_sequence_modifier_find_by_name(seq, name);
	if (!smd)
		return OPERATOR_CANCELLED;

	BLI_remlink(&seq->modifiers, smd);
	BKE_sequence_modifier_free(smd);

	BKE_sequence_invalidate_cache(scene, seq);
	WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

	return OPERATOR_FINISHED;
}

void SEQUENCER_OT_strip_modifier_remove(wmOperatorType *ot)
{
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
	RNA_def_string(ot->srna, "name", "Name", MAX_NAME, "Name", "Name of modifier to remove");
}

/*********************** Move operator *************************/

enum {
	SEQ_MODIFIER_MOVE_UP = 0,
	SEQ_MODIFIER_MOVE_DOWN
};

static int strip_modifier_move_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Sequence *seq = BKE_sequencer_active_get(scene);
	char name[MAX_NAME];
	int direction;
	SequenceModifierData *smd;

	RNA_string_get(op->ptr, "name", name);
	direction = RNA_enum_get(op->ptr, "direction");

	smd = BKE_sequence_modifier_find_by_name(seq, name);
	if (!smd)
		return OPERATOR_CANCELLED;

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

	BKE_sequence_invalidate_cache(scene, seq);
	WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

	return OPERATOR_FINISHED;
}

void SEQUENCER_OT_strip_modifier_move(wmOperatorType *ot)
{
	static EnumPropertyItem direction_items[] = {
		{SEQ_MODIFIER_MOVE_UP, "UP", 0, "Up", "Move modifier up in the stack"},
		{SEQ_MODIFIER_MOVE_DOWN, "DOWN", 0, "Down", "Move modifier down in the stack"},
		{0, NULL, 0, NULL, NULL}
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
	RNA_def_string(ot->srna, "name", "Name", MAX_NAME, "Name", "Name of modifier to remove");
	RNA_def_enum(ot->srna, "direction", direction_items, SEQ_MODIFIER_MOVE_UP, "Type", "");
}
