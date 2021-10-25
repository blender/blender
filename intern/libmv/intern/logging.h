/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 * The Original Code is Copyright (C) 2011 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

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
