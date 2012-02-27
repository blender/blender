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
 * Contributor(s): Blender Foundation (2008)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_sequencer.c
 *  \ingroup RNA
 */


#include <stdlib.h>
#include <limits.h>

#include "RNA_access.h"
#include "RNA_define.h"

#include "rna_internal.h"

#include "DNA_anim_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "BKE_animsys.h"
#include "BKE_global.h"
#include "BKE_sequencer.h"
#include "BKE_sound.h"

#include "MEM_guardedalloc.h"

#include "WM_types.h"
#include "BLI_math.h"

#ifdef RNA_RUNTIME

/* build a temp referene to the parent */
static void meta_tmp_ref(Sequence *seq_par, Sequence *seq)
{
	for (; seq; seq= seq->next) {
		seq->tmp= seq_par;
		if(seq->type == SEQ_META) {
			meta_tmp_ref(seq, seq->seqbase.first);
		}
	}
}

static void rna_SequenceEditor_sequences_all_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Scene *scene= (Scene*)ptr->id.data;
	Editing *ed= seq_give_editing(scene, FALSE);

	meta_tmp_ref(NULL, ed->seqbase.first);

	rna_iterator_listbase_begin(iter, &ed->seqbase, NULL);
}

static void rna_SequenceEditor_sequences_all_next(CollectionPropertyIterator *iter)
{
	ListBaseIterator *internal= iter->internal;
	Sequence *seq= (Sequence*)internal->link;

	if(seq->seqbase.first)
		internal->link= (Link*)seq->seqbase.first;
	else if(seq->next)
		internal->link= (Link*)seq->next;
	else {
		internal->link= NULL;

		do {
			seq= seq->tmp; // XXX - seq's dont reference their parents!
			if(seq && seq->next) {
				internal->link= (Link*)seq->next;
				break;
			}
		} while(seq);
	}

	iter->valid= (internal->link != NULL);
}

/* internal use */
static int rna_SequenceEditor_elements_length(PointerRNA *ptr)
{
	Sequence *seq= (Sequence*)ptr->data;

	/* Hack? copied from sequencer.c::reload_sequence_new_file() */
	size_t olen = MEM_allocN_len(seq->strip->stripdata)/sizeof(struct StripElem);
	
	/* the problem with seq->strip->len and seq->len is that it's discounted from the offset (hard cut trim) */
	return (int) olen;
}

static void rna_SequenceEditor_elements_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Sequence *seq= (Sequence*)ptr->data;
	rna_iterator_array_begin(iter, (void*)seq->strip->stripdata, sizeof(StripElem), rna_SequenceEditor_elements_length(ptr), 0, NULL);
}

static void rna_Sequence_frame_change_update(Scene *scene, Sequence *seq)
{
	Editing *ed= seq_give_editing(scene, FALSE);
	ListBase *seqbase= seq_seqbase(&ed->seqbase, seq);
	calc_sequence_disp(scene, seq);

	if(seq_test_overlap(seqbase, seq)) {
		shuffle_seq(seqbase, seq, scene); // XXX - BROKEN!, uses context seqbasep
	}
	sort_seq(scene);
}

static void rna_Sequence_start_frame_set(PointerRNA *ptr, int value)
{
	Sequence *seq= (Sequence*)ptr->data;
	Scene *scene= (Scene*)ptr->id.data;
	
	seq_translate(scene, seq, value - seq->start);
	rna_Sequence_frame_change_update(scene, seq);
}

static void rna_Sequence_start_frame_final_set(PointerRNA *ptr, int value)
{
	Sequence *seq= (Sequence*)ptr->data;
	Scene *scene= (Scene*)ptr->id.data;

	seq_tx_set_final_left(seq, value);
	seq_single_fix(seq);
	rna_Sequence_frame_change_update(scene, seq);
}

static void rna_Sequence_end_frame_final_set(PointerRNA *ptr, int value)
{
	Sequence *seq= (Sequence*)ptr->data;
	Scene *scene= (Scene*)ptr->id.data;

	seq_tx_set_final_right(seq, value);
	seq_single_fix(seq);
	rna_Sequence_frame_change_update(scene, seq);
}

static void rna_Sequence_anim_startofs_final_set(PointerRNA *ptr, int value)
{
	Sequence *seq= (Sequence*)ptr->data;
	Scene *scene= (Scene*)ptr->id.data;

	seq->anim_startofs = MIN2(value, seq->len + seq->anim_startofs);

	reload_sequence_new_file(scene, seq, FALSE);
	rna_Sequence_frame_change_update(scene, seq);
}

static void rna_Sequence_anim_endofs_final_set(PointerRNA *ptr, int value)
{
	Sequence *seq= (Sequence*)ptr->data;
	Scene *scene= (Scene*)ptr->id.data;

	seq->anim_endofs = MIN2(value, seq->len + seq->anim_endofs);

	reload_sequence_new_file(scene, seq, FALSE);
	rna_Sequence_frame_change_update(scene, seq);
}

static void rna_Sequence_frame_length_set(PointerRNA *ptr, int value)
{
	Sequence *seq= (Sequence*)ptr->data;
	Scene *scene= (Scene*)ptr->id.data;
	
	seq_tx_set_final_right(seq, seq->start+value);
	rna_Sequence_frame_change_update(scene, seq);
}

static int rna_Sequence_frame_length_get(PointerRNA *ptr)
{
	Sequence *seq= (Sequence*)ptr->data;
	return seq_tx_get_final_right(seq, 0)-seq_tx_get_final_left(seq, 0);
}

static int rna_Sequence_frame_editable(PointerRNA *ptr)
{
	Sequence *seq = (Sequence*)ptr->data;
	/* Effect sequences' start frame and length must be readonly! */
	return (get_sequence_effect_num_inputs(seq->type))? 0: PROP_EDITABLE;
}

static void rna_Sequence_channel_set(PointerRNA *ptr, int value)
{
	Sequence *seq= (Sequence*)ptr->data;
	Scene *scene= (Scene*)ptr->id.data;
	Editing *ed= seq_give_editing(scene, FALSE);
	ListBase *seqbase= seq_seqbase(&ed->seqbase, seq);

	seq->machine= value;
	
	if( seq_test_overlap(seqbase, seq) ) {
		shuffle_seq(seqbase, seq, scene);  // XXX - BROKEN!, uses context seqbasep
	}
	sort_seq(scene);
}

/* properties that need to allocate structs */
static void rna_Sequence_use_color_balance_set(PointerRNA *ptr, int value)
{
	Sequence *seq= (Sequence*)ptr->data;
	int c;
	
	if(value) {
		seq->flag |= SEQ_USE_COLOR_BALANCE;
		if(seq->strip->color_balance == NULL) {
			seq->strip->color_balance = MEM_callocN(sizeof(struct StripColorBalance), "StripColorBalance");
			for (c=0; c<3; c++) {
				seq->strip->color_balance->lift[c] = 1.0f;
				seq->strip->color_balance->gamma[c] = 1.0f;
				seq->strip->color_balance->gain[c] = 1.0f;
			}
		}
	} else {
		seq->flag ^= SEQ_USE_COLOR_BALANCE;
	}
}

static void rna_Sequence_use_proxy_set(PointerRNA *ptr, int value)
{
	Sequence *seq= (Sequence*)ptr->data;
	if(value) {
		seq->flag |= SEQ_USE_PROXY;
		if(seq->strip->proxy == NULL) {
			seq->strip->proxy = MEM_callocN(sizeof(struct StripProxy), "StripProxy");
			seq->strip->proxy->quality = 90;
			seq->strip->proxy->build_tc_flags = SEQ_PROXY_TC_ALL;
			seq->strip->proxy->build_size_flags 
				= SEQ_PROXY_IMAGE_SIZE_25;
		}
	} else {
		seq->flag ^= SEQ_USE_PROXY;
	}
}

static void rna_Sequence_use_translation_set(PointerRNA *ptr, int value)
{
	Sequence *seq= (Sequence*)ptr->data;
	if(value) {
		seq->flag |= SEQ_USE_TRANSFORM;
		if(seq->strip->transform == NULL) {
			seq->strip->transform = MEM_callocN(sizeof(struct StripTransform), "StripTransform");
		}
	} else {
		seq->flag ^= SEQ_USE_TRANSFORM;
	}
}

