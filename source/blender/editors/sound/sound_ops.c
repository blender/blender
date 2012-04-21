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
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/sound/sound_ops.c
 *  \ingroup edsnd
 */


#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "DNA_anim_types.h"
#include "DNA_packedFile_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_sequence_types.h"
#include "DNA_sound_types.h"
#include "DNA_userdef_types.h"

#include "BKE_context.h"
#include "BKE_fcurve.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_report.h"
#include "BKE_packedFile.h"
#include "BKE_scene.h"
#include "BKE_sound.h"
#include "BKE_sequencer.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "UI_interface.h"

#include "WM_api.h"
#include "WM_types.h"

#ifdef WITH_AUDASPACE
#  include "AUD_C-API.h"
#endif

#include "ED_sound.h"
#include "ED_util.h"

#include "sound_intern.h"

/******************** open sound operator ********************/

static int sound_open_cancel(bContext *UNUSED(C), wmOperator *op)
{
	MEM_freeN(op->customdata);
	op->customdata= NULL;
	return OPERATOR_CANCELLED;
}

static void sound_open_init(bContext *C, wmOperator *op)
{
	PropertyPointerRNA *pprop;

	op->customdata= pprop= MEM_callocN(sizeof(PropertyPointerRNA), "OpenPropertyPointerRNA");
	uiIDContextProperty(C, &pprop->ptr, &pprop->prop);
}

#ifdef WITH_AUDASPACE
static int sound_open_exec(bContext *C, wmOperator *op)
{
	char path[FILE_MAX];
	bSound *sound;
	PropertyPointerRNA *pprop;
	PointerRNA idptr;
	AUD_SoundInfo info;
	Main *bmain = CTX_data_main(C);

	RNA_string_get(op->ptr, "filepath", path);
	sound = sound_new_file(bmain, path);

	if (!op->customdata)
		sound_open_init(C, op);

	if (sound==NULL || sound->playback_handle == NULL) {
		if (op->customdata) MEM_freeN(op->customdata);
		BKE_report(op->reports, RPT_ERROR, "Unsupported audio format");
		return OPERATOR_CANCELLED;
	}

	info = AUD_getInfo(sound->playback_handle);

	if (info.specs.channels == AUD_CHANNELS_INVALID) {
		sound_delete(bmain, sound);
		if (op->customdata) MEM_freeN(op->customdata);
		BKE_report(op->reports, RPT_ERROR, "Unsupported audio format");
		return OPERATOR_CANCELLED;
	}

	if (RNA_boolean_get(op->ptr, "mono")) {
		sound->flags |= SOUND_FLAGS_MONO;
		sound_load(bmain, sound);
	}

	if (RNA_boolean_get(op->ptr, "cache")) {
		sound_cache(sound);
	}

	/* hook into UI */
	pprop= op->customdata;

	if (pprop->prop) {
		/* when creating new ID blocks, use is already 1, but RNA
		 * pointer se also increases user, so this compensates it */
		sound->id.us--;

		RNA_id_pointer_create(&sound->id, &idptr);
		RNA_property_pointer_set(&pprop->ptr, pprop->prop, idptr);
		RNA_property_update(C, &pprop->ptr, pprop->prop);
	}

	if (op->customdata) MEM_freeN(op->customdata);
	return OPERATOR_FINISHED;
}

#else //WITH_AUDASPACE

static int sound_open_exec(bContext *UNUSED(C), wmOperator *op)
{
	BKE_report(op->reports, RPT_ERROR, "Compiled without sound support");

	return OPERATOR_CANCELLED;
}

#endif

static int sound_open_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	if (RNA_struct_property_is_set(op->ptr, "filepath"))
		return sound_open_exec(C, op);

	sound_open_init(C, op);

	return WM_operator_filesel(C, op, event);
}

