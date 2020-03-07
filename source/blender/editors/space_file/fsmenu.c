/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup spfile
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_blenlib.h"
#include "BLI_ghash.h"

#include "BLT_translation.h"

#include "BKE_appdir.h"

#include "ED_fileselect.h"

#ifdef WIN32
/* Need to include windows.h so _WIN32_IE is defined. */
#  include <windows.h>
/* For SHGetSpecialFolderPath, has to be done before BLI_winstuff
 * because 'near' is disabled through BLI_windstuff. */
#  include <shlobj.h>
#  include "BLI_winstuff.h"
#endif

#include "WM_api.h"
#include "WM_types.h"
#include "UI_interface_icons.h"
#include "UI_resources.h"

#ifdef __APPLE__
#  include <Carbon/Carbon.h>
#endif /* __APPLE__ */

#ifdef __linux__
#  include <mntent.h>
#  include "BLI_fileops_types.h"
#endif

#include "fsmenu.h" /* include ourselves */

/* FSMENU HANDLING */

typedef struct FSMenu {
  FSMenuEntry *fsmenu_system;
  FSMenuEntry *fsmenu_system_bookmarks;
  FSMenuEntry *fsmenu_bookmarks;
  FSMenuEntry *fsmenu_recent;
  FSMenuEntry *fsmenu_other;
} FSMenu;

static FSMenu *g_fsmenu = NULL;

FSMenu *ED_fsmenu_get(void)
{
  if (!g_fsmenu) {
    g_fsmenu = MEM_callocN(sizeof(struct FSMenu), "fsmenu");
  }
  return g_fsmenu;
}

