/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include "BKE_colortools.hh"
#include "BKE_context.hh"

#include "BLI_rect.h"
#include "BLI_string_ref.hh"

#include "BLT_translation.hh"

#include "ED_screen.hh"
#include "ED_undo.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "UI_interface.hh"
#include "interface_intern.hh"
#include "interface_templates_intern.hh"

using blender::StringRefNull;

static bool curvemap_can_zoom_out(CurveMapping *cumap)
{
  return BLI_rctf_size_x(&cumap->curr) < BLI_rctf_size_x(&cumap->clipr);
}

static bool curvemap_can_zoom_in(CurveMapping *cumap)
{
  return BLI_rctf_size_x(&cumap->curr) > CURVE_ZOOM_MAX * BLI_rctf_size_x(&cumap->clipr);
}

static void curvemap_buttons_zoom_in(bContext *C, CurveMapping *cumap)
{
  if (curvemap_can_zoom_in(cumap)) {
    const float dx = 0.1154f * BLI_rctf_size_x(&cumap->curr);
    cumap->curr.xmin += dx;
    cumap->curr.xmax -= dx;
    const float dy = 0.1154f * BLI_rctf_size_y(&cumap->curr);
    cumap->curr.ymin += dy;
    cumap->curr.ymax -= dy;
  }

  ED_region_tag_redraw(CTX_wm_region(C));
}

static void curvemap_buttons_zoom_out(bContext *C, CurveMapping *cumap)
{
  float d, d1;

  if (curvemap_can_zoom_out(cumap)) {
    d = d1 = 0.15f * BLI_rctf_size_x(&cumap->curr);

    if (cumap->flag & CUMA_DO_CLIP) {
      if (cumap->curr.xmin - d < cumap->clipr.xmin) {
        d1 = cumap->curr.xmin - cumap->clipr.xmin;
      }
    }
    cumap->curr.xmin -= d1;

    d1 = d;
    if (cumap->flag & CUMA_DO_CLIP) {
      if (cumap->curr.xmax + d > cumap->clipr.xmax) {
        d1 = -cumap->curr.xmax + cumap->clipr.xmax;
      }
    }
    cumap->curr.xmax += d1;

    d = d1 = 0.15f * BLI_rctf_size_y(&cumap->curr);

    if (cumap->flag & CUMA_DO_CLIP) {
      if (cumap->curr.ymin - d < cumap->clipr.ymin) {
        d1 = cumap->curr.ymin - cumap->clipr.ymin;
      }
    }
    cumap->curr.ymin -= d1;

    d1 = d;
    if (cumap->flag & CUMA_DO_CLIP) {
      if (cumap->curr.ymax + d > cumap->clipr.ymax) {
        d1 = -cumap->curr.ymax + cumap->clipr.ymax;
      }
    }
    cumap->curr.ymax += d1;
  }

  ED_region_tag_redraw(CTX_wm_region(C));
}