static void rna_Sequence_use_crop_set(PointerRNA *ptr, int value)
{
	Sequence *seq= (Sequence*)ptr->data;
	if(value) {
		seq->flag |= SEQ_USE_CROP;
		if(seq->strip->crop == NULL) {
			seq->strip->crop = MEM_callocN(sizeof(struct StripCrop), "StripCrop");
		}
	} else {
		seq->flag ^= SEQ_USE_CROP;
	}
}

static int transform_seq_cmp_cb(Sequence *seq, void *arg_pt)
{
	struct { Sequence *seq; void *transform; } *data= arg_pt;

	if(seq->strip && seq->strip->transform == data->transform) {
		data->seq= seq;
		return -1; /* done so bail out */
	}
	return 1;
}

static char *rna_SequenceTransform_path(PointerRNA *ptr)
{
	Scene *scene= ptr->id.data;
	Editing *ed= seq_give_editing(scene, FALSE);
	Sequence *seq;

	struct { Sequence *seq; void *transform; } data;
	data.seq= NULL;
	data.transform= ptr->data;

	/* irritating we need to search for our sequence! */
	seqbase_recursive_apply(&ed->seqbase, transform_seq_cmp_cb, &data);
	seq= data.seq;

	if (seq && seq->name+2)
		return BLI_sprintfN("sequence_editor.sequences_all[\"%s\"].transform", seq->name+2);
	else
		return BLI_strdup("");
}

static int crop_seq_cmp_cb(Sequence *seq, void *arg_pt)
{
	struct { Sequence *seq; void *crop; } *data= arg_pt;

	if(seq->strip && seq->strip->crop == data->crop) {
		data->seq= seq;
		return -1; /* done so bail out */
	}
	return 1;
}

static char *rna_SequenceCrop_path(PointerRNA *ptr)
{
	Scene *scene= ptr->id.data;
	Editing *ed= seq_give_editing(scene, FALSE);
	Sequence *seq;

	struct { Sequence *seq; void *crop; } data;
	data.seq= NULL;
	data.crop= ptr->data;

	/* irritating we need to search for our sequence! */
	seqbase_recursive_apply(&ed->seqbase, crop_seq_cmp_cb, &data);
	seq= data.seq;

	if (seq && seq->name+2)
		return BLI_sprintfN("sequence_editor.sequences_all[\"%s\"].crop", seq->name+2);
	else
		return BLI_strdup("");
}


/* name functions that ignore the first two characters */
static void rna_Sequence_name_get(PointerRNA *ptr, char *value)
{
	Sequence *seq= (Sequence*)ptr->data;
	BLI_strncpy(value, seq->name+2, sizeof(seq->name)-2);
}

static int rna_Sequence_name_length(PointerRNA *ptr)
{
	Sequence *seq= (Sequence*)ptr->data;
	return strlen(seq->name+2);
}

static void rna_Sequence_name_set(PointerRNA *ptr, const char *value)
{
	Scene *scene= (Scene*)ptr->id.data;
	Sequence *seq= (Sequence*)ptr->data;
	char oldname[sizeof(seq->name)];
	AnimData *adt;
	
	/* make a copy of the old name first */
	BLI_strncpy(oldname, seq->name+2, sizeof(seq->name)-2);
	
	/* copy the new name into the name slot */
	BLI_strncpy_utf8(seq->name+2, value, sizeof(seq->name)-2);
	
	/* make sure the name is unique */
	seqbase_unique_name_recursive(&scene->ed->seqbase, seq);
	
	/* fix all the animation data which may link to this */

	/* dont rename everywhere because these are per scene */
	/* BKE_all_animdata_fix_paths_rename("sequence_editor.sequences_all", oldname, seq->name+2); */
	adt= BKE_animdata_from_id(&scene->id);
	if(adt)
		BKE_animdata_fix_paths_rename(&scene->id, adt, "sequence_editor.sequences_all", oldname, seq->name+2, 0, 0, 1);
}

static StructRNA* rna_Sequence_refine(struct PointerRNA *ptr)
{
	Sequence *seq= (Sequence*)ptr->data;

	switch(seq->type) {
		case SEQ_IMAGE:
			return &RNA_ImageSequence;
		case SEQ_META:
			return &RNA_MetaSequence;
		case SEQ_SCENE:
			return &RNA_SceneSequence;
		case SEQ_MOVIE:
			return &RNA_MovieSequence;
		case SEQ_SOUND:
			return &RNA_SoundSequence;
		case SEQ_CROSS:
		case SEQ_ADD:
		case SEQ_SUB:
		case SEQ_ALPHAOVER:
		case SEQ_ALPHAUNDER:
		case SEQ_GAMCROSS:
		case SEQ_MUL:
		case SEQ_OVERDROP:
			return &RNA_EffectSequence;
		case SEQ_MULTICAM:
			return &RNA_MulticamSequence;
		case SEQ_ADJUSTMENT:
			return &RNA_AdjustmentSequence;
		case SEQ_PLUGIN:
			return &RNA_PluginSequence;
		case SEQ_WIPE:
			return &RNA_WipeSequence;
		case SEQ_GLOW:
			return &RNA_GlowSequence;
		case SEQ_TRANSFORM:
			return &RNA_TransformSequence;
		case SEQ_COLOR:
			return &RNA_ColorSequence;
		case SEQ_SPEED:
			return &RNA_SpeedControlSequence;
		default:
			return &RNA_Sequence;
	}
}

static char *rna_Sequence_path(PointerRNA *ptr)
{
	Sequence *seq= (Sequence*)ptr->data;
	
	/* sequencer data comes from scene... 
	 * TODO: would be nice to make SequenceEditor data a datablock of its own (for shorter paths)
	 */
	if (seq->name+2)
		return BLI_sprintfN("sequence_editor.sequences_all[\"%s\"]", seq->name+2);
	else
		return BLI_strdup("");
}

static PointerRNA rna_SequenceEditor_meta_stack_get(CollectionPropertyIterator *iter)
{
	ListBaseIterator *internal= iter->internal;
	MetaStack *ms= (MetaStack*)internal->link;

	return rna_pointer_inherit_refine(&iter->parent, &RNA_Sequence, ms->parseq);
}

/* TODO, expose seq path setting as a higher level sequencer BKE function */
static void rna_Sequence_filepath_set(PointerRNA *ptr, const char *value)
{
	Sequence *seq= (Sequence*)(ptr->data);

	if(seq->type == SEQ_SOUND && seq->sound) {
		/* for sound strips we need to update the sound as well.
		 * arguably, this could load in a new sound rather than modify an existing one.
		 * but while using the sequencer its most likely your not using the sound in the game engine too.
		 */
		PointerRNA id_ptr;
		RNA_id_pointer_create((ID *)seq->sound, &id_ptr);
		RNA_string_set(&id_ptr, "filepath", value);
		sound_load(G.main, seq->sound);
		sound_update_scene_sound(seq->scene_sound, seq->sound);
	}

	BLI_split_dirfile(value, seq->strip->dir, seq->strip->stripdata->name, sizeof(seq->strip->dir), sizeof(seq->strip->stripdata->name));
}

static void rna_Sequence_filepath_get(PointerRNA *ptr, char *value)
{
	Sequence *seq= (Sequence*)(ptr->data);

	BLI_join_dirfile(value, FILE_MAX, seq->strip->dir, seq->strip->stripdata->name);
}

static int rna_Sequence_filepath_length(PointerRNA *ptr)
{
	Sequence *seq= (Sequence*)(ptr->data);
	char path[FILE_MAX];

	BLI_join_dirfile(path, sizeof(path), seq->strip->dir, seq->strip->stripdata->name);
	return strlen(path);
}

static void rna_Sequence_proxy_filepath_set(PointerRNA *ptr, const char *value)
{
	StripProxy *proxy= (StripProxy*)(ptr->data);
	BLI_split_dirfile(value, proxy->dir, proxy->file, sizeof(proxy->dir), sizeof(proxy->file));
}

static void rna_Sequence_proxy_filepath_get(PointerRNA *ptr, char *value)
{
	StripProxy *proxy= (StripProxy*)(ptr->data);

	BLI_join_dirfile(value, FILE_MAX, proxy->dir, proxy->file);
}

static int rna_Sequence_proxy_filepath_length(PointerRNA *ptr)
{
	StripProxy *proxy= (StripProxy*)(ptr->data);
	char path[FILE_MAX];

	BLI_join_dirfile(path, sizeof(path), proxy->dir, proxy->file);
	return strlen(path);
}

