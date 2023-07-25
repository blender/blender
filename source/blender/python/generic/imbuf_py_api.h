/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup pygen
 */

#ifdef __cplusplus
extern "C" {
#endif

PyObject *BPyInit_imbuf(void);

extern PyTypeObject Py_ImBuf_Type;

#ifdef __cplusplus
}
#endif
