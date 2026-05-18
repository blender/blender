/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_screen.hh"

#include "BLF_api.hh"

#include "BLI_listbase.h"
#include "BLI_listbase_iterator.hh"
#include "BLI_rect.h"
#include "BLI_string.h"

#include "DNA_screen_types.h"

#include "UI_interface_c.hh"

#include "interface_intern.hh"
#include "interface_textbox.hh"

namespace blender::ui {

void invalidate_text_wrap_cache(const ARegion &region)
{
  for (Block &block : region.runtime->uiblocks) {
    for (Button &button : block.buttons()) {
      if (button.type != ButtonType::TextBox) {
        continue;
      }
      ButtonTextBox &textbox = static_cast<ButtonTextBox &>(button);
      textbox.wrap_cache.reset();
      textbox.placeholder_wrap_cache.reset();
    }
  }
}

void textbox_add_scroll(ButtonTextBox *textbox, int step)
{
  textbox_wrap_lines(textbox);
  textbox->line_scroll_set(textbox->line_scroll() + step);
}

void textbox_scroll_to_cursor(ButtonTextBox *textbox)
{
  const Vector<StringRef> lines = textbox_wrap_lines(textbox);
  int line_cursor = 0;
  int but_pos = textbox->pos;
#ifdef WITH_INPUT_IME
  /* Include the ime composition string when scrolling to the cursor. */
  const wmIMEData *ime_data = button_ime_data_get(textbox);
  if (ime_data && ime_data->composite.size() && ime_data->cursor_pos != -1) {
    but_pos += ime_data->cursor_pos;
  }
#endif
  const char *cursor = lines[0].begin() + but_pos;
  for (const StringRef line : lines) {
    if (line.begin() > cursor) {
      line_cursor = std::max(0, line_cursor - 1);
      break;
    }
    line_cursor++;
  }
  const int visible_bounds[] = {textbox->line_scroll(),
                                textbox->line_scroll() + textbox->visible_lines()};
  if (visible_bounds[0] <= line_cursor && line_cursor < visible_bounds[1]) {
    return;
  }
  if (visible_bounds[0] > line_cursor) {
    textbox_add_scroll(textbox, line_cursor - visible_bounds[0]);
  }
  else {
    textbox_add_scroll(textbox, line_cursor - visible_bounds[1] + 1);
  }
}

void textbox_textedit_set_cursor_pos(ButtonTextBox *textbox,
                                     const ARegion *region,
                                     const float2 xy)
{

  /* Don't include grip bounds when selecting text with the mouse. */
  float2 start = {textbox->rect.xmin, textbox->rect.ymin};
  float2 end = {textbox->rect.xmax, textbox->rect.ymax};

  block_to_window_fl(region, textbox->block, &start.x, &start.y);
  block_to_window_fl(region, textbox->block, &end.x, &end.y);

  start.y += textbox_vertical_padding() / textbox->block->aspect;
  end.y -= textbox_vertical_padding() / textbox->block->aspect;

  const Vector<StringRef> lines = textbox_wrap_lines(textbox);
  uiFontStyle fstyle = style_get()->widget;
  const float aspect = textbox->block->aspect;
  fontscale(&fstyle.points, aspect);
  fontstyle_set(&fstyle);
  int line_under_mouse = textbox->line_scroll() +
                         (end.y - xy.y) / (end.y - start.y) * (textbox->visible_lines());
  line_under_mouse = std::clamp<int>(line_under_mouse,
                                     textbox->line_scroll(),
                                     textbox->line_scroll() + textbox->visible_lines() - 1);
  line_under_mouse = std::clamp<int>(line_under_mouse, 0, lines.size() - 1);

  const StringRef line = lines[line_under_mouse];

  start.x -= U.pixelsize / aspect;
  if (!(textbox->drawflag & BUT_NO_TEXT_PADDING)) {
    start.x += button_text_padding(textbox);
  }
  const int offset = BLF_str_offset_from_cursor_position(
      fstyle.uifont_id, line.data(), line.size(), int(xy.x - start.x));
  int position = line.begin() - lines[0].data() + offset;
#ifdef WITH_INPUT_IME
  /* Textbox text wrap includes the IME composition string, remove the ime string pad from the
   * selection.
   */
  const wmIMEData *ime_data = button_ime_data_get(textbox);
  if (ime_data && position > int(textbox->pos)) {
    position = std::max<int>(int(textbox->pos), position - int(ime_data->composite.size()));
  }
#endif
  textbox->pos = position;
  /* Do not scroll to cursor now, wait the HandleButtonData::text_select_auto_scroll timer or the
   * #LEFTMOUSE release event for scrolling to the cursor. */
}

int textbox_wrapped_line_index_from_char_offset(Span<StringRef> lines, int offset)
{
  const char *dest = lines.first().begin() + offset;
  int i = 0;
  for (const StringRef line : lines) {
    if (line.begin() > dest) {
      i = i - 1;
      break;
    }
    i++;
  }
  i = std::clamp<int>(i, 0, lines.size() - 1);
  return i;
}

void textbox_jump_line(ButtonTextBox *textbox,
                       eStrCursorJumpDirection direction,
                       const bool select)
{
  button_update(textbox);
  if (textbox->selend == textbox->selsta) {
    textbox->selsta = textbox->selend = textbox->pos;
  }
  const Vector<StringRef> lines = textbox_wrap_lines(textbox);
  const char *str = lines.first().begin();
  const bool append_selection = textbox->selend == textbox->pos;
  const int line_cursor = textbox_wrapped_line_index_from_char_offset(lines, textbox->pos);
  uiFontStyle fstyle = style_get()->widget;
  fontscale(&fstyle.points, textbox->block->aspect);
  fontstyle_set(&fstyle);
  const int fontid = fstyle.uifont_id;
  int offset = BLF_str_offset_to_cursor(fontid,
                                        lines[line_cursor].begin(),
                                        lines[line_cursor].size(),
                                        textbox->pos - (lines[line_cursor].begin() - str),
                                        0);
  StringRef dest_line = nullptr;
  if (direction == STRCUR_DIR_NEXT) {
    if (line_cursor == lines.size() - 1) {
      textbox->pos = lines.last().end() - str;
    }
    else {
      dest_line = lines[line_cursor + 1];
    }
  }
  else {
    if (line_cursor == 0) {
      textbox->pos = 0;
    }
    else {
      dest_line = lines[line_cursor - 1];
    }
  }
  if (dest_line.data()) {
    textbox->pos = dest_line.begin() - str +
                   BLF_str_offset_from_cursor_position(
                       fontid, dest_line.data(), dest_line.size(), offset);
  }
  if (!select) {
    textbox->selsta = textbox->selend = textbox->pos;
    return;
  }

  if (append_selection) {
    textbox->selend = textbox->pos;
  }
  else {
    textbox->selsta = textbox->pos;
  }
  if (textbox->selend < textbox->selsta) {
    std::swap(textbox->selend, textbox->selsta);
  }
}

Vector<StringRef> textbox_wrap_lines(ButtonTextBox *textbox)
{
  uiFontStyle fstyle = style_get()->widget;
  const float aspect = textbox->block->aspect;
  const int width = std::max<int>(std::ceil(BLI_rctf_size_x(&textbox->rect) -
                                            2.0f * UI_TEXT_MARGIN_X * float(U.widget_unit) - 2.0f),
                                  0) /
                    aspect;
  StringRef text = textbox->drawstr;
#ifdef WITH_INPUT_IME
  const wmIMEData *ime_data = button_ime_data_get(textbox);
  if (ime_data && ime_data->composite.size() > 0) {
    StringRef edit_str = textbox->editstr;
    StringRef l = edit_str.is_empty() ? StringRef("") : edit_str.substr(0, textbox->pos);
    StringRef r = edit_str.is_empty() ? StringRef("") : edit_str.substr(textbox->pos);
    StringRef ime_str = ime_data->composite;
    textbox->drawstr = fmt::format("{}{}{}", l, ime_str, r);
    text = textbox->drawstr;
  }
  else
#endif
      if (textbox->editstr)
  {
    text = textbox->editstr;
  }
  if (!textbox->wrap_cache) {
    textbox->wrap_cache = std::make_unique<TextWrapCache>();
  }
  TextWrapCache &cache = *textbox->wrap_cache;
  if (cache.aspect == aspect && cache.wrap_width == width && text == cache.text) {
    textbox->last_total_lines = cache.wrapped_lines.size();
    return cache.wrapped_lines;
  }
  cache.text = text;
  text = cache.text;
  cache.wrap_width = width;
  cache.aspect = aspect;

  fontscale(&fstyle.points, aspect);
  fontstyle_set(&fstyle);
  Vector<StringRef> lines = BLF_string_wrap(
      fstyle.uifont_id, text, width, BLFWrapMode::HardLimit | BLFWrapMode::Typographical);
  if (lines.is_empty()) {
    lines.append(text);
  }
  /* Add empty trailing line to put cursor in a new line. */
  if (text.endswith("\n")) {
    lines.append(StringRef(text.end(), text.end()));
  }
  textbox->last_total_lines = lines.size();

  /* WORKAROUND: Textbox event handling and drawing requires lines to not include line breaks, but
   * sometimes text wrap adds them and other times not. */
  for (int i : lines.index_range()) {
    if (lines[i].endswith("\n")) {
      lines[i] = lines[i].drop_suffix(1);
    }
  }

  cache.wrapped_lines = lines;

  return lines;
}

Vector<StringRef> textbox_wrap_placeholder(ButtonTextBox *textbox)
{
  BLI_assert(textbox->placeholder);
  uiFontStyle fstyle = style_get()->widget;
  const float aspect = textbox->block->aspect;
  const int width = std::max<int>(std::ceil(BLI_rctf_size_x(&textbox->rect) -
                                            2.0f * UI_TEXT_MARGIN_X * float(U.widget_unit) - 2.0f),
                                  0) /
                    aspect;
  StringRef text = textbox->placeholder;

  if (!textbox->placeholder_wrap_cache) {
    textbox->placeholder_wrap_cache = std::make_unique<TextWrapCache>();
  }
  TextWrapCache &cache = *textbox->placeholder_wrap_cache;
  if (cache.aspect == aspect && cache.wrap_width == width && text == cache.text) {
    return cache.wrapped_lines;
  }
  cache.text = text;
  cache.wrap_width = width;
  cache.aspect = aspect;

  fontscale(&fstyle.points, aspect);
  fontstyle_set(&fstyle);

  cache.wrapped_lines = BLF_string_wrap(
      fstyle.uifont_id, cache.text, width, BLFWrapMode::HardLimit | BLFWrapMode::Typographical);

  return cache.wrapped_lines;
}

float textbox_grip_height()
{
  return UI_UNIT_Y * 0.55f;
}

void ButtonTextBox::line_scroll_set(int line_scroll)
{
  this->state->scroll = line_scroll;
  /* Clamp stored value. */
  this->state->scroll = this->line_scroll();
}

float textbox_vertical_padding()
{
  /* Allow aligning text buttons with single line text-box buttons. */
  return float(UI_UNIT_Y - fontstyle_height_max(UI_FSTYLE_WIDGET)) / 2.0f;
}

TextboxState *textbox_ensure_state(ARegion *region, StringRefNull idname)
{
  for (uiTextboxStateLink &link : region->textbox_states) {
    if (link.idname == idname) {
      return &link.state;
    }
  }
  uiTextboxStateLink *link = MEM_new<uiTextboxStateLink>(__func__);
  link->idname = BLI_strdupn(idname.data(), idname.size());
  link->state.visible_lines = textbox_minimum_visible_lines;
  BLI_addtail(&region->textbox_states, link);
  return &link->state;
}

int ButtonTextBox::line_scroll() const
{
  const int max_scroll = std::max(this->last_total_lines - this->visible_lines(), 0);
  return std::clamp(this->state->scroll, 0, max_scroll);
}

int ButtonTextBox::visible_lines() const
{
  return std::max<int>(this->state->visible_lines, textbox_minimum_visible_lines);
}

}  // namespace blender::ui
