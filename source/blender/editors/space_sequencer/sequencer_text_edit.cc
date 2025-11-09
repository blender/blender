/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spseq
 */

#include <cstddef>

#include "DNA_sequence_types.h"

#include "BLI_math_matrix.hh"
#include "BLI_math_vector.hh"
#include "BLI_string.h"
#include "BLI_string_utf8.h"

#include "BKE_context.hh"
#include "BKE_scene.hh"

#include "SEQ_effects.hh"
#include "SEQ_relations.hh"
#include "SEQ_select.hh"
#include "SEQ_time.hh"
#include "SEQ_transform.hh"

#include "WM_api.hh"

#include "RNA_define.hh"

#include "UI_view2d.hh"

#include "ED_screen.hh"

/* Own include. */
#include "sequencer_intern.hh"

namespace blender::ed::vse {

static bool sequencer_text_editing_poll(bContext *C)
{
  if (!sequencer_editing_initialized_and_active(C)) {
    return false;
  }
  const Scene *scene = CTX_data_sequencer_scene(C);
  if (!scene) {
    return false;
  }

  const Strip *strip = seq::select_active_get(scene);
  if (strip == nullptr || strip->type != STRIP_TYPE_TEXT || !seq::effects_can_render_text(strip)) {
    return false;
  }

  const TextVars *data = static_cast<TextVars *>(strip->effectdata);
  if (data == nullptr || data->runtime == nullptr) {
    return false;
  }

  return true;
}

bool sequencer_text_editing_active_poll(bContext *C)
{
  const Scene *scene = CTX_data_sequencer_scene(C);
  if (!scene) {
    return false;
  }
  const Strip *strip = seq::select_active_get(scene);
  if (strip == nullptr || !sequencer_text_editing_poll(C)) {
    return false;
  }

  if (ED_screen_animation_no_scrub(CTX_wm_manager(C))) {
    return false;
  }

  if (!seq::time_strip_intersects_frame(scene, strip, BKE_scene_frame_get(scene))) {
    return false;
  }

  return (strip->flag & SEQ_FLAG_TEXT_EDITING_ACTIVE) != 0;
}

int2 strip_text_cursor_offset_to_position(const TextVarsRuntime *text, int cursor_offset)
{
  cursor_offset = std::clamp(cursor_offset, 0, text->character_count);

  int2 cursor_position{0, 0};
  for (const seq::LineInfo &line : text->lines) {
    if (cursor_offset < line.characters.size()) {
      cursor_position.x = cursor_offset;
      break;
    }
    cursor_offset -= line.characters.size();
    cursor_position.y += 1;
  }

  cursor_position.y = std::clamp(cursor_position.y, 0, int(text->lines.size() - 1));
  cursor_position.x = std::clamp(
      cursor_position.x, 0, int(text->lines[cursor_position.y].characters.size() - 1));

  return cursor_position;
}

static const seq::CharInfo &character_at_cursor_pos_get(const TextVarsRuntime *text,
                                                        const int2 cursor_pos)
{
  return text->lines[cursor_pos.y].characters[cursor_pos.x];
}

static const seq::CharInfo &character_at_cursor_offset_get(const TextVarsRuntime *text,
                                                           const int cursor_offset)
{
  const int2 cursor_pos = strip_text_cursor_offset_to_position(text, cursor_offset);
  return character_at_cursor_pos_get(text, cursor_pos);
}

static int cursor_position_to_offset(const TextVarsRuntime *text, int2 cursor_position)
{
  return character_at_cursor_pos_get(text, cursor_position).index;
}

static void text_selection_cancel(TextVars *data)
{
  data->selection_start_offset = 0;
  data->selection_end_offset = 0;
}

IndexRange strip_text_selection_range_get(const TextVars *data)
{
  /* Ensure, that selection start < selection end. */
  int sel_start_offset = data->selection_start_offset;
  int sel_end_offset = data->selection_end_offset;
  if (sel_start_offset > sel_end_offset) {
    std::swap(sel_start_offset, sel_end_offset);
  }

  return IndexRange(sel_start_offset, sel_end_offset - sel_start_offset);
}

static bool text_has_selection(const TextVars *data)
{
  return !strip_text_selection_range_get(data).is_empty();
}

static void delete_selected_text(TextVars *data)
{
  if (!text_has_selection(data)) {
    return;
  }

  TextVarsRuntime *text = data->runtime;
  IndexRange sel_range = strip_text_selection_range_get(data);

  seq::CharInfo char_start = character_at_cursor_offset_get(text, sel_range.first());
  seq::CharInfo char_end = character_at_cursor_offset_get(text, sel_range.last());

  const int offset_start = char_start.offset;
  const int offset_end = char_end.offset + char_end.byte_length;
  BLI_assert(offset_start >= 0 && offset_end <= data->text_len_bytes);
  BLI_assert(offset_end >= 0 && offset_end <= data->text_len_bytes);
  BLI_assert(offset_start <= offset_end);
  const int remaining = data->text_len_bytes - offset_end;

  std::memmove(data->text_ptr + offset_start, data->text_ptr + offset_end, remaining + 1);
  data->text_len_bytes = offset_start + remaining;

  const int2 sel_start = strip_text_cursor_offset_to_position(text, sel_range.first());
  data->cursor_offset = cursor_position_to_offset(text, sel_start);
  text_selection_cancel(data);
}

static void text_editing_update(const bContext *C)
{
  Strip *strip = seq::select_active_get(CTX_data_sequencer_scene(C));
  seq::relations_invalidate_cache_raw(CTX_data_sequencer_scene(C), strip);
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, CTX_data_sequencer_scene(C));
}

