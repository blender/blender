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
 * The Original Code is Copyright (C) 2011 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenfont/intern/blf_font_i18n.c
 *  \ingroup blf
 *
 * API for accessing font files.
 */

#include <stdlib.h>
#include <string.h>

#include "BLF_api.h"

#include "MEM_guardedalloc.h"

#include "BLI_fileops.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_appdir.h"

#ifdef WITH_INTERNATIONAL
static const char unifont_filename[] = "droidsans.ttf.gz";
static unsigned char *unifont_ttf = NULL;
static int unifont_size = 0;
static const char unifont_mono_filename[] = "bmonofont-i18n.ttf.gz";
static unsigned char *unifont_mono_ttf = NULL;
static int unifont_mono_size = 0;
#endif  /* WITH_INTERNATIONAL */

unsigned char *BLF_get_unifont(int *r_unifont_size)
{
#ifdef WITH_INTERNATIONAL
	if (unifont_ttf == NULL) {
		const char * const fontpath = BKE_appdir_folder_id(BLENDER_DATAFILES, "fonts");
		if (fontpath) {
			char unifont_path[1024];

			BLI_snprintf(unifont_path, sizeof(unifont_path), "%s/%s", fontpath, unifont_filename);

			unifont_ttf = (unsigned char *)BLI_file_ungzip_to_mem(unifont_path, &unifont_size);
		}
		else {
			printf("%s: 'fonts' data path not found for international font, continuing\n", __func__);
		}
	}

	*r_unifont_size = unifont_size;

	return unifont_ttf;
#else
	(void)r_unifont_size;
	return NULL;
#endif
}

void BLF_free_unifont(void)
{
#ifdef WITH_INTERNATIONAL
	if (unifont_ttf)
		MEM_freeN(unifont_ttf);
#else
#endif
}

unsigned char *BLF_get_unifont_mono(int *r_unifont_size)
{
#ifdef WITH_INTERNATIONAL
	if (unifont_mono_ttf == NULL) {
		const char *fontpath = BKE_appdir_folder_id(BLENDER_DATAFILES, "fonts");
		if (fontpath) {
			char unifont_path[1024];

			BLI_snprintf(unifont_path, sizeof(unifont_path), "%s/%s", fontpath, unifont_mono_filename);

			unifont_mono_ttf = (unsigned char *)BLI_file_ungzip_to_mem(unifont_path, &unifont_mono_size);
		}
		else {
			printf("%s: 'fonts' data path not found for international monospace font, continuing\n", __func__);
		}
	}

	*r_unifont_size = unifont_mono_size;

	return unifont_mono_ttf;
#else
	(void)r_unifont_size;
	return NULL;
#endif
}

void BLF_free_unifont_mono(void)
{
#ifdef WITH_INTERNATIONAL
	if (unifont_mono_ttf)
		MEM_freeN(unifont_mono_ttf);
#else
#endif
}