static void SOUND_OT_open(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Open Sound";
	ot->description = "Load a sound file";
	ot->idname = "SOUND_OT_open";

	/* api callbacks */
	ot->exec = sound_open_exec;
	ot->invoke = sound_open_invoke;
	ot->cancel = sound_open_cancel;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	WM_operator_properties_filesel(ot, FOLDERFILE|SOUNDFILE|MOVIEFILE, FILE_SPECIAL, FILE_OPENFILE, WM_FILESEL_FILEPATH | WM_FILESEL_RELPATH, FILE_DEFAULTDISPLAY);
	RNA_def_boolean(ot->srna, "cache", FALSE, "Cache", "Cache the sound in memory");
	RNA_def_boolean(ot->srna, "mono", FALSE, "Mono", "Mixdown the sound to mono");
}

static void SOUND_OT_open_mono(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Open Sound Mono";
	ot->description = "Load a sound file as mono";
	ot->idname = "SOUND_OT_open_mono";

	/* api callbacks */
	ot->exec = sound_open_exec;
	ot->invoke = sound_open_invoke;
	ot->cancel = sound_open_cancel;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	WM_operator_properties_filesel(ot, FOLDERFILE|SOUNDFILE|MOVIEFILE, FILE_SPECIAL, FILE_OPENFILE, WM_FILESEL_FILEPATH | WM_FILESEL_RELPATH, FILE_DEFAULTDISPLAY);
	RNA_def_boolean(ot->srna, "cache", FALSE, "Cache", "Cache the sound in memory");
	RNA_def_boolean(ot->srna, "mono", TRUE, "Mono", "Mixdown the sound to mono");
}

/* ******************************************************* */

static int sound_update_animation_flags_exec(bContext *C, wmOperator *UNUSED(op))
{
	Sequence* seq;
	Scene* scene = CTX_data_scene(C);
	struct FCurve* fcu;
	char driven;

	SEQ_BEGIN(scene->ed, seq) {
		fcu = id_data_find_fcurve(&scene->id, seq, &RNA_Sequence, "volume", 0, &driven);
		if (fcu || driven)
			seq->flag |= SEQ_AUDIO_VOLUME_ANIMATED;
		else
			seq->flag &= ~SEQ_AUDIO_VOLUME_ANIMATED;

		fcu = id_data_find_fcurve(&scene->id, seq, &RNA_Sequence, "pitch", 0, &driven);
		if (fcu || driven)
			seq->flag |= SEQ_AUDIO_PITCH_ANIMATED;
		else
			seq->flag &= ~SEQ_AUDIO_PITCH_ANIMATED;

		fcu = id_data_find_fcurve(&scene->id, seq, &RNA_Sequence, "pan", 0, &driven);
		if (fcu || driven)
			seq->flag |= SEQ_AUDIO_PAN_ANIMATED;
		else
			seq->flag &= ~SEQ_AUDIO_PAN_ANIMATED;
	}
	SEQ_END

	fcu = id_data_find_fcurve(&scene->id, scene, &RNA_Scene, "audio_volume", 0, &driven);
	if (fcu || driven)
		scene->audio.flag |= AUDIO_VOLUME_ANIMATED;
	else
		scene->audio.flag &= ~AUDIO_VOLUME_ANIMATED;

	return OPERATOR_FINISHED;
}

static void SOUND_OT_update_animation_flags(wmOperatorType *ot)
{
	/*
	 * This operator is needed to set a correct state of the sound animation
	 * System. Unfortunately there's no really correct place to call the exec
	 * function, that's why I made it an operator that's only visible in the
	 * search menu. Apart from that the bake animation operator calls it too.
	 */

	/* identifiers */
	ot->name = "Update animation";
	ot->description = "Update animation flags";
	ot->idname = "SOUND_OT_update_animation_flags";

	/* api callbacks */
	ot->exec = sound_update_animation_flags_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER;
}

/* ******************************************************* */

