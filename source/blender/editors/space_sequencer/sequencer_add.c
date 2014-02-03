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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation, 2003-2009, Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_sequencer/sequencer_add.c
 *  \ingroup spseq
 */


#include <stdlib.h>
#include <math.h>
#include <string.h>

#ifndef WIN32
#include <unistd.h>
#else
#include <io.h>
#endif
#include <sys/types.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "DNA_scene_types.h"
#include "DNA_mask_types.h"
#include "DNA_userdef_types.h"

#include "BLF_translation.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_sequencer.h"
#include "BKE_movieclip.h"
#include "BKE_sequencer.h"
#include "BKE_mask.h"
#include "BKE_report.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_define.h"
#include "RNA_enum_types.h"

/* for menu/popup icons etc etc*/

#include "ED_screen.h"
#include "ED_sequencer.h"

#include "UI_view2d.h"

#include "BKE_sound.h"

#ifdef WITH_AUDASPACE
#  include "AUD_C-API.h"
#endif

/* own include */
#include "sequencer_intern.h"

/* Generic functions, reused by add strip operators */

/* avoid passing multiple args and be more verbose */
#define SEQPROP_STARTFRAME  (1 << 0)
#define SEQPROP_ENDFRAME    (1 << 1)
#define SEQPROP_NOPATHS     (1 << 2)
#define SEQPROP_NOCHAN      (1 << 3)

#define SELECT 1

static void sequencer_generic_props__internal(wmOperatorType *ot, int flag)
{
	PropertyRNA *prop;

	if (flag & SEQPROP_STARTFRAME)
		RNA_def_int(ot->srna, "frame_start", 0, INT_MIN, INT_MAX, "Start Frame", "Start frame of the sequence strip", INT_MIN, INT_MAX);
	
	if (flag & SEQPROP_ENDFRAME)
		RNA_def_int(ot->srna, "frame_end", 0, INT_MIN, INT_MAX, "End Frame", "End frame for the color strip", INT_MIN, INT_MAX);  /* not usual since most strips have a fixed length */
	
	RNA_def_int(ot->srna, "channel", 1, 1, MAXSEQ, "Channel", "Channel to place this strip into", 1, MAXSEQ);
	
	RNA_def_boolean(ot->srna, "replace_sel", 1, "Replace Selection", "Replace the current selection");

	/* only for python scripts which import strips and place them after */
	prop = RNA_def_boolean(ot->srna, "overlap", 0, "Allow Overlap", "Don't correct overlap on new sequence strips");
	RNA_def_property_flag(prop, PROP_HIDDEN);
}

static void sequencer_generic_invoke_path__internal(bContext *C, wmOperator *op, const char *identifier)
{
	if (RNA_struct_find_property(op->ptr, identifier)) {
		Scene *scene = CTX_data_scene(C);
		Sequence *last_seq = BKE_sequencer_active_get(scene);
		if (last_seq && last_seq->strip && SEQ_HAS_PATH(last_seq)) {
			char path[FILE_MAX];
			BLI_strncpy(path, last_seq->strip->dir, sizeof(path));
			BLI_path_abs(path, G.main->name);
			RNA_string_set(op->ptr, identifier, path);
		}
	}
}

static int sequencer_generic_invoke_xy_guess_channel(bContext *C, int type)
{
	Sequence *tgt = NULL;
	Sequence *seq;
	Scene *scene = CTX_data_scene(C);
	Editing *ed = BKE_sequencer_editing_get(scene, TRUE);
	int cfra = (int) CFRA;
	int proximity = INT_MAX;

	if (!ed || !ed->seqbasep) {
		return 1;
	}

	for (seq = ed->seqbasep->first; seq; seq = seq->next) {
		if ((type == -1 || seq->type == type) &&
		    (seq->enddisp < cfra) &&
		    (cfra - seq->enddisp < proximity))
		{
			tgt = seq;
			proximity = cfra - seq->enddisp;
		}
	}
	
	if (tgt) {
		return tgt->machine;
	}
	return 1;
}