/* NOTE: this is a block-menu, needs 0 events, otherwise the menu closes */
static uiBlock *curvemap_clipping_func(bContext *C, ARegion *region, void *cumap_v)
{
  CurveMapping *cumap = static_cast<CurveMapping *>(cumap_v);
  uiBut *bt;
  const float width = 8 * UI_UNIT_X;

  uiBlock *block = UI_block_begin(C, region, __func__, UI_EMBOSS);
  UI_block_flag_enable(block, UI_BLOCK_KEEP_OPEN | UI_BLOCK_MOVEMOUSE_QUIT);
  UI_block_theme_style_set(block, UI_BLOCK_THEME_STYLE_POPUP);

  bt = uiDefButBitI(block,
                    UI_BTYPE_CHECKBOX,
                    CUMA_DO_CLIP,
                    1,
                    IFACE_("Use Clipping"),
                    0,
                    5 * UI_UNIT_Y,
                    width,
                    UI_UNIT_Y,
                    &cumap->flag,
                    0.0,
                    0.0,
                    "");
  UI_but_func_set(bt, [cumap](bContext & /*C*/) { BKE_curvemapping_changed(cumap, false); });

  UI_block_align_begin(block);
  bt = uiDefButF(block,
                 UI_BTYPE_NUM,
                 0,
                 IFACE_("Min X:"),
                 0,
                 4 * UI_UNIT_Y,
                 width,
                 UI_UNIT_Y,
                 &cumap->clipr.xmin,
                 -100.0,
                 cumap->clipr.xmax,
                 "");
  UI_but_number_step_size_set(bt, 10);
  UI_but_number_precision_set(bt, 2);
  bt = uiDefButF(block,
                 UI_BTYPE_NUM,
                 0,
                 IFACE_("Min Y:"),
                 0,
                 3 * UI_UNIT_Y,
                 width,
                 UI_UNIT_Y,
                 &cumap->clipr.ymin,
                 -100.0,
                 cumap->clipr.ymax,
                 "");
  UI_but_number_step_size_set(bt, 10);
  UI_but_number_precision_set(bt, 2);
  bt = uiDefButF(block,
                 UI_BTYPE_NUM,
                 0,
                 IFACE_("Max X:"),
                 0,
                 2 * UI_UNIT_Y,
                 width,
                 UI_UNIT_Y,
                 &cumap->clipr.xmax,
                 cumap->clipr.xmin,
                 100.0,
                 "");
  UI_but_number_step_size_set(bt, 10);
  UI_but_number_precision_set(bt, 2);
  bt = uiDefButF(block,
                 UI_BTYPE_NUM,
                 0,
                 IFACE_("Max Y:"),
                 0,
                 UI_UNIT_Y,
                 width,
                 UI_UNIT_Y,
                 &cumap->clipr.ymax,
                 cumap->clipr.ymin,
                 100.0,
                 "");
  UI_but_number_step_size_set(bt, 10);
  UI_but_number_precision_set(bt, 2);

  UI_block_bounds_set_normal(block, 0.3f * U.widget_unit);
  UI_block_direction_set(block, UI_DIR_DOWN);

  return block;
}

static uiBlock *curvemap_tools_func(
    bContext *C, ARegion *region, RNAUpdateCb &cb, bool show_extend, int reset_mode)
{
  PointerRNA cumap_ptr = RNA_property_pointer_get(&cb.ptr, cb.prop);
  CurveMapping *cumap = static_cast<CurveMapping *>(cumap_ptr.data);

  short yco = 0;
  const short menuwidth = 10 * UI_UNIT_X;

  uiBlock *block = UI_block_begin(C, region, __func__, UI_EMBOSS);

  {
    uiBut *but = uiDefIconTextBut(block,
                                  UI_BTYPE_BUT_MENU,
                                  1,
                                  ICON_BLANK1,
                                  IFACE_("Reset View"),
                                  0,
                                  yco -= UI_UNIT_Y,
                                  menuwidth,
                                  UI_UNIT_Y,
                                  nullptr,
                                  0.0,
                                  0.0,
                                  "");
    UI_but_func_set(but, [cumap](bContext &C) {
      BKE_curvemapping_reset_view(cumap);
      ED_region_tag_redraw(CTX_wm_region(&C));
    });
  }

  if (show_extend && !(cumap->flag & CUMA_USE_WRAPPING)) {
    {
      uiBut *but = uiDefIconTextBut(block,
                                    UI_BTYPE_BUT_MENU,
                                    1,
                                    ICON_BLANK1,
                                    IFACE_("Extend Horizontal"),
                                    0,
                                    yco -= UI_UNIT_Y,
                                    menuwidth,
                                    UI_UNIT_Y,
                                    nullptr,
                                    0.0,
                                    0.0,
                                    "");
      UI_but_func_set(but, [cumap, cb](bContext &C) {
        cumap->flag &= ~CUMA_EXTEND_EXTRAPOLATE;
        BKE_curvemapping_changed(cumap, false);
        rna_update_cb(C, cb);
        ED_undo_push(&C, "CurveMap tools");
        ED_region_tag_redraw(CTX_wm_region(&C));
      });
    }
    {
      uiBut *but = uiDefIconTextBut(block,
                                    UI_BTYPE_BUT_MENU,
                                    1,
                                    ICON_BLANK1,
                                    IFACE_("Extend Extrapolated"),
                                    0,
                                    yco -= UI_UNIT_Y,
                                    menuwidth,
                                    UI_UNIT_Y,
                                    nullptr,
                                    0.0,
                                    0.0,
                                    "");
      UI_but_func_set(but, [cumap, cb](bContext &C) {
        cumap->flag |= CUMA_EXTEND_EXTRAPOLATE;
        BKE_curvemapping_changed(cumap, false);
        rna_update_cb(C, cb);
        ED_undo_push(&C, "CurveMap tools");
        ED_region_tag_redraw(CTX_wm_region(&C));
      });
    }
  }

  {
    uiBut *but = uiDefIconTextBut(block,
                                  UI_BTYPE_BUT_MENU,
                                  1,
                                  ICON_BLANK1,
                                  IFACE_("Reset Curve"),
                                  0,
                                  yco -= UI_UNIT_Y,
                                  menuwidth,
                                  UI_UNIT_Y,
                                  nullptr,
                                  0.0,
                                  0.0,
                                  "");
    UI_but_func_set(but, [cumap, cb, reset_mode](bContext &C) {
      CurveMap *cuma = cumap->cm + cumap->cur;
      BKE_curvemap_reset(cuma, &cumap->clipr, cumap->preset, reset_mode);
      BKE_curvemapping_changed(cumap, false);
      rna_update_cb(C, cb);
      ED_undo_push(&C, "CurveMap tools");
      ED_region_tag_redraw(CTX_wm_region(&C));
    });
  }

  UI_block_direction_set(block, UI_DIR_DOWN);
  UI_block_bounds_set_text(block, 3.0f * UI_UNIT_X);

  return block;
}