static void rna_Sequence_volume_set(PointerRNA *ptr, float value)
{
	Sequence *seq= (Sequence*)(ptr->data);

	seq->volume = value;
	if(seq->scene_sound)
		sound_set_scene_sound_volume(seq->scene_sound, value, (seq->flag & SEQ_AUDIO_VOLUME_ANIMATED) != 0);
}

static void rna_Sequence_pitch_set(PointerRNA *ptr, float value)
{
	Sequence *seq= (Sequence*)(ptr->data);

	seq->pitch = value;
	if(seq->scene_sound)
		sound_set_scene_sound_pitch(seq->scene_sound, value, (seq->flag & SEQ_AUDIO_PITCH_ANIMATED) != 0);
}

static void rna_Sequence_pan_set(PointerRNA *ptr, float value)
{
	Sequence *seq= (Sequence*)(ptr->data);

	seq->pan = value;
	if(seq->scene_sound)
		sound_set_scene_sound_pan(seq->scene_sound, value, (seq->flag & SEQ_AUDIO_PAN_ANIMATED) != 0);
}


static int rna_Sequence_input_count_get(PointerRNA *ptr)
{
	Sequence *seq= (Sequence*)(ptr->data);

	return get_sequence_effect_num_inputs(seq->type);
}
/*static void rna_SoundSequence_filename_set(PointerRNA *ptr, const char *value)
{
	Sequence *seq= (Sequence*)(ptr->data);
	BLI_split_dirfile(value, seq->strip->dir, seq->strip->stripdata->name, sizeof(seq->strip->dir), sizeof(seq->strip->stripdata->name));
}

static void rna_SequenceElement_filename_set(PointerRNA *ptr, const char *value)
{
	StripElem *elem= (StripElem*)(ptr->data);
	BLI_split_file_part(value, elem->name, sizeof(elem->name));
}*/

static void rna_Sequence_update(Main *UNUSED(bmain), Scene *scene, PointerRNA *UNUSED(ptr))
{
	Editing *ed= seq_give_editing(scene, FALSE);

	if(ed)
		free_imbuf_seq(scene, &ed->seqbase, FALSE, TRUE);
}

static void rna_Sequence_update_reopen_files(Main *UNUSED(bmain), Scene *scene, PointerRNA *ptr)
{
	Editing *ed= seq_give_editing(scene, FALSE);

	free_imbuf_seq(scene, &ed->seqbase, FALSE, FALSE);

	if(RNA_struct_is_a(ptr->type, &RNA_SoundSequence))
		seq_update_sound_bounds(scene, ptr->data);
}

static void rna_Sequence_mute_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	Editing *ed= seq_give_editing(scene, FALSE);

	seq_update_muting(ed);
	rna_Sequence_update(bmain, scene, ptr);
}

static void rna_Sequence_filepath_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	Sequence *seq= (Sequence*)(ptr->data);
	reload_sequence_new_file(scene, seq, TRUE);
	calc_sequence(scene, seq);
	rna_Sequence_update(bmain, scene, ptr);
}

static int seqproxy_seq_cmp_cb(Sequence *seq, void *arg_pt)
{
	struct { Sequence *seq; void *seq_proxy; } *data= arg_pt;

	if(seq->strip && seq->strip->proxy == data->seq_proxy) {
		data->seq= seq;
		return -1; /* done so bail out */
	}
	return 1;
}

static void rna_Sequence_tcindex_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	Editing *ed= seq_give_editing(scene, FALSE);
	Sequence *seq;

	struct { Sequence *seq; void *seq_proxy; } data;

	data.seq= NULL;
	data.seq_proxy = ptr->data;

	seqbase_recursive_apply(&ed->seqbase, seqproxy_seq_cmp_cb, &data);
	seq= data.seq;

	reload_sequence_new_file(scene, seq, FALSE);
	rna_Sequence_frame_change_update(scene, seq);
}

/* do_versions? */
static float rna_Sequence_opacity_get(PointerRNA *ptr)
{
	Sequence *seq= (Sequence*)(ptr->data);
	return seq->blend_opacity / 100.0f;
}
static void rna_Sequence_opacity_set(PointerRNA *ptr, float value)
{
	Sequence *seq= (Sequence*)(ptr->data);
	CLAMP(value, 0.0f, 1.0f);
	seq->blend_opacity = value * 100.0f;
}


static int colbalance_seq_cmp_cb(Sequence *seq, void *arg_pt)
{
	struct { Sequence *seq; void *color_balance; } *data= arg_pt;

	if(seq->strip && seq->strip->color_balance == data->color_balance) {
		data->seq= seq;
		return -1; /* done so bail out */
	}
	return 1;
}
static char *rna_SequenceColorBalance_path(PointerRNA *ptr)
{
	Scene *scene= ptr->id.data;
	Editing *ed= seq_give_editing(scene, FALSE);
	Sequence *seq;

	struct { Sequence *seq; void *color_balance; } data;
	data.seq= NULL;
	data.color_balance= ptr->data;

	/* irritating we need to search for our sequence! */
	seqbase_recursive_apply(&ed->seqbase, colbalance_seq_cmp_cb, &data);
	seq= data.seq;

	if (seq && seq->name+2)
		return BLI_sprintfN("sequence_editor.sequences_all[\"%s\"].color_balance", seq->name+2);
	else
		return BLI_strdup("");
}

static void rna_SequenceEditor_overlay_lock_set(PointerRNA *ptr, int value)
{
	Scene *scene= ptr->id.data;
	Editing *ed= seq_give_editing(scene, FALSE);

	if(ed==NULL)
		return;

	/* convert from abs to relative and back */
	if((ed->over_flag & SEQ_EDIT_OVERLAY_ABS)==0 && value) {
		ed->over_cfra= scene->r.cfra + ed->over_ofs;
		ed->over_flag |= SEQ_EDIT_OVERLAY_ABS;
	}
	else if((ed->over_flag & SEQ_EDIT_OVERLAY_ABS) && !value) {
		ed->over_ofs= ed->over_cfra - scene->r.cfra;
		ed->over_flag &= ~SEQ_EDIT_OVERLAY_ABS;
	}
}

static int rna_SequenceEditor_overlay_frame_get(PointerRNA *ptr)
{
	Scene *scene= (Scene *)ptr->id.data;
	Editing *ed= seq_give_editing(scene, FALSE);

	if(ed==NULL)
		return scene->r.cfra;

	if(ed->over_flag & SEQ_EDIT_OVERLAY_ABS)
		return ed->over_cfra - scene->r.cfra;
	else
		return ed->over_ofs;

}

static void rna_SequenceEditor_overlay_frame_set(PointerRNA *ptr, int value)
{
	Scene *scene= (Scene *)ptr->id.data;
	Editing *ed= seq_give_editing(scene, FALSE);

	if(ed==NULL)
		return;


	if(ed->over_flag & SEQ_EDIT_OVERLAY_ABS)
		ed->over_cfra= (scene->r.cfra + value);
	else
		ed->over_ofs= value;
}


static void rna_WipeSequence_angle_set(PointerRNA *ptr, float value)
{
	Sequence *seq= (Sequence *)(ptr->data);
	value= RAD2DEGF(value);
	CLAMP(value, -90.0f, 90.0f);
	((WipeVars *)seq->effectdata)->angle= value;
}

static float rna_WipeSequence_angle_get(PointerRNA *ptr)
{
	Sequence *seq= (Sequence *)(ptr->data);

	return DEG2RADF(((WipeVars *)seq->effectdata)->angle);
}


#else

static void rna_def_strip_element(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "SequenceElement", NULL);
	RNA_def_struct_ui_text(srna, "Sequence Element", "Sequence strip data for a single frame");
	RNA_def_struct_sdna(srna, "StripElem");
	
	prop= RNA_def_property(srna, "filename", PROP_STRING, PROP_FILENAME);
	RNA_def_property_string_sdna(prop, NULL, "name");
	RNA_def_property_ui_text(prop, "Filename", "");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");

	prop= RNA_def_property(srna, "orig_width", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "orig_width");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Orig Width", "Original image width");

	prop= RNA_def_property(srna, "orig_height", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "orig_height");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Orig Height", "Original image height");
}

