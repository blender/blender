/**
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation, 2003-2009, Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
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
#include "BLI_storage_types.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "DNA_ipo_types.h"
#include "DNA_curve_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_sequence_types.h"
#include "DNA_view2d_types.h"
#include "DNA_userdef_types.h"
#include "DNA_sound_types.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_plugin_types.h"
#include "BKE_sequencer.h"
#include "BKE_scene.h"
#include "BKE_utildefines.h"
#include "BKE_report.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

/* for menu/popup icons etc etc*/
#include "UI_interface.h"
#include "UI_resources.h"

#include "ED_anim_api.h"
#include "ED_space_api.h"
#include "ED_types.h"
#include "ED_screen.h"
#include "ED_util.h"
#include "ED_fileselect.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "BKE_sound.h"
#include "AUD_C-API.h"

/* own include */
#include "sequencer_intern.h"

/* Generic functions, reused by add strip operators */

/* avoid passing multiple args and be more verbose */
#define SEQPROP_STARTFRAME	1<<0
#define SEQPROP_ENDFRAME	1<<1
#define SEQPROP_FILES		1<<2

static void sequencer_generic_props__internal(wmOperatorType *ot, int flag)
{
	RNA_def_string(ot->srna, "name", "", MAX_ID_NAME-2, "Name", "Name of the new sequence strip");

	if(flag & SEQPROP_STARTFRAME)
		RNA_def_int(ot->srna, "start_frame", 0, INT_MIN, INT_MAX, "Start Frame", "Start frame of the sequence strip", INT_MIN, INT_MAX);
	
	if(flag & SEQPROP_ENDFRAME)
		RNA_def_int(ot->srna, "end_frame", 0, INT_MIN, INT_MAX, "End Frame", "End frame for the color strip", INT_MIN, INT_MAX); /* not useual since most strips have a fixed length */
	
	RNA_def_int(ot->srna, "channel", 1, 1, MAXSEQ, "Channel", "Channel to place this strip into", 1, MAXSEQ);
	
	RNA_def_boolean(ot->srna, "replace_sel", 1, "Replace Selection", "replace the current selection");

	if(flag & SEQPROP_FILES)
		RNA_def_collection_runtime(ot->srna, "files", &RNA_OperatorFileListElement, "Files", "");
}

static void sequencer_generic_invoke_xy__internal(bContext *C, wmOperator *op, wmEvent *event, int flag)
{
	ARegion *ar= CTX_wm_region(C);
	View2D *v2d= UI_view2d_fromcontext(C);
	
	short mval[2];	
	float mval_v2d[2];
	

	mval[0]= event->x - ar->winrct.xmin;
	mval[1]= event->y - ar->winrct.ymin;
	
	UI_view2d_region_to_view(v2d, mval[0], mval[1], &mval_v2d[0], &mval_v2d[1]);
	
	RNA_int_set(op->ptr, "channel", (int)mval_v2d[1]+0.5f);
	RNA_int_set(op->ptr, "start_frame", (int)mval_v2d[0]);
	
	if ((flag & SEQPROP_ENDFRAME) && RNA_property_is_set(op->ptr, "end_frame")==0)
		RNA_int_set(op->ptr, "end_frame", (int)mval_v2d[0] + 25); // XXX arbitary but ok for now.
	
}

