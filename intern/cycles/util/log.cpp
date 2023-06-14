/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "util/log.h"

#include "util/math.h"
#include "util/string.h"

#include <stdio.h>
#ifdef _MSC_VER
#  define snprintf _snprintf
#endif

CCL_NAMESPACE_BEGIN

#ifdef WITH_CYCLES_LOGGING
static bool is_verbosity_set()
{
  using CYCLES_GFLAGS_NAMESPACE::GetCommandLineOption;

  std::string verbosity;
  if (!GetCommandLineOption("v", &verbosity)) {
    return false;
  }
  return verbosity != "0";
}
#endif

void util_logging_init(const char *argv0)
{
#ifdef WITH_CYCLES_LOGGING
  using CYCLES_GFLAGS_NAMESPACE::SetCommandLineOption;

  google::InitGoogleLogging(argv0);
  SetCommandLineOption("logtostderr", "1");
  if (!is_verbosity_set()) {
    SetCommandLineOption("v", "0");
  }
  SetCommandLineOption("stderrthreshold", "0");
  SetCommandLineOption("minloglevel", "0");
#else
  (void)argv0;
#endif
}

void util_logging_start()
{
#ifdef WITH_CYCLES_LOGGING
  using CYCLES_GFLAGS_NAMESPACE::SetCommandLineOption;
  SetCommandLineOption("logtostderr", "1");
  if (!is_verbosity_set()) {
    SetCommandLineOption("v", "2");
  }
  SetCommandLineOption("stderrthreshold", "0");
  SetCommandLineOption("minloglevel", "0");
#endif
}

void util_logging_verbosity_set(int verbosity)
{
#ifdef WITH_CYCLES_LOGGING
  using CYCLES_GFLAGS_NAMESPACE::SetCommandLineOption;
  char val[10];
  snprintf(val, sizeof(val), "%d", verbosity);
  SetCommandLineOption("v", val);
#else
  (void)verbosity;
#endif
}

std::ostream &operator<<(std::ostream &os, const int2 &value)
{
  os << "(" << value.x << ", " << value.y << ")";
  return os;
}

std::ostream &operator<<(std::ostream &os, const float3 &value)
{
  os << "(" << value.x << ", " << value.y << ", " << value.z << ")";
  return os;
}

CCL_NAMESPACE_END
