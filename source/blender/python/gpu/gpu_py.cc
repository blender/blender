/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bpygpu
 *
 * - Use `bpygpu_` for local API.
 * - Use `BPyGPU` for public API.
 */

#include <Python.h>

#include "GPU_init_exit.hh"
#include "GPU_primitive.hh"
#include "GPU_texture.hh"

#include "../generic/py_capi_utils.h"

#include "gpu_py.hh" /* own include */

/* -------------------------------------------------------------------- */
/** \name GPU Enums
 * \{ */

PyC_StringEnumItems bpygpu_primtype_items[] = {
    {GPU_PRIM_POINTS, "POINTS"},
    {GPU_PRIM_LINES, "LINES"},
    {GPU_PRIM_TRIS, "TRIS"},
    {GPU_PRIM_LINE_STRIP, "LINE_STRIP"},
    {GPU_PRIM_LINE_LOOP, "LINE_LOOP"},
    {GPU_PRIM_TRI_STRIP, "TRI_STRIP"},
    {GPU_PRIM_TRI_FAN, "TRI_FAN"},
    {GPU_PRIM_LINES_ADJ, "LINES_ADJ"},
    {GPU_PRIM_TRIS_ADJ, "TRIS_ADJ"},
    {GPU_PRIM_LINE_STRIP_ADJ, "LINE_STRIP_ADJ"},
    {0, nullptr},
};

PyC_StringEnumItems bpygpu_dataformat_items[] = {
    {GPU_DATA_FLOAT, "FLOAT"},
    {GPU_DATA_INT, "INT"},
    {GPU_DATA_UINT, "UINT"},
    {GPU_DATA_UBYTE, "UBYTE"},
    {GPU_DATA_UINT_24_8, "UINT_24_8"},
    {GPU_DATA_10_11_11_REV, "10_11_11_REV"},
    {0, nullptr},
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Utilities
 * \{ */

bool bpygpu_is_init_or_error(void)
{
  if (!GPU_is_init()) {
    PyErr_SetString(PyExc_SystemError,
                    "GPU functions for drawing are not available in background mode");

    return false;
  }

  return true;
}

/** \} */
