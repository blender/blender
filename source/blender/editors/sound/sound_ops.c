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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdio.h>

#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_sound_types.h"
#include "DNA_windowmanager_types.h"

#include "BKE_context.h"
#include "BKE_main.h"
#include "BKE_report.h"
#include "BKE_sound.h"

#include "ED_sound.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "AUD_C-API.h"

#include "sound_intern.h"

/******************** open sound operator ********************/

static int open_exec(bContext *C, wmOperator *op)
{
	char filename[FILE_MAX];
	bSound *sound;
	AUD_SoundInfo info;

	RNA_string_get(op->ptr, "filename", filename);

	sound = sound_new_file(CTX_data_main(C), filename);

	if (sound==NULL || sound->handle == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Unsupported audio format");
		return OPERATOR_CANCELLED;
	}

	info = AUD_getInfo(sound->handle);

	if (info.specs.format == AUD_FORMAT_INVALID) {
		sound_delete(C, sound);
		BKE_report(op->reports, RPT_ERROR, "Unsupported audio format");
		return OPERATOR_CANCELLED;
	}

	return OPERATOR_FINISHED;
}

static int open_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	return WM_operator_filesel(C, op, event);
}

void SOUND_OT_open(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Open Sound";
	ot->idname= "SOUND_OT_open";
	ot->description= "Load a sound file into blender";

	/* api callbacks */
	ot->exec= open_exec;
	ot->invoke= open_invoke;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	WM_operator_properties_filesel(ot, FOLDERFILE|SOUNDFILE|MOVIEFILE);
}

/* ******************************************************* */

void ED_operatortypes_sound(void)
{
	WM_operatortype_append(SOUND_OT_open);
}
