/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blt
 */

#pragma once

#include "BLI_string_ref.hh"

#define TEXT_DOMAIN_NAME "blender"

bool BLT_is_default_context(blender::StringRef msgctxt);
const char *BLT_pgettext(const char *msgctxt, const char *msgid);
blender::StringRef BLT_pgettext(blender::StringRef msgctxt, blender::StringRef msgid);

/* Translation */
/* - iface includes buttons in the user interface: short labels displayed in windows, panels,
 * menus.
 * - tooltips only include the popup tooltips when hovering a button.
 * - report is for longer, additional information displayed in the UI, such as error messages.
 * - new_dataname is the actual user-created data such as objects, meshes, etc. */
bool BLT_translate();
bool BLT_translate_iface();
bool BLT_translate_tooltips();
bool BLT_translate_reports();
bool BLT_translate_new_dataname();
const char *BLT_translate_do(const char *msgctxt, const char *msgid);
blender::StringRef BLT_translate_do(blender::StringRef msgctxt, blender::StringRef msgid);
const char *BLT_translate_do_iface(const char *msgctxt, const char *msgid);
blender::StringRef BLT_translate_do_iface(blender::StringRef msgctxt, blender::StringRef msgid);
const char *BLT_translate_do_tooltip(const char *msgctxt, const char *msgid);
blender::StringRef BLT_translate_do_tooltip(blender::StringRef msgctxt, blender::StringRef msgid);
const char *BLT_translate_do_report(const char *msgctxt, const char *msgid);
blender::StringRef BLT_translate_do_report(blender::StringRef msgctxt, blender::StringRef msgid);
const char *BLT_translate_do_new_dataname(const char *msgctxt, const char *msgid);
blender::StringRef BLT_translate_do_new_dataname(blender::StringRef msgctxt,
                                                 blender::StringRef msgid);

/* The "translation-marker" macro. */
#define N_(msgid) msgid
#define CTX_N_(context, msgid) msgid

/* These macros should be used everywhere in UI code. */
/*#  define _(msgid) BLT_gettext(msgid) */
#define IFACE_(msgid) BLT_translate_do_iface(NULL, msgid)
#define TIP_(msgid) BLT_translate_do_tooltip(NULL, msgid)
#define RPT_(msgid) BLT_translate_do_report(NULL, msgid)
#define DATA_(msgid) BLT_translate_do_new_dataname(NULL, msgid)
#define CTX_IFACE_(context, msgid) BLT_translate_do_iface(context, msgid)
#define CTX_TIP_(context, msgid) BLT_translate_do_tooltip(context, msgid)
#define CTX_RPT_(context, msgid) BLT_translate_do_report(context, msgid)
#define CTX_DATA_(context, msgid) BLT_translate_do_new_dataname(context, msgid)

/* Helper macro, when we want to define a same msgid for multiple msgctxt...
 * Does nothing in C, but is "parsed" by our i18n py tools.
 * XXX Currently limited to at most 16 contexts at once
 *     (but you can call it several times with the same msgid, should you need more contexts!).
 */
#define BLT_I18N_MSGID_MULTI_CTXT(msgid, ...)

/******************************************************************************
 * All i18n contexts must be defined here.
 * This is a nice way to be sure not to use a context twice for different
 * things, and limit the number of existing contexts!
 * WARNING! Contexts should not be longer than BKE_ST_MAXNAME - 1!
 */

/* Default, void context.
 * WARNING! The "" context is not the same as no (NULL) context at mo/boost::locale level!
 * NOTE: We translate BLT_I18NCONTEXT_DEFAULT as BLT_I18NCONTEXT_DEFAULT_BPY in Python,
 *       as we can't use "natural" None value in rna string properties... :/
 *       The void string "" is also interpreted as BLT_I18NCONTEXT_DEFAULT.
 *       For performance reason, we only use the first char to detect this context,
 *       so other contexts should never start with the same char!
 */
#define BLT_I18NCONTEXT_DEFAULT NULL
#define BLT_I18NCONTEXT_DEFAULT_BPYRNA "*"

/* Default context for operator names/labels. */
#define BLT_I18NCONTEXT_OPERATOR_DEFAULT "Operator"

/* Context for events/keymaps (necessary, since those often use one or two letters,
 * easy to get collisions with other areas...). */
#define BLT_I18NCONTEXT_UI_EVENTS "UI_Events_KeyMaps"

/* Mark the msgid applies to several elements
 * (needed in some cases, as English adjectives have no plural mark :( ). */
#define BLT_I18NCONTEXT_PLURAL "Plural"

