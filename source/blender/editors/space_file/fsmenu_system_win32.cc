/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spfile
 * \brief Windows / Win32 System File menu implementation
 */

/* Need to include windows.h so _WIN32_IE is defined. */
#include <windows.h>
/* For SHGetSpecialFolderPath, has to be done before BLI_winstuff
 * because 'near' is disabled through BLI_windstuff. */
#include "BLI_winstuff.h"
#include <comdef.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <wrl.h>

#include "utfconv.hh"

#include "MEM_guardedalloc.h"

#include "BLI_path_utils.hh"
#include "BLI_string.h"

#include "BLT_translation.hh"

#include "UI_resources.hh"

#include "fsmenu.hh"

namespace blender {

struct FSMenu;

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

static int fsmenu_external_drive_icon(char drive_letter)
{
  bool is_removable = false; /* ZIP, JAZ, CDROM, MO, etc. instead of a HDD. */
  bool is_hotplug = false;   /* 1394, USB, etc. */
  bool is_usb = false;       /* USB bus. */
  char volumeName[8] = "";
  SNPRINTF(volumeName, "\\\\.\\%c:", drive_letter);
  HANDLE volume = ::CreateFile(volumeName, 0, 0, NULL, OPEN_EXISTING, 0, NULL);

  if (volume != INVALID_HANDLE_VALUE) {
    STORAGE_HOTPLUG_INFO Info = {0};
    DWORD bytesReturned = 0;
    if (::DeviceIoControl(volume,
                          IOCTL_STORAGE_GET_HOTPLUG_INFO,
                          0,
                          0,
                          &Info,
                          sizeof(Info),
                          &bytesReturned,
                          NULL))
    {
      is_removable = Info.MediaRemovable != 0;
      is_hotplug = Info.DeviceHotplug != 0;
    }

    STORAGE_PROPERTY_QUERY Prop;
    Prop.PropertyId = StorageDeviceProperty;
    Prop.QueryType = PropertyStandardQuery;
    Prop.AdditionalParameters[0] = 0;
    STORAGE_DEVICE_DESCRIPTOR DevInfo = {0};
    if (::DeviceIoControl(volume,
                          IOCTL_STORAGE_QUERY_PROPERTY,
                          &Prop,
                          sizeof(Prop),
                          &DevInfo,
                          sizeof(DevInfo),
                          &bytesReturned,
                          NULL))
    {
      is_usb = (DevInfo.BusType == BusTypeUsb);
    }
    ::CloseHandle(volume);
  }

  return (is_removable && is_hotplug && is_usb) ? ICON_USB_DRIVE : ICON_EXTERNAL_DRIVE;
}

void fsmenu_read_system(FSMenu *fsmenu, int read_bookmarks)
{
  char line[FILE_MAXDIR];
  /* Add the drive names to the listing */
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
          icon = fsmenu_external_drive_icon('A' + i);
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
    fsmenu_add_windows_folder(
        fsmenu, FS_CATEGORY_OTHER, FOLDERID_UserProfiles, nullptr, ICON_COMMUNITY, FS_INSERT_LAST);

    /* Last add Quick Access items to avoid duplicates and use icons if available. */
    fsmenu_add_windows_quick_access(fsmenu, FS_CATEGORY_SYSTEM_BOOKMARKS, FS_INSERT_LAST);
  }
}

}  // namespace blender
