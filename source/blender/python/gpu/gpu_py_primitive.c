/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * Copyright 2015, Blender Foundation.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/python/gpu/gpu_py_primitive.c
 *  \ingroup bpygpu
 *
 * - Use ``bpygpu_`` for local API.
 * - Use ``BPyGPU`` for public API.
 */

#include <Python.h>

#include "BLI_utildefines.h"

#include "GPU_primitive.h"

#include "gpu_py_primitive.h" /* own include */


/* -------------------------------------------------------------------- */

/** \name Primitive Utils
 * \{ */

int bpygpu_ParsePrimType(PyObject *o, void *p)
{
	Py_ssize_t mode_id_len;
	const char *mode_id = _PyUnicode_AsStringAndSize(o, &mode_id_len);
	if (mode_id == NULL) {
		PyErr_Format(PyExc_ValueError,
		             "expected a string, got %s",
		             Py_TYPE(o)->tp_name);
		return 0;
	}
#define MATCH_ID(id) \
	if (mode_id_len == strlen(STRINGIFY(id))) { \
		if (STREQ(mode_id, STRINGIFY(id))) { \
			mode = GPU_PRIM_##id; \
			goto success; \
		} \
	} ((void)0)

	GPUPrimType mode;
	MATCH_ID(POINTS);
	MATCH_ID(LINES);
	MATCH_ID(TRIS);
	MATCH_ID(LINE_STRIP);
	MATCH_ID(LINE_LOOP);
	MATCH_ID(TRI_STRIP);
	MATCH_ID(TRI_FAN);
	MATCH_ID(LINE_STRIP_ADJ);

#undef MATCH_ID
	PyErr_Format(PyExc_ValueError,
	             "unknown type literal: '%s'",
	             mode_id);
	return 0;

success:
	(*(GPUPrimType *)p) = mode;
	return 1;
}