struct FSMenuEntry *ED_fsmenu_get_category(struct FSMenu *fsmenu, FSMenuCategory category)
{
  FSMenuEntry *fsm_head = NULL;

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

/* -------------------------------------------------------------------- */
/** \name XDG User Directory Support (Unix)
 *
 * Generic Unix, Use XDG when available, otherwise fallback to the home directory.
 * \{ */

/**
 * Look for `user-dirs.dirs`, where localized or custom user folders are defined,
 * and store their paths in a GHash.
 */
static GHash *fsmenu_xdg_user_dirs_parse(const char *home)
{
  /* Add to the default for variable, equals & quotes. */
  char l[128 + FILE_MAXDIR];
  FILE *fp;

  /* Check if the config file exists. */
  {
    char filepath[FILE_MAX];
    const char *xdg_config_home = getenv("XDG_CONFIG_HOME");
    if (xdg_config_home != NULL) {
      BLI_path_join(filepath, sizeof(filepath), xdg_config_home, "user-dirs.dirs", NULL);
    }
    else {
      BLI_path_join(filepath, sizeof(filepath), home, ".config", "user-dirs.dirs", NULL);
    }
    fp = BLI_fopen(filepath, "r");
    if (!fp) {
      return NULL;
    }
  }
  /* By default there are 8 paths. */
  GHash *xdg_map = BLI_ghash_str_new_ex(__func__, 8);
  while (fgets(l, sizeof(l), fp) != NULL) { /* read a line */

    /* Avoid inserting invalid values. */
    if (STRPREFIX(l, "XDG_")) {
      char *l_value = strchr(l, '=');
      if (l_value != NULL) {
        *l_value = '\0';
        l_value++;

        BLI_str_rstrip(l_value);
        const uint l_value_len = strlen(l_value);
        if ((l_value[0] == '"') && (l_value_len > 0) && (l_value[l_value_len - 1] == '"')) {
          l_value[l_value_len - 1] = '\0';
          l_value++;

          char l_value_expanded[FILE_MAX];
          char *l_value_final = l_value;

          /* This is currently the only variable used.
           * Based on the 'user-dirs.dirs' man page,
           * there is no need to resolve arbitrary environment variables. */
          if (STRPREFIX(l_value, "$HOME" SEP_STR)) {
            BLI_path_join(l_value_expanded, sizeof(l_value_expanded), home, l_value + 6, NULL);
            l_value_final = l_value_expanded;
          }

          BLI_ghash_insert(xdg_map, BLI_strdup(l), BLI_strdup(l_value_final));
        }
      }
    }
  }
  return xdg_map;
}

static void fsmenu_xdg_user_dirs_free(GHash *xdg_map)
{
  if (xdg_map != NULL) {
    BLI_ghash_free(xdg_map, MEM_freeN, MEM_freeN);
  }
}

/**
 *  Add fsmenu entry for system folders on linux.
 *  - Check if a path is stored in the GHash generated from user-dirs.dirs
 *  - If not, check for a default path in $HOME
 *
 *  \param key: Use `user-dirs.dirs` format "XDG_EXAMPLE_DIR"
 *  \param default_path: Directory name to check in $HOME, also used for the menu entry name.
 */
static void fsmenu_xdg_insert_entry(GHash *xdg_map,
                                    struct FSMenu *fsmenu,
                                    const char *key,
                                    const char *default_path,
                                    int icon,
                                    const char *home)
{
  char xdg_path_buf[FILE_MAXDIR];
  const char *xdg_path = xdg_map ? BLI_ghash_lookup(xdg_map, key) : NULL;
  if (xdg_path == NULL) {
    BLI_path_join(xdg_path_buf, sizeof(xdg_path_buf), home, default_path, NULL);
    xdg_path = xdg_path_buf;
  }
  fsmenu_insert_entry(
      fsmenu, FS_CATEGORY_SYSTEM_BOOKMARKS, xdg_path, IFACE_(default_path), icon, FS_INSERT_LAST);
}

/** \} */

void ED_fsmenu_set_category(struct FSMenu *fsmenu, FSMenuCategory category, FSMenuEntry *fsm_head)
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

int ED_fsmenu_get_nentries(struct FSMenu *fsmenu, FSMenuCategory category)
{
  FSMenuEntry *fsm_iter;
  int count = 0;

  for (fsm_iter = ED_fsmenu_get_category(fsmenu, category); fsm_iter; fsm_iter = fsm_iter->next) {
    count++;
  }

  return count;
}

FSMenuEntry *ED_fsmenu_get_entry(struct FSMenu *fsmenu, FSMenuCategory category, int index)
{
  FSMenuEntry *fsm_iter;

  for (fsm_iter = ED_fsmenu_get_category(fsmenu, category); fsm_iter && index;
       fsm_iter = fsm_iter->next) {
    index--;
  }

  return fsm_iter;
}

char *ED_fsmenu_entry_get_path(struct FSMenuEntry *fsentry)
{
  return fsentry->path;
}

void ED_fsmenu_entry_set_path(struct FSMenuEntry *fsentry, const char *path)
{
  if ((!fsentry->path || !path || !STREQ(path, fsentry->path)) && (fsentry->path != path)) {
    char tmp_name[FILE_MAXFILE];

    MEM_SAFE_FREE(fsentry->path);

    fsentry->path = (path && path[0]) ? BLI_strdup(path) : NULL;

    BLI_join_dirfile(tmp_name,
                     sizeof(tmp_name),
                     BKE_appdir_folder_id_create(BLENDER_USER_CONFIG, NULL),
                     BLENDER_BOOKMARK_FILE);
    fsmenu_write_file(ED_fsmenu_get(), tmp_name);
  }
}

int ED_fsmenu_entry_get_icon(struct FSMenuEntry *fsentry)
{
  return (fsentry->icon) ? fsentry->icon : ICON_FILE_FOLDER;
}

void ED_fsmenu_entry_set_icon(struct FSMenuEntry *fsentry, const int icon)
{
  fsentry->icon = icon;
}

static void fsmenu_entry_generate_name(struct FSMenuEntry *fsentry, char *name, size_t name_size)
{
  int offset = 0;
  int len = name_size;

  if (BLI_path_name_at_index(fsentry->path, -1, &offset, &len)) {
    /* use as size */
    len += 1;
  }

  BLI_strncpy(name, &fsentry->path[offset], MIN2(len, name_size));
  if (!name[0]) {
    name[0] = '/';
    name[1] = '\0';
  }
}

char *ED_fsmenu_entry_get_name(struct FSMenuEntry *fsentry)
{
  if (fsentry->name[0]) {
    return fsentry->name;
  }
  else {
    /* Here we abuse fsm_iter->name, keeping first char NULL. */
    char *name = fsentry->name + 1;
    size_t name_size = sizeof(fsentry->name) - 1;

    fsmenu_entry_generate_name(fsentry, name, name_size);
    return name;
  }
}

void ED_fsmenu_entry_set_name(struct FSMenuEntry *fsentry, const char *name)
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
      BLI_strncpy(fsentry->name, name, sizeof(fsentry->name));
    }

    BLI_join_dirfile(tmp_name,
                     sizeof(tmp_name),
                     BKE_appdir_folder_id_create(BLENDER_USER_CONFIG, NULL),
                     BLENDER_BOOKMARK_FILE);
    fsmenu_write_file(ED_fsmenu_get(), tmp_name);
  }
}

