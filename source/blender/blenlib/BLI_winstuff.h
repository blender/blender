/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 * \brief Compatibility-like things for windows.
 */

#ifndef _WIN32
#  error "This include is for Windows only!"
#endif

#include "BLI_sys_types.h"

#define WIN32_LEAN_AND_MEAN

#ifndef NOMINMAX
#  define NOMINMAX
#  include <windows.h>
#  undef NOMINMAX
#else
#  include <windows.h>
#endif

#undef rad
#undef rad1
#undef rad2
#undef rad3
#undef vec
#undef rect
#undef rct1
#undef rct2

#undef small

/* These definitions are also in BLI_math for simplicity. */

#if !defined(_USE_MATH_DEFINES)
#  define _USE_MATH_DEFINES
#endif

#ifndef S_ISREG
#  define S_ISREG(x) (((x) & _S_IFREG) == _S_IFREG)
#endif
#ifndef S_ISDIR
#  define S_ISDIR(x) (((x) & _S_IFDIR) == _S_IFDIR)
#endif

#if defined(_MSC_VER)
#  define R_OK 4
#  define W_OK 2
/* Not accepted by `access()` on windows. */
// #  define X_OK    1
#  define F_OK 0
#endif

typedef unsigned int mode_t;

/** Directory reading compatibility with UNIX. */
struct dirent {
  int d_ino;
  int d_off;
  unsigned short d_reclen;
  char *d_name;
};

/** Intentionally opaque to users. */
typedef struct __dirstream DIR;

DIR *opendir(const char *path);
struct dirent *readdir(DIR *dp);
int closedir(DIR *dp);
const char *dirname(char *path);

namespace blender {

/* Windows utility functions. */

bool BLI_windows_is_store_install(void);
bool BLI_windows_register_blend_extension(bool all_users);
bool BLI_windows_unregister_blend_extension(bool all_users);
bool BLI_windows_update_pinned_launcher(const char *launcher_path);

/* Gets the version of the currently loaded DirectX driver for the first device that matches
 * deviceString. This is required for Qualcomm devices which use Mesa's Gallium D2D12 layer for
 * OpenGL functionality */
bool BLI_windows_get_directx_driver_version(const wchar_t *deviceSubString,
                                            long long *r_driverVersion);

/* Checks the version of the Windows build in the format "major.minor.build".
 * Example: 10.0.22000 corresponds to Windows 11 21H2. */
bool BLI_windows_is_build_version_greater_or_equal(DWORD majorVersion,
                                                   DWORD minorVersion,
                                                   DWORD buildNumber);

/**
 * Set the `root_dir` to the default root directory on MS-Windows,
 * The string is guaranteed to be set with a length of 3 & null terminated,
 * using a fall-back in case the root directory can't be found.
 */
void BLI_windows_get_default_root_dir(char root_dir[4]);
int BLI_windows_get_executable_dir(char r_dirpath[/*FILE_MAXDIR*/]);

/* ShellExecute Helpers. */

bool BLI_windows_external_operation_supported(const char *filepath, const char *operation);
bool BLI_windows_external_operation_execute(const char *filepath, const char *operation);

/**
 * Launch our own executable.
 *
 * \param parameters: application parameters separated by spaces.
 * \param wait: whether to wait for the instance to exit.
 * \param elevated: run as administrator. Will do UAC prompt.
 * \param silent: Not show the launched program.
 */
bool BLI_windows_execute_self(const char *parameters,
                              const bool wait,
                              const bool elevated,
                              const bool silent);

/** Quality of Service (QoS) modes as defined in the Windows documentation at:
 * https://learn.microsoft.com/en-us/windows/win32/procthread/quality-of-service */
enum class QoSMode {
  /** Default mode uses heuristics described in Windows docs. */
  DEFAULT = 0,
  /** HighQoS mode for performance critical scenarios. */
  HIGH = 1,
  /** EcoQoS mode for preserving energy. */
  ECO = 2
};

/** QoS precedence (to make sure command line args overwrite what is set by jobs).
 * Higher values have more precedence. */
enum class QoSPrecedence {
  /** QoS mode requested set via the job system. */
  JOB = 0,
  /** QoS mode requested set via a command line argument. */
  CMDLINE_ARG = 1,
};

/**
 * Sets the Quality of Service (QoS) mode of the process.
 *
 * \param qos_mode: The QoS mode to use for the process.
 * \param qos_precedence: The precedence of the caller (higher wins).
 */
void BLI_windows_process_set_qos(QoSMode qos_mode, QoSPrecedence qos_precedence);

}  // namespace blender
