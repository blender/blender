/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spfile
 */

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_fileops.h"
#include "BLI_listbase.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_string_utils.hh"
#include "BLI_utildefines.h"

#include "DNA_userdef_types.h"

#include "BLT_translation.hh"

#include "BKE_appdir.hh"

#include "ED_fileselect.hh"

#include "UI_resources.hh"

#include "fsmenu.hh" /* include ourselves */

/* FSMENU HANDLING */

struct FSMenu {
  FSMenuEntry *fsmenu_system;
  FSMenuEntry *fsmenu_system_bookmarks;
  FSMenuEntry *fsmenu_bookmarks;
  FSMenuEntry *fsmenu_recent;
  FSMenuEntry *fsmenu_other;
};

static FSMenu *g_fsmenu = nullptr;

FSMenu *ED_fsmenu_get()
{
  if (!g_fsmenu) {
    g_fsmenu = MEM_callocN<FSMenu>(__func__);
  }
  return g_fsmenu;
}

FSMenuEntry *ED_fsmenu_get_category(FSMenu *fsmenu, FSMenuCategory category)
{
  FSMenuEntry *fsm_head = nullptr;

  switch (category) {
    case FS_CATEGORY_SYSTEM:
      fsm_head = fsmenu->fsmenu_system;
      break;
    case FS_CATEGORY_SYSTEM_BOOKMARKS:
      fsm_head = fsmenu->fsmenu_system_bookmarks;
      break;
    case FS_CATEGORY_BOOKMARKS:
      fsm_head = fsmenu->fsmenu_bookmarks;
      break;
    case FS_CATEGORY_RECENT:
      fsm_head = fsmenu->fsmenu_recent;
      break;
    case FS_CATEGORY_OTHER:
      fsm_head = fsmenu->fsmenu_other;
      break;
  }
  return fsm_head;
}

void ED_fsmenu_set_category(FSMenu *fsmenu, FSMenuCategory category, FSMenuEntry *fsm_head)
{
  switch (category) {
    case FS_CATEGORY_SYSTEM:
      fsmenu->fsmenu_system = fsm_head;
      break;
    case FS_CATEGORY_SYSTEM_BOOKMARKS:
      fsmenu->fsmenu_system_bookmarks = fsm_head;
      break;
    case FS_CATEGORY_BOOKMARKS:
      fsmenu->fsmenu_bookmarks = fsm_head;
      break;
    case FS_CATEGORY_RECENT:
      fsmenu->fsmenu_recent = fsm_head;
      break;
    case FS_CATEGORY_OTHER:
      fsmenu->fsmenu_other = fsm_head;
      break;
  }
}

int ED_fsmenu_get_nentries(FSMenu *fsmenu, FSMenuCategory category)
{
  FSMenuEntry *fsm_iter;
  int count = 0;

  for (fsm_iter = ED_fsmenu_get_category(fsmenu, category); fsm_iter; fsm_iter = fsm_iter->next) {
    count++;
  }

  return count;
}

FSMenuEntry *ED_fsmenu_get_entry(FSMenu *fsmenu, FSMenuCategory category, int idx)
{
  FSMenuEntry *fsm_iter;

  for (fsm_iter = ED_fsmenu_get_category(fsmenu, category); fsm_iter && idx;
       fsm_iter = fsm_iter->next)
  {
    idx--;
  }

  return fsm_iter;
}

char *ED_fsmenu_entry_get_path(FSMenuEntry *fsentry)
{
  return fsentry->path;
}

void ED_fsmenu_entry_set_path(FSMenuEntry *fsentry, const char *path)
{
  if ((!fsentry->path || !path || !STREQ(path, fsentry->path)) && (fsentry->path != path)) {
    char tmp_name[FILE_MAXFILE];

    MEM_SAFE_FREE(fsentry->path);

    fsentry->path = (path && path[0]) ? BLI_strdup(path) : nullptr;

    const std::optional<std::string> user_config_dir = BKE_appdir_folder_id_create(
        BLENDER_USER_CONFIG, nullptr);

    if (user_config_dir.has_value()) {
      BLI_path_join(tmp_name, sizeof(tmp_name), user_config_dir->c_str(), BLENDER_BOOKMARK_FILE);
      fsmenu_write_file(ED_fsmenu_get(), tmp_name);
    }
  }
}

