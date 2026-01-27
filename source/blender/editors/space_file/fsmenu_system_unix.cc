/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spfile
 * \brief Generic Unix System File menu implementation.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_fileops.h"
#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_userdef_types.h"

#include "BLT_translation.hh"

#include "ED_fileselect.hh"

#include "UI_resources.hh"

#ifdef __linux__
#  include "BLI_fileops_types.h"
#  include <mntent.h>

#  include "CLG_log.h"

static CLG_LogRef LOG = {"system.path"};
#endif

#include "fsmenu.hh"

namespace blender {

struct FSMenu;

/* -------------------------------------------------------------------- */
/** \name XDG User Directory Support (Unix)
 *
 * Generic Unix, Use XDG when available, otherwise fall back to the home directory.
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
    if (xdg_config_home != nullptr) {
      BLI_path_join(filepath, sizeof(filepath), xdg_config_home, "user-dirs.dirs");
    }
    else {
      BLI_path_join(filepath, sizeof(filepath), home, ".config", "user-dirs.dirs");
    }
    fp = BLI_fopen(filepath, "r");
    if (!fp) {
      return nullptr;
    }
  }
  /* By default there are 8 paths. */
  GHash *xdg_map = BLI_ghash_str_new_ex(__func__, 8);
  while (fgets(l, sizeof(l), fp) != nullptr) { /* read a line */

    /* Avoid inserting invalid values. */
    if (STRPREFIX(l, "XDG_")) {
      char *l_value = strchr(l, '=');
      if (l_value != nullptr) {
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
            BLI_path_join(l_value_expanded, sizeof(l_value_expanded), home, l_value + 6);
            l_value_final = l_value_expanded;
          }

          BLI_ghash_insert(xdg_map, BLI_strdup(l), BLI_strdup(l_value_final));
        }
      }
    }
  }
  fclose(fp);

  return xdg_map;
}

static void fsmenu_xdg_user_dirs_free(GHash *xdg_map)
{
  if (xdg_map != nullptr) {
    BLI_ghash_free(xdg_map, MEM_delete_void, MEM_delete_void);
  }
}

/**
 * Add fsmenu entry for system folders on linux.
 * - Check if a path is stored in the #GHash generated from `user-dirs.dirs`.
 * - If not, check for a default path in `$HOME`.
 *
 * \param key: Use `user-dirs.dirs` format "XDG_EXAMPLE_DIR"
 * \param default_path: Directory name to check in $HOME, also used for the menu entry name.
 */
static void fsmenu_xdg_insert_entry(GHash *xdg_map,
                                    FSMenu *fsmenu,
                                    const char *key,
                                    const char *default_path,
                                    int icon,
                                    const char *home)
{
  char xdg_path_buf[FILE_MAXDIR];
  const char *xdg_path = static_cast<const char *>(xdg_map ? BLI_ghash_lookup(xdg_map, key) :
                                                             nullptr);
  if (xdg_path == nullptr) {
    BLI_path_join(xdg_path_buf, sizeof(xdg_path_buf), home, default_path);
    xdg_path = xdg_path_buf;
  }
  fsmenu_insert_entry(
      fsmenu, FS_CATEGORY_SYSTEM_BOOKMARKS, xdg_path, default_path, icon, FS_INSERT_LAST);
}

/** \} */

void fsmenu_read_system(FSMenu *fsmenu, int read_bookmarks)
{
  const char *home = BLI_dir_home();

  if (read_bookmarks && home) {
    fsmenu_insert_entry(
        fsmenu, FS_CATEGORY_SYSTEM_BOOKMARKS, home, N_("Home"), ICON_HOME, FS_INSERT_LAST);

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
    bool found = false;
#ifdef __linux__
    /* loop over mount points */
    mntent *mnt;
    FILE *fp;

    fp = setmntent(MOUNTED, "r");
    if (fp == nullptr) {
      CLOG_WARN(&LOG, "Could not get a list of mounted file-systems");
    }
    else {

      /* Similar to `STRPREFIX`,
       * but ensures the prefix precedes a directory separator or null terminator.
       * Define locally since it's fairly specific to this particular use case. */
      auto strncmp_dir_delimit = [](const char *a, const char *b, size_t b_len) -> int {
        const int result = strncmp(a, b, b_len);
        return (result == 0 && !ELEM(a[b_len], '\0', '/')) ? 1 : result;
      };
#  define STRPREFIX_DIR_DELIMIT(a, b) (strncmp_dir_delimit((a), (b), strlen(b)) == 0)

      while ((mnt = getmntent(fp))) {
        if (STRPREFIX_DIR_DELIMIT(mnt->mnt_dir, "/boot") ||
            /* According to: https://wiki.archlinux.org/title/EFI_system_partition (2025),
             * this is a common path to mount the EFI partition. */
            STRPREFIX_DIR_DELIMIT(mnt->mnt_dir, "/efi"))
        {
          /* Hide share not usable to the user. */
          continue;
        }
        if (!STRPREFIX_DIR_DELIMIT(mnt->mnt_fsname, "/dev")) {
          continue;
        }
        /* Use non-delimited prefix since a slash isn't expected after loop. */
        if (STRPREFIX(mnt->mnt_fsname, "/dev/loop")) {
          /* The `/dev/loop*` entries are SNAPS used by desktop environment
           * (GNOME) no need for them to show up in the list. */
          continue;
        }

        fsmenu_insert_entry(
            fsmenu, FS_CATEGORY_SYSTEM, mnt->mnt_dir, nullptr, ICON_DISK_DRIVE, FS_INSERT_SORTED);

        found = true;
      }
#  undef STRPREFIX_DIR_DELIMIT

      if (endmntent(fp) == 0) {
        CLOG_WARN(&LOG, "Could not close the list of mounted file-systems");
      }
    }
    /* Check `gvfs` shares. */
    const char *const xdg_runtime_dir = BLI_getenv("XDG_RUNTIME_DIR");
    if (xdg_runtime_dir != nullptr) {
      direntry *dirs;
      char filepath[FILE_MAX];
      BLI_path_join(filepath, sizeof(filepath), xdg_runtime_dir, "gvfs/");
      /* Avoid error message if the directory doesn't exist as this isn't a requirement. */
      if (BLI_is_dir(filepath)) {
        const uint dirs_num = BLI_filelist_dir_contents(filepath, &dirs);
        for (uint i = 0; i < dirs_num; i++) {
          if ((dirs[i].type & S_IFDIR) == 0) {
            continue;
          }
          const char *dirname = dirs[i].relname;
          if (dirname[0] == '.') {
            continue;
          }

          /* Directory names contain a lot of unwanted text.
           * Assuming every entry ends with the share name. */
          const char *label = strstr(dirname, "share=");
          if (label != nullptr) {
            /* Move pointer so `share=` is trimmed off or use full `dirname` as label. */
            const char *label_test = label + 6;
            label = *label_test ? label_test : dirname;
          }
          char line[FILE_MAXDIR];
          SNPRINTF(line, "%s%s", filepath, dirname);
          fsmenu_insert_entry(
              fsmenu, FS_CATEGORY_SYSTEM, line, label, ICON_NETWORK_DRIVE, FS_INSERT_SORTED);
          found = true;
        }
        BLI_filelist_free(dirs, dirs_num);
      }
    }
#endif /* __linux__ */

    /* fallback */
    if (!found) {
      fsmenu_insert_entry(
          fsmenu, FS_CATEGORY_SYSTEM, "/", nullptr, ICON_DISK_DRIVE, FS_INSERT_SORTED);
    }
  }
}

}  // namespace blender