void fsmenu_entry_refresh_valid(struct FSMenuEntry *fsentry)
{
  if (fsentry->path && fsentry->path[0]) {
#ifdef WIN32
    /* XXX Special case, always consider those as valid.
     * Thanks to Windows, which can spend five seconds to perform a mere stat() call on those paths
     * See T43684. */
    const char *exceptions[] = {"A:\\", "B:\\", NULL};
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

short fsmenu_can_save(struct FSMenu *fsmenu, FSMenuCategory category, int idx)
{
  FSMenuEntry *fsm_iter;

  for (fsm_iter = ED_fsmenu_get_category(fsmenu, category); fsm_iter && idx;
       fsm_iter = fsm_iter->next) {
    idx--;
  }

  return fsm_iter ? fsm_iter->save : 0;
}

void fsmenu_insert_entry(struct FSMenu *fsmenu,
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
      else if ((flag & FS_INSERT_SORTED) && cmp_ret < 0) {
        break;
      }
    }
    else {
      /* if we're bookmarking this, file should come
       * before the last separator, only automatically added
       * current dir go after the last sep. */
      if (flag & FS_INSERT_SAVE) {
        break;
      }
    }
  }

  fsm_iter = MEM_mallocN(sizeof(*fsm_iter), "fsme");
  if (has_trailing_slash) {
    fsm_iter->path = BLI_strdup(path);
  }
  else {
    fsm_iter->path = BLI_strdupn(path, path_len + 1);
    fsm_iter->path[path_len] = SEP;
    fsm_iter->path[path_len + 1] = '\0';
  }
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
            name = tfsm->name;
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
    BLI_strncpy(fsm_iter->name, name, sizeof(fsm_iter->name));
  }
  else {
    fsm_iter->name[0] = '\0';
  }

  ED_fsmenu_entry_set_icon(fsm_iter, icon);

  fsmenu_entry_refresh_valid(fsm_iter);

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

