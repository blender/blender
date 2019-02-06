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
 * The Original Code is Copyright (C) 2011 Blender Foundation.
 * All rights reserved.
 */

/** \file \ingroup blf
 *
 * API for accessing font files.
 */

#include <stdlib.h>
#include <string.h>

#include "BLF_api.h"

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "BKE_appdir.h"

#ifdef WITH_INTERNATIONAL

#include "BLI_fileops.h"
#include "BLI_string.h"

struct FontBuf {
	const char *filename;
	uchar *data;
	int data_len;
};

static struct FontBuf unifont_ttf =  {"droidsans.ttf.gz"};
static struct FontBuf unifont_mono_ttf = {"bmonofont-i18n.ttf.gz"};

static void fontbuf_load(struct FontBuf *fb)
{
	const char *fontpath = BKE_appdir_folder_id(BLENDER_DATAFILES, "fonts");
	if (fontpath) {
		char unifont_path[1024];
		BLI_snprintf(unifont_path, sizeof(unifont_path), "%s/%s", fontpath, fb->filename);
		fb->data = (uchar *)BLI_file_ungzip_to_mem(unifont_path, &fb->data_len);

	}
	else {
		printf("%s: 'fonts' data path not found for '%s', continuing\n", __func__, fb->filename);
	}
}

static void fontbuf_free(struct FontBuf *fb)
{
	MEM_SAFE_FREE(fb->data);
	fb->data_len = 0;
}

static uchar *fontbuf_get_mem(struct FontBuf *fb, int *r_size)
{
	if (fb->data == NULL) {
		fontbuf_load(fb);
	}
	*r_size = fb->data_len;
	return fb->data;
}

#endif /* WITH_INTERNATIONAL */


uchar *BLF_get_unifont(int *r_unifont_size)
{
#ifdef WITH_INTERNATIONAL
	return fontbuf_get_mem(&unifont_ttf, r_unifont_size);
#else
	UNUSED_VARS(r_unifont_size);
	return NULL;
#endif
}

uchar *BLF_get_unifont_mono(int *r_unifont_size)
{
#ifdef WITH_INTERNATIONAL
	return fontbuf_get_mem(&unifont_mono_ttf, r_unifont_size);
#else
	UNUSED_VARS(r_unifont_size);
	return NULL;
#endif
}

void BLF_free_unifont(void)
{
#ifdef WITH_INTERNATIONAL
	fontbuf_free(&unifont_ttf);
#endif
}

void BLF_free_unifont_mono(void)
{
#ifdef WITH_INTERNATIONAL
	fontbuf_free(&unifont_mono_ttf);
#endif
}