static void sequencer_generic_invoke_xy__internal(bContext *C, wmOperator *op, int flag, int type)
{
	Scene *scene = CTX_data_scene(C);
	
	int cfra = (int) CFRA;
	
	/* effect strips don't need a channel initialized from the mouse */
	if (!(flag & SEQPROP_NOCHAN)) {
		RNA_int_set(op->ptr, "channel", sequencer_generic_invoke_xy_guess_channel(C, type));
	}

	RNA_int_set(op->ptr, "frame_start", cfra);
	
	if ((flag & SEQPROP_ENDFRAME) && RNA_struct_property_is_set(op->ptr, "frame_end") == 0)
		RNA_int_set(op->ptr, "frame_end", cfra + 25);  // XXX arbitary but ok for now.

	if (!(flag & SEQPROP_NOPATHS)) {
		sequencer_generic_invoke_path__internal(C, op, "filepath");
		sequencer_generic_invoke_path__internal(C, op, "directory");
	}
}

static void seq_load_operator_info(SeqLoadInfo *seq_load, wmOperator *op)
{
	PropertyRNA *prop;
	const bool relative = (prop = RNA_struct_find_property(op->ptr, "relative_path")) && RNA_property_boolean_get(op->ptr, prop);
	int is_file = -1;
	memset(seq_load, 0, sizeof(SeqLoadInfo));

	seq_load->start_frame =  RNA_int_get(op->ptr, "frame_start");
	seq_load->end_frame =    seq_load->start_frame; /* un-set */

	seq_load->channel =      RNA_int_get(op->ptr, "channel");
	seq_load->len =          1; // images only, if endframe isn't set!

	if ((prop = RNA_struct_find_property(op->ptr, "filepath"))) {
		RNA_property_string_get(op->ptr, prop, seq_load->path); /* full path, file is set by the caller */
		is_file = 1;
	}
	else if ((prop = RNA_struct_find_property(op->ptr, "directory"))) {
		RNA_property_string_get(op->ptr, prop, seq_load->path); /* full path, file is set by the caller */
		is_file = 0;
	}

	if ((is_file != -1) && relative)
		BLI_path_rel(seq_load->path, G.main->name);

	
	if ((prop = RNA_struct_find_property(op->ptr, "frame_end"))) {
		seq_load->end_frame = RNA_property_int_get(op->ptr, prop);
	}

	if ((prop = RNA_struct_find_property(op->ptr, "replace_sel")) && RNA_property_boolean_get(op->ptr, prop))
		seq_load->flag |= SEQ_LOAD_REPLACE_SEL;

	if ((prop = RNA_struct_find_property(op->ptr, "cache")) && RNA_property_boolean_get(op->ptr, prop))
		seq_load->flag |= SEQ_LOAD_SOUND_CACHE;

	if ((prop = RNA_struct_find_property(op->ptr, "sound")) && RNA_property_boolean_get(op->ptr, prop))
		seq_load->flag |= SEQ_LOAD_MOVIE_SOUND;

	/* always use this for ops */
	seq_load->flag |= SEQ_LOAD_FRAME_ADVANCE;


	if (is_file == 1) {
		BLI_strncpy(seq_load->name, BLI_path_basename(seq_load->path), sizeof(seq_load->name));
	}
	else if ((prop = RNA_struct_find_property(op->ptr, "files"))) {
		/* used for image strip */
		/* best guess, first images name */
		RNA_PROP_BEGIN (op->ptr, itemptr, prop)
		{
			char *name = RNA_string_get_alloc(&itemptr, "name", NULL, 0);
			BLI_strncpy(seq_load->name, name, sizeof(seq_load->name));
			MEM_freeN(name);
			break;
		}
		RNA_PROP_END;
	}
}