void fsmenu_remove_entry(struct FSMenu *fsmenu, FSMenuCategory category, int idx)
{
  FSMenuEntry *fsm_prev = NULL;
  FSMenuEntry *fsm_iter;
  FSMenuEntry *fsm_head;

  fsm_head = ED_fsmenu_get_category(fsmenu, category);

  for (fsm_iter = fsm_head; fsm_iter && idx; fsm_prev = fsm_iter, fsm_iter = fsm_iter->next) {
    idx--;
  }

  if (fsm_iter) {
    /* you should only be able to remove entries that were
     * not added by default, like windows drives.
     * also separators (where path == NULL) shouldn't be removed */
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

void fsmenu_write_file(struct FSMenu *fsmenu, const char *filename)
{
  FSMenuEntry *fsm_iter = NULL;
  char fsm_name[FILE_MAX];
  int nwritten = 0;

  FILE *fp = BLI_fopen(filename, "w");
  if (!fp) {
    return;
  }

  fprintf(fp, "[Bookmarks]\n");
  for (fsm_iter = ED_fsmenu_get_category(fsmenu, FS_CATEGORY_BOOKMARKS); fsm_iter;
       fsm_iter = fsm_iter->next) {
    if (fsm_iter->path && fsm_iter->save) {
      fsmenu_entry_generate_name(fsm_iter, fsm_name, sizeof(fsm_name));
      if (fsm_iter->name[0] && !STREQ(fsm_iter->name, fsm_name)) {
        fprintf(fp, "!%s\n", fsm_iter->name);
      }
      fprintf(fp, "%s\n", fsm_iter->path);
    }
  }
  fprintf(fp, "[Recent]\n");
  for (fsm_iter = ED_fsmenu_get_category(fsmenu, FS_CATEGORY_RECENT);
       fsm_iter && (nwritten < FSMENU_RECENT_MAX);
       fsm_iter = fsm_iter->next, nwritten++) {
    if (fsm_iter->path && fsm_iter->save) {
      fsmenu_entry_generate_name(fsm_iter, fsm_name, sizeof(fsm_name));
      if (fsm_iter->name[0] && !STREQ(fsm_iter->name, fsm_name)) {
        fprintf(fp, "!%s\n", fsm_iter->name);
      }
      fprintf(fp, "%s\n", fsm_iter->path);
    }
  }
  fclose(fp);
}

void fsmenu_read_bookmarks(struct FSMenu *fsmenu, const char *filename)
{
  char line[FILE_MAXDIR];
  char name[FILE_MAXFILE];
  FSMenuCategory category = FS_CATEGORY_BOOKMARKS;
  FILE *fp;

  fp = BLI_fopen(filename, "r");
  if (!fp) {
    return;
  }

  name[0] = '\0';

  while (fgets(line, sizeof(line), fp) != NULL) { /* read a line */
    if (STREQLEN(line, "[Bookmarks]", 11)) {
      category = FS_CATEGORY_BOOKMARKS;
    }
    else if (STREQLEN(line, "[Recent]", 8)) {
      category = FS_CATEGORY_RECENT;
    }
    else if (line[0] == '!') {
      int len = strlen(line);
      if (len > 0) {
        if (line[len - 1] == '\n') {
          line[len - 1] = '\0';
        }
        BLI_strncpy(name, line + 1, sizeof(name));
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

#ifdef WIN32
/* Add a Windows known folder path to the System list. */
static void fsmenu_add_windows_folder(struct FSMenu *fsmenu,
                                      FSMenuCategory category,
                                      REFKNOWNFOLDERID rfid,
                                      const char *name,
                                      const int icon,
                                      FSMenuInsert flag)
{
  LPWSTR pPath;
  char line[FILE_MAXDIR];
  if (SHGetKnownFolderPath(rfid, 0, NULL, &pPath) == S_OK) {
    BLI_strncpy_wchar_as_utf8(line, pPath, FILE_MAXDIR);
    CoTaskMemFree(pPath);
    fsmenu_insert_entry(fsmenu, category, line, name, icon, flag);
  }
}
#endif

void fsmenu_read_system(struct FSMenu *fsmenu, int read_bookmarks)
{
  char line[FILE_MAXDIR];
#ifdef WIN32
  /* Add the drive names to the listing */
  {
    wchar_t wline[FILE_MAXDIR];
    __int64 tmp;
    char tmps[4], *name;
    int i;

    tmp = GetLogicalDrives();

    for (i = 0; i < 26; i++) {
      if ((tmp >> i) & 1) {
        tmps[0] = 'A' + i;
        tmps[1] = ':';
        tmps[2] = '\\';
        tmps[3] = '\0';
        name = NULL;

        /* Flee from horrible win querying hover floppy drives! */
        if (i > 1) {
          /* Try to get a friendly drive description. */
          SHFILEINFOW shFile = {0};
          BLI_strncpy_wchar_from_utf8(wline, tmps, 4);
          if (SHGetFileInfoW(wline, 0, &shFile, sizeof(SHFILEINFOW), SHGFI_DISPLAYNAME)) {
            BLI_strncpy_wchar_as_utf8(line, shFile.szDisplayName, FILE_MAXDIR);
            name = line;
          }
        }
        if (name == NULL) {
          name = tmps;
        }

        int icon = ICON_DISK_DRIVE;
        switch (GetDriveType(tmps)) {
          case DRIVE_REMOVABLE:
            icon = ICON_EXTERNAL_DRIVE;
            break;
          case DRIVE_CDROM:
            icon = ICON_DISC;
            break;
          case DRIVE_FIXED:
          case DRIVE_RAMDISK:
            icon = ICON_DISK_DRIVE;
            break;
          case DRIVE_REMOTE:
            icon = ICON_NETWORK_DRIVE;
            break;
        }

        fsmenu_insert_entry(fsmenu, FS_CATEGORY_SYSTEM, tmps, name, icon, FS_INSERT_SORTED);
      }
    }

    /* Get Special Folder Locations. */
    if (read_bookmarks) {

      /* These items are shown in System List. */
      fsmenu_add_windows_folder(fsmenu,
                                FS_CATEGORY_SYSTEM_BOOKMARKS,
                                &FOLDERID_Profile,
                                IFACE_("Home"),
                                ICON_HOME,
                                FS_INSERT_LAST);
      fsmenu_add_windows_folder(fsmenu,
                                FS_CATEGORY_SYSTEM_BOOKMARKS,
                                &FOLDERID_Desktop,
                                IFACE_("Desktop"),
                                ICON_DESKTOP,
                                FS_INSERT_LAST);
      fsmenu_add_windows_folder(fsmenu,
                                FS_CATEGORY_SYSTEM_BOOKMARKS,
                                &FOLDERID_Documents,
                                IFACE_("Documents"),
                                ICON_DOCUMENTS,
                                FS_INSERT_LAST);
      fsmenu_add_windows_folder(fsmenu,
                                FS_CATEGORY_SYSTEM_BOOKMARKS,
                                &FOLDERID_Downloads,
                                IFACE_("Downloads"),
                                ICON_IMPORT,
                                FS_INSERT_LAST);
      fsmenu_add_windows_folder(fsmenu,
                                FS_CATEGORY_SYSTEM_BOOKMARKS,
                                &FOLDERID_Music,
                                IFACE_("Music"),
                                ICON_FILE_SOUND,
                                FS_INSERT_LAST);
      fsmenu_add_windows_folder(fsmenu,
                                FS_CATEGORY_SYSTEM_BOOKMARKS,
                                &FOLDERID_Pictures,
                                IFACE_("Pictures"),
                                ICON_FILE_IMAGE,
                                FS_INSERT_LAST);
      fsmenu_add_windows_folder(fsmenu,
                                FS_CATEGORY_SYSTEM_BOOKMARKS,
                                &FOLDERID_Videos,
                                IFACE_("Videos"),
                                ICON_FILE_MOVIE,
                                FS_INSERT_LAST);
      fsmenu_add_windows_folder(fsmenu,
                                FS_CATEGORY_SYSTEM_BOOKMARKS,
                                &FOLDERID_Fonts,
                                IFACE_("Fonts"),
                                ICON_FILE_FONT,
                                FS_INSERT_LAST);

      /* These items are just put in path cache for thumbnail views and if bookmarked. */

      fsmenu_add_windows_folder(
          fsmenu, FS_CATEGORY_OTHER, &FOLDERID_UserProfiles, NULL, ICON_COMMUNITY, FS_INSERT_LAST);

      fsmenu_add_windows_folder(
          fsmenu, FS_CATEGORY_OTHER, &FOLDERID_SkyDrive, NULL, ICON_URL, FS_INSERT_LAST);
    }
  }
#else
#  ifdef __APPLE__
  {
    /* We store some known macOS system paths and corresponding icons
     * and names in the FS_CATEGORY_OTHER (not displayed directly) category. */
    fsmenu_insert_entry(fsmenu,
                        FS_CATEGORY_OTHER,
                        "/Library/Fonts/",
                        IFACE_("Fonts"),
                        ICON_FILE_FONT,
                        FS_INSERT_LAST);
    fsmenu_insert_entry(fsmenu,
                        FS_CATEGORY_OTHER,
                        "/Applications/",
                        IFACE_("Applications"),
                        ICON_FILE_FOLDER,
                        FS_INSERT_LAST);

    const char *home = BLI_getenv("HOME");

#    define FS_MACOS_PATH(path, name, icon) \
      BLI_snprintf(line, sizeof(line), path, home); \
      fsmenu_insert_entry(fsmenu, FS_CATEGORY_OTHER, line, name, icon, FS_INSERT_LAST);

    FS_MACOS_PATH("%s/", NULL, ICON_HOME)
    FS_MACOS_PATH("%s/Desktop/", IFACE_("Desktop"), ICON_DESKTOP)
    FS_MACOS_PATH("%s/Documents/", IFACE_("Documents"), ICON_DOCUMENTS)
    FS_MACOS_PATH("%s/Downloads/", IFACE_("Downloads"), ICON_IMPORT)
    FS_MACOS_PATH("%s/Movies/", IFACE_("Movies"), ICON_FILE_MOVIE)
    FS_MACOS_PATH("%s/Music/", IFACE_("Music"), ICON_FILE_SOUND)
    FS_MACOS_PATH("%s/Pictures/", IFACE_("Pictures"), ICON_FILE_IMAGE)
    FS_MACOS_PATH("%s/Library/Fonts/", IFACE_("Fonts"), ICON_FILE_FONT)

#    undef FS_MACOS_PATH

    /* Get mounted volumes better method OSX 10.6 and higher, see:
     * https://developer.apple.com/library/mac/#documentation/CoreFOundation/Reference/CFURLRef/Reference/reference.html
     */

    /* We get all volumes sorted including network and do not relay
     * on user-defined finder visibility, less confusing. */

    CFURLRef cfURL = NULL;
    CFURLEnumeratorResult result = kCFURLEnumeratorSuccess;
    CFURLEnumeratorRef volEnum = CFURLEnumeratorCreateForMountedVolumes(
        NULL, kCFURLEnumeratorSkipInvisibles, NULL);

    while (result != kCFURLEnumeratorEnd) {
      char defPath[FILE_MAX];

      result = CFURLEnumeratorGetNextURL(volEnum, &cfURL, NULL);
      if (result != kCFURLEnumeratorSuccess) {
        continue;
      }

      CFURLGetFileSystemRepresentation(cfURL, false, (UInt8 *)defPath, FILE_MAX);

      /* Get name of the volume. */
      char name[FILE_MAXFILE] = "";
      CFStringRef nameString = NULL;
      CFURLCopyResourcePropertyForKey(cfURL, kCFURLVolumeLocalizedNameKey, &nameString, NULL);
      if (nameString != NULL) {
        CFStringGetCString(nameString, name, sizeof(name), kCFStringEncodingUTF8);
        CFRelease(nameString);
      }

      /* Set icon for regular, removable or network drive. */
      int icon = ICON_DISK_DRIVE;
      CFBooleanRef localKey = NULL;
      CFURLCopyResourcePropertyForKey(cfURL, kCFURLVolumeIsLocalKey, &localKey, NULL);
      if (localKey != NULL) {
        if (!CFBooleanGetValue(localKey)) {
          icon = ICON_NETWORK_DRIVE;
        }
        else {
          CFBooleanRef ejectableKey = NULL;
          CFURLCopyResourcePropertyForKey(cfURL, kCFURLVolumeIsEjectableKey, &ejectableKey, NULL);
          if (ejectableKey != NULL) {
            if (CFBooleanGetValue(ejectableKey)) {
              icon = ICON_EXTERNAL_DRIVE;
            }
            CFRelease(ejectableKey);
          }
        }
        CFRelease(localKey);
      }

      fsmenu_insert_entry(
          fsmenu, FS_CATEGORY_SYSTEM, defPath, name[0] ? name : NULL, icon, FS_INSERT_SORTED);
    }

    CFRelease(volEnum);

    /* Finally get user favorite places */
    if (read_bookmarks) {
      UInt32 seed;
      LSSharedFileListRef list = LSSharedFileListCreate(
          NULL, kLSSharedFileListFavoriteItems, NULL);
      CFArrayRef pathesArray = LSSharedFileListCopySnapshot(list, &seed);
      CFIndex pathesCount = CFArrayGetCount(pathesArray);

      for (CFIndex i = 0; i < pathesCount; i++) {
        LSSharedFileListItemRef itemRef = (LSSharedFileListItemRef)CFArrayGetValueAtIndex(
            pathesArray, i);

        CFURLRef cfURL = NULL;
        OSErr err = LSSharedFileListItemResolve(itemRef,
                                                kLSSharedFileListNoUserInteraction |
                                                    kLSSharedFileListDoNotMountVolumes,
                                                &cfURL,
                                                NULL);
        if (err != noErr || !cfURL) {
          continue;
        }

        CFStringRef pathString = CFURLCopyFileSystemPath(cfURL, kCFURLPOSIXPathStyle);

        if (pathString == NULL ||
            !CFStringGetCString(pathString, line, sizeof(line), kCFStringEncodingUTF8)) {
          continue;
        }

        /* Exclude "all my files" as it makes no sense in blender fileselector */
        /* Exclude "airdrop" if wlan not active as it would show "" ) */
        if (!strstr(line, "myDocuments.cannedSearch") && (*line != '\0')) {
          fsmenu_insert_entry(
              fsmenu, FS_CATEGORY_SYSTEM_BOOKMARKS, line, NULL, ICON_FILE_FOLDER, FS_INSERT_LAST);
        }

        CFRelease(pathString);
        CFRelease(cfURL);
      }

      CFRelease(pathesArray);
      CFRelease(list);
    }
  }
#  else
  /* unix */
  {
    const char *home = BLI_getenv("HOME");

    if (read_bookmarks && home) {

      fsmenu_insert_entry(
          fsmenu, FS_CATEGORY_SYSTEM_BOOKMARKS, home, IFACE_("Home"), ICON_HOME, FS_INSERT_LAST);

      /* Follow the XDG spec, check if these are available. */
      GHash *xdg_map = fsmenu_xdg_user_dirs_parse(home);

      struct {
        const char *key;
        const char *default_path;
        BIFIconID icon;
      } xdg_items[] = {
          {"XDG_DESKTOP_DIR", "Desktop", ICON_DESKTOP},
          {"XDG_DOCUMENTS_DIR", "Documents", ICON_DOCUMENTS},
          {"XDG_DOWNLOAD_DIR", "Downloads", ICON_IMPORT},
          {"XDG_VIDEOS_DIR", "Videos", ICON_FILE_MOVIE},
          {"XDG_PICTURES_DIR", "Pictures", ICON_FILE_IMAGE},
          {"XDG_MUSIC_DIR", "Music", ICON_FILE_SOUND},
      };

      for (int i = 0; i < ARRAY_SIZE(xdg_items); i++) {
        fsmenu_xdg_insert_entry(
            xdg_map, fsmenu, xdg_items[i].key, xdg_items[i].default_path, xdg_items[i].icon, home);
      }

      fsmenu_xdg_user_dirs_free(xdg_map);
    }

    {
      int found = 0;
#    ifdef __linux__
      /* loop over mount points */
      struct mntent *mnt;
      FILE *fp;

      fp = setmntent(MOUNTED, "r");
      if (fp == NULL) {
        fprintf(stderr, "could not get a list of mounted filesystems\n");
      }
      else {
        while ((mnt = getmntent(fp))) {
          if (STRPREFIX(mnt->mnt_dir, "/boot")) {
            /* Hide share not usable to the user. */
            continue;
          }
          else if (!STRPREFIX(mnt->mnt_fsname, "/dev")) {
            continue;
          }
          else if (STRPREFIX(mnt->mnt_fsname, "/dev/loop")) {
            /* The dev/loop* entries are SNAPS used by desktop environment
             * (Gnome) no need for them to show up in the list. */
            continue;
          }

          fsmenu_insert_entry(
              fsmenu, FS_CATEGORY_SYSTEM, mnt->mnt_dir, NULL, ICON_DISK_DRIVE, FS_INSERT_SORTED);

          found = 1;
        }
        if (endmntent(fp) == 0) {
          fprintf(stderr, "could not close the list of mounted filesystems\n");
        }
      }
      /* Check gvfs shares. */
      const char *const xdg_runtime_dir = BLI_getenv("XDG_RUNTIME_DIR");
      if (xdg_runtime_dir != NULL) {
        struct direntry *dir;
        char name[FILE_MAX];
        BLI_join_dirfile(name, sizeof(name), xdg_runtime_dir, "gvfs/");
        const uint dir_len = BLI_filelist_dir_contents(name, &dir);
        for (uint i = 0; i < dir_len; i++) {
          if ((dir[i].type & S_IFDIR)) {
            const char *dirname = dir[i].relname;
            if (dirname[0] != '.') {
              /* Dir names contain a lot of unwanted text.
               * Assuming every entry ends with the share name */
              const char *label = strstr(dirname, "share=");
              if (label != NULL) {
                /* Move pointer so "share=" is trimmed off
                 * or use full dirname as label. */
                const char *label_test = label + 6;
                label = *label_test ? label_test : dirname;
              }
              BLI_snprintf(line, sizeof(line), "%s%s", name, dirname);
              fsmenu_insert_entry(
                  fsmenu, FS_CATEGORY_SYSTEM, line, label, ICON_NETWORK_DRIVE, FS_INSERT_SORTED);
              found = 1;
            }
          }
        }
        BLI_filelist_free(dir, dir_len);
      }
#    endif

      /* fallback */
      if (!found) {
        fsmenu_insert_entry(
            fsmenu, FS_CATEGORY_SYSTEM, "/", NULL, ICON_DISK_DRIVE, FS_INSERT_SORTED);
      }
    }
  }
#  endif
#endif

#if defined(WIN32) || defined(__APPLE__)
  /* Quiet warnings. */
  UNUSED_VARS(fsmenu_xdg_insert_entry, fsmenu_xdg_user_dirs_parse, fsmenu_xdg_user_dirs_free);
#endif

  /* For all platforms, we add some directories from User Preferences to
   * the FS_CATEGORY_OTHER category so that these directories
   * have the appropriate icons when they are added to the Bookmarks. */
#define FS_UDIR_PATH(dir, icon) \
  if (BLI_strnlen(dir, 3) > 2) { \
    fsmenu_insert_entry(fsmenu, FS_CATEGORY_OTHER, dir, NULL, icon, FS_INSERT_LAST); \
  }

  FS_UDIR_PATH(U.fontdir, ICON_FILE_FONT)
  FS_UDIR_PATH(U.textudir, ICON_FILE_IMAGE)
  FS_UDIR_PATH(U.pythondir, ICON_FILE_SCRIPT)
  FS_UDIR_PATH(U.sounddir, ICON_FILE_SOUND)
  FS_UDIR_PATH(U.tempdir, ICON_TEMP)

#undef FS_UDIR_PATH
}

static void fsmenu_free_category(struct FSMenu *fsmenu, FSMenuCategory category)
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

void fsmenu_refresh_system_category(struct FSMenu *fsmenu)
{
  fsmenu_free_category(fsmenu, FS_CATEGORY_SYSTEM);
  ED_fsmenu_set_category(fsmenu, FS_CATEGORY_SYSTEM, NULL);

  fsmenu_free_category(fsmenu, FS_CATEGORY_SYSTEM_BOOKMARKS);
  ED_fsmenu_set_category(fsmenu, FS_CATEGORY_SYSTEM_BOOKMARKS, NULL);

  /* Add all entries to system category */
  fsmenu_read_system(fsmenu, true);
}

static void fsmenu_free_ex(FSMenu **fsmenu)
{
  if (*fsmenu != NULL) {
    fsmenu_free_category(*fsmenu, FS_CATEGORY_SYSTEM);
    fsmenu_free_category(*fsmenu, FS_CATEGORY_SYSTEM_BOOKMARKS);
    fsmenu_free_category(*fsmenu, FS_CATEGORY_BOOKMARKS);
    fsmenu_free_category(*fsmenu, FS_CATEGORY_RECENT);
    fsmenu_free_category(*fsmenu, FS_CATEGORY_OTHER);
    MEM_freeN(*fsmenu);
  }

  *fsmenu = NULL;
}

void fsmenu_free(void)
{
  fsmenu_free_ex(&g_fsmenu);
}

static void fsmenu_copy_category(struct FSMenu *fsmenu_dst,
                                 struct FSMenu *fsmenu_src,
                                 const FSMenuCategory category)
{
  FSMenuEntry *fsm_dst_prev = NULL, *fsm_dst_head = NULL;
  FSMenuEntry *fsm_src_iter = ED_fsmenu_get_category(fsmenu_src, category);

  for (; fsm_src_iter != NULL; fsm_src_iter = fsm_src_iter->next) {
    FSMenuEntry *fsm_dst = MEM_dupallocN(fsm_src_iter);
    if (fsm_dst->path != NULL) {
      fsm_dst->path = MEM_dupallocN(fsm_dst->path);
    }

    if (fsm_dst_prev != NULL) {
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
  FSMenu *fsmenu_copy = MEM_dupallocN(fsmenu);

  fsmenu_copy_category(fsmenu_copy, fsmenu_copy, FS_CATEGORY_SYSTEM);
  fsmenu_copy_category(fsmenu_copy, fsmenu_copy, FS_CATEGORY_SYSTEM_BOOKMARKS);
  fsmenu_copy_category(fsmenu_copy, fsmenu_copy, FS_CATEGORY_BOOKMARKS);
  fsmenu_copy_category(fsmenu_copy, fsmenu_copy, FS_CATEGORY_RECENT);
  fsmenu_copy_category(fsmenu_copy, fsmenu_copy, FS_CATEGORY_OTHER);

  return fsmenu_copy;
}

int fsmenu_get_active_indices(struct FSMenu *fsmenu, enum FSMenuCategory category, const char *dir)
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

/* Thanks to some bookmarks sometimes being network drives that can have tens of seconds of delay
 * before being defined as unreachable by the OS, we need to validate the bookmarks in an async
 * job...
 */
static void fsmenu_bookmark_validate_job_startjob(void *fsmenuv,
                                                  short *stop,
                                                  short *do_update,
                                                  float *UNUSED(progress))
{
  FSMenu *fsmenu = fsmenuv;

  int categories[] = {
      FS_CATEGORY_SYSTEM, FS_CATEGORY_SYSTEM_BOOKMARKS, FS_CATEGORY_BOOKMARKS, FS_CATEGORY_RECENT};

  for (size_t i = ARRAY_SIZE(categories); i--;) {
    FSMenuEntry *fsm_iter = ED_fsmenu_get_category(fsmenu, categories[i]);
    for (; fsm_iter; fsm_iter = fsm_iter->next) {
      if (*stop) {
        return;
      }
      /* Note that we do not really need atomics primitives or thread locks here, since this only
       * sets one short, which is assumed to be 'atomic'-enough for us here. */
      fsmenu_entry_refresh_valid(fsm_iter);
      *do_update = true;
    }
  }
}

static void fsmenu_bookmark_validate_job_update(void *fsmenuv)
{
  FSMenu *fsmenu_job = fsmenuv;

  int categories[] = {
      FS_CATEGORY_SYSTEM, FS_CATEGORY_SYSTEM_BOOKMARKS, FS_CATEGORY_BOOKMARKS, FS_CATEGORY_RECENT};

  for (size_t i = ARRAY_SIZE(categories); i--;) {
    FSMenuEntry *fsm_iter_src = ED_fsmenu_get_category(fsmenu_job, categories[i]);
    FSMenuEntry *fsm_iter_dst = ED_fsmenu_get_category(ED_fsmenu_get(), categories[i]);
    for (; fsm_iter_dst != NULL; fsm_iter_dst = fsm_iter_dst->next) {
      while (fsm_iter_src != NULL && !STREQ(fsm_iter_dst->path, fsm_iter_src->path)) {
        fsm_iter_src = fsm_iter_src->next;
      }
      if (fsm_iter_src == NULL) {
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
  FSMenu *fsmenu = fsmenuv;
  fsmenu_free_ex(&fsmenu);
}

static void fsmenu_bookmark_validate_job_start(wmWindowManager *wm)
{
  wmJob *wm_job;
  FSMenu *fsmenu_job = fsmenu_copy(g_fsmenu);

  /* setup job */
  wm_job = WM_jobs_get(
      wm, wm->winactive, wm, "Validating Bookmarks...", 0, WM_JOB_TYPE_FSMENU_BOOKMARK_VALIDATE);
  WM_jobs_customdata_set(wm_job, fsmenu_job, fsmenu_bookmark_validate_job_free);
  WM_jobs_timer(wm_job, 0.01, NC_SPACE | ND_SPACE_FILE_LIST, NC_SPACE | ND_SPACE_FILE_LIST);
  WM_jobs_callbacks(wm_job,
                    fsmenu_bookmark_validate_job_startjob,
                    NULL,
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
