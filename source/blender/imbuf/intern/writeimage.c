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

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"
#include "IMB_filetype.h"

#include "IMB_colormanagement.h"
#include "IMB_colormanagement_intern.h"

#include "imbuf.h"

static ImBuf *prepare_write_imbuf(ImFileType *type, ImBuf *ibuf)
{
	ImBuf *write_ibuf = ibuf;

	if (type->flag & IM_FTYPE_FLOAT) {
		/* pass */
	}
	else {
		if (ibuf->rect == NULL && ibuf->rect_float) {
			ibuf->rect_colorspace = colormanage_colorspace_get_roled(COLOR_ROLE_DEFAULT_BYTE);
			IMB_rect_from_float(ibuf);
		}
	}

	return write_ibuf;
}

short IMB_saveiff(struct ImBuf *ibuf, const char *name, int flags)
{
	ImFileType *type;

	if (ibuf == NULL) return (false);
	ibuf->flags = flags;

	for (type = IMB_FILE_TYPES; type < IMB_FILE_TYPES_LAST; type++) {
		if (type->save && type->ftype(type, ibuf)) {
			ImBuf *write_ibuf;
			short result = false;

			write_ibuf = prepare_write_imbuf(type, ibuf);

			result = type->save(write_ibuf, name, flags);

			if (write_ibuf != ibuf)
				IMB_freeImBuf(write_ibuf);

			return result;
		}
	}

	fprintf(stderr, "Couldn't save picture.\n");

	return false;
}

