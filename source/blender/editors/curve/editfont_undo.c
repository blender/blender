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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/curve/editfont_undo.c
 *  \ingroup edcurve
 */

#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "DNA_curve_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_context.h"
#include "BKE_font.h"

#include "ED_curve.h"
#include "ED_util.h"

/* TODO, remove */
#define MAXTEXT 32766

static void undoFont_to_editFont(void *strv, void *ecu, void *UNUSED(obdata))
{
	Curve *cu = (Curve *)ecu;
	EditFont *ef = cu->editfont;
	const char *str = strv;

	ef->pos = *((const short *)str);
	ef->len = *((const short *)(str + 2));

	memcpy(ef->textbuf, str + 4, (ef->len + 1) * sizeof(wchar_t));
	memcpy(ef->textbufinfo, str + 4 + (ef->len + 1) * sizeof(wchar_t), ef->len * sizeof(CharInfo));

	ef->selstart = ef->selend = 0;

}

static void *editFont_to_undoFont(void *ecu, void *UNUSED(obdata))
{
	Curve *cu = (Curve *)ecu;
	EditFont *ef = cu->editfont;
	char *str;

	/* The undo buffer includes [MAXTEXT+6]=actual string and [MAXTEXT+4]*sizeof(CharInfo)=charinfo */
	str = MEM_callocN((MAXTEXT + 6) * sizeof(wchar_t) + (MAXTEXT + 4) * sizeof(CharInfo), "string undo");

	/* Copy the string and string information */
	memcpy(str + 4, ef->textbuf, (ef->len + 1) * sizeof(wchar_t));
	memcpy(str + 4 + (ef->len + 1) * sizeof(wchar_t), ef->textbufinfo, ef->len * sizeof(CharInfo));

	*((short *)(str + 0)) = ef->pos;
	*((short *)(str + 2)) = ef->len;

	return str;
}

static void free_undoFont(void *strv)
{
	MEM_freeN(strv);
}

static void *get_undoFont(bContext *C)
{
	Object *obedit = CTX_data_edit_object(C);
	if (obedit && obedit->type == OB_FONT) {
		return obedit->data;
	}
	return NULL;
}

/* and this is all the undo system needs to know */
void undo_push_font(bContext *C, const char *name)
{
	undo_editmode_push(C, name, get_undoFont, free_undoFont, undoFont_to_editFont, editFont_to_undoFont, NULL);
}
