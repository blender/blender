/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 */
#include "wm_platform_support.hh"
#include "wm_window_private.hh"

#include <cstring>

#include "BLI_dynstr.h"
#include "BLI_fileops.h"
#include "BLI_linklist.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"

#include "BLT_translation.hh"

#include "BKE_appdir.hh"
#include "BKE_global.hh"

#include "GPU_context.hh"
#include "GPU_platform.hh"

#include "CLG_log.h"

#define WM_PLATFORM_SUPPORT_TEXT_SIZE 1024

static CLG_LogRef LOG = {"gpu.platform"};

/**
 * Check if user has already approved the given `platform_support_key`.
 */
static bool wm_platform_support_check_approval(const char *platform_support_key, bool update)
{
  if (G.factory_startup) {
    return false;
  }
  const std::optional<std::string> cfgdir = BKE_appdir_folder_id(BLENDER_USER_CONFIG, nullptr);
  if (!cfgdir.has_value()) {
    return false;
  }

  bool result = false;
  char filepath[FILE_MAX];
  BLI_path_join(filepath, sizeof(filepath), cfgdir->c_str(), BLENDER_PLATFORM_SUPPORT_FILE);
  LinkNode *lines = BLI_file_read_as_lines(filepath);
  for (LinkNode *line_node = lines; line_node; line_node = line_node->next) {
    const char *line = static_cast<char *>(line_node->link);
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
#else /* UNIX. */
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

  GPUSupportLevel support_level = GPU_platform_support_level();
  const char *platform_key = GPU_platform_support_level_key();

  CLOG_INFO(&LOG, "Using GPU \"%s\"", GPU_platform_gpu_name());
  CLOG_INFO(&LOG, "Using Backend \"%s\"", GPU_backend_get_name());

  /* Check if previous check matches the current check. Don't update the approval when running in
   * `background`. this could have been triggered by installing add-ons via installers. */
  if (support_level != GPU_SUPPORT_LEVEL_UNSUPPORTED && !G.factory_startup &&
      wm_platform_support_check_approval(platform_key, !G.background))
  {
    /* If it matches the user has confirmed and wishes to use it. */
    return result;
  }

  bool backend_detected = GPU_backend_get_type() != GPU_BACKEND_NONE;
  bool show_message = ELEM(
      support_level, GPU_SUPPORT_LEVEL_LIMITED, GPU_SUPPORT_LEVEL_UNSUPPORTED);
  bool show_continue = backend_detected && support_level != GPU_SUPPORT_LEVEL_UNSUPPORTED;
  bool show_link = backend_detected;
  link[0] = '\0';
  if (show_link) {
    wm_platform_support_create_link(link);
  }

  /* Update the message and link based on the found support level. */
  GHOST_DialogOptions dialog_options = GHOST_DialogOptions(0);

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
          CTX_IFACE_(
              BLT_I18NCONTEXT_ID_WINDOWMANAGER,
              "Your graphics card or driver version has limited support. It may work, but with "
              "issues."));

      /* TODO: Extra space is needed for the split function in GHOST_SystemX11. We should change
       * the behavior in GHOST_SystemX11. */
      STR_CONCAT(message, slen, "\n \n");
      STR_CONCAT(
          message,
          slen,
          CTX_IFACE_(
              BLT_I18NCONTEXT_ID_WINDOWMANAGER,
              "Newer graphics drivers might be available with better Blender compatibility."));
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

#ifdef __APPLE__
      if (GPU_type_matches(GPU_DEVICE_NVIDIA, GPU_OS_ANY, GPU_DRIVER_ANY)) {
        STR_CONCAT(
            message,
            slen,
            CTX_IFACE_(BLT_I18NCONTEXT_ID_WINDOWMANAGER, "Your graphics card is not supported"));
      }
      else {
        STR_CONCAT(message,
                   slen,
                   CTX_IFACE_(BLT_I18NCONTEXT_ID_WINDOWMANAGER,
                              "Your graphics card or macOS version is not supported"));
        STR_CONCAT(message, slen, "\n \n");

        STR_CONCAT(
            message,
            slen,
            CTX_IFACE_(BLT_I18NCONTEXT_ID_WINDOWMANAGER,
                       "Upgrading to the latest macOS version may improve Blender support"));
      }
#else
      STR_CONCAT(message,
                 slen,
                 CTX_IFACE_(BLT_I18NCONTEXT_ID_WINDOWMANAGER,
                            "Your graphics card or driver version is not supported."));
      STR_CONCAT(message, slen, "\n \n");
      STR_CONCAT(
          message,
          slen,
          CTX_IFACE_(
              BLT_I18NCONTEXT_ID_WINDOWMANAGER,
              "Newer graphics drivers might be available with better Blender compatibility."));

      STR_CONCAT(message, slen, "\n \n");
      STR_CONCAT(message, slen, CTX_IFACE_(BLT_I18NCONTEXT_ID_WINDOWMANAGER, "Graphics card:\n"));
      STR_CONCAT(message, slen, GPU_platform_gpu_name());
#endif
      STR_CONCAT(message, slen, "\n \n");

      if (!show_continue) {
        STR_CONCAT(message,
                   slen,
                   CTX_IFACE_(BLT_I18NCONTEXT_ID_WINDOWMANAGER, "Blender will now close."));
        dialog_options = GHOST_DialogError;
        result = false;
      }
      break;
    }
  }

  if (show_message) {
    /* Always print when in background mode or using debug argument. */
    if (G.background || G.debug & G_DEBUG) {
      CLOG_INFO_NOCHECK(&LOG, "%s\n\n%s\n%s\n", title, message, link);
    }
    else {
      CLOG_INFO(&LOG, "%s\n\n%s\n%s\n", title, message, link);
    }
  }
  if (G.background) {
    /* Don't show the message-box when running in background mode.
     * Printing to console is enough. */
    result = true;
  }
  else if (show_message) {
    WM_ghost_show_message_box(title,
                              message,
                              "Find Latest Drivers",
                              show_continue ? "Continue Anyway" : "Exit",
                              link,
                              dialog_options);
  }

  return result;
}
