/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "blender/CCL_api.h"
#include "util/log.h"

void CCL_init_logging(const char *argv0)
{
  ccl::util_logging_init(argv0);
}

void CCL_start_debug_logging()
{
  ccl::util_logging_start();
}

void CCL_logging_verbosity_set(int verbosity)
{
  ccl::util_logging_verbosity_set(verbosity);
}
