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
 */

#pragma once

int bpygpu_ParsePrimType(PyObject *o, void *p);

PyObject *BPyInit_gpu(void);

bool bpygpu_is_init_or_error(void);
#define BPYGPU_IS_INIT_OR_ERROR_OBJ \
  if (UNLIKELY(!bpygpu_is_init_or_error())) { \
    return NULL; \
  } \
  ((void)0)
#define BPYGPU_IS_INIT_OR_ERROR_INT \
  if (UNLIKELY(!bpygpu_is_init_or_error())) { \
    return -1; \
  } \
  ((void)0)
