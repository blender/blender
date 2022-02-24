/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2010 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup GHOST
 */

#pragma once

#include "GHOST_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

GHOST_DECLARE_HANDLE(GHOST_SystemPathsHandle);

/**
 * Creates the one and only instance of the system path access.
 * \return An indication of success.
 */
extern GHOST_TSuccess GHOST_CreateSystemPaths(void);

/**
 * Disposes the one and only system.
 * \return An indication of success.
 */
extern GHOST_TSuccess GHOST_DisposeSystemPaths(void);

/**
 * Determine the base dir in which shared resources are located. It will first try to use
 * "unpack and run" path, then look for properly installed path, including versioning.
 * \return Unsigned char string pointing to system dir (eg /usr/share/blender/).
 */
extern const char *GHOST_getSystemDir(int version, const char *versionstr);

/**
 * Determine the base dir in which user configuration is stored, including versioning.
 * \return Unsigned char string pointing to user dir (eg ~).
 */
extern const char *GHOST_getUserDir(int version, const char *versionstr);

/**
 * Determine a special ("well known") and easy to reach user directory.
 * \return Unsigned char string pointing to user dir (eg `~/Documents/`).
 */
extern const char *GHOST_getUserSpecialDir(GHOST_TUserSpecialDirTypes type);

/**
 * Determine the dir in which the binary file is found.
 * \return Unsigned char string pointing to binary dir (eg ~/usr/local/bin/).
 */
extern const char *GHOST_getBinaryDir(void);

/**
 * Add the file to the operating system most recently used files
 */
extern void GHOST_addToSystemRecentFiles(const char *filename);

#ifdef __cplusplus
}
#endif
