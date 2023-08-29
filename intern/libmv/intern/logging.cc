/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <gflags/gflags.h>

#include "intern/logging.h"
#include "intern/utildefines.h"
#include "libmv/logging/logging.h"

static bool is_verbosity_set() {
  using LIBMV_GFLAGS_NAMESPACE::GetCommandLineOption;

  std::string verbosity;
  if (!GetCommandLineOption("v", &verbosity)) {
    return false;
  }
  return verbosity != "0";
}

void libmv_initLogging(const char* argv0) {
  using LIBMV_GFLAGS_NAMESPACE::SetCommandLineOption;
  google::InitGoogleLogging(argv0);
  SetCommandLineOption("logtostderr", "1");
  if (!is_verbosity_set()) {
    SetCommandLineOption("v", "0");
  }
  SetCommandLineOption("stderrthreshold", "0");
  SetCommandLineOption("minloglevel", "0");
}

void libmv_startDebugLogging(void) {
  using LIBMV_GFLAGS_NAMESPACE::SetCommandLineOption;
  SetCommandLineOption("logtostderr", "1");
  if (!is_verbosity_set()) {
    SetCommandLineOption("v", "2");
  }
  SetCommandLineOption("stderrthreshold", "0");
  SetCommandLineOption("minloglevel", "0");
}

void libmv_setLoggingVerbosity(int verbosity) {
  using LIBMV_GFLAGS_NAMESPACE::SetCommandLineOption;
  char val[10];
  snprintf(val, sizeof(val), "%d", verbosity);
  SetCommandLineOption("v", val);
}
