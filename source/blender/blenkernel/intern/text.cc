/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cstdlib> /* abort */
#include <cstring> /* strstr */
#include <cwctype>
#include <optional>
#include <sys/stat.h>
#include <sys/types.h>

#include "MEM_guardedalloc.h"

#include "BLI_fileops.h"
#include "BLI_listbase.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_string_cursor_utf8.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "DNA_text_types.h"
#include "DNA_userdef_types.h"

#include "BKE_bpath.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_text.h"

#include "BLO_read_write.hh"

#ifdef WITH_PYTHON
#  include "BPY_extern.hh"
#endif

/* -------------------------------------------------------------------- */
/** \name Prototypes
 * \{ */

static void txt_pop_first(Text *text);
static void txt_pop_last(Text *text);
static void txt_delete_line(Text *text, TextLine *line);
static void txt_delete_sel(Text *text);
static void txt_make_dirty(Text *text);

static TextLine *txt_line_malloc() ATTR_MALLOC ATTR_WARN_UNUSED_RESULT;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Text Data-Block
 * \{ */

static void text_init_data(ID *id)
{
  Text *text = (Text *)id;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(text, id));

  text->filepath = nullptr;

  text->flags = TXT_ISDIRTY | TXT_ISMEM;
  if ((U.flag & USER_TXT_TABSTOSPACES_DISABLE) == 0) {
    text->flags |= TXT_TABSTOSPACES;
  }

  BLI_listbase_clear(&text->lines);

  TextLine *tmp = txt_line_malloc();
  tmp->line = MEM_malloc_arrayN<char>(1, "textline_string");
  tmp->format = nullptr;

  tmp->line[0] = 0;
  tmp->len = 0;

  tmp->next = nullptr;
  tmp->prev = nullptr;

  BLI_addhead(&text->lines, tmp);

  text->curl = static_cast<TextLine *>(text->lines.first);
  text->curc = 0;
  text->sell = static_cast<TextLine *>(text->lines.first);
  text->selc = 0;
}

/**
 * Only copy internal data of Text ID from source
 * to already allocated/initialized destination.
 * You probably never want to use that directly,
 * use #BKE_id_copy or #BKE_id_copy_ex for typical needs.
 *
 * WARNING! This function will not handle ID user count!
 *
 * \param flag: Copying options (see BKE_lib_id.hh's LIB_ID_COPY_... flags for more).
 */
static void text_copy_data(Main * /*bmain*/,
                           std::optional<Library *> /*owner_library*/,
                           ID *id_dst,
                           const ID *id_src,
                           const int /*flag*/)
{
  Text *text_dst = (Text *)id_dst;
  const Text *text_src = (Text *)id_src;

  /* File name can be nullptr. */
  if (text_src->filepath) {
    text_dst->filepath = BLI_strdup(text_src->filepath);
  }

  text_dst->flags |= TXT_ISDIRTY;

  BLI_listbase_clear(&text_dst->lines);
  text_dst->curl = text_dst->sell = nullptr;
  text_dst->compiled = nullptr;

  /* Walk down, reconstructing. */
  LISTBASE_FOREACH (TextLine *, line_src, &text_src->lines) {
    TextLine *line_dst = txt_line_malloc();

    line_dst->line = BLI_strdupn(line_src->line, line_src->len);
    line_dst->len = line_src->len;
    line_dst->format = nullptr;

    BLI_addtail(&text_dst->lines, line_dst);
  }

  text_dst->curl = text_dst->sell = static_cast<TextLine *>(text_dst->lines.first);
  text_dst->curc = text_dst->selc = 0;
}

/** Free (or release) any data used by this text (does not free the text itself). */
static void text_free_data(ID *id)
{
  /* No animation-data here. */
  Text *text = (Text *)id;

  BKE_text_free_lines(text);

  MEM_SAFE_FREE(text->filepath);
#ifdef WITH_PYTHON
  BPY_text_free_code(text);
#endif
}

static void text_foreach_path(ID *id, BPathForeachPathData *bpath_data)
{
  Text *text = (Text *)id;

  if (text->filepath != nullptr && text->filepath[0] != '\0') {
    BKE_bpath_foreach_path_allocated_process(bpath_data, &text->filepath);
  }
}

static void text_blend_write(BlendWriter *writer, ID *id, const void *id_address)
{
  Text *text = (Text *)id;

  /* NOTE: we are clearing local temp data here, *not* the flag in the actual 'real' ID. */
  if ((text->flags & TXT_ISMEM) && (text->flags & TXT_ISEXT)) {
    text->flags &= ~TXT_ISEXT;
  }

  /* Clean up, important in undo case to reduce false detection of changed data-blocks. */
  text->compiled = nullptr;

  /* write LibData */
  BLO_write_id_struct(writer, Text, id_address, &text->id);
  BKE_id_blend_write(writer, &text->id);

  if (text->filepath) {
    BLO_write_string(writer, text->filepath);
  }

  if (!(text->flags & TXT_ISEXT)) {
    /* Now write the text data, in two steps for optimization in the read-function. */
    LISTBASE_FOREACH (TextLine *, tmp, &text->lines) {
      BLO_write_struct(writer, TextLine, tmp);
    }

    LISTBASE_FOREACH (TextLine *, tmp, &text->lines) {
      BLO_write_string(writer, tmp->line);
    }
  }
}

static void text_blend_read_data(BlendDataReader *reader, ID *id)
{
  Text *text = (Text *)id;
  BLO_read_string(reader, &text->filepath);

  text->compiled = nullptr;

#if 0
  if (text->flags & TXT_ISEXT) {
    BKE_text_reload(text);
  }
/* else { */
#endif

  BLO_read_struct_list(reader, TextLine, &text->lines);

  BLO_read_struct(reader, TextLine, &text->curl);
  BLO_read_struct(reader, TextLine, &text->sell);

  LISTBASE_FOREACH (TextLine *, ln, &text->lines) {
    BLO_read_string(reader, &ln->line);
    ln->format = nullptr;

    if (ln->len != int(strlen(ln->line))) {
      printf("Error loading text, line lengths differ\n");
      ln->len = strlen(ln->line);
    }
  }

  text->flags = (text->flags) & ~TXT_ISEXT;
}

