/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

bool IMB_saveiff(struct ImBuf *ibuf, const char *filepath, int flags)
{
  errno = 0;

  BLI_assert(!BLI_path_is_rel(filepath));

  if (ibuf == nullptr) {
    return false;
  }
  ibuf->flags = flags;

  const ImFileType *type = IMB_file_type_from_ibuf(ibuf);
  if (type == nullptr || type->save == nullptr) {
    fprintf(stderr, "Couldn't save picture.\n");
    return false;
  }

  /* If writing byte image from float buffer, create a byte buffer for writing.
   *
   * For color managed image writing, IMB_colormanagement_imbuf_for_write should
   * have already created this byte buffer. This is a basic fallback for other
   * cases where we do not have a specific desired output colorspace. */
  if (!(type->flag & IM_FTYPE_FLOAT)) {
    if (ibuf->byte_buffer.data == nullptr && ibuf->float_buffer.data) {
      ibuf->rect_colorspace = colormanage_colorspace_get_roled(COLOR_ROLE_DEFAULT_BYTE);
      IMB_rect_from_float(ibuf);
    }
  }

  return type->save(ibuf, filepath, flags);
}
