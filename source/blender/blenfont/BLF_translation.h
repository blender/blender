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
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenfont/BLF_translation.h
 *  \ingroup blf
 */


#ifndef BLF_TRANSLATION_H
#define BLF_TRANSLATION_H

#define TEXT_DOMAIN_NAME "blender"

/* blf_translation.c  */

#ifdef WITH_INTERNATIONAL
unsigned char *BLF_get_unifont(int *unifont_size);
void BLF_free_unifont(void);
#endif

const char *BLF_gettext(const char *msgid);
const char *BLF_pgettext(const char *context, const char *message);

/* blf_lang.c */

/*
 * Search the path directory to the locale files, this try all
 * the case for Linux, Win and Mac.
 */
void BLF_lang_init(void);

/* Set the current locale. */
void BLF_lang_set(const char *);

/* Set the current encoding name. */
void BLF_lang_encoding_name(const char *str);

void BLF_lang_encoding(const char *str);

/* translation */
int BLF_translate_iface(void);
int BLF_translate_tooltips(void);
const char *BLF_translate_do_iface(const char *msgid);
const char *BLF_translate_do_tooltip(const char *msgid);


/*#define _(msgid) BLF_gettext(msgid)*/
/* The "translation-marker" macro. */
#define N_(msgid) msgid
/* Those macros should be used everywhere in UI code. */
#define IFACE_(msgid) BLF_translate_do_iface(msgid)
#define TIP_(msgid) BLF_translate_do_tooltip(msgid)

#endif /* BLF_TRANSLATION_H */
