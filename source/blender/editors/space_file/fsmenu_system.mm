/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spfile
 */

#ifdef __APPLE__
#  ifdef WITH_APPLE_CROSSPLATFORM
#    import <Foundation/Foundation.h>
#  else
#    include <Carbon/Carbon.h>
#  endif
#endif /* __APPLE__ */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_fileops.h"
#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "BKE_appdir.hh"
#include "BKE_blender_version.h"

#include "ED_fileselect.hh"

#ifdef WIN32
/* Need to include windows.h so _WIN32_IE is defined. */
#  include <windows.h>
/* For SHGetSpecialFolderPath, has to be done before BLI_winstuff
 * because 'near' is disabled through BLI_windstuff. */
#  include "BLI_winstuff.h"
#  include <shlobj.h>
#  include <shlwapi.h>
#endif

#include "UI_interface_icons.hh"
#include "UI_resources.hh"
#include "WM_api.hh"
#include "WM_types.hh"

#ifdef __linux__
#  include "BLI_fileops_types.h"
#  include <mntent.h>
#endif

#include "fsmenu.h" /* include ourselves */

/* FSMENU HANDLING */

struct FSMenu;

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
      BLI_path_join(filepath, sizeof(filepath), xdg_config_home, "user-dirs.dirs");
    }
    else {
      BLI_path_join(filepath, sizeof(filepath), home, ".config", "user-dirs.dirs");
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
  if (xdg_map != NULL) {
    BLI_ghash_free(xdg_map, MEM_freeN, MEM_freeN);
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
                                    struct FSMenu *fsmenu,
                                    const char *key,
                                    const char *default_path,
                                    int icon,
                                    const char *home)
{
  char xdg_path_buf[FILE_MAXDIR];
  const char *xdg_path = xdg_map ? (const char *)BLI_ghash_lookup(xdg_map, key) : NULL;
  if (xdg_path == NULL) {
    BLI_path_join(xdg_path_buf, sizeof(xdg_path_buf), home, default_path);
    xdg_path = xdg_path_buf;
  }
  fsmenu_insert_entry(
      fsmenu, FS_CATEGORY_SYSTEM_BOOKMARKS, xdg_path, N_(default_path), icon, FS_INSERT_LAST);
}

/** \} */

void fsmenu_read_system(struct FSMenu *fsmenu, int read_bookmarks)
{

  char line[FILE_MAXDIR];
#if defined(__APPLE__)

  {
#  ifndef WITH_APPLE_CROSSPLATFORM
    /* We store some known macOS system paths and corresponding icons
     * and names in the FS_CATEGORY_OTHER (not displayed directly) category. */
    fsmenu_insert_entry(
        fsmenu, FS_CATEGORY_OTHER, "/Library/Fonts/", N_("Fonts"), ICON_FILE_FONT, FS_INSERT_LAST);
    fsmenu_insert_entry(fsmenu,
                        FS_CATEGORY_OTHER,
                        "/Applications/",
                        N_("Applications"),
                        ICON_FILE_FOLDER,
                        FS_INSERT_LAST);

    const char *home = BLI_getenv("HOME");
    if (home) {
#    define FS_MACOS_PATH(path, name, icon) \
      BLI_snprintf(line, sizeof(line), path, home); \
      fsmenu_insert_entry(fsmenu, FS_CATEGORY_OTHER, line, name, icon, FS_INSERT_LAST);

      FS_MACOS_PATH("%s/", NULL, ICON_HOME)
      FS_MACOS_PATH("%s/Desktop/", N_("Desktop"), ICON_DESKTOP)
      FS_MACOS_PATH("%s/Documents/", N_("Documents"), ICON_DOCUMENTS)
      FS_MACOS_PATH("%s/Downloads/", N_("Downloads"), ICON_IMPORT)
      FS_MACOS_PATH("%s/Movies/", N_("Movies"), ICON_FILE_MOVIE)
      FS_MACOS_PATH("%s/Music/", N_("Music"), ICON_FILE_SOUND)
      FS_MACOS_PATH("%s/Pictures/", N_("Pictures"), ICON_FILE_IMAGE)
      FS_MACOS_PATH("%s/Library/Fonts/", N_("Fonts"), ICON_FILE_FONT)
#    undef FS_MACOS_PATH
    }
#  else

    /* Apple cross-platform - Add single path for Bundle directory. */
    BLI_snprintf(line, sizeof(line), "%s/Assets/", BKE_appdir_program_dir());
    fsmenu_insert_entry(fsmenu,
                        FS_CATEGORY_SYSTEM_BOOKMARKS,
                        line,
                        N_("Bundle Assets"),
                        ICON_DOCUMENTS,
                        FS_INSERT_LAST);
    UNUSED_VARS(read_bookmarks);
#  endif

    /* Get mounted volumes better method OSX 10.6 and higher, see:
     * https://developer.apple.com/library/mac/#documentation/CoreFoundation/Reference/CFURLRef/Reference/reference.html
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
  }
#endif

#if defined(__APPLE__)
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
  FS_UDIR_PATH(U.sounddir, ICON_FILE_SOUND)
  FS_UDIR_PATH(U.tempdir, ICON_TEMP)

#undef FS_UDIR_PATH
}
