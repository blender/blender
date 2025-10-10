/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include "BKE_colorband.hh"
#include "BKE_context.hh"
#include "BKE_library.hh"

#include "BLI_listbase.h"
#include "BLI_rect.h"
#include "BLI_string_ref.hh"

#include "BLT_translation.hh"

#include "DNA_texture_types.h"

#include "ED_screen.hh"
#include "ED_undo.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "UI_interface_layout.hh"
#include "interface_intern.hh"
#include "interface_templates_intern.hh"

using blender::StringRefNull;

static void colorband_flip(bContext *C, ColorBand *coba)
{
  CBData data_tmp[MAXCOLORBAND];

  for (int a = 0; a < coba->tot; a++) {
    data_tmp[a] = coba->data[coba->tot - (a + 1)];
  }
  for (int a = 0; a < coba->tot; a++) {
    data_tmp[a].pos = 1.0f - data_tmp[a].pos;
    coba->data[a] = data_tmp[a];
  }

  /* May as well flip the `cur`. */
  coba->cur = coba->tot - (coba->cur + 1);

  ED_undo_push(C, "Flip Color Ramp");
}

static void colorband_distribute(bContext *C, ColorBand *coba, bool evenly)
{
  if (coba->tot > 1) {
    const int tot = evenly ? coba->tot - 1 : coba->tot;
    const float gap = 1.0f / tot;
    float pos = 0.0f;
    for (int a = 0; a < coba->tot; a++) {
      coba->data[a].pos = pos;
      pos += gap;
    }
    const char *undo_str = evenly ? CTX_N_(BLT_I18NCONTEXT_OPERATOR_DEFAULT,
                                           "Distribute Stops Evenly") :
                                    CTX_N_(BLT_I18NCONTEXT_OPERATOR_DEFAULT,
                                           "Distribute Stops from Left");
    ED_undo_push(C, undo_str);
  }
}

static uiBlock *colorband_tools_fn(bContext *C, ARegion *region, void *cb_v)
{
  RNAUpdateCb &cb = *static_cast<RNAUpdateCb *>(cb_v);
  const uiStyle *style = UI_style_get_dpi();
  PointerRNA coba_ptr = RNA_property_pointer_get(&cb.ptr, cb.prop);
  ColorBand *coba = static_cast<ColorBand *>(coba_ptr.data);
  short yco = 0;
  const short menuwidth = 10 * UI_UNIT_X;

  uiBlock *block = UI_block_begin(C, region, __func__, blender::ui::EmbossType::Pulldown);

  uiLayout &layout = blender::ui::block_layout(block,
                                               blender::ui::LayoutDirection::Vertical,
                                               blender::ui::LayoutType::Menu,
                                               0,
                                               0,
                                               UI_MENU_WIDTH_MIN,
                                               0,
                                               UI_MENU_PADDING,
                                               style);
  blender::ui::block_layout_set_current(block, &layout);
  {
    layout.context_ptr_set("color_ramp", &coba_ptr);
  }

  /* We could move these to operators,
   * although this isn't important unless we want to assign key shortcuts to them. */
  {
    uiBut *but = uiDefIconTextBut(block,
                                  ButType::ButMenu,
                                  1,
                                  ICON_ARROW_LEFTRIGHT,
                                  IFACE_("Flip Color Ramp"),
                                  0,
                                  yco -= UI_UNIT_Y,
                                  menuwidth,
                                  UI_UNIT_Y,
                                  nullptr,
                                  "");
    UI_but_func_set(but, [coba, cb](bContext &C) {
      colorband_flip(&C, coba);
      ED_region_tag_redraw(CTX_wm_region(&C));
      rna_update_cb(C, cb);
    });
  }
  {
    uiBut *but = uiDefIconTextBut(block,
                                  ButType::ButMenu,
                                  1,
                                  ICON_BLANK1,
                                  IFACE_("Distribute Stops from Left"),
                                  0,
                                  yco -= UI_UNIT_Y,
                                  menuwidth,
                                  UI_UNIT_Y,
                                  nullptr,
                                  "");
    UI_but_func_set(but, [coba, cb](bContext &C) {
      colorband_distribute(&C, coba, false);
      ED_region_tag_redraw(CTX_wm_region(&C));
      rna_update_cb(C, cb);
    });
  }
  {
    uiBut *but = uiDefIconTextBut(block,
                                  ButType::ButMenu,
                                  1,
                                  ICON_BLANK1,
                                  IFACE_("Distribute Stops Evenly"),
                                  0,
                                  yco -= UI_UNIT_Y,
                                  menuwidth,
                                  UI_UNIT_Y,
                                  nullptr,
                                  "");
    UI_but_func_set(but, [coba, cb](bContext &C) {
      colorband_distribute(&C, coba, true);
      ED_region_tag_redraw(CTX_wm_region(&C));
      rna_update_cb(C, cb);
    });
  }

  layout.separator();

  layout.op("UI_OT_eyedropper_colorramp", IFACE_("Eyedropper"), ICON_EYEDROPPER);

  layout.separator();

  {
    uiBut *but = uiDefIconTextBut(block,
                                  ButType::ButMenu,
                                  1,
                                  ICON_LOOP_BACK,
                                  IFACE_("Reset Color Ramp"),
                                  0,
                                  yco -= UI_UNIT_Y,
                                  menuwidth,
                                  UI_UNIT_Y,
                                  nullptr,
                                  "");
    UI_but_func_set(but, [coba, cb](bContext &C) {
      BKE_colorband_init(coba, true);
      ED_undo_push(&C, "Reset Color Ramp");
      ED_region_tag_redraw(CTX_wm_region(&C));
      rna_update_cb(C, cb);
    });
  }

  UI_block_direction_set(block, UI_DIR_DOWN);
  UI_block_bounds_set_text(block, 3.0f * UI_UNIT_X);

  return block;
}

