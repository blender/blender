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

#include "intern/logging.h"
#include "intern/utildefines.h"
#include "libmv/logging/logging.h"

void libmv_initLogging(const char* argv0) {
  // Make it so FATAL messages are always print into console.
  char severity_fatal[32];
  snprintf(severity_fatal, sizeof(severity_fatal), "%d",
           google::GLOG_FATAL);

  google::InitGoogleLogging(argv0);
  google::SetCommandLineOption("logtostderr", "1");
  google::SetCommandLineOption("v", "0");
  google::SetCommandLineOption("stderrthreshold", severity_fatal);
  google::SetCommandLineOption("minloglevel", severity_fatal);
}

void libmv_startDebugLogging(void) {
  google::SetCommandLineOption("logtostderr", "1");
  google::SetCommandLineOption("v", "2");
  google::SetCommandLineOption("stderrthreshold", "1");
  google::SetCommandLineOption("minloglevel", "0");
}

void libmv_setLoggingVerbosity(int verbosity) {
  char val[10];
  snprintf(val, sizeof(val), "%d", verbosity);
  google::SetCommandLineOption("v", val);
}
