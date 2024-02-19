/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spfile
 */

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_string_utils.hh"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "BKE_appdir.hh"

#include "ED_fileselect.hh"

#include "UI_resources.hh"
#include "WM_api.hh"
#include "WM_types.hh"

#include "fsmenu.h" /* include ourselves */

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
    g_fsmenu = MEM_cnew<FSMenu>(__func__);
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

void fsmenu_entry_refresh_valid(FSMenuEntry *fsentry)
{
  if (fsentry->path && fsentry->path[0]) {
#ifdef WIN32
    /* XXX Special case, always consider those as valid.
     * Thanks to Windows, which can spend five seconds to perform a mere stat() call on those paths
     * See #43684. */
    const char *exceptions[] = {"A:\\", "B:\\", nullptr};
    const size_t exceptions_len[] = {strlen(exceptions[0]), strlen(exceptions[1]), 0};
    int i;

    for (i = 0; exceptions[i]; i++) {
      if (STRCASEEQLEN(fsentry->path, exceptions[i], exceptions_len[i])) {
        fsentry->valid = true;
        return;
      }
    }
#endif
    fsentry->valid = BLI_is_dir(fsentry->path);
  }
  else {
    fsentry->valid = false;
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
  const uint path_len = strlen(path);
  BLI_assert(path_len > 0);
  if (path_len == 0) {
    return;
  }
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

  fsm_iter = static_cast<FSMenuEntry *>(MEM_mallocN(sizeof(*fsm_iter), "fsme"));
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

  if (flag & FS_INSERT_NO_VALIDATE) {
    fsm_iter->valid = true;
  }
  else {
    fsmenu_entry_refresh_valid(fsm_iter);
  }

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
        /* don't do this because it can be slow on network drives,
         * having a bookmark from a drive that's ejected or so isn't
         * all _that_ bad */
#if 0
        if (BLI_exists(line))
#endif
        {
          fsmenu_insert_entry(fsmenu, category, line, name, ICON_FILE_FOLDER, FS_INSERT_SAVE);
        }
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

static void fsmenu_copy_category(FSMenu *fsmenu_dst,
                                 FSMenu *fsmenu_src,
                                 const FSMenuCategory category)
{
  FSMenuEntry *fsm_dst_prev = nullptr, *fsm_dst_head = nullptr;
  FSMenuEntry *fsm_src_iter = ED_fsmenu_get_category(fsmenu_src, category);

  for (; fsm_src_iter != nullptr; fsm_src_iter = fsm_src_iter->next) {
    FSMenuEntry *fsm_dst = static_cast<FSMenuEntry *>(MEM_dupallocN(fsm_src_iter));
    if (fsm_dst->path != nullptr) {
      fsm_dst->path = static_cast<char *>(MEM_dupallocN(fsm_dst->path));
    }

    if (fsm_dst_prev != nullptr) {
      fsm_dst_prev->next = fsm_dst;
    }
    else {
      fsm_dst_head = fsm_dst;
    }
    fsm_dst_prev = fsm_dst;
  }

  ED_fsmenu_set_category(fsmenu_dst, category, fsm_dst_head);
}

static FSMenu *fsmenu_copy(FSMenu *fsmenu)
{
  FSMenu *fsmenu_copy = static_cast<FSMenu *>(MEM_dupallocN(fsmenu));

  fsmenu_copy_category(fsmenu_copy, fsmenu_copy, FS_CATEGORY_SYSTEM);
  fsmenu_copy_category(fsmenu_copy, fsmenu_copy, FS_CATEGORY_SYSTEM_BOOKMARKS);
  fsmenu_copy_category(fsmenu_copy, fsmenu_copy, FS_CATEGORY_BOOKMARKS);
  fsmenu_copy_category(fsmenu_copy, fsmenu_copy, FS_CATEGORY_RECENT);
  fsmenu_copy_category(fsmenu_copy, fsmenu_copy, FS_CATEGORY_OTHER);

  return fsmenu_copy;
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

/**
 * Thanks to some bookmarks sometimes being network drives that can have tens of seconds of delay
 * before being defined as unreachable by the OS, we need to validate the bookmarks in an
 * asynchronous job.
 */
static void fsmenu_bookmark_validate_job_startjob(void *fsmenuv, wmJobWorkerStatus *worker_status)
{
  FSMenu *fsmenu = static_cast<FSMenu *>(fsmenuv);

  int categories[] = {
      FS_CATEGORY_SYSTEM, FS_CATEGORY_SYSTEM_BOOKMARKS, FS_CATEGORY_BOOKMARKS, FS_CATEGORY_RECENT};

  for (size_t i = ARRAY_SIZE(categories); i--;) {
    FSMenuEntry *fsm_iter = ED_fsmenu_get_category(fsmenu, FSMenuCategory(categories[i]));
    for (; fsm_iter; fsm_iter = fsm_iter->next) {
      if (worker_status->stop) {
        return;
      }
      /* Note that we do not really need atomics primitives or thread locks here, since this only
       * sets one short, which is assumed to be 'atomic'-enough for us here. */
      fsmenu_entry_refresh_valid(fsm_iter);
      worker_status->do_update = true;
    }
  }
}

static void fsmenu_bookmark_validate_job_update(void *fsmenuv)
{
  FSMenu *fsmenu_job = static_cast<FSMenu *>(fsmenuv);

  int categories[] = {
      FS_CATEGORY_SYSTEM, FS_CATEGORY_SYSTEM_BOOKMARKS, FS_CATEGORY_BOOKMARKS, FS_CATEGORY_RECENT};

  for (size_t i = ARRAY_SIZE(categories); i--;) {
    FSMenuEntry *fsm_iter_src = ED_fsmenu_get_category(fsmenu_job, FSMenuCategory(categories[i]));
    FSMenuEntry *fsm_iter_dst = ED_fsmenu_get_category(ED_fsmenu_get(),
                                                       FSMenuCategory(categories[i]));
    for (; fsm_iter_dst != nullptr; fsm_iter_dst = fsm_iter_dst->next) {
      while (fsm_iter_src != nullptr && !STREQ(fsm_iter_dst->path, fsm_iter_src->path)) {
        fsm_iter_src = fsm_iter_src->next;
      }
      if (fsm_iter_src == nullptr) {
        return;
      }
      fsm_iter_dst->valid = fsm_iter_src->valid;
    }
  }
}

static void fsmenu_bookmark_validate_job_end(void *fsmenuv)
{
  /* In case there would be some dangling update... */
  fsmenu_bookmark_validate_job_update(fsmenuv);
}

static void fsmenu_bookmark_validate_job_free(void *fsmenuv)
{
  FSMenu *fsmenu = static_cast<FSMenu *>(fsmenuv);
  fsmenu_free_ex(&fsmenu);
}

static void fsmenu_bookmark_validate_job_start(wmWindowManager *wm)
{
  wmJob *wm_job;
  FSMenu *fsmenu_job = fsmenu_copy(g_fsmenu);

  /* setup job */
  wm_job = WM_jobs_get(wm,
                       wm->winactive,
                       wm,
                       "Validating Bookmarks...",
                       eWM_JobFlag(0),
                       WM_JOB_TYPE_FSMENU_BOOKMARK_VALIDATE);
  WM_jobs_customdata_set(wm_job, fsmenu_job, fsmenu_bookmark_validate_job_free);
  WM_jobs_timer(wm_job, 0.01, NC_SPACE | ND_SPACE_FILE_LIST, NC_SPACE | ND_SPACE_FILE_LIST);
  WM_jobs_callbacks(wm_job,
                    fsmenu_bookmark_validate_job_startjob,
                    nullptr,
                    fsmenu_bookmark_validate_job_update,
                    fsmenu_bookmark_validate_job_end);

  /* start the job */
  WM_jobs_start(wm, wm_job);
}

static void fsmenu_bookmark_validate_job_stop(wmWindowManager *wm)
{
  WM_jobs_kill_type(wm, wm, WM_JOB_TYPE_FSMENU_BOOKMARK_VALIDATE);
}

void fsmenu_refresh_bookmarks_status(wmWindowManager *wm, FSMenu *fsmenu)
{
  BLI_assert(fsmenu == ED_fsmenu_get());
  UNUSED_VARS_NDEBUG(fsmenu);

  fsmenu_bookmark_validate_job_stop(wm);
  fsmenu_bookmark_validate_job_start(wm);
}
