/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sptext
 */

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <sstream>

#include "MEM_guardedalloc.h"

#include "DNA_camera_types.h"
#include "DNA_text_types.h"

#include "BLI_fileops.h"
#include "BLI_listbase.h"
#include "BLI_math_base.h"
#include "BLI_math_vector.h"
#include "BLI_path_utils.hh"
#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_string_cursor_utf8.h"
#include "BLI_string_utf8.h"
#include "BLI_time.h"
#include "BLI_vector_set.hh"

#include "BLT_translation.hh"

#include "BKE_context.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_node.hh"
#include "BKE_node_legacy_types.hh"
#include "BKE_node_runtime.hh"
#include "BKE_report.hh"
#include "BKE_text.h"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_curve.hh"
#include "ED_screen.hh"
#include "ED_text.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "RE_engine.h"

#include "RNA_access.hh"
#include "RNA_define.hh"

#ifdef WITH_PYTHON
#  include "BPY_extern.hh"
#  include "BPY_extern_run.hh"
#endif

#include "text_format.hh"
#include "text_intern.hh"

using blender::VectorSet;

static void space_text_screen_clamp(SpaceText *st, const ARegion *region);

/* -------------------------------------------------------------------- */
/** \name Utilities
 * \{ */

/**
 * Tests if the given character represents a start of a new line or the
 * indentation part of a line.
 * \param c: The current character.
 * \param r_last_state: A pointer to a flag representing the last state. The
 * flag may be modified.
 */
static void test_line_start(char c, bool *r_last_state)
{
  if (c == '\n') {
    *r_last_state = true;
  }
  else if (!ELEM(c, '\t', ' ')) {
    *r_last_state = false;
  }
}

/**
 * This function receives a character and returns its closing pair if it exists.
 * \param character: Character to find the closing pair.
 * \return The closing pair of the character if it exists.
 */
static char text_closing_character_pair_get(const char character)
{

  switch (character) {
    case '(':
      return ')';
    case '[':
      return ']';
    case '{':
      return '}';
    case '"':
      return '"';
    case '\'':
      return '\'';
    default:
      return 0;
  }
}

/**
 * This function receives a range and returns true if the range is blank.
 * \param line1: The first TextLine argument.
 * \param line1_char: The character number of line1.
 * \param line2: The second TextLine argument.
 * \param line2_char: The character number of line2.
 * \return True if the span is blank.
 */
static bool text_span_is_blank(TextLine *line1,
                               const int line1_char,
                               TextLine *line2,
                               const int line2_char)
{
  const TextLine *start_line;
  const TextLine *end_line;
  int start_char;
  int end_char;

  /* Get the start and end lines. */
  if (txt_get_span(line1, line2) > 0 || (line1 == line2 && line1_char <= line2_char)) {
    start_line = line1;
    end_line = line2;
    start_char = line1_char;
    end_char = line2_char;
  }
  else {
    start_line = line2;
    end_line = line1;
    start_char = line2_char;
    end_char = line1_char;
  }

  for (const TextLine *line = start_line; line != end_line->next; line = line->next) {
    const int start = (line == start_line) ? start_char : 0;
    const int end = (line == end_line) ? end_char : line->len;

    for (int i = start; i < end; i++) {
      if (!ELEM(line->line[i], ' ', '\t', '\n')) {
        return false;
      }
    }
  }

  return true;
}

/**
 * This function converts the indentation tabs from a buffer to spaces.
 * \param in_buf: A pointer to a cstring.
 * \param tab_size: The size, in spaces, of the tab character.
 * \param r_out_buf_len: The #strlen of the returned buffer.
 */
static char *buf_tabs_to_spaces(const char *in_buf, const int tab_size, int *r_out_buf_len)
{
  /* Get the number of tab characters in buffer. */
  bool line_start = true;
  int num_tabs = 0;

  for (int in_offset = 0; in_buf[in_offset]; in_offset++) {
    /* Verify if is an indentation whitespace character. */
    test_line_start(in_buf[in_offset], &line_start);

    if (in_buf[in_offset] == '\t' && line_start) {
      num_tabs++;
    }
  }

  /* Allocate output before with extra space for expanded tabs. */
  const int out_size = strlen(in_buf) + num_tabs * (tab_size - 1) + 1;
  char *out_buf = MEM_malloc_arrayN<char>(out_size, __func__);

  /* Fill output buffer. */
  int spaces_until_tab = 0;
  int out_offset = 0;
  line_start = true;

  for (int in_offset = 0; in_buf[in_offset]; in_offset++) {
    /* Verify if is an indentation whitespace character. */
    test_line_start(in_buf[in_offset], &line_start);

    if (in_buf[in_offset] == '\t' && line_start) {
      /* Calculate tab size so it fills until next indentation. */
      int num_spaces = tab_size - (spaces_until_tab % tab_size);
      spaces_until_tab = 0;

      /* Write to buffer. */
      memset(&out_buf[out_offset], ' ', num_spaces);
      out_offset += num_spaces;
    }
    else {
      if (in_buf[in_offset] == ' ') {
        spaces_until_tab++;
      }
      else if (in_buf[in_offset] == '\n') {
        spaces_until_tab = 0;
      }

      out_buf[out_offset++] = in_buf[in_offset];
    }
  }

  out_buf[out_offset] = '\0';
  *r_out_buf_len = out_offset;
  return out_buf;
}

BLI_INLINE int space_text_pixel_x_to_column(const SpaceText *st, const int x)
{
  /* Add half the char width so mouse cursor selection is in between letters. */
  return (x + (st->runtime->cwidth_px / 2)) / st->runtime->cwidth_px;
}