int ED_fsmenu_entry_get_icon(FSMenuEntry *fsentry)
{
  return (fsentry->icon) ? fsentry->icon : ICON_FILE_FOLDER;
}

void ED_fsmenu_entry_set_icon(FSMenuEntry *fsentry, const int icon)
{
  fsentry->icon = icon;
}

static void fsmenu_entry_generate_name(FSMenuEntry *fsentry, char *name, size_t name_size)
{
  int offset = 0;
  int len = name_size;

  if (BLI_path_name_at_index(fsentry->path, -1, &offset, &len)) {
    /* use as size */
    len += 1;
  }

  BLI_strncpy(name, &fsentry->path[offset], std::min(size_t(len), name_size));
  if (!name[0]) {
    name[0] = '/';
    name[1] = '\0';
  }
}

char *ED_fsmenu_entry_get_name(FSMenuEntry *fsentry)
{
  if (fsentry->name[0]) {
    return fsentry->name;
  }

  /* Here we abuse fsm_iter->name, keeping first char nullptr. */
  char *name = fsentry->name + 1;
  size_t name_size = sizeof(fsentry->name) - 1;

  fsmenu_entry_generate_name(fsentry, name, name_size);
  return name;
}

void ED_fsmenu_entry_set_name(FSMenuEntry *fsentry, const char *name)
{
  if (!STREQ(name, fsentry->name)) {
    char tmp_name[FILE_MAXFILE];
    size_t tmp_name_size = sizeof(tmp_name);

    fsmenu_entry_generate_name(fsentry, tmp_name, tmp_name_size);
    if (!name[0] || STREQ(tmp_name, name)) {
      /* reset name to default behavior. */
      fsentry->name[0] = '\0';
    }
    else {
      STRNCPY(fsentry->name, name);
    }

    const std::optional<std::string> user_config_dir = BKE_appdir_folder_id_create(
        BLENDER_USER_CONFIG, nullptr);

    if (user_config_dir.has_value()) {
      BLI_path_join(tmp_name, sizeof(tmp_name), user_config_dir->c_str(), BLENDER_BOOKMARK_FILE);
      fsmenu_write_file(ED_fsmenu_get(), tmp_name);
    }
  }
}

short fsmenu_can_save(FSMenu *fsmenu, FSMenuCategory category, int idx)
{
  FSMenuEntry *fsm_iter;

  for (fsm_iter = ED_fsmenu_get_category(fsmenu, category); fsm_iter && idx;
       fsm_iter = fsm_iter->next)
  {
    idx--;
  }

  return fsm_iter ? fsm_iter->save : 0;
}