/* add scene operator */
static int sequencer_add_scene_strip_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Editing *ed = BKE_sequencer_editing_get(scene, TRUE);
	
	Scene *sce_seq;

	Sequence *seq;  /* generic strip vars */
	Strip *strip;
	
	int start_frame, channel; /* operator props */
	
	start_frame = RNA_int_get(op->ptr, "frame_start");
	channel = RNA_int_get(op->ptr, "channel");
	
	sce_seq = BLI_findlink(&CTX_data_main(C)->scene, RNA_enum_get(op->ptr, "scene"));
	
	if (sce_seq == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Scene not found");
		return OPERATOR_CANCELLED;
	}
	
	seq = BKE_sequence_alloc(ed->seqbasep, start_frame, channel);
	seq->type = SEQ_TYPE_SCENE;
	seq->blend_mode = SEQ_TYPE_CROSS; /* so alpha adjustment fade to the strip below */

	seq->scene = sce_seq;
	
	/* basic defaults */
	seq->strip = strip = MEM_callocN(sizeof(Strip), "strip");
	seq->len = sce_seq->r.efra - sce_seq->r.sfra + 1;
	strip->us = 1;
	
	BLI_strncpy(seq->name + 2, sce_seq->id.name + 2, sizeof(seq->name) - 2);
	BKE_sequence_base_unique_name_recursive(&ed->seqbase, seq);

	seq->scene_sound = sound_scene_add_scene_sound(scene, seq, start_frame, start_frame + seq->len, 0);

	BKE_sequence_calc_disp(scene, seq);
	BKE_sequencer_sort(scene);
	
	if (RNA_boolean_get(op->ptr, "replace_sel")) {
		ED_sequencer_deselect_all(scene);
		BKE_sequencer_active_set(scene, seq);
		seq->flag |= SELECT;
	}

	if (RNA_boolean_get(op->ptr, "overlap") == FALSE) {
		if (BKE_sequence_test_overlap(ed->seqbasep, seq)) BKE_sequence_base_shuffle(ed->seqbasep, seq, scene);
	}

	WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);
	
	return OPERATOR_FINISHED;
}


static int sequencer_add_scene_strip_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	if (!ED_operator_sequencer_active(C)) {
		BKE_report(op->reports, RPT_ERROR, "Sequencer area not active");
		return OPERATOR_CANCELLED;
	}

	if (!RNA_struct_property_is_set(op->ptr, "scene"))
		return WM_enum_search_invoke(C, op, event);

	sequencer_generic_invoke_xy__internal(C, op, 0, SEQ_TYPE_SCENE);
	return sequencer_add_scene_strip_exec(C, op);
	// needs a menu
	// return WM_menu_invoke(C, op, event);
}


void SEQUENCER_OT_scene_strip_add(struct wmOperatorType *ot)
{
	PropertyRNA *prop;
	
	/* identifiers */
	ot->name = "Add Scene Strip";
	ot->idname = "SEQUENCER_OT_scene_strip_add";
	ot->description = "Add a strip to the sequencer using a blender scene as a source";

	/* api callbacks */
	ot->invoke = sequencer_add_scene_strip_invoke;
	ot->exec = sequencer_add_scene_strip_exec;

	ot->poll = ED_operator_scene_editable;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	sequencer_generic_props__internal(ot, SEQPROP_STARTFRAME);
	prop = RNA_def_enum(ot->srna, "scene", DummyRNA_NULL_items, 0, "Scene", "");
	RNA_def_enum_funcs(prop, RNA_scene_itemf);
	ot->prop = prop;
}

/* add movieclip operator */
static int sequencer_add_movieclip_strip_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Editing *ed = BKE_sequencer_editing_get(scene, TRUE);
	
	MovieClip *clip;

	Sequence *seq;  /* generic strip vars */
	Strip *strip;
	
	int start_frame, channel; /* operator props */
	
	start_frame = RNA_int_get(op->ptr, "frame_start");
	channel = RNA_int_get(op->ptr, "channel");
	
	clip = BLI_findlink(&CTX_data_main(C)->movieclip, RNA_enum_get(op->ptr, "clip"));
	
	if (clip == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Movie clip not found");
		return OPERATOR_CANCELLED;
	}
	
	seq = BKE_sequence_alloc(ed->seqbasep, start_frame, channel);
	seq->type = SEQ_TYPE_MOVIECLIP;
	seq->blend_mode = SEQ_TYPE_CROSS;
	seq->clip = clip;

	if (seq->clip->id.us == 0)
		seq->clip->id.us = 1;

	/* basic defaults */
	seq->strip = strip = MEM_callocN(sizeof(Strip), "strip");
	seq->len =  BKE_movieclip_get_duration(clip);
	strip->us = 1;
	
	BLI_strncpy(seq->name + 2, clip->id.name + 2, sizeof(seq->name) - 2);
	BKE_sequence_base_unique_name_recursive(&ed->seqbase, seq);

	BKE_sequence_calc_disp(scene, seq);
	BKE_sequencer_sort(scene);
	
	if (RNA_boolean_get(op->ptr, "replace_sel")) {
		ED_sequencer_deselect_all(scene);
		BKE_sequencer_active_set(scene, seq);
		seq->flag |= SELECT;
	}

	if (RNA_boolean_get(op->ptr, "overlap") == FALSE) {
		if (BKE_sequence_test_overlap(ed->seqbasep, seq)) BKE_sequence_base_shuffle(ed->seqbasep, seq, scene);
	}

	WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);
	
	return OPERATOR_FINISHED;
}