static void text_select_update_primary_clipboard(const Text *text)
{
  if ((WM_capabilities_flag() & WM_CAPABILITY_CLIPBOARD_PRIMARY) == 0) {
    return;
  }
  if (!txt_has_sel(text)) {
    return;
  }
  char *buf = txt_sel_to_buf(text, nullptr);
  if (buf == nullptr) {
    return;
  }
  WM_clipboard_text_set(buf, true);
  MEM_freeN(buf);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Operator Poll
 * \{ */

static bool text_new_poll(bContext * /*C*/)
{
  return true;
}

static bool text_data_poll(bContext *C)
{
  Text *text = CTX_data_edit_text(C);
  if (!text) {
    return false;
  }
  return true;
}

static bool text_edit_poll(bContext *C)
{
  Text *text = CTX_data_edit_text(C);

  if (!text) {
    return false;
  }

  if (!BKE_id_is_editable(CTX_data_main(C), &text->id)) {
    // BKE_report(op->reports, RPT_ERROR, "Cannot edit external library data");
    return false;
  }

  return true;
}

bool text_space_edit_poll(bContext *C)
{
  SpaceText *st = CTX_wm_space_text(C);
  Text *text = CTX_data_edit_text(C);

  if (!st || !text) {
    return false;
  }

  if (!BKE_id_is_editable(CTX_data_main(C), &text->id)) {
    // BKE_report(op->reports, RPT_ERROR, "Cannot edit external library data");
    return false;
  }

  return true;
}

static bool text_region_edit_poll(bContext *C)
{
  SpaceText *st = CTX_wm_space_text(C);
  Text *text = CTX_data_edit_text(C);
  ARegion *region = CTX_wm_region(C);

  if (!st || !text) {
    return false;
  }

  if (!region || region->regiontype != RGN_TYPE_WINDOW) {
    return false;
  }

  if (!BKE_id_is_editable(CTX_data_main(C), &text->id)) {
    // BKE_report(op->reports, RPT_ERROR, "Cannot edit external library data");
    return false;
  }

  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Updates
 * \{ */

void text_update_line_edited(TextLine *line)
{
  if (!line) {
    return;
  }

  /* We just free format here, and let it rebuild during draw. */
  MEM_SAFE_FREE(line->format);
}

void text_update_edited(Text *text)
{
  LISTBASE_FOREACH (TextLine *, line, &text->lines) {
    text_update_line_edited(line);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name New Operator
 * \{ */

static wmOperatorStatus text_new_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceText *st = CTX_wm_space_text(C);
  Main *bmain = CTX_data_main(C);
  Text *text;
  PointerRNA ptr;
  PropertyRNA *prop;

  text = BKE_text_add(bmain, DATA_("Text"));

  /* Hook into UI. */
  UI_context_active_but_prop_get_templateID(C, &ptr, &prop);

  if (prop) {
    PointerRNA idptr = RNA_id_pointer_create(&text->id);
    RNA_property_pointer_set(&ptr, prop, idptr, nullptr);
    RNA_property_update(C, &ptr, prop);
  }
  else if (st) {
    st->text = text;
    st->left = 0;
    st->top = 0;
    st->runtime->scroll_ofs_px[0] = 0;
    st->runtime->scroll_ofs_px[1] = 0;
    space_text_drawcache_tag_update(st, true);
  }

  WM_event_add_notifier(C, NC_TEXT | NA_ADDED, text);

  return OPERATOR_FINISHED;
}

void TEXT_OT_new(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "New Text";
  ot->idname = "TEXT_OT_new";
  ot->description = "Create a new text data-block";

  /* API callbacks. */
  ot->exec = text_new_exec;
  ot->poll = text_new_poll;

  /* Flags. */
  ot->flag = OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Open Operator
 * \{ */

static void text_open_init(bContext *C, wmOperator *op)
{
  PropertyPointerRNA *pprop = MEM_new<PropertyPointerRNA>(__func__);

  op->customdata = pprop;
  UI_context_active_but_prop_get_templateID(C, &pprop->ptr, &pprop->prop);
}

static void text_open_cancel(bContext * /*C*/, wmOperator *op)
{
  MEM_delete(static_cast<PropertyPointerRNA *>(op->customdata));
}

static wmOperatorStatus text_open_exec(bContext *C, wmOperator *op)
{
  SpaceText *st = CTX_wm_space_text(C);
  Main *bmain = CTX_data_main(C);
  Text *text;
  char filepath[FILE_MAX];
  const bool internal = RNA_boolean_get(op->ptr, "internal");

  RNA_string_get(op->ptr, "filepath", filepath);

  text = BKE_text_load_ex(bmain, filepath, BKE_main_blendfile_path(bmain), internal);

  if (!text) {
    PropertyPointerRNA *pprop = static_cast<PropertyPointerRNA *>(op->customdata);
    MEM_delete(pprop);
    op->customdata = nullptr;
    return OPERATOR_CANCELLED;
  }

  if (!op->customdata) {
    text_open_init(C, op);
  }

  /* Hook into UI. */
  PropertyPointerRNA *pprop = static_cast<PropertyPointerRNA *>(op->customdata);
  if (pprop->prop) {
    PointerRNA idptr = RNA_id_pointer_create(&text->id);
    RNA_property_pointer_set(&pprop->ptr, pprop->prop, idptr, nullptr);
    RNA_property_update(C, &pprop->ptr, pprop->prop);
  }
  else if (st) {
    st->text = text;
    st->left = 0;
    st->top = 0;
    st->runtime->scroll_ofs_px[0] = 0;
    st->runtime->scroll_ofs_px[1] = 0;
  }

  space_text_drawcache_tag_update(st, true);
  WM_event_add_notifier(C, NC_TEXT | NA_ADDED, text);

  MEM_delete(pprop);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus text_open_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  Main *bmain = CTX_data_main(C);
  if (RNA_struct_property_is_set(op->ptr, "filepath")) {
    return text_open_exec(C, op);
  }

  Text *text = CTX_data_edit_text(C);
  const char *filepath = nullptr;
  char filepath_buf[FILE_MAX];
  if (text && text->filepath) {
    if (BLI_path_is_rel(text->filepath)) {
      STRNCPY(filepath_buf, text->filepath);
      BLI_path_abs(filepath_buf, ID_BLEND_PATH(bmain, &text->id));
      filepath = filepath_buf;
    }
    else {
      filepath = text->filepath;
    }
  }
  else {
    filepath = BKE_main_blendfile_path(bmain);
  }
  BLI_assert(filepath != nullptr);

  text_open_init(C, op);
  RNA_string_set(op->ptr, "filepath", filepath);
  WM_event_add_fileselect(C, op);

  return OPERATOR_RUNNING_MODAL;
}

void TEXT_OT_open(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Open Text";
  ot->idname = "TEXT_OT_open";
  ot->description = "Open a new text data-block";

  /* API callbacks. */
  ot->exec = text_open_exec;
  ot->invoke = text_open_invoke;
  ot->cancel = text_open_cancel;
  ot->poll = text_new_poll;

  /* Flags. */
  ot->flag = OPTYPE_UNDO;

  /* Properties. */
  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER | FILE_TYPE_TEXT | FILE_TYPE_PYSCRIPT,
                                 FILE_SPECIAL,
                                 FILE_OPENFILE,
                                 WM_FILESEL_FILEPATH | WM_FILESEL_RELPATH,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);
  RNA_def_boolean(
      ot->srna, "internal", false, "Make Internal", "Make text file internal after loading");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Reload Operator
 * \{ */

static wmOperatorStatus text_reload_exec(bContext *C, wmOperator *op)
{
  SpaceText *st = CTX_wm_space_text(C);
  Text *text = CTX_data_edit_text(C);
  ARegion *region = CTX_wm_region(C);

  /* Store view & cursor state. */
  const int orig_top = st->top;
  const int orig_curl = BLI_findindex(&text->lines, text->curl);
  const int orig_curc = text->curc;

  /* Don't make this part of `poll`, since `Alt-R` will type `R`,
   * if poll checks for the filename. */
  if (text->filepath == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "This text has not been saved");
    return OPERATOR_CANCELLED;
  }

  if (!BKE_text_reload(text)) {
    BKE_report(op->reports, RPT_ERROR, "Could not reopen file");
    return OPERATOR_CANCELLED;
  }

#ifdef WITH_PYTHON
  if (text->compiled) {
    BPY_text_free_code(text);
  }
#endif

  text_update_edited(text);
  space_text_update_cursor_moved(C);
  space_text_drawcache_tag_update(st, true);
  WM_event_add_notifier(C, NC_TEXT | NA_EDITED, text);

  text->flags &= ~TXT_ISDIRTY;

  /* Return to scroll position. */
  st->top = orig_top;
  space_text_screen_clamp(st, region);
  /* Return cursor. */
  txt_move_to(text, orig_curl, orig_curc, false);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus text_reload_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  return WM_operator_confirm_ex(C,
                                op,
                                IFACE_("Reload active text file?"),
                                nullptr,
                                IFACE_("Reload"),
                                ALERT_ICON_NONE,
                                false);
}

void TEXT_OT_reload(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Reload";
  ot->idname = "TEXT_OT_reload";
  ot->description = "Reload active text data-block from its file";

  /* API callbacks. */
  ot->exec = text_reload_exec;
  ot->invoke = text_reload_invoke;
  ot->poll = text_edit_poll;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Delete Operator
 * \{ */

static bool text_unlink_poll(bContext *C)
{
  /* It should be possible to unlink texts if they're lib-linked in. */
  return CTX_data_edit_text(C) != nullptr;
}

static wmOperatorStatus text_unlink_exec(bContext *C, wmOperator * /*op*/)
{
  Main *bmain = CTX_data_main(C);
  SpaceText *st = CTX_wm_space_text(C);
  Text *text = CTX_data_edit_text(C);

  /* Make the previous text active, if its not there make the next text active. */
  if (st) {
    if (text->id.prev) {
      st->text = static_cast<Text *>(text->id.prev);
      space_text_update_cursor_moved(C);
    }
    else if (text->id.next) {
      st->text = static_cast<Text *>(text->id.next);
      space_text_update_cursor_moved(C);
    }
  }

  BKE_id_delete(bmain, text);

  space_text_drawcache_tag_update(st, true);
  WM_event_add_notifier(C, NC_TEXT | NA_REMOVED, nullptr);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus text_unlink_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  return WM_operator_confirm_ex(C,
                                op,
                                IFACE_("Delete active text file?"),
                                nullptr,
                                IFACE_("Delete"),
                                ALERT_ICON_NONE,
                                false);
}

void TEXT_OT_unlink(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Unlink";
  ot->idname = "TEXT_OT_unlink";
  ot->description = "Unlink active text data-block";

  /* API callbacks. */
  ot->exec = text_unlink_exec;
  ot->invoke = text_unlink_invoke;
  ot->poll = text_unlink_poll;

  /* Flags. */
  ot->flag = OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Make Internal Operator
 * \{ */

static wmOperatorStatus text_make_internal_exec(bContext *C, wmOperator * /*op*/)
{
  Text *text = CTX_data_edit_text(C);

  text->flags |= TXT_ISMEM | TXT_ISDIRTY;

  MEM_SAFE_FREE(text->filepath);

  space_text_update_cursor_moved(C);
  WM_event_add_notifier(C, NC_TEXT | NA_EDITED, text);

  return OPERATOR_FINISHED;
}

void TEXT_OT_make_internal(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Make Internal";
  ot->idname = "TEXT_OT_make_internal";
  ot->description = "Make active text file internal";

  /* API callbacks. */
  ot->exec = text_make_internal_exec;
  ot->poll = text_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Save Operator
 * \{ */

static void txt_write_file(Main *bmain, Text *text, ReportList *reports)
{
  FILE *fp;
  BLI_stat_t st;
  char filepath[FILE_MAX];

  if (text->filepath == nullptr) {
    BKE_reportf(reports, RPT_ERROR, "No file path for \"%s\"", text->id.name + 2);
    return;
  }

  STRNCPY(filepath, text->filepath);
  BLI_path_abs(filepath, BKE_main_blendfile_path(bmain));

  /* Check if file write permission is ok. */
  if (BLI_exists(filepath) && !BLI_file_is_writable(filepath)) {
    BKE_reportf(
        reports, RPT_ERROR, "Cannot save text file, path \"%s\" is not writable", filepath);
    return;
  }

  fp = BLI_fopen(filepath, "w");
  if (fp == nullptr) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Unable to save '%s': %s",
                filepath,
                errno ? strerror(errno) : RPT_("unknown error writing file"));
    return;
  }

  LISTBASE_FOREACH (TextLine *, tmp, &text->lines) {
    fputs(tmp->line, fp);
    if (tmp->next) {
      fputc('\n', fp);
    }
  }

  fclose(fp);

  if (BLI_stat(filepath, &st) == 0) {
    text->mtime = st.st_mtime;

    /* Report since this can be called from key shortcuts. */
    BKE_reportf(reports, RPT_INFO, "Saved text \"%s\"", filepath);
  }
  else {
    text->mtime = 0;
    BKE_reportf(reports,
                RPT_WARNING,
                "Unable to stat '%s': %s",
                filepath,
                errno ? strerror(errno) : RPT_("unknown error statting file"));
  }

  text->flags &= ~TXT_ISDIRTY;
}

static wmOperatorStatus text_save_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Text *text = CTX_data_edit_text(C);

  txt_write_file(bmain, text, op->reports);

  space_text_update_cursor_moved(C);
  WM_event_add_notifier(C, NC_TEXT | NA_EDITED, text);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus text_save_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Text *text = CTX_data_edit_text(C);

  /* Internal and texts without a filepath will go to "Save As". */
  if (text->filepath == nullptr || (text->flags & TXT_ISMEM)) {
    WM_operator_name_call(
        C, "TEXT_OT_save_as", blender::wm::OpCallContext::InvokeDefault, nullptr, event);
    return OPERATOR_CANCELLED;
  }
  return text_save_exec(C, op);
}

void TEXT_OT_save(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Save";
  ot->idname = "TEXT_OT_save";
  ot->description = "Save active text data-block";

  /* API callbacks. */
  ot->exec = text_save_exec;
  ot->invoke = text_save_invoke;
  ot->poll = text_edit_poll;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Save As Operator
 * \{ */

static wmOperatorStatus text_save_as_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Text *text = CTX_data_edit_text(C);
  char filepath[FILE_MAX];

  if (!text) {
    return OPERATOR_CANCELLED;
  }

  RNA_string_get(op->ptr, "filepath", filepath);

  if (text->filepath) {
    MEM_freeN(text->filepath);
  }
  text->filepath = BLI_strdup(filepath);
  text->flags &= ~TXT_ISMEM;

  txt_write_file(bmain, text, op->reports);

  space_text_update_cursor_moved(C);
  WM_event_add_notifier(C, NC_TEXT | NA_EDITED, text);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus text_save_as_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  Main *bmain = CTX_data_main(C);
  Text *text = CTX_data_edit_text(C);

  if (RNA_struct_property_is_set(op->ptr, "filepath")) {
    return text_save_as_exec(C, op);
  }

  const char *filepath = nullptr;
  char filepath_buf[FILE_MAX];
  if (text->filepath) {
    if (BLI_path_is_rel(text->filepath)) {
      STRNCPY(filepath_buf, text->filepath);
      BLI_path_abs(filepath_buf, ID_BLEND_PATH(bmain, &text->id));
      filepath = filepath_buf;
    }
    else {
      filepath = text->filepath;
    }
  }
  else if (text->flags & TXT_ISMEM) {
    filepath = text->id.name + 2;
  }
  else {
    filepath = BKE_main_blendfile_path(bmain);
  }
  BLI_assert(filepath != nullptr);

  RNA_string_set(op->ptr, "filepath", filepath);
  WM_event_add_fileselect(C, op);

  return OPERATOR_RUNNING_MODAL;
}

void TEXT_OT_save_as(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Save As";
  ot->idname = "TEXT_OT_save_as";
  ot->description = "Save active text file with options";

  /* API callbacks. */
  ot->exec = text_save_as_exec;
  ot->invoke = text_save_as_invoke;
  ot->poll = text_edit_poll;

  /* Properties. */
  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER | FILE_TYPE_TEXT | FILE_TYPE_PYSCRIPT,
                                 FILE_SPECIAL,
                                 FILE_SAVE,
                                 WM_FILESEL_FILEPATH,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT); /* XXX TODO: relative_path. */
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Run Script Operator
 * \{ */

static wmOperatorStatus text_run_script(bContext *C, ReportList *reports)
{
#ifdef WITH_PYTHON
  Text *text = CTX_data_edit_text(C);
  const bool is_live = (reports == nullptr);

  /* Only for comparison. */
  void *curl_prev = text->curl;
  int curc_prev = text->curc;
  int selc_prev = text->selc;

  if (BPY_run_text(C, text, reports, !is_live)) {
    if (is_live) {
      /* For nice live updates. */
      WM_event_add_notifier(C, NC_WINDOW | NA_EDITED, nullptr);
    }
    return OPERATOR_FINISHED;
  }

  /* Don't report error messages while live editing. */
  if (!is_live) {
    /* Text may have freed itself. */
    if (CTX_data_edit_text(C) == text) {
      if (text->curl != curl_prev || curc_prev != text->curc || selc_prev != text->selc) {
        space_text_update_cursor_moved(C);
        WM_event_add_notifier(C, NC_TEXT | NA_EDITED, text);
      }
    }

    /* No need to report the error, this has already been handled by #BPY_run_text. */
    return OPERATOR_FINISHED;
  }
#else
  (void)C;
  (void)reports;
#endif /* !WITH_PYTHON */
  return OPERATOR_CANCELLED;
}

static wmOperatorStatus text_run_script_exec(bContext *C, wmOperator *op)
{
#ifndef WITH_PYTHON
  UNUSED_VARS(C);

  BKE_report(op->reports, RPT_ERROR, "Python disabled in this build");

  return OPERATOR_CANCELLED;
#else
  return text_run_script(C, op->reports);
#endif /* WITH_PYTHON */
}

void TEXT_OT_run_script(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Run Script";
  ot->idname = "TEXT_OT_run_script";
  ot->description = "Run active script";

  /* API callbacks. */
  ot->poll = text_data_poll;
  ot->exec = text_run_script_exec;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Paste Operator
 * \{ */

static wmOperatorStatus text_paste_exec(bContext *C, wmOperator *op)
{
  SpaceText *st = CTX_wm_space_text(C);
  Text *text = CTX_data_edit_text(C);

  const bool selection = RNA_boolean_get(op->ptr, "selection");

  char *buf;
  int buf_len;

  /* No need for UTF8 validation as the conversion handles invalid sequences gracefully. */
  buf = WM_clipboard_text_get(selection, false, &buf_len);

  if (!buf) {
    return OPERATOR_CANCELLED;
  }

  space_text_drawcache_tag_update(st, false);

  ED_text_undo_push_init(C);

  /* Convert clipboard content indentation to spaces if specified. */
  if (text->flags & TXT_TABSTOSPACES) {
    char *new_buf = buf_tabs_to_spaces(buf, TXT_TABSIZE, &buf_len);
    MEM_freeN(buf);
    buf = new_buf;
  }

  txt_insert_buf(text, buf, buf_len);
  text_update_edited(text);

  MEM_freeN(buf);

  space_text_update_cursor_moved(C);
  WM_event_add_notifier(C, NC_TEXT | NA_EDITED, text);

  /* Run the script while editing, evil but useful. */
  if (st->live_edit) {
    text_run_script(C, nullptr);
  }

  return OPERATOR_FINISHED;
}

void TEXT_OT_paste(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Paste";
  ot->idname = "TEXT_OT_paste";
  ot->description = "Paste text from clipboard";

  /* API callbacks. */
  ot->exec = text_paste_exec;
  ot->poll = text_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_UNDO;

  /* Properties. */
  PropertyRNA *prop;
  prop = RNA_def_boolean(ot->srna,
                         "selection",
                         false,
                         "Selection",
                         "Paste text selected elsewhere rather than copied (X11/Wayland only)");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Duplicate Operator
 * \{ */

static wmOperatorStatus text_duplicate_line_exec(bContext *C, wmOperator * /*op*/)
{
  Text *text = CTX_data_edit_text(C);

  ED_text_undo_push_init(C);

  txt_duplicate_line(text);

  WM_event_add_notifier(C, NC_TEXT | NA_EDITED, text);

  /* Run the script while editing, evil but useful. */
  if (CTX_wm_space_text(C)->live_edit) {
    text_run_script(C, nullptr);
  }

  return OPERATOR_FINISHED;
}

void TEXT_OT_duplicate_line(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Duplicate Line";
  ot->idname = "TEXT_OT_duplicate_line";
  ot->description = "Duplicate the current line";

  /* API callbacks. */
  ot->exec = text_duplicate_line_exec;
  ot->poll = text_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Copy Operator
 * \{ */

static void txt_copy_clipboard(const Text *text)
{
  char *buf;

  if (!txt_has_sel(text)) {
    return;
  }

  buf = txt_sel_to_buf(text, nullptr);

  if (buf) {
    WM_clipboard_text_set(buf, false);
    MEM_freeN(buf);
  }
}

static wmOperatorStatus text_copy_exec(bContext *C, wmOperator * /*op*/)
{
  const Text *text = CTX_data_edit_text(C);

  txt_copy_clipboard(text);

  return OPERATOR_FINISHED;
}

void TEXT_OT_copy(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Copy";
  ot->idname = "TEXT_OT_copy";
  ot->description = "Copy selected text to clipboard";

  /* API callbacks. */
  ot->exec = text_copy_exec;
  ot->poll = text_edit_poll;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Cut Operator
 * \{ */

static wmOperatorStatus text_cut_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceText *st = CTX_wm_space_text(C);
  Text *text = CTX_data_edit_text(C);

  space_text_drawcache_tag_update(st, false);

  txt_copy_clipboard(text);

  ED_text_undo_push_init(C);
  txt_delete_selected(text);

  space_text_update_cursor_moved(C);
  WM_event_add_notifier(C, NC_TEXT | NA_EDITED, text);

  /* Run the script while editing, evil but useful. */
  if (st->live_edit) {
    text_run_script(C, nullptr);
  }

  return OPERATOR_FINISHED;
}

void TEXT_OT_cut(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Cut";
  ot->idname = "TEXT_OT_cut";
  ot->description = "Cut selected text to clipboard";

  /* API callbacks. */
  ot->exec = text_cut_exec;
  ot->poll = text_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Indent or Autocomplete Operator
 * \{ */

static wmOperatorStatus text_indent_or_autocomplete_exec(bContext *C, wmOperator * /*op*/)
{
  Text *text = CTX_data_edit_text(C);
  TextLine *line = text->curl;
  bool text_before_cursor = text->curc != 0 && !ELEM(line->line[text->curc - 1], ' ', '\t');
  if (text_before_cursor && (txt_has_sel(text) == false)) {
    WM_operator_name_call(
        C, "TEXT_OT_autocomplete", blender::wm::OpCallContext::InvokeDefault, nullptr, nullptr);
  }
  else {
    WM_operator_name_call(
        C, "TEXT_OT_indent", blender::wm::OpCallContext::ExecDefault, nullptr, nullptr);
  }
  return OPERATOR_FINISHED;
}

void TEXT_OT_indent_or_autocomplete(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Indent or Autocomplete";
  ot->idname = "TEXT_OT_indent_or_autocomplete";
  ot->description = "Indent selected text or autocomplete";

  /* API callbacks. */
  ot->exec = text_indent_or_autocomplete_exec;
  ot->poll = text_edit_poll;

  /* Flags. */
  ot->flag = 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Indent Operator
 * \{ */

static wmOperatorStatus text_indent_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceText *st = CTX_wm_space_text(C);
  Text *text = CTX_data_edit_text(C);

  space_text_drawcache_tag_update(st, false);

  ED_text_undo_push_init(C);

  if (txt_has_sel(text)) {
    txt_order_cursors(text, false);
    txt_indent(text);
  }
  else {
    txt_add_char(text, '\t');
  }

  text_update_edited(text);

  space_text_update_cursor_moved(C);
  WM_event_add_notifier(C, NC_TEXT | NA_EDITED, text);

  return OPERATOR_FINISHED;
}

void TEXT_OT_indent(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Indent";
  ot->idname = "TEXT_OT_indent";
  ot->description = "Indent selected text";

  /* API callbacks. */
  ot->exec = text_indent_exec;
  ot->poll = text_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Unindent Operator
 * \{ */

static wmOperatorStatus text_unindent_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceText *st = CTX_wm_space_text(C);
  Text *text = CTX_data_edit_text(C);

  space_text_drawcache_tag_update(st, false);

  ED_text_undo_push_init(C);

  txt_order_cursors(text, false);
  txt_unindent(text);

  text_update_edited(text);

  space_text_update_cursor_moved(C);
  WM_event_add_notifier(C, NC_TEXT | NA_EDITED, text);

  return OPERATOR_FINISHED;
}

void TEXT_OT_unindent(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Unindent";
  ot->idname = "TEXT_OT_unindent";
  ot->description = "Unindent selected text";

  /* API callbacks. */
  ot->exec = text_unindent_exec;
  ot->poll = text_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Line Break Operator
 * \{ */

static wmOperatorStatus text_line_break_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceText *st = CTX_wm_space_text(C);
  Text *text = CTX_data_edit_text(C);
  int a, curts;
  int space = (text->flags & TXT_TABSTOSPACES) ? st->tabnumber : 1;

  space_text_drawcache_tag_update(st, false);

  /* Double check tabs/spaces before splitting the line. */
  curts = txt_setcurr_tab_spaces(text, space);
  ED_text_undo_push_init(C);
  txt_split_curline(text);

  for (a = 0; a < curts; a++) {
    if (text->flags & TXT_TABSTOSPACES) {
      txt_add_char(text, ' ');
    }
    else {
      txt_add_char(text, '\t');
    }
  }

  if (text->curl) {
    if (text->curl->prev) {
      text_update_line_edited(text->curl->prev);
    }
    text_update_line_edited(text->curl);
  }

  space_text_update_cursor_moved(C);
  WM_event_add_notifier(C, NC_TEXT | NA_EDITED, text);

  return OPERATOR_FINISHED;
}

void TEXT_OT_line_break(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Line Break";
  ot->idname = "TEXT_OT_line_break";
  ot->description = "Insert line break at cursor position";

  /* API callbacks. */
  ot->exec = text_line_break_exec;
  ot->poll = text_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Toggle-Comment Operator
 * \{ */

static wmOperatorStatus text_comment_exec(bContext *C, wmOperator *op)
{
  SpaceText *st = CTX_wm_space_text(C);
  Text *text = CTX_data_edit_text(C);
  int type = RNA_enum_get(op->ptr, "type");
  const char *prefix = ED_text_format_comment_line_prefix(text);

  space_text_drawcache_tag_update(st, false);

  ED_text_undo_push_init(C);

  if (txt_has_sel(text)) {
    txt_order_cursors(text, false);
  }

  switch (type) {
    case 1:
      txt_comment(text, prefix);
      break;
    case -1:
      txt_uncomment(text, prefix);
      break;
    default:
      if (txt_uncomment(text, prefix) == false) {
        txt_comment(text, prefix);
      }
      break;
  }

  text_update_edited(text);

  space_text_update_cursor_moved(C);
  WM_event_add_notifier(C, NC_TEXT | NA_EDITED, text);

  return OPERATOR_FINISHED;
}

void TEXT_OT_comment_toggle(wmOperatorType *ot)
{
  static const EnumPropertyItem comment_items[] = {
      {0, "TOGGLE", 0, "Toggle Comments", nullptr},
      {1, "COMMENT", 0, "Comment", nullptr},
      {-1, "UNCOMMENT", 0, "Un-Comment", nullptr},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* Identifiers. */
  ot->name = "Toggle Comments";
  ot->idname = "TEXT_OT_comment_toggle";

  /* API callbacks. */
  ot->exec = text_comment_exec;
  ot->poll = text_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_UNDO;

  /* Properties. */
  PropertyRNA *prop;
  prop = RNA_def_enum(ot->srna, "type", comment_items, 0, "Type", "Add or remove comments");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Convert Whitespace Operator
 * \{ */

enum { TO_SPACES, TO_TABS };
static const EnumPropertyItem whitespace_type_items[] = {
    {TO_SPACES, "SPACES", 0, "To Spaces", nullptr},
    {TO_TABS, "TABS", 0, "To Tabs", nullptr},
    {0, nullptr, 0, nullptr, nullptr},
};

static wmOperatorStatus text_convert_whitespace_exec(bContext *C, wmOperator *op)
{
  SpaceText *st = CTX_wm_space_text(C);
  Text *text = CTX_data_edit_text(C);
  FlattenString fs;
  size_t a, j, max_len = 0;
  int type = RNA_enum_get(op->ptr, "type");

  const int curc_column = text->curl ?
                              BLI_str_utf8_offset_to_column_with_tabs(
                                  text->curl->line, text->curl->len, text->curc, TXT_TABSIZE) :
                              -1;
  const int selc_column = text->sell ?
                              BLI_str_utf8_offset_to_column_with_tabs(
                                  text->sell->line, text->sell->len, text->selc, TXT_TABSIZE) :
                              -1;

  /* First convert to all space, this make it a lot easier to convert to tabs
   * because there is no mixtures of ` ` && `\t`. */
  LISTBASE_FOREACH (TextLine *, tmp, &text->lines) {
    char *new_line;

    BLI_assert(tmp->line);

    flatten_string(st, &fs, tmp->line);
    new_line = BLI_strdup(fs.buf);
    flatten_string_free(&fs);

    MEM_freeN(tmp->line);
    if (tmp->format) {
      MEM_freeN(tmp->format);
    }

    /* Put new_line in the tmp->line spot still need to try and set the curc correctly. */
    tmp->line = new_line;
    tmp->len = strlen(new_line);
    tmp->format = nullptr;
    max_len = std::max<size_t>(tmp->len, max_len);
  }

  if (type == TO_TABS) {
    char *tmp_line = MEM_malloc_arrayN<char>(max_len + 1, __func__);

    LISTBASE_FOREACH (TextLine *, tmp, &text->lines) {
      const char *text_check_line = tmp->line;
      const int text_check_line_len = tmp->len;
      char *tmp_line_cur = tmp_line;
      const size_t tab_len = st->tabnumber;

      BLI_assert(text_check_line);

      for (a = 0; a < text_check_line_len;) {
        /* A tab can only start at a position multiple of `tab_len`. */
        if (!(a % tab_len) && (text_check_line[a] == ' ')) {
          /* A + 0 we already know to be ` ` character. */
          for (j = 1;
               (j < tab_len) && (a + j < text_check_line_len) && (text_check_line[a + j] == ' ');
               j++)
          {
            /* Pass. */
          }

          if (j == tab_len) {
            /* We found a set of spaces that can be replaced by a tab... */
            if ((tmp_line_cur == tmp_line) && a != 0) {
              /* Copy all *valid* string already *parsed*. */
              memcpy(tmp_line_cur, text_check_line, a);
              tmp_line_cur += a;
            }
            *tmp_line_cur = '\t';
            tmp_line_cur++;
            a += j;
          }
          else {
            if (tmp_line_cur != tmp_line) {
              memcpy(tmp_line_cur, &text_check_line[a], j);
              tmp_line_cur += j;
            }
            a += j;
          }
        }
        else {
          size_t len = BLI_str_utf8_size_safe(&text_check_line[a]);
          if (tmp_line_cur != tmp_line) {
            memcpy(tmp_line_cur, &text_check_line[a], len);
            tmp_line_cur += len;
          }
          a += len;
        }
      }

      if (tmp_line_cur != tmp_line) {
        *tmp_line_cur = '\0';

#ifndef NDEBUG
        BLI_assert(tmp_line_cur - tmp_line <= max_len);

        flatten_string(st, &fs, tmp_line);
        BLI_assert(STREQ(fs.buf, tmp->line));
        flatten_string_free(&fs);
#endif

        MEM_freeN(tmp->line);
        if (tmp->format) {
          MEM_freeN(tmp->format);
        }

        /* Put new_line in the `tmp->line` spot. */
        tmp->len = strlen(tmp_line);
        tmp->line = BLI_strdupn(tmp_line, tmp->len);
        tmp->format = nullptr;
      }
    }

    MEM_freeN(tmp_line);
  }

  if (curc_column != -1) {
    text->curc = BLI_str_utf8_offset_from_column_with_tabs(
        text->curl->line, text->curl->len, curc_column, TXT_TABSIZE);
  }
  if (selc_column != -1) {
    text->selc = BLI_str_utf8_offset_from_column_with_tabs(
        text->sell->line, text->sell->len, selc_column, TXT_TABSIZE);
  }

  text_update_edited(text);
  space_text_update_cursor_moved(C);
  space_text_drawcache_tag_update(st, true);
  WM_event_add_notifier(C, NC_TEXT | NA_EDITED, text);

  return OPERATOR_FINISHED;
}

void TEXT_OT_convert_whitespace(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Convert Whitespace";
  ot->idname = "TEXT_OT_convert_whitespace";
  ot->description = "Convert whitespaces by type";

  /* API callbacks. */
  ot->exec = text_convert_whitespace_exec;
  ot->poll = text_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_UNDO;

  /* Properties. */
  RNA_def_enum(ot->srna,
               "type",
               whitespace_type_items,
               TO_SPACES,
               "Type",
               "Type of whitespace to convert to");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select All Operator
 * \{ */

static wmOperatorStatus text_select_all_exec(bContext *C, wmOperator * /*op*/)
{
  Text *text = CTX_data_edit_text(C);

  txt_sel_all(text);

  space_text_update_cursor_moved(C);
  text_select_update_primary_clipboard(text);

  WM_event_add_notifier(C, NC_TEXT | NA_EDITED, text);

  return OPERATOR_FINISHED;
}

void TEXT_OT_select_all(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Select All";
  ot->idname = "TEXT_OT_select_all";
  ot->description = "Select all text";

  /* API callbacks. */
  ot->exec = text_select_all_exec;
  ot->poll = text_edit_poll;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Line Operator
 * \{ */

static wmOperatorStatus text_select_line_exec(bContext *C, wmOperator * /*op*/)
{
  Text *text = CTX_data_edit_text(C);

  txt_sel_line(text);

  space_text_update_cursor_moved(C);
  text_select_update_primary_clipboard(text);

  WM_event_add_notifier(C, NC_TEXT | NA_EDITED, text);

  return OPERATOR_FINISHED;
}

void TEXT_OT_select_line(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Select Line";
  ot->idname = "TEXT_OT_select_line";
  ot->description = "Select text by line";

  /* API callbacks. */
  ot->exec = text_select_line_exec;
  ot->poll = text_edit_poll;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Word Operator
 * \{ */

static wmOperatorStatus text_select_word_exec(bContext *C, wmOperator * /*op*/)
{
  Text *text = CTX_data_edit_text(C);

  BLI_str_cursor_step_bounds_utf8(
      text->curl->line, text->curl->len, text->selc, &text->curc, &text->selc);

  space_text_update_cursor_moved(C);
  text_select_update_primary_clipboard(text);

  WM_event_add_notifier(C, NC_TEXT | NA_EDITED, text);

  return OPERATOR_FINISHED;
}

void TEXT_OT_select_word(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Select Word";
  ot->idname = "TEXT_OT_select_word";
  ot->description = "Select word under cursor";

  /* API callbacks. */
  ot->exec = text_select_word_exec;
  ot->poll = text_edit_poll;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Move Lines Operators
 * \{ */

static wmOperatorStatus move_lines_exec(bContext *C, wmOperator *op)
{
  Text *text = CTX_data_edit_text(C);
  const int direction = RNA_enum_get(op->ptr, "direction");

  ED_text_undo_push_init(C);

  txt_move_lines(text, direction);

  space_text_update_cursor_moved(C);
  WM_event_add_notifier(C, NC_TEXT | NA_EDITED, text);

  /* Run the script while editing, evil but useful. */
  if (CTX_wm_space_text(C)->live_edit) {
    text_run_script(C, nullptr);
  }

  return OPERATOR_FINISHED;
}

void TEXT_OT_move_lines(wmOperatorType *ot)
{
  static const EnumPropertyItem direction_items[] = {
      {TXT_MOVE_LINE_UP, "UP", 0, "Up", ""},
      {TXT_MOVE_LINE_DOWN, "DOWN", 0, "Down", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* Identifiers. */
  ot->name = "Move Lines";
  ot->idname = "TEXT_OT_move_lines";
  ot->description = "Move the currently selected line(s) up/down";

  /* API callbacks. */
  ot->exec = move_lines_exec;
  ot->poll = text_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_UNDO;

  /* Properties. */
  RNA_def_enum(ot->srna, "direction", direction_items, 1, "Direction", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Move Operator
 * \{ */

static const EnumPropertyItem move_type_items[] = {
    {LINE_BEGIN, "LINE_BEGIN", 0, "Line Begin", ""},
    {LINE_END, "LINE_END", 0, "Line End", ""},
    {FILE_TOP, "FILE_TOP", 0, "File Top", ""},
    {FILE_BOTTOM, "FILE_BOTTOM", 0, "File Bottom", ""},
    {PREV_CHAR, "PREVIOUS_CHARACTER", 0, "Previous Character", ""},
    {NEXT_CHAR, "NEXT_CHARACTER", 0, "Next Character", ""},
    {PREV_WORD, "PREVIOUS_WORD", 0, "Previous Word", ""},
    {NEXT_WORD, "NEXT_WORD", 0, "Next Word", ""},
    {PREV_LINE, "PREVIOUS_LINE", 0, "Previous Line", ""},
    {NEXT_LINE, "NEXT_LINE", 0, "Next Line", ""},
    {PREV_PAGE, "PREVIOUS_PAGE", 0, "Previous Page", ""},
    {NEXT_PAGE, "NEXT_PAGE", 0, "Next Page", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

/**
 *  Get cursor position in line by relative wrapped line and column positions.
 */
static int space_text_get_cursor_rel(
    const SpaceText *st, const ARegion *region, TextLine *linein, int rell, int relc)
{
  int i, j, start, end, max, curs, endj, selc;
  bool chop, loop, found;
  char ch;

  max = space_text_wrap_width(st, region);

  selc = start = endj = curs = found = false;
  end = max;
  chop = loop = true;

  for (i = 0, j = 0; loop; j += BLI_str_utf8_size_safe(linein->line + j)) {
    int chars;
    const int columns = BLI_str_utf8_char_width_safe(linein->line + j); /* = 1 for tab. */

    /* Mimic replacement of tabs. */
    ch = linein->line[j];
    if (ch == '\t') {
      chars = st->tabnumber - i % st->tabnumber;
      ch = ' ';
    }
    else {
      chars = 1;
    }

    while (chars--) {
      if (rell == 0 && i - start <= relc && i + columns - start > relc) {
        /* Current position could be wrapped to next line. */
        /* This should be checked when end of current line would be reached. */
        selc = j;
        found = true;
      }
      else if (i - end <= relc && i + columns - end > relc) {
        curs = j;
      }
      if (i + columns - start > max) {
        end = std::min(end, i);

        if (found) {
          /* Exact cursor position was found, check if it's
           * still on needed line (hasn't been wrapped). */
          if (selc > endj && !chop) {
            selc = endj;
          }
          loop = false;
          break;
        }

        if (chop) {
          endj = j;
        }

        start = end;
        end += max;
        chop = true;
        rell--;

        if (rell == 0 && i + columns - start > relc) {
          selc = curs;
          loop = false;
          break;
        }
      }
      else if (ch == '\0') {
        if (!found) {
          selc = linein->len;
        }
        loop = false;
        break;
      }
      else if (ELEM(ch, ' ', '-')) {
        if (found) {
          loop = false;
          break;
        }

        if (rell == 0 && i + columns - start > relc) {
          selc = curs;
          loop = false;
          break;
        }
        end = i + 1;
        endj = j;
        chop = false;
      }
      i += columns;
    }
  }

  return selc;
}

static int cursor_skip_find_line(const SpaceText *st,
                                 const ARegion *region,
                                 int lines,
                                 TextLine **linep,
                                 int *charp,
                                 int *rell,
                                 int *relc)
{
  int offl, offc, visible_lines;

  space_text_wrap_offset_in_line(st, region, *linep, *charp, &offl, &offc);
  *relc = space_text_get_char_pos(st, (*linep)->line, *charp) + offc;
  *rell = lines;

  /* Handle current line. */
  if (lines > 0) {
    visible_lines = space_text_get_visible_lines(st, region, (*linep)->line);

    if (*rell - visible_lines + offl >= 0) {
      if (!(*linep)->next) {
        if (offl < visible_lines - 1) {
          *rell = visible_lines - 1;
          return 1;
        }

        *charp = (*linep)->len;
        return 0;
      }

      *rell -= visible_lines - offl;
      *linep = (*linep)->next;
    }
    else {
      *rell += offl;
      return 1;
    }
  }
  else {
    if (*rell + offl <= 0) {
      if (!(*linep)->prev) {
        if (offl) {
          *rell = 0;
          return 1;
        }

        *charp = 0;
        return 0;
      }

      *rell += offl;
      *linep = (*linep)->prev;
    }
    else {
      *rell += offl;
      return 1;
    }
  }

  /* Skip lines and find destination line and offsets. */
  while (*linep) {
    visible_lines = space_text_get_visible_lines(st, region, (*linep)->line);

    if (lines < 0) { /* Moving top. */
      if (*rell + visible_lines >= 0) {
        *rell += visible_lines;
        break;
      }

      if (!(*linep)->prev) {
        *rell = 0;
        break;
      }

      *rell += visible_lines;
      *linep = (*linep)->prev;
    }
    else { /* Moving bottom. */
      if (*rell - visible_lines < 0) {
        break;
      }

      if (!(*linep)->next) {
        *rell = visible_lines - 1;
        break;
      }

      *rell -= visible_lines;
      *linep = (*linep)->next;
    }
  }

  return 1;
}

static void txt_wrap_move_bol(SpaceText *st, ARegion *region, const bool sel)
{
  Text *text = st->text;
  TextLine **linep;
  int *charp;
  int oldc, i, j, max, start, end, endj;
  bool chop, loop;
  char ch;

  space_text_update_character_width(st);

  if (sel) {
    linep = &text->sell;
    charp = &text->selc;
  }
  else {
    linep = &text->curl;
    charp = &text->curc;
  }

  oldc = *charp;

  max = space_text_wrap_width(st, region);

  start = endj = 0;
  end = max;
  chop = loop = true;
  *charp = 0;

  for (i = 0, j = 0; loop; j += BLI_str_utf8_size_safe((*linep)->line + j)) {
    int chars;
    const int columns = BLI_str_utf8_char_width_safe((*linep)->line + j); /* = 1 for tab. */

    /* Mimic replacement of tabs. */
    ch = (*linep)->line[j];
    if (ch == '\t') {
      chars = st->tabnumber - i % st->tabnumber;
      ch = ' ';
    }
    else {
      chars = 1;
    }

    while (chars--) {
      if (i + columns - start > max) {
        end = std::min(end, i);

        *charp = endj;

        if (j >= oldc) {
          if (ch == '\0') {
            *charp = BLI_str_utf8_offset_from_column_with_tabs(
                (*linep)->line, (*linep)->len, start, TXT_TABSIZE);
          }
          loop = false;
          break;
        }

        if (chop) {
          endj = j;
        }

        start = end;
        end += max;
        chop = true;
      }
      else if (ELEM(ch, ' ', '-', '\0')) {
        if (j >= oldc) {
          *charp = BLI_str_utf8_offset_from_column_with_tabs(
              (*linep)->line, (*linep)->len, start, TXT_TABSIZE);
          loop = false;
          break;
        }

        end = i + 1;
        endj = j + 1;
        chop = false;
      }
      i += columns;
    }
  }

  if (!sel) {
    txt_pop_sel(text);
  }
}

static void txt_wrap_move_eol(SpaceText *st, ARegion *region, const bool sel)
{
  Text *text = st->text;
  TextLine **linep;
  int *charp;
  int oldc, i, j, max, start, end, endj;
  bool chop, loop;
  char ch;

  space_text_update_character_width(st);

  if (sel) {
    linep = &text->sell;
    charp = &text->selc;
  }
  else {
    linep = &text->curl;
    charp = &text->curc;
  }

  oldc = *charp;

  max = space_text_wrap_width(st, region);

  start = endj = 0;
  end = max;
  chop = loop = true;
  *charp = 0;

  for (i = 0, j = 0; loop; j += BLI_str_utf8_size_safe((*linep)->line + j)) {
    int chars;
    const int columns = BLI_str_utf8_char_width_safe((*linep)->line + j); /* = 1 for tab. */

    /* Mimic replacement of tabs. */
    ch = (*linep)->line[j];
    if (ch == '\t') {
      chars = st->tabnumber - i % st->tabnumber;
      ch = ' ';
    }
    else {
      chars = 1;
    }

    while (chars--) {
      if (i + columns - start > max) {
        end = std::min(end, i);

        if (chop) {
          endj = BLI_str_find_prev_char_utf8((*linep)->line + j, (*linep)->line) - (*linep)->line;
        }

        if (endj >= oldc) {
          if (ch == '\0') {
            *charp = (*linep)->len;
          }
          else {
            *charp = endj;
          }
          loop = false;
          break;
        }

        start = end;
        end += max;
        chop = true;
      }
      else if (ch == '\0') {
        *charp = (*linep)->len;
        loop = false;
        break;
      }
      else if (ELEM(ch, ' ', '-')) {
        end = i + 1;
        endj = j;
        chop = false;
      }
      i += columns;
    }
  }

  if (!sel) {
    txt_pop_sel(text);
  }
}

static void txt_wrap_move_up(SpaceText *st, ARegion *region, const bool sel)
{
  Text *text = st->text;
  TextLine **linep;
  int *charp;
  int offl, offc, col;

  space_text_update_character_width(st);

  if (sel) {
    linep = &text->sell;
    charp = &text->selc;
  }
  else {
    linep = &text->curl;
    charp = &text->curc;
  }

  space_text_wrap_offset_in_line(st, region, *linep, *charp, &offl, &offc);
  col = space_text_get_char_pos(st, (*linep)->line, *charp) + offc;
  if (offl) {
    *charp = space_text_get_cursor_rel(st, region, *linep, offl - 1, col);
  }
  else {
    if ((*linep)->prev) {
      int visible_lines;

      *linep = (*linep)->prev;
      visible_lines = space_text_get_visible_lines(st, region, (*linep)->line);
      *charp = space_text_get_cursor_rel(st, region, *linep, visible_lines - 1, col);
    }
    else {
      *charp = 0;
    }
  }

  if (!sel) {
    txt_pop_sel(text);
  }
}

static void txt_wrap_move_down(SpaceText *st, ARegion *region, const bool sel)
{
  Text *text = st->text;
  TextLine **linep;
  int *charp;
  int offl, offc, col, visible_lines;

  space_text_update_character_width(st);

  if (sel) {
    linep = &text->sell;
    charp = &text->selc;
  }
  else {
    linep = &text->curl;
    charp = &text->curc;
  }

  space_text_wrap_offset_in_line(st, region, *linep, *charp, &offl, &offc);
  col = space_text_get_char_pos(st, (*linep)->line, *charp) + offc;
  visible_lines = space_text_get_visible_lines(st, region, (*linep)->line);
  if (offl < visible_lines - 1) {
    *charp = space_text_get_cursor_rel(st, region, *linep, offl + 1, col);
  }
  else {
    if ((*linep)->next) {
      *linep = (*linep)->next;
      *charp = space_text_get_cursor_rel(st, region, *linep, 0, col);
    }
    else {
      *charp = (*linep)->len;
    }
  }

  if (!sel) {
    txt_pop_sel(text);
  }
}

/**
 * Moves the cursor vertically by the specified number of lines.
 * If the destination line is shorter than the current cursor position, the
 * cursor will be positioned at the end of this line.
 *
 * This is to replace screen_skip for PageUp/Down operations.
 */
static void space_text_cursor_skip(
    const SpaceText *st, const ARegion *region, Text *text, int lines, const bool sel)
{
  TextLine **linep;
  int *charp;

  if (sel) {
    linep = &text->sell;
    charp = &text->selc;
  }
  else {
    linep = &text->curl;
    charp = &text->curc;
  }

  if (st && region && st->wordwrap) {
    int rell, relc;

    /* Find line and offsets inside it needed to set cursor position. */
    if (cursor_skip_find_line(st, region, lines, linep, charp, &rell, &relc)) {
      *charp = space_text_get_cursor_rel(st, region, *linep, rell, relc);
    }
  }
  else {
    while (lines > 0 && (*linep)->next) {
      *linep = (*linep)->next;
      lines--;
    }
    while (lines < 0 && (*linep)->prev) {
      *linep = (*linep)->prev;
      lines++;
    }
  }

  *charp = std::min(*charp, (*linep)->len);

  if (!sel) {
    txt_pop_sel(text);
  }
}

static wmOperatorStatus text_move_cursor(bContext *C, int type, bool select)
{
  SpaceText *st = CTX_wm_space_text(C);
  Text *text = CTX_data_edit_text(C);
  ARegion *region = CTX_wm_region(C);

  /* Ensure we have the right region, it's optional. */
  if (region && region->regiontype != RGN_TYPE_WINDOW) {
    region = nullptr;
  }

  switch (type) {
    case LINE_BEGIN:
      if (!select) {
        txt_sel_clear(text);
      }
      if (st && st->wordwrap && region) {
        txt_wrap_move_bol(st, region, select);
      }
      else {
        txt_move_bol(text, select);
      }
      break;

    case LINE_END:
      if (!select) {
        txt_sel_clear(text);
      }
      if (st && st->wordwrap && region) {
        txt_wrap_move_eol(st, region, select);
      }
      else {
        txt_move_eol(text, select);
      }
      break;

    case FILE_TOP:
      txt_move_bof(text, select);
      break;

    case FILE_BOTTOM:
      txt_move_eof(text, select);
      break;

    case PREV_WORD:
      if (txt_cursor_is_line_start(text)) {
        txt_move_left(text, select);
      }
      txt_jump_left(text, select, true);
      break;

    case NEXT_WORD:
      if (txt_cursor_is_line_end(text)) {
        txt_move_right(text, select);
      }
      txt_jump_right(text, select, true);
      break;

    case PREV_CHAR:
      if (txt_has_sel(text) && !select) {
        txt_order_cursors(text, false);
        txt_pop_sel(text);
      }
      else {
        txt_move_left(text, select);
      }
      break;

    case NEXT_CHAR:
      if (txt_has_sel(text) && !select) {
        txt_order_cursors(text, true);
        txt_pop_sel(text);
      }
      else {
        txt_move_right(text, select);
      }
      break;

    case PREV_LINE:
      if (st && st->wordwrap && region) {
        txt_wrap_move_up(st, region, select);
      }
      else {
        txt_move_up(text, select);
      }
      break;

    case NEXT_LINE:
      if (st && st->wordwrap && region) {
        txt_wrap_move_down(st, region, select);
      }
      else {
        txt_move_down(text, select);
      }
      break;

    case PREV_PAGE:
      if (st) {
        space_text_cursor_skip(st, region, st->text, -st->runtime->viewlines, select);
      }
      else {
        space_text_cursor_skip(nullptr, nullptr, text, -10, select);
      }
      break;

    case NEXT_PAGE:
      if (st) {
        space_text_cursor_skip(st, region, st->text, st->runtime->viewlines, select);
      }
      else {
        space_text_cursor_skip(nullptr, nullptr, text, 10, select);
      }
      break;
  }

  space_text_update_cursor_moved(C);
  if (select) {
    text_select_update_primary_clipboard(st->text);
  }

  WM_event_add_notifier(C, NC_TEXT | ND_CURSOR, text);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus text_move_exec(bContext *C, wmOperator *op)
{
  int type = RNA_enum_get(op->ptr, "type");

  return text_move_cursor(C, type, false);
}

void TEXT_OT_move(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Move Cursor";
  ot->idname = "TEXT_OT_move";
  ot->description = "Move cursor to position type";

  /* API callbacks. */
  ot->exec = text_move_exec;
  ot->poll = text_edit_poll;

  /* Properties. */
  RNA_def_enum(ot->srna, "type", move_type_items, LINE_BEGIN, "Type", "Where to move cursor to");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Move Select Operator
 * \{ */

static wmOperatorStatus text_move_select_exec(bContext *C, wmOperator *op)
{
  int type = RNA_enum_get(op->ptr, "type");

  return text_move_cursor(C, type, true);
}

void TEXT_OT_move_select(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Move Select";
  ot->idname = "TEXT_OT_move_select";
  ot->description = "Move the cursor while selecting";

  /* API callbacks. */
  ot->exec = text_move_select_exec;
  ot->poll = text_space_edit_poll;

  /* Properties. */
  RNA_def_enum(ot->srna,
               "type",
               move_type_items,
               LINE_BEGIN,
               "Type",
               "Where to move cursor to, to make a selection");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Jump Operator
 * \{ */

static wmOperatorStatus text_jump_exec(bContext *C, wmOperator *op)
{
  Text *text = CTX_data_edit_text(C);
  int line = RNA_int_get(op->ptr, "line");
  short nlines = txt_get_span(static_cast<TextLine *>(text->lines.first),
                              static_cast<TextLine *>(text->lines.last)) +
                 1;

  if (line < 1) {
    txt_move_toline(text, 1, false);
  }
  else if (line > nlines) {
    txt_move_toline(text, nlines - 1, false);
  }
  else {
    txt_move_toline(text, line - 1, false);
  }

  space_text_update_cursor_moved(C);
  WM_event_add_notifier(C, NC_TEXT | ND_CURSOR, text);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus text_jump_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  return WM_operator_props_dialog_popup(C, op, 200, IFACE_("Jump to Line Number"));
}

void TEXT_OT_jump(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Jump";
  ot->idname = "TEXT_OT_jump";
  ot->description = "Jump cursor to line";

  /* API callbacks. */
  ot->invoke = text_jump_invoke;
  ot->exec = text_jump_exec;
  ot->poll = text_edit_poll;

  /* Properties. */
  ot->prop = RNA_def_int(
      ot->srna, "line", 1, 1, INT_MAX, "Line", "Line number to jump to", 1, 10000);
  RNA_def_property_translation_context(ot->prop, BLT_I18NCONTEXT_ID_TEXT);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Delete Operator
 * \{ */

static const EnumPropertyItem delete_type_items[] = {
    {DEL_NEXT_CHAR, "NEXT_CHARACTER", 0, "Next Character", ""},
    {DEL_PREV_CHAR, "PREVIOUS_CHARACTER", 0, "Previous Character", ""},
    {DEL_NEXT_WORD, "NEXT_WORD", 0, "Next Word", ""},
    {DEL_PREV_WORD, "PREVIOUS_WORD", 0, "Previous Word", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static wmOperatorStatus text_delete_exec(bContext *C, wmOperator *op)
{
  SpaceText *st = CTX_wm_space_text(C);
  Text *text = CTX_data_edit_text(C);
  int type = RNA_enum_get(op->ptr, "type");

  space_text_drawcache_tag_update(st, true);

  /* Behavior could be changed here,
   * but for now just don't jump words when we have a selection. */
  if (txt_has_sel(text)) {
    if (type == DEL_PREV_WORD) {
      type = DEL_PREV_CHAR;
    }
    else if (type == DEL_NEXT_WORD) {
      type = DEL_NEXT_CHAR;
    }
  }

  ED_text_undo_push_init(C);

  if (type == DEL_PREV_WORD) {
    if (txt_cursor_is_line_start(text)) {
      txt_backspace_char(text);
    }
    txt_backspace_word(text);
  }
  else if (type == DEL_PREV_CHAR) {

    if (text->flags & TXT_TABSTOSPACES) {
      if (!txt_has_sel(text) && !txt_cursor_is_line_start(text)) {
        int tabsize = 0;
        tabsize = txt_calc_tab_left(text->curl, text->curc);
        if (tabsize) {
          text->sell = text->curl;
          text->selc = text->curc - tabsize;
          txt_order_cursors(text, false);
        }
      }
    }
    if (U.text_flag & USER_TEXT_EDIT_AUTO_CLOSE) {
      const char *curr = text->curl->line + text->curc;
      if (*curr != '\0') {
        const char *prev = BLI_str_find_prev_char_utf8(curr, text->curl->line);
        if ((curr != prev) && /* When back-spacing from the start of the line. */
            (*curr == text_closing_character_pair_get(*prev)))
        {
          txt_move_right(text, false);
          txt_backspace_char(text);
        }
      }
    }
    txt_backspace_char(text);
  }
  else if (type == DEL_NEXT_WORD) {
    if (txt_cursor_is_line_end(text)) {
      txt_delete_char(text);
    }
    txt_delete_word(text);
  }
  else if (type == DEL_NEXT_CHAR) {

    if (text->flags & TXT_TABSTOSPACES) {
      if (!txt_has_sel(text) && !txt_cursor_is_line_end(text)) {
        int tabsize = 0;
        tabsize = txt_calc_tab_right(text->curl, text->curc);
        if (tabsize) {
          text->sell = text->curl;
          text->selc = text->curc + tabsize;
          txt_order_cursors(text, true);
        }
      }
    }

    txt_delete_char(text);
  }

  text_update_line_edited(text->curl);

  space_text_update_cursor_moved(C);
  WM_event_add_notifier(C, NC_TEXT | NA_EDITED, text);

  /* Run the script while editing, evil but useful. */
  if (st->live_edit) {
    text_run_script(C, nullptr);
  }

  return OPERATOR_FINISHED;
}

void TEXT_OT_delete(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Delete";
  ot->idname = "TEXT_OT_delete";
  ot->description = "Delete text by cursor position";

  /* API callbacks. */
  ot->exec = text_delete_exec;
  ot->poll = text_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_UNDO;

  /* Properties. */
  PropertyRNA *prop;
  prop = RNA_def_enum(ot->srna,
                      "type",
                      delete_type_items,
                      DEL_NEXT_CHAR,
                      "Type",
                      "Which part of the text to delete");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Toggle Overwrite Operator
 * \{ */

static wmOperatorStatus text_toggle_overwrite_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceText *st = CTX_wm_space_text(C);

  st->overwrite = !st->overwrite;

  WM_event_add_notifier(C, NC_TEXT | ND_CURSOR, st->text);

  return OPERATOR_FINISHED;
}

void TEXT_OT_overwrite_toggle(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Toggle Overwrite";
  ot->idname = "TEXT_OT_overwrite_toggle";
  ot->description = "Toggle overwrite while typing";

  /* API callbacks. */
  ot->exec = text_toggle_overwrite_exec;
  ot->poll = text_space_edit_poll;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Scroll Operator
 * \{ */

static void space_text_screen_clamp(SpaceText *st, const ARegion *region)
{
  if (st->top <= 0) {
    st->top = 0;
  }
  else {
    int last;
    last = space_text_get_total_lines(st, region);
    last = last - (st->runtime->viewlines / 2);
    if (last > 0 && st->top > last) {
      st->top = last;
    }
  }
}

/* Moves the view vertically by the specified number of lines. */
static void space_text_screen_skip(SpaceText *st, ARegion *region, int lines)
{
  st->top += lines;
  space_text_screen_clamp(st, region);
}

/** Enum for #TextScroll::zone (scroll-bar handles). */
enum eScrollZone {
  SCROLLHANDLE_INVALID_OUTSIDE = -1,
  SCROLLHANDLE_BAR,
  SCROLLHANDLE_MIN_OUTSIDE,
  SCROLLHANDLE_MAX_OUTSIDE,
};

struct TextScroll {
  int mval_prev[2];
  int mval_delta[2];

  bool is_first;
  bool is_scrollbar;

  enum eScrollZone zone;

  /* Store the state of the display, cache some constant vars. */
  struct {
    int ofs_init[2];
    int ofs_max[2];
    int size_px[2];
  } state;
  int ofs_delta[2];
  int ofs_delta_px[2];
};

static void text_scroll_state_init(TextScroll *tsc, SpaceText *st, ARegion *region)
{
  tsc->state.ofs_init[0] = st->left;
  tsc->state.ofs_init[1] = st->top;

  tsc->state.ofs_max[0] = INT_MAX;
  tsc->state.ofs_max[1] = max_ii(
      0, space_text_get_total_lines(st, region) - (st->runtime->viewlines / 2));

  tsc->state.size_px[0] = st->runtime->cwidth_px;
  tsc->state.size_px[1] = TXT_LINE_HEIGHT(st);
}

static bool text_scroll_poll(bContext *C)
{
  /* It should be possible to still scroll linked texts to read them,
   * even if they can't be edited... */
  return CTX_data_edit_text(C) != nullptr;
}

static wmOperatorStatus text_scroll_exec(bContext *C, wmOperator *op)
{
  SpaceText *st = CTX_wm_space_text(C);
  ARegion *region = CTX_wm_region(C);

  int lines = RNA_int_get(op->ptr, "lines");

  if (lines == 0) {
    return OPERATOR_CANCELLED;
  }

  space_text_screen_skip(st, region, lines * 3);

  ED_area_tag_redraw(CTX_wm_area(C));

  return OPERATOR_FINISHED;
}

static void text_scroll_apply(bContext *C, wmOperator *op, const wmEvent *event)
{
  SpaceText *st = CTX_wm_space_text(C);
  TextScroll *tsc = static_cast<TextScroll *>(op->customdata);
  const int mval[2] = {event->xy[0], event->xy[1]};

  space_text_update_character_width(st);

  /* Compute mouse move distance. */
  if (tsc->is_first) {
    copy_v2_v2_int(tsc->mval_prev, mval);
    tsc->is_first = false;
  }

  if (event->type != MOUSEPAN) {
    sub_v2_v2v2_int(tsc->mval_delta, mval, tsc->mval_prev);
  }

  /* Accumulate scroll, in float values for events that give less than one
   * line offset but taken together should still scroll. */
  if (!tsc->is_scrollbar) {
    tsc->ofs_delta_px[0] -= tsc->mval_delta[0];
    tsc->ofs_delta_px[1] += tsc->mval_delta[1];
  }
  else {
    tsc->ofs_delta_px[1] -= (tsc->mval_delta[1] * st->runtime->scroll_px_per_line) *
                            tsc->state.size_px[1];
  }

  for (int i = 0; i < 2; i += 1) {
    int lines_from_pixels = tsc->ofs_delta_px[i] / tsc->state.size_px[i];
    tsc->ofs_delta[i] += lines_from_pixels;
    tsc->ofs_delta_px[i] -= lines_from_pixels * tsc->state.size_px[i];
  }

  /* The final values need to be calculated from the inputs,
   * so clamping and ensuring an unsigned pixel offset doesn't conflict with
   * updating the cursor mval_delta. */
  int scroll_ofs_new[2] = {
      tsc->state.ofs_init[0] + tsc->ofs_delta[0],
      tsc->state.ofs_init[1] + tsc->ofs_delta[1],
  };
  int scroll_ofs_px_new[2] = {
      tsc->ofs_delta_px[0],
      tsc->ofs_delta_px[1],
  };

  for (int i = 0; i < 2; i += 1) {
    /* Ensure always unsigned (adjusting line/column accordingly). */
    while (scroll_ofs_px_new[i] < 0) {
      scroll_ofs_px_new[i] += tsc->state.size_px[i];
      scroll_ofs_new[i] -= 1;
    }

    /* Clamp within usable region. */
    if (scroll_ofs_new[i] < 0) {
      scroll_ofs_new[i] = 0;
      scroll_ofs_px_new[i] = 0;
    }
    else if (scroll_ofs_new[i] >= tsc->state.ofs_max[i]) {
      scroll_ofs_new[i] = tsc->state.ofs_max[i];
      scroll_ofs_px_new[i] = 0;
    }
  }

  /* Override for word-wrap. */
  if (st->wordwrap) {
    scroll_ofs_new[0] = 0;
    scroll_ofs_px_new[0] = 0;
  }

  /* Apply to the screen. */
  if (scroll_ofs_new[0] != st->left || scroll_ofs_new[1] != st->top ||
      /* Horizontal sub-pixel offset currently isn't used. */
      /* `scroll_ofs_px_new[0] != st->scroll_ofs_px[0] ||`. */
      scroll_ofs_px_new[1] != st->runtime->scroll_ofs_px[1])
  {

    st->left = scroll_ofs_new[0];
    st->top = scroll_ofs_new[1];
    st->runtime->scroll_ofs_px[0] = scroll_ofs_px_new[0];
    st->runtime->scroll_ofs_px[1] = scroll_ofs_px_new[1];
    ED_area_tag_redraw(CTX_wm_area(C));
  }

  tsc->mval_prev[0] = mval[0];
  tsc->mval_prev[1] = mval[1];
}

static void scroll_exit(bContext *C, wmOperator *op)
{
  SpaceText *st = CTX_wm_space_text(C);
  TextScroll *tsc = static_cast<TextScroll *>(op->customdata);

  st->flags &= ~ST_SCROLL_SELECT;

  if (st->runtime->scroll_ofs_px[1] > tsc->state.size_px[1] / 2) {
    st->top += 1;
  }

  st->runtime->scroll_ofs_px[0] = 0;
  st->runtime->scroll_ofs_px[1] = 0;
  ED_area_tag_redraw(CTX_wm_area(C));

  MEM_freeN(tsc);
  op->customdata = nullptr;
}

static wmOperatorStatus text_scroll_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  TextScroll *tsc = static_cast<TextScroll *>(op->customdata);
  SpaceText *st = CTX_wm_space_text(C);
  ARegion *region = CTX_wm_region(C);

  switch (event->type) {
    case MOUSEMOVE:
      if (tsc->zone == SCROLLHANDLE_BAR) {
        text_scroll_apply(C, op, event);
      }
      break;
    case LEFTMOUSE:
    case RIGHTMOUSE:
    case MIDDLEMOUSE:
      if (event->val == KM_RELEASE) {
        if (ELEM(tsc->zone, SCROLLHANDLE_MIN_OUTSIDE, SCROLLHANDLE_MAX_OUTSIDE)) {
          space_text_screen_skip(st,
                                 region,
                                 st->runtime->viewlines *
                                     (tsc->zone == SCROLLHANDLE_MIN_OUTSIDE ? 1 : -1));

          ED_area_tag_redraw(CTX_wm_area(C));
        }
        scroll_exit(C, op);
        return OPERATOR_FINISHED;
      }
    default: {
      break;
    }
  }

  return OPERATOR_RUNNING_MODAL;
}

static void text_scroll_cancel(bContext *C, wmOperator *op)
{
  scroll_exit(C, op);
}

static wmOperatorStatus text_scroll_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  SpaceText *st = CTX_wm_space_text(C);
  ARegion *region = CTX_wm_region(C);

  TextScroll *tsc;

  if (RNA_struct_property_is_set(op->ptr, "lines")) {
    return text_scroll_exec(C, op);
  }

  tsc = MEM_callocN<TextScroll>("TextScroll");
  tsc->is_first = true;
  tsc->zone = SCROLLHANDLE_BAR;

  text_scroll_state_init(tsc, st, region);

  op->customdata = tsc;

  st->flags |= ST_SCROLL_SELECT;

  if (event->type == MOUSEPAN) {
    space_text_update_character_width(st);

    copy_v2_v2_int(tsc->mval_prev, event->xy);
    /* Sensitivity of scroll set to 4pix per line/char. */
    tsc->mval_delta[0] = (event->xy[0] - event->prev_xy[0]) * st->runtime->cwidth_px / 4;
    tsc->mval_delta[1] = (event->xy[1] - event->prev_xy[1]) * st->runtime->lheight_px / 4;
    tsc->is_first = false;
    tsc->is_scrollbar = false;
    text_scroll_apply(C, op, event);
    scroll_exit(C, op);
    return OPERATOR_FINISHED;
  }

  WM_event_add_modal_handler(C, op);

  return OPERATOR_RUNNING_MODAL;
}

void TEXT_OT_scroll(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Scroll";
  /* Don't really see the difference between this and scroll_bar.
   * Both do basically the same thing (aside from key-maps). */
  ot->idname = "TEXT_OT_scroll";

  /* API callbacks. */
  ot->exec = text_scroll_exec;
  ot->invoke = text_scroll_invoke;
  ot->modal = text_scroll_modal;
  ot->cancel = text_scroll_cancel;
  ot->poll = text_scroll_poll;

  /* Flags. */
  ot->flag = OPTYPE_BLOCKING | OPTYPE_GRAB_CURSOR_XY | OPTYPE_INTERNAL;

  /* Properties. */
  PropertyRNA *prop;
  prop = RNA_def_int(
      ot->srna, "lines", 1, INT_MIN, INT_MAX, "Lines", "Number of lines to scroll", -100, 100);
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_TEXT);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Scroll Bar Operator
 * \{ */

static bool text_region_scroll_poll(bContext *C)
{
  /* Same as text_region_edit_poll except it works on libdata too. */
  SpaceText *st = CTX_wm_space_text(C);
  Text *text = CTX_data_edit_text(C);
  ARegion *region = CTX_wm_region(C);

  if (!st || !text) {
    return false;
  }

  if (!region || region->regiontype != RGN_TYPE_WINDOW) {
    return false;
  }

  return true;
}

static wmOperatorStatus text_scroll_bar_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  SpaceText *st = CTX_wm_space_text(C);
  ARegion *region = CTX_wm_region(C);
  TextScroll *tsc;
  const int *mval = event->mval;
  enum eScrollZone zone = SCROLLHANDLE_INVALID_OUTSIDE;

  if (RNA_struct_property_is_set(op->ptr, "lines")) {
    return text_scroll_exec(C, op);
  }

  /* Verify we are in the right zone. */
  if (mval[0] > st->runtime->scroll_region_handle.xmin &&
      mval[0] < st->runtime->scroll_region_handle.xmax)
  {
    if (mval[1] >= st->runtime->scroll_region_handle.ymin &&
        mval[1] <= st->runtime->scroll_region_handle.ymax)
    {
      /* Mouse inside scroll handle. */
      zone = SCROLLHANDLE_BAR;
    }
    else if (mval[1] > TXT_SCROLL_SPACE && mval[1] < region->winy - TXT_SCROLL_SPACE) {
      if (mval[1] < st->runtime->scroll_region_handle.ymin) {
        zone = SCROLLHANDLE_MIN_OUTSIDE;
      }
      else {
        zone = SCROLLHANDLE_MAX_OUTSIDE;
      }
    }
  }

  if (zone == SCROLLHANDLE_INVALID_OUTSIDE) {
    /* We are outside slider - nothing to do. */
    return OPERATOR_PASS_THROUGH;
  }

  tsc = MEM_callocN<TextScroll>("TextScroll");
  tsc->is_first = true;
  tsc->is_scrollbar = true;
  tsc->zone = zone;
  op->customdata = tsc;
  st->flags |= ST_SCROLL_SELECT;

  text_scroll_state_init(tsc, st, region);

  /* Jump scroll, works in `v2d` but needs to be added here too unfortunately. */
  if (event->type == MIDDLEMOUSE) {
    tsc->mval_prev[0] = region->winrct.xmin + BLI_rcti_cent_x(&st->runtime->scroll_region_handle);
    tsc->mval_prev[1] = region->winrct.ymin + BLI_rcti_cent_y(&st->runtime->scroll_region_handle);

    tsc->is_first = false;
    tsc->zone = SCROLLHANDLE_BAR;
    text_scroll_apply(C, op, event);
  }

  WM_event_add_modal_handler(C, op);

  return OPERATOR_RUNNING_MODAL;
}

void TEXT_OT_scroll_bar(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Scrollbar";
  /* Don't really see the difference between this and scroll.
   * Both do basically the same thing (aside from key-maps). */
  ot->idname = "TEXT_OT_scroll_bar";

  /* API callbacks. */
  ot->invoke = text_scroll_bar_invoke;
  ot->modal = text_scroll_modal;
  ot->cancel = text_scroll_cancel;
  ot->poll = text_region_scroll_poll;

  /* Flags. */
  ot->flag = OPTYPE_BLOCKING | OPTYPE_INTERNAL;

  /* Properties. */
  PropertyRNA *prop;
  prop = RNA_def_int(
      ot->srna, "lines", 1, INT_MIN, INT_MAX, "Lines", "Number of lines to scroll", -100, 100);
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_TEXT);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Set Selection Operator
 * \{ */

struct SetSelection {
  int selc, sell;
  short mval_prev[2];
  wmTimer *timer; /* Needed for scrolling when mouse at region bounds. */
};

static int flatten_width(SpaceText *st, const char *str)
{
  int total = 0;

  for (int i = 0; str[i]; i += BLI_str_utf8_size_safe(str + i)) {
    const int columns = (str[i] == '\t') ? (st->tabnumber - total % st->tabnumber) :
                                           BLI_str_utf8_char_width_safe(str + i);
    total += columns;
  }

  return total;
}

static int flatten_column_to_offset(SpaceText *st, const char *str, int index)
{
  int i = 0, j = 0;

  while (*(str + j)) {
    const int col = (str[j] == '\t') ? (st->tabnumber - i % st->tabnumber) :
                                       BLI_str_utf8_char_width_safe(str + j);
    if (i + col > index) {
      break;
    }

    i += col;
    j += BLI_str_utf8_size_safe(str + j);
  }

  return j;
}

static TextLine *space_text_get_line_pos_wrapped(const SpaceText *st,
                                                 const ARegion *region,
                                                 int *y)
{
  TextLine *linep = static_cast<TextLine *>(st->text->lines.first);
  int i, lines;

  if (*y < -st->top) {
    return nullptr; /* We are beyond the first line... */
  }

  for (i = -st->top; i <= *y && linep; linep = linep->next, i += lines) {
    lines = space_text_get_visible_lines(st, region, linep->line);

    if (i + lines > *y) {
      /* We found the line matching given vertical *coordinate*,
       * now set y relative to this line's start. */
      *y -= i;
      break;
    }
  }
  return linep;
}

static void space_text_cursor_set_to_pos_wrapped(
    const SpaceText *st, const ARegion *region, int x, int y, const bool sel)
{
  Text *text = st->text;
  int max = space_text_wrap_width(st, region); /* Column. */
  int charp = -1;                              /* Mem. */
  bool found = false;                          /* Flags. */

  /* Point to line matching given y position, if any. */
  TextLine *linep = space_text_get_line_pos_wrapped(st, region, &y);

  if (linep) {
    int i = 0, start = 0, end = max; /* Column. */
    int j, curs = 0, endj = 0;       /* Mem. */
    bool chop = true;                /* Flags. */
    char ch;

    for (j = 0; !found && ((ch = linep->line[j]) != '\0');
         j += BLI_str_utf8_size_safe(linep->line + j))
    {
      int chars;
      const int columns = BLI_str_utf8_char_width_safe(linep->line + j); /* = 1 for tab. */

      /* Mimic replacement of tabs. */
      if (ch == '\t') {
        chars = st->tabnumber - i % st->tabnumber;
        ch = ' ';
      }
      else {
        chars = 1;
      }

      while (chars--) {
        /* Gone too far, go back to last wrap point. */
        if (y < 0) {
          charp = endj;
          y = 0;
          found = true;
          break;
          /* Exactly at the cursor. */
        }
        if (y == 0 && i - start <= x && i + columns - start > x) {
          /* Current position could be wrapped to next line. */
          /* This should be checked when end of current line would be reached. */
          charp = curs = j;
          found = true;
          /* Prepare curs for next wrap. */
        }
        else if (i - end <= x && i + columns - end > x) {
          curs = j;
        }
        if (i + columns - start > max) {
          end = std::min(end, i);

          if (found) {
            /* Exact cursor position was found, check if it's still on needed line
             * (hasn't been wrapped). */
            if (charp > endj && !chop && ch != '\0') {
              charp = endj;
            }
            break;
          }

          if (chop) {
            endj = j;
          }
          start = end;
          end += max;

          if (j < linep->len) {
            y--;
          }

          chop = true;
          if (y == 0 && i + columns - start > x) {
            charp = curs;
            found = true;
            break;
          }
        }
        else if (ELEM(ch, ' ', '-', '\0')) {
          if (found) {
            break;
          }

          if (y == 0 && i + columns - start > x) {
            charp = curs;
            found = true;
            break;
          }
          end = i + 1;
          endj = j;
          chop = false;
        }
        i += columns;
      }
    }

    BLI_assert(y == 0);

    if (!found) {
      /* On correct line but didn't meet cursor, must be at end. */
      charp = linep->len;
    }
  }
  else if (y < 0) { /* Before start of text. */
    linep = static_cast<TextLine *>(st->text->lines.first);
    charp = 0;
  }
  else { /* Beyond end of text. */
    linep = static_cast<TextLine *>(st->text->lines.last);
    charp = linep->len;
  }

  BLI_assert(linep && charp != -1);

  if (sel) {
    text->sell = linep;
    text->selc = charp;
  }
  else {
    text->curl = linep;
    text->curc = charp;
  }
}

static void text_cursor_set_to_pos(
    SpaceText *st, const ARegion *region, int x, int y, const bool sel)
{
  Text *text = st->text;
  space_text_update_character_width(st);
  y = (region->winy - 2 - y) / TXT_LINE_HEIGHT(st);

  x -= TXT_BODY_LEFT(st);
  x = std::max(x, 0);
  x = space_text_pixel_x_to_column(st, x) + st->left;

  if (st->wordwrap) {
    space_text_cursor_set_to_pos_wrapped(st, region, x, y, sel);
  }
  else {
    TextLine **linep;
    int *charp;
    int w;

    if (sel) {
      linep = &text->sell;
      charp = &text->selc;
    }
    else {
      linep = &text->curl;
      charp = &text->curc;
    }

    y -= txt_get_span(static_cast<TextLine *>(text->lines.first), *linep) - st->top;

    if (y > 0) {
      while (y-- != 0) {
        if ((*linep)->next) {
          *linep = (*linep)->next;
        }
      }
    }
    else if (y < 0) {
      while (y++ != 0) {
        if ((*linep)->prev) {
          *linep = (*linep)->prev;
        }
      }
    }

    w = flatten_width(st, (*linep)->line);
    if (x < w) {
      *charp = flatten_column_to_offset(st, (*linep)->line, x);
    }
    else {
      *charp = (*linep)->len;
    }
  }
  if (!sel) {
    txt_pop_sel(text);
  }
}

static void text_cursor_timer_ensure(bContext *C, SetSelection *ssel)
{
  if (ssel->timer == nullptr) {
    wmWindowManager *wm = CTX_wm_manager(C);
    wmWindow *win = CTX_wm_window(C);

    ssel->timer = WM_event_timer_add(wm, win, TIMER, 0.02f);
  }
}

static void text_cursor_timer_remove(bContext *C, SetSelection *ssel)
{
  if (ssel->timer) {
    wmWindowManager *wm = CTX_wm_manager(C);
    wmWindow *win = CTX_wm_window(C);

    WM_event_timer_remove(wm, win, ssel->timer);
  }
  ssel->timer = nullptr;
}

static void text_cursor_set_apply(bContext *C, wmOperator *op, const wmEvent *event)
{
  SpaceText *st = CTX_wm_space_text(C);
  ARegion *region = CTX_wm_region(C);
  SetSelection *ssel = static_cast<SetSelection *>(op->customdata);

  if (event->mval[1] < 0 || event->mval[1] > region->winy) {
    text_cursor_timer_ensure(C, ssel);

    if (event->type == TIMER) {
      text_cursor_set_to_pos(st, region, event->mval[0], event->mval[1], true);
      ED_space_text_scroll_to_cursor(st, region, false);
      WM_event_add_notifier(C, NC_TEXT | ND_CURSOR, st->text);
    }
  }
  else if (!st->wordwrap && (event->mval[0] < 0 || event->mval[0] > region->winx)) {
    text_cursor_timer_ensure(C, ssel);

    if (event->type == TIMER) {
      text_cursor_set_to_pos(
          st, region, std::clamp(event->mval[0], 0, int(region->winx)), event->mval[1], true);
      ED_space_text_scroll_to_cursor(st, region, false);
      WM_event_add_notifier(C, NC_TEXT | ND_CURSOR, st->text);
    }
  }
  else {
    text_cursor_timer_remove(C, ssel);

    if (event->type != TIMER) {
      text_cursor_set_to_pos(st, region, event->mval[0], event->mval[1], true);
      ED_space_text_scroll_to_cursor(st, region, false);
      WM_event_add_notifier(C, NC_TEXT | ND_CURSOR, st->text);

      ssel->mval_prev[0] = event->mval[0];
      ssel->mval_prev[1] = event->mval[1];
    }
  }
}

static void text_cursor_set_exit(bContext *C, wmOperator *op)
{
  SpaceText *st = CTX_wm_space_text(C);
  SetSelection *ssel = static_cast<SetSelection *>(op->customdata);

  space_text_update_cursor_moved(C);
  text_select_update_primary_clipboard(st->text);

  WM_event_add_notifier(C, NC_TEXT | ND_CURSOR, st->text);

  text_cursor_timer_remove(C, ssel);
  MEM_freeN(ssel);
}

static wmOperatorStatus text_selection_set_invoke(bContext *C,
                                                  wmOperator *op,
                                                  const wmEvent *event)
{
  SpaceText *st = CTX_wm_space_text(C);
  SetSelection *ssel;

  if (event->mval[0] >= st->runtime->scroll_region_handle.xmin) {
    return OPERATOR_PASS_THROUGH;
  }

  op->customdata = MEM_callocN(sizeof(SetSelection), "SetCursor");
  ssel = static_cast<SetSelection *>(op->customdata);

  ssel->mval_prev[0] = event->mval[0];
  ssel->mval_prev[1] = event->mval[1];

  ssel->sell = txt_get_span(static_cast<TextLine *>(st->text->lines.first), st->text->sell);
  ssel->selc = st->text->selc;

  WM_event_add_modal_handler(C, op);

  text_cursor_set_apply(C, op, event);

  return OPERATOR_RUNNING_MODAL;
}

static wmOperatorStatus text_selection_set_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  switch (event->type) {
    case LEFTMOUSE:
    case MIDDLEMOUSE:
    case RIGHTMOUSE:
      text_cursor_set_exit(C, op);
      return OPERATOR_FINISHED;
    case TIMER:
    case MOUSEMOVE:
      text_cursor_set_apply(C, op, event);
      break;
    default: {
      break;
    }
  }

  return OPERATOR_RUNNING_MODAL;
}

static void text_selection_set_cancel(bContext *C, wmOperator *op)
{
  text_cursor_set_exit(C, op);
}

void TEXT_OT_selection_set(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Set Selection";
  ot->idname = "TEXT_OT_selection_set";
  ot->description = "Set text selection";

  /* API callbacks. */
  ot->invoke = text_selection_set_invoke;
  ot->modal = text_selection_set_modal;
  ot->cancel = text_selection_set_cancel;
  ot->poll = text_region_edit_poll;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Set Cursor Operator
 * \{ */

static wmOperatorStatus text_cursor_set_exec(bContext *C, wmOperator *op)
{
  SpaceText *st = CTX_wm_space_text(C);
  ARegion *region = CTX_wm_region(C);
  int x = RNA_int_get(op->ptr, "x");
  int y = RNA_int_get(op->ptr, "y");

  text_cursor_set_to_pos(st, region, x, y, false);

  space_text_update_cursor_moved(C);
  WM_event_add_notifier(C, NC_TEXT | ND_CURSOR, st->text);

  return OPERATOR_PASS_THROUGH;
}

static wmOperatorStatus text_cursor_set_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  SpaceText *st = CTX_wm_space_text(C);

  if (event->mval[0] >= st->runtime->scroll_region_handle.xmin) {
    return OPERATOR_PASS_THROUGH;
  }

  RNA_int_set(op->ptr, "x", event->mval[0]);
  RNA_int_set(op->ptr, "y", event->mval[1]);

  return text_cursor_set_exec(C, op);
}

void TEXT_OT_cursor_set(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Set Cursor";
  ot->idname = "TEXT_OT_cursor_set";
  ot->description = "Set cursor position";

  /* API callbacks. */
  ot->invoke = text_cursor_set_invoke;
  ot->exec = text_cursor_set_exec;
  ot->poll = text_region_edit_poll;

  /* Properties. */
  RNA_def_int(ot->srna, "x", 0, INT_MIN, INT_MAX, "X", "", INT_MIN, INT_MAX);
  RNA_def_int(ot->srna, "y", 0, INT_MIN, INT_MAX, "Y", "", INT_MIN, INT_MAX);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Line Number Operator
 * \{ */

static wmOperatorStatus text_line_number_invoke(bContext *C,
                                                wmOperator * /*op*/,
                                                const wmEvent *event)
{
  SpaceText *st = CTX_wm_space_text(C);
  Text *text = CTX_data_edit_text(C);
  ARegion *region = CTX_wm_region(C);
  const int *mval = event->mval;
  double time;
  static int jump_to = 0;
  static double last_jump = 0;

  space_text_update_character_width(st);

  if (!st->showlinenrs) {
    return OPERATOR_PASS_THROUGH;
  }

  if (!(mval[0] > 2 &&
        mval[0] < (TXT_NUMCOL_WIDTH(st) + (TXT_BODY_LPAD * st->runtime->cwidth_px)) &&
        mval[1] > 2 && mval[1] < region->winy - 2))
  {
    return OPERATOR_PASS_THROUGH;
  }

  const char event_ascii = WM_event_utf8_to_ascii(event);
  if (!(event_ascii >= '0' && event_ascii <= '9')) {
    return OPERATOR_PASS_THROUGH;
  }

  time = BLI_time_now_seconds();
  if (last_jump < time - 1) {
    jump_to = 0;
  }

  jump_to *= 10;
  jump_to += int(event_ascii - '0');

  txt_move_toline(text, jump_to - 1, false);
  last_jump = time;

  space_text_update_cursor_moved(C);
  WM_event_add_notifier(C, NC_TEXT | ND_CURSOR, text);

  return OPERATOR_FINISHED;
}

void TEXT_OT_line_number(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Line Number";
  ot->idname = "TEXT_OT_line_number";
  ot->description = "The current line number";

  /* API callbacks. */
  ot->invoke = text_line_number_invoke;
  ot->poll = text_region_edit_poll;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Insert Operator
 * \{ */

static wmOperatorStatus text_insert_exec(bContext *C, wmOperator *op)
{
  SpaceText *st = CTX_wm_space_text(C);
  Text *text = CTX_data_edit_text(C);
  char *str;
  int str_len;
  bool done = false;
  size_t i = 0;
  uint code;

  space_text_drawcache_tag_update(st, false);

  str = RNA_string_get_alloc(op->ptr, "text", nullptr, 0, &str_len);

  ED_text_undo_push_init(C);

  if (st && st->overwrite) {
    while (str[i]) {
      code = BLI_str_utf8_as_unicode_step_safe(str, str_len, &i);
      done |= txt_replace_char(text, code);
    }
  }
  else {
    while (str[i]) {
      code = BLI_str_utf8_as_unicode_step_safe(str, str_len, &i);
      done |= txt_add_char(text, code);
    }
  }

  MEM_freeN(str);

  if (!done) {
    return OPERATOR_CANCELLED;
  }

  text_update_line_edited(text->curl);

  space_text_update_cursor_moved(C);
  WM_event_add_notifier(C, NC_TEXT | NA_EDITED, text);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus text_insert_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  SpaceText *st = CTX_wm_space_text(C);
  wmOperatorStatus ret;

  /* Auto-close variables. */
  bool do_auto_close = false;
  bool do_auto_close_select = false;

  uint auto_close_char_input = 0;
  uint auto_close_char_match = 0;
  /* Variables needed to restore the selection when auto-closing around an existing selection. */
  struct {
    TextLine *sell;
    TextLine *curl;
    int selc;
    int curc;
  } auto_close_select = {nullptr}, auto_close_select_backup = {nullptr};

  /* NOTE: the "text" property is always set from key-map,
   * so we can't use #RNA_struct_property_is_set, check the length instead. */
  if (!RNA_string_length(op->ptr, "text")) {
    /* If Alt/Control/Super are pressed pass through except for UTF8 character event
     * (when input method are used for UTF8 inputs, the user may assign key event
     * including Alt/Control/Super like Control-M to commit UTF8 string.
     * In such case, the modifiers in the UTF8 character event make no sense). */
    if ((event->modifier & (KM_CTRL | KM_OSKEY)) && !event->utf8_buf[0]) {
      return OPERATOR_PASS_THROUGH;
    }

    char str[BLI_UTF8_MAX + 1];
    const size_t len = BLI_str_utf8_size_safe(event->utf8_buf);
    memcpy(str, event->utf8_buf, len);
    str[len] = '\0';
    RNA_string_set(op->ptr, "text", str);

    if (U.text_flag & USER_TEXT_EDIT_AUTO_CLOSE) {
      auto_close_char_input = BLI_str_utf8_as_unicode_or_error(str);
      if (isascii(auto_close_char_input)) {
        auto_close_char_match = text_closing_character_pair_get(auto_close_char_input);
        if (auto_close_char_match != 0) {
          do_auto_close = true;

          if (txt_has_sel(st->text) &&
              !text_span_is_blank(st->text->sell, st->text->selc, st->text->curl, st->text->curc))
          {
            do_auto_close_select = true;

            auto_close_select_backup.curl = st->text->curl;
            auto_close_select_backup.curc = st->text->curc;
            auto_close_select_backup.sell = st->text->sell;
            auto_close_select_backup.selc = st->text->selc;

            /* Movers the cursor to the start of the selection. */
            txt_order_cursors(st->text, false);

            auto_close_select.curl = st->text->curl;
            auto_close_select.curc = st->text->curc;
            auto_close_select.sell = st->text->sell;
            auto_close_select.selc = st->text->selc;

            txt_pop_sel(st->text);
          }
        }
      }
    }
  }

  ret = text_insert_exec(C, op);

  if (do_auto_close) {
    if (ret == OPERATOR_FINISHED) {
      const int auto_close_char_len = BLI_str_utf8_from_unicode_len(auto_close_char_input);
      /* If there was a selection, move cursor to the end of it. */
      if (do_auto_close_select) {
        /* Update the value in-place as needed. */
        if (auto_close_select.curl == auto_close_select.sell) {
          auto_close_select.selc += auto_close_char_len;
        }
        /* Move the cursor to the end of the selection. */
        st->text->curl = auto_close_select.sell;
        st->text->curc = auto_close_select.selc;
        txt_pop_sel(st->text);
      }

      txt_add_char(st->text, auto_close_char_match);
      txt_move_left(st->text, false);

      /* If there was a selection, restore it. */
      if (do_auto_close_select) {
        /* Mark the selection as edited. */
        if (auto_close_select.curl != auto_close_select.sell) {
          TextLine *line = auto_close_select.curl;
          do {
            line = line->next;
            text_update_line_edited(line);
          } while (line != auto_close_select.sell);
        }
        st->text->curl = auto_close_select.curl;
        st->text->curc = auto_close_select.curc + auto_close_char_len;
        st->text->sell = auto_close_select.sell;
        st->text->selc = auto_close_select.selc;
      }
    }
    else {
      /* If nothing was done & the selection was removed, restore the selection. */
      if (do_auto_close_select) {
        st->text->curl = auto_close_select_backup.curl;
        st->text->curc = auto_close_select_backup.curc;
        st->text->sell = auto_close_select_backup.sell;
        st->text->selc = auto_close_select_backup.selc;
      }
    }
  }

  /* Run the script while editing, evil but useful. */
  if (ret == OPERATOR_FINISHED && st->live_edit) {
    text_run_script(C, nullptr);
  }

  return ret;
}

void TEXT_OT_insert(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* Identifiers. */
  ot->name = "Insert";
  ot->idname = "TEXT_OT_insert";
  ot->description = "Insert text at cursor position";

  /* API callbacks. */
  ot->exec = text_insert_exec;
  ot->invoke = text_insert_invoke;
  ot->poll = text_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_UNDO;

  /* Properties. */
  prop = RNA_def_string(
      ot->srna, "text", nullptr, 0, "Text", "Text to insert at the cursor position");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Find Operator
 * \{ */

/* Mode. */
enum {
  TEXT_FIND = 0,
  TEXT_REPLACE = 1,
};

static wmOperatorStatus text_find_and_replace(bContext *C, wmOperator *op, short mode)
{
  Main *bmain = CTX_data_main(C);
  SpaceText *st = CTX_wm_space_text(C);
  Text *text = st->text;
  int flags;
  bool found = false;
  char *tmp;

  if (!st->findstr[0]) {
    return OPERATOR_CANCELLED;
  }

  flags = st->flags;
  if (flags & ST_FIND_ALL) {
    flags &= ~ST_FIND_WRAP;
  }

  /* Replace current. */
  if (mode != TEXT_FIND && txt_has_sel(text)) {
    tmp = txt_sel_to_buf(text, nullptr);

    if (flags & ST_MATCH_CASE) {
      found = STREQ(st->findstr, tmp);
    }
    else {
      found = BLI_strcasecmp(st->findstr, tmp) == 0;
    }

    if (found) {
      if (mode == TEXT_REPLACE) {
        ED_text_undo_push_init(C);
        txt_insert_buf(text, st->replacestr, strlen(st->replacestr));
        if (text->curl && text->curl->format) {
          MEM_freeN(text->curl->format);
          text->curl->format = nullptr;
        }
        space_text_update_cursor_moved(C);
        WM_event_add_notifier(C, NC_TEXT | NA_EDITED, text);
        space_text_drawcache_tag_update(st, true);
      }
    }
    MEM_freeN(tmp);
    tmp = nullptr;
  }

  /* Find next. */
  if (txt_find_string(text, st->findstr, flags & ST_FIND_WRAP, flags & ST_MATCH_CASE)) {
    space_text_update_cursor_moved(C);
    WM_event_add_notifier(C, NC_TEXT | ND_CURSOR, text);
  }
  else if (flags & ST_FIND_ALL) {
    if (text->id.next) {
      text = st->text = static_cast<Text *>(text->id.next);
    }
    else {
      text = st->text = static_cast<Text *>(bmain->texts.first);
    }
    txt_move_toline(text, 0, false);
    space_text_update_cursor_moved(C);
    WM_event_add_notifier(C, NC_TEXT | ND_CURSOR, text);
  }
  else {
    if (!found) {
      BKE_reportf(op->reports, RPT_INFO, "Text not found: %s", st->findstr);
    }
  }

  return OPERATOR_FINISHED;
}

static wmOperatorStatus text_find_exec(bContext *C, wmOperator *op)
{
  return text_find_and_replace(C, op, TEXT_FIND);
}

void TEXT_OT_find(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Find Next";
  ot->idname = "TEXT_OT_find";
  ot->description = "Find specified text";

  /* API callbacks. */
  ot->exec = text_find_exec;
  ot->poll = text_space_edit_poll;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Replace Operator
 * \{ */

static wmOperatorStatus text_replace_all(bContext *C)
{
  SpaceText *st = CTX_wm_space_text(C);
  Text *text = st->text;
  const int flags = st->flags;
  bool found = false;

  if (!st->findstr[0]) {
    return OPERATOR_CANCELLED;
  }

  const int orig_curl = BLI_findindex(&text->lines, text->curl);
  const int orig_curc = text->curc;
  bool has_sel = txt_has_sel(text);

  txt_move_toline(text, 0, false);

  found = txt_find_string(text, st->findstr, 0, flags & ST_MATCH_CASE);
  if (found) {
    ED_text_undo_push_init(C);

    do {
      txt_insert_buf(text, st->replacestr, strlen(st->replacestr));
      if (text->curl && text->curl->format) {
        MEM_freeN(text->curl->format);
        text->curl->format = nullptr;
      }
      found = txt_find_string(text, st->findstr, 0, flags & ST_MATCH_CASE);
    } while (found);

    WM_event_add_notifier(C, NC_TEXT | NA_EDITED, text);
    space_text_drawcache_tag_update(st, true);
  }
  else {
    /* Restore position. */
    txt_move_to(text, orig_curl, orig_curc, has_sel);
    return OPERATOR_CANCELLED;
  }

  return OPERATOR_FINISHED;
}

static wmOperatorStatus text_replace_exec(bContext *C, wmOperator *op)
{
  bool replace_all = RNA_boolean_get(op->ptr, "all");
  if (replace_all) {
    return text_replace_all(C);
  }
  return text_find_and_replace(C, op, TEXT_REPLACE);
}

void TEXT_OT_replace(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Replace";
  ot->idname = "TEXT_OT_replace";
  ot->description = "Replace text with the specified text";

  /* API callbacks. */
  ot->exec = text_replace_exec;
  ot->poll = text_space_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_UNDO;

  /* Properties. */
  PropertyRNA *prop;
  prop = RNA_def_boolean(ot->srna, "all", false, "Replace All", "Replace all occurrences");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Find Set Selected
 * \{ */

static wmOperatorStatus text_find_set_selected_exec(bContext *C, wmOperator *op)
{
  SpaceText *st = CTX_wm_space_text(C);
  Text *text = CTX_data_edit_text(C);
  char *tmp;

  tmp = txt_sel_to_buf(text, nullptr);
  STRNCPY_UTF8(st->findstr, tmp);
  MEM_freeN(tmp);

  if (!st->findstr[0]) {
    return OPERATOR_FINISHED;
  }

  return text_find_and_replace(C, op, TEXT_FIND);
}

void TEXT_OT_find_set_selected(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Find & Set Selection";
  ot->idname = "TEXT_OT_find_set_selected";
  ot->description = "Find specified text and set as selected";

  /* API callbacks. */
  ot->exec = text_find_set_selected_exec;
  ot->poll = text_space_edit_poll;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Replace Set Selected
 * \{ */

static wmOperatorStatus text_replace_set_selected_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceText *st = CTX_wm_space_text(C);
  Text *text = CTX_data_edit_text(C);
  char *tmp;

  tmp = txt_sel_to_buf(text, nullptr);
  STRNCPY_UTF8(st->replacestr, tmp);
  MEM_freeN(tmp);

  return OPERATOR_FINISHED;
}

void TEXT_OT_replace_set_selected(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Replace & Set Selection";
  ot->idname = "TEXT_OT_replace_set_selected";
  ot->description = "Replace text with specified text and set as selected";

  /* API callbacks. */
  ot->exec = text_replace_set_selected_exec;
  ot->poll = text_space_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Jump to File at Point
 * \{ */

static bool text_jump_to_file_at_point_external(bContext *C,
                                                ReportList *reports,
                                                const char *filepath,
                                                const int line_index,
                                                const int column_index)
{
  bool success = false;
#ifdef WITH_PYTHON
  BPy_RunErrInfo err_info = {};
  err_info.reports = reports;
  err_info.report_prefix = "External editor";

  const char *expr_imports[] = {"_bl_text_utils", "_bl_text_utils.external_editor", "os", nullptr};
  std::string expr;
  {
    std::stringstream expr_stream;
    expr_stream << "_bl_text_utils.external_editor.open_external_editor(os.fsdecode(b'";
    for (const char *ch = filepath; *ch; ch++) {
      expr_stream << "\\x" << std::hex << int(*ch);
    }
    expr_stream << "'), " << std::dec << line_index << ", " << std::dec << column_index << ")";
    expr = expr_stream.str();
  }

  char *expr_result = nullptr;
  if (BPY_run_string_as_string(C, expr_imports, expr.c_str(), &err_info, &expr_result)) {
    /* No error. */
    if (expr_result[0] == '\0') {
      BKE_reportf(
          reports, RPT_INFO, "See '%s' in the external editor", BLI_path_basename(filepath));
      success = true;
    }
    else {
      BKE_report(reports, RPT_ERROR, expr_result);
    }
    MEM_freeN(expr_result);
  }
#else
  UNUSED_VARS(C, reports, filepath, line_index, column_index);
#endif /* WITH_PYTHON */
  return success;
}

static bool text_jump_to_file_at_point_internal(bContext *C,
                                                ReportList *reports,
                                                const char *filepath,
                                                const int line_index,
                                                const int column_index)
{
  Main *bmain = CTX_data_main(C);
  Text *text = nullptr;
  BLI_assert(!BLI_path_is_rel(filepath));

  LISTBASE_FOREACH (Text *, text_iter, &bmain->texts) {
    if (text_iter->filepath == nullptr) {
      continue;
    }
    const char *filepath_iter;
    char filepath_iter_buf[FILE_MAX];
    if (BLI_path_is_rel(text_iter->filepath)) {
      STRNCPY(filepath_iter_buf, text_iter->filepath);
      BLI_path_abs(filepath_iter_buf, ID_BLEND_PATH(bmain, &text_iter->id));
      filepath_iter = filepath_iter_buf;
    }
    else {
      filepath_iter = text_iter->filepath;
    }

    if (BLI_path_cmp(filepath, filepath_iter) == 0) {
      text = text_iter;
      break;
    }
  }

  if (text == nullptr) {
    text = BKE_text_load(bmain, filepath, BKE_main_blendfile_path(bmain));
  }

  if (text == nullptr) {
    BKE_reportf(reports, RPT_WARNING, "File '%s' cannot be opened", filepath);
    return false;
  }

  txt_move_to(text, line_index, column_index, false);

  /* NOTE(@ideasman42): it's bad practice that this operator searches for the text area to set.
   * not a good precedent, since this is a developer tool allow it. */
  if (!ED_text_activate_in_screen(C, text)) {
    BKE_reportf(reports, RPT_INFO, "See '%s' in the text editor", text->id.name + 2);
  }

  WM_event_add_notifier(C, NC_TEXT | ND_CURSOR, text);

  return true;
}

static wmOperatorStatus text_jump_to_file_at_point_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  PropertyRNA *prop_filepath = RNA_struct_find_property(op->ptr, "filepath");
  PropertyRNA *prop_line = RNA_struct_find_property(op->ptr, "line");
  PropertyRNA *prop_column = RNA_struct_find_property(op->ptr, "column");

  if (!RNA_property_is_set(op->ptr, prop_filepath)) {
    if (const Text *text = CTX_data_edit_text(C)) {
      if (text->filepath != nullptr) {
        const TextLine *line = text->curl;
        const int line_index = BLI_findindex(&text->lines, text->curl);
        const int column_index = BLI_str_utf8_offset_to_index(line->line, line->len, text->curc);

        char filepath[FILE_MAX];
        STRNCPY(filepath, text->filepath);
        BLI_path_abs(filepath, ID_BLEND_PATH(bmain, &text->id));
        RNA_property_string_set(op->ptr, prop_filepath, filepath);
        RNA_property_int_set(op->ptr, prop_line, line_index);
        RNA_property_int_set(op->ptr, prop_column, column_index);
      }
    }
  }

  char filepath[FILE_MAX];
  RNA_property_string_get(op->ptr, prop_filepath, filepath);
  if (UNLIKELY(BLI_path_is_rel(filepath))) {
    BLI_path_abs(filepath, BKE_main_blendfile_path(bmain));
  }
  const int line_index = RNA_property_int_get(op->ptr, prop_line);
  const int column_index = RNA_property_int_get(op->ptr, prop_column);

  if (filepath[0] == '\0') {
    BKE_report(op->reports, RPT_WARNING, "File path property not set");
    return OPERATOR_CANCELLED;
  }

  /* Useful to copy-paste from the terminal. */
  printf("%s:%d:%d\n", filepath, line_index + 1, column_index);

  bool success;
  if (U.text_editor[0] != '\0') {
    success = text_jump_to_file_at_point_external(
        C, op->reports, filepath, line_index, column_index);
  }
  else {
    success = text_jump_to_file_at_point_internal(
        C, op->reports, filepath, line_index, column_index);
  }

  return success ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

void TEXT_OT_jump_to_file_at_point(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* Identifiers. */
  ot->name = "Jump to File at Point";
  ot->idname = "TEXT_OT_jump_to_file_at_point";
  ot->description = "Jump to a file for the text editor";

  /* API callbacks. */
  ot->exec = text_jump_to_file_at_point_exec;

  /* Flags. */
  ot->flag = 0;

  prop = RNA_def_string(ot->srna, "filepath", nullptr, FILE_MAX, "Filepath", "");
  RNA_def_property_subtype(prop, PROP_FILEPATH);
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  prop = RNA_def_int(ot->srna, "line", 0, 0, INT_MAX, "Line", "Line to jump to", 1, 10000);
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_TEXT);
  prop = RNA_def_int(ot->srna, "column", 0, 0, INT_MAX, "Column", "Column to jump to", 1, 10000);
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Resolve Conflict Operator
 * \{ */

enum { RESOLVE_IGNORE, RESOLVE_RELOAD, RESOLVE_SAVE, RESOLVE_MAKE_INTERNAL };
static const EnumPropertyItem resolution_items[] = {
    {RESOLVE_IGNORE, "IGNORE", 0, "Ignore", ""},
    {RESOLVE_RELOAD, "RELOAD", 0, "Reload", ""},
    {RESOLVE_SAVE, "SAVE", 0, "Save", ""},
    {RESOLVE_MAKE_INTERNAL, "MAKE_INTERNAL", 0, "Make Internal", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static bool text_resolve_conflict_poll(bContext *C)
{
  Text *text = CTX_data_edit_text(C);

  if (!text_edit_poll(C)) {
    return false;
  }

  return ((text->filepath != nullptr) && !(text->flags & TXT_ISMEM));
}

static wmOperatorStatus text_resolve_conflict_exec(bContext *C, wmOperator *op)
{
  Text *text = CTX_data_edit_text(C);
  int resolution = RNA_enum_get(op->ptr, "resolution");

  switch (resolution) {
    case RESOLVE_RELOAD:
      return text_reload_exec(C, op);
    case RESOLVE_SAVE:
      return text_save_exec(C, op);
    case RESOLVE_MAKE_INTERNAL:
      return text_make_internal_exec(C, op);
    case RESOLVE_IGNORE:
      BKE_text_file_modified_ignore(text);
      return OPERATOR_FINISHED;
  }

  return OPERATOR_CANCELLED;
}

static wmOperatorStatus text_resolve_conflict_invoke(bContext *C,
                                                     wmOperator *op,
                                                     const wmEvent * /*event*/)
{
  Text *text = CTX_data_edit_text(C);
  uiPopupMenu *pup;
  uiLayout *layout;

  switch (BKE_text_file_modified_check(text)) {
    case 1:
      if (text->flags & TXT_ISDIRTY) {
        /* Modified locally and externally, ah. offer more possibilities. */
        pup = UI_popup_menu_begin(
            C, IFACE_("File Modified Outside and Inside Blender"), ICON_NONE);
        layout = UI_popup_menu_layout(pup);
        PointerRNA op_ptr = layout->op(
            op->type, IFACE_("Reload from disk (ignore local changes)"), ICON_NONE);
        RNA_enum_set(&op_ptr, "resolution", RESOLVE_RELOAD);
        op_ptr = layout->op(op->type, IFACE_("Save to disk (ignore outside changes)"), ICON_NONE);
        RNA_enum_set(&op_ptr, "resolution", RESOLVE_SAVE);
        op_ptr = layout->op(op->type, IFACE_("Make text internal (separate copy)"), ICON_NONE);
        RNA_enum_set(&op_ptr, "resolution", RESOLVE_MAKE_INTERNAL);
        UI_popup_menu_end(C, pup);
      }
      else {
        pup = UI_popup_menu_begin(C, IFACE_("File Modified Outside Blender"), ICON_NONE);
        layout = UI_popup_menu_layout(pup);
        PointerRNA op_ptr = layout->op(op->type, IFACE_("Reload from disk"), ICON_NONE);
        RNA_enum_set(&op_ptr, "resolution", RESOLVE_RELOAD);
        op_ptr = layout->op(op->type, IFACE_("Make text internal (separate copy)"), ICON_NONE);
        RNA_enum_set(&op_ptr, "resolution", RESOLVE_MAKE_INTERNAL);
        op_ptr = layout->op(op->type, IFACE_("Ignore"), ICON_NONE);
        RNA_enum_set(&op_ptr, "resolution", RESOLVE_IGNORE);
        UI_popup_menu_end(C, pup);
      }
      break;
    case 2:
      pup = UI_popup_menu_begin(C, IFACE_("File Deleted Outside Blender"), ICON_NONE);
      layout = UI_popup_menu_layout(pup);
      PointerRNA op_ptr = layout->op(op->type, IFACE_("Make text internal"), ICON_NONE);
      RNA_enum_set(&op_ptr, "resolution", RESOLVE_MAKE_INTERNAL);
      op_ptr = layout->op(op->type, IFACE_("Recreate file"), ICON_NONE);
      RNA_enum_set(&op_ptr, "resolution", RESOLVE_SAVE);
      UI_popup_menu_end(C, pup);
      break;
  }

  return OPERATOR_INTERFACE;
}

void TEXT_OT_resolve_conflict(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Resolve Conflict";
  ot->idname = "TEXT_OT_resolve_conflict";
  ot->description = "When external text is out of sync, resolve the conflict";

  /* API callbacks. */
  ot->exec = text_resolve_conflict_exec;
  ot->invoke = text_resolve_conflict_invoke;
  ot->poll = text_resolve_conflict_poll;

  /* Properties. */
  RNA_def_enum(ot->srna,
               "resolution",
               resolution_items,
               RESOLVE_IGNORE,
               "Resolution",
               "How to solve conflict due to differences in internal and external text");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name To 3D Object Operator
 * \{ */

static wmOperatorStatus text_to_3d_object_exec(bContext *C, wmOperator *op)
{
  const Text *text = CTX_data_edit_text(C);
  const bool split_lines = RNA_boolean_get(op->ptr, "split_lines");

  ED_text_to_object(C, text, split_lines);

  return OPERATOR_FINISHED;
}

void TEXT_OT_to_3d_object(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "To 3D Object";
  ot->idname = "TEXT_OT_to_3d_object";
  ot->description = "Create 3D text object from active text data-block";

  /* API callbacks. */
  ot->exec = text_to_3d_object_exec;
  ot->poll = text_data_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Properties. */
  RNA_def_boolean(
      ot->srna, "split_lines", false, "Split Lines", "Create one object per line in the text");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shader Update
 * \{ */

static bool text_update_shader_poll(bContext *C)
{
  RenderEngineType *type = CTX_data_engine_type(C);
  const Text *text = CTX_data_edit_text(C);

  /* See if we have a text datablock in context. */
  if (text == nullptr) {
    return false;
  }

  /* Test if we have a render engine that supports shaders scripts. */
  if (!(type && (type->update_script_node || type->update_custom_camera))) {
    return false;
  }

  /* We don't check if text datablock is actually in use, too slow for poll. */
  return true;
}

/** Recursively check for script nodes in groups using this text and update. */
static bool text_update_shader_text_recursive(RenderEngine *engine,
                                              RenderEngineType *type,
                                              bNodeTree *ntree,
                                              Text *text,
                                              VectorSet<bNodeTree *> &done_trees)
{
  bool found = false;

  done_trees.add_new(ntree);

  /* Update each script that is using this text datablock. */
  for (bNode *node : ntree->all_nodes()) {
    if (node->type_legacy == NODE_GROUP) {
      bNodeTree *ngroup = (bNodeTree *)node->id;
      if (ngroup && !done_trees.contains(ngroup)) {
        found |= text_update_shader_text_recursive(engine, type, ngroup, text, done_trees);
      }
    }
    else if (node->type_legacy == SH_NODE_SCRIPT && node->id == &text->id) {
      type->update_script_node(engine, ntree, node);
      found = true;
    }
  }

  return found;
}

static wmOperatorStatus text_update_shader_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  RenderEngineType *type = CTX_data_engine_type(C);
  Text *text = CTX_data_edit_text(C);
  bool found = false;

  /* Setup render engine. */
  RenderEngine *engine = RE_engine_create(type);
  engine->reports = op->reports;

  /* Update all nodes using text datablock. */
  if (type->update_script_node != nullptr) {
    VectorSet<bNodeTree *> done_trees;
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_SHADER) {
        if (!done_trees.contains(ntree)) {
          found |= text_update_shader_text_recursive(engine, type, ntree, text, done_trees);
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  /* Update all cameras using text data-block. */
  if (type->update_custom_camera != nullptr) {
    LISTBASE_FOREACH (Camera *, cam, &bmain->cameras) {
      if (cam->custom_shader == text) {
        type->update_custom_camera(engine, cam);
        found = true;
      }
    }
  }

  if (!found) {
    BKE_report(op->reports, RPT_INFO, "Text not used by any node or camera, no update done");
  }

  RE_engine_free(engine);

  return (found) ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

void TEXT_OT_update_shader(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Shader Update";
  ot->description =
      "Update users of this shader, such as custom cameras and script nodes, with its new sockets "
      "and options";
  ot->idname = "TEXT_OT_update_shader";

  /* API callbacks. */
  ot->exec = text_update_shader_exec;
  ot->poll = text_update_shader_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */
