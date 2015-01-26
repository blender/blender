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
 *
 * Manages translation files and provides translation functions.
 * (which are optional and can be disabled as a preference).
 */

#include <stdlib.h>
#include <string.h>

#include "BLF_translation.h"

#include "MEM_guardedalloc.h"

#include "BLI_fileops.h"
#include "BLI_path_util.h"
#include "BLI_string.h"

#include "BKE_appdir.h"

#include "DNA_userdef_types.h" /* For user settings. */

#include "BPY_extern.h"

#ifdef WITH_INTERNATIONAL

#include "boost_locale_wrapper.h"

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

bool BLF_is_default_context(const char *msgctxt)
{
	/* We use the "short" test, a more complete one could be:
	 * return (!msgctxt || !msgctxt[0] || STREQ(msgctxt, BLF_I18NCONTEXT_DEFAULT_BPYRNA))
	 */
	/* Note: trying without the void string check for now, it *should* not be necessary... */
	return (!msgctxt || msgctxt[0] == BLF_I18NCONTEXT_DEFAULT_BPYRNA[0]);
}

const char *BLF_pgettext(const char *msgctxt, const char *msgid)
{
#ifdef WITH_INTERNATIONAL
	const char *ret = msgid;

	if (msgid && msgid[0]) {
		if (BLF_is_default_context(msgctxt)) {
			msgctxt = BLF_I18NCONTEXT_DEFAULT;
		}
		ret = bl_locale_pgettext(msgctxt, msgid);
		/* We assume if the returned string is the same (memory level) as the msgid, no translation was found,
		 * and we can try py scripts' ones!
		 */
		if (ret == msgid) {
			ret = BPY_app_translations_py_pgettext(msgctxt, msgid);
		}
	}

	return ret;
#else
	(void)msgctxt;
	return msgid;
#endif
}

bool BLF_translate_iface(void)
{
#ifdef WITH_INTERNATIONAL
	return (U.transopts & USER_DOTRANSLATE) && (U.transopts & USER_TR_IFACE);
#else
	return false;
#endif
}

bool BLF_translate_tooltips(void)
{
#ifdef WITH_INTERNATIONAL
	return (U.transopts & USER_DOTRANSLATE) && (U.transopts & USER_TR_TOOLTIPS);
#else
	return false;
#endif
}

bool BLF_translate_new_dataname(void)
{
#ifdef WITH_INTERNATIONAL
	return (U.transopts & USER_DOTRANSLATE) && (U.transopts & USER_TR_NEWDATANAME);
#else
	return false;
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

const char *BLF_translate_do_new_dataname(const char *msgctxt, const char *msgid)
{
#ifdef WITH_INTERNATIONAL
	if (BLF_translate_new_dataname()) {
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