static int sequencer_add_movieclip_strip_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	if (!ED_operator_sequencer_active(C)) {
		BKE_report(op->reports, RPT_ERROR, "Sequencer area not active");
		return OPERATOR_CANCELLED;
	}

	if (!RNA_struct_property_is_set(op->ptr, "clip"))
		return WM_enum_search_invoke(C, op, event);

	sequencer_generic_invoke_xy__internal(C, op, 0, SEQ_TYPE_MOVIECLIP);
	return sequencer_add_movieclip_strip_exec(C, op);
	// needs a menu
	// return WM_menu_invoke(C, op, event);
}

void SEQUENCER_OT_movieclip_strip_add(struct wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Add MovieClip Strip";
	ot->idname = "SEQUENCER_OT_movieclip_strip_add";
	ot->description = "Add a movieclip strip to the sequencer";

	/* api callbacks */
	ot->invoke = sequencer_add_movieclip_strip_invoke;
	ot->exec = sequencer_add_movieclip_strip_exec;

	ot->poll = ED_operator_scene_editable;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	sequencer_generic_props__internal(ot, SEQPROP_STARTFRAME);
	prop = RNA_def_enum(ot->srna, "clip", DummyRNA_NULL_items, 0, "Clip", "");
	RNA_def_enum_funcs(prop, RNA_movieclip_itemf);
	RNA_def_property_translation_context(prop, BLF_I18NCONTEXT_ID_MOVIECLIP);
	ot->prop = prop;
}

static int sequencer_add_mask_strip_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Editing *ed = BKE_sequencer_editing_get(scene, TRUE);

	Mask *mask;

	Sequence *seq;  /* generic strip vars */
	Strip *strip;

	int start_frame, channel; /* operator props */

	start_frame = RNA_int_get(op->ptr, "frame_start");
	channel = RNA_int_get(op->ptr, "channel");

	mask = BLI_findlink(&CTX_data_main(C)->mask, RNA_enum_get(op->ptr, "mask"));

	if (mask == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Mask not found");
		return OPERATOR_CANCELLED;
	}

	seq = BKE_sequence_alloc(ed->seqbasep, start_frame, channel);
	seq->type = SEQ_TYPE_MASK;
	seq->blend_mode = SEQ_TYPE_CROSS;
	seq->mask = mask;

	if (seq->mask->id.us == 0)
		seq->mask->id.us = 1;

	/* basic defaults */
	seq->strip = strip = MEM_callocN(sizeof(Strip), "strip");
	seq->len = BKE_mask_get_duration(mask);
	strip->us = 1;

	BLI_strncpy(seq->name + 2, mask->id.name + 2, sizeof(seq->name) - 2);
	BKE_sequence_base_unique_name_recursive(&ed->seqbase, seq);

	BKE_sequence_calc_disp(scene, seq);
	BKE_sequencer_sort(scene);

	if (RNA_boolean_get(op->ptr, "replace_sel")) {
		ED_sequencer_deselect_all(scene);
		BKE_sequencer_active_set(scene, seq);
		seq->flag |= SELECT;
	}

	if (RNA_boolean_get(op->ptr, "overlap") == FALSE) {
		if (BKE_sequence_test_overlap(ed->seqbasep, seq)) BKE_sequence_base_shuffle(ed->seqbasep, seq, scene);
	}

	WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

	return OPERATOR_FINISHED;
}

static int sequencer_add_mask_strip_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	if (!ED_operator_sequencer_active(C)) {
		BKE_report(op->reports, RPT_ERROR, "Sequencer area not active");
		return OPERATOR_CANCELLED;
	}

	if (!RNA_struct_property_is_set(op->ptr, "mask"))
		return WM_enum_search_invoke(C, op, event);

	sequencer_generic_invoke_xy__internal(C, op, 0, SEQ_TYPE_MASK);
	return sequencer_add_mask_strip_exec(C, op);
	// needs a menu
	// return WM_menu_invoke(C, op, event);
}


