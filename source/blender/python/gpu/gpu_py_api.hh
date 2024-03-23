/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bpygpu
 */

#pragma once

/* Each type object could have a method for free GPU resources.
 * However, it is currently of little use. */
// #define BPYGPU_USE_GPUOBJ_FREE_METHOD

PyObject *BPyInit_gpu(void);
