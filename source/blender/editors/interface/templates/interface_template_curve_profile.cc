/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include "BKE_context.hh"
#include "BKE_curveprofile.h"
#include "BKE_library.hh"

#include "BLI_math_base.h"
#include "BLI_rect.h"
#include "BLI_string_ref.hh"

#include "BLT_translation.hh"

#include "ED_screen.hh"
#include "ED_undo.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "UI_interface_layout.hh"
#include "interface_intern.hh"
#include "interface_templates_intern.hh"

namespace blender::ui {

static Block *curve_profile_presets_fn(bContext *C, ARegion *region, void *cb_v)
{
  RNAUpdateCb &cb = *static_cast<RNAUpdateCb *>(cb_v);
  PointerRNA profile_ptr = RNA_property_pointer_get(&cb.ptr, cb.prop);
  CurveProfile *profile = static_cast<CurveProfile *>(profile_ptr.data);
  short yco = 0;

  Block *block = block_begin(C, region, __func__, EmbossType::Emboss);

  for (const auto &item :
       {std::pair<StringRef, eCurveProfilePresets>(IFACE_("Default"), PROF_PRESET_LINE),
        std::pair<StringRef, eCurveProfilePresets>(
            CTX_IFACE_(BLT_I18NCONTEXT_ID_MESH, "Support Loops"), PROF_PRESET_SUPPORTS),
        std::pair<StringRef, eCurveProfilePresets>(
            CTX_IFACE_(BLT_I18NCONTEXT_ID_MESH, "Cornice Molding"), PROF_PRESET_CORNICE),
        std::pair<StringRef, eCurveProfilePresets>(
            CTX_IFACE_(BLT_I18NCONTEXT_ID_MESH, "Crown Molding"), PROF_PRESET_CROWN),
        std::pair<StringRef, eCurveProfilePresets>(CTX_IFACE_(BLT_I18NCONTEXT_ID_MESH, "Steps"),
                                                   PROF_PRESET_STEPS)})
  {
    Button *but = uiDefIconTextBut(block,
                                   ButtonType::ButMenu,
                                   ICON_BLANK1,
                                   item.first,
                                   0,
                                   yco -= UI_UNIT_Y,
                                   0,
                                   UI_UNIT_Y,
                                   nullptr,
                                   "");
    button_retval_set(but, 1);
    const eCurveProfilePresets preset = item.second;
    button_func_set(but, [profile, cb, preset](bContext &C) {
      profile->preset = preset;
      BKE_curveprofile_reset(profile);
      BKE_curveprofile_update(profile, PROF_UPDATE_NONE);
      ED_undo_push(&C, "Reset Curve Profile");
      ED_region_tag_redraw(CTX_wm_region(&C));
      rna_update_cb(C, cb);
    });
  }

  block_direction_set(block, UI_DIR_DOWN);
  block_bounds_set_text(block, int(3.0f * UI_UNIT_X));

  return block;
}

static Block *curve_profile_tools_fn(bContext *C, ARegion *region, void *cb_v)
{
  RNAUpdateCb &cb = *static_cast<RNAUpdateCb *>(cb_v);
  PointerRNA profile_ptr = RNA_property_pointer_get(&cb.ptr, cb.prop);
  CurveProfile *profile = static_cast<CurveProfile *>(profile_ptr.data);
  short yco = 0;

  Block *block = block_begin(C, region, __func__, EmbossType::Emboss);

  {
    Button *but = uiDefIconTextBut(block,
                                   ButtonType::ButMenu,
                                   ICON_BLANK1,
                                   IFACE_("Reset View"),
                                   0,
                                   yco -= UI_UNIT_Y,
                                   0,
                                   UI_UNIT_Y,
                                   nullptr,
                                   "");
    button_retval_set(but, 1);
    button_func_set(but, [profile](bContext &C) {
      BKE_curveprofile_reset_view(profile);
      ED_region_tag_redraw(CTX_wm_region(&C));
    });
  }
  {
    Button *but = uiDefIconTextBut(block,
                                   ButtonType::ButMenu,
                                   ICON_BLANK1,
                                   IFACE_("Reset Curve"),
                                   0,
                                   yco -= UI_UNIT_Y,
                                   0,
                                   UI_UNIT_Y,
                                   nullptr,
                                   "");
    button_retval_set(but, 1);
    button_func_set(but, [profile, cb](bContext &C) {
      BKE_curveprofile_reset(profile);
      BKE_curveprofile_update(profile, PROF_UPDATE_NONE);
      ED_undo_push(&C, "Reset Profile");
      ED_region_tag_redraw(CTX_wm_region(&C));
      rna_update_cb(C, cb);
    });
  }

  block_direction_set(block, UI_DIR_DOWN);
  block_bounds_set_text(block, int(3.0f * UI_UNIT_X));

  return block;
}

static bool curve_profile_can_zoom_in(CurveProfile *profile)
{
  return BLI_rctf_size_x(&profile->view_rect) >
         CURVE_ZOOM_MAX * BLI_rctf_size_x(&profile->clip_rect);
}

static bool curve_profile_can_zoom_out(CurveProfile *profile)
{
  return BLI_rctf_size_x(&profile->view_rect) < BLI_rctf_size_x(&profile->clip_rect);
}

static void curve_profile_zoom_in(bContext *C, CurveProfile *profile)
{
  if (curve_profile_can_zoom_in(profile)) {
    const float dx = 0.1154f * BLI_rctf_size_x(&profile->view_rect);
    profile->view_rect.xmin += dx;
    profile->view_rect.xmax -= dx;
    const float dy = 0.1154f * BLI_rctf_size_y(&profile->view_rect);
    profile->view_rect.ymin += dy;
    profile->view_rect.ymax -= dy;
  }

  ED_region_tag_redraw(CTX_wm_region(C));
}

static void curve_profile_zoom_out(bContext *C, CurveProfile *profile)
{
  if (curve_profile_can_zoom_out(profile)) {
    float d = 0.15f * BLI_rctf_size_x(&profile->view_rect);
    float d1 = d;

    if (profile->flag & PROF_USE_CLIP) {
      if (profile->view_rect.xmin - d < profile->clip_rect.xmin) {
        d1 = profile->view_rect.xmin - profile->clip_rect.xmin;
      }
    }
    profile->view_rect.xmin -= d1;

    d1 = d;
    if (profile->flag & PROF_USE_CLIP) {
      if (profile->view_rect.xmax + d > profile->clip_rect.xmax) {
        d1 = -profile->view_rect.xmax + profile->clip_rect.xmax;
      }
    }
    profile->view_rect.xmax += d1;

    d = d1 = 0.15f * BLI_rctf_size_y(&profile->view_rect);

    if (profile->flag & PROF_USE_CLIP) {
      if (profile->view_rect.ymin - d < profile->clip_rect.ymin) {
        d1 = profile->view_rect.ymin - profile->clip_rect.ymin;
      }
    }
    profile->view_rect.ymin -= d1;

    d1 = d;
    if (profile->flag & PROF_USE_CLIP) {
      if (profile->view_rect.ymax + d > profile->clip_rect.ymax) {
        d1 = -profile->view_rect.ymax + profile->clip_rect.ymax;
      }
    }
    profile->view_rect.ymax += d1;
  }

  ED_region_tag_redraw(CTX_wm_region(C));
}

static void CurveProfile_buttons_layout(Layout &layout, PointerRNA *ptr, const RNAUpdateCb &cb)
{
  CurveProfile *profile = static_cast<CurveProfile *>(ptr->data);
  Button *bt;

  Block *block = layout.block();

  block_emboss_set(block, EmbossType::Emboss);

  layout.use_property_split_set(false);

  /* Preset selector */
  /* There is probably potential to use simpler "Layout::prop" functions here, but
   * automatic updating after a preset is selected would be more complicated. */
  Layout *row = &layout.row(true);
  RNAUpdateCb *presets_cb = MEM_new<RNAUpdateCb>(__func__, cb);
  bt = uiDefBlockBut(block,
                     curve_profile_presets_fn,
                     presets_cb,
                     IFACE_("Preset"),
                     0,
                     0,
                     UI_UNIT_X,
                     UI_UNIT_X,
                     "");
  /* Pass ownership of `presets_cb` to the button. */
  button_funcN_set(
      bt,
      [](bContext *, void *, void *) {},
      presets_cb,
      nullptr,
      but_func_argN_free<RNAUpdateCb>,
      but_func_argN_copy<RNAUpdateCb>);

  /* Show a "re-apply" preset button when it has been changed from the preset. */
  if (profile->flag & PROF_DIRTY_PRESET) {
    /* Only for dynamic presets. */
    if (ELEM(profile->preset, PROF_PRESET_STEPS, PROF_PRESET_SUPPORTS)) {
      bt = uiDefIconTextBut(block,
                            ButtonType::But,
                            ICON_NONE,
                            IFACE_("Apply Preset"),
                            0,
                            0,
                            UI_UNIT_X,
                            UI_UNIT_X,
                            nullptr,
                            TIP_("Reapply and update the preset, removing changes"));
      button_func_set(bt, [profile, cb](bContext &C) {
        BKE_curveprofile_reset(profile);
        BKE_curveprofile_update(profile, PROF_UPDATE_NONE);
        rna_update_cb(C, cb);
      });
    }
  }

  row = &layout.row(false);

  /* (Left aligned) */
  Layout *sub = &row->row(true);
  sub->alignment_set(LayoutAlign::Left);

  /* Zoom in */
  bt = uiDefIconBut(block,
                    ButtonType::But,
                    ICON_ZOOM_IN,
                    0,
                    0,
                    UI_UNIT_X,
                    UI_UNIT_X,
                    nullptr,
                    0.0,
                    0.0,
                    TIP_("Zoom in"));
  button_func_set(bt, [profile](bContext &C) { curve_profile_zoom_in(&C, profile); });
  if (!curve_profile_can_zoom_in(profile)) {
    button_disable(bt, "");
  }

  /* Zoom out */
  bt = uiDefIconBut(block,
                    ButtonType::But,
                    ICON_ZOOM_OUT,
                    0,
                    0,
                    UI_UNIT_X,
                    UI_UNIT_X,
                    nullptr,
                    0.0,
                    0.0,
                    TIP_("Zoom out"));
  button_func_set(bt, [profile](bContext &C) { curve_profile_zoom_out(&C, profile); });
  if (!curve_profile_can_zoom_out(profile)) {
    button_disable(bt, "");
  }

  /* (Right aligned) */
  sub = &row->row(true);
  sub->alignment_set(LayoutAlign::Right);

  /* Flip path */
  bt = uiDefIconBut(block,
                    ButtonType::But,
                    ICON_ARROW_LEFTRIGHT,
                    0,
                    0,
                    UI_UNIT_X,
                    UI_UNIT_X,
                    nullptr,
                    0.0,
                    0.0,
                    TIP_("Reverse Path"));
  button_func_set(bt, [profile, cb](bContext &C) {
    BKE_curveprofile_reverse(profile);
    BKE_curveprofile_update(profile, PROF_UPDATE_NONE);
    rna_update_cb(C, cb);
  });

  /* Clipping toggle */
  const int icon = (profile->flag & PROF_USE_CLIP) ? ICON_CLIPUV_HLT : ICON_CLIPUV_DEHLT;
  bt = uiDefIconBut(block,
                    ButtonType::But,
                    icon,
                    0,
                    0,
                    UI_UNIT_X,
                    UI_UNIT_X,
                    nullptr,
                    0.0,
                    0.0,
                    TIP_("Toggle Profile Clipping"));
  button_func_set(bt, [profile, cb](bContext &C) {
    profile->flag ^= PROF_USE_CLIP;
    BKE_curveprofile_update(profile, PROF_UPDATE_NONE);
    rna_update_cb(C, cb);
  });

  /* Reset view, reset curve */
  RNAUpdateCb *tools_cb = MEM_new<RNAUpdateCb>(__func__, cb);
  bt = uiDefIconBlockBut(block,
                         curve_profile_tools_fn,
                         tools_cb,
                         ICON_NONE,
                         0,
                         0,
                         UI_UNIT_X,
                         UI_UNIT_X,
                         TIP_("Tools"));
  /* Pass ownership of `presets_cb` to the button. */
  button_funcN_set(
      bt,
      [](bContext *, void *, void *) {},
      tools_cb,
      nullptr,
      but_func_argN_free<RNAUpdateCb>,
      but_func_argN_copy<RNAUpdateCb>);

  block_funcN_set(block,
                  rna_update_cb,
                  MEM_new<RNAUpdateCb>(__func__, cb),
                  nullptr,
                  but_func_argN_free<RNAUpdateCb>,
                  but_func_argN_copy<RNAUpdateCb>);

  /* The path itself */
  int path_width = max_ii(layout.width(), UI_UNIT_X);
  path_width = min_ii(path_width, int(16.0f * UI_UNIT_X));
  const int path_height = path_width;
  layout.row(false);
  uiDefBut(block,
           ButtonType::CurveProfile,
           "",
           0,
           0,
           short(path_width),
           short(path_height),
           profile,
           0.0f,
           1.0f,
           "");

  /* Position sliders for (first) selected point */
  int i;
  float *selection_x, *selection_y;
  bool point_last_or_first = false;
  CurveProfilePoint *point = nullptr;
  for (i = 0; i < profile->path_len; i++) {
    if (profile->path[i].flag & PROF_SELECT) {
      point = &profile->path[i];
      selection_x = &point->x;
      selection_y = &point->y;
      break;
    }
    if (profile->path[i].flag & PROF_H1_SELECT) {
      point = &profile->path[i];
      selection_x = &point->h1_loc[0];
      selection_y = &point->h1_loc[1];
    }
    else if (profile->path[i].flag & PROF_H2_SELECT) {
      point = &profile->path[i];
      selection_x = &point->h2_loc[0];
      selection_y = &point->h2_loc[1];
    }
  }
  if (ELEM(i, 0, profile->path_len - 1)) {
    point_last_or_first = true;
  }

  /* Selected point data */
  rctf bounds;
  if (point) {
    if (profile->flag & PROF_USE_CLIP) {
      bounds = profile->clip_rect;
    }
    else {
      bounds.xmin = bounds.ymin = -1000.0;
      bounds.xmax = bounds.ymax = 1000.0;
    }

    row = &layout.row(true);

    PointerRNA point_ptr = RNA_pointer_create_discrete(
        ptr->owner_id, RNA_CurveProfilePoint, point);
    PropertyRNA *prop_handle_type = RNA_struct_find_property(&point_ptr, "handle_type_1");
    row->prop(&point_ptr,
              prop_handle_type,
              RNA_NO_INDEX,
              0,
              ITEM_R_EXPAND | ITEM_R_ICON_ONLY,
              "",
              ICON_NONE);

    /* Position */
    bt = uiDefButF(block,
                   ButtonType::Num,
                   "X:",
                   0,
                   2 * UI_UNIT_Y,
                   UI_UNIT_X * 10,
                   UI_UNIT_Y,
                   selection_x,
                   bounds.xmin,
                   bounds.xmax,
                   "");
    button_number_step_size_set(bt, 1);
    button_number_precision_set(bt, 5);
    button_func_set(bt, [profile, cb](bContext &C) {
      BKE_curveprofile_update(profile, PROF_UPDATE_REMOVE_DOUBLES | PROF_UPDATE_CLIP);
      rna_update_cb(C, cb);
    });
    if (point_last_or_first) {
      button_flag_enable(bt, BUT_DISABLED);
    }
    bt = uiDefButF(block,
                   ButtonType::Num,
                   "Y:",
                   0,
                   1 * UI_UNIT_Y,
                   UI_UNIT_X * 10,
                   UI_UNIT_Y,
                   selection_y,
                   bounds.ymin,
                   bounds.ymax,
                   "");
    button_number_step_size_set(bt, 1);
    button_number_precision_set(bt, 5);
    button_func_set(bt, [profile, cb](bContext &C) {
      BKE_curveprofile_update(profile, PROF_UPDATE_REMOVE_DOUBLES | PROF_UPDATE_CLIP);
      rna_update_cb(C, cb);
    });
    if (point_last_or_first) {
      button_flag_enable(bt, BUT_DISABLED);
    }

    /* Delete points */
    bt = uiDefIconBut(block,
                      ButtonType::But,
                      ICON_X,
                      0,
                      0,
                      UI_UNIT_X,
                      UI_UNIT_X,
                      nullptr,
                      0.0,
                      0.0,
                      TIP_("Delete points"));
    button_func_set(bt, [profile, cb](bContext &C) {
      BKE_curveprofile_remove_by_flag(profile, SELECT);
      BKE_curveprofile_update(profile, PROF_UPDATE_NONE);
      rna_update_cb(C, cb);
    });
    if (point_last_or_first) {
      button_flag_enable(bt, BUT_DISABLED);
    }
  }

  layout.prop(ptr, "use_sample_straight_edges", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout.prop(ptr, "use_sample_even_lengths", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  block_funcN_set(block, nullptr, nullptr, nullptr);
}

void template_curve_profile(Layout *layout, PointerRNA *ptr, const StringRefNull propname)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, propname.c_str());

  Block *block = layout->block();

  if (!prop) {
    RNA_warning("Curve Profile property not found: %s.%s",
                RNA_struct_identifier(ptr->type),
                propname.c_str());
    return;
  }

  if (RNA_property_type(prop) != PROP_POINTER) {
    RNA_warning("Curve Profile is not a pointer: %s.%s",
                RNA_struct_identifier(ptr->type),
                propname.c_str());
    return;
  }

  PointerRNA cptr = RNA_property_pointer_get(ptr, prop);
  if (!cptr.data || !RNA_struct_is_a(cptr.type, RNA_CurveProfile)) {
    return;
  }

  ID *id = cptr.owner_id;
  block_lock_set(block, (id && !ID_IS_EDITABLE(id)), ERROR_LIBDATA_MESSAGE);

  CurveProfile_buttons_layout(*layout, &cptr, RNAUpdateCb{*ptr, prop});

  block_lock_clear(block);
}

}  // namespace blender::ui