static uiBlock *curvemap_tools_posslope_func(bContext *C, ARegion *region, void *cb_v)
{
  return curvemap_tools_func(
      C, region, *static_cast<RNAUpdateCb *>(cb_v), true, CURVEMAP_SLOPE_POSITIVE);
}

static uiBlock *curvemap_tools_negslope_func(bContext *C, ARegion *region, void *cb_v)
{
  return curvemap_tools_func(
      C, region, *static_cast<RNAUpdateCb *>(cb_v), true, CURVEMAP_SLOPE_NEGATIVE);
}

static uiBlock *curvemap_brush_tools_func(bContext *C, ARegion *region, void *cb_v)
{
  return curvemap_tools_func(
      C, region, *static_cast<RNAUpdateCb *>(cb_v), false, CURVEMAP_SLOPE_POSITIVE);
}

static uiBlock *curvemap_brush_tools_negslope_func(bContext *C, ARegion *region, void *cb_v)
{
  return curvemap_tools_func(
      C, region, *static_cast<RNAUpdateCb *>(cb_v), false, CURVEMAP_SLOPE_POSITIVE);
}

static void curvemap_buttons_redraw(bContext &C)
{
  ED_region_tag_redraw(CTX_wm_region(&C));
}

/**
 * \note Still unsure how this call evolves.
 *
 * \param labeltype: Used for defining which curve-channels to show.
 */