static void rna_def_strip_crop(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "SequenceCrop", NULL);
	RNA_def_struct_ui_text(srna, "Sequence Crop", "Cropping parameters for a sequence strip");
	RNA_def_struct_sdna(srna, "StripCrop");

	prop= RNA_def_property(srna, "max_y", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "top");
	RNA_def_property_ui_text(prop, "Top", "");
	RNA_def_property_ui_range(prop, 0, 4096, 1, 0);
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
	
	prop= RNA_def_property(srna, "min_y", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "bottom");
	RNA_def_property_ui_text(prop, "Bottom", "");
	RNA_def_property_ui_range(prop, 0, 4096, 1, 0);
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
	
	prop= RNA_def_property(srna, "min_x", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "left");
	RNA_def_property_ui_text(prop, "Left", "");
	RNA_def_property_ui_range(prop, 0, 4096, 1, 0);
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");

	prop= RNA_def_property(srna, "max_x", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "right");
	RNA_def_property_ui_text(prop, "Right", "");
	RNA_def_property_ui_range(prop, 0, 4096, 1, 0);
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");

	RNA_def_struct_path_func(srna, "rna_SequenceCrop_path");
}

static void rna_def_strip_transform(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "SequenceTransform", NULL);
	RNA_def_struct_ui_text(srna, "Sequence Transform", "Transform parameters for a sequence strip");
	RNA_def_struct_sdna(srna, "StripTransform");

	prop= RNA_def_property(srna, "offset_x", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "xofs");
	RNA_def_property_ui_text(prop, "Offset X", "");
	RNA_def_property_ui_range(prop, -4096, 4096, 1, 0);
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");

	prop= RNA_def_property(srna, "offset_y", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "yofs");
	RNA_def_property_ui_text(prop, "Offset Y", "");
	RNA_def_property_ui_range(prop, -4096, 4096, 1, 0);
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");

	RNA_def_struct_path_func(srna, "rna_SequenceTransform_path");

}