void fsmenu_insert_entry(FSMenu *fsmenu,
                         FSMenuCategory category,
                         const char *path,
                         const char *name,
                         int icon,
                         FSMenuInsert flag)
{
  /* NOTE: this function must *not* perform file-system checks on `path`,
   * although the path may be inspected as a literal string.
   *
   * This is important because accessing the file system may reference drives
   * which are offline, network drives requiring an internet connection,
   * external drives that aren't plugged in, etc.
   * Delays in any file-system checks can causes hanging on startup.
   * See !138218 for details. */

  const uint path_len = strlen(path);
  BLI_assert(path_len > 0);
  if (path_len == 0) {
    return;
  }
  BLI_assert(!BLI_path_is_rel(path));
  const bool has_trailing_slash = (path[path_len - 1] == SEP);
  FSMenuEntry *fsm_prev;
  FSMenuEntry *fsm_iter;
  FSMenuEntry *fsm_head;

  fsm_head = ED_fsmenu_get_category(fsmenu, category);
  fsm_prev = fsm_head; /* this is odd and not really correct? */

  for (fsm_iter = fsm_head; fsm_iter; fsm_prev = fsm_iter, fsm_iter = fsm_iter->next) {
    if (fsm_iter->path) {
      /* Compare, with/without the trailing slash in 'path'. */
      const int cmp_ret = BLI_path_ncmp(path, fsm_iter->path, path_len);
      if (cmp_ret == 0 && STREQ(fsm_iter->path + path_len, has_trailing_slash ? "" : SEP_STR)) {
        if (flag & FS_INSERT_FIRST) {
          if (fsm_iter != fsm_head) {
            fsm_prev->next = fsm_iter->next;
            fsm_iter->next = fsm_head;
            ED_fsmenu_set_category(fsmenu, category, fsm_iter);
          }
        }
        return;
      }
      if ((flag & FS_INSERT_SORTED) && cmp_ret < 0) {
        break;
      }
    }
    else {
      /* if we're bookmarking this, file should come
       * before the last separator, only automatically added
       * current dir go after the last separator. */
      if (flag & FS_INSERT_SAVE) {
        break;
      }
    }
  }

  fsm_iter = MEM_mallocN<FSMenuEntry>("fsme");
  fsm_iter->path = has_trailing_slash ? BLI_strdup(path) : BLI_string_joinN(path, SEP_STR);
  fsm_iter->save = (flag & FS_INSERT_SAVE) != 0;

  /* If entry is also in another list, use that icon and maybe name. */
  /* On macOS we get icons and names for System Bookmarks from the FS_CATEGORY_OTHER list. */
  if (ELEM(category, FS_CATEGORY_SYSTEM_BOOKMARKS, FS_CATEGORY_BOOKMARKS, FS_CATEGORY_RECENT)) {

    const FSMenuCategory cats[] = {
        FS_CATEGORY_OTHER,
        FS_CATEGORY_SYSTEM,
        FS_CATEGORY_SYSTEM_BOOKMARKS,
        FS_CATEGORY_BOOKMARKS,
    };
    int i = ARRAY_SIZE(cats);
    if (category == FS_CATEGORY_BOOKMARKS) {
      i--;
    }

    while (i--) {
      FSMenuEntry *tfsm = ED_fsmenu_get_category(fsmenu, cats[i]);
      for (; tfsm; tfsm = tfsm->next) {
        if (STREQ(tfsm->path, fsm_iter->path)) {
          icon = tfsm->icon;
          if (tfsm->name[0] && (!name || !name[0])) {
            name = DATA_(tfsm->name);
          }
          break;
        }
      }
      if (tfsm) {
        break;
      }
    }
  }

  if (name && name[0]) {
    STRNCPY(fsm_iter->name, name);
  }
  else {
    fsm_iter->name[0] = '\0';
  }

  ED_fsmenu_entry_set_icon(fsm_iter, icon);

  if (fsm_prev) {
    if (flag & FS_INSERT_FIRST) {
      fsm_iter->next = fsm_head;
      ED_fsmenu_set_category(fsmenu, category, fsm_iter);
    }
    else {
      fsm_iter->next = fsm_prev->next;
      fsm_prev->next = fsm_iter;
    }
  }
  else {
    fsm_iter->next = fsm_head;
    ED_fsmenu_set_category(fsmenu, category, fsm_iter);
  }
}

