/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup pygen
 */

#include <Python.h>

namespace blender {

struct ImBuf;

[[nodiscard]] PyObject *BPyInit_imbuf();

extern PyTypeObject Py_ImBuf_Type;

/** Return the #ImBuf or null with an error set. */
[[nodiscard]] ImBuf *BPy_ImBuf_FromPyObject(PyObject *py_imbuf);

}  // namespace blender
