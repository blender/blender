/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pygen
 * \brief header-only compatibility defines.
 */

#pragma once

/* Removes `intialized` member from Python 3.13+. */
#if PY_VERSION_HEX >= 0x030d0000
#  define PY_ARG_PARSER_HEAD_COMPAT()
#elif PY_VERSION_HEX >= 0x030c0000
/* Add `intialized` member for Python 3.12+. */
#  define PY_ARG_PARSER_HEAD_COMPAT() 0,
#else
#  define PY_ARG_PARSER_HEAD_COMPAT()
#endif

/* Python 3.13 made some changes, use the "new" names. */
#if PY_VERSION_HEX < 0x030d0000
#  define PyObject_GetOptionalAttr _PyObject_LookupAttr
#endif