void fsmenu_remove_entry(FSMenu *fsmenu, FSMenuCategory category, int idx)
{
  FSMenuEntry *fsm_prev = nullptr;
  FSMenuEntry *fsm_iter;
  FSMenuEntry *fsm_head;

  fsm_head = ED_fsmenu_get_category(fsmenu, category);

  for (fsm_iter = fsm_head; fsm_iter && idx; fsm_prev = fsm_iter, fsm_iter = fsm_iter->next) {
    idx--;
  }

  if (fsm_iter) {
    /* you should only be able to remove entries that were
     * not added by default, like windows drives.
     * also separators (where path == nullptr) shouldn't be removed */
    if (fsm_iter->save && fsm_iter->path) {

      /* remove fsme from list */
      if (fsm_prev) {
        fsm_prev->next = fsm_iter->next;
      }
      else {
        fsm_head = fsm_iter->next;
        ED_fsmenu_set_category(fsmenu, category, fsm_head);
      }
      /* free entry */
      MEM_freeN(fsm_iter->path);
      MEM_freeN(fsm_iter);
    }
  }
}

bool fsmenu_write_file(FSMenu *fsmenu, const char *filepath)
{
  FSMenuEntry *fsm_iter = nullptr;
  char fsm_name[FILE_MAX];
  int nwritten = 0;

  FILE *fp = BLI_fopen(filepath, "w");
  if (!fp) {
    return false;
  }

  bool has_error = false;
  has_error |= (fprintf(fp, "[Bookmarks]\n") < 0);
  for (fsm_iter = ED_fsmenu_get_category(fsmenu, FS_CATEGORY_BOOKMARKS); fsm_iter;
       fsm_iter = fsm_iter->next)
  {
    if (fsm_iter->path && fsm_iter->save) {
      fsmenu_entry_generate_name(fsm_iter, fsm_name, sizeof(fsm_name));
      if (fsm_iter->name[0] && !STREQ(fsm_iter->name, fsm_name)) {
        has_error |= (fprintf(fp, "!%s\n", fsm_iter->name) < 0);
      }
      has_error |= (fprintf(fp, "%s\n", fsm_iter->path) < 0);
    }
  }
  has_error = (fprintf(fp, "[Recent]\n") < 0);
  for (fsm_iter = ED_fsmenu_get_category(fsmenu, FS_CATEGORY_RECENT);
       fsm_iter && (nwritten < FSMENU_RECENT_MAX);
       fsm_iter = fsm_iter->next, nwritten++)
  {
    if (fsm_iter->path && fsm_iter->save) {
      fsmenu_entry_generate_name(fsm_iter, fsm_name, sizeof(fsm_name));
      if (fsm_iter->name[0] && !STREQ(fsm_iter->name, fsm_name)) {
        has_error |= (fprintf(fp, "!%s\n", fsm_iter->name) < 0);
      }
      has_error |= (fprintf(fp, "%s\n", fsm_iter->path) < 0);
    }
  }
  fclose(fp);

  return !has_error;
}

void fsmenu_read_bookmarks(FSMenu *fsmenu, const char *filepath)
{
  char line[FILE_MAXDIR];
  char name[FILE_MAXFILE];
  FSMenuCategory category = FS_CATEGORY_BOOKMARKS;
  FILE *fp;

  fp = BLI_fopen(filepath, "r");
  if (!fp) {
    return;
  }

  name[0] = '\0';

  while (fgets(line, sizeof(line), fp) != nullptr) { /* read a line */
    if (STRPREFIX(line, "[Bookmarks]")) {
      category = FS_CATEGORY_BOOKMARKS;
    }
    else if (STRPREFIX(line, "[Recent]")) {
      category = FS_CATEGORY_RECENT;
    }
    else if (line[0] == '!') {
      int len = strlen(line);
      if (len > 0) {
        if (line[len - 1] == '\n') {
          line[len - 1] = '\0';
        }
        STRNCPY(name, line + 1);
      }
    }
    else {
      int len = strlen(line);
      if (len > 0) {
        if (line[len - 1] == '\n') {
          line[len - 1] = '\0';
        }
        /* Don't check if the path exists before adding because it can be slow on network drives,
         * having a bookmark from a drive that's ejected or so isn't all that bad.
         * See !138218 for details. */
        fsmenu_insert_entry(fsmenu, category, line, name, ICON_FILE_FOLDER, FS_INSERT_SAVE);
      }
      /* always reset name. */
      name[0] = '\0';
    }
  }
  fclose(fp);
}