static void curvemap_buttons_layout(uiLayout *layout,
                                    PointerRNA *ptr,
                                    char labeltype,
                                    bool levels,
                                    bool brush,
                                    bool neg_slope,
                                    bool tone,
                                    const RNAUpdateCb &cb)
{
  CurveMapping *cumap = static_cast<CurveMapping *>(ptr->data);
  CurveMap *cm = &cumap->cm[cumap->cur];
  uiBut *bt;
  const float dx = UI_UNIT_X;
  eButGradientType bg = UI_GRAD_NONE;

  uiBlock *block = uiLayoutGetBlock(layout);

  UI_block_emboss_set(block, UI_EMBOSS);

  if (tone) {
    uiLayout *split = uiLayoutSplit(layout, 0.0f, false);
    uiItemR(uiLayoutRow(split, false), ptr, "tone", UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);
  }

  /* curve chooser */
  uiLayout *row = uiLayoutRow(layout, false);

  if (labeltype == 'v') {
    /* vector */
    uiLayout *sub = uiLayoutRow(row, true);
    uiLayoutSetAlignment(sub, UI_LAYOUT_ALIGN_LEFT);

    if (cumap->cm[0].curve) {
      bt = uiDefButI(block, UI_BTYPE_ROW, 0, "X", 0, 0, dx, dx, &cumap->cur, 0.0, 0.0, "");
      UI_but_func_set(bt, curvemap_buttons_redraw);
    }
    if (cumap->cm[1].curve) {
      bt = uiDefButI(block, UI_BTYPE_ROW, 0, "Y", 0, 0, dx, dx, &cumap->cur, 0.0, 1.0, "");
      UI_but_func_set(bt, curvemap_buttons_redraw);
    }
    if (cumap->cm[2].curve) {
      bt = uiDefButI(block, UI_BTYPE_ROW, 0, "Z", 0, 0, dx, dx, &cumap->cur, 0.0, 2.0, "");
      UI_but_func_set(bt, curvemap_buttons_redraw);
    }
  }
  else if (labeltype == 'c' && cumap->tone != CURVE_TONE_FILMLIKE) {
    /* color */
    uiLayout *sub = uiLayoutRow(row, true);
    uiLayoutSetAlignment(sub, UI_LAYOUT_ALIGN_LEFT);

    if (cumap->cm[3].curve) {
      bt = uiDefButI(block,
                     UI_BTYPE_ROW,
                     0,
                     CTX_IFACE_(BLT_I18NCONTEXT_COLOR, "C"),
                     0,
                     0,
                     dx,
                     dx,
                     &cumap->cur,
                     0.0,
                     3.0,
                     TIP_("Combined channels"));
      UI_but_func_set(bt, curvemap_buttons_redraw);
    }
    if (cumap->cm[0].curve) {
      bt = uiDefButI(block,
                     UI_BTYPE_ROW,
                     0,
                     CTX_IFACE_(BLT_I18NCONTEXT_COLOR, "R"),
                     0,
                     0,
                     dx,
                     dx,
                     &cumap->cur,
                     0.0,
                     0.0,
                     TIP_("Red channel"));
      UI_but_func_set(bt, curvemap_buttons_redraw);
    }
    if (cumap->cm[1].curve) {
      bt = uiDefButI(block,
                     UI_BTYPE_ROW,
                     0,
                     CTX_IFACE_(BLT_I18NCONTEXT_COLOR, "G"),
                     0,
                     0,
                     dx,
                     dx,
                     &cumap->cur,
                     0.0,
                     1.0,
                     TIP_("Green channel"));
      UI_but_func_set(bt, curvemap_buttons_redraw);
    }
    if (cumap->cm[2].curve) {
      bt = uiDefButI(block,
                     UI_BTYPE_ROW,
                     0,
                     CTX_IFACE_(BLT_I18NCONTEXT_COLOR, "B"),
                     0,
                     0,
                     dx,
                     dx,
                     &cumap->cur,
                     0.0,
                     2.0,
                     TIP_("Blue channel"));
      UI_but_func_set(bt, curvemap_buttons_redraw);
    }
  }
  else if (labeltype == 'h') {
    /* HSV */
    uiLayout *sub = uiLayoutRow(row, true);
    uiLayoutSetAlignment(sub, UI_LAYOUT_ALIGN_LEFT);

    if (cumap->cm[0].curve) {
      bt = uiDefButI(block,
                     UI_BTYPE_ROW,
                     0,
                     IFACE_("H"),
                     0,
                     0,
                     dx,
                     dx,
                     &cumap->cur,
                     0.0,
                     0.0,
                     TIP_("Hue level"));
      UI_but_func_set(bt, curvemap_buttons_redraw);
    }
    if (cumap->cm[1].curve) {
      bt = uiDefButI(block,
                     UI_BTYPE_ROW,
                     0,
                     IFACE_("S"),
                     0,
                     0,
                     dx,
                     dx,
                     &cumap->cur,
                     0.0,
                     1.0,
                     TIP_("Saturation level"));
      UI_but_func_set(bt, curvemap_buttons_redraw);
    }
    if (cumap->cm[2].curve) {
      bt = uiDefButI(block,
                     UI_BTYPE_ROW,
                     0,
                     IFACE_("V"),
                     0,
                     0,
                     dx,
                     dx,
                     &cumap->cur,
                     0.0,
                     2.0,
                     TIP_("Value level"));
      UI_but_func_set(bt, curvemap_buttons_redraw);
    }
  }
  else {
    uiLayoutSetAlignment(row, UI_LAYOUT_ALIGN_RIGHT);
  }

  if (labeltype == 'h') {
    bg = UI_GRAD_H;
  }

  /* operation buttons */
  /* (Right aligned) */
  uiLayout *sub = uiLayoutRow(row, true);
  uiLayoutSetAlignment(sub, UI_LAYOUT_ALIGN_RIGHT);

  if (!(cumap->flag & CUMA_USE_WRAPPING)) {
    /* Zoom in */
    bt = uiDefIconBut(
        block, UI_BTYPE_BUT, 0, ICON_ZOOM_IN, 0, 0, dx, dx, nullptr, 0.0, 0.0, TIP_("Zoom in"));
    UI_but_func_set(bt, [cumap](bContext &C) { curvemap_buttons_zoom_in(&C, cumap); });
    if (!curvemap_can_zoom_in(cumap)) {
      UI_but_disable(bt, "");
    }

    /* Zoom out */
    bt = uiDefIconBut(
        block, UI_BTYPE_BUT, 0, ICON_ZOOM_OUT, 0, 0, dx, dx, nullptr, 0.0, 0.0, TIP_("Zoom out"));
    UI_but_func_set(bt, [cumap](bContext &C) { curvemap_buttons_zoom_out(&C, cumap); });
    if (!curvemap_can_zoom_out(cumap)) {
      UI_but_disable(bt, "");
    }

    /* Clipping button. */
    const int icon = (cumap->flag & CUMA_DO_CLIP) ? ICON_CLIPUV_HLT : ICON_CLIPUV_DEHLT;
    bt = uiDefIconBlockBut(
        block, curvemap_clipping_func, cumap, 0, icon, 0, 0, dx, dx, TIP_("Clipping Options"));
    bt->drawflag &= ~UI_BUT_ICON_LEFT;
    UI_but_func_set(bt, [cb](bContext &C) { rna_update_cb(C, cb); });
  }

  RNAUpdateCb *tools_cb = MEM_new<RNAUpdateCb>(__func__, cb);
  if (brush && neg_slope) {
    bt = uiDefIconBlockBut(block,
                           curvemap_brush_tools_negslope_func,
                           tools_cb,
                           0,
                           ICON_NONE,
                           0,
                           0,
                           dx,
                           dx,
                           TIP_("Tools"));
  }
  else if (brush) {
    bt = uiDefIconBlockBut(
        block, curvemap_brush_tools_func, tools_cb, 0, ICON_NONE, 0, 0, dx, dx, TIP_("Tools"));
  }
  else if (neg_slope) {
    bt = uiDefIconBlockBut(
        block, curvemap_tools_negslope_func, tools_cb, 0, ICON_NONE, 0, 0, dx, dx, TIP_("Tools"));
  }
  else {
    bt = uiDefIconBlockBut(
        block, curvemap_tools_posslope_func, tools_cb, 0, ICON_NONE, 0, 0, dx, dx, TIP_("Tools"));
  }
  /* Pass ownership of `tools_cb` to the button. */
  UI_but_funcN_set(
      bt,
      [](bContext *, void *, void *) {},
      tools_cb,
      nullptr,
      but_func_argN_free<RNAUpdateCb>,
      but_func_argN_copy<RNAUpdateCb>);

  UI_block_funcN_set(block,
                     rna_update_cb,
                     MEM_new<RNAUpdateCb>(__func__, cb),
                     nullptr,
                     but_func_argN_free<RNAUpdateCb>,
                     but_func_argN_copy<RNAUpdateCb>);

  /* Curve itself. */
  const int size = max_ii(uiLayoutGetWidth(layout), UI_UNIT_X);
  row = uiLayoutRow(layout, false);
  uiButCurveMapping *curve_but = (uiButCurveMapping *)uiDefBut(
      block, UI_BTYPE_CURVE, 0, "", 0, 0, size, 8.0f * UI_UNIT_X, cumap, 0.0f, 1.0f, "");
  curve_but->gradient_type = bg;

  /* Sliders for selected curve point. */
  int i;
  CurveMapPoint *cmp = nullptr;
  bool point_last_or_first = false;
  for (i = 0; i < cm->totpoint; i++) {
    if (cm->curve[i].flag & CUMA_SELECT) {
      cmp = &cm->curve[i];
      break;
    }
  }
  if (ELEM(i, 0, cm->totpoint - 1)) {
    point_last_or_first = true;
  }

  if (cmp) {
    rctf bounds;
    if (cumap->flag & CUMA_DO_CLIP) {
      bounds = cumap->clipr;
    }
    else {
      bounds.xmin = bounds.ymin = -1000.0;
      bounds.xmax = bounds.ymax = 1000.0;
    }

    UI_block_emboss_set(block, UI_EMBOSS);

    uiLayoutRow(layout, true);

    /* Curve handle buttons. */
    bt = uiDefIconBut(block,
                      UI_BTYPE_BUT,
                      1,
                      ICON_HANDLE_AUTO,
                      0,
                      UI_UNIT_Y,
                      UI_UNIT_X,
                      UI_UNIT_Y,
                      nullptr,
                      0.0,
                      0.0,
                      TIP_("Auto Handle"));
    UI_but_func_set(bt, [cumap, cb](bContext &C) {
      CurveMap *cuma = cumap->cm + cumap->cur;
      BKE_curvemap_handle_set(cuma, HD_AUTO);
      BKE_curvemapping_changed(cumap, false);
      rna_update_cb(C, cb);
    });
    if (((cmp->flag & CUMA_HANDLE_AUTO_ANIM) == false) &&
        ((cmp->flag & CUMA_HANDLE_VECTOR) == false))
    {
      bt->flag |= UI_SELECT_DRAW;
    }

    bt = uiDefIconBut(block,
                      UI_BTYPE_BUT,
                      1,
                      ICON_HANDLE_VECTOR,
                      0,
                      UI_UNIT_Y,
                      UI_UNIT_X,
                      UI_UNIT_Y,
                      nullptr,
                      0.0,
                      0.0,
                      TIP_("Vector Handle"));
    UI_but_func_set(bt, [cumap, cb](bContext &C) {
      CurveMap *cuma = cumap->cm + cumap->cur;
      BKE_curvemap_handle_set(cuma, HD_VECT);
      BKE_curvemapping_changed(cumap, false);
      rna_update_cb(C, cb);
    });
    if (cmp->flag & CUMA_HANDLE_VECTOR) {
      bt->flag |= UI_SELECT_DRAW;
    }

    bt = uiDefIconBut(block,
                      UI_BTYPE_BUT,
                      1,
                      ICON_HANDLE_AUTOCLAMPED,
                      0,
                      UI_UNIT_Y,
                      UI_UNIT_X,
                      UI_UNIT_Y,
                      nullptr,
                      0.0,
                      0.0,
                      TIP_("Auto Clamped"));
    UI_but_func_set(bt, [cumap, cb](bContext &C) {
      CurveMap *cuma = cumap->cm + cumap->cur;
      BKE_curvemap_handle_set(cuma, HD_AUTO_ANIM);
      BKE_curvemapping_changed(cumap, false);
      rna_update_cb(C, cb);
    });
    if (cmp->flag & CUMA_HANDLE_AUTO_ANIM) {
      bt->flag |= UI_SELECT_DRAW;
    }

    /* Curve handle position */
    bt = uiDefButF(block,
                   UI_BTYPE_NUM,
                   0,
                   "X:",
                   0,
                   2 * UI_UNIT_Y,
                   UI_UNIT_X * 10,
                   UI_UNIT_Y,
                   &cmp->x,
                   bounds.xmin,
                   bounds.xmax,
                   "");
    UI_but_number_step_size_set(bt, 1);
    UI_but_number_precision_set(bt, 5);
    UI_but_func_set(bt, [cumap, cb](bContext &C) {
      BKE_curvemapping_changed(cumap, true);
      rna_update_cb(C, cb);
    });

    bt = uiDefButF(block,
                   UI_BTYPE_NUM,
                   0,
                   "Y:",
                   0,
                   1 * UI_UNIT_Y,
                   UI_UNIT_X * 10,
                   UI_UNIT_Y,
                   &cmp->y,
                   bounds.ymin,
                   bounds.ymax,
                   "");
    UI_but_number_step_size_set(bt, 1);
    UI_but_number_precision_set(bt, 5);
    UI_but_func_set(bt, [cumap, cb](bContext &C) {
      BKE_curvemapping_changed(cumap, true);
      rna_update_cb(C, cb);
    });

    /* Curve handle delete point */
    bt = uiDefIconBut(
        block, UI_BTYPE_BUT, 0, ICON_X, 0, 0, dx, dx, nullptr, 0.0, 0.0, TIP_("Delete points"));
    UI_but_func_set(bt, [cumap, cb](bContext &C) {
      BKE_curvemap_remove(cumap->cm + cumap->cur, SELECT);
      BKE_curvemapping_changed(cumap, false);
      rna_update_cb(C, cb);
    });
    if (point_last_or_first) {
      UI_but_flag_enable(bt, UI_BUT_DISABLED);
    }
  }

  /* black/white levels */
  if (levels) {
    uiLayout *split = uiLayoutSplit(layout, 0.0f, false);
    uiItemR(uiLayoutColumn(split, false),
            ptr,
            "black_level",
            UI_ITEM_R_EXPAND,
            std::nullopt,
            ICON_NONE);
    uiItemR(uiLayoutColumn(split, false),
            ptr,
            "white_level",
            UI_ITEM_R_EXPAND,
            std::nullopt,
            ICON_NONE);

    uiLayoutRow(layout, false);
    bt = uiDefBut(block,
                  UI_BTYPE_BUT,
                  0,
                  IFACE_("Reset"),
                  0,
                  0,
                  UI_UNIT_X * 10,
                  UI_UNIT_Y,
                  nullptr,
                  0.0f,
                  0.0f,
                  TIP_("Reset Black/White point and curves"));
    UI_but_func_set(bt, [cumap, cb](bContext &C) {
      cumap->preset = CURVE_PRESET_LINE;
      for (int a = 0; a < CM_TOT; a++) {
        BKE_curvemap_reset(cumap->cm + a, &cumap->clipr, cumap->preset, CURVEMAP_SLOPE_POSITIVE);
      }

      cumap->black[0] = cumap->black[1] = cumap->black[2] = 0.0f;
      cumap->white[0] = cumap->white[1] = cumap->white[2] = 1.0f;
      BKE_curvemapping_set_black_white(cumap, nullptr, nullptr);

      BKE_curvemapping_changed(cumap, false);
      rna_update_cb(C, cb);
    });
  }

  UI_block_funcN_set(block, nullptr, nullptr, nullptr);
}

void uiTemplateCurveMapping(uiLayout *layout,
                            PointerRNA *ptr,
                            const StringRefNull propname,
                            int type,
                            bool levels,
                            bool brush,
                            bool neg_slope,
                            bool tone)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, propname.c_str());
  uiBlock *block = uiLayoutGetBlock(layout);

  if (!prop) {
    RNA_warning(
        "curve property not found: %s.%s", RNA_struct_identifier(ptr->type), propname.c_str());
    return;
  }

  if (RNA_property_type(prop) != PROP_POINTER) {
    RNA_warning(
        "curve is not a pointer: %s.%s", RNA_struct_identifier(ptr->type), propname.c_str());
    return;
  }

  PointerRNA cptr = RNA_property_pointer_get(ptr, prop);
  if (!cptr.data || !RNA_struct_is_a(cptr.type, &RNA_CurveMapping)) {
    return;
  }

  ID *id = cptr.owner_id;
  UI_block_lock_set(block, (id && !ID_IS_EDITABLE(id)), ERROR_LIBDATA_MESSAGE);

  curvemap_buttons_layout(
      layout, &cptr, type, levels, brush, neg_slope, tone, RNAUpdateCb{*ptr, prop});

  UI_block_lock_clear(block);
}
