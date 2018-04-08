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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 * writeimage.c
 *
 */

/** \file blender/imbuf/intern/writeimage.c
 *  \ingroup imbuf
 */


#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "BLI_utildefines.h"
#include "BLI_path_util.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"
#include "IMB_filetype.h"

#include "IMB_colormanagement.h"
#include "IMB_colormanagement_intern.h"

static bool prepare_write_imbuf(const ImFileType *type, ImBuf *ibuf)
{
	return IMB_prepare_write_ImBuf((type->flag & IM_FTYPE_FLOAT), ibuf);
}

short IMB_saveiff(struct ImBuf *ibuf, const char *name, int flags)
{
	const ImFileType *type;

	errno = 0;

	BLI_assert(!BLI_path_is_rel(name));

	if (ibuf == NULL) return (false);
	ibuf->flags = flags;

	for (type = IMB_FILE_TYPES; type < IMB_FILE_TYPES_LAST; type++) {
		if (type->save && type->ftype(type, ibuf)) {
			short result = false;

			prepare_write_imbuf(type, ibuf);

			result = type->save(ibuf, name, flags);

			return result;
		}
	}

	fprintf(stderr, "Couldn't save picture.\n");

	return false;
}

bool IMB_prepare_write_ImBuf(const bool isfloat, ImBuf *ibuf)
{
	bool changed = false;

	if (isfloat) {
		/* pass */
	}
	else {
		if (ibuf->rect == NULL && ibuf->rect_float) {
			ibuf->rect_colorspace = colormanage_colorspace_get_roled(COLOR_ROLE_DEFAULT_BYTE);
			IMB_rect_from_float(ibuf);
			if (ibuf->rect != NULL) {
				changed = true;
			}
		}
	}

	return changed;
}
