/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef WIN32
#  include <Windows.h>

#  include <KnownFolders.h>
#  include <filesystem>
#  include <propkey.h>
#  include <propvarutil.h>
#  include <shlobj_core.h>
#  include <wrl.h>

#  include "BLI_path_util.h"
#  include "BLI_winstuff.h"
#  include "BLI_winstuff_com.hh"

#  include "utf_winfunc.hh"
#  include "utfconv.hh"

/**
 * Pinning: Windows allows people to pin an application to their taskbar, when a user pins
 * blender, the data we set in `GHOST_WindowWin32::registerWindowAppUserModelProperties` is used
 * which includes the path to the `blender-launcher.exe`. Now once that shortcut is created on
 * the taskbar, this will never be updated, if people remove blender and install it again to a
 * different path (happens often when using nightly builds) this leads to the situation where the
 * shortcut on the taskbar points to a no longer existing blender installation. Now you may think,
 * just un-pin and re-pin that should clear that right up! It doesn't, it'll keep using the
 * outdated path till the end of time and there's no window API call we can do to update this
 * information. However this shortcut is stored in the user profile in a sub-folder we can easily
 * query, from there, we can iterate over all files, look for the one that has our APP-ID in it,
 * and when we find it, update the path to the blender launcher to the current installation, bit
 * of a hack, but Microsoft seemingly offers no other way to deal with this problem.
 *
 * this function returns true when it had no issues executing, it is NOT indicative of any changes
 * or updates being made
 */
bool BLI_windows_update_pinned_launcher(const char *launcher_path)
{
  WCHAR launcher_path_w[FILE_MAX];

  if (conv_utf_8_to_16(launcher_path, launcher_path_w, ARRAY_SIZE(launcher_path_w)) != 0) {
    return false;
  }

  blender::CoInitializeWrapper initialize(COINIT_APARTMENTTHREADED);
  if (FAILED(initialize)) {
    return false;
  }

  LPWSTR quick_launch_folder_path;
  if (SHGetKnownFolderPath(
          FOLDERID_ImplicitAppShortcuts, KF_FLAG_DEFAULT, NULL, &quick_launch_folder_path) != S_OK)
  {
    return false;
  }

  std::wstring search_path = quick_launch_folder_path;
  CoTaskMemFree(quick_launch_folder_path);

  for (auto const &dir_entry : std::filesystem::recursive_directory_iterator(search_path)) {

    Microsoft::WRL::ComPtr<IShellLinkW> shell_link;
    if (CoCreateInstance(__uuidof(ShellLink), NULL, CLSCTX_ALL, IID_PPV_ARGS(&shell_link)) != S_OK)
    {
      return false;
    }

    Microsoft::WRL::ComPtr<IPersistFile> persist_file;
    if (shell_link.As(&persist_file) != S_OK) {
      return false;
    }

    if (persist_file->Load(dir_entry.path().c_str(), STGM_READWRITE) != S_OK) {
      continue;
    }

    Microsoft::WRL::ComPtr<IPropertyStore> property_store;
    if (shell_link.As(&property_store) != S_OK) {
      continue;
    }

    UTF16_ENCODE(BLENDER_WIN_APPID);
    PROPVARIANT app_model;
    PropVariantInit(&app_model);
    if (property_store->GetValue(PKEY_AppUserModel_ID, &app_model) == S_OK) {
      if (app_model.vt == VT_LPWSTR && std::wstring(BLENDER_WIN_APPID_16) == app_model.pwszVal) {
        shell_link->SetPath(launcher_path_w);
        persist_file->Save(NULL, TRUE);
      }
    }
    PropVariantClear(&app_model);
    UTF16_UN_ENCODE(BLENDER_WIN_APPID);
  }
  return true;
}
#endif