static int sound_bake_animation_exec(bContext *C, wmOperator *UNUSED(op))
{
	Main* bmain = CTX_data_main(C);
	Scene* scene = CTX_data_scene(C);
	int oldfra = scene->r.cfra;
	int cfra;

	sound_update_animation_flags_exec(C, NULL);

	for (cfra = (scene->r.sfra > 0) ? (scene->r.sfra - 1) : 0; cfra <= scene->r.efra + 1; cfra++) {
		scene->r.cfra = cfra;
		scene_update_for_newframe(bmain, scene, scene->lay);
	}

	scene->r.cfra = oldfra;
	scene_update_for_newframe(bmain, scene, scene->lay);

	return OPERATOR_FINISHED;
}

static void SOUND_OT_bake_animation(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Update animation cache";
	ot->description = "Update the audio animation cache";
	ot->idname = "SOUND_OT_bake_animation";

	/* api callbacks */
	ot->exec = sound_bake_animation_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER;
}


/******************** mixdown operator ********************/

static int sound_mixdown_exec(bContext *C, wmOperator *op)
{
#ifdef WITH_AUDASPACE
	char path[FILE_MAX];
	char filename[FILE_MAX];
	Scene *scene;
	Main *bmain;

	int bitrate, accuracy;
	AUD_DeviceSpecs specs;
	AUD_Container container;
	AUD_Codec codec;
	const char* result;

	sound_bake_animation_exec(C, op);

	RNA_string_get(op->ptr, "filepath", path);
	bitrate = RNA_int_get(op->ptr, "bitrate") * 1000;
	accuracy = RNA_int_get(op->ptr, "accuracy");
	specs.format = RNA_enum_get(op->ptr, "format");
	container = RNA_enum_get(op->ptr, "container");
	codec = RNA_enum_get(op->ptr, "codec");
	scene = CTX_data_scene(C);
	bmain = CTX_data_main(C);
	specs.channels = scene->r.ffcodecdata.audio_channels;
	specs.rate = scene->r.ffcodecdata.audio_mixrate;

	BLI_strncpy(filename, path, sizeof(filename));
	BLI_path_abs(filename, bmain->name);

	result = AUD_mixdown(scene->sound_scene, SFRA * specs.rate / FPS, (EFRA - SFRA) * specs.rate / FPS,
						 accuracy, filename, specs, container, codec, bitrate);

	if (result) {
		BKE_report(op->reports, RPT_ERROR, result);
		return OPERATOR_CANCELLED;
	}
#else // WITH_AUDASPACE
	(void)C;
	(void)op;
#endif // WITH_AUDASPACE
	return OPERATOR_FINISHED;
}

static int sound_mixdown_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	if (RNA_struct_property_is_set(op->ptr, "filepath"))
		return sound_mixdown_exec(C, op);

	return WM_operator_filesel(C, op, event);
}

#ifdef WITH_AUDASPACE

static int sound_mixdown_draw_check_prop(PointerRNA *UNUSED(ptr), PropertyRNA *prop)
{
	const char *prop_id= RNA_property_identifier(prop);
	return !(	strcmp(prop_id, "filepath") == 0 ||
				strcmp(prop_id, "directory") == 0 ||
				strcmp(prop_id, "filename") == 0
	);
}

