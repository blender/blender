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

using namespace blender;

static bool sequencer_text_editing_poll(bContext *C)
{
  if (!sequencer_editing_initialized_and_active(C)) {
    return false;
  }

  const Strip *strip = SEQ_select_active_get(CTX_data_scene(C));
  if (strip == nullptr || strip->type != STRIP_TYPE_TEXT || !SEQ_effects_can_render_text(strip)) {
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
  const Strip *strip = SEQ_select_active_get(CTX_data_scene(C));
  if (strip == nullptr || !sequencer_text_editing_poll(C)) {
    return false;
  }

  if (ED_screen_animation_no_scrub(CTX_wm_manager(C))) {
    return false;
  }

  const Scene *scene = CTX_data_scene(C);

  if (!SEQ_time_strip_intersects_frame(scene, strip, BKE_scene_frame_get(scene))) {
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

  char *addr_start = const_cast<char *>(char_start.str_ptr);
  char *addr_end = const_cast<char *>(char_end.str_ptr) + char_end.byte_length;

  std::memmove(addr_start, addr_end, BLI_strnlen(addr_end, sizeof(data->text)) + 1);

  const int2 sel_start = strip_text_cursor_offset_to_position(text, sel_range.first());
  data->cursor_offset = cursor_position_to_offset(text, sel_start);
  text_selection_cancel(data);
}

static void text_editing_update(const bContext *C)
{
  Strip *strip = SEQ_select_active_get(CTX_data_scene(C));
  SEQ_relations_invalidate_cache_raw(CTX_data_scene(C), strip);
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, CTX_data_scene(C));
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

static int2 cursor_move_prev_word(int2 cursor_position, const TextVarsRuntime *text)
{
  cursor_position = cursor_move_by_character(cursor_position, text, -1);

  while (cursor_position.x > 0 || cursor_position.y > 0) {
    const seq::CharInfo character = character_at_cursor_pos_get(text, cursor_position);
    const int2 prev_cursor_pos = cursor_move_by_character(cursor_position, text, -1);
    const seq::CharInfo prev_character = character_at_cursor_pos_get(text, prev_cursor_pos);

    if (is_whitespace_transition(prev_character.str_ptr[0], character.str_ptr[0])) {
      break;
    }
    cursor_position = prev_cursor_pos;
  }
  return cursor_position;
}

static int2 cursor_move_next_word(int2 cursor_position, const TextVarsRuntime *text)
{
  const int maxline = text->lines.size() - 1;
  const int maxchar = text->lines.last().characters.size() - 1;

  while ((cursor_position.x < maxchar) || (cursor_position.y < maxline)) {
    const seq::CharInfo character = character_at_cursor_pos_get(text, cursor_position);
    cursor_position = cursor_move_by_character(cursor_position, text, 1);
    const seq::CharInfo next_character = character_at_cursor_pos_get(text, cursor_position);

    if (is_whitespace_transition(next_character.str_ptr[0], character.str_ptr[0])) {
      break;
    }
  }
  return cursor_position;
}

static int sequencer_text_cursor_move_exec(bContext *C, wmOperator *op)
{
  const Strip *strip = SEQ_select_active_get(CTX_data_scene(C));
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
      cursor_position = cursor_move_prev_word(cursor_position, text);
      break;
    case NEXT_WORD:
      cursor_position = cursor_move_next_word(cursor_position, text);
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

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, CTX_data_scene(C));
  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_text_cursor_move(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Move cursor";
  ot->description = "Move cursor in text";
  ot->idname = "SEQUENCER_OT_text_cursor_move";

  /* api callbacks */
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

static bool text_insert(TextVars *data, const char *buf)
{
  const TextVarsRuntime *text = data->runtime;

  const bool selection_was_deleted = text_has_selection(data);
  delete_selected_text(data);

  const size_t in_str_len = BLI_strnlen(buf, sizeof(buf));
  const size_t text_str_len = BLI_strnlen(data->text, sizeof(data->text));

  if (text_str_len + in_str_len + 1 > sizeof(data->text)) {
    return selection_was_deleted;
  }

  const seq::CharInfo cur_char = character_at_cursor_offset_get(text, data->cursor_offset);
  char *cursor_addr = const_cast<char *>(cur_char.str_ptr);
  const size_t move_str_len = BLI_strnlen(cursor_addr, sizeof(data->text)) + 1;

  std::memmove(cursor_addr + in_str_len, cursor_addr, move_str_len);
  std::memcpy(cursor_addr, buf, in_str_len);

  data->cursor_offset += 1;
  return true;
}

static int sequencer_text_insert_exec(bContext *C, wmOperator *op)
{
  const Strip *strip = SEQ_select_active_get(CTX_data_scene(C));
  TextVars *data = static_cast<TextVars *>(strip->effectdata);

  char str[512];
  RNA_string_get(op->ptr, "string", str);

  const size_t in_buf_len = BLI_strnlen(str, sizeof(str));
  if (in_buf_len == 0) {
    return OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH;
  }

  if (!text_insert(data, str)) {
    return OPERATOR_CANCELLED;
  }

  text_editing_update(C);
  return OPERATOR_FINISHED;
}

static int sequencer_text_insert_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  char str[6];
  BLI_strncpy(str, event->utf8_buf, BLI_str_utf8_size_safe(event->utf8_buf) + 1);
  RNA_string_set(op->ptr, "string", str);
  return sequencer_text_insert_exec(C, op);
}

void SEQUENCER_OT_text_insert(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Insert Character";
  ot->description = "Insert text at cursor position";
  ot->idname = "SEQUENCER_OT_text_insert";

  /* api callbacks */
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

static void delete_character(const seq::CharInfo character, const TextVars *data)
{
  char *cursor_addr = const_cast<char *>(character.str_ptr);
  char *next_char_addr = cursor_addr + character.byte_length;
  std::memmove(cursor_addr, next_char_addr, BLI_strnlen(next_char_addr, sizeof(data->text)) + 1);
}

static int sequencer_text_delete_exec(bContext *C, wmOperator *op)
{
  const Strip *strip = SEQ_select_active_get(CTX_data_scene(C));
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

  /* api callbacks */
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

static int sequencer_text_line_break_exec(bContext *C, wmOperator * /*op*/)
{
  const Strip *strip = SEQ_select_active_get(CTX_data_scene(C));
  TextVars *data = static_cast<TextVars *>(strip->effectdata);

  if (!text_insert(data, "\n")) {
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

  /* api callbacks */
  ot->exec = sequencer_text_line_break_exec;
  ot->poll = sequencer_text_editing_active_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO;
}

static int sequencer_text_select_all(bContext *C, wmOperator * /*op*/)
{
  const Strip *strip = SEQ_select_active_get(CTX_data_scene(C));
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

  /* api callbacks */
  ot->exec = sequencer_text_select_all;
  ot->poll = sequencer_text_editing_active_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO;
}

static int sequencer_text_deselect_all(bContext *C, wmOperator * /*op*/)
{
  Strip *strip = SEQ_select_active_get(CTX_data_scene(C));
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

  /* api callbacks */
  ot->exec = sequencer_text_deselect_all;
  ot->poll = sequencer_text_editing_active_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO;
}

static int sequencer_text_edit_mode_toggle(bContext *C, wmOperator * /*op*/)
{
  Strip *strip = SEQ_select_active_get(CTX_data_scene(C));
  if (sequencer_text_editing_active_poll(C)) {
    strip->flag &= ~SEQ_FLAG_TEXT_EDITING_ACTIVE;
  }
  else {
    strip->flag |= SEQ_FLAG_TEXT_EDITING_ACTIVE;
  }

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, CTX_data_scene(C));
  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_text_edit_mode_toggle(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Edit Text";
  ot->description = "Toggle text editing";
  ot->idname = "SEQUENCER_OT_text_edit_mode_toggle";

  /* api callbacks */
  ot->exec = sequencer_text_edit_mode_toggle;
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
  const Scene *scene = CTX_data_scene(C);
  const Strip *strip = SEQ_select_active_get(scene);
  TextVars *data = static_cast<TextVars *>(strip->effectdata);
  const View2D *v2d = UI_view2d_fromcontext(C);

  int2 mval_region;
  WM_event_drag_start_mval(event, CTX_wm_region(C), mval_region);
  float3 mouse_loc;
  UI_view2d_region_to_view(v2d, mval_region.x, mval_region.y, &mouse_loc.x, &mouse_loc.y);

  /* Convert cursor coordinates to domain of CharInfo::position. */
  const blender::float3 view_offs{-scene->r.xsch / 2.0f, -scene->r.ysch / 2.0f, 0.0f};
  const float view_aspect = scene->r.xasp / scene->r.yasp;
  blender::float4x4 transform_mat = SEQ_image_transform_matrix_get(CTX_data_scene(C), strip);
  transform_mat = blender::math::invert(transform_mat);

  mouse_loc.x /= view_aspect;
  mouse_loc = blender::math::transform_point(transform_mat, mouse_loc);
  mouse_loc -= view_offs;
  data->cursor_offset = find_closest_cursor_offset(data, float2(mouse_loc));
}

static int sequencer_text_cursor_set_modal(bContext *C, wmOperator * /*op*/, const wmEvent *event)
{
  const Scene *scene = CTX_data_scene(C);
  const Strip *strip = SEQ_select_active_get(scene);
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
  }

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, CTX_data_scene(C));
  return OPERATOR_RUNNING_MODAL;
}

static int sequencer_text_cursor_set_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  const Scene *scene = CTX_data_scene(C);
  Strip *strip = SEQ_select_active_get(scene);
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
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, CTX_data_scene(C));
  return OPERATOR_RUNNING_MODAL;
}

void SEQUENCER_OT_text_cursor_set(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Set Cursor";
  ot->description = "Set cursor position in text";
  ot->idname = "SEQUENCER_OT_text_cursor_set";

  /* api callbacks */
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
  const size_t len = end.str_ptr + end.byte_length - start.str_ptr;

  char clipboard_buf[sizeof(data->text)] = {0};
  memcpy(clipboard_buf, start.str_ptr, math::min(len, sizeof(clipboard_buf)));
  WM_clipboard_text_set(clipboard_buf, false);
}

static int sequencer_text_edit_copy_exec(bContext *C, wmOperator * /*op*/)
{
  const Strip *strip = SEQ_select_active_get(CTX_data_scene(C));
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

  /* api callbacks */
  ot->exec = sequencer_text_edit_copy_exec;
  ot->poll = sequencer_text_editing_active_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO;
}

static int sequencer_text_edit_paste_exec(bContext *C, wmOperator * /*op*/)
{
  const Strip *strip = SEQ_select_active_get(CTX_data_scene(C));
  TextVars *data = static_cast<TextVars *>(strip->effectdata);
  const TextVarsRuntime *text = data->runtime;

  int clipboard_len;
  char *clipboard_buf = WM_clipboard_text_get(false, true, &clipboard_len);

  if (clipboard_len == 0) {
    return OPERATOR_CANCELLED;
  }

  delete_selected_text(data);
  const int max_str_len = sizeof(data->text) - (BLI_strnlen(data->text, sizeof(data->text)) + 1);

  /* Maximum bytes that can be filled into `data->text`. */
  const int fillable_len = std::min(clipboard_len, max_str_len);

  /* Truncated string could contain invalid utf-8 sequence, thus ensure the length inserted is
   * always valid. */
  size_t valid_str_len;
  const int extra_offset = BLI_strnlen_utf8_ex(clipboard_buf, fillable_len, &valid_str_len);

  const seq::CharInfo cur_char = character_at_cursor_offset_get(text, data->cursor_offset);
  char *cursor_addr = const_cast<char *>(cur_char.str_ptr);
  const size_t move_str_len = BLI_strnlen(cursor_addr, sizeof(data->text)) + 1;

  std::memmove(cursor_addr + valid_str_len, cursor_addr, move_str_len);
  std::memcpy(cursor_addr, clipboard_buf, valid_str_len);

  data->cursor_offset += extra_offset;

  MEM_freeN(clipboard_buf);
  text_editing_update(C);
  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_text_edit_paste(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Paste Text";
  ot->description = "Paste text to clipboard";
  ot->idname = "SEQUENCER_OT_text_edit_paste";

  /* api callbacks */
  ot->exec = sequencer_text_edit_paste_exec;
  ot->poll = sequencer_text_editing_active_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO;
}

static int sequencer_text_edit_cut_exec(bContext *C, wmOperator * /*op*/)
{
  const Strip *strip = SEQ_select_active_get(CTX_data_scene(C));
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

  /* api callbacks */
  ot->exec = sequencer_text_edit_cut_exec;
  ot->poll = sequencer_text_editing_active_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO;
}
