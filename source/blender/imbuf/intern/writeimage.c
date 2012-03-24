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

#include "imbuf.h"

short IMB_saveiff(struct ImBuf *ibuf, const char *name, int flags)
{
	ImFileType *type;

	if (ibuf == NULL) return (FALSE);
	ibuf->flags = flags;

	for (type=IMB_FILE_TYPES; type->is_a; type++) {
		if (type->save && type->ftype(type, ibuf)) {
			if (!(type->flag & IM_FTYPE_FLOAT)) {
				if (ibuf->rect==NULL && ibuf->rect_float)
					IMB_rect_from_float(ibuf);
			}
			return type->save(ibuf, name, flags);
		}
	}

	fprintf(stderr, "Couldn't save picture.\n");

	return FALSE;
}

