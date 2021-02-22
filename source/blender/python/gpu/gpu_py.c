/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup bpygpu
 *
 * - Use ``bpygpu_`` for local API.
 * - Use ``BPyGPU`` for public API.
 */

#include <Python.h>

#include "GPU_primitive.h"
#include "GPU_texture.h"

#include "../generic/py_capi_utils.h"

#include "gpu_py.h" /* own include */

/* -------------------------------------------------------------------- */
/** \name GPU Enums
 * \{ */

struct PyC_StringEnumItems bpygpu_primtype_items[] = {
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
    {0, NULL},
};

struct PyC_StringEnumItems bpygpu_dataformat_items[] = {
    {GPU_DATA_FLOAT, "FLOAT"},
    {GPU_DATA_INT, "INT"},
    {GPU_DATA_UINT, "UINT"},
    {GPU_DATA_UBYTE, "UBYTE"},
    {GPU_DATA_UINT_24_8, "UINT_24_8"},
    {GPU_DATA_10_11_11_REV, "10_11_11_REV"},
    {0, NULL},
};
/** \} */
