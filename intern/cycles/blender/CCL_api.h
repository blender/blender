/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifndef __CCL_API_H__
#define __CCL_API_H__

#ifdef __cplusplus
extern "C" {
#endif

/* create python module _cycles used by addon */

void *CCL_python_module_init(void);

void CCL_init_logging(const char *argv0);
void CCL_start_debug_logging(void);
void CCL_logging_verbosity_set(int verbosity);

#ifdef __cplusplus
}
#endif

#endif /* __CCL_API_H__ */
