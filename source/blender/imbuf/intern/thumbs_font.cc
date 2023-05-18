/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#include "BLI_fileops.h"
#include "BLI_hash_md5.h"
#include "BLI_utildefines.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "IMB_thumbs.h" /* own include. */

/* XXX, bad level call */
#include "../../blenfont/BLF_api.h"

/* Only change if we need to update the previews in the on-disk cache. */
#define FONT_THUMB_VERSION "1.0.1"

struct ImBuf *IMB_thumb_load_font(const char *filename, uint x, uint y)
{
  struct ImBuf *ibuf = IMB_allocImBuf(x, y, 32, IB_rect | IB_metadata);

  /* fill with white and zero alpha */
  const float col[4] = {1.0f, 1.0f, 1.0f, 0.0f};
  IMB_rectfill(ibuf, col);

  if (!BLF_thumb_preview(filename, ibuf->byte_buffer.data, ibuf->x, ibuf->y, ibuf->channels)) {
    IMB_freeImBuf(ibuf);
    ibuf = nullptr;
  }

  return ibuf;
}

bool IMB_thumb_load_font_get_hash(char *r_hash)
{
  uchar digest[16];
  BLI_hash_md5_buffer(FONT_THUMB_VERSION, sizeof(FONT_THUMB_VERSION), digest);
  r_hash[0] = '\0';
  BLI_hash_md5_to_hexdigest(digest, r_hash);

  return true;
}