static void seq_load_operator_info(SeqLoadInfo *seq_load, wmOperator *op)
{
	memset(seq_load, 0, sizeof(SeqLoadInfo));

	seq_load->start_frame=	RNA_int_get(op->ptr, "start_frame");
	seq_load->end_frame=	seq_load->start_frame; /* un-set */

	seq_load->channel=		RNA_int_get(op->ptr, "channel");
	seq_load->len=			1; // images only, if endframe isnt set!

	RNA_string_get(op->ptr, "name", seq_load->name+2);

	RNA_string_get(op->ptr, "path", seq_load->path); /* full path, file is set by the caller */

	if (RNA_struct_find_property(op->ptr, "end_frame")) {
		seq_load->end_frame = RNA_int_get(op->ptr, "end_frame");
	}

	if (RNA_struct_find_property(op->ptr, "replace_sel") && RNA_boolean_get(op->ptr, "replace_sel"))
		seq_load->flag |= SEQ_LOAD_REPLACE_SEL;

	if (RNA_struct_find_property(op->ptr, "cache") && RNA_boolean_get(op->ptr, "cache"))
		seq_load->flag |= SEQ_LOAD_SOUND_CACHE;

	if (RNA_struct_find_property(op->ptr, "sound") && RNA_boolean_get(op->ptr, "sound"))
		seq_load->flag |= SEQ_LOAD_MOVIE_SOUND;

	/* always use this for ops */
	seq_load->flag |= SEQ_LOAD_FRAME_ADVANCE;
}

/* add scene operator */
static int sequencer_add_scene_strip_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Editing *ed= seq_give_editing(scene, TRUE);
	
	Scene *sce_seq;

	Sequence *seq;	/* generic strip vars */
	Strip *strip;
	StripElem *se;
	
	int start_frame, channel; /* operator props */
	
	start_frame= RNA_int_get(op->ptr, "start_frame");
	channel= RNA_int_get(op->ptr, "channel");
	
	sce_seq= BLI_findlink(&CTX_data_main(C)->scene, RNA_enum_get(op->ptr, "type"));
	
	if (sce_seq==NULL) {
		BKE_report(op->reports, RPT_ERROR, "Scene not found");
		return OPERATOR_CANCELLED;
	}
	
	seq = alloc_sequence(ed->seqbasep, start_frame, channel);
	
	seq->type= SEQ_SCENE;
	seq->scene= sce_seq;
	
	/* basic defaults */
	seq->strip= strip= MEM_callocN(sizeof(Strip), "strip");
	strip->len = seq->len = sce_seq->r.efra - sce_seq->r.sfra + 1;
	strip->us= 1;
	
	strip->stripdata= se= MEM_callocN(seq->len*sizeof(StripElem), "stripelem");
	
	if(RNA_property_is_set(op->ptr, "name"))
		RNA_string_get(op->ptr, "name", seq->name+2);
	else
		strcpy(seq->name+2, sce_seq->id.name+2);
	
	calc_sequence_disp(scene, seq);
	sort_seq(scene);
	
	if (RNA_boolean_get(op->ptr, "replace_sel")) {
		deselect_all_seq(scene);
		active_seq_set(scene, seq);
		seq->flag |= SELECT;
	}
	
	WM_event_add_notifier(C, NC_SCENE|ND_SEQUENCER, scene);
	
	return OPERATOR_FINISHED;
}


static int sequencer_add_scene_strip_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	sequencer_generic_invoke_xy__internal(C, op, event, 0);
	return sequencer_add_scene_strip_exec(C, op);
	// needs a menu
	// return WM_menu_invoke(C, op, event);
}


void SEQUENCER_OT_scene_strip_add(struct wmOperatorType *ot)
{
	PropertyRNA *prop;
	
	/* identifiers */
	ot->name= "Add Scene Strip";
	ot->idname= "SEQUENCER_OT_scene_strip_add";
	ot->description= "Add a strip to the sequencer using a blender scene as a source";

	/* api callbacks */
	ot->invoke= sequencer_add_scene_strip_invoke;
	ot->exec= sequencer_add_scene_strip_exec;

	ot->poll= ED_operator_sequencer_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	sequencer_generic_props__internal(ot, SEQPROP_STARTFRAME);
	prop= RNA_def_enum(ot->srna, "type", DummyRNA_NULL_items, 0, "Type", "");
	RNA_def_enum_funcs(prop, RNA_scene_itemf);
}

