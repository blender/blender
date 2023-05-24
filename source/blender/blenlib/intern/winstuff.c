/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup bli
 * WIN32-POSIX compatibility layer, MS-Windows-specific functions.
 */

#ifdef WIN32

#  include <conio.h>
#  include <shlwapi.h>
#  include <stdio.h>
#  include <stdlib.h>

#  include "MEM_guardedalloc.h"

#  define WIN32_SKIP_HKEY_PROTECTION /* Need to use HKEY. */
#  include "BLI_fileops.h"
#  include "BLI_path_util.h"
#  include "BLI_string.h"
#  include "BLI_utildefines.h"
#  include "BLI_winstuff.h"

#  include "utf_winfunc.h"
#  include "utfconv.h"

/* FILE_MAXDIR + FILE_MAXFILE */

int BLI_windows_get_executable_dir(char *str)
{
  char dir[FILE_MAXDIR];
  int a;
  /* Change to utf support. */
  GetModuleFileName(NULL, str, FILE_MAX);
  BLI_path_split_dir_part(str, dir, sizeof(dir)); /* shouldn't be relative */
  a = strlen(dir);
  if (dir[a - 1] == '\\') {
    dir[a - 1] = 0;
  }

  strcpy(str, dir);

  return 1;
}

bool BLI_windows_is_store_install(void) {
  char install_dir[FILE_MAXDIR];
  BLI_windows_get_executable_dir(install_dir);
  return (BLI_strcasestr(install_dir, "\\WindowsApps\\") != NULL);
}

static void registry_error(HKEY root, const char *message)
{
  if (root) {
    RegCloseKey(root);
  }
  fprintf(stderr, "%s\n", message);
}

static bool open_registry_hive(bool all_users, HKEY *r_root)
{
  if (RegOpenKeyEx(all_users ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
                   "Software\\Classes",
                   0,
                   KEY_ALL_ACCESS,
                   r_root) != ERROR_SUCCESS)
  {
    registry_error(*r_root, "Unable to open the registry with the required permissions");
    return false;
  }
  return true;
}

static bool register_blender_prog_id(const char *prog_id,
                                     const char *executable,
                                     const char *friendly_name,
                                     bool all_users)
{
  LONG lresult;
  HKEY root = 0;
  HKEY hkey_progid = 0;
  char buffer[256];
  DWORD dwd = 0;

  if (!open_registry_hive(all_users, &root)) {
    return false;
  }

  lresult = RegCreateKeyEx(
      root, prog_id, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hkey_progid, &dwd);

  if (lresult == ERROR_SUCCESS) {
    lresult = RegSetValueEx(
        hkey_progid, NULL, 0, REG_SZ, (BYTE *)friendly_name, strlen(friendly_name) + 1);
  }
  if (lresult == ERROR_SUCCESS) {
    lresult = RegSetValueEx(
        hkey_progid, "AppUserModelId", 0, REG_SZ, (BYTE *)prog_id, strlen(prog_id) + 1);
  }
  if (lresult != ERROR_SUCCESS) {
    registry_error(root, "Unable to register Blender App Id");
    return false;
  }

  SNPRINTF(buffer, "%s\\shell\\open", prog_id);
  lresult = RegCreateKeyEx(
      root, buffer, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hkey_progid, &dwd);

  lresult = RegSetValueEx(
      hkey_progid, "FriendlyAppName", 0, REG_SZ, (BYTE *)friendly_name, strlen(friendly_name) + 1);

  SNPRINTF(buffer, "%s\\shell\\open\\command", prog_id);

  lresult = RegCreateKeyEx(
      root, buffer, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hkey_progid, &dwd);

  if (lresult == ERROR_SUCCESS) {
    SNPRINTF(buffer, "\"%s\" \"%%1\"", executable);
    lresult = RegSetValueEx(hkey_progid, NULL, 0, REG_SZ, (BYTE *)buffer, strlen(buffer) + 1);
    RegCloseKey(hkey_progid);
  }
  if (lresult != ERROR_SUCCESS) {
    registry_error(root, "Unable to register Blender App Id");
    return false;
  }

  SNPRINTF(buffer, "%s\\DefaultIcon", prog_id);
  lresult = RegCreateKeyEx(
      root, buffer, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hkey_progid, &dwd);

  if (lresult == ERROR_SUCCESS) {
    SNPRINTF(buffer, "\"%s\", 1", executable);
    lresult = RegSetValueEx(hkey_progid, NULL, 0, REG_SZ, (BYTE *)buffer, strlen(buffer) + 1);
    RegCloseKey(hkey_progid);
  }
  if (lresult != ERROR_SUCCESS) {
    registry_error(root, "Unable to register Blender App Id");
    return false;
  }
  return true;
}