enum {
  LINE_BEGIN,
  LINE_END,
  TEXT_BEGIN,
  TEXT_END,
  PREV_CHAR,
  NEXT_CHAR,
  PREV_WORD,
  NEXT_WORD,
  PREV_LINE,
  NEXT_LINE,
};

static const EnumPropertyItem move_type_items[] = {
    {LINE_BEGIN, "LINE_BEGIN", 0, "Line Begin", ""},
    {LINE_END, "LINE_END", 0, "Line End", ""},
    {TEXT_BEGIN, "TEXT_BEGIN", 0, "Text Begin", ""},
    {TEXT_END, "TEXT_END", 0, "Text End", ""},
    {PREV_CHAR, "PREVIOUS_CHARACTER", 0, "Previous Character", ""},
    {NEXT_CHAR, "NEXT_CHARACTER", 0, "Next Character", ""},
    {PREV_WORD, "PREVIOUS_WORD", 0, "Previous Word", ""},
    {NEXT_WORD, "NEXT_WORD", 0, "Next Word", ""},
    {PREV_LINE, "PREVIOUS_LINE", 0, "Previous Line", ""},
    {NEXT_LINE, "NEXT_LINE", 0, "Next Line", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static int2 cursor_move_by_character(int2 cursor_position, const TextVarsRuntime *text, int offset)
{
  const seq::LineInfo &cur_line = text->lines[cursor_position.y];
  /* Move to next line. */
  if (cursor_position.x + offset > cur_line.characters.size() - 1 &&
      cursor_position.y < text->lines.size() - 1)
  {
    cursor_position.x = 0;
    cursor_position.y++;
  }
  /* Move to previous line. */
  else if (cursor_position.x + offset < 0 && cursor_position.y > 0) {
    cursor_position.y--;
    cursor_position.x = text->lines[cursor_position.y].characters.size() - 1;
  }
  else {
    cursor_position.x += offset;
    const int position_max = text->lines[cursor_position.y].characters.size() - 1;
    cursor_position.x = std::clamp(cursor_position.x, 0, position_max);
  }
  return cursor_position;
}

static int2 cursor_move_by_line(int2 cursor_position, const TextVarsRuntime *text, int offset)
{
  const seq::LineInfo &cur_line = text->lines[cursor_position.y];
  const int cur_pos_x = cur_line.characters[cursor_position.x].position.x;

  const int line_max = text->lines.size() - 1;
  const int new_line_index = std::clamp(cursor_position.y + offset, 0, line_max);
  const seq::LineInfo &new_line = text->lines[new_line_index];

  if (cursor_position.y == new_line_index) {
    return cursor_position;
  }

  /* Find character in another line closest to current position. */
  int best_distance = std::numeric_limits<int>::max();
  int best_character_index = 0;

  for (int i : new_line.characters.index_range()) {
    seq::CharInfo character = new_line.characters[i];
    const int distance = std::abs(character.position.x - cur_pos_x);
    if (distance < best_distance) {
      best_distance = distance;
      best_character_index = i;
    }
  }

  cursor_position.x = best_character_index;
  cursor_position.y = new_line_index;
  return cursor_position;
}

static int2 cursor_move_line_end(int2 cursor_position, const TextVarsRuntime *text)
{
  const seq::LineInfo &cur_line = text->lines[cursor_position.y];
  cursor_position.x = cur_line.characters.size() - 1;
  return cursor_position;
}

static bool is_whitespace_transition(char chr1, char chr2)
{
  return ELEM(chr1, ' ', '\t', '\n') && !ELEM(chr2, ' ', '\t', '\n');
}

static int2 cursor_move_prev_word(int2 cursor_position,
                                  const TextVarsRuntime *text,
                                  const char *text_ptr)
{
  cursor_position = cursor_move_by_character(cursor_position, text, -1);

  while (cursor_position.x > 0 || cursor_position.y > 0) {
    const seq::CharInfo character = character_at_cursor_pos_get(text, cursor_position);
    const int2 prev_cursor_pos = cursor_move_by_character(cursor_position, text, -1);
    const seq::CharInfo prev_character = character_at_cursor_pos_get(text, prev_cursor_pos);

    if (is_whitespace_transition(text_ptr[prev_character.offset], text_ptr[character.offset])) {
      break;
    }
    cursor_position = prev_cursor_pos;
  }
  return cursor_position;
}

static int2 cursor_move_next_word(int2 cursor_position,
                                  const TextVarsRuntime *text,
                                  const char *text_ptr)
{
  const int maxline = text->lines.size() - 1;
  const int maxchar = text->lines.last().characters.size() - 1;

  while ((cursor_position.x < maxchar) || (cursor_position.y < maxline)) {
    const seq::CharInfo character = character_at_cursor_pos_get(text, cursor_position);
    cursor_position = cursor_move_by_character(cursor_position, text, 1);
    const seq::CharInfo next_character = character_at_cursor_pos_get(text, cursor_position);

    if (is_whitespace_transition(text_ptr[next_character.offset], text_ptr[character.offset])) {
      break;
    }
  }
  return cursor_position;
}

static wmOperatorStatus sequencer_text_cursor_move_exec(bContext *C, wmOperator *op)
{
  const Strip *strip = seq::select_active_get(CTX_data_sequencer_scene(C));
  TextVars *data = static_cast<TextVars *>(strip->effectdata);
  const TextVarsRuntime *text = data->runtime;

  if (RNA_boolean_get(op->ptr, "select_text") && !text_has_selection(data)) {
    data->selection_start_offset = data->cursor_offset;
  }

  int2 cursor_position = strip_text_cursor_offset_to_position(text, data->cursor_offset);

  switch (RNA_enum_get(op->ptr, "type")) {
    case PREV_CHAR:
      cursor_position = cursor_move_by_character(cursor_position, text, -1);
      break;
    case NEXT_CHAR:
      cursor_position = cursor_move_by_character(cursor_position, text, 1);
      break;
    case PREV_LINE:
      cursor_position = cursor_move_by_line(cursor_position, text, -1);
      break;
    case NEXT_LINE:
      cursor_position = cursor_move_by_line(cursor_position, text, 1);
      break;
    case LINE_BEGIN:
      cursor_position.x = 0;
      break;
    case LINE_END:
      cursor_position = cursor_move_line_end(cursor_position, text);
      break;
    case TEXT_BEGIN:
      cursor_position = {0, 0};
      break;
    case TEXT_END:
      cursor_position.y = text->lines.size() - 1;
      cursor_position = cursor_move_line_end(cursor_position, text);
      break;
    case PREV_WORD:
      cursor_position = cursor_move_prev_word(cursor_position, text, data->text_ptr);
      break;
    case NEXT_WORD:
      cursor_position = cursor_move_next_word(cursor_position, text, data->text_ptr);
      break;
  }

  data->cursor_offset = cursor_position_to_offset(text, cursor_position);
  if (RNA_boolean_get(op->ptr, "select_text")) {
    data->selection_end_offset = data->cursor_offset;
  }

  if (!RNA_boolean_get(op->ptr, "select_text") ||
      data->cursor_offset == data->selection_start_offset)
  {
    text_selection_cancel(data);
  }

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, CTX_data_sequencer_scene(C));
  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_text_cursor_move(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Move Cursor";
  ot->description = "Move cursor in text";
  ot->idname = "SEQUENCER_OT_text_cursor_move";

  /* API callbacks. */
  ot->exec = sequencer_text_cursor_move_exec;
  ot->poll = sequencer_text_editing_active_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_enum(ot->srna,
               "type",
               move_type_items,
               LINE_BEGIN,
               "Type",
               "Where to move cursor to, to make a selection");

  PropertyRNA *prop = RNA_def_boolean(
      ot->srna, "select_text", false, "Select Text", "Select text while moving cursor");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

static bool text_insert(TextVars *data, const char *buf, const size_t buf_len)
{
  BLI_assert(strlen(buf) == buf_len);
  const TextVarsRuntime *text = data->runtime;

  delete_selected_text(data);

  size_t needed_size = data->text_len_bytes + buf_len + 1;
  char *new_text = MEM_malloc_arrayN<char>(needed_size, "text");

  const seq::CharInfo cur_char = character_at_cursor_offset_get(text, data->cursor_offset);
  BLI_assert(cur_char.offset >= 0 && cur_char.offset <= data->text_len_bytes);
  std::memcpy(new_text, data->text_ptr, cur_char.offset);
  std::memcpy(new_text + cur_char.offset, buf, buf_len);
  std::memcpy(new_text + cur_char.offset + buf_len,
              data->text_ptr + cur_char.offset,
              data->text_len_bytes - cur_char.offset + 1);
  data->text_len_bytes += buf_len;
  MEM_freeN(data->text_ptr);
  data->text_ptr = new_text;

  data->cursor_offset += 1;
  return true;
}

static wmOperatorStatus sequencer_text_insert_exec(bContext *C, wmOperator *op)
{
  const Strip *strip = seq::select_active_get(CTX_data_sequencer_scene(C));
  TextVars *data = static_cast<TextVars *>(strip->effectdata);

  char str[512];
  RNA_string_get(op->ptr, "string", str);

  const size_t in_buf_len = STRNLEN(str);
  if (in_buf_len == 0) {
    return OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH;
  }

  if (!text_insert(data, str, in_buf_len)) {
    return OPERATOR_CANCELLED;
  }

  text_editing_update(C);
  return OPERATOR_FINISHED;
}

static wmOperatorStatus sequencer_text_insert_invoke(bContext *C,
                                                     wmOperator *op,
                                                     const wmEvent *event)
{
  char str[6];
  BLI_strncpy_utf8(str, event->utf8_buf, BLI_str_utf8_size_safe(event->utf8_buf) + 1);
  RNA_string_set(op->ptr, "string", str);
  return sequencer_text_insert_exec(C, op);
}

void SEQUENCER_OT_text_insert(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Insert Character";
  ot->description = "Insert text at cursor position";
  ot->idname = "SEQUENCER_OT_text_insert";

  /* API callbacks. */
  ot->exec = sequencer_text_insert_exec;
  ot->invoke = sequencer_text_insert_invoke;
  ot->poll = sequencer_text_editing_active_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO;

  /* properties */
  RNA_def_string(
      ot->srna, "string", nullptr, 512, "String", "String to be inserted at cursor position");
}

enum { DEL_NEXT_SEL, DEL_PREV_SEL };
static const EnumPropertyItem delete_type_items[] = {
    {DEL_NEXT_SEL, "NEXT_OR_SELECTION", 0, "Next or Selection", ""},
    {DEL_PREV_SEL, "PREVIOUS_OR_SELECTION", 0, "Previous or Selection", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static void delete_character(const seq::CharInfo character, TextVars *data)
{
  const int offset_start = character.offset;
  const int offset_end = character.offset + character.byte_length;
  BLI_assert(offset_start >= 0 && offset_start <= data->text_len_bytes);
  BLI_assert(offset_end >= 0 && offset_end <= data->text_len_bytes);
  const int remaining = data->text_len_bytes - offset_end + 1;
  std::memmove(data->text_ptr + offset_start, data->text_ptr + offset_end, remaining);
  data->text_len_bytes -= character.byte_length;
  BLI_assert(data->text_len_bytes >= 0);
}

static wmOperatorStatus sequencer_text_delete_exec(bContext *C, wmOperator *op)
{
  const Strip *strip = seq::select_active_get(CTX_data_sequencer_scene(C));
  TextVars *data = static_cast<TextVars *>(strip->effectdata);
  const TextVarsRuntime *text = data->runtime;
  const int type = RNA_enum_get(op->ptr, "type");

  if (text_has_selection(data)) {
    delete_selected_text(data);
    text_editing_update(C);
    return OPERATOR_FINISHED;
  }

  if (type == DEL_NEXT_SEL) {
    if (data->cursor_offset >= text->character_count) {
      return OPERATOR_CANCELLED;
    }

    delete_character(character_at_cursor_offset_get(text, data->cursor_offset), data);
  }
  if (type == DEL_PREV_SEL) {
    if (data->cursor_offset == 0) {
      return OPERATOR_CANCELLED;
    }

    delete_character(character_at_cursor_offset_get(text, data->cursor_offset - 1), data);
    data->cursor_offset -= 1;
  }

  text_editing_update(C);
  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_text_delete(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Delete Character";
  ot->description = "Delete text at cursor position";
  ot->idname = "SEQUENCER_OT_text_delete";

  /* API callbacks. */
  ot->exec = sequencer_text_delete_exec;
  ot->poll = sequencer_text_editing_active_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO;

  /* properties */
  RNA_def_enum(ot->srna,
               "type",
               delete_type_items,
               DEL_NEXT_SEL,
               "Type",
               "Which part of the text to delete");
}

static wmOperatorStatus sequencer_text_line_break_exec(bContext *C, wmOperator * /*op*/)
{
  const Strip *strip = seq::select_active_get(CTX_data_sequencer_scene(C));
  TextVars *data = static_cast<TextVars *>(strip->effectdata);

  if (!text_insert(data, "\n", 1)) {
    return OPERATOR_CANCELLED;
  }

  text_editing_update(C);
  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_text_line_break(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Insert Line Break";
  ot->description = "Insert line break at cursor position";
  ot->idname = "SEQUENCER_OT_text_line_break";

  /* API callbacks. */
  ot->exec = sequencer_text_line_break_exec;
  ot->poll = sequencer_text_editing_active_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO;
}

static wmOperatorStatus sequencer_text_select_all_exec(bContext *C, wmOperator * /*op*/)
{
  const Strip *strip = seq::select_active_get(CTX_data_sequencer_scene(C));
  TextVars *data = static_cast<TextVars *>(strip->effectdata);
  data->selection_start_offset = 0;
  data->selection_end_offset = data->runtime->character_count;
  text_editing_update(C);
  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_text_select_all(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select All";
  ot->description = "Select all characters";
  ot->idname = "SEQUENCER_OT_text_select_all";

  /* API callbacks. */
  ot->exec = sequencer_text_select_all_exec;
  ot->poll = sequencer_text_editing_active_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO;
}

static wmOperatorStatus sequencer_text_deselect_all_exec(bContext *C, wmOperator * /*op*/)
{
  Strip *strip = seq::select_active_get(CTX_data_sequencer_scene(C));
  TextVars *data = static_cast<TextVars *>(strip->effectdata);

  if (!text_has_selection(data)) {
    /* Exit edit mode, so text can be translated by mouse. */
    strip->flag &= ~SEQ_FLAG_TEXT_EDITING_ACTIVE;
  }
  else {
    text_selection_cancel(data);
  }

  text_editing_update(C);
  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_text_deselect_all(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Deselect All";
  ot->description = "Deselect all characters";
  ot->idname = "SEQUENCER_OT_text_deselect_all";

  /* API callbacks. */
  ot->exec = sequencer_text_deselect_all_exec;
  ot->poll = sequencer_text_editing_active_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO;
}

static wmOperatorStatus sequencer_text_edit_mode_toggle_exec(bContext *C, wmOperator * /*op*/)
{
  Strip *strip = seq::select_active_get(CTX_data_sequencer_scene(C));
  if (sequencer_text_editing_active_poll(C)) {
    strip->flag &= ~SEQ_FLAG_TEXT_EDITING_ACTIVE;
  }
  else {
    strip->flag |= SEQ_FLAG_TEXT_EDITING_ACTIVE;
  }

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, CTX_data_sequencer_scene(C));
  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_text_edit_mode_toggle(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Edit Text";
  ot->description = "Toggle text editing";
  ot->idname = "SEQUENCER_OT_text_edit_mode_toggle";

  /* API callbacks. */
  ot->exec = sequencer_text_edit_mode_toggle_exec;
  ot->poll = sequencer_text_editing_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO;
}

static int find_closest_cursor_offset(const TextVars *data, float2 mouse_loc)
{
  const TextVarsRuntime *text = data->runtime;
  int best_cursor_offset = 0;
  float best_distance = std::numeric_limits<float>::max();

  for (const seq::LineInfo &line : text->lines) {
    for (const seq::CharInfo &character : line.characters) {
      const float distance = math::distance(mouse_loc, character.position);
      if (distance < best_distance) {
        best_distance = distance;
        best_cursor_offset = character.index;
      }
    }
  }

  return best_cursor_offset;
}

static void cursor_set_by_mouse_position(const bContext *C, const wmEvent *event)
{
  const Scene *scene = CTX_data_sequencer_scene(C);
  const Strip *strip = seq::select_active_get(scene);
  TextVars *data = static_cast<TextVars *>(strip->effectdata);
  const View2D *v2d = UI_view2d_fromcontext(C);

  int2 mval_region;
  WM_event_drag_start_mval(event, CTX_wm_region(C), mval_region);
  float2 mouse_loc;
  UI_view2d_region_to_view(v2d, mval_region.x, mval_region.y, &mouse_loc.x, &mouse_loc.y);

  /* Convert cursor coordinates to domain of CharInfo::position. */
  const float2 view_offs{-scene->r.xsch / 2.0f, -scene->r.ysch / 2.0f};
  const float view_aspect = scene->r.xasp / scene->r.yasp;
  float3x3 transform_mat = seq::image_transform_matrix_get(CTX_data_sequencer_scene(C), strip);
  // MSVC 2019 can't decide here for some reason, pick the template for it.
  transform_mat = blender::math::invert<float, 3>(transform_mat);

  mouse_loc.x /= view_aspect;
  mouse_loc = math::transform_point(transform_mat, mouse_loc);
  mouse_loc -= view_offs;
  data->cursor_offset = find_closest_cursor_offset(data, float2(mouse_loc));
}

static wmOperatorStatus sequencer_text_cursor_set_modal(bContext *C,
                                                        wmOperator * /*op*/,
                                                        const wmEvent *event)
{
  const Scene *scene = CTX_data_sequencer_scene(C);
  const Strip *strip = seq::select_active_get(scene);
  TextVars *data = static_cast<TextVars *>(strip->effectdata);
  bool make_selection = false;

  switch (event->type) {
    case LEFTMOUSE:
      if (event->val == KM_RELEASE) {
        cursor_set_by_mouse_position(C, event);
        if (make_selection) {
          data->selection_end_offset = data->cursor_offset;
        }
        return OPERATOR_FINISHED;
      }
      break;
    case MIDDLEMOUSE:
    case RIGHTMOUSE:
      return OPERATOR_FINISHED;
    case MOUSEMOVE:
      make_selection = true;
      if (!text_has_selection(data)) {
        data->selection_start_offset = data->cursor_offset;
      }
      cursor_set_by_mouse_position(C, event);
      data->selection_end_offset = data->cursor_offset;
      break;
    default: {
      break;
    }
  }

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, CTX_data_sequencer_scene(C));
  return OPERATOR_RUNNING_MODAL;
}

static wmOperatorStatus sequencer_text_cursor_set_invoke(bContext *C,
                                                         wmOperator *op,
                                                         const wmEvent *event)
{
  const Scene *scene = CTX_data_sequencer_scene(C);
  Strip *strip = seq::select_active_get(scene);
  TextVars *data = static_cast<TextVars *>(strip->effectdata);
  const View2D *v2d = UI_view2d_fromcontext(C);

  int2 mval_region;
  WM_event_drag_start_mval(event, CTX_wm_region(C), mval_region);
  float2 mouse_loc;
  UI_view2d_region_to_view(v2d, mval_region.x, mval_region.y, &mouse_loc.x, &mouse_loc.y);

  if (!strip_point_image_isect(scene, strip, mouse_loc)) {
    strip->flag &= ~SEQ_FLAG_TEXT_EDITING_ACTIVE;
    return OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH;
  }

  text_selection_cancel(data);
  cursor_set_by_mouse_position(C, event);

  WM_event_add_modal_handler(C, op);
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, CTX_data_sequencer_scene(C));
  return OPERATOR_RUNNING_MODAL;
}

void SEQUENCER_OT_text_cursor_set(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Set Cursor";
  ot->description = "Set cursor position in text";
  ot->idname = "SEQUENCER_OT_text_cursor_set";

  /* API callbacks. */
  ot->invoke = sequencer_text_cursor_set_invoke;
  ot->modal = sequencer_text_cursor_set_modal;
  ot->poll = sequencer_text_editing_active_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */

  PropertyRNA *prop = RNA_def_boolean(
      ot->srna, "select_text", false, "Select Text", "Select text while moving cursor");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

static void text_edit_copy(const TextVars *data)
{
  const TextVarsRuntime *text = data->runtime;
  const IndexRange selection_range = strip_text_selection_range_get(data);
  const seq::CharInfo start = character_at_cursor_offset_get(text, selection_range.first());
  const seq::CharInfo end = character_at_cursor_offset_get(text, selection_range.last());

  const int offset_start = start.offset;
  const int offset_end = end.offset + end.byte_length;
  BLI_assert(offset_start >= 0 && offset_start <= data->text_len_bytes);
  BLI_assert(offset_end >= 0 && offset_end <= data->text_len_bytes);
  BLI_assert(offset_start <= offset_end);

  const size_t len = offset_end - offset_start;
  char *buf = MEM_malloc_arrayN<char>(len + 1, "text clipboard");
  memcpy(buf, data->text_ptr + offset_start, len);
  buf[len] = 0;
  WM_clipboard_text_set(buf, false);
  MEM_freeN(buf);
}

static wmOperatorStatus sequencer_text_edit_copy_exec(bContext *C, wmOperator * /*op*/)
{
  const Strip *strip = seq::select_active_get(CTX_data_sequencer_scene(C));
  const TextVars *data = static_cast<TextVars *>(strip->effectdata);

  if (!text_has_selection(data)) {
    return OPERATOR_CANCELLED;
  }

  text_edit_copy(data);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_text_edit_copy(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Copy Text";
  ot->description = "Copy text to clipboard";
  ot->idname = "SEQUENCER_OT_text_edit_copy";

  /* API callbacks. */
  ot->exec = sequencer_text_edit_copy_exec;
  ot->poll = sequencer_text_editing_active_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO;
}

static wmOperatorStatus sequencer_text_edit_paste_exec(bContext *C, wmOperator * /*op*/)
{
  const Strip *strip = seq::select_active_get(CTX_data_sequencer_scene(C));
  TextVars *data = static_cast<TextVars *>(strip->effectdata);
  const TextVarsRuntime *text = data->runtime;

  int buf_len;
  char *buf = WM_clipboard_text_get(false, true, &buf_len);

  if (buf_len == 0) {
    return OPERATOR_CANCELLED;
  }

  delete_selected_text(data);
  size_t needed_size = data->text_len_bytes + buf_len + 1;
  char *new_text = MEM_malloc_arrayN<char>(needed_size, "text");

  const seq::CharInfo cur_char = character_at_cursor_offset_get(text, data->cursor_offset);
  BLI_assert(cur_char.offset >= 0 && cur_char.offset <= data->text_len_bytes);
  std::memcpy(new_text, data->text_ptr, cur_char.offset);
  std::memcpy(new_text + cur_char.offset, buf, buf_len);
  std::memcpy(new_text + cur_char.offset + buf_len,
              data->text_ptr + cur_char.offset,
              data->text_len_bytes - cur_char.offset + 1);
  data->text_len_bytes += buf_len;
  MEM_freeN(data->text_ptr);
  data->text_ptr = new_text;

  data->cursor_offset += BLI_strlen_utf8(buf);

  MEM_freeN(buf);
  text_editing_update(C);
  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_text_edit_paste(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Paste Text";
  ot->description = "Paste text from clipboard";
  ot->idname = "SEQUENCER_OT_text_edit_paste";

  /* API callbacks. */
  ot->exec = sequencer_text_edit_paste_exec;
  ot->poll = sequencer_text_editing_active_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO;
}

static wmOperatorStatus sequencer_text_edit_cut_exec(bContext *C, wmOperator * /*op*/)
{
  const Strip *strip = seq::select_active_get(CTX_data_sequencer_scene(C));
  TextVars *data = static_cast<TextVars *>(strip->effectdata);

  if (!text_has_selection(data)) {
    return OPERATOR_CANCELLED;
  }

  text_edit_copy(data);
  delete_selected_text(data);

  text_editing_update(C);
  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_text_edit_cut(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Cut Text";
  ot->description = "Cut text to clipboard";
  ot->idname = "SEQUENCER_OT_text_edit_cut";

  /* API callbacks. */
  ot->exec = sequencer_text_edit_cut_exec;
  ot->poll = sequencer_text_editing_active_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO;
}

}  // namespace blender::ed::vse
