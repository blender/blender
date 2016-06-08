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

typedef struct UndoFont {
	wchar_t *textbuf;
	struct CharInfo *textbufinfo;

	int len, pos;
} UndoFont;

static void undoFont_to_editFont(void *uf_v, void *ecu, void *UNUSED(obdata))
{
	Curve *cu = (Curve *)ecu;
	EditFont *ef = cu->editfont;
	const UndoFont *uf = uf_v;

	size_t final_size;

	final_size = sizeof(wchar_t) * (uf->len + 1);
	memcpy(ef->textbuf, uf->textbuf, final_size);

	final_size = sizeof(CharInfo) * (uf->len + 1);
	memcpy(ef->textbufinfo, uf->textbufinfo, final_size);

	ef->pos = uf->pos;
	ef->len = uf->len;

	ef->selstart = ef->selend = 0;
}

static void *editFont_to_undoFont(void *ecu, void *UNUSED(obdata))
{
	Curve *cu = (Curve *)ecu;
	EditFont *ef = cu->editfont;

	UndoFont *uf = MEM_callocN(sizeof(*uf), __func__);

	size_t final_size;

	final_size = sizeof(wchar_t) * (ef->len + 1);
	uf->textbuf = MEM_mallocN(final_size, __func__);
	memcpy(uf->textbuf, ef->textbuf, final_size);

	final_size = sizeof(CharInfo) * (ef->len + 1);
	uf->textbufinfo = MEM_mallocN(final_size, __func__);
	memcpy(uf->textbufinfo, ef->textbufinfo, final_size);

	uf->pos = ef->pos;
	uf->len = ef->len;

	return uf;
}

static void free_undoFont(void *uf_v)
{
	UndoFont *uf = uf_v;
	MEM_freeN(uf->textbuf);
	MEM_freeN(uf->textbufinfo);
	MEM_freeN(uf);
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