void SEQUENCER_OT_mask_strip_add(struct wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Add Mask Strip";
	ot->idname = "SEQUENCER_OT_mask_strip_add";
	ot->description = "Add a mask strip to the sequencer";

	/* api callbacks */
	ot->invoke = sequencer_add_mask_strip_invoke;
	ot->exec = sequencer_add_mask_strip_exec;

	ot->poll = ED_operator_scene_editable;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	sequencer_generic_props__internal(ot, SEQPROP_STARTFRAME);
	prop = RNA_def_enum(ot->srna, "mask", DummyRNA_NULL_items, 0, "Mask", "");
	RNA_def_enum_funcs(prop, RNA_mask_itemf);
	ot->prop = prop;
}


static int sequencer_add_generic_strip_exec(bContext *C, wmOperator *op, SeqLoadFunc seq_load_func)
{
	Scene *scene = CTX_data_scene(C); /* only for sound */
	Editing *ed = BKE_sequencer_editing_get(scene, TRUE);
	SeqLoadInfo seq_load;
	Sequence *seq;
	int tot_files;
	const short overlap = RNA_boolean_get(op->ptr, "overlap");

	seq_load_operator_info(&seq_load, op);

	if (seq_load.flag & SEQ_LOAD_REPLACE_SEL)
		ED_sequencer_deselect_all(scene);

	if (RNA_struct_property_is_set(op->ptr, "files"))
		tot_files = RNA_property_collection_length(op->ptr, RNA_struct_find_property(op->ptr, "files"));
	else
		tot_files = 0;

	if (tot_files) {
		/* multiple files */
		char dir_only[FILE_MAX];
		char file_only[FILE_MAX];

		BLI_split_dir_part(seq_load.path, dir_only, sizeof(dir_only));

		RNA_BEGIN (op->ptr, itemptr, "files")
		{
			RNA_string_get(&itemptr, "name", file_only);
			BLI_join_dirfile(seq_load.path, sizeof(seq_load.path), dir_only, file_only);

			/* Set seq_load.name, else all video/audio files get the same name! ugly! */
			BLI_strncpy(seq_load.name, file_only, sizeof(seq_load.name));

			seq = seq_load_func(C, ed->seqbasep, &seq_load);
			if (seq) {
				if (overlap == FALSE) {
					if (BKE_sequence_test_overlap(ed->seqbasep, seq))
						BKE_sequence_base_shuffle(ed->seqbasep, seq, scene);
				}
			}
		}
		RNA_END;
	}
	else {
		/* single file */
		seq = seq_load_func(C, ed->seqbasep, &seq_load);
		if (seq) {
			if (overlap == FALSE) {
				if (BKE_sequence_test_overlap(ed->seqbasep, seq))
					BKE_sequence_base_shuffle(ed->seqbasep, seq, scene);
			}
		}
	}

	if (seq_load.tot_success == 0) {
		BKE_reportf(op->reports, RPT_ERROR, "File '%s' could not be loaded", seq_load.path);
		return OPERATOR_CANCELLED;
	}

	BKE_sequencer_sort(scene);
	BKE_sequencer_update_muting(ed);

	WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

	return OPERATOR_FINISHED;
}

/* add movie operator */
static int sequencer_add_movie_strip_exec(bContext *C, wmOperator *op)
{
	return sequencer_add_generic_strip_exec(C, op, BKE_sequencer_add_movie_strip);
}


static int sequencer_add_movie_strip_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{

	if (!ED_operator_sequencer_active(C)) {
		BKE_report(op->reports, RPT_ERROR, "Sequencer area not active");
		return OPERATOR_CANCELLED;
	}

	/* This is for drag and drop */
	if ((RNA_struct_property_is_set(op->ptr, "files") && RNA_collection_length(op->ptr, "files")) ||
	    RNA_struct_property_is_set(op->ptr, "filepath"))
	{
		sequencer_generic_invoke_xy__internal(C, op, SEQPROP_NOPATHS, SEQ_TYPE_MOVIE);
		return sequencer_add_movie_strip_exec(C, op);
	}
	
	sequencer_generic_invoke_xy__internal(C, op, 0, SEQ_TYPE_MOVIE);
	
	WM_event_add_fileselect(C, op);
	return OPERATOR_RUNNING_MODAL;

	//return sequencer_add_movie_strip_exec(C, op);
}