static void sound_mixdown_draw(bContext *C, wmOperator *op)
{
	static EnumPropertyItem pcm_format_items[] = {
		{AUD_FORMAT_U8, "U8", 0, "U8", "8 bit unsigned"},
		{AUD_FORMAT_S16, "S16", 0, "S16", "16 bit signed"},
#ifdef WITH_SNDFILE
		{AUD_FORMAT_S24, "S24", 0, "S24", "24 bit signed"},
#endif
		{AUD_FORMAT_S32, "S32", 0, "S32", "32 bit signed"},
		{AUD_FORMAT_FLOAT32, "F32", 0, "F32", "32 bit floating point"},
		{AUD_FORMAT_FLOAT64, "F64", 0, "F64", "64 bit floating point"},
		{0, NULL, 0, NULL, NULL}};

	static EnumPropertyItem mp3_format_items[] = {
		{AUD_FORMAT_S16, "S16", 0, "S16", "16 bit signed"},
		{AUD_FORMAT_S32, "S32", 0, "S32", "32 bit signed"},
		{0, NULL, 0, NULL, NULL}};

	static EnumPropertyItem ac3_format_items[] = {
		{AUD_FORMAT_S16, "S16", 0, "S16", "16 bit signed"},
		{AUD_FORMAT_FLOAT32, "F32", 0, "F32", "32 bit floating point"},
		{0, NULL, 0, NULL, NULL}};

#ifdef WITH_SNDFILE
	static EnumPropertyItem flac_format_items[] = {
		{AUD_FORMAT_S16, "S16", 0, "S16", "16 bit signed"},
		{AUD_FORMAT_S24, "S24", 0, "S24", "24 bit signed"},
		{0, NULL, 0, NULL, NULL}};
#endif

	static EnumPropertyItem all_codec_items[] = {
		{AUD_CODEC_AAC, "AAC", 0, "AAC", "Advanced Audio Coding"},
		{AUD_CODEC_AC3, "AC3", 0, "AC3", "Dolby Digital ATRAC 3"},
		{AUD_CODEC_FLAC, "FLAC", 0, "FLAC", "Free Lossless Audio Codec"},
		{AUD_CODEC_MP2, "MP2", 0, "MP2", "MPEG-1 Audio Layer II"},
		{AUD_CODEC_MP3, "MP3", 0, "MP3", "MPEG-2 Audio Layer III"},
		{AUD_CODEC_PCM, "PCM", 0, "PCM", "Pulse Code Modulation (RAW)"},
		{AUD_CODEC_VORBIS, "VORBIS", 0, "Vorbis", "Xiph.Org Vorbis Codec"},
		{0, NULL, 0, NULL, NULL}};

	static EnumPropertyItem ogg_codec_items[] = {
		{AUD_CODEC_FLAC, "FLAC", 0, "FLAC", "Free Lossless Audio Codec"},
		{AUD_CODEC_VORBIS, "VORBIS", 0, "Vorbis", "Xiph.Org Vorbis Codec"},
		{0, NULL, 0, NULL, NULL}};

	uiLayout *layout = op->layout;
	wmWindowManager *wm= CTX_wm_manager(C);
	PointerRNA ptr;
	PropertyRNA *prop_format;
	PropertyRNA *prop_codec;
	PropertyRNA *prop_bitrate;

	AUD_Container container = RNA_enum_get(op->ptr, "container");
	AUD_Codec codec = RNA_enum_get(op->ptr, "codec");

	prop_format = RNA_struct_find_property(op->ptr, "format");
	prop_codec = RNA_struct_find_property(op->ptr, "codec");
	prop_bitrate = RNA_struct_find_property(op->ptr, "bitrate");

	RNA_def_property_clear_flag(prop_bitrate, PROP_HIDDEN);
	RNA_def_property_flag(prop_codec, PROP_HIDDEN);
	RNA_def_property_flag(prop_format, PROP_HIDDEN);

	switch(container)
	{
	case AUD_CONTAINER_AC3:
		RNA_def_property_clear_flag(prop_format, PROP_HIDDEN);
		RNA_def_property_enum_items(prop_format, ac3_format_items);
		RNA_def_property_enum_items(prop_codec, all_codec_items);
		RNA_enum_set(op->ptr, "codec", AUD_CODEC_AC3);
		break;
	case AUD_CONTAINER_FLAC:
		RNA_def_property_flag(prop_bitrate, PROP_HIDDEN);
		RNA_def_property_enum_items(prop_codec, all_codec_items);
		RNA_enum_set(op->ptr, "codec", AUD_CODEC_FLAC);
#ifdef WITH_SNDFILE
		RNA_def_property_clear_flag(prop_format, PROP_HIDDEN);
		RNA_def_property_enum_items(prop_format, flac_format_items);
#else
		RNA_enum_set(op->ptr, "format", AUD_FORMAT_S16);
#endif
		break;
	case AUD_CONTAINER_MATROSKA:
		RNA_def_property_clear_flag(prop_codec, PROP_HIDDEN);
		RNA_def_property_enum_items(prop_codec, all_codec_items);

		switch(codec)
		{
		case AUD_CODEC_AAC:
			RNA_enum_set(op->ptr, "format", AUD_FORMAT_S16);
			break;
		case AUD_CODEC_AC3:
			RNA_def_property_enum_items(prop_format, ac3_format_items);
			RNA_def_property_clear_flag(prop_format, PROP_HIDDEN);
			break;
		case AUD_CODEC_FLAC:
			RNA_def_property_flag(prop_bitrate, PROP_HIDDEN);
			RNA_enum_set(op->ptr, "format", AUD_FORMAT_S16);
			break;
		case AUD_CODEC_MP2:
			RNA_enum_set(op->ptr, "format", AUD_FORMAT_S16);
			break;
		case AUD_CODEC_MP3:
			RNA_def_property_enum_items(prop_format, mp3_format_items);
			RNA_def_property_clear_flag(prop_format, PROP_HIDDEN);
			break;
		case AUD_CODEC_PCM:
			RNA_def_property_flag(prop_bitrate, PROP_HIDDEN);
			RNA_def_property_enum_items(prop_format, pcm_format_items);
			RNA_def_property_clear_flag(prop_format, PROP_HIDDEN);
			break;
		case AUD_CODEC_VORBIS:
			RNA_enum_set(op->ptr, "format", AUD_FORMAT_S16);
			break;
		default:
			break;
		}

		break;
	case AUD_CONTAINER_MP2:
		RNA_enum_set(op->ptr, "format", AUD_FORMAT_S16);
		RNA_enum_set(op->ptr, "codec", AUD_CODEC_MP2);
		RNA_def_property_enum_items(prop_codec, all_codec_items);
		break;
	case AUD_CONTAINER_MP3:
		RNA_def_property_clear_flag(prop_format, PROP_HIDDEN);
		RNA_def_property_enum_items(prop_format, mp3_format_items);
		RNA_def_property_enum_items(prop_codec, all_codec_items);
		RNA_enum_set(op->ptr, "codec", AUD_CODEC_MP3);
		break;
	case AUD_CONTAINER_OGG:
		RNA_def_property_clear_flag(prop_codec, PROP_HIDDEN);
		RNA_def_property_enum_items(prop_codec, ogg_codec_items);
		RNA_enum_set(op->ptr, "format", AUD_FORMAT_S16);
		break;
	case AUD_CONTAINER_WAV:
		RNA_def_property_flag(prop_bitrate, PROP_HIDDEN);
		RNA_def_property_clear_flag(prop_format, PROP_HIDDEN);
		RNA_def_property_enum_items(prop_format, pcm_format_items);
		RNA_def_property_enum_items(prop_codec, all_codec_items);
		RNA_enum_set(op->ptr, "codec", AUD_CODEC_PCM);
		break;
	default:
		break;
	}

	RNA_pointer_create(&wm->id, op->type->srna, op->properties, &ptr);

	/* main draw call */
	uiDefAutoButsRNA(layout, &ptr, sound_mixdown_draw_check_prop, '\0');
}
#endif // WITH_AUDASPACE

