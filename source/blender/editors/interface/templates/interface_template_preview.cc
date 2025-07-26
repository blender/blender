/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include "BKE_context.hh"
#include "BKE_idtype.hh"
#include "BKE_linestyle.h"
#include "BKE_scene.hh"

#include "BLI_listbase.h"
#include "BLI_string_utf8.h"

#include "BLT_translation.hh"

#include "DNA_light_types.h"
#include "DNA_material_types.h"
#include "DNA_texture_types.h"
#include "DNA_world_types.h"

#include "ED_render.hh"

#include "RNA_access.hh"

#include "WM_api.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"

#define B_MATPRV 1

static void do_preview_buttons(bContext *C, void *arg, int event)
{
  switch (event) {
    case B_MATPRV:
      WM_event_add_notifier(C, NC_MATERIAL | ND_SHADING_PREVIEW, arg);
      break;
  }
}

void uiTemplatePreview(uiLayout *layout,
                       bContext *C,
                       ID *id,
                       bool show_buttons,
                       ID *parent,
                       MTex *slot,
                       const char *preview_id)
{
  Material *ma = nullptr;
  short *pr_texture = nullptr;

  char _preview_id[sizeof(uiPreview::preview_id)];

  if (id && !ELEM(GS(id->name), ID_MA, ID_TE, ID_WO, ID_LA, ID_LS)) {
    RNA_warning("Expected ID of type material, texture, light, world or line style");
    return;
  }

  /* decide what to render */
  ID *pid = id;
  ID *pparent = nullptr;

  if (id && (GS(id->name) == ID_TE)) {
    if (parent && (GS(parent->name) == ID_MA)) {
      pr_texture = &((Material *)parent)->pr_texture;
    }
    else if (parent && (GS(parent->name) == ID_WO)) {
      pr_texture = &((World *)parent)->pr_texture;
    }
    else if (parent && (GS(parent->name) == ID_LA)) {
      pr_texture = &((Light *)parent)->pr_texture;
    }
    else if (parent && (GS(parent->name) == ID_LS)) {
      pr_texture = &((FreestyleLineStyle *)parent)->pr_texture;
    }

    if (pr_texture) {
      if (*pr_texture == TEX_PR_OTHER) {
        pid = parent;
      }
      else if (*pr_texture == TEX_PR_BOTH) {
        pparent = parent;
      }
    }
  }

  if (!preview_id || (preview_id[0] == '\0')) {
    /* If no identifier given, generate one from ID type. */
    SNPRINTF_UTF8(_preview_id, "uiPreview_%s", BKE_idtype_idcode_to_name(GS(id->name)));
    preview_id = _preview_id;
  }

  /* Find or add the uiPreview to the current Region. */
  ARegion *region = CTX_wm_region(C);
  uiPreview *ui_preview = static_cast<uiPreview *>(
      BLI_findstring(&region->ui_previews, preview_id, offsetof(uiPreview, preview_id)));

  if (!ui_preview) {
    ui_preview = MEM_callocN<uiPreview>(__func__);
    STRNCPY_UTF8(ui_preview->preview_id, preview_id);
    ui_preview->height = short(UI_UNIT_Y * 7.6f);
    ui_preview->id_session_uid = pid->session_uid;
    ui_preview->tag = UI_PREVIEW_TAG_DIRTY;
    BLI_addtail(&region->ui_previews, ui_preview);
  }
  else if (ui_preview->id_session_uid != pid->session_uid) {
    ui_preview->id_session_uid = pid->session_uid;
    ui_preview->tag |= UI_PREVIEW_TAG_DIRTY;
  }

  if (ui_preview->height < UI_UNIT_Y) {
    ui_preview->height = UI_UNIT_Y;
  }
  else if (ui_preview->height > UI_UNIT_Y * 50) { /* Rather high upper limit, yet not insane! */
    ui_preview->height = UI_UNIT_Y * 50;
  }

  /* layout */
  uiBlock *block = layout->block();
  uiLayout *row = &layout->row(false);
  uiLayout *col = &row->column(false);

  /* add preview */
  uiDefBut(
      block, ButType::Extra, 0, "", 0, 0, UI_UNIT_X * 10, ui_preview->height, pid, 0.0, 0.0, "");
  UI_but_func_drawextra_set(block,
                            [pid, pparent, slot, ui_preview](const bContext *C, rcti *rect) {
                              ED_preview_draw(C, pid, pparent, slot, ui_preview, rect);
                            });
  UI_block_func_handle_set(block, do_preview_buttons, nullptr);

  uiDefIconButS(block,
                ButType::Grip,
                0,
                ICON_GRIP,
                0,
                0,
                UI_UNIT_X * 10,
                short(UI_UNIT_Y * 0.3f),
                &ui_preview->height,
                UI_UNIT_Y,
                UI_UNIT_Y * 50.0f,
                "");

  /* add buttons */
  if (pid && show_buttons) {
    if (GS(pid->name) == ID_MA || (pparent && GS(pparent->name) == ID_MA)) {
      if (GS(pid->name) == ID_MA) {
        ma = (Material *)pid;
      }
      else {
        ma = (Material *)pparent;
      }

      /* Create RNA Pointer */
      PointerRNA material_ptr = RNA_id_pointer_create(&ma->id);

      col = &row->column(true);
      col->scale_x_set(1.5);
      col->prop(&material_ptr, "preview_render_type", UI_ITEM_R_EXPAND, "", ICON_NONE);

      /* EEVEE preview file has baked lighting so use_preview_world has no effect,
       * just hide the option until this feature is supported. */
      if (!BKE_scene_uses_blender_eevee(CTX_data_scene(C))) {
        col->separator();
        col->prop(&material_ptr, "use_preview_world", UI_ITEM_NONE, "", ICON_WORLD);
      }
    }

    if (pr_texture) {
      /* Create RNA Pointer */
      PointerRNA texture_ptr = RNA_id_pointer_create(id);

      layout->row(true);
      uiDefButS(block,
                ButType::Row,
                B_MATPRV,
                IFACE_("Texture"),
                0,
                0,
                UI_UNIT_X * 10,
                UI_UNIT_Y,
                pr_texture,
                10,
                TEX_PR_TEXTURE,
                "");
      if (GS(parent->name) == ID_MA) {
        uiDefButS(block,
                  ButType::Row,
                  B_MATPRV,
                  IFACE_("Material"),
                  0,
                  0,
                  UI_UNIT_X * 10,
                  UI_UNIT_Y,
                  pr_texture,
                  10,
                  TEX_PR_OTHER,
                  "");
      }
      else if (GS(parent->name) == ID_LA) {
        uiDefButS(block,
                  ButType::Row,
                  B_MATPRV,
                  CTX_IFACE_(BLT_I18NCONTEXT_ID_LIGHT, "Light"),
                  0,
                  0,
                  UI_UNIT_X * 10,
                  UI_UNIT_Y,
                  pr_texture,
                  10,
                  TEX_PR_OTHER,
                  "");
      }
      else if (GS(parent->name) == ID_WO) {
        uiDefButS(block,
                  ButType::Row,
                  B_MATPRV,
                  CTX_IFACE_(BLT_I18NCONTEXT_ID_WORLD, "World"),
                  0,
                  0,
                  UI_UNIT_X * 10,
                  UI_UNIT_Y,
                  pr_texture,
                  10,
                  TEX_PR_OTHER,
                  "");
      }
      else if (GS(parent->name) == ID_LS) {
        uiDefButS(block,
                  ButType::Row,
                  B_MATPRV,
                  IFACE_("Line Style"),
                  0,
                  0,
                  UI_UNIT_X * 10,
                  UI_UNIT_Y,
                  pr_texture,
                  10,
                  TEX_PR_OTHER,
                  "");
      }
      uiDefButS(block,
                ButType::Row,
                B_MATPRV,
                IFACE_("Both"),
                0,
                0,
                UI_UNIT_X * 10,
                UI_UNIT_Y,
                pr_texture,
                10,
                TEX_PR_BOTH,
                "");

      /* Alpha button for texture preview */
      if (*pr_texture != TEX_PR_OTHER) {
        row = &layout->row(false);
        row->prop(&texture_ptr, "use_preview_alpha", UI_ITEM_NONE, std::nullopt, ICON_NONE);
      }
    }
  }
}
