/**
 * $Id$
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Blender Foundation (2008)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <limits.h>

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_types.h"

#include "rna_internal.h"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "BKE_sequencer.h"

#include "MEM_guardedalloc.h"

#include "WM_types.h"

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
	
	seq->start= value;
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

static void rna_Sequence_length_set(PointerRNA *ptr, int value)
{
	Sequence *seq= (Sequence*)ptr->data;
	Scene *scene= (Scene*)ptr->id.data;
	
	seq_tx_set_final_right(seq, seq->start+value);
	rna_Sequence_frame_change_update(scene, seq);
}

static int rna_Sequence_length_get(PointerRNA *ptr)
{
	Sequence *seq= (Sequence*)ptr->data;
	return seq_tx_get_final_right(seq, 0)-seq_tx_get_final_left(seq, 0);
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
	char oldname[32];
	
	/* make a copy of the old name first */
	BLI_strncpy(oldname, seq->name+2, sizeof(seq->name)-2);
	
	/* copy the new name into the name slot */
	BLI_strncpy(seq->name+2, value, sizeof(seq->name)-2);
	
	/* make sure the name is unique */
	seqbase_unique_name_recursive(&scene->ed->seqbase, seq);
	
	/* fix all the animation data which may link to this */
	BKE_all_animdata_fix_paths_rename("sequence_editor.sequences_all", oldname, seq->name+2);
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

static void rna_Sequence_filepath_set(PointerRNA *ptr, const char *value)
{
	Sequence *seq= (Sequence*)(ptr->data);
	char dir[FILE_MAX], name[FILE_MAX];

	BLI_split_dirfile_basic(value, dir, name);
	BLI_strncpy(seq->strip->dir, dir, sizeof(seq->strip->dir));
	BLI_strncpy(seq->strip->stripdata->name, name, sizeof(seq->strip->stripdata->name));
}

static void rna_Sequence_filepath_get(PointerRNA *ptr, char *value)
{
	Sequence *seq= (Sequence*)(ptr->data);
	char path[FILE_MAX];

	BLI_join_dirfile(path, seq->strip->dir, seq->strip->stripdata->name);
	BLI_strncpy(value, path, strlen(path)+1);
}

static int rna_Sequence_filepath_length(PointerRNA *ptr)
{
	Sequence *seq= (Sequence*)(ptr->data);
	char path[FILE_MAX];

	BLI_join_dirfile(path, seq->strip->dir, seq->strip->stripdata->name);
	return strlen(path)+1;
}

static void rna_SoundSequence_filename_set(PointerRNA *ptr, const char *value)
{
	Sequence *seq= (Sequence*)(ptr->data);
	char dir[FILE_MAX], name[FILE_MAX];

	BLI_split_dirfile_basic(value, dir, name);
	BLI_strncpy(seq->strip->dir, dir, sizeof(seq->strip->dir));
	BLI_strncpy(seq->strip->stripdata->name, name, sizeof(seq->strip->stripdata->name));
}

static void rna_SequenceElement_filename_set(PointerRNA *ptr, const char *value)
{
	StripElem *elem= (StripElem*)(ptr->data);
	char name[FILE_MAX];

	BLI_split_dirfile_basic(value, NULL, name);
	BLI_strncpy(elem->name, name, sizeof(elem->name));
}

static void rna_Sequence_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	Editing *ed= seq_give_editing(scene, FALSE);

	free_imbuf_seq(scene, &ed->seqbase, FALSE);

	if(RNA_struct_is_a(ptr->type, &RNA_SoundSequence))
		seq_update_sound(scene, ptr->data);
}

static void rna_Sequence_mute_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	Editing *ed= seq_give_editing(scene, FALSE);

	seq_update_muting(scene, ed);
	rna_Sequence_update(bmain, scene, ptr);
}

/* do_versions? */
static float rna_Sequence_opacity_get(PointerRNA *ptr) {
	return ((Sequence*)(ptr->data))->blend_opacity / 100.0f;
}
static void rna_Sequence_opacity_set(PointerRNA *ptr, float value) {
	((Sequence*)(ptr->data))->blend_opacity = value * 100.0f;
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
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Filename", "");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_SequenceElement_filename_set");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
}

