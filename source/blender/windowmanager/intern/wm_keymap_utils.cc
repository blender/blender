/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Utilities to help define keymaps.
 */

#include <cstring>

#include "DNA_object_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"

#include "RNA_access.hh"

#include "WM_api.hh"
#include "WM_types.hh"

/* menu wrapper for WM_keymap_add_item */

/* -------------------------------------------------------------------- */
/** \name Wrappers for #WM_keymap_add_item
 * \{ */

wmKeyMapItem *WM_keymap_add_menu(wmKeyMap *keymap,
                                 const char *idname,
                                 const KeyMapItem_Params *params)
{
  wmKeyMapItem *kmi = WM_keymap_add_item(keymap, "WM_OT_call_menu", params);
  RNA_string_set(kmi->ptr, "name", idname);
  return kmi;
}

wmKeyMapItem *WM_keymap_add_menu_pie(wmKeyMap *keymap,
                                     const char *idname,
                                     const KeyMapItem_Params *params)
{
  wmKeyMapItem *kmi = WM_keymap_add_item(keymap, "WM_OT_call_menu_pie", params);
  RNA_string_set(kmi->ptr, "name", idname);
  return kmi;
}

wmKeyMapItem *WM_keymap_add_panel(wmKeyMap *keymap,
                                  const char *idname,
                                  const KeyMapItem_Params *params)
{
  wmKeyMapItem *kmi = WM_keymap_add_item(keymap, "WM_OT_call_panel", params);
  RNA_string_set(kmi->ptr, "name", idname);
  /* TODO: we might want to disable this. */
  RNA_boolean_set(kmi->ptr, "keep_open", false);
  return kmi;
}