static int sequencer_add_generic_strip_exec(bContext *C, wmOperator *op, SeqLoadFunc seq_load_func)
{
	Scene *scene= CTX_data_scene(C); /* only for sound */
	Editing *ed= seq_give_editing(scene, TRUE);
	SeqLoadInfo seq_load;
	Sequence *seq;
	int tot_files;

	seq_load_operator_info(&seq_load, op);

	if (seq_load.flag & SEQ_LOAD_REPLACE_SEL)
		deselect_all_seq(scene);

	tot_files= RNA_property_collection_length(op->ptr, RNA_struct_find_property(op->ptr, "files"));

	if(tot_files) {
		/* multiple files */
		char dir_only[FILE_MAX];
		char file_only[FILE_MAX];

		BLI_split_dirfile_basic(seq_load.path, dir_only, NULL);

		RNA_BEGIN(op->ptr, itemptr, "files") {
			RNA_string_get(&itemptr, "name", file_only);
			BLI_join_dirfile(seq_load.path, dir_only, file_only);

			seq= seq_load_func(C, ed->seqbasep, &seq_load);
		}
		RNA_END;
	}
	else {
		/* single file */
		seq= seq_load_func(C, ed->seqbasep, &seq_load);
	}

	if (seq_load.tot_success==0) {
		BKE_reportf(op->reports, RPT_ERROR, "File \"%s\" could not be loaded", seq_load.path);
		return OPERATOR_CANCELLED;
	}

	sort_seq(scene);
	seq_update_muting(scene, ed);

	WM_event_add_notifier(C, NC_SCENE|ND_SEQUENCER, scene);

	return OPERATOR_FINISHED;
}

/* add movie operator */
static int sequencer_add_movie_strip_exec(bContext *C, wmOperator *op)
{
	return sequencer_add_generic_strip_exec(C, op, sequencer_add_movie_strip);
}


static int sequencer_add_movie_strip_invoke(bContext *C, wmOperator *op, wmEvent *event)
{	
	sequencer_generic_invoke_xy__internal(C, op, event, 0);
	return WM_operator_filesel(C, op, event);
	//return sequencer_add_movie_strip_exec(C, op);
}


void SEQUENCER_OT_movie_strip_add(struct wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "Add Movie Strip";
	ot->idname= "SEQUENCER_OT_movie_strip_add";
	ot->description= "Add a movie strip to the sequencer";

	/* api callbacks */
	ot->invoke= sequencer_add_movie_strip_invoke;
	ot->exec= sequencer_add_movie_strip_exec;

	ot->poll= ED_operator_sequencer_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	WM_operator_properties_filesel(ot, FOLDERFILE|MOVIEFILE, FILE_SPECIAL, FILE_OPENFILE);
	sequencer_generic_props__internal(ot, SEQPROP_STARTFRAME|SEQPROP_FILES);
	RNA_def_boolean(ot->srna, "sound", TRUE, "Sound", "Load sound with the movie");
}

/* add sound operator */

static int sequencer_add_sound_strip_exec(bContext *C, wmOperator *op)
{
	return sequencer_add_generic_strip_exec(C, op, sequencer_add_sound_strip);
}

static int sequencer_add_sound_strip_invoke(bContext *C, wmOperator *op, wmEvent *event)
{	
	sequencer_generic_invoke_xy__internal(C, op, event, 0);
	return WM_operator_filesel(C, op, event);
	//return sequencer_add_sound_strip_exec(C, op);
}


void SEQUENCER_OT_sound_strip_add(struct wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "Add Sound Strip";
	ot->idname= "SEQUENCER_OT_sound_strip_add";
	ot->description= "Add a sound strip to the sequencer";

	/* api callbacks */
	ot->invoke= sequencer_add_sound_strip_invoke;
	ot->exec= sequencer_add_sound_strip_exec;

	ot->poll= ED_operator_sequencer_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	WM_operator_properties_filesel(ot, FOLDERFILE|SOUNDFILE, FILE_SPECIAL, FILE_OPENFILE);
	sequencer_generic_props__internal(ot, SEQPROP_STARTFRAME|SEQPROP_FILES);
	RNA_def_boolean(ot->srna, "cache", FALSE, "Cache", "Cache the sound in memory.");
}

