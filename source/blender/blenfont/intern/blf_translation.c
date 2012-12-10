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

/** \file blender/blenfont/intern/blf_translation.c
 *  \ingroup blf
 */

#include <stdlib.h>
#include <string.h>

#include "BLF_translation.h"

#ifdef WITH_INTERNATIONAL

#include "boost_locale_wrapper.h"

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_path_util.h"
#include "BLI_fileops.h"

#include "DNA_userdef_types.h" /* For user settings. */

static const char unifont_filename[] = "droidsans.ttf.gz";
static unsigned char *unifont_ttf = NULL;
static int unifont_size = 0;

unsigned char *BLF_get_unifont(int *unifont_size_r)
{
	if (unifont_ttf == NULL) {
		char *fontpath = BLI_get_folder(BLENDER_DATAFILES, "fonts");
		if (fontpath) {
			char unifont_path[1024];

			BLI_snprintf(unifont_path, sizeof(unifont_path), "%s/%s", fontpath, unifont_filename);

			unifont_ttf = (unsigned char *)BLI_file_ungzip_to_mem(unifont_path, &unifont_size);
		}
		else {
			printf("%s: 'fonts' data path not found for international font, continuing\n", __func__);
		}
	}

	*unifont_size_r = unifont_size;

	return unifont_ttf;
}

void BLF_free_unifont(void)
{
	if (unifont_ttf)
		MEM_freeN(unifont_ttf);
}

#endif

const char *BLF_pgettext(const char *msgctxt, const char *msgid)
{
#ifdef WITH_INTERNATIONAL
	if (msgid && msgid[0]) {
		return bl_locale_pgettext(msgctxt, msgid);
	}
	return "";
#else
	(void)msgctxt;
	return msgid;
#endif
}

int BLF_translate_iface(void)
{
#ifdef WITH_INTERNATIONAL
	return (U.transopts & USER_DOTRANSLATE) && (U.transopts & USER_TR_IFACE);
#else
	return 0;
#endif
}

int BLF_translate_tooltips(void)
{
#ifdef WITH_INTERNATIONAL
	return (U.transopts & USER_DOTRANSLATE) && (U.transopts & USER_TR_TOOLTIPS);
#else
	return 0;
#endif
}

const char *BLF_translate_do_iface(const char *msgctxt, const char *msgid)
{
#ifdef WITH_INTERNATIONAL
	if (BLF_translate_iface()) {
		return BLF_pgettext(msgctxt, msgid);
	}
	else {
		return msgid;
	}
#else
	(void)msgctxt;
	return msgid;
#endif
}

const char *BLF_translate_do_tooltip(const char *msgctxt, const char *msgid)
{
#ifdef WITH_INTERNATIONAL
	if (BLF_translate_tooltips()) {
		return BLF_pgettext(msgctxt, msgid);
	}
	else {
		return msgid;
	}
#else
	(void)msgctxt;
	return msgid;
#endif
}