static void colorband_add(bContext &C, const RNAUpdateCb &cb, ColorBand &coba)
{
  float pos = 0.5f;

  if (coba.tot > 1) {
    if (coba.cur > 0) {
      pos = (coba.data[coba.cur - 1].pos + coba.data[coba.cur].pos) * 0.5f;
    }
    else {
      pos = (coba.data[coba.cur + 1].pos + coba.data[coba.cur].pos) * 0.5f;
    }
  }

  if (BKE_colorband_element_add(&coba, pos)) {
    rna_update_cb(C, cb);
    ED_undo_push(&C, "Add Color Ramp Stop");
  }
}

static void colorband_update_cb(bContext * /*C*/, void *bt_v, void *coba_v)
{
  uiBut *bt = static_cast<uiBut *>(bt_v);
  ColorBand *coba = static_cast<ColorBand *>(coba_v);

  /* Sneaky update here, we need to sort the color-band points to be in order,
   * however the RNA pointer then is wrong, so we update it */
  BKE_colorband_update_sort(coba);
  bt->rnapoin.data = coba->data + coba->cur;
}

static void colorband_buttons_layout(uiLayout *layout,
                                     uiBlock *block,
                                     ColorBand *coba,
                                     const rctf *butr,
                                     const RNAUpdateCb &cb,
                                     int expand)
{
  uiBut *bt;
  const float unit = BLI_rctf_size_x(butr) / 14.0f;
  const float xs = butr->xmin;
  const float ys = butr->ymin;

  PointerRNA ptr = RNA_pointer_create_discrete(cb.ptr.owner_id, &RNA_ColorRamp, coba);

  uiLayout *split = &layout->split(0.4f, false);

  UI_block_emboss_set(block, blender::ui::EmbossType::None);
  UI_block_align_begin(block);
  uiLayout *row = &split->row(false);

  bt = uiDefIconTextBut(block,
                        ButType::But,
                        0,
                        ICON_ADD,
                        "",
                        0,
                        0,
                        2.0f * unit,
                        UI_UNIT_Y,
                        nullptr,
                        TIP_("Add a new color stop to the color ramp"));
  UI_but_func_set(bt, [coba, cb](bContext &C) { colorband_add(C, cb, *coba); });

  bt = uiDefIconTextBut(block,
                        ButType::But,
                        0,
                        ICON_REMOVE,
                        "",
                        xs + 2.0f * unit,
                        ys + UI_UNIT_Y,
                        2.0f * unit,
                        UI_UNIT_Y,
                        nullptr,
                        TIP_("Delete the active position"));
  UI_but_func_set(bt, [coba, cb](bContext &C) {
    if (BKE_colorband_element_remove(coba, coba->cur)) {
      rna_update_cb(C, cb);
      ED_undo_push(&C, "Delete Color Ramp Stop");
    }
  });

  RNAUpdateCb *tools_cb = MEM_new<RNAUpdateCb>(__func__, cb);
  bt = uiDefIconBlockBut(block,
                         colorband_tools_fn,
                         tools_cb,
                         0,
                         ICON_DOWNARROW_HLT,
                         xs + 4.0f * unit,
                         ys + UI_UNIT_Y,
                         2.0f * unit,
                         UI_UNIT_Y,
                         TIP_("Tools"));
  /* Pass ownership of `tools_cb` to the button. */
  UI_but_funcN_set(
      bt,
      [](bContext *, void *, void *) {},
      tools_cb,
      nullptr,
      but_func_argN_free<RNAUpdateCb>,
      but_func_argN_copy<RNAUpdateCb>);

  UI_block_align_end(block);
  UI_block_emboss_set(block, blender::ui::EmbossType::Emboss);

  row = &split->row(false);

  UI_block_align_begin(block);
  row->prop(&ptr, "color_mode", UI_ITEM_NONE, "", ICON_NONE);
  if (ELEM(coba->color_mode, COLBAND_BLEND_HSV, COLBAND_BLEND_HSL)) {
    row->prop(&ptr, "hue_interpolation", UI_ITEM_NONE, "", ICON_NONE);
  }
  else { /* COLBAND_BLEND_RGB */
    row->prop(&ptr, "interpolation", UI_ITEM_NONE, "", ICON_NONE);
  }
  UI_block_align_end(block);

  row = &layout->row(false);

  bt = uiDefBut(
      block, ButType::ColorBand, 0, "", xs, ys, BLI_rctf_size_x(butr), UI_UNIT_Y, coba, 0, 0, "");
  bt->rnapoin = cb.ptr;
  bt->rnaprop = cb.prop;
  UI_but_func_set(bt, [cb](bContext &C) { rna_update_cb(C, cb); });

  row = &layout->row(false);

  if (coba->tot) {
    CBData *cbd = coba->data + coba->cur;

    ptr = RNA_pointer_create_discrete(cb.ptr.owner_id, &RNA_ColorRampElement, cbd);

    if (!expand) {
      split = &layout->split(0.3f, false);

      row = &split->row(false);
      bt = uiDefButS(block,
                     ButType::Num,
                     0,
                     "",
                     0,
                     0,
                     5.0f * UI_UNIT_X,
                     UI_UNIT_Y,
                     &coba->cur,
                     0.0,
                     float(std::max(0, coba->tot - 1)),
                     TIP_("Choose active color stop"));
      UI_but_number_step_size_set(bt, 1);

      row = &split->row(false);
      row->prop(&ptr, "position", UI_ITEM_NONE, IFACE_("Pos"), ICON_NONE);

      row = &layout->row(false);
      row->prop(&ptr, "color", UI_ITEM_NONE, "", ICON_NONE);
    }
    else {
      split = &layout->split(0.5f, false);
      uiLayout *subsplit = &split->split(0.35f, false);

      row = &subsplit->row(false);
      bt = uiDefButS(block,
                     ButType::Num,
                     0,
                     "",
                     0,
                     0,
                     5.0f * UI_UNIT_X,
                     UI_UNIT_Y,
                     &coba->cur,
                     0.0,
                     float(std::max(0, coba->tot - 1)),
                     TIP_("Choose active color stop"));
      UI_but_number_step_size_set(bt, 1);

      row = &subsplit->row(false);
      row->prop(&ptr, "position", UI_ITEM_R_SLIDER, IFACE_("Pos"), ICON_NONE);

      row = &split->row(false);
      row->prop(&ptr, "color", UI_ITEM_NONE, "", ICON_NONE);
    }

    /* Some special (rather awkward) treatment to update UI state on certain property changes. */
    for (int i = block->buttons.size() - 1; i >= 0; i--) {
      uiBut *but = block->buttons[i].get();
      if (but->rnapoin.data != ptr.data) {
        continue;
      }
      if (!but->rnaprop) {
        continue;
      }

      const char *prop_identifier = RNA_property_identifier(but->rnaprop);
      if (STREQ(prop_identifier, "position")) {
        UI_but_func_set(but, colorband_update_cb, but, coba);
      }

      if (STREQ(prop_identifier, "color")) {
        UI_but_func_set(bt, [cb](bContext &C) { rna_update_cb(C, cb); });
      }
    }
  }
}

void uiTemplateColorRamp(uiLayout *layout,
                         PointerRNA *ptr,
                         const StringRefNull propname,
                         bool expand)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, propname.c_str());

  if (!prop || RNA_property_type(prop) != PROP_POINTER) {
    return;
  }

  const PointerRNA cptr = RNA_property_pointer_get(ptr, prop);
  if (!cptr.data || !RNA_struct_is_a(cptr.type, &RNA_ColorRamp)) {
    return;
  }

  rctf rect;
  rect.xmin = 0;
  rect.xmax = 10.0f * UI_UNIT_X;
  rect.ymin = 0;
  rect.ymax = 19.5f * UI_UNIT_X;

  uiBlock *block = layout->absolute_block();

  ID *id = cptr.owner_id;
  UI_block_lock_set(block, (id && !ID_IS_EDITABLE(id)), ERROR_LIBDATA_MESSAGE);

  colorband_buttons_layout(
      layout, block, static_cast<ColorBand *>(cptr.data), &rect, RNAUpdateCb{*ptr, prop}, expand);

  UI_block_lock_clear(block);
}