void SEQUENCER_OT_movie_strip_add(struct wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name = "Add Movie Strip";
	ot->idname = "SEQUENCER_OT_movie_strip_add";
	ot->description = "Add a movie strip to the sequencer";

	/* api callbacks */
	ot->invoke = sequencer_add_movie_strip_invoke;
	ot->exec = sequencer_add_movie_strip_exec;

	ot->poll = ED_operator_scene_editable;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	WM_operator_properties_filesel(ot, FOLDERFILE | MOVIEFILE, FILE_SPECIAL, FILE_OPENFILE,
	                               WM_FILESEL_FILEPATH | WM_FILESEL_RELPATH | WM_FILESEL_FILES, FILE_DEFAULTDISPLAY);
	sequencer_generic_props__internal(ot, SEQPROP_STARTFRAME);
	RNA_def_boolean(ot->srna, "sound", TRUE, "Sound", "Load sound with the movie");
}

/* add sound operator */

static int sequencer_add_sound_strip_exec(bContext *C, wmOperator *op)
{
	return sequencer_add_generic_strip_exec(C, op, BKE_sequencer_add_sound_strip);
}

static int sequencer_add_sound_strip_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{

	if (!ED_operator_sequencer_active(C)) {
		BKE_report(op->reports, RPT_ERROR, "Sequencer area not active");
		return OPERATOR_CANCELLED;
	}
	
	/* This is for drag and drop */
	if ((RNA_struct_property_is_set(op->ptr, "files") && RNA_collection_length(op->ptr, "files")) ||
	    RNA_struct_property_is_set(op->ptr, "filepath"))
	{
		sequencer_generic_invoke_xy__internal(C, op, SEQPROP_NOPATHS, SEQ_TYPE_SOUND_RAM);
		return sequencer_add_sound_strip_exec(C, op);
	}
	
	sequencer_generic_invoke_xy__internal(C, op, 0, SEQ_TYPE_SOUND_RAM);

	WM_event_add_fileselect(C, op);
	return OPERATOR_RUNNING_MODAL;

	//return sequencer_add_sound_strip_exec(C, op);
}


void SEQUENCER_OT_sound_strip_add(struct wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name = "Add Sound Strip";
	ot->idname = "SEQUENCER_OT_sound_strip_add";
	ot->description = "Add a sound strip to the sequencer";

	/* api callbacks */
	ot->invoke = sequencer_add_sound_strip_invoke;
	ot->exec = sequencer_add_sound_strip_exec;

	ot->poll = ED_operator_scene_editable;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	WM_operator_properties_filesel(ot, FOLDERFILE | SOUNDFILE, FILE_SPECIAL, FILE_OPENFILE,
	                               WM_FILESEL_FILEPATH | WM_FILESEL_RELPATH | WM_FILESEL_FILES, FILE_DEFAULTDISPLAY);
	sequencer_generic_props__internal(ot, SEQPROP_STARTFRAME);
	RNA_def_boolean(ot->srna, "cache", FALSE, "Cache", "Cache the sound in memory");
}

/* add image operator */
static int sequencer_add_image_strip_exec(bContext *C, wmOperator *op)
{
	/* cant use the generic function for this */

	Scene *scene = CTX_data_scene(C); /* only for sound */
	Editing *ed = BKE_sequencer_editing_get(scene, TRUE);
	SeqLoadInfo seq_load;
	Sequence *seq;

	Strip *strip;
	StripElem *se;

	seq_load_operator_info(&seq_load, op);

	/* images are unique in how they handle this - 1 per strip elem */
	seq_load.len = RNA_property_collection_length(op->ptr, RNA_struct_find_property(op->ptr, "files"));

	if (seq_load.len == 0)
		return OPERATOR_CANCELLED;

	if (seq_load.flag & SEQ_LOAD_REPLACE_SEL)
		ED_sequencer_deselect_all(scene);


	/* main adding function */
	seq = BKE_sequencer_add_image_strip(C, ed->seqbasep, &seq_load);
	strip = seq->strip;
	se = strip->stripdata;

	RNA_BEGIN (op->ptr, itemptr, "files")
	{
		char *filename = RNA_string_get_alloc(&itemptr, "name", NULL, 0);
		BLI_strncpy(se->name, filename, sizeof(se->name));
		MEM_freeN(filename);
		se++;
	}
	RNA_END;

	if (seq_load.len == 1) {
		if (seq_load.start_frame < seq_load.end_frame) {
			seq->endstill = seq_load.end_frame - seq_load.start_frame;
		}
	}

	BKE_sequence_init_colorspace(seq);

	BKE_sequence_calc_disp(scene, seq);

	BKE_sequencer_sort(scene);

	/* last active name */
	BLI_strncpy(ed->act_imagedir, strip->dir, sizeof(ed->act_imagedir));

	if (RNA_boolean_get(op->ptr, "overlap") == FALSE) {
		if (BKE_sequence_test_overlap(ed->seqbasep, seq))
			BKE_sequence_base_shuffle(ed->seqbasep, seq, scene);
	}

	WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

	return OPERATOR_FINISHED;
}

