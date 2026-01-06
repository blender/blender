/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * Utilities to inspect the interface, extract information.
 */

#include "BLI_listbase.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_screen_types.h"

#include "BKE_screen.hh"

#include "UI_view2d.hh"

#include "RNA_access.hh"

#include "interface_intern.hh"

#include "UI_abstract_view.hh"

#include "WM_api.hh"
#include "WM_types.hh"

namespace blender::ui {

/* -------------------------------------------------------------------- */
/** \name Button (#Button) State
 * \{ */

bool button_is_editable(const Button *but)
{
  return !ELEM(but->type,
               ButtonType::Label,
               ButtonType::Sepr,
               ButtonType::SeprLine,
               ButtonType::Roundbox,
               ButtonType::ListBox,
               ButtonType::Progress);
}

bool button_is_editable_as_text(const Button *but)
{
  return ELEM(
      but->type, ButtonType::Text, ButtonType::Num, ButtonType::NumSlider, ButtonType::SearchMenu);
}

bool button_is_toggle(const Button *but)
{
  return ELEM(but->type,
              ButtonType::ButToggle,
              ButtonType::Toggle,
              ButtonType::IconToggle,
              ButtonType::IconToggleN,
              ButtonType::ToggleN,
              ButtonType::Checkbox,
              ButtonType::CheckboxN,
              ButtonType::Row);
}

bool button_is_interactive_ex(const Button *but, const bool labeledit, const bool for_tooltip)
{
  /* NOTE: #ButtonType::Label is included for highlights, this allows drags. */
  if (ELEM(but->type, ButtonType::Label, ButtonType::PreviewTile)) {
    if (for_tooltip) {
      /* It's important labels are considered interactive for the purpose of showing tooltip. */
      if (!button_drag_is_draggable(but) && but->tip_func == nullptr &&
          but->tip_custom_func == nullptr && but->tip_quick_func == nullptr &&
          (but->tip == nullptr || but->tip[0] == '\0'))
      {
        return false;
      }
    }
    else {
      if (!button_drag_is_draggable(but)) {
        return false;
      }
    }
  }

  if (ELEM(but->type,
           ButtonType::Roundbox,
           ButtonType::Sepr,
           ButtonType::SeprLine,
           ButtonType::ListBox))
  {
    return false;
  }
  if (but->flag & UI_HIDDEN) {
    return false;
  }
  if (but->flag & UI_SCROLLED) {
    return false;
  }
  if ((but->type == ButtonType::Text) &&
      ELEM(but->emboss, EmbossType::None, EmbossType::NoneOrStatus) && !labeledit)
  {
    return false;
  }
  if ((but->type == ButtonType::ListRow) && labeledit) {
    return false;
  }
  if (but->type == ButtonType::ViewItem) {
    const auto *but_item = static_cast<const ButtonViewItem *>(but);
    return but_item->view_item->is_interactive();
  }

  return true;
}

bool button_is_interactive(const Button *but, const bool labeledit)
{
  return button_is_interactive_ex(but, labeledit, false);
}

bool but_is_utf8(const Button *but)
{
  if (but->rnaprop) {
    return RNA_property_string_is_utf8(but->rnaprop);
  }
  return !(but->flag & BUT_NO_UTF8);
}

#ifdef USE_UI_POPOVER_ONCE
bool button_is_popover_once_compat(const Button *but)
{
  return (ELEM(but->type, ButtonType::But, ButtonType::Decorator) || button_is_toggle(but));
}
#endif

bool button_has_array_value(const Button *but)
{
  return (but->rnapoin.data && but->rnaprop && RNA_property_array_check(but->rnaprop));
}

static wmOperatorType *g_ot_tool_set_by_id = nullptr;
bool but_is_tool(const Button *but)
{
  /* very evil! */
  if (but->optype != nullptr) {
    if (g_ot_tool_set_by_id == nullptr) {
      g_ot_tool_set_by_id = WM_operatortype_find("WM_OT_tool_set_by_id", false);
    }
    if (but->optype == g_ot_tool_set_by_id) {
      return true;
    }
  }
  return false;
}

bool but_has_quick_tooltip(const Button *but)
{
  return (but->drawflag & BUT_HAS_QUICK_TOOLTIP) != 0;
}

int button_icon(const Button *but)
{
  if (!(but->flag & UI_HAS_ICON)) {
    return ICON_NONE;
  }

  const bool is_preview = (but->flag & BUT_ICON_PREVIEW) != 0;

  /* While icon is loading, show loading icon at the normal icon size. */
  if (icon_is_preview_deferred_loading(but->icon, is_preview)) {
    return ICON_PREVIEW_LOADING;
  }

  /* Consecutive icons can be toggle between. */
  if (but->drawflag & BUT_ICON_REVERSE) {
    return but->icon - but->iconadd;
  }
  return but->icon + but->iconadd;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Button (#Button) Spatial
 * \{ */

void button_pie_dir(RadialDirection dir, float vec[2])
{
  float angle;

  BLI_assert(dir != UI_RADIAL_NONE);

  angle = DEG2RADF(float(ui_radial_dir_to_angle[dir]));
  vec[0] = cosf(angle);
  vec[1] = sinf(angle);
}

static bool ui_but_isect_pie_seg(const Block *block, const Button *but)
{
  if (block->pie_data.flags & PIE_INVALID_DIR) {
    return false;
  }

  /* Plus/minus 45 degrees: `cosf(DEG2RADF(45)) == M_SQRT1_2`. */
  const float angle_4th_cos = M_SQRT1_2;
  /* Plus/minus 22.5 degrees: `cosf(DEG2RADF(22.5))`. */
  const float angle_8th_cos = 0.9238795f;

  /* Use a large bias so edge-cases fall back to comparing with the adjacent direction. */
  const float eps_bias = 1e-4;

  float but_dir[2];
  button_pie_dir(but->pie_dir, but_dir);

  const float angle_but_cos = dot_v2v2(but_dir, block->pie_data.pie_dir);
  /* Outside range (with bias). */
  if (angle_but_cos < angle_4th_cos - eps_bias) {
    return false;
  }
  /* Inside range (with bias). */
  if (angle_but_cos > angle_8th_cos + eps_bias) {
    return true;
  }

  /* Check if adjacent direction is closer (with tie breaker). */
  RadialDirection dir_adjacent_8th, dir_adjacent_4th;
  if (angle_signed_v2v2(but_dir, block->pie_data.pie_dir) < 0.0f) {
    dir_adjacent_8th = UI_RADIAL_DIRECTION_PREV(but->pie_dir);
    dir_adjacent_4th = UI_RADIAL_DIRECTION_PREV(dir_adjacent_8th);
  }
  else {
    dir_adjacent_8th = UI_RADIAL_DIRECTION_NEXT(but->pie_dir);
    dir_adjacent_4th = UI_RADIAL_DIRECTION_NEXT(dir_adjacent_8th);
  }

  const bool has_8th_adjacent = block->pie_data.pie_dir_mask & (1 << int(dir_adjacent_8th));

  /* Compare with the adjacent direction (even if there is no button). */
  const RadialDirection dir_adjacent = has_8th_adjacent ? dir_adjacent_8th : dir_adjacent_4th;
  float but_dir_adjacent[2];
  button_pie_dir(dir_adjacent, but_dir_adjacent);

  const float angle_adjacent_cos = dot_v2v2(but_dir_adjacent, block->pie_data.pie_dir);

  /* Tie breaker, so one of the buttons is always selected. */
  if (UNLIKELY(angle_but_cos == angle_adjacent_cos)) {
    return but->pie_dir > dir_adjacent;
  }
  return angle_but_cos > angle_adjacent_cos;
}

bool button_contains_pt(const Button *but, float mx, float my)
{
  return BLI_rctf_isect_pt(&but->rect, mx, my);
}

bool button_contains_rect(const Button *but, const rctf *rect)
{
  return BLI_rctf_isect(&but->rect, rect, nullptr);
}

bool button_contains_point_px(const Button *but, const ARegion *region, const int xy[2])
{
  Block *block = but->block;
  if (!region_contains_point_px(region, xy)) {
    return false;
  }

  float mx = xy[0], my = xy[1];
  window_to_block_fl(region, block, &mx, &my);

  if (but->pie_dir != UI_RADIAL_NONE) {
    if (!ui_but_isect_pie_seg(block, but)) {
      return false;
    }
  }
  else if (!button_contains_pt(but, mx, my)) {
    return false;
  }

  return true;
}

bool ui_but_contains_point_px_icon(const Button *but, ARegion *region, const wmEvent *event)
{
  rcti rect;
  int x = event->xy[0], y = event->xy[1];

  window_to_block(region, but->block, &x, &y);

  BLI_rcti_rctf_copy(&rect, &but->rect);

  if (but->dragflag & BUT_DRAG_FULL_BUT) {
    /* use button size itself */
  }
  else if (but->drawflag & BUT_ICON_LEFT) {
    rect.xmax = rect.xmin + BLI_rcti_size_y(&rect);
  }
  else {
    const int delta = BLI_rcti_size_x(&rect) - BLI_rcti_size_y(&rect);
    rect.xmin += delta / 2;
    rect.xmax -= delta / 2;
  }

  return BLI_rcti_isect_pt(&rect, x, y);
}

static Button *ui_but_find(const ARegion *region,
                           const ButtonFindPollFn find_poll,
                           const void *find_custom_data)
{
  for (Block &block : region->runtime->uiblocks) {
    for (int i = block.buttons.size() - 1; i >= 0; i--) {
      Button *but = block.buttons[i].get();
      if (find_poll && find_poll(but, find_custom_data) == false) {
        continue;
      }
      return but;
    }
  }

  return nullptr;
}

Button *button_find_mouse_over_ex(const ARegion *region,
                                  const int xy[2],
                                  const bool labeledit,
                                  const bool for_tooltip,
                                  const ButtonFindPollFn find_poll,
                                  const void *find_custom_data)
{
  Button *butover = nullptr;

  if (!region_contains_point_px(region, xy)) {
    return nullptr;
  }
  for (Block &block : region->runtime->uiblocks) {
    float mx = xy[0], my = xy[1];
    window_to_block_fl(region, &block, &mx, &my);

    for (int i = block.buttons.size() - 1; i >= 0; i--) {
      Button *but = block.buttons[i].get();
      if (find_poll && find_poll(but, find_custom_data) == false) {
        continue;
      }
      if (button_is_interactive_ex(but, labeledit, for_tooltip)) {
        if (but->pie_dir != UI_RADIAL_NONE) {
          if (ui_but_isect_pie_seg(&block, but)) {
            butover = but;
            break;
          }
        }
        else if (button_contains_pt(but, mx, my)) {
          butover = but;
          break;
        }
      }
    }

    /* CLIP_EVENTS prevents the event from reaching other blocks */
    if (block.flag & BLOCK_CLIP_EVENTS) {
      /* check if mouse is inside block */
      if (BLI_rctf_isect_pt(&block.rect, mx, my)) {
        break;
      }
    }
  }

  return butover;
}

Button *but_find_mouse_over(const ARegion *region, const wmEvent *event)
{
  return button_find_mouse_over_ex(
      region, event->xy, event->modifier & KM_CTRL, false, nullptr, nullptr);
}

Button *button_find_rect_over(const ARegion *region, const rcti *rect_px)
{
  if (!region_contains_rect_px(region, rect_px)) {
    return nullptr;
  }

  /* Currently no need to expose this at the moment. */
  const bool labeledit = true;
  rctf rect_px_fl;
  BLI_rctf_rcti_copy(&rect_px_fl, rect_px);
  Button *butover = nullptr;

  for (Block &block : region->runtime->uiblocks) {
    rctf rect_block;
    window_to_block_rctf(region, &block, &rect_block, &rect_px_fl);

    for (int i = block.buttons.size() - 1; i >= 0; i--) {
      Button *but = block.buttons[i].get();
      if (button_is_interactive(but, labeledit)) {
        /* No pie menu support. */
        BLI_assert(but->pie_dir == UI_RADIAL_NONE);
        if (button_contains_rect(but, &rect_block)) {
          butover = but;
          break;
        }
      }
    }

    /* CLIP_EVENTS prevents the event from reaching other blocks */
    if (block.flag & BLOCK_CLIP_EVENTS) {
      /* check if mouse is inside block */
      if (BLI_rctf_isect(&block.rect, &rect_block, nullptr)) {
        break;
      }
    }
  }
  return butover;
}

Button *list_find_mouse_over_ex(const ARegion *region, const int xy[2])
{
  if (!region_contains_point_px(region, xy)) {
    return nullptr;
  }
  for (Block &block : region->runtime->uiblocks) {
    float mx = xy[0], my = xy[1];
    window_to_block_fl(region, &block, &mx, &my);
    for (int i = block.buttons.size() - 1; i >= 0; i--) {
      Button *but = block.buttons[i].get();
      if (but->type == ButtonType::ListBox && button_contains_pt(but, mx, my)) {
        return but;
      }
    }
  }

  return nullptr;
}

Button *ui_list_find_mouse_over(const ARegion *region, const wmEvent *event)
{
  if (event == nullptr) {
    /* If there is no info about the mouse, just act as if there is nothing underneath it. */
    return nullptr;
  }
  return list_find_mouse_over_ex(region, event->xy);
}

uiList *list_find_mouse_over(const ARegion *region, const wmEvent *event)
{
  Button *list_but = ui_list_find_mouse_over(region, event);
  if (!list_but) {
    return nullptr;
  }

  return static_cast<uiList *>(list_but->custom_data);
}

static bool ui_list_contains_row(const Button *listbox_but, const Button *listrow_but)
{
  BLI_assert(listbox_but->type == ButtonType::ListBox);
  BLI_assert(listrow_but->type == ButtonType::ListRow);
  /* The list box and its rows have the same RNA data (active data pointer/prop). */
  return button_rna_equals(listbox_but, listrow_but);
}

static bool ui_but_is_listrow(const Button *but, const void * /*customdata*/)
{
  return but->type == ButtonType::ListRow;
}

Button *list_row_find_mouse_over(const ARegion *region, const int xy[2])
{
  return button_find_mouse_over_ex(region, xy, false, false, ui_but_is_listrow, nullptr);
}

struct ListRowFindIndexData {
  int index;
  Button *listbox;
};

static bool ui_but_is_listrow_at_index(const Button *but, const void *customdata)
{
  const ListRowFindIndexData *find_data = static_cast<const ListRowFindIndexData *>(customdata);

  return ui_but_is_listrow(but, nullptr) && ui_list_contains_row(find_data->listbox, but) &&
         (but->hardmax == find_data->index);
}

Button *list_row_find_index(const ARegion *region, const int index, Button *listbox)
{
  BLI_assert(listbox->type == ButtonType::ListBox);
  ListRowFindIndexData data = {};
  data.index = index;
  data.listbox = listbox;
  return ui_but_find(region, ui_but_is_listrow_at_index, &data);
}

static bool ui_but_is_view_item_fn(const Button *but, const void * /*customdata*/)
{
  return but->type == ButtonType::ViewItem;
}

Button *view_item_find_mouse_over(const ARegion *region, const int xy[2])
{
  return button_find_mouse_over_ex(region, xy, false, false, ui_but_is_view_item_fn, nullptr);
}

static bool ui_but_is_active_view_item(const Button *but, const void * /*customdata*/)
{
  if (but->type != ButtonType::ViewItem) {
    return false;
  }

  const auto *view_item_but = static_cast<const ButtonViewItem *>(but);
  return view_item_but->view_item->is_active();
}

Button *view_item_find_active(const ARegion *region)
{
  return ui_but_find(region, ui_but_is_active_view_item, nullptr);
}

Button *view_item_find_search_highlight(const ARegion *region)
{
  return ui_but_find(
      region,
      [](const Button *but, const void * /*find_custom_data*/) {
        if (but->type != ButtonType::ViewItem) {
          return false;
        }

        const auto *view_item_but = static_cast<const ButtonViewItem *>(but);
        return view_item_but->view_item->is_search_highlight();
      },
      nullptr);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Button (#Button) Relations
 * \{ */

Button *button_prev(Button *but)
{
  for (int idx = but->block->but_index(but) - 1; idx >= 0; idx--) {
    but = but->block->buttons[idx].get();
    if (button_is_editable(but)) {
      return but;
    }
  }
  return nullptr;
}

Button *button_next(Button *but)
{
  for (int i = but->block->but_index(but) + 1; i < but->block->buttons.size(); i++) {
    but = but->block->buttons[i].get();
    if (button_is_editable(but)) {
      return but;
    }
  }
  return nullptr;
}

Button *button_first(Block *block)
{
  for (const std::unique_ptr<Button> &but : block->buttons) {
    if (button_is_editable(but.get())) {
      return but.get();
    }
  }
  return nullptr;
}

Button *button_last(Block *block)
{
  for (int i = block->buttons.size() - 1; i >= 0; i--) {
    Button *but = block->buttons[i].get();
    if (button_is_editable(but)) {
      return but;
    }
  }
  return nullptr;
}

bool button_is_cursor_warp(const Button *but)
{
  if (U.uiflag & USER_CONTINUOUS_MOUSE) {
    if (ELEM(but->type,
             ButtonType::Num,
             ButtonType::NumSlider,
             ButtonType::TrackPreview,
             ButtonType::HsvCube,
             ButtonType::HsvCircle,
             ButtonType::Curve,
             ButtonType::CurveProfile))
    {
      return true;
    }
  }

  return false;
}

bool but_contains_password(const Button *but)
{
  return but->rnaprop && (RNA_property_subtype(but->rnaprop) == PROP_PASSWORD);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Button (#Button) Text
 * \{ */

size_t button_drawstr_len_without_sep_char(const Button *but)
{
  if (but->flag & BUT_HAS_SEP_CHAR) {
    const size_t sep_index = but->drawstr.find(UI_SEP_CHAR);
    if (sep_index != std::string::npos) {
      return sep_index;
    }
  }
  return but->drawstr.size();
}

StringRef button_drawstr_without_sep_char(const Button *but)
{
  size_t str_len_clip = button_drawstr_len_without_sep_char(but);
  return StringRef(but->drawstr).substr(0, str_len_clip);
}

size_t button_tip_len_only_first_line(const Button *but)
{
  if (but->tip == nullptr) {
    return 0;
  }
  const int64_t str_step = but->tip.find('\n');
  if (str_step == StringRef::not_found) {
    return but->tip.size();
  }
  return str_step;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Block (#Block) State
 * \{ */

Button *block_active_but_get(const Block *block)
{
  for (const std::unique_ptr<Button> &but : block->buttons) {
    if (but->active) {
      return but.get();
    }
  }

  return nullptr;
}

bool block_is_menu(const Block *block)
{
  return (((block->flag & BLOCK_LOOP) != 0) &&
          /* non-menu popups use keep-open, so check this is off */
          ((block->flag & BLOCK_KEEP_OPEN) == 0));
}

bool block_is_popover(const Block *block)
{
  return (block->flag & BLOCK_POPOVER) != 0;
}

bool block_is_pie_menu(const Block *block)
{
  return ((block->flag & BLOCK_PIE_MENU) != 0);
}

bool block_is_popup_any(const Block *block)
{
  return (block_is_menu(block) || block_is_popover(block) || block_is_pie_menu(block));
}

static const Button *ui_but_next_non_separator(const Button *but)
{
  if (!but) {
    return nullptr;
  }
  for (int i = but->block->but_index(but); i < but->block->buttons.size(); i++) {
    but = but->block->buttons[i].get();
    if (!ELEM(but->type, ButtonType::Sepr, ButtonType::SeprLine)) {
      return but;
    }
  }
  return nullptr;
}

bool block_is_empty_ex(const Block *block, const bool skip_title)
{
  const Button *but = block->first_but();
  if (skip_title) {
    /* Skip the first label, since popups often have a title,
     * we may want to consider the block empty in this case. */
    but = ui_but_next_non_separator(but);
    if (but && but->type == ButtonType::Label) {
      but = block->next_but(but);
    }
  }
  return (ui_but_next_non_separator(but) == nullptr);
}

bool block_is_empty(const Block *block)
{
  return block_is_empty_ex(block, false);
}

bool block_can_add_separator(const Block *block)
{
  if (block_is_menu(block) && !block_is_pie_menu(block)) {
    const Button *but = block->last_but();
    return (but && !ELEM(but->type, ButtonType::SeprLine, ButtonType::Sepr));
  }
  return true;
}

bool block_has_active_default_button(const Block *block)
{
  for (const std::unique_ptr<Button> &but : block->buttons) {
    if ((but->flag & BUT_ACTIVE_DEFAULT) && ((but->flag & UI_HIDDEN) == 0)) {
      return true;
    }
  }
  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Block (#Block) Spatial
 * \{ */

Block *block_find_mouse_over_ex(const ARegion *region, const int xy[2], bool only_clip)
{
  if (!region_contains_point_px(region, xy)) {
    return nullptr;
  }
  for (Block &block : region->runtime->uiblocks) {
    if (only_clip) {
      if ((block.flag & BLOCK_CLIP_EVENTS) == 0) {
        continue;
      }
    }
    float mx = xy[0], my = xy[1];
    window_to_block_fl(region, &block, &mx, &my);
    if (BLI_rctf_isect_pt(&block.rect, mx, my)) {
      return &block;
    }
  }
  return nullptr;
}

Block *block_find_mouse_over(const ARegion *region, const wmEvent *event, bool only_clip)
{
  return block_find_mouse_over_ex(region, event->xy, only_clip);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Region (#ARegion) State
 * \{ */

Button *region_find_active_but(ARegion *region)
{
  for (Block &block : region->runtime->uiblocks) {
    Button *but = block_active_but_get(&block);
    if (but) {
      return but;
    }
  }

  return nullptr;
}

Button *region_find_first_but_test_flag(ARegion *region, int flag_include, int flag_exclude)
{
  for (Block &block : region->runtime->uiblocks) {
    for (const std::unique_ptr<Button> &but : block.buttons) {
      if (((but->flag & flag_include) == flag_include) && ((but->flag & flag_exclude) == 0)) {
        return but.get();
      }
    }
  }

  return nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Region (#ARegion) Spatial
 * \{ */

bool region_contains_point_px(const ARegion *region, const int xy[2])
{
  rcti winrct;
  region_winrct_get_no_margin(region, &winrct);
  if (!BLI_rcti_isect_pt_v(&winrct, xy)) {
    return false;
  }

  /* also, check that with view2d, that the mouse is not over the scroll-bars
   * NOTE: care is needed here, since the mask rect may include the scroll-bars
   * even when they are not visible, so we need to make a copy of the mask to
   * use to check
   */
  if (region->v2d.mask.xmin != region->v2d.mask.xmax) {
    const View2D *v2d = &region->v2d;
    int mx = xy[0], my = xy[1];

    window_to_region(region, &mx, &my);
    if (!BLI_rcti_isect_pt(&v2d->mask, mx, my) ||
        view2d_mouse_in_scrollers(region, &region->v2d, xy))
    {
      return false;
    }
  }

  return true;
}

bool region_contains_rect_px(const ARegion *region, const rcti *rect_px)
{
  rcti winrct;
  region_winrct_get_no_margin(region, &winrct);
  if (!BLI_rcti_isect(&winrct, rect_px, nullptr)) {
    return false;
  }

  /* See comment in 'region_contains_point_px' */
  if (region->v2d.mask.xmin != region->v2d.mask.xmax) {
    const View2D *v2d = &region->v2d;
    rcti rect_region;
    window_to_region_rcti(region, &rect_region, rect_px);
    if (!BLI_rcti_isect(&v2d->mask, &rect_region, nullptr) ||
        view2d_rect_in_scrollers(region, &region->v2d, rect_px))
    {
      return false;
    }
  }

  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Screen (#bScreen) Spatial
 * \{ */

ARegion *screen_region_find_mouse_over_ex(bScreen *screen, const int xy[2])
{
  for (ARegion &region : screen->regionbase) {
    rcti winrct;

    region_winrct_get_no_margin(&region, &winrct);

    if (BLI_rcti_isect_pt_v(&winrct, xy)) {
      return &region;
    }
  }
  return nullptr;
}

ARegion *screen_region_find_mouse_over(bScreen *screen, const wmEvent *event)
{
  return screen_region_find_mouse_over_ex(screen, event->xy);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Manage Internal State
 * \{ */

void interface_tag_script_reload_queries()
{
  g_ot_tool_set_by_id = nullptr;
}

/** \} */

}  // namespace blender::ui
