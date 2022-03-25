/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void python_script_error_jump(const char *filepath, int *lineno, int *offset);

#ifdef __cplusplus
}
#endif