/* add image operator */
static int sequencer_add_image_strip_exec(bContext *C, wmOperator *op)
{
	/* cant use the generic function for this */

	Scene *scene= CTX_data_scene(C); /* only for sound */
	Editing *ed= seq_give_editing(scene, TRUE);
	SeqLoadInfo seq_load;
	Sequence *seq;

	Strip *strip;
	StripElem *se;

	seq_load_operator_info(&seq_load, op);

	/* images are unique in how they handle this - 1 per strip elem */
	seq_load.len= RNA_property_collection_length(op->ptr, RNA_struct_find_property(op->ptr, "files"));

	if(seq_load.len==0)
		seq_load.len= 1;

	if(seq_load.flag & SEQ_LOAD_REPLACE_SEL)
		deselect_all_seq(scene);

	
	/* main adding function */
	seq= sequencer_add_image_strip(C, ed->seqbasep, &seq_load);
	strip= seq->strip;
	se= strip->stripdata;

	if(seq_load.len > 1) {
		RNA_BEGIN(op->ptr, itemptr, "files") {
			RNA_string_get(&itemptr, "name", se->name);
			se++;
		}
		RNA_END;
	}
	else {
		BLI_split_dirfile_basic(seq_load.path, NULL, se->name);
		if(seq_load.start_frame < seq_load.end_frame) {
			seq->endstill= seq_load.end_frame - seq_load.start_frame;
		}
	}
	
	calc_sequence_disp(scene, seq);

	sort_seq(scene);

	/* last active name */
	strncpy(ed->act_imagedir, strip->dir, FILE_MAXDIR-1);
	
	WM_event_add_notifier(C, NC_SCENE|ND_SEQUENCER, scene);

	return OPERATOR_FINISHED;
}

static int sequencer_add_image_strip_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	sequencer_generic_invoke_xy__internal(C, op, event, SEQPROP_ENDFRAME);
	return WM_operator_filesel(C, op, event);	
	//return sequencer_add_image_strip_exec(C, op);
}


void SEQUENCER_OT_image_strip_add(struct wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "Add Image Strip";
	ot->idname= "SEQUENCER_OT_image_strip_add";
	ot->description= "Add an image or image sequence to the sequencer";

	/* api callbacks */
	ot->invoke= sequencer_add_image_strip_invoke;
	ot->exec= sequencer_add_image_strip_exec;

	ot->poll= ED_operator_sequencer_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	WM_operator_properties_filesel(ot, FOLDERFILE|IMAGEFILE, FILE_SPECIAL, FILE_OPENFILE);
	sequencer_generic_props__internal(ot, SEQPROP_STARTFRAME|SEQPROP_ENDFRAME|SEQPROP_FILES);
}