/* Some words can be either countable or uncountable in English, but translate to different words
 * in other languages. An exemple is "Amount", which can refer to "a number of things", countable,
 * or "a quantity or volume", uncountable. */
#define BLT_I18NCONTEXT_COUNTABLE "Countable"

/* Special cases when translation cannot be avoided, for example in an interface where some props
 * are built-in (translatable) and others are user-defined (non-translatable), but we don't know
 * which ones in advance.
 * It allows specifying explicitly that translation should not occur for user data when building
 * the UI. */
#define BLT_I18NCONTEXT_NO_TRANSLATION "Do not translate"

/* ID-types contexts. */
/* WARNING! Keep it in sync with ID-types in `blenkernel/intern/idtype.cc`. */
#define BLT_I18NCONTEXT_ID_ACTION "Action"
#define BLT_I18NCONTEXT_ID_ANIMATION "Animation"
#define BLT_I18NCONTEXT_ID_ARMATURE "Armature"
#define BLT_I18NCONTEXT_ID_BRUSH "Brush"
#define BLT_I18NCONTEXT_ID_CACHEFILE "CacheFile"
#define BLT_I18NCONTEXT_ID_CAMERA "Camera"
#define BLT_I18NCONTEXT_ID_COLLECTION "Collection"
#define BLT_I18NCONTEXT_ID_CURVES "Curves"
#define BLT_I18NCONTEXT_ID_CURVE_LEGACY "Curve"
#define BLT_I18NCONTEXT_ID_FREESTYLELINESTYLE "FreestyleLineStyle"
#define BLT_I18NCONTEXT_ID_GPENCIL "GPencil"
#define BLT_I18NCONTEXT_ID_ID "ID"
#define BLT_I18NCONTEXT_ID_IMAGE "Image"
// #define BLT_I18NCONTEXT_ID_IPO "Ipo" /* DEPRECATED */
#define BLT_I18NCONTEXT_ID_LATTICE "Lattice"
#define BLT_I18NCONTEXT_ID_LIBRARY "Library"
#define BLT_I18NCONTEXT_ID_LIGHT "Light"
#define BLT_I18NCONTEXT_ID_LIGHTPROBE "LightProbe"
#define BLT_I18NCONTEXT_ID_MASK "Mask"
#define BLT_I18NCONTEXT_ID_MATERIAL "Material"
#define BLT_I18NCONTEXT_ID_MESH "Mesh"
#define BLT_I18NCONTEXT_ID_METABALL "Metaball"
#define BLT_I18NCONTEXT_ID_MOVIECLIP "MovieClip"
#define BLT_I18NCONTEXT_ID_NODETREE "NodeTree"
#define BLT_I18NCONTEXT_ID_OBJECT "Object"
#define BLT_I18NCONTEXT_ID_PAINTCURVE "PaintCurve"
#define BLT_I18NCONTEXT_ID_PALETTE "Palette"
#define BLT_I18NCONTEXT_ID_PARTICLESETTINGS "ParticleSettings"
#define BLT_I18NCONTEXT_ID_POINTCLOUD "PointCloud"
#define BLT_I18NCONTEXT_ID_SCENE "Scene"
#define BLT_I18NCONTEXT_ID_SCREEN "Screen"
#define BLT_I18NCONTEXT_ID_SEQUENCE "Sequence"
#define BLT_I18NCONTEXT_ID_SHAPEKEY "Key"
#define BLT_I18NCONTEXT_ID_SIMULATION "Simulation"
#define BLT_I18NCONTEXT_ID_SOUND "Sound"
#define BLT_I18NCONTEXT_ID_SPEAKER "Speaker"
#define BLT_I18NCONTEXT_ID_TEXT "Text"
#define BLT_I18NCONTEXT_ID_TEXTURE "Texture"
#define BLT_I18NCONTEXT_ID_VFONT "VFont"
#define BLT_I18NCONTEXT_ID_VOLUME "Volume"
#define BLT_I18NCONTEXT_ID_WINDOWMANAGER "WindowManager"
#define BLT_I18NCONTEXT_ID_WORKSPACE "WorkSpace"
#define BLT_I18NCONTEXT_ID_WORLD "World"

/* Editors-types contexts. */
#define BLT_I18NCONTEXT_EDITOR_FILEBROWSER "File browser"
#define BLT_I18NCONTEXT_EDITOR_PREFERENCES "Preferences"
#define BLT_I18NCONTEXT_EDITOR_PYTHON_CONSOLE "Python console"
#define BLT_I18NCONTEXT_EDITOR_VIEW3D "View3D"