static void rna_def_strip_crop(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "SequenceCrop", NULL);
	RNA_def_struct_ui_text(srna, "Sequence Crop", "Cropping parameters for a sequence strip");
	RNA_def_struct_sdna(srna, "StripCrop");

	prop= RNA_def_property(srna, "top", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_ui_text(prop, "Top", "");
	RNA_def_property_ui_range(prop, 0, 4096, 1, 0);
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
	
	prop= RNA_def_property(srna, "bottom", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_ui_text(prop, "Bottom", "");
	RNA_def_property_ui_range(prop, 0, 4096, 1, 0);
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
	
	prop= RNA_def_property(srna, "left", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_ui_text(prop, "Left", "");
	RNA_def_property_ui_range(prop, 0, 4096, 1, 0);
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");

	prop= RNA_def_property(srna, "right", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_ui_text(prop, "Right", "");
	RNA_def_property_ui_range(prop, 0, 4096, 1, 0);
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
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
	RNA_def_property_ui_text(prop, "Offset Y", "");
	RNA_def_property_ui_range(prop, -4096, 4096, 1, 0);
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");

	prop= RNA_def_property(srna, "offset_y", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "yofs");
	RNA_def_property_ui_text(prop, "Offset Y", "");
	RNA_def_property_ui_range(prop, -4096, 4096, 1, 0);
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
}

static void rna_def_strip_proxy(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "SequenceProxy", NULL);
	RNA_def_struct_ui_text(srna, "Sequence Proxy", "Proxy parameters for a sequence strip");
	RNA_def_struct_sdna(srna, "StripProxy");
	
	prop= RNA_def_property(srna, "directory", PROP_STRING, PROP_DIRPATH);
	RNA_def_property_string_sdna(prop, NULL, "dir");
	RNA_def_property_ui_text(prop, "Directory", "Location to story the proxy file");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
	
	prop= RNA_def_property(srna, "file", PROP_STRING, PROP_DIRPATH);
	RNA_def_property_string_sdna(prop, NULL, "file");
	RNA_def_property_ui_text(prop, "File", "Proxy file name");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
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
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
	
	prop= RNA_def_property(srna, "gamma", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_ui_text(prop, "Gamma", "Color balance gamma (midtones)");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
	
	prop= RNA_def_property(srna, "gain", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_ui_text(prop, "Gain", "Color balance gain (highlights)");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
	
	prop= RNA_def_property(srna, "inverse_gain", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_COLOR_BALANCE_INVERSE_GAIN);
	RNA_def_property_ui_text(prop, "Inverse Gain", "");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");

	prop= RNA_def_property(srna, "inverse_gamma", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_COLOR_BALANCE_INVERSE_GAMMA);
	RNA_def_property_ui_text(prop, "Inverse Gamma", "");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");

	prop= RNA_def_property(srna, "inverse_lift", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_COLOR_BALANCE_INVERSE_LIFT);
	RNA_def_property_ui_text(prop, "Inverse Lift", "");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
	
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
	FunctionRNA *func;

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
		{SEQ_PLUGIN, "PLUGIN", 0, "plugin", ""}, 
		{SEQ_WIPE, "WIPE", 0, "Wipe", ""}, 
		{SEQ_GLOW, "GLOW", 0, "Glow", ""}, 
		{SEQ_TRANSFORM, "TRANSFORM", 0, "Transform", ""}, 
		{SEQ_COLOR, "COLOR", 0, "Color", ""}, 
		{SEQ_SPEED, "SPEED", 0, "Speed", ""}, 
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
	//RNA_def_property_ui_text(prop, "Ipo Curves", "Ipo curves used by this sequence");

	/* flags */

	prop= RNA_def_property(srna, "selected", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SELECT);
	RNA_def_property_ui_text(prop, "Selected", "");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER_SELECT, NULL);

	prop= RNA_def_property(srna, "left_handle_selected", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_LEFTSEL);
	RNA_def_property_ui_text(prop, "Left Handle Selected", "");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER_SELECT, NULL);

	prop= RNA_def_property(srna, "right_handle_selected", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_RIGHTSEL);
	RNA_def_property_ui_text(prop, "Right Handle Selected", "");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER_SELECT, NULL);

	prop= RNA_def_property(srna, "mute", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_MUTE);
	RNA_def_property_ui_text(prop, "Mute", "");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_mute_update");

	prop= RNA_def_property(srna, "frame_locked", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_IPO_FRAME_LOCKED);
	RNA_def_property_ui_text(prop, "Frame Locked", "Lock the animation curve to the global frame counter");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");

	prop= RNA_def_property(srna, "lock", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_LOCK);
	RNA_def_property_ui_text(prop, "Lock", "Lock strip so that it can't be transformed");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, NULL);

	/* strip positioning */

	prop= RNA_def_property(srna, "length", PROP_INT, PROP_TIME);
	RNA_def_property_int_sdna(prop, NULL, "len");
	RNA_def_property_range(prop, 1, MAXFRAME);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Length", "The length of the contents of this strip before the handles are applied");
	RNA_def_property_int_funcs(prop, "rna_Sequence_length_get", "rna_Sequence_length_set",NULL);
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
	
	prop= RNA_def_property(srna, "start_frame", PROP_INT, PROP_TIME);
	RNA_def_property_int_sdna(prop, NULL, "start");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Start Frame", "");
	RNA_def_property_int_funcs(prop, NULL, "rna_Sequence_start_frame_set",NULL); // overlap tests and calc_seq_disp
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
	
	prop= RNA_def_property(srna, "start_frame_final", PROP_INT, PROP_TIME);
	RNA_def_property_int_sdna(prop, NULL, "startdisp");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Start Frame", "Start frame displayed in the sequence editor after offsets are applied, setting this is equivilent to moving the handle, not the actual start frame");
	RNA_def_property_int_funcs(prop, NULL, "rna_Sequence_start_frame_final_set", NULL); // overlap tests and calc_seq_disp
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");

	prop= RNA_def_property(srna, "end_frame_final", PROP_INT, PROP_TIME);
	RNA_def_property_int_sdna(prop, NULL, "enddisp");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "End Frame", "End frame displayed in the sequence editor after offsets are applied");
	RNA_def_property_int_funcs(prop, NULL, "rna_Sequence_end_frame_final_set", NULL); // overlap tests and calc_seq_disp
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");

	prop= RNA_def_property(srna, "start_offset", PROP_INT, PROP_TIME);
	RNA_def_property_int_sdna(prop, NULL, "startofs");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE); // overlap tests
	RNA_def_property_ui_text(prop, "Start Offset", "");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
	
	prop= RNA_def_property(srna, "end_offset", PROP_INT, PROP_TIME);
	RNA_def_property_int_sdna(prop, NULL, "endofs");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE); // overlap tests
	RNA_def_property_ui_text(prop, "End offset", "");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
	
	prop= RNA_def_property(srna, "start_still", PROP_INT, PROP_TIME);
	RNA_def_property_int_sdna(prop, NULL, "startstill");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE); // overlap tests
	RNA_def_property_range(prop, 0, MAXFRAME);
	RNA_def_property_ui_text(prop, "Start Still", "");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
	
	prop= RNA_def_property(srna, "end_still", PROP_INT, PROP_TIME);
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

	prop= RNA_def_property(srna, "blend_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, blend_mode_items);
	RNA_def_property_ui_text(prop, "Blend Mode", "");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
	
	prop= RNA_def_property(srna, "blend_opacity", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Blend Opacity", "");
	RNA_def_property_float_funcs(prop, "rna_Sequence_opacity_get", "rna_Sequence_opacity_set", NULL); // stupid 0-100 -> 0-1
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
	
	prop= RNA_def_property(srna, "effect_fader", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_float_sdna(prop, NULL, "effect_fader");
	RNA_def_property_ui_text(prop, "Effect fader position", "");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");

	prop= RNA_def_property(srna, "use_effect_default_fade", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_USE_EFFECT_DEFAULT_FADE);
	RNA_def_property_ui_text(prop, "Use Default Fade", "Fade effect using the builtin default (usually make transition as long as effect strip)");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
	

	prop= RNA_def_property(srna, "speed_fader", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "speed_fader");
	RNA_def_property_ui_text(prop, "Speed effect fader position", "");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");

	/* functions */
	func= RNA_def_function(srna, "getStripElem", "give_stripelem");
	RNA_def_function_ui_description(func, "Return the strip element from a given frame or None.");
	prop= RNA_def_int(func, "frame", 0, -MAXFRAME, MAXFRAME, "Frame", "The frame to get the strip element from", -MAXFRAME, MAXFRAME);
	RNA_def_property_flag(prop, PROP_REQUIRED);
	RNA_def_function_return(func, RNA_def_pointer(func, "elem", "SequenceElement", "", "strip element of the current frame"));
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
	RNA_def_property_collection_funcs(prop, "rna_SequenceEditor_sequences_all_begin", "rna_SequenceEditor_sequences_all_next", 0, 0, 0, 0, 0);

	prop= RNA_def_property(srna, "meta_stack", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "metastack", NULL);
	RNA_def_property_struct_type(prop, "Sequence");
	RNA_def_property_ui_text(prop, "Meta Stack", "Meta strip stack, last is currently edited meta strip");
	RNA_def_property_collection_funcs(prop, 0, 0, 0, "rna_SequenceEditor_meta_stack_get", 0, 0, 0);
	
	prop= RNA_def_property(srna, "active_strip", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "act_seq");
	RNA_def_property_ui_text(prop, "Active Strip", "Sequencers active strip");
}

static void rna_def_filter_video(StructRNA *srna)
{
	PropertyRNA *prop;

	prop= RNA_def_property(srna, "de_interlace", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_FILTERY);
	RNA_def_property_ui_text(prop, "De-Interlace", "For video movies to remove fields");

	prop= RNA_def_property(srna, "premultiply", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_MAKE_PREMUL);
	RNA_def_property_ui_text(prop, "Premultiply", "Convert RGB from key alpha to premultiplied alpha");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, NULL);

	prop= RNA_def_property(srna, "flip_x", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_FLIPX);
	RNA_def_property_ui_text(prop, "Flip X", "Flip on the X axis");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");

	prop= RNA_def_property(srna, "flip_y", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_FLIPY);
	RNA_def_property_ui_text(prop, "Flip Y", "Flip on the Y axis");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");

	prop= RNA_def_property(srna, "convert_float", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_MAKE_FLOAT);
	RNA_def_property_ui_text(prop, "Convert Float", "Convert input to float data");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");

	prop= RNA_def_property(srna, "reverse_frames", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_REVERSE_FRAMES);
	RNA_def_property_ui_text(prop, "Flip Time", "Reverse frame order");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");

	prop= RNA_def_property(srna, "multiply_colors", PROP_FLOAT, PROP_UNSIGNED);
	RNA_def_property_float_sdna(prop, NULL, "mul");
	RNA_def_property_range(prop, 0.0f, 20.0f);
	RNA_def_property_ui_text(prop, "Multiply Colors", "");
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
	RNA_def_property_ui_text(prop, "Use Proxy", "Use a preview proxy for this strip");
	RNA_def_property_boolean_funcs(prop, NULL, "rna_Sequence_use_proxy_set");

	prop= RNA_def_property(srna, "proxy", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "strip->proxy");
	RNA_def_property_ui_text(prop, "Proxy", "");

	prop= RNA_def_property(srna, "proxy_custom_directory", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_USE_PROXY_CUSTOM_DIR);
	RNA_def_property_ui_text(prop, "Proxy Custom Directory", "Use a custom directory to store data");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
}

static void rna_def_input(StructRNA *srna)
{
	PropertyRNA *prop;

	prop= RNA_def_property(srna, "animation_start_offset", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "anim_startofs");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE); // overlap test
	RNA_def_property_ui_text(prop, "Animation Start Offset", "Animation start offset (trim start)");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
	
	prop= RNA_def_property(srna, "animation_end_offset", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "anim_endofs");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE); // overlap test
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
	RNA_def_property_collection_sdna(prop, NULL, "strip->stripdata", "strip->len");
	RNA_def_property_struct_type(prop, "SequenceElement");
	RNA_def_property_ui_text(prop, "Elements", "");

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

	prop= RNA_def_property(srna, "filepath", PROP_STRING, PROP_FILEPATH);
	RNA_def_property_ui_text(prop, "File", "");
	RNA_def_property_string_funcs(prop, "rna_Sequence_filepath_get", "rna_Sequence_filepath_length",
										"rna_Sequence_filepath_set");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");

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
	RNA_def_property_range(prop, 0.0f, 2.0f);
	RNA_def_property_ui_text(prop, "Volume", "Playback volume of the sound");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");

	prop= RNA_def_property(srna, "filepath", PROP_STRING, PROP_FILEPATH);
	RNA_def_property_ui_text(prop, "File", "");
	RNA_def_property_string_funcs(prop, "rna_Sequence_filepath_get", "rna_Sequence_filepath_length", 
										"rna_Sequence_filepath_set");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
	
	rna_def_input(srna);
}

