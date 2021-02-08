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
 */

/** \file
 * \ingroup imbuf
 */

#include <stdlib.h>
#include <string.h>

#include "BLI_linklist.h"
#include "BLI_listbase.h" /* Needed due to import of BLO_readfile.h */
#include "BLI_utildefines.h"

#include "BLO_blend_defs.h"
#include "BLO_readfile.h"

#include "BKE_icons.h"
#include "BKE_idtype.h"
#include "BKE_main.h"

#include "DNA_ID.h" /* For preview images... */

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_thumbs.h"

#include "MEM_guardedalloc.h"

ImBuf *IMB_thumb_load_blend(const char *blen_path, const char *blen_group, const char *blen_id)
{
  ImBuf *ima = NULL;

  if (blen_group && blen_id) {
    LinkNode *ln, *names, *lp, *previews = NULL;
    struct BlendHandle *libfiledata = BLO_blendhandle_from_file(blen_path, NULL);
    int idcode = BKE_idtype_idcode_from_name(blen_group);
    int i, nprevs, nnames;

    if (libfiledata == NULL) {
      return ima;
    }

    /* Note: we should handle all previews for a same group at once, would avoid reopening
     * `.blend` file for each and every ID. However, this adds some complexity,
     * so keep it for later. */
    names = BLO_blendhandle_get_datablock_names(libfiledata, idcode, false, &nnames);
    previews = BLO_blendhandle_get_previews(libfiledata, idcode, &nprevs);

    BLO_blendhandle_close(libfiledata);

    if (!previews || (nnames != nprevs)) {
      if (previews != 0) {
        /* No previews at all is not a bug! */
        printf("%s: error, found %d items, %d previews\n", __func__, nnames, nprevs);
      }
      BLI_linklist_free(previews, BKE_previewimg_freefunc);
      BLI_linklist_freeN(names);
      return ima;
    }

    for (i = 0, ln = names, lp = previews; i < nnames; i++, ln = ln->next, lp = lp->next) {
      const char *blockname = ln->link;
      PreviewImage *img = lp->link;

      if (STREQ(blockname, blen_id)) {
        if (img) {
          ima = BKE_previewimg_to_imbuf(img, ICON_SIZE_PREVIEW);
        }
        break;
      }
    }

    BLI_linklist_free(previews, BKE_previewimg_freefunc);
    BLI_linklist_freeN(names);
  }
  else {
    BlendThumbnail *data;

    data = BLO_thumbnail_from_file(blen_path);
    ima = BKE_main_thumbnail_to_imbuf(NULL, data);

    if (data) {
      MEM_freeN(data);
    }
  }

  return ima;
}

/* add a fake passepartout overlay to a byte buffer, use for blend file thumbnails */
#define MARGIN 2

void IMB_thumb_overlay_blend(unsigned int *thumb, int width, int height, float aspect)
{
  unsigned char *px = (unsigned char *)thumb;
  int margin_l = MARGIN;
  int margin_b = MARGIN;
  int margin_r = width - MARGIN;
  int margin_t = height - MARGIN;

  if (aspect < 1.0f) {
    margin_l = (int)((width - ((float)width * aspect)) / 2.0f);
    margin_l += MARGIN;
    CLAMP(margin_l, MARGIN, (width / 2));
    margin_r = width - margin_l;
  }
  else if (aspect > 1.0f) {
    margin_b = (int)((height - ((float)height / aspect)) / 2.0f);
    margin_b += MARGIN;
    CLAMP(margin_b, MARGIN, (height / 2));
    margin_t = height - margin_b;
  }

  {
    int x, y;
    int stride_x = (margin_r - margin_l) - 2;

    for (y = 0; y < height; y++) {
      for (x = 0; x < width; x++, px += 4) {
        int hline = 0, vline = 0;
        if ((x > margin_l && x < margin_r) && (y > margin_b && y < margin_t)) {
          /* interior. skip */
          x += stride_x;
          px += stride_x * 4;
        }
        else if ((hline = (((x == margin_l || x == margin_r)) && y >= margin_b &&
                           y <= margin_t)) ||
                 (vline = (((y == margin_b || y == margin_t)) && x >= margin_l &&
                           x <= margin_r))) {
          /* dashed line */
          if ((hline && y % 2) || (vline && x % 2)) {
            px[0] = px[1] = px[2] = 0;
            px[3] = 255;
          }
        }
        else {
          /* outside, fill in alpha, like passepartout */
          px[0] *= 0.5f;
          px[1] *= 0.5f;
          px[2] *= 0.5f;
          px[3] = (px[3] * 0.5f) + 96;
        }
      }
    }
  }
}