static void rna_def_strip_proxy(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static const EnumPropertyItem seq_tc_items[]= {
		{SEQ_PROXY_TC_NONE, "NONE", 0, "No TC in use", ""},
		{SEQ_PROXY_TC_RECORD_RUN, "RECORD_RUN", 0, "Record Run",
		                          "Use images in the order as they are recorded"},
		{SEQ_PROXY_TC_FREE_RUN, "FREE_RUN", 0, "Free Run",
		                        "Use global timestamp written by recording device"},
		{SEQ_PROXY_TC_INTERP_REC_DATE_FREE_RUN, "FREE_RUN_REC_DATE", 0, "Free Run (rec date)",
		                                        "Interpolate a global timestamp using the "
		                                        "record date and time written by recording device"},
		{SEQ_PROXY_TC_RECORD_RUN_NO_GAPS, "FREE_RUN_NO_GAPS", 0, "Free Run No Gaps",
		                                        "Record run, but ignore timecode, "
		                                        "changes in framerate or dropouts"},
		{0, NULL, 0, NULL, NULL}};
	
	srna = RNA_def_struct(brna, "SequenceProxy", NULL);
	RNA_def_struct_ui_text(srna, "Sequence Proxy", "Proxy parameters for a sequence strip");
	RNA_def_struct_sdna(srna, "StripProxy");
	
	prop= RNA_def_property(srna, "directory", PROP_STRING, PROP_DIRPATH);
	RNA_def_property_string_sdna(prop, NULL, "dir");
	RNA_def_property_ui_text(prop, "Directory", "Location to store the proxy files");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
	
	prop= RNA_def_property(srna, "filepath", PROP_STRING, PROP_FILEPATH);
	RNA_def_property_ui_text(prop, "Path", "Location of custom proxy file");
	RNA_def_property_string_funcs(prop, "rna_Sequence_proxy_filepath_get", "rna_Sequence_proxy_filepath_length", "rna_Sequence_proxy_filepath_set");

	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");

	prop= RNA_def_property(srna, "build_25", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "build_size_flags", SEQ_PROXY_IMAGE_SIZE_25);
	RNA_def_property_ui_text(prop, "25%", "Build 25% proxy resolution");

	prop= RNA_def_property(srna, "build_50", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "build_size_flags", SEQ_PROXY_IMAGE_SIZE_50);
	RNA_def_property_ui_text(prop, "50%", "Build 50% proxy resolution");

	prop= RNA_def_property(srna, "build_75", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "build_size_flags", SEQ_PROXY_IMAGE_SIZE_75);
	RNA_def_property_ui_text(prop, "75%", "Build 75% proxy resolution");

	prop= RNA_def_property(srna, "build_100", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "build_size_flags", SEQ_PROXY_IMAGE_SIZE_100);
	RNA_def_property_ui_text(prop, "100%", "Build 100% proxy resolution");

	prop= RNA_def_property(srna, "build_record_run", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "build_tc_flags", SEQ_PROXY_TC_RECORD_RUN);
	RNA_def_property_ui_text(prop, "Rec Run", "Build record run time code index");

	prop= RNA_def_property(srna, "build_free_run", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "build_tc_flags", SEQ_PROXY_TC_FREE_RUN);
	RNA_def_property_ui_text(prop, "Free Run", "Build free run time code index");

	prop= RNA_def_property(srna, "build_free_run_rec_date", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "build_tc_flags", SEQ_PROXY_TC_INTERP_REC_DATE_FREE_RUN);
	RNA_def_property_ui_text(prop, "Free Run (Rec Date)", "Build free run time code index using Record Date/Time");

	prop= RNA_def_property(srna, "quality", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "quality");
	RNA_def_property_ui_text(prop, "Quality", "JPEG Quality of proxies to build");
	RNA_def_property_ui_range(prop, 1, 100, 1, 0);

	prop= RNA_def_property(srna, "timecode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "tc");
	RNA_def_property_enum_items(prop, seq_tc_items);
	RNA_def_property_ui_text(prop, "Timecode", "");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_tcindex_update");

}

static void rna_def_strip_color_balance(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "SequenceColorBalance", NULL);
	RNA_def_struct_ui_text(srna, "Sequence Color Balance", "Color balance parameters for a sequence strip");
	RNA_def_struct_sdna(srna, "StripColorBalance");

	prop= RNA_def_property(srna, "lift", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_ui_text(prop, "Lift", "Color balance lift (shadows)");
	RNA_def_property_ui_range(prop, 0, 2, 0.1, 3);
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
	
	prop= RNA_def_property(srna, "gamma", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_ui_text(prop, "Gamma", "Color balance gamma (midtones)");
	RNA_def_property_ui_range(prop, 0, 2, 0.1, 3);
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
	
	prop= RNA_def_property(srna, "gain", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_ui_text(prop, "Gain", "Color balance gain (highlights)");
	RNA_def_property_ui_range(prop, 0, 2, 0.1, 3);
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
	
	prop= RNA_def_property(srna, "invert_gain", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_COLOR_BALANCE_INVERSE_GAIN);
	RNA_def_property_ui_text(prop, "Inverse Gain", "");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");

	prop= RNA_def_property(srna, "invert_gamma", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_COLOR_BALANCE_INVERSE_GAMMA);
	RNA_def_property_ui_text(prop, "Inverse Gamma", "");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");

	prop= RNA_def_property(srna, "invert_lift", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_COLOR_BALANCE_INVERSE_LIFT);
	RNA_def_property_ui_text(prop, "Inverse Lift", "");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");

	RNA_def_struct_path_func(srna, "rna_SequenceColorBalance_path");

	/* not yet used
	prop= RNA_def_property(srna, "exposure", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Exposure", "");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
	
	prop= RNA_def_property(srna, "saturation", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Saturation", "");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update"); */
}

static void rna_def_sequence(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static const EnumPropertyItem seq_type_items[]= {
		{SEQ_IMAGE, "IMAGE", 0, "Image", ""}, 
		{SEQ_META, "META", 0, "Meta", ""}, 
		{SEQ_SCENE, "SCENE", 0, "Scene", ""}, 
		{SEQ_MOVIE, "MOVIE", 0, "Movie", ""}, 
		{SEQ_SOUND, "SOUND", 0, "Sound", ""},
		{SEQ_CROSS, "CROSS", 0, "Cross", ""}, 
		{SEQ_ADD, "ADD", 0, "Add", ""}, 
		{SEQ_SUB, "SUBTRACT", 0, "Subtract", ""}, 
		{SEQ_ALPHAOVER, "ALPHA_OVER", 0, "Alpha Over", ""}, 
		{SEQ_ALPHAUNDER, "ALPHA_UNDER", 0, "Alpha Under", ""}, 
		{SEQ_GAMCROSS, "GAMMA_CROSS", 0, "Gamma Cross", ""}, 
		{SEQ_MUL, "MULTIPLY", 0, "Multiply", ""}, 
		{SEQ_OVERDROP, "OVER_DROP", 0, "Over Drop", ""}, 
		{SEQ_PLUGIN, "PLUGIN", 0, "Plugin", ""}, 
		{SEQ_WIPE, "WIPE", 0, "Wipe", ""}, 
		{SEQ_GLOW, "GLOW", 0, "Glow", ""}, 
		{SEQ_TRANSFORM, "TRANSFORM", 0, "Transform", ""}, 
		{SEQ_COLOR, "COLOR", 0, "Color", ""}, 
		{SEQ_SPEED, "SPEED", 0, "Speed", ""}, 
		{SEQ_MULTICAM, "MULTICAM", 0, "Multicam Selector", ""},
		{SEQ_ADJUSTMENT, "ADJUSTMENT", 0, "Adjustment Layer", ""},
		{0, NULL, 0, NULL, NULL}};

	static const EnumPropertyItem blend_mode_items[]= {
		{SEQ_BLEND_REPLACE, "REPLACE", 0, "Replace", ""}, 
		{SEQ_CROSS, "CROSS", 0, "Cross", ""}, 
		{SEQ_ADD, "ADD", 0, "Add", ""}, 
		{SEQ_SUB, "SUBTRACT", 0, "Subtract", ""}, 
		{SEQ_ALPHAOVER, "ALPHA_OVER", 0, "Alpha Over", ""}, 
		{SEQ_ALPHAUNDER, "ALPHA_UNDER", 0, "Alpha Under", ""}, 
		{SEQ_GAMCROSS, "GAMMA_CROSS", 0, "Gamma Cross", ""}, 
		{SEQ_MUL, "MULTIPLY", 0, "Multiply", ""}, 
		{SEQ_OVERDROP, "OVER_DROP", 0, "Over Drop", ""}, 
		{0, NULL, 0, NULL, NULL}};
	
	srna = RNA_def_struct(brna, "Sequence", NULL);
	RNA_def_struct_ui_text(srna, "Sequence", "Sequence strip in the sequence editor");
	RNA_def_struct_refine_func(srna, "rna_Sequence_refine");
	RNA_def_struct_path_func(srna, "rna_Sequence_path");

	prop= RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_Sequence_name_get", "rna_Sequence_name_length", "rna_Sequence_name_set");
	RNA_def_property_string_maxlength(prop, sizeof(((Sequence*)NULL)->name)-2);
	RNA_def_property_ui_text(prop, "Name", "");
	RNA_def_struct_name_property(srna, prop);
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, NULL);

	prop= RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_enum_items(prop, seq_type_items);
	RNA_def_property_ui_text(prop, "Type", "");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");

	//prop= RNA_def_property(srna, "ipo", PROP_POINTER, PROP_NONE);
	//RNA_def_property_ui_text(prop, "IPO Curves", "IPO curves used by this sequence");

	/* flags */

	prop= RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SELECT);
	RNA_def_property_ui_text(prop, "Select", "");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER|NA_SELECTED, NULL);

	prop= RNA_def_property(srna, "select_left_handle", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_LEFTSEL);
	RNA_def_property_ui_text(prop, "Left Handle Selected", "");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER|NA_SELECTED, NULL);

	prop= RNA_def_property(srna, "select_right_handle", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_RIGHTSEL);
	RNA_def_property_ui_text(prop, "Right Handle Selected", "");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER|NA_SELECTED, NULL);

	prop= RNA_def_property(srna, "mute", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_MUTE);
	RNA_def_property_ui_text(prop, "Mute", "");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_mute_update");

	prop= RNA_def_property(srna, "lock", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_LOCK);
	RNA_def_property_ui_text(prop, "Lock", "Lock strip so that it can't be transformed");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, NULL);

	prop= RNA_def_property(srna, "waveform", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_AUDIO_DRAW_WAVEFORM);
	RNA_def_property_ui_text(prop, "Draw Waveform", "Whether to draw the sound's waveform");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, NULL);

	/* strip positioning */

	prop= RNA_def_property(srna, "frame_final_duration", PROP_INT, PROP_TIME);
	RNA_def_property_range(prop, 1, MAXFRAME);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Length", "The length of the contents of this strip after the handles are applied");
	RNA_def_property_int_funcs(prop, "rna_Sequence_frame_length_get", "rna_Sequence_frame_length_set",NULL);
	RNA_def_property_editable_func(prop, "rna_Sequence_frame_editable");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");

	prop= RNA_def_property(srna, "frame_duration", PROP_INT, PROP_TIME);
	RNA_def_property_int_sdna(prop, NULL, "len");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE|PROP_ANIMATABLE);
	RNA_def_property_range(prop, 1, MAXFRAME);
	RNA_def_property_ui_text(prop, "Length", "The length of the contents of this strip before the handles are applied");
	
	prop= RNA_def_property(srna, "frame_start", PROP_INT, PROP_TIME);
	RNA_def_property_int_sdna(prop, NULL, "start");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Start Frame", "");
	RNA_def_property_int_funcs(prop, NULL, "rna_Sequence_start_frame_set",NULL); // overlap tests and calc_seq_disp
	RNA_def_property_editable_func(prop, "rna_Sequence_frame_editable");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
	
	prop= RNA_def_property(srna, "frame_final_start", PROP_INT, PROP_TIME);
	RNA_def_property_int_sdna(prop, NULL, "startdisp");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Start Frame", "Start frame displayed in the sequence editor after offsets are applied, setting this is equivalent to moving the handle, not the actual start frame");
	RNA_def_property_int_funcs(prop, NULL, "rna_Sequence_start_frame_final_set", NULL); // overlap tests and calc_seq_disp
	RNA_def_property_editable_func(prop, "rna_Sequence_frame_editable");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");

	prop= RNA_def_property(srna, "frame_final_end", PROP_INT, PROP_TIME);
	RNA_def_property_int_sdna(prop, NULL, "enddisp");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "End Frame", "End frame displayed in the sequence editor after offsets are applied");
	RNA_def_property_int_funcs(prop, NULL, "rna_Sequence_end_frame_final_set", NULL); // overlap tests and calc_seq_disp
	RNA_def_property_editable_func(prop, "rna_Sequence_frame_editable");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");

	prop= RNA_def_property(srna, "frame_offset_start", PROP_INT, PROP_TIME);
	RNA_def_property_int_sdna(prop, NULL, "startofs");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE); // overlap tests
	RNA_def_property_ui_text(prop, "Start Offset", "");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
	
	prop= RNA_def_property(srna, "frame_offset_end", PROP_INT, PROP_TIME);
	RNA_def_property_int_sdna(prop, NULL, "endofs");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE); // overlap tests
	RNA_def_property_ui_text(prop, "End Offset", "");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
	
	prop= RNA_def_property(srna, "frame_still_start", PROP_INT, PROP_TIME);
	RNA_def_property_int_sdna(prop, NULL, "startstill");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE); // overlap tests
	RNA_def_property_range(prop, 0, MAXFRAME);
	RNA_def_property_ui_text(prop, "Start Still", "");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
	
	prop= RNA_def_property(srna, "frame_still_end", PROP_INT, PROP_TIME);
	RNA_def_property_int_sdna(prop, NULL, "endstill");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE); // overlap tests
	RNA_def_property_range(prop, 0, MAXFRAME);
	RNA_def_property_ui_text(prop, "End Still", "");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
	
	prop= RNA_def_property(srna, "channel", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "machine");
	RNA_def_property_range(prop, 0, MAXSEQ-1);
	RNA_def_property_ui_text(prop, "Channel", "Y position of the sequence strip");
	RNA_def_property_int_funcs(prop, NULL, "rna_Sequence_channel_set",NULL); // overlap test
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");

	/* blending */

	prop= RNA_def_property(srna, "blend_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "blend_mode");
	RNA_def_property_enum_items(prop, blend_mode_items);
	RNA_def_property_ui_text(prop, "Blend Mode", "");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
	
	prop= RNA_def_property(srna, "blend_alpha", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Blend Opacity", "");
	RNA_def_property_float_funcs(prop, "rna_Sequence_opacity_get", "rna_Sequence_opacity_set", NULL); // stupid 0-100 -> 0-1
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
	
	prop= RNA_def_property(srna, "effect_fader", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_float_sdna(prop, NULL, "effect_fader");
	RNA_def_property_ui_text(prop, "Effect fader position", "");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");

	prop= RNA_def_property(srna, "use_default_fade", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_USE_EFFECT_DEFAULT_FADE);
	RNA_def_property_ui_text(prop, "Use Default Fade", "Fade effect using the built-in default (usually make transition as long as effect strip)");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
	

	prop= RNA_def_property(srna, "speed_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "speed_fader");
	RNA_def_property_ui_text(prop, "Speed factor", "Multiply the current speed of the sequence with this number or remap current frame to this frame");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");

	/* effect strip inputs */

	prop= RNA_def_property(srna, "input_count", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_int_funcs(prop, "rna_Sequence_input_count_get", NULL, NULL);

	prop= RNA_def_property(srna, "input_1",  PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "seq1");
	RNA_def_property_ui_text(prop, "Input 1", "First input for the effect strip");

	prop= RNA_def_property(srna, "input_2",  PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "seq2");
	RNA_def_property_ui_text(prop, "Input 2", "Second input for the effect strip");

	prop= RNA_def_property(srna, "input_3",  PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "seq1");
	RNA_def_property_ui_text(prop, "Input 3", "Third input for the effect strip");

	RNA_api_sequence_strip(srna);
}

static void rna_def_editor(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "SequenceEditor", NULL);
	RNA_def_struct_ui_text(srna, "Sequence Editor", "Sequence editing data for a Scene datablock");
	RNA_def_struct_ui_icon(srna, ICON_SEQUENCE);
	RNA_def_struct_sdna(srna, "Editing");

	prop= RNA_def_property(srna, "sequences", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "seqbase", NULL);
	RNA_def_property_struct_type(prop, "Sequence");
	RNA_def_property_ui_text(prop, "Sequences", "");

	prop= RNA_def_property(srna, "sequences_all", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "seqbase", NULL);
	RNA_def_property_struct_type(prop, "Sequence");
	RNA_def_property_ui_text(prop, "Sequences", "");
	RNA_def_property_collection_funcs(prop, "rna_SequenceEditor_sequences_all_begin", "rna_SequenceEditor_sequences_all_next", NULL, NULL, NULL, NULL, NULL, NULL);

	prop= RNA_def_property(srna, "meta_stack", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "metastack", NULL);
	RNA_def_property_struct_type(prop, "Sequence");
	RNA_def_property_ui_text(prop, "Meta Stack", "Meta strip stack, last is currently edited meta strip");
	RNA_def_property_collection_funcs(prop, NULL, NULL, NULL, "rna_SequenceEditor_meta_stack_get", NULL, NULL, NULL, NULL);
	
	prop= RNA_def_property(srna, "active_strip", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "act_seq");
	RNA_def_property_flag(prop, PROP_EDITABLE);

	prop= RNA_def_property(srna, "show_overlay", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "over_flag", SEQ_EDIT_OVERLAY_SHOW);
	RNA_def_property_ui_text(prop, "Draw Axes", "Partial overlay on top of the sequencer");
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_SEQUENCER, NULL);

	prop= RNA_def_property(srna, "overlay_lock", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "over_flag", SEQ_EDIT_OVERLAY_ABS);
	RNA_def_property_ui_text(prop, "Overlay Lock", "");
	RNA_def_property_boolean_funcs(prop, NULL, "rna_SequenceEditor_overlay_lock_set");
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_SEQUENCER, NULL);

	/* access to fixed and relative frame */
	prop= RNA_def_property(srna, "overlay_frame", PROP_INT, PROP_NONE);
	RNA_def_property_ui_text(prop, "Overlay Offset", "");
	RNA_def_property_int_funcs(prop, "rna_SequenceEditor_overlay_frame_get", "rna_SequenceEditor_overlay_frame_set", NULL);
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_SEQUENCER, NULL);
	RNA_def_property_ui_text(prop, "Active Strip", "Sequencer's active strip");
}

