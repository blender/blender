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
 * Contributor(s): Thomas Beck.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/imbuf/intern/thumbs_font.c
 *  \ingroup imbuf
 */

#include "BLI_utildefines.h"
#include "BLI_string.h"
#include "BLI_fileops.h"
#include "BLI_hash_md5.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "IMB_thumbs.h"


/* XXX, bad level call */
#include "../../blenfont/BLF_api.h"
#include "../../blentranslation/BLT_translation.h"

static const char *thumb_str[] = {
    N_("AaBbCc"),

    N_("The quick"),
    N_("brown fox"),
    N_("jumps over"),
    N_("the lazy dog"),
};

struct ImBuf *IMB_thumb_load_font(const char *filename, unsigned int x, unsigned int y)
{
	const int font_size = y / 4;

	struct ImBuf *ibuf;
	float font_color[4];

	/* create a white image (theme color is used for drawing) */
	font_color[0] = font_color[1] = font_color[2] = 1.0f;

	/* fill with zero alpha */
	font_color[3] = 0.0f;

	ibuf = IMB_allocImBuf(x, y, 32, IB_rect | IB_metadata);
	IMB_rectfill(ibuf, font_color);

	/* draw with full alpha */
	font_color[3] = 1.0f;

	BLF_thumb_preview(
	        filename, thumb_str, ARRAY_SIZE(thumb_str),
	        font_color, font_size,
	        (unsigned char *)ibuf->rect, ibuf->x, ibuf->y, ibuf->channels);

	return ibuf;
}

bool IMB_thumb_load_font_get_hash(char *r_hash)
{
	char buf[1024];
	char *str = buf;
	size_t len = 0;

	int draw_str_lines = ARRAY_SIZE(thumb_str);
	int i;

	unsigned char digest[16];

	len += BLI_strncpy_rlen(str + len, THUMB_DEFAULT_HASH, sizeof(buf) - len);

	for (i = 0; (i < draw_str_lines) && (len < sizeof(buf)); i++) {
		len += BLI_strncpy_rlen(str + len, BLT_translate_do(BLT_I18NCONTEXT_DEFAULT, thumb_str[i]), sizeof(buf) - len);
	}

	BLI_hash_md5_buffer(str, len, digest);
	r_hash[0] = '\0';
	BLI_hash_md5_to_hexdigest(digest, r_hash);

	return true;
}
