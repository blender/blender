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
 * The Original Code is Copyright (C) 2019 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup wm
 */
#include "wm_platform_support.h"
#include "wm_window_private.h"

#include <string.h>

#include "BLI_dynstr.h"
#include "BLI_fileops.h"
#include "BLI_linklist.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_sys_types.h"

#include "BLT_translation.h"

#include "BKE_appdir.h"
#include "BKE_global.h"

#include "GPU_platform.h"

#include "GHOST_C-api.h"

#define WM_PLATFORM_SUPPORT_TEXT_SIZE 1024

/**
 * Check if user has already approved the given `platform_support_key`.
 */
static bool wm_platform_support_check_approval(const char *platform_support_key, bool update)
{
  const char *const cfgdir = BKE_appdir_folder_id(BLENDER_USER_CONFIG, NULL);
  bool result = false;

  if (G.factory_startup) {
    return result;
  }

  if (cfgdir) {
    char filepath[FILE_MAX];
    BLI_join_dirfile(filepath, sizeof(filepath), cfgdir, BLENDER_PLATFORM_SUPPORT_FILE);
    LinkNode *lines = BLI_file_read_as_lines(filepath);
    for (LinkNode *line_node = lines; line_node; line_node = line_node->next) {
      char *line = line_node->link;
      if (STREQ(line, platform_support_key)) {
        result = true;
        break;
      }
    }

    if (!result && update) {
      FILE *fp = BLI_fopen(filepath, "a");
      if (fp) {
        fprintf(fp, "%s\n", platform_support_key);
        fclose(fp);
      }
    }

    BLI_file_free_lines(lines);
  }
  return result;
}

static void wm_platform_support_create_link(char *link)
{
  DynStr *ds = BLI_dynstr_new();

  BLI_dynstr_append(ds, "https://docs.blender.org/manual/en/dev/troubleshooting/gpu/");
#if defined(_WIN32)
  BLI_dynstr_append(ds, "windows/");
#elif defined(__APPLE__)
  BLI_dynstr_append(ds, "apple/");
#else /* UNIX */
  BLI_dynstr_append(ds, "linux/");
#endif

  if (GPU_type_matches(GPU_DEVICE_INTEL, GPU_OS_ANY, GPU_DRIVER_ANY)) {
    BLI_dynstr_append(ds, "intel.html");
  }
  else if (GPU_type_matches(GPU_DEVICE_NVIDIA, GPU_OS_ANY, GPU_DRIVER_ANY)) {
    BLI_dynstr_append(ds, "nvidia.html");
  }
  else if (GPU_type_matches(GPU_DEVICE_ATI, GPU_OS_ANY, GPU_DRIVER_ANY)) {
    BLI_dynstr_append(ds, "amd.html");
  }
  else {
    BLI_dynstr_append(ds, "unknown.html");
  }

  BLI_assert(BLI_dynstr_get_len(ds) < WM_PLATFORM_SUPPORT_TEXT_SIZE);
  BLI_dynstr_get_cstring_ex(ds, link);
  BLI_dynstr_free(ds);
}

bool WM_platform_support_perform_checks()
{
  char title[WM_PLATFORM_SUPPORT_TEXT_SIZE];
  char message[WM_PLATFORM_SUPPORT_TEXT_SIZE];
  char link[WM_PLATFORM_SUPPORT_TEXT_SIZE];

  bool result = true;

  eGPUSupportLevel support_level = GPU_platform_support_level();
  const char *platform_key = GPU_platform_support_level_key();

  /* Check if previous check matches the current check. Don't update the approval when running in
   * `background`. this could have been triggered by installing add-ons via installers.  */
  if (support_level != GPU_SUPPORT_LEVEL_UNSUPPORTED && !G.factory_startup &&
      wm_platform_support_check_approval(platform_key, !G.background)) {
    /* If it matches the user has confirmed and wishes to use it. */
    return result;
  }

  /* update the message and link based on the found support level */
  GHOST_DialogOptions dialog_options = 0;

  switch (support_level) {
    default:
    case GPU_SUPPORT_LEVEL_SUPPORTED:
      break;

    case GPU_SUPPORT_LEVEL_LIMITED: {
      size_t slen = 0;
      STR_CONCAT(title, slen, "Blender - ");
      STR_CONCAT(
          title, slen, CTX_IFACE_(BLT_I18NCONTEXT_ID_WINDOWMANAGER, "Limited Platform Support"));
      slen = 0;
      STR_CONCAT(
          message,
          slen,
          CTX_IFACE_(BLT_I18NCONTEXT_ID_WINDOWMANAGER,
                     "Your graphics card or driver has limited support. It may work, but with "
                     "issues."));

      /* TODO: Extra space is needed for the split function in GHOST_SystemX11. We should change
       * the behavior in GHOST_SystemX11. */
      STR_CONCAT(message, slen, "\n \n");
      STR_CONCAT(
          message,
          slen,
          CTX_IFACE_(BLT_I18NCONTEXT_ID_WINDOWMANAGER,
                     "Newer graphics drivers may be available to improve Blender support."));
      STR_CONCAT(message, slen, "\n \n");
      STR_CONCAT(message, slen, CTX_IFACE_(BLT_I18NCONTEXT_ID_WINDOWMANAGER, "Graphics card:\n"));
      STR_CONCAT(message, slen, GPU_platform_gpu_name());

      dialog_options = GHOST_DialogWarning;
      break;
    }

    case GPU_SUPPORT_LEVEL_UNSUPPORTED: {
      size_t slen = 0;
      STR_CONCAT(title, slen, "Blender - ");
      STR_CONCAT(
          title, slen, CTX_IFACE_(BLT_I18NCONTEXT_ID_WINDOWMANAGER, "Platform Unsupported"));
      slen = 0;
      STR_CONCAT(message,
                 slen,
                 CTX_IFACE_(BLT_I18NCONTEXT_ID_WINDOWMANAGER,
                            "Your graphics card or driver is not supported."));

      STR_CONCAT(message, slen, "\n \n");
      STR_CONCAT(
          message,
          slen,
          CTX_IFACE_(BLT_I18NCONTEXT_ID_WINDOWMANAGER,
                     "Newer graphics drivers may be available to improve Blender support."));

      STR_CONCAT(message, slen, "\n \n");
      STR_CONCAT(message, slen, CTX_IFACE_(BLT_I18NCONTEXT_ID_WINDOWMANAGER, "Graphics card:\n"));
      STR_CONCAT(message, slen, GPU_platform_gpu_name());
      STR_CONCAT(message, slen, "\n \n");

      STR_CONCAT(message,
                 slen,
                 CTX_IFACE_(BLT_I18NCONTEXT_ID_WINDOWMANAGER, "The program will now close."));
      dialog_options = GHOST_DialogError;
      result = false;
      break;
    }
  }
  wm_platform_support_create_link(link);

  bool show_message = ELEM(
      support_level, GPU_SUPPORT_LEVEL_LIMITED, GPU_SUPPORT_LEVEL_UNSUPPORTED);

  /* We are running in the background print the message in the console. */
  if ((G.background || G.debug & G_DEBUG) && show_message) {
    printf("%s\n\n%s\n%s\n", title, message, link);
  }
  if (G.background) {
    /* Don't show the message-box when running in background mode.
     * Printing to console is enough. */
    result = true;
  }
  else if (show_message) {
    WM_ghost_show_message_box(
        title, message, "Find Latest Drivers", "Continue Anyway", link, dialog_options);
  }

  return result;
}