static void rna_def_filter_video(StructRNA *srna)
{
	PropertyRNA *prop;

	prop= RNA_def_property(srna, "use_deinterlace", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_FILTERY);
	RNA_def_property_ui_text(prop, "De-Interlace", "For video movies to remove fields");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update_reopen_files");

	prop= RNA_def_property(srna, "use_premultiply", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_MAKE_PREMUL);
	RNA_def_property_ui_text(prop, "Premultiply", "Convert RGB from key alpha to premultiplied alpha");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, NULL);

	prop= RNA_def_property(srna, "use_flip_x", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_FLIPX);
	RNA_def_property_ui_text(prop, "Flip X", "Flip on the X axis");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");

	prop= RNA_def_property(srna, "use_flip_y", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_FLIPY);
	RNA_def_property_ui_text(prop, "Flip Y", "Flip on the Y axis");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");

	prop= RNA_def_property(srna, "use_float", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_MAKE_FLOAT);
	RNA_def_property_ui_text(prop, "Convert Float", "Convert input to float data");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");

	prop= RNA_def_property(srna, "use_reverse_frames", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_REVERSE_FRAMES);
	RNA_def_property_ui_text(prop, "Flip Time", "Reverse frame order");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");

	prop= RNA_def_property(srna, "color_multiply", PROP_FLOAT, PROP_UNSIGNED);
	RNA_def_property_float_sdna(prop, NULL, "mul");
	RNA_def_property_range(prop, 0.0f, 20.0f);
	RNA_def_property_ui_text(prop, "Multiply Colors", "");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");

	prop= RNA_def_property(srna, "color_saturation", PROP_FLOAT, PROP_UNSIGNED);
	RNA_def_property_float_sdna(prop, NULL, "sat");
	RNA_def_property_range(prop, 0.0f, 20.0f);
	RNA_def_property_ui_range(prop, 0.0f, 2.0f, 3, 3);
	RNA_def_property_ui_text(prop, "Saturation", "");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");

	prop= RNA_def_property(srna, "strobe", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 1.0f, 30.0f);
	RNA_def_property_ui_text(prop, "Strobe", "Only display every nth frame");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");

	prop= RNA_def_property(srna, "use_color_balance", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_USE_COLOR_BALANCE);
	RNA_def_property_ui_text(prop, "Use Color Balance", "(3-Way color correction) on input");
	RNA_def_property_boolean_funcs(prop, NULL, "rna_Sequence_use_color_balance_set");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");

	prop= RNA_def_property(srna, "color_balance", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "strip->color_balance");
	RNA_def_property_ui_text(prop, "Color Balance", "");

	prop= RNA_def_property(srna, "use_translation", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_USE_TRANSFORM);
	RNA_def_property_ui_text(prop, "Use Translation", "Translate image before processing");
	RNA_def_property_boolean_funcs(prop, NULL, "rna_Sequence_use_translation_set");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");

	prop= RNA_def_property(srna, "transform", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "strip->transform");
	RNA_def_property_ui_text(prop, "Transform", "");

	prop= RNA_def_property(srna, "use_crop", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_USE_CROP);
	RNA_def_property_ui_text(prop, "Use Crop", "Crop image before processing");
	RNA_def_property_boolean_funcs(prop, NULL, "rna_Sequence_use_crop_set");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");

	prop= RNA_def_property(srna, "crop", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "strip->crop");
	RNA_def_property_ui_text(prop, "Crop", "");
}

static void rna_def_proxy(StructRNA *srna)
{
	PropertyRNA *prop;

	prop= RNA_def_property(srna, "use_proxy", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_USE_PROXY);
	RNA_def_property_ui_text(prop, "Use Proxy / Timecode", "Use a preview proxy and/or timecode index for this strip");
	RNA_def_property_boolean_funcs(prop, NULL, "rna_Sequence_use_proxy_set");

	prop= RNA_def_property(srna, "proxy", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "strip->proxy");
	RNA_def_property_ui_text(prop, "Proxy", "");

	prop= RNA_def_property(srna, "use_proxy_custom_directory", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_USE_PROXY_CUSTOM_DIR);
	RNA_def_property_ui_text(prop, "Proxy Custom Directory", "Use a custom directory to store data");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");

	prop= RNA_def_property(srna, "use_proxy_custom_file", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_USE_PROXY_CUSTOM_FILE);
	RNA_def_property_ui_text(prop, "Proxy Custom File", "Use a custom file to read proxy data from");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
}

static void rna_def_input(StructRNA *srna)
{
	PropertyRNA *prop;

	prop= RNA_def_property(srna, "animation_offset_start", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "anim_startofs");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_int_funcs(prop, NULL, "rna_Sequence_anim_startofs_final_set", NULL); // overlap tests

	RNA_def_property_ui_text(prop, "Animation Start Offset", "Animation start offset (trim start)");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
	
	prop= RNA_def_property(srna, "animation_offset_end", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "anim_endofs");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_int_funcs(prop, NULL, "rna_Sequence_anim_endofs_final_set", NULL); // overlap tests
	RNA_def_property_ui_text(prop, "Animation End Offset", "Animation end offset (trim end)");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
}

