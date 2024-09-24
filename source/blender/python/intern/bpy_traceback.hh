/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 */

#pragma once

bool python_script_error_jump(
    const char *filepath, int *r_lineno, int *r_offset, int *r_lineno_end, int *r_offset_end);