static void SOUND_OT_mixdown(wmOperatorType *ot)
{
#ifdef WITH_AUDASPACE
	static EnumPropertyItem format_items[] = {
		{AUD_FORMAT_U8, "U8", 0, "U8", "8 bit unsigned"},
		{AUD_FORMAT_S16, "S16", 0, "S16", "16 bit signed"},
		{AUD_FORMAT_S24, "S24", 0, "S24", "24 bit signed"},
		{AUD_FORMAT_S32, "S32", 0, "S32", "32 bit signed"},
		{AUD_FORMAT_FLOAT32, "F32", 0, "F32", "32 bit floating point"},
		{AUD_FORMAT_FLOAT64, "F64", 0, "F64", "64 bit floating point"},
		{0, NULL, 0, NULL, NULL}};

	static EnumPropertyItem container_items[] = {
#ifdef WITH_FFMPEG
		{AUD_CONTAINER_AC3, "AC3", 0, "ac3", "Dolby Digital ATRAC 3"},
#endif
		{AUD_CONTAINER_FLAC, "FLAC", 0, "flac", "Free Lossless Audio Codec"},
#ifdef WITH_FFMPEG
		{AUD_CONTAINER_MATROSKA, "MATROSKA", 0, "mkv", "Matroska"},
		{AUD_CONTAINER_MP2, "MP2", 0, "mp2", "MPEG-1 Audio Layer II"},
		{AUD_CONTAINER_MP3, "MP3", 0, "mp3", "MPEG-2 Audio Layer III"},
#endif
		{AUD_CONTAINER_OGG, "OGG", 0, "ogg", "Xiph.Org Ogg Container"},
		{AUD_CONTAINER_WAV, "WAV", 0, "wav", "Waveform Audio File Format"},
		{0, NULL, 0, NULL, NULL}};

	static EnumPropertyItem codec_items[] = {
#ifdef WITH_FFMPEG
		{AUD_CODEC_AAC, "AAC", 0, "AAC", "Advanced Audio Coding"},
		{AUD_CODEC_AC3, "AC3", 0, "AC3", "Dolby Digital ATRAC 3"},
#endif
		{AUD_CODEC_FLAC, "FLAC", 0, "FLAC", "Free Lossless Audio Codec"},
#ifdef WITH_FFMPEG
		{AUD_CODEC_MP2, "MP2", 0, "MP2", "MPEG-1 Audio Layer II"},
		{AUD_CODEC_MP3, "MP3", 0, "MP3", "MPEG-2 Audio Layer III"},
#endif
		{AUD_CODEC_PCM, "PCM", 0, "PCM", "Pulse Code Modulation (RAW)"},
		{AUD_CODEC_VORBIS, "VORBIS", 0, "Vorbis", "Xiph.Org Vorbis Codec"},
		{0, NULL, 0, NULL, NULL}};

#endif // WITH_AUDASPACE

	/* identifiers */
	ot->name = "Mixdown";
	ot->description = "Mixes the scene's audio to a sound file";
	ot->idname = "SOUND_OT_mixdown";

	/* api callbacks */
	ot->exec = sound_mixdown_exec;
	ot->invoke = sound_mixdown_invoke;

#ifdef WITH_AUDASPACE
	ot->ui = sound_mixdown_draw;
#endif
	/* flags */
	ot->flag = OPTYPE_REGISTER;

	/* properties */
	WM_operator_properties_filesel(ot, FOLDERFILE|SOUNDFILE, FILE_SPECIAL, FILE_SAVE, WM_FILESEL_FILEPATH, FILE_DEFAULTDISPLAY);
#ifdef WITH_AUDASPACE
	RNA_def_int(ot->srna, "accuracy", 1024, 1, 16777216, "Accuracy", "Sample accuracy, important for animation data (the lower the value, the more accurate)", 1, 16777216);
	RNA_def_enum(ot->srna, "container", container_items, AUD_CONTAINER_FLAC, "Container", "File format");
	RNA_def_enum(ot->srna, "codec", codec_items, AUD_CODEC_FLAC, "Codec", "Audio Codec");
	RNA_def_enum(ot->srna, "format", format_items, AUD_FORMAT_S16, "Format", "Sample format");
	RNA_def_int(ot->srna, "bitrate", 192, 32, 512, "Bitrate", "Bitrate in kbit/s", 32, 512);
#endif // WITH_AUDASPACE
}