/* add_effect_strip operator */
static int sequencer_add_effect_strip_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Editing *ed= seq_give_editing(scene, TRUE);

	Sequence *seq;	/* generic strip vars */
	Strip *strip;
	StripElem *se;
	struct SeqEffectHandle sh;

	int start_frame, end_frame, channel, type; /* operator props */
	
	Sequence *seq1, *seq2, *seq3;
	char *error_msg;

	start_frame= RNA_int_get(op->ptr, "start_frame");
	end_frame= RNA_int_get(op->ptr, "end_frame");
	channel= RNA_int_get(op->ptr, "channel");

	type= RNA_enum_get(op->ptr, "type");
	
	// XXX We need unique names and move to invoke
	if(!seq_effect_find_selected(scene, NULL, type, &seq1, &seq2, &seq3, &error_msg)) {
		BKE_report(op->reports, RPT_ERROR, error_msg);
		return OPERATOR_CANCELLED;
	}

	/* If seq1 is NULL and no error was rasied it means the seq is standalone
	 * (like color strips) and we need to check its start and end frames are valid */
	if (seq1==NULL && end_frame <= start_frame) {
		BKE_report(op->reports, RPT_ERROR, "Start and end frame are not set");
		return OPERATOR_CANCELLED;
	}

	seq = alloc_sequence(ed->seqbasep, start_frame, channel);
	seq->type= type;

	if(RNA_property_is_set(op->ptr, "name"))
		RNA_string_get(op->ptr, "name", seq->name+2);
	else
		strcpy(seq->name+2, give_seqname(seq));

	seqUniqueName(ed->seqbasep, seq);

	sh = get_sequence_effect(seq);

	seq->seq1= seq1;
	seq->seq2= seq2;
	seq->seq3= seq3;

	sh.init(seq);

	if (!seq1) { /* effect has no deps */
		seq->len= 1;
		seq_tx_set_final_right(seq, end_frame);
	}

	seq->flag |= SEQ_USE_EFFECT_DEFAULT_FADE;

	calc_sequence(scene, seq);
	
	/* basic defaults */
	seq->strip= strip= MEM_callocN(sizeof(Strip), "strip");
	strip->len = seq->len;
	strip->us= 1;
	if(seq->len>0)
		strip->stripdata= se= MEM_callocN(seq->len*sizeof(StripElem), "stripelem");

	if (seq->type==SEQ_PLUGIN) {
		char path[FILE_MAX];
		RNA_string_get(op->ptr, "path", path);

		sh.init_plugin(seq, path);

		if(seq->plugin==NULL) {
			BLI_remlink(ed->seqbasep, seq);
			seq_free_sequence(scene, seq);
			BKE_reportf(op->reports, RPT_ERROR, "Sequencer plugin \"%s\" could not load.", path);
			return OPERATOR_CANCELLED;
		}
	}
	else if (seq->type==SEQ_COLOR) {
		SolidColorVars *colvars= (SolidColorVars *)seq->effectdata;
		RNA_float_get_array(op->ptr, "color", colvars->col);
	}

	if(seq_test_overlap(ed->seqbasep, seq)) shuffle_seq(ed->seqbasep, seq, scene);

	update_changed_seq_and_deps(scene, seq, 1, 1); /* runs calc_sequence */


	/* not sure if this is needed with update_changed_seq_and_deps.
	 * it was NOT called in blender 2.4x, but wont hurt */
	sort_seq(scene); 

	if (RNA_boolean_get(op->ptr, "replace_sel")) {
		deselect_all_seq(scene);
		active_seq_set(scene, seq);
		seq->flag |= SELECT;
	}

	WM_event_add_notifier(C, NC_SCENE|ND_SEQUENCER, scene);

	return OPERATOR_FINISHED;
}


/* add color */
static int sequencer_add_effect_strip_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	sequencer_generic_invoke_xy__internal(C, op, event, SEQPROP_ENDFRAME);

	if (RNA_property_is_set(op->ptr, "type") && RNA_enum_get(op->ptr, "type")==SEQ_PLUGIN) {
		/* only plugins need the file selector */
		return WM_operator_filesel(C, op, event);
	}
	else {
		return sequencer_add_effect_strip_exec(C, op);
	}
}

void SEQUENCER_OT_effect_strip_add(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add Effect Strip";
	ot->idname= "SEQUENCER_OT_effect_strip_add";
	ot->description= "Add an effect to the sequencer, most are applied on top of existing strips";

	/* api callbacks */
	ot->invoke= sequencer_add_effect_strip_invoke;
	ot->exec= sequencer_add_effect_strip_exec;

	ot->poll= ED_operator_sequencer_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	WM_operator_properties_filesel(ot, 0, FILE_SPECIAL, FILE_OPENFILE);
	sequencer_generic_props__internal(ot, SEQPROP_STARTFRAME|SEQPROP_ENDFRAME);
	RNA_def_enum(ot->srna, "type", sequencer_prop_effect_types, SEQ_CROSS, "Type", "Sequencer effect type");
	RNA_def_float_vector(ot->srna, "color", 3, NULL, 0.0f, 1.0f, "Color", "Initialize the strip with this color (only used when type='COLOR')", 0.0f, 1.0f);
}