bool BLI_windows_register_blend_extension(const bool all_users)
{
  if (BLI_windows_is_store_install()) {
    fprintf(stderr, "Registration not possible from Microsoft Store installation.");
    return false;
  }

  HKEY root = 0;
  char blender_path[MAX_PATH];
  char *blender_app;
  HKEY hkey = 0;
  LONG lresult;
  DWORD dwd = 0;
  const char *prog_id = BLENDER_WIN_APPID;
  const char *friendly_name = BLENDER_WIN_APPID_FRIENDLY_NAME;

  GetModuleFileName(0, blender_path, sizeof(blender_path));

  /* Prevent overflow when we add -launcher to the executable name. */
  if (strlen(blender_path) > (sizeof(blender_path) - 10))
    return false;

  /* Replace the actual app name with the wrapper. */
  blender_app = strstr(blender_path, "blender.exe");
  if (!blender_app) {
    return false;
  }
  strcpy(blender_app, "blender-launcher.exe");

  if (!open_registry_hive(all_users, &root)) {
    return false;
  }

  if (!register_blender_prog_id(prog_id, blender_path, friendly_name, all_users)) {
    registry_error(root, "Unable to register Blend document type");
    return false;
  }

  lresult = RegCreateKeyEx(
      root, ".blend", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hkey, &dwd);
  if (lresult == ERROR_SUCCESS) {
    /* Set this instance the default. */
    lresult = RegSetValueEx(hkey, NULL, 0, REG_SZ, (BYTE *)prog_id, strlen(prog_id) + 1);

    if (lresult != ERROR_SUCCESS) {
      registry_error(root, "Unable to register Blend document type");
      RegCloseKey(hkey);
      return false;
    }
    RegCloseKey(hkey);

    lresult = RegCreateKeyEx(root,
                             ".blend\\OpenWithProgids",
                             0,
                             NULL,
                             REG_OPTION_NON_VOLATILE,
                             KEY_ALL_ACCESS,
                             NULL,
                             &hkey,
                             &dwd);

    if (lresult != ERROR_SUCCESS) {
      registry_error(root, "Unable to register Blend document type");
      RegCloseKey(hkey);
      return false;
    }
    lresult = RegSetValueEx(hkey, prog_id, 0, REG_NONE, NULL, 0);
    RegCloseKey(hkey);
  }

  if (lresult != ERROR_SUCCESS) {
    registry_error(root, "Unable to register Blend document type");
    return false;
  }

#  ifdef WITH_BLENDER_THUMBNAILER
  {
    char reg_cmd[MAX_PATH * 2];
    char install_dir[FILE_MAXDIR];
    char system_dir[FILE_MAXDIR];
    BLI_windows_get_executable_dir(install_dir);
    GetSystemDirectory(system_dir, sizeof(system_dir));
    const char *thumbnail_handler = "BlendThumb.dll";
    SNPRINTF(reg_cmd, "%s\\regsvr32 /s \"%s\\%s\"", system_dir, install_dir, thumbnail_handler);
    system(reg_cmd);
  }
#  endif

  RegCloseKey(root);
  char message[256];
  SNPRINTF(message,
           "Blend file extension registered for %s.",
           all_users ? "all users" : "the current user");
  printf("%s\n", message);

  return true;
}

bool BLI_windows_unregister_blend_extension(const bool all_users)
{
  if (BLI_windows_is_store_install()) {
    fprintf(stderr, "Unregistration not possible from Microsoft Store installation.");
    return false;
  }

  HKEY root = 0;
  HKEY hkey = 0;
  LONG lresult;

  if (!open_registry_hive(all_users, &root)) {
    return false;
  }

  /* Don't stop on failure. We want to allow unregister after unregister. */

  RegDeleteTree(root, BLENDER_WIN_APPID);

  lresult = RegOpenKeyEx(root, ".blend", 0, KEY_ALL_ACCESS, &hkey);
  if (lresult == ERROR_SUCCESS) {
    char buffer[256] = {0};
    DWORD size = sizeof(buffer);
    lresult = RegGetValueA(hkey, NULL, NULL, RRF_RT_REG_SZ, NULL, &buffer, &size);
    if (lresult == ERROR_SUCCESS && STREQ(buffer, BLENDER_WIN_APPID)) {
      RegSetValueEx(hkey, NULL, 0, REG_SZ, 0, 0);
    }
  }

#  ifdef WITH_BLENDER_THUMBNAILER
  {
    char reg_cmd[MAX_PATH * 2];
    char install_dir[FILE_MAXDIR];
    char system_dir[FILE_MAXDIR];
    BLI_windows_get_executable_dir(install_dir);
    GetSystemDirectory(system_dir, sizeof(system_dir));
    const char *thumbnail_handler = "BlendThumb.dll";
    SNPRINTF(reg_cmd, "%s\\regsvr32 /u \"%s\\%s\"", system_dir, install_dir, thumbnail_handler);
    system(reg_cmd);
  }
#  endif

  lresult = RegOpenKeyEx(hkey, "OpenWithProgids", 0, KEY_ALL_ACCESS, &hkey);
  if (lresult == ERROR_SUCCESS) {
    RegDeleteValue(hkey, BLENDER_WIN_APPID);
  }

  RegCloseKey(root);
  char message[256];
  SNPRINTF(message,
           "Blend file extension unregistered for %s.",
           all_users ? "all users" : "the current user");
  printf("%s\n", message);

  return true;
}

