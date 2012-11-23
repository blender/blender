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


#ifndef __BLF_TRANSLATION_H__
#define __BLF_TRANSLATION_H__

#define TEXT_DOMAIN_NAME "blender"

/* blf_lang.c */

/* Search the path directory to the locale files, this try all
 * the case for Linux, Win and Mac.
 * Also dynamically builds locales and locales' menu from "languages" text file.
 */
void BLF_lang_init(void);

/* Free languages and locales_menu arrays created by BLF_lang_init. */
void BLF_lang_free(void);

/* Set the current locale. */
void BLF_lang_set(const char *);
/* Get the current locale (short code, e.g. es_ES). */
const char *BLF_lang_get(void);

/* Get EnumPropertyItem's for translations menu. */
struct EnumPropertyItem *BLF_RNA_lang_enum_properties(void);

/* blf_translation.c  */

#ifdef WITH_INTERNATIONAL
unsigned char *BLF_get_unifont(int *unifont_size);
void BLF_free_unifont(void);
#endif

const char *BLF_pgettext(const char *msgctxt, const char *msgid);

/* translation */
int BLF_translate_iface(void);
int BLF_translate_tooltips(void);
const char *BLF_translate_do_iface(const char *msgctxt, const char *msgid);
const char *BLF_translate_do_tooltip(const char *msgctxt, const char *msgid);


/* The "translation-marker" macro. */
#define N_(msgid) msgid
#define CTX_N_(context, msgid) msgid

/* Those macros should be used everywhere in UI code. */
#ifdef WITH_INTERNATIONAL
/*	#define _(msgid) BLF_gettext(msgid) */
	#define IFACE_(msgid) BLF_translate_do_iface(NULL, msgid)
	#define TIP_(msgid) BLF_translate_do_tooltip(NULL, msgid)
	#define CTX_IFACE_(context, msgid) BLF_translate_do_iface(context, msgid)
	#define CTX_TIP_(context, msgid) BLF_translate_do_tooltip(context, msgid)
#else
/*	#define _(msgid) msgid */
	#define IFACE_(msgid) msgid
	#define TIP_(msgid) msgid
	#define CTX_IFACE_(context, msgid) msgid
	#define CTX_TIP_(context, msgid) msgid
#endif

/* Helper macro, when we want to define a same msgid for multiple msgctxt...
 * Does nothing in C, but is "parsed" by our i18n py tools.
 * XXX Currently limited to at most 16 contexts at most
 *     (but you can call it several times with the same msgid, should you need more contexts!).
 */
#define BLF_I18N_MSGID_MULTI_CTXT(msgid, ...)

/******************************************************************************
 * All i18n contexts must be defined here.
 * This is a nice way to be sure not to use a context twice for different
 * things, and limit the number of existing contexts!
 */

/* Default, void context. Just in case... */
#define BLF_I18NCONTEXT_DEFAULT ""

/* Default context for operator names/labels. */
#define BLF_I18NCONTEXT_OPERATOR_DEFAULT "Operator"

/* ID-types contexts. */
/* WARNING! Keep it in sync with idtypes in blenkernel/intern/idcode.c */
#define BLF_I18NCONTEXT_ID_ACTION               "Action"
#define BLF_I18NCONTEXT_ID_ARMATURE             "Armature"
#define BLF_I18NCONTEXT_ID_BRUSH                "Brush"
#define BLF_I18NCONTEXT_ID_CAMERA               "Camera"
#define BLF_I18NCONTEXT_ID_CURVE                "Curve"
#define BLF_I18NCONTEXT_ID_GPENCIL              "GPencil"
#define BLF_I18NCONTEXT_ID_GROUP                "Group"
#define BLF_I18NCONTEXT_ID_ID                   "ID"
#define BLF_I18NCONTEXT_ID_IMAGE                "Image"
/*#define BLF_I18NCONTEXT_ID_IPO                  "Ipo"*/ /* Deprecated */
#define BLF_I18NCONTEXT_ID_SHAPEKEY             "Key"
#define BLF_I18NCONTEXT_ID_LAMP                 "Lamp"
#define BLF_I18NCONTEXT_ID_LIBRARY              "Library"
#define BLF_I18NCONTEXT_ID_LATTICE              "Lattice"
#define BLF_I18NCONTEXT_ID_MATERIAL             "Material"
#define BLF_I18NCONTEXT_ID_METABALL             "Metaball"
#define BLF_I18NCONTEXT_ID_MESH                 "Mesh"
#define BLF_I18NCONTEXT_ID_NODETREE             "NodeTree"
#define BLF_I18NCONTEXT_ID_OBJECT               "Object"
#define BLF_I18NCONTEXT_ID_PARTICLESETTINGS     "ParticleSettings"
#define BLF_I18NCONTEXT_ID_SCENE                "Scene"
#define BLF_I18NCONTEXT_ID_SCREEN               "Screen"
#define BLF_I18NCONTEXT_ID_SEQUENCE             "Sequence"
#define BLF_I18NCONTEXT_ID_SPEAKER              "Speaker"
#define BLF_I18NCONTEXT_ID_SOUND                "Sound"
#define BLF_I18NCONTEXT_ID_TEXTURE              "Texture"
#define BLF_I18NCONTEXT_ID_TEXT                 "Text"
#define BLF_I18NCONTEXT_ID_VFONT                "VFont"
#define BLF_I18NCONTEXT_ID_WORLD                "World"
#define BLF_I18NCONTEXT_ID_WINDOWMANAGER        "WindowManager"
#define BLF_I18NCONTEXT_ID_MOVIECLIP            "MovieClip"
#define BLF_I18NCONTEXT_ID_MASK                 "Mask"

#endif /* __BLF_TRANSLATION_H__ */
