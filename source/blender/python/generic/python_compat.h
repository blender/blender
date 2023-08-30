/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pygen
 * \brief header-only compatibility defines.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Add `intialized` member for Python 3.12+. */
#if PY_VERSION_HEX >= 0x030c0000
#  define PY_ARG_PARSER_HEAD_COMPAT() 0,
#else
#  define PY_ARG_PARSER_HEAD_COMPAT()
#endif

#ifdef __cplusplus
}
#endif
