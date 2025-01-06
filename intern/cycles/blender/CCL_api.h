/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* create python module _cycles used by addon */

void *CCL_python_module_init(void);

void CCL_init_logging(const char *argv0);
void CCL_start_debug_logging(void);
void CCL_logging_verbosity_set(const int verbosity);

#ifdef __cplusplus
}
#endif
