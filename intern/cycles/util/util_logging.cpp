/*
 * Copyright 2011-2014 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "util/util_logging.h"

#include "util/util_math.h"
#include "util/util_string.h"

#include <stdio.h>
#ifdef _MSC_VER
#  define snprintf _snprintf
#endif

CCL_NAMESPACE_BEGIN

static bool is_verbosity_set()
{
#ifdef WITH_CYCLES_LOGGING
  using CYCLES_GFLAGS_NAMESPACE::GetCommandLineOption;

  std::string verbosity;
  if (!GetCommandLineOption("v", &verbosity)) {
    return false;
  }
  return verbosity != "0";
#else
  return false;
#endif
}

void util_logging_init(const char *argv0)
{
#ifdef WITH_CYCLES_LOGGING
  using CYCLES_GFLAGS_NAMESPACE::SetCommandLineOption;

  /* Make it so ERROR messages are always print into console. */
  char severity_fatal[32];
  snprintf(severity_fatal, sizeof(severity_fatal), "%d", google::GLOG_ERROR);

  google::InitGoogleLogging(argv0);
  SetCommandLineOption("logtostderr", "1");
  if (!is_verbosity_set()) {
    SetCommandLineOption("v", "0");
  }
  SetCommandLineOption("stderrthreshold", severity_fatal);
  SetCommandLineOption("minloglevel", severity_fatal);
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
  SetCommandLineOption("stderrthreshold", "1");
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
