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

#include "BLI_utildefines.h"  /* for bool type */

#define TEXT_DOMAIN_NAME "blender"

#ifdef __cplusplus
extern "C" {
#endif

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
/* Get the current locale ([partial] ISO code, e.g. es_ES). */
const char *BLF_lang_get(void);

/* Get locale's elements (if relevant pointer is not NULL and element actually exists, e.g. if there is no variant,
 * *variant and *language_variant will always be NULL).
 * Non-null elements are always MEM_mallocN'ed, it's the caller's responsibility to free them.
 * NOTE: Always available, even in non-WITH_INTERNATIONAL builds.
 */
void BLF_locale_explode(const char *locale, char **language, char **country, char **variant,
                        char **language_country, char **language_variant);

/* Get EnumPropertyItem's for translations menu. */
struct EnumPropertyItem *BLF_RNA_lang_enum_properties(void);

/* blf_translation.c  */

unsigned char *BLF_get_unifont(int *unifont_size);
void BLF_free_unifont(void);
unsigned char *BLF_get_unifont_mono(int *unifont_size);
void BLF_free_unifont_mono(void);

bool BLF_is_default_context(const char *msgctxt);
const char *BLF_pgettext(const char *msgctxt, const char *msgid);

/* translation */
bool BLF_translate_iface(void);
bool BLF_translate_tooltips(void);
bool BLF_translate_new_dataname(void);
const char *BLF_translate_do_iface(const char *msgctxt, const char *msgid);
const char *BLF_translate_do_tooltip(const char *msgctxt, const char *msgid);
const char *BLF_translate_do_new_dataname(const char *msgctxt, const char *msgid);


/* The "translation-marker" macro. */
#define N_(msgid) msgid
#define CTX_N_(context, msgid) msgid

/* Those macros should be used everywhere in UI code. */
#ifdef WITH_INTERNATIONAL
/*#  define _(msgid) BLF_gettext(msgid) */
#  define IFACE_(msgid) BLF_translate_do_iface(NULL, msgid)
#  define TIP_(msgid) BLF_translate_do_tooltip(NULL, msgid)
#  define DATA_(msgid) BLF_translate_do_new_dataname(NULL, msgid)
#  define CTX_IFACE_(context, msgid) BLF_translate_do_iface(context, msgid)
#  define CTX_TIP_(context, msgid) BLF_translate_do_tooltip(context, msgid)
#  define CTX_DATA_(context, msgid) BLF_translate_do_new_dataname(context, msgid)
#else
/*#  define _(msgid) msgid */
#  define IFACE_(msgid) msgid
#  define TIP_(msgid)   msgid
#  define DATA_(msgid)  msgid
#  define CTX_IFACE_(context, msgid) msgid
#  define CTX_TIP_(context, msgid)   msgid
#  define CTX_DATA_(context, msgid)  msgid
#endif

/* Helper macro, when we want to define a same msgid for multiple msgctxt...
 * Does nothing in C, but is "parsed" by our i18n py tools.
 * XXX Currently limited to at most 16 contexts at once
 *     (but you can call it several times with the same msgid, should you need more contexts!).
 */
#define BLF_I18N_MSGID_MULTI_CTXT(msgid, ...)

/******************************************************************************
 * All i18n contexts must be defined here.
 * This is a nice way to be sure not to use a context twice for different
 * things, and limit the number of existing contexts!
 * WARNING! Contexts should not be longer than BKE_ST_MAXNAME - 1!
 */

/* Default, void context.
 * WARNING! The "" context is not the same as no (NULL) context at mo/boost::locale level!
 * NOTE: We translate BLF_I18NCONTEXT_DEFAULT as BLF_I18NCONTEXT_DEFAULT_BPY in Python, as we can't use "natural"
 *       None value in rna string properties... :/
 *       The void string "" is also interpreted as BLF_I18NCONTEXT_DEFAULT.
 *       For perf reason, we only use the first char to detect this context, so other contexts should never start
 *       with the same char!
 */
#define BLF_I18NCONTEXT_DEFAULT NULL
#define BLF_I18NCONTEXT_DEFAULT_BPYRNA "*"

/* Default context for operator names/labels. */
#define BLF_I18NCONTEXT_OPERATOR_DEFAULT "Operator"

/* Mark the msgid applies to several elements (needed in some cases, as english adjectives have no plural mark. :( */
#define BLF_I18NCONTEXT_PLURAL "Plural"

/* ID-types contexts. */
/* WARNING! Keep it in sync with idtypes in blenkernel/intern/idcode.c */
#define BLF_I18NCONTEXT_ID_ACTION               "Action"
#define BLF_I18NCONTEXT_ID_ARMATURE             "Armature"
#define BLF_I18NCONTEXT_ID_BRUSH                "Brush"
#define BLF_I18NCONTEXT_ID_CAMERA               "Camera"
#define BLF_I18NCONTEXT_ID_CURVE                "Curve"
#define BLF_I18NCONTEXT_ID_FREESTYLELINESTYLE   "FreestyleLineStyle"
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
#define BLF_I18NCONTEXT_ID_PAINTCURVE           "PaintCurve"
#define BLF_I18NCONTEXT_ID_PALETTE              "Palette"
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

/* Helper for bpy.app.i18n object... */
typedef struct
{
	const char *c_id;
	const char *py_id;
	const char *value;
} BLF_i18n_contexts_descriptor;

#define BLF_I18NCONTEXTS_ITEM(ctxt_id, py_id) {#ctxt_id, py_id, ctxt_id}

#define BLF_I18NCONTEXTS_DESC {                                                                                        \
	BLF_I18NCONTEXTS_ITEM(BLF_I18NCONTEXT_DEFAULT, "default_real"),                                                    \
	BLF_I18NCONTEXTS_ITEM(BLF_I18NCONTEXT_DEFAULT_BPYRNA, "default"),                                                  \
	BLF_I18NCONTEXTS_ITEM(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "operator_default"),                                       \
	BLF_I18NCONTEXTS_ITEM(BLF_I18NCONTEXT_PLURAL, "plural"),                                                           \
	BLF_I18NCONTEXTS_ITEM(BLF_I18NCONTEXT_ID_ACTION, "id_action"),                                                     \
	BLF_I18NCONTEXTS_ITEM(BLF_I18NCONTEXT_ID_ARMATURE, "id_armature"),                                                 \
	BLF_I18NCONTEXTS_ITEM(BLF_I18NCONTEXT_ID_BRUSH, "id_brush"),                                                       \
	BLF_I18NCONTEXTS_ITEM(BLF_I18NCONTEXT_ID_CAMERA, "id_camera"),                                                     \
	BLF_I18NCONTEXTS_ITEM(BLF_I18NCONTEXT_ID_CURVE, "id_curve"),                                                       \
	BLF_I18NCONTEXTS_ITEM(BLF_I18NCONTEXT_ID_FREESTYLELINESTYLE, "id_fs_linestyle"),                                   \
	BLF_I18NCONTEXTS_ITEM(BLF_I18NCONTEXT_ID_GPENCIL, "id_gpencil"),                                                   \
	BLF_I18NCONTEXTS_ITEM(BLF_I18NCONTEXT_ID_GROUP, "id_group"),                                                       \
	BLF_I18NCONTEXTS_ITEM(BLF_I18NCONTEXT_ID_ID, "id_id"),                                                             \
	BLF_I18NCONTEXTS_ITEM(BLF_I18NCONTEXT_ID_IMAGE, "id_image"),                                                       \
	/*BLF_I18NCONTEXTS_ITEM(BLF_I18NCONTEXT_ID_IPO, "id_ipo"),*/                                                       \
	BLF_I18NCONTEXTS_ITEM(BLF_I18NCONTEXT_ID_SHAPEKEY, "id_shapekey"),                                                 \
	BLF_I18NCONTEXTS_ITEM(BLF_I18NCONTEXT_ID_LAMP, "id_lamp"),                                                         \
	BLF_I18NCONTEXTS_ITEM(BLF_I18NCONTEXT_ID_LIBRARY, "id_library"),                                                   \
	BLF_I18NCONTEXTS_ITEM(BLF_I18NCONTEXT_ID_LATTICE, "id_lattice"),                                                   \
	BLF_I18NCONTEXTS_ITEM(BLF_I18NCONTEXT_ID_MASK, "id_mask"),                                                         \
	BLF_I18NCONTEXTS_ITEM(BLF_I18NCONTEXT_ID_MATERIAL, "id_material"),                                                 \
	BLF_I18NCONTEXTS_ITEM(BLF_I18NCONTEXT_ID_METABALL, "id_metaball"),                                                 \
	BLF_I18NCONTEXTS_ITEM(BLF_I18NCONTEXT_ID_MESH, "id_mesh"),                                                         \
	BLF_I18NCONTEXTS_ITEM(BLF_I18NCONTEXT_ID_MOVIECLIP, "id_movieclip"),                                               \
	BLF_I18NCONTEXTS_ITEM(BLF_I18NCONTEXT_ID_NODETREE, "id_nodetree"),                                                 \
	BLF_I18NCONTEXTS_ITEM(BLF_I18NCONTEXT_ID_OBJECT, "id_object"),                                                     \
	BLF_I18NCONTEXTS_ITEM(BLF_I18NCONTEXT_ID_PAINTCURVE, "id_paintcurve"),                                             \
	BLF_I18NCONTEXTS_ITEM(BLF_I18NCONTEXT_ID_PALETTE, "id_palette"),                                                   \
	BLF_I18NCONTEXTS_ITEM(BLF_I18NCONTEXT_ID_PARTICLESETTINGS, "id_particlesettings"),                                 \
	BLF_I18NCONTEXTS_ITEM(BLF_I18NCONTEXT_ID_SCENE, "id_scene"),                                                       \
	BLF_I18NCONTEXTS_ITEM(BLF_I18NCONTEXT_ID_SCREEN, "id_screen"),                                                     \
	BLF_I18NCONTEXTS_ITEM(BLF_I18NCONTEXT_ID_SEQUENCE, "id_sequence"),                                                 \
	BLF_I18NCONTEXTS_ITEM(BLF_I18NCONTEXT_ID_SPEAKER, "id_speaker"),                                                   \
	BLF_I18NCONTEXTS_ITEM(BLF_I18NCONTEXT_ID_SOUND, "id_sound"),                                                       \
	BLF_I18NCONTEXTS_ITEM(BLF_I18NCONTEXT_ID_TEXTURE, "id_texture"),                                                   \
	BLF_I18NCONTEXTS_ITEM(BLF_I18NCONTEXT_ID_TEXT, "id_text"),                                                         \
	BLF_I18NCONTEXTS_ITEM(BLF_I18NCONTEXT_ID_VFONT, "id_vfont"),                                                       \
	BLF_I18NCONTEXTS_ITEM(BLF_I18NCONTEXT_ID_WORLD, "id_world"),                                                       \
	BLF_I18NCONTEXTS_ITEM(BLF_I18NCONTEXT_ID_WINDOWMANAGER, "id_windowmanager"),                                       \
	{NULL, NULL, NULL}                                                                                                 \
}

#ifdef __cplusplus
};
#endif

#endif /* __BLF_TRANSLATION_H__ */
