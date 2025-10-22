/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spfile
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

#ifdef WIN32
#  include "utfconv.hh"

/* Need to include windows.h so _WIN32_IE is defined. */
#  include <windows.h>
/* For SHGetSpecialFolderPath, has to be done before BLI_winstuff
 * because 'near' is disabled through BLI_windstuff. */
#  include "BLI_winstuff.h"
#  include <comdef.h>
#  include <comutil.h>
#  include <shlobj.h>
#  include <shlwapi.h>
#  include <wrl.h>
#endif

#include "UI_resources.hh"

#ifdef __APPLE__
#  include <Carbon/Carbon.h>
#endif /* __APPLE__ */

#ifdef __linux__
#  include "BLI_fileops_types.h"
#  include <mntent.h>
#endif

#include "fsmenu.h"

#ifdef __linux__
#  include "CLG_log.h"
static CLG_LogRef LOG = {"system.path"};
#endif

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
                                    FSMenu *fsmenu,
                                    const char *key,
                                    const char *default_path,
                                    int icon,
                                    const char *home)
{
  char xdg_path_buf[FILE_MAXDIR];
  const char *xdg_path = (const char *)(xdg_map ? BLI_ghash_lookup(xdg_map, key) : nullptr);
  if (xdg_path == nullptr) {
    BLI_path_join(xdg_path_buf, sizeof(xdg_path_buf), home, default_path);
    xdg_path = xdg_path_buf;
  }
  fsmenu_insert_entry(
      fsmenu, FS_CATEGORY_SYSTEM_BOOKMARKS, xdg_path, default_path, icon, FS_INSERT_LAST);
}

/** \} */

#ifdef WIN32
/* Add Windows Quick Access items to the System list. */
static void fsmenu_add_windows_quick_access(FSMenu *fsmenu,
                                            FSMenuCategory category,
                                            FSMenuInsert flag)
{
  Microsoft::WRL::ComPtr<IShellDispatch> shell;
  if (CoCreateInstance(CLSID_Shell, nullptr, CLSCTX_ALL, IID_PPV_ARGS(shell.GetAddressOf())) !=
      S_OK)
  {
    return;
  }

  /* Open Quick Access folder. */
  Microsoft::WRL::ComPtr<Folder> dir;
  if (shell->NameSpace(_variant_t(L"shell:::{679f85cb-0220-4080-b29b-5540cc05aab6}"),
                       dir.GetAddressOf()) != S_OK)
  {
    return;
  }

  /* Get FolderItems. */
  Microsoft::WRL::ComPtr<FolderItems> items;
  if (dir->Items(items.GetAddressOf()) != S_OK) {
    return;
  }

  long count = 0;
  if (items->get_Count(&count) != S_OK) {
    return;
  }

  /* Iterate through the folder. */
  for (long i = 0; i < count; i++) {
    Microsoft::WRL::ComPtr<FolderItem> item;

    if (items->Item(_variant_t(i), item.GetAddressOf()) != S_OK) {
      continue;
    }

    VARIANT_BOOL isFolder;
    /* Skip if it's not a folder. */
    if (item->get_IsFolder(&isFolder) != S_OK || isFolder == VARIANT_FALSE) {
      continue;
    }

    _bstr_t path;
    if (item->get_Path(path.GetAddress()) != S_OK) {
      continue;
    }

    char utf_path[FILE_MAXDIR];
    conv_utf_16_to_8(path, utf_path, FILE_MAXDIR);

    /* Despite the above IsFolder check, Windows considers libraries and archives to be folders.
     * However, as Blender does not support opening them, they must be filtered out. #138863. */
    const char *ext_folderlike[] = {
        ".library-ms",
        ".zip",
        ".rar",
        ".7z",
        ".tar",
        ".gz",
        ".bz2",
        ".zst",
        ".xz",
        ".cab",
        ".iso",
        nullptr,
    };
    if (!BLI_path_extension_check_array(utf_path, ext_folderlike)) {
      /* Add folder to the fsmenu. */
      fsmenu_insert_entry(fsmenu, category, utf_path, NULL, ICON_FILE_FOLDER, flag);
    }
  }
}

