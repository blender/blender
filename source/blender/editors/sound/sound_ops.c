/*
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

#include "DNA_packedFile_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_sequence_types.h"
#include "DNA_sound_types.h"
#include "DNA_userdef_types.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_report.h"
#include "BKE_packedFile.h"
#include "BKE_sound.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "UI_interface.h"

#include "WM_api.h"
#include "WM_types.h"

#include "AUD_C-API.h"

#include "ED_sound.h"
#include "ED_util.h"

#include "sound_intern.h"

/******************** open sound operator ********************/

static void open_init(bContext *C, wmOperator *op)
{
	PropertyPointerRNA *pprop;
	
	op->customdata= pprop= MEM_callocN(sizeof(PropertyPointerRNA), "OpenPropertyPointerRNA");
	uiIDContextProperty(C, &pprop->ptr, &pprop->prop);
}

static int open_exec(bContext *C, wmOperator *op)
{
	char path[FILE_MAX];
	bSound *sound;
	PropertyPointerRNA *pprop;
	PointerRNA idptr;
	AUD_SoundInfo info;

	RNA_string_get(op->ptr, "filepath", path);
	sound = sound_new_file(CTX_data_main(C), path);

	if(!op->customdata)
		open_init(C, op);
	
	if (sound==NULL || sound->playback_handle == NULL) {
		if(op->customdata) MEM_freeN(op->customdata);
		BKE_report(op->reports, RPT_ERROR, "Unsupported audio format");
		return OPERATOR_CANCELLED;
	}

	info = AUD_getInfo(sound->playback_handle);

	if (info.specs.channels == AUD_CHANNELS_INVALID) {
		sound_delete(C, sound);
		if(op->customdata) MEM_freeN(op->customdata);
		BKE_report(op->reports, RPT_ERROR, "Unsupported audio format");
		return OPERATOR_CANCELLED;
	}

	if (RNA_boolean_get(op->ptr, "cache")) {
		sound_cache(sound, 0);
	}
	
	/* hook into UI */
	pprop= op->customdata;
	
	if(pprop->prop) {
		/* when creating new ID blocks, use is already 1, but RNA
		 * pointer se also increases user, so this compensates it */
		sound->id.us--;
		
		RNA_id_pointer_create(&sound->id, &idptr);
		RNA_property_pointer_set(&pprop->ptr, pprop->prop, idptr);
		RNA_property_update(C, &pprop->ptr, pprop->prop);
	}

	if(op->customdata) MEM_freeN(op->customdata);
	return OPERATOR_FINISHED;
}

static int open_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	if(!RNA_property_is_set(op->ptr, "relative_path"))
		RNA_boolean_set(op->ptr, "relative_path", U.flag & USER_RELPATHS);
	
	if(RNA_property_is_set(op->ptr, "filepath"))
		return open_exec(C, op);
	
	open_init(C, op);
	
	return WM_operator_filesel(C, op, event);
}

void SOUND_OT_open(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Open Sound";
	ot->description= "Load a sound file";
	ot->idname= "SOUND_OT_open";

	/* api callbacks */
	ot->exec= open_exec;
	ot->invoke= open_invoke;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	WM_operator_properties_filesel(ot, FOLDERFILE|SOUNDFILE|MOVIEFILE, FILE_SPECIAL, FILE_OPENFILE, WM_FILESEL_FILEPATH | WM_FILESEL_RELPATH);
	RNA_def_boolean(ot->srna, "cache", FALSE, "Cache", "Cache the sound in memory.");
}

/* ******************************************************* */

static int sound_poll(bContext *C)
{
	Editing* ed = CTX_data_scene(C)->ed;

	if(!ed || !ed->act_seq || ed->act_seq->type != SEQ_SOUND)
		return 0;

	return 1;
}
/********************* pack operator *********************/

static int pack_exec(bContext *C, wmOperator *op)
{
	Editing* ed = CTX_data_scene(C)->ed;
	bSound* sound;

	if(!ed || !ed->act_seq || ed->act_seq->type != SEQ_SOUND)
		return OPERATOR_CANCELLED;

	sound = ed->act_seq->sound;

	if(!sound || sound->packedfile)
		return OPERATOR_CANCELLED;

	sound->packedfile= newPackedFile(op->reports, sound->name);
	sound_load(CTX_data_main(C), sound);

	return OPERATOR_FINISHED;
}

static void SOUND_OT_pack(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Pack Sound";
	ot->description= "Pack the sound into the current blend file";
	ot->idname= "SOUND_OT_pack";

	/* api callbacks */
	ot->exec= pack_exec;
	ot->poll= sound_poll;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/********************* unpack operator *********************/

static int sound_unpack_exec(bContext *C, wmOperator *op)
{
	int method= RNA_enum_get(op->ptr, "method");
	bSound* sound= NULL;

	/* find the suppplied image by name */
	if (RNA_property_is_set(op->ptr, "id")) {
		char sndname[22];
		RNA_string_get(op->ptr, "id", sndname);
		sound = BLI_findstring(&CTX_data_main(C)->sound, sndname, offsetof(ID, name) + 2);
	}

	if(!sound || !sound->packedfile)
		return OPERATOR_CANCELLED;

	if(G.fileflags & G_AUTOPACK)
		BKE_report(op->reports, RPT_WARNING, "AutoPack is enabled, so image will be packed again on file save.");

	unpackSound(CTX_data_main(C), op->reports, sound, method);

	return OPERATOR_FINISHED;
}

static int sound_unpack_invoke(bContext *C, wmOperator *op, wmEvent *UNUSED(event))
{
	Editing* ed = CTX_data_scene(C)->ed;
	bSound* sound;

	if(RNA_property_is_set(op->ptr, "id"))
		return sound_unpack_exec(C, op);

	if(!ed || !ed->act_seq || ed->act_seq->type != SEQ_SOUND)
		return OPERATOR_CANCELLED;

	sound = ed->act_seq->sound;

	if(!sound || !sound->packedfile)
		return OPERATOR_CANCELLED;

	if(G.fileflags & G_AUTOPACK)
		BKE_report(op->reports, RPT_WARNING, "AutoPack is enabled, so image will be packed again on file save.");

	unpack_menu(C, "SOUND_OT_unpack", sound->id.name+2, sound->name, "audio", sound->packedfile);

	return OPERATOR_FINISHED;
}

static void SOUND_OT_unpack(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Unpack Sound";
	ot->description= "Unpack the sound to the samples filename";
	ot->idname= "SOUND_OT_unpack";

	/* api callbacks */
	ot->exec= sound_unpack_exec;
	ot->invoke= sound_unpack_invoke;
	ot->poll= sound_poll;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	RNA_def_enum(ot->srna, "method", unpack_method_items, PF_USE_LOCAL, "Method", "How to unpack.");
	RNA_def_string(ot->srna, "id", "", 21, "Sound Name", "Sound datablock name to unpack."); /* XXX, weark!, will fail with library, name collisions */
}

/* ******************************************************* */

void ED_operatortypes_sound(void)
{
	WM_operatortype_append(SOUND_OT_open);
	WM_operatortype_append(SOUND_OT_pack);
	WM_operatortype_append(SOUND_OT_unpack);
}
