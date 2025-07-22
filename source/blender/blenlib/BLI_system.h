/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <stdio.h>

/** \file
 * \ingroup bli
 */

int BLI_cpu_support_sse2(void);
int BLI_cpu_support_sse42(void);
/**
 * Write a backtrace into a file for systems which support it.
 */
void BLI_system_backtrace_with_os_info(FILE *fp, const void *os_info);
void BLI_system_backtrace(FILE *fp);

/** Get CPU brand, result is to be MEM_freeN()-ed. */
char *BLI_cpu_brand_string(void);

/**
 * Obtain the hostname from the system.
 *
 * This simply determines the host's name, and doesn't do any DNS lookup of any
 * IP address of the machine. As such, it's only usable for identification
 * purposes, and not for reachability over a network.
 *
 * \param buffer: Character buffer to write the hostname into.
 * \param bufsize: Size of the character buffer, including trailing '\0'.
 */
void BLI_hostname_get(char *buffer, size_t bufsize);

/** Get maximum addressable memory in megabytes. */
size_t BLI_system_memory_max_in_megabytes(void);
/** Get maximum addressable memory in megabytes (clamped to #INT_MAX). */
int BLI_system_memory_max_in_megabytes_int(void);

/* For `getpid`. */
#ifdef WIN32
#  define BLI_SYSTEM_PID_H <process.h>

/**
 * \note Use `void *` for `os_info` since we really do not want to drag Windows.h
 * in to get the proper `typedef`.
 */
void BLI_windows_exception_print_message(const void *os_info);

/**
 * Displays a crash report dialog with options to open the crash log, restart the application, and
 * report a bug. This is based on the `showMessageBox` function in `GHOST_SystemWin32.cc`.
 */
void BLI_windows_exception_show_dialog(const char *filepath_crashlog,
                                       const char *filepath_relaunch,
                                       const char *gpu_name,
                                       const char *build_version);

#else
#  define BLI_SYSTEM_PID_H <unistd.h>
#endif