/* ******************************************************* */

static int sound_poll(bContext *C)
{
	Editing* ed = CTX_data_scene(C)->ed;

	if (!ed || !ed->act_seq || ed->act_seq->type != SEQ_SOUND)
		return 0;

	return 1;
}
/********************* pack operator *********************/

static int sound_pack_exec(bContext *C, wmOperator *op)
{
	Main *bmain= CTX_data_main(C);
	Editing* ed = CTX_data_scene(C)->ed;
	bSound* sound;

	if (!ed || !ed->act_seq || ed->act_seq->type != SEQ_SOUND)
		return OPERATOR_CANCELLED;

	sound = ed->act_seq->sound;

	if (!sound || sound->packedfile)
		return OPERATOR_CANCELLED;

	sound->packedfile= newPackedFile(op->reports, sound->name, ID_BLEND_PATH(bmain, &sound->id));
	sound_load(CTX_data_main(C), sound);

	return OPERATOR_FINISHED;
}

static void SOUND_OT_pack(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Pack Sound";
	ot->description = "Pack the sound into the current blend file";
	ot->idname = "SOUND_OT_pack";

	/* api callbacks */
	ot->exec = sound_pack_exec;
	ot->poll = sound_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

/********************* unpack operator *********************/

static int sound_unpack_exec(bContext *C, wmOperator *op)
{
	int method= RNA_enum_get(op->ptr, "method");
	bSound* sound= NULL;

	/* find the suppplied image by name */
	if (RNA_struct_property_is_set(op->ptr, "id")) {
		char sndname[MAX_ID_NAME-2];
		RNA_string_get(op->ptr, "id", sndname);
		sound = BLI_findstring(&CTX_data_main(C)->sound, sndname, offsetof(ID, name) + 2);
	}

	if (!sound || !sound->packedfile)
		return OPERATOR_CANCELLED;

	if (G.fileflags & G_AUTOPACK)
		BKE_report(op->reports, RPT_WARNING, "AutoPack is enabled, so image will be packed again on file save");

	unpackSound(CTX_data_main(C), op->reports, sound, method);

	return OPERATOR_FINISHED;
}

static int sound_unpack_invoke(bContext *C, wmOperator *op, wmEvent *UNUSED(event))
{
	Editing* ed = CTX_data_scene(C)->ed;
	bSound* sound;

	if (RNA_struct_property_is_set(op->ptr, "id"))
		return sound_unpack_exec(C, op);

	if (!ed || !ed->act_seq || ed->act_seq->type != SEQ_SOUND)
		return OPERATOR_CANCELLED;

	sound = ed->act_seq->sound;

	if (!sound || !sound->packedfile)
		return OPERATOR_CANCELLED;

	if (G.fileflags & G_AUTOPACK)
		BKE_report(op->reports, RPT_WARNING, "AutoPack is enabled, so image will be packed again on file save");

	unpack_menu(C, "SOUND_OT_unpack", sound->id.name+2, sound->name, "sounds", sound->packedfile);

	return OPERATOR_FINISHED;
}

static void SOUND_OT_unpack(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Unpack Sound";
	ot->description = "Unpack the sound to the samples filename";
	ot->idname = "SOUND_OT_unpack";

	/* api callbacks */
	ot->exec = sound_unpack_exec;
	ot->invoke = sound_unpack_invoke;
	ot->poll = sound_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	RNA_def_enum(ot->srna, "method", unpack_method_items, PF_USE_LOCAL, "Method", "How to unpack");
	RNA_def_string(ot->srna, "id", "", MAX_ID_NAME-2, "Sound Name", "Sound datablock name to unpack"); /* XXX, weark!, will fail with library, name collisions */
}

/* ******************************************************* */

void ED_operatortypes_sound(void)
{
	WM_operatortype_append(SOUND_OT_open);
	WM_operatortype_append(SOUND_OT_open_mono);
	WM_operatortype_append(SOUND_OT_mixdown);
	WM_operatortype_append(SOUND_OT_pack);
	WM_operatortype_append(SOUND_OT_unpack);
	WM_operatortype_append(SOUND_OT_update_animation_flags);
	WM_operatortype_append(SOUND_OT_bake_animation);
}