/* Generic contexts. */
#define BLT_I18NCONTEXT_AMOUNT "Amount"
#define BLT_I18NCONTEXT_COLOR "Color"
#define BLT_I18NCONTEXT_CONSTRAINT "Constraint"
#define BLT_I18NCONTEXT_MODIFIER "Modifier"
#define BLT_I18NCONTEXT_NAVIGATION "Navigation"
#define BLT_I18NCONTEXT_RENDER_LAYER "Render Layer"
#define BLT_I18NCONTEXT_TIME "Time"
#define BLT_I18NCONTEXT_UNIT "Unit"

/* Helper for bpy.app.i18n object... */
struct BLT_i18n_contexts_descriptor {
  const char *c_id;
  const char *py_id;
  const char *value;
};

#define BLT_I18NCONTEXTS_ITEM(ctxt_id, py_id) {#ctxt_id, py_id, ctxt_id}

#define BLT_I18NCONTEXTS_DESC \
  { \
    BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_DEFAULT, "default_real"), \
        BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_DEFAULT_BPYRNA, "default"), \
        BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "operator_default"), \
        BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_UI_EVENTS, "ui_events_keymaps"), \
        BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_PLURAL, "plural"), \
        BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_COUNTABLE, "countable"), \
        BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_ID_ACTION, "id_action"), \
        BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_ID_ARMATURE, "id_armature"), \
        BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_NO_TRANSLATION, "no_translation"), \
        BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_ID_BRUSH, "id_brush"), \
        BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_ID_CACHEFILE, "id_cachefile"), \
        BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_ID_CAMERA, "id_camera"), \
        BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_ID_COLLECTION, "id_collection"), \
        BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_ID_CURVES, "id_curves"), \
        BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_ID_CURVE_LEGACY, "id_curve"), \
        BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_ID_FREESTYLELINESTYLE, "id_fs_linestyle"), \
        BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_ID_GPENCIL, "id_gpencil"), \
        BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_ID_ID, "id_id"), \
        BLT_I18NCONTEXTS_ITEM( \
            BLT_I18NCONTEXT_ID_IMAGE, \
            "id_image"), /* BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_ID_IPO, "id_ipo"), */ \
        BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_ID_LATTICE, "id_lattice"), \
        BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_ID_LIBRARY, "id_library"), \
        BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_ID_LIGHT, "id_light"), \
        BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_ID_LIGHTPROBE, "id_lightprobe"), \
        BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_ID_MASK, "id_mask"), \
        BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_ID_MATERIAL, "id_material"), \
        BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_ID_MESH, "id_mesh"), \
        BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_ID_METABALL, "id_metaball"), \
        BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_ID_MOVIECLIP, "id_movieclip"), \
        BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_ID_NODETREE, "id_nodetree"), \
        BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_ID_OBJECT, "id_object"), \
        BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_ID_PAINTCURVE, "id_paintcurve"), \
        BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_ID_PALETTE, "id_palette"), \
        BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_ID_PARTICLESETTINGS, "id_particlesettings"), \
        BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_ID_POINTCLOUD, "id_pointcloud"), \
        BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_ID_SCENE, "id_scene"), \
        BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_ID_SCREEN, "id_screen"), \
        BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_ID_SEQUENCE, "id_sequence"), \
        BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_ID_SHAPEKEY, "id_shapekey"), \
        BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_ID_SIMULATION, "id_simulation"), \
        BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_ID_SOUND, "id_sound"), \
        BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_ID_SPEAKER, "id_speaker"), \
        BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_ID_TEXT, "id_text"), \
        BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_ID_TEXTURE, "id_texture"), \
        BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_ID_VFONT, "id_vfont"), \
        BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_ID_VOLUME, "id_volume"), \
        BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_ID_WINDOWMANAGER, "id_windowmanager"), \
        BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_ID_WORKSPACE, "id_workspace"), \
        BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_ID_WORLD, "id_world"), \
        BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_EDITOR_FILEBROWSER, "editor_filebrowser"), \
        BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_EDITOR_PYTHON_CONSOLE, "editor_python_console"), \
        BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_EDITOR_PREFERENCES, "editor_preferences"), \
        BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_EDITOR_VIEW3D, "editor_view3d"), \
        BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_AMOUNT, "amount"), \
        BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_COLOR, "color"), \
        BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_CONSTRAINT, "constraint"), \
        BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_MODIFIER, "modifier"), \
        BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_NAVIGATION, "navigation"), \
        BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_RENDER_LAYER, "render_layer"), \
        BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_TIME, "time"), \
        BLT_I18NCONTEXTS_ITEM(BLT_I18NCONTEXT_UNIT, "unit"), \
    { \
      NULL, NULL, NULL \
    } \
  }