wmKeyMapItem *WM_keymap_add_tool(wmKeyMap *keymap,
                                 const char *idname,
                                 const KeyMapItem_Params *params)
{
  wmKeyMapItem *kmi = WM_keymap_add_item(keymap, "WM_OT_tool_set_by_id", params);
  RNA_string_set(kmi->ptr, "name", idname);
  return kmi;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Introspection
 * \{ */

wmKeyMap *WM_keymap_guess_from_context(const bContext *C)
{
  SpaceLink *sl = CTX_wm_space_data(C);
  const char *km_id = nullptr;
  if (sl->spacetype == SPACE_VIEW3D) {
    const enum eContextObjectMode mode = CTX_data_mode_enum(C);
    switch (mode) {
      case CTX_MODE_EDIT_MESH:
        km_id = "Mesh";
        break;
      case CTX_MODE_EDIT_CURVE:
        km_id = "Curve";
        break;
      case CTX_MODE_EDIT_CURVES:
        km_id = "Curves";
        break;
      case CTX_MODE_EDIT_SURFACE:
        km_id = "Curve";
        break;
      case CTX_MODE_EDIT_TEXT:
        km_id = "Font";
        break;
      case CTX_MODE_EDIT_ARMATURE:
        km_id = "Armature";
        break;
      case CTX_MODE_EDIT_METABALL:
        km_id = "Metaball";
        break;
      case CTX_MODE_EDIT_LATTICE:
        km_id = "Lattice";
        break;
      case CTX_MODE_EDIT_GREASE_PENCIL:
        km_id = "Grease Pencil Edit Mode";
        break;
      case CTX_MODE_EDIT_POINT_CLOUD:
        km_id = "Point Cloud Edit Mode";
        break;
      case CTX_MODE_POSE:
        km_id = "Pose";
        break;
      case CTX_MODE_SCULPT:
        km_id = "Sculpt";
        break;
      case CTX_MODE_PAINT_WEIGHT:
        km_id = "Weight Paint";
        break;
      case CTX_MODE_PAINT_VERTEX:
        km_id = "Vertex Paint";
        break;
      case CTX_MODE_PAINT_TEXTURE:
        km_id = "Image Paint";
        break;
      case CTX_MODE_PARTICLE:
        km_id = "Particle";
        break;
      case CTX_MODE_OBJECT:
        km_id = "Object Mode";
        break;
      case CTX_MODE_PAINT_GPENCIL_LEGACY:
        km_id = "Grease Pencil Stroke Paint Mode";
        break;
      case CTX_MODE_EDIT_GPENCIL_LEGACY:
        km_id = "Grease Pencil Stroke Edit Mode";
        break;
      case CTX_MODE_SCULPT_GPENCIL_LEGACY:
        km_id = "Grease Pencil Stroke Sculpt Mode";
        break;
      case CTX_MODE_WEIGHT_GPENCIL_LEGACY:
        km_id = "Grease Pencil Stroke Weight Mode";
        break;
      case CTX_MODE_VERTEX_GPENCIL_LEGACY:
        km_id = "Grease Pencil Stroke Vertex Mode";
        break;
      case CTX_MODE_SCULPT_CURVES:
        km_id = "Sculpt Curves";
        break;
      case CTX_MODE_PAINT_GREASE_PENCIL:
        km_id = "Grease Pencil Paint Mode";
        break;
    }
  }
  else if (sl->spacetype == SPACE_IMAGE) {
    const SpaceImage *sima = (SpaceImage *)sl;
    const eSpaceImage_Mode mode = eSpaceImage_Mode(sima->mode);
    switch (mode) {
      case SI_MODE_VIEW:
        km_id = "Image";
        break;
      case SI_MODE_PAINT:
        km_id = "Image Paint";
        break;
      case SI_MODE_MASK:
        km_id = "Mask Editing";
        break;
      case SI_MODE_UV:
        km_id = "UV Editor";
        break;
    }
  }
  else {
    return nullptr;
  }

  wmKeyMap *km = WM_keymap_find_all(CTX_wm_manager(C), km_id, SPACE_EMPTY, RGN_TYPE_WINDOW);
  BLI_assert(km);
  return km;
}

wmKeyMap *WM_keymap_guess_opname(const bContext *C, const char *opname)
{
  /* Op types purposely skipped for now:
   *     BRUSH_OT
   *     BOID_OT
   *     BUTTONS_OT
   *     CONSTRAINT_OT
   *     PAINT_OT
   *     ED_OT
   *     FLUID_OT
   *     TEXTURE_OT
   *     WORLD_OT
   */

  wmKeyMap *km = nullptr;
  wmWindowManager *wm = CTX_wm_manager(C);
  SpaceLink *sl = CTX_wm_space_data(C);

  /* Window */
  if (STRPREFIX(opname, "WM_OT") || STRPREFIX(opname, "ED_OT_undo")) {
    if (STREQ(opname, "WM_OT_tool_set_by_id")) {
      km = WM_keymap_guess_from_context(C);
    }

    if (km == nullptr) {
      km = WM_keymap_find_all(wm, "Window", SPACE_EMPTY, RGN_TYPE_WINDOW);
    }
  }
  /* Screen & Render */
  else if (STRPREFIX(opname, "SCREEN_OT") || STRPREFIX(opname, "RENDER_OT") ||
           STRPREFIX(opname, "SOUND_OT") || STRPREFIX(opname, "SCENE_OT"))
  {
    km = WM_keymap_find_all(wm, "Screen", SPACE_EMPTY, RGN_TYPE_WINDOW);
  }
  /* Grease Pencil */
  else if (STRPREFIX(opname, "GPENCIL_OT")) {
    km = WM_keymap_find_all(wm, "Grease Pencil", SPACE_EMPTY, RGN_TYPE_WINDOW);
  }
  else if (STRPREFIX(opname, "GREASE_PENCIL_OT")) {
    km = WM_keymap_find_all(wm, "Grease Pencil", SPACE_EMPTY, RGN_TYPE_WINDOW);
  }
  /* Markers */
  else if (STRPREFIX(opname, "MARKER_OT")) {
    km = WM_keymap_find_all(wm, "Markers", SPACE_EMPTY, RGN_TYPE_WINDOW);
  }
  /* Import/Export */
  else if (STRPREFIX(opname, "IMPORT_") || STRPREFIX(opname, "EXPORT_")) {
    km = WM_keymap_find_all(wm, "Window", SPACE_EMPTY, RGN_TYPE_WINDOW);
  }

  /* 3D View */
  else if (STRPREFIX(opname, "VIEW3D_OT")) {
    km = WM_keymap_find_all(wm, "3D View", sl->spacetype, RGN_TYPE_WINDOW);
  }
  else if (STRPREFIX(opname, "OBJECT_OT")) {
    /* exception, this needs to work outside object mode too */
    if (STRPREFIX(opname, "OBJECT_OT_mode_set")) {
      km = WM_keymap_find_all(wm, "Object Non-modal", SPACE_EMPTY, RGN_TYPE_WINDOW);
    }
    else {
      km = WM_keymap_find_all(wm, "Object Mode", SPACE_EMPTY, RGN_TYPE_WINDOW);
    }
  }
  /* Object mode related */
  else if (STRPREFIX(opname, "GROUP_OT") || STRPREFIX(opname, "MATERIAL_OT") ||
           STRPREFIX(opname, "PTCACHE_OT") || STRPREFIX(opname, "RIGIDBODY_OT"))
  {
    km = WM_keymap_find_all(wm, "Object Mode", SPACE_EMPTY, RGN_TYPE_WINDOW);
  }

  /* Editing Modes */
  else if (STRPREFIX(opname, "MESH_OT")) {
    km = WM_keymap_find_all(wm, "Mesh", SPACE_EMPTY, RGN_TYPE_WINDOW);

    /* some mesh operators are active in object mode too, like add-prim */
    if (km && !WM_keymap_poll((bContext *)C, km)) {
      km = WM_keymap_find_all(wm, "Object Mode", SPACE_EMPTY, RGN_TYPE_WINDOW);
    }
  }
  else if (STRPREFIX(opname, "CURVE_OT") || STRPREFIX(opname, "SURFACE_OT")) {
    km = WM_keymap_find_all(wm, "Curve", SPACE_EMPTY, RGN_TYPE_WINDOW);

    /* some curve operators are active in object mode too, like add-prim */
    if (km && !WM_keymap_poll((bContext *)C, km)) {
      km = WM_keymap_find_all(wm, "Object Mode", SPACE_EMPTY, RGN_TYPE_WINDOW);
    }
  }
  else if (STRPREFIX(opname, "ARMATURE_OT") || STRPREFIX(opname, "SKETCH_OT")) {
    km = WM_keymap_find_all(wm, "Armature", SPACE_EMPTY, RGN_TYPE_WINDOW);
  }
  else if (STRPREFIX(opname, "POSE_OT") || STRPREFIX(opname, "POSELIB_OT")) {
    km = WM_keymap_find_all(wm, "Pose", SPACE_EMPTY, RGN_TYPE_WINDOW);
  }
  else if (STRPREFIX(opname, "SCULPT_OT")) {
    switch (CTX_data_mode_enum(C)) {
      case CTX_MODE_SCULPT:
        km = WM_keymap_find_all(wm, "Sculpt", SPACE_EMPTY, RGN_TYPE_WINDOW);
        break;
      default:
        break;
    }
  }
  else if (STRPREFIX(opname, "CURVES_OT")) {
    km = WM_keymap_find_all(wm, "Curves", SPACE_EMPTY, RGN_TYPE_WINDOW);
  }
  else if (STRPREFIX(opname, "SCULPT_CURVES_OT")) {
    km = WM_keymap_find_all(wm, "Sculpt Curves", SPACE_EMPTY, RGN_TYPE_WINDOW);
  }
  else if (STRPREFIX(opname, "MBALL_OT")) {
    km = WM_keymap_find_all(wm, "Metaball", SPACE_EMPTY, RGN_TYPE_WINDOW);

    /* Some meta-ball operators are active in object mode too, like add-primitive. */
    if (km && !WM_keymap_poll((bContext *)C, km)) {
      km = WM_keymap_find_all(wm, "Object Mode", SPACE_EMPTY, RGN_TYPE_WINDOW);
    }
  }
  else if (STRPREFIX(opname, "LATTICE_OT")) {
    km = WM_keymap_find_all(wm, "Lattice", SPACE_EMPTY, RGN_TYPE_WINDOW);
  }
  else if (STRPREFIX(opname, "PARTICLE_OT")) {
    km = WM_keymap_find_all(wm, "Particle", SPACE_EMPTY, RGN_TYPE_WINDOW);
  }
  else if (STRPREFIX(opname, "FONT_OT")) {
    km = WM_keymap_find_all(wm, "Font", SPACE_EMPTY, RGN_TYPE_WINDOW);
  }
  /* Paint Face Mask */
  else if (STRPREFIX(opname, "PAINT_OT_face_select")) {
    km = WM_keymap_find_all(
        wm, "Paint Face Mask (Weight, Vertex, Texture)", SPACE_EMPTY, RGN_TYPE_WINDOW);
  }
  else if (STRPREFIX(opname, "PAINT_OT")) {
    /* check for relevant mode */
    switch (CTX_data_mode_enum(C)) {
      case CTX_MODE_PAINT_WEIGHT:
        km = WM_keymap_find_all(wm, "Weight Paint", SPACE_EMPTY, RGN_TYPE_WINDOW);
        break;
      case CTX_MODE_PAINT_VERTEX:
        km = WM_keymap_find_all(wm, "Vertex Paint", SPACE_EMPTY, RGN_TYPE_WINDOW);
        break;
      case CTX_MODE_PAINT_TEXTURE:
        km = WM_keymap_find_all(wm, "Image Paint", SPACE_EMPTY, RGN_TYPE_WINDOW);
        break;
      case CTX_MODE_SCULPT:
        km = WM_keymap_find_all(wm, "Sculpt", SPACE_EMPTY, RGN_TYPE_WINDOW);
        break;
      default:
        break;
    }
  }
  /* General 2D View, not bound to a specific spacetype. */
  else if (STRPREFIX(opname, "VIEW2D_OT")) {
    km = WM_keymap_find_all(wm, "View2D", SPACE_EMPTY, RGN_TYPE_WINDOW);
  }
  /* Image Editor */
  else if (STRPREFIX(opname, "IMAGE_OT")) {
    km = WM_keymap_find_all(wm, "Image", sl->spacetype, RGN_TYPE_WINDOW);
  }
  /* Clip Editor */
  else if (STRPREFIX(opname, "CLIP_OT")) {
    km = WM_keymap_find_all(wm, "Clip", sl->spacetype, RGN_TYPE_WINDOW);
  }
  else if (STRPREFIX(opname, "MASK_OT")) {
    km = WM_keymap_find_all(wm, "Mask Editing", SPACE_EMPTY, RGN_TYPE_WINDOW);
  }
  /* UV Editor */
  else if (STRPREFIX(opname, "UV_OT")) {
    /* Hack to allow using UV unwrapping ops from 3DView/editmode.
     * Mesh keymap is probably not ideal, but best place I could find to put those. */
    if (sl->spacetype == SPACE_VIEW3D) {
      km = WM_keymap_find_all(wm, "Mesh", SPACE_EMPTY, RGN_TYPE_WINDOW);
      if (km && !WM_keymap_poll((bContext *)C, km)) {
        km = nullptr;
      }
    }
    if (!km) {
      km = WM_keymap_find_all(wm, "UV Editor", SPACE_EMPTY, RGN_TYPE_WINDOW);
    }
  }
  /* Node Editor */
  else if (STRPREFIX(opname, "NODE_OT")) {
    km = WM_keymap_find_all(wm, "Node Editor", sl->spacetype, RGN_TYPE_WINDOW);
  }
  /* Animation Editor Channels */
  else if (STRPREFIX(opname, "ANIM_OT_channels")) {
    km = WM_keymap_find_all(wm, "Animation Channels", SPACE_EMPTY, RGN_TYPE_WINDOW);
  }
  /* Animation Generic - after channels */
  else if (STRPREFIX(opname, "ANIM_OT")) {
    if (sl->spacetype == SPACE_VIEW3D) {
      switch (CTX_data_mode_enum(C)) {
        case CTX_MODE_OBJECT:
          km = WM_keymap_find_all(wm, "Object Mode", SPACE_EMPTY, RGN_TYPE_WINDOW);
          break;
        case CTX_MODE_POSE:
          km = WM_keymap_find_all(wm, "Pose", SPACE_EMPTY, RGN_TYPE_WINDOW);
          break;
        default:
          break;
      }
      if (km && !WM_keymap_poll((bContext *)C, km)) {
        km = nullptr;
      }
    }

    if (!km) {
      km = WM_keymap_find_all(wm, "Animation", SPACE_EMPTY, RGN_TYPE_WINDOW);
    }
  }
  /* Graph Editor */
  else if (STRPREFIX(opname, "GRAPH_OT")) {
    km = WM_keymap_find_all(wm, "Graph Editor", sl->spacetype, RGN_TYPE_WINDOW);
  }
  /* Dopesheet Editor */
  else if (STRPREFIX(opname, "ACTION_OT")) {
    km = WM_keymap_find_all(wm, "Dopesheet", sl->spacetype, RGN_TYPE_WINDOW);
  }
  /* NLA Editor */
  else if (STRPREFIX(opname, "NLA_OT")) {
    km = WM_keymap_find_all(wm, "NLA Editor", sl->spacetype, RGN_TYPE_WINDOW);
  }
  /* Script */
  else if (STRPREFIX(opname, "SCRIPT_OT")) {
    km = WM_keymap_find_all(wm, "Script", sl->spacetype, RGN_TYPE_WINDOW);
  }
  /* Text */
  else if (STRPREFIX(opname, "TEXT_OT")) {
    km = WM_keymap_find_all(wm, "Text", sl->spacetype, RGN_TYPE_WINDOW);
  }
  /* Sequencer */
  else if (STRPREFIX(opname, "SEQUENCER_OT")) {
    km = WM_keymap_find_all(wm, "Sequencer", sl->spacetype, RGN_TYPE_WINDOW);
  }
  /* Console */
  else if (STRPREFIX(opname, "CONSOLE_OT")) {
    km = WM_keymap_find_all(wm, "Console", sl->spacetype, RGN_TYPE_WINDOW);
  }
  /* Console */
  else if (STRPREFIX(opname, "INFO_OT")) {
    km = WM_keymap_find_all(wm, "Info", sl->spacetype, RGN_TYPE_WINDOW);
  }
  /* File browser */
  else if (STRPREFIX(opname, "FILE_OT")) {
    km = WM_keymap_find_all(wm, "File Browser", sl->spacetype, RGN_TYPE_WINDOW);
  }
  /* Asset browser */
  else if (STRPREFIX(opname, "ASSET_OT")) {
    km = WM_keymap_find_all(wm, "Asset Browser", sl->spacetype, 0);
  }
  /* Logic Editor */
  else if (STRPREFIX(opname, "LOGIC_OT")) {
    km = WM_keymap_find_all(wm, "Logic Editor", sl->spacetype, RGN_TYPE_WINDOW);
  }
  /* Outliner */
  else if (STRPREFIX(opname, "OUTLINER_OT")) {
    km = WM_keymap_find_all(wm, "Outliner", sl->spacetype, RGN_TYPE_WINDOW);
  }
  /* Transform */
  else if (STRPREFIX(opname, "TRANSFORM_OT")) {
    /* check for relevant editor */
    switch (sl->spacetype) {
      case SPACE_VIEW3D:
        km = WM_keymap_find_all(wm, "3D View", sl->spacetype, RGN_TYPE_WINDOW);
        break;
      case SPACE_GRAPH:
        km = WM_keymap_find_all(wm, "Graph Editor", sl->spacetype, RGN_TYPE_WINDOW);
        break;
      case SPACE_ACTION:
        km = WM_keymap_find_all(wm, "Dopesheet", sl->spacetype, RGN_TYPE_WINDOW);
        break;
      case SPACE_NLA:
        km = WM_keymap_find_all(wm, "NLA Editor", sl->spacetype, RGN_TYPE_WINDOW);
        break;
      case SPACE_IMAGE:
        km = WM_keymap_find_all(wm, "UV Editor", SPACE_EMPTY, RGN_TYPE_WINDOW);
        break;
      case SPACE_NODE:
        km = WM_keymap_find_all(wm, "Node Editor", sl->spacetype, RGN_TYPE_WINDOW);
        break;
      case SPACE_SEQ:
        km = WM_keymap_find_all(wm, "Sequencer", sl->spacetype, RGN_TYPE_WINDOW);
        break;
    }
  }
  /* User Interface */
  else if (STRPREFIX(opname, "UI_OT")) {
    km = WM_keymap_find_all(wm, "User Interface", SPACE_EMPTY, RGN_TYPE_WINDOW);
  }

  return km;
}

static bool wm_keymap_item_uses_modifier(const wmKeyMapItem *kmi, const int event_modifier)
{
  if (kmi->ctrl != KM_ANY) {
    if ((kmi->ctrl == KM_NOTHING) != ((event_modifier & KM_CTRL) == 0)) {
      return false;
    }
  }

  if (kmi->alt != KM_ANY) {
    if ((kmi->alt == KM_NOTHING) != ((event_modifier & KM_ALT) == 0)) {
      return false;
    }
  }

  if (kmi->shift != KM_ANY) {
    if ((kmi->shift == KM_NOTHING) != ((event_modifier & KM_SHIFT) == 0)) {
      return false;
    }
  }

  if (kmi->oskey != KM_ANY) {
    if ((kmi->oskey == KM_NOTHING) != ((event_modifier & KM_OSKEY) == 0)) {
      return false;
    }
  }
  return true;
}

bool WM_keymap_uses_event_modifier(const wmKeyMap *keymap, const int event_modifier)
{
  LISTBASE_FOREACH (const wmKeyMapItem *, kmi, &keymap->items) {
    if ((kmi->flag & KMI_INACTIVE) == 0) {
      if (wm_keymap_item_uses_modifier(kmi, event_modifier)) {
        return true;
      }
    }
  }
  return false;
}

void WM_keymap_fix_linking() {}

/** \} */