static void fsmenu_free_category(FSMenu *fsmenu, FSMenuCategory category)
{
  FSMenuEntry *fsm_iter = ED_fsmenu_get_category(fsmenu, category);

  while (fsm_iter) {
    FSMenuEntry *fsm_next = fsm_iter->next;

    if (fsm_iter->path) {
      MEM_freeN(fsm_iter->path);
    }
    MEM_freeN(fsm_iter);

    fsm_iter = fsm_next;
  }
}

void fsmenu_refresh_system_category(FSMenu *fsmenu)
{
  fsmenu_free_category(fsmenu, FS_CATEGORY_SYSTEM);
  ED_fsmenu_set_category(fsmenu, FS_CATEGORY_SYSTEM, nullptr);

  fsmenu_free_category(fsmenu, FS_CATEGORY_SYSTEM_BOOKMARKS);
  ED_fsmenu_set_category(fsmenu, FS_CATEGORY_SYSTEM_BOOKMARKS, nullptr);

  /* Add all entries to system category */
  fsmenu_read_system(fsmenu, true);
}

static void fsmenu_free_ex(FSMenu **fsmenu)
{
  if (*fsmenu != nullptr) {
    fsmenu_free_category(*fsmenu, FS_CATEGORY_SYSTEM);
    fsmenu_free_category(*fsmenu, FS_CATEGORY_SYSTEM_BOOKMARKS);
    fsmenu_free_category(*fsmenu, FS_CATEGORY_BOOKMARKS);
    fsmenu_free_category(*fsmenu, FS_CATEGORY_RECENT);
    fsmenu_free_category(*fsmenu, FS_CATEGORY_OTHER);
    MEM_freeN(*fsmenu);
  }

  *fsmenu = nullptr;
}

void fsmenu_free()
{
  fsmenu_free_ex(&g_fsmenu);
}

int fsmenu_get_active_indices(FSMenu *fsmenu, enum FSMenuCategory category, const char *dir)
{
  FSMenuEntry *fsm_iter = ED_fsmenu_get_category(fsmenu, category);
  int i;

  for (i = 0; fsm_iter; fsm_iter = fsm_iter->next, i++) {
    if (BLI_path_cmp(dir, fsm_iter->path) == 0) {
      return i;
    }
  }

  return -1;
}

void fsmenu_add_common_platform_directories(FSMenu *fsmenu)
{
  /* For all platforms, we add some directories from User Preferences to
   * the FS_CATEGORY_OTHER category so that these directories
   * have the appropriate icons when they are added to the Bookmarks.
   *
   * NOTE: of the preferences support as `//` prefix.
   * Skip them since they depend on the current loaded blend file. */

  auto add_user_dir = [fsmenu](const char *dir, int icon) {
    if (dir[0] && !BLI_path_is_rel(dir)) {
      fsmenu_insert_entry(fsmenu, FS_CATEGORY_OTHER, dir, nullptr, icon, FS_INSERT_LAST);
    }
  };

  add_user_dir(U.fontdir, ICON_FILE_FONT);
  add_user_dir(U.textudir, ICON_FILE_IMAGE);

  LISTBASE_FOREACH (bUserScriptDirectory *, script_dir, &U.script_directories) {
    if (UNLIKELY(script_dir->dir_path[0] == '\0')) {
      continue;
    }
    fsmenu_insert_entry(fsmenu,
                        FS_CATEGORY_OTHER,
                        script_dir->dir_path,
                        script_dir->name,
                        ICON_FILE_SCRIPT,
                        FS_INSERT_LAST);
  }

  add_user_dir(U.sounddir, ICON_FILE_SOUND);
  add_user_dir(U.tempdir, ICON_TEMP);
}