/**
 * Check the registry to see if there is an operation association to a file
 * extension. Extension *should almost always contain a dot like `.txt`,
 * but this does allow querying non - extensions *like "Directory", "Drive",
 * "AllProtocols", etc - anything in Classes with a "shell" branch.
 */
static bool BLI_windows_file_operation_is_registered(const char *extension, const char *operation)
{
  HKEY hKey;
  HRESULT hr = AssocQueryKey(ASSOCF_INIT_IGNOREUNKNOWN,
                             ASSOCKEY_SHELLEXECCLASS,
                             (LPCTSTR)extension,
                             (LPCTSTR)operation,
                             &hKey);
  if (SUCCEEDED(hr)) {
    RegCloseKey(hKey);
    return true;
  }
  return false;
}

bool BLI_windows_external_operation_supported(const char *filepath, const char *operation)
{
  if (STREQ(operation, "open") || STREQ(operation, "properties")) {
    return true;
  }

  if (BLI_is_dir(filepath)) {
    return BLI_windows_file_operation_is_registered("Directory", operation);
  }

  const char *extension = BLI_path_extension(filepath);
  return BLI_windows_file_operation_is_registered(extension, operation);
}

bool BLI_windows_external_operation_execute(const char *filepath, const char *operation)
{
  WCHAR wpath[FILE_MAX];
  if (conv_utf_8_to_16(filepath, wpath, ARRAY_SIZE(wpath)) != 0) {
    return false;
  }

  WCHAR woperation[FILE_MAX];
  if (conv_utf_8_to_16(operation, woperation, ARRAY_SIZE(woperation)) != 0) {
    return false;
  }

  SHELLEXECUTEINFOW shellinfo = {0};
  shellinfo.cbSize = sizeof(SHELLEXECUTEINFO);
  shellinfo.fMask = SEE_MASK_INVOKEIDLIST;
  shellinfo.lpVerb = woperation;
  shellinfo.lpFile = wpath;
  shellinfo.nShow = SW_SHOW;

  return ShellExecuteExW(&shellinfo);
}

bool BLI_windows_execute_self(const char *parameters,
                              const bool wait,
                              const bool elevated,
                              const bool silent)
{
  char blender_path[MAX_PATH];
  GetModuleFileName(0, blender_path, MAX_PATH);

  SHELLEXECUTEINFOA shellinfo = {0};
  shellinfo.cbSize = sizeof(SHELLEXECUTEINFO);
  shellinfo.fMask = wait ? SEE_MASK_NOCLOSEPROCESS : SEE_MASK_DEFAULT;
  shellinfo.hwnd = NULL;
  shellinfo.lpVerb = elevated ? "runas" : NULL;
  shellinfo.lpFile = blender_path;
  shellinfo.lpParameters = parameters;
  shellinfo.lpDirectory = NULL;
  shellinfo.nShow = silent ? SW_HIDE : SW_SHOW;
  shellinfo.hInstApp = NULL;
  shellinfo.hProcess = 0;

  DWORD exitCode = 0;
  if (!ShellExecuteExA(&shellinfo)) {
    return false;
  }
  if (!wait) {
    return true;
  }

  if (shellinfo.hProcess != 0) {
    WaitForSingleObject(shellinfo.hProcess, INFINITE);
    GetExitCodeProcess(shellinfo.hProcess, &exitCode);
    CloseHandle(shellinfo.hProcess);
    return (exitCode == 0);
  }

  return false;
}

void BLI_windows_get_default_root_dir(char root[4])
{
  char str[MAX_PATH + 1];

  /* the default drive to resolve a directory without a specified drive
   * should be the Windows installation drive, since this was what the OS
   * assumes. */
  if (GetWindowsDirectory(str, MAX_PATH + 1)) {
    root[0] = str[0];
    root[1] = ':';
    root[2] = '\\';
    root[3] = '\0';
  }
  else {
    /* if GetWindowsDirectory fails, something has probably gone wrong,
     * we are trying the blender install dir though */
    if (GetModuleFileName(NULL, str, MAX_PATH + 1)) {
      printf(
          "Error! Could not get the Windows Directory - "
          "Defaulting to Blender installation Dir!\n");
      root[0] = str[0];
      root[1] = ':';
      root[2] = '\\';
      root[3] = '\0';
    }
    else {
      DWORD tmp;
      int i;
      int rc = 0;
      /* now something has gone really wrong - still trying our best guess */
      printf(
          "Error! Could not get the Windows Directory - "
          "Defaulting to first valid drive! Path might be invalid!\n");
      tmp = GetLogicalDrives();
      for (i = 2; i < 26; i++) {
        if ((tmp >> i) & 1) {
          root[0] = 'a' + i;
          root[1] = ':';
          root[2] = '\\';
          root[3] = '\0';
          if (GetFileAttributes(root) != 0xFFFFFFFF) {
            rc = i;
            break;
          }
        }
      }
      if (0 == rc) {
        printf("ERROR in 'BLI_windows_get_default_root_dir': can't find a valid drive!\n");
        root[0] = 'C';
        root[1] = ':';
        root[2] = '\\';
        root[3] = '\0';
      }
    }
  }
}

#else

/* intentionally empty for UNIX */

#endif