/* Add a Windows known folder path to the System list. */
static void fsmenu_add_windows_folder(FSMenu *fsmenu,
                                      FSMenuCategory category,
                                      REFKNOWNFOLDERID rfid,
                                      const char *name,
                                      const int icon,
                                      FSMenuInsert flag)
{
  LPWSTR pPath;
  char line[FILE_MAXDIR];
  if (SHGetKnownFolderPath(rfid, 0, nullptr, &pPath) == S_OK) {
    conv_utf_16_to_8(pPath, line, FILE_MAXDIR);
    fsmenu_insert_entry(fsmenu, category, line, name, icon, flag);
  }
  CoTaskMemFree(pPath);
}
#endif

void fsmenu_read_system(FSMenu *fsmenu, int read_bookmarks)
{
  char line[FILE_MAXDIR];
#ifdef WIN32
  /* Add the drive names to the listing */
  {
    wchar_t wline[FILE_MAXDIR];
    __int64 tmp;
    char tmps[4], *name;

    tmp = GetLogicalDrives();

    for (int i = 0; i < 26; i++) {
      if ((tmp >> i) & 1) {
        tmps[0] = 'A' + i;
        tmps[1] = ':';
        tmps[2] = '\\';
        tmps[3] = '\0';
        name = nullptr;

        /* Skip over floppy disks A & B. */
        if (i > 1) {
          /* Friendly volume descriptions without using SHGetFileInfoW (#85689). */
          conv_utf_8_to_16(tmps, wline, 4);
          IShellFolder *desktop;
          if (SHGetDesktopFolder(&desktop) == S_OK) {
            PIDLIST_RELATIVE volume;
            if (desktop->ParseDisplayName(nullptr, nullptr, wline, nullptr, &volume, nullptr) ==
                S_OK)
            {
              STRRET volume_name;
              volume_name.uType = STRRET_WSTR;
              if (desktop->GetDisplayNameOf(volume, SHGDN_FORADDRESSBAR, &volume_name) == S_OK) {
                wchar_t *volume_name_wchar;
                if (StrRetToStrW(&volume_name, volume, &volume_name_wchar) == S_OK) {
                  conv_utf_16_to_8(volume_name_wchar, line, FILE_MAXDIR);
                  name = line;
                  CoTaskMemFree(volume_name_wchar);
                }
              }
              CoTaskMemFree(volume);
            }
            desktop->Release();
          }
        }
        if (name == nullptr) {
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

        fsmenu_insert_entry(
            fsmenu, FS_CATEGORY_SYSTEM, tmps, name, icon, FSMenuInsert(FS_INSERT_SORTED));
      }
    }

    /* Get Special Folder Locations. */
    if (read_bookmarks) {

      /* These items are shown in System List. */
      fsmenu_add_windows_folder(fsmenu,
                                FS_CATEGORY_SYSTEM_BOOKMARKS,
                                FOLDERID_Profile,
                                N_("Home"),
                                ICON_HOME,
                                FS_INSERT_LAST);
      fsmenu_add_windows_folder(fsmenu,
                                FS_CATEGORY_SYSTEM_BOOKMARKS,
                                FOLDERID_Desktop,
                                N_("Desktop"),
                                ICON_DESKTOP,
                                FS_INSERT_LAST);
      fsmenu_add_windows_folder(fsmenu,
                                FS_CATEGORY_SYSTEM_BOOKMARKS,
                                FOLDERID_Documents,
                                N_("Documents"),
                                ICON_DOCUMENTS,
                                FS_INSERT_LAST);
      fsmenu_add_windows_folder(fsmenu,
                                FS_CATEGORY_SYSTEM_BOOKMARKS,
                                FOLDERID_Downloads,
                                N_("Downloads"),
                                ICON_IMPORT,
                                FS_INSERT_LAST);
      fsmenu_add_windows_folder(fsmenu,
                                FS_CATEGORY_SYSTEM_BOOKMARKS,
                                FOLDERID_Music,
                                N_("Music"),
                                ICON_FILE_SOUND,
                                FS_INSERT_LAST);
      fsmenu_add_windows_folder(fsmenu,
                                FS_CATEGORY_SYSTEM_BOOKMARKS,
                                FOLDERID_Pictures,
                                N_("Pictures"),
                                ICON_FILE_IMAGE,
                                FS_INSERT_LAST);
      fsmenu_add_windows_folder(fsmenu,
                                FS_CATEGORY_SYSTEM_BOOKMARKS,
                                FOLDERID_Videos,
                                N_("Videos"),
                                ICON_FILE_MOVIE,
                                FS_INSERT_LAST);
      fsmenu_add_windows_folder(fsmenu,
                                FS_CATEGORY_SYSTEM_BOOKMARKS,
                                FOLDERID_Fonts,
                                N_("Fonts"),
                                ICON_FILE_FONT,
                                FS_INSERT_LAST);
      fsmenu_add_windows_folder(fsmenu,
                                FS_CATEGORY_SYSTEM_BOOKMARKS,
                                FOLDERID_SkyDrive,
                                N_("OneDrive"),
                                ICON_INTERNET,
                                FS_INSERT_LAST);

      /* These items are just put in path cache for thumbnail views and if bookmarked. */
      fsmenu_add_windows_folder(fsmenu,
                                FS_CATEGORY_OTHER,
                                FOLDERID_UserProfiles,
                                nullptr,
                                ICON_COMMUNITY,
                                FS_INSERT_LAST);

      /* Last add Quick Access items to avoid duplicates and use icons if available. */
      fsmenu_add_windows_quick_access(fsmenu, FS_CATEGORY_SYSTEM_BOOKMARKS, FS_INSERT_LAST);
    }
  }
#elif defined(__APPLE__)
  {
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

    const char *home = BLI_dir_home();
    if (home) {
#  define FS_MACOS_PATH(path, name, icon) \
\
    SNPRINTF(line, path, home); \
\
    fsmenu_insert_entry(fsmenu, FS_CATEGORY_OTHER, line, name, icon, FS_INSERT_LAST);

      FS_MACOS_PATH("%s/", nullptr, ICON_HOME)
      FS_MACOS_PATH("%s/Desktop/", N_("Desktop"), ICON_DESKTOP)
      FS_MACOS_PATH("%s/Documents/", N_("Documents"), ICON_DOCUMENTS)
      FS_MACOS_PATH("%s/Downloads/", N_("Downloads"), ICON_IMPORT)
      FS_MACOS_PATH("%s/Movies/", N_("Movies"), ICON_FILE_MOVIE)
      FS_MACOS_PATH("%s/Music/", N_("Music"), ICON_FILE_SOUND)
      FS_MACOS_PATH("%s/Pictures/", N_("Pictures"), ICON_FILE_IMAGE)
      FS_MACOS_PATH("%s/Library/Fonts/", N_("Fonts"), ICON_FILE_FONT)

#  undef FS_MACOS_PATH
    }

    /* Get mounted volumes better method OSX 10.6 and higher, see:
     * https://developer.apple.com/library/mac/#documentation/CoreFoundation/Reference/CFURLRef/Reference/reference.html
     */

    /* We get all volumes sorted including network and do not relay
     * on user-defined finder visibility, less confusing. */

    CFURLRef cfURL = nullptr;
    CFURLEnumeratorResult result = kCFURLEnumeratorSuccess;
    CFURLEnumeratorRef volEnum = CFURLEnumeratorCreateForMountedVolumes(
        nullptr, kCFURLEnumeratorSkipInvisibles, nullptr);

    while (result != kCFURLEnumeratorEnd) {
      char defPath[FILE_MAX];

      result = CFURLEnumeratorGetNextURL(volEnum, &cfURL, nullptr);
      if (result != kCFURLEnumeratorSuccess) {
        continue;
      }

      CFURLGetFileSystemRepresentation(cfURL, false, (UInt8 *)defPath, FILE_MAX);

      /* Get name of the volume. */
      char display_name[FILE_MAXFILE] = "";
      CFStringRef nameString = nullptr;
      CFURLCopyResourcePropertyForKey(cfURL, kCFURLVolumeLocalizedNameKey, &nameString, nullptr);
      if (nameString != nullptr) {
        CFStringGetCString(nameString, display_name, sizeof(display_name), kCFStringEncodingUTF8);
        CFRelease(nameString);
      }

      /* Set icon for regular, removable or network drive. */
      int icon = ICON_DISK_DRIVE;
      CFBooleanRef localKey = nullptr;
      CFURLCopyResourcePropertyForKey(cfURL, kCFURLVolumeIsLocalKey, &localKey, nullptr);
      if (localKey != nullptr) {
        if (!CFBooleanGetValue(localKey)) {
          icon = ICON_NETWORK_DRIVE;
        }
        else {
          CFBooleanRef ejectableKey = nullptr;
          CFURLCopyResourcePropertyForKey(
              cfURL, kCFURLVolumeIsEjectableKey, &ejectableKey, nullptr);
          if (ejectableKey != nullptr) {
            if (CFBooleanGetValue(ejectableKey)) {
              icon = ICON_EXTERNAL_DRIVE;
            }
            CFRelease(ejectableKey);
          }
        }
        CFRelease(localKey);
      }

      fsmenu_insert_entry(fsmenu,
                          FS_CATEGORY_SYSTEM,
                          defPath,
                          display_name[0] ? display_name : nullptr,
                          icon,
                          FS_INSERT_SORTED);
    }

    CFRelease(volEnum);

/* kLSSharedFileListFavoriteItems is deprecated, but available till macOS 10.15.
 * Will have to find a new method to sync the Finder Favorites with File Browser. */
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    /* Finally get user favorite places */
    if (read_bookmarks) {
      UInt32 seed;
      LSSharedFileListRef list = LSSharedFileListCreate(
          nullptr, kLSSharedFileListFavoriteItems, nullptr);
      CFArrayRef pathesArray = LSSharedFileListCopySnapshot(list, &seed);
      CFIndex pathesCount = CFArrayGetCount(pathesArray);

      for (CFIndex i = 0; i < pathesCount; i++) {
        LSSharedFileListItemRef itemRef = (LSSharedFileListItemRef)CFArrayGetValueAtIndex(
            pathesArray, i);

        CFURLRef cfURL = nullptr;
        OSErr err = LSSharedFileListItemResolve(itemRef,
                                                kLSSharedFileListNoUserInteraction |
                                                    kLSSharedFileListDoNotMountVolumes,
                                                &cfURL,
                                                nullptr);
        if (err != noErr || !cfURL) {
          continue;
        }

        CFStringRef pathString = CFURLCopyFileSystemPath(cfURL, kCFURLPOSIXPathStyle);

        if (pathString == nullptr ||
            !CFStringGetCString(pathString, line, sizeof(line), kCFStringEncodingUTF8))
        {
          continue;
        }

        /* Exclude "all my files" as it makes no sense in blender file-selector. */
        /* Exclude "airdrop" if WLAN not active as it would show "". */
        if (!strstr(line, "myDocuments.cannedSearch") && (*line != '\0')) {
          fsmenu_insert_entry(fsmenu,
                              FS_CATEGORY_SYSTEM_BOOKMARKS,
                              line,
                              nullptr,
                              ICON_FILE_FOLDER,
                              FS_INSERT_LAST);
        }

        CFRelease(pathString);
        CFRelease(cfURL);
      }

      CFRelease(pathesArray);
      CFRelease(list);
    }
#  pragma GCC diagnostic pop
  }
#else /* `!defined(WIN32) && !defined(__APPLE__)` */
  /* Generic Unix. */
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
#  ifdef __linux__
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
#    define STRPREFIX_DIR_DELIMIT(a, b) (strncmp_dir_delimit((a), (b), strlen(b)) == 0)

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

          fsmenu_insert_entry(fsmenu,
                              FS_CATEGORY_SYSTEM,
                              mnt->mnt_dir,
                              nullptr,
                              ICON_DISK_DRIVE,
                              FS_INSERT_SORTED);

          found = true;
        }
#    undef STRPREFIX_DIR_DELIMIT

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
            SNPRINTF(line, "%s%s", filepath, dirname);
            fsmenu_insert_entry(
                fsmenu, FS_CATEGORY_SYSTEM, line, label, ICON_NETWORK_DRIVE, FS_INSERT_SORTED);
            found = true;
          }
          BLI_filelist_free(dirs, dirs_num);
        }
      }
#  endif /* __linux__ */

      /* fallback */
      if (!found) {
        fsmenu_insert_entry(
            fsmenu, FS_CATEGORY_SYSTEM, "/", nullptr, ICON_DISK_DRIVE, FS_INSERT_SORTED);
      }
    }
  }
#endif

#if defined(__APPLE__)
  /* Quiet warnings. */
  UNUSED_VARS(fsmenu_xdg_insert_entry, fsmenu_xdg_user_dirs_parse, fsmenu_xdg_user_dirs_free);
#endif

/* For all platforms, we add some directories from User Preferences to
 * the FS_CATEGORY_OTHER category so that these directories
 * have the appropriate icons when they are added to the Bookmarks.
 *
 * NOTE: of the preferences support as `//` prefix.
 * Skip them since they depend on the current loaded blend file. */
#define FS_UDIR_PATH(dir, icon) \
  if (dir[0] && !BLI_path_is_rel(dir)) { \
    fsmenu_insert_entry(fsmenu, FS_CATEGORY_OTHER, dir, nullptr, icon, FS_INSERT_LAST); \
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