IDTypeInfo IDType_ID_TXT = {
    /*id_code*/ Text::id_type,
    /*id_filter*/ FILTER_ID_TXT,
    /*dependencies_id_types*/ 0,
    /*main_listbase_index*/ INDEX_ID_TXT,
    /*struct_size*/ sizeof(Text),
    /*name*/ "Text",
    /*name_plural*/ N_("texts"),
    /*translation_context*/ BLT_I18NCONTEXT_ID_TEXT,
    /*flags*/ IDTYPE_FLAGS_NO_ANIMDATA | IDTYPE_FLAGS_APPEND_IS_REUSABLE,
    /*asset_type_info*/ nullptr,

    /*init_data*/ text_init_data,
    /*copy_data*/ text_copy_data,
    /*free_data*/ text_free_data,
    /*make_local*/ nullptr,
    /*foreach_id*/ nullptr,
    /*foreach_cache*/ nullptr,
    /*foreach_path*/ text_foreach_path,
    /*foreach_working_space_color*/ nullptr,
    /*owner_pointer_get*/ nullptr,

    /*blend_write*/ text_blend_write,
    /*blend_read_data*/ text_blend_read_data,
    /*blend_read_after_liblink*/ nullptr,

    /*blend_read_undo_preserve*/ nullptr,

    /*lib_override_apply_post*/ nullptr,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Text Add, Free, Validation
 * \{ */

void BKE_text_free_lines(Text *text)
{
  for (TextLine *tmp = static_cast<TextLine *>(text->lines.first), *tmp_next; tmp; tmp = tmp_next)
  {
    tmp_next = tmp->next;
    MEM_freeN(tmp->line);
    if (tmp->format) {
      MEM_freeN(tmp->format);
    }
    MEM_freeN(tmp);
  }

  BLI_listbase_clear(&text->lines);

  text->curl = text->sell = nullptr;
}

Text *BKE_text_add(Main *bmain, const char *name)
{
  Text *ta;

  ta = BKE_id_new<Text>(bmain, name);
  /* Texts have no users by default... Set the fake user flag to ensure that this text block
   * doesn't get deleted by default when cleaning up data blocks. */
  id_us_min(&ta->id);
  id_fake_user_set(&ta->id);

  return ta;
}

int txt_extended_ascii_as_utf8(char **str)
{
  ptrdiff_t bad_char, i = 0;
  const ptrdiff_t length = ptrdiff_t(strlen(*str));
  int added = 0;

  while ((*str)[i]) {
    if ((bad_char = BLI_str_utf8_invalid_byte(*str + i, length - i)) == -1) {
      break;
    }

    added++;
    i += bad_char + 1;
  }

  if (added != 0) {
    char *newstr = MEM_malloc_arrayN<char>(size_t(length) + size_t(added) + 1, "text_line");
    ptrdiff_t mi = 0;
    i = 0;

    while ((*str)[i]) {
      if ((bad_char = BLI_str_utf8_invalid_byte((*str) + i, length - i)) == -1) {
        memcpy(newstr + mi, (*str) + i, length - i + 1);
        break;
      }

      memcpy(newstr + mi, (*str) + i, bad_char);

      const int mofs = mi + bad_char;
      BLI_str_utf8_from_unicode((*str)[i + bad_char], newstr + mofs, (length + added) - mofs);
      i += bad_char + 1;
      mi += bad_char + 2;
    }
    newstr[length + added] = '\0';
    MEM_freeN(*str);
    *str = newstr;
  }

  return added;
}

/**
 * Removes any control characters from a text-line and fixes invalid UTF8 sequences.
 */
static void cleanup_textline(TextLine *tl)
{
  int i;

  for (i = 0; i < tl->len; i++) {
    if (tl->line[i] < ' ' && tl->line[i] != '\t') {
      memmove(tl->line + i, tl->line + i + 1, tl->len - i);
      tl->len--;
      i--;
    }
  }
  tl->len += txt_extended_ascii_as_utf8(&tl->line);
}

/**
 * used for load and reload (unlike txt_insert_buf)
 * assumes all fields are empty
 */
static void text_from_buf(Text *text, const uchar *buffer, const int len)
{
  int i, llen, lines_count;

  BLI_assert(BLI_listbase_is_empty(&text->lines));

  llen = 0;
  lines_count = 0;
  for (i = 0; i < len; i++) {
    if (buffer[i] == '\n') {
      TextLine *tmp = txt_line_malloc();
      tmp->line = MEM_malloc_arrayN<char>(size_t(llen) + 1, "textline_string");
      tmp->format = nullptr;

      if (llen) {
        memcpy(tmp->line, &buffer[i - llen], llen);
      }
      tmp->line[llen] = 0;
      tmp->len = llen;

      cleanup_textline(tmp);

      BLI_addtail(&text->lines, tmp);
      lines_count += 1;

      llen = 0;
      continue;
    }
    llen++;
  }

  /* create new line in cases:
   * - rest of line (if last line in file hasn't got \n terminator).
   *   in this case content of such line would be used to fill text line buffer
   * - file is empty. in this case new line is needed to start editing from.
   * - last character in buffer is \n. in this case new line is needed to
   *   deal with newline at end of file. (see #28087) (sergey) */
  if (llen != 0 || lines_count == 0 || buffer[len - 1] == '\n') {
    TextLine *tmp = txt_line_malloc();
    tmp->line = MEM_malloc_arrayN<char>(size_t(llen) + 1, "textline_string");
    tmp->format = nullptr;

    if (llen) {
      memcpy(tmp->line, &buffer[i - llen], llen);
    }

    tmp->line[llen] = 0;
    tmp->len = llen;

    cleanup_textline(tmp);

    BLI_addtail(&text->lines, tmp);
    // lines_count += 1; /* UNUSED. */
  }

  text->curl = text->sell = static_cast<TextLine *>(text->lines.first);
  text->curc = text->selc = 0;
}

bool BKE_text_reload(Text *text)
{
  uchar *buffer;
  size_t buffer_len;
  char filepath_abs[FILE_MAX];
  BLI_stat_t st;

  if (!text->filepath) {
    return false;
  }

  STRNCPY(filepath_abs, text->filepath);
  BLI_path_abs(filepath_abs, ID_BLEND_PATH_FROM_GLOBAL(&text->id));

  buffer = static_cast<uchar *>(BLI_file_read_text_as_mem(filepath_abs, 0, &buffer_len));
  if (buffer == nullptr) {
    return false;
  }

  /* free memory: */
  BKE_text_free_lines(text);
  txt_make_dirty(text);

  /* clear undo buffer */
  if (BLI_stat(filepath_abs, &st) != -1) {
    text->mtime = st.st_mtime;
  }
  else {
    text->mtime = 0;
  }

  text_from_buf(text, buffer, buffer_len);

  MEM_freeN(buffer);
  return true;
}

Text *BKE_text_load_ex(Main *bmain,
                       const char *filepath,
                       const char *relbase,
                       const bool is_internal)
{
  uchar *buffer;
  size_t buffer_len;
  Text *ta;
  char filepath_abs[FILE_MAX];
  BLI_stat_t st;

  STRNCPY(filepath_abs, filepath);
  BLI_path_abs(filepath_abs, relbase);

  buffer = static_cast<uchar *>(BLI_file_read_text_as_mem(filepath_abs, 0, &buffer_len));
  if (buffer == nullptr) {
    return nullptr;
  }

  ta = static_cast<Text *>(BKE_libblock_alloc(bmain, ID_TXT, BLI_path_basename(filepath_abs), 0));
  id_us_min(&ta->id);
  id_fake_user_set(&ta->id);

  BLI_listbase_clear(&ta->lines);
  ta->curl = ta->sell = nullptr;

  if ((U.flag & USER_TXT_TABSTOSPACES_DISABLE) == 0) {
    ta->flags = TXT_TABSTOSPACES;
  }

  if (is_internal == false) {
    const size_t filepath_len = strlen(filepath);
    ta->filepath = MEM_malloc_arrayN<char>(filepath_len + 1, "text_name");
    memcpy(ta->filepath, filepath, filepath_len + 1);
  }
  else {
    ta->flags |= TXT_ISMEM | TXT_ISDIRTY;
  }

  /* clear undo buffer */
  if (BLI_stat(filepath_abs, &st) != -1) {
    ta->mtime = st.st_mtime;
  }
  else {
    ta->mtime = 0;
  }

  text_from_buf(ta, buffer, buffer_len);

  MEM_freeN(buffer);

  return ta;
}

Text *BKE_text_load(Main *bmain, const char *filepath, const char *relbase)
{
  return BKE_text_load_ex(bmain, filepath, relbase, false);
}

void BKE_text_clear(Text *text) /* called directly from rna */
{
  txt_sel_all(text);
  txt_delete_sel(text);
  txt_make_dirty(text);
}

void BKE_text_write(Text *text, const char *str, int str_len) /* called directly from rna */
{
  txt_insert_buf(text, str, str_len);
  txt_move_eof(text, false);
  txt_make_dirty(text);
}

int BKE_text_file_modified_check(const Text *text)
{
  BLI_stat_t st;
  int result;
  char filepath[FILE_MAX];

  if (!text->filepath) {
    return 0;
  }

  STRNCPY(filepath, text->filepath);
  BLI_path_abs(filepath, ID_BLEND_PATH_FROM_GLOBAL(&text->id));

  if (!BLI_exists(filepath)) {
    return 2;
  }

  result = BLI_stat(filepath, &st);

  if (result == -1) {
    return -1;
  }

  if ((st.st_mode & S_IFMT) != S_IFREG) {
    return -1;
  }

  if (st.st_mtime > text->mtime) {
    return 1;
  }

  return 0;
}

void BKE_text_file_modified_ignore(Text *text)
{
  BLI_stat_t st;
  int result;
  char filepath[FILE_MAX];

  if (!text->filepath) {
    return;
  }

  STRNCPY(filepath, text->filepath);
  BLI_path_abs(filepath, ID_BLEND_PATH_FROM_GLOBAL(&text->id));

  if (!BLI_exists(filepath)) {
    return;
  }

  result = BLI_stat(filepath, &st);

  if (result == -1 || (st.st_mode & S_IFMT) != S_IFREG) {
    return;
  }

  text->mtime = st.st_mtime;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Editing Utility Functions
 * \{ */

static TextLine *txt_line_malloc()
{
  TextLine *l = MEM_mallocN<TextLine>("textline");
  /* Quiet VALGRIND warning, may avoid unintended differences with MEMFILE undo as well. */
  memset(l->_pad0, 0, sizeof(l->_pad0));
  return l;
}

static void make_new_line(TextLine *line, char *newline)
{
  if (line->line) {
    MEM_freeN(line->line);
  }
  if (line->format) {
    MEM_freeN(line->format);
  }

  line->line = newline;
  line->len = strlen(newline);
  line->format = nullptr;
}

static TextLine *txt_new_linen(const char *str, int str_len)
{
  TextLine *tmp = txt_line_malloc();
  tmp->line = MEM_malloc_arrayN<char>(size_t(str_len) + 1, "textline_string");
  tmp->format = nullptr;

  memcpy(tmp->line, str, str_len);
  tmp->line[str_len] = '\0';
  tmp->len = str_len;
  tmp->next = tmp->prev = nullptr;

  BLI_assert(strlen(tmp->line) == str_len);

  return tmp;
}

static TextLine *txt_new_line(const char *str)
{
  return txt_new_linen(str, strlen(str));
}

void txt_clean_text(Text *text)
{
  TextLine **top, **bot;

  if (!text->lines.first) {
    if (text->lines.last) {
      text->lines.first = text->lines.last;
    }
    else {
      text->lines.first = text->lines.last = txt_new_line("");
    }
  }

  if (!text->lines.last) {
    text->lines.last = text->lines.first;
  }

  top = (TextLine **)&text->lines.first;
  bot = (TextLine **)&text->lines.last;

  while ((*top)->prev) {
    *top = (*top)->prev;
  }
  while ((*bot)->next) {
    *bot = (*bot)->next;
  }

  if (!text->curl) {
    if (text->sell) {
      text->curl = text->sell;
    }
    else {
      text->curl = static_cast<TextLine *>(text->lines.first);
    }
    text->curc = 0;
  }

  if (!text->sell) {
    text->sell = text->curl;
    text->selc = 0;
  }
}

int txt_get_span(const TextLine *from, const TextLine *to)
{
  int ret = 0;
  const TextLine *tmp = from;

  if (!to || !from) {
    return 0;
  }
  if (from == to) {
    return 0;
  }

  /* Look forwards */
  while (tmp) {
    if (tmp == to) {
      return ret;
    }
    ret++;
    tmp = tmp->next;
  }

  /* Look backwards */
  if (!tmp) {
    tmp = from;
    ret = 0;
    while (tmp) {
      if (tmp == to) {
        break;
      }
      ret--;
      tmp = tmp->prev;
    }
    if (!tmp) {
      ret = 0;
    }
  }

  return ret;
}

static void txt_make_dirty(Text *text)
{
  text->flags |= TXT_ISDIRTY;
#ifdef WITH_PYTHON
  if (text->compiled) {
    BPY_text_free_code(text);
  }
#endif
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Cursor Utility Functions
 * \{ */

static void txt_curs_cur(Text *text, TextLine ***linep, int **charp)
{
  *linep = &text->curl;
  *charp = &text->curc;
}

static void txt_curs_sel(Text *text, TextLine ***linep, int **charp)
{
  *linep = &text->sell;
  *charp = &text->selc;
}

bool txt_cursor_is_line_start(const Text *text)
{
  return (text->selc == 0);
}

bool txt_cursor_is_line_end(const Text *text)
{
  return (text->selc == text->sell->len);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Cursor Movement Functions
 *
 * \note If the user moves the cursor the space containing that cursor should be popped
 * See #txt_pop_first, #txt_pop_last
 * Other space-types retain their own top location.
 * \{ */

void txt_move_up(Text *text, const bool sel)
{
  TextLine **linep;
  int *charp;

  if (sel) {
    txt_curs_sel(text, &linep, &charp);
  }
  else {
    txt_pop_first(text);
    txt_curs_cur(text, &linep, &charp);
  }
  if (!*linep) {
    return;
  }

  if ((*linep)->prev) {
    int column = BLI_str_utf8_offset_to_column_with_tabs(
        (*linep)->line, (*linep)->len, *charp, TXT_TABSIZE);
    *linep = (*linep)->prev;
    *charp = BLI_str_utf8_offset_from_column_with_tabs(
        (*linep)->line, (*linep)->len, column, TXT_TABSIZE);
  }
  else {
    txt_move_bol(text, sel);
  }

  if (!sel) {
    txt_pop_sel(text);
  }
}

void txt_move_down(Text *text, const bool sel)
{
  TextLine **linep;
  int *charp;

  if (sel) {
    txt_curs_sel(text, &linep, &charp);
  }
  else {
    txt_pop_last(text);
    txt_curs_cur(text, &linep, &charp);
  }
  if (!*linep) {
    return;
  }

  if ((*linep)->next) {
    int column = BLI_str_utf8_offset_to_column_with_tabs(
        (*linep)->line, (*linep)->len, *charp, TXT_TABSIZE);
    *linep = (*linep)->next;
    *charp = BLI_str_utf8_offset_from_column_with_tabs(
        (*linep)->line, (*linep)->len, column, TXT_TABSIZE);
  }
  else {
    txt_move_eol(text, sel);
  }

  if (!sel) {
    txt_pop_sel(text);
  }
}

int txt_calc_tab_left(const TextLine *tl, int ch)
{
  /* do nice left only if there are only spaces */

  int tabsize = (ch < TXT_TABSIZE) ? ch : TXT_TABSIZE;

  for (int i = 0; i < ch; i++) {
    if (tl->line[i] != ' ') {
      tabsize = 0;
      break;
    }
  }

  /* if in the middle of the space-tab */
  if (tabsize && ch % TXT_TABSIZE != 0) {
    tabsize = (ch % TXT_TABSIZE);
  }
  return tabsize;
}

int txt_calc_tab_right(const TextLine *tl, int ch)
{
  if (tl->line[ch] == ' ') {
    int i;
    for (i = 0; i < ch; i++) {
      if (tl->line[i] != ' ') {
        return 0;
      }
    }

    int tabsize = (ch) % TXT_TABSIZE + 1;
    for (i = ch + 1; tl->line[i] == ' ' && tabsize < TXT_TABSIZE; i++) {
      tabsize++;
    }

    return i - ch;
  }

  return 0;
}

void txt_move_left(Text *text, const bool sel)
{
  TextLine **linep;
  int *charp;
  int tabsize = 0;

  if (sel) {
    txt_curs_sel(text, &linep, &charp);
  }
  else {
    txt_pop_first(text);
    txt_curs_cur(text, &linep, &charp);
  }
  if (!*linep) {
    return;
  }

  if (*charp == 0) {
    if ((*linep)->prev) {
      txt_move_up(text, sel);
      *charp = (*linep)->len;
    }
  }
  else {
    /* do nice left only if there are only spaces */
    /* #TXT_TABSIZE hard-coded in DNA_text_types.h */
    if (text->flags & TXT_TABSTOSPACES) {
      tabsize = txt_calc_tab_left(*linep, *charp);
    }

    if (tabsize) {
      (*charp) -= tabsize;
    }
    else {
      BLI_str_cursor_step_prev_utf8((*linep)->line, (*linep)->len, charp);
    }
  }

  if (!sel) {
    txt_pop_sel(text);
  }
}

void txt_move_right(Text *text, const bool sel)
{
  TextLine **linep;
  int *charp;

  if (sel) {
    txt_curs_sel(text, &linep, &charp);
  }
  else {
    txt_pop_last(text);
    txt_curs_cur(text, &linep, &charp);
  }
  if (!*linep) {
    return;
  }

  if (*charp == (*linep)->len) {
    if ((*linep)->next) {
      txt_move_down(text, sel);
      *charp = 0;
    }
  }
  else {
    /* do nice right only if there are only spaces */
    /* spaces hardcoded in DNA_text_types.h */
    int tabsize = 0;

    if (text->flags & TXT_TABSTOSPACES) {
      tabsize = txt_calc_tab_right(*linep, *charp);
    }

    if (tabsize) {
      (*charp) += tabsize;
    }
    else {
      BLI_str_cursor_step_next_utf8((*linep)->line, (*linep)->len, charp);
    }
  }

  if (!sel) {
    txt_pop_sel(text);
  }
}

void txt_jump_left(Text *text, const bool sel, const bool use_init_step)
{
  TextLine **linep;
  int *charp;

  if (sel) {
    txt_curs_sel(text, &linep, &charp);
  }
  else {
    txt_pop_first(text);
    txt_curs_cur(text, &linep, &charp);
  }
  if (!*linep) {
    return;
  }

  BLI_str_cursor_step_utf8(
      (*linep)->line, (*linep)->len, charp, STRCUR_DIR_PREV, STRCUR_JUMP_DELIM, use_init_step);

  if (!sel) {
    txt_pop_sel(text);
  }
}

void txt_jump_right(Text *text, const bool sel, const bool use_init_step)
{
  TextLine **linep;
  int *charp;

  if (sel) {
    txt_curs_sel(text, &linep, &charp);
  }
  else {
    txt_pop_last(text);
    txt_curs_cur(text, &linep, &charp);
  }
  if (!*linep) {
    return;
  }

  BLI_str_cursor_step_utf8(
      (*linep)->line, (*linep)->len, charp, STRCUR_DIR_NEXT, STRCUR_JUMP_DELIM, use_init_step);

  if (!sel) {
    txt_pop_sel(text);
  }
}

void txt_move_bol(Text *text, const bool sel)
{
  TextLine **linep;
  int *charp;

  if (sel) {
    txt_curs_sel(text, &linep, &charp);
  }
  else {
    txt_curs_cur(text, &linep, &charp);
  }
  if (!*linep) {
    return;
  }

  *charp = 0;

  if (!sel) {
    txt_pop_sel(text);
  }
}

void txt_move_eol(Text *text, const bool sel)
{
  TextLine **linep;
  int *charp;

  if (sel) {
    txt_curs_sel(text, &linep, &charp);
  }
  else {
    txt_curs_cur(text, &linep, &charp);
  }
  if (!*linep) {
    return;
  }

  *charp = (*linep)->len;

  if (!sel) {
    txt_pop_sel(text);
  }
}

void txt_move_bof(Text *text, const bool sel)
{
  TextLine **linep;
  int *charp;

  if (sel) {
    txt_curs_sel(text, &linep, &charp);
  }
  else {
    txt_curs_cur(text, &linep, &charp);
  }
  if (!*linep) {
    return;
  }

  *linep = static_cast<TextLine *>(text->lines.first);
  *charp = 0;

  if (!sel) {
    txt_pop_sel(text);
  }
}

void txt_move_eof(Text *text, const bool sel)
{
  TextLine **linep;
  int *charp;

  if (sel) {
    txt_curs_sel(text, &linep, &charp);
  }
  else {
    txt_curs_cur(text, &linep, &charp);
  }
  if (!*linep) {
    return;
  }

  *linep = static_cast<TextLine *>(text->lines.last);
  *charp = (*linep)->len;

  if (!sel) {
    txt_pop_sel(text);
  }
}

void txt_move_toline(Text *text, uint line, const bool sel)
{
  txt_move_to(text, line, 0, sel);
}

void txt_move_to(Text *text, uint line, uint ch, const bool sel)
{
  TextLine **linep;
  int *charp;
  uint i;

  if (sel) {
    txt_curs_sel(text, &linep, &charp);
  }
  else {
    txt_curs_cur(text, &linep, &charp);
  }
  if (!*linep) {
    return;
  }

  *linep = static_cast<TextLine *>(text->lines.first);
  for (i = 0; i < line; i++) {
    if ((*linep)->next) {
      *linep = (*linep)->next;
    }
    else {
      break;
    }
  }
  if (ch > uint((*linep)->len)) {
    ch = uint((*linep)->len);
  }
  *charp = ch;

  if (!sel) {
    txt_pop_sel(text);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Text Selection Functions
 * \{ */

static void txt_curs_swap(Text *text)
{
  TextLine *tmpl;
  int tmpc;

  tmpl = text->curl;
  text->curl = text->sell;
  text->sell = tmpl;

  tmpc = text->curc;
  text->curc = text->selc;
  text->selc = tmpc;
}

static void txt_pop_first(Text *text)
{
  if (txt_get_span(text->curl, text->sell) < 0 ||
      (text->curl == text->sell && text->curc > text->selc))
  {
    txt_curs_swap(text);
  }

  txt_pop_sel(text);
}

static void txt_pop_last(Text *text)
{
  if (txt_get_span(text->curl, text->sell) > 0 ||
      (text->curl == text->sell && text->curc < text->selc))
  {
    txt_curs_swap(text);
  }

  txt_pop_sel(text);
}

void txt_pop_sel(Text *text)
{
  text->sell = text->curl;
  text->selc = text->curc;
}

void txt_order_cursors(Text *text, const bool reverse)
{
  if (!text->curl) {
    return;
  }
  if (!text->sell) {
    return;
  }

  /* Flip so text->curl is before/after text->sell */
  if (reverse == false) {
    if ((txt_get_span(text->curl, text->sell) < 0) ||
        (text->curl == text->sell && text->curc > text->selc))
    {
      txt_curs_swap(text);
    }
  }
  else {
    if ((txt_get_span(text->curl, text->sell) > 0) ||
        (text->curl == text->sell && text->curc < text->selc))
    {
      txt_curs_swap(text);
    }
  }
}

bool txt_has_sel(const Text *text)
{
  return ((text->curl != text->sell) || (text->curc != text->selc));
}

static void txt_delete_sel(Text *text)
{
  TextLine *tmpl;
  char *buf;

  if (!text->curl) {
    return;
  }
  if (!text->sell) {
    return;
  }

  if (!txt_has_sel(text)) {
    return;
  }

  txt_order_cursors(text, false);

  buf = MEM_malloc_arrayN<char>(
      size_t(text->curc) + (size_t(text->sell->len) - size_t(text->selc)) + 1, "textline_string");

  memcpy(buf, text->curl->line, text->curc);
  memcpy(buf + text->curc, text->sell->line + text->selc, text->sell->len - text->selc);
  buf[text->curc + (text->sell->len - text->selc)] = 0;

  make_new_line(text->curl, buf);

  tmpl = text->sell;
  while (tmpl != text->curl) {
    tmpl = tmpl->prev;
    if (!tmpl) {
      break;
    }

    txt_delete_line(text, tmpl->next);
  }

  text->sell = text->curl;
  text->selc = text->curc;
}

void txt_sel_all(Text *text)
{
  text->curl = static_cast<TextLine *>(text->lines.first);
  text->curc = 0;

  text->sell = static_cast<TextLine *>(text->lines.last);
  text->selc = text->sell->len;
}

void txt_sel_clear(Text *text)
{
  if (text->sell) {
    text->curl = text->sell;
    text->curc = text->selc;
  }
}

void txt_sel_line(Text *text)
{
  if (!text->curl) {
    return;
  }

  text->curc = 0;
  text->sell = text->curl;
  text->selc = text->sell->len;
}

void txt_sel_set(Text *text, int startl, int startc, int endl, int endc)
{
  TextLine *froml, *tol;

  /* Support negative indices. */
  if (startl < 0 || endl < 0) {
    int end = BLI_listbase_count(&text->lines) - 1;
    if (startl < 0) {
      startl = end + startl + 1;
    }
    if (endl < 0) {
      endl = end + endl + 1;
    }
  }
  CLAMP_MIN(startl, 0);
  CLAMP_MIN(endl, 0);

  froml = static_cast<TextLine *>(BLI_findlink(&text->lines, startl));
  if (froml == nullptr) {
    froml = static_cast<TextLine *>(text->lines.last);
  }
  if (startl == endl) {
    tol = froml;
  }
  else {
    tol = static_cast<TextLine *>(BLI_findlink(&text->lines, endl));
    if (tol == nullptr) {
      tol = static_cast<TextLine *>(text->lines.last);
    }
  }

  /* Support negative indices. */
  if (startc < 0) {
    const int fromllen = BLI_strlen_utf8(froml->line);
    startc = std::max(0, fromllen + startc + 1);
  }
  if (endc < 0) {
    const int tollen = BLI_strlen_utf8(tol->line);
    endc = std::max(0, tollen + endc + 1);
  }

  text->curl = froml;
  text->curc = BLI_str_utf8_offset_from_index(froml->line, froml->len, startc);
  text->sell = tol;
  text->selc = BLI_str_utf8_offset_from_index(tol->line, tol->len, endc);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Buffer Conversion for Undo/Redo
 *
 * Buffer conversion functions that rely on the buffer already being validated.
 *
 * The only requirement for these functions is that they're reverse-able,
 * the undo logic doesn't inspect their content.
 *
 * Currently buffers:
 * - Always ends with a new-line.
 * - Are not null terminated.
 * \{ */

char *txt_to_buf_for_undo(Text *text, size_t *r_buf_len)
{
  int buf_len = 0;
  LISTBASE_FOREACH (const TextLine *, l, &text->lines) {
    buf_len += l->len + 1;
  }
  char *buf = MEM_malloc_arrayN<char>(size_t(buf_len), __func__);
  char *buf_step = buf;
  LISTBASE_FOREACH (const TextLine *, l, &text->lines) {
    memcpy(buf_step, l->line, l->len);
    buf_step += l->len;
    *buf_step++ = '\n';
  }
  *r_buf_len = buf_len;
  return buf;
}

void txt_from_buf_for_undo(Text *text, const char *buf, size_t buf_len)
{
  const char *buf_end = buf + buf_len;
  const char *buf_step = buf;

  /* First re-use existing lines.
   * Good for undo since it means in practice many operations re-use all
   * except for the modified line. */
  TextLine *l_src = static_cast<TextLine *>(text->lines.first);
  BLI_listbase_clear(&text->lines);
  while (buf_step != buf_end && l_src) {
    /* New lines are ensured by #txt_to_buf_for_undo. */
    const char *buf_step_next = strchr(buf_step, '\n');
    const int len = buf_step_next - buf_step;

    TextLine *l = l_src;
    l_src = l_src->next;
    if (l->len != len) {
      l->line = static_cast<char *>(MEM_reallocN(l->line, len + 1));
      l->len = len;
    }
    MEM_SAFE_FREE(l->format);

    memcpy(l->line, buf_step, len);
    l->line[len] = '\0';
    BLI_addtail(&text->lines, l);
    buf_step = buf_step_next + 1;
  }

  /* If we have extra lines. */
  while (l_src != nullptr) {
    TextLine *l_src_next = l_src->next;
    MEM_freeN(l_src->line);
    if (l_src->format) {
      MEM_freeN(l_src->format);
    }
    MEM_freeN(l_src);
    l_src = l_src_next;
  }

  while (buf_step != buf_end) {
    /* New lines are ensured by #txt_to_buf_for_undo. */
    const char *buf_step_next = strchr(buf_step, '\n');
    const int len = buf_step_next - buf_step;

    TextLine *l = txt_line_malloc();
    l->line = MEM_malloc_arrayN<char>(size_t(len) + 1, "textline_string");
    l->len = len;
    l->format = nullptr;

    memcpy(l->line, buf_step, len);
    l->line[len] = '\0';
    BLI_addtail(&text->lines, l);
    buf_step = buf_step_next + 1;
  }

  text->curl = text->sell = static_cast<TextLine *>(text->lines.first);
  text->curc = text->selc = 0;

  txt_make_dirty(text);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Cut and Paste Functions
 * \{ */

char *txt_to_buf(Text *text, size_t *r_buf_strlen)
{
  const bool has_data = !BLI_listbase_is_empty(&text->lines);
  /* Identical to #txt_to_buf_for_undo except that the string is nil terminated. */
  size_t buf_len = 0;
  LISTBASE_FOREACH (const TextLine *, l, &text->lines) {
    buf_len += l->len + 1;
  }
  if (has_data) {
    buf_len -= 1;
  }
  char *buf = MEM_malloc_arrayN<char>(buf_len + 1, __func__);
  char *buf_step = buf;
  LISTBASE_FOREACH (const TextLine *, l, &text->lines) {
    memcpy(buf_step, l->line, l->len);
    buf_step += l->len;
    *buf_step++ = '\n';
  }
  /* Remove the trailing new-line so a round-trip doesn't add a newline:
   * Python for example `text.from_string(text.as_string())`. */
  if (has_data) {
    buf_step--;
  }
  *buf_step = '\0';
  *r_buf_strlen = buf_len;
  return buf;
}

char *txt_sel_to_buf(const Text *text, size_t *r_buf_strlen)
{
  char *buf;
  size_t length = 0;
  TextLine *tmp, *linef, *linel;
  int charf, charl;

  if (r_buf_strlen) {
    *r_buf_strlen = 0;
  }

  if (!text->curl) {
    return nullptr;
  }
  if (!text->sell) {
    return nullptr;
  }

  if (text->curl == text->sell) {
    linef = linel = text->curl;

    if (text->curc < text->selc) {
      charf = text->curc;
      charl = text->selc;
    }
    else {
      charf = text->selc;
      charl = text->curc;
    }
  }
  else if (txt_get_span(text->curl, text->sell) < 0) {
    linef = text->sell;
    linel = text->curl;

    charf = text->selc;
    charl = text->curc;
  }
  else {
    linef = text->curl;
    linel = text->sell;

    charf = text->curc;
    charl = text->selc;
  }

  if (linef == linel) {
    length = charl - charf;
    buf = MEM_malloc_arrayN<char>(length + 1, "sel buffer");
    memcpy(buf, linef->line + charf, length);
    buf[length] = '\0';
  }
  else {
    /* Add 1 for the '\n' */
    length = (linef->len - charf) + charl + 1;

    for (tmp = linef->next; tmp && tmp != linel; tmp = tmp->next) {
      length += tmp->len + 1;
    }

    buf = MEM_malloc_arrayN<char>(length + 1, "sel buffer");

    memcpy(buf, linef->line + charf, linef->len - charf);
    length = linef->len - charf;
    buf[length++] = '\n';

    for (tmp = linef->next; tmp && tmp != linel; tmp = tmp->next) {
      memcpy(buf + length, tmp->line, tmp->len);
      length += tmp->len;
      buf[length++] = '\n';
    }

    memcpy(buf + length, linel->line, charl);
    length += charl;
    buf[length] = '\0';
  }

  if (r_buf_strlen) {
    *r_buf_strlen = length;
  }

  return buf;
}

void txt_insert_buf(Text *text, const char *in_buffer, int in_buffer_len)
{
  BLI_assert(in_buffer_len == strlen(in_buffer));

  int l = 0;
  size_t i = 0, j;
  TextLine *add;
  char *buffer;

  txt_delete_sel(text);

  buffer = BLI_strdupn(in_buffer, in_buffer_len);
  in_buffer_len += txt_extended_ascii_as_utf8(&buffer);

  /* Read the first line (or as close as possible */
  while (buffer[i] && buffer[i] != '\n') {
    txt_add_raw_char(text, BLI_str_utf8_as_unicode_step_safe(buffer, in_buffer_len, &i));
  }

  if (buffer[i] == '\n') {
    txt_split_curline(text);
    i++;

    while (i < in_buffer_len) {
      l = 0;

      while (buffer[i] && buffer[i] != '\n') {
        i++;
        l++;
      }

      if (buffer[i] == '\n') {
        add = txt_new_linen(buffer + (i - l), l);
        BLI_insertlinkbefore(&text->lines, text->curl, add);
        i++;
      }
      else {
        for (j = i - l; j < i && j < in_buffer_len;) {
          txt_add_raw_char(text, BLI_str_utf8_as_unicode_step_safe(buffer, in_buffer_len, &j));
        }
        break;
      }
    }
  }

  MEM_freeN(buffer);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Find String in Text
 * \{ */

bool txt_find_string(Text *text, const char *findstr, int wrap, int match_case)
{
  TextLine *tl, *startl;
  const char *s = nullptr;

  if (!text->curl || !text->sell) {
    return false;
  }

  txt_order_cursors(text, false);

  tl = startl = text->sell;

  if (match_case) {
    s = strstr(&tl->line[text->selc], findstr);
  }
  else {
    s = BLI_strcasestr(&tl->line[text->selc], findstr);
  }
  while (!s) {
    tl = tl->next;
    if (!tl) {
      if (wrap) {
        tl = static_cast<TextLine *>(text->lines.first);
      }
      else {
        break;
      }
    }

    if (match_case) {
      s = strstr(tl->line, findstr);
    }
    else {
      s = BLI_strcasestr(tl->line, findstr);
    }
    if (tl == startl) {
      break;
    }
  }

  if (s) {
    int newl = txt_get_span(static_cast<TextLine *>(text->lines.first), tl);
    int newc = int(s - tl->line);
    txt_move_to(text, newl, newc, false);
    txt_move_to(text, newl, newc + strlen(findstr), true);
    return true;
  }

  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Line Editing Functions
 * \{ */

void txt_split_curline(Text *text)
{
  TextLine *ins;
  char *left, *right;

  if (!text->curl) {
    return;
  }

  txt_delete_sel(text);

  /* Make the two half strings */

  left = MEM_malloc_arrayN<char>(size_t(text->curc) + 1, "textline_string");
  if (text->curc) {
    memcpy(left, text->curl->line, text->curc);
  }
  left[text->curc] = 0;

  right = MEM_malloc_arrayN<char>(size_t(text->curl->len) - size_t(text->curc) + 1,
                                  "textline_string");
  memcpy(right, text->curl->line + text->curc, text->curl->len - text->curc + 1);

  MEM_freeN(text->curl->line);
  if (text->curl->format) {
    MEM_freeN(text->curl->format);
  }

  /* Make the new TextLine */

  ins = txt_line_malloc();
  ins->line = left;
  ins->format = nullptr;
  ins->len = text->curc;

  text->curl->line = right;
  text->curl->format = nullptr;
  text->curl->len = text->curl->len - text->curc;

  BLI_insertlinkbefore(&text->lines, text->curl, ins);

  text->curc = 0;

  txt_make_dirty(text);
  txt_clean_text(text);

  txt_pop_sel(text);
}

static void txt_delete_line(Text *text, TextLine *line)
{
  if (!text->curl) {
    return;
  }

  BLI_remlink(&text->lines, line);

  if (line->line) {
    MEM_freeN(line->line);
  }
  if (line->format) {
    MEM_freeN(line->format);
  }

  MEM_freeN(line);

  txt_make_dirty(text);
  txt_clean_text(text);
}

static void txt_combine_lines(Text *text, TextLine *linea, TextLine *lineb)
{
  char *tmp, *s;

  if (!linea || !lineb) {
    return;
  }

  tmp = MEM_malloc_arrayN<char>(size_t(linea->len) + size_t(lineb->len) + 1, "textline_string");

  s = tmp;
  memcpy(s, linea->line, linea->len);
  s += linea->len;
  memcpy(s, lineb->line, lineb->len);
  s += lineb->len;
  *s = '\0';
  (void)s;

  make_new_line(linea, tmp);

  txt_delete_line(text, lineb);

  txt_make_dirty(text);
  txt_clean_text(text);
}

void txt_duplicate_line(Text *text)
{
  TextLine *textline;

  if (!text->curl) {
    return;
  }

  if (text->curl == text->sell) {
    textline = txt_new_line(text->curl->line);
    BLI_insertlinkafter(&text->lines, text->curl, textline);

    txt_make_dirty(text);
    txt_clean_text(text);
  }
}

void txt_delete_char(Text *text)
{
  if (!text->curl) {
    return;
  }

  if (txt_has_sel(text)) { /* deleting a selection */
    txt_delete_sel(text);
    txt_make_dirty(text);
    return;
  }
  if (text->curc == text->curl->len) { /* Appending two lines */
    if (text->curl->next) {
      txt_combine_lines(text, text->curl, text->curl->next);
      txt_pop_sel(text);
    }
    else {
      return;
    }
  }
  else { /* Just deleting a char */
    int pos = text->curc;
    BLI_str_cursor_step_next_utf8(text->curl->line, text->curl->len, &pos);
    size_t c_len = pos - text->curc;

    memmove(text->curl->line + text->curc,
            text->curl->line + text->curc + c_len,
            text->curl->len - text->curc - c_len + 1);

    text->curl->len -= c_len;

    txt_pop_sel(text);
  }

  txt_make_dirty(text);
  txt_clean_text(text);
}

void txt_delete_word(Text *text)
{
  txt_jump_right(text, true, true);
  txt_delete_sel(text);
  txt_make_dirty(text);
}

void txt_backspace_char(Text *text)
{
  if (!text->curl) {
    return;
  }

  if (txt_has_sel(text)) { /* deleting a selection */
    txt_delete_sel(text);
    txt_make_dirty(text);
    return;
  }
  if (text->curc == 0) { /* Appending two lines */
    if (!text->curl->prev) {
      return;
    }

    text->curl = text->curl->prev;
    text->curc = text->curl->len;

    txt_combine_lines(text, text->curl, text->curl->next);
    txt_pop_sel(text);
  }
  else { /* Just backspacing a char */
    int pos = text->curc;
    BLI_str_cursor_step_prev_utf8(text->curl->line, text->curl->len, &pos);
    size_t c_len = text->curc - pos;

    /* source and destination overlap, don't use memcpy() */
    memmove(text->curl->line + text->curc - c_len,
            text->curl->line + text->curc,
            text->curl->len - text->curc + 1);

    text->curl->len -= c_len;
    text->curc -= c_len;

    txt_pop_sel(text);
  }

  txt_make_dirty(text);
  txt_clean_text(text);
}

void txt_backspace_word(Text *text)
{
  txt_jump_left(text, true, true);
  txt_delete_sel(text);
  txt_make_dirty(text);
}

/* Max spaces to replace a tab with, currently hardcoded to TXT_TABSIZE = 4.
 * Used by txt_convert_tab_to_spaces, indent and unindent.
 * Remember to change this string according to max tab size */
static char tab_to_spaces[] = "    ";

static void txt_convert_tab_to_spaces(Text *text)
{
  /* sb aims to pad adjust the tab-width needed so that the right number of spaces
   * is added so that the indentation of the line is the right width (i.e. aligned
   * to multiples of TXT_TABSIZE)
   */
  const char *sb = &tab_to_spaces[text->curc % TXT_TABSIZE];
  txt_insert_buf(text, sb, strlen(sb));
}

static bool txt_add_char_intern(Text *text, uint add, bool replace_tabs)
{
  char *tmp, ch[BLI_UTF8_MAX];
  size_t add_len;

  if (!text->curl) {
    return false;
  }

  if (add == '\n') {
    txt_split_curline(text);
    return true;
  }

  /* insert spaces rather than tabs */
  if (add == '\t' && replace_tabs) {
    txt_convert_tab_to_spaces(text);
    return true;
  }

  txt_delete_sel(text);

  add_len = BLI_str_utf8_from_unicode(add, ch, sizeof(ch));

  tmp = MEM_malloc_arrayN<char>(size_t(text->curl->len) + add_len + 1, "textline_string");

  memcpy(tmp, text->curl->line, text->curc);
  memcpy(tmp + text->curc, ch, add_len);
  memcpy(
      tmp + text->curc + add_len, text->curl->line + text->curc, text->curl->len - text->curc + 1);

  make_new_line(text->curl, tmp);

  text->curc += add_len;

  txt_pop_sel(text);

  txt_make_dirty(text);
  txt_clean_text(text);

  return true;
}

bool txt_add_char(Text *text, uint add)
{
  return txt_add_char_intern(text, add, (text->flags & TXT_TABSTOSPACES) != 0);
}

bool txt_add_raw_char(Text *text, uint add)
{
  return txt_add_char_intern(text, add, false);
}

void txt_delete_selected(Text *text)
{
  txt_delete_sel(text);
  txt_make_dirty(text);
}

bool txt_replace_char(Text *text, uint add)
{
  uint del;
  size_t del_size = 0, add_size;
  char ch[BLI_UTF8_MAX];

  if (!text->curl) {
    return false;
  }

  /* If text is selected or we're at the end of the line just use txt_add_char */
  if (text->curc == text->curl->len || txt_has_sel(text) || add == '\n') {
    return txt_add_char(text, add);
  }

  del_size = text->curc;
  del = BLI_str_utf8_as_unicode_step_safe(text->curl->line, text->curl->len, &del_size);
  del_size -= text->curc;
  UNUSED_VARS(del);
  add_size = BLI_str_utf8_from_unicode(add, ch, sizeof(ch));

  if (add_size > del_size) {
    char *tmp = MEM_malloc_arrayN<char>(size_t(text->curl->len) + add_size - del_size + 1,
                                        "textline_string");
    memcpy(tmp, text->curl->line, text->curc);
    memcpy(tmp + text->curc + add_size,
           text->curl->line + text->curc + del_size,
           text->curl->len - text->curc - del_size + 1);
    MEM_freeN(text->curl->line);
    text->curl->line = tmp;
  }
  else if (add_size < del_size) {
    char *tmp = text->curl->line;
    memmove(tmp + text->curc + add_size,
            tmp + text->curc + del_size,
            text->curl->len - text->curc - del_size + 1);
  }

  memcpy(text->curl->line + text->curc, ch, add_size);
  text->curc += add_size;
  text->curl->len += add_size - del_size;

  txt_pop_sel(text);
  txt_make_dirty(text);
  txt_clean_text(text);
  return true;
}

/**
 * Generic prefix operation, use for comment & indent.
 *
 * \note caller must handle undo.
 */
static void txt_select_prefix(Text *text, const char *add, bool skip_blank_lines)
{
  int len, num, curc_old, selc_old;
  char *tmp;

  const int indentlen = strlen(add);

  BLI_assert(!ELEM(nullptr, text->curl, text->sell));

  curc_old = text->curc;
  selc_old = text->selc;

  num = 0;
  while (true) {

    /* don't indent blank lines */
    if ((text->curl->len != 0) || (skip_blank_lines == 0)) {
      tmp = MEM_malloc_arrayN<char>(size_t(text->curl->len) + size_t(indentlen) + 1,
                                    "textline_string");

      text->curc = 0;
      if (text->curc) {
        memcpy(tmp, text->curl->line, text->curc); /* XXX never true, check prev line */
      }
      memcpy(tmp + text->curc, add, indentlen);

      len = text->curl->len - text->curc;
      if (len > 0) {
        memcpy(tmp + text->curc + indentlen, text->curl->line + text->curc, len);
      }
      tmp[text->curl->len + indentlen] = 0;

      make_new_line(text->curl, tmp);

      text->curc += indentlen;

      txt_make_dirty(text);
      txt_clean_text(text);
    }

    if (text->curl == text->sell) {
      if (text->curl->len != 0) {
        text->selc += indentlen;
      }
      break;
    }

    text->curl = text->curl->next;
    num++;
  }

  while (num > 0) {
    text->curl = text->curl->prev;
    num--;
  }

  /* Keep the cursor left aligned if we don't have a selection. */
  if (curc_old == 0 && !(text->curl == text->sell && curc_old == selc_old)) {
    if (text->curl == text->sell) {
      if (text->curc == text->selc) {
        text->selc = 0;
      }
    }
    text->curc = 0;
  }
  else {
    if (text->curl->len != 0) {
      text->curc = curc_old + indentlen;
    }
  }
}

/**
 * Generic un-prefix operation, use for comment & indent.
 *
 * \param require_all: When true, all non-empty lines must have this prefix.
 * Needed for comments where we might want to un-comment a block which contains some comments.
 *
 * \note caller must handle undo.
 */
static bool txt_select_unprefix(Text *text, const char *remove, const bool require_all)
{
  int num = 0;
  const int indentlen = strlen(remove);
  bool unindented_first = false;
  bool changed_any = false;

  BLI_assert(!ELEM(nullptr, text->curl, text->sell));

  if (require_all) {
    /* Check all non-empty lines use this 'remove',
     * so the operation is applied equally or not at all. */
    TextLine *l = text->curl;
    while (true) {
      if (STREQLEN(l->line, remove, indentlen)) {
        /* pass */
      }
      else {
        /* Blank lines or whitespace can be skipped. */
        for (int i = 0; i < l->len; i++) {
          if (!ELEM(l->line[i], '\t', ' ')) {
            return false;
          }
        }
      }
      if (l == text->sell) {
        break;
      }
      l = l->next;
    }
  }

  while (true) {
    bool changed = false;
    if (STREQLEN(text->curl->line, remove, indentlen)) {
      if (num == 0) {
        unindented_first = true;
      }
      text->curl->len -= indentlen;
      memmove(text->curl->line, text->curl->line + indentlen, text->curl->len + 1);
      changed = true;
      changed_any = true;
    }

    txt_make_dirty(text);
    txt_clean_text(text);

    if (text->curl == text->sell) {
      if (changed) {
        text->selc = std::max(text->selc - indentlen, 0);
      }
      break;
    }

    text->curl = text->curl->next;
    num++;
  }

  if (unindented_first) {
    text->curc = std::max(text->curc - indentlen, 0);
  }

  while (num > 0) {
    text->curl = text->curl->prev;
    num--;
  }

  /* caller must handle undo */
  return changed_any;
}

void txt_comment(Text *text, const char *prefix)
{
  if (ELEM(nullptr, text->curl, text->sell)) {
    return;
  }

  const bool skip_blank_lines = txt_has_sel(text);
  txt_select_prefix(text, prefix, skip_blank_lines);
}

bool txt_uncomment(Text *text, const char *prefix)
{
  if (ELEM(nullptr, text->curl, text->sell)) {
    return false;
  }

  return txt_select_unprefix(text, prefix, true);
}

void txt_indent(Text *text)
{
  const char *prefix = (text->flags & TXT_TABSTOSPACES) ? tab_to_spaces : "\t";

  if (ELEM(nullptr, text->curl, text->sell)) {
    return;
  }

  txt_select_prefix(text, prefix, true);
}

bool txt_unindent(Text *text)
{
  const char *prefix = (text->flags & TXT_TABSTOSPACES) ? tab_to_spaces : "\t";

  if (ELEM(nullptr, text->curl, text->sell)) {
    return false;
  }

  return txt_select_unprefix(text, prefix, false);
}

void txt_move_lines(Text *text, const int direction)
{
  TextLine *line_other;

  BLI_assert(ELEM(direction, TXT_MOVE_LINE_UP, TXT_MOVE_LINE_DOWN));

  if (!text->curl || !text->sell) {
    return;
  }

  txt_order_cursors(text, false);

  line_other = (direction == TXT_MOVE_LINE_DOWN) ? text->sell->next : text->curl->prev;

  if (!line_other) {
    return;
  }

  BLI_remlink(&text->lines, line_other);

  if (direction == TXT_MOVE_LINE_DOWN) {
    BLI_insertlinkbefore(&text->lines, text->curl, line_other);
  }
  else {
    BLI_insertlinkafter(&text->lines, text->sell, line_other);
  }

  txt_make_dirty(text);
  txt_clean_text(text);
}

int txt_setcurr_tab_spaces(Text *text, int space)
{
  int i = 0;
  int test = 0;
  const char *word = ":";
  const char *comm = "#";
  const char indent = (text->flags & TXT_TABSTOSPACES) ? ' ' : '\t';
  static const char *back_words[] = {"return", "break", "continue", "pass", "yield", nullptr};

  if (!text->curl) {
    return 0;
  }

  while (text->curl->line[i] == indent) {
    /* We only count those tabs/spaces that are before any text or before the `curs`. */
    if (i == text->curc) {
      return i;
    }

    i++;
  }
  if (strstr(text->curl->line, word)) {
    /* if we find a ':' on this line, then add a tab but not if it is:
     * 1) in a comment
     * 2) within an identifier
     * 3) after the cursor (text->curc), i.e. when creating space before a function def #25414.
     */
    int a;
    bool is_indent = false;
    for (a = 0; (a < text->curc) && (text->curl->line[a] != '\0'); a++) {
      char ch = text->curl->line[a];
      if (ch == '#') {
        break;
      }
      if (ch == ':') {
        is_indent = true;
      }
      else if (!ELEM(ch, ' ', '\t')) {
        is_indent = false;
      }
    }
    if (is_indent) {
      i += space;
    }
  }

  for (test = 0; back_words[test]; test++) {
    /* if there are these key words then remove a tab because we are done with the block */
    if (strstr(text->curl->line, back_words[test]) && i > 0) {
      if (strcspn(text->curl->line, back_words[test]) < strcspn(text->curl->line, comm)) {
        i -= space;
      }
    }
  }
  return i;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Character Queries
 * \{ */

int text_check_bracket(const char ch)
{
  int a;
  const char opens[] = "([{";
  const char close[] = ")]}";

  for (a = 0; a < (sizeof(opens) - 1); a++) {
    if (ch == opens[a]) {
      return a + 1;
    }
    if (ch == close[a]) {
      return -(a + 1);
    }
  }
  return 0;
}

bool text_check_delim(const char ch)
{
  /* TODO: have a function for operators:
   * http://docs.python.org/py3k/reference/lexical_analysis.html#operators */

  int a;
  char delims[] = "():\"\' ~!%^&*-+=[]{};/<>|.#\t,@";

  for (a = 0; a < (sizeof(delims) - 1); a++) {
    if (ch == delims[a]) {
      return true;
    }
  }
  return false;
}

bool text_check_digit(const char ch)
{
  if (ch < '0') {
    return false;
  }
  if (ch <= '9') {
    return true;
  }
  return false;
}

bool text_check_identifier(const char ch)
{
  if (ch < '0') {
    return false;
  }
  if (ch <= '9') {
    return true;
  }
  if (ch < 'A') {
    return false;
  }
  if (ch <= 'Z' || ch == '_') {
    return true;
  }
  if (ch < 'a') {
    return false;
  }
  if (ch <= 'z') {
    return true;
  }
  return false;
}

bool text_check_identifier_nodigit(const char ch)
{
  if (ch <= '9') {
    return false;
  }
  if (ch < 'A') {
    return false;
  }
  if (ch <= 'Z' || ch == '_') {
    return true;
  }
  if (ch < 'a') {
    return false;
  }
  if (ch <= 'z') {
    return true;
  }
  return false;
}

#ifndef WITH_PYTHON
int text_check_identifier_unicode(const uint ch)
{
  return (ch < 255 && text_check_identifier(uint(ch)));
}

int text_check_identifier_nodigit_unicode(const uint ch)
{
  return (ch < 255 && text_check_identifier_nodigit(char(ch)));
}
#endif /* !WITH_PYTHON */

bool text_check_whitespace(const char ch)
{
  if (ELEM(ch, ' ', '\t', '\r', '\n')) {
    return true;
  }
  return false;
}

int text_find_identifier_start(const char *str, int i)
{
  if (UNLIKELY(i <= 0)) {
    return 0;
  }

  while (i--) {
    if (!text_check_identifier(str[i])) {
      break;
    }
  }
  i++;
  return i;
}

/** \} */
