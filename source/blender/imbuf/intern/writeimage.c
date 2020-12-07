/*
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
 * writeimage.c
 */

/** \file
 * \ingroup imbuf
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "BLI_path_util.h"
#include "BLI_utildefines.h"

#include "IMB_filetype.h"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "IMB_colormanagement.h"
#include "IMB_colormanagement_intern.h"

static bool prepare_write_imbuf(const ImFileType *type, ImBuf *ibuf)
{
  return IMB_prepare_write_ImBuf((type->flag & IM_FTYPE_FLOAT), ibuf);
}

bool IMB_saveiff(struct ImBuf *ibuf, const char *filepath, int flags)
{
  errno = 0;

  BLI_assert(!BLI_path_is_rel(filepath));

  if (ibuf == NULL) {
    return false;
  }
  ibuf->flags = flags;

  const ImFileType *type = IMB_file_type_from_ibuf(ibuf);
  if (type != NULL) {
    if (type->save != NULL) {
      prepare_write_imbuf(type, ibuf);
      return type->save(ibuf, filepath, flags);
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