static int sequencer_add_image_strip_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{

	if (!ED_operator_sequencer_active(C)) {
		BKE_report(op->reports, RPT_ERROR, "Sequencer area not active");
		return OPERATOR_CANCELLED;
	}


	/* drag drop has set the names */
	if (RNA_struct_property_is_set(op->ptr, "files") && RNA_collection_length(op->ptr, "files")) {
		sequencer_generic_invoke_xy__internal(C, op, SEQPROP_ENDFRAME | SEQPROP_NOPATHS, SEQ_TYPE_IMAGE);
		return sequencer_add_image_strip_exec(C, op);
	}
	
	sequencer_generic_invoke_xy__internal(C, op, SEQPROP_ENDFRAME, SEQ_TYPE_IMAGE);

	WM_event_add_fileselect(C, op);
	return OPERATOR_RUNNING_MODAL;
}


void SEQUENCER_OT_image_strip_add(struct wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name = "Add Image Strip";
	ot->idname = "SEQUENCER_OT_image_strip_add";
	ot->description = "Add an image or image sequence to the sequencer";

	/* api callbacks */
	ot->invoke = sequencer_add_image_strip_invoke;
	ot->exec = sequencer_add_image_strip_exec;

	ot->poll = ED_operator_scene_editable;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	WM_operator_properties_filesel(ot, FOLDERFILE | IMAGEFILE, FILE_SPECIAL, FILE_OPENFILE,
	                               WM_FILESEL_DIRECTORY | WM_FILESEL_RELPATH | WM_FILESEL_FILES, FILE_DEFAULTDISPLAY);
	sequencer_generic_props__internal(ot, SEQPROP_STARTFRAME | SEQPROP_ENDFRAME);
}