static void rna_def_effect(BlenderRNA *brna)
{
	StructRNA *srna;

	srna = RNA_def_struct(brna, "EffectSequence", "Sequence");
	RNA_def_struct_ui_text(srna, "Effect Sequence", "Sequence strip applying an effect on the images created by other strips");
	RNA_def_struct_sdna(srna, "Sequence");

	rna_def_proxy(srna);
}

static void rna_def_plugin(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "PluginSequence", "EffectSequence");
	RNA_def_struct_ui_text(srna, "Plugin Sequence", "Sequence strip applying an effect, loaded from an external plugin");
	RNA_def_struct_sdna_from(srna, "PluginSeq", "plugin");

	prop= RNA_def_property(srna, "filename", PROP_STRING, PROP_FILEPATH);
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
	
	prop= RNA_def_property(srna, "angle", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "angle");
	RNA_def_property_range(prop, -90.0f, 90.0f);
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
	RNA_def_property_ui_text(prop, "Clamp", "rightness limit of intensity");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
	
	prop= RNA_def_property(srna, "boost_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "fBoost");
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Boost Factor", "Brightness multiplier");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
	
	prop= RNA_def_property(srna, "blur_distance", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "dDist");
	RNA_def_property_range(prop, 0.5f, 20.0f);
	RNA_def_property_ui_text(prop, "Blur Distance", "Radius of glow effect");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
	
	prop= RNA_def_property(srna, "quality", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "dQuality");
	RNA_def_property_range(prop, 1, 5);
	RNA_def_property_ui_text(prop, "Quality", "Accuracy of the blur effect");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
	
	prop= RNA_def_property(srna, "only_boost", PROP_BOOLEAN, PROP_NONE);
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
	
	prop= RNA_def_property(srna, "uniform_scale", PROP_BOOLEAN, PROP_NONE);
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
	
	prop= RNA_def_property(srna, "global_speed", PROP_FLOAT, PROP_UNSIGNED);
	RNA_def_property_float_sdna(prop, NULL, "globalSpeed");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE); /* seq->facf0 is used to animate this */
	RNA_def_property_ui_text(prop, "Global Speed", "");
	RNA_def_property_ui_range(prop, 0.0f, 100.0f, 1, 0);
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");
	
	prop= RNA_def_property(srna, "curve_velocity", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", SEQ_SPEED_INTEGRATE);
	RNA_def_property_ui_text(prop, "F-Curve Velocity", "Interpret the F-Curve value as a velocity instead of a frame number");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");

	prop= RNA_def_property(srna, "frame_blending", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", SEQ_SPEED_BLEND);
	RNA_def_property_ui_text(prop, "Frame Blending", "Blend two frames into the target for a smoother result");
	RNA_def_property_update(prop, NC_SCENE|ND_SEQUENCER, "rna_Sequence_update");

	prop= RNA_def_property(srna, "curve_compress_y", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", SEQ_SPEED_COMPRESS_IPO_Y);
	RNA_def_property_ui_text(prop, "F-Curve Compress Y", "Scale F-Curve value to get the target frame number, F-Curve value runs from 0.0 to 1.0");
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
	rna_def_plugin(brna);
	rna_def_wipe(brna);
	rna_def_glow(brna);
	rna_def_transform(brna);
	rna_def_solid_color(brna);
	rna_def_speed_control(brna);
}

#endif

