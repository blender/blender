/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef LIBMV_C_API_LOGGING_H_
#define LIBMV_C_API_LOGGING_H_

#ifdef __cplusplus
extern "C" {
#endif

// Initialize GLog logging.
void libmv_initLogging(const char* argv0);

// Switch Glog to debug logging level.
void libmv_startDebugLogging(void);

// Set GLog logging verbosity level.
void libmv_setLoggingVerbosity(int verbosity);

#ifdef __cplusplus
}
#endif

#endif  // LIBMV_C_API_LOGGING_H_
