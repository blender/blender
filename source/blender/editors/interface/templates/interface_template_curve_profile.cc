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
#include "BLI_math_vector_types.hh"
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

struct CurveRuntimeProperties {
  CurveProfilePoint *last_pt = nullptr;
  float2 last_pos;
};

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
  Vector<CurveProfilePoint *> selected_points;
  bool point_last_or_first = false;
  for (int i = 0; i < profile->path_len; i++) {
    if (profile->path[i].flag & (PROF_SELECT | PROF_H1_SELECT | PROF_H2_SELECT)) {
      selected_points.append(&profile->path[i]);
      if (ELEM(i, 0, profile->path_len - 1) && profile->path[i].flag & PROF_SELECT) {
        point_last_or_first = true;
      }
    }
  }

  /* Selected point data */
  if (!selected_points.is_empty()) {
    rctf bounds;
    if (profile->flag & PROF_USE_CLIP) {
      bounds = profile->clip_rect;
    }
    else {
      bounds.xmin = bounds.ymin = -1000.0;
      bounds.xmax = bounds.ymax = 1000.0;
    }

    row = &layout.row(true);

    auto curve_runtime = std::make_shared<CurveRuntimeProperties>();
    curve_runtime->last_pt = BKE_curveprofile_active_get(profile);

    PointerRNA point_ptr = RNA_pointer_create_discrete(
        ptr->owner_id, RNA_CurveProfilePoint, curve_runtime->last_pt);
    PropertyRNA *prop_handle_type = RNA_struct_find_property(&point_ptr, "handle_type_1");
    row->prop(&point_ptr,
              prop_handle_type,
              RNA_NO_INDEX,
              0,
              ITEM_R_EXPAND | ITEM_R_ICON_ONLY,
              "",
              ICON_NONE);

    /* Position */
    float *last_x_ptr = BKE_curveprofile_active_location_get(curve_runtime->last_pt);
    float *last_y_ptr = last_x_ptr + 1;
    curve_runtime->last_pos.x = *last_x_ptr;
    curve_runtime->last_pos.y = *last_y_ptr;

    /* While the slider controls the active element, all selected points move together.
     * Contract the slider range so the outermost selected points stay within the clip region. */
    rctf slider_bounds = bounds;
    if (selected_points.size() > 1) {
      rctf selection_bounds;
      BLI_rctf_init_minmax(&selection_bounds);

      /* The slider only shows the active point's position, but moves all selected points by the
       * same delta. Clamp the range so points at the edges of the selection can't be moved outside
       * the clip region. */
      for (const CurveProfilePoint *pt : selected_points) {
        if (pt->flag & PROF_SELECT) {
          const float loc[2] = {pt->x, pt->y};
          BLI_rctf_do_minmax_v(&selection_bounds, loc);
        }
        if (pt->flag & PROF_H1_SELECT) {
          BLI_rctf_do_minmax_v(&selection_bounds, pt->h1_loc);
        }
        if (pt->flag & PROF_H2_SELECT) {
          BLI_rctf_do_minmax_v(&selection_bounds, pt->h2_loc);
        }
      }

      slider_bounds.xmin += curve_runtime->last_pos.x - selection_bounds.xmin;
      slider_bounds.xmax += curve_runtime->last_pos.x - selection_bounds.xmax;
      slider_bounds.ymin += curve_runtime->last_pos.y - selection_bounds.ymin;
      slider_bounds.ymax += curve_runtime->last_pos.y - selection_bounds.ymax;
    }

    /* Requires BKE_curveprofile_translate_selection to handle the handle manipulation, no
     * simplified logic. */
    const char *const axis_labels[2] = {"X:", "Y:"};
    float *const axis_ptrs[2] = {last_x_ptr, last_y_ptr};
    const float axis_min[2] = {slider_bounds.xmin, slider_bounds.ymin};
    const float axis_max[2] = {slider_bounds.xmax, slider_bounds.ymax};
    for (int axis = 0; axis < 2; axis++) {
      bt = uiDefButV(block,
                     ButtonType::Num,
                     axis_labels[axis],
                     0,
                     (2 - axis) * UI_UNIT_Y,
                     UI_UNIT_X * 10,
                     UI_UNIT_Y,
                     axis_ptrs[axis],
                     axis_min[axis],
                     axis_max[axis],
                     "");
      button_number_step_size_set(bt, 1);
      button_number_precision_set(bt, 5);
      button_func_set(bt, [profile, cb, curve_runtime, axis](bContext &C) {
        float *last_pt_co = BKE_curveprofile_active_location_get(curve_runtime->last_pt);
        const float delta = last_pt_co[axis] - curve_runtime->last_pos[axis];
        /* Logically `-= delta`, better restore the original value. */
        last_pt_co[axis] = curve_runtime->last_pos[axis];
        float2 offset(0.0f);
        offset[axis] = delta;
        BKE_curveprofile_translate_selection(profile, offset);
        BKE_curveprofile_update(profile, PROF_UPDATE_REMOVE_DOUBLES | PROF_UPDATE_CLIP);
        rna_update_cb(C, cb);

        /* Update the active point if the pointer changed. */
        curve_runtime->last_pt = BKE_curveprofile_active_get(profile);
        last_pt_co = BKE_curveprofile_active_location_get(curve_runtime->last_pt);
        curve_runtime->last_pos[axis] = last_pt_co[axis];
      });
      if (point_last_or_first) {
        button_flag_enable(bt, BUT_DISABLED);
      }
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