static void rna_def_image(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "ImageSequence", "Sequence");
	RNA_def_struct_ui_text(srna, "Image Sequence", "Sequence strip to load one or more images");
	RNA_def_struct_sdna(srna, "Sequence");

	prop= RNA_def_property(srna, "directory", PROP_STRING, PROP_DIRPATH);
	RNA_def_property_string_sdna(prop, NULL, "strip->dir");
	RNA_def_property_ui_text(prop, "Directory", "");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");

	prop= RNA_def_property(srna, "elements", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "strip->stripdata", NULL);
	RNA_def_property_struct_type(prop, "SequenceElement");
	RNA_def_property_ui_text(prop, "Elements", "");
	RNA_def_property_collection_funcs(prop, "rna_SequenceEditor_elements_begin", "rna_iterator_array_next", "rna_iterator_array_end", "rna_iterator_array_get", "rna_SequenceEditor_elements_length", NULL, NULL, NULL);

	rna_def_filter_video(srna);
	rna_def_proxy(srna);
	rna_def_input(srna);
}

static void rna_def_meta(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "MetaSequence", "Sequence");
	RNA_def_struct_ui_text(srna, "Meta Sequence", "Sequence strip to group other strips as a single sequence strip");
	RNA_def_struct_sdna(srna, "Sequence");

	prop = RNA_def_property(srna, "sequences", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "seqbase", NULL);
	RNA_def_property_struct_type(prop, "Sequence");
	RNA_def_property_ui_text(prop, "Sequences", "");

	rna_def_filter_video(srna);
	rna_def_proxy(srna);
	rna_def_input(srna);
}

static void rna_def_scene(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "SceneSequence", "Sequence");
	RNA_def_struct_ui_text(srna, "Scene Sequence", "Sequence strip to used the rendered image of a scene");
	RNA_def_struct_sdna(srna, "Sequence");

	prop= RNA_def_property(srna, "scene", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Scene", "Scene that this sequence uses");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");

	prop= RNA_def_property(srna, "scene_camera", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_pointer_funcs(prop, NULL, NULL, NULL, "rna_Camera_object_poll");
	RNA_def_property_ui_text(prop, "Camera Override", "Override the scenes active camera");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");

	rna_def_filter_video(srna);
	rna_def_proxy(srna);
	rna_def_input(srna);
}

static void rna_def_movie(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "MovieSequence", "Sequence");
	RNA_def_struct_ui_text(srna, "Movie Sequence", "Sequence strip to load a video");
	RNA_def_struct_sdna(srna, "Sequence");

	prop= RNA_def_property(srna, "mpeg_preseek", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "anim_preseek");
	RNA_def_property_range(prop, 0, 50);
	RNA_def_property_ui_text(prop, "MPEG Preseek", "For MPEG movies, preseek this many frames");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");

	prop= RNA_def_property(srna, "stream_index", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "streamindex");
	RNA_def_property_range(prop, 0, 20);
	RNA_def_property_ui_text(prop, "Streamindex", "For files with several movie streams, use the stream with the given index");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update_reopen_files");

	prop= RNA_def_property(srna, "elements", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "strip->stripdata", NULL);
	RNA_def_property_struct_type(prop, "SequenceElement");
	RNA_def_property_ui_text(prop, "Elements", "");
	RNA_def_property_collection_funcs(prop, "rna_SequenceEditor_elements_begin", "rna_iterator_array_next", "rna_iterator_array_end", "rna_iterator_array_get", "rna_SequenceEditor_elements_length", NULL, NULL, NULL);

	prop= RNA_def_property(srna, "filepath", PROP_STRING, PROP_FILEPATH);
	RNA_def_property_ui_text(prop, "File", "");
	RNA_def_property_string_funcs(prop, "rna_Sequence_filepath_get", "rna_Sequence_filepath_length",
										"rna_Sequence_filepath_set");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_filepath_update");

	rna_def_filter_video(srna);
	rna_def_proxy(srna);
	rna_def_input(srna);
}

static void rna_def_sound(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "SoundSequence", "Sequence");
	RNA_def_struct_ui_text(srna, "Sound Sequence", "Sequence strip defining a sound to be played over a period of time");
	RNA_def_struct_sdna(srna, "Sequence");

	prop= RNA_def_property(srna, "sound", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Sound");
	RNA_def_property_ui_text(prop, "Sound", "Sound datablock used by this sequence");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");

	prop= RNA_def_property(srna, "volume", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "volume");
	RNA_def_property_range(prop, 0.0f, 100.0f);
	RNA_def_property_ui_text(prop, "Volume", "Playback volume of the sound");
	RNA_def_property_float_funcs(prop, NULL, "rna_Sequence_volume_set", NULL);
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");

	prop= RNA_def_property(srna, "pitch", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "pitch");
	RNA_def_property_range(prop, 0.1f, 10.0f);
	RNA_def_property_ui_text(prop, "Pitch", "Playback pitch of the sound");
	RNA_def_property_float_funcs(prop, NULL, "rna_Sequence_pitch_set", NULL);
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");

	prop= RNA_def_property(srna, "pan", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "pan");
	RNA_def_property_range(prop, -2.0f, 2.0f);
	RNA_def_property_ui_text(prop, "Pan", "Playback panning of the sound (only for Mono sources)");
	RNA_def_property_float_funcs(prop, NULL, "rna_Sequence_pan_set", NULL);
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");

	prop= RNA_def_property(srna, "filepath", PROP_STRING, PROP_FILEPATH);
	RNA_def_property_ui_text(prop, "File", "");
	RNA_def_property_string_funcs(prop, "rna_Sequence_filepath_get", "rna_Sequence_filepath_length", 
										"rna_Sequence_filepath_set");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_filepath_update");
	
	rna_def_input(srna);
}

static void rna_def_effect(BlenderRNA *brna)
{
	StructRNA *srna;

	srna = RNA_def_struct(brna, "EffectSequence", "Sequence");
	RNA_def_struct_ui_text(srna, "Effect Sequence", "Sequence strip applying an effect on the images created by other strips");
	RNA_def_struct_sdna(srna, "Sequence");

	rna_def_filter_video(srna);
	rna_def_proxy(srna);
}

static void rna_def_multicam(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "MulticamSequence", "Sequence");
	RNA_def_struct_ui_text(srna, "Multicam Select Sequence", "Sequence strip to perform multicam editing: select channel from below");
	RNA_def_struct_sdna(srna, "Sequence");

	prop= RNA_def_property(srna, "multicam_source", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "multicam_source");
	RNA_def_property_range(prop, 0, MAXSEQ-1);
	RNA_def_property_ui_text(prop, "Multicam Source Channel", "");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");

	rna_def_filter_video(srna);
	rna_def_proxy(srna);
	rna_def_input(srna);
}

static void rna_def_adjustment(BlenderRNA *brna)
{
	StructRNA *srna;
//	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "AdjustmentSequence", "Sequence");
	RNA_def_struct_ui_text(srna, "Adjustment Layer Sequence", "Sequence strip to perform filter adjustments to layers below");
	RNA_def_struct_sdna(srna, "Sequence");

	rna_def_filter_video(srna);
	rna_def_proxy(srna);
	rna_def_input(srna);
}

static void rna_def_plugin(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "PluginSequence", "EffectSequence");
	RNA_def_struct_ui_text(srna, "Plugin Sequence", "Sequence strip applying an effect, loaded from an external plugin");
	RNA_def_struct_sdna_from(srna, "PluginSeq", "plugin");

	prop= RNA_def_property(srna, "filename", PROP_STRING, PROP_FILENAME);
	RNA_def_property_string_sdna(prop, NULL, "name");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Filename", "");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
	
	/* plugin properties need custom wrapping code like ID properties */
}

