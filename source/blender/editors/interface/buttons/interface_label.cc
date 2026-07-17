/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLF_api.hh"

#include "BLI_rect.hh"

#include "interface_intern.hh"
#include "interface_label.hh"

namespace blender::ui {

bool button_label_is_multiline(const Button *button)
{
  return button->type == ButtonType::Label &&
         static_cast<const ButtonLabel *>(button)->is_multiline;
}

void label_multiline_wrap_lines(ButtonLabel *button, int icon_pad)
{
  const float aspect = button->block->aspect;

  const int width = std::max<int>(std::ceil(BLI_rctf_size_x(&button->rect)) - icon_pad, 0) /
                    aspect;
  StringRef text = button->str;
  text = text.trim();
  /* Sometimes blocks are updated from old blocks before resolving the layout, so the button could
   * have acquire old button wrap cache. */
  if (button->wrap_cache) {
    TextWrapCache &cache = *button->wrap_cache;
    if (!(cache.aspect == aspect && cache.wrap_width == width && cache.text == text)) {
      button->wrap_cache.reset();
    }
  }
  if (!button->wrap_cache && button->block->oldblock) {
    int i = 0;
    for (std::shared_ptr<TextWrapCache> &cache_ptr : button->block->oldblock->text_wrap_cache) {
      TextWrapCache &cache = *cache_ptr;
      if (cache.aspect == aspect && cache.wrap_width == width && cache.text == text) {
        button->wrap_cache = cache_ptr;
        break;
      }
      i++;
    }
    if (button->wrap_cache) {
      button->block->oldblock->text_wrap_cache.remove(i);
    }
  }
  if (button->wrap_cache) {
    button->block->text_wrap_cache.append(button->wrap_cache);
    return;
  }
  button->wrap_cache = std::make_unique<TextWrapCache>();
  TextWrapCache &cache = *button->wrap_cache;
  cache.text = text;
  cache.wrap_width = width;
  cache.aspect = aspect;

  button->block->text_wrap_cache.append(button->wrap_cache);
  uiFontStyle fstyle = style_get()->widget;
  fontscale(&fstyle.points, aspect);
  fontstyle_set(&fstyle);
  cache.wrapped_lines = BLF_string_wrap(
      fstyle.uifont_id, cache.text, width, BLFWrapMode::HardLimit);
}

}  // namespace blender::ui
