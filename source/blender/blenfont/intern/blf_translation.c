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

#ifdef WITH_INTERNATIONAL
#include <libintl.h>
#include <locale.h>

#define GETTEXT_CONTEXT_GLUE "\004"

/* needed for windows version of gettext */
#ifndef LC_MESSAGES
#	define LC_MESSAGES 1729
#endif

#endif

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_path_util.h"
#include "BLI_fileops.h"

#include "BLF_translation.h"

#include "DNA_userdef_types.h" /* For user settings. */

#ifdef WITH_INTERNATIONAL
static const char unifont_filename[] ="droidsans.ttf.gz";
static unsigned char *unifont_ttf = NULL;
static int unifont_size = 0;

unsigned char *BLF_get_unifont(int *unifont_size_r)
{
	if (unifont_ttf == NULL) {
		char *fontpath = BLI_get_folder(BLENDER_DATAFILES, "fonts");
		if (fontpath) {
			char unifont_path[1024];

			BLI_snprintf(unifont_path, sizeof(unifont_path), "%s/%s", fontpath, unifont_filename);

			unifont_ttf = (unsigned char*)BLI_file_ungzip_to_mem(unifont_path, &unifont_size);
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

const char* BLF_gettext(const char *msgid)
{
#ifdef WITH_INTERNATIONAL
	if (msgid[0])
		return gettext(msgid);
	return "";
#else
	return msgid;
#endif
}

const char *BLF_pgettext(const char *context, const char *message)
{
#ifdef WITH_INTERNATIONAL
	char static_msg_ctxt_id[1024];
	char *dynamic_msg_ctxt_id = NULL;
	char *msg_ctxt_id;
	const char *translation;

	size_t overall_length = strlen(context) + strlen(message) + sizeof(GETTEXT_CONTEXT_GLUE) + 1;

	if (overall_length > sizeof(static_msg_ctxt_id)) {
		dynamic_msg_ctxt_id = malloc(overall_length);
		msg_ctxt_id = dynamic_msg_ctxt_id;
	}
	else {
		msg_ctxt_id = static_msg_ctxt_id;
	}

	sprintf(msg_ctxt_id, "%s%s%s", context, GETTEXT_CONTEXT_GLUE, message);

	translation = (char*)dcgettext(TEXT_DOMAIN_NAME, msg_ctxt_id, LC_MESSAGES);

	if (dynamic_msg_ctxt_id)
		free(dynamic_msg_ctxt_id);

	if (translation == msg_ctxt_id)
		translation = message;

	return translation;
#else
	(void)context;
	return message;
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

const char *BLF_translate_do_iface(const char *msgid)
{
#ifdef WITH_INTERNATIONAL
	if (BLF_translate_iface())
		return BLF_gettext(msgid);
	else
		return msgid;
#else
	return msgid;
#endif
}

const char *BLF_translate_do_tooltip(const char *msgid)
{
#ifdef WITH_INTERNATIONAL
	if (BLF_translate_tooltips())
		return BLF_gettext(msgid);
	else
		return msgid;
#else
	return msgid;
#endif
}