static void rna_def_wipe(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static const EnumPropertyItem wipe_type_items[]= {
		{0, "SINGLE", 0, "Single", ""}, 
		{1, "DOUBLE", 0, "Double", ""}, 
		/* not used yet {2, "BOX", 0, "Box", ""}, */
		/* not used yet {3, "CROSS", 0, "Cross", ""}, */
		{4, "IRIS", 0, "Iris", ""}, 
		{5, "CLOCK", 0, "Clock", ""}, 	
		{0, NULL, 0, NULL, NULL}
	};

	static const EnumPropertyItem wipe_direction_items[]= {
		{0, "OUT", 0, "Out", ""},
		{1, "IN", 0, "In", ""},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "WipeSequence", "EffectSequence");
	RNA_def_struct_ui_text(srna, "Wipe Sequence", "Sequence strip creating a wipe transition");
	RNA_def_struct_sdna_from(srna, "WipeVars", "effectdata");

	prop= RNA_def_property(srna, "blur_width", PROP_FLOAT, PROP_UNSIGNED);
	RNA_def_property_float_sdna(prop, NULL, "edgeWidth");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Blur Width", "Width of the blur edge, in percentage relative to the image size");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");

#if 1 /* expose as radians */
	prop= RNA_def_property(srna, "angle", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_float_funcs(prop, "rna_WipeSequence_angle_get", "rna_WipeSequence_angle_set", NULL);
	RNA_def_property_range(prop, DEG2RAD(-90.0), DEG2RAD(90.0));
#else
	prop= RNA_def_property(srna, "angle", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "angle");
	RNA_def_property_range(prop, -90.0f, 90.0f);
#endif
	RNA_def_property_ui_text(prop, "Angle", "Edge angle");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
	
	prop= RNA_def_property(srna, "direction", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "forward");
	RNA_def_property_enum_items(prop, wipe_direction_items);
	RNA_def_property_ui_text(prop, "Direction", "Wipe direction");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
	
	prop= RNA_def_property(srna, "transition_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "wipetype");
	RNA_def_property_enum_items(prop, wipe_type_items);
	RNA_def_property_ui_text(prop, "Transition Type", "");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
}

static void rna_def_glow(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "GlowSequence", "EffectSequence");
	RNA_def_struct_ui_text(srna, "Glow Sequence", "Sequence strip creating a glow effect");
	RNA_def_struct_sdna_from(srna, "GlowVars", "effectdata");
	
	prop= RNA_def_property(srna, "threshold", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "fMini");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Threshold", "Minimum intensity to trigger a glow");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
	
	prop= RNA_def_property(srna, "clamp", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "fClamp");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Clamp", "Brightness limit of intensity");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
	
	prop= RNA_def_property(srna, "boost_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "fBoost");
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Boost Factor", "Brightness multiplier");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
	
	prop= RNA_def_property(srna, "blur_radius", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "dDist");
	RNA_def_property_range(prop, 0.5f, 20.0f);
	RNA_def_property_ui_text(prop, "Blur Distance", "Radius of glow effect");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
	
	prop= RNA_def_property(srna, "quality", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "dQuality");
	RNA_def_property_range(prop, 1, 5);
	RNA_def_property_ui_text(prop, "Quality", "Accuracy of the blur effect");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
	
	prop= RNA_def_property(srna, "use_only_boost", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "bNoComp", 0);
	RNA_def_property_ui_text(prop, "Only Boost", "Show the glow buffer only");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
}

static void rna_def_transform(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static const EnumPropertyItem interpolation_items[]= {
		{0, "NONE", 0, "None", "No interpolation"},
		{1, "BILINEAR", 0, "Bilinear", "Bilinear interpolation"},
		{2, "BICUBIC", 0, "Bicubic", "Bicubic interpolation"},
		{0, NULL, 0, NULL, NULL}
	};

	static const EnumPropertyItem translation_unit_items[]= {
		{0, "PIXELS", 0, "Pixels", ""},
		{1, "PERCENT", 0, "Percent", ""},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "TransformSequence", "EffectSequence");
	RNA_def_struct_ui_text(srna, "Transform Sequence", "Sequence strip applying affine transformations to other strips");
	RNA_def_struct_sdna_from(srna, "TransformVars", "effectdata");
	
	prop= RNA_def_property(srna, "scale_start_x", PROP_FLOAT, PROP_UNSIGNED);
	RNA_def_property_float_sdna(prop, NULL, "ScalexIni");
	RNA_def_property_ui_text(prop, "Scale X", "");
	RNA_def_property_ui_range(prop, 0, 10, 3, 10);
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
	
	prop= RNA_def_property(srna, "scale_start_y", PROP_FLOAT, PROP_UNSIGNED);
	RNA_def_property_float_sdna(prop, NULL, "ScaleyIni");
	RNA_def_property_ui_text(prop, "Scale Y", "");
	RNA_def_property_ui_range(prop, 0, 10, 3, 10);
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
	
	prop= RNA_def_property(srna, "use_uniform_scale", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "uniform_scale", 0);
	RNA_def_property_ui_text(prop, "Uniform Scale", "Scale uniformly, preserving aspect ratio");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
	
	prop= RNA_def_property(srna, "translate_start_x", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "xIni");
	RNA_def_property_ui_text(prop, "Translate X", "");
	RNA_def_property_ui_range(prop, -500.0f, 500.0f, 3, 10);
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
	
	prop= RNA_def_property(srna, "translate_start_y", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "yIni");
	RNA_def_property_ui_text(prop, "Translate Y", "");
	RNA_def_property_ui_range(prop, -500.0f, 500.0f, 3, 10);
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
	
	prop= RNA_def_property(srna, "rotation_start", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "rotIni");
	RNA_def_property_range(prop, -360.0f, 360.0f);
	RNA_def_property_ui_text(prop, "Rotation", "");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
	
	prop= RNA_def_property(srna, "translation_unit", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "percent");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE); /* not meant to be animated */
	RNA_def_property_enum_items(prop, translation_unit_items);
	RNA_def_property_ui_text(prop, "Translation Unit", "");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
	
	prop= RNA_def_property(srna, "interpolation", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, interpolation_items);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE); /* not meant to be animated */
	RNA_def_property_ui_text(prop, "Interpolation", "");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
}

static void rna_def_solid_color(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "ColorSequence", "EffectSequence");
	RNA_def_struct_ui_text(srna, "Color Sequence", "Sequence strip creating an image filled with a single color");
	RNA_def_struct_sdna_from(srna, "SolidColorVars", "effectdata");
	
	prop= RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_float_sdna(prop, NULL, "col");
	RNA_def_property_ui_text(prop, "Color", "");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
}

static void rna_def_speed_control(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "SpeedControlSequence", "EffectSequence");
	RNA_def_struct_ui_text(srna, "SpeedControl Sequence", "Sequence strip to control the speed of other strips");
	RNA_def_struct_sdna_from(srna, "SpeedControlVars", "effectdata");
	
	prop= RNA_def_property(srna, "multiply_speed", PROP_FLOAT, PROP_UNSIGNED);
	RNA_def_property_float_sdna(prop, NULL, "globalSpeed");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE); /* seq->facf0 is used to animate this */
	RNA_def_property_ui_text(prop, "Multiply Speed", "Multiply the resulting speed after the speed factor");
	RNA_def_property_ui_range(prop, 0.0f, 100.0f, 1, 0);
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
	
	prop= RNA_def_property(srna, "use_as_speed", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", SEQ_SPEED_INTEGRATE);
	RNA_def_property_ui_text(prop, "Use as speed", "Interpret the value as speed instead of a frame number");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");

	prop= RNA_def_property(srna, "use_frame_blend", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", SEQ_SPEED_BLEND);
	RNA_def_property_ui_text(prop, "Frame Blending", "Blend two frames into the target for a smoother result");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");

	prop= RNA_def_property(srna, "scale_to_length", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", SEQ_SPEED_COMPRESS_IPO_Y);
	RNA_def_property_ui_text(prop, "Scale to length", "Scale values from 0.0 to 1.0 to target sequence length");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
}

void RNA_def_sequencer(BlenderRNA *brna)
{
	rna_def_strip_element(brna);
	rna_def_strip_proxy(brna);
	rna_def_strip_color_balance(brna);
	rna_def_strip_crop(brna);
	rna_def_strip_transform(brna);

	rna_def_sequence(brna);
	rna_def_editor(brna);

	rna_def_image(brna);
	rna_def_meta(brna);
	rna_def_scene(brna);
	rna_def_movie(brna);
	rna_def_sound(brna);
	rna_def_effect(brna);
	rna_def_multicam(brna);
	rna_def_adjustment(brna);
	rna_def_plugin(brna);
	rna_def_wipe(brna);
	rna_def_glow(brna);
	rna_def_transform(brna);
	rna_def_solid_color(brna);
	rna_def_speed_control(brna);
}

#endif