/* add_effect_strip operator */
static int sequencer_add_effect_strip_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Editing *ed = BKE_sequencer_editing_get(scene, TRUE);

	Sequence *seq;  /* generic strip vars */
	Strip *strip;
	struct SeqEffectHandle sh;

	int start_frame, end_frame, channel, type; /* operator props */
	
	Sequence *seq1, *seq2, *seq3;
	const char *error_msg;

	start_frame = RNA_int_get(op->ptr, "frame_start");
	end_frame = RNA_int_get(op->ptr, "frame_end");
	channel = RNA_int_get(op->ptr, "channel");

	type = RNA_enum_get(op->ptr, "type");
	
	// XXX move to invoke
	if (!seq_effect_find_selected(scene, NULL, type, &seq1, &seq2, &seq3, &error_msg)) {
		BKE_report(op->reports, RPT_ERROR, error_msg);
		return OPERATOR_CANCELLED;
	}

	/* If seq1 is NULL and no error was raised it means the seq is standalone
	 * (like color strips) and we need to check its start and end frames are valid */
	if (seq1 == NULL && end_frame <= start_frame) {
		BKE_report(op->reports, RPT_ERROR, "Start and end frame are not set");
		return OPERATOR_CANCELLED;
	}

	seq = BKE_sequence_alloc(ed->seqbasep, start_frame, channel);
	seq->type = type;

	BLI_strncpy(seq->name + 2, BKE_sequence_give_name(seq), sizeof(seq->name) - 2);
	BKE_sequence_base_unique_name_recursive(&ed->seqbase, seq);

	sh = BKE_sequence_get_effect(seq);

	seq->seq1 = seq1;
	seq->seq2 = seq2;
	seq->seq3 = seq3;

	sh.init(seq);

	if (!seq1) { /* effect has no deps */
		seq->len = 1;
		BKE_sequence_tx_set_final_right(seq, end_frame);
	}

	seq->flag |= SEQ_USE_EFFECT_DEFAULT_FADE;

	BKE_sequence_calc(scene, seq);
	
	/* basic defaults */
	seq->strip = strip = MEM_callocN(sizeof(Strip), "strip");
	strip->us = 1;

	if (seq->type == SEQ_TYPE_COLOR) {
		SolidColorVars *colvars = (SolidColorVars *)seq->effectdata;
		RNA_float_get_array(op->ptr, "color", colvars->col);
		seq->blend_mode = SEQ_TYPE_CROSS; /* so alpha adjustment fade to the strip below */

	}
	else if (seq->type == SEQ_TYPE_ADJUSTMENT) {
		seq->blend_mode = SEQ_TYPE_CROSS;
	}

	/* an unset channel is a special case where we automatically go above
	 * the other strips. */
	if (!RNA_struct_property_is_set(op->ptr, "channel")) {
		if (seq->seq1) {
			int chan = max_iii(seq->seq1 ? seq->seq1->machine : 0,
			                   seq->seq2 ? seq->seq2->machine : 0,
			                   seq->seq3 ? seq->seq3->machine : 0);
			if (chan < MAXSEQ)
				seq->machine = chan;
		}
	}

	if (RNA_boolean_get(op->ptr, "overlap") == FALSE) {
		if (BKE_sequence_test_overlap(ed->seqbasep, seq)) BKE_sequence_base_shuffle(ed->seqbasep, seq, scene);
	}

	BKE_sequencer_update_changed_seq_and_deps(scene, seq, 1, 1); /* runs BKE_sequence_calc */


	/* not sure if this is needed with update_changed_seq_and_deps.
	 * it was NOT called in blender 2.4x, but wont hurt */
	BKE_sequencer_sort(scene); 

	if (RNA_boolean_get(op->ptr, "replace_sel")) {
		ED_sequencer_deselect_all(scene);
		BKE_sequencer_active_set(scene, seq);
		seq->flag |= SELECT;
	}

	WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

	return OPERATOR_FINISHED;
}


/* add color */
static int sequencer_add_effect_strip_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	short is_type_set = RNA_struct_property_is_set(op->ptr, "type");
	int type = -1;
	int prop_flag = SEQPROP_ENDFRAME;

	if (!ED_operator_sequencer_active(C)) {
		BKE_report(op->reports, RPT_ERROR, "Sequencer area not active");
		return OPERATOR_CANCELLED;
	}

	if (is_type_set) {
		type = RNA_enum_get(op->ptr, "type");

		/* when invoking an effect strip which uses inputs,
		 * skip initializing the channel from the mouse.
		 * Instead leave the property unset so exec() initializes it to be
		 * above the strips its applied to. */
		if (BKE_sequence_effect_get_num_inputs(type) != 0) {
			prop_flag |= SEQPROP_NOCHAN;
		}
	}

	sequencer_generic_invoke_xy__internal(C, op, prop_flag, type);

	return sequencer_add_effect_strip_exec(C, op);
}

void SEQUENCER_OT_effect_strip_add(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Effect Strip";
	ot->idname = "SEQUENCER_OT_effect_strip_add";
	ot->description = "Add an effect to the sequencer, most are applied on top of existing strips";

	/* api callbacks */
	ot->invoke = sequencer_add_effect_strip_invoke;
	ot->exec = sequencer_add_effect_strip_exec;

	ot->poll = ED_operator_scene_editable;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	WM_operator_properties_filesel(ot, 0, FILE_SPECIAL, FILE_OPENFILE,
	                               WM_FILESEL_FILEPATH | WM_FILESEL_RELPATH, FILE_DEFAULTDISPLAY);
	sequencer_generic_props__internal(ot, SEQPROP_STARTFRAME | SEQPROP_ENDFRAME);
	RNA_def_enum(ot->srna, "type", sequencer_prop_effect_types, SEQ_TYPE_CROSS, "Type", "Sequencer effect type");
	RNA_def_float_vector(ot->srna, "color", 3, NULL, 0.0f, 1.0f, "Color", "Initialize the strip with this color (only used when type='COLOR')", 0.0f, 1.0f);
}
